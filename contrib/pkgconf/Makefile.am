libdir                 = @libdir@
datarootdir            = @datarootdir@
datadir                = @datadir@
includedir             = @includedir@
system_includedir      = @SYSTEM_INCLUDEDIR@
system_libdir          = @SYSTEM_LIBDIR@
pkg_default_dir        = @PKG_DEFAULT_PATH@
personality_dir        = @PERSONALITY_PATH@
pkgconfigdir           = $(libdir)/pkgconfig
nodist_pkgconfig_DATA  = libpkgconf.pc

ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -DPERSONALITY_PATH=\"$(personality_dir)\" -DPKG_DEFAULT_PATH=\"$(pkg_default_dir)\" -DSYSTEM_INCLUDEDIR=\"$(system_includedir)\" -DSYSTEM_LIBDIR=\"$(system_libdir)\"

bin_PROGRAMS = pkgconf bomtool
lib_LTLIBRARIES = libpkgconf.la

EXTRA_DIST =	pkg.m4 \
		meson.build \
		meson_options.txt \
		pkgconf.wxs.in \
		txt2rtf.py \
		libpkgconf/meson.build \
		libpkgconf/config.h.meson \
		libpkgconf/win-dirent.h \
		tests/lib-relocatable/lib/pkgconfig/foo.pc \
		tests/lib1/argv-parse-2.pc \
		tests/lib1/billion-laughs.pc \
		tests/lib1/dos-lineendings.pc \
		tests/lib1/paren-quoting.pc \
		tests/lib1/argv-parse-3.pc \
		tests/lib1/foo.pc \
		tests/lib1/foobar.pc \
		tests/lib1/unavailable-provider.pc \
		tests/lib1/prefix-foo1.pc \
		tests/lib1/argv-parse.pc \
		tests/lib1/framework-1.pc \
		tests/lib1/prefix-foo2.pc \
		tests/lib1/bar.pc \
		tests/lib1/framework-2.pc \
		tests/lib1/private-libs-duplication.pc \
		tests/lib1/baz.pc \
		tests/lib1/incomplete.pc \
		tests/lib1/quotes.pc \
		tests/lib1/case-sensitivity.pc \
		tests/lib1/intermediary-1.pc \
		tests/lib1/static-archive-libs.pc \
		tests/lib1/cflags-libs-only.pc \
		tests/lib1/intermediary-2.pc \
		tests/lib1/static-libs.pc \
		tests/lib1/circular-1.pc \
		tests/lib1/missing-require.pc \
		tests/lib1/sysroot-dir.pc \
		tests/lib1/circular-2.pc \
		tests/lib1/multiline.pc \
		tests/lib1/multiline-bogus.pc \
		tests/lib1/tilde-quoting.pc \
		tests/lib1/circular-3.pc \
		tests/lib1/no-trailing-newline.pc \
		tests/lib1/tilde.pc \
		tests/lib1/comments-in-fields.pc \
		tests/lib1/nocflag.pc \
		tests/lib1/typelibdir.pc \
		tests/lib2/foo.pc \
		tests/lib1/comments.pc \
		tests/lib1/nolib.pc \
		tests/lib3/bar.pc \
		tests/lib1/conflicts.pc \
		tests/lib1/omg-uninstalled.pc \
		tests/lib1/omg-sysroot-uninstalled.pc \
		tests/lib1/isystem.pc \
		tests/lib1/idirafter.pc \
		tests/lib1/idirafter-ordering.pc \
		tests/lib1/depgraph-break.pc \
		tests/lib1/cflags-whitespace.pc \
		tests/lib1/cflags-whitespace-trailing.pc \
		tests/lib1/provides.pc \
		tests/lib1/provides-request-simple.pc \
		tests/lib1/flag-order-1.pc \
		tests/lib1/flag-order-3.pc \
		tests/lib1/variable-whitespace.pc \
		tests/lib1/fragment-collision.pc \
		tests/lib1/fragment-collision-intermediary.pc \
		tests/lib1/fragment-collision-1.pc \
		tests/lib1/fragment-collision-2.pc \
		tests/lib1/fragment-comment.pc \
		tests/lib1/fragment-escaping-1.pc \
		tests/lib1/fragment-escaping-2.pc \
		tests/lib1/fragment-escaping-3.pc \
		tests/lib1/fragment-quoting.pc \
		tests/lib1/fragment-quoting-2.pc \
		tests/lib1/fragment-quoting-3.pc \
		tests/lib1/fragment-quoting-5.pc \
		tests/lib1/fragment-quoting-7.pc \
		tests/lib1/fragment-groups.pc \
		tests/lib1/fragment-groups-2.pc \
		tests/lib1/fragment-group-a.pc \
		tests/lib1/fragment-group-b.pc \
		tests/lib1/fragment-group-c.pc \
		tests/lib1/malformed-1.pc \
		tests/lib1/malformed-quoting.pc \
		tests/lib1/malformed-version.pc \
		tests/lib1/metapackage.pc \
		tests/lib1/metapackage-1.pc \
		tests/lib1/metapackage-2.pc \
		tests/lib1/metapackage-3.pc \
		tests/lib1/explicit-sysroot.pc \
		tests/lib1/escaped-backslash.pc \
		tests/lib1/cflags-internal.pc \
		tests/lib1/requires-internal.pc \
		tests/lib1/requires-internal-2.pc \
		tests/lib1/requires-internal-missing.pc \
		tests/lib1/requires-internal-collision.pc \
		tests/lib1/tuple-quoting.pc \
		tests/lib1/empty-tuple.pc \
		tests/lib1/orphaned-requires-private.pc \
		tests/lib1/pcfiledir.pc \
		tests/lib1/sysroot-dir-2.pc \
		tests/lib1/sysroot-dir-3.pc \
		tests/lib1/sysroot-dir-4.pc \
		tests/lib1/sysroot-dir-5.pc \
		tests/lib1/child-prefix/pkgconfig/child-prefix-1.pc \
		tests/lib1/cflags-libs-private-a.pc \
		tests/lib1/cflags-libs-private-b.pc \
		tests/lib1/cflags-libs-private-c.pc \
		tests/lib1/truncated.pc \
		tests/lib1/c-comment.pc \
		tests/meson.build \
		$(test_scripts) \
		doc/conf.py \
		doc/extract.py \
		doc/index.rst \
		doc/libpkgconf.rst \
		doc/libpkgconf-argvsplit.rst \
		doc/libpkgconf-audit.rst \
		doc/libpkgconf-cache.rst \
		doc/libpkgconf-client.rst \
		doc/libpkgconf-dependency.rst \
		doc/libpkgconf-fragment.rst \
		doc/libpkgconf-path.rst \
		doc/libpkgconf-pkg.rst \
		doc/libpkgconf-queue.rst \
		doc/libpkgconf-tuple.rst

