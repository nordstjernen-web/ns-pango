ns_pango_sources = [
  'break.c',
  'ellipsize.c',
  'fonts.c',
  'glyphstring.c',
  'itemize.c',
  'modules.c',
  'pango-attributes.c',
  'pango-bidi-type.c',
  'pango-color.c',
  'pango-context.c',
  'pango-coverage.c',
  'pango-emoji.c',
  'pango-engine.c',
  'pango-fontmap.c',
  'pango-fontset.c',
  'pango-fontset-simple.c',
  'pango-glyph-item.c',
  'pango-gravity.c',
  'pango-item.c',
  'pango-language.c',
  'pango-layout.c',
  'pango-markup.c',
  'pango-matrix.c',
  'pango-renderer.c',
  'pango-script.c',
  'pango-tabs.c',
  'pango-utils.c',
  'reorder-items.c',
  'shape.c',
  'serializer.c',
  'json/gtkjsonparser.c',
  'json/gtkjsonprinter.c',
]

ns_pango_headers = [
  'pango.h',
  'pango-attributes.h',
  'pango-bidi-type.h',
  'pango-break.h',
  'pango-color.h',
  'pango-context.h',
  'pango-coverage.h',
  'pango-direction.h',
  'pango-engine.h',
  'pango-font.h',
  'pango-fontmap.h',
  'pango-fontset.h',
  'pango-fontset-simple.h',
  'pango-glyph.h',
  'pango-glyph-item.h',
  'pango-gravity.h',
  'pango-item.h',
  'pango-language.h',
  'pango-layout.h',
  'pango-matrix.h',
  'pango-markup.h',
  'pango-modules.h',
  'pango-renderer.h',
  'pango-script.h',
  'pango-tabs.h',
  'pango-types.h',
  'pango-utils.h',
]

ns_pango_installed_headers = ns_pango_headers + [ 'pango-version-macros.h' ]

install_headers(ns_pango_installed_headers, subdir: ns_pango_api_path)

# Features header
ns_pango_features_conf = configuration_data()
ns_pango_features_conf.set('NS_PANGO_VERSION_MAJOR', ns_pango_major_version)
ns_pango_features_conf.set('NS_PANGO_VERSION_MINOR', ns_pango_minor_version)
ns_pango_features_conf.set('NS_PANGO_VERSION_MICRO', ns_pango_micro_version)
ns_pango_features_conf.set('NS_PANGO_API_VERSION', ns_pango_api_version)
ns_pango_features_conf.set('NS_PANGO_CURRENT_MINUS_AGE', '0')

ns_pango_features_h = configure_file(
  input: 'pango-features.h.meson',
  output: 'pango-features.h',
  configuration: ns_pango_features_conf,
  install: true,
  install_dir: join_paths(ns_pango_includedir, ns_pango_api_path),
)

# Enumerations for GType
ns_pango_enums = gnome.mkenums(
  'pango-enum-types',
  sources: ns_pango_headers,
  c_template: 'pango-enum-types.c.template',
  h_template: 'pango-enum-types.h.template',
  install_dir: join_paths(ns_pango_includedir, ns_pango_api_path),
  install_header: true,
)
ns_pango_enum_h = ns_pango_enums[1]

if host_system == 'windows'
  ns_pango_rc = configure_file(
    input: 'pango.rc.in',
    output: 'pango.rc',
    configuration: ns_pango_features_conf,
  )
  ns_pango_res = import('windows').compile_resources(ns_pango_rc)
  ns_pango_sources += ns_pango_res
endif

ns_pango_cflags = [
  '-DG_LOG_DOMAIN="NsPango"',
  '-DG_LOG_USE_STRUCTURED=1',
  '-DNS_PANGO_COMPILATION',
  '-DSYSCONFDIR="@0@"'.format(ns_pango_sysconfdir),
  '-DLIBDIR="@0@"'.format(ns_pango_libdir),
]

ns_pango_inc = include_directories('.')

libpango = library(
  ns_pango_api_name,
  sources: ns_pango_sources + ns_pango_enums,
  version: ns_pango_libversion,
  soversion: ns_pango_soversion,
  darwin_versions : ns_pango_osxversion,
  install: true,
  dependencies: ns_pango_deps,
  include_directories: [ root_inc, ns_pango_inc ],
  c_args: common_cflags + ns_pango_debug_cflags + ns_pango_cflags,
  link_args: common_ldflags,
)

ns_pango_dep_sources = [ns_pango_enum_h]


libpango_dep = declare_dependency(
  link_with: libpango,
  include_directories: root_inc,
  dependencies: ns_pango_deps,
  sources: ns_pango_dep_sources,
)
meson.override_dependency('ns-pango', libpango_dep)

ns_pango_pkg_requires = ['gobject-2.0', 'harfbuzz']

pkgconfig.generate(libpango,
  name: 'Pango',
  description: 'Internationalized text handling',
  requires: ns_pango_pkg_requires,
  filebase: 'ns-pango',
  subdirs: ns_pango_api_name,
)

