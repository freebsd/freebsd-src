/*
 * Ticket creation routines for pam-krb5.
 *
 * pam_setcred and pam_open_session need to do similar but not identical work
 * to create the user's ticket cache.  The shared code is abstracted here into
 * the pamk5_setcred function.
 *
 * Copyright 2005-2009, 2014, 2017, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright 2005 Andres Salomon <dilinger@debian.org>
 * Copyright 1999-2000 Frank Cusack <fcusack@fcusack.com>
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

#include <config.h>
#include <portable/krb5.h>
#include <portable/pam.h>
#include <portable/system.h>

#include <assert.h>
#include <errno.h>
#include <pwd.h>

#include <module/internal.h>
#include <pam-util/args.h>
#include <pam-util/logging.h>


/*
 * Given a cache name and an existing cache, initialize a new cache, store the
 * credentials from the existing cache in it, and return a pointer to the new
 * cache in the cache argument.  Returns either PAM_SUCCESS or
 * PAM_SERVICE_ERR.
 */
static int
cache_init_from_cache(struct pam_args *args, const char *ccname,
                      krb5_ccache old, krb5_ccache *cache)
{
    struct context *ctx;
    krb5_creds creds;
    krb5_cc_cursor cursor;
    int pamret;
    krb5_error_code status;

    *cache = NULL;
    memset(&creds, 0, sizeof(creds));
    if (args == NULL || args->config == NULL || args->config->ctx == NULL
        || args->config->ctx->context == NULL)
        return PAM_SERVICE_ERR;
    if (old == NULL)
        return PAM_SERVICE_ERR;
    ctx = args->config->ctx;
    status = krb5_cc_start_seq_get(ctx->context, old, &cursor);
    if (status != 0) {
        putil_err_krb5(args, status, "cannot open new credentials");
        return PAM_SERVICE_ERR;
    }
    status = krb5_cc_next_cred(ctx->context, old, &cursor, &creds);
    if (status != 0) {
        putil_err_krb5(args, status, "cannot read new credentials");
        pamret = PAM_SERVICE_ERR;
        goto done;
    }
    pamret = pamk5_cache_init(args, ccname, &creds, cache);
    if (pamret != PAM_SUCCESS) {
        krb5_free_cred_contents(ctx->context, &creds);
        pamret = PAM_SERVICE_ERR;
        goto done;
    }
    krb5_free_cred_contents(ctx->context, &creds);

    /*
     * There probably won't be any additional credentials, but check for them
     * and copy them just in case.
     */
    while (krb5_cc_next_cred(ctx->context, old, &cursor, &creds) == 0) {
        status = krb5_cc_store_cred(ctx->context, *cache, &creds);
        krb5_free_cred_contents(ctx->context, &creds);
        if (status != 0) {
            putil_err_krb5(args, status,
                           "cannot store additional credentials"
                           " in %s",
                           ccname);
            pamret = PAM_SERVICE_ERR;
            goto done;
        }
    }
    pamret = PAM_SUCCESS;

done:
    krb5_cc_end_seq_get(ctx->context, ctx->cache, &cursor);
    if (pamret != PAM_SUCCESS && *cache != NULL) {
        krb5_cc_destroy(ctx->context, *cache);
        *cache = NULL;
    }
    return pamret;
}


/*
 * Determine the name of a new ticket cache.  Handles ccache and ccache_dir
 * PAM options and returns newly allocated memory.
 *
 * The ccache option, if set, contains a string with possible %u and %p
 * escapes.  The former is replaced by the UID and the latter is replaced by
 * the PID (a suitable unique string).
 */
