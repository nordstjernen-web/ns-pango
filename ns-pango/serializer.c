/* Pango
 * serializer.c: Code to serialize various Pango objects
 *
 * Copyright (C) 2021 Red Hat, Inc
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

#include <ns-pango/pango-layout.h>
#include <ns-pango/pango-layout-private.h>
#include <ns-pango/pango-context-private.h>
#include <ns-pango/pango-enum-types.h>
#include <ns-pango/pango-font-private.h>

#include <hb-ot.h>
#include "ns-pango/json/gtkjsonparserprivate.h"
#include "ns-pango/json/gtkjsonprinterprivate.h"

/* {{{ Error handling */

G_DEFINE_QUARK(pango-layout-deserialize-error-quark, ns_pango_layout_deserialize_error)

/* }}} */
/* {{{ Enum names */

static const char *style_names[] = {
  "normal",
  "oblique",
  "italic",
  NULL
};

static const char *variant_names[] = {
  "normal",
  "small-caps",
  "all-small-caps",
  "petite-caps",
  "all-petite-caps",
  "unicase",
  "titlecase",
  NULL
};

static const char *stretch_names[] = {
  "ultra-condensed",
  "extra-condensed",
  "condensed",
  "semi-condensed",
  "normal",
  "semi-expanded",
  "expanded",
  "extra-expanded",
  "ultra-expanded",
  NULL
};

static const char *underline_names[] = {
  "none",
  "single",
  "double",
  "low",
  "error",
  "single-line",
  "double-line",
  "error-line",
  NULL
};

static const char *overline_names[] = {
  "none",
  "single",
  NULL
};

static const char *gravity_names[] = {
  "south",
  "east",
  "north",
  "west",
  "auto",
  NULL
};

static const char *gravity_hint_names[] = {
  "natural",
  "strong",
  "line",
  NULL
};

static const char *text_transform_names[] = {
  "none",
  "lowercase",
  "uppercase",
  "capitalize",
  NULL
};

static const char *baseline_shift_names[] = {
  "none",
  "superscript",
  "subscript",
  NULL
};

static const char *font_scale_names[] = {
  "none",
  "superscript",
  "subscript",
  "small-caps",
  NULL
};

static const char *weight_names[] = {
  "thin",
  "ultralight",
  "light",
  "semilight",
  "book",
  "normal",
  "medium",
  "semibold",
  "bold",
  "ultrabold",
  "heavy",
  "ultraheavy",
  NULL
};

static int named_weights[] = { 100, 200, 300, 350, 380, 400, 500, 600, 700, 800, 900, 1000 };

static int
get_weight (int pos)
{
  return named_weights[pos];
}

static const char *
get_weight_name (int weight)
{
  for (int i = 0; i < G_N_ELEMENTS (named_weights); i++)
    {
      if (named_weights[i] == weight)
        return weight_names[i];
    }

  return NULL;
}

static const char *width_names[] = {
  "ultracondensed",
  "extracondensed",
  "condensed",
  "semicondensed",
  "normal",
  "semiexpanded",
  "expanded",
  "extraexpanded",
  "ultraexpanded",
};

static int named_widths[] = {
  500, 625, 750, 875, 1000, 1125, 1250, 1500, 2000
};

static int
get_width (int pos)
{
  return named_widths[pos];
}

static const char *
get_width_name (int width)
{
  for (int i = 0; i < G_N_ELEMENTS (named_widths); i++)
    {
      if (named_widths[i] == width)
        return width_names[i];
    }

  return NULL;
}

static const char *attr_type_names[] = {
  "invalid",
  "language",
  "family",
  "style",
  "weight",
  "variant",
  "stretch",
  "size",
  "font-desc",
  "foreground",
  "background",
  "underline",
  "strikethrough",
  "rise",
  "shape",
  "scale",
  "fallback",
  "letter-spacing",
  "underline-color",
  "strikethrough-color",
  "absolute-size",
  "gravity",
  "gravity-hint",
  "font-features",
  "foreground-alpha",
  "background-alpha",
  "allow-breaks",
  "show",
  "insert-hyphens",
  "overline",
  "overline-color",
  "line-height",
  "absolute-line-height",
  "text-transform",
  "word",
  "sentence",
  "baseline-shift",
  "font-scale",
  "width",
  NULL
};

static const char *
get_script_name (NsPangoScript script)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (NS_TYPE_PANGO_SCRIPT);
  enum_value = g_enum_get_value (enum_class, script);
  g_type_class_unref (enum_class);

  if (enum_value)
    return enum_value->value_nick;

  return NULL;
}

static const char *tab_align_names[] = {
  "left",
  "right",
  "center",
  "decimal",
  NULL
};

static const char *direction_names[] = {
  "ltr",
  "rtl",
  "ttb-ltr",
  "ttb-rtl",
  "weak-ltr",
  "weak-rtl",
  "neutral",
  NULL
};

static const char *alignment_names[] = {
  "left",
  "center",
  "right",
  NULL
};

static const char *wrap_names[] = {
  "word",
  "char",
  "word-char",
  "none",
  NULL
};

static const char *ellipsize_names[] = {
  "none",
  "start",
  "middle",
  "end",
  NULL
};

/* }}} */
/* {{{ Serialization */

