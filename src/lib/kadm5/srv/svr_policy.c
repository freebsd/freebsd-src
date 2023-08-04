/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 * $Header$
 */

#include        <sys/types.h>
#include        <kadm5/admin.h>
#include        "server_internal.h"
#include        <stdlib.h>
#include        <string.h>
#include        <errno.h>

#define MIN_PW_HISTORY  1
#define MIN_PW_CLASSES  1
#define MAX_PW_CLASSES  5
#define MIN_PW_LENGTH   1

/* Validate allowed_keysalts. */
static kadm5_ret_t
validate_allowed_keysalts(const char *allowed_keysalts)
{
    kadm5_ret_t ret;
    krb5_key_salt_tuple *ks_tuple = NULL;
    krb5_int32 n_ks_tuple = 0;

    if (strchr(allowed_keysalts, '\t') != NULL)
        return KADM5_BAD_KEYSALTS;
    ret = krb5_string_to_keysalts(allowed_keysalts, ",", NULL, 0,
                                  &ks_tuple, &n_ks_tuple);
    free(ks_tuple);
    if (ret == EINVAL)
        return KADM5_BAD_KEYSALTS;
    return ret;
}

/*
 * Function: kadm5_create_policy
 *
 * Purpose: Create Policies in the policy DB.
 *
 * Arguments:
 *      entry   (input) The policy entry to be written out to the DB.
 *      mask    (input) Specifies which fields in entry are to ge written out
 *                      and which get default values.
 *      <return value> 0 if successful otherwise an error code is returned.
 *
 * Requires:
 *      Entry must be a valid principal entry, and mask have a valid value.
 *
 * Effects:
 *      Writes the data to the database, and does a database sync if
 *      successful.
 *
 */

