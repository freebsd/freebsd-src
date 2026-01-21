# pkgconf [![test](https://github.com/pkgconf/pkgconf/actions/workflows/test.yml/badge.svg)](https://github.com/pkgconf/pkgconf/actions/workflows/test.yml)

`pkgconf` is a program which helps to configure compiler and linker flags for
development libraries.  It is similar to pkg-config from freedesktop.org.

`libpkgconf` is a library which provides access to most of `pkgconf`'s functionality, 
to allow other tooling such as compilers and IDEs to discover and use libraries 
configured by pkgconf.

## using `pkgconf` with autotools

Implementations of pkg-config, such as pkgconf, are typically used with the
PKG_CHECK_MODULES autoconf macro.  As far as we know, pkgconf is
compatible with all known variations of this macro. pkgconf detects at
runtime whether or not it was started as 'pkg-config', and if so, attempts
to set program options such that its behaviour is similar.

In terms of the autoconf macro, it is possible to specify the PKG_CONFIG
environment variable, so that you can test pkgconf without overwriting your
pkg-config binary.  Some other build systems may also respect the PKG_CONFIG
environment variable.

To set the environment variable on the bourne shell and clones (i.e. bash), you
can run:

    $ export PKG_CONFIG=/usr/bin/pkgconf

## comparison of `pkgconf` and `pkg-config` dependency resolvers

pkgconf builds an acyclic directed dependency graph.  This allows for the user
to more conservatively link their binaries -- which may be helpful in some
environments, such as when prelink(1) is being used.  As a result of building
a directed dependency graph designed for the specific problem domain provided
by the user, more accurate dependencies can be determined.

Current release versions of pkg-config, on the other hand, build a database of all
known pkg-config files on the system before attempting to resolve dependencies, which
is a considerably slower and less efficient design.  Efforts have been made recently
to improve this behaviour.

As of the 1.1 series, pkgconf also fully implements support for `Provides` rules,
while pkg-config does not.  pkg-config only provides the `--print-provides` functionality
as a stub.  There are other intentional implementation differences in pkgconf's dependency
resolver versus pkg-config's dependency resolver in terms of completeness and correctness,
such as, for example, how `Conflicts` rules are processed.

## linker flags optimization

As previously mentioned, pkgconf makes optimizations to the linker flags in both the
case of static and shared linking in order to avoid overlinking binaries and also
simplifies the `CFLAGS` and `LIBS` output of the pkgconf tool for improved readability.

This functionality depends on the pkg-config module properly declaring its dependency
tree instead of using `Libs` and `Cflags` fields to directly link against other modules
which have pkg-config metadata files installed.

Doing so is discouraged by the [freedesktop tutorial][fd-tut] anyway.

   [fd-tut]: http://people.freedesktop.org/~dbn/pkg-config-guide.html

## compatibility with pkg-config

I really hate that I have to have this section, I like being a nice person, but we
unfortunately have to say this because otherwise we get passive-aggressive people who
try to argue with us about what pkg-config compatibility means.

We do not provide bug-level compatibility with pkg-config.

What that means is, if you feel that there is a legitimate regression versus pkg-config,
do let us know, but also make sure that the .pc files are valid and follow the rules of
the [pkg-config tutorial][fd-tut], as most likely fixing them to follow the specified
rules will solve the problem.

Additionally, **we do not consider pkgconf doing what you tell it to do, in cases for 
which pkg-config fails to do so, to be a bug**.

If, for example, you use environment variables such as `PKG_CONFIG_SYSTEM_[INCLUDE|LIBRARY]_PATH`
and then find yourself surprised that `pkgconf` is stripping `-I` and `-L` flags relating
to those paths, it is not a `pkgconf` problem -- `pkgconf` is doing exactly what you told
it to do.

We will reject bugs like this, and if someone insists on fixing such a non-bug, this 
constitutes a violation of our [Code of Conduct](CODE_OF_CONDUCT.md), which may be 
addressed by banning from this repository.

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

There are a few defines such as SYSTEM_LIBDIR, PKGCONFIGDIR and SYSTEM_INCLUDEDIR.
However, on Windows, the default PKGCONFIGDIR value is usually overridden at runtime based
on path relocation.

## pkg-config symlink

If you want pkgconf to be used when you invoke `pkg-config`, you should install a
symlink for this.  We do not do this for you, as we believe it is better for vendors
to make this determination themselves.

    $ ln -sf pkgconf /usr/bin/pkg-config

## release tarballs

Release tarballs are available at <https://distfiles.ariadne.space/pkgconf/>.
Please only use the tarballs from distfiles.ariadne.space.

## contacts

You can report bugs at <https://github.com/pkgconf/pkgconf/issues>.

There is a mailing list at <https://lists.sr.ht/~kaniini/pkgconf>.

You can contact us via IRC at `#pkgconf` at `irc.oftc.net`.
