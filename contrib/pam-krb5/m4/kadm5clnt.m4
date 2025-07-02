dnl Find the compiler and linker flags for the kadmin client library.
dnl
dnl Finds the compiler and linker flags for linking with the kadmin client
dnl library.  Provides the --with-kadm5clnt, --with-kadm5clnt-include, and
dnl --with-kadm5clnt-lib configure option to specify a non-standard path to
dnl the library.  Uses krb5-config where available unless reduced dependencies
dnl is requested or --with-kadm5clnt-include or --with-kadm5clnt-lib are
dnl given.
dnl
dnl Provides the macros RRA_LIB_KADM5CLNT and RRA_LIB_KADM5CLNT_OPTIONAL and
dnl sets the substitution variables KADM5CLNT_CPPFLAGS, KADM5CLNT_LDFLAGS, and
dnl KADM5CLNT_LIBS.  Also provides RRA_LIB_KADM5CLNT_SWITCH to set CPPFLAGS,
dnl LDFLAGS, and LIBS to include the kadmin client libraries, saving the
dnl ecurrent values, and RRA_LIB_KADM5CLNT_RESTORE to restore those settings
dnl to before the last RRA_LIB_KADM5CLNT_SWITCH.  Defines HAVE_KADM5CLNT and
dnl sets rra_use_KADM5CLNT to true if the library is found.
dnl
dnl Depends on the RRA_LIB helper routines.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2005-2009, 2011, 2013
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

dnl Save the current CPPFLAGS, LDFLAGS, and LIBS settings and switch to
dnl versions that include the kadmin client flags.  Used as a wrapper, with
dnl RRA_LIB_KADM5CLNT_RESTORE, around tests.
AC_DEFUN([RRA_LIB_KADM5CLNT_SWITCH], [RRA_LIB_HELPER_SWITCH([KADM5CLNT])])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values (before
dnl RRA_LIB_KADM5CLNT_SWITCH was called).
AC_DEFUN([RRA_LIB_KADM5CLNT_RESTORE], [RRA_LIB_HELPER_RESTORE([KADM5CLNT])])

dnl Set KADM5CLNT_CPPFLAGS and KADM5CLNT_LDFLAGS based on rra_KADM5CLNT_root,
dnl rra_KADM5CLNT_libdir, and rra_KADM5CLNT_includedir.
AC_DEFUN([_RRA_LIB_KADM5CLNT_PATHS], [RRA_LIB_HELPER_PATHS([KADM5CLNT])])

dnl Does the appropriate library checks for reduced-dependency kadmin client
dnl linkage.  The single argument, if "true", says to fail if the kadmin
dnl client library could not be found.
AC_DEFUN([_RRA_LIB_KADM5CLNT_REDUCED],
[RRA_LIB_KADM5CLNT_SWITCH
 AC_CHECK_LIB([kadm5clnt], [kadm5_init_with_password],
    [KADM5CLNT_LIBS=-lkadm5clnt],
    [AS_IF([test x"$1" = xtrue],
        [AC_MSG_ERROR([cannot find usable kadmin client library])])])
 RRA_LIB_KADM5CLNT_RESTORE])

dnl Sanity-check the results of krb5-config and be sure we can really link a
dnl GSS-API program.  If not, fall back on the manual check.
AC_DEFUN([_RRA_LIB_KADM5CLNT_CHECK],
[RRA_LIB_HELPER_CHECK([$1], [KADM5CLNT], [kadm5_init_with_password],
    [kadmin client])])

dnl Determine GSS-API compiler and linker flags from krb5-config.
AC_DEFUN([_RRA_LIB_KADM5CLNT_CONFIG],
[RRA_KRB5_CONFIG([${rra_KADM5CLNT_root}], [kadm-client], [KADM5CLNT],
    [_RRA_LIB_KADM5CLNT_CHECK([$1])],
    [_RRA_LIB_KADM5CLNT_PATHS
     _RRA_LIB_KADM5CLNT_REDUCED([$1])])])

dnl The core of the library checking, shared between RRA_LIB_KADM5CLNT and
dnl RRA_LIB_KADM5CLNT_OPTIONAL.  The single argument, if "true", says to fail
dnl if the kadmin client library could not be found.
AC_DEFUN([_RRA_LIB_KADM5CLNT_INTERNAL],
[AC_REQUIRE([RRA_ENABLE_REDUCED_DEPENDS])
 AS_IF([test x"$rra_reduced_depends" = xtrue],
    [_RRA_LIB_KADM5CLNT_PATHS
     _RRA_LIB_KADM5CLNT_REDUCED([$1])],
    [AS_IF([test x"$rra_KADM5CLNT_includedir" = x \
            && test x"$rra_KADM5CLNT_libdir" = x],
        [_RRA_LIB_KADM5CLNT_CONFIG([$1])],
        [_RRA_LIB_KADM5CLNT_PATHS
         _RRA_LIB_KADM5CLNT_REDUCED([$1])])])])

dnl The main macro for packages with mandatory kadmin client support.
AC_DEFUN([RRA_LIB_KADM5CLNT],
[RRA_LIB_HELPER_VAR_INIT([KADM5CLNT])
 RRA_LIB_HELPER_WITH([kadm-client], [kadmin client], [KADM5CLNT])
 _RRA_LIB_KADM5CLNT_INTERNAL([true])
 rra_use_KADM5CLNT=true
 AC_DEFINE([HAVE_KADM5CLNT], 1, [Define to enable kadmin client features.])])

dnl The main macro for packages with optional kadmin client support.
AC_DEFUN([RRA_LIB_KADM5CLNT_OPTIONAL],
[RRA_LIB_HELPER_VAR_INIT([KADM5CLNT])
 RRA_LIB_HELPER_WITH_OPTIONAL([kadm-client], [kadmin client], [KADM5CLNT])
 AS_IF([test x"$rra_use_KADM5CLNT" != xfalse],
    [AS_IF([test x"$rra_use_KADM5CLNT" = xtrue],
        [_RRA_LIB_KADM5CLNT_INTERNAL([true])],
        [_RRA_LIB_KADM5CLNT_INTERNAL([false])])])
 AS_IF([test x"$KADM5CLNT_LIBS" != x],
    [rra_use_KADM5CLNT=true
     AC_DEFINE([HAVE_KADM5CLNT], 1,
        [Define to enable kadmin client features.])])])
