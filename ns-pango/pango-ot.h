/* Pango
 * pango-ot.h:
 *
 * Copyright (C) 2000,2007 Red Hat Software
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

#ifndef __PANGO_OT_H__
#define __PANGO_OT_H__

/* Deprecated.  Use HarfBuzz directly! */

#include <ns-pango/pangofc-font.h>
#include <ns-pango/pango-glyph.h>
#include <ns-pango/pango-font.h>
#include <ns-pango/pango-script.h>
#include <ns-pango/pango-language.h>

#include <ft2build.h>
#include FT_FREETYPE_H

G_BEGIN_DECLS

#ifndef NS_PANGO_DISABLE_DEPRECATED

/**
 * NsPangoOTTag:
 *
 * The `NsPangoOTTag` typedef is used to represent TrueType and OpenType
 * four letter tags inside Pango. Use NS_PANGO_OT_TAG_MAKE()
 * or NS_PANGO_OT_TAG_MAKE_FROM_STRING() macros to create NsPangoOTTags manually.
 */
typedef guint32 NsPangoOTTag;

/**
 * NS_PANGO_OT_TAG_MAKE:
 * @c1: First character.
 * @c2: Second character.
 * @c3: Third character.
 * @c4: Fourth character.
 *
 * Creates a `NsPangoOTTag` from four characters.  This is similar and
 * compatible with the FT_MAKE_TAG() macro from FreeType.
 */
/**
 * NS_PANGO_OT_TAG_MAKE_FROM_STRING:
 * @s: The string representation of the tag.
 *
 * Creates a `NsPangoOTTag` from a string. The string should be at least
 * four characters long (pad with space characters if needed), and need
 * not be nul-terminated.  This is a convenience wrapper around
 * NS_PANGO_OT_TAG_MAKE(), but cannot be used in certain situations, for
 * example, as a switch expression, as it dereferences pointers.
 */
#define NS_PANGO_OT_TAG_MAKE(c1,c2,c3,c4)		((NsPangoOTTag) FT_MAKE_TAG (c1, c2, c3, c4))
#define NS_PANGO_OT_TAG_MAKE_FROM_STRING(s)	(NS_PANGO_OT_TAG_MAKE(((const char *) s)[0], \
								   ((const char *) s)[1], \
								   ((const char *) s)[2], \
								   ((const char *) s)[3]))

typedef struct _PangoOTInfo       NsPangoOTInfo;
typedef struct _PangoOTBuffer     NsPangoOTBuffer;
typedef struct _PangoOTGlyph      NsPangoOTGlyph;
typedef struct _PangoOTRuleset    NsPangoOTRuleset;
typedef struct _PangoOTFeatureMap NsPangoOTFeatureMap;
typedef struct _PangoOTRulesetDescription NsPangoOTRulesetDescription;

/**
 * NsPangoOTTableType:
 * @NS_PANGO_OT_TABLE_GSUB: The GSUB table.
 * @NS_PANGO_OT_TABLE_GPOS: The GPOS table.
 *
 * The NsPangoOTTableType enumeration values are used to
 * identify the various OpenType tables in the
 * ns_pango_ot_info_… functions.
 */
typedef enum
{
  NS_PANGO_OT_TABLE_GSUB,
  NS_PANGO_OT_TABLE_GPOS
} NsPangoOTTableType;

/**
 * NS_PANGO_OT_ALL_GLYPHS:
 *
 * This is used as the property bit in ns_pango_ot_ruleset_add_feature() when a
 * feature should be applied to all glyphs.
 *
 * Since: 1.16
 */
/**
 * NS_PANGO_OT_NO_FEATURE:
 *
 * This is used as a feature index that represent no feature, that is, should be
 * skipped.  It may be returned as feature index by ns_pango_ot_info_find_feature()
 * if the feature is not found, and ns_pango_ot_ruleset_add_feature() function
 * automatically skips this value, so no special handling is required by the user.
 *
 * Since: 1.18
 */
