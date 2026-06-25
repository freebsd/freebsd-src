project('pkgconf', 'c',
  version : '2.9.92',
  license : 'ISC',
  default_options : [
    'c_std=c99',
    'warning_level=2',
  ],
  meson_version: '>=0.56',
)

cc = meson.get_compiler('c')

if cc.get_argument_syntax() != 'msvc'
  if get_option('analyzer')
    add_project_arguments(
      cc.get_supported_arguments('-fanalyzer'),
      language : 'c',
    )
  endif
  add_project_arguments(
    '-D_BSD_SOURCE',
    '-D_DARWIN_C_SOURCE',
    '-D_DEFAULT_SOURCE',
    '-D_POSIX_C_SOURCE=200809L',
    '-D_XOPEN_SOURCE=700',
    cc.get_supported_arguments(
      '-Wdate-time',
      '-Wformat=2',
      '-Wimplicit-function-declaration',
      '-Wmisleading-indentation',
      '-Wmissing-prototypes',
      '-Wmissing-variable-declarations',
      '-Wnested-externs',
      '-Wold-style-definition',
      '-Wpointer-arith',
      '-Wshadow',
      '-Wstrict-prototypes',
    ),
    language : 'c',
  )
else
  add_project_arguments(
    '/wd4130',
    '/wd4200',
    '/wd4774',
    '/wd4820',
    '/wd4668',
    '/wd4996',
    '/wd5045',
    '/D_CRT_SECURE_NO_WARNINGS',
    language : 'c',
  )
endif

fuzzing = get_option('fuzzing')

if host_machine.system() != 'windows' and cc.get_id() == 'clang' and fuzzing
  add_project_arguments('-fno-omit-frame-pointer', '-fsanitize=fuzzer-no-link', language: 'c')
  add_project_link_arguments('-fsanitize=fuzzer-no-link', language: 'c')
elif fuzzing
  warning('fuzzing requested, but not on a supported platform.  a clang compiler in a unix-like environment is required')
  fuzzing = false
endif

cdata = configuration_data()

check_functions = [
  ['strndup', 'string.h'],
  ['strdup', 'string.h'],
  ['strncasecmp', 'strings.h'],
  ['strcasecmp', 'strings.h'],
  ['reallocarray', 'stdlib.h'],
  ['pledge', 'unistd.h'],
  ['unveil', 'unistd.h'],
  ['readlinkat', 'unistd.h'],
]

# Feature-test macros mirroring those defined globally by the build, so that
# the function probes below see the same declarations as the actual sources.
feature_macros = {
  '_BSD_SOURCE': '',
  '_DARWIN_C_SOURCE': '',
  '_DEFAULT_SOURCE': '',
  '_POSIX_C_SOURCE': '200809L',
  '_XOPEN_SOURCE': '700',
}

feature_define_lines = []
foreach macro, value : feature_macros
  if value == ''
    feature_define_lines += '#define @0@'.format(macro)
  else
    feature_define_lines += '#define @0@ @1@'.format(macro, value)
  endif
endforeach
feature_defines = '\n'.join(feature_define_lines)

foreach f : check_functions
  name = f[0].to_upper().underscorify()
  if cc.has_function(f[0], prefix : feature_defines + '\n#include <@0@>'.format(f[1])) and cc.has_header_symbol(f[1], f[0], prefix : feature_defines)
    cdata.set('HAVE_@0@'.format(name), 1)
    cdata.set('HAVE_DECL_@0@'.format(name), 1)
  else
    cdata.set('HAVE_DECL_@0@'.format(name), 0)
  endif
endforeach

# nl_langinfo_l() needs locale.h for locale_t in addition to langinfo.h (on
# macOS it also needs xlocale.h), so it can't share the single-header
# check_functions loop above.
langinfo_prefix = feature_defines + '\n#ifdef __APPLE__\n#include <xlocale.h>\n#endif\n#include <locale.h>'
if cc.has_function('nl_langinfo_l', prefix : langinfo_prefix + '\n#include <langinfo.h>') and cc.has_header_symbol('langinfo.h', 'nl_langinfo_l', prefix : langinfo_prefix)
  cdata.set('HAVE_DECL_NL_LANGINFO_L', 1)
else
  cdata.set('HAVE_DECL_NL_LANGINFO_L', 0)
endif

default_path = []
foreach f : ['libdir', 'datadir']
  default_path += [join_paths(get_option('prefix'), get_option(f), 'pkgconfig')]
