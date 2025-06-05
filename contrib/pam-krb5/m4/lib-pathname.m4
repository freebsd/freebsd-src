dnl Determine the library path name.
dnl
dnl Red Hat systems and some other Linux systems use lib64 and lib32 rather
dnl than just lib in some circumstances.  This file provides an Autoconf
dnl macro, RRA_SET_LDFLAGS, which given a variable, a prefix, and an optional
dnl suffix, adds -Lprefix/lib, -Lprefix/lib32, or -Lprefix/lib64 to the
dnl variable depending on which directories exist and the size of a long in
dnl the compilation environment.  If a suffix is given, a slash and that
dnl suffix will be appended, to allow for adding a subdirectory of the library
dnl directory.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2021 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2008-2009
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

dnl Probe for the alternate library name that we should attempt on this
dnl architecture, given the size of an int, and set rra_lib_arch_name to that
dnl name.  Separated out so that it can be AC_REQUIRE'd and not run multiple
dnl times.
dnl
dnl There is an unfortunate abstraction violation here where we assume we know
dnl the cache variable name used by Autoconf.  Unfortunately, Autoconf doesn't
dnl provide any other way of getting at that information in shell that I can
dnl see.
AC_DEFUN([_RRA_LIB_ARCH_NAME],
[rra_lib_arch_name=lib
 AC_CHECK_SIZEOF([long])
 AS_IF([test "$ac_cv_sizeof_long" -eq 4],
     [rra_lib_arch_name=lib32],
     [AS_IF([test "$ac_cv_sizeof_long" -eq 8],
         [rra_lib_arch_name=lib64])])])

dnl Set VARIABLE to -LPREFIX/lib{,32,64} or -LPREFIX/lib{,32,64}/SUFFIX as
dnl appropriate.
AC_DEFUN([RRA_SET_LDFLAGS],
[AC_REQUIRE([_RRA_LIB_ARCH_NAME])
 AS_IF([test -d "$2/$rra_lib_arch_name"],
    [AS_IF([test x"$3" = x],
        [$1="[$]$1 -L$2/${rra_lib_arch_name}"],
        [$1="[$]$1 -L$2/${rra_lib_arch_name}/$3"])],
    [AS_IF([test x"$3" = x],
        [$1="[$]$1 -L$2/lib"],
        [$1="[$]$1 -L$2/lib/$3"])])
 $1=`AS_ECHO(["[$]$1"]) | sed -e 's/^ *//'`])
