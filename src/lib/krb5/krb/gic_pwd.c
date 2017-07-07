/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "k5-int.h"
#include "com_err.h"
#include "init_creds_ctx.h"
#include "int-proto.h"
#include "os-proto.h"

krb5_error_code
krb5_get_as_key_password(krb5_context context,
                         krb5_principal client,
                         krb5_enctype etype,
                         krb5_prompter_fct prompter,
                         void *prompter_data,
                         krb5_data *salt,
                         krb5_data *params,
                         krb5_keyblock *as_key,
                         void *gak_data,
                         k5_response_items *ritems)
{
    struct gak_password *gp = gak_data;
    krb5_error_code ret;
    krb5_data defsalt;
    char *clientstr;
    char promptstr[1024], pwbuf[1024];
    krb5_data pw;
    krb5_prompt prompt;
    krb5_prompt_type prompt_type;
    const char *rpass;

    /* If we need to get the AS key via the responder, ask for it. */
    if (as_key == NULL) {
        if (gp->password != NULL)
            return 0;

        return k5_response_items_ask_question(ritems,
                                              KRB5_RESPONDER_QUESTION_PASSWORD,
                                              "");
    }

    /* If there's already a key of the correct etype, we're done.
       If the etype is wrong, free the existing key, and make
       a new one.

       XXX This was the old behavior, and was wrong in hw preauth
       cases.  Is this new behavior -- always asking -- correct in all
       cases?  */

    if (as_key->length) {
        if (as_key->enctype != etype) {
            krb5_free_keyblock_contents (context, as_key);
            as_key->length = 0;
        }
    }

    if (gp->password == NULL) {
        /* Check the responder for the password. */
        rpass = k5_response_items_get_answer(ritems,
                                             KRB5_RESPONDER_QUESTION_PASSWORD);
        if (rpass != NULL) {
            ret = alloc_data(&gp->storage, strlen(rpass));
            if (ret)
                return ret;
            memcpy(gp->storage.data, rpass, strlen(rpass));
            gp->password = &gp->storage;
        }
    }

    if (gp->password == NULL) {
        if (prompter == NULL)
            return(EIO);

        if ((ret = krb5_unparse_name(context, client, &clientstr)))
            return(ret);

        snprintf(promptstr, sizeof(promptstr), _("Password for %s"),
                 clientstr);
        free(clientstr);

        pw = make_data(pwbuf, sizeof(pwbuf));
        prompt.prompt = promptstr;
        prompt.hidden = 1;
        prompt.reply = &pw;
        prompt_type = KRB5_PROMPT_TYPE_PASSWORD;

        /* PROMPTER_INVOCATION */
        k5_set_prompt_types(context, &prompt_type);
        ret = (*prompter)(context, prompter_data, NULL, NULL, 1, &prompt);
        k5_set_prompt_types(context, 0);
        if (ret)
            return(ret);

        ret = krb5int_copy_data_contents(context, &pw, &gp->storage);
        zap(pw.data, pw.length);
        if (ret)
            return ret;
        gp->password = &gp->storage;
    }

    if (salt == NULL) {
        if ((ret = krb5_principal2salt(context, client, &defsalt)))
            return(ret);

        salt = &defsalt;
    } else {
        defsalt.length = 0;
    }

    ret = krb5_c_string_to_key_with_params(context, etype, gp->password, salt,
                                           params->data?params:NULL, as_key);

    if (defsalt.length)
        free(defsalt.data);

    return(ret);
}

krb5_error_code KRB5_CALLCONV
krb5_init_creds_set_password(krb5_context context,
                             krb5_init_creds_context ctx,
                             const char *password)
{
    char *s;

    s = strdup(password);
    if (s == NULL)
        return ENOMEM;

    zapfree(ctx->gakpw.storage.data, ctx->gakpw.storage.length);
    ctx->gakpw.storage = string2data(s);
    ctx->gakpw.password = &ctx->gakpw.storage;
    ctx->gak_fct = krb5_get_as_key_password;
    ctx->gak_data = &ctx->gakpw;
    return 0;
}

