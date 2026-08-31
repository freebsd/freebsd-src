dnl Copyright 2011 Google Inc.
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

AC_INIT([Lutok], [0.6.2],
        [testing@FreeBSD.org], [lutok],
        [https://github.com/freebsd/lutok/])
AC_PREREQ([2.68])


AC_COPYRIGHT([Copyright 2011 Google Inc.])
AC_CONFIG_AUX_DIR([admin])
AC_CONFIG_FILES([Doxyfile Makefile])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([state.hpp])


AM_INIT_AUTOMAKE([1.9 check-news foreign subdir-objects -Wall])

AM_PROG_AR
LT_INIT


AC_LANG([C++])
AC_PROG_CXX
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

# Check for Git
AC_PATH_PROG([GIT], [git])

# Define pkg-config and tests directory
AC_SUBST(pkgconfigdir, \${libdir}/pkgconfig)
AC_SUBST(testsdir, \${exec_prefix}/tests/lutok)

# Output configuration
AC_OUTPUT
