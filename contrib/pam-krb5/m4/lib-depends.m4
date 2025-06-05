dnl Provides option to change library probes.
dnl
dnl This file provides RRA_ENABLE_REDUCED_DEPENDS, which adds the configure
dnl option --enable-reduced-depends to request that library probes assume
dnl shared libraries are in use and dependencies of libraries should not be
dnl probed.  If this option is given, the shell variable rra_reduced_depends
dnl is set to true; otherwise, it is set to false.
dnl
dnl This macro doesn't do much but is defined separately so that other macros
dnl can require it with AC_REQUIRE.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2005-2007
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

AC_DEFUN([RRA_ENABLE_REDUCED_DEPENDS],
[rra_reduced_depends=false
AC_ARG_ENABLE([reduced-depends],
    [AS_HELP_STRING([--enable-reduced-depends],
        [Try to minimize shared library dependencies])],
    [AS_IF([test x"$enableval" = xyes], [rra_reduced_depends=true])])])