kadm5_ret_t
kadm5_create_policy(void *server_handle, kadm5_policy_ent_t entry, long mask)
{
    kadm5_server_handle_t handle = server_handle;
    osa_policy_ent_rec  pent, *check_pol;
    int                 ret;
    char                *p;

    CHECK_HANDLE(server_handle);

    krb5_clear_error_message(handle->context);

    if ((entry == (kadm5_policy_ent_t) NULL) || (entry->policy == NULL))
        return EINVAL;
    if(strlen(entry->policy) == 0)
        return KADM5_BAD_POLICY;
    if (!(mask & KADM5_POLICY) || (mask & ~ALL_POLICY_MASK))
        return KADM5_BAD_MASK;
    if ((mask & KADM5_POLICY_ALLOWED_KEYSALTS) &&
        entry->allowed_keysalts != NULL) {
        ret = validate_allowed_keysalts(entry->allowed_keysalts);
        if (ret)
            return ret;
    }

    ret = krb5_db_get_policy(handle->context, entry->policy, &check_pol);
    if (!ret) {
        krb5_db_free_policy(handle->context, check_pol);
        return KADM5_DUP;
    } else if (ret != KRB5_KDB_NOENTRY) {
        return ret;
    }

    memset(&pent, 0, sizeof(pent));
    pent.name = entry->policy;
    p = entry->policy;
    while(*p != '\0') {
        if(*p < ' ' || *p > '~')
            return KADM5_BAD_POLICY;
        else
            p++;
    }
    if (!(mask & KADM5_PW_MAX_LIFE))
        pent.pw_max_life = 0;
    else
        pent.pw_max_life = entry->pw_max_life;
    if (!(mask & KADM5_PW_MIN_LIFE))
        pent.pw_min_life = 0;
    else {
        if((mask & KADM5_PW_MAX_LIFE)) {
            if(entry->pw_min_life > entry->pw_max_life && entry->pw_max_life != 0)
                return KADM5_BAD_MIN_PASS_LIFE;
        }
        pent.pw_min_life = entry->pw_min_life;
    }
    if (!(mask & KADM5_PW_MIN_LENGTH))
        pent.pw_min_length = MIN_PW_LENGTH;
    else {
        if(entry->pw_min_length < MIN_PW_LENGTH)
            return KADM5_BAD_LENGTH;
        pent.pw_min_length = entry->pw_min_length;
    }
    if (!(mask & KADM5_PW_MIN_CLASSES))
        pent.pw_min_classes = MIN_PW_CLASSES;
    else {
        if(entry->pw_min_classes > MAX_PW_CLASSES || entry->pw_min_classes < MIN_PW_CLASSES)
            return KADM5_BAD_CLASS;
        pent.pw_min_classes = entry->pw_min_classes;
    }
    if (!(mask & KADM5_PW_HISTORY_NUM))
        pent.pw_history_num = MIN_PW_HISTORY;
    else {
        if(entry->pw_history_num < MIN_PW_HISTORY)
            return KADM5_BAD_HISTORY;
        else
            pent.pw_history_num = entry->pw_history_num;
    }

    if (handle->api_version >= KADM5_API_VERSION_4) {
        if (!(mask & KADM5_POLICY_ATTRIBUTES))
            pent.attributes = 0;
        else
            pent.attributes = entry->attributes;
        if (!(mask & KADM5_POLICY_MAX_LIFE))
            pent.max_life = 0;
        else
            pent.max_life = entry->max_life;
        if (!(mask & KADM5_POLICY_MAX_RLIFE))
            pent.max_renewable_life = 0;
        else
            pent.max_renewable_life = entry->max_renewable_life;
        if (!(mask & KADM5_POLICY_ALLOWED_KEYSALTS))
            pent.allowed_keysalts = 0;
        else
            pent.allowed_keysalts = entry->allowed_keysalts;
        if (!(mask & KADM5_POLICY_TL_DATA)) {
            pent.n_tl_data = 0;
            pent.tl_data = NULL;
        } else {
            pent.n_tl_data = entry->n_tl_data;
            pent.tl_data = entry->tl_data;
        }
    }
    if (handle->api_version >= KADM5_API_VERSION_3) {
        if (!(mask & KADM5_PW_MAX_FAILURE))
            pent.pw_max_fail = 0;
        else
            pent.pw_max_fail = entry->pw_max_fail;
        if (!(mask & KADM5_PW_FAILURE_COUNT_INTERVAL))
            pent.pw_failcnt_interval = 0;
        else
            pent.pw_failcnt_interval = entry->pw_failcnt_interval;
        if (!(mask & KADM5_PW_LOCKOUT_DURATION))
            pent.pw_lockout_duration = 0;
        else
            pent.pw_lockout_duration = entry->pw_lockout_duration;
    }

    if ((ret = krb5_db_create_policy(handle->context, &pent)))
        return ret;
    else
        return KADM5_OK;
}

kadm5_ret_t
kadm5_delete_policy(void *server_handle, kadm5_policy_t name)
{
    kadm5_server_handle_t handle = server_handle;
    osa_policy_ent_t            entry;
    int                         ret;

    CHECK_HANDLE(server_handle);

    krb5_clear_error_message(handle->context);

    if(name == (kadm5_policy_t) NULL)
        return EINVAL;
    if(strlen(name) == 0)
        return KADM5_BAD_POLICY;
    ret = krb5_db_get_policy(handle->context, name, &entry);
    if (ret == KRB5_KDB_NOENTRY)
        return KADM5_UNK_POLICY;
    else if (ret)
        return ret;

    krb5_db_free_policy(handle->context, entry);
    ret = krb5_db_delete_policy(handle->context, name);
    if (ret == KRB5_KDB_POLICY_REF)
        ret = KADM5_POLICY_REF;
    return (ret == 0) ? KADM5_OK : ret;
}

/* Allocate and form a TL data list of a desired size. */
static int
alloc_tl_data(krb5_int16 n_tl_data, krb5_tl_data **tldp)
{
    krb5_tl_data **tlp = tldp;
    int i;

    for (i = 0; i < n_tl_data; i++) {
        *tlp = calloc(1, sizeof(krb5_tl_data));
        if (*tlp == NULL)
            return ENOMEM; /* caller cleans up */
        memset(*tlp, 0, sizeof(krb5_tl_data));
        tlp = &((*tlp)->tl_data_next);
    }

    return 0;
}

