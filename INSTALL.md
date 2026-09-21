# Introduction

ATF uses the GNU Automake, GNU Autoconf and GNU Libtool utilities as its
build system. These are used only when compiling the application from the
source code package. If you want to install ATF from a binary package, you
do not need to read this document.

For the impatient:

```shell
$ ./configure
$ make
$ make check
$ sudo make install # or `make install` with root privileges
$ make installcheck
```

Or alternatively, install as a regular user into your home directory:

```shell
$ ./configure --prefix ~/local
$ make
$ make check
$ make install
$ make installcheck
```
# Dependencies

To build and use ATF successfully you need:

* A C++-20 standards-compliant compiler.
* pkg-config or an equivalent tool, e.g., pkgconf.
* A POSIX shell interpreter.
* A make(1) utility.

Optionally, if you want to build and run the tests (recommended), you
need:

* Kyua 0.5 or greater.

If you are building ATF from the code on the repository, you will also
need the following tools:

* GNU Autoconf 2.68 (or later).
* GNU Automake 1.9 (or later).
* GNU Libtool.

# Regenerating the build system

This is not necessary if you are building from a formal release
distribution file.

The following environment variables are specific to ATF's `configure`
script:

- `ATF_BUILD_CC`:

  **Possible values:** empty, or an absolute or relative path to a C compiler.

  **Default:** the value of CC as detected by the configure script.

  Specifies the C compiler that ATF will use at run time whenever the
  build-time-specific checks are used.

- `ATF_BUILD_CFLAGS`:

  **Possible values:** empty, or a list of valid C compiler flags.

  **Default:** the value of CFLAGS as detected by the configure script.

  Specifies the C compiler flags that ATF will use at run time whenever the
  build-time-specific checks are used.

- `ATF_BUILD_CPP`:

  **Possible values:** empty, or an absolute or relative path to a C/C++
  preprocessor.

  **Default:** the value of CPP as detected by `configure` script.

  Specifies the C/C++ preprocessor that ATF will use at run time whenever
  the build-time-specific checks are used.

- `ATF_BUILD_CPPFLAGS`:

  **Possible values:** empty, or a list of valid C/C++ preprocessor flags.

  **Default:** the value of `CPPFLAGS` as detected by the configure script.

  Specifies the C/C++ preprocessor flags that ATF will use at run time
  whenever the build-time-specific checks are used.

- `ATF_BUILD_CXX`:

  **Possible values:** empty, or an absolute or relative path to a C++ compiler.

  **Default:** the value of `CXX` as detected by the configure script.

  Specifies the C++ compiler that ATF will use at run time whenever the
  build-time-specific checks are used.

- `ATF_BUILD_CXXFLAGS`:

  **Possible values:** empty, or a list of valid C++ compiler flags.

  **Default:** the value of `CXXFLAGS` as detected by `configure` script.

  Specifies the C++ compiler flags that ATF will use at run time whenever
  the build-time-specific checks are used.

- `ATF_SHELL`:

  **Possible values:** empty, an absolute path to a POSIX shell interpreter.

  **Default:** empty.

  Specifies the POSIX shell interpreter that ATF will use at run time to
  execute its scripts and the test programs written using the atf-sh
  library. If empty, the configure script will try to find a suitable
  interpreter for you.

# Configuration flags

The most common, standard flags given to `configure` are:

- `--prefix=directory`

  **Possible values**: any path
  **Default**: "/usr/local"

  Specifies where the library (binaries and all associated files) will be
  installed.

The following flags are specific to ATF's `configure` script:

- `--enable-developer`

  **Default:** `yes` in HEAD builds; `no` in release builds.

  Enables several features useful for development, such as the inclusion
  of debugging symbols in all objects or the enforcement of compilation
  warnings.

  The compiler will be executed with an exhaustive collection of warning
  detection features regardless of the value of this flag. However, such
  warnings are only fatal when `--enable-developer` is `yes`.

- `--enable-asan`

  **Default:** `no`.

  Enables ASAN (Address Sanitizer) compiler support in the toolchain.

  Platform-specific support varies depending on the toolchain and OS.

- `--enable-code-coverage`

  **Default:** `no`.

  Enable runtime code coverage support support in the toolchain.

  This option is automatically "plumbed in" to the `make distcheck` target.
  code coverage output (LCOV files; html report) are placed in the [root]
  directory defined by `CODE_COVERAGE_OUTPUT_DIR` (this variable defaults to
  `build/code_coverage`). `genhtml` behavior can be tuned via the
  `EXTRA_GENHTML_FLAGS` variable.

  This functionality requires the [lcov](https://github.com/linux-test-project/lcov)
  package and a compatible toolchain be installed beforehand.

- `--enable-lsan`

  **Default:** `no`.

  Enables LSAN (Leak Sanitizer) compiler support in the toolchain.

  Platform-specific support varies depending on the toolchain and OS.

  **Important:**
  * llvm support is only available in Linux and macOS at time of writing.
  * The test suite does not currently pass with this option enabled. See
    [Issue #77](https://github.com/freebsd/atf/issues/77) for more details.

- `--enable-ubsan`

  **Default:** `no`.

  Enables UBSAN (Undefined Behavior Sanitizier) compiler support in the
  toolchain.

  Platform-specific support varies depending on the toolchain and OS.
