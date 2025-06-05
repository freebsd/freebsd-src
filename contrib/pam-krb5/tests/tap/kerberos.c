/*
 * Utility functions for tests that use Kerberos.
 *
 * The core function is kerberos_setup, which loads Kerberos test
 * configuration and returns a struct of information.  It also supports
 * obtaining initial tickets from the configured keytab and setting up
 * KRB5CCNAME and KRB5_KTNAME if a Kerberos keytab is present.  Also included
 * are utility functions for setting up a krb5.conf file and reporting
 * Kerberos errors or warnings during testing.
 *
 * Some of the functionality here is only available if the Kerberos libraries
 * are available.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2017 Russ Allbery <eagle@eyrie.org>
 * Copyright 2006-2007, 2009-2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#ifdef HAVE_KRB5
#    include <portable/krb5.h>
#endif
#include <portable/system.h>

#include <sys/stat.h>

#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/macros.h>
#include <tests/tap/process.h>
#include <tests/tap/string.h>

/*
 * Disable the requirement that format strings be literals, since it's easier
 * to handle the possible patterns for kinit commands as an array.
 */
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 2) || defined(__clang__)
#    pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif


/*
 * These variables hold the allocated configuration struct, the environment to
 * point to a different Kerberos ticket cache, keytab, and configuration file,
 * and the temporary directories used.  We store them so that we can free them
 * on exit for cleaner valgrind output, making it easier to find real memory
 * leaks in the tested programs.
 */
static struct kerberos_config *config = NULL;
static char *krb5ccname = NULL;
static char *krb5_ktname = NULL;
static char *krb5_config = NULL;
static char *tmpdir_ticket = NULL;
static char *tmpdir_conf = NULL;


/*
 * Obtain Kerberos tickets and fill in the principal config entry.
 *
 * There are two implementations of this function, one if we have native
 * Kerberos libraries available and one if we don't.  Uses keytab to obtain
 * credentials, and fills in the cache member of the provided config struct.
 */
#ifdef HAVE_KRB5

static void
kerberos_kinit(void)
{
    char *name, *krbtgt;
    krb5_error_code code;
    krb5_context ctx;
    krb5_ccache ccache;
    krb5_principal kprinc;
    krb5_keytab keytab;
    krb5_get_init_creds_opt *opts;
    krb5_creds creds;
    const char *realm;

    /*
     * Determine the principal corresponding to that keytab.  We copy the
     * memory to ensure that it's allocated in the right memory domain on
     * systems where that may matter (like Windows).
     */
    code = krb5_init_context(&ctx);
    if (code != 0)
        bail_krb5(ctx, code, "error initializing Kerberos");
    kprinc = kerberos_keytab_principal(ctx, config->keytab);
    code = krb5_unparse_name(ctx, kprinc, &name);
    if (code != 0)
        bail_krb5(ctx, code, "error unparsing name");
    krb5_free_principal(ctx, kprinc);
    config->principal = bstrdup(name);
    krb5_free_unparsed_name(ctx, name);

    /* Now do the Kerberos initialization. */
    code = krb5_cc_default(ctx, &ccache);
    if (code != 0)
        bail_krb5(ctx, code, "error setting ticket cache");
    code = krb5_parse_name(ctx, config->principal, &kprinc);
    if (code != 0)
        bail_krb5(ctx, code, "error parsing principal %s", config->principal);
    realm = krb5_principal_get_realm(ctx, kprinc);
    basprintf(&krbtgt, "krbtgt/%s@%s", realm, realm);
    code = krb5_kt_resolve(ctx, config->keytab, &keytab);
    if (code != 0)
        bail_krb5(ctx, code, "cannot open keytab %s", config->keytab);
    code = krb5_get_init_creds_opt_alloc(ctx, &opts);
    if (code != 0)
        bail_krb5(ctx, code, "cannot allocate credential options");
    krb5_get_init_creds_opt_set_default_flags(ctx, NULL, realm, opts);
    krb5_get_init_creds_opt_set_forwardable(opts, 0);
    krb5_get_init_creds_opt_set_proxiable(opts, 0);
    code = krb5_get_init_creds_keytab(ctx, &creds, kprinc, keytab, 0, krbtgt,
                                      opts);
    if (code != 0)
        bail_krb5(ctx, code, "cannot get Kerberos tickets");
    code = krb5_cc_initialize(ctx, ccache, kprinc);
    if (code != 0)
        bail_krb5(ctx, code, "error initializing ticket cache");
    code = krb5_cc_store_cred(ctx, ccache, &creds);
    if (code != 0)
        bail_krb5(ctx, code, "error storing credentials");
    krb5_cc_close(ctx, ccache);
    krb5_free_cred_contents(ctx, &creds);
    krb5_kt_close(ctx, keytab);
    krb5_free_principal(ctx, kprinc);
    krb5_get_init_creds_opt_free(ctx, opts);
    krb5_free_context(ctx);
    free(krbtgt);
}

