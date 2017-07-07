/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 */

#include    <k5-int.h>
#include    <kdb.h>
#include    <kadm5/server_internal.h>
#include    <kadm5/server_acl.h>
#include    "misc.h"
#include    "net-server.h"

/*
 * Function: chpass_principal_wrapper_3
 *
 * Purpose: wrapper to kadm5_chpass_principal that checks to see if
 *          pw_min_life has been reached. if not it returns an error.
 *          otherwise it calls kadm5_chpass_principal
 *
 * Arguments:
 *      principal       (input) krb5_principals whose password we are
 *                              changing
 *      keepold         (input) whether to preserve old keys
 *      n_ks_tuple      (input) the number of key-salt tuples in ks_tuple
 *      ks_tuple        (input) array of tuples indicating the caller's
 *                              requested enctypes/salttypes
 *      password        (input) password we are going to change to.
 *      <return value>  0 on success error code on failure.
 *
 * Requires:
 *      kadm5_init to have been run.
 *
 * Effects:
 *      calls kadm5_chpass_principal which changes the kdb and the
 *      the admin db.
 *
 */
kadm5_ret_t
chpass_principal_wrapper_3(void *server_handle,
                           krb5_principal principal,
                           krb5_boolean keepold,
                           int n_ks_tuple,
                           krb5_key_salt_tuple *ks_tuple,
                           char *password)
{
    kadm5_ret_t                 ret;

    ret = check_min_life(server_handle, principal, NULL, 0);
    if (ret)
        return ret;

    return kadm5_chpass_principal_3(server_handle, principal,
                                    keepold, n_ks_tuple, ks_tuple,
                                    password);
}


/*
 * Function: randkey_principal_wrapper_3
 *
 * Purpose: wrapper to kadm5_randkey_principal which checks the
 *          password's min. life.
 *
 * Arguments:
 *      principal           (input) krb5_principal whose password we are
 *                                  changing
 *      keepold         (input) whether to preserve old keys
 *      n_ks_tuple      (input) the number of key-salt tuples in ks_tuple
 *      ks_tuple        (input) array of tuples indicating the caller's
 *                              requested enctypes/salttypes
 *      key                 (output) new random key
 *      <return value>      0, error code on error.
 *
 * Requires:
 *      kadm5_init       needs to be run
 *
 * Effects:
 *      calls kadm5_randkey_principal
 *
 */
kadm5_ret_t
randkey_principal_wrapper_3(void *server_handle,
                            krb5_principal principal,
                            krb5_boolean keepold,
                            int n_ks_tuple,
                            krb5_key_salt_tuple *ks_tuple,
                            krb5_keyblock **keys, int *n_keys)
{
    kadm5_ret_t                 ret;

    ret = check_min_life(server_handle, principal, NULL, 0);
    if (ret)
        return ret;
    return kadm5_randkey_principal_3(server_handle, principal,
                                     keepold, n_ks_tuple, ks_tuple,
                                     keys, n_keys);
}

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
    krb5_boolean                access_granted;
    krb5_boolean                self;

    /*
     * If no target is explicitly provided, then the target principal
     * is the client principal.
     */
    if (target == NULL)
        target = client;

    /*
     * A principal can always change its own password, as long as it
     * has an initial ticket and meets the minimum password lifetime
     * requirement.
     */
    self = krb5_principal_compare(handle->context, client, target);
    if (self) {
        ret = check_min_life(server_handle, target, msg_ret, msg_len);
        if (ret != 0)
            return ret;

        access_granted = initial_flag;
    } else
        access_granted = FALSE;

    if (!access_granted &&
        kadm5int_acl_check_krb(handle->context, client,
                               ACL_CHANGEPW, target, NULL)) {
        /*
         * Otherwise, principals with appropriate privileges can change
         * any password
         */
        access_granted = TRUE;
    }

    if (access_granted) {
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
    krb5_int32                  now;
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
        if((now - princ.last_pwd_change) < pol.pw_min_life &&
           !(princ.attributes & KRB5_KDB_REQUIRES_PWCHANGE)) {
            if (msg_ret != NULL) {
                time_t until;
                char *time_string, *ptr;
                const char *errstr;

                until = princ.last_pwd_change + pol.pw_min_life;

                time_string = ctime(&until);
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
    kadm5_server_handle_t server_handle = (kadm5_server_handle_t)handle;

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
    kadm5_server_handle_t server_handle = (kadm5_server_handle_t)handle;
    return server_handle->context;
}