# FreeType
if build_pangoft2
  pangofc_headers = [
    'pangofc-font.h',
    'pangofc-fontmap.h',
    'pangofc-decoder.h',
  ]

  pangofc_public_sources = [
    'pangofc-font.c',
    'pangofc-fontmap.c',
    'pangofc-decoder.c',
    'pango-trace.c',
  ]

  pangoot_headers = [
    'pango-ot.h',
  ]

  pangoot_public_sources = [
    'pango-ot-buffer.c',
    'pango-ot-info.c',
    'pango-ot-ruleset.c',
    'pango-ot-tag.c',
  ]

  pangoft2_headers = [
    'pangoft2.h',
  ]

  pangoft2_public_sources = [
    'pangoft2-fontmap.c',
    'pangoft2-render.c',
    'pangoft2.c',
  ]

  pangoft2_sources = pangofc_public_sources + pangoot_public_sources + pangoft2_public_sources

  if host_system == 'windows'
    pangoft2_rc = configure_file(
      input: 'pangoft2.rc.in',
      output: 'pangoft2.rc',
      configuration: ns_pango_features_conf,
    )
    pangoft2_res = import('windows').compile_resources(pangoft2_rc)
    pangoft2_sources += pangoft2_res
  endif

  install_headers(pangoft2_headers + pangofc_headers + pangoot_headers, subdir: ns_pango_api_path)

  libpangoft2 = library(
    'ns-pangoft2-@0@'.format(ns_pango_api_version),
    sources: pangoft2_sources,
    version: ns_pango_libversion,
    soversion: ns_pango_soversion,
    darwin_versions : ns_pango_osxversion,
    install: true,
    dependencies: ns_pango_deps + [ libpango_dep ],
    include_directories: [ root_inc, ns_pango_inc ],
    c_args: common_cflags + ns_pango_debug_cflags + ns_pango_cflags + [
      '-DNS_PANGO_DISABLE_DEPRECATION_WARNINGS',
    ],
    link_args: common_ldflags,
  )

  pangoft2_dep_sources = []


  libpangoft2_dep = declare_dependency(
    link_with: libpangoft2,
    include_directories: root_inc,
    dependencies: ns_pango_deps + [ libpango_dep ],
    sources: pangoft2_dep_sources,
  )
  meson.override_dependency('ns-pangoft2', libpangoft2_dep)

  pkgconfig.generate(libpangoft2,
    name: 'Pango FT2 and Pango Fc',
    description: 'FreeType 2.0 and fontconfig font support for Pango',
    filebase: 'ns-pangoft2',
    subdirs: ns_pango_api_name,
    libraries: freetype_lib,
    requires: [ 'ns-pango', freetype2_pc, 'fontconfig' ],
  )

  # Since we split the introspection data, we also need a split pkg-config
  # file for Vala
  pkgconfig.generate(
    name: 'Pango OT',
    description: 'OpenType font support for Pango (deprecated)',
    filebase: 'ns-pangoot',
    subdirs: ns_pango_api_name,
    requires: [ 'pangoft2' ],
  )

  pkgconfig.generate(
    name: 'Pango FC',
    description: 'Fontconfig support for Pango',
    filebase: 'ns-pangofc',
    subdirs: ns_pango_api_name,
    requires: [ 'pangoft2' ],
  )
else
  # For usage as a subproject
  libpangoft2_dep = dependency('', required : false)
  # Needed for making ft2 no-op while generating pangoxft_gir below
  pangoft2_gir_dep = dependency('', required : false)
  pangoft2_gir = [[]]
endif

# Xft

# Win32

# Cairo
if cairo_dep.found()
  pangocairo_headers = [
    'pangocairo.h',
  ]

  pangocairo_sources = [
    'pangocairo-context.c',
    'pangocairo-font.c',
    'pangocairo-fontmap.c',
    'pangocairo-render.c',
  ]

  pangocairo_deps = ns_pango_deps + [ libpango_dep ]

  if ns_pango_font_backends.contains('freetype')
    pangocairo_sources += [
      'pangocairo-fcfont.c',
      'pangocairo-fcfontmap.c',
    ]

    pangocairo_deps += libpangoft2_dep
  endif



  install_headers(pangocairo_headers, subdir: ns_pango_api_path)

  libpangocairo = library(
    'ns-pangocairo-@0@'.format(ns_pango_api_version),
    sources: pangocairo_sources,
    version: ns_pango_libversion,
    soversion: ns_pango_soversion,
    darwin_versions : ns_pango_osxversion,
    install: true,
    dependencies: ns_pango_deps + pangocairo_deps,
    include_directories: [ root_inc, ns_pango_inc ],
    c_args: common_cflags + ns_pango_debug_cflags + ns_pango_cflags + [
      '-DNS_PANGO_DISABLE_DEPRECATION_WARNINGS',
    ],
    link_args: common_ldflags
  )
  pangocairo_dep_sources = []


  libpangocairo_dep = declare_dependency(
    link_with: libpangocairo,
    include_directories: root_inc,
    dependencies: ns_pango_deps + [ libpango_dep ],
    sources: pangocairo_dep_sources,
  )
  meson.override_dependency('ns-pangocairo', libpangocairo_dep)
  ns_pango_cairo_requires = [ 'ns-pango', 'cairo' ]

  pkgconfig.generate(libpangocairo,
    name: 'Pango Cairo',
    description: 'Cairo rendering support for Pango',
    filebase: 'ns-pangocairo',
    subdirs: ns_pango_api_name,
    requires: ns_pango_cairo_requires,
  )
else
  # For usage as a subproject
  libpangocairo_dep = dependency('', required: false)
endif
