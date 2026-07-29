/* Pango
 * pango-ot-ruleset.c: Shaping using OpenType features
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

static void ns_pango_ot_ruleset_finalize   (GObject        *object);

/**
 * NsPangoOTRuleset:
 *
 * The `NsPangoOTRuleset` structure holds a set of features selected
 * from the tables in an OpenType font.
 *
 * A feature is an operation such as adjusting glyph positioning
 * that should be applied to a text feature such as a certain
 * type of accent.
 *
 * A `NsPangoOTRuleset` is created with [ctor@NsPangoOT.Ruleset.new],
 * features are added to it with [method@NsPangoOT.Ruleset.add_feature],
 * then it is applied to a `NsPangoGlyphString` with
 * [method@NsPangoOT.Ruleset.position].
 */
G_DEFINE_TYPE (NsPangoOTRuleset, ns_pango_ot_ruleset, G_TYPE_OBJECT);

static void
ns_pango_ot_ruleset_class_init (NsPangoOTRulesetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ns_pango_ot_ruleset_finalize;
}

static void
ns_pango_ot_ruleset_init (NsPangoOTRuleset *ruleset)
{
}

static void
ns_pango_ot_ruleset_finalize (GObject *object)
{
  G_OBJECT_CLASS (ns_pango_ot_ruleset_parent_class)->finalize (object);
}

/**
 * ns_pango_ot_ruleset_get_for_description:
 * @info: a `NsPangoOTInfo`
 * @desc: a `NsPangoOTRulesetDescription`
 *
 * Returns a ruleset for the given OpenType info and ruleset
 * description.
 *
 * Rulesets are created on demand using
 * [ctor@NsPangoOT.Ruleset.new_from_description].
 * The returned ruleset should not be modified or destroyed.
 *
 * The static feature map members of @desc should be alive as
 * long as @info is.
 *
 * Return value: the `NsPangoOTRuleset` for @desc. This object will have
 *   the same lifetime as @info.
 *
 * Since: 1.18
 */
const NsPangoOTRuleset *
ns_pango_ot_ruleset_get_for_description (NsPangoOTInfo                     *info,
				      const NsPangoOTRulesetDescription *desc)
{
  static NsPangoOTRuleset *ruleset; /* MT-safe */

  if (g_once_init_enter (&ruleset))
    g_once_init_leave (&ruleset, g_object_new (NS_TYPE_PANGO_OT_RULESET, NULL));

  return ruleset;
}

/**
 * ns_pango_ot_ruleset_new:
 * @info: a `NsPangoOTInfo`
 *
 * Creates a new `NsPangoOTRuleset` for the given OpenType info.
 *
 * Return value: the newly allocated `NsPangoOTRuleset`
 */
NsPangoOTRuleset *
ns_pango_ot_ruleset_new (NsPangoOTInfo *info)
{
  return g_object_new (NS_TYPE_PANGO_OT_RULESET, NULL);
}

/**
 * ns_pango_ot_ruleset_new_for:
 * @info: a `NsPangoOTInfo`
 * @script: a `NsPangoScript`
 * @language: a `NsPangoLanguage`
 *
 * Creates a new `NsPangoOTRuleset` for the given OpenType info, script, and
 * language.
 *
 * This function is part of a convenience scheme that highly simplifies
 * using a `NsPangoOTRuleset` to represent features for a specific pair of script
 * and language.  So one can use this function passing in the script and
 * language of interest, and later try to add features to the ruleset by just
 * specifying the feature name or tag, without having to deal with finding
 * script, language, or feature indices manually.
 *
 * In addition to what [ctor@NsPangoOT.Ruleset.new] does, this function will:
 *
 * * Find the `NsPangoOTTag` script and language tags associated with
 *   @script and @language using [func@NsPangoOT.tag_from_script] and
 *   [func@NsPangoOT.tag_from_language],
 *
 * * For each of table types %NS_PANGO_OT_TABLE_GSUB and %NS_PANGO_OT_TABLE_GPOS,
 *   find the script index of the script tag found and the language
 *   system index of the language tag found in that script system, using
 *   [method@NsPangoOT.Info.find_script] and [method@NsPangoOT.Info.find_language],
 *
 * * For found language-systems, if they have required feature index,
 *   add that feature to the ruleset using [method@NsPangoOT.Ruleset.add_feature],
 *
 * * Remember found script and language indices for both table types,
 *   and use them in future [method@NsPangoOT.Ruleset.maybe_add_feature] and
 *   [method@NsPangoOT.Ruleset.maybe_add_features].
 *
 * Because of the way return values of [method@NsPangoOT.Info.find_script] and
 * [method@NsPangoOT.Info.find_language] are ignored, this function automatically
 * finds and uses the 'DFLT' script and the default language-system.
 *
 * Return value: the newly allocated `NsPangoOTRuleset`
 *
 * Since: 1.18
 */