#else /* !HAVE_KRB5 */

static void
kerberos_kinit(void)
{
    static const char *const format[] = {
        "kinit --no-afslog -k -t %s %s >/dev/null 2>&1 </dev/null",
        "kinit -k -t %s %s >/dev/null 2>&1 </dev/null",
        "kinit -t %s %s >/dev/null 2>&1 </dev/null",
        "kinit -k -K %s %s >/dev/null 2>&1 </dev/null"};
    FILE *file;
    char *path;
    char principal[BUFSIZ], *command;
    size_t i;
    int status;

    /* Read the principal corresponding to the keytab. */
    path = test_file_path("config/principal");
    if (path == NULL) {
        test_file_path_free(config->keytab);
        config->keytab = NULL;
        return;
    }
    file = fopen(path, "r");
    if (file == NULL) {
        test_file_path_free(path);
        return;
    }
    test_file_path_free(path);
    if (fgets(principal, sizeof(principal), file) == NULL)
        bail("cannot read %s", path);
    fclose(file);
    if (principal[strlen(principal) - 1] != '\n')
        bail("no newline in %s", path);
    principal[strlen(principal) - 1] = '\0';
    config->principal = bstrdup(principal);

    /* Now do the Kerberos initialization. */
    for (i = 0; i < ARRAY_SIZE(format); i++) {
        basprintf(&command, format[i], config->keytab, principal);
        status = system(command);
        free(command);
        if (status != -1 && WEXITSTATUS(status) == 0)
            break;
    }
    if (status == -1 || WEXITSTATUS(status) != 0)
        bail("cannot get Kerberos tickets");
}

#endif /* !HAVE_KRB5 */


/*
 * Free all the memory associated with our Kerberos setup, but don't remove
 * the ticket cache.  This is used when cleaning up on exit from a non-primary
 * process so that test programs that fork don't remove the ticket cache still
 * used by the main program.
 */
static void
kerberos_free(void)
{
    test_tmpdir_free(tmpdir_ticket);
    tmpdir_ticket = NULL;
    if (config != NULL) {
        test_file_path_free(config->keytab);
        free(config->principal);
        free(config->cache);
        free(config->userprinc);
        free(config->username);
        free(config->password);
        free(config->pkinit_principal);
        free(config->pkinit_cert);
        free(config);
        config = NULL;
    }
    if (krb5ccname != NULL) {
        putenv((char *) "KRB5CCNAME=");
        free(krb5ccname);
        krb5ccname = NULL;
    }
    if (krb5_ktname != NULL) {
        putenv((char *) "KRB5_KTNAME=");
        free(krb5_ktname);
        krb5_ktname = NULL;
    }
}


/*
 * Clean up at the end of a test.  This removes the ticket cache and resets
 * and frees the memory allocated for the environment variables so that
 * valgrind output on test suites is cleaner.  Most of the work is done by
 * kerberos_free, but this function also deletes the ticket cache.
 */
void
kerberos_cleanup(void)
{
    char *path;

    if (tmpdir_ticket != NULL) {
        basprintf(&path, "%s/krb5cc_test", tmpdir_ticket);
        unlink(path);
        free(path);
    }
    kerberos_free();
}


/*
 * The cleanup handler for the TAP framework.  Call kerberos_cleanup if we're
 * in the primary process and kerberos_free if not.  The first argument, which
 * indicates whether the test succeeded or not, is ignored, since we need to
 * do the same thing either way.
 */
static void
kerberos_cleanup_handler(int success UNUSED, int primary)
{
    if (primary)
        kerberos_cleanup();
    else
        kerberos_free();
}


