/*
 * Authentication tests for the pam-krb5 module with an existing AUTHTOK.
 *
 * This test case includes tests that require Kerberos to be configured and a
 * username and password available and that run with AUTHTOK already set, but
 * which don't write a ticket cache (which requires additional work to test
 * the cache ownership).
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011-2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

#include <config.h>
#include <portable/system.h>

#include <tests/fakepam/script.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/process.h>
#include <tests/tap/string.h>


int
main(void)
{
    struct script_config config;
    struct kerberos_config *krbconf;

    /* Load the Kerberos principal and password from a file. */
    krbconf = kerberos_setup(TAP_KRB_NEEDS_PASSWORD);
    memset(&config, 0, sizeof(config));
    config.user = krbconf->userprinc;
    config.password = krbconf->password;
    config.authtok = krbconf->password;

    /*
     * Generate a testing krb5.conf file with a nonexistent default realm so
     * that we can be sure that our principals will stay fully-qualified in
     * the logs.
     */
    kerberos_generate_conf("bogus.example.com");

    plan_lazy();
    run_script_dir("data/scripts/stacked", &config);

    return 0;
}
