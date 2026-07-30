/* ns-pango
 * ns-shape-cache.c: Cross-layout cache of shaped runs.
 *
 * Copyright (C) 2026 Northstar contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>

#include "ns-shape-cache.h"
#include "ns-pango-cache.h"
#include "pango-impl-utils.h"

/* Pango has no cache that outlives a NsPangoLayout, so a browser that measures
 * a run and then paints it, or that asks for min-content and max-content before
 * laying a cell out, shapes the same bytes with the same font several times
 * over. This keeps the finished glyph string, keyed on everything hb_shape
 * reads.
 *
 * An item is only cached when its shaping cannot depend on the text around it:
 * HarfBuzz is given the paragraph as pre- and post-context, so an item that
 * starts or ends mid-word may join, reorder or ligate across its own boundary.
 * A boundary is independent when the character on either side of it neither
 * joins nor ligates: any space character, or an ideograph, kana letter or
 * Hangul syllable, which are written without spaces and never join.
 */

#define NS_SHAPE_CACHE_MAX_ENTRIES 20000
#define NS_SHAPE_CACHE_MAX_TEXT    4096
#define NS_SHAPE_CACHE_MAX_BYTES   (48 * 1024 * 1024)

struct _NsPangoShapeKey
{
  guint      hash;
  gpointer   font;
  gpointer   language;
  guint32    level;
  guint32    gravity;
  guint32    script;
  guint32    analysis_flags;
  guint32    shape_flags;
  guint32    show_flags;
  guint32    transform;
  guint32    hyphen;
  guint32    n_features;
  guint32    text_length;
  hb_feature_t features[8];
  char       text[1];
};

typedef struct
{
  int               num_glyphs;
  gboolean          read;
  NsPangoGlyphInfo *glyphs;
  int              *log_clusters;
} CachedRun;

static GHashTable *shape_cache;
G_LOCK_DEFINE_STATIC (shape_cache);
static gsize cache_bytes;

/* Diagnostics only, so counters that may wrap are good enough. They are
 * atomic rather than plain because every thread that shapes writes them, and a
 * data race is undefined behaviour even where the value does not matter.
 */
static gint cache_hits;
static gint cache_misses;
static gint cache_skips;
static gint skip_font, skip_len, skip_feat, skip_ctx;

static gboolean
enabled_from_env (void)
{
  const char *v = g_getenv ("NS_PANGO_SHAPE_CACHE");

  return !(v && (strcmp (v, "0") == 0 || g_ascii_strcasecmp (v, "off") == 0));
}

gboolean
ns_pango_shape_cache_enabled (void)
{
  static gint enabled = -1;
  gint value = g_atomic_int_get (&enabled);

  if (G_UNLIKELY (value < 0))
    {
      value = enabled_from_env () ? 1 : 0;
      g_atomic_int_set (&enabled, value);
    }

  return value == 1;
}

gboolean
ns_pango_shape_cache_verifying (void)
{
  static gint verifying = -1;
  gint value = g_atomic_int_get (&verifying);

  if (G_UNLIKELY (value < 0))
    {
      const char *v = g_getenv ("NS_PANGO_SHAPE_CACHE");

      value = (v && g_ascii_strcasecmp (v, "verify") == 0) ? 1 : 0;
      g_atomic_int_set (&verifying, value);
    }

  return value == 1;
}

/* CJK text has no spaces in it, so a rule that wants whitespace at both ends
 * of an item never caches a CJK paragraph at all -- and CJK is where a page
 * shapes the most glyphs. An ideograph, a kana letter and a precomposed Hangul
 * syllable neither join nor ligate with a neighbour, so a boundary next to one
 * is as independent as a boundary next to a space.
 *
 * The ranges are deliberately narrow. Left out, because they can interact
 * across the boundary: CJK punctuation, which fonts squeeze with contextual
 * spacing features; the prolonged sound mark and the iteration marks, which are
 * modifier letters; the combining voiced sound marks; the halfwidth and
 * fullwidth forms; and Hangul jamo, which compose into syllables.
 */
static gboolean
isolated_ideograph (gunichar ch)
{
  return (ch >= 0x4E00 && ch <= 0x9FFF) ||    /* CJK Unified Ideographs */
         (ch >= 0x3400 && ch <= 0x4DBF) ||    /* ... Extension A */
         (ch >= 0xF900 && ch <= 0xFAFF) ||    /* Compatibility Ideographs */
         (ch >= 0x20000 && ch <= 0x2A6DF) ||  /* ... Extension B */
         (ch >= 0x2A700 && ch <= 0x2EBEF) ||  /* ... Extensions C to F */
         (ch >= 0x3041 && ch <= 0x3096) ||    /* Hiragana letters */
         (ch >= 0x30A1 && ch <= 0x30FA) ||    /* Katakana letters */
         (ch >= 0xAC00 && ch <= 0xD7A3);      /* Hangul syllables */
}

