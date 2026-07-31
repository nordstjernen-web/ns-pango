/* ns-pango
 * ns-shape-cache.h: Cross-layout cache of shaped runs.
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

#ifndef __NS_SHAPE_CACHE_H__
#define __NS_SHAPE_CACHE_H__

#include <ns-pango/pango-item.h>
#include <ns-pango/pango-glyph.h>
#include <hb.h>

G_BEGIN_DECLS

/* The key is laid out here rather than hidden in the cache so that a lookup can
 * build one on the stack. Shaping asks the cache far more often than it fills
 * it, and a heap key per question cost a malloc, a free and a pair of atomic
 * refcount updates on the font for every hit.
 *
 * @text points at the caller's item text in a key built to ask a question, and
 * at @owned_text in the copy the table keeps.
 */
typedef struct
{
  guint        hash;
  gpointer     font;
  gpointer     language;
  guint32      level;
  guint32      gravity;
  guint32      script;
  guint32      analysis_flags;
  guint32      shape_flags;
  guint32      show_flags;
  guint32      transform;
  guint32      hyphen;
  guint32      n_features;
  guint32      text_length;
  const char  *text;
  hb_feature_t features[8];
  char         owned_text[];
} NsPangoShapeKey;

/* What shaping will do to a hyphenated item: which hyphen it appends, if the
 * font has one, and whether it drops the character before the break. Both
 * change the glyphs, so both belong in the cache key; and appending a hyphen
 * also hides the text after the item from the shaper, which is what makes a
 * mid-word break cacheable at all.
 */
typedef enum
{
  NS_PANGO_SHAPE_HYPHEN_NONE    = 0,
  NS_PANGO_SHAPE_HYPHEN_UNICODE = 1,
  NS_PANGO_SHAPE_HYPHEN_ASCII   = 2,
  NS_PANGO_SHAPE_HYPHEN_MISSING = 3,

  /* Or'd onto one of the above when the character before the break goes away. */
  NS_PANGO_SHAPE_HYPHEN_TRIMMED = 4,
} NsPangoShapeHyphen;

#define NS_PANGO_SHAPE_HYPHEN_KIND(h) ((h) & 3)

gboolean          ns_pango_shape_cache_enabled   (void);

gboolean          ns_pango_shape_cache_verifying (void);

gboolean          ns_pango_shape_cache_key_init  (NsPangoShapeKey       *key,
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
                                                  guint                  n_features);

gboolean          ns_pango_shape_cache_lookup    (const NsPangoShapeKey *key,
                                                  NsPangoGlyphString    *glyphs);

void              ns_pango_shape_cache_insert    (const NsPangoShapeKey    *key,
                                                  const NsPangoGlyphString *glyphs);

void              ns_pango_shape_cache_font_map_changed (void);

gboolean          ns_pango_shape_cache_matches   (const NsPangoShapeKey    *key,
                                                  const NsPangoGlyphString *glyphs);

G_END_DECLS

#endif /* __NS_SHAPE_CACHE_H__ */
