/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 * $Header$
 */

#include "k5-int.h"

#include <sys/file.h>
#include "policy_db.h"

#define OPENLOCK(db, mode)                                              \
    {                                                                   \
        int olret;                                                      \
        if (db == NULL)                                                 \
            return EINVAL;                                              \
        else if (db->magic != OSA_ADB_POLICY_DB_MAGIC)                  \
            return OSA_ADB_DBINIT;                                      \
        else if ((olret = osa_adb_open_and_lock(db, mode)) != OSA_ADB_OK) \
            return olret;                                               \
    }

#define CLOSELOCK(db)                                                   \
    {                                                                   \
        int cl_ret;                                                     \
        if ((cl_ret = osa_adb_close_and_unlock(db)) != OSA_ADB_OK)      \
            return cl_ret;                                              \
    }


/*
 * Function: osa_adb_create_policy
 *
 * Purpose: create a policy entry in the policy db.
 *
 * Arguments:
 *      entry           (input) pointer to the entry to be added
 *      <return value>  OSA_ADB_OK on success, else error code.
 *
 * Requires:
 *      entry have a valid name.
 *
 * Effects:
 *      creates the entry in the db
 *
 * Modifies:
 *      the policy db.
 *
 */
krb5_error_code
osa_adb_create_policy(osa_adb_policy_t db, osa_policy_ent_t entry)
{
    DBT                 dbkey;
    DBT                 dbdata;
    XDR                 xdrs;
    int                 ret;

    OPENLOCK(db, KRB5_DB_LOCKMODE_EXCLUSIVE);

    if(entry->name == NULL) {
        ret = EINVAL;
        goto error;
    }
    dbkey.data = entry->name;
    dbkey.size = (strlen(entry->name) + 1);

    switch(db->db->get(db->db, &dbkey, &dbdata, 0)) {
    case 0:
        ret = OSA_ADB_DUP;
        goto error;
    case 1:
        break;
    default:
        ret = errno;
        goto error;
    }
    xdralloc_create(&xdrs, XDR_ENCODE);
    if(!xdr_osa_policy_ent_rec(&xdrs, entry)) {
        xdr_destroy(&xdrs);
        ret = OSA_ADB_XDR_FAILURE;
        goto error;
    }
    dbdata.data = xdralloc_getdata(&xdrs);
    dbdata.size = xdr_getpos(&xdrs);
    switch(db->db->put(db->db, &dbkey, &dbdata, R_NOOVERWRITE)) {
    case 0:
        if((db->db->sync(db->db, 0)) == -1)
            ret = OSA_ADB_FAILURE;
        ret = OSA_ADB_OK;
        break;
    case 1:
        ret = OSA_ADB_DUP;
        break;
    default:
        ret = OSA_ADB_FAILURE;
        break;
    }
    xdr_destroy(&xdrs);

error:
    CLOSELOCK(db);
    return ret;
}

/*
 * Function: osa_adb_destroy_policy
 *
 * Purpose: destroy a policy entry
 *
 * Arguments:
 *      db              (input) database handle
 *      name            (input) name of policy
 *      <return value>  OSA_ADB_OK on success, or error code.
 *
 * Requires:
 *      db being valid.
 *      name being non-null.
 * Effects:
 *      deletes policy from db.
 *
 * Modifies:
 *      policy db.
 *
 */
krb5_error_code
osa_adb_destroy_policy(osa_adb_policy_t db, char *name)
{
    DBT     dbkey;
    int     status, ret;

    OPENLOCK(db, KRB5_DB_LOCKMODE_EXCLUSIVE);

    if(name == NULL) {
        ret = EINVAL;
        goto error;
    }
    dbkey.data = name;
    dbkey.size = (strlen(name) + 1);

    status = db->db->del(db->db, &dbkey, 0);
    switch(status) {
    case 1:
        ret = OSA_ADB_NOENT;
        goto error;
    case 0:
        if ((db->db->sync(db->db, 0)) == -1) {
            ret = OSA_ADB_FAILURE;
            goto error;
        }
        ret = OSA_ADB_OK;
        break;
    default:
        ret = OSA_ADB_FAILURE;
        goto error;
    }

error:
    CLOSELOCK(db);
    return ret;
}

/*
 * Function: osa_adb_get_policy
 *
 * Purpose: retrieve policy
 *
 * Arguments:
 *      db              (input) db handle
 *      name            (input) name of policy
 *      entry           (output) policy entry
 *      cnt             (inout) Number of entries
 *      <return value>  0 on success, error code on failure.
 *
 * Requires:
 * Effects:
 * Modifies:
 */
