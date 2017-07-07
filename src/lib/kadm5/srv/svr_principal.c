/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 * $Header$
 */
#include "k5-int.h"
#include        <sys/time.h>
#include        <kadm5/admin.h>
#include        <kdb.h>
#include        "server_internal.h"
#ifdef USE_PASSWORD_SERVER
#include        <sys/wait.h>
#include        <signal.h>
#endif

#include <krb5/kadm5_hook_plugin.h>

#ifdef USE_VALGRIND
#include <valgrind/memcheck.h>
#else
#define VALGRIND_CHECK_DEFINED(LVALUE) ((void)0)
#endif

extern  krb5_principal      master_princ;
extern  krb5_principal      hist_princ;
extern  krb5_keyblock       master_keyblock;
extern  krb5_db_entry       master_db;

static int decrypt_key_data(krb5_context context,
                            int n_key_data, krb5_key_data *key_data,
                            krb5_keyblock **keyblocks, int *n_keys);

/*
 * XXX Functions that ought to be in libkrb5.a, but aren't.
 */
kadm5_ret_t krb5_copy_key_data_contents(context, from, to)
    krb5_context context;
    krb5_key_data *from, *to;
{
    int i, idx;

    *to = *from;

    idx = (from->key_data_ver == 1 ? 1 : 2);

    for (i = 0; i < idx; i++) {
        if ( from->key_data_length[i] ) {
            to->key_data_contents[i] = malloc(from->key_data_length[i]);
            if (to->key_data_contents[i] == NULL) {
                for (i = 0; i < idx; i++)
                    zapfree(to->key_data_contents[i], to->key_data_length[i]);
                return ENOMEM;
            }
            memcpy(to->key_data_contents[i], from->key_data_contents[i],
                   from->key_data_length[i]);
        }
    }
    return 0;
}

static krb5_tl_data *dup_tl_data(krb5_tl_data *tl)
{
    krb5_tl_data *n;

    n = (krb5_tl_data *) malloc(sizeof(krb5_tl_data));
    if (n == NULL)
        return NULL;
    n->tl_data_contents = malloc(tl->tl_data_length);
    if (n->tl_data_contents == NULL) {
        free(n);
        return NULL;
    }
    memcpy(n->tl_data_contents, tl->tl_data_contents, tl->tl_data_length);
    n->tl_data_type = tl->tl_data_type;
    n->tl_data_length = tl->tl_data_length;
    n->tl_data_next = NULL;
    return n;
}

/* This is in lib/kdb/kdb_cpw.c, but is static */
static void cleanup_key_data(context, count, data)
    krb5_context   context;
    int                    count;
    krb5_key_data        * data;
{
    int i;

    for (i = 0; i < count; i++)
        krb5_free_key_data_contents(context, &data[i]);
    free(data);
}

/* Check whether a ks_tuple is present in an array of ks_tuples. */
static krb5_boolean
ks_tuple_present(int n_ks_tuple, krb5_key_salt_tuple *ks_tuple,
                 krb5_key_salt_tuple *looking_for)
{
    int i;

    for (i = 0; i < n_ks_tuple; i++) {
        if (ks_tuple[i].ks_enctype == looking_for->ks_enctype &&
            ks_tuple[i].ks_salttype == looking_for->ks_salttype)
            return TRUE;
    }
    return FALSE;
}

/* Fetch a policy if it exists; set *have_pol_out appropriately.  Return
 * success whether or not the policy exists. */
static kadm5_ret_t
get_policy(kadm5_server_handle_t handle, const char *name,
           kadm5_policy_ent_t policy_out, krb5_boolean *have_pol_out)
{
    kadm5_ret_t ret;

    *have_pol_out = FALSE;
    if (name == NULL)
        return 0;
    ret = kadm5_get_policy(handle->lhandle, (char *)name, policy_out);
    if (ret == 0)
        *have_pol_out = TRUE;
    return (ret == KADM5_UNK_POLICY) ? 0 : ret;
}

/*
 * Apply the -allowedkeysalts policy (see kadmin(1)'s addpol/modpol
 * commands).  We use the allowed key/salt tuple list as a default if
 * no ks tuples as provided by the caller.  We reject lists that include
 * key/salts outside the policy.  We re-order the requested ks tuples
 * (which may be a subset of the policy) to reflect the policy order.
 */
static kadm5_ret_t
apply_keysalt_policy(kadm5_server_handle_t handle, const char *policy,
                     int n_ks_tuple, krb5_key_salt_tuple *ks_tuple,
                     int *new_n_kstp, krb5_key_salt_tuple **new_kstp)
{
    kadm5_ret_t ret;
    kadm5_policy_ent_rec polent;
    krb5_boolean have_polent;
    int ak_n_ks_tuple = 0;
    int new_n_ks_tuple = 0;
    krb5_key_salt_tuple *ak_ks_tuple = NULL;
    krb5_key_salt_tuple *new_ks_tuple = NULL;
    krb5_key_salt_tuple *subset;
    int i, m;

    if (new_n_kstp != NULL) {
        *new_n_kstp = 0;
        *new_kstp = NULL;
    }

    memset(&polent, 0, sizeof(polent));
    ret = get_policy(handle, policy, &polent, &have_polent);
    if (ret)
        goto cleanup;

    if (polent.allowed_keysalts == NULL) {
        /* Requested keysalts allowed or default to supported_enctypes. */
        if (n_ks_tuple == 0) {
            /* Default to supported_enctypes. */
            n_ks_tuple = handle->params.num_keysalts;
            ks_tuple = handle->params.keysalts;
        }
        /* Dup the requested or defaulted keysalt tuples. */
        new_ks_tuple = malloc(n_ks_tuple * sizeof(*new_ks_tuple));
        if (new_ks_tuple == NULL) {
            ret = ENOMEM;
            goto cleanup;
        }
        memcpy(new_ks_tuple, ks_tuple, n_ks_tuple * sizeof(*new_ks_tuple));
        new_n_ks_tuple = n_ks_tuple;
        ret = 0;
        goto cleanup;
    }

    ret = krb5_string_to_keysalts(polent.allowed_keysalts,
                                  ",",   /* Tuple separators */
                                  NULL,  /* Key/salt separators */
                                  0,     /* No duplicates */
                                  &ak_ks_tuple,
                                  &ak_n_ks_tuple);
    /*
     * Malformed policy?  Shouldn't happen, but it's remotely possible
     * someday, so we don't assert, just bail.
     */
    if (ret)
        goto cleanup;

    /* Check that the requested ks_tuples are within policy, if we have one. */
    for (i = 0; i < n_ks_tuple; i++) {
        if (!ks_tuple_present(ak_n_ks_tuple, ak_ks_tuple, &ks_tuple[i])) {
            ret = KADM5_BAD_KEYSALTS;
            goto cleanup;
        }
    }

    /* Have policy but no ks_tuple input?  Output the policy. */
    if (n_ks_tuple == 0) {
        new_n_ks_tuple = ak_n_ks_tuple;
        new_ks_tuple = ak_ks_tuple;
        ak_ks_tuple = NULL;
        goto cleanup;
    }

    /*
     * Now filter the policy ks tuples by the requested ones so as to
     * preserve in the requested sub-set the relative ordering from the
     * policy.  We could optimize this (if (n_ks_tuple == ak_n_ks_tuple)
     * then skip this), but we don't bother.
     */
    subset = calloc(n_ks_tuple, sizeof(*subset));
    if (subset == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }
    for (m = 0, i = 0; i < ak_n_ks_tuple && m < n_ks_tuple; i++) {
        if (ks_tuple_present(n_ks_tuple, ks_tuple, &ak_ks_tuple[i]))
            subset[m++] = ak_ks_tuple[i];
    }
    new_ks_tuple = subset;
    new_n_ks_tuple = m;
    ret = 0;

cleanup:
    if (have_polent)
        kadm5_free_policy_ent(handle->lhandle, &polent);
    free(ak_ks_tuple);

    if (new_n_kstp != NULL) {
        *new_n_kstp = new_n_ks_tuple;
        *new_kstp = new_ks_tuple;
    } else {
        free(new_ks_tuple);
    }
    return ret;
}


/*
 * Set *passptr to NULL if the request looks like the first part of a krb5 1.6
 * addprinc -randkey operation.  The krb5 1.6 dummy password for these requests
 * was invalid UTF-8, which runs afoul of the arcfour string-to-key.
 */
static void
check_1_6_dummy(kadm5_principal_ent_t entry, long mask,
                int n_ks_tuple, krb5_key_salt_tuple *ks_tuple, char **passptr)
{
    int i;
    char *password = *passptr;

    /* Old-style randkey operations disallowed tickets to start. */
    if (password == NULL || !(mask & KADM5_ATTRIBUTES) ||
        !(entry->attributes & KRB5_KDB_DISALLOW_ALL_TIX))
        return;

    /* The 1.6 dummy password was the octets 1..255. */
    for (i = 0; (unsigned char) password[i] == i + 1; i++);
    if (password[i] != '\0' || i != 255)
        return;

    /* This will make the caller use a random password instead. */
    *passptr = NULL;
}

/* Return the number of keys with the newest kvno.  Assumes that all key data
 * with the newest kvno are at the front of the key data array. */
static int
count_new_keys(int n_key_data, krb5_key_data *key_data)
{
    int n;

    for (n = 1; n < n_key_data; n++) {
        if (key_data[n - 1].key_data_kvno != key_data[n].key_data_kvno)
            return n;
    }
    return n_key_data;
}