static void
add_attribute (GtkJsonPrinter *printer,
               NsPangoAttribute *attr)
{
  char *str;

  gtk_json_printer_start_object (printer, NULL);

  if (attr->start_index != NS_PANGO_ATTR_INDEX_FROM_TEXT_BEGINNING)
    gtk_json_printer_add_integer (printer, "start", (int)attr->start_index);
  if (attr->end_index != NS_PANGO_ATTR_INDEX_TO_TEXT_END)
    gtk_json_printer_add_integer (printer, "end", (int)attr->end_index);
  gtk_json_printer_add_string (printer, "type", attr_type_names[attr->klass->type]);

  switch (attr->klass->type)
    {
    default:
    case NS_PANGO_ATTR_INVALID:
      g_assert_not_reached ();
    case NS_PANGO_ATTR_LANGUAGE:
      gtk_json_printer_add_string (printer, "value", ns_pango_language_to_string (((NsPangoAttrLanguage*)attr)->value));
      break;
    case NS_PANGO_ATTR_FAMILY:
    case NS_PANGO_ATTR_FONT_FEATURES:
      gtk_json_printer_add_string (printer, "value", ((NsPangoAttrString*)attr)->value);
      break;
    case NS_PANGO_ATTR_STYLE:
      gtk_json_printer_add_string (printer, "value", style_names[((NsPangoAttrInt*)attr)->value]);
      break;

    case NS_PANGO_ATTR_VARIANT:
      gtk_json_printer_add_string (printer, "value", variant_names[((NsPangoAttrInt*)attr)->value]);
      break;

    case NS_PANGO_ATTR_STRETCH:
      gtk_json_printer_add_string (printer, "value", stretch_names[((NsPangoAttrInt*)attr)->value]);
      break;

    case NS_PANGO_ATTR_WIDTH:
      {
        const char *name = get_width_name (((NsPangoAttrInt*)attr)->value);
        if (name)
          gtk_json_printer_add_string (printer, "value", name);
        else
          gtk_json_printer_add_integer (printer, "value", ((NsPangoAttrInt*)attr)->value);
      }
      break;

    case NS_PANGO_ATTR_UNDERLINE:
      gtk_json_printer_add_string (printer, "value", underline_names[((NsPangoAttrInt*)attr)->value]);
      break;

    case NS_PANGO_ATTR_OVERLINE:
      gtk_json_printer_add_string (printer, "value", overline_names[((NsPangoAttrInt*)attr)->value]);
      break;

    case NS_PANGO_ATTR_GRAVITY:
      gtk_json_printer_add_string (printer, "value", gravity_names[((NsPangoAttrInt*)attr)->value]);
      break;

    case NS_PANGO_ATTR_GRAVITY_HINT:
      gtk_json_printer_add_string (printer, "value", gravity_hint_names[((NsPangoAttrInt*)attr)->value]);
      break;

    case NS_PANGO_ATTR_TEXT_TRANSFORM:
      gtk_json_printer_add_string (printer, "value", text_transform_names[((NsPangoAttrInt*)attr)->value]);
      break;

    case NS_PANGO_ATTR_FONT_SCALE:
      gtk_json_printer_add_string (printer, "value", font_scale_names[((NsPangoAttrInt*)attr)->value]);
      break;

    case NS_PANGO_ATTR_WEIGHT:
      {
        const char *name = get_weight_name (((NsPangoAttrInt*)attr)->value);
        if (name)
          gtk_json_printer_add_string (printer, "value", name);
        else
          gtk_json_printer_add_integer (printer, "value", ((NsPangoAttrInt*)attr)->value);
      }
      break;

    case NS_PANGO_ATTR_BASELINE_SHIFT:
      gtk_json_printer_add_string (printer, "value", baseline_shift_names[((NsPangoAttrInt*)attr)->value]);
      break;


    case NS_PANGO_ATTR_SIZE:
    case NS_PANGO_ATTR_RISE:
    case NS_PANGO_ATTR_LETTER_SPACING:
    case NS_PANGO_ATTR_ABSOLUTE_SIZE:
    case NS_PANGO_ATTR_FOREGROUND_ALPHA:
    case NS_PANGO_ATTR_BACKGROUND_ALPHA:
    case NS_PANGO_ATTR_SHOW:
    case NS_PANGO_ATTR_WORD:
    case NS_PANGO_ATTR_SENTENCE:
    case NS_PANGO_ATTR_ABSOLUTE_LINE_HEIGHT:
      gtk_json_printer_add_integer (printer, "value", ((NsPangoAttrInt*)attr)->value);
      break;

    case NS_PANGO_ATTR_FONT_DESC:
      str = ns_pango_font_description_to_string (((NsPangoAttrFontDesc*)attr)->desc);
      gtk_json_printer_add_string (printer, "value", str);
      g_free (str);
      break;

    case NS_PANGO_ATTR_FOREGROUND:
    case NS_PANGO_ATTR_BACKGROUND:
    case NS_PANGO_ATTR_UNDERLINE_COLOR:
    case NS_PANGO_ATTR_OVERLINE_COLOR:
    case NS_PANGO_ATTR_STRIKETHROUGH_COLOR:
      str = ns_pango_color_to_string (&((NsPangoAttrColor*)attr)->color);
      gtk_json_printer_add_string (printer, "value", str);
      g_free (str);
      break;

    case NS_PANGO_ATTR_STRIKETHROUGH:
    case NS_PANGO_ATTR_FALLBACK:
    case NS_PANGO_ATTR_ALLOW_BREAKS:
    case NS_PANGO_ATTR_INSERT_HYPHENS:
      gtk_json_printer_add_boolean (printer, "value", ((NsPangoAttrInt*)attr)->value != 0);
      break;

    case NS_PANGO_ATTR_SHAPE:
      gtk_json_printer_add_string (printer, "value", "shape");
      break;

    case NS_PANGO_ATTR_SCALE:
    case NS_PANGO_ATTR_LINE_HEIGHT:
      gtk_json_printer_add_number (printer, "value", ((NsPangoAttrFloat*)attr)->value);
    }

  gtk_json_printer_end (printer);
}

static void
add_attr_list (GtkJsonPrinter *printer,
               NsPangoAttrList  *attrs)
{
  GSList *attributes, *l;

  if (!attrs)
    return;

  attributes = ns_pango_attr_list_get_attributes (attrs);

  if (!attributes)
    return;

  gtk_json_printer_start_array (printer, "attributes");

  for (l = attributes; l; l = l->next)
    {
      NsPangoAttribute *attr = l->data;
      add_attribute (printer, attr);
    }
  g_slist_free_full (attributes, (GDestroyNotify) ns_pango_attribute_destroy);

  gtk_json_printer_end (printer);
}

static void
add_tab_array (GtkJsonPrinter *printer,
               NsPangoTabArray  *tabs)
{
  if (!tabs || ns_pango_tab_array_get_size (tabs) == 0)
    return;

  gtk_json_printer_start_object (printer, "tabs");

  gtk_json_printer_add_boolean (printer, "positions-in-pixels", ns_pango_tab_array_get_positions_in_pixels (tabs));
  gtk_json_printer_start_array (printer, "positions");
  for (int i = 0; i < ns_pango_tab_array_get_size (tabs); i++)
    {
      NsPangoTabAlign align;
      int pos;
      ns_pango_tab_array_get_tab (tabs, i, &align, &pos);
      gtk_json_printer_start_object (printer, NULL);
      gtk_json_printer_add_integer (printer, "position", pos);
      gtk_json_printer_add_string (printer, "alignment", tab_align_names[align]);
      gtk_json_printer_add_integer (printer, "decimal-point", ns_pango_tab_array_get_decimal_point (tabs, i));
      gtk_json_printer_end (printer);
    }
  gtk_json_printer_end (printer);

  gtk_json_printer_end (printer);
}

static void
add_context (GtkJsonPrinter *printer,
             NsPangoContext   *context)
{
  char *str;
  const NsPangoMatrix *matrix;
  NsPangoMatrix identity = NS_PANGO_MATRIX_INIT;

  gtk_json_printer_start_object (printer, "context");

  /* Note: since we don't create the context when deserializing,
   * we don't strip out default values here to ensure that the
   * context gets updated as expected.
   */

  str = ns_pango_font_description_to_string (context->font_desc);
  gtk_json_printer_add_string (printer, "font", str);
  g_free (str);

  if (context->set_language)
    gtk_json_printer_add_string (printer, "language", ns_pango_language_to_string (context->set_language));

  gtk_json_printer_add_string (printer, "base-gravity", gravity_names[context->base_gravity]);
  gtk_json_printer_add_string (printer, "gravity-hint", gravity_hint_names[context->gravity_hint]);
  gtk_json_printer_add_string (printer, "base-dir", direction_names[context->base_dir]);
  gtk_json_printer_add_boolean (printer, "round-glyph-positions", context->round_glyph_positions);

  matrix = ns_pango_context_get_matrix (context);
  if (!matrix)
    matrix = &identity;

  gtk_json_printer_start_array (printer, "transform");
  gtk_json_printer_add_number (printer, NULL, matrix->xx);
  gtk_json_printer_add_number (printer, NULL, matrix->xy);
  gtk_json_printer_add_number (printer, NULL, matrix->yx);
  gtk_json_printer_add_number (printer, NULL, matrix->yy);
  gtk_json_printer_add_number (printer, NULL, matrix->x0);
  gtk_json_printer_add_number (printer, NULL, matrix->y0);
  gtk_json_printer_end (printer);

  gtk_json_printer_end (printer);
}

