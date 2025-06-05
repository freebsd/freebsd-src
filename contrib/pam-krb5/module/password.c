/*
 * Kerberos password changing.
 *
 * Copyright 2005-2009, 2020 Russ Allbery <eagle@eyrie.org>
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

#include <errno.h>

#include <module/internal.h>
#include <pam-util/args.h>
#include <pam-util/logging.h>


/*
 * Get the new password.  Store it in PAM_AUTHTOK if we obtain it and verify
 * it successfully and return it in the pass parameter.  If pass is set to
 * NULL, only store the new password in PAM_AUTHTOK.
 *
 * Returns a PAM error code, usually either PAM_AUTHTOK_ERR or PAM_SUCCESS.
 */
int
pamk5_password_prompt(struct pam_args *args, char **pass)
{
    int pamret = PAM_AUTHTOK_ERR;
    char *pass1 = NULL;
    char *pass2;
    PAM_CONST void *tmp;

    /* Use the password from a previous module, if so configured. */
    if (pass != NULL)
        *pass = NULL;
    if (args->config->use_authtok) {
        pamret = pam_get_item(args->pamh, PAM_AUTHTOK, &tmp);
        if (tmp == NULL) {
            putil_debug_pam(args, pamret, "no stored password");
            pamret = PAM_AUTHTOK_ERR;
            goto done;
        }
        if (strlen(tmp) > PAM_MAX_RESP_SIZE - 1) {
            putil_debug(args, "rejecting password longer than %d",
                        PAM_MAX_RESP_SIZE - 1);
            pamret = PAM_AUTHTOK_ERR;
            goto done;
        }
        pass1 = strdup((const char *) tmp);
    }

    /* Prompt for the new password if necessary. */
    if (pass1 == NULL) {
        pamret = pamk5_get_password(args, "Enter new", &pass1);
        if (pamret != PAM_SUCCESS) {
            putil_debug_pam(args, pamret, "error getting new password");
            pamret = PAM_AUTHTOK_ERR;
            goto done;
        }
        if (strlen(pass1) > PAM_MAX_RESP_SIZE - 1) {
            putil_debug(args, "rejecting password longer than %d",
                        PAM_MAX_RESP_SIZE - 1);
            pamret = PAM_AUTHTOK_ERR;
            explicit_bzero(pass1, strlen(pass1));
            free(pass1);
            goto done;
        }
        pamret = pamk5_get_password(args, "Retype new", &pass2);
        if (pamret != PAM_SUCCESS) {
            putil_debug_pam(args, pamret, "error getting new password");
            pamret = PAM_AUTHTOK_ERR;
            explicit_bzero(pass1, strlen(pass1));
            free(pass1);
            goto done;
        }
        if (strcmp(pass1, pass2) != 0) {
            putil_debug(args, "new passwords don't match");
            pamk5_conv(args, "Passwords don't match", PAM_ERROR_MSG, NULL);
            explicit_bzero(pass1, strlen(pass1));
            free(pass1);
            explicit_bzero(pass2, strlen(pass2));
            free(pass2);
            pamret = PAM_AUTHTOK_ERR;
            goto done;
        }
        explicit_bzero(pass2, strlen(pass2));
        free(pass2);

        /* Save the new password for other modules. */
        pamret = pam_set_item(args->pamh, PAM_AUTHTOK, pass1);
        if (pamret != PAM_SUCCESS) {
            putil_err_pam(args, pamret, "error storing password");
            pamret = PAM_AUTHTOK_ERR;
            explicit_bzero(pass1, strlen(pass1));
            free(pass1);
            goto done;
        }
    }
    if (pass != NULL)
        *pass = pass1;
    else {
        explicit_bzero(pass1, strlen(pass1));
        free(pass1);
    }

done:
    return pamret;
}


/*
 * We've obtained credentials for the password changing interface and gotten
 * the new password, so do the work of actually changing the password.
 */
