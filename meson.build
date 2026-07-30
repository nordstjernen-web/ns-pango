project('ns-pango', 'c',
        version: '1.58.0',
        license: 'LGPL-2.1-or-later',
        license_files: 'COPYING',
        default_options: [
          'buildtype=release',
          'warning_level=2',
          'c_std=gnu17',
          'b_ndebug=if-release',
        ],
        meson_version: '>=1.2.0')

add_project_arguments([
    '-D_POSIX_C_SOURCE=200809L',
    '-D_POSIX_THREAD_SAFE_FUNCTIONS',
    '-D_GNU_SOURCE',
  ],
  language: 'c'
)

ns_pango_prefix = get_option('prefix')
ns_pango_libdir = join_paths(ns_pango_prefix, get_option('libdir'))
ns_pango_sysconfdir = join_paths(ns_pango_prefix, get_option('sysconfdir'))
ns_pango_includedir = join_paths(ns_pango_prefix, get_option('includedir'))
ns_pango_datadir = join_paths(ns_pango_prefix, get_option('datadir'))
ns_pango_libexecdir = join_paths(ns_pango_prefix, get_option('libexecdir'))

version = meson.project_version().split('.')
ns_pango_major_version = version[0].to_int()
ns_pango_minor_version = version[1].to_int()
ns_pango_micro_version = version[2].to_int()

ns_pango_interface_age = ns_pango_minor_version.is_odd() ? 0 : ns_pango_micro_version
ns_pango_binary_age = ns_pango_minor_version * 100 + ns_pango_micro_version

ns_pango_api_version = '@0@.0'.format(ns_pango_major_version)
ns_pango_api_name = 'ns-pango-@0@'.format(ns_pango_api_version)
ns_pango_api_path = join_paths(ns_pango_api_name, 'ns-pango')

ns_pango_conf = configuration_data()

ns_pango_conf.set('NS_PANGO_BINARY_AGE', ns_pango_binary_age)
ns_pango_conf.set('NS_PANGO_INTERFACE_AGE', ns_pango_interface_age)
ns_pango_conf.set('NS_PANGO_VERSION_MAJOR', ns_pango_major_version)
ns_pango_conf.set('NS_PANGO_VERSION_MINOR', ns_pango_minor_version)
ns_pango_conf.set('NS_PANGO_VERSION_MICRO', ns_pango_micro_version)

# Maintain version scheme with libtool
ns_pango_soversion = 0
ns_pango_libversion = '@0@.@1@.@2@'.format(ns_pango_soversion, (ns_pango_binary_age - ns_pango_interface_age), ns_pango_interface_age)
osx_current = ns_pango_binary_age - ns_pango_interface_age + 1
ns_pango_osxversion = [osx_current, '@0@.@1@.0'.format(osx_current, ns_pango_interface_age)]

cc = meson.get_compiler('c')
host_system = host_machine.system()
subsystem = ''
cc_argument_syntax = cc.get_argument_syntax()

if host_system == 'windows'
  add_project_arguments('-D_WIN32_WINNT=0x0A00', language: 'c')
elif host_system == 'darwin'
  subsystem = host_machine.subsystem()
endif

# Compiler and linker flags
common_cflags = []
common_ldflags = []

# Add more compiler warnings to the default set
if cc_argument_syntax == 'msvc'
  # Compiler options taken from msvc_recommended_pragmas.h
  # in GLib, based on _Win32_Programming_ by Rector and Newcomer
  test_cflags = ['-FImsvc_recommended_pragmas.h', '-utf-8']
  add_project_arguments(cc.get_supported_arguments(test_cflags), language: 'c')
  test_c_only_flags = []
