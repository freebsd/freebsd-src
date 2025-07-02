AC_PREREQ([2.69])
AC_INIT([OpenPAM], [20250531], [des@des.dev], [openpam], [https://openpam.org/])
AC_CONFIG_SRCDIR([lib/libpam/pam_start.c])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS([config.h])

# C compiler and features
AC_LANG(C)
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC([clang gcc cc])
AC_PROG_CPP
AC_PROG_CXX([clang++ g++ c++])
AC_C_CONST
AC_C_RESTRICT
AC_C_VOLATILE
AX_COMPILER_VENDOR

# libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static dlopen])

# pkg-config
AX_PROG_PKG_CONFIG

# other programs
AC_PROG_INSTALL

LIB_MAJ=2
AC_SUBST(LIB_MAJ)
AC_DEFINE_UNQUOTED(LIB_MAJ, $LIB_MAJ, [OpenPAM library major number])

AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug],
        [turn debugging macros on]),
    AC_DEFINE(OPENPAM_DEBUG, 1, [Turn debugging macros on]))

AC_ARG_ENABLE([unversioned-modules],
    AS_HELP_STRING([--disable-unversioned-modules],
        [support loading of unversioned modules]),
    [AS_IF([test x"$enableval" = x"no"], [
        AC_DEFINE(DISABLE_UNVERSIONED_MODULES,
            1,
            [Whether loading unversioned modules support is disabled])
    ])])

AC_ARG_WITH([modules-dir],
    AS_HELP_STRING([--with-modules-dir=DIR],
        [OpenPAM modules directory]),
    [AS_IF([test x"$withval" != x"no"], [
        OPENPAM_MODULES_DIR="$withval"
        AC_DEFINE_UNQUOTED(OPENPAM_MODULES_DIR,
            "${OPENPAM_MODULES_DIR%/}",
            [OpenPAM modules directory])
    ])])
AC_SUBST(OPENPAM_MODULES_DIR)
AM_CONDITIONAL([CUSTOM_MODULES_DIR], [test x"$OPENPAM_MODULES_DIR" != x""])

AS_IF([test x"$LOCALBASE" = x""], [
    AS_IF([test x"$prefix" = x"NONE"],
        [LOCALBASE=/usr/local], [LOCALBASE=$prefix])])
AC_ARG_WITH([localbase],
    AS_HELP_STRING([--with-localbase=DIR],
        [Base directory for local policies and modules]),
    AS_CASE([$withval],
	[no], [LOCALBASE=],
        [yes], [LOCALBASE=$ac_default_prefix],
	[*], [LOCALBASE="$withval"]))
AS_IF([test x"$LOCALBASE" != x""], [
    AC_DEFINE_UNQUOTED(LOCALBASE, "${LOCALBASE%/}",
        [Base directory for local policies and modules])])

AC_ARG_WITH([doc],
    AS_HELP_STRING([--without-doc], [do not build documentation]),
    [],
    [with_doc=yes])
AM_CONDITIONAL([WITH_DOC], [test x"$with_doc" = x"yes"])

AC_ARG_WITH([pam-unix],
    AS_HELP_STRING([--with-pam-unix], [build sample pam_unix(8) module]),
    [],
    [with_pam_unix=no])
AM_CONDITIONAL([WITH_PAM_UNIX], [test x"$with_pam_unix" = x"yes"])

AC_ARG_WITH(pamtest,
    AS_HELP_STRING([--with-pamtest], [build test application]),
    [],
    [with_pamtest=no])
AM_CONDITIONAL([WITH_PAMTEST], [test x"$with_pamtest" = x"yes"])

AC_ARG_WITH(su,
    AS_HELP_STRING([--with-su], [build sample su(1) implementation]),
    [],
    [with_su=no])
AM_CONDITIONAL([WITH_SU], [test x"$with_su" = x"yes"])

AC_ARG_WITH(system-libpam,
    AS_HELP_STRING([--with-system-libpam], [use system libpam]),
    [],
    [with_system_libpam=no])
