/* Pango
 * pangoft2-fontmap.c:
 *
 * Copyright (C) 2000 Red Hat Software
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

#include "config.h"

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <fontconfig/fontconfig.h>

#include "pango-impl-utils.h"
#include "pangoft2-private.h"
#include "pangofc-fontmap.h"

typedef struct _PangoFT2Family       NsPangoFT2Family;
typedef struct _PangoFT2FontMapClass NsPangoFT2FontMapClass;

/**
 * NsPangoFT2FontMap:
 *
 * The `NsPangoFT2FontMap` is the `NsPangoFontMap` implementation for FreeType fonts.
 */
struct _PangoFT2FontMap
{
  NsPangoFcFontMap parent_instance;

  FT_Library library;

  guint serial;
  double dpi_x;
  double dpi_y;

  NsPangoRenderer *renderer;
};

struct _PangoFT2FontMapClass
{
  NsPangoFcFontMapClass parent_class;
};

static void          ns_pango_ft2_font_map_finalize            (GObject              *object);
static NsPangoFcFont * ns_pango_ft2_font_map_new_font            (NsPangoFcFontMap       *fcfontmap,
							     FcPattern            *pattern);
static double        ns_pango_ft2_font_map_get_resolution      (NsPangoFcFontMap       *fcfontmap,
							     NsPangoContext         *context);
static guint         ns_pango_ft2_font_map_get_serial          (NsPangoFontMap         *fontmap);
static void          ns_pango_ft2_font_map_changed             (NsPangoFontMap         *fontmap);

static NsPangoFT2FontMap *ns_pango_ft2_global_fontmap = NULL; /* MT-safe */

G_DEFINE_TYPE (NsPangoFT2FontMap, ns_pango_ft2_font_map, NS_TYPE_PANGO_FC_FONT_MAP)

static void
ns_pango_ft2_font_map_class_init (NsPangoFT2FontMapClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  NsPangoFontMapClass *fontmap_class = NS_PANGO_FONT_MAP_CLASS (class);
  NsPangoFcFontMapClass *fcfontmap_class = NS_PANGO_FC_FONT_MAP_CLASS (class);

  gobject_class->finalize = ns_pango_ft2_font_map_finalize;
  fontmap_class->get_serial = ns_pango_ft2_font_map_get_serial;
  fontmap_class->changed = ns_pango_ft2_font_map_changed;
  fcfontmap_class->default_substitute = _ns_pango_ft2_font_map_default_substitute;
  fcfontmap_class->new_font = ns_pango_ft2_font_map_new_font;
  fcfontmap_class->get_resolution = ns_pango_ft2_font_map_get_resolution;
}

static void
ns_pango_ft2_font_map_init (NsPangoFT2FontMap *fontmap)
{
  FT_Error error;

  fontmap->serial = 1;
  fontmap->library = NULL;
  fontmap->dpi_x   = 72.0;
  fontmap->dpi_y   = 72.0;

  error = FT_Init_FreeType (&fontmap->library);
  if (error != FT_Err_Ok)
    g_critical ("ns_pango_ft2_font_map_init: Could not initialize freetype");
}

static void
ns_pango_ft2_font_map_finalize (GObject *object)
{
  NsPangoFT2FontMap *ft2fontmap = NS_PANGO_FT2_FONT_MAP (object);

  if (ft2fontmap->renderer)
    g_object_unref (ft2fontmap->renderer);

  G_OBJECT_CLASS (ns_pango_ft2_font_map_parent_class)->finalize (object);

  FT_Done_FreeType (ft2fontmap->library);
}

/**
 * ns_pango_ft2_font_map_new:
 *
 * Create a new `NsPangoFT2FontMap` object.
 *
 * A fontmap is used to cache information about available fonts,
 * and holds certain global parameters such as the resolution and
 * the default substitute function (see
 * [method@NsPangoFT2.FontMap.set_default_substitute]).
 *
 * Return value: the newly created fontmap object. Unref
 * with g_object_unref() when you are finished with it.
 *
 * Since: 1.2
 **/
NsPangoFontMap *
ns_pango_ft2_font_map_new (void)
{
  return (NsPangoFontMap *) g_object_new (NS_TYPE_PANGO_FT2_FONT_MAP, NULL);
}

static guint
ns_pango_ft2_font_map_get_serial (NsPangoFontMap *fontmap)
{
  NsPangoFT2FontMap *ft2fontmap = NS_PANGO_FT2_FONT_MAP (fontmap);

  return ft2fontmap->serial;
}

