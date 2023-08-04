/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 */

#include    <k5-int.h>
#include    <kdb.h>
#include    <kadm5/server_internal.h>
#include    "misc.h"
#include    "auth.h"
#include    "net-server.h"
kadm5_ret_t
schpw_util_wrapper(void *server_handle,
                   krb5_principal client,
                   krb5_principal target,
                   krb5_boolean initial_flag,
                   char *new_pw, char **ret_pw,
                   char *msg_ret, unsigned int msg_len)
{
    kadm5_ret_t                 ret;
    kadm5_server_handle_t       handle = server_handle;

    /*
     * If no target is explicitly provided, then the target principal
     * is the client principal.
     */
    if (target == NULL)
        target = client;

    /* If the client is changing its own password, require it to use an initial
     * ticket, and enforce the policy min_life. */
    if (krb5_principal_compare(handle->context, client, target)) {
        if (!initial_flag) {
            strlcpy(msg_ret, "Ticket must be derived from a password",
                    msg_len);
            return KADM5_AUTH_INITIAL;
        }

        ret = check_min_life(server_handle, target, msg_ret, msg_len);
        if (ret != 0)
            return ret;
    }

    if (auth(handle->context, OP_CPW, client, target,
             NULL, NULL, NULL, NULL, 0)) {
        ret = kadm5_chpass_principal_util(server_handle,
                                          target,
                                          new_pw, ret_pw,
                                          msg_ret, msg_len);
    } else {
        ret = KADM5_AUTH_CHANGEPW;
        strlcpy(msg_ret, "Unauthorized request", msg_len);
    }

    return ret;
}

kadm5_ret_t
check_min_life(void *server_handle, krb5_principal principal,
               char *msg_ret, unsigned int msg_len)
{
    krb5_timestamp              now;
    kadm5_ret_t                 ret;
    kadm5_policy_ent_rec        pol;
    kadm5_principal_ent_rec     princ;
    kadm5_server_handle_t       handle = server_handle;

    if (msg_ret != NULL)
        *msg_ret = '\0';

    ret = krb5_timeofday(handle->context, &now);
    if (ret)
        return ret;

    ret = kadm5_get_principal(handle->lhandle, principal,
                              &princ, KADM5_PRINCIPAL_NORMAL_MASK);
    if(ret)
        return ret;
    if(princ.aux_attributes & KADM5_POLICY) {
        /* Look up the policy.  If it doesn't exist, treat this principal as if
         * it had no policy. */
        if((ret=kadm5_get_policy(handle->lhandle,
                                 princ.policy, &pol)) != KADM5_OK) {
            (void) kadm5_free_principal_ent(handle->lhandle, &princ);
            return (ret == KADM5_UNK_POLICY) ? 0 : ret;
        }
        if(ts_delta(now, princ.last_pwd_change) < pol.pw_min_life &&
           !(princ.attributes & KRB5_KDB_REQUIRES_PWCHANGE)) {
            if (msg_ret != NULL) {
                time_t until;
                char *time_string, *ptr;
                const char *errstr;

                until = princ.last_pwd_change + pol.pw_min_life;

                time_string = ctime(&until);
                if (time_string == NULL)
                    time_string = "(error)";
                errstr = error_message(CHPASS_UTIL_PASSWORD_TOO_SOON);

                if (strlen(errstr) + strlen(time_string) < msg_len) {
                    if (*(ptr = &time_string[strlen(time_string)-1]) == '\n')
                        *ptr = '\0';
                    snprintf(msg_ret, msg_len, errstr, time_string);
                }
            }

            (void) kadm5_free_policy_ent(handle->lhandle, &pol);
            (void) kadm5_free_principal_ent(handle->lhandle, &princ);
            return KADM5_PASS_TOOSOON;
        }

        ret = kadm5_free_policy_ent(handle->lhandle, &pol);
        if (ret) {
            (void) kadm5_free_principal_ent(handle->lhandle, &princ);
            return ret;
        }
    }

    return kadm5_free_principal_ent(handle->lhandle, &princ);
}

#define MAXPRINCLEN 125

void
trunc_name(size_t *len, char **dots)
{
    *dots = *len > MAXPRINCLEN ? "..." : "";
    *len = *len > MAXPRINCLEN ? MAXPRINCLEN : *len;
}

krb5_error_code
make_toolong_error (void *handle, krb5_data **out)
{
    krb5_error errpkt;
    krb5_error_code retval;
    krb5_data *scratch;
    kadm5_server_handle_t server_handle = *(void **)handle;

    retval = krb5_us_timeofday(server_handle->context, &errpkt.stime, &errpkt.susec);
    if (retval)
        return retval;
    errpkt.error = KRB_ERR_FIELD_TOOLONG;
    retval = krb5_build_principal(server_handle->context, &errpkt.server,
                                  strlen(server_handle->params.realm),
                                  server_handle->params.realm,
                                  "kadmin", "changepw", NULL);
    if (retval)
        return retval;
    errpkt.client = NULL;
    errpkt.cusec = 0;
    errpkt.ctime = 0;
    errpkt.text.length = 0;
    errpkt.text.data = 0;
    errpkt.e_data.length = 0;
    errpkt.e_data.data = 0;
    scratch = malloc(sizeof(*scratch));
    if (scratch == NULL)
        return ENOMEM;
    retval = krb5_mk_error(server_handle->context, &errpkt, scratch);
    if (retval) {
        free(scratch);
        return retval;
    }

    *out = scratch;
    return 0;
}

krb5_context get_context(void *handle)
{
    kadm5_server_handle_t server_handle = *(void **)handle;
    return server_handle->context;
}
