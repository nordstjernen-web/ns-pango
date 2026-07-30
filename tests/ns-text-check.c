/* ns-pango
 * ns-text-check.c: Equivalence and timing harness for the fork's caches.
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

/* The fork's shape cache must not change what shaping returns. Total line
 * widths are far too weak a check for that: two different glyph strings very
 * often add up to the same advance. So `dump' prints every glyph of every run
 * of every line -- glyph id, advance, offsets and log cluster -- and the CI
 * job diffs that dump with the cache serving, with the cache off, and with the
 * cache verifying. Any difference at all shows up.
 *
 * `bench' times the two things a browser actually does: laying a paragraph out
 * once, and measuring the same text repeatedly the way intrinsic sizing does.
 *
 * Usage:
 *   ns-text-check dump            print the glyph-level dump of the corpus
 *   ns-text-check bench [iters]   time layout and intrinsic sizing
 *   ns-text-check threads [n]     dump the corpus from n threads at once and
 *                                 check they all agree with a lone thread
 *   ns-text-check spacing         check word-spacing against what CSS specifies
 *   ns-text-check synthesis       check every family's advances agree between
 *                                 HarfBuzz, which measures, and cairo, which draws
 */

#include <ns-pango/pangocairo.h>
#include <ns-pango/ns-pango-cache.h>

#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The corpus deliberately covers the shaping behaviours the cache's
 * admission rule reasons about: scripts that join, scripts that reorder,
 * scripts written without spaces, marks and format characters at item
 * boundaries, and the space characters that are not ASCII space.
 */
typedef struct
{
  const char *name;
  const char *text;
} Sample;

static const Sample samples[] = {
  { "latin",          "the quick brown fox jumps over the lazy dog" },
  { "latin-kern",     "AV To W. Yo LT fi ffl 1/2 -- ... Waltz, nymph" },
  { "latin-punct",    "Handgloves 0123456789 punctuation, semicolons; dashes -- done" },
  { "latin-long",     "Pango is a library for layout and rendering of text, with an "
                      "emphasis on internationalization. It can be used anywhere that "
                      "text layout is needed, and forms the core of text and font "
                      "handling for GTK." },
  { "hebrew",         "שלום וברכה" },
  { "hebrew-mixed",   "שלום mixed english 123 וברכה" },
  { "arabic",         "العربية مرحبا "
                      "بالعالم" },
  { "arabic-mixed",   "العربية latin مرحبا" },
  { "devanagari",     "देवनागरी "
                      "क्षितिज" },
  { "thai",           "ภาษาไทยสวัสดี"
                      "ครับ" },
  { "japanese",       "日本語のテキストを"
                      "準備しました" },
  { "japanese-punct", "日本語、テキスト。"
                      "「引用」（注）" },
  { "chinese",        "中文排版测试的文本内容" },
  { "korean",         "한국어 텍스트 배열" },
  { "cjk-latin",      "日本語ABCテキスト123中文" },
  { "nbsp",           "no break space and figure space" },
  { "ideographic-sp", "日本　語　テキスト" },
  { "combining",      "éàôü Å ñ combining" },
  { "zwj-emoji",      "\U0001f469‍\U0001f4bb \U0001f3f4\U000e0067 \U0001f1f3\U0001f1f4 ok" },
  { "varsel",         "✈️ ❤️ 1️⃣ text" },
  { "softhyphen",     "extra­ordinarily hy­phen­ated word" },
  { "tabs",           "col\tone\ttwo\tthree" },
  { "newlines",       "first paragraph\nsecond paragraph\nthird" },
  { "spaces-runs",    "  leading and   interior   and trailing  " },
};

static const char *fonts[] = {
  "sans 16",
  "sans bold 11",
  "serif 22",
  "monospace 13",
};

/* Wrapping, ellipsization and spacing all change how a paragraph is cut into
 * items, which is exactly what the cache's admission rule keys on.
 */
typedef struct
{
  const char        *name;
  int                width;           /* in points; -1 for unconstrained */
  NsPangoWrapMode    wrap;
  NsPangoEllipsizeMode ellipsize;
  gboolean           justify;
  int                letter_spacing;  /* in points */
  int                word_spacing;    /* in points */
} Mode;