static void
ns_pango_ft2_font_map_changed (NsPangoFontMap *fontmap)
{
  NsPangoFT2FontMap *ft2fontmap = NS_PANGO_FT2_FONT_MAP (fontmap);

  ft2fontmap->serial++;
  if (ft2fontmap->serial == 0)
    ft2fontmap->serial++;
}

/**
 * ns_pango_ft2_font_map_set_default_substitute:
 * @fontmap: a `NsPangoFT2FontMap`
 * @func: function to call to to do final config tweaking
 *        on #FcPattern objects.
 * @data: data to pass to @func
 * @notify: function to call when @data is no longer used.
 *
 * Sets a function that will be called to do final configuration
 * substitution on a `FcPattern` before it is used to load
 * the font.
 *
 * This function can be used to do things like set
 * hinting and antialiasing options.
 *
 * Deprecated: 1.46: Use [method@NsPangoFc.FontMap.set_default_substitute]
 * instead.
 *
 * Since: 1.2
 **/
void
ns_pango_ft2_font_map_set_default_substitute (NsPangoFT2FontMap        *fontmap,
					   NsPangoFT2SubstituteFunc  func,
					   gpointer                data,
					   GDestroyNotify          notify)
{
  NsPangoFcFontMap *fcfontmap = NS_PANGO_FC_FONT_MAP (fontmap);
  ns_pango_fc_font_map_set_default_substitute(fcfontmap, func, data, notify);
}

/**
 * ns_pango_ft2_font_map_substitute_changed:
 * @fontmap: a `NsPangoFT2FontMap`
 *
 * Call this function any time the results of the
 * default substitution function set with
 * ns_pango_ft2_font_map_set_default_substitute() change.
 *
 * That is, if your substitution function will return different
 * results for the same input pattern, you must call this function.
 *
 * Deprecated: 1.46: Use [method@NsPangoFc.FontMap.substitute_changed]
 * instead.
 *
 * Since: 1.2
 **/
void
ns_pango_ft2_font_map_substitute_changed (NsPangoFT2FontMap *fontmap)
{
  ns_pango_fc_font_map_substitute_changed(NS_PANGO_FC_FONT_MAP (fontmap));
}

/**
 * ns_pango_ft2_font_map_set_resolution:
 * @fontmap: a `NsPangoFT2FontMap`
 * @dpi_x: dots per inch in the X direction
 * @dpi_y: dots per inch in the Y direction
 *
 * Sets the horizontal and vertical resolutions for the fontmap.
 *
 * Since: 1.2
 **/
void
ns_pango_ft2_font_map_set_resolution (NsPangoFT2FontMap *fontmap,
				   double           dpi_x,
				   double           dpi_y)
{
  g_return_if_fail (NS_PANGO_FT2_IS_FONT_MAP (fontmap));

  fontmap->dpi_x = dpi_x;
  fontmap->dpi_y = dpi_y;

  ns_pango_ft2_font_map_substitute_changed (fontmap);
}

/**
 * ns_pango_ft2_font_map_create_context: (skip)
 * @fontmap: a `NsPangoFT2FontMap`
 *
 * Create a `NsPangoContext` for the given fontmap.
 *
 * Return value: (transfer full): the newly created context; free with
 *     g_object_unref().
 *
 * Since: 1.2
 *
 * Deprecated: 1.22: Use [method@Pango.FontMap.create_context] instead.
 **/
NsPangoContext *
ns_pango_ft2_font_map_create_context (NsPangoFT2FontMap *fontmap)
{
  g_return_val_if_fail (NS_PANGO_FT2_IS_FONT_MAP (fontmap), NULL);

  return ns_pango_font_map_create_context (NS_PANGO_FONT_MAP (fontmap));
}

/**
 * ns_pango_ft2_get_context: (skip)
 * @dpi_x:  the horizontal DPI of the target device
 * @dpi_y:  the vertical DPI of the target device
 *
 * Retrieves a `NsPangoContext` for the default NsPangoFT2 fontmap
 * (see ns_pango_ft2_font_map_for_display()) and sets the resolution
 * for the default fontmap to @dpi_x by @dpi_y.
 *
 * Return value: (transfer full): the new `NsPangoContext`
 *
 * Deprecated: 1.22: Use [method@Pango.FontMap.create_context] instead.
 **/
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
NsPangoContext *
ns_pango_ft2_get_context (double dpi_x, double dpi_y)
{
  NsPangoFontMap *fontmap;

  fontmap = ns_pango_ft2_font_map_for_display ();
  ns_pango_ft2_font_map_set_resolution (NS_PANGO_FT2_FONT_MAP (fontmap), dpi_x, dpi_y);

  return ns_pango_font_map_create_context (fontmap);
}
G_GNUC_END_IGNORE_DEPRECATIONS