kadm5_ret_t
kadm5_create_principal(void *server_handle,
                       kadm5_principal_ent_t entry, long mask,
                       char *password)
{
    return
        kadm5_create_principal_3(server_handle, entry, mask,
                                 0, NULL, password);
}
kadm5_ret_t
kadm5_create_principal_3(void *server_handle,
                         kadm5_principal_ent_t entry, long mask,
                         int n_ks_tuple, krb5_key_salt_tuple *ks_tuple,
                         char *password)
{
    krb5_db_entry               *kdb;
    osa_princ_ent_rec           adb;
    kadm5_policy_ent_rec        polent;
    krb5_boolean                have_polent = FALSE;
    krb5_int32                  now;
    krb5_tl_data                *tl_data_tail;
    unsigned int                ret;
    kadm5_server_handle_t handle = server_handle;
    krb5_keyblock               *act_mkey;
    krb5_kvno                   act_kvno;
    int                         new_n_ks_tuple = 0;
    krb5_key_salt_tuple         *new_ks_tuple = NULL;

    CHECK_HANDLE(server_handle);

    krb5_clear_error_message(handle->context);

    check_1_6_dummy(entry, mask, n_ks_tuple, ks_tuple, &password);

    /*
     * Argument sanity checking, and opening up the DB
     */
    if (entry == NULL)
        return EINVAL;
    if(!(mask & KADM5_PRINCIPAL) || (mask & KADM5_MOD_NAME) ||
       (mask & KADM5_MOD_TIME) || (mask & KADM5_LAST_PWD_CHANGE) ||
       (mask & KADM5_MKVNO) || (mask & KADM5_AUX_ATTRIBUTES) ||
       (mask & KADM5_LAST_SUCCESS) || (mask & KADM5_LAST_FAILED) ||
       (mask & KADM5_FAIL_AUTH_COUNT))
        return KADM5_BAD_MASK;
    if ((mask & KADM5_KEY_DATA) && entry->n_key_data != 0)
        return KADM5_BAD_MASK;
    if((mask & KADM5_POLICY) && entry->policy == NULL)
        return KADM5_BAD_MASK;
    if((mask & KADM5_POLICY) && (mask & KADM5_POLICY_CLR))
        return KADM5_BAD_MASK;
    if((mask & ~ALL_PRINC_MASK))
        return KADM5_BAD_MASK;

    /*
     * Check to see if the principal exists
     */
    ret = kdb_get_entry(handle, entry->principal, &kdb, &adb);

    switch(ret) {
    case KADM5_UNK_PRINC:
        break;
    case 0:
        kdb_free_entry(handle, kdb, &adb);
        return KADM5_DUP;
    default:
        return ret;
    }

    kdb = calloc(1, sizeof(*kdb));
    if (kdb == NULL)
        return ENOMEM;
    memset(&adb, 0, sizeof(osa_princ_ent_rec));

    /*
     * If a policy was specified, load it.
     * If we can not find the one specified return an error
     */
    if ((mask & KADM5_POLICY)) {
        ret = get_policy(handle, entry->policy, &polent, &have_polent);
        if (ret)
            goto cleanup;
    }
    if (password) {
        ret = passwd_check(handle, password, have_polent ? &polent : NULL,
                           entry->principal);
        if (ret)
            goto cleanup;
    }
    /*
     * Start populating the various DB fields, using the
     * "defaults" for fields that were not specified by the
     * mask.
     */
    if ((ret = krb5_timeofday(handle->context, &now)))
        goto cleanup;

    kdb->magic = KRB5_KDB_MAGIC_NUMBER;
    kdb->len = KRB5_KDB_V1_BASE_LENGTH; /* gag me with a chainsaw */

    if ((mask & KADM5_ATTRIBUTES))
        kdb->attributes = entry->attributes;
    else
        kdb->attributes = handle->params.flags;

    if ((mask & KADM5_MAX_LIFE))
        kdb->max_life = entry->max_life;
    else
        kdb->max_life = handle->params.max_life;

    if (mask & KADM5_MAX_RLIFE)
        kdb->max_renewable_life = entry->max_renewable_life;
    else
        kdb->max_renewable_life = handle->params.max_rlife;

    if ((mask & KADM5_PRINC_EXPIRE_TIME))
        kdb->expiration = entry->princ_expire_time;
    else
        kdb->expiration = handle->params.expiration;

    kdb->pw_expiration = 0;
    if (have_polent) {
        if(polent.pw_max_life)
            kdb->pw_expiration = now + polent.pw_max_life;
        else
            kdb->pw_expiration = 0;
    }
    if ((mask & KADM5_PW_EXPIRATION))
        kdb->pw_expiration = entry->pw_expiration;

    kdb->last_success = 0;
    kdb->last_failed = 0;
    kdb->fail_auth_count = 0;

    /* this is kind of gross, but in order to free the tl data, I need
       to free the entire kdb entry, and that will try to free the
       principal. */

    ret = krb5_copy_principal(handle->context, entry->principal, &kdb->princ);
    if (ret)
        goto cleanup;

    if ((ret = krb5_dbe_update_last_pwd_change(handle->context, kdb, now)))
        goto cleanup;

    if (mask & KADM5_TL_DATA) {
        /* splice entry->tl_data onto the front of kdb->tl_data */
        for (tl_data_tail = entry->tl_data; tl_data_tail;
             tl_data_tail = tl_data_tail->tl_data_next)
        {
            ret = krb5_dbe_update_tl_data(handle->context, kdb, tl_data_tail);
            if( ret )
                goto cleanup;
        }
    }

    /*
     * We need to have setup the TL data, so we have strings, so we can
     * check enctype policy, which is why we check/initialize ks_tuple
     * this late.
     */
    ret = apply_keysalt_policy(handle, entry->policy, n_ks_tuple, ks_tuple,
                               &new_n_ks_tuple, &new_ks_tuple);
    if (ret)
        goto cleanup;

    /* initialize the keys */

    ret = kdb_get_active_mkey(handle, &act_kvno, &act_mkey);
    if (ret)
        goto cleanup;

    if (mask & KADM5_KEY_DATA) {
        /* The client requested no keys for this principal. */
        assert(entry->n_key_data == 0);
    } else if (password) {
        ret = krb5_dbe_cpw(handle->context, act_mkey, new_ks_tuple,
                           new_n_ks_tuple, password,
                           (mask & KADM5_KVNO)?entry->kvno:1,
                           FALSE, kdb);
    } else {
        /* Null password means create with random key (new in 1.8). */
        ret = krb5_dbe_crk(handle->context, &master_keyblock,
                           new_ks_tuple, new_n_ks_tuple, FALSE, kdb);
    }
    if (ret)
        goto cleanup;

    /* Record the master key VNO used to encrypt this entry's keys */
    ret = krb5_dbe_update_mkvno(handle->context, kdb, act_kvno);
    if (ret)
        goto cleanup;

    ret = k5_kadm5_hook_create(handle->context, handle->hook_handles,
                               KADM5_HOOK_STAGE_PRECOMMIT, entry, mask,
                               new_n_ks_tuple, new_ks_tuple, password);
    if (ret)
        goto cleanup;

    /* populate the admin-server-specific fields.  In the OV server,
       this used to be in a separate database.  Since there's already
       marshalling code for the admin fields, to keep things simple,
       I'm going to keep it, and make all the admin stuff occupy a
       single tl_data record, */

    adb.admin_history_kvno = INITIAL_HIST_KVNO;
    if (mask & KADM5_POLICY) {
        adb.aux_attributes = KADM5_POLICY;

        /* this does *not* need to be strdup'ed, because adb is xdr */
        /* encoded in osa_adb_create_princ, and not ever freed */

        adb.policy = entry->policy;
    }

    /* In all cases key and the principal data is set, let the database provider know */
    kdb->mask = mask | KADM5_KEY_DATA | KADM5_PRINCIPAL ;

    /* store the new db entry */
    ret = kdb_put_entry(handle, kdb, &adb);

    (void) k5_kadm5_hook_create(handle->context, handle->hook_handles,
                                KADM5_HOOK_STAGE_POSTCOMMIT, entry, mask,
                                new_n_ks_tuple, new_ks_tuple, password);

cleanup:
    free(new_ks_tuple);
    krb5_db_free_principal(handle->context, kdb);
    if (have_polent)
        (void) kadm5_free_policy_ent(handle->lhandle, &polent);
    return ret;
}


kadm5_ret_t
kadm5_delete_principal(void *server_handle, krb5_principal principal)
{
    unsigned int                ret;
    krb5_db_entry               *kdb;
    osa_princ_ent_rec           adb;
    kadm5_server_handle_t handle = server_handle;

    CHECK_HANDLE(server_handle);

    krb5_clear_error_message(handle->context);

    if (principal == NULL)
        return EINVAL;

    if ((ret = kdb_get_entry(handle, principal, &kdb, &adb)))
        return(ret);
    ret = k5_kadm5_hook_remove(handle->context, handle->hook_handles,
                               KADM5_HOOK_STAGE_PRECOMMIT, principal);
    if (ret) {
        kdb_free_entry(handle, kdb, &adb);
        return ret;
    }

    ret = kdb_delete_entry(handle, principal);

    kdb_free_entry(handle, kdb, &adb);

    if (ret == 0)
        (void) k5_kadm5_hook_remove(handle->context,
                                    handle->hook_handles,
                                    KADM5_HOOK_STAGE_POSTCOMMIT, principal);

    return ret;
}

