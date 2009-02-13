# liboptschk.m4 serial 1 (autogen - 5.7.3)
dnl Copyright (C) 2005 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl Time-stamp:        "2006-09-23 19:42:31 bkorb"
dnl Last Committed:    $Date: 2006/09/24 02:59:00 $

dnl This file can can be used in projects which are not available under
dnl the GNU General Public License or the GNU Library General Public
dnl License but which still want to provide support for the GNU gettext
dnl functionality.
dnl Please note that the actual code of the GNU gettext library is covered
dnl by the GNU Library General Public License, and the rest of the GNU
dnl gettext package package is covered by the GNU General Public License.
dnl They are *not* in the public domain.

dnl Authors:
dnl   Ulrich Drepper <drepper@cygnus.com>, 1995-2000.
dnl   Bruno Haible <haible@clisp.cons.org>, 2000-2003.

AC_PREREQ(2.50)

AC_DEFUN([ag_FIND_LIBOPTS],
    [if test "X${ac_cv_header_autoopts_options_h}" == Xno
    then
      :
    else
      f=`autoopts-config cflags` 2>/dev/null
      test X"${f}" = X && f=`libopts-config cflags` 2>/dev/null
      if test X"${f}" = X
      then
        :
      else
        AC_DEFINE([HAVE_LIBOPTS],[1],[define if we can find libopts])
        CFLAGS="${CFLAGS} ${f}"
        f=`autoopts-config ldflags` 2>/dev/null
        test X"${f}" = X && f=`libopts-config ldflags` 2>/dev/null
        LIBS="${LIBS} ${f}"
      fi
    fi])
