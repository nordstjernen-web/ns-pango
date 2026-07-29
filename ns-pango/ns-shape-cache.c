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
 * HarfBuzz is given the paragraph as context, so an item that starts or ends
 * mid-word may join, reorder or ligate across its own boundary. Requiring
 * whitespace (or the paragraph edge) on both sides makes the cached result
 * independent of where the item sits.
 */

#define NS_SHAPE_CACHE_MAX_ENTRIES 20000
#define NS_SHAPE_CACHE_MAX_TEXT    512

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
  guint32    n_features;
  guint32    text_length;
  hb_feature_t features[8];
  char       text[1];
};

typedef struct
{
  int              num_glyphs;
  NsPangoGlyphInfo *glyphs;
  int             *log_clusters;
} CachedRun;

static GHashTable *shape_cache;
static guint64 cache_hits;
static guint64 cache_misses;
static guint64 cache_skips;

static gboolean
enabled_from_env (void)
{
  const char *v = g_getenv ("NS_PANGO_SHAPE_CACHE");

  return !(v && (strcmp (v, "0") == 0 || g_ascii_strcasecmp (v, "off") == 0));
}

gboolean
ns_pango_shape_cache_enabled (void)
{
  static int enabled = -1;

  if (G_UNLIKELY (enabled < 0))
    enabled = enabled_from_env () ? 1 : 0;

  return enabled == 1;
}

gboolean
ns_pango_shape_cache_verifying (void)
{
  static int verifying = -1;

  if (G_UNLIKELY (verifying < 0))
    {
      const char *v = g_getenv ("NS_PANGO_SHAPE_CACHE");

      verifying = (v && g_ascii_strcasecmp (v, "verify") == 0) ? 1 : 0;
    }

  return verifying == 1;
}

static gboolean
boundary_char (gunichar ch)
{
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static gboolean
attaches_to_neighbour (gunichar ch)
{
  switch (g_unichar_type (ch))
    {
    case G_UNICODE_NON_SPACING_MARK:
    case G_UNICODE_SPACING_MARK:
    case G_UNICODE_ENCLOSING_MARK:
    case G_UNICODE_FORMAT:
      return TRUE;
    default:
      return FALSE;
    }
}

static gboolean
context_independent (const char *item_text,
                     int         item_length,
                     const char *paragraph_text,
                     int         paragraph_length)
{
  const char *item_end = item_text + item_length;

  if (item_text > paragraph_text)
    {
      const char *prev = g_utf8_prev_char (item_text);

      if (!boundary_char (g_utf8_get_char (prev)))
        return FALSE;
      if (attaches_to_neighbour (g_utf8_get_char (item_text)))
        return FALSE;
    }

  if (item_end < paragraph_text + paragraph_length)
    {
      if (!boundary_char (g_utf8_get_char (item_end)))
        return FALSE;
      if (attaches_to_neighbour (g_utf8_get_char (g_utf8_prev_char (item_end))))
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
      n_features > G_N_ELEMENTS (key->features) ||
      (analysis->flags & NS_PANGO_ANALYSIS_FLAG_NEED_HYPHEN) != 0)
    {
      cache_skips++;
      return NULL;
    }

  if (!context_independent (item_text, item_length,
                            paragraph_text, paragraph_length))
    {
      cache_skips++;
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
  key->n_features = n_features;
  key->text_length = item_length;
  if (n_features > 0)
    memcpy (key->features, features, n_features * sizeof (hb_feature_t));
  memcpy (key->text, item_text, item_length);

  hash = g_str_hash (""); /* 5381 */
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

  if (key == NULL || shape_cache == NULL)
    return FALSE;

  run = g_hash_table_lookup (shape_cache, key);
  if (run == NULL)
    {
      cache_misses++;
      return FALSE;
    }

  cache_hits++;
  copy_run_to_glyphs (run, glyphs);

  return TRUE;
}

gboolean
ns_pango_shape_cache_matches (const NsPangoShapeKey    *key,
                              const NsPangoGlyphString *glyphs)
{
  CachedRun *run;

  if (key == NULL || shape_cache == NULL)
    return TRUE;

  run = g_hash_table_lookup (shape_cache, key);
  if (run == NULL)
    return TRUE;

  if (run->num_glyphs != glyphs->num_glyphs)
    return FALSE;

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
        return FALSE;
    }

  return TRUE;
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

  if (G_UNLIKELY (shape_cache == NULL))
    shape_cache = g_hash_table_new_full (key_hash, key_equal, key_free, run_free);
  else if (g_hash_table_size (shape_cache) >= NS_SHAPE_CACHE_MAX_ENTRIES)
    g_hash_table_remove_all (shape_cache);

  run = g_new (CachedRun, 1);
  run->num_glyphs = glyphs->num_glyphs;
  run->glyphs = g_memdup2 (glyphs->glyphs,
                           glyphs->num_glyphs * sizeof (NsPangoGlyphInfo));
  run->log_clusters = g_memdup2 (glyphs->log_clusters,
                                 glyphs->num_glyphs * sizeof (int));

  g_hash_table_replace (shape_cache, key, run);
}

void
ns_pango_cache_clear (void)
{
  if (shape_cache != NULL)
    g_hash_table_remove_all (shape_cache);
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
  if (hits) *hits = cache_hits;
  if (misses) *misses = cache_misses;
  if (skipped) *skipped = cache_skips;
  if (entries) *entries = shape_cache ? g_hash_table_size (shape_cache) : 0;
}
