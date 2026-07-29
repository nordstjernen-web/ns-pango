/* Pango
 * pangoft2-private.h:
 *
 * Copyright (C) 1999 Red Hat Software
 * Copyright (C) 2000 Tor Lillqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __PANGOFT2_PRIVATE_H__
#define __PANGOFT2_PRIVATE_H__

#include <ns-pango/pangoft2.h>
#include <ns-pango/pangofc-fontmap-private.h>
#include <ns-pango/pango-renderer.h>
#include <fontconfig/fontconfig.h>

/* Debugging... */
/*#define DEBUGGING 1*/

#if defined(DEBUGGING) && DEBUGGING
#ifdef __GNUC__
#define PING(printlist)					\
(g_print ("%s:%d ", __PRETTY_FUNCTION__, __LINE__),	\
 g_print printlist,					\
 g_print ("\n"))
#else
#define PING(printlist)					\
(g_print ("%s:%d ", __FILE__, __LINE__),		\
 g_print printlist,					\
 g_print ("\n"))
#endif
#else  /* !DEBUGGING */
#define PING(printlist)
#endif

typedef struct _PangoFT2Font      NsPangoFT2Font;
typedef struct _PangoFT2GlyphInfo NsPangoFT2GlyphInfo;
typedef struct _PangoFT2Renderer  NsPangoFT2Renderer;

struct _PangoFT2Font
{
  NsPangoFcFont font;

  FT_Face face;
  int load_flags;

  int size;

  GSList *metrics_by_lang;

  GHashTable *glyph_info;
  GDestroyNotify glyph_cache_destroy;
};

struct _PangoFT2GlyphInfo
{
  NsPangoRectangle logical_rect;
  NsPangoRectangle ink_rect;
  void *cached_glyph;
};

#define NS_TYPE_PANGO_FT2_FONT              (ns_pango_ft2_font_get_type ())
#define NS_PANGO_FT2_FONT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), NS_TYPE_PANGO_FT2_FONT, NsPangoFT2Font))
#define NS_PANGO_FT2_IS_FONT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), NS_TYPE_PANGO_FT2_FONT))

_PANGO_EXTERN
GType ns_pango_ft2_font_get_type (void) G_GNUC_CONST;

NsPangoFT2Font * _ns_pango_ft2_font_new                (NsPangoFT2FontMap   *ft2fontmap,
						   FcPattern         *pattern);
FT_Library     _ns_pango_ft2_font_map_get_library    (NsPangoFontMap      *fontmap);
void _ns_pango_ft2_font_map_default_substitute (NsPangoFcFontMap *fcfontmap,
					     FcPattern      *pattern);

void *_ns_pango_ft2_font_get_cache_glyph_data    (NsPangoFont      *font,
					       int             glyph_index);
void  _ns_pango_ft2_font_set_cache_glyph_data    (NsPangoFont      *font,
					       int             glyph_index,
					       void           *cached_glyph);
void  _ns_pango_ft2_font_set_glyph_cache_destroy (NsPangoFont      *font,
					       GDestroyNotify  destroy_notify);

#define NS_TYPE_PANGO_FT2_RENDERER            (ns_pango_ft2_renderer_get_type())
#define NS_PANGO_FT2_RENDERER(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), NS_TYPE_PANGO_FT2_RENDERER, NsPangoFT2Renderer))
#define NS_PANGO_IS_FT2_RENDERER(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), NS_TYPE_PANGO_FT2_RENDERER))

_PANGO_EXTERN
GType ns_pango_ft2_renderer_get_type    (void) G_GNUC_CONST;

NsPangoRenderer *_ns_pango_ft2_font_map_get_renderer (NsPangoFT2FontMap *ft2fontmap);

#endif /* __PANGOFT2_PRIVATE_H__ */
