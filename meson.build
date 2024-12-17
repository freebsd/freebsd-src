project('libecc', 'c',
    meson_version: '>=1.1.0',
    version: run_command('dunamai', 'from', 'git', '--style', 'semver', '--dirty', check: true).stdout().strip(),
    default_options: ['c_std=gnu11', 'default_library=static', 'warning_level=3' ],
    license: 'BSD-3-Clause OR GPL-2.0-or-later',
    license_files: [ 'LICENSE' ],
)

top_dir = meson.current_source_dir()

# dunamai is used for version sync with gconf (in the same way poetry-dynamic-versioning does)
pymod = import('python')
py3 = pymod.find_installation('python3', modules: ['dunamai'])

# about libecc
# below file listings should be moved in successive small 'meson.build' file in each subdir, that
# only contains the file listing, replacing them with a lonely 'subdir('src/xxx') instead.

ecc_inc = include_directories('include')
subdir('include/libecc')

# module declaration, each module files listing is declare in its own directory
subdir('src/utils')
subdir('src/nn')
subdir('src/fp')
subdir('src/curves')
subdir('src/hash')
subdir('src/sig')
subdir('src/ecdh')
subdir('src/external_deps')

# detect if an external rand source implementation is passed
use_external_rand_opt = get_option('use_external_rand')
if use_external_rand_opt
  rand_src = files(get_option('with_rand_source'))
else
  rand_src = files()
endif
# detect if an external print source implementation is passed
use_external_print_opt = get_option('use_external_print')
if use_external_print_opt
  print_src = files(get_option('with_print_source'))
else
  print_src = files()
endif
# detect if an external time source implementation is passed
use_external_time_opt = get_option('use_external_time')
if use_external_time_opt
  time_src = files(get_option('with_time_source'))
else
  time_src = files()
endif


# globally used build args, TODO: to make configurable
build_args = [
  '-fno-builtin',
  '-D_FORTIFY_SOURCE=2',
  '-fstack-protector-strong',
  '-DUSE_WARN_UNUSED_RET',
  '-ffreestanding'
]


# About configurable options, based on makefiles

# 1. build-relative options first

# info compilers arguments (such as m32/m64 should be cross-file related.
# just using a different cross-file (event for m32 on m64 arch for e.g. is enough
# to change the behavior
with_wordsize_opt = get_option('with_wordsize')
if with_wordsize_opt == '16'
  build_args += '-DWORDSIZE=16'
elif with_wordsize_opt == '32'
  build_args += '-DWORDSIZE=32'
elif with_wordsize_opt == '64'
  build_args += '-DWORDSIZE=64'
endif

with_stdlib_opt = get_option('with_stdlib')
if with_stdlib_opt
  build_args += '-DWITH_STDLIB'
endif

with_debug_opt = get_option('with_debug')
if with_debug_opt
  build_args += '-DDEBUG'
endif

use_cryptofuzz_opt = get_option('use_cryptofuzz')
if use_cryptofuzz_opt
  build_args += '-DUSE_CRYPTOFUZZ'
endif

assert_print_opt = get_option('assert_print')
if assert_print_opt
  build_args += '-DUSE_ASSERT_PRINT'
endif

no_warn_unused_ret_opt = get_option('no_warn_unused_ret')
if not no_warn_unused_ret_opt
  build_args += '-DUSE_WARN_UNUSED_RET'
endif

# 2. security relative options

with_sig_blinding_opt = get_option('with_sig_blinding')
if with_sig_blinding_opt
  build_args += '-DUSE_SIG_BLINDING'
endif

with_complete_formulas_opt = get_option('with_complete_formulas')
if with_complete_formulas_opt
  build_args += '-DNO_USE_COMPLETE_FORMULAS'
endif

with_double_add_opt = get_option('with_double_add')
if with_double_add_opt == 'true'
  build_args += '-DUSE_DOUBLE_ADD_ALWAYS'
elif with_double_add_opt == 'false'
  build_args += '-DUSE_MONTY_LADDER'
endif

with_monty_ladder_opt = get_option('with_monty_ladder')
if with_monty_ladder_opt == 'true'
  build_args += '-DUSE_MONTY_LADDER'
elif with_monty_ladder_opt == 'false'
  build_args += '-DUSE_DOUBLE_ADD_ALWAYS'
endif

assert(not (with_double_add_opt == 'true' and with_monty_ladder_opt == 'true'), 'with_double_add and with_monty_ladder are incompatible options!')

# 3. crypto engines relative options. It is possible to fully disable all
# engines and manually select which one must be built