static gboolean
boundary_char (gunichar ch)
{
  /* Every space character, not only ASCII space: &nbsp; is everywhere in HTML,
   * and U+3000 is the space CJK text uses when it uses one at all.
   */
  return ch == '\t' || ch == '\n' || ch == '\v' || ch == '\f' || ch == '\r' ||
         ch == 0x2028 || ch == 0x2029 ||
         g_unichar_type (ch) == G_UNICODE_SPACE_SEPARATOR ||
         isolated_ideograph (ch);
}

static gboolean
attaches_to_neighbour (gunichar ch)
{
  GUnicodeType type = g_unichar_type (ch);

  return type == G_UNICODE_NON_SPACING_MARK ||
         type == G_UNICODE_SPACING_MARK ||
         type == G_UNICODE_ENCLOSING_MARK ||
         type == G_UNICODE_FORMAT;
}

/* @trailing_edge_is_final says the shaper will not see any of the following
 * text, so only the leading boundary can make the result depend on position.
 */
static gboolean
context_independent (const char *item_text,
                     int         item_length,
                     const char *paragraph_text,
                     int         paragraph_length,
                     gboolean    trailing_edge_is_final)
{
  const char *item_end = item_text + item_length;

  if (item_text > paragraph_text)
    {
      gunichar first = g_utf8_get_char (item_text);

      if (attaches_to_neighbour (first))
        return FALSE;
      if (!boundary_char (first) &&
          !boundary_char (g_utf8_get_char (g_utf8_prev_char (item_text))))
        return FALSE;
    }

  if (!trailing_edge_is_final && item_end < paragraph_text + paragraph_length)
    {
      gunichar last = g_utf8_get_char (g_utf8_prev_char (item_end));

      if (attaches_to_neighbour (last))
        return FALSE;
      if (!boundary_char (last) &&
          !boundary_char (g_utf8_get_char (item_end)))
        return FALSE;
    }

  return TRUE;
}

static guint
key_hash (gconstpointer v)
{
  return ((const NsPangoShapeKey *) v)->hash;
}

static gboolean
key_equal (gconstpointer a,
           gconstpointer b)
{
  const NsPangoShapeKey *ka = a;
  const NsPangoShapeKey *kb = b;

  if (ka->hash != kb->hash ||
      ka->font != kb->font ||
      ka->language != kb->language ||
      ka->level != kb->level ||
      ka->gravity != kb->gravity ||
      ka->script != kb->script ||
      ka->analysis_flags != kb->analysis_flags ||
      ka->shape_flags != kb->shape_flags ||
      ka->show_flags != kb->show_flags ||
      ka->transform != kb->transform ||
      ka->hyphen != kb->hyphen ||
      ka->n_features != kb->n_features ||
      ka->text_length != kb->text_length)
    return FALSE;

  if (ka->n_features > 0 &&
      memcmp (ka->features, kb->features,
              ka->n_features * sizeof (hb_feature_t)) != 0)
    return FALSE;

  return memcmp (ka->text, kb->text, ka->text_length) == 0;
}

static void
key_free (gpointer data)
{
  NsPangoShapeKey *key = data;

  if (key == NULL)
    return;

  g_clear_object (&key->font);
  g_free (key);
}

static gsize
run_size (const CachedRun *run,
          guint32          text_length)
{
  return run->num_glyphs * (sizeof (NsPangoGlyphInfo) + sizeof (int)) +
         sizeof (NsPangoShapeKey) + text_length;
}

static void
run_free (gpointer data)
{
  CachedRun *run = data;

  g_free (run->glyphs);
  g_free (run->log_clusters);
  g_free (run);
}