endforeach

personality_path = []
foreach f : ['libdir', 'datadir']
  personality_path += [join_paths(get_option('prefix'), get_option(f), 'pkgconfig', 'personality.d')]
endforeach

SYSTEM_LIBDIR = get_option('with-system-libdir')
if SYSTEM_LIBDIR != ''
  cdata.set_quoted('SYSTEM_LIBDIR', SYSTEM_LIBDIR)
else
  cdata.set_quoted('SYSTEM_LIBDIR', join_paths(get_option('prefix'), get_option('libdir')))
endif
SYSTEM_INCLUDEDIR = get_option('with-system-includedir')
if SYSTEM_INCLUDEDIR != ''
  cdata.set_quoted('SYSTEM_INCLUDEDIR', SYSTEM_INCLUDEDIR)
else
  cdata.set_quoted('SYSTEM_INCLUDEDIR', join_paths(get_option('prefix'), get_option('includedir')))
endif
cdata.set_quoted('PKG_DEFAULT_PATH', ':'.join(default_path))
cdata.set_quoted('PERSONALITY_PATH', ':'.join(personality_path))
cdata.set_quoted('PACKAGE_NAME', meson.project_name())
cdata.set_quoted('PACKAGE_VERSION', meson.project_version())
cdata.set_quoted('PACKAGE_BUGREPORT', 'https://todo.sr.ht/~kaniini/pkgconf')
cdata.set('abs_top_srcdir', meson.current_source_dir())
cdata.set('abs_top_builddir', meson.current_build_dir())


subdir('libpkgconf')

libtype = get_option('default_library')
if libtype == 'static'
  build_static = '-DPKGCONFIG_IS_STATIC'
else
  build_static = '-DPKGCONFIG_IS_NOT_STATIC'
endif

libpkgconf_sources = [
  'libpkgconf/argvsplit.c',
  'libpkgconf/audit.c',
  'libpkgconf/buffer.c',
  'libpkgconf/bufferset.c',
  'libpkgconf/bytecode.c',
  'libpkgconf/bsdstubs.c',
  'libpkgconf/cache.c',
  'libpkgconf/client.c',
  'libpkgconf/dependency.c',
  'libpkgconf/fileio.c',
  'libpkgconf/fragment.c',
  'libpkgconf/license.c',
  'libpkgconf/output.c',
  'libpkgconf/parser.c',
  'libpkgconf/path.c',
  'libpkgconf/personality.c',
  'libpkgconf/pkg.c',
  'libpkgconf/queue.c',
  'libpkgconf/tuple.c',
  'libpkgconf/variable.c',
  'libpkgconf/version.c',
]

libpkgconf = library('pkgconf',
  libpkgconf_sources,
  c_args: ['-DLIBPKGCONF_EXPORT', build_static],
  install : true,
  version : '8.0.0',
  soversion : '8',
)

# For other projects using libpkgconfig as a subproject
dep_libpkgconf = declare_dependency(
  link_with : libpkgconf,
  include_directories : include_directories('.'),
)

if fuzzing
  subdir('fuzzer')
endif

# If we have a new enough meson override the dependency so that only
# `dependency('libpkgconf')` is required from the consumer
if meson.version().version_compare('>= 0.54.0')
  meson.override_dependency('libpkgconf', dep_libpkgconf)
endif

pkg = import('pkgconfig')
pkg.generate(libpkgconf,
  name : 'libpkgconf',
  description : 'a library for accessing and manipulating development framework configuration',
  url: 'http://github.com/pkgconf/pkgconf',
  filebase : 'libpkgconf',
  subdirs: ['pkgconf'],
  extra_cflags : build_static
)

cli_include = include_directories('cli')

windows_manifest = []
if host_machine.system() == 'windows'
  manifest_conf_data = configuration_data()
  # The assemblyIdentity.version must always be in the format X.X.X.X.
  manifest_conf_data.set('VERSION', meson.project_version() + '.0')
  manifest_file = configure_file(input: 'pkgconf.manifest.in', output: 'pkgconf.manifest', configuration: manifest_conf_data)
  
  resource_file_conf_data = configuration_data()
  resource_file_conf_data.set('MANIFEST_FILE', join_paths(meson.current_build_dir(), 'pkgconf.manifest'))
  resources_file = configure_file(input: 'pkgconf.rc.in', output: 'pkgconf.rc', configuration: resource_file_conf_data)

  windows = import('windows')
  windows_manifest += windows.compile_resources(resources_file, depend_files: manifest_file)
