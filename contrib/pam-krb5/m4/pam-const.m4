dnl Determine whether PAM uses const in prototypes.
dnl
dnl Linux marks several PAM arguments const, including the argument to
dnl pam_get_item and some arguments to conversation functions, which Solaris
dnl doesn't.  Mac OS X, OS X, and macOS mark the first argument to
dnl pam_strerror const, and other platforms don't.  This test tries to
dnl determine which style is in use to select whether to declare variables
dnl const and how to prototype functions in order to avoid compiler warnings.
dnl
dnl Since this is just for compiler warnings, it's not horribly important if
dnl we guess wrong.  This test is ugly, but it seems to work.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Markus Moeller
dnl Copyright 2007, 2015 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2007-2008 Markus Moeller
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

dnl Source used by RRA_HEADER_PAM_CONST.
AC_DEFUN([_RRA_HEADER_PAM_CONST_SOURCE],
[#ifdef HAVE_SECURITY_PAM_APPL_H
# include <security/pam_appl.h>
#else
# include <pam/pam_appl.h>
#endif
])

AC_DEFUN([RRA_HEADER_PAM_CONST],
[AC_CACHE_CHECK([whether PAM prefers const], [rra_cv_header_pam_const],
    [AC_EGREP_CPP([const void \*\* *_?item], _RRA_HEADER_PAM_CONST_SOURCE(),
        [rra_cv_header_pam_const=yes], [rra_cv_header_pam_const=no])])
 AS_IF([test x"$rra_cv_header_pam_const" = xyes],
    [rra_header_pam_const=const], [rra_header_pam_const=])
 AC_DEFINE_UNQUOTED([PAM_CONST], [$rra_header_pam_const],
    [Define to const if PAM uses const in pam_get_item, empty otherwise.])])

AC_DEFUN([RRA_HEADER_PAM_STRERROR_CONST],
[AC_CACHE_CHECK([whether pam_strerror uses const],
    [rra_cv_header_pam_strerror_const],
    [AC_EGREP_CPP([pam_strerror *\(const], _RRA_HEADER_PAM_CONST_SOURCE(),
        [rra_cv_header_pam_strerror_const=yes],
        [rra_cv_header_pam_strerror_const=no])])
 AS_IF([test x"$rra_cv_header_pam_strerror_const" = xyes],
    [rra_header_pam_strerror_const=const], [rra_header_pam_strerror_const=])
 AC_DEFINE_UNQUOTED([PAM_STRERROR_CONST], [$rra_header_pam_strerror_const],
    [Define to const if PAM uses const in pam_strerror, empty otherwise.])])