static int
change_password(struct pam_args *args, const char *pass)
{
    struct context *ctx;
    int retval = PAM_SUCCESS;
    int result_code;
    krb5_data result_code_string, result_string;
    const char *message;

    /* Sanity check. */
    if (args == NULL || args->config == NULL || args->config->ctx == NULL
        || args->config->ctx->creds == NULL)
        return PAM_AUTHTOK_ERR;
    ctx = args->config->ctx;

    /*
     * The actual change.
     *
     * There are two password protocols in use: the change password protocol,
     * which doesn't allow specification of the principal, and the newer set
     * password protocol, which does.  For our purposes, either will do.
     *
     * Both Heimdal and MIT provide krb5_set_password.  With Heimdal,
     * krb5_change_password is deprecated and krb5_set_password tries both
     * protocols in turn, so will work with new and old servers.  With MIT,
     * krb5_set_password will use the old protocol if the principal is NULL
     * and the new protocol if it is not.
     *
     * We would like to just use krb5_set_password with a NULL principal
     * argument, but Heimdal 1.5 uses the default principal for the local user
     * rather than the principal from the credentials, so we need to pass in a
     * principal for Heimdal.  So we're stuck with an #ifdef.
     */
#ifdef HAVE_KRB5_MIT
    retval =
        krb5_set_password(ctx->context, ctx->creds, (char *) pass, NULL,
                          &result_code, &result_code_string, &result_string);
#else
    retval =
        krb5_set_password(ctx->context, ctx->creds, (char *) pass, ctx->princ,
                          &result_code, &result_code_string, &result_string);
#endif

    /* Everything from here on is just handling diagnostics and output. */
    if (retval != 0) {
        putil_debug_krb5(args, retval, "krb5_change_password failed");
        message = krb5_get_error_message(ctx->context, retval);
        pamk5_conv(args, message, PAM_ERROR_MSG, NULL);
        krb5_free_error_message(ctx->context, message);
        retval = PAM_AUTHTOK_ERR;
        goto done;
    }
    if (result_code != 0) {
        char *output;
        int status;

        putil_debug(args, "krb5_change_password: %s",
                    (char *) result_code_string.data);
        retval = PAM_AUTHTOK_ERR;
        status =
            asprintf(&output, "%.*s%s%.*s", (int) result_code_string.length,
                     (char *) result_code_string.data,
                     result_string.length == 0 ? "" : ": ",
                     (int) result_string.length, (char *) result_string.data);
        if (status < 0)
            putil_crit(args, "asprintf failed: %s", strerror(errno));
        else {
            pamk5_conv(args, output, PAM_ERROR_MSG, NULL);
            free(output);
        }
    }
    krb5_free_data_contents(ctx->context, &result_string);
    krb5_free_data_contents(ctx->context, &result_code_string);

done:
    /*
     * On failure, when clear_on_fail is set, we set the new password to NULL
     * so that subsequent password change PAM modules configured with
     * use_authtok will also fail.  Otherwise, since the order of the stack is
     * fixed once the pre-check function runs, subsequent modules would
     * continue even when we failed.
     */
    if (retval != PAM_SUCCESS && args->config->clear_on_fail) {
        if (pam_set_item(args->pamh, PAM_AUTHTOK, NULL))
            putil_err(args, "error clearing password");
    }
    return retval;
}


/*
 * Change a user's password.  Returns a PAM status code for success or
 * failure.  This does the work of pam_sm_chauthtok, but also needs to be
 * called from pam_sm_authenticate if we're working around a library that
 * can't handle password change during authentication.
 *
 * If the second argument is true, only do the authentication without actually
 * doing the password change (PAM_PRELIM_CHECK).
 */
int
pamk5_password_change(struct pam_args *args, bool only_auth)
{
    struct context *ctx = args->config->ctx;
    int pamret = PAM_SUCCESS;
    char *pass = NULL;

    /*
     * Authenticate to the password changing service using the old password.
     */
    if (ctx->creds == NULL) {
        pamret = pamk5_password_auth(args, "kadmin/changepw", &ctx->creds);
        if (pamret == PAM_SERVICE_ERR || pamret == PAM_AUTH_ERR)
            pamret = PAM_AUTHTOK_RECOVER_ERR;
        if (pamret != PAM_SUCCESS)
            goto done;
    }

    /*
     * Now, get the new password and change it unless we're just doing the
     * first check.
     */
    if (only_auth)
        goto done;
    pamret = pamk5_password_prompt(args, &pass);
    if (pamret != PAM_SUCCESS)
        goto done;
    pamret = change_password(args, pass);
    if (pamret == PAM_SUCCESS)
        pam_syslog(args->pamh, LOG_INFO, "user %s changed Kerberos password",
                   ctx->name);

done:
    if (pass != NULL) {
        explicit_bzero(pass, strlen(pass));
        free(pass);
    }
    return pamret;
}


/*
 * The function underlying the main PAM interface for password changing.
 * Performs preliminary checks, user notification, and any reauthentication
 * that's required.
 *
 * If the second argument is true, only do the authentication without actually
 * doing the password change (PAM_PRELIM_CHECK).
 */