kadm5_ret_t
kadm5_modify_principal(void *server_handle,
                       kadm5_principal_ent_t entry, long mask)
{
    int                     ret, ret2, i;
    kadm5_policy_ent_rec    pol;
    krb5_boolean            have_pol = FALSE;
    krb5_db_entry           *kdb;
    krb5_tl_data            *tl_data_orig;
    osa_princ_ent_rec       adb;
    kadm5_server_handle_t handle = server_handle;

    CHECK_HANDLE(server_handle);

    krb5_clear_error_message(handle->context);

    if(entry == NULL)
        return EINVAL;
    if((mask & KADM5_PRINCIPAL) || (mask & KADM5_LAST_PWD_CHANGE) ||
       (mask & KADM5_MOD_TIME) || (mask & KADM5_MOD_NAME) ||
       (mask & KADM5_MKVNO) || (mask & KADM5_AUX_ATTRIBUTES) ||
       (mask & KADM5_KEY_DATA) || (mask & KADM5_LAST_SUCCESS) ||
       (mask & KADM5_LAST_FAILED))
        return KADM5_BAD_MASK;
    if((mask & ~ALL_PRINC_MASK))
        return KADM5_BAD_MASK;
    if((mask & KADM5_POLICY) && entry->policy == NULL)
        return KADM5_BAD_MASK;
    if((mask & KADM5_POLICY) && (mask & KADM5_POLICY_CLR))
        return KADM5_BAD_MASK;
    if (mask & KADM5_TL_DATA) {
        tl_data_orig = entry->tl_data;
        while (tl_data_orig) {
            if (tl_data_orig->tl_data_type < 256)
                return KADM5_BAD_TL_TYPE;
            tl_data_orig = tl_data_orig->tl_data_next;
        }
    }

    ret = kdb_get_entry(handle, entry->principal, &kdb, &adb);
    if (ret)
        return(ret);

    /*
     * This is pretty much the same as create ...
     */

    if ((mask & KADM5_POLICY)) {
        ret = get_policy(handle, entry->policy, &pol, &have_pol);
        if (ret)
            goto done;

        /* set us up to use the new policy */
        adb.aux_attributes |= KADM5_POLICY;
        if (adb.policy)
            free(adb.policy);
        adb.policy = strdup(entry->policy);
    }
    if (have_pol) {
        /* set pw_max_life based on new policy */
        if (pol.pw_max_life) {
            ret = krb5_dbe_lookup_last_pwd_change(handle->context, kdb,
                                                  &(kdb->pw_expiration));
            if (ret)
                goto done;
            kdb->pw_expiration += pol.pw_max_life;
        } else {
            kdb->pw_expiration = 0;
        }
    }

    if ((mask & KADM5_POLICY_CLR) && (adb.aux_attributes & KADM5_POLICY)) {
        free(adb.policy);
        adb.policy = NULL;
        adb.aux_attributes &= ~KADM5_POLICY;
        kdb->pw_expiration = 0;
    }

    if ((mask & KADM5_ATTRIBUTES))
        kdb->attributes = entry->attributes;
    if ((mask & KADM5_MAX_LIFE))
        kdb->max_life = entry->max_life;
    if ((mask & KADM5_PRINC_EXPIRE_TIME))
        kdb->expiration = entry->princ_expire_time;
    if (mask & KADM5_PW_EXPIRATION)
        kdb->pw_expiration = entry->pw_expiration;
    if (mask & KADM5_MAX_RLIFE)
        kdb->max_renewable_life = entry->max_renewable_life;

    if((mask & KADM5_KVNO)) {
        for (i = 0; i < kdb->n_key_data; i++)
            kdb->key_data[i].key_data_kvno = entry->kvno;
    }

    if (mask & KADM5_TL_DATA) {
        krb5_tl_data *tl;

        /* may have to change the version number of the API. Updates the list with the given tl_data rather than over-writting */

        for (tl = entry->tl_data; tl;
             tl = tl->tl_data_next)
        {
            ret = krb5_dbe_update_tl_data(handle->context, kdb, tl);
            if( ret )
            {
                goto done;
            }
        }
    }

    /*
     * Setting entry->fail_auth_count to 0 can be used to manually unlock
     * an account. It is not possible to set fail_auth_count to any other
     * value using kadmin.
     */
    if (mask & KADM5_FAIL_AUTH_COUNT) {
        if (entry->fail_auth_count != 0) {
            ret = KADM5_BAD_SERVER_PARAMS;
            goto done;
        }

        kdb->fail_auth_count = 0;
    }

    /* let the mask propagate to the database provider */
    kdb->mask = mask;

    ret = k5_kadm5_hook_modify(handle->context, handle->hook_handles,
                               KADM5_HOOK_STAGE_PRECOMMIT, entry, mask);
    if (ret)
        goto done;

    ret = kdb_put_entry(handle, kdb, &adb);
    if (ret) goto done;
    (void) k5_kadm5_hook_modify(handle->context, handle->hook_handles,
                                KADM5_HOOK_STAGE_POSTCOMMIT, entry, mask);

    ret = KADM5_OK;
done:
    if (have_pol) {
        ret2 = kadm5_free_policy_ent(handle->lhandle, &pol);
        ret = ret ? ret : ret2;
    }
    kdb_free_entry(handle, kdb, &adb);
    return ret;
}

kadm5_ret_t
kadm5_rename_principal(void *server_handle,
                       krb5_principal source, krb5_principal target)
{
    krb5_db_entry *kdb;
    osa_princ_ent_rec adb;
    krb5_error_code ret;
    kadm5_server_handle_t handle = server_handle;

    CHECK_HANDLE(server_handle);

    krb5_clear_error_message(handle->context);

    if (source == NULL || target == NULL)
        return EINVAL;

    if ((ret = kdb_get_entry(handle, target, &kdb, &adb)) == 0) {
        kdb_free_entry(handle, kdb, &adb);
        return(KADM5_DUP);
    }

    ret = k5_kadm5_hook_rename(handle->context, handle->hook_handles,
                               KADM5_HOOK_STAGE_PRECOMMIT, source, target);
    if (ret)
        return ret;

    ret = krb5_db_rename_principal(handle->context, source, target);
    if (ret)
        return ret;

    /* Update the principal mod data. */
    ret = kdb_get_entry(handle, target, &kdb, &adb);
    if (ret)
        return ret;
    kdb->mask = 0;
    ret = kdb_put_entry(handle, kdb, &adb);
    kdb_free_entry(handle, kdb, &adb);
    if (ret)
        return ret;

    (void) k5_kadm5_hook_rename(handle->context, handle->hook_handles,
                                KADM5_HOOK_STAGE_POSTCOMMIT, source, target);
    return 0;
}

kadm5_ret_t
kadm5_get_principal(void *server_handle, krb5_principal principal,
                    kadm5_principal_ent_t entry,
                    long in_mask)
{
    krb5_db_entry               *kdb;
    osa_princ_ent_rec           adb;
    krb5_error_code             ret = 0;
    long                        mask;
    int i;
    kadm5_server_handle_t handle = server_handle;

    CHECK_HANDLE(server_handle);

    krb5_clear_error_message(handle->context);

    /*
     * In version 1, all the defined fields are always returned.
     * entry is a pointer to a kadm5_principal_ent_t_v1 that should be
     * filled with allocated memory.
     */
    mask = in_mask;

    memset(entry, 0, sizeof(*entry));

    if (principal == NULL)
        return EINVAL;

    if ((ret = kdb_get_entry(handle, principal, &kdb, &adb)))
        return ret;

    if ((mask & KADM5_POLICY) &&
        adb.policy && (adb.aux_attributes & KADM5_POLICY)) {
        if ((entry->policy = strdup(adb.policy)) == NULL) {
            ret = ENOMEM;
            goto done;
        }
    }

    if (mask & KADM5_AUX_ATTRIBUTES)
        entry->aux_attributes = adb.aux_attributes;

    if ((mask & KADM5_PRINCIPAL) &&
        (ret = krb5_copy_principal(handle->context, kdb->princ,
                                   &entry->principal))) {
        goto done;
    }

    if (mask & KADM5_PRINC_EXPIRE_TIME)
        entry->princ_expire_time = kdb->expiration;

    if ((mask & KADM5_LAST_PWD_CHANGE) &&
        (ret = krb5_dbe_lookup_last_pwd_change(handle->context, kdb,
                                               &(entry->last_pwd_change)))) {
        goto done;
    }

    if (mask & KADM5_PW_EXPIRATION)
        entry->pw_expiration = kdb->pw_expiration;
    if (mask & KADM5_MAX_LIFE)
        entry->max_life = kdb->max_life;

    /* this is a little non-sensical because the function returns two */
    /* values that must be checked separately against the mask */
    if ((mask & KADM5_MOD_NAME) || (mask & KADM5_MOD_TIME)) {
        ret = krb5_dbe_lookup_mod_princ_data(handle->context, kdb,
                                             &(entry->mod_date),
                                             &(entry->mod_name));
        if (ret) {
            goto done;
        }

        if (! (mask & KADM5_MOD_TIME))
            entry->mod_date = 0;
        if (! (mask & KADM5_MOD_NAME)) {
            krb5_free_principal(handle->context, entry->mod_name);
            entry->mod_name = NULL;
        }
    }

    if (mask & KADM5_ATTRIBUTES)
        entry->attributes = kdb->attributes;

    if (mask & KADM5_KVNO)
        for (entry->kvno = 0, i=0; i<kdb->n_key_data; i++)
            if ((krb5_kvno) kdb->key_data[i].key_data_kvno > entry->kvno)
                entry->kvno = kdb->key_data[i].key_data_kvno;

    if (mask & KADM5_MKVNO) {
        ret = krb5_dbe_get_mkvno(handle->context, kdb, &entry->mkvno);
        if (ret)
            goto done;
    }

    if (mask & KADM5_MAX_RLIFE)
        entry->max_renewable_life = kdb->max_renewable_life;
    if (mask & KADM5_LAST_SUCCESS)
        entry->last_success = kdb->last_success;
    if (mask & KADM5_LAST_FAILED)
        entry->last_failed = kdb->last_failed;
    if (mask & KADM5_FAIL_AUTH_COUNT)
        entry->fail_auth_count = kdb->fail_auth_count;
    if (mask & KADM5_TL_DATA) {
        krb5_tl_data *tl, *tl2;

        entry->tl_data = NULL;

        tl = kdb->tl_data;
        while (tl) {
            if (tl->tl_data_type > 255) {
                if ((tl2 = dup_tl_data(tl)) == NULL) {
                    ret = ENOMEM;
                    goto done;
                }
                tl2->tl_data_next = entry->tl_data;
                entry->tl_data = tl2;
                entry->n_tl_data++;
            }

            tl = tl->tl_data_next;
        }
    }
    if (mask & KADM5_KEY_DATA) {
        entry->n_key_data = kdb->n_key_data;
        if(entry->n_key_data) {
            entry->key_data = k5calloc(entry->n_key_data,
                                       sizeof(krb5_key_data), &ret);
            if (entry->key_data == NULL)
                goto done;
        } else
            entry->key_data = NULL;

        for (i = 0; i < entry->n_key_data; i++)
            ret = krb5_copy_key_data_contents(handle->context,
                                              &kdb->key_data[i],
                                              &entry->key_data[i]);
        if (ret)
            goto done;
    }

    ret = KADM5_OK;

done:
    if (ret && entry->principal) {
        krb5_free_principal(handle->context, entry->principal);
        entry->principal = NULL;
    }
    kdb_free_entry(handle, kdb, &adb);

    return ret;
}

