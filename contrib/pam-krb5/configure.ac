dnl Autoconf configuration for pam-krb5.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2005-2009, 2014, 2017, 2020-2021 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2009-2013
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl Copyright 2005 Andres Salomon <dilinger@debian.org>
dnl Copyright 1999-2000 Frank Cusack <fcusack@fcusack.com>
dnl
dnl SPDX-License-Identifier: BSD-3-clause or GPL-1+

AC_PREREQ([2.64])
AC_INIT([pam-krb5], [4.11], [eagle@eyrie.org])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_LIBOBJ_DIR([portable])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.11 check-news dist-xz foreign silent-rules subdir-objects
    -Wall -Werror])
AM_MAINTAINER_MODE

dnl Detect unexpanded macros.
m4_pattern_forbid([^PKG_])
m4_pattern_forbid([^_?RRA_])

AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
RRA_PROG_CC_WARNINGS_FLAGS
AC_SYS_LARGEFILE
AM_PROG_CC_C_O
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_INSTALL
LT_INIT([disable-static])
AC_CANONICAL_HOST
RRA_LD_VERSION_SCRIPT

dnl Only used for the test suite.
AC_ARG_VAR([PATH_OPENSSL], [Path to openssl for the test suite])
AC_PATH_PROG([PATH_OPENSSL], [openssl])
AS_IF([test x"$PATH_OPENSSL" != x],
    [AC_DEFINE_UNQUOTED([PATH_OPENSSL], ["$PATH_OPENSSL"],
        [Define to the full path to openssl for some tests.])])
AC_ARG_VAR([PATH_VALGRIND], [Path to valgrind for the test suite])
AC_PATH_PROG([PATH_VALGRIND], [valgrind])

dnl Probe for the functionality of the PAM libraries and their include file
dnl naming.  Mac OS X puts them in pam/* instead of security/*.
AC_SEARCH_LIBS([pam_set_data], [pam])
AC_CHECK_FUNCS([pam_getenv pam_getenvlist pam_modutil_getpwnam])
AC_REPLACE_FUNCS([pam_syslog pam_vsyslog])
AC_CHECK_HEADERS([security/pam_modutil.h], [],
    [AC_CHECK_HEADERS([pam/pam_modutil.h])])
AC_CHECK_HEADERS([security/pam_appl.h], [],
    [AC_CHECK_HEADERS([pam/pam_appl.h], [],
        [AC_MSG_ERROR([No PAM header files found])])])
AC_CHECK_HEADERS([security/pam_ext.h], [],
    [AC_CHECK_HEADERS([pam/pam_ext.h])])
RRA_HEADER_PAM_CONST
RRA_HEADER_PAM_STRERROR_CONST
AC_DEFINE([MODULE_NAME], ["pam_krb5"],
    [The name of the PAM module, used by the pam_vsyslog replacement.])

dnl Probe for the location and functionality of the Kerberos libraries.
RRA_LIB_KRB5
RRA_LIB_KRB5_SWITCH
AC_CHECK_HEADERS([hx509_err.h])
AC_CHECK_MEMBER([krb5_creds.session],
    [AC_DEFINE([HAVE_KRB5_HEIMDAL], [1],
        [Define if your Kerberos implementation is Heimdal.])],
    [AC_DEFINE([HAVE_KRB5_MIT], [1],
        [Define if your Kerberos implementation is MIT.])],
    [RRA_INCLUDES_KRB5])
AC_CHECK_TYPES([krb5_realm], [], [], [RRA_INCLUDES_KRB5])
AC_CHECK_FUNCS([krb5_cc_get_full_name \
    krb5_data_free \
    krb5_free_default_realm \
    krb5_free_string \
    krb5_get_init_creds_opt_alloc \
    krb5_get_init_creds_opt_set_anonymous \
    krb5_get_init_creds_opt_set_change_password_prompt \
    krb5_get_init_creds_opt_set_default_flags \
    krb5_get_init_creds_opt_set_fast_ccache_name \
    krb5_get_init_creds_opt_set_out_ccache \
    krb5_get_init_creds_opt_set_pa \
    krb5_get_prompt_types \
    krb5_init_secure_context \
    krb5_principal_get_realm \
    krb5_principal_set_comp_string \
    krb5_set_password \
    krb5_set_trace_filename \
    krb5_verify_init_creds_opt_init \
    krb5_xfree])
AC_CHECK_FUNCS([krb5_get_init_creds_opt_set_pkinit],
    [RRA_FUNC_KRB5_GET_INIT_CREDS_OPT_SET_PKINIT_ARGS])
AC_CHECK_FUNCS([krb5_get_init_creds_opt_free],
    [RRA_FUNC_KRB5_GET_INIT_CREDS_OPT_FREE_ARGS])
AC_CHECK_DECLS([krb5_kt_free_entry], [], [], [RRA_INCLUDES_KRB5])
AC_CHECK_FUNCS([krb5_appdefault_string], [],
    [AC_CHECK_FUNCS([krb5_get_profile])
     AC_CHECK_HEADERS([k5profile.h profile.h])
     AC_LIBOBJ([krb5-profile])])
AC_LIBOBJ([krb5-extra])
RRA_LIB_KRB5_RESTORE

dnl The kadmin client libraries are only used for the test suite.
RRA_LIB_KADM5CLNT_OPTIONAL
RRA_LIB_KADM5CLNT_SWITCH
AC_CHECK_HEADERS([kadm5/kadm5_err.h])
AC_CHECK_FUNCS([kadm5_init_krb5_context kadm5_init_with_skey_ctx])
RRA_LIB_KADM5CLNT_RESTORE

dnl Regex support is only used for the test suite.
AC_CHECK_HEADER([regex.h], [AC_CHECK_FUNCS([regcomp])])

dnl Other probes of the system libraries.
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([strings.h sys/bittypes.h sys/select.h sys/time.h])
AC_CHECK_DECLS([reallocarray])
AC_TYPE_LONG_LONG_INT
AC_CHECK_TYPES([ssize_t], [], [],
    [#include <sys/types.h>])
AC_CHECK_FUNCS([explicit_bzero])
AC_REPLACE_FUNCS([asprintf issetugid mkstemp reallocarray strndup])

dnl Try to specify the binding so that any references within the PAM module
dnl are resolved to the functions in that module in preference to any external
dnl function.
dnl
dnl More platforms could be handled here.  Contributions welcome.
AS_CASE([$host],
    [*-hpux*],
    [AS_IF([test x"$GCC" = x"yes"],
        [AM_LDFLAGS="-Wl,-Bsymbolic $AM_LDFLAGS"],
        [AM_LDFLAGS="-Wl,+vshlibunsats $AM_LDFLAGS"])],

    [*-linux*],
    [AM_LDFLAGS="-Wl,-z,defs -Wl,-Bsymbolic $AM_LDFLAGS"],

    [*-solaris2*],
    [AS_IF([test x"$GCC" = x"yes"],
        [AM_LDFLAGS="-Wl,-Bsymbolic $AM_LDFLAGS"],
        [AM_LDFLAGS="-Wl,-xldscope=symbolic $AM_LDFLAGS"])])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
