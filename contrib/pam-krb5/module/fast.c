/*
 * Support for FAST (Flexible Authentication Secure Tunneling).
 *
 * FAST is a mechanism to protect Kerberos against password guessing attacks
 * and provide other security improvements.  It requires existing credentials
 * to protect the initial preauthentication exchange.  These can come either
 * from a ticket cache for another principal or via anonymous PKINIT.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Contributions from Sam Hartman and Yair Yarom
 * Copyright 2017, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2010, 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

#include <config.h>
#include <portable/krb5.h>
#include <portable/system.h>

#include <errno.h>

#include <module/internal.h>
#include <pam-util/args.h>
#include <pam-util/logging.h>


/*
 * Initialize an internal anonymous ticket cache with a random name and store
 * the resulting ticket cache in the ccache argument.  Returns a Kerberos
 * error code.
 */
#ifndef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_ANONYMOUS

static krb5_error_code
cache_init_anonymous(struct pam_args *args, krb5_ccache *ccache UNUSED)
{
    putil_debug(args, "not built with anonymous FAST support");
    return KRB5KDC_ERR_BADOPTION;
}

#else /* HAVE_KRB5_GET_INIT_CREDS_OPT_SET_ANONYMOUS */

static krb5_error_code
cache_init_anonymous(struct pam_args *args, krb5_ccache *ccache)
{
    krb5_context c = args->config->ctx->context;
    krb5_error_code retval;
    krb5_principal princ = NULL;
    char *realm;
    char *name = NULL;
    krb5_creds creds;
    bool creds_valid = false;
    krb5_get_init_creds_opt *opts = NULL;

    *ccache = NULL;
    memset(&creds, 0, sizeof(creds));

    /* Construct the anonymous principal name. */
    retval = krb5_get_default_realm(c, &realm);
    if (retval != 0) {
        putil_debug_krb5(args, retval, "cannot find realm for anonymous FAST");
        return retval;
    }
    retval = krb5_build_principal_ext(
        c, &princ, (unsigned int) strlen(realm), realm,
        strlen(KRB5_WELLKNOWN_NAME), KRB5_WELLKNOWN_NAME,
        strlen(KRB5_ANON_NAME), KRB5_ANON_NAME, NULL);
    if (retval != 0) {
        krb5_free_default_realm(c, realm);
        putil_debug_krb5(args, retval, "cannot create anonymous principal");
        return retval;
    }
    krb5_free_default_realm(c, realm);

    /*
     * Set up the credential cache the anonymous credentials.  We use a
     * memory cache whose name is based on the pointer value of our Kerberos
     * context, since that should be unique among threads.
     */
    if (asprintf(&name, "MEMORY:%p", (void *) c) < 0) {
        putil_crit(args, "malloc failure: %s", strerror(errno));
        retval = errno;
        goto done;
    }
    retval = krb5_cc_resolve(c, name, ccache);
    if (retval != 0) {
        putil_err_krb5(args, retval,
                       "cannot create anonymous FAST credential cache %s",
                       name);
        goto done;
    }

    /* Obtain the credentials. */
    retval = krb5_get_init_creds_opt_alloc(c, &opts);
    if (retval != 0) {
        putil_err_krb5(args, retval, "cannot create FAST credential options");
        goto done;
    }
    krb5_get_init_creds_opt_set_anonymous(opts, 1);
    krb5_get_init_creds_opt_set_tkt_life(opts, 60);
#    ifdef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_OUT_CCACHE
    krb5_get_init_creds_opt_set_out_ccache(c, opts, *ccache);
#    endif
    retval = krb5_get_init_creds_password(c, &creds, princ, NULL, NULL, NULL,
                                          0, NULL, opts);
    if (retval != 0) {
        putil_debug_krb5(args, retval,
                         "cannot obtain anonymous credentials for FAST");
        goto done;
    }
    creds_valid = true;

    /*
     * If set_out_ccache was available, we're done.  Otherwise, we have to
     * manually set up the ticket cache.  Use the principal from the acquired
     * credentials when initializing the ticket cache, since the realm will
     * not match the realm of our input principal.
     */
#    ifndef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_OUT_CCACHE
    retval = krb5_cc_initialize(c, *ccache, creds.client);
    if (retval != 0) {
        putil_err_krb5(args, retval, "cannot initialize FAST ticket cache");
        goto done;
    }
    retval = krb5_cc_store_cred(c, *ccache, &creds);
    if (retval != 0) {
        putil_err_krb5(args, retval, "cannot store FAST credentials");
        goto done;
    }
#    endif /* !HAVE_KRB5_GET_INIT_CREDS_OPT_SET_OUT_CCACHE */

done:
    if (retval != 0 && *ccache != NULL) {
        krb5_cc_destroy(c, *ccache);
        *ccache = NULL;
    }
    if (princ != NULL)
        krb5_free_principal(c, princ);
    free(name);
    if (opts != NULL)
        krb5_get_init_creds_opt_free(c, opts);
    if (creds_valid)
        krb5_free_cred_contents(c, &creds);
    return retval;
}
#endif     /* HAVE_KRB5_GET_INIT_CREDS_OPT_SET_ANONYMOUS */


