project('pkgconf', 'c',
  version : '2.5.1',
  license : 'ISC',
  default_options : ['c_std=c99'],
  meson_version: '>=0.52',
)

cc = meson.get_compiler('c')

add_project_arguments(
  '-D_BSD_SOURCE',
  '-D_DARWIN_C_SOURCE',
  '-D_DEFAULT_SOURCE',
  '-D_POSIX_C_SOURCE=200809L',
  cc.get_supported_arguments(
    '-Wimplicit-function-declaration',
    '-Wmisleading-indentation',
  ),
  language : 'c',
)

cdata = configuration_data()

check_functions = [
  ['strlcat', 'string.h'],
  ['strlcpy', 'string.h'],
  ['strndup', 'string.h'],
  ['strdup', 'string.h'],
  ['strncasecmp', 'strings.h'],
  ['strcasecmp', 'strings.h'],
  ['reallocarray', 'stdlib.h'],
  ['pledge', 'unistd.h'],
  ['unveil', 'unistd.h'],
  ['readlinkat', 'unistd.h'],
]

foreach f : check_functions
  name = f[0].to_upper().underscorify()
  if cc.has_function(f[0], prefix : '#define _BSD_SOURCE\n#define _DARWIN_C_SOURCE\n#define _DEFAULT_SOURCE\n#define _POSIX_C_SOURCE 200809L\n#include <@0@>'.format(f[1])) and cc.has_header_symbol(f[1], f[0], prefix : '#define _BSD_SOURCE\n#define _DARWIN_C_SOURCE\n#define _DEFAULT_SOURCE\n#define _POSIX_C_SOURCE 200809L')
    cdata.set('HAVE_@0@'.format(name), 1)
    cdata.set('HAVE_DECL_@0@'.format(name), 1)
  else
    cdata.set('HAVE_DECL_@0@'.format(name), 0)
  endif
endforeach

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

libpkgconf = library('pkgconf',
  'libpkgconf/argvsplit.c',
  'libpkgconf/audit.c',
  'libpkgconf/buffer.c',
  'libpkgconf/bsdstubs.c',
  'libpkgconf/cache.c',
  'libpkgconf/client.c',
  'libpkgconf/dependency.c',
  'libpkgconf/fileio.c',
  'libpkgconf/fragment.c',
  'libpkgconf/output.c',
  'libpkgconf/parser.c',
  'libpkgconf/path.c',
  'libpkgconf/personality.c',
  'libpkgconf/pkg.c',
  'libpkgconf/queue.c',
  'libpkgconf/tuple.c',
  c_args: ['-DLIBPKGCONF_EXPORT', build_static],
  install : true,
  version : '7.0.0',
  soversion : '7',
)

# For other projects using libpkgconfig as a subproject
dep_libpkgconf = declare_dependency(
  link_with : libpkgconf,
  include_directories : include_directories('.'),
)

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

pkgconf_exe = executable('pkgconf',
  'cli/main.c',
  'cli/core.c',
  'cli/getopt_long.c',
  'cli/renderer-msvc.c',
  link_with : libpkgconf,
  c_args : build_static,
  include_directories : cli_include,
  install : true)

bomtool_exe = executable('bomtool',
  'cli/bomtool/main.c',
  'cli/getopt_long.c',
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
  link_with : libpkgconf,
  c_args : build_static,
  include_directories : include_directories('cli', 'cli/spdxtool'),
  install : true)

test_runner_exe = executable('test-runner',
  'cli/core.c',
  'cli/getopt_long.c',
  'cli/renderer-msvc.c',
  'tests/test-runner.c',
  link_with : libpkgconf,
  c_args : build_static,
  include_directories : cli_include,
  install : true)

fixtures_dir = '@0@/tests'.format(meson.current_source_dir())
test_suites = [
  'basic',
  'ordering',
  'parser',
  'solver',
  'sbom',
  'sysroot',
  'tuple',
]

foreach t : test_suites
  test(t, test_runner_exe, args : ['--test-fixtures', fixtures_dir, '@0@/t/@1@'.format(meson.current_source_dir(), t)])
endforeach

with_tests = get_option('tests')
kyua_exe = find_program('kyua', required : with_tests, disabler : true, native : true)
atf_sh_exe = find_program('atf-sh', required : with_tests, disabler : true, native : true)
kyuafile = configure_file(input : 'Kyuafile.in', output : 'Kyuafile', configuration : cdata)
test('kyua', kyua_exe, args : ['--config=none', 'test', '--kyuafile', kyuafile, '--build-root', meson.current_build_dir()])
subdir('tests')

install_man('man/bomtool.1')
install_man('man/pkgconf.1')
install_man('man/pkg.m4.7')
install_man('man/pc.5')
install_man('man/pkgconf-personality.5')
install_data('pkg.m4', install_dir: 'share/aclocal')
install_data('AUTHORS', install_dir: 'share/doc/pkgconf')
install_data('README.md', install_dir: 'share/doc/pkgconf')

if host_machine.system() == 'windows'
  conf_data = configuration_data()
  conf_data.set('VERSION', meson.project_version())
  conf_data.set('EXE', pkgconf_exe.full_path())
  conf_data.set('DLL', libpkgconf.full_path())
  if host_machine.cpu() != 'x86_64'
    wixl_arch = 'x86'
  else
    wixl_arch = 'x64'
  endif
  conf_data.set('WIXL_ARCH', wixl_arch)

  python = find_program('python3')
  wixl = find_program('wixl', required: false, version: '>= 0.105')
  msi_filename = 'pkgconf-@0@-@1@.msi'.format(wixl_arch, meson.project_version())

  wxsfile = configure_file(input: 'pkgconf.wxs.in', output: 'pkgconf.wxs', configuration: conf_data)

  if wixl.found()
    licensefile = custom_target(
      'License.rtf',
      input: 'COPYING',
      output: 'License.rtf',
      command: [python, files('txt2rtf.py'), '@INPUT@', '@OUTPUT@'],
    )

    msi = custom_target(
      msi_filename,
      input: [wxsfile, licensefile, pkgconf_exe],
      output: msi_filename,
      command: [wixl, '--arch', wixl_arch, '--ext', 'ui', '-o', msi_filename, wxsfile],
    )

    alias_target('msi', msi)
  endif
endif
