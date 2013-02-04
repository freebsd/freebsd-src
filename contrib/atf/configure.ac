dnl
dnl Automated Testing Framework (atf)
dnl
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
dnl

dnl -----------------------------------------------------------------------
dnl Initialize the GNU build system.
dnl -----------------------------------------------------------------------

AC_INIT([Automated Testing Framework], [0.16], [atf-devel@NetBSD.org], [atf],
        [http://code.google.com/p/kyua/wiki/ATF])
AC_PREREQ([2.65])
AC_COPYRIGHT([Copyright (c) 2007-2012 The NetBSD Foundation, Inc.])
AC_DEFINE([PACKAGE_COPYRIGHT],
          ["Copyright (c) 2007-2012 The NetBSD Foundation, Inc."],
          [Define to the copyright string applicable to this package.])
AC_CONFIG_AUX_DIR([admin])
AC_CONFIG_HEADERS([bconfig.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([atf-c.h])
AC_CONFIG_TESTDIR([bootstrap])

AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([1.9 check-news foreign subdir-objects -Wall])

LT_INIT

AC_ARG_ENABLE(tools,
    AS_HELP_STRING([--enable-tools],
        [Enables the build of the deprecated ATF tools]),
    [case $enableval in
     yes|no) enable_tools=${enableval} ;;
     *) AC_MSG_ERROR([Invalid value passed to --enable-tools]) ;;
     esac],
    [enable_tools=no])
AC_SUBST([ENABLE_TOOLS], ${enable_tools})
AM_CONDITIONAL([ENABLE_TOOLS], [test "${enable_tools}" = yes])

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

ATF_MODULE_APPLICATION
ATF_MODULE_DEFS
ATF_MODULE_ENV
ATF_MODULE_FS
ATF_MODULE_SANITY
ATF_MODULE_SIGNALS

AC_CHECK_TYPE([timer_t], [], [], [[#include <time.h>]])

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
dnl Architecture and machine checks.
dnl -----------------------------------------------------------------------

atf_arch=`uname -p`
atf_machine=`uname -m`

AC_MSG_NOTICE([Machine type: ${atf_machine}, architecture: ${atf_arch}])
AC_SUBST(atf_arch, ${atf_arch})
AC_SUBST(atf_machine, ${atf_machine})

dnl -----------------------------------------------------------------------
dnl User-customizable variables.
dnl -----------------------------------------------------------------------

AC_ARG_VAR([ATF_CONFSUBDIR],
           [Subdirectory of sysconfdir under which to look for files])
if test x"${ATF_CONFSUBDIR-unset}" = x"unset"; then
    ATF_CONFSUBDIR=atf
else
    case ${ATF_CONFSUBDIR} in
    /*)
        AC_MSG_ERROR([ATF_CONFSUBDIR must hold a relative path])
        ;;
    *)
        ;;
    esac
fi
if test x"${ATF_CONFSUBDIR}" = x""; then
    AC_SUBST(atf_confdir, \${sysconfdir})
else
    AC_SUBST(atf_confdir, \${sysconfdir}/${ATF_CONFSUBDIR})
fi

AC_ARG_VAR([ATF_WORKDIR],
           [Default location to use for ATF work directories])
if test x"${ATF_WORKDIR}" = x""; then
    for t in /tmp /var/tmp; do
        if test -d ${t}; then
            ATF_WORKDIR=${t}
            break
        fi
    done
    if test x"${ATF_WORKDIR}" = x""; then
        AC_MSG_ERROR([Could not guess a value for ATF_WORKDIR])
    fi
else
    case ${ATF_WORKDIR} in
    /*)
        ;;
    *)
        AC_MSG_ERROR([ATF_WORKDIR must hold an absolute path])
        ;;
    esac
fi

AC_SUBST(atf_aclocaldir, \${datadir}/aclocal)
AC_SUBST(atf_cssdir, \${datadir}/examples/atf)
AC_SUBST(atf_dtddir, \${datadir}/xml/atf)
AC_SUBST(atf_egdir, \${datadir}/examples/atf)
AC_SUBST(atf_pkgconfigdir, \${libdir}/pkgconfig)
AC_SUBST(atf_xsldir, \${datadir}/xsl/atf)

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

AC_PATH_PROG([GDB], [gdb])
AC_PATH_PROG([KYUA], [kyua])
AM_CONDITIONAL([HAVE_KYUA], [test -n "${KYUA}"])
AC_PATH_PROG([GIT], [git])

dnl -----------------------------------------------------------------------
dnl Finally, generate output.
dnl -----------------------------------------------------------------------

AC_OUTPUT([Makefile atf-c/defs.h])

if test ${enable_tools} = yes; then
    AC_MSG_WARN([Building the deprecated ATF tools (atf-run and atf-report);])
    AC_MSG_WARN([please migrate to Kyua as soon as feasible.])
fi

dnl vim: syntax=m4:expandtab:shiftwidth=4:softtabstop=4
