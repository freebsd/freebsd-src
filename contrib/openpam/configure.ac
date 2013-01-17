dnl $Id: configure.ac 610 2012-05-26 14:03:45Z des $

AC_PREREQ([2.62])
AC_REVISION([$Id: configure.ac 610 2012-05-26 14:03:45Z des $])
AC_INIT([OpenPAM], [20120526], [des@des.no])
AC_CONFIG_SRCDIR([lib/pam_start.c])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER([config.h])

# C compiler and features
AC_LANG(C)
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_CPP
AC_GNU_SOURCE
AC_C_CONST
AC_C_RESTRICT
AC_C_VOLATILE

# libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static dlopen])

# other programs
AC_PROG_INSTALL

LIB_MAJ=2
AC_SUBST(LIB_MAJ)
AC_DEFINE_UNQUOTED(LIB_MAJ, $LIB_MAJ, [OpenPAM library major number])

AC_ARG_ENABLE([debug],
    AC_HELP_STRING([--enable-debug],
	[turn debugging on by default]),
    AC_DEFINE(OPENPAM_DEBUG, 1, [Turn debugging on by default]))

AC_ARG_ENABLE([unversioned-modules],
    AC_HELP_STRING([--disable-unversioned-modules],
	[support loading of unversioned modules]),
    [AS_IF([test x"$enableval" = x"no"], [
	AC_DEFINE(DISABLE_UNVERSIONED_MODULES,
	    1,
	    [Whether loading unversioned modules support is disabled])
    ])])

AC_ARG_WITH([modules-dir],
    AC_HELP_STRING([--with-modules-dir=DIR],
	[OpenPAM modules directory]),
    [AS_IF([test x"$withval" != x"no"], [
	OPENPAM_MODULES_DIR="$withval"
    ], [
	OPENPAM_MODULES_DIR="$libdir"
    ])],
    [OPENPAM_MODULES_DIR="$libdir"])
AC_DEFINE_UNQUOTED(OPENPAM_MODULES_DIR,
    "${OPENPAM_MODULES_DIR%/}/",
    [OpenPAM modules directory])
AC_SUBST(OPENPAM_MODULES_DIR)

AC_ARG_WITH([doc],
    AC_HELP_STRING([--without-doc], [do not build documentation]),
    [],
    [with_doc=yes])
AM_CONDITIONAL([WITH_DOC], [test x"$with_doc" = x"yes"])

AC_ARG_WITH([pam-unix],
    AC_HELP_STRING([--with-pam-unix], [compile sample pam_unix(8) implementation]),
    [],
    [with_pam_unix=no])
AM_CONDITIONAL([WITH_PAM_UNIX], [test x"$with_pam_unix" = x"yes"])

AC_ARG_WITH(pamtest,
    AC_HELP_STRING([--with-pamtest], [compile test application]),
    [],
    [with_pamtest=no])
AM_CONDITIONAL([WITH_PAMTEST], [test x"$with_pamtest" = x"yes"])

AC_ARG_WITH(su,
    AC_HELP_STRING([--with-su], [compile sample su(1) implementation]),
    [],
    [with_su=no])
AM_CONDITIONAL([WITH_SU], [test x"$with_su" = x"yes"])

AC_CHECK_HEADERS([crypt.h])

AC_CHECK_FUNCS([fdlopen fpurge strlcat strlcmp strlcpy])

saved_LIBS="${LIBS}"
LIBS=""
AC_CHECK_LIB([dl], [dlopen])
DL_LIBS="${LIBS}"
LIBS="${saved_LIBS}"
AC_SUBST(DL_LIBS)

saved_LIBS="${LIBS}"
LIBS=""
AC_CHECK_LIB([crypt], [crypt])
CRYPT_LIBS="${LIBS}"
LIBS="${saved_LIBS}"
AC_SUBST(CRYPT_LIBS)

AC_ARG_ENABLE([developer-warnings],
    AS_HELP_STRING([--enable-developer-warnings], [enable strict warnings (default is NO)]),
    [CFLAGS="${CFLAGS} -Wall -Wextra"])
AC_ARG_ENABLE([debugging-symbols],
    AS_HELP_STRING([--enable-debugging-symbols], [enable debugging symbols (default is NO)]),
    [CFLAGS="${CFLAGS} -O0 -g -fno-inline"])
AC_ARG_ENABLE([werror],
    AS_HELP_STRING([--enable-werror], [use -Werror (default is NO)]),
    [CFLAGS="${CFLAGS} -Werror"])

AC_CONFIG_FILES([
    Makefile
    bin/Makefile
    bin/openpam_dump_policy/Makefile
    bin/pamtest/Makefile
    bin/su/Makefile
    doc/Makefile
    doc/man/Makefile
    include/Makefile
    include/security/Makefile
    lib/Makefile
    modules/Makefile
    modules/pam_deny/Makefile
    modules/pam_permit/Makefile
    modules/pam_unix/Makefile
    t/Makefile
])
AC_CONFIG_FILES([pamgdb],[chmod +x pamgdb])
AC_OUTPUT
