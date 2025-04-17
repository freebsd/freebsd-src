/*
 * Portability wrapper around krb5.h.
 *
 * This header includes krb5.h and then adjusts for various portability
 * issues, primarily between MIT Kerberos and Heimdal, so that code can be
 * written to a consistent API.
 *
 * Unfortunately, due to the nature of the differences between MIT Kerberos
 * and Heimdal, it's not possible to write code to either one of the APIs and
 * adjust for the other one.  In general, this header tries to make available
 * the Heimdal API and fix it for MIT Kerberos, but there are places where MIT
 * Kerberos requires a more specific call.  For those cases, it provides the
 * most specific interface.
 *
 * For example, MIT Kerberos has krb5_free_unparsed_name() whereas Heimdal
 * prefers the generic krb5_xfree().  In this case, this header provides
 * krb5_free_unparsed_name() for both APIs since it's the most specific call.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2015, 2017, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2010-2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#ifndef PORTABLE_KRB5_H
#define PORTABLE_KRB5_H 1

/*
 * Allow inclusion of config.h to be skipped, since sometimes we have to use a
 * stripped-down version of config.h with a different name.
 */
#ifndef CONFIG_H_INCLUDED
#    include <config.h>
#endif
#include <portable/macros.h>

#if defined(HAVE_KRB5_H)
#    include <krb5.h>
#elif defined(HAVE_KERBEROSV5_KRB5_H)
#    include <kerberosv5/krb5.h>
#else
#    include <krb5/krb5.h>
#endif
#include <stdlib.h>

/* Heimdal: KRB5_WELLKNOWN_NAME, MIT: KRB5_WELLKNOWN_NAMESTR. */
#ifndef KRB5_WELLKNOWN_NAME
#    ifdef KRB5_WELLKNOWN_NAMESTR
#        define KRB5_WELLKNOWN_NAME KRB5_WELLKNOWN_NAMESTR
#    else
#        define KRB5_WELLKNOWN_NAME "WELLKNOWN"
#    endif
#endif

/* Heimdal: KRB5_ANON_NAME, MIT: KRB5_ANONYMOUS_PRINCSTR. */
#ifndef KRB5_ANON_NAME
#    ifdef KRB5_ANONYMOUS_PRINCSTR
#        define KRB5_ANON_NAME KRB5_ANONYMOUS_PRINCSTR
#    else
#        define KRB5_ANON_NAME "ANONYMOUS"
#    endif
#endif

/* Heimdal: KRB5_ANON_REALM, MIT: KRB5_ANONYMOUS_REALMSTR. */
#ifndef KRB5_ANON_REALM
#    ifdef KRB5_ANONYMOUS_REALMSTR
#        define KRB5_ANON_REALM KRB5_ANONYMOUS_REALMSTR
#    else
#        define KRB5_ANON_REALM "WELLKNOWN:ANONYMOUS"
#    endif
#endif

BEGIN_DECLS

/* Default to a hidden visibility for all portability functions. */
#pragma GCC visibility push(hidden)

/*
 * AIX included Kerberos includes the profile library but not the
 * krb5_appdefault functions, so we provide replacements that we have to
 * prototype.
 */
#ifndef HAVE_KRB5_APPDEFAULT_STRING
void krb5_appdefault_boolean(krb5_context, const char *, const krb5_data *,
                             const char *, int, int *);
void krb5_appdefault_string(krb5_context, const char *, const krb5_data *,
                            const char *, const char *, char **);
#endif

/*
 * Now present in both Heimdal and MIT, but very new in MIT and not present in
 * older Heimdal.
 */
#ifndef HAVE_KRB5_CC_GET_FULL_NAME
krb5_error_code krb5_cc_get_full_name(krb5_context, krb5_ccache, char **);
#endif

/* Heimdal: krb5_data_free, MIT: krb5_free_data_contents. */
#ifdef HAVE_KRB5_DATA_FREE
#    define krb5_free_data_contents(c, d) krb5_data_free(d)
#endif

/*
 * MIT-specific.  The Heimdal documentation says to use free(), but that
 * doesn't actually make sense since the memory is allocated inside the
 * Kerberos library.  Use krb5_xfree instead.
 */
#ifndef HAVE_KRB5_FREE_DEFAULT_REALM
#    define krb5_free_default_realm(c, r) krb5_xfree(r)
#endif

/*
 * Heimdal: krb5_xfree, MIT: krb5_free_string, older MIT uses free().  Note
 * that we can incorrectly allocate in the library and call free() if
 * krb5_free_string is not available but something we use that API for is
 * available, such as krb5_appdefaults_*, but there isn't anything we can
 * really do about it.
 */