test_scripts=	tests/basic.sh \
		tests/builtins.sh \
		tests/conflicts.sh \
		tests/framework.sh \
		tests/parser.sh \
		tests/provides.sh \
		tests/regress.sh \
		tests/requires.sh \
		tests/symlink.sh \
		tests/sysroot.sh \
		tests/version.sh

test_sh = $(test_scripts)
check_SCRIPTS = ${test_sh:.sh=}

SUFFIXES=	.sh

nobase_pkginclude_HEADERS = libpkgconf/bsdstubs.h libpkgconf/iter.h libpkgconf/libpkgconf.h libpkgconf/stdinc.h libpkgconf/libpkgconf-api.h
libpkgconf_la_SOURCES  =	\
		libpkgconf/audit.c		\
		libpkgconf/buffer.c		\
		libpkgconf/cache.c		\
		libpkgconf/client.c		\
		libpkgconf/pkg.c		\
		libpkgconf/bsdstubs.c		\
		libpkgconf/fragment.c		\
		libpkgconf/argvsplit.c		\
		libpkgconf/fileio.c		\
		libpkgconf/tuple.c		\
		libpkgconf/dependency.c		\
		libpkgconf/queue.c		\
		libpkgconf/path.c		\
		libpkgconf/personality.c	\
		libpkgconf/parser.c
libpkgconf_la_LDFLAGS = -no-undefined -version-info 7:0:0 -export-symbols-regex '^pkgconf_'

dist_man_MANS    = 		\
	man/bomtool.1		\
	man/pkgconf.1		\
	man/pkg.m4.7		\
	man/pc.5		\
	man/pkgconf-personality.5

pkgconf_LDADD    = libpkgconf.la
pkgconf_SOURCES  = \
	cli/main.c				\
	cli/getopt_long.c			\
	cli/renderer-msvc.c
pkgconf_CPPFLAGS = -I$(top_srcdir)/libpkgconf -I$(top_srcdir)/cli
noinst_HEADERS   = \
	cli/getopt_long.h			\
	cli/renderer-msvc.h

bomtool_LDADD    = libpkgconf.la
bomtool_SOURCES  = \
	cli/bomtool/main.c				\
	cli/getopt_long.c
bomtool_CPPFLAGS = -I$(top_srcdir)/libpkgconf -I$(top_srcdir)/cli -I$(top_srcdir)/cli/bomtool

dist_doc_DATA = README.md AUTHORS

m4datadir              = $(datadir)/aclocal
m4data_DATA            = pkg.m4

CLEANFILES =	$(EXTRA_PROGRAMS) \
		$(check_SCRIPTS)

check: pkgconf $(check_SCRIPTS)
	kyua --config=none test --kyuafile='$(top_builddir)/Kyuafile' \
		--build-root='$(top_builddir)'

.sh:
	install -m 755 $< $@
