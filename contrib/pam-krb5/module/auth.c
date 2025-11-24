/*
 * Core authentication routines for pam_krb5.
 *
 * The actual authentication work is done here, either via password or via
 * PKINIT.  The only external interface is pamk5_password_auth, which calls
 * the appropriate internal functions.  This interface is used by both the
 * authentication and the password groups.
 *
 * Copyright 2005-2010, 2014-2015, 2017, 2020
 *     Russ Allbery <eagle@eyrie.org>
 * Copyright 2010-2012, 2014
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
#ifdef HAVE_HX509_ERR_H
#    include <hx509_err.h>
#endif
#include <pwd.h>
#include <sys/stat.h>

#include <module/internal.h>
#include <pam-util/args.h>
#include <pam-util/logging.h>
#include <pam-util/vector.h>

/*
 * If the PKINIT smart card error statuses aren't defined, define them to 0.
 * This will cause the right thing to happen with the logic around PKINIT.
 */
#ifndef HX509_PKCS11_NO_TOKEN
#    define HX509_PKCS11_NO_TOKEN 0
#endif
#ifndef HX509_PKCS11_NO_SLOT
#    define HX509_PKCS11_NO_SLOT 0
#endif


/*
 * Fill in ctx->princ from the value of ctx->name or (if configured) from
 * prompting.  If we don't prompt and ctx->name contains an @-sign,
 * canonicalize it to a local account name unless no_update_user is set.  If
 * the canonicalization fails, don't worry about it.  It may be that the
 * application doesn't care.
 */
static krb5_error_code
parse_name(struct pam_args *args)
{
    struct context *ctx = args->config->ctx;
    krb5_context c = ctx->context;
    char *user_realm;
    char *user = ctx->name;
    char *newuser = NULL;
    char kuser[65] = ""; /* MAX_USERNAME == 65 (MIT Kerberos 1.4.1). */
    krb5_error_code k5_errno;
    int retval;

    /*
     * If configured to prompt for the principal, do that first.  Fall back on
     * using the local username as normal if prompting fails or if the user
     * just presses Enter.
     */
    if (args->config->prompt_principal) {
        retval = pamk5_conv(args, "Principal: ", PAM_PROMPT_ECHO_ON, &user);
        if (retval != PAM_SUCCESS)
            putil_err_pam(args, retval, "error getting principal");
        if (*user == '\0') {
            free(user);
            user = ctx->name;
        }
    }

    /*
     * We don't just call krb5_parse_name so that we can work around a bug in
     * MIT Kerberos versions prior to 1.4, which store the realm in a static
     * variable inside the library and don't notice changes.  If no realm is
     * specified and a realm is set in our arguments, append the realm to
     * force krb5_parse_name to do the right thing.
     */
    user_realm = args->realm;
    if (args->config->user_realm)
        user_realm = args->config->user_realm;
    if (user_realm != NULL && strchr(user, '@') == NULL) {
        if (asprintf(&newuser, "%s@%s", user, user_realm) < 0) {
            if (user != ctx->name)
                free(user);
            return KRB5_CC_NOMEM;
        }
        if (user != ctx->name)
            free(user);
        user = newuser;
    }
    k5_errno = krb5_parse_name(c, user, &ctx->princ);
    if (user != ctx->name)
        free(user);
    if (k5_errno != 0)
        return k5_errno;

    /*
     * Now that we have a principal to call krb5_aname_to_localname, we can
     * canonicalize ctx->name to a local name.  We do this even if we were
     * explicitly prompting for a principal, but we use ctx->name to generate
     * the local username, not the principal name.  It's unlikely, and would
     * be rather weird, if the user were to specify a principal name for the
     * username and then enter a different username at the principal prompt,
     * but this behavior seems to make the most sense.
     *
     * Skip canonicalization if no_update_user was set.  In that case,
     * continue to use the initial authentication identity everywhere.
     */
    if (strchr(ctx->name, '@') != NULL && !args->config->no_update_user) {
        if (krb5_aname_to_localname(c, ctx->princ, sizeof(kuser), kuser) != 0)
            return 0;
        user = strdup(kuser);
        if (user == NULL) {
            putil_crit(args, "cannot allocate memory: %s", strerror(errno));
            return 0;
        }
        free(ctx->name);
        ctx->name = user;
        args->user = user;
    }
    return k5_errno;
}


/*
 * Set initial credential options based on our configuration information, and
 * using the Heimdal call to set initial credential options if it's available.
 * This function is used both for regular password authentication and for
 * PKINIT.  It also configures FAST if requested and the Kerberos libraries
 * support it.
 *
 * Takes a flag indicating whether we're getting tickets for a specific
 * service.  If so, we don't try to get forwardable, renewable, or proxiable
 * tickets.
 */
