# The fork keeps no unit tests -- what it needs instead is proof that its caches
# do not change what shaping returns, and a way to see whether they are worth
# their memory. ns-text-check provides both; see the comment at the top of it.

ns_text_check = executable(
  'ns-text-check',
  sources: 'ns-text-check.c',
  dependencies: [ libpango_dep, libpangocairo_dep ],
  include_directories: [ root_inc ],
  c_args: common_cflags,
  install: false,
)

# Shaping the corpus three ways and diffing the glyph dumps is the real check;
# the CI job does that. As a test, assert only that each mode runs clean.
test('dump', ns_text_check, args: [ 'dump' ], timeout: 120)
test('dump-uncached', ns_text_check, args: [ 'dump' ], timeout: 120,
     env: { 'NS_PANGO_SHAPE_CACHE': '0' })
test('dump-verify', ns_text_check, args: [ 'dump' ], timeout: 240,
     env: { 'NS_PANGO_SHAPE_CACHE': 'verify' })
