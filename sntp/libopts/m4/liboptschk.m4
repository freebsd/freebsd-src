# liboptschk.m4 serial 2 (autogen - 5.11.4)
dnl Copyright (c) 2005-2011 by Bruce Korb - all rights reserved
dnl
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
dnl
dnl Time-stamp:        "2010-12-07 12:57:56 bkorb"
dnl
AC_DEFUN([ag_FIND_LIBOPTS],
  [if test "X${ac_cv_header_autoopts_options_h}" = Xno
  then :
  else
    f=`autoopts-config cflags` 2>/dev/null
    if test X"${f}" = X
    then
      :
    else
      AC_DEFINE([HAVE_LIBOPTS],[1],[define if we can find libopts])
      CFLAGS="${CFLAGS} ${f}"
      ao_CFLAGS="${f}"
      AC_SUBST(ao_CFLAGS)

      f=`autoopts-config ldflags` 2>/dev/null
      LIBS="${LIBS} ${f}"
      ao_LIBS="${f}"
      AC_SUBST(ao_LIBS)
    fi
  fi])
