/*
 * Utility functions for tests that use Kerberos.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2017, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2006-2007, 2009, 2011-2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TAP_KERBEROS_H
#define TAP_KERBEROS_H 1

#include <config.h>
#include <tests/tap/macros.h>

#ifdef HAVE_KRB5
#    include <portable/krb5.h>
#endif

/* Holds the information parsed from the Kerberos test configuration. */
struct kerberos_config {
    char *keytab;           /* Path to the keytab. */
    char *principal;        /* Principal whose keys are in the keytab. */
    char *cache;            /* Path to the Kerberos ticket cache. */
    char *userprinc;        /* The fully-qualified principal. */
    char *username;         /* The local (non-realm) part of principal. */
    char *realm;            /* The realm part of the principal. */
    char *password;         /* The password. */
    char *pkinit_principal; /* Principal for PKINIT authentication. */
    char *pkinit_cert;      /* Path to certificates for PKINIT. */
};

/*
 * Whether to skip all tests (by calling skip_all) in kerberos_setup if
 * certain configuration information isn't available.  "_BOTH" means that the
 * tests require both keytab and password, but PKINIT is not required.
 */
enum kerberos_needs
{
    /* clang-format off */
    TAP_KRB_NEEDS_NONE     = 0x00,
    TAP_KRB_NEEDS_KEYTAB   = 0x01,
    TAP_KRB_NEEDS_PASSWORD = 0x02,
    TAP_KRB_NEEDS_BOTH     = 0x01 | 0x02,
    TAP_KRB_NEEDS_PKINIT   = 0x04
    /* clang-format on */
};

BEGIN_DECLS

/*
 * Set up Kerberos, returning the test configuration information.  This
 * obtains Kerberos tickets from config/keytab, if one is present, and stores
 * them in a Kerberos ticket cache, sets KRB5_KTNAME and KRB5CCNAME.  It also
 * loads the principal and password from config/password, if it exists, and
 * stores the principal, password, username, and realm in the returned struct.
 *
 * If there is no config/keytab file, KRB5_KTNAME and KRB5CCNAME won't be set
 * and the keytab field will be NULL.  If there is no config/password file,
 * the principal field will be NULL.  If the files exist but loading them
 * fails, or authentication fails, kerberos_setup calls bail.
 *
 * kerberos_cleanup will be run as a cleanup function normally, freeing all
 * resources and cleaning up temporary files on process exit.  It can,
 * however, be called directly if for some reason the caller needs to delete
 * the Kerberos environment again.  However, normally the caller can just call
 * kerberos_setup again.
 */
struct kerberos_config *kerberos_setup(enum kerberos_needs)
    __attribute__((__malloc__));
void kerberos_cleanup(void);

/*
 * Generate a krb5.conf file for testing and set KRB5_CONFIG to point to it.
 * The [appdefaults] section will be stripped out and the default realm will
 * be set to the realm specified, if not NULL.  This will use config/krb5.conf
 * in preference, so users can configure the tests by creating that file if
 * the system file isn't suitable.
 *
 * Depends on data/generate-krb5-conf being present in the test suite.
 *
 * kerberos_cleanup_conf will clean up after this function, but usually
 * doesn't need to be called directly since it's registered as an atexit
 * handler.
 */
void kerberos_generate_conf(const char *realm);
void kerberos_cleanup_conf(void);

/* These interfaces are only available with native Kerberos support. */
#ifdef HAVE_KRB5

/* Bail out with an error, appending the Kerberos error message. */
void bail_krb5(krb5_context, long, const char *format, ...)
    __attribute__((__noreturn__, __nonnull__(3), __format__(printf, 3, 4)));

/* Report a diagnostic with Kerberos error to stderr prefixed with #. */
void diag_krb5(krb5_context, long, const char *format, ...)
    __attribute__((__nonnull__(3), __format__(printf, 3, 4)));

/*
 * Given a Kerberos context and the path to a keytab, retrieve the principal
 * for the first entry in the keytab and return it.  Calls bail on failure.
 * The returned principal should be freed with krb5_free_principal.
 */
krb5_principal kerberos_keytab_principal(krb5_context, const char *path)
    __attribute__((__nonnull__));

#endif /* HAVE_KRB5 */

END_DECLS

#endif /* !TAP_MESSAGES_H */