static void
add_log_attrs (GtkJsonPrinter *printer,
               NsPangoLayout    *layout)
{
  const NsPangoLogAttr *log_attrs;
  int n_attrs;

  gtk_json_printer_start_array (printer, "log-attrs");

  log_attrs = ns_pango_layout_get_log_attrs_readonly (layout, &n_attrs);
  for (int i = 0; i < n_attrs; i++)
    {
      gtk_json_printer_start_object (printer, NULL);
      if (log_attrs[i].is_line_break)
        gtk_json_printer_add_boolean (printer, "line-break", TRUE);
      if (log_attrs[i].is_mandatory_break)
        gtk_json_printer_add_boolean (printer, "mandatory-break", TRUE);
      if (log_attrs[i].is_char_break)
        gtk_json_printer_add_boolean (printer, "char-break", TRUE);
      if (log_attrs[i].is_white)
        gtk_json_printer_add_boolean (printer, "white", TRUE);
      if (log_attrs[i].is_cursor_position)
        gtk_json_printer_add_boolean (printer, "cursor-position", TRUE);
      if (log_attrs[i].is_word_start)
        gtk_json_printer_add_boolean (printer, "word-start", TRUE);
      if (log_attrs[i].is_word_end)
        gtk_json_printer_add_boolean (printer, "word-end", TRUE);
      if (log_attrs[i].is_sentence_boundary)
        gtk_json_printer_add_boolean (printer, "sentence-boundary", TRUE);
      if (log_attrs[i].is_sentence_start)
        gtk_json_printer_add_boolean (printer, "sentence-start", TRUE);
      if (log_attrs[i].is_sentence_end)
        gtk_json_printer_add_boolean (printer, "sentence-end", TRUE);
      if (log_attrs[i].backspace_deletes_character)
        gtk_json_printer_add_boolean (printer, "backspace-deletes-character", TRUE);
      if (log_attrs[i].is_expandable_space)
        gtk_json_printer_add_boolean (printer, "expandable-space", TRUE);
      if (log_attrs[i].is_word_boundary)
        gtk_json_printer_add_boolean (printer, "word-boundary", TRUE);
      if (log_attrs[i].break_inserts_hyphen)
        gtk_json_printer_add_boolean (printer, "break-inserts-hyphen", TRUE);
      if (log_attrs[i].break_removes_preceding)
        gtk_json_printer_add_boolean (printer, "break-removes-preceding", TRUE);
      gtk_json_printer_end (printer);
    }

  gtk_json_printer_end (printer);
}

static void
add_font (GtkJsonPrinter *printer,
          const char     *member,
          NsPangoFont      *font)
{
  NsPangoFontDescription *desc;
  char *str;
  hb_font_t *hb_font;
  hb_face_t *face;
  hb_blob_t *blob;
  const char *data;
  guint length;
  const int *coords;
  hb_feature_t features[32];
  NsPangoMatrix matrix;

  gtk_json_printer_start_object (printer, member);

  desc = ns_pango_font_describe (font);
  str = ns_pango_font_description_to_string (desc);
  gtk_json_printer_add_string (printer, "description", str);
  g_free (str);
  ns_pango_font_description_free (desc);

  hb_font = ns_pango_font_get_hb_font (font);
  face = hb_font_get_face (hb_font);
  blob = hb_face_reference_blob (face);

  data = hb_blob_get_data (blob, &length);
  str = g_compute_checksum_for_data (G_CHECKSUM_SHA256, (const guchar *)data, length);

  gtk_json_printer_add_string (printer, "checksum", str);

  g_free (str);
  hb_blob_destroy (blob);

  coords = hb_font_get_var_coords_normalized (hb_font, &length);
  if (length > 0)
    {
      guint count;
      hb_ot_var_axis_info_t *axes;

      count = hb_ot_var_get_axis_count (face);
      g_assert (count == length);

      axes = g_alloca (count * sizeof (hb_ot_var_axis_info_t));
      hb_ot_var_get_axis_infos (face, 0, &count, axes);

      gtk_json_printer_start_object (printer, "variations");

      for (int i = 0; i < length; i++)
        {
          char buf[5] = { 0, };

          hb_tag_to_string (axes[i].tag, buf);
          gtk_json_printer_add_integer (printer, buf, coords[i]);
        }

      gtk_json_printer_end (printer);
    }

  length = 0;
  ns_pango_font_get_features (font, features, G_N_ELEMENTS (features), &length);
  if (length > 0)
    {
      gtk_json_printer_start_object (printer, "features");

      for (int i = 0; i < length; i++)
        {
          char buf[5] = { 0, };

          hb_tag_to_string (features[i].tag, buf);
          gtk_json_printer_add_integer (printer, buf, features[i].value);
        }

      gtk_json_printer_end (printer);
    }

  ns_pango_font_get_matrix (font, &matrix);
  if (memcmp (&matrix, &(NsPangoMatrix)NS_PANGO_MATRIX_INIT, sizeof (NsPangoMatrix)) != 0)
    {
      gtk_json_printer_start_array (printer, "matrix");
      gtk_json_printer_add_number (printer, NULL, matrix.xx);
      gtk_json_printer_add_number (printer, NULL, matrix.xy);
      gtk_json_printer_add_number (printer, NULL, matrix.yx);
      gtk_json_printer_add_number (printer, NULL, matrix.yy);
      gtk_json_printer_add_number (printer, NULL, matrix.x0);
      gtk_json_printer_add_number (printer, NULL, matrix.y0);
      gtk_json_printer_end (printer);
    }

  gtk_json_printer_end (printer);
}

#define ANALYSIS_FLAGS (NS_PANGO_ANALYSIS_FLAG_CENTERED_BASELINE | \
                        NS_PANGO_ANALYSIS_FLAG_IS_ELLIPSIS | \
                        NS_PANGO_ANALYSIS_FLAG_NEED_HYPHEN)