static const Mode modes[] = {
  { "free",      -1, NS_PANGO_WRAP_WORD,      NS_PANGO_ELLIPSIZE_NONE,   FALSE, 0, 0 },
  { "wrap-word", 120, NS_PANGO_WRAP_WORD,      NS_PANGO_ELLIPSIZE_NONE,   FALSE, 0, 0 },
  { "wrap-char", 120, NS_PANGO_WRAP_CHAR,      NS_PANGO_ELLIPSIZE_NONE,   FALSE, 0, 0 },
  { "wrap-both",  60, NS_PANGO_WRAP_WORD_CHAR, NS_PANGO_ELLIPSIZE_NONE,   FALSE, 0, 0 },
  { "justify",   140, NS_PANGO_WRAP_WORD,      NS_PANGO_ELLIPSIZE_NONE,   TRUE,  0, 0 },
  { "ellipsis",  100, NS_PANGO_WRAP_WORD,      NS_PANGO_ELLIPSIZE_END,    FALSE, 0, 0 },
  { "spacing",   130, NS_PANGO_WRAP_WORD,      NS_PANGO_ELLIPSIZE_NONE,   FALSE, 2, 0 },
  { "word-space", 130, NS_PANGO_WRAP_WORD,      NS_PANGO_ELLIPSIZE_NONE,   FALSE, 0, 4 },
  { "both-space", 130, NS_PANGO_WRAP_WORD,      NS_PANGO_ELLIPSIZE_NONE,   FALSE, 2, 3 },
};

static NsPangoLayout *
build_layout (NsPangoContext *context,
              const char     *font,
              const char     *text,
              const Mode     *mode)
{
  NsPangoLayout *layout = ns_pango_layout_new (context);
  NsPangoFontDescription *desc = ns_pango_font_description_from_string (font);

  ns_pango_layout_set_font_description (layout, desc);
  ns_pango_font_description_free (desc);

  ns_pango_layout_set_text (layout, text, -1);
  ns_pango_layout_set_wrap (layout, mode->wrap);
  ns_pango_layout_set_ellipsize (layout, mode->ellipsize);
  ns_pango_layout_set_justify (layout, mode->justify);

  if (mode->width > 0)
    ns_pango_layout_set_width (layout, mode->width * NS_PANGO_SCALE);
  else
    ns_pango_layout_set_width (layout, -1);

  if (mode->letter_spacing != 0 || mode->word_spacing != 0)
    {
      NsPangoAttrList *attrs = ns_pango_attr_list_new ();

      if (mode->letter_spacing != 0)
        ns_pango_attr_list_insert (attrs,
                                   ns_pango_attr_letter_spacing_new (mode->letter_spacing * NS_PANGO_SCALE));
      if (mode->word_spacing != 0)
        ns_pango_attr_list_insert (attrs,
                                   ns_pango_attr_word_spacing_new (mode->word_spacing * NS_PANGO_SCALE));
      ns_pango_layout_set_attributes (layout, attrs);
      ns_pango_attr_list_unref (attrs);
    }

  return layout;
}

