dnl Copyright (c) 2007 The NetBSD Foundation, Inc.
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions
dnl are met:
dnl 1. Redistributions of source code must retain the above copyright
dnl    notice, this list of conditions and the following disclaimer.
dnl 2. Redistributions in binary form must reproduce the above copyright
dnl    notice, this list of conditions and the following disclaimer in the
dnl    documentation and/or other materials provided with the distribution.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
dnl CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
dnl INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
dnl MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
dnl IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
dnl DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
dnl DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
dnl GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
dnl INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
dnl IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
dnl OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
dnl IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

dnl -----------------------------------------------------------------------
dnl Initialize the GNU build system.
dnl -----------------------------------------------------------------------

AC_INIT([Automated Testing Framework], [0.22],
        [atf-discuss@googlegroups.com], [atf],
        [https://github.com/jmmv/atf/])
AC_PREREQ([2.65])
AC_COPYRIGHT([Copyright (c) 2007-2012 The NetBSD Foundation, Inc.])
AC_DEFINE([PACKAGE_COPYRIGHT],
          ["Copyright (c) 2007-2012 The NetBSD Foundation, Inc."],
          [Define to the copyright string applicable to this package.])
AC_CONFIG_AUX_DIR([admin])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([atf-c.h])
AC_CONFIG_TESTDIR([bootstrap])

AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([1.9 check-news foreign subdir-objects -Wall])

AM_PROG_AR
LT_INIT

dnl -----------------------------------------------------------------------
dnl Check for the C and C++ compilers and required features.
dnl -----------------------------------------------------------------------

AC_LANG(C)
AC_PROG_CC
AM_PROG_CC_C_O
dnl The C compiler check automatically aborts if the compiler does not work.
dnl Nothing to do here.

AC_LANG(C++)
AC_PROG_CXX
AC_CACHE_CHECK([whether the C++ compiler works],
               [atf_cv_prog_cxx_works],
               [AC_LANG_PUSH([C++])
                AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
                               [atf_cv_prog_cxx_works=yes],
                               [atf_cv_prog_cxx_works=no])
                AC_LANG_POP])
if test "${atf_cv_prog_cxx_works}" = no; then
    AC_MSG_ERROR([C++ compiler cannot create executables])
fi

KYUA_DEVELOPER_MODE([C,C++])

dnl TODO(jmmv): Remove once the atf-*-api.3 symlinks are removed.
AC_PROG_LN_S

ATF_MODULE_APPLICATION
ATF_MODULE_DEFS
ATF_MODULE_ENV
ATF_MODULE_FS

ATF_RUNTIME_TOOL([ATF_BUILD_CC],
                 [C compiler to use at runtime], [${CC}])
ATF_RUNTIME_TOOL([ATF_BUILD_CFLAGS],
                 [C compiler flags to use at runtime], [${CFLAGS}])
ATF_RUNTIME_TOOL([ATF_BUILD_CPP],
                 [C/C++ preprocessor to use at runtime], [${CPP}])
ATF_RUNTIME_TOOL([ATF_BUILD_CPPFLAGS],
                 [C/C++ preprocessor flags to use at runtime], [${CPPFLAGS}])
ATF_RUNTIME_TOOL([ATF_BUILD_CXX],
                 [C++ compiler to use at runtime], [${CXX}])
ATF_RUNTIME_TOOL([ATF_BUILD_CXXFLAGS],
                 [C++ compiler flags to use at runtime], [${CXXFLAGS}])

dnl -----------------------------------------------------------------------
dnl Generation of files in srcdir.
dnl -----------------------------------------------------------------------

dnl BSD make(1) doesn't deal with targets specified as './foo' well: they
dnl need to be specified as 'foo'.  The following hack is to workaround this
dnl issue.
if test "${srcdir}" = .; then
    target_srcdir=
else
    target_srcdir="${srcdir}/"
fi
AC_SUBST([target_srcdir])

dnl -----------------------------------------------------------------------
dnl User-customizable variables.
dnl -----------------------------------------------------------------------

AC_SUBST(atf_aclocaldir, \${datadir}/aclocal)
AC_SUBST(atf_pkgconfigdir, \${libdir}/pkgconfig)

dnl -----------------------------------------------------------------------
dnl Check for the shell and portability problems.
dnl -----------------------------------------------------------------------

AC_ARG_VAR([ATF_SHELL], [Location of the POSIX shell interpreter to use])
if test x"${ATF_SHELL}" = x""; then
    AC_PATH_PROGS(ATF_SHELL, [bash sh])
else
    case ${ATF_SHELL} in
    /*)
        ;;
    *)
        AC_MSG_ERROR([ATF_SHELL must hold an absolute path])
        ;;
    esac
fi
if test x"${ATF_SHELL}" = x""; then
    AC_MSG_ERROR([No POSIX shell interpreter found; maybe set ATF_SHELL?])
fi

dnl -----------------------------------------------------------------------
dnl Check for required tools.
dnl -----------------------------------------------------------------------

AC_PATH_PROG([KYUA], [kyua])
AM_CONDITIONAL([HAVE_KYUA], [test -n "${KYUA}"])
AC_PATH_PROG([GIT], [git])

dnl -----------------------------------------------------------------------
dnl Finally, generate output.
dnl -----------------------------------------------------------------------

AC_OUTPUT([Makefile atf-c/defs.h])

dnl vim: syntax=m4:expandtab:shiftwidth=4:softtabstop=4