static void
add_run (GtkJsonPrinter *printer,
         NsPangoLayout    *layout,
         NsPangoLayoutRun *run)
{
  char *str;

  gtk_json_printer_start_object (printer, NULL);

  gtk_json_printer_add_integer (printer, "offset", run->item->offset);
  gtk_json_printer_add_integer (printer, "length", run->item->length);

  str = g_strndup (layout->text + run->item->offset, run->item->length);
  gtk_json_printer_add_string (printer, "text", str);
  g_free (str);

  gtk_json_printer_add_integer (printer, "bidi-level", run->item->analysis.level);
  gtk_json_printer_add_string (printer, "gravity", gravity_names[run->item->analysis.gravity]);
  gtk_json_printer_add_string (printer, "language", ns_pango_language_to_string (run->item->analysis.language));
  gtk_json_printer_add_string (printer, "script", get_script_name (run->item->analysis.script));

  if (run->item->analysis.font)
    add_font (printer, "font", run->item->analysis.font);

  gtk_json_printer_add_integer (printer, "flags", run->item->analysis.flags & ANALYSIS_FLAGS);

  if (run->item->analysis.extra_attrs)
    {
      GSList *l;

      gtk_json_printer_start_array (printer, "extra-attributes");
      for (l = run->item->analysis.extra_attrs; l; l = l->next)
        {
          NsPangoAttribute *attr = l->data;
          add_attribute (printer, attr);
        }
      gtk_json_printer_end (printer);
    }

  gtk_json_printer_add_integer (printer, "y-offset", run->y_offset);
  gtk_json_printer_add_integer (printer, "start-x-offset", run->start_x_offset);
  gtk_json_printer_add_integer (printer, "end-x-offset", run->end_x_offset);

  gtk_json_printer_start_array (printer, "glyphs");
  for (int i = 0; i < run->glyphs->num_glyphs; i++)
    {
      gtk_json_printer_start_object (printer, NULL);

      gtk_json_printer_add_integer (printer, "glyph", run->glyphs->glyphs[i].glyph);
      gtk_json_printer_add_integer (printer, "width", run->glyphs->glyphs[i].geometry.width);

      if (run->glyphs->glyphs[i].geometry.x_offset != 0)
        gtk_json_printer_add_integer (printer, "x-offset", run->glyphs->glyphs[i].geometry.x_offset);

      if (run->glyphs->glyphs[i].geometry.y_offset != 0)
        gtk_json_printer_add_integer (printer, "y-offset", run->glyphs->glyphs[i].geometry.y_offset);

      if (run->glyphs->glyphs[i].attr.is_cluster_start)
        gtk_json_printer_add_boolean (printer, "is-cluster-start", TRUE);

      if (run->glyphs->glyphs[i].attr.is_color)
        gtk_json_printer_add_boolean (printer, "is-color", TRUE);

      gtk_json_printer_add_integer (printer, "log-cluster", run->glyphs->log_clusters[i]);

      gtk_json_printer_end (printer);
    }

  gtk_json_printer_end (printer);

  gtk_json_printer_end (printer);
}

#undef ANALYSIS_FLAGS

static void
add_line (GtkJsonPrinter  *printer,
          NsPangoLayoutLine *line)
{
  gtk_json_printer_start_object (printer, NULL);

  gtk_json_printer_add_integer (printer, "start-index", line->start_index);
  gtk_json_printer_add_integer (printer, "length", line->length);
  gtk_json_printer_add_boolean (printer, "paragraph-start", line->is_paragraph_start);
  gtk_json_printer_add_string (printer, "direction", direction_names[line->resolved_dir]);

  gtk_json_printer_start_array (printer, "runs");
  for (GSList *l = line->runs; l; l = l->next)
    {
      NsPangoLayoutRun *run = l->data;
      add_run (printer, line->layout, run);
    }
  gtk_json_printer_end (printer);

  gtk_json_printer_end (printer);
}

static void
add_output (GtkJsonPrinter *printer,
            NsPangoLayout    *layout)
{
  int width, height;

  gtk_json_printer_start_object (printer, "output");

  gtk_json_printer_add_boolean (printer, "is-wrapped", ns_pango_layout_is_wrapped (layout));
  gtk_json_printer_add_boolean (printer, "is-ellipsized", ns_pango_layout_is_ellipsized (layout));
  gtk_json_printer_add_integer (printer, "unknown-glyphs", ns_pango_layout_get_unknown_glyphs_count (layout));

  ns_pango_layout_get_size (layout, &width, &height);
  gtk_json_printer_add_integer (printer, "width", width);
  gtk_json_printer_add_integer (printer, "height", height);

  add_log_attrs (printer, layout);
  gtk_json_printer_start_array (printer, "lines");
  for (GSList *l = layout->lines; l; l = l->next)
    {
      NsPangoLayoutLine *line = l->data;
      add_line (printer, line);
    }
  gtk_json_printer_end (printer);

  gtk_json_printer_end (printer);
}

static void
layout_to_json (GtkJsonPrinter            *printer,
                NsPangoLayout               *layout,
                NsPangoLayoutSerializeFlags  flags)
{
  const char *str;

  gtk_json_printer_start_object (printer, NULL);

  if (flags & NS_PANGO_LAYOUT_SERIALIZE_CONTEXT)
    add_context (printer, layout->context);

  str = (const char *) g_object_get_data (G_OBJECT (layout), "comment");
  if (str)
    gtk_json_printer_add_string (printer, "comment", str);

  gtk_json_printer_add_string (printer, "text", layout->text);

  add_attr_list (printer, layout->attrs);

  if (layout->font_desc)
    {
      char *str = ns_pango_font_description_to_string (layout->font_desc);
      gtk_json_printer_add_string (printer, "font", str);
      g_free (str);
    }

  add_tab_array (printer, layout->tabs);

  if (layout->justify)
    gtk_json_printer_add_boolean (printer, "justify", TRUE);

  if (layout->justify_last_line)
    gtk_json_printer_add_boolean (printer, "justify-last-line", TRUE);

  if (layout->single_paragraph)
    gtk_json_printer_add_boolean (printer, "single-paragraph", TRUE);

  if (!layout->auto_dir)
    gtk_json_printer_add_boolean (printer, "auto-dir", FALSE);

  if (layout->alignment != NS_PANGO_ALIGN_LEFT)
    gtk_json_printer_add_string (printer, "alignment", alignment_names[layout->alignment]);

  if (layout->wrap != NS_PANGO_WRAP_WORD)
    gtk_json_printer_add_string (printer, "wrap", wrap_names[layout->wrap]);

  if (layout->ellipsize != NS_PANGO_ELLIPSIZE_NONE)
    gtk_json_printer_add_string (printer, "ellipsize", ellipsize_names[layout->ellipsize]);

  if (layout->width != -1)
    gtk_json_printer_add_integer (printer, "width", layout->width);

  if (layout->height != -1)
    gtk_json_printer_add_integer (printer, "height", layout->height);

  if (layout->indent != 0)
    gtk_json_printer_add_integer (printer, "indent", layout->indent);

  if (layout->spacing != 0)
    gtk_json_printer_add_integer (printer, "spacing", layout->spacing);

  if (layout->line_spacing != 0.)
    gtk_json_printer_add_number (printer, "line-spacing", layout->line_spacing);

  if (flags & NS_PANGO_LAYOUT_SERIALIZE_OUTPUT)
    add_output (printer, layout);

  gtk_json_printer_end (printer);
}

static void
gstring_write (GtkJsonPrinter *printer,
               const char     *s,
               gpointer        data)
{
  GString *str = data;
  g_string_append (str, s);
}

/* }}} */
/* {{{ Deserialization */

static int
parser_select_string (GtkJsonParser  *parser,
                      const char    **options)
{
  int value;

  value = gtk_json_parser_select_string (parser, options);
  if (value == -1)
    {
      char *str = gtk_json_parser_get_string (parser);
      char *opts = g_strjoinv (", ", (char **)options);

      gtk_json_parser_value_error (parser,
                                   "Failed to parse string: %s, valid options are: %s",
                                   str, opts);

      g_free (opts);
      g_free (str);

      value = 0;
    }

  return value;
}

