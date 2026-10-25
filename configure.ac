dnl Copyright 2010 The Kyua Authors.
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions are
dnl met:
dnl
dnl * Redistributions of source code must retain the above copyright
dnl   notice, this list of conditions and the following disclaimer.
dnl * Redistributions in binary form must reproduce the above copyright
dnl   notice, this list of conditions and the following disclaimer in the
dnl   documentation and/or other materials provided with the distribution.
dnl * Neither the name of Google Inc. nor the names of its contributors
dnl   may be used to endorse or promote products derived from this software
dnl   without specific prior written permission.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
dnl "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
dnl LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
dnl A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
dnl OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
dnl SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
dnl LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
dnl DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
dnl THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
dnl (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
dnl OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

AC_INIT([Kyua], [0.15.1], [freebsd-testing@FreeBSD.org], [kyua],
        [https://github.com/freebsd/kyua/])
AC_PREREQ([2.65])


AC_COPYRIGHT([Copyright 2010 The Kyua Authors.])
AC_CONFIG_AUX_DIR([admin])
AC_CONFIG_FILES([Doxyfile Makefile utils/defs.hpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([main.cpp])
AC_CONFIG_TESTDIR([bootstrap])


AM_INIT_AUTOMAKE([1.9 foreign subdir-objects -Wall])

pc_cflags=
pc_cxxflags=
pc_ldflags=

AC_ARG_ENABLE([asan],
AS_HELP_STRING([--enable-asan], [Turn on AddressSanitizer]),,
[enable_asan=no])

if test ${enable_asan} = yes; then
    AC_MSG_NOTICE([building with AddressSanitizer (ASAN)])
    enable_developer=yes
    CXXFLAGS="${CXXFLAGS} -fsanitize=address"
    pc_cxxflags="${pc_cxxflags} -fsanitize=address"
    LDFLAGS="${LDFLAGS} -fsanitize=address"
    pc_ldflags="${pc_ldflags} -fsanitize=address"
fi
AM_CONDITIONAL([HAVE_ASAN], [test "${enable_asan}" = yes])

AC_ARG_ENABLE([code-coverage],
AS_HELP_STRING([--enable-code-coverage], [Turn on runtime code coverage]),,
[enable_code_coverage=no])

if test ${enable_code_coverage} = yes; then
    AC_MSG_NOTICE([building with code coverage])
    enable_developer=yes
    CXXFLAGS="${CXXFLAGS} --coverage"
    pc_cxxflags="${pc_cxxflags} --coverage"
    LDFLAGS="${LDFLAGS} --coverage"
    pc_ldflags="${pc_ldflags} --coverage"
fi
AM_CONDITIONAL([CODE_COVERAGE], [test "${enable_code_coverage}" = yes])

AC_ARG_ENABLE([lsan],
AS_HELP_STRING([--enable-lsan], [Turn on LeakSanitizer]),,
[enable_lsan=no])

if test ${enable_lsan} = yes; then
    AC_MSG_NOTICE([building with LeakSanitizer (LSAN)])
    enable_developer=yes
    CXXFLAGS="${CXXFLAGS} -fsanitize=leak"
    pc_cxxflags="${pc_cxxflags} -fsanitize=leak"
    LDFLAGS="${LDFLAGS} -fsanitize=leak"
    pc_ldflags="${pc_ldflags} -fsanitize=leak"
fi
AM_CONDITIONAL([HAVE_LSAN], [test "${enable_lsan}" = yes])

AC_ARG_ENABLE([ubsan],
AS_HELP_STRING([--enable-ubsan], [Turn on UndefinedBehaviourSanitizer]),,
[enable_ubsan=no])

if test ${enable_ubsan} = yes; then
    AC_MSG_NOTICE([building with UndefinedBehaviourSanitizer (UBSAN)])
    enable_developer=yes
    CXXFLAGS="${CXXFLAGS} -fsanitize=undefined"
    pc_cxxflags="${pc_cxxflags} -fsanitize=undefined"
    LDFLAGS="${LDFLAGS} -fsanitize=undefined"
    pc_ldflags="${pc_ldflags} -fsanitize=undefined"
fi
AM_CONDITIONAL([HAVE_UBSAN], [test "${enable_ubsan}" = yes])

AC_SUBST([pc_cflags])
AC_SUBST([pc_cxxflags])
AC_SUBST([pc_ldflags])

AC_PROG_CPP

AC_LANG_COMPILER(C++)
AC_PROG_CXX
AM_PROG_AR
AX_CXX_COMPILE_STDCXX(20, noext, mandatory)
KYUA_DEVELOPER_MODE([C++])
KYUA_ATTRIBUTE_PURE
KYUA_FS_MODULE
KYUA_GETOPT
KYUA_LAST_SIGNO
KYUA_MEMORY
AC_CHECK_FUNCS([putenv setenv unsetenv])
AC_CHECK_HEADERS([termios.h])

LT_INIT

m4_ifndef([PKG_CHECK_MODULES],
    [m4_fatal([Cannot find pkg.m4; see the INSTALL.md document for help])])

# Check for ATF and add --enable-atf flag
AC_ARG_ENABLE([atf],
    [AS_HELP_STRING([--enable-atf], [Enable ATF tests (default: no)])],
    [enable_atf=$enableval],
    [enable_atf=no])
AM_CONDITIONAL([WITH_ATF], [test "x$enable_atf" = "xyes"])

AS_IF([test "x$enable_atf" = "xyes"],[
m4_ifndef([ATF_CHECK_CXX],
    [m4_fatal([Cannot find atf-c++.m4; see the INSTALL.md document for help])])
ATF_CHECK_CXX([>= 0.22])
m4_ifndef([ATF_CHECK_SH],
    [m4_fatal([Cannot find atf-sh.m4; see the INSTALL.md document for help])])
ATF_CHECK_SH([>= 0.22])
],[])

PKG_CHECK_MODULES([LUTOK], [lutok >= 0.5],
                  [],
                  AC_MSG_ERROR([lutok (0.5 or newer) is required]))
PKG_CHECK_MODULES([SQLITE3], [sqlite3 >= 3.6.22],
                  [],
                  AC_MSG_ERROR([sqlite3 (3.6.22 or newer) is required]))
KYUA_DOXYGEN
AC_PATH_PROG([GDB], [gdb])
test -n "${GDB}" || GDB=gdb
AC_PATH_PROG([GIT], [git])
if test x"${enable_code_coverage}" = xyes; then
    AC_MSG_NOTICE([Checking for lcov package])
    AC_PATH_PROG([GENHTML], [genhtml])
    AC_PATH_PROG([LCOV], [lcov])
    if test -z "${GENHTML}" -a -z "${LCOV}"; then
        AC_MSG_ERROR([Please install lcov to obtain code coverage])
    fi
fi


KYUA_UNAME_ARCHITECTURE
KYUA_UNAME_PLATFORM


AC_ARG_VAR([KYUA_CONFSUBDIR],
           [Subdirectory of sysconfdir under which to look for files])
if test x"${KYUA_CONFSUBDIR-unset}" = x"unset"; then
    KYUA_CONFSUBDIR=kyua
else
    case ${KYUA_CONFSUBDIR} in
    /*)
        AC_MSG_ERROR([KYUA_CONFSUBDIR must hold a relative path])
        ;;
    *)
        ;;
    esac
fi
if test x"${KYUA_CONFSUBDIR}" = x""; then
    AC_SUBST(kyua_confdir, \${sysconfdir})
else
    AC_SUBST(kyua_confdir, \${sysconfdir}/${KYUA_CONFSUBDIR})
fi


dnl Allow the caller of 'make check', 'make installcheck' and 'make distcheck'
dnl on the Kyua source tree to override the configuration file passed to our
dnl own test runs.  This is for the development of Kyua only and the value of
dnl this setting has no effect on the built product in any way.  If we go
dnl through great extents in validating the value of this setting, it is to
dnl minimize the chance of false test run negatives later on.
AC_ARG_VAR([KYUA_CONFIG_FILE_FOR_CHECK],
           [kyua.conf file to use at 'make (|dist|install)check' time])
case "${KYUA_CONFIG_FILE_FOR_CHECK-none}" in
none)
    KYUA_CONFIG_FILE_FOR_CHECK=none
    ;;
/*)
    if test -f "${KYUA_CONFIG_FILE_FOR_CHECK}"; then
        : # All good!
    else
        AC_MSG_ERROR([KYUA_CONFIG_FILE_FOR_CHECK file does not exist])
    fi
    ;;
*)
    AC_MSG_ERROR([KYUA_CONFIG_FILE_FOR_CHECK must hold an absolute path])
    ;;
esac


AC_ARG_VAR([KYUA_TMPDIR],
           [Path to the directory in which to place work directories])
case "${KYUA_TMPDIR:-unset}" in
    unset)
        KYUA_TMPDIR=/tmp
        ;;
    /*)
        ;;
    *)
        AC_MSG_ERROR([KYUA_TMPDIR must be an absolute path])
        ;;
esac


AC_SUBST(examplesdir, \${pkgdatadir}/examples)
AC_SUBST(luadir, \${pkgdatadir}/lua)
AC_SUBST(miscdir, \${pkgdatadir}/misc)
AC_SUBST(pkgtestsdir, \${testsdir}/kyua)
AC_SUBST(storedir, \${pkgdatadir}/store)
AC_SUBST(testsdir, \${exec_prefix}/tests)


dnl BSD make(1) doesn't deal with targets specified as './foo' well: they
dnl need to be specified as 'foo'.  The following hack is to workaround this
dnl issue.
if test "${srcdir}" = .; then
    target_srcdir=
else
    target_srcdir="${srcdir}/"
fi
AM_CONDITIONAL(TARGET_SRCDIR_EMPTY, [test -z "${target_srcdir}"])
AC_SUBST([target_srcdir])

AM_CONDITIONAL([FreeBSD], [test "$(uname -o)" = "FreeBSD"])

AC_OUTPUT