static void
set_credential_options(struct pam_args *args, krb5_get_init_creds_opt *opts,
                       int service)
{
    struct pam_config *config = args->config;
    krb5_context c = config->ctx->context;

    krb5_get_init_creds_opt_set_default_flags(c, "pam", args->realm, opts);
    if (!service) {
        if (config->forwardable)
            krb5_get_init_creds_opt_set_forwardable(opts, 1);
        if (config->ticket_lifetime != 0)
            krb5_get_init_creds_opt_set_tkt_life(opts,
                                                 config->ticket_lifetime);
        if (config->renew_lifetime != 0)
            krb5_get_init_creds_opt_set_renew_life(opts,
                                                   config->renew_lifetime);
        krb5_get_init_creds_opt_set_change_password_prompt(
            opts, (config->defer_pwchange || config->fail_pwchange) ? 0 : 1);
    } else {
        krb5_get_init_creds_opt_set_forwardable(opts, 0);
        krb5_get_init_creds_opt_set_proxiable(opts, 0);
        krb5_get_init_creds_opt_set_renew_life(opts, 0);
    }
    pamk5_fast_setup(args, opts);

    /*
     * Set options for PKINIT.  Only used with MIT Kerberos; Heimdal's
     * implementation of PKINIT uses a separate API instead of setting
     * get_init_creds options.
     */
#ifdef HAVE_KRB5_GET_INIT_CREDS_OPT_SET_PA
    if (config->use_pkinit || config->try_pkinit) {
        if (config->pkinit_user != NULL)
            krb5_get_init_creds_opt_set_pa(c, opts, "X509_user_identity",
                                           config->pkinit_user);
        if (config->pkinit_anchors != NULL)
            krb5_get_init_creds_opt_set_pa(c, opts, "X509_anchors",
                                           config->pkinit_anchors);
        if (config->preauth_opt != NULL && config->preauth_opt->count > 0) {
            size_t i;
            char *name, *value;
            char save = '\0';

            for (i = 0; i < config->preauth_opt->count; i++) {
                name = config->preauth_opt->strings[i];
                if (name == NULL)
                    continue;
                value = strchr(name, '=');
                if (value != NULL) {
                    save = *value;
                    *value = '\0';
                    value++;
                }
                krb5_get_init_creds_opt_set_pa(
                    c, opts, name, (value != NULL) ? value : "yes");
                if (value != NULL)
                    value[-1] = save;
            }
        }
    }
#endif /* HAVE_KRB5_GET_INIT_CREDS_OPT_SET_PA */
}


/*
 * Retrieve the existing password (authtok) stored in the PAM data if
 * appropriate and if available.  We decide whether to retrieve it based on
 * the PAM configuration, and also decied whether failing to retrieve it is a
 * fatal error.  Takes the PAM arguments, the PAM authtok code to retrieve
 * (may be PAM_AUTHTOK or PAM_OLDAUTHTOK depending on whether we're
 * authenticating or changing the password), and the place to store the
 * password.  Returns a PAM status code.
 *
 * If try_first_pass, use_first_pass, or force_first_pass is set, grab the old
 * password (if set).  If force_first_pass is set, fail if the password is not
 * already set.
 *
 * The empty password has to be handled separately, since the Kerberos
 * libraries may treat it as equivalent to no password and prompt when we
 * don't want them to.  We make the assumption here that the empty password is
 * always invalid and is an authentication failure.
 */
static int
maybe_retrieve_password(struct pam_args *args, int authtok, const char **pass)
{
    int status;
    const bool try_first = args->config->try_first_pass;
    const bool use = args->config->use_first_pass;
    const bool force = args->config->force_first_pass;

    *pass = NULL;
    if (!try_first && !use && !force)
        return PAM_SUCCESS;
    status = pam_get_item(args->pamh, authtok, (PAM_CONST void **) pass);
    if (*pass != NULL && **pass == '\0') {
        if (use || force) {
            putil_debug(args, "rejecting empty password");
            return PAM_AUTH_ERR;
        }
        *pass = NULL;
    }
    if (*pass != NULL && strlen(*pass) > PAM_MAX_RESP_SIZE - 1) {
        putil_debug(args, "rejecting password longer than %d",
                    PAM_MAX_RESP_SIZE - 1);
        return PAM_AUTH_ERR;
    }
    if (force && (status != PAM_SUCCESS || *pass == NULL)) {
        putil_debug_pam(args, status, "no stored password");
        return PAM_AUTH_ERR;
    }
    return PAM_SUCCESS;
}


/*
 * Prompt for the password.  Takes the PAM arguments, the authtok for which
 * we're prompting (may be PAM_AUTHTOK or PAM_OLDAUTHTOK depending on whether
 * we're authenticating or changing the password), and the place to store the
 * password.  Returns a PAM status code.
 *
 * If we successfully get a password, store it in the PAM data, free it, and
 * then return the password as retrieved from the PAM data so that we don't
 * have to worry about memory allocation later.
 *
 * The empty password has to be handled separately, since the Kerberos
 * libraries may treat it as equivalent to no password and prompt when we
 * don't want them to.  We make the assumption here that the empty password is
 * always invalid and is an authentication failure.
 */