/*
 * Attempt to use an existing ticket cache for FAST.  Checks whether
 * fast_ccache is set in the options and, if so, opens that cache and does
 * some sanity checks, returning the cache name to use if everything checks
 * out in newly allocated memory.  Caller is responsible for freeing.  If not,
 * returns NULL.
 */
UNUSED static char *
fast_setup_cache(struct pam_args *args)
{
    krb5_context c = args->config->ctx->context;
    krb5_error_code retval;
    krb5_principal princ;
    krb5_ccache ccache;
    char *result;
    const char *cache = args->config->fast_ccache;

    if (cache == NULL)
        return NULL;
    retval = krb5_cc_resolve(c, cache, &ccache);
    if (retval != 0) {
        putil_debug_krb5(args, retval, "cannot open FAST ccache %s", cache);
        return NULL;
    }
    retval = krb5_cc_get_principal(c, ccache, &princ);
    if (retval != 0) {
        putil_debug_krb5(args, retval,
                         "failed to get principal from FAST"
                         " ccache %s",
                         cache);
        krb5_cc_close(c, ccache);
        return NULL;
    } else {
        krb5_free_principal(c, princ);
        krb5_cc_close(c, ccache);
        result = strdup(cache);
        if (result == NULL)
            putil_crit(args, "strdup failure: %s", strerror(errno));
        return result;
    }
}


/*
 * Attempt to use an anonymous ticket cache for FAST.  Checks whether
 * anon_fast is set in the options and, if so, opens that cache and does some
 * sanity checks, returning the cache name to use if everything checks out in
 * newly allocated memory.  Caller is responsible for freeing.  If not,
 * returns NULL.
 *
 * If successful, store the anonymous FAST cache in the context where it will
 * be freed following authentication.
 */
UNUSED static char *
fast_setup_anon(struct pam_args *args)
{
    krb5_context c = args->config->ctx->context;
    krb5_error_code retval;
    krb5_ccache ccache;
    char *cache, *result;

    if (!args->config->anon_fast)
        return NULL;
    retval = cache_init_anonymous(args, &ccache);
    if (retval != 0) {
        putil_debug_krb5(args, retval, "skipping anonymous FAST");
        return NULL;
    }
    retval = krb5_cc_get_full_name(c, ccache, &cache);
    if (retval != 0) {
        putil_debug_krb5(args, retval,
                         "cannot get name of anonymous FAST"
                         " credential cache");
        krb5_cc_destroy(c, ccache);
        return NULL;
    }
    result = strdup(cache);
    if (result == NULL) {
        putil_crit(args, "strdup failure: %s", strerror(errno));
        krb5_cc_destroy(c, ccache);
    }
    krb5_free_string(c, cache);
    putil_debug(args, "anonymous authentication for FAST succeeded");
    if (args->config->ctx->fast_cache != NULL)
        krb5_cc_destroy(c, args->config->ctx->fast_cache);
    args->config->ctx->fast_cache = ccache;
    return result;
}


/*
 * Set initial credential options for FAST if support is available.
 *
 * If fast_ccache is set, we try to use that ticket cache first.  Open it and
 * read the principal from it first to ensure that the cache exists and
 * contains credentials.  If that fails, skip setting the FAST cache.
 *
 * If anon_fast is set and fast_ccache is not or is skipped for the reasons
 * described above, try to obtain anonymous credentials and then use them as
 * FAST armor.
 *
 * Note that this function cannot fail.  If anything about FAST setup doesn't
 * work, we continue without FAST.
 */
#ifndef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_FAST_CCACHE_NAME

void
pamk5_fast_setup(struct pam_args *args UNUSED,
                 krb5_get_init_creds_opt *opts UNUSED)
{
}

#else /* HAVE_KRB5_GET_INIT_CREDS_OPT_SET_FAST_CCACHE_NAME */

void
pamk5_fast_setup(struct pam_args *args, krb5_get_init_creds_opt *opts)
{
    krb5_context c = args->config->ctx->context;
    krb5_error_code retval;
    char *cache;

    /* First try to use fast_ccache, and then fall back on anon_fast. */
    cache = fast_setup_cache(args);
    if (cache == NULL)
        cache = fast_setup_anon(args);
    if (cache == NULL)
        return;

    /* We have a valid FAST ticket cache.  Set the option. */
    retval = krb5_get_init_creds_opt_set_fast_ccache_name(c, opts, cache);
    if (retval != 0)
        putil_err_krb5(args, retval, "failed to set FAST ccache");
    else
        putil_debug(args, "setting FAST credential cache to %s", cache);
    free(cache);
}

#endif /* HAVE_KRB5_GET_INIT_CREDS_OPT_SET_FAST_CCACHE_NAME */