krb5_error_code
osa_adb_get_policy(osa_adb_policy_t db, char *name,
                   osa_policy_ent_t *entry_ptr)
{
    DBT                 dbkey;
    DBT                 dbdata;
    XDR                 xdrs;
    int                 ret;
    char                *aligned_data = NULL;
    osa_policy_ent_t    entry = NULL;

    *entry_ptr = NULL;
    OPENLOCK(db, KRB5_DB_LOCKMODE_SHARED);

    if(name == NULL) {
        ret = EINVAL;
        goto error;
    }
    dbkey.data = name;
    dbkey.size = (strlen(dbkey.data) + 1);
    dbdata.data = NULL;
    dbdata.size = 0;
    switch((db->db->get(db->db, &dbkey, &dbdata, 0))) {
    case 1:
        ret = KRB5_KDB_NOENTRY;
        goto error;
    case 0:
        break;
    default:
        ret = OSA_ADB_FAILURE;
        goto error;
    }
    entry = k5alloc(sizeof(*entry), &ret);
    if (entry == NULL)
        goto error;
    aligned_data = k5memdup(dbdata.data, dbdata.size, &ret);
    if (aligned_data == NULL)
        goto error;
    xdrmem_create(&xdrs, aligned_data, dbdata.size, XDR_DECODE);
    if (!xdr_osa_policy_ent_rec(&xdrs, entry)) {
        ret = OSA_ADB_FAILURE;
        goto error;
    }
    ret = OSA_ADB_OK;
    xdr_destroy(&xdrs);
    *entry_ptr = entry;
    entry = NULL;

error:
    free(aligned_data);
    free(entry);
    CLOSELOCK(db);
    return ret;
}

/*
 * Function: osa_adb_put_policy
 *
 * Purpose: update a policy in the dababase
 *
 * Arguments:
 *      db              (input) db handle
 *      entry           (input) policy entry
 *      <return value>  0 on success error code on failure.
 *
 * Requires:
 *      [requires]
 *
 * Effects:
 *      [effects]
 *
 * Modifies:
 *      [modifies]
 *
 */
krb5_error_code
osa_adb_put_policy(osa_adb_policy_t db, osa_policy_ent_t entry)
{
    DBT                 dbkey;
    DBT                 dbdata;
    DBT                 tmpdb;
    XDR                 xdrs;
    int                 ret;

    OPENLOCK(db, KRB5_DB_LOCKMODE_EXCLUSIVE);

    if(entry->name == NULL) {
        ret = EINVAL;
        goto error;
    }
    dbkey.data = entry->name;
    dbkey.size = (strlen(entry->name) + 1);
    switch(db->db->get(db->db, &dbkey, &tmpdb, 0)) {
    case 0:
        break;
    case 1:
        ret = OSA_ADB_NOENT;
        goto error;
    default:
        ret = OSA_ADB_FAILURE;
        goto error;
    }
    xdralloc_create(&xdrs, XDR_ENCODE);
    if(!xdr_osa_policy_ent_rec(&xdrs, entry)) {
        xdr_destroy(&xdrs);
        ret = OSA_ADB_XDR_FAILURE;
        goto error;
    }
    dbdata.data = xdralloc_getdata(&xdrs);
    dbdata.size = xdr_getpos(&xdrs);
    switch(db->db->put(db->db, &dbkey, &dbdata, 0)) {
    case 0:
        if((db->db->sync(db->db, 0)) == -1)
            ret = OSA_ADB_FAILURE;
        ret = OSA_ADB_OK;
        break;
    default:
        ret = OSA_ADB_FAILURE;
        break;
    }
    xdr_destroy(&xdrs);

error:
    CLOSELOCK(db);
    return ret;
}

/*
 * Function: osa_adb_iter_policy
 *
 * Purpose: iterate over the policy database.
 *
 * Arguments:
 *      db              (input) db handle
 *      func            (input) function pointer to call
 *      data            opaque data type
 *      <return value>  0 on success error code on failure
 *
 * Requires:
 * Effects:
 * Modifies:
 */
krb5_error_code
osa_adb_iter_policy(osa_adb_policy_t db, osa_adb_iter_policy_func func,
                    void *data)
{
    DBT                     dbkey,
        dbdata;
    XDR                     xdrs;
    int                     ret;
    osa_policy_ent_t        entry;
    char                    *aligned_data;

    OPENLOCK(db, KRB5_DB_LOCKMODE_EXCLUSIVE); /* hmmm */

    if((ret = db->db->seq(db->db, &dbkey, &dbdata, R_FIRST)) == -1) {
        ret = errno;
        goto error;
    }

    while (ret == 0) {
        entry = k5alloc(sizeof(osa_policy_ent_rec), &ret);
        if (entry == NULL)
            goto error;

        aligned_data = k5memdup(dbdata.data, dbdata.size, &ret);
        if (aligned_data == NULL) {
            free(entry);
            goto error;
        }

        xdrmem_create(&xdrs, aligned_data, dbdata.size, XDR_DECODE);
        if(!xdr_osa_policy_ent_rec(&xdrs, entry)) {
            xdr_destroy(&xdrs);
            free(aligned_data);
            osa_free_policy_ent(entry);
            ret = OSA_ADB_FAILURE;
            goto error;
        }
        (*func)(data, entry);
        xdr_destroy(&xdrs);
        free(aligned_data);
        osa_free_policy_ent(entry);
        ret = db->db->seq(db->db, &dbkey, &dbdata, R_NEXT);
    }
    if(ret == -1)
        ret = errno;
    else ret = OSA_ADB_OK;

error:
    CLOSELOCK(db);
    return ret;
}

void
osa_free_policy_ent(osa_policy_ent_t val)
{
    XDR xdrs;

    xdrmem_create(&xdrs, NULL, 0, XDR_FREE);

    xdr_osa_policy_ent_rec(&xdrs, val);

    free(val);
}
