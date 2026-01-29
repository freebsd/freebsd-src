dnl configure.ac
dnl m4 preprocessor script for autotools
dnl
dnl Copyright (c) 2011, 2012, 2013, 2014 pkgconf authors (see AUTHORS).
dnl
dnl Permission to use, copy, modify, and/or distribute this software for any
dnl purpose with or without fee is hereby granted, provided that the above
dnl copyright notice and this permission notice appear in all copies.
dnl
dnl This software is provided 'as is' and without any warranty, express or
dnl implied.  In no event shall the authors be liable for any damages arising
dnl from the use of this software.

AC_PREREQ([2.71])
AC_INIT([pkgconf],[2.5.1],[https://github.com/pkgconf/pkgconf/issues/new])
AC_CONFIG_SRCDIR([cli/main.c])
AC_CONFIG_MACRO_DIR([m4])
AX_CHECK_COMPILE_FLAG([-Wall], [CFLAGS="$CFLAGS -Wall"])
AX_CHECK_COMPILE_FLAG([-Wextra], [CFLAGS="$CFLAGS -Wextra"])
AX_CHECK_COMPILE_FLAG([-Wformat=2], [CFLAGS="$CFLAGS -Wformat=2"])
AX_CHECK_COMPILE_FLAG([-std=gnu99], [CFLAGS="$CFLAGS -std=gnu99"], [
	AX_CHECK_COMPILE_FLAG([-std=c99], [CFLAGS="$CFLAGS -std=c99"])
])
AC_CONFIG_HEADERS([libpkgconf/config.h])
AC_CHECK_DECLS([strlcpy, strlcat, strndup], [], [], [[#include <string.h>]])
AC_CHECK_DECLS([pledge, unveil], [], [], [[#include <unistd.h>]])
AC_CHECK_DECLS([readlinkat], [], [], [[#include <unistd.h>]])
AC_CHECK_DECLS([reallocarray], [], [], [[#include <stdlib.h>]])
AC_CHECK_HEADERS([sys/stat.h])
AM_INIT_AUTOMAKE([foreign dist-xz subdir-objects])
AM_SILENT_RULES([yes])
LT_INIT

AC_SYS_LARGEFILE

AC_ARG_WITH([personality-dir],[AS_HELP_STRING([--with-personality-dir],[specify
	     the place where cross-compile personality files will be found])],
	     PERSONALITY_PATH="$withval",
	     PERSONALITY_PATH="${datadir}/pkgconfig/personality.d:${sysconfdir}/pkgconfig/personality.d")

AC_SUBST([PERSONALITY_PATH])

AC_ARG_WITH([pkg-config-dir],[AS_HELP_STRING([--with-pkg-config-dir],[specify
	     the place where pc files will be found])],PKG_DEFAULT_PATH="$withval",
	     PKG_DEFAULT_PATH="${libdir}/pkgconfig:${datadir}/pkgconfig")

AC_SUBST([PKG_DEFAULT_PATH])

AC_ARG_WITH([system-libdir],[AS_HELP_STRING([--with-system-libdir],[specify the
	     system library directory (default LIBDIR)])],
	     [SYSTEM_LIBDIR="$withval"], [
	      AC_MSG_WARN([--with-system-libdir is not set, assuming ${libdir}])
	      AC_MSG_WARN([It is important that this value be properly set for correct behavior!])
	      SYSTEM_LIBDIR="${libdir}"
	     ])

AC_SUBST([SYSTEM_LIBDIR])

AC_ARG_WITH([system-includedir],[AS_HELP_STRING([--with-system-includedir],[specify the
	     system include directory (default INCLUDEDIR)])],
	     [SYSTEM_INCLUDEDIR="$withval"], [
	      AC_MSG_WARN([--with-system-includedir is not set, assuming ${includedir}])
	      AC_MSG_WARN([It is important that this value be properly set for correct behavior!])
	      SYSTEM_INCLUDEDIR="${includedir}"
	     ])

AC_SUBST([SYSTEM_INCLUDEDIR])

AC_PROG_CPP
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

AC_CONFIG_FILES([Makefile Kyuafile libpkgconf.pc tests/Kyuafile tests/test_env.sh])
AC_OUTPUT

cat << EOF
Build configuration:

	Default personality search paths:	${PERSONALITY_PATH}
	Default package search paths:		${PKG_DEFAULT_PATH}
	Default include search paths:		${SYSTEM_INCLUDEDIR}
	Default library search paths:		${SYSTEM_LIBDIR}

Incorrect paths may result in unexpected behavior from pkgconf.
EOF
