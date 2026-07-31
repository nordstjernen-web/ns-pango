# How Northstar uses ns-pango, and what replacing it would take

This describes the boundary between the [Northstar
browser](https://github.com/nordstjernen-web/northstar-browser) and this fork as
it actually is, not as the API allows: which entry points the browser calls,
which it does not, and what a replacement would have to reproduce to be dropped
in.

Everything below was read out of northstar at `859c6b9` and measured by building
it against ns-pango `3ecf2a1`.

## The dependency

Northstar pins this fork as a meson subproject in
`subprojects/ns-pango.wrap`, by commit, and consumes it like this:

```meson
ns_pango_sp = subproject('ns-pango',
  default_options: [
    'default_library=static', 'fontconfig=enabled', 'freetype=enabled',
    'introspection=disabled', 'sysprof=disabled', 'libthai=auto',
    'warning_level=0', 'werror=false',
  ])
pango      = ns_pango_sp.get_variable('libpango_dep')
pangocairo = ns_pango_sp.get_variable('libpangocairo_dep')
```

Three things about that are part of the contract and easy to break by accident:

- It reaches in with `get_variable`, so the **meson variable names**
  `libpango_dep` and `libpangocairo_dep` matter as much as the library names.
  It does not take `libpangoft2_dep`, though the wrap's `[provide]` block lists
  it, so `ns-pangoft2` is only linked transitively.
- The **option names** above must keep existing. `fontconfig`, `freetype`,
  `introspection`, `sysprof` and `libthai` are load-bearing; the options this
  fork deleted (`xft`, `documentation`, `gtk_doc`, `man-pages`,
  `build-examples`) were not passed by anything.
- It builds **static**, which is why a broken shared link went unnoticed for so
  long: an unresolved symbol in an archive member nothing references is not an
  error.

## What it actually calls

100 distinct entry points, 367 call sites, in ten files. The distribution is
lopsided:

| file | calls | what for |
| --- | --- | --- |
| `src/paint.c` | 219 | building a layout per inline box and drawing it through cairo |
| `src/layout.c` | 99 | the same layouts for measurement during box layout |
| `src/js_canvas.c` | 21 | `<canvas>` `fillText`, `measureText` |
| `src/mathml.c` | 13 | MathML runs |
| `src/svg.c` | 12 | SVG `<text>` |
| `src/selection.c` | 7 | caret placement, selection rectangles |
| `src/js_intl.c` | 2 | `Intl.Segmenter`, via `ns_pango_get_log_attrs` |
| `src/font.c` | 2 | telling the fontmap a web font arrived |
| `src/net.c` | 1 | `ns_pango_version_string` in a diagnostics dump |
| `src/appmain.c` | 2 | `ns_pango_cache_get_stats` and `ns_pango_break_cache_stats` for the perf readout |

Grouped by what it is asking for:

- **`NsPangoLayout` as the whole text engine.** `set_text`, `set_width`,
  `set_height`, `set_wrap`, `set_ellipsize`, `set_alignment`, `set_justify`,
  `set_indent`, `set_tabs`, `set_line_spacing`, `set_auto_dir`,
  `set_attributes`, `set_font_description`. Northstar hands over a paragraph and
  a set of constraints and takes back geometry. It never itemises or shapes
  itself.
- **Geometry and hit-testing.** `get_pixel_size`, `get_extents`,
  `get_pixel_extents`, `get_baseline`, `get_line_count`, `get_line_readonly`,
  `get_iter` and the iterator, `index_to_pos`, `xy_to_index`,
  `index_to_line_x`, `line_index_to_x`, `line_get_x_ranges`. This is the caret
  and selection contract, and it is the part with the least slack: a
  replacement that is a pixel out here puts carets in the wrong place.
- **Attributes**, about twenty constructors, as the channel for everything CSS
  says about a span of text: family, weight, style, variant, stretch, absolute
  size, scale, rise, letter spacing, word spacing, foreground, background,
  foreground alpha, underline and its colour, overline and its colour,
  strikethrough and its colour, font features, language, allow-breaks,
  insert-hyphens, shape (for atomic inlines), and a whole font description.
- **Font descriptions**: `new`, `from_string`, `set_family`, `set_style`,
  `set_weight`, `set_stretch`, `set_absolute_size`, `set_variations`, `free`.
- **Fontmap and context**: `ns_pango_cairo_font_map_get_default`,
  `font_map_create_context`, `context_get_metrics` and the ascent/descent/
  approximate-char-width getters off it, `font_map_list_families`,
  `font_family_get_name`, `font_map_get_serial`, `context_set_base_dir`,
  `context_get_base_dir`, `fc_font_map_config_changed`.
- **Drawing**, entirely through cairo: `cairo_show_layout`,
  `cairo_show_layout_line`, `cairo_layout_path`, `cairo_create_layout`,
  `cairo_context_set_font_options`, `cairo_context_get_font_options`.
- **Segmentation**: `ns_pango_get_log_attrs` and `ns_pango_language_from_string`
  for `Intl.Segmenter`.

`NS_PANGO_SCALE` appears 92 times. The 1/1024-of-a-pixel fixed-point convention
is not an implementation detail to the browser; it is the unit of its geometry.

### What it does not call

This is the more useful half of the picture.

- **Nothing below `NsPangoLayout`.** No `ns_pango_shape*`, no
  `ns_pango_itemize*`, no `NsPangoGlyphString`, no `NsPangoItem`, no
  `NsPangoGlyphItem`, no coverage, no `NsPangoRenderer` subclass, no
  `ns_pango_markup`. The entire low-level surface — most of the headers, and
  most of what the fork's own tests exercise — has no consumer in the browser.
- **Almost no struct internals.** The only reach into a Pango struct is
  `NsPangoLayoutLine`'s `start_index` and `length`, in `paint.c` and
  `selection.c`. `NsPangoRectangle` is read field-wise, but that is a plain
  value type. Nothing touches `line->runs`.

### Where web fonts enter

`@font-face` does not go through a Pango API at all. `src/font.c` fetches the
file, converts WOFF to bare SFNT itself, writes it into a cache directory,
and then registers it with **fontconfig** — including generating a fontconfig
`<alias>` XML fragment that maps the CSS family name onto the font's internal
family name and loading it with `FcConfigParseAndLoadFromMemory`. Only then does
it tell this library anything:

```c
NsPangoFontMap *fm = ns_pango_cairo_font_map_get_default();
if (fm && NS_PANGO_IS_FC_FONT_MAP(fm))
    ns_pango_fc_font_map_config_changed(NS_PANGO_FC_FONT_MAP(fm));
```

So fontconfig *is* the browser's font database, and `pangofc-fontmap.h` —
`NS_PANGO_IS_FC_FONT_MAP`, `ns_pango_fc_font_map_config_changed` — is part of the
public contract even though it looks like a backend detail. A replacement that
wanted its own font database would have to take over WOFF handling, family
aliasing and fallback as well, which is a much bigger change than replacing the
layout engine.

## Threading, as it stands

Northstar's parallelism is processes and I/O threads, not text.

- The engine runs in **one process**; `docs/architecture.md` is explicit that
  there is no per-tab or per-origin renderer process in this edition.
- Layout and paint run on the **main loop thread**. The in-process renderer's
  `reader_thread_main` only does blocking socket reads and posts each parsed
  request to the main context with `g_idle_source_new`, so
  `ns_renderer_session_handle` — and everything under it — runs on the main
  thread.
- Threads exist for image decoding, audio, WebSockets, EventSource, the
  watchdog, and Web/Service Workers. None of them call into this library:
  worker realms get a minimal global object and never get `Intl`, so even
  `ns_pango_get_log_attrs` stays on the main thread.

That matters for a replacement in two ways. It is free to be
single-thread-only today. And the reason it cannot usefully be *more* parallel
today is not in the layout engine:

- `NsPangoContext` and `NsPangoLayout` are per-object, so a worker would make
  its own — fine.
- But a worker needs a fontmap, and `ns_pango_cairo_font_map_get_default()`
  hands out **one per thread**, deliberately, because
  `pangofc-fontmap.c`'s six caches (`patterns_hash`, `font_hash`,
  `pattern_hash`, `fontset_hash`, the `fontset_cache` LRU queue and
  `font_face_data_hash`) are mutated with no lock at all. Sharing one races.