NsPangoShapeKey *
ns_pango_shape_cache_key_new (const NsPangoAnalysis *analysis,
                              const char            *item_text,
                              int                    item_length,
                              const char            *paragraph_text,
                              int                    paragraph_length,
                              NsPangoShapeFlags      shape_flags,
                              guint                  show_flags,
                              guint                  transform,
                              NsPangoShapeHyphen     hyphen,
                              const hb_feature_t    *features,
                              guint                  n_features)
{
  NsPangoShapeKey *key;
  guint hash;

  if (!ns_pango_shape_cache_enabled ())
    return NULL;

  if (analysis->font == NULL ||
      item_length <= 0 ||
      item_length > NS_SHAPE_CACHE_MAX_TEXT ||
      n_features > G_N_ELEMENTS (key->features))
    {
      if (analysis->font == NULL)
        g_atomic_int_inc (&skip_font);
      else if (item_length <= 0 || item_length > NS_SHAPE_CACHE_MAX_TEXT)
        g_atomic_int_inc (&skip_len);
      else
        g_atomic_int_inc (&skip_feat);

      g_atomic_int_inc (&cache_skips);

      return NULL;
    }

  /* Appending a hyphen clears the post-context HarfBuzz was given, so a
   * hyphenated item is shaped as if it ended the paragraph. That is what makes
   * hyphenation cacheable at all: the break it follows is mid-word, so the
   * trailing boundary test could never pass on its own.
   */
  if (!context_independent (item_text, item_length,
                            paragraph_text, paragraph_length,
                            NS_PANGO_SHAPE_HYPHEN_KIND (hyphen) == NS_PANGO_SHAPE_HYPHEN_UNICODE ||
                            NS_PANGO_SHAPE_HYPHEN_KIND (hyphen) == NS_PANGO_SHAPE_HYPHEN_ASCII))
    {
      g_atomic_int_inc (&skip_ctx);
      g_atomic_int_inc (&cache_skips);
      return NULL;
    }

  key = g_malloc0 (sizeof (NsPangoShapeKey) + item_length);
  key->font = g_object_ref (analysis->font);
  key->language = analysis->language;
  key->level = analysis->level;
  key->gravity = analysis->gravity;
  key->script = analysis->script;
  key->analysis_flags = analysis->flags;
  key->shape_flags = shape_flags;
  key->show_flags = show_flags;
  key->transform = transform;
  key->hyphen = hyphen;
  key->n_features = n_features;
  key->text_length = item_length;
  if (n_features > 0)
    memcpy (key->features, features, n_features * sizeof (hb_feature_t));
  memcpy (key->text, item_text, item_length);

  hash = 5381;
  for (int i = 0; i < item_length; i++)
    hash = hash * 33 + (guchar) key->text[i];
  hash = hash * 33 + GPOINTER_TO_UINT (key->font);
  hash = hash * 33 + GPOINTER_TO_UINT (key->language);
  hash = hash * 33 + key->level;
  hash = hash * 33 + key->gravity;
  hash = hash * 33 + key->script;
  hash = hash * 33 + key->analysis_flags;
  hash = hash * 33 + key->shape_flags;
  hash = hash * 33 + key->show_flags;
  hash = hash * 33 + key->transform;
  hash = hash * 33 + key->hyphen;
  for (guint i = 0; i < n_features; i++)
    hash = hash * 33 + (guint) key->features[i].tag + key->features[i].value;
  key->hash = hash;

  return key;
}

void
ns_pango_shape_cache_key_free (NsPangoShapeKey *key)
{
  key_free (key);
}

static void
copy_run_to_glyphs (const CachedRun    *run,
                    NsPangoGlyphString *glyphs)
{
  ns_pango_glyph_string_set_size (glyphs, run->num_glyphs);
  memcpy (glyphs->glyphs, run->glyphs,
          run->num_glyphs * sizeof (NsPangoGlyphInfo));
  memcpy (glyphs->log_clusters, run->log_clusters,
          run->num_glyphs * sizeof (int));
}

gboolean
ns_pango_shape_cache_lookup (const NsPangoShapeKey *key,
                             NsPangoGlyphString    *glyphs)
{
  CachedRun *run;

  if (key == NULL)
    return FALSE;

  G_LOCK (shape_cache);

  run = shape_cache != NULL ? g_hash_table_lookup (shape_cache, key) : NULL;
  if (run == NULL)
    {
      G_UNLOCK (shape_cache);
      g_atomic_int_inc (&cache_misses);
      return FALSE;
    }

  /* Marking the entry is what lets eviction keep the working set: a sweep drops
   * only what nothing has asked for since the previous sweep.
   */
  run->read = TRUE;
  copy_run_to_glyphs (run, glyphs);

  G_UNLOCK (shape_cache);

  g_atomic_int_inc (&cache_hits);

  return TRUE;
}

