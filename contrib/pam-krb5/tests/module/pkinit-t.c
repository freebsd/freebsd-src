/*
 * PKINIT authentication tests for the pam-krb5 module.
 *
 * This test case includes tests that require a PKINIT certificate, but which
 * don't write a Kerberos ticket cache.
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
#if defined(HAVE_KRB5_MIT) && defined(PATH_OPENSSL)
    const char **generate_pkcs12;
    char *tmpdir, *pkcs12_path;
#endif

    /* Load the Kerberos principal and certificate path. */
    krbconf = kerberos_setup(TAP_KRB_NEEDS_PKINIT);
    memset(&config, 0, sizeof(config));
    config.user = krbconf->pkinit_principal;
    config.extra[0] = krbconf->pkinit_cert;

    /*
     * Generate a testing krb5.conf file with a nonexistent default realm so
     * that we can be sure that our principals will stay fully-qualified in
     * the logs.
     */
    kerberos_generate_conf("bogus.example.com");

    /* Check things that are the same with both Kerberos implementations. */
    plan_lazy();
    run_script("data/scripts/pkinit/basic", &config);
    run_script("data/scripts/pkinit/basic-debug", &config);
    run_script("data/scripts/pkinit/prompt-use", &config);
    run_script("data/scripts/pkinit/prompt-try", &config);
    run_script("data/scripts/pkinit/try-pkinit", &config);

    /* Debugging output is a little different between the implementations. */
#ifdef HAVE_KRB5_HEIMDAL
    run_script("data/scripts/pkinit/try-pkinit-debug", &config);
#else
    run_script("data/scripts/pkinit/try-pkinit-debug-mit", &config);
#endif

    /* Only MIT Kerberos supports setting preauth options. */
#ifdef HAVE_KRB5_MIT
    run_script("data/scripts/pkinit/preauth-opt-mit", &config);
#endif

    /*
     * If OpenSSL is available, test prompting with MIT Kerberos since we have
     * to implement the prompting for the use_pkinit case ourselves.  To do
     * this, convert the input PKINIT certificate to a PKCS12 file with a
     * password.
     */
#if defined(HAVE_KRB5_MIT) && defined(PATH_OPENSSL)
    tmpdir = test_tmpdir();
    basprintf(&pkcs12_path, "%s/%s", tmpdir, "pkinit-pkcs12");
    generate_pkcs12 = bcalloc_type(10, const char *);
    generate_pkcs12[0] = PATH_OPENSSL;
    generate_pkcs12[1] = "pkcs12";
    generate_pkcs12[2] = "-export";
    generate_pkcs12[3] = "-in";
    generate_pkcs12[4] = krbconf->pkinit_cert;
    generate_pkcs12[5] = "-password";
    generate_pkcs12[6] = "pass:some-password";
    generate_pkcs12[7] = "-out";
    generate_pkcs12[8] = pkcs12_path;
    generate_pkcs12[9] = NULL;
    run_setup(generate_pkcs12);
    free(generate_pkcs12);
    config.extra[0] = pkcs12_path;
    config.extra[1] = "some-password";
    run_script("data/scripts/pkinit/pin-mit", &config);
    unlink(pkcs12_path);
    free(pkcs12_path);
    test_tmpdir_free(tmpdir);
#endif /* HAVE_KRB5_MIT && PATH_OPENSSL */

    return 0;
}
