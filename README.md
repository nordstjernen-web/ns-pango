ns-pango
========

A fork of [Pango](https://gitlab.gnome.org/GNOME/pango) for the
[Northstar](https://github.com/nordstjernen-web/northstar-browser) browser
engine. It is not a drop-in replacement for Pango and is not meant to be
installed system-wide; Northstar consumes it as a pinned meson subproject.

Five things differ from upstream.

**A cross-layout shaping cache.** Upstream keeps nothing that outlives a
`PangoLayout`, so a browser that measures a run and then paints it hands
the same bytes to HarfBuzz twice, and a table cell is shaped for
`min-content`, for `max-content` and again to lay out. `ns-shape-cache.c`
keeps the finished glyph string in a process-wide hash keyed on everything
`hb_shape` reads, and only for runs whose shaping cannot depend on the text
around them — which includes runs ending at a hyphenated break, and CJK,
where there is no whitespace to key off but ideographs, kana and Hangul
syllables neither join nor ligate. A full cache drops the entries nothing
has read since the last sweep rather than clearing outright.
`pango_context_get_metrics` is likewise cached per font description rather
than only for the context's own, bounded, and dropped when the fontmap
changes. Set `NS_PANGO_SHAPE_CACHE=0` to disable the cache, or `=verify` to
shape both ways and warn on any difference; `NS_PANGO_CACHE_DEBUG=1`
reports why runs were not cached.

**Every symbol is renamed** — `ns_pango_*`, `NsPango*`, `NS_PANGO_*`,
`NS_TYPE_PANGO_*`, and headers under `ns-pango/`. GTK loads the system
Pango into the same process, and GObject aborts when a second library
registers a type name it already holds. `ns-rename.py` performs the whole
rewrite and is idempotent, so run it after merging upstream instead of
editing by hand.

**CSS properties upstream cannot express.** `word-spacing` had no route
through Pango at all: letter spacing goes between every grapheme, so the
nearest a browser could get was an attribute per separator character, which
splits the paragraph into an item per word. `NS_PANGO_ATTR_WORD_SPACING`
and `ns_pango_glyph_item_word_space` add the space to the advance of each
word-separator character, at the seven characters CSS Text names and
nowhere else.

**Fixes for what a browser hits and a widget toolkit does not.** The font
metrics recursion guard was one process-wide flag, so two threads laying
out the same paragraph could break its lines differently — Northstar shapes
off the main thread. A synthesised bold face was measured with HarfBuzz's
unbolded advances and drawn with FreeType's bolded ones, so
`font-synthesis: weight` set too tight, worst in CJK.

**Backends and tooling a browser never links are gone**: Xft,
Win32/DirectWrite, CoreText, the deprecated `pango_ot_*` API, layout
serialization, FT2 rendering and its fontmap, the test suite, examples,
utilities, documentation and GObject-introspection. What remains is the
core text layer with the cairo and fontconfig/FreeType backends.

What replaces the test suite is `tests/ns-text-check`, built with
`-Dbuild-testsuite=true`. `dump` prints every glyph of every run of every
line for a corpus of scripts and wrapping modes, so CI can diff shaping
with the cache serving, off and verifying; `threads` checks that threads
sharing the cache agree with a thread on its own; `spacing` checks
word-spacing against CSS; `synthesis` checks that every family's advances
agree between HarfBuzz, which measures, and cairo, which draws; and
`bench` times laying a paragraph out and measuring it the way intrinsic
sizing does.

Upstream's own description follows.

Pango
=====

Pango is a library for layout and rendering of text, with an emphasis
on internationalization. Pango can be used anywhere that text layout
is needed; however, most of the work on Pango so far has been done using
the GTK widget toolkit as a test platform. Pango forms the core of text
and font handling for GTK.

Pango is designed to be modular; the core Pango layout can be used
with different font backends. There are three basic backends, with
multiple options for rendering with each.

- Client-side fonts using the FreeType and FontConfig libraries.
  Rendering can be with with Cairo or Xft libraries, or directly
  to an in-memory buffer with no additional libraries.
- Native fonts on Microsoft Windows. Rendering can be done via Cairo
  or directly using the native Win32 API.
- Native fonts on MacOS X with the CoreText framework, rendering via
  Cairo.

The integration of Pango with [Cairo](https://cairographics.org)
provides a complete solution with high quality text handling and
graphics rendering.

As well as the low level layout rendering routines, Pango includes
PangoLayout, a high level driver for laying out entire blocks of text,
and routines to assist in editing internationalized text.

For more information about Pango, see:

 https://www.pango.org/

Dependencies
------------
Pango depends on the GLib library; more information about GLib can
be found at https://www.gtk.org/.

To use the Free Software stack backend, Pango depends on the following
libraries:

- [FontConfig](https://www.fontconfig.org) for font discovery,
- [FreeType](https://www.freetype.org) for font access,
- [HarfBuzz](http://www.harfbuzz.org) for complex text shaping
- [fribidi](http://fribidi.org) for bidirectional text handling

Cairo support depends on the [Cairo](https://cairographics.org) library.
The Cairo backend is the preferred backend to use Pango with and is
subject of most of the development in the future.  It has the
advantage that the same code can be used for display and printing.

We suggest using Pango with Cairo as described above, but you can also
do X-specific rendering using the Xft library. The Xft backend uses
version 2 of the Xft library to manage client side fonts. Version 2 of
Xft is available from https://xlibs.freedesktop.org/release/.  You'll
need the libXft package, and possibly the libXrender and renderext
packages as well.  You'll also need FontConfig.

Installation of Pango on Win32 is possible, see README.win32.

License
-------
Most of the code of Pango is licensed under the terms of the
GNU Lesser Public License (LGPL) - see the file COPYING for details.

Versioning
----------

Historically, Pango was following the traditionally even/odd library
versioning scheme where stable releases are marked by even minor
and development releases by an odd minor.

In recent years, Pango development has slowed down so much that it
no longer makes sense to have unstable cycles, or even unstable releases.
Going forward, pango versions will simply be increasing triples, with
no particular significance to the parity of the minor version.
