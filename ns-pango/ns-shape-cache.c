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
#include "ns-break-cache.h"
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
 * joins nor ligates: most space characters, or an ideograph, kana letter or
 * Hangul syllable, which are written without spaces and never join. See
 * boundary_char() for what is deliberately left out of both of those.
 */

#define NS_SHAPE_CACHE_MAX_ENTRIES 20000
#define NS_SHAPE_CACHE_MAX_TEXT    4096
#define NS_SHAPE_CACHE_MAX_BYTES   (48 * 1024 * 1024)

/* One table behind one lock does not survive being shared. Every thread gets
 * its own fontmap, because the fontconfig fontmap's caches are unlocked, so it
 * gets its own NsPangoFont objects too -- and the key names the font, so
 * nothing one thread shapes can be served to another. Threads therefore all
 * miss, all take the lock exclusively to insert, and all serialise behind each
 * other's memcpy: measured over the corpus on four cores, four threads with one
 * table ran no faster than four threads with the cache switched off entirely.
 *
 * Splitting the table on the high bits of the hash, which the table's own
 * bucket index does not use, gives each shard its own lock. Two threads
 * inserting different runs now only collide one time in NS_SHAPE_CACHE_SHARDS.
 */
#define NS_SHAPE_CACHE_SHARDS      16
#define NS_SHAPE_CACHE_SHARD(hash) (((hash) >> 27) & (NS_SHAPE_CACHE_SHARDS - 1))

typedef struct
{
  int               num_glyphs;
  gint              read;      /* set by readers, so atomic */
  NsPangoGlyphInfo *glyphs;
  int              *log_clusters;
} CachedRun;

/* Once the cache is warm almost every visit is a lookup, and a lookup only
 * reads the table -- so readers run concurrently and only the writers, which are
 * the inserts and the eviction sweeps, take a shard exclusively. With one
 * plain mutex the memcpy of every cached glyph string was serialised across all
 * the threads a browser shapes on.
 */
typedef struct
{
  GRWLock     lock;
  GHashTable *table;
  gsize       bytes;
} ShapeShard;

static ShapeShard shards[NS_SHAPE_CACHE_SHARDS];

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
  /* Two space characters are not boundaries, for the same reason CJK punctuation
   * is not: U+3000 IDEOGRAPHIC SPACE is what a CJK font's contextual spacing
   * features compress against a neighbour, and U+1680 OGHAM SPACE MARK draws a
   * visible stem that Ogham fonts join to the letters either side of it.
   */
  if (ch == 0x3000 || ch == 0x1680)
    return FALSE;

  /* Any other space character, not only ASCII space: &nbsp; is everywhere in
   * HTML, and the fixed-width spaces are common in typeset Latin text.
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

gboolean
ns_pango_shape_cache_key_init (NsPangoShapeKey       *key,
                               const NsPangoAnalysis *analysis,
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
  guint hash;

  if (!ns_pango_shape_cache_enabled ())
    return FALSE;

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

      return FALSE;
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
      return FALSE;
    }

  key->font = analysis->font;
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
  key->text = item_text;
  if (n_features > 0)
    memcpy (key->features, features, n_features * sizeof (hb_feature_t));

  hash = 5381;
  for (int i = 0; i < item_length; i++)
    hash = hash * 33 + (guchar) item_text[i];
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

  return TRUE;
}

/* The table has to own both its key's text and a reference on the font the key
 * names, neither of which a stack key carries.
 */
static NsPangoShapeKey *
key_dup_owned (const NsPangoShapeKey *key)
{
  NsPangoShapeKey *owned = g_malloc (sizeof (NsPangoShapeKey) + key->text_length);

  *owned = *key;
  memcpy (owned->owned_text, key->text, key->text_length);
  owned->text = owned->owned_text;
  owned->font = g_object_ref (key->font);

  return owned;
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
  ShapeShard *shard = &shards[NS_SHAPE_CACHE_SHARD (key->hash)];
  CachedRun *run;

  g_rw_lock_reader_lock (&shard->lock);

  run = shard->table != NULL ? g_hash_table_lookup (shard->table, key) : NULL;
  if (run == NULL)
    {
      g_rw_lock_reader_unlock (&shard->lock);
      g_atomic_int_inc (&cache_misses);
      return FALSE;
    }

  /* Marking the entry is what lets eviction keep the working set: a sweep drops
   * only what nothing has asked for since the previous sweep. Several readers
   * can be marking at once, hence the atomic.
   */
  g_atomic_int_set (&run->read, TRUE);
  copy_run_to_glyphs (run, glyphs);

  g_rw_lock_reader_unlock (&shard->lock);

  g_atomic_int_inc (&cache_hits);

  return TRUE;
}