AM_CONDITIONAL([WITH_SYSTEM_LIBPAM], [test x"$with_system_libpam" = x"yes"])

AC_CHECK_HEADERS([crypt.h])

AC_CHECK_FUNCS([asprintf vasprintf])
AC_CHECK_FUNCS([dlfunc fdlopen])
AC_CHECK_FUNCS([fpurge])
AC_CHECK_FUNCS([setlogmask])
AC_CHECK_FUNCS([strlcat strlcmp strlcpy strlset])

saved_LIBS="${LIBS}"
LIBS=""
AC_CHECK_LIB([dl], [dlopen])
DL_LIBS="${LIBS}"
LIBS="${saved_LIBS}"
AC_SUBST(DL_LIBS)

saved_LIBS="${LIBS}"
LIBS=""
AC_CHECK_LIB([pam], [pam_start])
SYSTEM_LIBPAM="${LIBS}"
LIBS="${saved_LIBS}"
AC_SUBST(SYSTEM_LIBPAM)

AX_PKG_CONFIG_CHECK([cryb-test],
  [AC_MSG_NOTICE([Cryb test framework found, unit tests enabled.])],
  [AC_MSG_WARN([Cryb test framework not found, unit tests disabled.])])
AM_CONDITIONAL([WITH_TEST], [test x"$CRYB_TEST_LIBS" != x""])

AC_ARG_ENABLE([developer-warnings],
    AS_HELP_STRING([--enable-developer-warnings], [enable strict warnings (default is NO)]),
    [CFLAGS="${CFLAGS} -Wall -Wextra -Wcast-qual"])
AC_ARG_ENABLE([debugging-symbols],
    AS_HELP_STRING([--enable-debugging-symbols], [enable debugging symbols (default is NO)]),
    [CFLAGS="${CFLAGS} -O0 -g -fno-inline"])
AC_ARG_ENABLE([werror],
    AS_HELP_STRING([--enable-werror], [use -Werror (default is NO)]),
    [CFLAGS="${CFLAGS} -Werror"])

AC_ARG_ENABLE([code-coverage],
    AS_HELP_STRING([--enable-code-coverage],
        [enable code coverage]))
AS_IF([test x"$enable_code_coverage" = x"yes"], [
    AM_COND_IF([WITH_TEST], [
        AS_IF([test x"$ax_cv_c_compiler_vendor" = x"clang"], [
            CFLAGS="${CFLAGS} -fprofile-instr-generate -fcoverage-mapping"
            clang_code_coverage="yes"
	    AC_SUBST([clang_ver], [${CC#clang}])
        ], [
            AC_MSG_ERROR([code coverage is only supported with clang])
        ])
        AC_DEFINE([WITH_CODE_COVERAGE], [1], [Define to 1 if code coverage is enabled])
        AC_MSG_NOTICE([code coverage enabled])
    ], [
        AC_MSG_ERROR([code coverage requires unit tests])
    ])
])
AM_CONDITIONAL([WITH_CODE_COVERAGE], [test x"$enable_code_coverage" = x"yes"])
AM_CONDITIONAL([CLANG_CODE_COVERAGE], [test x"$clang_code_coverage" = x"yes"])

AC_CONFIG_FILES([
    Makefile
    bin/Makefile
    bin/openpam_dump_policy/Makefile
    bin/pamtest/Makefile
    bin/su/Makefile
    doc/Makefile
    doc/man/Makefile
    freebsd/Makefile
    include/Makefile
    include/security/Makefile
    lib/Makefile
    lib/libpam/Makefile
    misc/Makefile
    modules/Makefile
    modules/pam_deny/Makefile
    modules/pam_permit/Makefile
    modules/pam_return/Makefile
    modules/pam_unix/Makefile
    t/Makefile
])
AC_CONFIG_FILES([misc/coverity.sh],[chmod +x misc/coverity.sh])
AC_OUTPUT