NsPangoOTRuleset *
ns_pango_ot_ruleset_new_for (NsPangoOTInfo       *info,
			  NsPangoScript        script,
			  NsPangoLanguage     *language)
{
  return g_object_new (NS_TYPE_PANGO_OT_RULESET, NULL);
}

/**
 * ns_pango_ot_ruleset_new_from_description:
 * @info: a `NsPangoOTInfo`
 * @desc: a `NsPangoOTRulesetDescription`
 *
 * Creates a new `NsPangoOTRuleset` for the given OpenType info and
 * matching the given ruleset description.
 *
 * This is a convenience function that calls [ctor@NsPangoOT.Ruleset.new_for]
 * and adds the static GSUB/GPOS features to the resulting ruleset,
 * followed by adding other features to both GSUB and GPOS.
 *
 * The static feature map members of @desc should be alive as
 * long as @info is.
 *
 * Return value: the newly allocated `NsPangoOTRuleset`
 *
 * Since: 1.18
 */
NsPangoOTRuleset *
ns_pango_ot_ruleset_new_from_description (NsPangoOTInfo                     *info,
				       const NsPangoOTRulesetDescription *desc)
{
  return g_object_new (NS_TYPE_PANGO_OT_RULESET, NULL);
}

/**
 * ns_pango_ot_ruleset_add_feature:
 * @ruleset: a `NsPangoOTRuleset`
 * @table_type: the table type to add a feature to
 * @feature_index: the index of the feature to add
 * @property_bit: the property bit to use for this feature. Used to
 *   identify the glyphs that this feature should be applied to, or
 *   %NS_PANGO_OT_ALL_GLYPHS if it should be applied to all glyphs.
 *
 * Adds a feature to the ruleset.
 */
void
ns_pango_ot_ruleset_add_feature (NsPangoOTRuleset   *ruleset,
			      NsPangoOTTableType  table_type,
			      guint             feature_index,
			      gulong            property_bit)
{
}

/**
 * ns_pango_ot_ruleset_maybe_add_feature:
 * @ruleset: a `NsPangoOTRuleset`
 * @table_type: the table type to add a feature to
 * @feature_tag: the tag of the feature to add
 * @property_bit: the property bit to use for this feature. Used to
 *   identify the glyphs that this feature should be applied to, or
 *   %NS_PANGO_OT_ALL_GLYPHS if it should be applied to all glyphs.
 *
 * This is a convenience function that first tries to find the feature
 * using [method@NsPangoOT.Info.find_feature] and the ruleset script and
 * language passed to [ctor@NsPangoOT.Ruleset.new_for] and if the feature
 * is found, adds it to the ruleset.
 *
 * If @ruleset was not created using [ctor@NsPangoOT.Ruleset.new_for],
 * this function does nothing.
 *
 * Return value: %TRUE if the feature was found and added to ruleset,
 *   %FALSE otherwise
 *
 * Since: 1.18
 */
gboolean
ns_pango_ot_ruleset_maybe_add_feature (NsPangoOTRuleset          *ruleset,
				    NsPangoOTTableType         table_type,
				    NsPangoOTTag               feature_tag,
				    gulong                   property_bit)
{
  return FALSE;
}

/**
 * ns_pango_ot_ruleset_maybe_add_features:
 * @ruleset: a `NsPangoOTRuleset`
 * @table_type: the table type to add features to
 * @features: array of feature name and property bits to add
 * @n_features: number of feature records in @features array
 *
 * This is a convenience function that for each feature in the feature map
 * array @features converts the feature name to a `NsPangoOTTag` feature tag
 * using NS_PANGO_OT_TAG_MAKE() and calls [method@NsPangoOT.Ruleset.maybe_add_feature]
 * on it.
 *
 * Return value: The number of features in @features that were found
 *   and added to @ruleset
 *
 * Since: 1.18
 */
guint
ns_pango_ot_ruleset_maybe_add_features (NsPangoOTRuleset          *ruleset,
				     NsPangoOTTableType         table_type,
				     const NsPangoOTFeatureMap *features,
				     guint                    n_features)
{
  return 0;
}

