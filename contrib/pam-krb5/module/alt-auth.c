/*
 * Support for alternate authentication mapping.
 *
 * pam-krb5 supports a feature where the principal for authentication can be
 * set via a PAM option and possibly based on the authenticating user.  This
 * can be used to, for example, require /root instances be used with sudo
 * while still using normal instances for other system authentications.
 *
 * This file collects all the pieces related to that support.
 *
 * Original support written by Booker Bense <bbense@slac.stanford.edu>
 * Further updates by Russ Allbery <eagle@eyrie.org>
 * Copyright 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2008-2012
 *     The Board of Trustees of the Leland Stanford Junior University
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
 * Map the user to a Kerberos principal according to alt_auth_map.  Returns 0
 * on success, storing the mapped principal name in newly allocated memory in
 * principal.  The caller is responsible for freeing.  Returns an errno value
 * on any error.
 */
int
pamk5_map_principal(struct pam_args *args, const char *username,
                    char **principal)
{
    char *realm;
    char *new_user = NULL;
    const char *user;
    const char *p;
    size_t needed, offset;
    int oerrno;

    /* Makes no sense if alt_auth_map isn't set. */
    if (args->config->alt_auth_map == NULL)
        return EINVAL;

    /* Need to split off the realm if it is present. */
    realm = strchr(username, '@');
    if (realm == NULL)
        user = username;
    else {
        new_user = strdup(username);
        if (new_user == NULL)
            return errno;
        realm = strchr(new_user, '@');
        if (realm == NULL)
            goto fail;
        *realm = '\0';
        realm++;
        user = new_user;
    }

    /* Now, allocate a string and build the principal. */
    needed = 0;
    for (p = args->config->alt_auth_map; *p != '\0'; p++) {
        if (p[0] == '%' && p[1] == 's') {
            needed += strlen(user);
            p++;
        } else {
            needed++;
        }
    }
    if (realm != NULL && strchr(args->config->alt_auth_map, '@') == NULL)
        needed += 1 + strlen(realm);
    needed++;
    *principal = malloc(needed);
    if (*principal == NULL)
        goto fail;
    offset = 0;
    for (p = args->config->alt_auth_map; *p != '\0'; p++) {
        if (p[0] == '%' && p[1] == 's') {
            memcpy(*principal + offset, user, strlen(user));
            offset += strlen(user);
            p++;
        } else {
            (*principal)[offset] = *p;
            offset++;
        }
    }
    if (realm != NULL && strchr(args->config->alt_auth_map, '@') == NULL) {
        (*principal)[offset] = '@';
        offset++;
        memcpy(*principal + offset, realm, strlen(realm));
        offset += strlen(realm);
    }
    (*principal)[offset] = '\0';
    free(new_user);
    return 0;

fail:
    if (new_user != NULL) {
        oerrno = errno;
        free(new_user);
        errno = oerrno;
    }
    return errno;
}


/*
 * Authenticate using an alternate principal mapping.
 *
 * Create a principal based on the principal mapping and the user, and use the
 * provided password to try to authenticate as that user.  If we succeed, fill
 * out creds, set princ to the successful principal in the context, and return
 * 0.  Otherwise, return a Kerberos error code or an errno value.
 */
krb5_error_code
pamk5_alt_auth(struct pam_args *args, const char *service,
               krb5_get_init_creds_opt *opts, const char *pass,
               krb5_creds *creds)
{
    struct context *ctx = args->config->ctx;
    char *kuser;
    krb5_principal princ;
    krb5_error_code retval;

    retval = pamk5_map_principal(args, ctx->name, &kuser);
    if (retval != 0)
        return retval;
    retval = krb5_parse_name(ctx->context, kuser, &princ);
    if (retval != 0) {
        free(kuser);
        return retval;
    }
    free(kuser);

    /* Log the principal we're attempting to authenticate as. */
    if (args->debug) {
        char *principal;

        retval = krb5_unparse_name(ctx->context, princ, &principal);
        if (retval != 0)
            putil_debug_krb5(args, retval, "krb5_unparse_name failed");
        else {
            putil_debug(args, "mapping %s to %s", ctx->name, principal);
            krb5_free_unparsed_name(ctx->context, principal);
        }
    }

    /*
     * Now, attempt to authenticate as that user.  On success, save the
     * principal.  Return the Kerberos status code.
     */
    retval = krb5_get_init_creds_password(ctx->context, creds, princ,
                                          (char *) pass, pamk5_prompter_krb5,
                                          args, 0, (char *) service, opts);
    if (retval != 0) {
        putil_debug_krb5(args, retval, "alternate authentication failed");
        krb5_free_principal(ctx->context, princ);
        return retval;
    } else {
        putil_debug(args, "alternate authentication successful");
        if (ctx->princ != NULL)
            krb5_free_principal(ctx->context, ctx->princ);
        ctx->princ = princ;
        return 0;
    }
}


/*
 * Verify an alternate authentication.
 *
 * Meant to be called from pamk5_authorized, this checks that the principal in
 * the context matches the alt_auth_map-derived identity of the user we're
 * authenticating.  Returns PAM_SUCCESS if they match, PAM_AUTH_ERR if they
 * don't match, and PAM_SERVICE_ERR on an internal error.
 */
int
pamk5_alt_auth_verify(struct pam_args *args)
{
    struct context *ctx;
    char *name = NULL;
    char *mapped = NULL;
    char *authed = NULL;
    krb5_principal princ = NULL;
    krb5_error_code retval;
    int status = PAM_SERVICE_ERR;

    if (args == NULL || args->config == NULL || args->config->ctx == NULL)
        return PAM_SERVICE_ERR;
    ctx = args->config->ctx;
    if (ctx->context == NULL || ctx->name == NULL)
        return PAM_SERVICE_ERR;
    if (pamk5_map_principal(args, ctx->name, &name) != 0) {
        putil_err(args, "cannot map principal name");
        goto done;
    }
    retval = krb5_parse_name(ctx->context, name, &princ);
    if (retval != 0) {
        putil_err_krb5(args, retval, "cannot parse mapped principal name %s",
                       mapped);
        goto done;
    }
    retval = krb5_unparse_name(ctx->context, princ, &mapped);
    if (retval != 0) {
        putil_err_krb5(args, retval,
                       "krb5_unparse_name on mapped principal failed");
        goto done;
    }
    retval = krb5_unparse_name(ctx->context, ctx->princ, &authed);
    if (retval != 0) {
        putil_err_krb5(args, retval, "krb5_unparse_name failed");
        goto done;
    }
    if (strcmp(authed, mapped) == 0)
        status = PAM_SUCCESS;
    else {
        putil_debug(args, "mapped user %s does not match principal %s", mapped,
                    authed);
        status = PAM_AUTH_ERR;
    }

done:
    free(name);
    if (authed != NULL)
        krb5_free_unparsed_name(ctx->context, authed);
    if (mapped != NULL)
        krb5_free_unparsed_name(ctx->context, mapped);
    if (princ != NULL)
        krb5_free_principal(ctx->context, princ);
    return status;
}
