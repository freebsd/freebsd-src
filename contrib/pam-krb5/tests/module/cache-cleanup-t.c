/*
 * Test for properly cleaning up ticket caches.
 *
 * Verify that the temporary Kerberos ticket cache generated during
 * authentication is cleaned up on pam_end, even if no session was opened.
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

#include <dirent.h>

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
    DIR *tmpdir;
    struct dirent *file;
    char *tmppath, *path;

    /* Load the Kerberos principal and password from a file. */
    krbconf = kerberos_setup(TAP_KRB_NEEDS_PASSWORD);
    memset(&config, 0, sizeof(config));
    config.user = krbconf->username;
    config.authtok = krbconf->password;
    config.extra[0] = krbconf->userprinc;

    /* Generate a testing krb5.conf file. */
    kerberos_generate_conf(krbconf->realm);

    /* Get the temporary directory and store that as the %1 substitution. */
    tmppath = test_tmpdir();
    config.extra[1] = tmppath;

    plan_lazy();

    /*
     * We need to ensure that the only thing in the test temporary directory
     * is the krb5.conf file that we generated and any valgrind logs, since
     * we're going to check for cleanup by looking for any out-of-place files.
     */
    tmpdir = opendir(tmppath);
    if (tmpdir == NULL)
        sysbail("cannot open directory %s", tmppath);
    while ((file = readdir(tmpdir)) != NULL) {
        if (strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0)
            continue;
        if (strcmp(file->d_name, "krb5.conf") == 0)
            continue;
        if (strcmp(file->d_name, "valgrind") == 0)
            continue;
        basprintf(&path, "%s/%s", tmppath, file->d_name);
        if (unlink(path) < 0)
            sysbail("cannot delete temporary file %s", path);
        free(path);
    }
    closedir(tmpdir);

    /*
     * Authenticate only, call pam_end, and be sure the ticket cache is
     * gone.  The auth-only script sets ccache_dir to the temporary directory,
     * so the module will create a temporary ticket cache there and then
     * should clean it up.
     */
    run_script("data/scripts/cache-cleanup/auth-only", &config);
    path = NULL;
    tmpdir = opendir(tmppath);
    if (tmpdir == NULL)
        sysbail("cannot open directory %s", tmppath);
    while ((file = readdir(tmpdir)) != NULL) {
        if (strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0)
            continue;
        if (strcmp(file->d_name, "krb5.conf") == 0)
            continue;
        if (strcmp(file->d_name, "valgrind") == 0)
            continue;
        if (path == NULL)
            basprintf(&path, "%s/%s", tmppath, file->d_name);
    }
    closedir(tmpdir);
    if (path != NULL)
        diag("found stray temporary file %s", path);
    ok(path == NULL, "ticket cache cleaned up");
    if (path != NULL)
        free(path);

    test_tmpdir_free(tmppath);
    return 0;
}