#ifndef HAVE_KRB5_FREE_STRING
#    ifdef HAVE_KRB5_XFREE
#        define krb5_free_string(c, s) krb5_xfree(s)
#    else
#        define krb5_free_string(c, s) free(s)
#    endif
#endif

/* Heimdal: krb5_xfree, MIT: krb5_free_unparsed_name. */
#ifdef HAVE_KRB5_XFREE
#    define krb5_free_unparsed_name(c, p) krb5_xfree(p)
#endif

/*
 * krb5_{get,free}_error_message are the preferred APIs for both current MIT
 * and current Heimdal, but there are tons of older APIs we may have to fall
 * back on for earlier versions.
 *
 * This function should be called immediately after the corresponding error
 * without any intervening Kerberos calls.  Otherwise, the correct error
 * message and supporting information may not be returned.
 */
#ifndef HAVE_KRB5_GET_ERROR_MESSAGE
const char *krb5_get_error_message(krb5_context, krb5_error_code);
#endif
#ifndef HAVE_KRB5_FREE_ERROR_MESSAGE
void krb5_free_error_message(krb5_context, const char *);
#endif

/*
 * Both current MIT and current Heimdal prefer _opt_alloc and _opt_free, but
 * older versions of both require allocating your own struct and calling
 * _opt_init.
 */
#ifndef HAVE_KRB5_GET_INIT_CREDS_OPT_ALLOC
krb5_error_code krb5_get_init_creds_opt_alloc(krb5_context,
                                              krb5_get_init_creds_opt **);
#endif
#ifdef HAVE_KRB5_GET_INIT_CREDS_OPT_FREE
#    ifndef HAVE_KRB5_GET_INIT_CREDS_OPT_FREE_2_ARGS
#        define krb5_get_init_creds_opt_free(c, o) \
            krb5_get_init_creds_opt_free(o)
#    endif
#else
#    define krb5_get_init_creds_opt_free(c, o) free(o)
#endif

/* Not available in versions of Heimdal prior to 7.0.1. */
#ifndef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_CHANGE_PASSWORD_PROMPT
#    define krb5_get_init_creds_opt_set_change_password_prompt(o, f) /* */
#endif

/* Heimdal-specific. */
#ifndef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_DEFAULT_FLAGS
#    define krb5_get_init_creds_opt_set_default_flags(c, p, r, o) /* empty */
#endif

/*
 * Old versions of Heimdal (0.7 and earlier) take only nine arguments to the
 * krb5_get_init_creds_opt_set_pkinit instead of the 11 arguments that current
 * versions take.  Adjust if needed.  This function is Heimdal-specific.
 */
#ifdef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_PKINIT
#    ifdef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_PKINIT_9_ARGS
#        define krb5_get_init_creds_opt_set_pkinit(c, o, p, u, a, l, r, f, m, \
                                                   d, s)                      \
            krb5_get_init_creds_opt_set_pkinit((c), (o), (p), (u), (a), (f),  \
                                               (m), (d), (s));
#    endif
#endif

/*
 * MIT-specific.  Heimdal automatically ignores environment variables if
 * called in a setuid context.
 */
#ifndef HAVE_KRB5_INIT_SECURE_CONTEXT
#    define krb5_init_secure_context(c) krb5_init_context(c)
#endif

/*
 * Heimdal: krb5_kt_free_entry, MIT: krb5_free_keytab_entry_contents.  We
 * check for the declaration rather than the function since the function is
 * present in older MIT Kerberos libraries but not prototyped.
 */
#if !HAVE_DECL_KRB5_KT_FREE_ENTRY
#    define krb5_kt_free_entry(c, e) krb5_free_keytab_entry_contents((c), (e))
#endif

/*
 * Heimdal provides a nice function that just returns a const char *.  On MIT,
 * there's an accessor macro that returns the krb5_data pointer, which
 * requires more work to get at the underlying char *.
 */
#ifndef HAVE_KRB5_PRINCIPAL_GET_REALM
const char *krb5_principal_get_realm(krb5_context, krb5_const_principal);
#endif

/*
 * krb5_change_password is deprecated in favor of krb5_set_password in current
 * Heimdal.  Current MIT provides both.
 */
#ifndef HAVE_KRB5_SET_PASSWORD
#    define krb5_set_password(c, cr, pw, p, rc, rcs, rs) \
        krb5_change_password((c), (cr), (pw), (rc), (rcs), (rs))
#endif

/*
 * AIX's NAS Kerberos implementation mysteriously provides the struct and the
 * krb5_verify_init_creds function but not this function.
 */
#ifndef HAVE_KRB5_VERIFY_INIT_CREDS_OPT_INIT
void krb5_verify_init_creds_opt_init(krb5_verify_init_creds_opt *opt);
#endif

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* !PORTABLE_KRB5_H */