static char *
build_ccache_name(struct pam_args *args, uid_t uid)
{
    char *cache_name = NULL;
    int retval;

    if (args->config->ccache == NULL) {
        retval = asprintf(&cache_name, "%s/krb5cc_%d_XXXXXX",
                          args->config->ccache_dir, (int) uid);
        if (retval < 0) {
            putil_crit(args, "malloc failure: %s", strerror(errno));
            return NULL;
        }
    } else {
        size_t len = 0, delta;
        char *p, *q;

        for (p = args->config->ccache; *p != '\0'; p++) {
            if (p[0] == '%' && p[1] == 'u') {
                len += snprintf(NULL, 0, "%ld", (long) uid);
                p++;
            } else if (p[0] == '%' && p[1] == 'p') {
                len += snprintf(NULL, 0, "%ld", (long) getpid());
                p++;
            } else {
                len++;
            }
        }
        len++;
        cache_name = malloc(len);
        if (cache_name == NULL) {
            putil_crit(args, "malloc failure: %s", strerror(errno));
            return NULL;
        }
        for (p = args->config->ccache, q = cache_name; *p != '\0'; p++) {
            if (p[0] == '%' && p[1] == 'u') {
                delta = snprintf(q, len, "%ld", (long) uid);
                q += delta;
                len -= delta;
                p++;
            } else if (p[0] == '%' && p[1] == 'p') {
                delta = snprintf(q, len, "%ld", (long) getpid());
                q += delta;
                len -= delta;
                p++;
            } else {
                *q = *p;
                q++;
                len--;
            }
        }
        *q = '\0';
    }
    return cache_name;
}


/*
 * Create a new context for a session if we've lost the context created during
 * authentication (such as when running under OpenSSH).  Return PAM_IGNORE if
 * we're ignoring this user or if apparently our pam_authenticate never
 * succeeded.
 */
static int
create_session_context(struct pam_args *args)
{
    struct context *ctx = NULL;
    PAM_CONST char *user;
    const char *tmpname;
    int status, pamret;

    /* If we're going to ignore the user anyway, don't even bother. */
    if (args->config->ignore_root || args->config->minimum_uid > 0) {
        pamret = pam_get_user(args->pamh, &user, NULL);
        if (pamret == PAM_SUCCESS && pamk5_should_ignore(args, user)) {
            pamret = PAM_IGNORE;
            goto fail;
        }
    }

    /*
     * Create the context and locate the temporary ticket cache.  Load the
     * ticket cache back into the context and flush out the other data that
     * would have been set if we'd kept our original context.
     */
    pamret = pamk5_context_new(args);
    if (pamret != PAM_SUCCESS) {
        putil_crit_pam(args, pamret, "creating session context failed");
        goto fail;
    }
    ctx = args->config->ctx;
    tmpname = pamk5_get_krb5ccname(args, "PAM_KRB5CCNAME");
    if (tmpname == NULL) {
        putil_debug(args, "unable to get PAM_KRB5CCNAME, assuming"
                          " non-Kerberos login");
        pamret = PAM_IGNORE;
        goto fail;
    }
    putil_debug(args, "found initial ticket cache at %s", tmpname);
    status = krb5_cc_resolve(ctx->context, tmpname, &ctx->cache);
    if (status != 0) {
        putil_err_krb5(args, status, "cannot resolve cache %s", tmpname);
        pamret = PAM_SERVICE_ERR;
        goto fail;
    }
    status = krb5_cc_get_principal(ctx->context, ctx->cache, &ctx->princ);
    if (status != 0) {
        putil_err_krb5(args, status, "cannot retrieve principal");
        pamret = PAM_SERVICE_ERR;
        goto fail;
    }

    /*
     * We've rebuilt the context.  Push it back into the PAM state for any
     * further calls to session or account management, which OpenSSH does keep
     * the context for.
     */
    pamret = pam_set_data(args->pamh, "pam_krb5", ctx, pamk5_context_destroy);
    if (pamret != PAM_SUCCESS) {
        putil_err_pam(args, pamret, "cannot set context data");
        goto fail;
    }
    return PAM_SUCCESS;

fail:
    pamk5_context_free(args);
    return pamret;
}


/*
 * Sets user credentials by creating the permanent ticket cache and setting
 * the proper ownership.  This function may be called by either pam_sm_setcred
 * or pam_sm_open_session.  The refresh flag should be set to true if we
 * should reinitialize an existing ticket cache instead of creating a new one.
 */
