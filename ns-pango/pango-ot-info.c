/* Pango
 * pango-ot-info.c: Store tables for OpenType
 *
 * Copyright (C) 2000 Red Hat Software
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

static void ns_pango_ot_info_finalize   (GObject *object);

G_DEFINE_TYPE (NsPangoOTInfo, ns_pango_ot_info, G_TYPE_OBJECT);

static void
ns_pango_ot_info_init (NsPangoOTInfo *self)
{
}

static void
ns_pango_ot_info_class_init (NsPangoOTInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ns_pango_ot_info_finalize;
}

static void
ns_pango_ot_info_finalize (GObject *object)
{
  NsPangoOTInfo *info = NS_PANGO_OT_INFO (object);

  if (info->hb_face)
    hb_face_destroy (info->hb_face);

  G_OBJECT_CLASS (ns_pango_ot_info_parent_class)->finalize (object);
}

static void
ns_pango_ot_info_finalizer (void *object)
{
  FT_Face face = object;
  NsPangoOTInfo *info = face->generic.data;

  info->face = NULL;
  g_object_unref (info);
}


/**
 * ns_pango_ot_info_get:
 * @face: a `FT_Face`
 *
 * Returns the `NsPangoOTInfo` structure for the given FreeType font face.
 *
 * Return value: (transfer none): the `NsPangoOTInfo` for @face.
 *   This object will have the same lifetime as @face.
 *
 * Since: 1.2
 */
NsPangoOTInfo *
ns_pango_ot_info_get (FT_Face face)
{
  NsPangoOTInfo *info;

  if (G_UNLIKELY (!face))
    return NULL;

  if (G_LIKELY (face->generic.data && face->generic.finalizer == ns_pango_ot_info_finalizer))
    return face->generic.data;
  else
    {
      if (face->generic.finalizer)
        face->generic.finalizer (face);

      info = face->generic.data = g_object_new (NS_TYPE_PANGO_OT_INFO, NULL);
      face->generic.finalizer = ns_pango_ot_info_finalizer;

      info->face = face;
      info->hb_face = hb_ft_face_create (face, NULL);
    }

  return info;
}

static hb_tag_t
get_hb_table_type (NsPangoOTTableType table_type)
{
  switch (table_type) {
    case NS_PANGO_OT_TABLE_GSUB: return HB_OT_TAG_GSUB;
    case NS_PANGO_OT_TABLE_GPOS: return HB_OT_TAG_GPOS;
    default:                  return HB_TAG_NONE;
  }
}

/**
 * ns_pango_ot_info_find_script:
 * @info: a `NsPangoOTInfo`
 * @table_type: the table type to obtain information about
 * @script_tag: the tag of the script to find
 * @script_index: (out) (optional): location to store the index of the script
 *
 * Finds the index of a script.
 *
 * If not found, tries to find the 'DFLT' and then 'dflt' scripts and
 * return the index of that in @script_index. If none of those is found
 * either, %NS_PANGO_OT_NO_SCRIPT is placed in @script_index.
 *
 * All other functions taking an input script_index parameter know
 * how to handle %NS_PANGO_OT_NO_SCRIPT, so one can ignore the return
 * value of this function completely and proceed, to enjoy the automatic
 * fallback to the 'DFLT'/'dflt' script.
 *
 * Return value: %TRUE if the script was found
 */
gboolean
ns_pango_ot_info_find_script (NsPangoOTInfo      *info,
			   NsPangoOTTableType  table_type,
			   NsPangoOTTag        script_tag,
			   guint            *script_index)
{
  hb_tag_t tt = get_hb_table_type (table_type);

  return hb_ot_layout_table_find_script (info->hb_face, tt,
					 script_tag,
					 script_index);
}

/**
 * ns_pango_ot_info_find_language:
 * @info: a `NsPangoOTInfo`
 * @table_type: the table type to obtain information about
 * @script_index: the index of the script whose languages are searched
 * @language_tag: the tag of the language to find
 * @language_index: (out) (optional): location to store the index of the language
 * @required_feature_index: (out) (optional): location to store the
 *   required feature index of the language
 *
 * Finds the index of a language and its required feature index.
 *
 * If the language is not found, sets @language_index to %NS_PANGO_OT_DEFAULT_LANGUAGE
 * and the required feature of the default language system is returned in
 * required_feature_index. For best compatibility with some fonts, also
 * searches the language system tag 'dflt' before falling back to the default
 * language system, but that is transparent to the user. The user can simply
 * ignore the return value of this function to automatically fall back to the
 * default language system.
 *
 * Return value: %TRUE if the language was found
 */
gboolean
ns_pango_ot_info_find_language (NsPangoOTInfo      *info,
			     NsPangoOTTableType  table_type,
			     guint             script_index,
			     NsPangoOTTag        language_tag,
			     guint            *language_index,
			     guint            *required_feature_index)
{
  gboolean ret;
  guint l_index;
  hb_tag_t tt = get_hb_table_type (table_type);

  ret = hb_ot_layout_script_select_language (info->hb_face,
                                             table_type,
                                             script_index,
                                             1,
                                             &language_tag,
                                             &l_index);
  if (language_index)
    *language_index = l_index;

  hb_ot_layout_language_get_required_feature_index (info->hb_face, tt,
						    script_index,
						    l_index,
						    required_feature_index);

  return ret;
}