/*
 * Function: check_pw_reuse
 *
 * Purpose: Check if a key appears in a list of keys, in order to
 * enforce password history.
 *
 * Arguments:
 *
 *      context                 (r) the krb5 context
 *      hist_keyblock           (r) the key that hist_key_data is
 *                              encrypted in
 *      n_new_key_data          (r) length of new_key_data
 *      new_key_data            (r) keys to check against
 *                              pw_hist_data, encrypted in hist_keyblock
 *      n_pw_hist_data          (r) length of pw_hist_data
 *      pw_hist_data            (r) passwords to check new_key_data against
 *
 * Effects:
 * For each new_key in new_key_data:
 *      decrypt new_key with the master_keyblock
 *      for each password in pw_hist_data:
 *              for each hist_key in password:
 *                      decrypt hist_key with hist_keyblock
 *                      compare the new_key and hist_key
 *
 * Returns krb5 errors, KADM5_PASS_RESUSE if a key in
 * new_key_data is the same as a key in pw_hist_data, or 0.
 */
static kadm5_ret_t
check_pw_reuse(krb5_context context,
               krb5_keyblock *hist_keyblocks,
               int n_new_key_data, krb5_key_data *new_key_data,
               unsigned int n_pw_hist_data, osa_pw_hist_ent *pw_hist_data)
{
    unsigned int x, y, z;
    krb5_keyblock newkey, histkey, *kb;
    krb5_key_data *key_data;
    krb5_error_code ret;

    assert (n_new_key_data >= 0);
    for (x = 0; x < (unsigned) n_new_key_data; x++) {
        /* Check only entries with the most recent kvno. */
        if (new_key_data[x].key_data_kvno != new_key_data[0].key_data_kvno)
            break;
        ret = krb5_dbe_decrypt_key_data(context, NULL, &(new_key_data[x]),
                                        &newkey, NULL);
        if (ret)
            return(ret);
        for (y = 0; y < n_pw_hist_data; y++) {
            for (z = 0; z < (unsigned int) pw_hist_data[y].n_key_data; z++) {
                for (kb = hist_keyblocks; kb->enctype != 0; kb++) {
                    key_data = &pw_hist_data[y].key_data[z];
                    ret = krb5_dbe_decrypt_key_data(context, kb, key_data,
                                                    &histkey, NULL);
                    if (ret)
                        continue;
                    if (newkey.length == histkey.length &&
                        newkey.enctype == histkey.enctype &&
                        memcmp(newkey.contents, histkey.contents,
                               histkey.length) == 0) {
                        krb5_free_keyblock_contents(context, &histkey);
                        krb5_free_keyblock_contents(context, &newkey);
                        return KADM5_PASS_REUSE;
                    }
                    krb5_free_keyblock_contents(context, &histkey);
                }
            }
        }
        krb5_free_keyblock_contents(context, &newkey);
    }

    return(0);
}

static void
free_history_entry(krb5_context context, osa_pw_hist_ent *hist)
{
    int i;

    for (i = 0; i < hist->n_key_data; i++)
        krb5_free_key_data_contents(context, &hist->key_data[i]);
    free(hist->key_data);
}

/*
 * Function: create_history_entry
 *
 * Purpose: Creates a password history entry from an array of
 * key_data.
 *
 * Arguments:
 *
 *      context         (r) krb5_context to use
 *      mkey            (r) master keyblock to decrypt key data with
 *      hist_key        (r) history keyblock to encrypt key data with
 *      n_key_data      (r) number of elements in key_data
 *      key_data        (r) keys to add to the history entry
 *      hist_out        (w) history entry to fill in
 *
 * Effects:
 *
 * hist->key_data is allocated to store n_key_data key_datas.  Each
 * element of key_data is decrypted with master_keyblock, re-encrypted
 * in hist_key, and added to hist->key_data.  hist->n_key_data is
 * set to n_key_data.
 */
static
int create_history_entry(krb5_context context,
                         krb5_keyblock *hist_key, int n_key_data,
                         krb5_key_data *key_data, osa_pw_hist_ent *hist_out)
{
    int i;
    krb5_error_code ret = 0;
    krb5_keyblock key;
    krb5_keysalt salt;
    krb5_ui_2 kvno;
    osa_pw_hist_ent hist;

    hist_out->key_data = NULL;
    hist_out->n_key_data = 0;

    if (n_key_data < 0)
        return EINVAL;

    memset(&key, 0, sizeof(key));
    memset(&hist, 0, sizeof(hist));

    if (n_key_data == 0)
        goto cleanup;

    hist.key_data = k5calloc(n_key_data, sizeof(krb5_key_data), &ret);
    if (hist.key_data == NULL)
        goto cleanup;

    /* We only want to store the most recent kvno, and key_data should already
     * be sorted in descending order by kvno. */
    kvno = key_data[0].key_data_kvno;

    for (i = 0; i < n_key_data; i++) {
        if (key_data[i].key_data_kvno < kvno)
            break;
        ret = krb5_dbe_decrypt_key_data(context, NULL,
                                        &key_data[i], &key,
                                        &salt);
        if (ret)
            goto cleanup;

        ret = krb5_dbe_encrypt_key_data(context, hist_key, &key, &salt,
                                        key_data[i].key_data_kvno,
                                        &hist.key_data[hist.n_key_data]);
        if (ret)
            goto cleanup;
        hist.n_key_data++;
        krb5_free_keyblock_contents(context, &key);
        /* krb5_free_keysalt(context, &salt); */
    }

    *hist_out = hist;
    hist.n_key_data = 0;
    hist.key_data = NULL;

cleanup:
    krb5_free_keyblock_contents(context, &key);
    free_history_entry(context, &hist);
    return ret;
}

/*
 * Function: add_to_history
 *
 * Purpose: Adds a password to a principal's password history.
 *
 * Arguments:
 *
 *      context         (r) krb5_context to use
 *      hist_kvno       (r) kvno of current history key
 *      adb             (r/w) admin principal entry to add keys to
 *      pol             (r) adb's policy
 *      pw              (r) keys for the password to add to adb's key history
 *
 * Effects:
 *
 * add_to_history adds a single password to adb's password history.
 * pw contains n_key_data keys in its key_data, in storage should be
 * allocated but not freed by the caller (XXX blech!).
 *
 * This function maintains adb->old_keys as a circular queue.  It
 * starts empty, and grows each time this function is called until it
 * is pol->pw_history_num items long.  adb->old_key_len holds the
 * number of allocated entries in the array, and must therefore be [0,
 * pol->pw_history_num).  adb->old_key_next is the index into the
 * array where the next element should be written, and must be [0,
 * adb->old_key_len).
 */
static kadm5_ret_t add_to_history(krb5_context context,
                                  krb5_kvno hist_kvno,
                                  osa_princ_ent_t adb,
                                  kadm5_policy_ent_t pol,
                                  osa_pw_hist_ent *pw)
{
    osa_pw_hist_ent *histp;
    uint32_t nhist;
    unsigned int i, knext, nkeys;

    nhist = pol->pw_history_num;
    /* A history of 1 means just check the current password */
    if (nhist <= 1)
        return 0;

    if (adb->admin_history_kvno != hist_kvno) {
        /* The history key has changed since the last password change, so we
         * have to reset the password history. */
        free(adb->old_keys);
        adb->old_keys = NULL;
        adb->old_key_len = 0;
        adb->old_key_next = 0;
        adb->admin_history_kvno = hist_kvno;
    }

    nkeys = adb->old_key_len;
    knext = adb->old_key_next;
    /* resize the adb->old_keys array if necessary */
    if (nkeys + 1 < nhist) {
        if (adb->old_keys == NULL) {
            adb->old_keys = (osa_pw_hist_ent *)
                malloc((nkeys + 1) * sizeof (osa_pw_hist_ent));
        } else {
            adb->old_keys = (osa_pw_hist_ent *)
                realloc(adb->old_keys,
                        (nkeys + 1) * sizeof (osa_pw_hist_ent));
        }
        if (adb->old_keys == NULL)
            return(ENOMEM);

        memset(&adb->old_keys[nkeys], 0, sizeof(osa_pw_hist_ent));
        nkeys = ++adb->old_key_len;
        /*
         * To avoid losing old keys, shift forward each entry after
         * knext.
         */
        for (i = nkeys - 1; i > knext; i--) {
            adb->old_keys[i] = adb->old_keys[i - 1];
        }
        memset(&adb->old_keys[knext], 0, sizeof(osa_pw_hist_ent));
    } else if (nkeys + 1 > nhist) {
        /*
         * The policy must have changed!  Shrink the array.
         * Can't simply realloc() down, since it might be wrapped.
         * To understand the arithmetic below, note that we are
         * copying into new positions 0 .. N-1 from old positions
         * old_key_next-N .. old_key_next-1, modulo old_key_len,
         * where N = pw_history_num - 1 is the length of the
         * shortened list.        Matt Crawford, FNAL
         */
        /*
         * M = adb->old_key_len, N = pol->pw_history_num - 1
         *
         * tmp[0] .. tmp[N-1] = old[(knext-N)%M] .. old[(knext-1)%M]
         */
        int j;
        osa_pw_hist_t tmp;

        tmp = (osa_pw_hist_ent *)
            malloc((nhist - 1) * sizeof (osa_pw_hist_ent));
        if (tmp == NULL)
            return ENOMEM;
        for (i = 0; i < nhist - 1; i++) {
            /*
             * Add nkeys once before taking remainder to avoid
             * negative values.
             */
            j = (i + nkeys + knext - (nhist - 1)) % nkeys;
            tmp[i] = adb->old_keys[j];
        }
        /* Now free the ones we don't keep (the oldest ones) */
        for (i = 0; i < nkeys - (nhist - 1); i++) {
            j = (i + nkeys + knext) % nkeys;
            histp = &adb->old_keys[j];
            for (j = 0; j < histp->n_key_data; j++) {
                krb5_free_key_data_contents(context, &histp->key_data[j]);
            }
            free(histp->key_data);
        }
        free(adb->old_keys);
        adb->old_keys = tmp;
        nkeys = adb->old_key_len = nhist - 1;
        knext = adb->old_key_next = 0;
    }

    /*
     * If nhist decreased since the last password change, and nkeys+1
     * is less than the previous nhist, it is possible for knext to
     * index into unallocated space.  This condition would not be
     * caught by the resizing code above.
     */
    if (knext + 1 > nkeys)
        knext = adb->old_key_next = 0;
    /* free the old pw history entry if it contains data */
    histp = &adb->old_keys[knext];
    for (i = 0; i < (unsigned int) histp->n_key_data; i++)
        krb5_free_key_data_contents(context, &histp->key_data[i]);
    free(histp->key_data);

    /* store the new entry */
    adb->old_keys[knext] = *pw;

    /* update the next pointer */
    if (++adb->old_key_next == nhist - 1)
        adb->old_key_next = 0;

    return(0);
}