static NsPangoFontDescription *
parser_get_font_description (GtkJsonParser *parser)
{
  char *str = gtk_json_parser_get_string (parser);
  NsPangoFontDescription *desc = ns_pango_font_description_from_string (str);

  if (!desc)
    gtk_json_parser_value_error (parser,
                                 "Failed to parse font: %s", str);
  g_free (str);

  return desc;
}

static void
parser_get_color (GtkJsonParser *parser,
                  NsPangoColor    *color)
{
  char *str = gtk_json_parser_get_string (parser);
  if (!ns_pango_color_parse (color, str))
    {
      gtk_json_parser_value_error (parser,
                                   "Failed to parse color: %s", str);
      color->red = color->green = color->blue = 0;
    }

  g_free (str);
}

static NsPangoAttribute *
attr_for_type (GtkJsonParser *parser,
               NsPangoAttrType  type,
               int            start,
               int            end)
{
  NsPangoAttribute *attr;
  NsPangoFontDescription *desc;
  NsPangoColor color;
  char *str;

  switch (type)
    {
    default:
      g_assert_not_reached ();

    case NS_PANGO_ATTR_INVALID:
      gtk_json_parser_schema_error (parser, "Missing attribute type");
      return NULL;

    case NS_PANGO_ATTR_LANGUAGE:
      str = gtk_json_parser_get_string (parser);
      attr = ns_pango_attr_language_new (ns_pango_language_from_string (str));
      g_free (str);
      break;

    case NS_PANGO_ATTR_FAMILY:
      str = gtk_json_parser_get_string (parser);
      attr = ns_pango_attr_family_new (str);
      g_free (str);
      break;

    case NS_PANGO_ATTR_STYLE:
      attr = ns_pango_attr_style_new ((NsPangoStyle) parser_select_string (parser, style_names));
      break;

    case NS_PANGO_ATTR_WEIGHT:
      if (gtk_json_parser_get_node (parser) == GTK_JSON_STRING)
        attr = ns_pango_attr_weight_new (get_weight (parser_select_string (parser, weight_names)));
      else
        attr = ns_pango_attr_weight_new ((int) gtk_json_parser_get_int (parser));
      break;

    case NS_PANGO_ATTR_VARIANT:
      attr = ns_pango_attr_variant_new ((NsPangoVariant) parser_select_string (parser, variant_names));
      break;

    case NS_PANGO_ATTR_STRETCH:
      attr = ns_pango_attr_stretch_new ((NsPangoStretch) parser_select_string (parser, stretch_names));
      break;

    case NS_PANGO_ATTR_WIDTH:
      if (gtk_json_parser_get_node (parser) == GTK_JSON_STRING)
        attr = ns_pango_attr_width_new (get_width (parser_select_string (parser, width_names)));
      else
        attr = ns_pango_attr_width_new ((int) gtk_json_parser_get_int (parser));
      break;

    case NS_PANGO_ATTR_SIZE:
      attr = ns_pango_attr_size_new ((int) gtk_json_parser_get_number (parser));
      break;

    case NS_PANGO_ATTR_FONT_DESC:
      desc = parser_get_font_description (parser);
      attr = ns_pango_attr_font_desc_new (desc);
      ns_pango_font_description_free (desc);
      break;

    case NS_PANGO_ATTR_FOREGROUND:
      parser_get_color (parser, &color);
      attr = ns_pango_attr_foreground_new (color.red, color.green, color.blue);
      break;

    case NS_PANGO_ATTR_BACKGROUND:
      parser_get_color (parser, &color);
      attr = ns_pango_attr_background_new (color.red, color.green, color.blue);
      break;

    case NS_PANGO_ATTR_UNDERLINE:
      attr = ns_pango_attr_underline_new ((NsPangoUnderline) parser_select_string (parser, underline_names));
      break;

    case NS_PANGO_ATTR_STRIKETHROUGH:
      attr = ns_pango_attr_strikethrough_new (gtk_json_parser_get_boolean (parser));
      break;

    case NS_PANGO_ATTR_RISE:
      attr = ns_pango_attr_rise_new ((int) gtk_json_parser_get_number (parser));
      break;

    case NS_PANGO_ATTR_SHAPE:
      /* FIXME */
      attr = ns_pango_attr_shape_new (&(NsPangoRectangle) { 0, 0, 0, 0}, &(NsPangoRectangle) { 0, 0, 0, 0});
      break;

    case NS_PANGO_ATTR_SCALE:
      attr = ns_pango_attr_scale_new (gtk_json_parser_get_number (parser));
      break;

    case NS_PANGO_ATTR_FALLBACK:
      attr = ns_pango_attr_fallback_new (gtk_json_parser_get_boolean (parser));
      break;

    case NS_PANGO_ATTR_LETTER_SPACING:
      attr = ns_pango_attr_letter_spacing_new ((int) gtk_json_parser_get_number (parser));
      break;

    case NS_PANGO_ATTR_UNDERLINE_COLOR:
      parser_get_color (parser, &color);
      attr = ns_pango_attr_underline_color_new (color.red, color.green, color.blue);
      break;

    case NS_PANGO_ATTR_STRIKETHROUGH_COLOR:
      parser_get_color (parser, &color);
      attr = ns_pango_attr_strikethrough_color_new (color.red, color.green, color.blue);
      break;

    case NS_PANGO_ATTR_ABSOLUTE_SIZE:
      attr = ns_pango_attr_size_new_absolute ((int) gtk_json_parser_get_number (parser));
      break;

    case NS_PANGO_ATTR_GRAVITY:
      attr = ns_pango_attr_gravity_new ((NsPangoGravity) parser_select_string (parser, gravity_names));
      break;

    case NS_PANGO_ATTR_GRAVITY_HINT:
      attr = ns_pango_attr_gravity_hint_new ((NsPangoGravityHint) parser_select_string (parser, gravity_hint_names));
      break;

    case NS_PANGO_ATTR_FONT_FEATURES:
      str = gtk_json_parser_get_string (parser);
      attr = ns_pango_attr_font_features_new (str);
      g_free (str);
      break;

    case NS_PANGO_ATTR_FOREGROUND_ALPHA:
      attr = ns_pango_attr_foreground_alpha_new ((int) gtk_json_parser_get_number (parser));
      break;

    case NS_PANGO_ATTR_BACKGROUND_ALPHA:
      attr = ns_pango_attr_background_alpha_new ((int) gtk_json_parser_get_number (parser));
      break;

    case NS_PANGO_ATTR_ALLOW_BREAKS:
      attr = ns_pango_attr_allow_breaks_new (gtk_json_parser_get_boolean (parser));
      break;

    case NS_PANGO_ATTR_SHOW:
      attr = ns_pango_attr_show_new ((int) gtk_json_parser_get_number (parser));
      break;

    case NS_PANGO_ATTR_INSERT_HYPHENS:
      attr = ns_pango_attr_insert_hyphens_new ((int) gtk_json_parser_get_number (parser));
      break;

    case NS_PANGO_ATTR_OVERLINE:
      attr = ns_pango_attr_overline_new ((NsPangoOverline) parser_select_string (parser, overline_names));
      break;

    case NS_PANGO_ATTR_OVERLINE_COLOR:
      parser_get_color (parser, &color);
      attr = ns_pango_attr_overline_color_new (color.red, color.green, color.blue);
      break;

    case NS_PANGO_ATTR_LINE_HEIGHT:
      attr = ns_pango_attr_line_height_new (gtk_json_parser_get_number (parser));
      break;

    case NS_PANGO_ATTR_ABSOLUTE_LINE_HEIGHT:
      attr = ns_pango_attr_line_height_new_absolute ((int) gtk_json_parser_get_number (parser));
      break;

    case NS_PANGO_ATTR_TEXT_TRANSFORM:
      attr = ns_pango_attr_text_transform_new ((NsPangoTextTransform) parser_select_string (parser, text_transform_names));
      break;

    case NS_PANGO_ATTR_WORD:
      attr = ns_pango_attr_word_new ();
      break;

    case NS_PANGO_ATTR_SENTENCE:
      attr = ns_pango_attr_sentence_new ();
      break;

    case NS_PANGO_ATTR_BASELINE_SHIFT:
      attr = ns_pango_attr_baseline_shift_new (parser_select_string (parser, baseline_shift_names));
      break;

    case NS_PANGO_ATTR_FONT_SCALE:
      attr = ns_pango_attr_font_scale_new ((NsPangoFontScale) parser_select_string (parser, font_scale_names));
      break;
    }

  attr->start_index = start;
  attr->end_index = end;

  return attr;
}

