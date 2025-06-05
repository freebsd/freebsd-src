/*
 * Authentication tests for realm support in pam-krb5.
 *
 * Test the realm and user_realm option in the PAM configuration, which is
 * special in several ways since it influences krb5.conf parsing and is read
 * out of order in the initial configuration.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011-2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

#include <config.h>
#include <portable/krb5.h>
#include <portable/system.h>

#include <pwd.h>

#include <tests/fakepam/pam.h>
#include <tests/fakepam/script.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/string.h>


int
main(void)
{
    struct script_config config;
    struct kerberos_config *krbconf;
    struct passwd pwd;
    FILE *file;
    char *k5login;

    /* Load the Kerberos principal and password from a file. */
    krbconf = kerberos_setup(TAP_KRB_NEEDS_PASSWORD);
    memset(&config, 0, sizeof(config));
    config.user = krbconf->username;
    config.authtok = krbconf->password;

    /* Don't keep track of the tests in each script. */
    plan_lazy();

    /* Start with a nonexistent default realm for authentication failure. */
    kerberos_generate_conf("bogus.example.com");
    config.extra[0] = "bogus.example.com";
    run_script("data/scripts/realm/fail-no-realm", &config);
    run_script("data/scripts/realm/fail-no-realm-debug", &config);

    /* Running a script that sets realm properly should pass. */
    config.extra[0] = krbconf->realm;
    run_script("data/scripts/realm/pass-realm", &config);

    /* Setting user_realm should continue to fail due to no .k5login file. */
    run_script("data/scripts/realm/fail-user-realm", &config);

    /* If we add a .k5login file for the user, user_realm should work. */
    pwd.pw_name = krbconf->username;
    pwd.pw_uid = getuid();
    pwd.pw_gid = getgid();
    pwd.pw_dir = test_tmpdir();
    pam_set_pwd(&pwd);
    basprintf(&k5login, "%s/.k5login", pwd.pw_dir);
    file = fopen(k5login, "w");
    if (file == NULL)
        sysbail("cannot create %s", k5login);
    if (fprintf(file, "%s\n", krbconf->userprinc) < 0)
        sysbail("cannot write to %s", k5login);
    if (fclose(file) < 0)
        sysbail("cannot flush %s", k5login);
    run_script("data/scripts/realm/pass-user-realm", &config);
    pam_set_pwd(NULL);
    unlink(k5login);
    free(k5login);
    test_tmpdir_free(pwd.pw_dir);

    /* Switch to the correct realm, but set the wrong realm in PAM. */
    kerberos_generate_conf(krbconf->realm);
    config.extra[0] = "bogus.example.com";
    run_script("data/scripts/realm/fail-realm", &config);
    run_script("data/scripts/realm/fail-bad-user-realm", &config);

    return 0;
}
