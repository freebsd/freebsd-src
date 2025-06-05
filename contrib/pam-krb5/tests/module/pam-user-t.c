/*
 * Tests for PAM_USER handling.
 *
 * This test case includes tests that require Kerberos to be configured and a
 * username and password available, but which don't write a ticket cache
 * (which requires additional work to test the cache ownership).
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2014, 2020 Russ Allbery <eagle@eyrie.org>
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

#include <config.h>
#include <portable/system.h>

#include <tests/fakepam/script.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/macros.h>


/*
 * Callback to check that PAM_USER matches the desired value, passed in as the
 * data parameter.
 */
static void
check_pam_user(pam_handle_t *pamh, const struct script_config *config UNUSED,
               void *data)
{
    int retval;
    const char *name = NULL;
    const char *expected = data;

    retval = pam_get_item(pamh, PAM_USER, (PAM_CONST void **) &name);
    is_int(PAM_SUCCESS, retval, "Found PAM_USER");
    is_string(expected, name, "...matching %s", expected);
}


int
main(void)
{
    struct script_config config;
    struct kerberos_config *krbconf;

    /* Load the Kerberos principal and password from a file. */
    krbconf = kerberos_setup(TAP_KRB_NEEDS_PASSWORD);
    memset(&config, 0, sizeof(config));
    config.password = krbconf->password;
    config.callback = check_pam_user;
    config.extra[0] = krbconf->username;
    config.extra[1] = krbconf->userprinc;

    /*
     * Generate a testing krb5.conf file matching the realm of the Kerberos
     * configuration so that canonicalization will work.
     */
    kerberos_generate_conf(krbconf->realm);

    /* Declare our plan. */
    plan_lazy();

    /* Authentication without a realm.  No canonicalization. */
    config.user = krbconf->username;
    config.data = krbconf->username;
    run_script("data/scripts/pam-user/update", &config);

    /* Authentication with the local realm.  Should be canonicalized. */
    config.user = krbconf->userprinc;
    run_script("data/scripts/pam-user/update", &config);

    /*
     * Now, test again with user updates disabled.  The PAM_USER value should
     * now not be canonicalized.
     */
    config.data = krbconf->userprinc;
    run_script("data/scripts/pam-user/no-update", &config);

    return 0;
}