- Per-thread fontmaps are safe but give each thread its own `NsPangoFont`
  objects, and the shape cache keys on the font pointer — so threads cannot
  share cached shaping. Measured: one thread populates 3053 cache entries for
  the test corpus where eight threads populate 14290.

So parallel layout would today either race in the fontmap or lose most of the
shape cache while multiplying font memory. **Locking the fontconfig fontmap's
caches is the prerequisite**, and it is the one piece of work that would unlock
real parallel text layout.

`ns-text-check scale` measures how far it gets without that. On four cores,
before the caches were sharded:

| threads | layouts/s | speedup |
| --- | --- | --- |
| 1 | 36 796 | 1.00x |
| 2 | 55 063 | 1.50x |
| 4 | 49 921 | 1.36x |

Four threads were slower than two, and — the number that matters — four
threads with the cache serving ran no faster than four threads with
`NS_PANGO_SHAPE_CACHE=0` (49 921 against 50 820). Every thread missed on
everything, because of the per-thread fonts above; every miss took the one
lock exclusively to insert; and the memcpy of each glyph string ran with all
the other threads queued behind it. The cache had become a semaphore.

Sharding both caches sixteen ways on the high bits of the hash — which the
hash tables' own bucket index does not use — leaves the single-threaded path
alone and gives:

