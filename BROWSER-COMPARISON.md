# This fork against the text stacks in Firefox and Chrome

A review of ns-pango as it stands, next to the two engines that solve the same
problem at the same scale: Gecko's `gfx/thebes` and Blink's
`platform/fonts`. It covers what each caches and how it bounds it, where the
performance actually comes from, which CSS behaviours each can express, and what
each is willing to let an attacker's page reach.

Everything about this fork was read at `8c2a96d`, built here with meson against
glib 2.80.0, HarfBuzz 8.3.0, cairo 1.18.0, fontconfig 2.15.0, FreeType 2.13 and
fribidi 1.0.13, and measured with `tests/ns-text-check` plus two probes written
for this review (appendix). All of the harness passes here — `dump` three ways,
`threads`, `spacing`, `synthesis`. Blink and Gecko were read at their current
`main`/`master`; file names are given so the claims can be checked.

The fork's own delta is small enough to read in full: **+643/-196 lines across 19
upstream files, 1820 lines of new cache code, 782 lines of harness**. That is
what this review is mostly about; the other 60 000 lines are upstream Pango.

## Summary

The caching work is sound in outline and, on the evidence here, worth what it
claims — roughly **1.9x on layout and 1.9x on intrinsic sizing** in a
single-threaded run of the fork's own benchmark. The three caches map exactly
onto the three passes a browser repeats, which is the right decomposition, and
the locking is more careful than Pango has ever been.

Two things need attention. First, **the word-level shape cache has a correctness
bug that reproduces on Liberation Sans and Liberation Serif** — what `Arial`,
`Helvetica` and `Times New Roman` resolve to on a stock Linux font set — because
it assumes shaping never crosses a space. Firefox has an explicit guard for
exactly this case; Blink avoids it by construction. Second,
**`NS_PANGO_SHAPE_CACHE=verify`
stopped covering the path that carries the risk** when caching went
word-at-a-time, so neither the verifier nor CI could have caught the first
problem.

Below that, the structural gap is architectural rather than a bug: both browsers
shape a paragraph **once** and slice the shaped result per line using HarfBuzz's
own safe-to-break flags. This fork reshapes each line, which is why it needs a
word cache at all.

## The three stacks side by side

