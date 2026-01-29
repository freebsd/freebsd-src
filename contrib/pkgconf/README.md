# pkgconf [![test](https://github.com/pkgconf/pkgconf/actions/workflows/test.yml/badge.svg)](https://github.com/pkgconf/pkgconf/actions/workflows/test.yml)

`pkgconf` is a program which helps to configure compiler and linker flags for
development libraries.  It is a superset of the functionality provided by
pkg-config from freedesktop.org, but does not provide bug-compatibility with
the original pkg-config.

`libpkgconf` is a library which provides access to most of `pkgconf`'s functionality, 
to allow other tooling such as compilers and IDEs to discover and use libraries 
configured by pkgconf.

## release tarballs

Release tarballs are available on [distfiles.ariadne.space][distfiles].

   [distfiles]: https://distfiles.ariadne.space/pkgconf/

## build system setup

If you would like to use the git sources directly, or a snapshot of the
sources from GitHub, you will need to regenerate the autotools build
system artifacts yourself, or use Meson instead.  For example, on Alpine:

    $ apk add autoconf automake libtool build-base
    $ sh ./autogen.sh

## pkgconf-lite

If you only need the original pkg-config functionality, there is also pkgconf-lite,
which builds the `pkgconf` frontend and relevant portions of `libpkgconf` functionality
into a single binary:

    $ make -f Makefile.lite

## why `pkgconf` over original `pkg-config`?

pkgconf builds a flattened directed dependency graph, which allows for more insight
into relationships between dependencies, allowing for some link-time dependency
optimization, which allows for the user to more conservatively link their binaries,
which may be helpful in some environments, such as when prelink(1) is being used.

The solver is also optimized to handle large dependency graphs with hundreds of
thousands of edges, which can be seen in any project using the Abseil frameworks
for example.

In addition, pkgconf has full support for virtual packages, while the original
pkg-config does not, as well as fully supporting `Conflicts` at dependency
resolution time, which is more efficient than checking for `Conflicts` while
walking the dependency graph.

## linker flags optimization

pkgconf, when used effectively, can make optimizations to avoid overlinking binaries.

This functionality depends on the pkg-config module properly declaring its dependency
tree instead of using `Libs` and `Cflags` fields to directly link against other modules
which have pkg-config metadata files installed.

The practice of using `Libs` and `Cflags` to describe unrelated dependencies is
not recommended in [Dan Nicholson's pkg-config tutorial][fd-tut] for this reason.

   [fd-tut]: http://people.freedesktop.org/~dbn/pkg-config-guide.html

## bug compatibility with original pkg-config

In general, we do not provide bug-level compatibility with pkg-config.

What that means is, if you feel that there is a legitimate regression versus pkg-config,
do let us know, but also make sure that the .pc files are valid and follow the rules of
the [pkg-config tutorial][fd-tut], as most likely fixing them to follow the specified
rules will solve the problem.

## debug output

Please use only the stable interfaces to query pkg-config.  Do not screen-scrape the
output from `--debug`: this is sent to `stderr` for a reason, it is not intended to be
scraped.  The `--debug` output is **not** a stable interface, and should **never** be
depended on as a source of information.  If you need a stable interface to query pkg-config
which is not covered, please get in touch.

## compiling `pkgconf` and `libpkgconf` on UNIX

pkgconf is basically compiled the same way any other autotools-based project is
compiled:

    $ ./configure
    $ make
    $ sudo make install

If you are installing pkgconf into a custom prefix, such as `/opt/pkgconf`, you will
likely want to define the default system includedir and libdir for your toolchain.
To do this, use the `--with-system-includedir` and `--with-system-libdir` configure
flags like so:

    $ ./configure \
         --prefix=/opt/pkgconf \
         --with-system-libdir=/lib:/usr/lib \
         --with-system-includedir=/usr/include
    $ make
    $ sudo make install

## compiling `pkgconf` and `libpkgconf` with Meson (usually for Windows)

pkgconf is compiled using [Meson](https://mesonbuild.com) on Windows. In theory, you could also use
Meson to build on UNIX, but this is not recommended at this time as pkgconf is typically built
much earlier than Meson.

    $ meson setup build -Dtests=disabled
    $ meson compile -C build
    $ meson install -C build

There are a few defines such as `SYSTEM_LIBDIR`, `PKGCONFIGDIR` and `SYSTEM_INCLUDEDIR`.
However, on Windows, the default `PKGCONFIGDIR` value is usually overridden at runtime based
on path relocation.

## pkg-config symlink

If you want pkgconf to be used when you invoke `pkg-config`, you should install a
symlink for this.  We do not do this for you, as we believe it is better for vendors
to make this determination themselves.

    $ ln -sf pkgconf /usr/bin/pkg-config

## contacts

You can report bugs at <https://github.com/pkgconf/pkgconf/issues>.

There is a mailing list at <https://lists.sr.ht/~kaniini/pkgconf>.

You can contact us via IRC at `#pkgconf` at `irc.oftc.net`.