/* FIXME: don't use global variable for this */
krb5_boolean use_password_server = 0;

#ifdef USE_PASSWORD_SERVER
static krb5_boolean
kadm5_use_password_server (void)
{
    return use_password_server;
}
#endif

void kadm5_set_use_password_server (void);

void
kadm5_set_use_password_server (void)
{
    use_password_server = 1;
}

#ifdef USE_PASSWORD_SERVER

/*
 * kadm5_launch_task () runs a program (task_path) to synchronize the
 * Apple password server with the Kerberos database.  Password server
 * programs can receive arguments on the command line (task_argv)
 * and a block of data via stdin (data_buffer).
 *
 * Because a failure to communicate with the tool results in the
 * password server falling out of sync with the database,
 * kadm5_launch_task() always fails if it can't talk to the tool.
 */

static kadm5_ret_t
kadm5_launch_task (krb5_context context,
                   const char *task_path, char * const task_argv[],
                   const char *buffer)
{
    kadm5_ret_t ret;
    int data_pipe[2];

    ret = pipe (data_pipe);
    if (ret)
        ret = errno;

    if (!ret) {
        pid_t pid = fork ();
        if (pid == -1) {
            ret = errno;
            close (data_pipe[0]);
            close (data_pipe[1]);
        } else if (pid == 0) {
            /* The child: */

            if (dup2 (data_pipe[0], STDIN_FILENO) == -1)
                _exit (1);

            close (data_pipe[0]);
            close (data_pipe[1]);

            execv (task_path, task_argv);

            _exit (1); /* Fail if execv fails */
        } else {
            /* The parent: */
            int status;

            ret = 0;

            close (data_pipe[0]);

            /* Write out the buffer to the child, add \n */
            if (buffer) {
                if (krb5_net_write (context, data_pipe[1], buffer, strlen (buffer)) < 0
                    || krb5_net_write (context, data_pipe[1], "\n", 1) < 0)
                {
                    /* kill the child to make sure waitpid() won't hang later */
                    ret = errno;
                    kill (pid, SIGKILL);
                }
            }
            close (data_pipe[1]);

            waitpid (pid, &status, 0);

            if (!ret) {
                if (WIFEXITED (status)) {
                    /* child read password and exited.  Check the return value. */
                    if ((WEXITSTATUS (status) != 0) && (WEXITSTATUS (status) != 252)) {
                        ret = KRB5KDC_ERR_POLICY; /* password change rejected */
                    }
                } else {
                    /* child read password but crashed or was killed */
                    ret = KRB5KRB_ERR_GENERIC; /* FIXME: better error */
                }
            }
        }
    }

    return ret;
}

#endif

kadm5_ret_t
kadm5_chpass_principal(void *server_handle,
                       krb5_principal principal, char *password)
{
    return
        kadm5_chpass_principal_3(server_handle, principal, FALSE,
                                 0, NULL, password);
}

kadm5_ret_t
kadm5_chpass_principal_3(void *server_handle,
                         krb5_principal principal, krb5_boolean keepold,
                         int n_ks_tuple, krb5_key_salt_tuple *ks_tuple,
                         char *password)
{
    krb5_int32                  now;
    kadm5_policy_ent_rec        pol;
    osa_princ_ent_rec           adb;
    krb5_db_entry               *kdb;
    int                         ret, ret2, last_pwd, hist_added;
    krb5_boolean                have_pol = FALSE;
    kadm5_server_handle_t       handle = server_handle;
    osa_pw_hist_ent             hist;
    krb5_keyblock               *act_mkey, *hist_keyblocks = NULL;
    krb5_kvno                   act_kvno, hist_kvno;
    int                         new_n_ks_tuple = 0;
    krb5_key_salt_tuple         *new_ks_tuple = NULL;

    CHECK_HANDLE(server_handle);

    krb5_clear_error_message(handle->context);

    hist_added = 0;
    memset(&hist, 0, sizeof(hist));

    if (principal == NULL || password == NULL)
        return EINVAL;
    if ((krb5_principal_compare(handle->context,
                                principal, hist_princ)) == TRUE)
        return KADM5_PROTECT_PRINCIPAL;

    if ((ret = kdb_get_entry(handle, principal, &kdb, &adb)))
        return(ret);

    ret = apply_keysalt_policy(handle, adb.policy, n_ks_tuple, ks_tuple,
                               &new_n_ks_tuple, &new_ks_tuple);
    if (ret)
        goto done;

    if ((adb.aux_attributes & KADM5_POLICY)) {
        ret = get_policy(handle, adb.policy, &pol, &have_pol);
        if (ret)
            goto done;
    }
    if (have_pol) {
        /* Create a password history entry before we change kdb's key_data. */
        ret = kdb_get_hist_key(handle, &hist_keyblocks, &hist_kvno);
        if (ret)
            goto done;
        ret = create_history_entry(handle->context, &hist_keyblocks[0],
                                   kdb->n_key_data, kdb->key_data, &hist);
        if (ret)
            goto done;
    }

    if ((ret = passwd_check(handle, password, have_pol ? &pol : NULL,
                            principal)))
        goto done;

    ret = kdb_get_active_mkey(handle, &act_kvno, &act_mkey);
    if (ret)
        goto done;

    ret = krb5_dbe_cpw(handle->context, act_mkey, new_ks_tuple, new_n_ks_tuple,
                       password, 0 /* increment kvno */,
                       keepold, kdb);
    if (ret)
        goto done;

    ret = krb5_dbe_update_mkvno(handle->context, kdb, act_kvno);
    if (ret)
        goto done;

    kdb->attributes &= ~KRB5_KDB_REQUIRES_PWCHANGE;

    ret = krb5_timeofday(handle->context, &now);
    if (ret)
        goto done;

    if ((adb.aux_attributes & KADM5_POLICY)) {
        /* the policy was loaded before */

        ret = krb5_dbe_lookup_last_pwd_change(handle->context, kdb, &last_pwd);
        if (ret)
            goto done;

#if 0
        /*
         * The spec says this check is overridden if the caller has
         * modify privilege.  The admin server therefore makes this
         * check itself (in chpass_principal_wrapper, misc.c). A
         * local caller implicitly has all authorization bits.
         */
        if ((now - last_pwd) < pol.pw_min_life &&
            !(kdb->attributes & KRB5_KDB_REQUIRES_PWCHANGE)) {
            ret = KADM5_PASS_TOOSOON;
            goto done;
        }
#endif

        ret = check_pw_reuse(handle->context, hist_keyblocks,
                             kdb->n_key_data, kdb->key_data,
                             1, &hist);
        if (ret)
            goto done;

        if (pol.pw_history_num > 1) {
            /* If hist_kvno has changed since the last password change, we
             * can't check the history. */
            if (adb.admin_history_kvno == hist_kvno) {
                ret = check_pw_reuse(handle->context, hist_keyblocks,
                                     kdb->n_key_data, kdb->key_data,
                                     adb.old_key_len, adb.old_keys);
                if (ret)
                    goto done;
            }

            /* Don't save empty history. */
            if (hist.n_key_data > 0) {
                ret = add_to_history(handle->context, hist_kvno, &adb, &pol,
                                     &hist);
                if (ret)
                    goto done;
                hist_added = 1;
            }
        }

        if (pol.pw_max_life)
            kdb->pw_expiration = now + pol.pw_max_life;
        else
            kdb->pw_expiration = 0;
    } else {
        kdb->pw_expiration = 0;
    }

#ifdef USE_PASSWORD_SERVER
    if (kadm5_use_password_server () &&
        (krb5_princ_size (handle->context, principal) == 1)) {
        krb5_data *princ = krb5_princ_component (handle->context, principal, 0);
        const char *path = "/usr/sbin/mkpassdb";
        char *argv[] = { "mkpassdb", "-setpassword", NULL, NULL };
        char *pstring = NULL;

        if (!ret) {
            pstring = malloc ((princ->length + 1) * sizeof (char));
            if (pstring == NULL) { ret = ENOMEM; }
        }

        if (!ret) {
            memcpy (pstring, princ->data, princ->length);
            pstring [princ->length] = '\0';
            argv[2] = pstring;

            ret = kadm5_launch_task (handle->context, path, argv, password);
        }

        if (pstring != NULL)
            free (pstring);

        if (ret)
            goto done;
    }
#endif

    ret = krb5_dbe_update_last_pwd_change(handle->context, kdb, now);
    if (ret)
        goto done;

    /* unlock principal on this KDC */
    kdb->fail_auth_count = 0;

    /* key data and attributes changed, let the database provider know */
    kdb->mask = KADM5_KEY_DATA | KADM5_ATTRIBUTES |
        KADM5_FAIL_AUTH_COUNT;
    /* | KADM5_CPW_FUNCTION */

    if (hist_added)
        kdb->mask |= KADM5_KEY_HIST;

    ret = k5_kadm5_hook_chpass(handle->context, handle->hook_handles,
                               KADM5_HOOK_STAGE_PRECOMMIT, principal, keepold,
                               new_n_ks_tuple, new_ks_tuple, password);
    if (ret)
        goto done;

    if ((ret = kdb_put_entry(handle, kdb, &adb)))
        goto done;

    (void) k5_kadm5_hook_chpass(handle->context, handle->hook_handles,
                                KADM5_HOOK_STAGE_POSTCOMMIT, principal,
                                keepold, new_n_ks_tuple, new_ks_tuple, password);
    ret = KADM5_OK;
done:
    free(new_ks_tuple);
    if (!hist_added && hist.key_data)
        free_history_entry(handle->context, &hist);
    kdb_free_entry(handle, kdb, &adb);
    kdb_free_keyblocks(handle, hist_keyblocks);

    if (have_pol && (ret2 = kadm5_free_policy_ent(handle->lhandle, &pol))
        && !ret)
        ret = ret2;

    return ret;
}