static kadm5_ret_t
copy_tl_data(krb5_int16 n_tl_data, krb5_tl_data *tl_data,
             krb5_tl_data **out)
{
    kadm5_ret_t ret;
    krb5_tl_data *tl, *tl_new;

    if ((ret = alloc_tl_data(n_tl_data, out)))
        return ret; /* caller cleans up */

    tl = tl_data;
    tl_new = *out;
    for (; tl; tl = tl->tl_data_next, tl_new = tl_new->tl_data_next) {
        tl_new->tl_data_contents = malloc(tl->tl_data_length);
        if (tl_new->tl_data_contents == NULL)
            return ENOMEM;
        memcpy(tl_new->tl_data_contents, tl->tl_data_contents,
               tl->tl_data_length);
        tl_new->tl_data_type = tl->tl_data_type;
        tl_new->tl_data_length = tl->tl_data_length;
    }

    return 0;
}

kadm5_ret_t
kadm5_modify_policy(void *server_handle, kadm5_policy_ent_t entry, long mask)
{
    kadm5_server_handle_t    handle = server_handle;
    krb5_tl_data            *tl;
    osa_policy_ent_t         p;
    int                      ret;

    CHECK_HANDLE(server_handle);

    krb5_clear_error_message(handle->context);

    if((entry == (kadm5_policy_ent_t) NULL) || (entry->policy == NULL))
        return EINVAL;
    if(strlen(entry->policy) == 0)
        return KADM5_BAD_POLICY;
    if ((mask & KADM5_POLICY) || (mask & ~ALL_POLICY_MASK))
        return KADM5_BAD_MASK;
    if ((mask & KADM5_POLICY_ALLOWED_KEYSALTS) &&
        entry->allowed_keysalts != NULL) {
        ret = validate_allowed_keysalts(entry->allowed_keysalts);
        if (ret)
            return ret;
    }
    if ((mask & KADM5_POLICY_TL_DATA)) {
        tl = entry->tl_data;
        while (tl != NULL) {
            if (tl->tl_data_type < 256)
                return KADM5_BAD_TL_TYPE;
            tl = tl->tl_data_next;
        }
    }

    ret = krb5_db_get_policy(handle->context, entry->policy, &p);
    if (ret == KRB5_KDB_NOENTRY)
        return KADM5_UNK_POLICY;
    else if (ret)
        return ret;

    if ((mask & KADM5_PW_MAX_LIFE))
        p->pw_max_life = entry->pw_max_life;
    if ((mask & KADM5_PW_MIN_LIFE)) {
        if(entry->pw_min_life > p->pw_max_life && p->pw_max_life != 0)  {
            krb5_db_free_policy(handle->context, p);
            return KADM5_BAD_MIN_PASS_LIFE;
        }
        p->pw_min_life = entry->pw_min_life;
    }
    if ((mask & KADM5_PW_MIN_LENGTH)) {
        if(entry->pw_min_length < MIN_PW_LENGTH) {
            krb5_db_free_policy(handle->context, p);
            return KADM5_BAD_LENGTH;
        }
        p->pw_min_length = entry->pw_min_length;
    }
    if ((mask & KADM5_PW_MIN_CLASSES)) {
        if(entry->pw_min_classes > MAX_PW_CLASSES ||
           entry->pw_min_classes < MIN_PW_CLASSES) {
            krb5_db_free_policy(handle->context, p);
            return KADM5_BAD_CLASS;
        }
        p->pw_min_classes = entry->pw_min_classes;
    }
    if ((mask & KADM5_PW_HISTORY_NUM)) {
        if(entry->pw_history_num < MIN_PW_HISTORY) {
            krb5_db_free_policy(handle->context, p);
            return KADM5_BAD_HISTORY;
        }
        p->pw_history_num = entry->pw_history_num;
    }
    if (handle->api_version >= KADM5_API_VERSION_3) {
        if ((mask & KADM5_PW_MAX_FAILURE))
            p->pw_max_fail = entry->pw_max_fail;
        if ((mask & KADM5_PW_FAILURE_COUNT_INTERVAL))
            p->pw_failcnt_interval = entry->pw_failcnt_interval;
        if ((mask & KADM5_PW_LOCKOUT_DURATION))
            p->pw_lockout_duration = entry->pw_lockout_duration;
    }
    if (handle->api_version >= KADM5_API_VERSION_4) {
        if ((mask & KADM5_POLICY_ATTRIBUTES))
            p->attributes = entry->attributes;
        if ((mask & KADM5_POLICY_MAX_LIFE))
            p->max_life = entry->max_life;
        if ((mask & KADM5_POLICY_MAX_RLIFE))
            p->max_renewable_life = entry->max_renewable_life;
        if ((mask & KADM5_POLICY_ALLOWED_KEYSALTS)) {
            free(p->allowed_keysalts);
            p->allowed_keysalts = NULL;
            if (entry->allowed_keysalts != NULL) {
                p->allowed_keysalts = strdup(entry->allowed_keysalts);
                if (p->allowed_keysalts == NULL) {
                    ret = ENOMEM;
                    goto cleanup;
                }
            }
        }
        if ((mask & KADM5_POLICY_TL_DATA)) {
            for (tl = entry->tl_data; tl != NULL; tl = tl->tl_data_next) {
                ret = krb5_db_update_tl_data(handle->context, &p->n_tl_data,
                                             &p->tl_data, tl);
                if (ret)
                    goto cleanup;
            }
        }
    }
    ret = krb5_db_put_policy(handle->context, p);

cleanup:
    krb5_db_free_policy(handle->context, p);
    return ret;
}

