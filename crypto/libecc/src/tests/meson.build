hostcc = meson.get_compiler('c', native: true)

# ec_self_test tests suite
ec_self_test_src = files(
  'ec_self_tests_core.c',
  'ec_self_tests.c',
)

# the executable is built with the same architecture
# as the library itself, meaning that in case of cross-compilation
# the test binary have the same architecture.
#
# Yet, it can still be executed on the build machine architecture if
# the cross file used has declared an 'exe_wrapper' entry, such as,
# for e.g. in a arm32 cross-file:
#
# [binaries]
# exe_wrapper = 'qemu-arm-static'

ec_self_test = executable(
    'ec_self_test',
    sources: [
      ec_self_test_src,
      external_deps_src,
    ],
    dependencies: libsign_dep,
    include_directories: ecc_inc,
    c_args: build_args,
    install: false,
)

test(
    'vectors',
    ec_self_test,
    args: 'vectors',
    timeout: 120,
)

# ec_tuils tests suite is kept native as it is a build machine tooling
ec_utils_src = files(
  'ec_utils.c',
)

ec_utils = executable(
    'ec_utils',
    sources: [
      ec_utils_src,
      external_deps_src,
    ],
    dependencies: native_libsign_dep,
    include_directories: ecc_inc,
    c_args: build_args,
    native: true,
    install: false,
)