/**
 * NS_PANGO_OT_NO_SCRIPT:
 *
 * This is used as a script index that represent no script, that is, when the
 * requested script was not found, and a default ('DFLT') script was not found
 * either.  It may be returned as script index by ns_pango_ot_info_find_script()
 * if the script or a default script are not found, all other functions
 * taking a script index essentially return if the input script index is
 * this value, so no special handling is required by the user.
 *
 * Since: 1.18
 */
/**
 * NS_PANGO_OT_DEFAULT_LANGUAGE:
 *
 * This is used as the language index in ns_pango_ot_info_find_feature() when
 * the default language system of the script is desired.
 *
 * It is also returned by ns_pango_ot_info_find_language() if the requested language
 * is not found, or the requested language tag was NS_PANGO_OT_TAG_DEFAULT_LANGUAGE.
 * The end result is that one can always call ns_pango_ot_tag_from_language()
 * followed by ns_pango_ot_info_find_language() and pass the result to
 * ns_pango_ot_info_find_feature() without having to worry about falling back to
 * default language system explicitly.
 *
 * Since: 1.16
 */
#define NS_PANGO_OT_ALL_GLYPHS			((guint) 0xFFFF)
#define NS_PANGO_OT_NO_FEATURE			((guint) 0xFFFF)
#define NS_PANGO_OT_NO_SCRIPT			((guint) 0xFFFF)
#define NS_PANGO_OT_DEFAULT_LANGUAGE		((guint) 0xFFFF)

/**
 * NS_PANGO_OT_TAG_DEFAULT_SCRIPT:
 *
 * This is a `NsPangoOTTag` representing the special script tag 'DFLT'.  It is
 * returned as script tag by ns_pango_ot_tag_from_script() if the requested script
 * is not found.
 *
 * Since: 1.18
 */
/**
 * NS_PANGO_OT_TAG_DEFAULT_LANGUAGE:
 *
 * This is a `NsPangoOTTag` representing a special language tag 'dflt'.  It is
 * returned as language tag by ns_pango_ot_tag_from_language() if the requested
 * language is not found.  It is safe to pass this value to
 * ns_pango_ot_info_find_language() as that function falls back to returning default
 * language-system if the requested language tag is not found.
 *
 * Since: 1.18
 */
#define NS_PANGO_OT_TAG_DEFAULT_SCRIPT		NS_PANGO_OT_TAG_MAKE ('D', 'F', 'L', 'T')
#define NS_PANGO_OT_TAG_DEFAULT_LANGUAGE		NS_PANGO_OT_TAG_MAKE ('d', 'f', 'l', 't')

/* Note that this must match hb_glyph_info_t */
/**
 * NsPangoOTGlyph:
 * @glyph: the glyph itself.
 * @properties: the properties value, identifying which features should be
 * applied on this glyph.  See ns_pango_ot_ruleset_add_feature().
 * @cluster: the cluster that this glyph belongs to.
 * @component: a component value, set by the OpenType layout engine.
 * @ligID: a ligature index value, set by the OpenType layout engine.
 * @internal: for Pango internal use
 *
 * The `NsPangoOTGlyph` structure represents a single glyph together with
 * information used for OpenType layout processing of the glyph.
 * It contains the following fields.
 */
struct _PangoOTGlyph
{
  guint32  glyph;
  guint    properties;
  guint    cluster;
  gushort  component;
  gushort  ligID;

  guint    internal;
};

/**
 * NsPangoOTFeatureMap:
 * @feature_name: feature tag in represented as four-letter ASCII string.
 * @property_bit: the property bit to use for this feature.  See
 * ns_pango_ot_ruleset_add_feature() for details.
 *
 * The `NsPangoOTFeatureMap` typedef is used to represent an OpenType
 * feature with the property bit associated with it.  The feature tag is
 * represented as a char array instead of a `NsPangoOTTag` for convenience.
 *
 * Since: 1.18
 */