with_override_ecc_config_opt = get_option('with_override_ecc_config')
if with_override_ecc_config_opt
  build_args += '-DWITH_LIBECC_CONFIG_OVERRIDE'
endif

if with_override_ecc_config_opt
  # Handle the asked curves
  with_curves_opt = get_option('with_curves')
  assert(with_curves_opt.length() != 0, 'You have selected a libecc configuration override with no curve: please select at least one proper curve!')
  foreach curve : with_curves_opt
    build_args += '-DWITH_CURVE_'+curve.to_upper()
  endforeach
  # Handle the asked hashes
  with_hashes_opt = get_option('with_hashes')
  assert(with_hashes_opt.length() != 0, 'You have selected a libecc configuration override with no hash: please select at least one proper hash!')
  foreach hash : with_hashes_opt
    build_args += '-DWITH_HASH_'+hash.to_upper()
  endforeach
  # Handle the asked algorithms
  with_algs_opt = get_option('with_algs')
  assert(with_algs_opt.length() != 0, 'You have selected a libecc configuration override with no algorithm: please select at least one proper algorithm!')
  foreach alg : with_algs_opt
    build_args += '-DWITH_'+alg.to_upper()
  endforeach
endif

# Small stack handling
with_small_stack_opt = get_option('with_small_stack')
if with_small_stack_opt
  build_args += '-DUSE_SMALL_STACK'
endif
assert(not (with_small_stack_opt and with_sig_eddsa25519_opt), 'Small stack and EdDSA are incompatible options!')
assert(not (with_small_stack_opt and with_sig_eddsa448_opt), 'Small stack and EdDSA are incompatible options!')
assert(not (with_small_stack_opt and with_x25519_opt), 'Small stack and X25519 are incompatible options!')
assert(not (with_small_stack_opt and with_x448_opt), 'Small stack and X448 are incompatible options!')


# libecc libraries declaration. For each library, the library itself and the
# corresponding dependency object (includedir and library to link with) is
# also declared
#
# INFO: defaulting to static lib only (see project declaration).
# to build both static and dynamic library, use -Ddefault_library=both option
#
libarith_lib = library('arith',
    sources: [
      fp_mod_src,
      nn_mod_src,
      rand_src,
      print_src,
      time_src,
      utils_arith_src,
    ],
    include_directories: ecc_inc,
    install : true,
    c_args: build_args,
)
libarith_dep = declare_dependency(
    link_with: libarith_lib,
    include_directories: ecc_inc,
)

libec_lib = library('ecc',
  sources: [
    curves_mod_src,
    utils_ec_src,
    fp_mod_src,
    nn_mod_src,
    rand_src,
    print_src,
    time_src,
    utils_arith_src
  ],
  include_directories: ecc_inc,
  install : true,
  c_args: build_args,
)
libec_dep = declare_dependency(
    link_with: libec_lib,
    include_directories: ecc_inc,
)

libsign_lib = library('sign',
  sources: [
    hash_mod_src,
    sig_mod_src,
    key_mod_src,
    utils_sign_src,
    ecdh_mod_src,
    curves_mod_src,
    utils_ec_src,
    fp_mod_src,
    nn_mod_src,
    rand_src,
    print_src,
    time_src,
    utils_arith_src
  ],
  include_directories: ecc_inc,
  install : true,
  c_args: build_args,
)
libsign_dep = declare_dependency(
    link_with: libsign_lib,
    include_directories: ecc_inc,
)

# in order to build native tools that depends on libsign library, we should
# check if the nominal library built has been made native or cross. If
# cross, another build must be made natively for native tooling
if meson.is_cross_build()
  native_libsign_lib = library('sign_native',
    sources: [
      hash_mod_src,
      sig_mod_src,
      key_mod_src,
      utils_sign_src,
      ecdh_mod_src,
      curves_mod_src,
      utils_ec_src,
      fp_mod_src,
      nn_mod_src,
      rand_src,
      print_src,
      time_src,
      utils_arith_src
    ],
    include_directories: ecc_inc,
    install : false,
    c_args: build_args,
    native: true,
  )
  native_libsign_dep = declare_dependency(
    link_with: native_libsign_lib,
    include_directories: ecc_inc,
  )
else
    native_libsign_lib = libsign_lib
    native_libsign_dep = libsign_dep
endif

# About tests, see src/tests/meson.build. Enabled with -Dwith_tests=true
with_tests_opt = get_option('with_tests')
if with_tests_opt
  subdir('src/tests')
endif