gboolean
ns_pango_shape_cache_matches (const NsPangoShapeKey    *key,
                              const NsPangoGlyphString *glyphs)
{
  CachedRun *run;

  if (key == NULL)
    return TRUE;

  G_LOCK (shape_cache);

  run = shape_cache != NULL ? g_hash_table_lookup (shape_cache, key) : NULL;
  if (run == NULL)
    {
      G_UNLOCK (shape_cache);
      return TRUE;
    }

  if (run->num_glyphs != glyphs->num_glyphs)
    {
      G_UNLOCK (shape_cache);
      return FALSE;
    }

  for (int i = 0; i < run->num_glyphs; i++)
    {
      const NsPangoGlyphInfo *a = &run->glyphs[i];
      const NsPangoGlyphInfo *b = &glyphs->glyphs[i];

      if (a->glyph != b->glyph ||
          a->geometry.width != b->geometry.width ||
          a->geometry.x_offset != b->geometry.x_offset ||
          a->geometry.y_offset != b->geometry.y_offset ||
          a->attr.is_cluster_start != b->attr.is_cluster_start ||
          a->attr.is_color != b->attr.is_color ||
          run->log_clusters[i] != glyphs->log_clusters[i])
        {
          G_UNLOCK (shape_cache);
          return FALSE;
        }
    }

  G_UNLOCK (shape_cache);

  return TRUE;
}

/* Both of these run with the lock held. */

static gboolean
drop_unread_entries (void)
{
  GHashTableIter iter;
  gpointer k, v;
  gsize freed = 0;

  g_hash_table_iter_init (&iter, shape_cache);
  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      const NsPangoShapeKey *key = k;
      CachedRun *run = v;

      if (run->read)
        run->read = FALSE;
      else
        {
          freed += run_size (run, key->text_length);
          g_hash_table_iter_remove (&iter);
        }
    }

  cache_bytes -= MIN (freed, cache_bytes);

  return freed > 0;
}

static void
make_room (void)
{
  if (g_hash_table_size (shape_cache) < NS_SHAPE_CACHE_MAX_ENTRIES &&
      cache_bytes < NS_SHAPE_CACHE_MAX_BYTES)
    return;

  /* Dropping the lot is a cliff a browser hits mid-scroll: the next frame
   * reshapes every run on screen. Sweep away what has not been read since the
   * last sweep instead, and clear outright only when that frees nothing --
   * which means the whole cache is the working set and there is nothing better
   * to throw away.
   */
  if (!drop_unread_entries ())
    {
      g_hash_table_remove_all (shape_cache);
      cache_bytes = 0;
    }
}

void
ns_pango_shape_cache_insert (NsPangoShapeKey          *key,
                             const NsPangoGlyphString *glyphs)
{
  CachedRun *run;

  if (key == NULL)
    return;

  if (glyphs->num_glyphs <= 0)
    {
      ns_pango_shape_cache_key_free (key);
      return;
    }

  G_LOCK (shape_cache);

  if (G_UNLIKELY (shape_cache == NULL))
    shape_cache = g_hash_table_new_full (key_hash, key_equal, key_free, run_free);
  else
    make_room ();

  run = g_hash_table_lookup (shape_cache, key);
  if (run != NULL)
    cache_bytes -= run_size (run, key->text_length);

  run = g_new (CachedRun, 1);
  run->num_glyphs = glyphs->num_glyphs;
  run->read = FALSE;
  run->glyphs = g_memdup2 (glyphs->glyphs,
                           glyphs->num_glyphs * sizeof (NsPangoGlyphInfo));
  run->log_clusters = g_memdup2 (glyphs->log_clusters,
                                 glyphs->num_glyphs * sizeof (int));

  cache_bytes += run_size (run, key->text_length);

  g_hash_table_replace (shape_cache, key, run);

  G_UNLOCK (shape_cache);
}

void
ns_pango_cache_clear (void)
{
  G_LOCK (shape_cache);

  if (shape_cache != NULL)
    g_hash_table_remove_all (shape_cache);
  cache_bytes = 0;

  G_UNLOCK (shape_cache);
}

void
ns_pango_shape_cache_font_map_changed (void)
{
  ns_pango_cache_clear ();
}

void
ns_pango_cache_get_stats (guint64 *hits,
                          guint64 *misses,
                          guint64 *skipped,
                          guint64 *entries)
{
  if (hits) *hits = (guint) g_atomic_int_get (&cache_hits);
  if (misses) *misses = (guint) g_atomic_int_get (&cache_misses);
  if (skipped) *skipped = (guint) g_atomic_int_get (&cache_skips);

  if (entries)
    {
      G_LOCK (shape_cache);
      *entries = shape_cache != NULL ? g_hash_table_size (shape_cache) : 0;
      G_UNLOCK (shape_cache);
    }

  if (g_getenv ("NS_PANGO_CACHE_DEBUG") != NULL)
    g_printerr ("[ns-pango] skips: font=%d len=%d features=%d context=%d\n",
                g_atomic_int_get (&skip_font),
                g_atomic_int_get (&skip_len),
                g_atomic_int_get (&skip_feat),
                g_atomic_int_get (&skip_ctx));
}