endif

pkgconf_exe = executable('pkgconf',
  'cli/main.c',
  'cli/core.c',
  'cli/getopt_long.c',
  'cli/renderer-msvc.c',
  windows_manifest,
  link_with : libpkgconf,
  c_args : build_static,
  include_directories : cli_include,
  install : true)

bomtool_exe = executable('bomtool',
  'cli/bomtool/main.c',
  'cli/getopt_long.c',
  windows_manifest,
  link_with : libpkgconf,
  c_args : build_static,
  include_directories : cli_include,
  install : true)

spdxtool_exe = executable('spdxtool',
  'cli/spdxtool/main.c',
  'cli/spdxtool/core.c',
  'cli/spdxtool/software.c',
  'cli/spdxtool/serialize.c',
  'cli/spdxtool/simplelicensing.c',
  'cli/spdxtool/util.c',
  'cli/getopt_long.c',
  windows_manifest,
  link_with : libpkgconf,
  c_args : build_static,
  include_directories : include_directories('cli', 'cli/spdxtool'),
  install : true)

test_runner_exe = executable('test-runner',
  'cli/core.c',
  'cli/getopt_long.c',
  'cli/renderer-msvc.c',
  'tests/test-runner.c',
  windows_manifest,
  link_with : libpkgconf,
  c_args : build_static,
  include_directories : cli_include,
  install : false)

api_tests = [
  'audit',
  'buffer',
  'bytecode',
  'client',
  'dependency',
  'fileio',
  'fragment',
  'license',
  'path-utils',
  'personality',
  'queue',
  'tuple',
  'variable',
  'version',
]

foreach t : api_tests
  exe = executable('test-api-' + t,
    'tests/api/test-' + t + '.c',
    windows_manifest,
    link_with : libpkgconf,
    c_args : build_static,
    include_directories : [include_directories('.'), include_directories('tests/api')],
    install : false)
  test('api-' + t, exe)
endforeach

# Unit test for spdxtool's JSON serializer.  Unlike the api_tests above it must
# also compile the spdxtool sources it exercises (everything but main.c).
test_api_serialize_exe = executable('test-api-serialize',
  'tests/api/test-serialize.c',
  'cli/spdxtool/core.c',
  'cli/spdxtool/software.c',
  'cli/spdxtool/serialize.c',
  'cli/spdxtool/simplelicensing.c',
  'cli/spdxtool/util.c',
  windows_manifest,
  link_with : libpkgconf,
  c_args : build_static,
  include_directories : [include_directories('.'), include_directories('tests/api'), include_directories('cli/spdxtool')],
  install : false)
test('api-serialize', test_api_serialize_exe)

# Allocation-failure (OOM) tests.  These drive the shared fuzzer/alloc-inject
# fault injector through the linker's --wrap, so they are built wherever --wrap
# is supported: glibc and musl Linux (GNU ld / lld), and skipped on macOS ld64
# and MSVC.
build_oom_tests = host_machine.system() == 'linux' \
  and cc.has_link_argument('-Wl,--wrap=calloc')

# Allocator set wrapped by fuzzer/alloc-inject.c.
oom_wrap_args = [
  '-Wl,--wrap=malloc', '-Wl,--wrap=calloc', '-Wl,--wrap=realloc',
  '-Wl,--wrap=reallocarray', '-Wl,--wrap=strdup', '-Wl,--wrap=strndup',
]

