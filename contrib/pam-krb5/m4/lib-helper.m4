dnl Helper functions to manage compiler variables.
dnl
dnl These are a wide variety of helper macros to make it easier to construct
dnl standard macros to probe for a library and to set library-specific
dnl CPPFLAGS, LDFLAGS, and LIBS shell substitution variables.  Most of them
dnl take as one of the arguments the prefix string to use for variables, which
dnl is usually something like "KRB5" or "GSSAPI".
dnl
dnl Depends on RRA_SET_LDFLAGS.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2018 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2011, 2013
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

dnl Add the library flags to the default compiler flags and then remove them.
dnl
dnl To use these macros, pass the prefix string used for the variables as the
dnl only argument.  For example, to use these for a library with KRB5 as a
dnl prefix, one would use:
dnl
dnl     AC_DEFUN([RRA_LIB_KRB5_SWITCH], [RRA_LIB_HELPER_SWITCH([KRB5])])
dnl     AC_DEFUN([RRA_LIB_KRB5_RESTORE], [RRA_LIB_HELPER_RESTORE([KRB5])])
dnl
dnl Then, wrap checks for library features with RRA_LIB_KRB5_SWITCH and
dnl RRA_LIB_KRB5_RESTORE.
AC_DEFUN([RRA_LIB_HELPER_SWITCH],
[rra_$1[]_save_CPPFLAGS="$CPPFLAGS"
 rra_$1[]_save_LDFLAGS="$LDFLAGS"
 rra_$1[]_save_LIBS="$LIBS"
 CPPFLAGS="$$1[]_CPPFLAGS $CPPFLAGS"
 LDFLAGS="$$1[]_LDFLAGS $LDFLAGS"
 LIBS="$$1[]_LIBS $LIBS"])

AC_DEFUN([RRA_LIB_HELPER_RESTORE],
[CPPFLAGS="$rra_$1[]_save_CPPFLAGS"
 LDFLAGS="$rra_$1[]_save_LDFLAGS"
 LIBS="$rra_$1[]_save_LIBS"])

dnl Given _root, _libdir, and _includedir variables set for a library (set by
dnl RRA_LIB_HELPER_WITH*), set the LDFLAGS and CPPFLAGS variables for that
dnl library accordingly.  Takes the variable prefix as the only argument.
AC_DEFUN([RRA_LIB_HELPER_PATHS],
[AS_IF([test x"$rra_$1[]_libdir" != x],
    [$1[]_LDFLAGS="-L$rra_$1[]_libdir"],
    [AS_IF([test x"$rra_$1[]_root" != x],
        [RRA_SET_LDFLAGS([$1][_LDFLAGS], [${rra_$1[]_root}])])])
 AS_IF([test x"$rra_$1[]_includedir" != x],
    [$1[]_CPPFLAGS="-I$rra_$1[]_includedir"],
    [AS_IF([test x"$rra_$1[]_root" != x],
        [AS_IF([test x"$rra_$1[]_root" != x/usr],
            [$1[]_CPPFLAGS="-I${rra_$1[]_root}/include"])])])])

dnl Check whether a library works.  This is used as a sanity check on the
dnl results of *-config shell scripts.  Takes four arguments; the first, if
dnl "true", says that a working library is mandatory and errors out if it
dnl doesn't.  The second is the variable prefix.  The third is a function to
dnl look for that should be in the libraries.  The fourth is the
dnl human-readable name of the library for error messages.
AC_DEFUN([RRA_LIB_HELPER_CHECK],
[RRA_LIB_HELPER_SWITCH([$2])
 AC_CHECK_FUNC([$3], [],
    [AS_IF([test x"$1" = xtrue],
        [AC_MSG_FAILURE([unable to link with $4 library])])
     $2[]_CPPFLAGS=
     $2[]_LDFLAGS=
     $2[]_LIBS=])
 RRA_LIB_HELPER_RESTORE([$2])])

dnl Initialize the variables used by a library probe and set the appropriate
dnl ones as substitution variables.  Takes the library variable prefix as its
dnl only argument.
AC_DEFUN([RRA_LIB_HELPER_VAR_INIT],
[rra_$1[]_root=
 rra_$1[]_libdir=
 rra_$1[]_includedir=
 rra_use_$1=
 $1[]_CPPFLAGS=
 $1[]_LDFLAGS=
 $1[]_LIBS=
 AC_SUBST([$1][_CPPFLAGS])
 AC_SUBST([$1][_LDFLAGS])
 AC_SUBST([$1][_LIBS])])

dnl Unset all of the variables used by a library probe.  Used with the
dnl _OPTIONAL versions of header probes when a header or library wasn't found
dnl and therefore the library isn't usable.
AC_DEFUN([RRA_LIB_HELPER_VAR_CLEAR],
[$1[]_CPPFLAGS=
 $1[]_LDFLAGS=
 $1[]_LIBS=])

dnl Handles --with options for a non-optional library.  First argument is the
dnl base for the switch names.  Second argument is the short description.
dnl Third argument is the variable prefix.  The variables set are used by
dnl RRA_LIB_HELPER_PATHS.
AC_DEFUN([RRA_LIB_HELPER_WITH],
[AC_ARG_WITH([$1],
    [AS_HELP_STRING([--with-][$1][=DIR],
        [Location of $2 headers and libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_$3[]_root="$withval"])])
 AC_ARG_WITH([$1][-include],
    [AS_HELP_STRING([--with-][$1][-include=DIR],
        [Location of $2 headers])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_$3[]_includedir="$withval"])])
 AC_ARG_WITH([$1][-lib],
    [AS_HELP_STRING([--with-][$1][-lib=DIR],
        [Location of $2 libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_$3[]_libdir="$withval"])])])

dnl Handles --with options for an optional library, so --with-<library> can
dnl cause the checks to be skipped entirely or become mandatory.  Sets an
dnl rra_use_PREFIX variable to true or false if the library is explicitly
dnl enabled or disabled.
dnl
dnl First argument is the base for the switch names.  Second argument is the
dnl short description.  Third argument is the variable prefix.
dnl
dnl The variables set are used by RRA_LIB_HELPER_PATHS.
AC_DEFUN([RRA_LIB_HELPER_WITH_OPTIONAL],
[AC_ARG_WITH([$1],
    [AS_HELP_STRING([--with-][$1][@<:@=DIR@:>@],
        [Location of $2 headers and libraries])],
    [AS_IF([test x"$withval" = xno],
        [rra_use_$3=false],
        [AS_IF([test x"$withval" != xyes], [rra_$3[]_root="$withval"])
         rra_use_$3=true])])
 AC_ARG_WITH([$1][-include],
    [AS_HELP_STRING([--with-][$1][-include=DIR],
        [Location of $2 headers])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_$3[]_includedir="$withval"])])
 AC_ARG_WITH([$1][-lib],
    [AS_HELP_STRING([--with-][$1][-lib=DIR],
        [Location of $2 libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_$3[]_libdir="$withval"])])])