kadm5_ret_t
kadm5_randkey_principal(void *server_handle,
                        krb5_principal principal,
                        krb5_keyblock **keyblocks,
                        int *n_keys)
{
    return
        kadm5_randkey_principal_3(server_handle, principal,
                                  FALSE, 0, NULL,
                                  keyblocks, n_keys);
}
kadm5_ret_t
kadm5_randkey_principal_3(void *server_handle,
                          krb5_principal principal,
                          krb5_boolean keepold,
                          int n_ks_tuple, krb5_key_salt_tuple *ks_tuple,
                          krb5_keyblock **keyblocks,
                          int *n_keys)
{
    krb5_db_entry               *kdb;
    osa_princ_ent_rec           adb;
    krb5_int32                  now;
    kadm5_policy_ent_rec        pol;
    int                         ret, last_pwd, n_new_keys;
    krb5_boolean                have_pol = FALSE;
    kadm5_server_handle_t       handle = server_handle;
    krb5_keyblock               *act_mkey;
    krb5_kvno                   act_kvno;
    int                         new_n_ks_tuple = 0;
    krb5_key_salt_tuple         *new_ks_tuple = NULL;

    if (keyblocks)
        *keyblocks = NULL;

    CHECK_HANDLE(server_handle);

    krb5_clear_error_message(handle->context);

    if (principal == NULL)
        return EINVAL;

    if ((ret = kdb_get_entry(handle, principal, &kdb, &adb)))
        return(ret);

    ret = apply_keysalt_policy(handle, adb.policy, n_ks_tuple, ks_tuple,
                               &new_n_ks_tuple, &new_ks_tuple);
    if (ret)
        goto done;

    if (krb5_principal_compare(handle->context, principal, hist_princ)) {
        /* If changing the history entry, the new entry must have exactly one
         * key. */
        if (keepold)
            return KADM5_PROTECT_PRINCIPAL;
        new_n_ks_tuple = 1;
    }

    ret = kdb_get_active_mkey(handle, &act_kvno, &act_mkey);
    if (ret)
        goto done;

    ret = krb5_dbe_crk(handle->context, act_mkey, new_ks_tuple, new_n_ks_tuple,
                       keepold, kdb);
    if (ret)
        goto done;

    ret = krb5_dbe_update_mkvno(handle->context, kdb, act_kvno);
    if (ret)
        goto done;

    kdb->attributes &= ~KRB5_KDB_REQUIRES_PWCHANGE;

    ret = krb5_timeofday(handle->context, &now);
    if (ret)
        goto done;

    if ((adb.aux_attributes & KADM5_POLICY)) {
        ret = get_policy(handle, adb.policy, &pol, &have_pol);
        if (ret)
            goto done;
    }
    if (have_pol) {
        ret = krb5_dbe_lookup_last_pwd_change(handle->context, kdb, &last_pwd);
        if (ret)
            goto done;

#if 0
        /*
         * The spec says this check is overridden if the caller has
         * modify privilege.  The admin server therefore makes this
         * check itself (in chpass_principal_wrapper, misc.c).  A
         * local caller implicitly has all authorization bits.
         */
        if((now - last_pwd) < pol.pw_min_life &&
           !(kdb->attributes & KRB5_KDB_REQUIRES_PWCHANGE)) {
            ret = KADM5_PASS_TOOSOON;
            goto done;
        }
#endif

        if (pol.pw_max_life)
            kdb->pw_expiration = now + pol.pw_max_life;
        else
            kdb->pw_expiration = 0;
    } else {
        kdb->pw_expiration = 0;
    }

    ret = krb5_dbe_update_last_pwd_change(handle->context, kdb, now);
    if (ret)
        goto done;

    /* unlock principal on this KDC */
    kdb->fail_auth_count = 0;

    if (keyblocks) {
        /* Return only the new keys added by krb5_dbe_crk. */
        n_new_keys = count_new_keys(kdb->n_key_data, kdb->key_data);
        ret = decrypt_key_data(handle->context, n_new_keys, kdb->key_data,
                               keyblocks, n_keys);
        if (ret)
            goto done;
    }

    /* key data changed, let the database provider know */
    kdb->mask = KADM5_KEY_DATA | KADM5_FAIL_AUTH_COUNT;
    /* | KADM5_RANDKEY_USED */;

    ret = k5_kadm5_hook_chpass(handle->context, handle->hook_handles,
                               KADM5_HOOK_STAGE_PRECOMMIT, principal, keepold,
                               new_n_ks_tuple, new_ks_tuple, NULL);
    if (ret)
        goto done;
    if ((ret = kdb_put_entry(handle, kdb, &adb)))
        goto done;

    (void) k5_kadm5_hook_chpass(handle->context, handle->hook_handles,
                                KADM5_HOOK_STAGE_POSTCOMMIT, principal,
                                keepold, new_n_ks_tuple, new_ks_tuple, NULL);
    ret = KADM5_OK;
done:
    free(new_ks_tuple);
    kdb_free_entry(handle, kdb, &adb);
    if (have_pol)
        kadm5_free_policy_ent(handle->lhandle, &pol);

    return ret;
}

/*
 * kadm5_setv4key_principal:
 *
 * Set only ONE key of the principal, removing all others.  This key
 * must have the DES_CBC_CRC enctype and is entered as having the
 * krb4 salttype.  This is to enable things like kadmind4 to work.
 */
kadm5_ret_t
kadm5_setv4key_principal(void *server_handle,
                         krb5_principal principal,
                         krb5_keyblock *keyblock)
{
    krb5_db_entry               *kdb;
    osa_princ_ent_rec           adb;
    krb5_int32                  now;
    kadm5_policy_ent_rec        pol;
    krb5_keysalt                keysalt;
    int                         i, kvno, ret;
    krb5_boolean                have_pol = FALSE;
#if 0
    int                         last_pwd;
#endif
    kadm5_server_handle_t       handle = server_handle;
    krb5_key_data               tmp_key_data;
    krb5_keyblock               *act_mkey;

    memset( &tmp_key_data, 0, sizeof(tmp_key_data));

    CHECK_HANDLE(server_handle);

    krb5_clear_error_message(handle->context);

    if (principal == NULL || keyblock == NULL)
        return EINVAL;
    if (hist_princ && /* this will be NULL when initializing the databse */
        ((krb5_principal_compare(handle->context,
                                 principal, hist_princ)) == TRUE))
        return KADM5_PROTECT_PRINCIPAL;

    if (keyblock->enctype != ENCTYPE_DES_CBC_CRC)
        return KADM5_SETV4KEY_INVAL_ENCTYPE;

    if ((ret = kdb_get_entry(handle, principal, &kdb, &adb)))
        return(ret);

    for (kvno = 0, i=0; i<kdb->n_key_data; i++)
        if (kdb->key_data[i].key_data_kvno > kvno)
            kvno = kdb->key_data[i].key_data_kvno;

    if (kdb->key_data != NULL)
        cleanup_key_data(handle->context, kdb->n_key_data, kdb->key_data);

    kdb->key_data = calloc(1, sizeof(krb5_key_data));
    if (kdb->key_data == NULL)
        return ENOMEM;
    kdb->n_key_data = 1;
    keysalt.type = KRB5_KDB_SALTTYPE_V4;
    /* XXX data.magic? */
    keysalt.data.length = 0;
    keysalt.data.data = NULL;

    ret = kdb_get_active_mkey(handle, NULL, &act_mkey);
    if (ret)
        goto done;

    /* use tmp_key_data as temporary location and reallocate later */
    ret = krb5_dbe_encrypt_key_data(handle->context, act_mkey, keyblock,
                                    &keysalt, kvno + 1, kdb->key_data);
    if (ret) {
        goto done;
    }

    kdb->attributes &= ~KRB5_KDB_REQUIRES_PWCHANGE;

    ret = krb5_timeofday(handle->context, &now);
    if (ret)
        goto done;

    if ((adb.aux_attributes & KADM5_POLICY)) {
        ret = get_policy(handle, adb.policy, &pol, &have_pol);
        if (ret)
            goto done;
    }
    if (have_pol) {
#if 0
        /*
         * The spec says this check is overridden if the caller has
         * modify privilege.  The admin server therefore makes this
         * check itself (in chpass_principal_wrapper, misc.c).  A
         * local caller implicitly has all authorization bits.
         */
        if (ret = krb5_dbe_lookup_last_pwd_change(handle->context,
                                                  kdb, &last_pwd))
            goto done;
        if((now - last_pwd) < pol.pw_min_life &&
           !(kdb->attributes & KRB5_KDB_REQUIRES_PWCHANGE)) {
            ret = KADM5_PASS_TOOSOON;
            goto done;
        }
#endif

        if (pol.pw_max_life)
            kdb->pw_expiration = now + pol.pw_max_life;
        else
            kdb->pw_expiration = 0;
    } else {
        kdb->pw_expiration = 0;
    }

    ret = krb5_dbe_update_last_pwd_change(handle->context, kdb, now);
    if (ret)
        goto done;

    /* unlock principal on this KDC */
    kdb->fail_auth_count = 0;

    if ((ret = kdb_put_entry(handle, kdb, &adb)))
        goto done;

    ret = KADM5_OK;
done:
    for (i = 0; i < tmp_key_data.key_data_ver; i++) {
        if (tmp_key_data.key_data_contents[i]) {
            memset (tmp_key_data.key_data_contents[i], 0, tmp_key_data.key_data_length[i]);
            free (tmp_key_data.key_data_contents[i]);
        }
    }

    kdb_free_entry(handle, kdb, &adb);
    if (have_pol)
        kadm5_free_policy_ent(handle->lhandle, &pol);

    return ret;
}

