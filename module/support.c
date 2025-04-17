/*
 * Support functions for pam-krb5.
 *
 * Some general utility functions used by multiple PAM groups that aren't
 * associated with any particular chunk of functionality.
 *
 * Copyright 2005-2007, 2009, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011-2012
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

#include <errno.h>
#include <pwd.h>

#include <module/internal.h>
#include <pam-util/args.h>
#include <pam-util/logging.h>


/*
 * Given the PAM arguments and the user we're authenticating, see if we should
 * ignore that user because they're root or have a low-numbered UID and we
 * were configured to ignore such users.  Returns true if we should ignore
 * them, false otherwise.  Ignores any fully-qualified principal names.
 */
int
pamk5_should_ignore(struct pam_args *args, PAM_CONST char *username)
{
    struct passwd *pwd;

    if (args->config->ignore_root && strcmp("root", username) == 0) {
        putil_debug(args, "ignoring root user");
        return 1;
    }
    if (args->config->minimum_uid > 0 && strchr(username, '@') == NULL) {
        pwd = pam_modutil_getpwnam(args->pamh, username);
        if (pwd != NULL && pwd->pw_uid < (uid_t) args->config->minimum_uid) {
            putil_debug(args, "ignoring low-UID user (%lu < %ld)",
                        (unsigned long) pwd->pw_uid,
                        args->config->minimum_uid);
            return 1;
        }
    }
    return 0;
}


/*
 * Verify the user authorization.  Call krb5_kuserok if this is a local
 * account, or do the krb5_aname_to_localname verification if ignore_k5login
 * was requested.  For non-local accounts, the principal must match the
 * authentication identity.
 */
int
pamk5_authorized(struct pam_args *args)
{
    struct context *ctx;
    krb5_context c;
    krb5_error_code retval;
    int status;
    struct passwd *pwd;
    char kuser[65]; /* MAX_USERNAME == 65 (MIT Kerberos 1.4.1). */

    if (args == NULL || args->config == NULL || args->config->ctx == NULL
        || args->config->ctx->context == NULL)
        return PAM_SERVICE_ERR;
    ctx = args->config->ctx;
    if (ctx->name == NULL)
        return PAM_SERVICE_ERR;
    c = ctx->context;

    /*
     * If alt_auth_map was set, authorize the user if the authenticated
     * principal matches the mapped principal.  alt_auth_map essentially
     * serves as a supplemental .k5login.  PAM_SERVICE_ERR indicates fatal
     * errors that should abort remaining processing; PAM_AUTH_ERR indicates
     * that it just didn't match, in which case we continue to try other
     * authorization methods.
     */
    if (args->config->alt_auth_map != NULL) {
        status = pamk5_alt_auth_verify(args);
        if (status == PAM_SUCCESS || status == PAM_SERVICE_ERR)
            return status;
    }

    /*
     * If the name to which we're authenticating contains @ (is fully
     * qualified), it must match the principal exactly.
     */
    if (strchr(ctx->name, '@') != NULL) {
        char *principal;

        retval = krb5_unparse_name(c, ctx->princ, &principal);
        if (retval != 0) {
            putil_err_krb5(args, retval, "krb5_unparse_name failed");
            return PAM_SERVICE_ERR;
        }
        if (strcmp(principal, ctx->name) != 0) {
            putil_err(args, "user %s does not match principal %s", ctx->name,
                      principal);
            krb5_free_unparsed_name(c, principal);
            return PAM_AUTH_ERR;
        }
        krb5_free_unparsed_name(c, principal);
        return PAM_SUCCESS;
    }

    /*
     * Otherwise, apply either krb5_aname_to_localname or krb5_kuserok
     * depending on the situation.
     */
    pwd = pam_modutil_getpwnam(args->pamh, ctx->name);
    if (args->config->ignore_k5login || pwd == NULL) {
        retval = krb5_aname_to_localname(c, ctx->princ, sizeof(kuser), kuser);
        if (retval != 0) {
            putil_err_krb5(args, retval, "cannot convert principal to user");
            return PAM_AUTH_ERR;
        }
        if (strcmp(kuser, ctx->name) != 0) {
            putil_err(args, "user %s does not match local name %s", ctx->name,
                      kuser);
            return PAM_AUTH_ERR;
        }
    } else {
        if (!krb5_kuserok(c, ctx->princ, ctx->name)) {
            putil_err(args, "krb5_kuserok for user %s failed", ctx->name);
            return PAM_AUTH_ERR;
        }
    }

    return PAM_SUCCESS;
}
