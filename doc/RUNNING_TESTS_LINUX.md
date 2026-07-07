---
name: libarchive-running-tests-linux
description: Explains how to build and run the libarchive test suite on Linux.
---

WIP:  This is still being developed.
Of course, we'll need other `RUNNING_TESTS_<platform>.md`,
as well as `BUILDING_<platform>.md` files with more details about various build options and setup.

# Building and Running Tests on Linux

The following is a summary of the information in [doc/BUILDING_LINUX.md](BUILDING_LINUX.md).

### Configuring and Building the Tests

We recommend using the CMake build system with the Ninja build tool.
A parallel build system based on GNU autoconf is also available.

To configure with CMake:
```bash
cmake -G Ninja -B _build -S .
```
(Omit `-G Ninja` to use the platform default build system
-- usually `make` -- instead of Ninja.)

To build all of libarchive, including the test suites,
using whatever build system you selected during configuration:
```bash
cmake --build _build
```

### Testing a component

There is a single test executable for each major libarchive component.

To test the libarchive library:
```bash
./_build/bin/libarchive_test
```

To test other components:
```bash
./_build/bin/bsdcat_test
./_build/bin/bsdcpio_test
./_build/bin/bsdtar_test
./_build/bin/bsdunzip_test
```

Each test executables runs a series of tests on the corresponding component.
Each test runs in its own temporary directory;
if the test fails, the temporary directory is left behind so you can inspect the contents.

Reference files are loaded from a specific hardcoded path.
This assumes the current working directory is the root of the libarchive source tree.
If you need to run a test executable from a different directory,
use the `-r` option to provide a path to the reference files.

Use the `-h` (help) option for more details or `-v` (verbose) for more detailed output.

### Running a single test

You can run a single test by name
```bash
./_build/bin/libarchive_test test_read_format_zip
```
or by number
```bash
./_build/bin/libarchive_test 307
./_build/bin/libarchive_test 446-448
```