struct _PangoOTFeatureMap
{
  char     feature_name[5];
  gulong   property_bit;
};

/**
 * NsPangoOTRulesetDescription:
 * @script: a `NsPangoScript`
 * @language: a `NsPangoLanguage`
 * @static_gsub_features: (nullable): static map of GSUB features
 * @n_static_gsub_features: length of @static_gsub_features, or 0.
 * @static_gpos_features: (nullable): static map of GPOS features
 * @n_static_gpos_features: length of @static_gpos_features, or 0.
 * @other_features: (nullable): map of extra features to add to both
 *   GSUB and GPOS. Unlike the static maps, this pointer need not
 *   live beyond the life of function calls taking this struct.
 * @n_other_features: length of @other_features, or 0.
 *
 * The `NsPangoOTRuleset` structure holds all the information needed
 * to build a complete `NsPangoOTRuleset` from an OpenType font.
 * The main use of this struct is to act as the key for a per-font
 * hash of rulesets.  The user populates a ruleset description and
 * gets the ruleset using ns_pango_ot_ruleset_get_for_description()
 * or create a new one using ns_pango_ot_ruleset_new_from_description().
 *
 * Since: 1.18
 */
struct _PangoOTRulesetDescription {
  NsPangoScript               script;
  NsPangoLanguage            *language;
  const NsPangoOTFeatureMap  *static_gsub_features;
  guint                   n_static_gsub_features;
  const NsPangoOTFeatureMap  *static_gpos_features;
  guint                   n_static_gpos_features;
  const NsPangoOTFeatureMap  *other_features;
  guint                   n_other_features;
};

#ifdef __GI_SCANNER__
#define NS_PANGO_OT_TYPE_INFO              (ns_pango_ot_info_get_type ())
#define NS_PANGO_OT_INFO(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), NS_PANGO_OT_TYPE_INFO, NsPangoOTInfo))
#define NS_PANGO_OT_IS_INFO(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), NS_PANGO_OT_TYPE_INFO))
#else
#define NS_TYPE_PANGO_OT_INFO              (ns_pango_ot_info_get_type ())
#define NS_PANGO_OT_INFO(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), NS_TYPE_PANGO_OT_INFO, NsPangoOTInfo))
#define NS_PANGO_IS_OT_INFO(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), NS_TYPE_PANGO_OT_INFO))
#endif

NS_PANGO_DEPRECATED
GType ns_pango_ot_info_get_type (void) G_GNUC_CONST;

#ifdef __GI_SCANNER__
#define NS_PANGO_OT_TYPE_RULESET           (ns_pango_ot_ruleset_get_type ())
#define NS_PANGO_OT_RULESET(object)        (G_TYPE_CHECK_INSTANCE_CAST ((object), NS_PANGO_OT_TYPE_RULESET, NsPangoOTRuleset))
#define NS_PANGO_OT_IS_RULESET(object)     (G_TYPE_CHECK_INSTANCE_TYPE ((object), NS_PANGO_OT_TYPE_RULESET))
#else
#define NS_TYPE_PANGO_OT_RULESET           (ns_pango_ot_ruleset_get_type ())
#define NS_PANGO_OT_RULESET(object)        (G_TYPE_CHECK_INSTANCE_CAST ((object), NS_TYPE_PANGO_OT_RULESET, NsPangoOTRuleset))
#define NS_PANGO_IS_OT_RULESET(object)     (G_TYPE_CHECK_INSTANCE_TYPE ((object), NS_TYPE_PANGO_OT_RULESET))
#endif

NS_PANGO_DEPRECATED
GType ns_pango_ot_ruleset_get_type (void) G_GNUC_CONST;


NS_PANGO_DEPRECATED
NsPangoOTInfo *ns_pango_ot_info_get (FT_Face face);