static int
prompt_password(struct pam_args *args, int authtok, const char **pass)
{
    char *password;
    int status;
    const char *prompt = (authtok == PAM_AUTHTOK) ? NULL : "Current";

    *pass = NULL;
    status = pamk5_get_password(args, prompt, &password);
    if (status != PAM_SUCCESS) {
        putil_debug_pam(args, status, "error getting password");
        return PAM_AUTH_ERR;
    }
    if (password[0] == '\0') {
        putil_debug(args, "rejecting empty password");
        free(password);
        return PAM_AUTH_ERR;
    }
    if (strlen(password) > PAM_MAX_RESP_SIZE - 1) {
        putil_debug(args, "rejecting password longer than %d",
                    PAM_MAX_RESP_SIZE - 1);
        explicit_bzero(password, strlen(password));
        free(password);
        return PAM_AUTH_ERR;
    }

    /* Set this for the next PAM module. */
    status = pam_set_item(args->pamh, authtok, password);
    explicit_bzero(password, strlen(password));
    free(password);
    if (status != PAM_SUCCESS) {
        putil_err_pam(args, status, "error storing password");
        return PAM_AUTH_ERR;
    }

    /* Return the password retrieved from PAM. */
    status = pam_get_item(args->pamh, authtok, (PAM_CONST void **) pass);
    if (status != PAM_SUCCESS) {
        putil_err_pam(args, status, "error retrieving password");
        status = PAM_AUTH_ERR;
    }
    return status;
}


/*
 * Authenticate via password.
 *
 * This is our basic authentication function.  Log what principal we're
 * attempting to authenticate with and then attempt password authentication.
 * Returns 0 on success or a Kerberos error on failure.
 */
static krb5_error_code
password_auth(struct pam_args *args, krb5_creds *creds,
              krb5_get_init_creds_opt *opts, const char *service,
              const char *pass)
{
    struct context *ctx = args->config->ctx;
    krb5_error_code retval;

    /* Log the principal as which we're attempting authentication. */
    if (args->debug) {
        char *principal;

        retval = krb5_unparse_name(ctx->context, ctx->princ, &principal);
        if (retval != 0)
            putil_debug_krb5(args, retval, "krb5_unparse_name failed");
        else {
            if (service == NULL)
                putil_debug(args, "attempting authentication as %s",
                            principal);
            else
                putil_debug(args, "attempting authentication as %s for %s",
                            principal, service);
            free(principal);
        }
    }

    /* Do the authentication. */
    retval = krb5_get_init_creds_password(ctx->context, creds, ctx->princ,
                                          (char *) pass, pamk5_prompter_krb5,
                                          args, 0, (char *) service, opts);

    /*
     * Heimdal may return an expired key error even if the password is
     * incorrect.  To avoid accepting any incorrect password for the user
     * in the fully correct password change case, confirm that we can get
     * a password change ticket for the user using this password, and
     * otherwise change the error to invalid password.
     */
    if (retval == KRB5KDC_ERR_KEY_EXP) {
        krb5_get_init_creds_opt *heimdal_opts = NULL;

        retval = krb5_get_init_creds_opt_alloc(ctx->context, &heimdal_opts);
        if (retval == 0) {
            set_credential_options(args, opts, 1);
            retval = krb5_get_init_creds_password(
                ctx->context, creds, ctx->princ, (char *) pass,
                pamk5_prompter_krb5, args, 0, (char *) "kadmin/changepw",
                heimdal_opts);
            krb5_get_init_creds_opt_free(ctx->context, heimdal_opts);
        }
        if (retval == 0) {
            retval = KRB5KDC_ERR_KEY_EXP;
            krb5_free_cred_contents(ctx->context, creds);
            explicit_bzero(creds, sizeof(krb5_creds));
        }
    }
    return retval;
}


/*
 * Authenticate by trying each principal in the .k5login file.
 *
 * Read through each line that parses correctly as a principal and use the
 * provided password to try to authenticate as that user.  If at any point we
 * succeed, fill out creds, set princ to the successful principal in the
 * context, and return 0.  Otherwise, return either a Kerberos error code or
 * errno for a system error.
 */