/*
 * Obtain Kerberos tickets for the principal specified in config/principal
 * using the keytab specified in config/keytab, both of which are presumed to
 * be in tests in either the build or the source tree.  Also sets KRB5_KTNAME
 * and KRB5CCNAME.
 *
 * Returns the contents of config/principal in newly allocated memory or NULL
 * if Kerberos tests are apparently not configured.  If Kerberos tests are
 * configured but something else fails, calls bail.
 */
struct kerberos_config *
kerberos_setup(enum kerberos_needs needs)
{
    char *path;
    char buffer[BUFSIZ];
    FILE *file = NULL;

    /* If we were called before, clean up after the previous run. */
    if (config != NULL)
        kerberos_cleanup();
    config = bcalloc(1, sizeof(struct kerberos_config));

    /*
     * If we have a config/keytab file, set the KRB5CCNAME and KRB5_KTNAME
     * environment variables and obtain initial tickets.
     */
    config->keytab = test_file_path("config/keytab");
    if (config->keytab == NULL) {
        if (needs == TAP_KRB_NEEDS_KEYTAB || needs == TAP_KRB_NEEDS_BOTH)
            skip_all("Kerberos tests not configured");
    } else {
        tmpdir_ticket = test_tmpdir();
        basprintf(&config->cache, "%s/krb5cc_test", tmpdir_ticket);
        basprintf(&krb5ccname, "KRB5CCNAME=%s/krb5cc_test", tmpdir_ticket);
        basprintf(&krb5_ktname, "KRB5_KTNAME=%s", config->keytab);
        putenv(krb5ccname);
        putenv(krb5_ktname);
        kerberos_kinit();
    }

    /*
     * If we have a config/password file, read it and fill out the relevant
     * members of our config struct.
     */
    path = test_file_path("config/password");
    if (path != NULL)
        file = fopen(path, "r");
    if (file == NULL) {
        if (needs == TAP_KRB_NEEDS_PASSWORD || needs == TAP_KRB_NEEDS_BOTH)
            skip_all("Kerberos tests not configured");
    } else {
        if (fgets(buffer, sizeof(buffer), file) == NULL)
            bail("cannot read %s", path);
        if (buffer[strlen(buffer) - 1] != '\n')
            bail("no newline in %s", path);
        buffer[strlen(buffer) - 1] = '\0';
        config->userprinc = bstrdup(buffer);
        if (fgets(buffer, sizeof(buffer), file) == NULL)
            bail("cannot read password from %s", path);
        fclose(file);
        if (buffer[strlen(buffer) - 1] != '\n')
            bail("password too long in %s", path);
        buffer[strlen(buffer) - 1] = '\0';
        config->password = bstrdup(buffer);

        /*
         * Strip the realm from the principal and set realm and username.
         * This is not strictly correct; it doesn't cope with escaped @-signs
         * or enterprise names.
         */
        config->username = bstrdup(config->userprinc);
        config->realm = strchr(config->username, '@');
        if (config->realm == NULL)
            bail("test principal has no realm");
        *config->realm = '\0';
        config->realm++;
    }
    test_file_path_free(path);

    /*
     * If we have PKINIT configuration, read it and fill out the relevant
     * members of our config struct.
     */
    path = test_file_path("config/pkinit-principal");
    if (path != NULL)
        file = fopen(path, "r");
    if (path != NULL && file != NULL) {
        if (fgets(buffer, sizeof(buffer), file) == NULL)
            bail("cannot read %s", path);
        if (buffer[strlen(buffer) - 1] != '\n')
            bail("no newline in %s", path);
        buffer[strlen(buffer) - 1] = '\0';
        fclose(file);
        test_file_path_free(path);
        path = test_file_path("config/pkinit-cert");
        if (path != NULL) {
            config->pkinit_principal = bstrdup(buffer);
            config->pkinit_cert = bstrdup(path);
        }
    }
    test_file_path_free(path);
    if (config->pkinit_cert == NULL && (needs & TAP_KRB_NEEDS_PKINIT) != 0)
        skip_all("PKINIT tests not configured");

    /*
     * Register the cleanup function so that the caller doesn't have to do
     * explicit cleanup.
     */
    test_cleanup_register(kerberos_cleanup_handler);

    /* Return the configuration. */
    return config;
}


/*
 * Clean up the krb5.conf file generated by kerberos_generate_conf and free
 * the memory used to set the environment variable.  This doesn't fail if the
 * file and variable are already gone, allowing it to be harmlessly run
 * multiple times.
 *
 * Normally called via an atexit handler.
 */