int
pamk5_password(struct pam_args *args, bool only_auth)
{
    struct context *ctx = NULL;
    int pamret, status;
    PAM_CONST char *user;
    char *pass = NULL;
    bool set_context = false;

    /*
     * Check whether we should ignore this user.
     *
     * If we do ignore this user, and we're not in the preliminary check
     * phase, still prompt the user for the new password, but suppress our
     * banner.  This is a little strange, but it allows another module to be
     * stacked behind pam-krb5 with use_authtok and have it still work for
     * ignored users.
     *
     * We ignore the return status when prompting for the new password in this
     * case.  The worst thing that can happen is to fail to get the password,
     * in which case the other module will fail (or might even not care).
     */
    if (args->config->ignore_root || args->config->minimum_uid > 0) {
        status = pam_get_user(args->pamh, &user, NULL);
        if (status == PAM_SUCCESS && pamk5_should_ignore(args, user)) {
            if (!only_auth) {
                if (args->config->banner != NULL) {
                    free(args->config->banner);
                    args->config->banner = NULL;
                }
                pamk5_password_prompt(args, NULL);
            }
            pamret = PAM_IGNORE;
            goto done;
        }
    }

    /*
     * If we weren't able to find an existing context to use, we're going
     * into this fresh and need to create a new context.
     */
    if (args->config->ctx == NULL) {
        pamret = pamk5_context_new(args);
        if (pamret != PAM_SUCCESS) {
            putil_debug_pam(args, pamret, "creating context failed");
            pamret = PAM_AUTHTOK_ERR;
            goto done;
        }
        pamret = pam_set_data(args->pamh, "pam_krb5", args->config->ctx,
                              pamk5_context_destroy);
        if (pamret != PAM_SUCCESS) {
            putil_err_pam(args, pamret, "cannot set context data");
            pamret = PAM_AUTHTOK_ERR;
            goto done;
        }
        set_context = true;
    }
    ctx = args->config->ctx;

    /*
     * Tell the user what's going on if we're handling an expiration, but not
     * if we were configured to use the same password as an earlier module in
     * the stack.  The correct behavior here is not clear (what if the
     * Kerberos password expired but the other one didn't?), but warning
     * unconditionally leads to a strange message in the middle of doing the
     * password change.
     */
    if (ctx->expired && ctx->creds == NULL)
        if (!args->config->force_first_pass && !args->config->use_first_pass)
            pamk5_conv(args, "Password expired.  You must change it now.",
                       PAM_TEXT_INFO, NULL);

    /*
     * Do the password change.  This may only get tickets if we're doing the
     * preliminary check phase.
     */
    pamret = pamk5_password_change(args, only_auth);
    if (only_auth)
        goto done;

    /*
     * If we were handling a forced password change for an expired password,
     * now try to get a ticket cache with the new password.  If this succeeds,
     * clear the expired flag in the context.
     */
    if (pamret == PAM_SUCCESS && ctx->expired) {
        krb5_creds *creds = NULL;
        char *principal;
        krb5_error_code retval;

        putil_debug(args, "obtaining credentials with new password");
        args->config->force_first_pass = 1;
        pamret = pamk5_password_auth(args, NULL, &creds);
        if (pamret != PAM_SUCCESS)
            goto done;
        retval = krb5_unparse_name(ctx->context, ctx->princ, &principal);
        if (retval != 0) {
            putil_err_krb5(args, retval, "krb5_unparse_name failed");
            pam_syslog(args->pamh, LOG_INFO,
                       "user %s authenticated as UNKNOWN", ctx->name);
        } else {
            pam_syslog(args->pamh, LOG_INFO, "user %s authenticated as %s",
                       ctx->name, principal);
            krb5_free_unparsed_name(ctx->context, principal);
        }
        ctx->expired = false;
        pamret = pamk5_cache_init_random(args, creds);
        krb5_free_cred_contents(ctx->context, creds);
        free(creds);
    }

done:
    if (pass != NULL) {
        explicit_bzero(pass, strlen(pass));
        free(pass);
    }

    /*
     * Don't free our Kerberos context if we set a context, since the context
     * will take care of that.
     */
    if (set_context)
        args->ctx = NULL;

    if (pamret != PAM_SUCCESS) {
        if (pamret == PAM_SERVICE_ERR || pamret == PAM_AUTH_ERR)
            pamret = PAM_AUTHTOK_ERR;
        if (pamret == PAM_AUTHINFO_UNAVAIL)
            pamret = PAM_AUTHTOK_ERR;
    }
    return pamret;
}