gboolean
ns_pango_shape_cache_matches (const NsPangoShapeKey    *key,
                              const NsPangoGlyphString *glyphs)
{
  ShapeShard *shard = &shards[NS_SHAPE_CACHE_SHARD (key->hash)];
  CachedRun *run;
  gboolean same = TRUE;

  g_rw_lock_reader_lock (&shard->lock);

  run = shard->table != NULL ? g_hash_table_lookup (shard->table, key) : NULL;
  if (run != NULL && run->num_glyphs != glyphs->num_glyphs)
    same = FALSE;

  for (int i = 0; run != NULL && same && i < run->num_glyphs; i++)
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
        same = FALSE;
    }

  g_rw_lock_reader_unlock (&shard->lock);

  return same;
}

/* Both of these run with the shard held for writing. */

static void
drop_unread_entries (ShapeShard *shard)
{
  GHashTableIter iter;
  gpointer k, v;
  gsize freed = 0;

  g_hash_table_iter_init (&iter, shard->table);
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

  shard->bytes -= MIN (freed, shard->bytes);
}

static void
make_room (ShapeShard *shard)
{
  if (g_hash_table_size (shard->table) < NS_SHAPE_CACHE_MAX_ENTRIES / NS_SHAPE_CACHE_SHARDS &&
      shard->bytes < NS_SHAPE_CACHE_MAX_BYTES / NS_SHAPE_CACHE_SHARDS)
    return;

  /* Dropping the lot is a cliff a browser hits mid-scroll: the next frame
   * reshapes every run on screen. Sweep away what has not been read since the
   * last sweep instead.
   */
  drop_unread_entries (shard);

  /* A sweep walks the whole shard, so it has to buy enough room to be worth
   * repeating -- otherwise a working set that fills the cache would sweep once
   * per insert. Getting under half the ceiling leaves at least that many inserts
   * before the next sweep; failing that, the working set really is the whole
   * cache and there is nothing better to throw away than all of it.
   */
  if (g_hash_table_size (shard->table) >= NS_SHAPE_CACHE_MAX_ENTRIES / NS_SHAPE_CACHE_SHARDS / 2 ||
      shard->bytes >= NS_SHAPE_CACHE_MAX_BYTES / NS_SHAPE_CACHE_SHARDS / 2)
    {
      g_hash_table_remove_all (shard->table);
      shard->bytes = 0;
    }
}

void
ns_pango_shape_cache_insert (const NsPangoShapeKey    *key,
                             const NsPangoGlyphString *glyphs)
{
  ShapeShard *shard = &shards[NS_SHAPE_CACHE_SHARD (key->hash)];
  CachedRun *run;

  if (glyphs->num_glyphs <= 0)
    return;

  /* Copying the glyphs before taking the lock keeps two allocations and two
   * memcpys of every shaped run out of the shard's exclusive section.
   */
  run = g_new (CachedRun, 1);
  run->num_glyphs = glyphs->num_glyphs;
  run->read = FALSE;
  run->glyphs = g_memdup2 (glyphs->glyphs,
                           glyphs->num_glyphs * sizeof (NsPangoGlyphInfo));
  run->log_clusters = g_memdup2 (glyphs->log_clusters,
                                 glyphs->num_glyphs * sizeof (int));

  g_rw_lock_writer_lock (&shard->lock);

  if (G_UNLIKELY (shard->table == NULL))
    shard->table = g_hash_table_new_full (key_hash, key_equal, key_free, run_free);
  else
    make_room (shard);

  {
    const CachedRun *old = g_hash_table_lookup (shard->table, key);

    if (old != NULL)
      shard->bytes -= MIN (run_size (old, key->text_length), shard->bytes);
  }

  shard->bytes += run_size (run, key->text_length);

  g_hash_table_replace (shard->table, key_dup_owned (key), run);

  g_rw_lock_writer_unlock (&shard->lock);
}

/* Break attributes are a function of the text alone, so a new font invalidates
 * the glyphs and nothing else -- which is why only the caller who wants
 * everything gone gets everything gone.
 */
static void
drop_shaped_runs (void)
{
  for (guint i = 0; i < NS_SHAPE_CACHE_SHARDS; i++)
    {
      g_rw_lock_writer_lock (&shards[i].lock);

      if (shards[i].table != NULL)
        g_hash_table_remove_all (shards[i].table);
      shards[i].bytes = 0;

      g_rw_lock_writer_unlock (&shards[i].lock);
    }
}

void
ns_pango_cache_clear (void)
{
  drop_shaped_runs ();
  ns_pango_break_cache_clear ();
}

void
ns_pango_shape_cache_font_map_changed (void)
{
  drop_shaped_runs ();
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
      *entries = 0;

      for (guint i = 0; i < NS_SHAPE_CACHE_SHARDS; i++)
        {
          g_rw_lock_reader_lock (&shards[i].lock);
          if (shards[i].table != NULL)
            *entries += g_hash_table_size (shards[i].table);
          g_rw_lock_reader_unlock (&shards[i].lock);
        }
    }

  if (g_getenv ("NS_PANGO_CACHE_DEBUG") != NULL)
    g_printerr ("[ns-pango] skips: font=%d len=%d features=%d context=%d\n",
                g_atomic_int_get (&skip_font),
                g_atomic_int_get (&skip_len),
                g_atomic_int_get (&skip_feat),
                g_atomic_int_get (&skip_ctx));
}