static krb5_error_code
k5login_password_auth(struct pam_args *args, krb5_creds *creds,
                      krb5_get_init_creds_opt *opts, const char *service,
                      const char *pass)
{
    struct context *ctx = args->config->ctx;
    char *filename = NULL;
    char line[BUFSIZ];
    size_t len;
    FILE *k5login;
    struct passwd *pwd;
    struct stat st;
    krb5_error_code k5_errno, retval;
    krb5_principal princ;

    /*
     * C sucks at string manipulation.  Generate the filename for the user's
     * .k5login file.  If the user doesn't exist, the .k5login file doesn't
     * exist, or the .k5login file cannot be read, fall back on the easy way
     * and assume ctx->princ is already set properly.
     */
    pwd = pam_modutil_getpwnam(args->pamh, ctx->name);
    if (pwd != NULL)
        if (asprintf(&filename, "%s/.k5login", pwd->pw_dir) < 0) {
            putil_crit(args, "malloc failure: %s", strerror(errno));
            return errno;
        }
    if (pwd == NULL || filename == NULL || access(filename, R_OK) != 0) {
        free(filename);
        return krb5_get_init_creds_password(ctx->context, creds, ctx->princ,
                                            (char *) pass, pamk5_prompter_krb5,
                                            args, 0, (char *) service, opts);
    }

    /*
     * Make sure the ownership on .k5login is okay.  The user must own their
     * own .k5login or it must be owned by root.  If that fails, set the
     * Kerberos error code to errno.
     */
    k5login = fopen(filename, "r");
    if (k5login == NULL) {
        retval = errno;
        free(filename);
        return retval;
    }
    free(filename);
    if (fstat(fileno(k5login), &st) != 0) {
        retval = errno;
        goto fail;
    }
    if (st.st_uid != 0 && (st.st_uid != pwd->pw_uid)) {
        retval = EACCES;
        putil_err(args, "unsafe .k5login ownership (saw %lu, expected %lu)",
                  (unsigned long) st.st_uid, (unsigned long) pwd->pw_uid);
        goto fail;
    }

    /*
     * Parse the .k5login file and attempt authentication for each principal.
     * Ignore any lines that are too long or that don't parse into a Kerberos
     * principal.  Assume an invalid password error if there are no valid
     * lines in .k5login.
     */
    retval = KRB5KRB_AP_ERR_BAD_INTEGRITY;
    while (fgets(line, BUFSIZ, k5login) != NULL) {
        len = strlen(line);
        if (line[len - 1] != '\n') {
            while (fgets(line, BUFSIZ, k5login) != NULL) {
                len = strlen(line);
                if (line[len - 1] == '\n')
                    break;
            }
            continue;
        }
        line[len - 1] = '\0';
        k5_errno = krb5_parse_name(ctx->context, line, &princ);
        if (k5_errno != 0)
            continue;

        /* Now, attempt to authenticate as that user. */
        if (service == NULL)
            putil_debug(args, "attempting authentication as %s", line);
        else
            putil_debug(args, "attempting authentication as %s for %s", line,
                        service);
        retval = krb5_get_init_creds_password(
            ctx->context, creds, princ, (char *) pass, pamk5_prompter_krb5,
            args, 0, (char *) service, opts);

        /*
         * If that worked, update ctx->princ and return success.  Otherwise,
         * continue on to the next line.
         */
        if (retval == 0) {
            if (ctx->princ != NULL)
                krb5_free_principal(ctx->context, ctx->princ);
            ctx->princ = princ;
            fclose(k5login);
            return 0;
        }
        krb5_free_principal(ctx->context, princ);
    }

fail:
    fclose(k5login);
    return retval;
}


#if (defined(HAVE_KRB5_HEIMDAL)                           \
     && defined(HAVE_KRB5_GET_INIT_CREDS_OPT_SET_PKINIT)) \
    || defined(HAVE_KRB5_GET_PROMPT_TYPES)
/*
 * Attempt authentication via PKINIT.  Currently, this uses an API specific to
 * Heimdal.  Once MIT Kerberos supports PKINIT, some of the details may need
 * to move into the compat layer.
 *
 * Some smart card readers require the user to enter the PIN at the keyboard
 * after inserting the smart card.  Others have a pad on the card and no
 * prompting by PAM is required.  The Kerberos library prompting functions
 * should be able to work out which is required.
 *
 * PKINIT is just one of many pre-authentication mechanisms that could be
 * used.  It's handled separately because of possible smart card interactions
 * and the possibility that some users may be authenticated via PKINIT and
 * others may not.
 *
 * Takes the same arguments as pamk5_password_auth and returns a
 * krb5_error_code.  If successful, the credentials will be stored in creds.
 */
static krb5_error_code
pkinit_auth(struct pam_args *args, const char *service, krb5_creds **creds)
{
    struct context *ctx = args->config->ctx;
    krb5_get_init_creds_opt *opts = NULL;
    krb5_error_code retval;
    char *dummy = NULL;

    /*
     * We may not be able to dive directly into the PKINIT functions because
     * the user may not have a chance to enter the smart card.  For example,
     * gnome-screensaver jumps into PAM as soon as the mouse is moved and
     * expects to be prompted for a password, which may not happen if the
     * smart card is the type that has a pad for the PIN on the card.
     *
     * Allow the user to set pkinit_prompt as an option.  If set, we tell the
     * user they need to insert the card.
     *
     * We always ignore the input.  If the user wants to use a password
     * instead, they'll be prompted later when the PKINIT code discovers that
     * no smart card is available.
     */
    if (args->config->pkinit_prompt) {
        pamk5_conv(args,
                   args->config->use_pkinit
                       ? "Insert smart card and press Enter: "
                       : "Insert smart card if desired, then press Enter: ",
                   PAM_PROMPT_ECHO_OFF, &dummy);
    }

    /*
     * Set credential options.  We have to use the allocated version of the
     * credential option struct to store the PKINIT options.
     */
    *creds = calloc(1, sizeof(krb5_creds));
    if (*creds == NULL)
        return ENOMEM;
    retval = krb5_get_init_creds_opt_alloc(ctx->context, &opts);
    if (retval != 0)
        return retval;
    set_credential_options(args, opts, service != NULL);

    /* Finally, do the actual work and return the results. */
#    ifdef HAVE_KRB5_HEIMDAL
    retval = krb5_get_init_creds_opt_set_pkinit(
        ctx->context, opts, ctx->princ, args->config->pkinit_user,
        args->config->pkinit_anchors, NULL, NULL, 0, pamk5_prompter_krb5, args,
        NULL);
    if (retval == 0)
        retval = krb5_get_init_creds_password(ctx->context, *creds, ctx->princ,
                                              NULL, NULL, args, 0,
                                              (char *) service, opts);
#    else  /* !HAVE_KRB5_HEIMDAL */
    retval = krb5_get_init_creds_password(
        ctx->context, *creds, ctx->princ, NULL,
        pamk5_prompter_krb5_no_password, args, 0, (char *) service, opts);
#    endif /* !HAVE_KRB5_HEIMDAL */

    krb5_get_init_creds_opt_free(ctx->context, opts);
    if (retval != 0) {
        krb5_free_cred_contents(ctx->context, *creds);
        free(*creds);
        *creds = NULL;
    }
    return retval;
}
#endif