enum {
  ATTR_START,
  ATTR_END,
  ATTR_TYPE,
  ATTR_VALUE
};

static const char *attr_members[] = {
  "start",
  "end",
  "type",
  "value",
  NULL
};

static NsPangoAttribute *
json_to_attribute (GtkJsonParser *parser)
{
  NsPangoAttribute *attr = NULL;
  NsPangoAttrType type = NS_PANGO_ATTR_INVALID;
  guint start = NS_PANGO_ATTR_INDEX_FROM_TEXT_BEGINNING;
  guint end = NS_PANGO_ATTR_INDEX_TO_TEXT_END;

  gtk_json_parser_start_object (parser);

  do
    {
      switch (gtk_json_parser_select_member (parser, attr_members))
        {
        case ATTR_START:
          start = (int)gtk_json_parser_get_number (parser);
          break;

        case ATTR_END:
          end = (int)gtk_json_parser_get_number (parser);
          break;

        case ATTR_TYPE:
          type = parser_select_string (parser, attr_type_names);
          break;

        case ATTR_VALUE:
          attr = attr_for_type (parser, type, start, end);
          break;

        default:
          break;
        }
    }
  while (gtk_json_parser_next (parser));

  if (!attr && !gtk_json_parser_get_error (parser))
    gtk_json_parser_schema_error (parser, "Attribute missing \"value\"");

  gtk_json_parser_end (parser);

  return attr;
}

static void
json_parser_fill_attr_list (GtkJsonParser *parser,
                            NsPangoAttrList *attributes)
{
  gtk_json_parser_start_array (parser);

  do
    {
      NsPangoAttribute *attr = json_to_attribute (parser);
      if (attr)
        ns_pango_attr_list_insert (attributes, attr);
    }
  while (gtk_json_parser_next (parser));

  gtk_json_parser_end (parser);
}

enum {
  TAB_POSITION,
  TAB_ALIGNMENT,
  TAB_DECIMAL_POINT
};

static const char *tab_members[] = {
  "position",
  "alignment",
  "decimal-point",
  NULL,
};


static void
json_parser_fill_tabs (GtkJsonParser *parser,
                       NsPangoTabArray *tabs)
{
  int index;

  gtk_json_parser_start_array (parser);

  index = 0;
  do
    {
      int pos = 0;
      NsPangoTabAlign align = NS_PANGO_TAB_LEFT;
      gunichar ch = 0;

      if (gtk_json_parser_get_node (parser) == GTK_JSON_OBJECT)
        {
          gtk_json_parser_start_object (parser);
          do
            {
              switch (gtk_json_parser_select_member (parser, tab_members))
                {
                case TAB_POSITION:
                  pos = (int) gtk_json_parser_get_number (parser);
                  break;

                case TAB_ALIGNMENT:
                  align = (NsPangoTabAlign) parser_select_string (parser, tab_align_names);
                  break;

                case TAB_DECIMAL_POINT:
                  ch = (int) gtk_json_parser_get_number (parser);
                  break;

                default:
                  break;
                }
            }
          while (gtk_json_parser_next (parser));

          gtk_json_parser_end (parser);
        }
      else
        pos = (int) gtk_json_parser_get_number (parser);

      ns_pango_tab_array_set_tab (tabs, index, align, pos);
      ns_pango_tab_array_set_decimal_point (tabs, index, ch);
      index++;
    }
  while (gtk_json_parser_next (parser));

  gtk_json_parser_end (parser);
}

enum {
  TABS_POSITIONS_IN_PIXELS,
  TABS_POSITIONS
};

static const char *tabs_members[] = {
  "positions-in-pixels",
  "positions",
  NULL
};

static void
json_parser_fill_tab_array (GtkJsonParser *parser,
                            NsPangoTabArray *tabs)
{
  gtk_json_parser_start_object (parser);

  do
    {
      switch (gtk_json_parser_select_member (parser, tabs_members))
        {
        case TABS_POSITIONS_IN_PIXELS:
          ns_pango_tab_array_set_positions_in_pixels (tabs, gtk_json_parser_get_boolean (parser));
          break;

        case TABS_POSITIONS:
          json_parser_fill_tabs (parser, tabs);
          break;

        default:
          break;
        }
    }
  while (gtk_json_parser_next (parser));

  gtk_json_parser_end (parser);
}

enum {
  CONTEXT_LANGUAGE,
  CONTEXT_FONT,
  CONTEXT_BASE_GRAVITY,
  CONTEXT_GRAVITY_HINT,
  CONTEXT_BASE_DIR,
  CONTEXT_ROUND_GLYPH_POSITIONS,
  CONTEXT_TRANSFORM,
};

static const char *context_members[] = {
  "language",
  "font",
  "base-gravity",
  "gravity-hint",
  "base-dir",
  "round-glyph-positions",
  "transform",
  NULL,
};

