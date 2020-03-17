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

AC_INIT([Lutok], [0.4],
        [lutok-discuss@googlegroups.com], [lutok],
        [https://github.com/jmmv/lutok/])
AC_PREREQ([2.65])


AC_COPYRIGHT([Copyright 2011 Google Inc.])
AC_CONFIG_AUX_DIR([admin])
AC_CONFIG_FILES([Doxyfile Makefile])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([state.hpp])


AM_INIT_AUTOMAKE([1.9 check-news foreign subdir-objects -Wall])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
LT_INIT


AC_LANG([C++])
AC_PROG_CXX
KYUA_REQUIRE_CXX
KYUA_DEVELOPER_MODE([C++])


ATF_CHECK_CXX([>= 0.15])
ATF_CHECK_SH([>= 0.15])
ATF_ARG_WITH
KYUA_DOXYGEN
KYUA_LUA


AC_PATH_PROG([KYUA], [kyua])
AM_CONDITIONAL([HAVE_KYUA], [test -n "${KYUA}"])
AC_PATH_PROG([GIT], [git])


AC_SUBST(pkgconfigdir, \${libdir}/pkgconfig)
AC_SUBST(testsdir, \${exec_prefix}/tests/lutok)


AC_OUTPUT