elif cc_argument_syntax == 'gcc'
  test_c_only_flags = [
    '-Wno-missing-include-dirs',
    '-Wduplicated-branches',
    '-Wduplicated-cond',
    '-Wformat=2',
    '-Wformat-nonliteral',
    '-Wformat-security',
    '-Wignored-qualifiers',
    '-Wimplicit-function-declaration',
    '-Wlogical-op',
    '-Wmisleading-indentation',
    '-Wmissing-format-attribute',
    '-Wmissing-include-dirs',
    '-Wmissing-noreturn',
    '-Wnested-externs',
    '-Wold-style-definition',
    '-Wpointer-arith',
    '-Wshadow',
    '-Wstrict-prototypes',
    '-Wswitch-default',
    '-Wswitch-enum',
    '-Wundef',
    '-Wuninitialized',
    '-Wunused',
    '-Werror=address',
    '-Werror=empty-body',
    '-Werror=implicit',
    '-Werror=implicit-fallthrough',
    '-Werror=init-self',
    '-Werror=int-to-pointer-cast',
    '-Werror=main',
    '-Werror=missing-braces',
    '-Werror=missing-declarations',
    '-Werror=missing-prototypes',
    '-Werror=nonnull',
    '-Werror=pointer-to-int-cast',
    '-Werror=redundant-decls',
    '-Werror=return-type',
    '-Werror=sequence-point',
    '-Werror=trigraphs',
    '-Werror=vla',
    '-Werror=write-strings',
  ]

  test_cflags = test_c_only_flags + [
    '-fno-strict-aliasing',
    '-Wpointer-arith',
    '-Wmissing-declarations',
    '-Wformat=2',
    '-Wformat-nonliteral',
    '-Wformat-security',
    '-Wunused',
    '-Wcast-align',
    '-Wmissing-noreturn',
    '-Wmissing-format-attribute',
    '-Wmissing-include-dirs',
    '-Wlogical-op',
    '-Wno-uninitialized',
    '-Wno-shadow',
    '-Werror=implicit-fallthrough',
    '-Werror=nonnull',
    '-Werror=init-self',
    '-Werror=main',
    '-Werror=missing-braces',
    '-Werror=sequence-point',
    '-Werror=return-type',
    '-Werror=trigraphs',
    '-Werror=write-strings',
    '-Werror=address',
    '-Werror=int-to-pointer-cast',
    '-Werror=empty-body',
    '-Werror=write-strings',
    '-Werror=unused-but-set-variable',
    '-Wundef',
  ]

  if host_system == 'windows'
    test_cflags += [ '-mms-bitfields' ]
  else
    test_cflags += [ '-Werror=redundant-decls' ]
  endif
else
  test_cflags = []
  test_c_only_flags = []
endif

# Symbol visibility
if get_option('default_library') != 'static'
  if host_system == 'windows'
    ns_pango_conf.set('DLL_EXPORT', true)
    ns_pango_conf.set('_PANGO_EXTERN', '__declspec(dllexport) extern')
    if cc_argument_syntax != 'msvc'
      test_cflags += ['-fvisibility=hidden']
    endif
  else
    ns_pango_conf.set('_PANGO_EXTERN', '__attribute__((visibility("default"))) extern')
    test_cflags += ['-fvisibility=hidden']
  endif
endif

# Check all compiler flags
deduplicated_cflags = []
foreach cflag: test_cflags
  if not deduplicated_cflags.contains(cflag)
    deduplicated_cflags += cflag
  endif
endforeach
common_cflags += cc.get_supported_arguments(deduplicated_cflags)

ns_pango_optimized = get_option('optimization') in ['2', '3', 's']
ns_pango_optimization_cflags = []
if ns_pango_optimized
  if cc_argument_syntax == 'msvc'
    ns_pango_optimization_cflags += cc.get_supported_arguments(['/Gy', '/Gw'])
  else
    ns_pango_optimization_cflags += cc.get_supported_arguments([
      '-ffunction-sections',
      '-fdata-sections',
    ])
    if get_option('nordstjernen') and host_system == 'linux'
      ns_pango_optimization_cflags += cc.get_supported_arguments([
        '-fno-semantic-interposition',
        '-fno-plt',
      ])
    endif
  endif
endif
common_cflags += ns_pango_optimization_cflags

# Linker flags
if host_machine.system() == 'linux'
  common_ldflags += cc.get_supported_link_arguments([ '-Wl,-Bsymbolic', '-Wl,-z,relro', '-Wl,-z,now', ])
endif
if ns_pango_optimized
  if cc_argument_syntax == 'msvc'
    common_ldflags += cc.get_supported_link_arguments(['/OPT:REF', '/OPT:ICF'])
  elif host_system == 'darwin'
    common_ldflags += cc.get_supported_link_arguments('-Wl,-dead_strip')
  else
    common_ldflags += cc.get_supported_link_arguments('-Wl,--gc-sections')
  endif
endif

# Use debug/optimization flags to determine whether to enable debug or disable
# cast checks
ns_pango_debug_cflags = []
if get_option('debug') and get_option('b_ndebug') != 'true'
  ns_pango_debug_cflags = [ '-DNS_PANGO_ENABLE_DEBUG' ]
  message('Enabling various debug infrastructure')
elif get_option('optimization') in ['2', '3', 's']
  ns_pango_debug_cflags = [ '-DG_DISABLE_CAST_CHECKS' ]
  message('Disabling cast checks')
  # TODO: We may want a configuration argument to add `G_DISABLE_CHECKS`
  # and `G_DISABLE_ASSERT` from the build, for specific build environments.
  # On the other hand, those who need these symbols can inject them in their
  # build as well.