| role | ns-pango | Blink | Gecko |
| --- | --- | --- | --- |
| segmentation (UAX #14/#29) | `break.c` + `ns-break-cache.c` | `LazyLineBreakIterator`, ICU | `intl/lwbrk`, ICU |
| itemisation | `itemize.c` + `ns-item-cache.c` | `RunSegmenter`, `InlineItemsBuilder` | `gfxScriptItemizer`, `BuildTextRuns` |
| shaping | `shape.c` → HarfBuzz | `HarfBuzzShaper` → HarfBuzz | `gfxHarfBuzzShaper`, `gfxGraphiteShaper` |
| shaped-run cache | `ns-shape-cache.c`, process-wide | `NGShapeCache`, per `SimpleFontData` | `gfxFont::mWordCache`, per `gfxFont` |
| line breaking over shaped text | `pango-layout.c`, reshapes per line | `ShapingLineBreaker` → `ShapeResultView` | `gfxTextRun::BreakAndMeasureText`, no reshape |
| spacing | `pango-glyph-item.c` | `ShapeResultSpacing` | `nsTextFrame::PropertyProvider` |
| font database | fontconfig, one fontmap per thread | `FontCache` + font service (out of process on Linux) | `gfxPlatformFontList` |
| web fonts | fontconfig, registered by the browser | `CSSFontSelector` / `FontFaceCache`, per document | `gfxUserFontSet`, per document |

## Caches

| | keyed on | unit | bound | eviction | invalidation | threads |
| --- | --- | --- | --- | --- | --- | --- |
| ns-pango shape | font ptr, language, level, gravity, script, 4 flag words, hyphen, ≤8 features, text | a word | 20 000 entries / 48 MB, 16 shards | 2-generation sweep, then wholesale | fontmap serial change drops all | `GRWLock` per shard |
| ns-pango break | text (≤64 KB), NUL-free | a paragraph | 8 000 entries / 16 MB, 16 shards | same | never (pure function of text) | `GRWLock` per shard |
| ns-pango item | context ptr + serial, base dir, text (≤64 KB), full attr list | a paragraph | **4 000 entries, no byte bound** | same, entries only | serial change makes entries unreachable | `GRWLock` per shard |
| ns-pango metrics | font description + language | one metrics struct | 512 per context | same | cleared on `context_changed` | per context, unlocked |
| Blink `NGShapeCache` | text, start/end offset, locale, features, direction | an inline item | **2048 entries, ≤30 chars** | weak refs (dies with the `ShapeResult`), or `OnReleaseMemory()` | lives in the font; dies with it | main thread / per-thread fonts |
| Gecko word cache | text, length, flags, script, language, app-units, rounding | a word or a space | 10 000 entries per font (`wordcache.maxentries`), ≤32 chars (`wordcache.charlimit`) | age 3 on a 60 s timer; wholesale flush on overflow | lives in the `gfxFont`; font itself expires | `RWLock` per font |

Read that table for who owns whom, because that is the real difference. In both
browsers the cache lives **inside the font object**, so the font's lifetime
bounds the cache and dropping a font drops its shaping. Here the ownership is
inverted: the key holds a reference on the font (`ns-shape-cache.c:482`), so
**cached words keep fonts alive**, and a page that touched two hundred faces
keeps two hundred `NsPangoFont`s, `FT_Face`s and `hb_font_t`s resident until the
byte budget forces a sweep. The item cache does the same to `NsPangoContext`s
(`ns-item-cache.c:317`), and each pinned context can itself be holding 512
cached metrics.

Three smaller observations on bounds:

- The item cache is bounded by **entry count only**. Its ceiling is therefore
  4 000 × 64 KB = **256 MB of paragraph text**, plus one item list, one context
  reference and one font reference per entry. Measured here on 100 distinct
  60 KB paragraphs, laid out twice each: RSS grew 42.0 MB with the caches on
  against 7.3 MB with them off, and of the three only the item cache had not
  reached a ceiling — the break cache had already swept itself down to 36 entries
  against its byte budget. The item cache is the one that cannot say no.
- The shape cache's per-entry overhead is dominated by a fixed
  `hb_feature_t features[8]` in the key: `sizeof (NsPangoShapeKey)` is **200
  bytes**, 128 of it that array, and it is paid **per cached word**. A
  three-glyph word costs 60 bytes of glyphs and 200 bytes of key. Storing
  features out of line when non-empty would take about a third off the cache at
  no cost to the common path, since almost no run has features at all.
- Neither `ns_pango_cache_clear()` nor `ns_pango_shape_cache_font_map_changed()`
  touches the item cache (`ns-shape-cache.c:762-773`), and
  `ns_pango_item_cache_clear()` has no caller anywhere in the tree. After a web
  font arrives, item entries keyed on the old serial are unreachable but
  retained, and the browser has no way to reclaim them. Both browsers have an
  explicit path for this: Blink's `NGShapeCache` registers as a
  `base::MemoryConsumer` and clears on `OnReleaseMemory()`, and Gecko ages its
  word caches on a 60-second timer whether or not anything is under pressure.
  This fork has no memory-pressure entry point at all.

In fairness, the eviction policy here is **gentler** than Gecko's. A
two-generation sweep that drops what nothing has read is strictly better than
`ClearCachedWordsLocked()`, which is what Gecko does when a font's word cache
passes 10 000 entries (`gfxFont.cpp:3331`). The fork's wholesale fallback only
fires when a sweep failed to free half the budget, which is the case where the
working set really is the whole cache.

## The shape cache assumes shaping never crosses a space. It does.

`segment_starts_here()` (`ns-shape-cache.c:236`) cuts an item before any
character whose left neighbour is a space, and `context_independent()`
(`ns-shape-cache.c:284`) admits an item whose edges sit next to a space or an
isolated ideograph. The stated rule is that a boundary is independent when the
characters either side of it "neither join nor ligate".

Joining and ligating are not the only things that cross a boundary. **Kerning
does**, and in the common Latin fonts it kerns *the space itself*:

```
$ ./spacekern /usr/share/fonts/truetype/liberation/*.ttf
LiberationSans-Regular.ttf: "T A" head 816 vs 871, tail 667 vs 667
LiberationSans-Regular.ttf: 39 of 169 pairs differ when cut at the space
LiberationSerif-Regular.ttf: 65 of 169 pairs differ when cut at the space
```

The advance of `"T "` is 871/1000 em standing alone and 816 when an `A` follows,
because the pair is (space, A) and OpenType puts the adjustment on the first
glyph of the pair — which is in the *previous* piece. The fork's split rule
deliberately keeps a trailing space with the word before it, so that adjustment
lands inside a cached piece and depends on text the piece does not contain.

End to end, through this library:

```
$ ./warmprobe "Liberation Sans 20" "Type A" "of of" "Type of"
text "Type of" width 88064
  ... [3 w=6144 c=4] ...                        <- the space, kerned for "A"
$ NS_PANGO_SHAPE_CACHE=0 ./warmprobe "Liberation Sans 20" "Type A" "of of" "Type of"
text "Type of" width 89088
  ... [3 w=7168 c=4] ...
```

"Type of" comes out **1024 units — a whole pixel — narrower** than it should,
because some earlier paragraph on the page happened to write "Type A". The error
is not deterministic from the paragraph alone: it depends on what else the
process has laid out. That is the worst property a layout bug can have.

This is not an exotic font. Liberation Sans and Liberation Serif are the
metric-compatible stand-ins for Arial and Times New Roman, and on this machine —
a stock Ubuntu font set — fontconfig resolves `Arial`, `Helvetica` and
`Times New Roman` straight to them:

```
$ fc-match Arial
LiberationSans-Regular.ttf: "Liberation Sans" "Regular"
```

So any page that says `font-family: Arial, sans-serif`, which is most of the web,
gets a font this cache mis-measures. Across the 57 font files installed here the
Liberation family was the only one the Latin pairs caught, and DejaVu — which is
what `sans-serif` resolves to, and what CI installs — has no space kern pairs at
all. That is precisely why `ns-text-check dump` is green.

### How the browsers handle the same problem

**Gecko caches by word too, and guards it explicitly.**
`gfxFont::SplitAndInitTextRun` splits on space and no-break space — but only
after asking `SpaceMayParticipateInShaping(script)` (`gfxFont.cpp:1515`). That
calls `CheckForFeaturesInvolvingSpace()`, which walks the font's GSUB and GPOS
looking for lookups that involve the space glyph, memoises the answer on the
`gfxFontEntry` as `mHasSpaceFeatures`, and distinguishes `Kerning` from
`NonKerning` so that kerning only disqualifies the font when kerning is actually
enabled. When it says yes, the run goes to `ShapeTextWithoutWordCache()` and the
word cache is bypassed for that font and script entirely. This is the check this
fork is missing, and it is missing the whole of it.

**Blink does not slice at spaces at all.** `InlineNode::ShapeText` shapes
adjacent items together "as this is required for accurate cross-element shaping"
and hands the shaper the full node as context. Where it does reuse a previously
shaped result across an edit, it does not guess: `GetFirstSafeToReuse()` and
`GetLastSafeToReuse()` (`inline_node.cc:923-960`) walk to a HarfBuzz
safe-to-break offset and additionally skip `max_context` glyphs — 2 normally, 10
for 16-bit text because of ZWJ emoji — with the comment "to handle kerning, e.g.
'AV', we should not reuse last glyph". `NGShapeCache` only caches whole items,
only up to 30 characters, only when `IsNGShapeCacheAllowed()` finds a simple item
structure, and only when the result used no fallback font.

**HarfBuzz answers this question directly.** `HB_GLYPH_FLAG_UNSAFE_TO_CONCAT`
exists for exactly this use: its documentation describes splicing separately
shaped segments and says two pieces may be concatenated only if both are clear of
the flag at the join. Reading that flag off the whole-item shaping in
`insert_segments()` and refusing to store a piece whose first or last cluster
carries it would fix this class of bug at its root, for kerning, for contextual
alternates, for the CJK contextual spacing the fork already found by hand, and
for whatever the next font does — without a per-font table scan, and without
giving up the wins the cache actually delivers. That is the recommendation.

The same reasoning applies, less urgently, to the whole-item rule: an item
boundary that falls between two ideographs is still admitted by
`context_independent()`, even though commit "Never cut an item between two
ideographs" established that Noto Sans CJK adjusts an ideograph's advance against
its neighbour. Item boundaries between two ideographs are rare — they need a font
or attribute change mid-run — but the rule that rejected the cut should reject
the edge.

## Verify mode no longer covers the path that needs verifying

`NS_PANGO_SHAPE_CACHE=verify` is documented as shaping both ways and warning on
any difference. Since caching went word-at-a-time it does not do that for
multi-word items. In `shape_by_segment()` the verifying branch sets `i = 0`
(`shape.c:979`), skips every lookup, shapes the item whole, stores the pieces and
returns `TRUE` — so `ns_pango_shape_cache_matches()` at `shape.c:1086` is never
reached. Only single-segment items (a lone word, CJK runs, hyphenated and
transformed items) are ever compared.

Two consequences. The CI step that diffs `uncached.txt` against `verified.txt` is
close to vacuous, because in verify mode the cache never serves and the dump is
identical by construction — the meaningful check is the `cached` versus
`uncached` diff, which does exercise serving. And the verifier is otherwise
exactly the right tool for the bug above: `matches()` compares a cached entry
against a fresh shaping *in the current context*, which is precisely the
comparison that fails for "Type " reused before "of". Extending the segmented
path to look up, compare and then discard would have caught it on any machine
with Liberation installed.

Worth adding to the corpus regardless: a font that kerns spaces (Liberation), and
a mode that lays paragraphs out in different orders and requires the same
geometry, since order-dependence is the symptom this class of bug produces.

## Where the time goes, and where the browsers get theirs

Measured here, single thread, `ns-text-check bench`:

| | cache on | cache off |
| --- | --- | --- |
| layout | 655.6 ms | 1286.2 ms |
| intrinsic sizing | 34.6 ms | 64.6 ms |

Corpus dump: 34 326 hits, 1 996 misses, 6 390 admissions refused — all six
thousand of them for context dependence, none for length, font or features.
About 15% of all shaping calls are not cacheable under the current rule.

That ~2x is real, and it is bought back from a structural disadvantage. Pango
reshapes an item every time a line boundary cuts it, so a paragraph measured
unconstrained and then laid out at a real width shares no shaped run with itself.
Neither browser has this problem:

- Blink shapes the inline formatting context once and produces per-line
  `ShapeResultView`s — views into the one shaped result — reshaping only the
  neighbourhood of a break when the break offset is not safe-to-break
  (`shaping_line_breaker.cc`, `ShapeToEnd`, `ConcatShapeResults`).
- Gecko builds a text run per span of same-styled text — across inline element
  boundaries, in `BuildTextRuns` — and breaks lines over it without reshaping,
  clipping ligatures that straddle a break rather than shaping again.

So the word cache is compensation for reshaping, not an advance over what the
browsers do. It reaches roughly the same place by a different route, and it pays
for it with the correctness assumption above. A version of `ShapeResultView` —
shaping the paragraph once and slicing per line at safe-to-break offsets — is the
larger prize, and it would make the word cache mostly redundant. It is also a
serious change to an 8000-line line breaker, so the honest ordering is: fix the
admission rule first, keep the cache, and treat slicing as a separate project.

On threading, the integration notes are accurate and the analysis is good: the
fontconfig fontmap's six unlocked caches are the ceiling, per-thread fontmaps
mean threads cannot share a shaped run, and 2.7x on four cores with the cache
against 3.5x without it follows directly. Gecko has done the work this fork has
not: `gfxFont::mWordCache` is `MOZ_GUARDED_BY(mLock)`, shaping happens outside
the lock and only insertion takes it — the same shape as `ns_pango_shape_cache_insert`,
which copies the glyphs before taking the writer lock. Blink sidesteps it by
keeping font objects per-thread. Sharding on the high bits of a mixed hash, which
this fork does and neither browser needs to, is a good answer to a problem the
browsers arranged not to have.

## Features

**word-spacing.** The fork's `ns_pango_glyph_item_word_space()`
(`pango-glyph-item.c:840`) is *more* faithful to CSS Text than either browser:
it uses the seven word separators the spec names. Gecko's
`IsCSSWordSpacingSpace()` (`nsTextFrame.cpp:736`) counts space, no-break space,
and — depending on whether white space is significant — tab, CR and newline;
Blink's `ShapeResultSpacing::ComputeSpacing` uses `Character::TreatAsSpace` with
its own tab and leading-character rules. Neither implements the Ethiopic,
Aegean, Ugaritic or Old Persian separators. Two differences run the other way:
both browsers can put word-spacing on a tab or a newline where this fork never
does, and both exclude a separator followed by a combining mark, where the
cluster-based iteration here would still space it.

**letter-spacing.** Pango splits the spacing half before and half after each
cluster; CSS puts it after each character, and both browsers do that. The
integration notes already list this as unresolved. The bigger gap is that CSS
requires optional ligatures to be suppressed when letter-spacing is non-zero —
Blink implements it in `FontDescription::UpdateTypesettingFeatures()`, quoting the
spec — and nothing in Pango does. A page with `letter-spacing: 1px` on a font
with `liga` renders "fi" as a ligature here and as two glyphs in both browsers.

**hyphens: auto.** Not available. Pango breaks at soft hyphens and inserts the
hyphen glyph, which the fork keys correctly into the cache, but there is no
dictionary hyphenation. Firefox ships hyphenation dictionaries with the browser;
Chrome fetches them through the component updater. A browser on this stack cannot
implement `hyphens: auto` without adding one.

**Cross-element shaping.** `NORTHSTAR-INTEGRATION.md` describes a layout per
inline box, which means shaping cannot cross an inline boundary at all. Blink
goes out of its way to shape across them — open and close tags are opaque to
shaping, and only a real style change breaks the run — and Gecko merges
same-styled text frames into one text run for the same reason. So
`<span>fi</span>rst` ligates in both browsers and cannot here. This is a
browser-side design choice rather than a library limit: this library shapes with
paragraph context whenever it is given one.

**CJK typography.** Blink has `TextAutoSpace`, `text-spacing-trim` and Han
kerning at paragraph start (`han_kerning_start` in `InlineNode::ShapeText`).
Nothing here corresponds. For CJK, this fork's ideograph admission rule is doing
work no browser needs to do, because no browser slices runs that way.

**Emoji.** Both this fork and Blink use the same Chrome-derived Ragel presentation
scanner. The fork's ASCII fast path in `_ns_pango_emoji_iter_init` is sound —
every alternative in the `emoji_presentation` production requires a character
above U+007F, so ASCII text is one non-emoji run by construction — and it skips
both the per-character classification and the scan for the text most pages are
made of. Good change.

**Synthetic bold.** The fix in `pangofc-font.c:976-1024` — telling HarfBuzz about
`FC_EMBOLDEN` so measured advances match what cairo draws — is the same
correction Gecko makes in `gfxFont::PostShapingFixup`, which calls
`ApplyTrackingToClusters(GetSyntheticBoldOffset(), ...)` after shaping. Both
engines had to do this; upstream Pango had not.

**Metrics.** Caching `ns_pango_context_get_metrics` per font description
(`pango-context.c:701-830`) has no direct counterpart — browsers resolve
`line-height: normal` off `SimpleFontData::GetFontMetrics()` / `gfxFont::GetMetrics()`,
which are computed once per font instance rather than per description. Making
`check_fontmap_changed()` run on the metrics path was a real bug fix: a caller
that only ever asked for metrics used to keep pre-webfont answers indefinitely.

## Security

**Process model.** Both browsers put a compromised text stack inside a sandbox
and, since site isolation and Fission, inside a process that holds one site's
data. Northstar runs one process for everything, so this library parses every
origin's text and fonts in the process that also holds every origin's data. That
is the dominating fact, and no amount of care in this library changes it.

It has a direct consequence for the caches added here. They are process-wide and
keyed on text, so a hit is cheap and a miss is not: `measureText` on a
carefully chosen string is a timing oracle for whether some other origin in the
process has laid that string out in that font. Chrome and Firefox get immunity
here for free, because the cache lives in a renderer that only ever sees one
site. If Northstar ever grows per-tab processes this evaporates; until then it is
worth writing down.

**The web font path is the real exposure, and it is on the browser side.** Per
`NORTHSTAR-INTEGRATION.md`, Northstar fetches `@font-face` resources, converts
WOFF to SFNT itself, writes the result into a cache directory, and registers it
with the process-global fontconfig config, generating an `<alias>` fragment
mapping the CSS family name onto the font's internal family name. Compare:

- Chrome runs every web font through **OTS**, which reconstructs a sanitised font
  rather than passing attacker bytes to a parser, keeps the font in the
  document's `FontFaceCache`, and since Chrome 133 parses web fonts with
  **Skrifa**, the Rust font stack, on Linux, Android and ChromeOS — with FreeType
  removed from Blink entirely in Chrome 145.
- Firefox runs every web font through **OTS** as well
  (`gfxUserFontEntry::SanitizeOpenTypeData`, which also does the WOFF decode) and
  scopes it to the document's `gfxUserFontSet`.

Northstar does neither. Attacker-controlled SFNT bytes reach FreeType with no
sanitisation step, in an unsandboxed process. And registering the font with the
**process-global** fontconfig config makes it visible to every document in the
process: a family name loaded by one origin can be named by another with a plain
`font-family` declaration, which is a cross-origin read of a resource the second
origin never fetched and may not be allowed to fetch. Both browsers scope user
fonts to the document precisely to prevent this. Neither problem is in this
repository, but both are in the contract this repository documents, and the
second is the more interesting one because it is a same-process information leak
rather than a memory-safety risk.

**Attacker-controlled text into C.** This library does pointer arithmetic over
UTF-8 and fixed-point arithmetic on widths, and hands buffers between HarfBuzz,
FreeType, cairo and fontconfig. The new code is careful about it: keys compare
the full text rather than trusting a hash (`ns-shape-cache.c:324`,
`ns-break-cache.c:111`), the break cache refuses text containing NUL because
`default_break()` would stop early (`ns-break-cache.c:145`), the fill path
declines to serve when the caller's array is shorter than the stored attributes —
stricter than upstream, which ignores `attrs_len` entirely — and
`insert_segments()` verifies that each piece's glyphs are one contiguous, ordered
stretch before storing anything. The stack cost of the segmented path
(`starts[512]` plus `first[512]` and `count[512]`, about 6 KB across two frames)
is worth knowing about given that layout can re-enter itself through metrics, but
it is bounded.

**Denial of service.** The caches are attacker-influenced allocations. Shape and
break are bounded in bytes; the item cache is not, as above. A page with several
thousand distinct large paragraphs can hold a few hundred megabytes there and the
browser has no call to give it back. Both browsers bound theirs in entries *and*
tie them to an object whose death frees them, and both drop them under memory
pressure.

**Build.** `-Wl,-z,relro -Wl,-z,now` are set; there is no `_FORTIFY_SOURCE` or
`-fstack-protector-strong` in the fork's own flags, so hardening depends on what
the embedder passes. Chrome's official builds compile this layer with CFI, and
Firefox ships it inside a sandbox. Worth passing explicitly rather than
inheriting whatever the embedder happens to set.

## Findings

| # | where | what | severity |
| --- | --- | --- | --- |
| 1 | `ns-shape-cache.c:236`, `:284` | Word slicing assumes no shaping crosses a space; kerning does. Reproduces on Liberation Sans/Serif — a cached piece carries the advance it had next to a different word. Wrong widths, order-dependent | high |
| 2 | `shape.c:979` | `verify` mode skips the segmented path, so the strongest safety net does not cover the riskiest code; the CI's `uncached` vs `verified` diff is vacuous | high |
| 3 | `ns-item-cache.c:49`, `:283` | Item cache bounded by entry count only: ceiling is 4 000 × 64 KB of text plus items, contexts and fonts | medium |
| 4 | `ns-shape-cache.c:762` | `ns_pango_cache_clear()` does not clear the item cache; `ns_pango_item_cache_clear()` has no caller; stale entries survive a fontmap change | medium |
| 5 | library-wide | No memory-pressure or trim entry point. Blink clears `NGShapeCache` on `OnReleaseMemory()`; Gecko ages word caches on a timer | medium |
| 6 | `ns-shape-cache.h:39` | 200-byte key per cached word, 128 bytes of it a features array almost always empty | low |
| 7 | `ns-shape-cache.c:482`, `ns-item-cache.c:317` | Cache entries pin fonts and contexts alive, inverting the ownership both browsers use | low |
| 8 | `ns-shape-cache.c:798` | `ns_pango_cache_get_stats()` writes to stderr as a side effect under `NS_PANGO_CACHE_DEBUG`, against this project's own naming rule that a `get` is side-effect free | low |
| 9 | `ns-break-cache.c:169`, `ns-item-cache.c:177` | `NS_PANGO_SHAPE_CACHE=0` silently disables the break and item caches too; the name and the README say shape | low |
| 10 | `ns-shape-cache.c:781` | Statistics counters are `gint`, wrap silently, and are widened to `guint64` at the API boundary | cosmetic |

Nothing found is a memory-safety defect. The new code is defensive in the places
that matter, and ThreadSanitizer's outstanding reports are, as documented, inside
fontconfig rather than here.

## Recommendations, in order

1. **Admit pieces on HarfBuzz's evidence, not on a Unicode heuristic.** Read
   `HB_GLYPH_FLAG_UNSAFE_TO_CONCAT` from the whole-item shaping and refuse to
   store any piece whose first or last cluster carries it. This is a small change
   in `insert_segments()`, it costs nothing at lookup time, and it retires the
   entire class of bug — kerning, contextual alternates, CJK spacing — instead of
   the members of it someone has thought of. Keep the existing rule as a cheap
   pre-filter if it helps.
2. **Make `verify` verify the segmented path**: look up each piece, compare
   against the fresh shaping, warn and discard on a mismatch. Then add a
   space-kerning font to the CI image and an order-shuffled mode to the corpus.
3. **Give the item cache a byte budget** and wire `ns_pango_item_cache_clear()`
   into `ns_pango_cache_clear()`.
4. **Add a trim entry point** — `ns_pango_cache_trim(level)` — so the browser can
   hand memory back under pressure, as both browsers do.
5. **Store shape-key features out of line.** Takes about a third off the cache's
   memory in the overwhelmingly common case of no features at all.
6. Then, and only as a separate project, consider shaping a paragraph once and
   slicing it per line at safe-to-break offsets, the way Blink's
   `ShapeResultView` does. That is where the remaining factor lives, and it would
   make the word cache incidental rather than load-bearing.

The locking work already done is the prerequisite for none of these; the
fontconfig fontmap remains the prerequisite for parallel layout, and that
assessment in `NORTHSTAR-INTEGRATION.md` still reads correctly.

## Appendix: the two probes

`spacekern.c` — does shaping cross the space? Pure HarfBuzz, no Pango:

```c
/* For each pair, compare the advance of "<a> " inside "<a> <b>" against the
 * advance of "<a> " shaped alone. Any difference means the piece before the
 * space depends on the word after it.
 */
whole_head = shape_advance (font, "T A", 0, 2, NULL);   /* 816 */
alone_head = shape_advance (font, "T ",  0, 2, NULL);   /* 871 */
```

`warmprobe.c` — end to end through this library. Lay out every paragraph given,
dump the glyphs of the last, and run it twice:

```
warmprobe "Liberation Sans 20" "Type A" "of of" "Type of"
NS_PANGO_SHAPE_CACHE=0 warmprobe "Liberation Sans 20" "Type A" "of of" "Type of"
```

The first two paragraphs put `"Type "` and `"of"` in the cache; the third finds
both and never reshapes, so it inherits the space advance that was kerned against
`A`. The dumps differ in the space's width, 6144 against 7168, and in the
paragraph width, 88064 against 89088.

## Sources

- [Blink's text stack](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/platform/fonts/README.md) —
  note that its Word Cache section describes `CachingWordShaper`, which is no
  longer present in `main`; `NGShapeCache` is what exists now.
- Blink: `platform/fonts/shaping/ng_shape_cache.h`, `shaping_line_breaker.cc`,
  `shape_result_spacing.cc`, `font_description.cc`,
  `core/layout/inline/inline_node.cc`, `platform/fonts/simple_font_data.h`.
- Gecko: `gfx/thebes/gfxFont.h`, `gfxFont.cpp`, `gfxUserFontSet.cpp`,
  `layout/generic/nsTextFrame.cpp`, `modules/libpref/init/StaticPrefList.yaml`.
- [HarfBuzz `hb_glyph_flags_t`](https://harfbuzz.github.io/harfbuzz-hb-buffer.html) —
  `HB_GLYPH_FLAG_UNSAFE_TO_BREAK`, `HB_GLYPH_FLAG_UNSAFE_TO_CONCAT`.
- [Memory safety for web fonts](https://developer.chrome.com/blog/memory-safety-fonts) —
  OTS, sandboxing, and the FreeType-to-Skrifa migration.
- [Chromium Linux sandbox IPC](https://github.com/chromium/chromium/blob/main/docs/linux/sandbox_ipc.md) —
  why a renderer does not talk to fontconfig directly.
- [OTS](https://github.com/khaledhosny/ots) — the sanitiser both browsers run
  every web font through.