void
kerberos_cleanup_conf(void)
{
    char *path;

    if (tmpdir_conf != NULL) {
        basprintf(&path, "%s/krb5.conf", tmpdir_conf);
        unlink(path);
        free(path);
        test_tmpdir_free(tmpdir_conf);
        tmpdir_conf = NULL;
    }
    putenv((char *) "KRB5_CONFIG=");
    free(krb5_config);
    krb5_config = NULL;
}


/*
 * Generate a krb5.conf file for testing and set KRB5_CONFIG to point to it.
 * The [appdefaults] section will be stripped out and the default realm will
 * be set to the realm specified, if not NULL.  This will use config/krb5.conf
 * in preference, so users can configure the tests by creating that file if
 * the system file isn't suitable.
 *
 * Depends on data/generate-krb5-conf being present in the test suite.
 */
void
kerberos_generate_conf(const char *realm)
{
    char *path;
    const char *argv[3];

    if (tmpdir_conf != NULL)
        kerberos_cleanup_conf();
    path = test_file_path("data/generate-krb5-conf");
    if (path == NULL)
        bail("cannot find generate-krb5-conf");
    argv[0] = path;
    argv[1] = realm;
    argv[2] = NULL;
    run_setup(argv);
    test_file_path_free(path);
    tmpdir_conf = test_tmpdir();
    basprintf(&krb5_config, "KRB5_CONFIG=%s/krb5.conf", tmpdir_conf);
    putenv(krb5_config);
    if (atexit(kerberos_cleanup_conf) != 0)
        sysdiag("cannot register cleanup function");
}


/*
 * The remaining functions in this file are only available if Kerberos
 * libraries are available.
 */
#ifdef HAVE_KRB5


/*
 * Report a Kerberos error and bail out.  Takes a long instead of a
 * krb5_error_code because it can also handle a kadm5_ret_t (which may be a
 * different size).
 */
void
bail_krb5(krb5_context ctx, long code, const char *format, ...)
{
    const char *k5_msg = NULL;
    char *message;
    va_list args;

    if (ctx != NULL)
        k5_msg = krb5_get_error_message(ctx, (krb5_error_code) code);
    va_start(args, format);
    bvasprintf(&message, format, args);
    va_end(args);
    if (k5_msg == NULL)
        bail("%s", message);
    else
        bail("%s: %s", message, k5_msg);
}


/*
 * Report a Kerberos error as a diagnostic to stderr.  Takes a long instead of
 * a krb5_error_code because it can also handle a kadm5_ret_t (which may be a
 * different size).
 */
void
diag_krb5(krb5_context ctx, long code, const char *format, ...)
{
    const char *k5_msg = NULL;
    char *message;
    va_list args;

    if (ctx != NULL)
        k5_msg = krb5_get_error_message(ctx, (krb5_error_code) code);
    va_start(args, format);
    bvasprintf(&message, format, args);
    va_end(args);
    if (k5_msg == NULL)
        diag("%s", message);
    else
        diag("%s: %s", message, k5_msg);
    free(message);
    if (k5_msg != NULL)
        krb5_free_error_message(ctx, k5_msg);
}


/*
 * Find the principal of the first entry of a keytab and return it.  The
 * caller is responsible for freeing the result with krb5_free_principal.
 * Exit on error.
 */
krb5_principal
kerberos_keytab_principal(krb5_context ctx, const char *path)
{
    krb5_keytab keytab;
    krb5_kt_cursor cursor;
    krb5_keytab_entry entry;
    krb5_principal princ;
    krb5_error_code status;

    status = krb5_kt_resolve(ctx, path, &keytab);
    if (status != 0)
        bail_krb5(ctx, status, "error opening %s", path);
    status = krb5_kt_start_seq_get(ctx, keytab, &cursor);
    if (status != 0)
        bail_krb5(ctx, status, "error reading %s", path);
    status = krb5_kt_next_entry(ctx, keytab, &entry, &cursor);
    if (status != 0)
        bail("no principal found in keytab file %s", path);
    status = krb5_copy_principal(ctx, entry.principal, &princ);
    if (status != 0)
        bail_krb5(ctx, status, "error copying principal from %s", path);
    krb5_kt_free_entry(ctx, &entry);
    krb5_kt_end_seq_get(ctx, keytab, &cursor);
    krb5_kt_close(ctx, keytab);
    return princ;
}

#endif /* HAVE_KRB5 */
