/*
 * Excessively long password tests for the pam-krb5 module.
 *
 * This test case includes all tests for excessively long passwords that can
 * be done without having Kerberos configured and a username and password
 * available.
 *
 * Copyright 2020 Russ Allbery <eagle@eyrie.org>
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

#include <config.h>
#include <portable/system.h>

#include <tests/fakepam/script.h>
#include <tests/tap/basic.h>


int
main(void)
{
    struct script_config config;
    char *password;

    plan_lazy();

    memset(&config, 0, sizeof(config));
    config.user = "test";

    /* Test a password that is too long. */
    password = bcalloc_type(PAM_MAX_RESP_SIZE + 1, char);
    memset(password, 'a', PAM_MAX_RESP_SIZE);
    config.password = password;
    run_script("data/scripts/long/password", &config);
    run_script("data/scripts/long/password-debug", &config);

    /* Test a stored authtok that's too long. */
    config.authtok = password;
    config.password = "testing";
    run_script("data/scripts/long/use-first", &config);
    run_script("data/scripts/long/use-first-debug", &config);

    free(password);
    return 0;
}