/* Return the password expiry time indicated by enc_part2.  Set *is_last_req
 * if the information came from a last_req value. */
static void
get_expiry_times(krb5_enc_kdc_rep_part *enc_part2, krb5_timestamp *pw_exp,
                 krb5_timestamp *acct_exp, krb5_boolean *is_last_req)
{
    krb5_last_req_entry **last_req;
    krb5_int32 lr_type;

    *pw_exp = 0;
    *acct_exp = 0;
    *is_last_req = FALSE;

    /* Look for last-req entries for password or account expiration. */
    if (enc_part2->last_req) {
        for (last_req = enc_part2->last_req; *last_req; last_req++) {
            lr_type = (*last_req)->lr_type;
            if (lr_type == KRB5_LRQ_ALL_PW_EXPTIME ||
                lr_type == KRB5_LRQ_ONE_PW_EXPTIME) {
                *is_last_req = TRUE;
                *pw_exp = (*last_req)->value;
            } else if (lr_type == KRB5_LRQ_ALL_ACCT_EXPTIME ||
                       lr_type == KRB5_LRQ_ONE_ACCT_EXPTIME) {
                *is_last_req = TRUE;
                *acct_exp = (*last_req)->value;
            }
        }
    }

    /* If we didn't find any, use the ambiguous key_exp field. */
    if (*is_last_req == FALSE)
        *pw_exp = enc_part2->key_exp;
}

/*
 * Send an appropriate warning prompter if as_reply indicates that the password
 * is going to expire soon.  If an expire callback was provided, use that
 * instead.
 */
static void
warn_pw_expiry(krb5_context context, krb5_get_init_creds_opt *options,
               krb5_prompter_fct prompter, void *data,
               const char *in_tkt_service, krb5_kdc_rep *as_reply)
{
    krb5_error_code ret;
    krb5_expire_callback_func expire_cb;
    void *expire_data;
    krb5_timestamp pw_exp, acct_exp, now;
    krb5_boolean is_last_req;
    krb5_deltat delta;
    char ts[256], banner[1024];

    get_expiry_times(as_reply->enc_part2, &pw_exp, &acct_exp, &is_last_req);

    k5_gic_opt_get_expire_cb(options, &expire_cb, &expire_data);
    if (expire_cb != NULL) {
        /* Invoke the expire callback and don't send prompter warnings. */
        (*expire_cb)(context, expire_data, pw_exp, acct_exp, is_last_req);
        return;
    }

    /* Don't warn if no password expiry value was sent. */
    if (pw_exp == 0)
        return;

    /* Don't warn if the password is being changed. */
    if (in_tkt_service && strcmp(in_tkt_service, "kadmin/changepw") == 0)
        return;

    /*
     * If the expiry time came from a last_req field, assume the KDC wants us
     * to warn.  Otherwise, warn only if the expiry time is less than a week
     * from now.
     */
    ret = krb5_timeofday(context, &now);
    if (ret != 0)
        return;
    if (!is_last_req &&
        (pw_exp < now || (pw_exp - now) > 7 * 24 * 60 * 60))
        return;

    if (!prompter)
        return;

    ret = krb5_timestamp_to_string(pw_exp, ts, sizeof(ts));
    if (ret != 0)
        return;

    delta = pw_exp - now;
    if (delta < 3600) {
        snprintf(banner, sizeof(banner),
                 _("Warning: Your password will expire in less than one hour "
                   "on %s"), ts);
    } else if (delta < 86400*2) {
        snprintf(banner, sizeof(banner),
                 _("Warning: Your password will expire in %d hour%s on %s"),
                 delta / 3600, delta < 7200 ? "" : "s", ts);
    } else {
        snprintf(banner, sizeof(banner),
                 _("Warning: Your password will expire in %d days on %s"),
                 delta / 86400, ts);
    }

    /* PROMPTER_INVOCATION */
    (*prompter)(context, data, 0, banner, 0, 0);
}

