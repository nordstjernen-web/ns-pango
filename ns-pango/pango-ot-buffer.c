/* Pango
 * pango-ot-buffer.c: Buffer of glyphs for shaping/positioning
 *
 * Copyright (C) 2004 Red Hat Software
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

#include "pango-ot-private.h"

static NsPangoOTBuffer *
ns_pango_ot_buffer_copy (NsPangoOTBuffer *src)
{
  NsPangoOTBuffer *dst = g_slice_new (NsPangoOTBuffer);

  dst->buffer = hb_buffer_reference (src->buffer);

  return dst;
}

G_DEFINE_BOXED_TYPE (NsPangoOTBuffer, ns_pango_ot_buffer,
                     ns_pango_ot_buffer_copy,
                     ns_pango_ot_buffer_destroy)

/**
 * ns_pango_ot_buffer_new:
 * @font: a `NsPangoFcFont`
 *
 * Creates a new `NsPangoOTBuffer` for the given OpenType font.
 *
 * Return value: the newly allocated `NsPangoOTBuffer`, which should
 *   be freed with [method@NsPangoOT.Buffer.destroy].
 *
 * Since: 1.4
 */
NsPangoOTBuffer *
ns_pango_ot_buffer_new (NsPangoFcFont *font)
{
  NsPangoOTBuffer *buffer = g_slice_new (NsPangoOTBuffer);

  buffer->buffer = hb_buffer_create ();

  return buffer;
}

/**
 * ns_pango_ot_buffer_destroy:
 * @buffer: a `NsPangoOTBuffer`
 *
 * Destroys a `NsPangoOTBuffer` and free all associated memory.
 *
 * Since: 1.4
 */
void
ns_pango_ot_buffer_destroy (NsPangoOTBuffer *buffer)
{
  hb_buffer_destroy (buffer->buffer);
  g_slice_free (NsPangoOTBuffer, buffer);
}

/**
 * ns_pango_ot_buffer_clear:
 * @buffer: a `NsPangoOTBuffer`
 *
 * Empties a `NsPangoOTBuffer`, make it ready to add glyphs to.
 *
 * Since: 1.4
 */
void
ns_pango_ot_buffer_clear (NsPangoOTBuffer *buffer)
{
  hb_buffer_reset (buffer->buffer);
}

/**
 * ns_pango_ot_buffer_add_glyph:
 * @buffer: a `NsPangoOTBuffer`
 * @glyph: the glyph index to add, like a `NsPangoGlyph`
 * @properties: the glyph properties
 * @cluster: the cluster that this glyph belongs to
 *
 * Appends a glyph to a `NsPangoOTBuffer`, with @properties identifying which
 * features should be applied on this glyph.
 *
 * See [method@NsPangoOT.Ruleset.add_feature].
 *
 * Since: 1.4
 */
void
ns_pango_ot_buffer_add_glyph (NsPangoOTBuffer *buffer,
			   guint          glyph,
			   guint          properties,
			   guint          cluster)
{
  hb_buffer_add (buffer->buffer, glyph, cluster);
}

/**
 * ns_pango_ot_buffer_set_rtl:
 * @buffer: a `NsPangoOTBuffer`
 * @rtl: %TRUE for right-to-left text
 *
 * Sets whether glyphs will be rendered right-to-left.
 *
 * This setting is needed for proper horizontal positioning
 * of right-to-left scripts.
 *
 * Since: 1.4
 */
void
ns_pango_ot_buffer_set_rtl (NsPangoOTBuffer *buffer,
			 gboolean       rtl)
{
  hb_buffer_set_direction (buffer->buffer, rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
}

/**
 * ns_pango_ot_buffer_set_zero_width_marks
 * @buffer: a `NsPangoOTBuffer`
 * @zero_width_marks: %TRUE if characters with a mark class should
 *   be forced to zero width
 *
 * Sets whether characters with a mark class should be forced to zero width.
 *
 * This setting is needed for proper positioning of Arabic accents,
 * but will produce incorrect results with standard OpenType Indic
 * fonts.
 *
 * Since: 1.6
 */
void
ns_pango_ot_buffer_set_zero_width_marks (NsPangoOTBuffer *buffer,
				      gboolean       zero_width_marks)
{
}

/**
 * ns_pango_ot_buffer_get_glyphs
 * @buffer: a `NsPangoOTBuffer`
 * @glyphs: (array length=n_glyphs) (out) (optional): location to
 *   store the array of glyphs
 * @n_glyphs: (out) (optional): location to store the number of glyphs
 *
 * Gets the glyph array contained in a `NsPangoOTBuffer`.
 *
 * The glyphs are owned by the buffer and should not be freed,
 * and are only valid as long as buffer is not modified.
 *
 * Since: 1.4
 */
void
ns_pango_ot_buffer_get_glyphs (const NsPangoOTBuffer  *buffer,
			    NsPangoOTGlyph        **glyphs,
			    int                  *n_glyphs)
{
  if (glyphs)
    *glyphs = (NsPangoOTGlyph *) hb_buffer_get_glyph_infos (buffer->buffer, NULL);

  if (n_glyphs)
    *n_glyphs = hb_buffer_get_length (buffer->buffer);
}

/**
 * ns_pango_ot_buffer_output
 * @buffer: a `NsPangoOTBuffer`
 * @glyphs: a `NsPangoGlyphString`
 *
 * Exports the glyphs in a `NsPangoOTBuffer` into a `NsPangoGlyphString`.
 *
 * This is typically used after the OpenType layout processing
 * is over, to convert the resulting glyphs into a generic Pango
 * glyph string.
 *
 * Since: 1.4
 */
void
ns_pango_ot_buffer_output (const NsPangoOTBuffer *buffer,
			NsPangoGlyphString    *glyphs)
{
  unsigned int i;
  int last_cluster;

  unsigned int num_glyphs;
  hb_buffer_t *hb_buffer = buffer->buffer;
  hb_glyph_info_t *hb_glyph;
  hb_glyph_position_t *hb_position;

  if (HB_DIRECTION_IS_BACKWARD (hb_buffer_get_direction (buffer->buffer)))
    hb_buffer_reverse (buffer->buffer);

  /* Copy glyphs into output glyph string */
  num_glyphs = hb_buffer_get_length (hb_buffer);
  hb_glyph = hb_buffer_get_glyph_infos (hb_buffer, NULL);
  hb_position = hb_buffer_get_glyph_positions (hb_buffer, NULL);
  ns_pango_glyph_string_set_size (glyphs, num_glyphs);
  last_cluster = -1;
  for (i = 0; i < num_glyphs; i++)
    {
      glyphs->glyphs[i].glyph = hb_glyph->codepoint;
      glyphs->log_clusters[i] = hb_glyph->cluster;
      glyphs->glyphs[i].attr.is_cluster_start = glyphs->log_clusters[i] != last_cluster;
      last_cluster = glyphs->log_clusters[i];

      glyphs->glyphs[i].geometry.width = hb_position->x_advance;
      glyphs->glyphs[i].geometry.x_offset = hb_position->x_offset;
      glyphs->glyphs[i].geometry.y_offset = hb_position->y_offset;

      hb_glyph++;
      hb_position++;
    }

  if (HB_DIRECTION_IS_BACKWARD (hb_buffer_get_direction (buffer->buffer)))
    hb_buffer_reverse (buffer->buffer);
}
