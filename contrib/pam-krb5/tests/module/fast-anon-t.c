/*
 * Tests for anonymous FAST support in pam-krb5.
 *
 * Tests for anonymous Flexible Authentication Secure Tunneling, a mechanism
 * for improving the preauthentication part of the Kerberos protocol and
 * protecting it against various attacks.
 *
 * This is broken out from the other FAST tests because it uses PKINIT, and
 * PKINIT code cannot be tested under valgrind with MIT Kerberos due to some
 * bug in valgrind.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2017, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

#include <config.h>
#include <portable/krb5.h>
#include <portable/system.h>

#include <tests/fakepam/script.h>
#include <tests/tap/kerberos.h>


/*
 * Test whether anonymous authentication works.  If this doesn't, we need to
 * skip the tests of anonymous FAST.
 */
static bool
anon_fast_works(void)
{
    krb5_context ctx;
    krb5_error_code retval;
    krb5_principal princ = NULL;
    char *realm;
    krb5_creds creds;
    krb5_get_init_creds_opt *opts = NULL;

    /* Construct the anonymous principal name. */
    retval = krb5_init_context(&ctx);
    if (retval != 0)
        bail("cannot initialize Kerberos");
    retval = krb5_get_default_realm(ctx, &realm);
    if (retval != 0)
        bail("cannot get default realm");
    retval = krb5_build_principal_ext(
        ctx, &princ, (unsigned int) strlen(realm), realm,
        strlen(KRB5_WELLKNOWN_NAME), KRB5_WELLKNOWN_NAME,
        strlen(KRB5_ANON_NAME), KRB5_ANON_NAME, NULL);
    if (retval != 0)
        bail("cannot construct anonymous principal");
    krb5_free_default_realm(ctx, realm);

    /* Obtain the credentials. */
    memset(&creds, 0, sizeof(creds));
    retval = krb5_get_init_creds_opt_alloc(ctx, &opts);
    if (retval != 0)
        bail("cannot create credential options");
    krb5_get_init_creds_opt_set_anonymous(opts, 1);
    krb5_get_init_creds_opt_set_tkt_life(opts, 60);
    retval = krb5_get_init_creds_password(ctx, &creds, princ, NULL, NULL, NULL,
                                          0, NULL, opts);

    /* Clean up. */
    if (princ != NULL)
        krb5_free_principal(ctx, princ);
    if (opts != NULL)
        krb5_get_init_creds_opt_free(ctx, opts);
    krb5_free_cred_contents(ctx, &creds);

    /* Return whether authentication succeeded. */
    return (retval == 0);
}


int
main(void)
{
    struct script_config config;
    struct kerberos_config *krbconf;

    /* Skip the test if FAST is not available. */
#ifndef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_FAST_CCACHE_NAME
    skip_all("FAST support not available");
#endif

    /* Initialize Kerberos configuration. */
    krbconf = kerberos_setup(TAP_KRB_NEEDS_PASSWORD);
    memset(&config, 0, sizeof(config));
    config.user = krbconf->username;
    config.authtok = krbconf->password;
    config.extra[0] = krbconf->userprinc;
    kerberos_generate_conf(krbconf->realm);

    /* Skip the test if anonymous PKINIT doesn't work. */
    if (!anon_fast_works())
        skip_all("anonymous PKINIT failed");

    /* Test anonymous FAST. */
    plan_lazy();
    run_script("data/scripts/fast/anonymous", &config);
    run_script("data/scripts/fast/anonymous-debug", &config);

    return 0;
}
