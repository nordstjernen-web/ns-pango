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
 *   ns-text-check dump           print the glyph-level dump of the corpus
 *   ns-text-check bench [iters]  time layout and intrinsic sizing
 */

#include <ns-pango/pangocairo.h>
#include <ns-pango/ns-pango-cache.h>

#include <locale.h>
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
} Mode;

static const Mode modes[] = {
  { "free",      -1, NS_PANGO_WRAP_WORD,      NS_PANGO_ELLIPSIZE_NONE,   FALSE, 0 },
  { "wrap-word", 120, NS_PANGO_WRAP_WORD,      NS_PANGO_ELLIPSIZE_NONE,   FALSE, 0 },
  { "wrap-char", 120, NS_PANGO_WRAP_CHAR,      NS_PANGO_ELLIPSIZE_NONE,   FALSE, 0 },
  { "wrap-both",  60, NS_PANGO_WRAP_WORD_CHAR, NS_PANGO_ELLIPSIZE_NONE,   FALSE, 0 },
  { "justify",   140, NS_PANGO_WRAP_WORD,      NS_PANGO_ELLIPSIZE_NONE,   TRUE,  0 },
  { "ellipsis",  100, NS_PANGO_WRAP_WORD,      NS_PANGO_ELLIPSIZE_END,    FALSE, 0 },
  { "spacing",   130, NS_PANGO_WRAP_WORD,      NS_PANGO_ELLIPSIZE_NONE,   FALSE, 2 },
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

  if (mode->letter_spacing != 0)
    {
      NsPangoAttrList *attrs = ns_pango_attr_list_new ();

      ns_pango_attr_list_insert (attrs,
                                 ns_pango_attr_letter_spacing_new (mode->letter_spacing * NS_PANGO_SCALE));
      ns_pango_layout_set_attributes (layout, attrs);
      ns_pango_attr_list_unref (attrs);
    }

  return layout;
}

static void
dump_layout (const char     *sample_name,
             const char     *font,
             const Mode     *mode,
             NsPangoLayout  *layout)
{
  GSList *lines;
  int line_no = 0;
  int width, height;

  ns_pango_layout_get_size (layout, &width, &height);
  printf ("%s|%s|%s|size %d %d|lines %d\n",
          sample_name, font, mode->name, width, height,
          ns_pango_layout_get_line_count (layout));

  for (lines = ns_pango_layout_get_lines_readonly (layout); lines; lines = lines->next, line_no++)
    {
      NsPangoLayoutLine *line = lines->data;
      NsPangoRectangle ink, logical;
      GSList *runs;
      int run_no = 0;

      ns_pango_layout_line_get_extents (line, &ink, &logical);
      printf ("  line %d start %d len %d dir %d ink %d %d %d %d logical %d %d %d %d\n",
              line_no, line->start_index, line->length, line->resolved_dir,
              ink.x, ink.y, ink.width, ink.height,
              logical.x, logical.y, logical.width, logical.height);

      for (runs = line->runs; runs; runs = runs->next, run_no++)
        {
          NsPangoLayoutRun *run = runs->data;
          NsPangoFontDescription *desc = ns_pango_font_describe (run->item->analysis.font);
          char *desc_str = ns_pango_font_description_to_string (desc);

          printf ("    run %d offset %d len %d chars %d level %d gravity %d script %d font %s\n",
                  run_no, run->item->offset, run->item->length, run->item->num_chars,
                  run->item->analysis.level, run->item->analysis.gravity,
                  run->item->analysis.script, desc_str);

          for (int i = 0; i < run->glyphs->num_glyphs; i++)
            {
              NsPangoGlyphInfo *gi = &run->glyphs->glyphs[i];

              printf ("      g %d %u w %d o %d %d c %d cs %d col %d\n",
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

static int
do_dump (NsPangoContext *context)
{
  /* Two passes, so that everything the first pass shaped is served from the
   * cache in the second. The dump of both passes must be identical.
   */
  for (int pass = 0; pass < 2; pass++)
    for (unsigned f = 0; f < G_N_ELEMENTS (fonts); f++)
      for (unsigned m = 0; m < G_N_ELEMENTS (modes); m++)
        for (unsigned s = 0; s < G_N_ELEMENTS (samples); s++)
          {
            NsPangoLayout *layout = build_layout (context, fonts[f], samples[s].text, &modes[m]);

            dump_layout (samples[s].name, fonts[f], &modes[m], layout);
            g_object_unref (layout);
          }

  report_stats ();

  return 0;
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
  else
    {
      fprintf (stderr, "usage: %s [dump|bench [iterations]]\n", argv[0]);
      status = 2;
    }

  g_object_unref (context);

  return status;
}