static void
dump_layout (GString        *out,
             const char     *sample_name,
             const char     *font,
             const Mode     *mode,
             NsPangoLayout  *layout)
{
  GSList *lines;
  int line_no = 0;
  int width, height;

  ns_pango_layout_get_size (layout, &width, &height);
  g_string_append_printf (out, "%s|%s|%s|size %d %d|lines %d\n",
                          sample_name, font, mode->name, width, height,
                          ns_pango_layout_get_line_count (layout));

  for (lines = ns_pango_layout_get_lines_readonly (layout); lines; lines = lines->next, line_no++)
    {
      NsPangoLayoutLine *line = lines->data;
      NsPangoRectangle ink, logical;
      GSList *runs;
      int run_no = 0;

      ns_pango_layout_line_get_extents (line, &ink, &logical);
      g_string_append_printf (out,
                              "  line %d start %d len %d dir %d ink %d %d %d %d logical %d %d %d %d\n",
                              line_no, line->start_index, line->length, line->resolved_dir,
                              ink.x, ink.y, ink.width, ink.height,
                              logical.x, logical.y, logical.width, logical.height);

      for (runs = line->runs; runs; runs = runs->next, run_no++)
        {
          NsPangoLayoutRun *run = runs->data;
          NsPangoFontDescription *desc = ns_pango_font_describe (run->item->analysis.font);
          char *desc_str = ns_pango_font_description_to_string (desc);

          g_string_append_printf (out,
                                  "    run %d offset %d len %d chars %d level %d gravity %d script %d font %s\n",
                                  run_no, run->item->offset, run->item->length,
                                  run->item->num_chars, run->item->analysis.level,
                                  run->item->analysis.gravity,
                                  run->item->analysis.script, desc_str);

          for (int i = 0; i < run->glyphs->num_glyphs; i++)
            {
              NsPangoGlyphInfo *gi = &run->glyphs->glyphs[i];

              g_string_append_printf (out, "      g %d %u w %d o %d %d c %d cs %d col %d\n",
                                      i, gi->glyph, gi->geometry.width,
                                      gi->geometry.x_offset, gi->geometry.y_offset,
                                      run->glyphs->log_clusters[i],
                                      gi->attr.is_cluster_start, gi->attr.is_color);
            }

          g_free (desc_str);
          ns_pango_font_description_free (desc);
        }
    }
}

static void
report_stats (void)
{
  guint64 hits = 0, misses = 0, skipped = 0, entries = 0;

  ns_pango_cache_get_stats (&hits, &misses, &skipped, &entries);
  fprintf (stderr, "hits=%llu misses=%llu skipped=%llu entries=%llu\n",
           (unsigned long long) hits, (unsigned long long) misses,
           (unsigned long long) skipped, (unsigned long long) entries);
}

/* Two passes, so that everything the first pass shaped is served from the cache
 * in the second. Both passes must dump identically.
 */
static GString *
dump_corpus (NsPangoContext *context)
{
  GString *out = g_string_new (NULL);

  for (int pass = 0; pass < 2; pass++)
    for (unsigned f = 0; f < G_N_ELEMENTS (fonts); f++)
      for (unsigned m = 0; m < G_N_ELEMENTS (modes); m++)
        for (unsigned s = 0; s < G_N_ELEMENTS (samples); s++)
          {
            NsPangoLayout *layout = build_layout (context, fonts[f], samples[s].text, &modes[m]);

            dump_layout (out, samples[s].name, fonts[f], &modes[m], layout);
            g_object_unref (layout);
          }

  return out;
}

static int
do_dump (NsPangoContext *context)
{
  GString *out = dump_corpus (context);

  fwrite (out->str, 1, out->len, stdout);
  g_string_free (out, TRUE);

  report_stats ();

  return 0;
}

/* The shape cache is one table for the whole process, and Northstar shapes off
 * the main thread, so every thread has to see the same glyphs as a thread on its
 * own would. Each thread gets its own fontmap and context -- those are
 * per-object, not shared -- and the cache underneath them is what is shared.
 */
typedef struct
{
  char *checksum;
} ThreadResult;

static gpointer
dump_in_thread (gpointer data)
{
  ThreadResult *result = data;
  NsPangoFontMap *map = ns_pango_cairo_font_map_get_default ();
  NsPangoContext *context = ns_pango_font_map_create_context (map);
  GString *out = dump_corpus (context);

  result->checksum = g_compute_checksum_for_string (G_CHECKSUM_SHA256, out->str, out->len);

  g_string_free (out, TRUE);
  g_object_unref (context);

  return NULL;
}