| threads | layouts/s | speedup |
| --- | --- | --- |
| 1 | 36 166 | 1.00x |
| 2 | 60 398 | 1.67x |
| 4 | 100 486 | 2.78x |

Reader locks needed splitting as much as writer locks did: a `GRWLock` reader
lock is an atomic read-modify-write, so one lock for the whole cache put every
lookup on every thread onto the same cache line whether or not any of them
ever wrote.

What is left between 2.78x and 4x is the per-thread fontmap: four threads
still shape everything four times over and hold four copies of it. That is the
fontconfig locking work, and it is still the prerequisite. Note also that none
of this helps Northstar yet — its layout runs on the main loop thread, so
these are the numbers a future parallel layout would start from, not a speedup
the browser sees today.

## Replacing this with an API-compatible implementation

The narrowness of the surface is the headline: a replacement has to satisfy 100
entry points, not the 581 this library exports. But the entry points it does
have to satisfy include the hardest parts of text layout.

### What is genuinely mechanical

- The value types: `NsPangoRectangle`, `NsPangoFontDescription` and its
  setters, `NsPangoAttrList` and the attribute constructors, `NsPangoTabArray`,
  `NsPangoLanguage` interning, the enums, `NS_PANGO_SCALE`. Tedious, bounded,
  and testable in isolation.
- `ns_pango_get_log_attrs`. This is UAX #14 line breaking plus UAX #29 word and
  grapheme segmentation. It is a large amount of table-driven code but it is
  self-contained, deterministic, and has an existing conformance suite in the
  Unicode data files.
- The cairo drawing calls, given a glyph run: `cairo_show_layout` is a loop over
  runs calling `cairo_show_text_glyphs`.

### What is not

- **Line breaking that agrees with the old one.** `pango-layout.c` is 8000
  lines, and the browser's caret positions, selection rectangles, table
  intrinsic sizes and float placement are all downstream of exactly where it
  breaks. "Correct" is not the bar; "identical, or the page reflows differently"
  is. The retry-with-char-breaks path, the hyphenation width estimate that
  deliberately measures with the wrong advances, the trailing-space collapsing,
  the tab-stop interaction with letter spacing — each is a behaviour some page
  depends on.
- **Font matching and fallback.** Not because matching is hard, but because it
  has to keep going through fontconfig with the same results, including
  northstar's alias-injection trick for web fonts.
- **Hit-testing.** `index_to_pos`, `xy_to_index` and `line_get_x_ranges` have to
  agree with the glyph positions to the unit, in bidi text, inside ligatures,
  and across cluster boundaries.
- **Bidi.** Reordering via fribidi is a call; getting the *runs* and their
  visual order to match, including the resolved paragraph direction with
  `set_auto_dir`, is not.

### A route that is actually incremental

The dependency being narrow, and being consumed by variable name, means a
replacement can be introduced without a flag day:

1. Freeze the boundary. Write down the 100 entry points as a header, and
   generate a differential harness: drive both implementations from the same
   corpus and diff geometry, line breaks, caret positions and glyph runs. The
   existing `tests/ns-text-check dump` is that harness in miniature — it already
   prints every glyph of every run of every line, which is the right output to
   compare — but it would need extending to the hit-testing calls.
2. Replace from the bottom, not the top. Take over the pieces with a
   conformance oracle first — segmentation, attribute plumbing, font
   descriptions — and keep this library's line breaker.
3. Replace the line breaker last, behind a runtime switch, and require
   byte-identical dumps on a page corpus before it becomes the default.
4. Keep fontconfig. It is not the interesting part and it is where the
   browser's own font pipeline is already wired.