/**
 * ns_pango_ot_ruleset_get_feature_count:
 * @ruleset: a `NsPangoOTRuleset`
 * @n_gsub_features: (out) (optional): location to store number of GSUB features
 * @n_gpos_features: (out) (optional): location to store number of GPOS features
 *
 * Gets the number of GSUB and GPOS features in the ruleset.
 *
 * Return value: Total number of features in the @ruleset
 *
 * Since: 1.18
 */
guint
ns_pango_ot_ruleset_get_feature_count (const NsPangoOTRuleset   *ruleset,
				    guint                  *n_gsub_features,
				    guint                  *n_gpos_features)
{
  return 0;
}

/**
 * ns_pango_ot_ruleset_substitute:
 * @ruleset: a `NsPangoOTRuleset`
 * @buffer: a `NsPangoOTBuffer`
 *
 * Performs the OpenType GSUB substitution on @buffer using
 * the features in @ruleset.
 *
 * Since: 1.4
 */
void
ns_pango_ot_ruleset_substitute  (const NsPangoOTRuleset *ruleset,
			      NsPangoOTBuffer        *buffer)
{
}

/**
 * ns_pango_ot_ruleset_position:
 * @ruleset: a `NsPangoOTRuleset`
 * @buffer: a `NsPangoOTBuffer`
 *
 * Performs the OpenType GPOS positioning on @buffer using
 * the features in @ruleset.
 *
 * Since: 1.4
 */
void
ns_pango_ot_ruleset_position (const NsPangoOTRuleset *ruleset,
			   NsPangoOTBuffer        *buffer)
{
}


/* ruleset descriptions */

/**
 * ns_pango_ot_ruleset_description_hash:
 * @desc: a ruleset description
 *
 * Computes a hash of a `NsPangoOTRulesetDescription` structure suitable
 * to be used, for example, as an argument to g_hash_table_new().
 *
 * Return value: the hash value
 *
 * Since: 1.18
 */
guint
ns_pango_ot_ruleset_description_hash  (const NsPangoOTRulesetDescription *desc)
{
  return 0;
}

/**
 * ns_pango_ot_ruleset_description_equal:
 * @desc1: a ruleset description
 * @desc2: a ruleset description
 *
 * Compares two ruleset descriptions for equality.
 *
 * Two ruleset descriptions are considered equal if the rulesets
 * they describe are provably identical. This means that their
 * script, language, and all feature sets should be equal.
 *
 * For static feature sets, the array addresses are compared directly,
 * while for other features, the list of features is compared one by
 * one.(Two ruleset descriptions may result in identical rulesets
 * being created, but still compare %FALSE.)
 *
 * Return value: %TRUE if two ruleset descriptions are identical,
 *   %FALSE otherwise
 *
 * Since: 1.18
 **/
gboolean
ns_pango_ot_ruleset_description_equal (const NsPangoOTRulesetDescription *desc1,
				    const NsPangoOTRulesetDescription *desc2)
{
  return TRUE;
}

G_DEFINE_BOXED_TYPE (NsPangoOTRulesetDescription, ns_pango_ot_ruleset_description,
                     ns_pango_ot_ruleset_description_copy,
                     ns_pango_ot_ruleset_description_free)

/**
 * ns_pango_ot_ruleset_description_copy:
 * @desc: ruleset description to copy
 *
 * Creates a copy of @desc, which should be freed with
 * [method@NsPangoOT.RulesetDescription.free].
 *
 * Primarily used internally by [func@NsPangoOT.Ruleset.get_for_description]
 * to cache rulesets for ruleset descriptions.
 *
 * Return value: the newly allocated `NsPangoOTRulesetDescription`
 *
 * Since: 1.18
 */
NsPangoOTRulesetDescription *
ns_pango_ot_ruleset_description_copy  (const NsPangoOTRulesetDescription *desc)
{
  NsPangoOTRulesetDescription *copy;

  g_return_val_if_fail (desc != NULL, NULL);

  copy = g_slice_new (NsPangoOTRulesetDescription);

  *copy = *desc;

  return copy;
}

/**
 * ns_pango_ot_ruleset_description_free:
 * @desc: an allocated `NsPangoOTRulesetDescription`
 *
 * Frees a ruleset description allocated by
 * ns_pango_ot_ruleset_description_copy().
 *
 * Since: 1.18
 */
void
ns_pango_ot_ruleset_description_free (NsPangoOTRulesetDescription *desc)
{
  g_slice_free (NsPangoOTRulesetDescription, desc);
}