NS_PANGO_DEPRECATED
gboolean ns_pango_ot_info_find_script   (NsPangoOTInfo      *info,
				      NsPangoOTTableType  table_type,
				      NsPangoOTTag        script_tag,
				      guint            *script_index);

NS_PANGO_DEPRECATED
gboolean ns_pango_ot_info_find_language (NsPangoOTInfo      *info,
				      NsPangoOTTableType  table_type,
				      guint             script_index,
				      NsPangoOTTag        language_tag,
				      guint            *language_index,
				      guint            *required_feature_index);
NS_PANGO_DEPRECATED
gboolean ns_pango_ot_info_find_feature  (NsPangoOTInfo      *info,
				      NsPangoOTTableType  table_type,
				      NsPangoOTTag        feature_tag,
				      guint             script_index,
				      guint             language_index,
				      guint            *feature_index);

NS_PANGO_DEPRECATED
NsPangoOTTag *ns_pango_ot_info_list_scripts   (NsPangoOTInfo      *info,
					  NsPangoOTTableType  table_type);
NS_PANGO_DEPRECATED
NsPangoOTTag *ns_pango_ot_info_list_languages (NsPangoOTInfo      *info,
					  NsPangoOTTableType  table_type,
					  guint             script_index,
					  NsPangoOTTag        language_tag);
NS_PANGO_DEPRECATED
NsPangoOTTag *ns_pango_ot_info_list_features  (NsPangoOTInfo      *info,
					  NsPangoOTTableType  table_type,
					  NsPangoOTTag        tag,
					  guint             script_index,
					  guint             language_index);

#ifdef __GI_SCANNER__
#define NS_PANGO_OT_TYPE_BUFFER (ns_pango_ot_buffer_get_type())
#else
#define NS_TYPE_PANGO_OT_BUFFER (ns_pango_ot_buffer_get_type())
#endif

NS_PANGO_DEPRECATED
GType          ns_pango_ot_buffer_get_type   (void) G_GNUC_CONST;

NS_PANGO_DEPRECATED
NsPangoOTBuffer *ns_pango_ot_buffer_new        (NsPangoFcFont       *font);
NS_PANGO_DEPRECATED
void           ns_pango_ot_buffer_destroy    (NsPangoOTBuffer     *buffer);
NS_PANGO_DEPRECATED
void           ns_pango_ot_buffer_clear      (NsPangoOTBuffer     *buffer);
NS_PANGO_DEPRECATED
void           ns_pango_ot_buffer_set_rtl    (NsPangoOTBuffer     *buffer,
					   gboolean           rtl);
NS_PANGO_DEPRECATED
void           ns_pango_ot_buffer_add_glyph  (NsPangoOTBuffer     *buffer,
					   guint              glyph,
					   guint              properties,
					   guint              cluster);
NS_PANGO_DEPRECATED
void           ns_pango_ot_buffer_get_glyphs (const NsPangoOTBuffer  *buffer,
					   NsPangoOTGlyph        **glyphs,
					   int                  *n_glyphs);
NS_PANGO_DEPRECATED
void           ns_pango_ot_buffer_output     (const NsPangoOTBuffer  *buffer,
					   NsPangoGlyphString     *glyphs);

NS_PANGO_DEPRECATED
void           ns_pango_ot_buffer_set_zero_width_marks (NsPangoOTBuffer     *buffer,
						     gboolean           zero_width_marks);

NS_PANGO_DEPRECATED
const NsPangoOTRuleset *ns_pango_ot_ruleset_get_for_description (NsPangoOTInfo                     *info,
							    const NsPangoOTRulesetDescription *desc);
NS_PANGO_DEPRECATED
NsPangoOTRuleset *ns_pango_ot_ruleset_new (NsPangoOTInfo       *info);
NS_PANGO_DEPRECATED
NsPangoOTRuleset *ns_pango_ot_ruleset_new_for (NsPangoOTInfo       *info,
					  NsPangoScript        script,
					  NsPangoLanguage     *language);
