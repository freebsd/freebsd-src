/*
 * Manage context structure.
 *
 * The context structure is the internal state maintained by the pam-krb5
 * module between calls to the various public interfaces.
 *
 * Copyright 2005-2009, 2014, 2020-2021 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright 2005 Andres Salomon <dilinger@debian.org>
 * Copyright 1999-2000 Frank Cusack <fcusack@fcusack.com>
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

#include <config.h>
#include <portable/pam.h>
#include <portable/system.h>

#include <errno.h>

#include <module/internal.h>
#include <pam-util/args.h>
#include <pam-util/logging.h>


/*
 * Create a new context and populate it with the user from PAM and the current
 * Kerberos context.  Set the default realm if one was configured.
 */
int
pamk5_context_new(struct pam_args *args)
{
    struct context *ctx;
    int retval;
    PAM_CONST char *name;

    ctx = calloc(1, sizeof(struct context));
    if (ctx == NULL) {
        retval = PAM_BUF_ERR;
        goto done;
    }
    ctx->cache = NULL;
    ctx->princ = NULL;
    ctx->creds = NULL;
    ctx->fast_cache = NULL;
    ctx->context = args->ctx;
    args->config->ctx = ctx;

    /*
     * This will prompt for the username if it's not already set (generally it
     * will be).  Otherwise, grab the saved username.
     */
    retval = pam_get_user(args->pamh, &name, NULL);
    if (retval != PAM_SUCCESS || name == NULL) {
        if (retval == PAM_CONV_AGAIN)
            retval = PAM_INCOMPLETE;
        else
            retval = PAM_SERVICE_ERR;
        goto done;
    }
    ctx->name = strdup(name);
    args->user = ctx->name;

    /* Set a default realm if one was configured. */
    if (args->realm != NULL) {
        retval = krb5_set_default_realm(ctx->context, args->realm);
        if (retval != 0) {
            putil_err_krb5(args, retval, "cannot set default realm");
            retval = PAM_SERVICE_ERR;
            goto done;
        }
    }

done:
    if (ctx != NULL && retval != PAM_SUCCESS)
        pamk5_context_free(args);
    return retval;
}


/*
 * Retrieve a context from the PAM data structures, returning failure if no
 * context was present.  Note that OpenSSH loses contexts between authenticate
 * and setcred, so failure shouldn't always be fatal.
 */
int
pamk5_context_fetch(struct pam_args *args)
{
    int pamret;

    pamret = pam_get_data(args->pamh, "pam_krb5", (void *) &args->config->ctx);
    if (pamret != PAM_SUCCESS)
        args->config->ctx = NULL;
    if (pamret == PAM_SUCCESS && args->config->ctx == NULL)
        return PAM_SERVICE_ERR;
    if (args->config->ctx != NULL)
        args->user = args->config->ctx->name;
    return pamret;
}


/*
 * Free a context and all of the data that's stored in it.  Normally this also
 * includes destroying the ticket cache, but don't do this (just close it) if
 * a flag was set to preserve it.
 *
 * This function is common code between pamk5_context_free (called internally
 * by our code) and pamk5_context_destroy (called by PAM as a data callback).
 */
static void
context_free(struct context *ctx, bool free_context)
{
    if (ctx == NULL)
        return;
    free(ctx->name);
    if (ctx->context != NULL) {
        if (ctx->princ != NULL)
            krb5_free_principal(ctx->context, ctx->princ);
        if (ctx->cache != NULL) {
            if (ctx->dont_destroy_cache)
                krb5_cc_close(ctx->context, ctx->cache);
            else
                krb5_cc_destroy(ctx->context, ctx->cache);
        }
        if (ctx->creds != NULL) {
            krb5_free_cred_contents(ctx->context, ctx->creds);
            free(ctx->creds);
        }
        if (free_context)
            krb5_free_context(ctx->context);
    }
    if (ctx->fast_cache != NULL)
        krb5_cc_destroy(ctx->context, ctx->fast_cache);
    free(ctx);
}


/*
 * Free the current context, used internally by pam-krb5 code.  This is a
 * wrapper around context_free that makes sure we don't destroy the Kerberos
 * context if it's the same as the top-level context and handles other
 * bookkeeping in the top-level pam_args struct.
 */
void
pamk5_context_free(struct pam_args *args)
{
    if (args->config->ctx == NULL)
        return;
    if (args->user == args->config->ctx->name)
        args->user = NULL;
    context_free(args->config->ctx, args->ctx != args->config->ctx->context);
    args->config->ctx = NULL;
}


/*
 * The PAM callback to destroy the context stored in the PAM data structures.
 */
void
pamk5_context_destroy(pam_handle_t *pamh UNUSED, void *data,
                      int pam_end_status)
{
    struct context *ctx = (struct context *) data;

    /*
     * Do not destroy the cache if the status contains PAM_DATA_SILENT, since
     * in that case we may be in a child and the parent will still rely on
     * underlying resources such as the ticket cache to exist.
     */
    if (PAM_DATA_SILENT != 0 && (pam_end_status & PAM_DATA_SILENT))
        ctx->dont_destroy_cache = true;

    /* The rest of the work is in context_free. */
    if (ctx != NULL)
        context_free(ctx, true);
}
