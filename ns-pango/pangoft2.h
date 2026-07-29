/* Pango
 * pangoft2.h:
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

#ifndef __PANGOFT2_H__
#define __PANGOFT2_H__

#include <fontconfig/fontconfig.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <ns-pango/pangofc-fontmap.h>
#include <ns-pango/pango-layout.h>
#include <ns-pango/pangofc-font.h>

G_BEGIN_DECLS

#ifndef __GI_SCANNER__

#ifndef NS_PANGO_DISABLE_DEPRECATED
/**
 * NS_PANGO_RENDER_TYPE_FT2: (skip)
 *
 * A string constant that was used to identify shape engines that work
 * with the FreeType backend. See %NS_PANGO_RENDER_TYPE_FC for the replacement.
 */
#define NS_PANGO_RENDER_TYPE_FT2 "NsPangoRenderFT2"
#endif

#endif /* __GI_SCANNER__ */

#ifdef __GI_SCANNER__
#define NS_PANGO_FT2_TYPE_FONT_MAP              (ns_pango_ft2_font_map_get_type ())
#define NS_PANGO_FT2_FONT_MAP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), NS_PANGO_FT2_TYPE_FONT_MAP, NsPangoFT2FontMap))
#define NS_PANGO_FT2_IS_FONT_MAP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), NS_PANGO_FT2_TYPE_FONT_MAP))
#else
#define NS_TYPE_PANGO_FT2_FONT_MAP              (ns_pango_ft2_font_map_get_type ())
#define NS_PANGO_FT2_FONT_MAP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), NS_TYPE_PANGO_FT2_FONT_MAP, NsPangoFT2FontMap))
#define NS_PANGO_FT2_IS_FONT_MAP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), NS_TYPE_PANGO_FT2_FONT_MAP))
#endif

typedef struct _PangoFT2FontMap      NsPangoFT2FontMap;

/**
 * NsPangoFT2SubstituteFunc:
 * @pattern: the FcPattern to tweak.
 * @data: user data.
 *
 * Function type for doing final config tweaking on prepared FcPatterns.
 */
typedef void (*NsPangoFT2SubstituteFunc) (FcPattern *pattern,
				        gpointer   data);

/* Calls for applications */

NS_PANGO_AVAILABLE_IN_ALL
void ns_pango_ft2_render             (FT_Bitmap         *bitmap,
				   NsPangoFont         *font,
				   NsPangoGlyphString  *glyphs,
				   gint               x,
				   gint               y);
NS_PANGO_AVAILABLE_IN_1_6
void ns_pango_ft2_render_transformed (FT_Bitmap         *bitmap,
				   const NsPangoMatrix *matrix,
				   NsPangoFont         *font,
				   NsPangoGlyphString  *glyphs,
				   int                x,
				   int                y);

NS_PANGO_AVAILABLE_IN_ALL
void ns_pango_ft2_render_layout_line          (FT_Bitmap        *bitmap,
					    NsPangoLayoutLine  *line,
					    int               x,
					    int               y);
NS_PANGO_AVAILABLE_IN_1_6
void ns_pango_ft2_render_layout_line_subpixel (FT_Bitmap        *bitmap,
					    NsPangoLayoutLine  *line,
					    int               x,
					    int               y);
NS_PANGO_AVAILABLE_IN_ALL
void ns_pango_ft2_render_layout               (FT_Bitmap        *bitmap,
					    NsPangoLayout      *layout,
					    int               x,
					    int               y);
NS_PANGO_AVAILABLE_IN_1_6
void ns_pango_ft2_render_layout_subpixel      (FT_Bitmap        *bitmap,
					    NsPangoLayout      *layout,
					    int               x,
					    int               y);

NS_PANGO_AVAILABLE_IN_ALL
GType ns_pango_ft2_font_map_get_type (void) G_GNUC_CONST;

NS_PANGO_AVAILABLE_IN_1_2
NsPangoFontMap *ns_pango_ft2_font_map_new                    (void);
NS_PANGO_AVAILABLE_IN_1_2
void          ns_pango_ft2_font_map_set_resolution         (NsPangoFT2FontMap        *fontmap,
							 double                  dpi_x,
							 double                  dpi_y);
#ifndef NS_PANGO_DISABLE_DEPRECATED
NS_PANGO_DEPRECATED_IN_1_48_FOR(ns_pango_fc_font_map_set_default_substitute)
void          ns_pango_ft2_font_map_set_default_substitute (NsPangoFT2FontMap        *fontmap,
							 NsPangoFT2SubstituteFunc  func,
							 gpointer                data,
							 GDestroyNotify          notify);
NS_PANGO_DEPRECATED_IN_1_48_FOR(ns_pango_fc_font_map_substitute_changed)
void          ns_pango_ft2_font_map_substitute_changed     (NsPangoFT2FontMap         *fontmap);
NS_PANGO_DEPRECATED_IN_1_22_FOR(ns_pango_font_map_create_context)
NsPangoContext *ns_pango_ft2_font_map_create_context         (NsPangoFT2FontMap         *fontmap);
#endif


/* API for rendering modules
 */
#ifndef NS_PANGO_DISABLE_DEPRECATED
NS_PANGO_DEPRECATED_FOR(ns_pango_font_map_create_context)
NsPangoContext      *ns_pango_ft2_get_context          (double dpi_x,
						   double dpi_y);
NS_PANGO_DEPRECATED_FOR(ns_pango_ft2_font_map_new)
NsPangoFontMap      *ns_pango_ft2_font_map_for_display (void);
NS_PANGO_DEPRECATED
void               ns_pango_ft2_shutdown_display     (void);

NS_PANGO_DEPRECATED_FOR(NS_PANGO_GET_UNKNOWN_GLYPH)
NsPangoGlyph     ns_pango_ft2_get_unknown_glyph (NsPangoFont       *font);
NS_PANGO_DEPRECATED_FOR(ns_pango_fc_font_kern_glyphs)
int            ns_pango_ft2_font_get_kerning  (NsPangoFont       *font,
					    NsPangoGlyph       left,
					    NsPangoGlyph       right);
NS_PANGO_DEPRECATED_FOR(ns_pango_fc_font_lock_face)
FT_Face        ns_pango_ft2_font_get_face     (NsPangoFont       *font);
NS_PANGO_DEPRECATED_FOR(ns_pango_font_get_coverage)
NsPangoCoverage *ns_pango_ft2_font_get_coverage (NsPangoFont       *font,
					    NsPangoLanguage   *language);
#endif /* NS_PANGO_DISABLE_DEPRECATED */

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NsPangoFT2FontMap, g_object_unref)

G_END_DECLS

#endif /* __PANGOFT2_H__ */
