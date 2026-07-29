/* Pango
 * pango-ot-tag.h:
 *
 * Copyright (C) 2007 Red Hat Software
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

/**
 * ns_pango_ot_tag_from_script:
 * @script: A `NsPangoScript`
 *
 * Finds the OpenType script tag corresponding to @script.
 *
 * The %NS_PANGO_SCRIPT_COMMON, %NS_PANGO_SCRIPT_INHERITED, and
 * %NS_PANGO_SCRIPT_UNKNOWN scripts are mapped to the OpenType
 * 'DFLT' script tag that is also defined as
 * %NS_PANGO_OT_TAG_DEFAULT_SCRIPT.
 *
 * Note that multiple `NsPangoScript` values may map to the same
 * OpenType script tag.  In particular, %NS_PANGO_SCRIPT_HIRAGANA
 * and %NS_PANGO_SCRIPT_KATAKANA both map to the OT tag 'kana'.
 *
 * Return value: `NsPangoOTTag` corresponding to @script or
 * %NS_PANGO_OT_TAG_DEFAULT_SCRIPT if none found.
 *
 * Since: 1.18
 **/
NsPangoOTTag
ns_pango_ot_tag_from_script (NsPangoScript script)
{
  unsigned int count = 1;
  hb_tag_t tags[1];

  hb_ot_tags_from_script_and_language ((hb_script_t) g_unicode_script_to_iso15924 ((GUnicodeScript) script),
                                       HB_LANGUAGE_INVALID,
                                       &count,
                                       tags,
                                       NULL, NULL);
  if (count > 0)
    return (NsPangoOTTag) tags[0];

  return NS_PANGO_OT_TAG_DEFAULT_SCRIPT;
}

/**
 * ns_pango_ot_tag_to_script:
 * @script_tag: A `NsPangoOTTag` OpenType script tag
 *
 * Finds the `NsPangoScript` corresponding to @script_tag.
 *
 * The 'DFLT' script tag is mapped to %NS_PANGO_SCRIPT_COMMON.
 *
 * Note that an OpenType script tag may correspond to multiple
 * `NsPangoScript` values.  In such cases, the `NsPangoScript` value
 * with the smallest value is returned.
 * In particular, %NS_PANGO_SCRIPT_HIRAGANA
 * and %NS_PANGO_SCRIPT_KATAKANA both map to the OT tag 'kana'.
 * This function will return %NS_PANGO_SCRIPT_HIRAGANA for
 * 'kana'.
 *
 * Return value: `NsPangoScript` corresponding to @script_tag or
 * %NS_PANGO_SCRIPT_UNKNOWN if none found.
 *
 * Since: 1.18
 **/
NsPangoScript
ns_pango_ot_tag_to_script (NsPangoOTTag script_tag)
{
  return (NsPangoScript) g_unicode_script_from_iso15924 (hb_ot_tag_to_script ((hb_tag_t) script_tag));
}


/**
 * ns_pango_ot_tag_from_language:
 * @language: (nullable): A `NsPangoLanguage`
 *
 * Finds the OpenType language-system tag best describing @language.
 *
 * Return value: `NsPangoOTTag` best matching @language or
 * %NS_PANGO_OT_TAG_DEFAULT_LANGUAGE if none found or if @language
 * is %NULL.
 *
 * Since: 1.18
 **/
NsPangoOTTag
ns_pango_ot_tag_from_language (NsPangoLanguage *language)
{
  unsigned int count = 1;
  hb_tag_t tags[1];

  hb_ot_tags_from_script_and_language (HB_SCRIPT_UNKNOWN,
                                       hb_language_from_string (ns_pango_language_to_string (language), -1),
                                       NULL, NULL,
                                       &count, tags);

  if (count > 0)
    return (NsPangoOTTag) tags[0];

  return NS_PANGO_OT_TAG_DEFAULT_LANGUAGE;
}

/**
 * ns_pango_ot_tag_to_language:
 * @language_tag: A `NsPangoOTTag` OpenType language-system tag
 *
 * Finds a `NsPangoLanguage` corresponding to @language_tag.
 *
 * Return value: `NsPangoLanguage` best matching @language_tag or
 * `NsPangoLanguage` corresponding to the string "xx" if none found.
 *
 * Since: 1.18
 **/
NsPangoLanguage *
ns_pango_ot_tag_to_language (NsPangoOTTag language_tag)
{
  return ns_pango_language_from_string (hb_language_to_string (hb_ot_tag_to_language ((hb_tag_t) language_tag)));
}
