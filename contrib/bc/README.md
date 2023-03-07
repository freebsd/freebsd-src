# `bc`

***WARNING: New user registration for <https://git.gavinhoward.com/> is disabled
because of spam. If you need to report a bug with `bc`, email gavin at this site
minus the `git.` part for an account, and I will create one for you. Or you can
report an issue at [GitHub][29].***

***WARNING: This project has moved to [https://git.gavinhoward.com/][20] for
[these reasons][21], though GitHub will remain a mirror.***

This is an implementation of the [POSIX `bc` calculator][12] that implements
[GNU `bc`][1] extensions, as well as the period (`.`) extension for the BSD
flavor of `bc`.

For more information, see this `bc`'s full manual.

This `bc` also includes an implementation of `dc` in the same binary, accessible
via a symbolic link, which implements all FreeBSD and GNU extensions. (If a
standalone `dc` binary is desired, `bc` can be copied and renamed to `dc`.) The
`!` command is omitted; I believe this poses security concerns and that such
functionality is unnecessary.

For more information, see the `dc`'s full manual.

This `bc` also provides `bc`'s math as a library with C bindings, called `bcl`.

For more information, see the full manual for `bcl`.

## License

This `bc` is Free and Open Source Software (FOSS). It is offered under the BSD
2-clause License. Full license text may be found in the [`LICENSE.md`][4] file.

## Prerequisites

This `bc` only requires either:

1.	Windows 10 or later, or
2.	A C99-compatible compiler and a (mostly) POSIX 2008-compatible system with
	the XSI (X/Open System Interfaces) option group.

Since POSIX 2008 with XSI requires the existence of a C99 compiler as `c99`, any
POSIX and XSI-compatible system will have everything needed.

POSIX-compatible systems that are known to work:

* Linux
* FreeBSD
* OpenBSD
* NetBSD
* Mac OSX
* Solaris* (as long as the Solaris version supports POSIX 2008)
* AIX
* HP-UX* (except for history)

In addition, there is compatibility code to make this `bc` work on Windows.

Please submit bug reports if this `bc` does not build out of the box on any
system.

## Build

This `bc` should build unmodified on any POSIX-compliant system or on Windows
starting with Windows 10 (though earlier versions may work).

For more complex build requirements than the ones below, see the [build
manual][5].

### Windows

There is no guarantee that this `bc` will work on any version of Windows earlier
than Windows 10 (I cannot test on earlier versions), but it is guaranteed to
work on Windows 10 at least.

Also, if building with MSBuild, the MSBuild bundled with Visual Studio is
required.

**Note**: Unlike the POSIX-compatible platforms, only one build configuration is
supported on Windows: extra math and history enabled, NLS (locale support)
disabled, with both calculators built.

#### `bc`

To build `bc`, you can open the `vs/bc.sln` file in Visual Studio, select the
configuration, and build.

You can also build using MSBuild with the following from the root directory:

```
msbuild -property:Configuration=<config> vs/bc.sln
```

where `<config>` is either one of `Debug` or `Release`.

On Windows, the calculators are built as `vs/bin/<platform>/<config>/bc.exe` and
`vs/bin/<Platform>/<Config>/dc.exe`, where `<platform>` can be either `Win32` or
`x64`, and `<config>` can be `Debug` or `Release`.

**Note**: On Windows, `dc.exe` is just copied from `bc.exe`; it is not linked.
Patches are welcome for a way to do that.

#### `bcl` (Library)

To build the library, you can open the `vs/bcl.sln` file in Visual Studio,
select the configuration, and build.

You can also build using MSBuild with the following from the root directory:

```
msbuild -property:Configuration=<config> vs/bcl.sln
```

where `<config>` is either one of `Debug`, `ReleaseMD`, or `ReleaseMT`.

On Windows, the library is built as `vs/lib/<platform>/<config>/bcl.lib`, where
`<platform>` can be either `Win32` or `x64`, and `<config>` can be `Debug`,
`ReleaseMD`, or `ReleaseMT`.

### POSIX-Compatible Systems

On POSIX-compatible systems, `bc` is built as `bin/bc` and `dc` is built as
`bin/dc` by default.

#### Default

For the default build with optimization, use the following commands in the root
directory:

```
./configure.sh -O3
make
```

#### One Calculator

To only build `bc`, use the following commands:

```
./configure.sh --disable-dc
make
```

To only build `dc`, use the following commands:

```
./configure.sh --disable-bc
make
```

#### Debug

For debug builds, use the following commands in the root directory:

```
./configure.sh -g
make
```

#### Install

To install, use the following command:

```
make install
```

By default, `bc` and `dc` will be installed in `/usr/local`. For installing in
other locations, use the `PREFIX` environment variable when running
`configure.sh` or pass the `--prefix=<prefix>` option to `configure.sh`. See the
[build manual][5], or run `./configure.sh --help`, for more details.

#### Library

To build the math library, pass the `-a` or `--library` options to
`configure.sh`:

```
./configure.sh -a
```

When building the library, the executables are not built. For more information,
see the [build manual][5].

The library API can be found in [`manuals/bcl.3.md`][26] or `man bcl` once the
library is installed.

#### Package and Distro Maintainers

This section is for package and distro maintainers.

##### Out-of-Source Builds

Out-of-source builds are supported; just call `configure.sh` from the directory
where the actual build will happen.

For example, if the source is in `bc`, the build should happen in `build`, then
call `configure.sh` and `make` like so:

```
../bc/configure.sh
make
```

***WARNING***: The path to `configure.sh` from the build directory must not have
spaces because `make` does not support target names with spaces.

##### Recommended Compiler

When I ran benchmarks with my `bc` compiled under `clang`, it performed much
better than when compiled under `gcc`. I recommend compiling this `bc` with
`clang`.

I also recommend building this `bc` with C11 if you can because `bc` will detect
a C11 compiler and add `_Noreturn` to any relevant function(s).

##### Recommended Optimizations

I wrote this `bc` with Separation of Concerns, which means that there are many
small functions that could be inlined. However, they are often called across
file boundaries, and the default optimizer can only look at the current file,
which means that they are not inlined.

Thus, because of the way this `bc` is built, it will automatically be slower
than other `bc` implementations when running scripts with no math. (My `bc`'s
math is *much* faster, so any non-trivial script should run faster in my `bc`.)

Some, or all, of the difference can be made up with the right optimizations. The
optimizations I recommend are:

1.	`-O3`
2.	`-flto` (link-time optimization)

in that order.

Link-time optimization, in particular, speeds up the `bc` a lot. This is because
when link-time optimization is turned on, the optimizer can look across files
and inline *much* more heavily.

However, I recommend ***NOT*** using `-march=native`. Doing so will reduce this
`bc`'s performance, at least when building with link-time optimization. See the
[benchmarks][19] for more details.

##### Stripping Binaries

By default, non-debug binaries are stripped, but stripping can be disabled with
the `-T` option to `configure.sh`.

##### Using This `bc` as an Alternative

If this `bc` is packaged as an alternative to an already existing `bc` package,
it is possible to rename it in the build to prevent name collision. To prepend
to the name, just run the following:

```
EXECPREFIX=<some_prefix> ./configure.sh
```

To append to the name, just run the following:

```
EXECSUFFIX=<some_suffix> ./configure.sh
```

If a package maintainer wishes to add both a prefix and a suffix, that is
allowed.

**Note**: The suggested name (and package name) when `bc` is not available is
`bc-gh`.

##### Karatsuba Number

Package and distro maintainers have one tool at their disposal to build this
`bc` in the optimal configuration: `scripts/karatsuba.py`.

This script is not a compile-time or runtime prerequisite; it is for package and
distro maintainers to run once when a package is being created. It finds the
optimal Karatsuba number (see the [algorithms manual][7] for more information)
for the machine that it is running on.

The easiest way to run this script is with `make karatsuba`.

If desired, maintainers can also skip running this script because there is a
sane default for the Karatsuba number.

## Status

This `bc` is robust.

It is well-tested, fuzzed, and fully standards-compliant (though not certified)
with POSIX `bc`. The math has been tested with 40+ million random problems, so
it is as correct as I can make it.

This `bc` can be used as a drop-in replacement for any existing `bc`. This `bc`
is also compatible with MinGW toolchains.

In addition, this `bc` is considered complete; i.e., there will be no more
releases with additional features. However, it *is* actively maintained, so if
any bugs are found, they will be fixed in new releases. Also, additional
translations will also be added as they are provided.

### Development

If I (Gavin D. Howard) get [hit by a bus][27] and future programmers need to
handle work themselves, the best place to start is the [Development manual][28].

## Vim Syntax

I have developed (using other people's code to start) [`vim` syntax files][17]
for this `bc` and `dc`, including the extensions.

## `bc` Libs

I have gathered some excellent [`bc` and `dc` libraries][18]. These libraries
may prove useful to any serious users.

## Comparison to GNU `bc`

This `bc` compares favorably to GNU `bc`.

* This `bc` builds natively on Windows.
* It has more extensions, which make this `bc` more useful for scripting. (See
  [Extensions](#extensions).)
* This `bc` is a bit more POSIX compliant.
* It has a much less buggy parser. The GNU `bc` will give parse errors for what
  is actually valid `bc` code, or should be. For example, putting an `else` on
  a new line after a brace can cause GNU `bc` to give a parse error.
* This `bc` has fewer crashes.
* GNU `bc` calculates the wrong number of significant digits for `length(x)`.
* GNU `bc` will sometimes print numbers incorrectly. For example, when running
  it on the file `tests/bc/power.txt` in this repo, GNU `bc` gets all the right
  answers, but it fails to wrap the numbers at the proper place when outputting
  to a file.
* This `bc` is faster. (See [Performance](#performance).)

### Performance

Because this `bc` packs more than `1` decimal digit per hardware integer, this
`bc` is faster than GNU `bc` and can be *much* faster. Full benchmarks can be
found at [manuals/benchmarks.md][19].

There is one instance where this `bc` is slower: if scripts are light on math.
This is because this `bc`'s intepreter is slightly slower than GNU `bc`, but
that is because it is more robust. See the [benchmarks][19].

### Extensions

Below is a non-comprehensive list of extensions that this `bc` and `dc` have
that all others do not.

* An extended math library. (See [here][30] for more information.)
* A command-line prompt.
* Turning on and off digit clamping. (Digit clamping is about how to treat
  "invalid" digits for a particular base. GNU `bc` uses it, and the BSD `bc`
  does not. Mine does both.)
* A pseudo-random number generator. This includes the ability to set the seed
  and get reproducible streams of random numbers.
* The ability to use stacks for the globals `scale`, `ibase`, and `obase`
  instead of needing to restore them in *every* function.
* The ability to *not* use non-standard keywords. For example, `abs` is a
  keyword (a built-in function), but if some script actually defines a function
  called that, it's possible to tell my `bc` to not treat it as a keyword, which
  will make the script parses correctly.
* The ability to turn on and off printing leading zeroes on numbers greater than
  `-1` and less than `1`.
* Outputting in scientific and engineering notation.
* Accepting input in scientific and engineering notation.
* Passing strings and arrays to the `length()` built-in function. (In `dc`, the
  `Y` command will do this for arrays, and the `Z` command will do this for both
  numbers and strings.)
* The `abs()` built-in function. (This is the `b` command in `dc`.)
* The `is_number()` and `is_string()` built-in functions. (These tell whether a
  variable is holding a string or a number, for runtime type checking. The
  commands are `u` and `t` in `dc`.)
* For `bc` only, the `divmod()` built-in function for computing a quotient and
  remainder at the same time.
* For `bc` only, the `asciify()` built-in function for converting an array to a
  string.
* The `$` truncation operator. (It's the same in `bc` and `dc`.)
* The `@` "set scale" operator. (It's the same in `bc` and `dc`.)
* The decimal shift operators. (`<<` and `>>` in `bc`, `H` and `h` in `dc`.)
* Built-in functions or commands to get the max of `scale`, `ibase`, and
  `obase`.
* The ability to put strings into variables in `bc`. (This always existed in
  `dc`.)
* The `'` command in `dc` for the depth of the execution stack.
* The `y` command in `dc` for the depth of register stacks.
* Built-in functions or commands to get the value of certain environment
  variables that might affect execution.
* The `stream` keyword to do the same thing as the `P` command in `dc`.
* Defined order of evaluation.
* Defined exit statuses.
* All environment variables other than `POSIXLY_CORRECT`, `BC_ENV_ARGS`, and
  `BC_LINE_LENGTH`.
* The ability for users to define their own defaults for various options during
  build. (See [here][31] for more information.)

## Algorithms

To see what algorithms this `bc` uses, see the [algorithms manual][7].

## Locales

Currently, there is no locale support on Windows.

Additionally, this `bc` only has support for English (and US English), French,
German, Portuguese, Dutch, Polish, Russian, Japanese, and Chinese locales.
Patches are welcome for translations; use the existing `*.msg` files in
`locales/` as a starting point.

In addition, patches for improvements are welcome; the last two messages in
Portuguese were made with Google Translate, and the Dutch, Polish, Russian,
Japanese, and Chinese locales were all generated with [DeepL][22].

The message files provided assume that locales apply to all regions where a
language is used, but this might not be true for, e.g., `fr_CA` and `fr_CH`.
Any corrections or a confirmation that the current texts are acceptable for
those regions would be appreciated, too.

## Other Projects

Other projects based on this bc are:

* [busybox `bc`][8]. The busybox maintainers have made their own changes, so any
  bugs in the busybox `bc` should be reported to them.

* [toybox `bc`][9]. The maintainer has also made his own changes, so bugs in the
  toybox `bc` should be reported there.

* [FreeBSD `bc`][23]. While the `bc` in FreeBSD is kept up-to-date, it is better
  to [report bugs there][24], as well as [submit patches][25], and the
  maintainers of the package will contact me if necessary.

## Language

This `bc` is written in pure ISO C99, using POSIX 2008 APIs with custom Windows
compatibility code.

## Commit Messages

This `bc` uses the commit message guidelines laid out in [this blog post][10].

## Semantic Versioning

This `bc` uses [semantic versioning][11].

## AI-Free

This repository is 100% AI-Free code.

## Contents

Items labeled with `(maintainer use only)` are not included in release source
tarballs.

Files:

	.gitignore           The git ignore file (maintainer use only).
	.gitattributes       The git attributes file (maintainer use only).
	bcl.pc.in            A template pkg-config file for bcl.
	configure            A symlink to configure.sh to make packaging easier.
	configure.sh         The configure script.
	LICENSE.md           A Markdown form of the BSD 2-clause License.
	Makefile.in          The Makefile template.
	NEWS.md              The changelog.
	NOTICE.md            List of contributors and copyright owners.

Folders:

	benchmarks  A folder of benchmarks for various aspects of bc performance.
	gen         The bc math library, help texts, and code to generate C source.
	include     All header files.
	locales     Locale files, in .msg format. Patches welcome for translations.
	manuals     Manuals for both programs.
	src         All source code.
	scripts     A bunch of shell scripts to help with development and building.
	tests       All tests.
	vs          Files needed for the build on Windows.

[1]: https://www.gnu.org/software/bc/
[4]: ./LICENSE.md
[5]: ./manuals/build.md
[7]: ./manuals/algorithms.md
[8]: https://git.busybox.net/busybox/tree/miscutils/bc.c
[9]: https://github.com/landley/toybox/blob/master/toys/pending/bc.c
[10]: http://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html
[11]: http://semver.org/
[12]: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/bc.html
[17]: https://git.gavinhoward.com/gavin/vim-bc
[18]: https://git.gavinhoward.com/gavin/bc_libs
[19]: ./manuals/benchmarks.md
[20]: https://git.gavinhoward.com/gavin/bc
[21]: https://gavinhoward.com/2020/04/i-am-moving-away-from-github/
[22]: https://www.deepl.com/translator
[23]: https://cgit.freebsd.org/src/tree/contrib/bc
[24]: https://bugs.freebsd.org/
[25]: https://reviews.freebsd.org/
[26]: ./manuals/bcl.3.md
[27]: https://en.wikipedia.org/wiki/Bus_factor
[28]: ./manuals/development.md
[29]: https://github.com/gavinhoward/bc
[30]: ./manuals/bc/A.1.md#extended-library
[31]: ./manuals/build.md#settings