The step nobody should skip is (1). A text layout engine has no small errors:
either the corpus matches or the browser's carets are wrong on some page nobody
tested.

## Would Rust help?

Honestly: for parts of it, clearly yes; for the part that is hard, not much.

**Where it would pay.** The memory-safety surface here is real. This library
parses attacker-controlled text with pointer arithmetic over UTF-8, does
fixed-point arithmetic where an overflow is a wrong width at best, and hands
buffers between HarfBuzz, FreeType, cairo and fontconfig. The bugs found in this
fork in one review pass were: a process-wide flag that made two threads disagree
about line breaks, six unlocked caches, an unresolved symbol that a static build
hid, and header guards that silently voided a system header. Rust's type system
addresses the second directly — `Send`/`Sync` would have made the shared fontmap
a compile error rather than something to discover with ThreadSanitizer — and
`&str` removes the class of bug where a byte index lands mid-codepoint. Cargo
would also make the Unicode tables a dependency rather than generated headers
checked into the tree.

**Where it would not.** Rust does not make UAX #14 agree with Pango's UAX #14.
The expensive, risky work in this project is behavioural equivalence with an
8000-line line breaker whose exact quirks a browser has been calibrated against,
and that cost is identical in any language. Nor does Rust remove the
dependencies: HarfBuzz, FreeType, cairo and fontconfig would still be there,
now behind `unsafe extern` blocks, so the raw-pointer surface moves rather than
disappears. There is also a real integration cost — the browser is 240k lines of
C consuming a C API through meson, so a Rust implementation still has to export
`extern "C"` with GObject-compatible types, which is the least pleasant way to
write Rust and gives up most of the ergonomics.

**The honest comparison.** There is an existing Rust stack that covers a lot of
this ground — `rustybuzz`, `swash`, `cosmic-text`, `parley` — and the serious
question is not "rewrite in Rust" but "adopt one of those and rewrite
northstar's text layer against its API". That trades a bounded, well-understood
C dependency for a different API and a different set of behaviours, without the
equivalence problem being any smaller, but with a much better long-term
maintenance story than a hand-written engine in any language.

My recommendation, on the evidence in this repository: **do not rewrite the line
breaker, in Rust or otherwise.** The measured wins so far all came from
sharpening what is already here — the caches, the locking, the synthesis path,
the compliance gaps — and there is more of that available, starting with
locking the fontconfig fontmap. If a rewrite does happen, start with the
differential harness, keep fontconfig, and treat "identical output on a page
corpus" as the acceptance test rather than "passes the spec".

## Where the time goes now

Northstar laying a 200 KB page out headless, by inclusive share of the whole
process — parse, cascade, layout, paint and teardown included:

| | share | what is left to take |
| --- | --- | --- |
| `ns_pango_layout_check_lines` | 59.5% | — |
| HarfBuzz shaping | 28.6% | nothing: each distinct run is shaped once and the cache serves the rest |
| `ns_pango_default_break` | 9.2% | nothing: 1120 distinct paragraphs, one pass each, 2492 further passes served from cache |
| `ns_pango_itemize_with_font` | 7.1% | **the last uncached block** |
| glyph extents | 2.4% | little |

Itemization is where a fourth cache would go. It is a pure function of the
text, the base direction, the itemisation attributes and the context's font
description, language and gravity — but the items it returns carry an analysis
that points into the attribute list, and the line breaker splits and mutates
them, so a cache has to hand out copies and the key has to cover the fontmap
serial. That is a much wider correctness surface than the three caches here,
each of which is keyed on bytes and returns a value nothing downstream writes
to.

## Known unresolved

- `pangofc-fontmap.c`'s caches are unlocked; a shared fontmap races. This is
  the prerequisite for parallel layout, and the reason
  `ns_pango_cairo_font_map_get_default()` is per-thread. It is also what caps
  `ns-text-check scale` at 2.78x on four cores: threads still shape everything
  once per thread because they cannot share a font.
- ThreadSanitizer reports races inside fontconfig's own `FcFontSetMatch` and
  `FcFontSetSort` when two fontmaps match concurrently. They do not currently
  change output, and AddressSanitizer is clean.
- Pango splits letter spacing half before and half after each cluster, where
  CSS adds it after each character. Changing it would move every glyph in every
  layout.
- Northstar's `word-spacing` and `letter-spacing` only take `px` values;
  `em`, `rem` and percentages are dropped before they reach this library.