static void
json_parser_fill_context (GtkJsonParser *parser,
                          NsPangoContext  *context)
{
  gtk_json_parser_start_object (parser);

  do
    {
      char *str;

      switch (gtk_json_parser_select_member (parser, context_members))
        {
        case CONTEXT_LANGUAGE:
          str = gtk_json_parser_get_string (parser);
          NsPangoLanguage *language = ns_pango_language_from_string (str);
          ns_pango_context_set_language (context, language);
          g_free (str);
          break;

        case CONTEXT_FONT:
          {
            NsPangoFontDescription *desc = parser_get_font_description (parser);
            ns_pango_context_set_font_description (context, desc);
            ns_pango_font_description_free (desc);
          }
          break;

        case CONTEXT_BASE_GRAVITY:
          ns_pango_context_set_base_gravity (context, (NsPangoGravity) parser_select_string (parser, gravity_names));
          break;

        case CONTEXT_GRAVITY_HINT:
          ns_pango_context_set_gravity_hint (context, (NsPangoGravityHint) parser_select_string (parser, gravity_hint_names));
          break;

        case CONTEXT_BASE_DIR:
          ns_pango_context_set_base_dir (context, (NsPangoDirection) parser_select_string (parser, direction_names));
          break;

        case CONTEXT_ROUND_GLYPH_POSITIONS:
          ns_pango_context_set_round_glyph_positions (context, gtk_json_parser_get_boolean (parser));
          break;

        case CONTEXT_TRANSFORM:
          {
            NsPangoMatrix m = NS_PANGO_MATRIX_INIT;

            gtk_json_parser_start_array (parser);
            m.xx = gtk_json_parser_get_number (parser);
            gtk_json_parser_next (parser);
            m.xy = gtk_json_parser_get_number (parser);
            gtk_json_parser_next (parser);
            m.yx = gtk_json_parser_get_number (parser);
            gtk_json_parser_next (parser);
            m.yy = gtk_json_parser_get_number (parser);
            gtk_json_parser_next (parser);
            m.x0 = gtk_json_parser_get_number (parser);
            gtk_json_parser_next (parser);
            m.y0 = gtk_json_parser_get_number (parser);
            gtk_json_parser_end (parser);

            ns_pango_context_set_matrix (context, &m);
          }
          break;

        default:
          break;
        }
    }
  while (gtk_json_parser_next (parser));

  gtk_json_parser_end (parser);
}

enum {
  LAYOUT_CONTEXT,
  LAYOUT_COMMENT,
  LAYOUT_TEXT,
  LAYOUT_ATTRIBUTES,
  LAYOUT_FONT,
  LAYOUT_TABS,
  LAYOUT_JUSTIFY,
  LAYOUT_JUSTIFY_LAST_LINE,
  LAYOUT_SINGLE_PARAGRAPH,
  LAYOUT_AUTO_DIR,
  LAYOUT_ALIGNMENT,
  LAYOUT_WRAP,
  LAYOUT_ELLIPSIZE,
  LAYOUT_WIDTH,
  LAYOUT_HEIGHT,
  LAYOUT_INDENT,
  LAYOUT_SPACING,
  LAYOUT_LINE_SPACING,
  LAYOUT_OUTPUT
};

static const char *layout_members[] = {
  "context",
  "comment",
  "text",
  "attributes",
  "font",
  "tabs",
  "justify",
  "justify-last-line",
  "single-paragraph",
  "auto-dir",
  "alignment",
  "wrap",
  "ellipsize",
  "width",
  "height",
  "indent",
  "spacing",
  "line-spacing",
  "output",
  NULL
};

static void
json_parser_fill_layout (GtkJsonParser               *parser,
                         NsPangoLayout                 *layout,
                         NsPangoLayoutDeserializeFlags  flags)
{
  gtk_json_parser_start_object (parser);

  do
    {
      char *str;

      switch (gtk_json_parser_select_member (parser, layout_members))
        {
        case LAYOUT_CONTEXT:
          if (flags & NS_PANGO_LAYOUT_DESERIALIZE_CONTEXT)
            json_parser_fill_context (parser, ns_pango_layout_get_context (layout));
          break;

        case LAYOUT_COMMENT:
          str = gtk_json_parser_get_string (parser);
          g_object_set_data_full (G_OBJECT (layout), "comment", str, g_free);
          break;

        case LAYOUT_TEXT:
          str = gtk_json_parser_get_string (parser);
          ns_pango_layout_set_text (layout, str, -1);
          g_free (str);
          break;

        case LAYOUT_ATTRIBUTES:
          {
            NsPangoAttrList *attributes = ns_pango_attr_list_new ();
            json_parser_fill_attr_list (parser, attributes);
            ns_pango_layout_set_attributes (layout, attributes);
            ns_pango_attr_list_unref (attributes);
          }
          break;

        case LAYOUT_FONT:
          {
            NsPangoFontDescription *desc = parser_get_font_description (parser);;
            ns_pango_layout_set_font_description (layout, desc);
            ns_pango_font_description_free (desc);
          }
          break;

        case LAYOUT_TABS:
          {
            NsPangoTabArray *tabs = ns_pango_tab_array_new (0, FALSE);
            json_parser_fill_tab_array (parser, tabs);
            ns_pango_layout_set_tabs (layout, tabs);
            ns_pango_tab_array_free (tabs);
          }
          break;

        case LAYOUT_JUSTIFY:
          ns_pango_layout_set_justify (layout, gtk_json_parser_get_boolean (parser));
          break;

        case LAYOUT_JUSTIFY_LAST_LINE:
          ns_pango_layout_set_justify_last_line (layout, gtk_json_parser_get_boolean (parser));
          break;

        case LAYOUT_SINGLE_PARAGRAPH:
          ns_pango_layout_set_single_paragraph_mode (layout, gtk_json_parser_get_boolean (parser));
          break;

        case LAYOUT_AUTO_DIR:
          ns_pango_layout_set_auto_dir (layout, gtk_json_parser_get_boolean (parser));
          break;

        case LAYOUT_ALIGNMENT:
          ns_pango_layout_set_alignment (layout, (NsPangoAlignment) parser_select_string (parser, alignment_names));
          break;

        case LAYOUT_WRAP:
          ns_pango_layout_set_wrap (layout, (NsPangoWrapMode) parser_select_string (parser, wrap_names));
          break;

        case LAYOUT_ELLIPSIZE:
          ns_pango_layout_set_ellipsize (layout, (NsPangoEllipsizeMode) parser_select_string (parser, ellipsize_names));
          break;

        case LAYOUT_WIDTH:
          ns_pango_layout_set_width (layout, (int) gtk_json_parser_get_number (parser));
          break;

        case LAYOUT_HEIGHT:
          ns_pango_layout_set_height (layout, (int) gtk_json_parser_get_number (parser));
          break;

        case LAYOUT_INDENT:
          ns_pango_layout_set_indent (layout, (int) gtk_json_parser_get_number (parser));
          break;

        case LAYOUT_SPACING:
          ns_pango_layout_set_spacing (layout, (int) gtk_json_parser_get_number (parser));
          break;

        case LAYOUT_LINE_SPACING:
          ns_pango_layout_set_line_spacing (layout, gtk_json_parser_get_number (parser));
          break;

        case LAYOUT_OUTPUT:
          break;

        default:
          break;
        }
    }
  while (gtk_json_parser_next (parser));

  gtk_json_parser_end (parser);
}

enum {
  FONT_DESCRIPTION,
  FONT_CHECKSUM,
  FONT_VARIATIONS,
  FONT_FEATURES,
  FONT_MATRIX
};

static const char *font_members[] = {
  "description",
  "checksum",
  "variations",
  "features",
  "matrix",
  NULL
};

static NsPangoFont *
json_parser_load_font (GtkJsonParser  *parser,
                       NsPangoContext   *context,
                       GError        **error)
{
  NsPangoFont *font = NULL;

  gtk_json_parser_start_object (parser);

  switch (gtk_json_parser_select_member (parser, font_members))
    {
    case FONT_DESCRIPTION:
      {
        NsPangoFontDescription *desc = parser_get_font_description (parser);
        font = ns_pango_context_load_font (context, desc);
        ns_pango_font_description_free (desc);
      }
      break;

    default:
      break;
    }

  gtk_json_parser_end (parser);

  return font;
}

