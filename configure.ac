dnl Copyright (c) 2011-2014 Google Inc.
dnl Copyright (c) 2024-2026 Enji Cooper <ngie@FreeBSD.org>.
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

AC_INIT([Lutok], [0.7.0],
        [testing@FreeBSD.org], [lutok],
        [https://github.com/freebsd/lutok/])
AC_PREREQ([2.68])


AC_COPYRIGHT([Copyright 2011 Google Inc.])
AC_CONFIG_AUX_DIR([admin])
AC_CONFIG_FILES([Doxyfile Makefile])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([state.hpp])

AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([1.9 check-news foreign subdir-objects -Wall])

AM_PROG_AR
LT_INIT

pc_cflags=
pc_cxxflags=
pc_ldflags=

AC_ARG_ENABLE([asan],
AS_HELP_STRING([--enable-asan], [Turn on AddressSanitizer]),,
[enable_asan=no])

if test ${enable_asan} = yes; then
    AC_MSG_NOTICE([building with AddressSanitizer (ASAN)])
    enable_developer=yes
    pc_cflags="${pc_cflags} -fsanitize=address"
    pc_cxxflags="${pc_cxxflags} -fsanitize=address"
    pc_ldflags="${pc_ldflags} -fsanitize=address"
fi
AM_CONDITIONAL([HAVE_ASAN], [test "${enable_asan}" = yes])

AC_ARG_ENABLE([code-coverage],
AS_HELP_STRING([--enable-code-coverage], [Turn on runtime code coverage]),,
[enable_code_coverage=no])

if test ${enable_code_coverage} = yes; then
    AC_MSG_NOTICE([building with code coverage])
    enable_developer=yes
    pc_cflags="${pc_cflags} --coverage"
    pc_cxxflags="${pc_cxxflags} --coverage"
    pc_ldflags="${pc_ldflags} --coverage"
fi
AM_CONDITIONAL([CODE_COVERAGE], [test "${enable_code_coverage}" = yes])

AC_ARG_ENABLE([lsan],
AS_HELP_STRING([--enable-lsan], [Turn on LeakSanitizer]),,
[enable_lsan=no])

if test ${enable_lsan} = yes; then
    AC_MSG_NOTICE([building with LeakSanitizer (LSAN)])
    enable_developer=yes
    pc_cflags="${pc_cflags} -fsanitize=leak"
    pc_cxxflags="${pc_cxxflags} -fsanitize=leak"
    pc_ldflags="${pc_ldflags} -fsanitize=leak"
fi
AM_CONDITIONAL([HAVE_LSAN], [test "${enable_lsan}" = yes])

AC_ARG_ENABLE([ubsan],
AS_HELP_STRING([--enable-ubsan], [Turn on UndefinedBehaviourSanitizer]),,
[enable_ubsan=no])

if test ${enable_ubsan} = yes; then
    AC_MSG_NOTICE([building with UndefinedBehaviourSanitizer (UBSAN)])
    enable_developer=yes
    pc_cflags="${pc_cflags} -fsanitize=undefined"
    pc_cxxflags="${pc_cxxflags} -fsanitize=undefined"
    pc_ldflags="${pc_ldflags} -fsanitize=undefined"
fi
AM_CONDITIONAL([HAVE_UBSAN], [test "${enable_ubsan}" = yes])

AC_SUBST([pc_cflags])
AC_SUBST([pc_cxxflags])
AC_SUBST([pc_ldflags])


dnl -----------------------------------------------------------------------
dnl Check for the C and C++ compilers and required features.
dnl -----------------------------------------------------------------------

AC_LANG(C)
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP
dnl The C compiler check automatically aborts if the compiler does not work.
dnl Nothing to do here.

AC_PROG_CXX
AC_LANG_COMPILER(C++)
AC_PROG_CPP
AX_CXX_COMPILE_STDCXX(20, noext, mandatory)
KYUA_DEVELOPER_MODE([C++])

# Check for ATF and add --enable-atf flag
AC_ARG_ENABLE([atf],
  [AS_HELP_STRING([--enable-atf], [Enable ATF tests (default: no)])],
  [enable_atf=$enableval],
  [enable_atf=no])
AM_CONDITIONAL([WITH_ATF], [test "x$enable_atf" = "xyes"])

# ATF dependencies
m4_ifdef([ATF_CHECK_CXX],[
AS_IF([test "x$enable_atf" = "xyes"],[
  ATF_CHECK_CXX([>= 0.21])
  ATF_CHECK_SH([>= 0.21])
],[AC_MSG_NOTICE([ATF support disabled])])
])

KYUA_DOXYGEN
KYUA_LUA

# Check for Kyua
AC_PATH_PROG([KYUA], [kyua])
AM_CONDITIONAL([HAVE_KYUA], [test -n "${KYUA}"])
AC_PATH_PROG([GIT], [git])
if test x"${enable_code_coverage}" = xyes; then
    AC_MSG_NOTICE([Checking for lcov package])
    AC_PATH_PROG([GENHTML], [genhtml])
    AC_PATH_PROG([LCOV], [lcov])
    if test -z "${GENHTML}" -a -z "${LCOV}"; then
        AC_MSG_ERROR([Please install lcov to obtain code coverage])
    fi
fi

# Define pkg-config and tests directory
AC_SUBST(pkgconfigdir, \${libdir}/pkgconfig)
AC_SUBST(testsdir, \${exec_prefix}/tests)

AC_OUTPUT

dnl vim: syntax=m4:expandtab:shiftwidth=4:softtabstop=4