/**
 * ns_pango_ft2_font_map_for_display: (skip)
 *
 * Returns a `NsPangoFT2FontMap`.
 *
 * This font map is cached and should
 * not be freed. If the font map is no longer needed, it can
 * be released with ns_pango_ft2_shutdown_display(). Use of the
 * global NsPangoFT2 fontmap is deprecated; use ns_pango_ft2_font_map_new()
 * instead.
 *
 * Return value: (transfer none): a `NsPangoFT2FontMap`.
 **/
NsPangoFontMap *
ns_pango_ft2_font_map_for_display (void)
{
  if (g_once_init_enter (&ns_pango_ft2_global_fontmap))
    g_once_init_leave (&ns_pango_ft2_global_fontmap, NS_PANGO_FT2_FONT_MAP (ns_pango_ft2_font_map_new ()));

  return NS_PANGO_FONT_MAP (ns_pango_ft2_global_fontmap);
}

/**
 * ns_pango_ft2_shutdown_display:
 *
 * Free the global fontmap. (See ns_pango_ft2_font_map_for_display())
 * Use of the global NsPangoFT2 fontmap is deprecated.
 **/
void
ns_pango_ft2_shutdown_display (void)
{
  if (ns_pango_ft2_global_fontmap)
    {
      ns_pango_fc_font_map_cache_clear (NS_PANGO_FC_FONT_MAP (ns_pango_ft2_global_fontmap));

      g_object_unref (ns_pango_ft2_global_fontmap);

      ns_pango_ft2_global_fontmap = NULL;
    }
}

FT_Library
_ns_pango_ft2_font_map_get_library (NsPangoFontMap *fontmap)
{
  NsPangoFT2FontMap *ft2fontmap = (NsPangoFT2FontMap *)fontmap;

  return ft2fontmap->library;
}


/**
 * _ns_pango_ft2_font_map_get_renderer:
 * @fontmap: a `NsPangoFT2FontMap`
 *
 * Gets the singleton NsPangoFT2Renderer for this fontmap.
 *
 * Return value: the renderer.
 **/
NsPangoRenderer *
_ns_pango_ft2_font_map_get_renderer (NsPangoFT2FontMap *ft2fontmap)
{
  if (!ft2fontmap->renderer)
    ft2fontmap->renderer = g_object_new (NS_TYPE_PANGO_FT2_RENDERER, NULL);

  return ft2fontmap->renderer;
}

void
_ns_pango_ft2_font_map_default_substitute (NsPangoFcFontMap *fcfontmap,
				       FcPattern      *pattern)
{
  NsPangoFT2FontMap *ft2fontmap = NS_PANGO_FT2_FONT_MAP (fcfontmap);
  FcValue v;

  FcConfigSubstitute (ns_pango_fc_font_map_get_config (fcfontmap),
                      pattern, FcMatchPattern);

  if (fcfontmap->substitute_func)
    fcfontmap->substitute_func (pattern, fcfontmap->substitute_data);

  if (FcPatternGet (pattern, FC_DPI, 0, &v) == FcResultNoMatch)
    FcPatternAddDouble (pattern, FC_DPI, ft2fontmap->dpi_y);
  FcConfigSetDefaultSubstitute (ns_pango_fc_font_map_get_config (fcfontmap), pattern);
}

static double
ns_pango_ft2_font_map_get_resolution (NsPangoFcFontMap       *fcfontmap,
				   NsPangoContext         *context G_GNUC_UNUSED)
{
  return ((NsPangoFT2FontMap *)fcfontmap)->dpi_y;
}

static NsPangoFcFont *
ns_pango_ft2_font_map_new_font (NsPangoFcFontMap  *fcfontmap,
			     FcPattern       *pattern)
{
  return (NsPangoFcFont *)_ns_pango_ft2_font_new (NS_PANGO_FT2_FONT_MAP (fcfontmap), pattern);
}
