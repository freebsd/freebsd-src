/*
 * Basic tests for the pam-krb5 module.
 *
 * This test case includes all tests that can be done without having Kerberos
 * configured and a username and password available, and without any special
 * configuration.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

#include <config.h>
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
    struct passwd pwd;
    char *uid;
    char *uidplus;

    plan_lazy();

    /*
     * Generate a testing krb5.conf file with a nonexistent default realm so
     * that this test will run on any system.
     */
    kerberos_generate_conf("bogus.example.com");

    /* Create a fake passwd struct for our user. */
    memset(&pwd, 0, sizeof(pwd));
    pwd.pw_name = (char *) "root";
    pwd.pw_uid = getuid();
    pwd.pw_gid = getgid();
    pam_set_pwd(&pwd);

    /*
     * Attempt login as the root user to test ignore_root.  Set our current
     * UID and a UID one larger for testing minimum_uid.
     */
    basprintf(&uid, "%lu", (unsigned long) pwd.pw_uid);
    basprintf(&uidplus, "%lu", (unsigned long) pwd.pw_uid + 1);
    memset(&config, 0, sizeof(config));
    config.user = "root";
    config.extra[0] = uid;
    config.extra[1] = uidplus;

    run_script_dir("data/scripts/basic", &config);

    free(uid);
    free(uidplus);
    return 0;
}
