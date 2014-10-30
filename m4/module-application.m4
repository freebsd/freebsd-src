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

AC_DEFUN([ATF_MODULE_APPLICATION], [
    ATF_CHECK_STD_VSNPRINTF

    AC_CACHE_CHECK(
      [whether getopt allows a + sign for POSIX behavior],
      [kyua_cv_getopt_plus], [
      AC_LANG_PUSH([C])
      AC_RUN_IFELSE([AC_LANG_PROGRAM([#include <stdlib.h>
#include <string.h>
#include <unistd.h>], [
    int argc = 4;
    char* argv@<:@5@:>@ = {
        strdup("conftest"),
        strdup("-+"),
        strdup("-a"),
        strdup("bar"),
        NULL
    };
    int ch;
    int seen_a = 0, seen_plus = 0;

    while ((ch = getopt(argc, argv, "+a:")) != -1) {
        switch (ch) {
        case 'a':
            seen_a = 1;
            break;

        case '+':
            seen_plus = 1;
            break;

        case '?':
        default:
            ;
        }
    }

    return (seen_a && !seen_plus) ? EXIT_SUCCESS : EXIT_FAILURE;
])],
      [kyua_cv_getopt_plus=yes],
      [kyua_cv_getopt_plus=no])
      AC_LANG_POP([C])
    ])
    if test x"${kyua_cv_getopt_plus}" = xyes; then
        AC_DEFINE([HAVE_GNU_GETOPT], [1],
                  [Define to 1 if getopt allows a + sign for POSIX behavior])
    fi

    AC_CACHE_CHECK(
      [whether getopt has optreset],
      [kyua_cv_getopt_optreset], [
      AC_LANG_PUSH([C])
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([#include <stdlib.h>
#include <unistd.h>], [
    optreset = 1;
    return EXIT_SUCCESS;
])],
      [kyua_cv_getopt_optreset=yes],
      [kyua_cv_getopt_optreset=no])
      AC_LANG_POP([C])
    ])
    if test x"${kyua_cv_getopt_optreset}" = xyes; then
        AC_DEFINE([HAVE_OPTRESET], [1], [Define to 1 if getopt has optreset])
    fi
])