kadm5_ret_t
kadm5_get_policy(void *server_handle, kadm5_policy_t name,
                 kadm5_policy_ent_t entry)
{
    osa_policy_ent_t            t;
    kadm5_ret_t                 ret;
    kadm5_server_handle_t handle = server_handle;

    memset(entry, 0, sizeof(*entry));

    CHECK_HANDLE(server_handle);

    krb5_clear_error_message(handle->context);

    if (name == (kadm5_policy_t) NULL)
        return EINVAL;
    if(strlen(name) == 0)
        return KADM5_BAD_POLICY;
    ret = krb5_db_get_policy(handle->context, name, &t);
    if (ret == KRB5_KDB_NOENTRY)
        return KADM5_UNK_POLICY;
    else if (ret)
        return ret;

    if ((entry->policy = strdup(t->name)) == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }
    entry->pw_min_life = t->pw_min_life;
    entry->pw_max_life = t->pw_max_life;
    entry->pw_min_length = t->pw_min_length;
    entry->pw_min_classes = t->pw_min_classes;
    entry->pw_history_num = t->pw_history_num;
    if (handle->api_version >= KADM5_API_VERSION_3) {
        entry->pw_max_fail = t->pw_max_fail;
        entry->pw_failcnt_interval = t->pw_failcnt_interval;
        entry->pw_lockout_duration = t->pw_lockout_duration;
    }
    if (handle->api_version >= KADM5_API_VERSION_4) {
        entry->attributes = t->attributes;
        entry->max_life = t->max_life;
        entry->max_renewable_life = t->max_renewable_life;
        if (t->allowed_keysalts) {
            entry->allowed_keysalts = strdup(t->allowed_keysalts);
            if (!entry->allowed_keysalts) {
                ret = ENOMEM;
                goto cleanup;
            }
        }
        ret = copy_tl_data(t->n_tl_data, t->tl_data, &entry->tl_data);
        if (ret)
            goto cleanup;
        entry->n_tl_data = t->n_tl_data;
    }

    ret = 0;

cleanup:
    if (ret)
        kadm5_free_policy_ent(handle, entry);
    krb5_db_free_policy(handle->context, t);
    return ret;
}