/*
 * Attempt authentication once with a given password.  This is the core of the
 * authentication loop, and handles alt_auth_map and search_k5login.  It takes
 * the PAM arguments, the service for which to get tickets (NULL for the
 * default TGT), the initial credential options, and the password, and returns
 * a Kerberos status code or errno.  On success (return status 0), it stores
 * the obtained credentials in the provided creds argument.
 */
static krb5_error_code
password_auth_attempt(struct pam_args *args, const char *service,
                      krb5_get_init_creds_opt *opts, const char *pass,
                      krb5_creds *creds)
{
    krb5_error_code retval;

    /*
     * First, try authenticating as the alternate principal if one were
     * configured.  If that fails or wasn't configured, continue on to trying
     * search_k5login or a regular authentication unless configuration
     * indicates that regular authentication should not be attempted.
     */
    if (args->config->alt_auth_map != NULL) {
        retval = pamk5_alt_auth(args, service, opts, pass, creds);
        if (retval == 0)
            return retval;

        /* If only_alt_auth is set, we cannot continue. */
        if (args->config->only_alt_auth)
            return retval;

        /*
         * If force_alt_auth is set, skip attempting normal authentication iff
         * the alternate principal exists.
         */
        if (args->config->force_alt_auth)
            if (retval != KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN)
                return retval;
    }

    /* Attempt regular authentication, via either search_k5login or normal. */
    if (args->config->search_k5login)
        retval = k5login_password_auth(args, creds, opts, service, pass);
    else
        retval = password_auth(args, creds, opts, service, pass);
    if (retval != 0)
        putil_debug_krb5(args, retval, "krb5_get_init_creds_password");
    return retval;
}


/*
 * Try to verify credentials by obtaining and checking a service ticket.  This
 * is required to verify that no one is spoofing the KDC, but requires read
 * access to a keytab with a valid key.  By default, the Kerberos library will
 * silently succeed if no verification keys are available, but the user can
 * change this by setting verify_ap_req_nofail in [libdefaults] in
 * /etc/krb5.conf.
 *
 * The MIT Kerberos implementation of krb5_verify_init_creds hardwires the
 * host key for the local system as the desired principal if no principal is
 * given.  If we have an explicitly configured keytab, instead read that
 * keytab, find the first principal in that keytab, and use that.
 *
 * Returns a Kerberos status code (0 for success).
 */
static krb5_error_code
verify_creds(struct pam_args *args, krb5_creds *creds)
{
    krb5_verify_init_creds_opt opts;
    krb5_keytab keytab = NULL;
    krb5_kt_cursor cursor;
    int cursor_valid = 0;
    krb5_keytab_entry entry;
    krb5_principal princ = NULL;
    krb5_error_code retval;
    krb5_context c = args->config->ctx->context;

    memset(&entry, 0, sizeof(entry));
    krb5_verify_init_creds_opt_init(&opts);
    if (args->config->keytab) {
        retval = krb5_kt_resolve(c, args->config->keytab, &keytab);
        if (retval != 0) {
            putil_err_krb5(args, retval, "cannot open keytab %s",
                           args->config->keytab);
            keytab = NULL;
        }
        if (retval == 0)
            retval = krb5_kt_start_seq_get(c, keytab, &cursor);
        if (retval == 0) {
            cursor_valid = 1;
            retval = krb5_kt_next_entry(c, keytab, &entry, &cursor);
        }
        if (retval == 0)
            retval = krb5_copy_principal(c, entry.principal, &princ);
        if (retval != 0)
            putil_err_krb5(args, retval, "error reading keytab %s",
                           args->config->keytab);
        if (entry.principal != NULL)
            krb5_kt_free_entry(c, &entry);
        if (cursor_valid)
            krb5_kt_end_seq_get(c, keytab, &cursor);
    }
#ifdef __FreeBSD__
    if (args->config->allow_kdc_spoof)
	opts.flags &= ~KRB5_VERIFY_INIT_CREDS_OPT_AP_REQ_NOFAIL;
    else
	opts.flags |= KRB5_VERIFY_INIT_CREDS_OPT_AP_REQ_NOFAIL;
#endif /* __FreeBSD__ */
    retval = krb5_verify_init_creds(c, creds, princ, keytab, NULL, &opts);
    if (retval != 0)
        putil_err_krb5(args, retval, "credential verification failed");
    if (princ != NULL)
        krb5_free_principal(c, princ);
    if (keytab != NULL)
        krb5_kt_close(c, keytab);
    return retval;
}


/*
 * Give the user a nicer error message when we've attempted PKINIT without
 * success.  We can only do this if the rich status codes are available.
 * Currently, this only works with Heimdal.
 */
