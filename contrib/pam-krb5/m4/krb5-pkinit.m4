dnl Additional probes for Kerberos PKINIT support.
dnl
dnl Additional Kerberos library probes that check behavior of the library
dnl relevant to PKINIT support.  Provides the macro:
dnl
dnl     RRA_FUNC_KRB5_GET_INIT_CREDS_OPT_SET_PKINIT_ARGS
dnl
dnl and defines HAVE_KRB5_GET_INIT_CREDS_OPT_SET_PKINIT_9_ARGS if it takes
dnl only nine arguments.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2007, 2018, 2020-2021 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2011
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

dnl Source used by RRA_FUNC_KRB5_GET_INIT_CREDS_OPT_SET_PKINIT_ARGS.
AC_DEFUN([_RRA_FUNC_KRB5_PKINIT_ARGS_SOURCE], [RRA_INCLUDES_KRB5] [[
int
main(void)
{
    krb5_context c;
    krb5_get_init_creds_opt *o;
    krb5_principal p;

    krb5_get_init_creds_opt_set_pkinit(c, o, p, NULL, NULL, 0, NULL, NULL,
                                       NULL);
}
]])

dnl Check whether krb5_get_init_creds_opt_set_pkinit takes eleven arguments
dnl (0.8 release candidates and later) or only nine (0.7).  Defines
dnl HAVE_KRB5_GET_INIT_CREDS_OPT_SET_PKINIT_9_ARGS if it takes nine arguments.
AC_DEFUN([RRA_FUNC_KRB5_GET_INIT_CREDS_OPT_SET_PKINIT_ARGS],
[AC_CACHE_CHECK([if krb5_get_init_creds_opt_set_pkinit takes 9 arguments],
    [rra_cv_func_krb5_get_init_creds_opt_set_pkinit_args],
    [AC_COMPILE_IFELSE([AC_LANG_SOURCE([_RRA_FUNC_KRB5_PKINIT_ARGS_SOURCE])],
        [rra_cv_func_krb5_get_init_creds_opt_set_pkinit_args=yes],
        [rra_cv_func_krb5_get_init_creds_opt_set_pkinit_args=no])])
AS_IF([test $rra_cv_func_krb5_get_init_creds_opt_set_pkinit_args = yes],
    [AC_DEFINE([HAVE_KRB5_GET_INIT_CREDS_OPT_SET_PKINIT_9_ARGS], 1,
        [Define if krb5_get_init_creds_opt_set_pkinit takes 9 arguments.])])])
