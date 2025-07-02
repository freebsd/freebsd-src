/*
 * Implements the PAM authorization function (pam_acct_mgmt).
 *
 * We don't have much to do for account management, but we do recheck the
 * user's authorization against .k5login (or whatever equivalent we've been
 * configured for).
 *
 * Copyright 2005-2009, 2014, 2020-2021 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright 2005 Andres Salomon <dilinger@debian.org>
 * Copyright 1999-2000 Frank Cusack <fcusack@fcusack.com>
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

/* Get prototypes for the account management functions. */
#define PAM_SM_ACCOUNT

#include <config.h>
#include <portable/krb5.h>
#include <portable/pam.h>
#include <portable/system.h>

#include <errno.h>

#include <module/internal.h>
#include <pam-util/args.h>
#include <pam-util/logging.h>


/*
 * Check the authorization of the user.  It's not entirely clear what this
 * function is supposed to do, but rechecking .k5login and friends makes the
 * most sense.
 */
int
pamk5_account(struct pam_args *args)
{
    struct context *ctx;
    int retval;
    const char *name;

    /* If the account was expired, here's where we actually fail. */
    ctx = args->config->ctx;
    if (ctx->expired) {
        pam_syslog(args->pamh, LOG_INFO, "user %s account password is expired",
                   ctx->name);
        return PAM_NEW_AUTHTOK_REQD;
    }

    /*
     * Re-retrieve the user rather than trusting our context; it's conceivable
     * the application could have changed it.  We have to cast &name due to
     * C's broken type system.
     *
     * Use pam_get_item rather than pam_get_user here since the user should be
     * set by the time we get to this point.  If we would have to prompt for a
     * user, something is definitely broken and we should fail.
     */
    retval = pam_get_item(args->pamh, PAM_USER, (PAM_CONST void **) &name);
    if (retval != PAM_SUCCESS || name == NULL) {
        putil_err_pam(args, retval, "unable to retrieve user");
        return PAM_AUTH_ERR;
    }
    if (ctx->name != name) {
        free(ctx->name);
        ctx->name = strdup(name);
        args->user = ctx->name;
    }

    /*
     * If we have a ticket cache, then we can apply an additional bit of
     * paranoia.  Rather than trusting princ in the context, extract the
     * principal from the Kerberos ticket cache we actually received and then
     * validate that.  This should make no difference in practice, but it's a
     * bit more thorough.
     */
    if (ctx->cache != NULL) {
        putil_debug(args, "retrieving principal from cache");
        if (ctx->princ != NULL) {
            krb5_free_principal(ctx->context, ctx->princ);
            ctx->princ = NULL;
        }
        retval = krb5_cc_get_principal(ctx->context, ctx->cache, &ctx->princ);
        if (retval != 0) {
            putil_err_krb5(args, retval, "cannot get principal from cache");
            return PAM_AUTH_ERR;
        }
    }
    return pamk5_authorized(args);
}