/*
 * Create a temporary options structure for getting a kadmin/changepw ticket,
 * based on the appplication-specified options.  Propagate all application
 * options which affect preauthentication, but not options which affect the
 * resulting ticket or how it is stored.  Set lifetime and flags appropriate
 * for a ticket which we will use immediately and then discard.
 *
 * The caller should free the result with free().
 */
static krb5_error_code
make_chpw_options(krb5_context context, krb5_get_init_creds_opt *in,
                  krb5_get_init_creds_opt **out)
{
    krb5_get_init_creds_opt *opt;

    *out = NULL;
    opt = k5_gic_opt_shallow_copy(in);
    if (opt == NULL)
        return ENOMEM;

    /* Get a non-forwardable, non-proxiable, short-lifetime ticket. */
    krb5_get_init_creds_opt_set_tkt_life(opt, 5 * 60);
    krb5_get_init_creds_opt_set_renew_life(opt, 0);
    krb5_get_init_creds_opt_set_forwardable(opt, 0);
    krb5_get_init_creds_opt_set_proxiable(opt, 0);

    /* Unset options which should only apply to the actual ticket. */
    opt->flags &= ~KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST;
    opt->flags &= ~KRB5_GET_INIT_CREDS_OPT_ANONYMOUS;

    /* The output ccache should only be used for the actual ticket. */
    krb5_get_init_creds_opt_set_out_ccache(context, opt, NULL);

    *out = opt;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_password(krb5_context context,
                             krb5_creds *creds,
                             krb5_principal client,
                             const char *password,
                             krb5_prompter_fct prompter,
                             void *data,
                             krb5_deltat start_time,
                             const char *in_tkt_service,
                             krb5_get_init_creds_opt *options)
{
    krb5_error_code ret;
    int use_master;
    krb5_kdc_rep *as_reply;
    int tries;
    krb5_creds chpw_creds;
    krb5_get_init_creds_opt *chpw_opts = NULL;
    struct gak_password gakpw;
    krb5_data pw0, pw1;
    char banner[1024], pw0array[1024], pw1array[1024];
    krb5_prompt prompt[2];
    krb5_prompt_type prompt_types[sizeof(prompt)/sizeof(prompt[0])];
    struct errinfo errsave = EMPTY_ERRINFO;
    char *message;

    use_master = 0;
    as_reply = NULL;
    memset(&chpw_creds, 0, sizeof(chpw_creds));
    memset(&gakpw, 0, sizeof(gakpw));

    if (password != NULL) {
        pw0 = string2data((char *)password);
        gakpw.password = &pw0;
    }

    /* first try: get the requested tkt from any kdc */

    ret = k5_get_init_creds(context, creds, client, prompter, data, start_time,
                            in_tkt_service, options, krb5_get_as_key_password,
                            &gakpw, &use_master, &as_reply);

    /* check for success */

    if (ret == 0)
        goto cleanup;

    /* If all the kdc's are unavailable, or if the error was due to a
       user interrupt, fail */

    if (ret == KRB5_KDC_UNREACH || ret == KRB5_REALM_CANT_RESOLVE ||
        ret == KRB5_LIBOS_PWDINTR || ret == KRB5_LIBOS_CANTREADPWD)
        goto cleanup;

    /* if the reply did not come from the master kdc, try again with
       the master kdc */

    if (!use_master) {
        TRACE_GIC_PWD_MASTER(context);
        use_master = 1;

        k5_save_ctx_error(context, ret, &errsave);
        if (as_reply) {
            krb5_free_kdc_rep( context, as_reply);
            as_reply = NULL;
        }
        ret = k5_get_init_creds(context, creds, client, prompter, data,
                                start_time, in_tkt_service, options,
                                krb5_get_as_key_password, &gakpw, &use_master,
                                &as_reply);

        if (ret == 0)
            goto cleanup;

        /* If the master is unreachable, return the error from the slave we
         * were able to contact and reset the use_master flag. */
        if (ret == KRB5_KDC_UNREACH || ret == KRB5_REALM_CANT_RESOLVE ||
            ret == KRB5_REALM_UNKNOWN) {
            ret = k5_restore_ctx_error(context, &errsave);
            use_master = 0;
        }
    }

    /* at this point, we have an error from the master.  if the error
       is not password expired, or if it is but there's no prompter,
       return this error */

    if ((ret != KRB5KDC_ERR_KEY_EXP) ||
        (prompter == NULL))
        goto cleanup;

    /* historically the default has been to prompt for password change.
     * if the change password prompt option has not been set, we continue
     * to prompt.  Prompting is only disabled if the option has been set
     * and the value has been set to false.
     */
    if (options && !(options->flags & KRB5_GET_INIT_CREDS_OPT_CHG_PWD_PRMPT))
        goto cleanup;
    TRACE_GIC_PWD_EXPIRED(context);

    /* ok, we have an expired password.  Give the user a few chances
       to change it */

    ret = make_chpw_options(context, options, &chpw_opts);
    if (ret)
        goto cleanup;
    ret = k5_get_init_creds(context, &chpw_creds, client, prompter, data,
                            start_time, "kadmin/changepw", chpw_opts,
                            krb5_get_as_key_password, &gakpw, &use_master,
                            NULL);
    if (ret)
        goto cleanup;

    pw0.data = pw0array;
    pw0.data[0] = '\0';
    pw0.length = sizeof(pw0array);
    prompt[0].prompt = _("Enter new password");
    prompt[0].hidden = 1;
    prompt[0].reply = &pw0;
    prompt_types[0] = KRB5_PROMPT_TYPE_NEW_PASSWORD;

    pw1.data = pw1array;
    pw1.data[0] = '\0';
    pw1.length = sizeof(pw1array);
    prompt[1].prompt = _("Enter it again");
    prompt[1].hidden = 1;
    prompt[1].reply = &pw1;
    prompt_types[1] = KRB5_PROMPT_TYPE_NEW_PASSWORD_AGAIN;

    strlcpy(banner, _("Password expired.  You must change it now."),
            sizeof(banner));

    for (tries = 3; tries; tries--) {
        TRACE_GIC_PWD_CHANGEPW(context, tries);
        pw0.length = sizeof(pw0array);
        pw1.length = sizeof(pw1array);

        /* PROMPTER_INVOCATION */
        k5_set_prompt_types(context, prompt_types);
        ret = (*prompter)(context, data, 0, banner,
                          sizeof(prompt)/sizeof(prompt[0]), prompt);
        k5_set_prompt_types(context, 0);
        if (ret)
            goto cleanup;

        if (strcmp(pw0.data, pw1.data) != 0) {
            ret = KRB5_LIBOS_BADPWDMATCH;
            snprintf(banner, sizeof(banner),
                     _("%s.  Please try again."), error_message(ret));
        } else if (pw0.length == 0) {
            ret = KRB5_CHPW_PWDNULL;
            snprintf(banner, sizeof(banner),
                     _("%s.  Please try again."), error_message(ret));
        } else {
            int result_code;
            krb5_data code_string;
            krb5_data result_string;

            if ((ret = krb5_change_password(context, &chpw_creds, pw0array,
                                            &result_code, &code_string,
                                            &result_string)))
                goto cleanup;

            /* the change succeeded.  go on */

            if (result_code == 0) {
                free(code_string.data);
                free(result_string.data);
                break;
            }

            /* set this in case the retry loop falls through */

            ret = KRB5_CHPW_FAIL;

            if (result_code != KRB5_KPASSWD_SOFTERROR) {
                free(code_string.data);
                free(result_string.data);
                goto cleanup;
            }

            /* the error was soft, so try again */

            if (krb5_chpw_message(context, &result_string, &message) != 0)
                message = NULL;

            /* 100 is I happen to know that no code_string will be longer
               than 100 chars */

            if (message != NULL && strlen(message) > (sizeof(banner) - 100))
                message[sizeof(banner) - 100] = '\0';

            snprintf(banner, sizeof(banner),
                     _("%.*s%s%s.  Please try again.\n"),
                     (int) code_string.length, code_string.data,
                     message ? ": " : "", message ? message : "");

            free(message);
            free(code_string.data);
            free(result_string.data);
        }
    }

    if (ret)
        goto cleanup;

    /* the password change was successful.  Get an initial ticket
       from the master.  this is the last try.  the return from this
       is final.  */

    TRACE_GIC_PWD_CHANGED(context);
    gakpw.password = &pw0;
    ret = k5_get_init_creds(context, creds, client, prompter, data,
                            start_time, in_tkt_service, options,
                            krb5_get_as_key_password, &gakpw, &use_master,
                            &as_reply);
    if (ret)
        goto cleanup;

cleanup:
    if (ret == 0)
        warn_pw_expiry(context, options, prompter, data, in_tkt_service,
                       as_reply);
    free(chpw_opts);
    zapfree(gakpw.storage.data, gakpw.storage.length);
    memset(pw0array, 0, sizeof(pw0array));
    memset(pw1array, 0, sizeof(pw1array));
    krb5_free_cred_contents(context, &chpw_creds);
    if (as_reply)
        krb5_free_kdc_rep(context, as_reply);
    k5_clear_error(&errsave);

    return(ret);
}

/*
  Rewrites get_in_tkt in terms of newer get_init_creds API.
  Attempts to get an initial ticket for creds->client to use server
  creds->server, (realm is taken from creds->client), with options
  options, and using creds->times.starttime, creds->times.endtime,
  creds->times.renew_till as from, till, and rtime.
  creds->times.renew_till is ignored unless the RENEWABLE option is requested.

  If addrs is non-NULL, it is used for the addresses requested.  If it is
  null, the system standard addresses are used.

  If password is non-NULL, it is converted using the cryptosystem entry
  point for a string conversion routine, seeded with the client's name.
  If password is passed as NULL, the password is read from the terminal,
  and then converted into a key.

  A succesful call will place the ticket in the credentials cache ccache.

  returns system errors, encryption errors
*/
krb5_error_code KRB5_CALLCONV
krb5_get_in_tkt_with_password(krb5_context context, krb5_flags options,
                              krb5_address *const *addrs, krb5_enctype *ktypes,
                              krb5_preauthtype *pre_auth_types,
                              const char *password, krb5_ccache ccache,
                              krb5_creds *creds, krb5_kdc_rep **ret_as_reply)
{
    krb5_error_code retval;
    struct gak_password gakpw;
    krb5_data pw;
    char * server;
    krb5_principal server_princ, client_princ;
    int use_master = 0;
    krb5_get_init_creds_opt *opts = NULL;

    memset(&gakpw, 0, sizeof(gakpw));
    if (password != NULL) {
        pw = string2data((char *)password);
        gakpw.password = &pw;
    }
    retval = k5_populate_gic_opt(context, &opts, options, addrs, ktypes,
                                 pre_auth_types, creds);
    if (retval)
        return (retval);
    retval = krb5_unparse_name( context, creds->server, &server);
    if (retval) {
        krb5_get_init_creds_opt_free(context, opts);
        return (retval);
    }
    server_princ = creds->server;
    client_princ = creds->client;
    retval = k5_get_init_creds(context, creds, creds->client,
                               krb5_prompter_posix, NULL, 0, server, opts,
                               krb5_get_as_key_password, &gakpw, &use_master,
                               ret_as_reply);
    krb5_free_unparsed_name( context, server);
    krb5_get_init_creds_opt_free(context, opts);
    zapfree(gakpw.storage.data, gakpw.storage.length);
    if (retval) {
        return (retval);
    }
    krb5_free_principal( context, creds->server);
    krb5_free_principal( context, creds->client);
    creds->client = client_princ;
    creds->server = server_princ;
    /* store it in the ccache! */
    if (ccache)
        if ((retval = krb5_cc_store_cred(context, ccache, creds)))
            return (retval);
    return retval;
}