static int
do_threads (NsPangoContext *context,
            int             n_threads)
{
  GString *reference = dump_corpus (context);
  char *want = g_compute_checksum_for_string (G_CHECKSUM_SHA256, reference->str, reference->len);
  GThread **threads = g_new0 (GThread *, n_threads);
  ThreadResult *results = g_new0 (ThreadResult, n_threads);
  int failed = 0;

  for (int i = 0; i < n_threads; i++)
    threads[i] = g_thread_new ("dump", dump_in_thread, &results[i]);

  for (int i = 0; i < n_threads; i++)
    {
      g_thread_join (threads[i]);

      if (g_strcmp0 (results[i].checksum, want) != 0)
        {
          fprintf (stderr, "thread %d dumped %s, wanted %s\n",
                   i, results[i].checksum, want);
          failed = 1;
        }

      g_free (results[i].checksum);
    }

  printf ("%d threads agreed on %s\n", n_threads, want);

  g_free (results);
  g_free (threads);
  g_free (want);
  g_string_free (reference, TRUE);

  report_stats ();

  return failed;
}

/* CSS Text says word-spacing is added at word-separator characters and at no
 * other, so the width of a line has to grow by exactly the spacing times the
 * number of separators in it -- no matter the script or its direction, and
 * whether or not letter spacing is also in play.
 */
typedef struct
{
  const char *name;
  const char *text;
  int         separators;
} SpacingCase;

static const SpacingCase spacing_cases[] = {
  { "one space",         "a b",              1 },
  { "three spaces",      "a b c d",          3 },
  { "leading, trailing", " a b ",            3 },
  { "run of spaces",     "a   b",            3 },
  { "no-break space",    "a b",         1 },
  { "ethiopic",          "ሀ፡ለ", 1 },
  { "tab is not one",    "a\tb",             0 },
  { "ideographic space", "日　本", 0 },
  { "hebrew",            "שלו םב", 1 },
  { "arabic",            "الع مر",  1 },
  { "cjk without any",   "日本語", 0 },
  { "none at all",       "abcdef",           0 },
};

static int
measure_spaced (NsPangoContext *context,
                const char     *text,
                int             word_spacing,
                int             letter_spacing)
{
  NsPangoLayout *layout = ns_pango_layout_new (context);
  NsPangoFontDescription *desc = ns_pango_font_description_from_string (fonts[0]);
  NsPangoAttrList *attrs = ns_pango_attr_list_new ();
  int width = 0;

  ns_pango_layout_set_font_description (layout, desc);
  ns_pango_font_description_free (desc);

  if (word_spacing != 0)
    ns_pango_attr_list_insert (attrs, ns_pango_attr_word_spacing_new (word_spacing));
  if (letter_spacing != 0)
    ns_pango_attr_list_insert (attrs, ns_pango_attr_letter_spacing_new (letter_spacing));
  ns_pango_layout_set_attributes (layout, attrs);
  ns_pango_attr_list_unref (attrs);

  ns_pango_layout_set_text (layout, text, -1);
  ns_pango_layout_get_size (layout, &width, NULL);
  g_object_unref (layout);

  return width;
}

static int
do_spacing (NsPangoContext *context)
{
  const int spacing = 10 * NS_PANGO_SCALE;
  int failed = 0;

  for (unsigned i = 0; i < G_N_ELEMENTS (spacing_cases); i++)
    {
      const SpacingCase *c = &spacing_cases[i];
      int plain = measure_spaced (context, c->text, 0, 0);
      int spaced = measure_spaced (context, c->text, spacing, 0);
      int want = plain + c->separators * spacing;

      printf ("%-20s %d separators: %d -> %d, wanted %d%s\n",
              c->name, c->separators, plain, spaced, want,
              spaced == want ? "" : "  WRONG");

      if (spaced != want)
        failed = 1;
    }

  /* Negative spacing has to take the same amount away. */
  {
    int plain = measure_spaced (context, "a b c", 0, 0);
    int shrunk = measure_spaced (context, "a b c", -2 * NS_PANGO_SCALE, 0);
    int want = plain - 2 * 2 * NS_PANGO_SCALE;

    printf ("%-20s %d -> %d, wanted %d%s\n", "negative", plain, shrunk, want,
            shrunk == want ? "" : "  WRONG");
    if (shrunk != want)
      failed = 1;
  }

  /* Word and letter spacing have to add up, not interfere. */
  {
    int plain = measure_spaced (context, "a b c", 0, 0);
    int lettered = measure_spaced (context, "a b c", 0, 3 * NS_PANGO_SCALE);
    int worded = measure_spaced (context, "a b c", 5 * NS_PANGO_SCALE, 0);
    int both = measure_spaced (context, "a b c", 5 * NS_PANGO_SCALE, 3 * NS_PANGO_SCALE);
    int want = lettered + (worded - plain);

    printf ("%-20s letter %d, word %d, both %d, wanted %d%s\n", "composed",
            lettered, worded, both, want, both == want ? "" : "  WRONG");
    if (both != want)
      failed = 1;
  }

  return failed;
}