kadm5_ret_t
kadm5_setkey_principal(void *server_handle,
                       krb5_principal principal,
                       krb5_keyblock *keyblocks,
                       int n_keys)
{
    return
        kadm5_setkey_principal_3(server_handle, principal,
                                 FALSE, 0, NULL,
                                 keyblocks, n_keys);
}

kadm5_ret_t
kadm5_setkey_principal_3(void *server_handle,
                         krb5_principal principal,
                         krb5_boolean keepold,
                         int n_ks_tuple, krb5_key_salt_tuple *ks_tuple,
                         krb5_keyblock *keyblocks,
                         int n_keys)
{
    kadm5_key_data *key_data;
    kadm5_ret_t ret;
    int i;

    if (keyblocks == NULL)
        return EINVAL;

    if (n_ks_tuple) {
        if (n_ks_tuple != n_keys)
            return KADM5_SETKEY3_ETYPE_MISMATCH;
        for (i = 0; i < n_ks_tuple; i++) {
            if (ks_tuple[i].ks_enctype != keyblocks[i].enctype)
                return KADM5_SETKEY3_ETYPE_MISMATCH;
        }
    }

    key_data = calloc(n_keys, sizeof(kadm5_key_data));
    if (key_data == NULL)
        return ENOMEM;

    for (i = 0; i < n_keys; i++) {
        key_data[i].key = keyblocks[i];
        key_data[i].salt.type =
            n_ks_tuple ? ks_tuple[i].ks_salttype : KRB5_KDB_SALTTYPE_NORMAL;
    }

    ret = kadm5_setkey_principal_4(server_handle, principal, keepold,
                                   key_data, n_keys);
    free(key_data);
    return ret;
}

/* Create a key/salt list from a key_data array. */
static kadm5_ret_t
make_ks_from_key_data(krb5_context context, kadm5_key_data *key_data,
                      int n_key_data, krb5_key_salt_tuple **out)
{
    int i;
    krb5_key_salt_tuple *ks;

    *out = NULL;

    ks = calloc(n_key_data, sizeof(*ks));
    if (ks == NULL)
        return ENOMEM;

    for (i = 0; i < n_key_data; i++) {
        ks[i].ks_enctype = key_data[i].key.enctype;
        ks[i].ks_salttype = key_data[i].salt.type;
    }
    *out = ks;
    return 0;
}

kadm5_ret_t
kadm5_setkey_principal_4(void *server_handle, krb5_principal principal,
                         krb5_boolean keepold, kadm5_key_data *key_data,
                         int n_key_data)
{
    krb5_db_entry *kdb;
    osa_princ_ent_rec adb;
    krb5_int32 now;
    kadm5_policy_ent_rec pol;
    krb5_key_data *new_key_data = NULL;
    int i, j, ret, n_new_key_data = 0;
    krb5_kvno kvno;
    krb5_boolean similar, have_pol = FALSE;
    kadm5_server_handle_t handle = server_handle;
    krb5_keyblock *act_mkey;
    krb5_key_salt_tuple *ks_from_keys = NULL;

    CHECK_HANDLE(server_handle);

    krb5_clear_error_message(handle->context);

    if (principal == NULL || key_data == NULL || n_key_data == 0)
        return EINVAL;

    /* hist_princ will be NULL when initializing the database. */
    if (hist_princ != NULL &&
        krb5_principal_compare(handle->context, principal, hist_princ))
        return KADM5_PROTECT_PRINCIPAL;

    /* For now, all keys must have the same kvno. */
    kvno = key_data[0].kvno;
    for (i = 1; i < n_key_data; i++) {
        if (key_data[i].kvno != kvno)
            return KADM5_SETKEY_BAD_KVNO;
    }

    ret = kdb_get_entry(handle, principal, &kdb, &adb);
    if (ret)
        return ret;

    if (kvno == 0) {
        /* Pick the next kvno. */
        for (i = 0; i < kdb->n_key_data; i++) {
            if (kdb->key_data[i].key_data_kvno > kvno)
                kvno = kdb->key_data[i].key_data_kvno;
        }
        kvno++;
    } else if (keepold) {
        /* Check that the kvno does collide with existing keys. */
        for (i = 0; i < kdb->n_key_data; i++) {
            if (kdb->key_data[i].key_data_kvno == kvno) {
                ret = KADM5_SETKEY_BAD_KVNO;
                goto done;
            }
        }
    }

    ret = make_ks_from_key_data(handle->context, key_data, n_key_data,
                                &ks_from_keys);
    if (ret)
        goto done;

    ret = apply_keysalt_policy(handle, adb.policy, n_key_data, ks_from_keys,
                               NULL, NULL);
    free(ks_from_keys);
    if (ret)
        goto done;

    for (i = 0; i < n_key_data; i++) {
        for (j = i + 1; j < n_key_data; j++) {
            ret = krb5_c_enctype_compare(handle->context,
                                         key_data[i].key.enctype,
                                         key_data[j].key.enctype,
                                         &similar);
            if (ret)
                goto done;
            if (similar) {
                if (key_data[i].salt.type == key_data[j].salt.type) {
                    ret = KADM5_SETKEY_DUP_ENCTYPES;
                    goto done;
                }
            }
        }
    }

    n_new_key_data = n_key_data + (keepold ? kdb->n_key_data : 0);
    new_key_data = calloc(n_new_key_data, sizeof(krb5_key_data));
    if (new_key_data == NULL) {
        n_new_key_data = 0;
        ret = ENOMEM;
        goto done;
    }

    n_new_key_data = 0;
    for (i = 0; i < n_key_data; i++) {

        ret = kdb_get_active_mkey(handle, NULL, &act_mkey);
        if (ret)
            goto done;

        ret = krb5_dbe_encrypt_key_data(handle->context, act_mkey,
                                        &key_data[i].key, &key_data[i].salt,
                                        kvno, &new_key_data[i]);
        if (ret)
            goto done;

        n_new_key_data++;
    }

    /* Copy old key data if necessary. */
    if (keepold) {
        memcpy(new_key_data + n_new_key_data, kdb->key_data,
               kdb->n_key_data * sizeof(krb5_key_data));
        memset(kdb->key_data, 0, kdb->n_key_data * sizeof(krb5_key_data));

        /*
         * Sort the keys to maintain the defined kvno order.  We only need to
         * sort if we keep old keys, as otherwise we allow only a single kvno
         * to be specified.
         */
        krb5_dbe_sort_key_data(new_key_data, n_new_key_data);
    }

    /* Replace kdb->key_data with the new keys. */
    cleanup_key_data(handle->context, kdb->n_key_data, kdb->key_data);
    kdb->key_data = new_key_data;
    kdb->n_key_data = n_new_key_data;
    new_key_data = NULL;
    n_new_key_data = 0;

    kdb->attributes &= ~KRB5_KDB_REQUIRES_PWCHANGE;

    ret = krb5_timeofday(handle->context, &now);
    if (ret)
        goto done;

    if (adb.aux_attributes & KADM5_POLICY) {
        ret = get_policy(handle, adb.policy, &pol, &have_pol);
        if (ret)
            goto done;
    }
    if (have_pol) {
        if (pol.pw_max_life)
            kdb->pw_expiration = now + pol.pw_max_life;
        else
            kdb->pw_expiration = 0;
    } else {
        kdb->pw_expiration = 0;
    }

    ret = krb5_dbe_update_last_pwd_change(handle->context, kdb, now);
    if (ret)
        goto done;

    /* Unlock principal on this KDC. */
    kdb->fail_auth_count = 0;

    ret = kdb_put_entry(handle, kdb, &adb);
    if (ret)
        goto done;

    ret = KADM5_OK;

done:
    cleanup_key_data(handle->context, n_new_key_data, new_key_data);
    kdb_free_entry(handle, kdb, &adb);
    if (have_pol)
        kadm5_free_policy_ent(handle->lhandle, &pol);
    return ret;
}

/*
 * Return the list of keys like kadm5_randkey_principal,
 * but don't modify the principal.
 */