endif

# Dependencies
ns_pango_deps = []

glib_major_req = 2
glib_minor_req = 80

glib_req       = '>= @0@.@1@'.format(glib_major_req, glib_minor_req)
gi_req         = '>= 1.83.2'
fribidi_req    = '>= 1.0.6'
libthai_req    = '>= 0.1.9'
harfbuzz_req   = '>= 8.3.0'
fontconfig_req = '>= 2.15.0'
cairo_req      = '>= 1.18.0'

add_project_arguments([
    '-DG_LOG_USE_STRUCTURED=1',
    '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_@0@_@1@'.format(glib_major_req, glib_minor_req),
  ],
  language: 'c'
)

mathlib_dep = cc.find_library('m', required: false)
glib_dep = dependency('glib-2.0', version: glib_req)
gobject_dep = dependency('gobject-2.0', version: glib_req)
gio_dep = dependency('gio-2.0', version: glib_req)
gi_dep = dependency('gobject-introspection-1.0', version: gi_req, required: get_option('introspection').enabled())
fribidi_dep = dependency('fribidi', version: fribidi_req, default_options: ['docs=false'])
ns_pango_deps += [mathlib_dep, glib_dep, gobject_dep, gio_dep, fribidi_dep]

thai_dep = dependency('libthai', version: libthai_req, required: get_option('libthai'))
if thai_dep.found()
  ns_pango_conf.set('HAVE_LIBTHAI', 1)
  ns_pango_deps += thai_dep

  if cc.has_function('th_brk_find_breaks', dependencies: thai_dep)
    ns_pango_conf.set('HAVE_TH_BRK_FIND_BREAKS', 1)
  endif
endif

# These are for the various .pc files so that things will link
# properly, depending on whether we have the pkg-config files
# for those non-GNOME dependencies, or when we find them manually
# for MSVC builds, as their MSVC build systems do not generate
# pkg-config files for them
freetype2_pc = ''
freetype_lib = []

# Platforms where fontconfig is required
fontconfig_required = host_system not in ['windows', 'darwin']
# Disable fontconfig by default on platforms where it is optional
fontconfig_option = get_option('fontconfig').disable_auto_if(not fontconfig_required)

harfbuzz_fallback_options = [
  'coretext=enabled',
  'directwrite=enabled',
  'gdi=enabled',
  'cairo=disabled',
  'docs=disabled',
  'utilities=disabled',
  'tests=disabled',
]

# Enable FreeType support in HarfBuzz if building it as a
# subproject and FontConfig support is required or requested
if fontconfig_required or fontconfig_option.enabled()
  harfbuzz_fallback_options += 'freetype=enabled'
else
  harfbuzz_fallback_options += 'freetype=disabled'
endif

harfbuzz_dep = dependency('harfbuzz',
                          version: harfbuzz_req,
                          default_options: harfbuzz_fallback_options)

ns_pango_deps += harfbuzz_dep
# In meson 1.1.0, this check can be replaced with .enable_if() to simplify the
# logic in the fallback handling.
if fontconfig_option.disabled() and fontconfig_required
  error('Fontconfig support cannot be disabled on this platform')
endif

# Override user option 'auto' to 'enabled' on platforms that always
# require fontconfig.
fontconfig_option = fontconfig_required ? true : fontconfig_option
fontconfig_dep = dependency('fontconfig', version: fontconfig_req,
                            required: fontconfig_option)

if fontconfig_dep.found()
  ns_pango_deps += fontconfig_dep
  # Added in fontconfig 2.15.90; Ubuntu 24.04 still ships 2.15.0.
  ns_pango_conf.set('HAVE_FC_CONFIG_SET_DEFAULT_SUBSTITUTE',
                    cc.has_function('FcConfigSetDefaultSubstitute',
                                    dependencies: fontconfig_dep,
                                    prefix: '#include <fontconfig/fontconfig.h>'))
endif

# If option is 'auto' or 'enabled' it is not required to find freetype2 on the
# system because a fallback is done at the end. Override 'disabled' option on
# if fontconfig has been found.
freetype_option = get_option('freetype')
freetype_required = fontconfig_dep.found()
if not freetype_option.disabled() or freetype_required
  freetype_option = false
endif

# The first version of freetype with a pkg-config file is 2.1.5,
# CMake uses 'freetype' rather than 'freetype2' for the package name
# Disable HarfBuzz support in FreeType if HarfBuzz *and* FreeType are
# being built as subprojects
freetype_fallback_options = []
if harfbuzz_dep.type_name() == 'internal'
  freetype_fallback_options = ['harfbuzz=disabled']