/* Pango takes advances from HarfBuzz and hands the glyphs to cairo to draw, so
 * the two have to agree about how wide a glyph is. They stop agreeing as soon as
 * something is synthesised -- fontconfig asking for an emboldened face, which is
 * what CSS font-synthesis: weight comes down to -- because cairo applies the
 * synthesis and HarfBuzz has to be told to. Where they disagree, the glyphs are
 * drawn wider than the space they were measured into and the text sets too
 * tight.
 *
 * Rather than name fonts that happen to lack a bold face, walk every family the
 * fontmap has and check all of them, regular and bold.
 */
static gboolean
advances_agree (NsPangoContext *context,
                const char     *family,
                gboolean        bold,
                const char    **why)
{
  NsPangoFontMap *map = ns_pango_context_get_font_map (context);
  NsPangoFontDescription *desc = ns_pango_font_description_new ();
  NsPangoFont *font;
  hb_font_t *hb_font;
  cairo_scaled_font_t *scaled_font;
  gboolean agree = TRUE;
  /* Latin, Cyrillic, Greek, Hebrew, Arabic, Han, Hiragana, Hangul: enough that
   * every family covers at least one of them.
   */
  static const gunichar probes[] = { 'H', 'n', 0x0416, 0x03A9, 0x05D0, 0x0627, 0x4E2D, 0x3042, 0xAC00 };

  ns_pango_font_description_set_family (desc, family);
  ns_pango_font_description_set_size (desc, 40 * NS_PANGO_SCALE);
  if (bold)
    ns_pango_font_description_set_weight (desc, NS_PANGO_WEIGHT_BOLD);

  font = ns_pango_font_map_load_font (map, context, desc);
  ns_pango_font_description_free (desc);

  if (font == NULL)
    return TRUE;

  scaled_font = ns_pango_cairo_font_get_scaled_font ((NsPangoCairoFont *) font);
  hb_font = ns_pango_font_get_hb_font (font);

  if (scaled_font == NULL || hb_font == NULL)
    {
      g_object_unref (font);
      return TRUE;
    }

  for (unsigned i = 0; i < G_N_ELEMENTS (probes) && agree; i++)
    {
      hb_codepoint_t glyph;
      cairo_glyph_t cairo_glyph;
      cairo_text_extents_t extents;
      double from_hb;

      if (!hb_font_get_nominal_glyph (hb_font, probes[i], &glyph))
        continue;

      cairo_glyph.index = glyph;
      cairo_glyph.x = 0;
      cairo_glyph.y = 0;
      cairo_scaled_font_glyph_extents (scaled_font, &cairo_glyph, 1, &extents);

      from_hb = hb_font_get_glyph_h_advance (hb_font, glyph) / (double) NS_PANGO_SCALE;

      /* They are not expected to match exactly. cairo hints the advance to a
       * whole pixel where FreeType is hinting, which Pango reproduces by
       * rounding what HarfBuzz gives it, and FreeType computes its embolden
       * strength in 26.6 and truncates. Together that is under two thirds of a
       * pixel. A synthesis applied on one side and not the other is a 24th of
       * the em, which at this size is more than two pixels.
       */
      if (fabs (from_hb - extents.x_advance) > 1.0)
        {
          static char detail[256];

          g_snprintf (detail, sizeof detail,
                      "U+%04X: HarfBuzz %.3f px, cairo %.3f px",
                      probes[i], from_hb, extents.x_advance);
          *why = detail;
          agree = FALSE;
        }
    }

  g_object_unref (font);

  return agree;
}