kadm5_ret_t
kadm5_get_principal_keys(void *server_handle /* IN */,
                         krb5_principal principal /* IN */,
                         krb5_kvno kvno /* IN */,
                         kadm5_key_data **key_data_out /* OUT */,
                         int *n_key_data_out /* OUT */)
{
    krb5_db_entry               *kdb;
    osa_princ_ent_rec           adb;
    kadm5_ret_t                 ret;
    kadm5_server_handle_t       handle = server_handle;
    kadm5_key_data              *key_data = NULL;
    int i, nkeys = 0;

    if (principal == NULL || key_data_out == NULL || n_key_data_out == NULL)
        return EINVAL;

    CHECK_HANDLE(server_handle);

    if ((ret = kdb_get_entry(handle, principal, &kdb, &adb)))
        return(ret);

    key_data = calloc(kdb->n_key_data, sizeof(kadm5_key_data));
    if (key_data == NULL) {
        ret = ENOMEM;
        goto done;
    }

    for (i = 0, nkeys = 0; i < kdb->n_key_data; i++) {
        if (kvno != 0 && kvno != kdb->key_data[i].key_data_kvno)
            continue;
        key_data[nkeys].kvno = kdb->key_data[i].key_data_kvno;

        ret = krb5_dbe_decrypt_key_data(handle->context, NULL,
                                        &kdb->key_data[i],
                                        &key_data[nkeys].key,
                                        &key_data[nkeys].salt);
        if (ret)
            goto done;
        nkeys++;
    }

    *n_key_data_out = nkeys;
    *key_data_out = key_data;
    key_data = NULL;
    nkeys = 0;
    ret = KADM5_OK;

done:
    kdb_free_entry(handle, kdb, &adb);
    kadm5_free_kadm5_key_data(handle->context, nkeys, key_data);

    return ret;
}


/*
 * Allocate an array of n_key_data krb5_keyblocks, fill in each
 * element with the results of decrypting the nth key in key_data,
 * and if n_keys is not NULL fill it in with the
 * number of keys decrypted.
 */
static int decrypt_key_data(krb5_context context,
                            int n_key_data, krb5_key_data *key_data,
                            krb5_keyblock **keyblocks, int *n_keys)
{
    krb5_keyblock *keys;
    int ret, i;

    keys = (krb5_keyblock *) malloc(n_key_data*sizeof(krb5_keyblock));
    if (keys == NULL)
        return ENOMEM;
    memset(keys, 0, n_key_data*sizeof(krb5_keyblock));

    for (i = 0; i < n_key_data; i++) {
        ret = krb5_dbe_decrypt_key_data(context, NULL, &key_data[i], &keys[i],
                                        NULL);
        if (ret) {
            for (; i >= 0; i--) {
                if (keys[i].contents) {
                    memset (keys[i].contents, 0, keys[i].length);
                    free( keys[i].contents );
                }
            }

            memset(keys, 0, n_key_data*sizeof(krb5_keyblock));
            free(keys);
            return ret;
        }
    }

    *keyblocks = keys;
    if (n_keys)
        *n_keys = n_key_data;

    return 0;
}

/*
 * Function: kadm5_decrypt_key
 *
 * Purpose: Retrieves and decrypts a principal key.
 *
 * Arguments:
 *
 *      server_handle   (r) kadm5 handle
 *      entry           (r) principal retrieved with kadm5_get_principal
 *      ktype           (r) enctype to search for, or -1 to ignore
 *      stype           (r) salt type to search for, or -1 to ignore
 *      kvno            (r) kvno to search for, -1 for max, 0 for max
 *                      only if it also matches ktype and stype
 *      keyblock        (w) keyblock to fill in
 *      keysalt         (w) keysalt to fill in, or NULL
 *      kvnop           (w) kvno to fill in, or NULL
 *
 * Effects: Searches the key_data array of entry, which must have been
 * retrived with kadm5_get_principal with the KADM5_KEY_DATA mask, to
 * find a key with a specified enctype, salt type, and kvno in a
 * principal entry.  If not found, return ENOENT.  Otherwise, decrypt
 * it with the master key, and return the key in keyblock, the salt
 * in salttype, and the key version number in kvno.
 *
 * If ktype or stype is -1, it is ignored for the search.  If kvno is
 * -1, ktype and stype are ignored and the key with the max kvno is
 * returned.  If kvno is 0, only the key with the max kvno is returned
 * and only if it matches the ktype and stype; otherwise, ENOENT is
 * returned.
 */
kadm5_ret_t kadm5_decrypt_key(void *server_handle,
                              kadm5_principal_ent_t entry, krb5_int32
                              ktype, krb5_int32 stype, krb5_int32
                              kvno, krb5_keyblock *keyblock,
                              krb5_keysalt *keysalt, int *kvnop)
{
    kadm5_server_handle_t handle = server_handle;
    krb5_db_entry dbent;
    krb5_key_data *key_data;
    krb5_keyblock *mkey_ptr;
    int ret;

    CHECK_HANDLE(server_handle);

    if (entry->n_key_data == 0 || entry->key_data == NULL)
        return EINVAL;

    /* find_enctype only uses these two fields */
    dbent.n_key_data = entry->n_key_data;
    dbent.key_data = entry->key_data;
    if ((ret = krb5_dbe_find_enctype(handle->context, &dbent, ktype,
                                     stype, kvno, &key_data)))
        return ret;

    /* find_mkey only uses this field */
    dbent.tl_data = entry->tl_data;
    if ((ret = krb5_dbe_find_mkey(handle->context, &dbent, &mkey_ptr))) {
        /* try refreshing master key list */
        /* XXX it would nice if we had the mkvno here for optimization */
        if (krb5_db_fetch_mkey_list(handle->context, master_princ,
                                    &master_keyblock) == 0) {
            if ((ret = krb5_dbe_find_mkey(handle->context, &dbent,
                                          &mkey_ptr))) {
                return ret;
            }
        } else {
            return ret;
        }
    }

    if ((ret = krb5_dbe_decrypt_key_data(handle->context, NULL, key_data,
                                         keyblock, keysalt)))
        return ret;

    /*
     * Coerce the enctype of the output keyblock in case we got an
     * inexact match on the enctype; this behavior will go away when
     * the key storage architecture gets redesigned for 1.3.
     */
    if (ktype != -1)
        keyblock->enctype = ktype;

    if (kvnop)
        *kvnop = key_data->key_data_kvno;

    return KADM5_OK;
}

kadm5_ret_t
kadm5_purgekeys(void *server_handle,
                krb5_principal principal,
                int keepkvno)
{
    kadm5_server_handle_t handle = server_handle;
    kadm5_ret_t ret;
    krb5_db_entry *kdb;
    osa_princ_ent_rec adb;
    krb5_key_data *old_keydata;
    int n_old_keydata;
    int i, j, k;

    CHECK_HANDLE(server_handle);

    if (principal == NULL)
        return EINVAL;

    ret = kdb_get_entry(handle, principal, &kdb, &adb);
    if (ret)
        return(ret);

    if (keepkvno <= 0) {
        keepkvno = krb5_db_get_key_data_kvno(handle->context, kdb->n_key_data,
                                             kdb->key_data);
    }

    old_keydata = kdb->key_data;
    n_old_keydata = kdb->n_key_data;
    kdb->n_key_data = 0;
    /* Allocate one extra key_data to avoid allocating 0 bytes. */
    kdb->key_data = calloc(n_old_keydata, sizeof(krb5_key_data));
    if (kdb->key_data == NULL) {
        ret = ENOMEM;
        goto done;
    }
    memset(kdb->key_data, 0, n_old_keydata * sizeof(krb5_key_data));
    for (i = 0, j = 0; i < n_old_keydata; i++) {
        if (old_keydata[i].key_data_kvno < keepkvno)
            continue;

        /* Alias the key_data_contents pointers; we null them out in the
         * source array immediately after. */
        kdb->key_data[j] = old_keydata[i];
        for (k = 0; k < old_keydata[i].key_data_ver; k++) {
            old_keydata[i].key_data_contents[k] = NULL;
        }
        j++;
    }
    kdb->n_key_data = j;
    cleanup_key_data(handle->context, n_old_keydata, old_keydata);

    kdb->mask = KADM5_KEY_DATA;
    ret = kdb_put_entry(handle, kdb, &adb);
    if (ret)
        goto done;

done:
    kdb_free_entry(handle, kdb, &adb);
    return ret;
}

kadm5_ret_t
kadm5_get_strings(void *server_handle, krb5_principal principal,
                  krb5_string_attr **strings_out, int *count_out)
{
    kadm5_server_handle_t handle = server_handle;
    kadm5_ret_t ret;
    krb5_db_entry *kdb = NULL;

    *strings_out = NULL;
    *count_out = 0;
    CHECK_HANDLE(server_handle);
    if (principal == NULL)
        return EINVAL;

    ret = kdb_get_entry(handle, principal, &kdb, NULL);
    if (ret)
        return ret;

    ret = krb5_dbe_get_strings(handle->context, kdb, strings_out, count_out);
    kdb_free_entry(handle, kdb, NULL);
    return ret;
}

kadm5_ret_t
kadm5_set_string(void *server_handle, krb5_principal principal,
                 const char *key, const char *value)
{
    kadm5_server_handle_t handle = server_handle;
    kadm5_ret_t ret;
    krb5_db_entry *kdb;
    osa_princ_ent_rec adb;

    CHECK_HANDLE(server_handle);
    if (principal == NULL || key == NULL)
        return EINVAL;

    ret = kdb_get_entry(handle, principal, &kdb, &adb);
    if (ret)
        return ret;

    ret = krb5_dbe_set_string(handle->context, kdb, key, value);
    if (ret)
        goto done;

    kdb->mask = KADM5_TL_DATA;
    ret = kdb_put_entry(handle, kdb, &adb);

done:
    kdb_free_entry(handle, kdb, &adb);
    return ret;
}