static void UNUSED
report_pkinit_error(struct pam_args *args, krb5_error_code retval UNUSED)
{
    const char *message;

#ifdef HAVE_HX509_ERR_H
    switch (retval) {
#    ifdef HX509_PKCS11_PIN_LOCKED
    case HX509_PKCS11_PIN_LOCKED:
        message = "PKINIT failed: user PIN locked";
        break;
#    endif
#    ifdef HX509_PKCS11_PIN_EXPIRED
    case HX509_PKCS11_PIN_EXPIRED:
        message = "PKINIT failed: user PIN expired";
        break;
#    endif
#    ifdef HX509_PKCS11_PIN_INCORRECT
    case HX509_PKCS11_PIN_INCORRECT:
        message = "PKINIT failed: user PIN incorrect";
        break;
#    endif
#    ifdef HX509_PKCS11_PIN_NOT_INITIALIZED
    case HX509_PKCS11_PIN_NOT_INITIALIZED:
        message = "PKINIT fialed: user PIN not initialized";
        break;
#    endif
    default:
        message = "PKINIT failed";
        break;
    }
#else
    message = "PKINIT failed";
#endif
    pamk5_conv(args, message, PAM_TEXT_INFO, NULL);
}


/*
 * Prompt the user for a password and authenticate the password with the KDC.
 * If correct, fill in creds with the obtained TGT or ticket.  service, if
 * non-NULL, specifies the service to get tickets for; the only interesting
 * non-null case is kadmin/changepw for changing passwords.  Therefore, if it
 * is non-null, we look for the password in PAM_OLDAUTHOK and save it there
 * instead of using PAM_AUTHTOK.
 */
int
pamk5_password_auth(struct pam_args *args, const char *service,
                    krb5_creds **creds)
{
    struct context *ctx;
    krb5_get_init_creds_opt *opts = NULL;
    krb5_error_code retval = 0;
    int status = PAM_SUCCESS;
    bool retry, prompt;
    bool creds_valid = false;
    const char *pass = NULL;
    int authtok = (service == NULL) ? PAM_AUTHTOK : PAM_OLDAUTHTOK;

    /* Sanity check and initialization. */
    if (args->config->ctx == NULL)
        return PAM_SERVICE_ERR;
    ctx = args->config->ctx;

    /*
     * Fill in the default principal to authenticate as.  alt_auth_map or
     * search_k5login may change this later.
     */
    if (ctx->princ == NULL) {
        retval = parse_name(args);
        if (retval != 0) {
            putil_err_krb5(args, retval, "parse_name failed");
            return PAM_SERVICE_ERR;
        }
    }

    /*
     * If PKINIT is available and we were configured to attempt it, try
     * authenticating with PKINIT first.  Otherwise, fail all authentication
     * if PKINIT is not available and use_pkinit was set.  Fake an error code
     * that gives an approximately correct error message.
     */
#if defined(HAVE_KRB5_HEIMDAL) \
    && defined(HAVE_KRB5_GET_INIT_CREDS_OPT_SET_PKINIT)
    if (args->config->use_pkinit || args->config->try_pkinit) {
        retval = pkinit_auth(args, service, creds);
        if (retval == 0)
            goto verify;
        putil_debug_krb5(args, retval, "PKINIT failed");
        if (retval != HX509_PKCS11_NO_TOKEN && retval != HX509_PKCS11_NO_SLOT)
            goto done;
        if (retval != 0) {
            report_pkinit_error(args, retval);
            if (args->config->use_pkinit)
                goto done;
        }
    }
#elif defined(HAVE_KRB5_GET_PROMPT_TYPES)
    if (args->config->use_pkinit) {
        retval = pkinit_auth(args, service, creds);
        if (retval == 0)
            goto verify;
        putil_debug_krb5(args, retval, "PKINIT failed");
        report_pkinit_error(args, retval);
        goto done;
    }
#endif

    /* Allocate cred structure and set credential options. */
    *creds = calloc(1, sizeof(krb5_creds));
    if (*creds == NULL) {
        putil_crit(args, "cannot allocate memory: %s", strerror(errno));
        status = PAM_SERVICE_ERR;
        goto done;
    }
    retval = krb5_get_init_creds_opt_alloc(ctx->context, &opts);
    if (retval != 0) {
        putil_crit_krb5(args, retval, "cannot allocate credential options");
        goto done;
    }
    set_credential_options(args, opts, service != NULL);

    /*
     * Obtain the saved password, if appropriate and available, and determine
     * our retry strategy.  If try_first_pass is set, we will prompt for a
     * password and retry the authentication if the stored password didn't
     * work.
     */
    status = maybe_retrieve_password(args, authtok, &pass);
    if (status != PAM_SUCCESS)
        goto done;

    /*
     * Main authentication loop.
     *
     * If we had no stored password, we prompt for a password the first time
     * through.  If try_first_pass is set and we had an old password, we try
     * with it.  If the old password doesn't work, we loop once, prompt for a
     * password, and retry.  If use_first_pass is set, we'll prompt once if
     * the password isn't already set but won't retry.
     *
     * If we don't have a password but try_pkinit or no_prompt are true, we
     * don't attempt to prompt for a password and we go into the Kerberos
     * libraries with no password.  We rely on the Kerberos libraries to do
     * the prompting if PKINIT fails.  In this case, make sure we don't retry.
     * Be aware that in this case, we also have no way of saving whatever
     * password or other credentials the user might enter, so subsequent PAM
     * modules will not see a stored authtok.
     *
     * We've already handled empty passwords in our other functions.
     */
    retry = args->config->try_first_pass;
    prompt = !(args->config->try_pkinit || args->config->no_prompt);
    do {
        if (pass == NULL)
            retry = false;
        if (pass == NULL && prompt) {
            status = prompt_password(args, authtok, &pass);
            if (status != PAM_SUCCESS)
                goto done;
        }

        /*
         * Attempt authentication.  If we succeeded, we're done.  Otherwise,
         * clear the password and then see if we should try again after
         * prompting for a password.
         */
        retval = password_auth_attempt(args, service, opts, pass, *creds);
        if (retval == 0) {
            creds_valid = true;
            break;
        }
        pass = NULL;
    } while (retry
             && (retval == KRB5KRB_AP_ERR_BAD_INTEGRITY
                 || retval == KRB5KRB_AP_ERR_MODIFIED
                 || retval == KRB5KDC_ERR_PREAUTH_FAILED
                 || retval == KRB5_GET_IN_TKT_LOOP
                 || retval == KRB5_BAD_ENCTYPE));

verify:
    UNUSED
    /*
     * If we think we succeeded, whether through the regular path or via
     * PKINIT, try to verify the credentials.  Don't do this if we're
     * authenticating for password changes (or any other case where we're not
     * getting a TGT).  We can't get a service ticket from a kadmin/changepw
     * ticket.
     */
    if (retval == 0 && service == NULL)
        retval = verify_creds(args, *creds);

done:
    /*
     * Free resources, including any credentials we have sitting around if we
     * failed, and return the appropriate PAM error code.  If status is
     * already set to something other than PAM_SUCCESS, we encountered a PAM
     * error and will just return that code.  Otherwise, we need to map the
     * Kerberos status code in retval to a PAM error code.
     */
    if (status == PAM_SUCCESS) {
        switch (retval) {
        case 0:
            status = PAM_SUCCESS;
            break;
        case KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN:
            status = PAM_USER_UNKNOWN;
            break;
        case KRB5KDC_ERR_KEY_EXP:
            status = PAM_NEW_AUTHTOK_REQD;
            break;
        case KRB5KDC_ERR_NAME_EXP:
            status = PAM_ACCT_EXPIRED;
            break;
        case KRB5_KDC_UNREACH:
        case KRB5_LIBOS_CANTREADPWD:
        case KRB5_REALM_CANT_RESOLVE:
        case KRB5_REALM_UNKNOWN:
            status = PAM_AUTHINFO_UNAVAIL;
            break;
        default:
            status = PAM_AUTH_ERR;
            break;
        }
    }
    if (status != PAM_SUCCESS && *creds != NULL) {
        if (creds_valid)
            krb5_free_cred_contents(ctx->context, *creds);
        free(*creds);
        *creds = NULL;
    }
    if (opts != NULL)
        krb5_get_init_creds_opt_free(ctx->context, opts);

    /* Whatever the results, destroy the anonymous FAST cache. */
    if (ctx->fast_cache != NULL) {
        krb5_cc_destroy(ctx->context, ctx->fast_cache);
        ctx->fast_cache = NULL;
    }
    return status;
}


