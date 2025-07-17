/*
 * Tests for authenticated FAST support in pam-krb5.
 *
 * Tests for Flexible Authentication Secure Tunneling, a mechanism for
 * improving the preauthentication part of the Kerberos protocol and
 * protecting it against various attacks.  This tests authenticated FAST;
 * anonymous FAST is tested separately.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2017, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

#include <config.h>
#include <portable/system.h>

#include <tests/fakepam/script.h>
#include <tests/tap/kerberos.h>


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
    krbconf = kerberos_setup(TAP_KRB_NEEDS_BOTH);
    memset(&config, 0, sizeof(config));
    config.user = krbconf->userprinc;
    config.authtok = krbconf->password;
    config.extra[0] = krbconf->cache;

    /*
     * Generate a testing krb5.conf file with a nonexistent default realm so
     * that we can be sure that our principals will stay fully-qualified in
     * the logs.
     */
    kerberos_generate_conf("bogus.example.com");

    /* Test fast_ccache */
    plan_lazy();
    run_script("data/scripts/fast/ccache", &config);
    run_script("data/scripts/fast/ccache-debug", &config);
    run_script("data/scripts/fast/no-ccache", &config);
    run_script("data/scripts/fast/no-ccache-debug", &config);

    return 0;
}