NS_PANGO_DEPRECATED
NsPangoOTRuleset *ns_pango_ot_ruleset_new_from_description (NsPangoOTInfo                     *info,
						       const NsPangoOTRulesetDescription *desc);
NS_PANGO_DEPRECATED
void            ns_pango_ot_ruleset_add_feature (NsPangoOTRuleset   *ruleset,
					      NsPangoOTTableType  table_type,
					      guint             feature_index,
					      gulong            property_bit);
NS_PANGO_DEPRECATED
gboolean        ns_pango_ot_ruleset_maybe_add_feature (NsPangoOTRuleset   *ruleset,
						    NsPangoOTTableType  table_type,
						    NsPangoOTTag        feature_tag,
						    gulong            property_bit);
NS_PANGO_DEPRECATED
guint           ns_pango_ot_ruleset_maybe_add_features (NsPangoOTRuleset          *ruleset,
						     NsPangoOTTableType         table_type,
						     const NsPangoOTFeatureMap *features,
						     guint                    n_features);
NS_PANGO_DEPRECATED
guint           ns_pango_ot_ruleset_get_feature_count (const NsPangoOTRuleset   *ruleset,
						    guint                  *n_gsub_features,
						    guint                  *n_gpos_features);

NS_PANGO_DEPRECATED
void            ns_pango_ot_ruleset_substitute  (const NsPangoOTRuleset   *ruleset,
					      NsPangoOTBuffer          *buffer);

NS_PANGO_DEPRECATED
void            ns_pango_ot_ruleset_position    (const NsPangoOTRuleset   *ruleset,
					      NsPangoOTBuffer          *buffer);

NS_PANGO_DEPRECATED
NsPangoScript     ns_pango_ot_tag_to_script     (NsPangoOTTag     script_tag) G_GNUC_CONST;

NS_PANGO_DEPRECATED
NsPangoOTTag      ns_pango_ot_tag_from_script   (NsPangoScript    script) G_GNUC_CONST;

NS_PANGO_DEPRECATED
NsPangoLanguage  *ns_pango_ot_tag_to_language   (NsPangoOTTag     language_tag) G_GNUC_CONST;

NS_PANGO_DEPRECATED
NsPangoOTTag      ns_pango_ot_tag_from_language (NsPangoLanguage *language) G_GNUC_CONST;

#ifdef __GI_SCANNER__
#define NS_PANGO_OT_TYPE_RULESET_DESCRIPTION (ns_pango_ot_ruleset_description_get_type())
#else
#define NS_TYPE_PANGO_OT_RULESET_DESCRIPTION (ns_pango_ot_ruleset_description_get_type())
#endif

NS_PANGO_DEPRECATED
GType           ns_pango_ot_ruleset_description_get_type (void) G_GNUC_CONST;

NS_PANGO_DEPRECATED
guint           ns_pango_ot_ruleset_description_hash  (const NsPangoOTRulesetDescription *desc) G_GNUC_PURE;

NS_PANGO_DEPRECATED
gboolean        ns_pango_ot_ruleset_description_equal (const NsPangoOTRulesetDescription *desc1,
						    const NsPangoOTRulesetDescription *desc2) G_GNUC_PURE;

NS_PANGO_DEPRECATED
NsPangoOTRulesetDescription *ns_pango_ot_ruleset_description_copy  (const NsPangoOTRulesetDescription *desc);

NS_PANGO_DEPRECATED
void            ns_pango_ot_ruleset_description_free  (NsPangoOTRulesetDescription       *desc);


#endif /* NS_PANGO_DISABLE_DEPRECATED */

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NsPangoOTRulesetDescription, ns_pango_ot_ruleset_description_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (NsPangoOTBuffer, ns_pango_ot_buffer_destroy)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (NsPangoOTRuleset, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (NsPangoOTInfo, g_object_unref)

G_END_DECLS

#endif /* __PANGO_OT_H__ */