endif
freetype_dep = dependency(['freetype2', 'freetype'],
                          required: freetype_option,
                          default_options: freetype_fallback_options)

if freetype_dep.found() and freetype_dep.type_name() in ['pkgconfig', 'internal']
  freetype2_pc = 'freetype2'
else
  freetype_lib = freetype_dep
endif

# To build pangoft2, we need HarfBuzz, FontConfig and FreeType
build_pangoft2 = fontconfig_dep.found() and freetype_dep.found()
if build_pangoft2
  ns_pango_conf.set('HAVE_FREETYPE', 1)
  ns_pango_deps += freetype_dep
endif

# Enable cairo-ft with FreeType and FontConfig support if
# building as a subproject and FontConfig support is required
# or requested
cairo_fallback_options = []
if fontconfig_required or fontconfig_option.enabled()
  cairo_fallback_options = [
    'freetype=enabled',
    'fontconfig=enabled',
  ]
endif

cairo_dep = dependency(
  'cairo',
  version: cairo_req,
  required: get_option('cairo'),
  default_options: cairo_fallback_options,
)
cairo_found_type = cairo_dep.type_name()

# cairo-ft over fontconfig is the only font backend left, so there is nothing to
# choose between: either cairo has it and this builds, or it does not and there
# is no way to load a font.
ns_pango_font_backends = []

if cairo_dep.found()
  if cairo_found_type == 'internal'
    cairo_features = subproject('cairo').get_variable('built_features')
    cairo_ft_dep = dependency('', required: false)
    foreach f: cairo_features
      if f['name'] == 'cairo-ft'
        cairo_ft_dep = cairo_dep
      endif
    endforeach
  else
    cairo_ft_dep = dependency('cairo-ft', version: cairo_req, required: false)
  endif

  if not cairo_ft_dep.found() or not build_pangoft2
    error('ns-pango needs cairo built with FreeType support, plus fontconfig and freetype')
  endif

  if cairo_ft_dep.type_name() != 'internal'
    # cairo-ft can be built without fontconfig, and then it cannot make a font
    # face out of the pattern the fontmap matched.
    if not cc.links('''
#include <cairo-ft.h>
int main() {
  FcPattern *p = NULL;
  cairo_ft_font_face_create_for_pattern (p);
  return 0;
}''',
                    dependencies: cairo_ft_dep,
                    name: 'Cairo is built with FreeType and FontConfig support')
      error('cairo-ft does not have the required FontConfig support')
    endif
  endif

  ns_pango_conf.set('HAVE_CAIRO_FREETYPE', 1)
  ns_pango_conf.set('HAVE_CAIRO', 1)
  ns_pango_font_backends += 'freetype'
  ns_pango_deps += cairo_dep
endif

# introspection


build_gir = false

# libsysprof-capture support
libsysprof_capture_dep = dependency('sysprof-capture-4',
  required: get_option('sysprof'),
  default_options: [
    'enable_examples=false',
    'enable_gtk=false',
    'enable_tests=false',
    'enable_tools=false',
    'libsysprof=false',
    'with_sysprofd=none',
    'help=false',
  ],
)
ns_pango_conf.set('HAVE_SYSPROF', libsysprof_capture_dep.found())
ns_pango_deps += libsysprof_capture_dep

gnome = import('gnome')
pkgconfig = import('pkgconfig')

# Internal configuration header
configure_file(output: 'config.h', configuration: ns_pango_conf)

root_inc = include_directories('.')
ns_pango_inc = include_directories('ns-pango')

subdir('ns-pango')

if get_option('build-testsuite')
  subdir('tests')
endif

summary('Fontconfig', ns_pango_font_backends.contains('freetype'), section: 'Font backends')

summary('Cairo', cairo_dep.found(), section: 'Features')
summary('Thai', thai_dep.found(), section: 'Features')
summary('Sysprof', libsysprof_capture_dep.found(), section: 'Features')

summary('Compiler', cc.get_id(), section: 'Toolchain')
summary('Linker', cc.get_linker_id(), section: 'Toolchain')

summary('Debugging', get_option('debug'), section: 'Build')
summary('Optimization', get_option('optimization'), section: 'Build')
summary('Link-time optimization', get_option('b_lto'), section: 'Build')
summary('Nordstjernen integration', get_option('nordstjernen'), section: 'Build')
summary('Introspection', build_gir, section: 'Build')

summary('prefix', ns_pango_prefix, section: 'Directories')
summary('includedir', ns_pango_includedir, section: 'Directories')
summary('libdir', ns_pango_libdir, section: 'Directories')
summary('datadir', ns_pango_datadir, section: 'Directories')