/*
 * Authenticate a user via Kerberos.
 *
 * It would be nice to be able to save the ticket cache temporarily as a
 * memory cache and then only write it out to disk during the session
 * initialization.  Unfortunately, OpenSSH 4.2 and later do PAM authentication
 * in a subprocess and therefore has no saved module-specific data available
 * once it opens a session, so we have to save the ticket cache to disk and
 * store in the environment where it is.  The alternative is to use something
 * like System V shared memory, which seems like more trouble than it's worth.
 */
int
pamk5_authenticate(struct pam_args *args)
{
    struct context *ctx = NULL;
    krb5_creds *creds = NULL;
    char *pass = NULL;
    char *principal;
    int pamret;
    bool set_context = false;
    krb5_error_code retval;

    /* Temporary backward compatibility. */
    if (args->config->use_authtok && !args->config->force_first_pass) {
        putil_err(args, "use_authtok option in authentication group should"
                        " be changed to force_first_pass");
        args->config->force_first_pass = true;
    }

    /* Create a context and obtain the user. */
    pamret = pamk5_context_new(args);
    if (pamret != PAM_SUCCESS)
        goto done;
    ctx = args->config->ctx;

    /* Check whether we should ignore this user. */
    if (pamk5_should_ignore(args, ctx->name)) {
        pamret = PAM_USER_UNKNOWN;
        goto done;
    }

    /*
     * Do the actual authentication.
     *
     * The complexity arises if the password was expired (which means the
     * Kerberos library was also unable to prompt for the password change
     * internally).  In that case, there are three possibilities:
     * fail_pwchange says we treat that as an authentication failure and stop,
     * defer_pwchange says to set a flag that will result in an error at the
     * acct_mgmt step, and force_pwchange says that we should change the
     * password here and now.
     *
     * defer_pwchange is the formally correct behavior.  Set a flag in the
     * context and return success.  That flag will later be checked by
     * pam_sm_acct_mgmt.  We need to set the context as PAM data in the
     * defer_pwchange case, but we don't want to set the PAM data until we've
     * checked .k5login.  If we've stacked multiple pam-krb5 invocations in
     * different realms as optional, we don't want to override a previous
     * successful authentication.
     *
     * Note this means that, if the user can authenticate with multiple realms
     * and authentication succeeds in one realm and is then expired in a later
     * realm, the expiration in the latter realm wins.  This isn't ideal, but
     * avoiding that case is more complicated than it's worth.
     *
     * We would like to set the current password as PAM_OLDAUTHTOK so that
     * when the application subsequently calls pam_chauthtok, the user won't
     * be reprompted.  However, the PAM library clears all the auth tokens
     * when pam_authenticate exits, so this isn't possible.
     *
     * In the force_pwchange case, try to use the password the user just
     * entered to authenticate to the password changing service, but don't
     * throw an error if that doesn't work.  We have to move it from
     * PAM_AUTHTOK to PAM_OLDAUTHTOK to be in the place where password
     * changing expects, and have to unset PAM_AUTHTOK or we'll just change
     * the password to the same thing it was.
     */
    pamret = pamk5_password_auth(args, NULL, &creds);
    if (pamret == PAM_NEW_AUTHTOK_REQD) {
        if (args->config->fail_pwchange)
            pamret = PAM_AUTH_ERR;
        else if (args->config->defer_pwchange) {
            putil_debug(args, "expired account, deferring failure");
            ctx->expired = 1;
            pamret = PAM_SUCCESS;
        } else if (args->config->force_pwchange) {
            pam_syslog(args->pamh, LOG_INFO,
                       "user %s password expired, forcing password change",
                       ctx->name);
            pamk5_conv(args, "Password expired.  You must change it now.",
                       PAM_TEXT_INFO, NULL);
            pamret = pam_get_item(args->pamh, PAM_AUTHTOK,
                                  (PAM_CONST void **) &pass);
            if (pamret == PAM_SUCCESS && pass != NULL)
                pam_set_item(args->pamh, PAM_OLDAUTHTOK, pass);
            pam_set_item(args->pamh, PAM_AUTHTOK, NULL);
            args->config->use_first_pass = true;
            pamret = pamk5_password_change(args, false);
            if (pamret == PAM_SUCCESS)
                putil_debug(args, "successfully changed expired password");
        }
    }
    if (pamret != PAM_SUCCESS) {
        putil_log_failure(args, "authentication failure");
        goto done;
    }

    /* Check .k5login and alt_auth_map. */
    pamret = pamk5_authorized(args);
    if (pamret != PAM_SUCCESS) {
        putil_log_failure(args, "failed authorization check");
        goto done;
    }

    /* Reset PAM_USER in case we canonicalized, but ignore errors. */
    if (!ctx->expired && !args->config->no_update_user) {
        pamret = pam_set_item(args->pamh, PAM_USER, ctx->name);
        if (pamret != PAM_SUCCESS)
            putil_err_pam(args, pamret, "cannot set PAM_USER");
    }

    /* Log the successful authentication. */
    retval = krb5_unparse_name(ctx->context, ctx->princ, &principal);
    if (retval != 0) {
        putil_err_krb5(args, retval, "krb5_unparse_name failed");
        pam_syslog(args->pamh, LOG_INFO, "user %s authenticated as UNKNOWN",
                   ctx->name);
    } else {
        pam_syslog(args->pamh, LOG_INFO, "user %s authenticated as %s%s",
                   ctx->name, principal, ctx->expired ? " (expired)" : "");
        krb5_free_unparsed_name(ctx->context, principal);
    }

    /* Now that we know we're successful, we can store the context. */
    pamret = pam_set_data(args->pamh, "pam_krb5", ctx, pamk5_context_destroy);
    if (pamret != PAM_SUCCESS) {
        putil_err_pam(args, pamret, "cannot set context data");
        pamk5_context_free(args);
        pamret = PAM_SERVICE_ERR;
        goto done;
    }
    set_context = true;

    /*
     * If we have an expired account or if we're not creating a ticket cache,
     * we're done.  Otherwise, store the obtained credentials in a temporary
     * cache.
     */
    if (!args->config->no_ccache && !ctx->expired)
        pamret = pamk5_cache_init_random(args, creds);

done:
    if (creds != NULL && ctx != NULL) {
        krb5_free_cred_contents(ctx->context, creds);
        free(creds);
    }

    /*
     * Don't free our Kerberos context if we set a context, since the context
     * will take care of that.
     */
    if (set_context)
        args->ctx = NULL;

    /*
     * Clear the context on failure so that the account management module
     * knows that we didn't authenticate with Kerberos.  Only clear the
     * context if we set it.  Otherwise, we may be blowing away the context of
     * a previous successful authentication.
     */
    if (pamret != PAM_SUCCESS) {
        if (set_context)
            pam_set_data(args->pamh, "pam_krb5", NULL, NULL);
        else
            pamk5_context_free(args);
    }
    return pamret;
}