static int
do_synthesis (NsPangoContext *context)
{
  NsPangoFontFamily **families = NULL;
  int n_families = 0;
  int failed = 0;
  int checked = 0;

  ns_pango_font_map_list_families (ns_pango_context_get_font_map (context),
                                  &families, &n_families);

  for (int i = 0; i < n_families; i++)
    {
      const char *family = ns_pango_font_family_get_name (families[i]);

      for (int bold = 0; bold < 2; bold++)
        {
          const char *why = NULL;

          checked++;
          if (!advances_agree (context, family, bold != 0, &why))
            {
              printf ("%s%s: %s\n", family, bold ? " bold" : "", why);
              failed = 1;
            }
        }
    }

  g_free (families);

  printf ("checked %d family and weight combinations: %s\n", checked,
          failed ? "some disagree" : "HarfBuzz and cairo agree");

  return failed;
}

static double
elapsed_ms (GTimer *timer)
{
  return g_timer_elapsed (timer, NULL) * 1000.0;
}

static int
do_bench (NsPangoContext *context,
          int             iters)
{
  GTimer *timer = g_timer_new ();
  double layout_ms, intrinsic_ms;
  int sink = 0;

  /* Laying out every sample in every mode, cold then warm. */
  g_timer_start (timer);
  for (int i = 0; i < iters; i++)
    for (unsigned f = 0; f < G_N_ELEMENTS (fonts); f++)
      for (unsigned m = 0; m < G_N_ELEMENTS (modes); m++)
        for (unsigned s = 0; s < G_N_ELEMENTS (samples); s++)
          {
            NsPangoLayout *layout = build_layout (context, fonts[f], samples[s].text, &modes[m]);
            int w, h;

            ns_pango_layout_get_size (layout, &w, &h);
            sink += w + h;
            g_object_unref (layout);
          }
  layout_ms = elapsed_ms (timer);

  /* What intrinsic sizing does: measure the same text unconstrained, then
   * constrained, then lay it out -- three shaping passes over one string.
   */
  g_timer_start (timer);
  for (int i = 0; i < iters; i++)
    for (unsigned s = 0; s < G_N_ELEMENTS (samples); s++)
      {
        for (unsigned m = 0; m < 3; m++)
          {
            NsPangoLayout *layout = build_layout (context, fonts[0], samples[s].text, &modes[m]);
            int w, h;

            ns_pango_layout_get_size (layout, &w, &h);
            sink += w + h;
            g_object_unref (layout);
          }
      }
  intrinsic_ms = elapsed_ms (timer);

  g_timer_destroy (timer);

  printf ("layout %.1f ms, intrinsic %.1f ms, %d iterations (checksum %d)\n",
          layout_ms, intrinsic_ms, iters, sink);

  report_stats ();

  return 0;
}

int
main (int    argc,
      char **argv)
{
  const char *command = argc > 1 ? argv[1] : "dump";
  NsPangoFontMap *map;
  NsPangoContext *context;
  int status;

  /* Rendering must not depend on the caller's locale. */
  setlocale (LC_ALL, "C");

  map = ns_pango_cairo_font_map_get_default ();
  context = ns_pango_font_map_create_context (map);

  if (strcmp (command, "dump") == 0)
    status = do_dump (context);
  else if (strcmp (command, "bench") == 0)
    status = do_bench (context, argc > 2 ? atoi (argv[2]) : 20);
  else if (strcmp (command, "threads") == 0)
    status = do_threads (context, argc > 2 ? MAX (atoi (argv[2]), 1) : 8);
  else if (strcmp (command, "spacing") == 0)
    status = do_spacing (context);
  else if (strcmp (command, "synthesis") == 0)
    status = do_synthesis (context);
  else
    {
      fprintf (stderr, "usage: %s [dump|bench [iterations]|threads [count]|spacing|synthesis]\n",
               argv[0]);
      status = 2;
    }

  g_object_unref (context);

  return status;
}