if build_oom_tests
  # Unit test: fault-inject spdxtool's own allocations.  It links libpkgconf
  # normally (shared); only spdxtool's compiled-in allocations are wrapped,
  # which is exactly what we want to fail here.
  test_api_oom_spdxtool_exe = executable('test-api-oom-spdxtool',
    'tests/api/test-oom-spdxtool.c',
    'fuzzer/alloc-inject.c',
    'cli/spdxtool/core.c',
    'cli/spdxtool/software.c',
    'cli/spdxtool/serialize.c',
    'cli/spdxtool/simplelicensing.c',
    'cli/spdxtool/util.c',
    windows_manifest,
    link_with : libpkgconf,
    c_args : build_static,
    link_args : oom_wrap_args,
    include_directories : [include_directories('.'), include_directories('tests/api'), include_directories('cli/spdxtool'), include_directories('fuzzer')],
    install : false)
  test('api-oom-spdxtool', test_api_oom_spdxtool_exe)

  # Replay the existing parser/solver fuzz targets over their seed corpus,
  # deterministically and without libFuzzer, so the libpkgconf OOM paths they
  # exercise via alloc-inject are captured by a coverage build (the clang/
  # libFuzzer fuzz job is not coverage-instrumented).  --wrap only reaches
  # allocations linked into the executable, so these link a static libpkgconf.
  libpkgconf_fault = static_library('pkgconf-fault',
    libpkgconf_sources,
    c_args : ['-DLIBPKGCONF_EXPORT', build_static],
    install : false)

  fuzz_replays = [
    ['parser', join_paths(meson.current_source_dir(), 'tests', 'lib1')],
    ['solver', join_paths(meson.current_source_dir(), 'fuzzer', 'solver-corpus')],
  ]
  foreach r : fuzz_replays
    replay_exe = executable('fuzz-replay-' + r[0],
      'fuzzer/replay.c',
      'fuzzer' / (r[0] + '-fuzzer.c'),
      'fuzzer/alloc-inject.c',
      link_with : libpkgconf_fault,
      c_args : build_static,
      link_args : oom_wrap_args,
      include_directories : [include_directories('.'), include_directories('fuzzer')],
      install : false)
    # The exhaustive per-input injection loop is slow under ASAN; give it room.
    test('fuzz-replay-' + r[0], replay_exe, args : [r[1]], timeout : 120)
  endforeach
endif

fixtures_dir = join_paths(meson.current_source_dir(), 'tests')
tool_dir = meson.current_build_dir()

test_suites = [
  'basic',
  'cli',
  'link-abi',
  'ordering',
  'parser',
  'personality',
  'solver',
  'sbom',
  'sysroot',
  'tuple',
  'bomtool',
  'spdxtool',
  'symlink'
]

foreach t : test_suites
  test(t, test_runner_exe,
    args : ['--test-fixtures', fixtures_dir, '--tool-dir', tool_dir, join_paths(meson.current_source_dir(), 't', t)],
    depends : [pkgconf_exe, spdxtool_exe, bomtool_exe])
endforeach

install_man('man/bomtool.1')
install_man('man/pkgconf.1')
install_man('man/pkg.m4.7')
install_man('man/pc.5')
install_man('man/pkgconf-personality.5')
install_man('man/spdxtool.1')
install_data('pkg.m4', install_dir: 'share/aclocal')
install_data('AUTHORS', install_dir: 'share/doc/pkgconf')
install_data('README.md', install_dir: 'share/doc/pkgconf')
install_data('COPYING', install_dir: 'share/doc/pkgconf')

if host_machine.system() == 'windows'
  conf_data = configuration_data()
  conf_data.set('VERSION', meson.project_version())
  conf_data.set('EXE', pkgconf_exe.full_path())
  conf_data.set('DLL', libpkgconf.full_path())
  conf_data.set('BUILD_TYPE', libtype)

  if host_machine.cpu_family() == 'x86'
    wix_arch = 'x86'
  elif host_machine.cpu_family() == 'x86_64'
    wix_arch = 'x64'
  elif host_machine.cpu_family() == 'aarch64'
    wix_arch = 'arm64'
  else
    error('WIX does not support CPU family ' + host_machine.cpu_family())
  endif

  python = find_program('python3')
  licensefile = custom_target(
    'License.rtf',
    input: 'COPYING',
    output: 'License.rtf',
    command: [python, files('txt2rtf.py'), '@INPUT@', '@OUTPUT@'],
  )
  conf_data.set('LICENSE_RTF', licensefile.full_path())

  wix = find_program('wix', required: false, version: '>= 5.0.0')
  msi_filename = 'pkgconf-@0@-@1@.msi'.format(wix_arch, meson.project_version())

  wxsfile = configure_file(input: 'pkgconf.wxs.in', output: 'pkgconf.wxs', configuration: conf_data)

  if wix.found()
    msi = custom_target(
      msi_filename,
      input: [wxsfile, licensefile, pkgconf_exe],
      output: msi_filename,
      command: [wix, 'build', wxsfile, '-arch', wix_arch, '-ext', 'WixToolset.UI.wixext', '-o', msi_filename],
    )

    alias_target('msi', msi)
  endif
endif