int
pamk5_setcred(struct pam_args *args, bool refresh)
{
    struct context *ctx = NULL;
    krb5_ccache cache = NULL;
    char *cache_name = NULL;
    bool set_context = false;
    int status = 0;
    int pamret;
    struct passwd *pw = NULL;
    uid_t uid;
    gid_t gid;

    /* If configured not to create a cache, we have nothing to do. */
    if (args->config->no_ccache) {
        pamret = PAM_SUCCESS;
        goto done;
    }

    /*
     * If we weren't able to obtain a context, we were probably run by OpenSSH
     * with its weird PAM handling, so we're going to cobble up a new context
     * for ourselves.
     */
    pamret = pamk5_context_fetch(args);
    if (pamret != PAM_SUCCESS) {
        putil_debug(args, "no context found, creating one");
        pamret = create_session_context(args);
        if (pamret != PAM_SUCCESS || args->config->ctx == NULL)
            goto done;
        set_context = true;
    }
    ctx = args->config->ctx;

    /*
     * Some programs (xdm, for instance) appear to call setcred over and over
     * again, so avoid doing useless work.
     */
    if (ctx->initialized) {
        pamret = PAM_SUCCESS;
        goto done;
    }

    /*
     * Get the uid.  The user is not required to be a local account for
     * pam_authenticate, but for either pam_setcred (other than DELETE) or for
     * pam_open_session, the user must be a local account.
     */
    pw = pam_modutil_getpwnam(args->pamh, ctx->name);
    if (pw == NULL) {
        putil_err(args, "getpwnam failed for %s", ctx->name);
        pamret = PAM_USER_UNKNOWN;
        goto done;
    }
    uid = pw->pw_uid;
    gid = pw->pw_gid;

    /* Get the cache name.  If reinitializing, this is our existing cache. */
    if (refresh) {
        const char *name, *k5name;

        /*
         * Solaris su calls pam_setcred as root with PAM_REINITIALIZE_CREDS,
         * preserving the user-supplied environment.  An xlock program may
         * also do this if it's setuid root and doesn't drop credentials
         * before calling pam_setcred.
         *
         * There isn't any safe way of reinitializing the exiting ticket cache
         * for the user if we're setuid without calling setreuid().  Calling
         * setreuid() is possible, but if the calling application is threaded,
         * it will change credentials for the whole application, with possibly
         * bizarre and unintended (and insecure) results.  Trying to verify
         * ownership of the existing ticket cache before using it fails under
         * various race conditions (for example, having one of the elements of
         * the path be a symlink and changing the target of that symlink
         * between our check and the call to krb5_cc_resolve).  Without
         * calling setreuid(), we run the risk of replacing a file owned by
         * another user with a credential cache.
         *
         * We could fail with an error in the setuid case, which would be
         * maximally safe, but it would prevent use of the module for
         * authentication with programs such as Solaris su.  Failure to
         * reinitialize the cache is normally not a serious problem, just a
         * missing feature.  We therefore log an error and exit with
         * PAM_SUCCESS for the setuid case.
         *
         * We do not use issetugid here since it always returns true if setuid
         * was was involved anywhere in the process of running the binary.
         * This would prevent a setuid screensaver that drops permissions from
         * refreshing a credential cache.  The issetugid behavior is safer,
         * since the environment should ideally not be trusted even if the
         * binary completely changed users away from the original user, but in
         * that case the binary needs to take some responsibility for either
         * sanitizing the environment or being certain that the calling user
         * is permitted to act as the target user.
         */
        if (getuid() != geteuid() || getgid() != getegid()) {
            putil_err(args, "credential reinitialization in a setuid context"
                            " ignored");
            pamret = PAM_SUCCESS;
            goto done;
        }
        name = pamk5_get_krb5ccname(args, "KRB5CCNAME");
        if (name == NULL)
            name = krb5_cc_default_name(ctx->context);
        if (name == NULL) {
            putil_err(args, "unable to get ticket cache name");
            pamret = PAM_SERVICE_ERR;
            goto done;
        }
        if (strncmp(name, "FILE:", strlen("FILE:")) == 0)
            name += strlen("FILE:");

        /*
         * If the cache we have in the context and the cache we're
         * reinitializing are the same cache, don't do anything; otherwise,
         * we'll end up destroying the cache.  This should never happen; this
         * case triggering is a sign of a bug, probably in the calling
         * application.
         */
        if (ctx->cache != NULL) {
            k5name = krb5_cc_get_name(ctx->context, ctx->cache);
            if (k5name != NULL) {
                if (strncmp(k5name, "FILE:", strlen("FILE:")) == 0)
                    k5name += strlen("FILE:");
                if (strcmp(name, k5name) == 0) {
                    pamret = PAM_SUCCESS;
                    goto done;
                }
            }
        }

        cache_name = strdup(name);
        if (cache_name == NULL) {
            putil_crit(args, "malloc failure: %s", strerror(errno));
            pamret = PAM_BUF_ERR;
            goto done;
        }
        putil_debug(args, "refreshing ticket cache %s", cache_name);

        /*
         * If we're refreshing the cache, we didn't really create it and the
         * user's open session created by login is probably still managing
         * it.  Thus, don't remove it when PAM is shut down.
         */
        ctx->dont_destroy_cache = 1;
    } else {
        char *cache_name_tmp;
        size_t len;

        cache_name = build_ccache_name(args, uid);
        if (cache_name == NULL) {
            pamret = PAM_BUF_ERR;
            goto done;
        }
        len = strlen(cache_name);
        if (len > 6 && strncmp("XXXXXX", cache_name + len - 6, 6) == 0) {
            if (strncmp(cache_name, "FILE:", strlen("FILE:")) == 0)
                cache_name_tmp = cache_name + strlen("FILE:");
            else
                cache_name_tmp = cache_name;
            pamret = pamk5_cache_mkstemp(args, cache_name_tmp);
            if (pamret != PAM_SUCCESS)
                goto done;
        }
        putil_debug(args, "initializing ticket cache %s", cache_name);
    }

    /*
     * Initialize the new ticket cache and point the environment at it.  Only
     * chown the cache if the cache is of type FILE or has no type (making the
     * assumption that the default cache type is FILE; otherwise, due to the
     * type prefix, we'd end up with an invalid path.
     */
    pamret = cache_init_from_cache(args, cache_name, ctx->cache, &cache);
    if (pamret != PAM_SUCCESS)
        goto done;
    if (strncmp(cache_name, "FILE:", strlen("FILE:")) == 0)
        status = chown(cache_name + strlen("FILE:"), uid, gid);
    else if (strchr(cache_name, ':') == NULL)
        status = chown(cache_name, uid, gid);
    if (status == -1) {
        putil_crit(args, "chown of ticket cache failed: %s", strerror(errno));
        pamret = PAM_SERVICE_ERR;
        goto done;
    }
    pamret = pamk5_set_krb5ccname(args, cache_name, "KRB5CCNAME");
    if (pamret != PAM_SUCCESS) {
        putil_crit(args, "setting KRB5CCNAME failed: %s", strerror(errno));
        goto done;
    }

    /*
     * If we had a temporary ticket cache, delete the environment variable so
     * that we won't get confused and think we still have a temporary ticket
     * cache when called again.
     *
     * FreeBSD PAM, at least as of 7.2, doesn't support deleting environment
     * variables using the syntax supported by Solaris and Linux.  Work
     * around that by setting the variable to an empty value if deleting it
     * fails.
     */
    if (pam_getenv(args->pamh, "PAM_KRB5CCNAME") != NULL) {
        pamret = pam_putenv(args->pamh, "PAM_KRB5CCNAME");
        if (pamret != PAM_SUCCESS)
            pamret = pam_putenv(args->pamh, "PAM_KRB5CCNAME=");
        if (pamret != PAM_SUCCESS)
            goto done;
    }

    /* Destroy the temporary cache and put the new cache in the context. */
    krb5_cc_destroy(ctx->context, ctx->cache);
    ctx->cache = cache;
    cache = NULL;
    ctx->initialized = 1;
    if (args->config->retain_after_close)
        ctx->dont_destroy_cache = 1;

done:
    if (ctx != NULL && cache != NULL)
        krb5_cc_destroy(ctx->context, cache);
    free(cache_name);

    /* If we stored our Kerberos context in PAM data, don't free it. */
    if (set_context)
        args->ctx = NULL;

    return pamret;
}