/**
 * ns_pango_ot_info_find_feature:
 * @info: a `NsPangoOTInfo`
 * @table_type: the table type to obtain information about
 * @feature_tag: the tag of the feature to find
 * @script_index: the index of the script
 * @language_index: the index of the language whose features are searched,
 *   or %NS_PANGO_OT_DEFAULT_LANGUAGE to use the default language of the script
 * @feature_index: (out) (optional): location to store the index of
 *   the feature
 *
 * Finds the index of a feature.
 *
 * If the feature is not found, sets @feature_index to NS_PANGO_OT_NO_FEATURE,
 * which is safe to pass to [method@NsPangoOT.Ruleset.add_feature] and similar
 * functions.
 *
 * In the future, this may set @feature_index to an special value that if
 * used in [method@NsPangoOT.Ruleset.add_feature] will ask Pango to synthesize
 * the requested feature based on Unicode properties and data. However, this
 * function will still return %FALSE in those cases. So, users may want to
 * ignore the return value of this function in certain cases.
 *
 * Return value: %TRUE if the feature was found
 */
gboolean
ns_pango_ot_info_find_feature  (NsPangoOTInfo      *info,
			     NsPangoOTTableType  table_type,
			     NsPangoOTTag        feature_tag,
			     guint             script_index,
			     guint             language_index,
			     guint            *feature_index)
{
  hb_tag_t tt = get_hb_table_type (table_type);

  return hb_ot_layout_language_find_feature (info->hb_face, tt,
					     script_index,
					     language_index,
					     feature_tag,
					     feature_index);
}

/**
 * ns_pango_ot_info_list_scripts:
 * @info: a `NsPangoOTInfo`
 * @table_type: the table type to obtain information about
 *
 * Obtains the list of available scripts.
 *
 * Return value: a newly-allocated zero-terminated
 *   array containing the tags of the available scripts
 */
NsPangoOTTag *
ns_pango_ot_info_list_scripts (NsPangoOTInfo      *info,
			    NsPangoOTTableType  table_type)
{
  hb_tag_t tt = get_hb_table_type (table_type);
  NsPangoOTTag *result;
  unsigned int count;

  count = hb_ot_layout_table_get_script_tags (info->hb_face, tt, 0, NULL, NULL);
  result = g_new (NsPangoOTTag, count + 1);
  hb_ot_layout_table_get_script_tags (info->hb_face, tt, 0, &count, result);
  result[count] = 0;

  return result;
}

/**
 * ns_pango_ot_info_list_languages:
 * @info: a `NsPangoOTInfo`
 * @table_type: the table type to obtain information about
 * @script_index: the index of the script to list languages for
 * @language_tag: unused parameter
 *
 * Obtains the list of available languages for a given script.
 *
 * Return value: a newly-allocated zero-terminated
 *   array containing the tags of the available languages
 */
NsPangoOTTag *
ns_pango_ot_info_list_languages (NsPangoOTInfo      *info,
			      NsPangoOTTableType  table_type,
			      guint             script_index,
			      NsPangoOTTag        language_tag G_GNUC_UNUSED)
{
  hb_tag_t tt = get_hb_table_type (table_type);
  NsPangoOTTag *result;
  unsigned int count;

  count = hb_ot_layout_script_get_language_tags (info->hb_face, tt, script_index, 0, NULL, NULL);
  result = g_new (NsPangoOTTag, count + 1);
  hb_ot_layout_script_get_language_tags (info->hb_face, tt, script_index, 0, &count, result);
  result[count] = 0;

  return result;
}

/**
 * ns_pango_ot_info_list_features:
 * @info: a `NsPangoOTInfo`
 * @table_type: the table type to obtain information about
 * @tag: unused parameter
 * @script_index: the index of the script to obtain information about
 * @language_index: the index of the language to list features for, or
 *   %NS_PANGO_OT_DEFAULT_LANGUAGE, to list features for the default
 *   language of the script
 *
 * Obtains the list of features for the given language of the given script.
 *
 * Return value: a newly-allocated zero-terminated
 *   array containing the tags of the available features
 */
NsPangoOTTag *
ns_pango_ot_info_list_features  (NsPangoOTInfo      *info,
			      NsPangoOTTableType  table_type,
			      NsPangoOTTag        tag G_GNUC_UNUSED,
			      guint             script_index,
			      guint             language_index)
{
  hb_tag_t tt = get_hb_table_type (table_type);
  NsPangoOTTag *result;
  unsigned int count;

  count = hb_ot_layout_language_get_feature_tags (info->hb_face, tt, script_index, language_index, 0, NULL, NULL);
  result = g_new (NsPangoOTTag, count + 1);
  hb_ot_layout_language_get_feature_tags (info->hb_face, tt, script_index, language_index, 0, &count, result);
  result[count] = 0;

  return result;
}
