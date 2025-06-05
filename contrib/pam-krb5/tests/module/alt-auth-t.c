/*
 * Tests for the alt_auth_map functionality in libpam-krb5.
 *
 * This test case tests the variations of the alt_auth_map functionality for
 * both authentication and account management.  It requires a Kerberos
 * configuration, but does not attempt to save a session ticket cache (to
 * avoid requiring user configuration).
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2012
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
    char *user;

    /*
     * Load the Kerberos principal and password from a file, but set the
     * principal as extra[0] and use something else bogus as the user.  We
     * want to test that alt_auth_map works when there's no relationship
     * between the mapped principal and the user.
     */
    krbconf = kerberos_setup(TAP_KRB_NEEDS_PASSWORD);
    memset(&config, 0, sizeof(config));
    config.user = "bogus-nonexistent-account";
    config.authtok = krbconf->password;
    config.extra[0] = krbconf->username;
    config.extra[1] = krbconf->userprinc;

    /*
     * Generate a testing krb5.conf file with a nonexistent default realm so
     * that we can be sure that our principals will stay fully-qualified in
     * the logs.
     */
    kerberos_generate_conf("bogus.example.com");
    config.extra[2] = "bogus.example.com";

    /* Test without password prompting. */
    plan_lazy();
    run_script("data/scripts/alt-auth/basic", &config);
    run_script("data/scripts/alt-auth/basic-debug", &config);
    run_script("data/scripts/alt-auth/fail", &config);
    run_script("data/scripts/alt-auth/fail-debug", &config);
    run_script("data/scripts/alt-auth/force", &config);
    run_script("data/scripts/alt-auth/only", &config);

    /*
     * If the alternate account exists but the password is incorrect, we
     * should not fall back to the regular account.  Test with debug so that
     * we don't need two principals configured.
     */
    config.authtok = "bogus incorrect password";
    run_script("data/scripts/alt-auth/force-fail-debug", &config);

    /*
     * Switch to our correct user (but wrong realm) realm to test username
     * mapping to a different realm.
     */
    config.authtok = krbconf->password;
    config.user = krbconf->username;
    config.extra[2] = krbconf->realm;
    run_script("data/scripts/alt-auth/username-map", &config);

    /*
     * Split the username into two parts, one in the PAM configuration and one
     * in the real username, so that we can test interpolation of the username
     * when %s isn't the first token.
     */
    config.user = &krbconf->username[1];
    user = bstrndup(krbconf->username, 1);
    config.extra[3] = user;
    run_script("data/scripts/alt-auth/username-map-prefix", &config);
    free(user);
    config.extra[3] = NULL;

    /*
     * Ensure that we don't add the realm of the authentication username when
     * the alt_auth_map already includes a realm.
     */
    basprintf(&user, "%s@foo.example.com", krbconf->username);
    config.user = user;
    diag("re-running username-map with fully-qualified PAM user");
    run_script("data/scripts/alt-auth/username-map", &config);
    free(user);

    /*
     * Add the password and make the user match our authentication principal,
     * and then test fallback to normal authentication when alternative
     * authentication fails.
     */
    config.user = krbconf->userprinc;
    config.password = krbconf->password;
    config.extra[2] = krbconf->realm;
    run_script("data/scripts/alt-auth/fallback", &config);
    run_script("data/scripts/alt-auth/fallback-debug", &config);
    run_script("data/scripts/alt-auth/fallback-realm", &config);
    run_script("data/scripts/alt-auth/force-fallback", &config);
    run_script("data/scripts/alt-auth/only-fail", &config);

    return 0;
}