/* }}} */
/* {{{ Public API */

/**
 * ns_pango_layout_serialize:
 * @layout: a `NsPangoLayout`
 * @flags: `NsPangoLayoutSerializeFlags`
 *
 * Serializes the @layout for later deserialization via [func@Pango.Layout.deserialize].
 *
 * There are no guarantees about the format of the output across different
 * versions of Pango and [func@Pango.Layout.deserialize] will reject data
 * that it cannot parse.
 *
 * The intended use of this function is testing, benchmarking and debugging.
 * The format is not meant as a permanent storage format.
 *
 * Returns: a `GBytes` containing the serialized form of @layout
 *
 * Since: 1.50
 */
GBytes *
ns_pango_layout_serialize (NsPangoLayout               *layout,
                        NsPangoLayoutSerializeFlags  flags)
{
  GString *str;
  GtkJsonPrinter *printer;
  char *data;
  gsize size;

  g_return_val_if_fail (NS_PANGO_IS_LAYOUT (layout), NULL);

  str = g_string_new ("");

  printer = gtk_json_printer_new (gstring_write, str, NULL);
  gtk_json_printer_set_flags (printer, GTK_JSON_PRINTER_PRETTY);
  layout_to_json (printer, layout, flags);
  gtk_json_printer_free (printer);

  g_string_append_c (str, '\n');

  size = str->len;
  data = g_string_free (str, FALSE);

  return g_bytes_new_take (data, size);
}

/**
 * ns_pango_layout_write_to_file:
 * @layout: a `NsPangoLayout`
 * @flags: `NsPangoLayoutSerializeFlags`
 * @filename: (type filename): the file to save it to
 * @error: Return location for a potential error
 *
 * A convenience method to serialize a layout to a file.
 *
 * It is equivalent to calling [method@Pango.Layout.serialize]
 * followed by [func@GLib.file_set_contents].
 *
 * See those two functions for details on the arguments.
 *
 * It is mostly intended for use inside a debugger to quickly dump
 * a layout to a file for later inspection.
 *
 * Returns: %TRUE if saving was successful
 *
 * Since: 1.50
 */
gboolean
ns_pango_layout_write_to_file (NsPangoLayout                *layout,
                            NsPangoLayoutSerializeFlags   flags,
                            const char                 *filename,
                            GError                    **error)
{
  GBytes *bytes;
  gboolean result;

  g_return_val_if_fail (NS_PANGO_IS_LAYOUT (layout), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  bytes = ns_pango_layout_serialize (layout, flags);
  result = g_file_set_contents (filename,
                                g_bytes_get_data (bytes, NULL),
                                g_bytes_get_size (bytes),
                                error);
  g_bytes_unref (bytes);

  return result;

}

/**
 * ns_pango_layout_deserialize:
 * @context: a `NsPangoContext`
 * @flags: `NsPangoLayoutDeserializeFlags`
 * @bytes: the bytes containing the data
 * @error: return location for an error
 *
 * Loads data previously created via [method@Pango.Layout.serialize].
 *
 * For a discussion of the supported format, see that function.
 *
 * Note: to verify that the returned layout is identical to
 * the one that was serialized, you can compare @bytes to the
 * result of serializing the layout again.
 *
 * Returns: (nullable) (transfer full): a new `NsPangoLayout`
 *
 * Since: 1.50
 */
NsPangoLayout *
ns_pango_layout_deserialize (NsPangoContext                 *context,
                          GBytes                       *bytes,
                          NsPangoLayoutDeserializeFlags   flags,
                          GError                      **error)
{
  NsPangoLayout *layout;
  GtkJsonParser *parser;
  const GError *parser_error;

  g_return_val_if_fail (NS_PANGO_IS_CONTEXT (context), NULL);

  layout = ns_pango_layout_new (context);

  parser = gtk_json_parser_new_for_bytes (bytes);
  json_parser_fill_layout (parser, layout, flags);

  parser_error = gtk_json_parser_get_error (parser);

  if (parser_error)
    {
      gsize start, end;
      int code;

      gtk_json_parser_get_error_offset (parser, &start, &end);

      if (g_error_matches (parser_error, GTK_JSON_ERROR, GTK_JSON_ERROR_VALUE))
        code = NS_PANGO_LAYOUT_DESERIALIZE_INVALID_VALUE;
      else if (g_error_matches (parser_error, GTK_JSON_ERROR, GTK_JSON_ERROR_SCHEMA))
        code = NS_PANGO_LAYOUT_DESERIALIZE_MISSING_VALUE;
      else
        code = NS_PANGO_LAYOUT_DESERIALIZE_INVALID;

      g_set_error (error, NS_PANGO_LAYOUT_DESERIALIZE_ERROR, code,
                   "%" G_GSIZE_FORMAT ":%" G_GSIZE_FORMAT ": %s", start, end, parser_error->message);

      g_clear_object (&layout);
    }

  gtk_json_parser_free (parser);

  return layout;
}

/**
 * ns_pango_font_serialize:
 * @font: a `NsPangoFont`
 *
 * Serializes the @font in a way that can be uniquely identified.
 *
 * There are no guarantees about the format of the output across different
 * versions of Pango.
 *
 * The intended use of this function is testing, benchmarking and debugging.
 * The format is not meant as a permanent storage format.
 *
 * To recreate a font from its serialized form, use [func@Pango.Font.deserialize].
 *
 * Returns: a `GBytes` containing the serialized form of @font
 *
 * Since: 1.50
 */
GBytes *
ns_pango_font_serialize (NsPangoFont *font)
{
  GString *str;
  GtkJsonPrinter *printer;
  char *data;
  gsize size;

  g_return_val_if_fail (NS_PANGO_IS_FONT (font), NULL);

  str = g_string_new ("");

  printer = gtk_json_printer_new (gstring_write, str, NULL);
  gtk_json_printer_set_flags (printer, GTK_JSON_PRINTER_PRETTY);
  add_font (printer, NULL, font);
  gtk_json_printer_free (printer);

  size = str->len;
  data = g_string_free (str, FALSE);

  return g_bytes_new_take (data, size);
}

/**
 * ns_pango_font_deserialize:
 * @context: a `NsPangoContext`
 * @bytes: the bytes containing the data
 * @error: return location for an error
 *
 * Loads data previously created via [method@Pango.Font.serialize].
 *
 * For a discussion of the supported format, see that function.
 *
 * Note: to verify that the returned font is identical to
 * the one that was serialized, you can compare @bytes to the
 * result of serializing the font again.
 *
 * Returns: (nullable) (transfer full): a new `NsPangoFont`
 *
 * Since: 1.50
 */
NsPangoFont *
ns_pango_font_deserialize (NsPangoContext  *context,
                        GBytes        *bytes,
                        GError       **error)
{
  NsPangoFont *font;
  GtkJsonParser *parser;

  g_return_val_if_fail (NS_PANGO_IS_CONTEXT (context), NULL);

  parser = gtk_json_parser_new_for_bytes (bytes);
  font = json_parser_load_font (parser, context, error);
  gtk_json_parser_free (parser);

  return font;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
