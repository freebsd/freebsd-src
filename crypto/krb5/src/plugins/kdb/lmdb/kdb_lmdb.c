/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/lmdb/klmdb.c - KDB module using LMDB */
/*
 * Copyright (C) 2018 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Thread-safety note: unlike the other two in-tree KDB modules, this module
 * performs no mutex locking to ensure thread safety.  As the KDC and kadmind
 * are single-threaded, and applications are not allowed to access the same
 * krb5_context in multiple threads simultaneously, there is no current need
 * for this code to be thread-safe.  If a need arises in the future, mutex
 * locking should be added around the read_txn and load_txn fields of
 * lmdb_context to ensure that only one thread at a time accesses those
 * transactions.
 */

/*
 * This KDB module stores principal and policy data using LMDB (Lightning
 * Memory-Mapped Database).  We use two LMDB environments, the first to hold
 * the majority of principal and policy data (suffix ".mdb") in the "principal"
 * and "policy" databases, and the second to hold the three non-replicated
 * account lockout attributes (suffix ".lockout.mdb") in the "lockout"
 * database.  The KDC only needs to write to the lockout database.
 *
 * For iteration we create a read transaction in the main environment for the
 * cursor.  Because the iteration callback might need to create its own
 * transactions for write operations (e.g. for kdb5_util
 * update_princ_encryption), we set the MDB_NOTLS flag on the main environment,
 * so that a thread can hold multiple transactions.
 *
 * To mitigate the overhead from MDB_NOTLS, we keep around a read_txn handle
 * in the database context for get operations, using mdb_txn_reset() and
 * mdb_txn_renew() between calls.
 *
 * For database loads, kdb5_util calls the create() method with the "temporary"
 * db_arg, and then promotes the finished contents at the end with the
 * promote_db() method.  In this case we create or open the same LMDB
 * environments as above, open a write_txn handle for the lifetime of the
 * context, and empty out the principal and policy databases.  On promote_db()
 * we commit the transaction.  We do not empty the lockout database and write
 * to it non-transactionally during the load so that we don't block writes by
 * the KDC; this isn't ideal if the load is aborted, but it shouldn't cause any
 * practical issues.
 *
 * For iprop loads, kdb5_util also includes the "merge_nra" db_arg, signifying
 * that the lockout attributes from existing principal entries should be
 * preserved.  This attribute is noted in the LMDB context, and put_principal
 * operations will not write to the lockout database if an existing lockout
 * entry is already present for the principal.
 */

#include "k5-int.h"
#include <kadm5/admin.h>
#include "kdb5.h"
#include "klmdb-int.h"
#include <lmdb.h>

/* The presence of any of these mask bits indicates a change to one of the
 * three principal lockout attributes. */
#define LOCKOUT_MASK (KADM5_LAST_SUCCESS | KADM5_LAST_FAILED |  \
                      KADM5_FAIL_AUTH_COUNT)

/* The default map size (for both environments) in megabytes. */
#define DEFAULT_MAPSIZE 128

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

typedef struct {
    char *path;
    char *lockout_path;
    krb5_boolean temporary;     /* save changes until promote_db */
    krb5_boolean merge_nra;     /* preserve existing lockout attributes */
    krb5_boolean disable_last_success;
    krb5_boolean disable_lockout;
    krb5_boolean nosync;
    size_t mapsize;
    unsigned int maxreaders;

    MDB_env *env;
    MDB_env *lockout_env;
    MDB_dbi princ_db;
    MDB_dbi policy_db;
    MDB_dbi lockout_db;

    /* Used for get operations; each transaction is short-lived but we save the
     * handle between calls to reduce overhead from MDB_NOTLS. */
    MDB_txn *read_txn;

    /* Write transaction for load operations (create() with the "temporary"
     * db_arg).  */
    MDB_txn *load_txn;
} klmdb_context;

static krb5_error_code
klerr(krb5_context context, int err, const char *msg)
{
    krb5_error_code ret;
    klmdb_context *dbc = context->dal_handle->db_context;

    /* Pass through system errors; map MDB errors to a com_err code. */
    ret = (err > 0) ? err : KRB5_KDB_ACCESS_ERROR;

    k5_setmsg(context, ret, _("%s (path: %s): %s"), msg, dbc->path,
              mdb_strerror(err));
    return ret;
}

/* Using db_args and the profile, create a DB context inside context and
 * initialize its configurable parameters. */
static krb5_error_code
configure_context(krb5_context context, const char *conf_section,
                  char *const *db_args)
{
    krb5_error_code ret;
    klmdb_context *dbc;
    char *pval = NULL;
    const char *path = NULL;
    profile_t profile = context->profile;
    int i, bval, ival;

    dbc = k5alloc(sizeof(*dbc), &ret);
    if (dbc == NULL)
        return ret;
    context->dal_handle->db_context = dbc;

    for (i = 0; db_args != NULL && db_args[i] != NULL; i++) {
        if (strcmp(db_args[i], "temporary") == 0) {
            dbc->temporary = TRUE;
        } else if (strcmp(db_args[i], "merge_nra") == 0) {
            dbc->merge_nra = TRUE;
        } else if (strncmp(db_args[i], "dbname=", 7) == 0) {
            path = db_args[i] + 7;
        } else {
            ret = EINVAL;
            k5_setmsg(context, ret, _("Unsupported argument \"%s\" for LMDB"),
                      db_args[i]);
            goto cleanup;
        }
    }

    if (path == NULL) {
        /* Check for database_name in the db_module section. */
        ret = profile_get_string(profile, KDB_MODULE_SECTION, conf_section,
                                 KRB5_CONF_DATABASE_NAME, NULL, &pval);
        if (!ret && pval == NULL) {
            /* For compatibility, check for database_name in the realm. */
            ret = profile_get_string(profile, KDB_REALM_SECTION,
                                     KRB5_DB_GET_REALM(context),
                                     KRB5_CONF_DATABASE_NAME, DEFAULT_KDB_FILE,
                                     &pval);
        }
        if (ret)
            goto cleanup;
        path = pval;
    }

    if (asprintf(&dbc->path, "%s.mdb", path) < 0) {
        dbc->path = NULL;
        ret = ENOMEM;
        goto cleanup;
    }
    if (asprintf(&dbc->lockout_path, "%s.lockout.mdb", path) < 0) {
        dbc->lockout_path = NULL;
        ret = ENOMEM;
        goto cleanup;
    }

    ret = profile_get_boolean(profile, KDB_MODULE_SECTION, conf_section,
                              KRB5_CONF_DISABLE_LAST_SUCCESS, FALSE, &bval);
    if (ret)
        goto cleanup;
    dbc->disable_last_success = bval;

    ret = profile_get_boolean(profile, KDB_MODULE_SECTION, conf_section,
                              KRB5_CONF_DISABLE_LOCKOUT, FALSE, &bval);
    if (ret)
        goto cleanup;
    dbc->disable_lockout = bval;

    ret = profile_get_integer(profile, KDB_MODULE_SECTION, conf_section,
                              KRB5_CONF_MAPSIZE, DEFAULT_MAPSIZE, &ival);
    if (ret)
        goto cleanup;
    dbc->mapsize = (size_t)ival * 1024 * 1024;

    ret = profile_get_integer(profile, KDB_MODULE_SECTION, conf_section,
                              KRB5_CONF_MAX_READERS, 0, &ival);
    if (ret)
        goto cleanup;
    dbc->maxreaders = ival;

    ret = profile_get_boolean(profile, KDB_MODULE_SECTION, conf_section,
                              KRB5_CONF_NOSYNC, FALSE, &bval);
    if (ret)
        goto cleanup;
    dbc->nosync = bval;

cleanup:
    profile_release_string(pval);
    return ret;
}

static krb5_error_code
open_lmdb_env(krb5_context context, klmdb_context *dbc,
              krb5_boolean is_lockout, krb5_boolean readonly,
              MDB_env **env_out)
{
    krb5_error_code ret;
    const char *path = is_lockout ? dbc->lockout_path : dbc->path;
    unsigned int flags;
    MDB_env *env = NULL;
    int err;

    *env_out = NULL;

    err = mdb_env_create(&env);
    if (err)
        goto lmdb_error;

    /* Use a pair of files instead of a subdirectory. */
    flags = MDB_NOSUBDIR;

    /*
     * For the primary database, tie read transaction locktable slots to the
     * transaction and not the thread, so read transactions for iteration
     * cursors can coexist with short-lived transactions for operations invoked
     * by the iteration callback..
     */
    if (!is_lockout)
        flags |= MDB_NOTLS;

    if (readonly)
        flags |= MDB_RDONLY;

    /* Durability for lockout records is never worth the performance penalty.
     * For the primary environment it might be, so we make it configurable. */
    if (is_lockout || dbc->nosync)
        flags |= MDB_NOSYNC;

    /* We use one database in the lockout env, two in the primary env. */
    err = mdb_env_set_maxdbs(env, is_lockout ? 1 : 2);
    if (err)
        goto lmdb_error;

    if (dbc->mapsize) {
        err = mdb_env_set_mapsize(env, dbc->mapsize);
        if (err)
            goto lmdb_error;
    }

    if (dbc->maxreaders) {
        err = mdb_env_set_maxreaders(env, dbc->maxreaders);
        if (err)
            goto lmdb_error;
    }

    err = mdb_env_open(env, path, flags, S_IRUSR | S_IWUSR);
    if (err)
        goto lmdb_error;

    *env_out = env;
    return 0;

lmdb_error:
    ret = klerr(context, err, _("LMDB environment open failure"));
    mdb_env_close(env);
    return ret;
}

/* Read a key from the primary environment, using a saved read transaction from
 * the database context.  Return KRB5_KDB_NOENTRY if the key is not found. */
static krb5_error_code
fetch(krb5_context context, MDB_dbi db, MDB_val *key, MDB_val *val_out)
{
    krb5_error_code ret = 0;
    klmdb_context *dbc = context->dal_handle->db_context;
    int err;

    if (dbc->read_txn == NULL)
        err = mdb_txn_begin(dbc->env, NULL, MDB_RDONLY, &dbc->read_txn);
    else
        err = mdb_txn_renew(dbc->read_txn);

    if (!err)
        err = mdb_get(dbc->read_txn, db, key, val_out);

    if (err == MDB_NOTFOUND)
        ret = KRB5_KDB_NOENTRY;
    else if (err)
        ret = klerr(context, err, _("LMDB read failure"));

    mdb_txn_reset(dbc->read_txn);
    return ret;
}

/* If we are using a lockout database, try to fetch the lockout attributes for
 * key and set them in entry. */
static void
fetch_lockout(krb5_context context, MDB_val *key, krb5_db_entry *entry)
{
    klmdb_context *dbc = context->dal_handle->db_context;
    MDB_txn *txn = NULL;
    MDB_val val;
    int err;

    if (dbc->lockout_env == NULL)
        return;
    err = mdb_txn_begin(dbc->lockout_env, NULL, MDB_RDONLY, &txn);
    if (!err)
        err = mdb_get(txn, dbc->lockout_db, key, &val);
    if (!err && val.mv_size >= LOCKOUT_RECORD_LEN)
        klmdb_decode_princ_lockout(context, entry, val.mv_data);
    mdb_txn_abort(txn);
}

/*
 * Store a value for key in the specified database within the primary
 * environment.  Use the saved load transaction if one is present, or a
 * temporary write transaction if not.  If no_overwrite is true and the key
 * already exists, return KRB5_KDB_INUSE.  If must_overwrite is true and the
 * key does not already exist, return KRB5_KDB_NOENTRY.
 */
static krb5_error_code
put(krb5_context context, MDB_dbi db, char *keystr, uint8_t *bytes, size_t len,
    krb5_boolean no_overwrite, krb5_boolean must_overwrite)
{
    klmdb_context *dbc = context->dal_handle->db_context;
    unsigned int putflags = no_overwrite ? MDB_NOOVERWRITE : 0;
    MDB_txn *temp_txn = NULL, *txn;
    MDB_val key = { strlen(keystr), keystr }, val = { len, bytes }, dummy;
    int err;

    if (dbc->load_txn != NULL) {
        txn = dbc->load_txn;
    } else {
        err = mdb_txn_begin(dbc->env, NULL, 0, &temp_txn);
        if (err)
            goto error;
        txn = temp_txn;
    }

    if (must_overwrite && mdb_get(txn, db, &key, &dummy) == MDB_NOTFOUND) {
        mdb_txn_abort(temp_txn);
        return KRB5_KDB_NOENTRY;
    }

    err = mdb_put(txn, db, &key, &val, putflags);
    if (err)
        goto error;

    if (temp_txn != NULL) {
        err = mdb_txn_commit(temp_txn);
        temp_txn = NULL;
        if (err)
            goto error;
    }

    return 0;

error:
    mdb_txn_abort(temp_txn);
    if (err == MDB_KEYEXIST)
        return KRB5_KDB_INUSE;
    else
        return klerr(context, err, _("LMDB write failure"));
}

/* Delete an entry from the specified env and database, using a temporary write
 * transaction.  Return KRB5_KDB_NOENTRY if the key does not exist. */
static krb5_error_code
del(krb5_context context, MDB_env *env, MDB_dbi db, char *keystr)
{
    krb5_error_code ret = 0;
    MDB_txn *txn = NULL;
    MDB_val key = { strlen(keystr), keystr };
    int err;

    err = mdb_txn_begin(env, NULL, 0, &txn);
    if (!err)
        err = mdb_del(txn, db, &key, NULL);
    if (!err) {
        err = mdb_txn_commit(txn);
        txn = NULL;
    }

    if (err == MDB_NOTFOUND)
        ret = KRB5_KDB_NOENTRY;
    else if (err)
        ret = klerr(context, err, _("LMDB delete failure"));

    mdb_txn_abort(txn);
    return ret;
}

/* Zero out and unlink filename. */
static krb5_error_code
destroy_file(const char *filename)
{
    krb5_error_code ret;
    struct stat st;
    ssize_t len;
    off_t pos;
    uint8_t buf[BUFSIZ], zbuf[BUFSIZ] = { 0 };
    int fd;

    fd = open(filename, O_RDWR | O_CLOEXEC, 0);
    if (fd < 0)
        return errno;
    set_cloexec_fd(fd);
    if (fstat(fd, &st) == -1)
        goto error;

    memset(zbuf, 0, BUFSIZ);
    pos = 0;
    while (pos < st.st_size) {
        len = read(fd, buf, BUFSIZ);
        if (len < 0)
            goto error;
        /* Only rewrite the block if it's not already zeroed, in case the file
         * is sparse. */
        if (memcmp(buf, zbuf, len) != 0) {
            (void)lseek(fd, pos, SEEK_SET);
            len = write(fd, zbuf, len);
            if (len < 0)
                goto error;
        }
        pos += len;
    }
    close(fd);

    if (unlink(filename) != 0)
        return errno;
    return 0;

error:
    ret = errno;
    close(fd);
    return ret;
}

static krb5_error_code
klmdb_lib_init()
{
    return 0;
}

static krb5_error_code
klmdb_lib_cleanup()
{
    return 0;
}

static krb5_error_code
klmdb_fini(krb5_context context)
{
    klmdb_context *dbc;

    dbc = context->dal_handle->db_context;
    if (dbc == NULL)
        return 0;
    mdb_txn_abort(dbc->read_txn);
    mdb_txn_abort(dbc->load_txn);
    mdb_env_close(dbc->env);
    mdb_env_close(dbc->lockout_env);
    free(dbc->path);
    free(dbc->lockout_path);
    free(dbc);
    context->dal_handle->db_context = NULL;
    return 0;
}

static krb5_error_code
klmdb_open(krb5_context context, char *conf_section, char **db_args, int mode)
{
    krb5_error_code ret;
    klmdb_context *dbc;
    krb5_boolean readonly;
    MDB_txn *txn = NULL;
    struct stat st;
    int err;

    if (context->dal_handle->db_context != NULL)
        return 0;

    ret = configure_context(context, conf_section, db_args);
    if (ret)
        return ret;
    dbc = context->dal_handle->db_context;

    if (stat(dbc->path, &st) != 0) {
        ret = ENOENT;
        k5_setmsg(context, ret, _("LMDB file %s does not exist"), dbc->path);
        goto error;
    }

    /* Open the primary environment and databases.  The KDC can open this
     * environment read-only. */
    readonly = (mode & KRB5_KDB_OPEN_RO) || (mode & KRB5_KDB_SRV_TYPE_KDC);
    ret = open_lmdb_env(context, dbc, FALSE, readonly, &dbc->env);
    if (ret)
        goto error;
    err = mdb_txn_begin(dbc->env, NULL, MDB_RDONLY, &txn);
    if (err)
        goto lmdb_error;
    err = mdb_dbi_open(txn, "principal", 0, &dbc->princ_db);
    if (err)
        goto lmdb_error;
    err = mdb_dbi_open(txn, "policy", 0, &dbc->policy_db);
    if (err)
        goto lmdb_error;
    err = mdb_txn_commit(txn);
    txn = NULL;
    if (err)
        goto lmdb_error;

    /* Open the lockout environment and database if we will need it. */
    if (!dbc->disable_last_success || !dbc->disable_lockout) {
        readonly = !!(mode & KRB5_KDB_OPEN_RO);
        ret = open_lmdb_env(context, dbc, TRUE, readonly, &dbc->lockout_env);
        if (ret)
            goto error;
        err = mdb_txn_begin(dbc->lockout_env, NULL, MDB_RDONLY, &txn);
        if (err)
            goto lmdb_error;
        err = mdb_dbi_open(txn, "lockout", 0, &dbc->lockout_db);
        if (err)
            goto lmdb_error;
        err = mdb_txn_commit(txn);
        txn = NULL;
        if (err)
            goto lmdb_error;
    }

    return 0;

lmdb_error:
    ret = klerr(context, err, _("LMDB open failure"));
error:
    mdb_txn_abort(txn);
    klmdb_fini(context);
    return ret;
}

static krb5_error_code
klmdb_create(krb5_context context, char *conf_section, char **db_args)
{
    krb5_error_code ret;
    klmdb_context *dbc;
    MDB_txn *txn = NULL;
    struct stat st;
    int err;

    if (context->dal_handle->db_context != NULL)
        return 0;

    ret = configure_context(context, conf_section, db_args);
    if (ret)
        return ret;
    dbc = context->dal_handle->db_context;

    if (!dbc->temporary) {
        if (stat(dbc->path, &st) == 0) {
            ret = ENOENT;
            k5_setmsg(context, ret, _("LMDB file %s already exists"),
                      dbc->path);
            goto error;
        }
    }

    /* Open (and create if necessary) the LMDB environments. */
    ret = open_lmdb_env(context, dbc, FALSE, FALSE, &dbc->env);
    if (ret)
        goto error;
    ret = open_lmdb_env(context, dbc, TRUE, FALSE, &dbc->lockout_env);
    if (ret)
        goto error;

    /* Open the primary databases, creating them if they don't exist. */
    err = mdb_txn_begin(dbc->env, NULL, 0, &txn);
    if (err)
        goto lmdb_error;
    err = mdb_dbi_open(txn, "principal", MDB_CREATE, &dbc->princ_db);
    if (err)
        goto lmdb_error;
    err = mdb_dbi_open(txn, "policy", MDB_CREATE, &dbc->policy_db);
    if (err)
        goto lmdb_error;
    err = mdb_txn_commit(txn);
    txn = NULL;
    if (err)
        goto lmdb_error;

    /* Create the lockout database if it doesn't exist. */
    err = mdb_txn_begin(dbc->lockout_env, NULL, 0, &txn);
    if (err)
        goto lmdb_error;
    err = mdb_dbi_open(txn, "lockout", MDB_CREATE, &dbc->lockout_db);
    if (err)
        goto lmdb_error;
    err = mdb_txn_commit(txn);
    txn = NULL;
    if (err)
        goto lmdb_error;

    if (dbc->temporary) {
        /* Create a load transaction and empty the primary databases within
         * it. */
        err = mdb_txn_begin(dbc->env, NULL, 0, &dbc->load_txn);
        if (err)
            goto lmdb_error;
        err = mdb_drop(dbc->load_txn, dbc->princ_db, 0);
        if (err)
            goto lmdb_error;
        err = mdb_drop(dbc->load_txn, dbc->policy_db, 0);
        if (err)
            goto lmdb_error;
    }

    /* Close the lockout environment if we won't need it. */
    if (dbc->disable_last_success && dbc->disable_lockout) {
        mdb_env_close(dbc->lockout_env);
        dbc->lockout_env = NULL;
        dbc->lockout_db = 0;
    }

    return 0;

lmdb_error:
    ret = klerr(context, err, _("LMDB create error"));
error:
    mdb_txn_abort(txn);
    klmdb_fini(context);
    return ret;
}

/* Unlink the "-lock" extension of path. */
static krb5_error_code
unlink_lock_file(krb5_context context, const char *path)
{
    char *lock_path;
    int st;

    if (asprintf(&lock_path, "%s-lock", path) < 0)
        return ENOMEM;
    st = unlink(lock_path);
    if (st)
        k5_prependmsg(context, st, _("Could not unlink %s"), lock_path);
    free(lock_path);
    return st;
}

static krb5_error_code
klmdb_destroy(krb5_context context, char *conf_section, char **db_args)
{
    krb5_error_code ret;
    klmdb_context *dbc;

    if (context->dal_handle->db_context != NULL)
        klmdb_fini(context);
    ret = configure_context(context, conf_section, db_args);
    if (ret)
        goto cleanup;
    dbc = context->dal_handle->db_context;

    ret = destroy_file(dbc->path);
    if (ret)
        goto cleanup;
    ret = unlink_lock_file(context, dbc->path);
    if (ret)
        goto cleanup;

    ret = destroy_file(dbc->lockout_path);
    if (ret)
        goto cleanup;
    ret = unlink_lock_file(context, dbc->lockout_path);

cleanup:
    klmdb_fini(context);
    return ret;
}

static krb5_error_code
klmdb_get_principal(krb5_context context, krb5_const_principal searchfor,
                    unsigned int flags, krb5_db_entry **entry_out)
{
    krb5_error_code ret;
    klmdb_context *dbc = context->dal_handle->db_context;
    MDB_val key, val;
    char *name = NULL;

    *entry_out = NULL;
    if (dbc == NULL)
        return KRB5_KDB_DBNOTINITED;

    ret = krb5_unparse_name(context, searchfor, &name);
    if (ret)
        goto cleanup;

    key.mv_data = name;
    key.mv_size = strlen(name);
    ret = fetch(context, dbc->princ_db, &key, &val);
    if (ret)
        goto cleanup;

    ret = klmdb_decode_princ(context, name, strlen(name),
                             val.mv_data, val.mv_size, entry_out);
    if (ret)
        goto cleanup;

    fetch_lockout(context, &key, *entry_out);

cleanup:
    krb5_free_unparsed_name(context, name);
    return ret;
}

static krb5_error_code
klmdb_put_principal(krb5_context context, krb5_db_entry *entry, char **db_args)
{
    krb5_error_code ret;
    klmdb_context *dbc = context->dal_handle->db_context;
    MDB_val key, val, dummy;
    MDB_txn *txn = NULL;
    uint8_t lockout[LOCKOUT_RECORD_LEN], *enc;
    size_t len;
    char *name = NULL;
    int err;

    if (db_args != NULL) {
        /* This module does not support DB arguments for put_principal. */
        k5_setmsg(context, EINVAL, _("Unsupported argument \"%s\" for lmdb"),
                  db_args[0]);
        return EINVAL;
    }

    if (dbc == NULL)
        return KRB5_KDB_DBNOTINITED;

    ret = krb5_unparse_name(context, entry->princ, &name);
    if (ret)
        goto cleanup;

    ret = klmdb_encode_princ(context, entry, &enc, &len);
    if (ret)
        goto cleanup;
    ret = put(context, dbc->princ_db, name, enc, len, FALSE, FALSE);
    free(enc);
    if (ret)
        goto cleanup;

    /*
     * Write the lockout attributes to the lockout database if we are using
     * one.  During a load operation, changes to lockout attributes will become
     * visible before the load is finished, which is an acceptable compromise
     * on load atomicity.
     */
    if (dbc->lockout_env != NULL &&
        (entry->mask & (LOCKOUT_MASK | KADM5_PRINCIPAL))) {
        key.mv_data = name;
        key.mv_size = strlen(name);
        klmdb_encode_princ_lockout(context, entry, lockout);
        val.mv_data = lockout;
        val.mv_size = sizeof(lockout);
        err = mdb_txn_begin(dbc->lockout_env, NULL, 0, &txn);
        if (!err && dbc->merge_nra) {
            /* During an iprop load, do not change existing lockout entries. */
            if (mdb_get(txn, dbc->lockout_db, &key, &dummy) == 0)
                goto cleanup;
        }
        if (!err)
            err = mdb_put(txn, dbc->lockout_db, &key, &val, 0);
        if (!err) {
            err = mdb_txn_commit(txn);
            txn = NULL;
        }
        if (err) {
            ret = klerr(context, err, _("LMDB lockout write failure"));
            goto cleanup;
        }
    }

cleanup:
    mdb_txn_abort(txn);
    krb5_free_unparsed_name(context, name);
    return ret;
}

static krb5_error_code
klmdb_delete_principal(krb5_context context, krb5_const_principal searchfor)
{
    krb5_error_code ret;
    klmdb_context *dbc = context->dal_handle->db_context;
    char *name;

    if (dbc == NULL)
        return KRB5_KDB_DBNOTINITED;

    ret = krb5_unparse_name(context, searchfor, &name);
    if (ret)
        return ret;

    ret = del(context, dbc->env, dbc->princ_db, name);
    if (!ret && dbc->lockout_env != NULL)
        (void)del(context, dbc->lockout_env, dbc->lockout_db, name);

    krb5_free_unparsed_name(context, name);
    return ret;
}

static krb5_error_code
klmdb_iterate(krb5_context context, char *match_expr,
              krb5_error_code (*func)(void *, krb5_db_entry *), void *arg,
              krb5_flags iterflags)
{
    krb5_error_code ret;
    klmdb_context *dbc = context->dal_handle->db_context;
    krb5_db_entry *entry;
    MDB_txn *txn = NULL;
    MDB_cursor *cursor = NULL;
    MDB_val key, val;
    MDB_cursor_op op = (iterflags & KRB5_DB_ITER_REV) ? MDB_PREV : MDB_NEXT;
    int err;

    if (dbc == NULL)
        return KRB5_KDB_DBNOTINITED;

    err = mdb_txn_begin(dbc->env, NULL, MDB_RDONLY, &txn);
    if (err)
        goto lmdb_error;
    err = mdb_cursor_open(txn, dbc->princ_db, &cursor);
    if (err)
        goto lmdb_error;
    for (;;) {
        err = mdb_cursor_get(cursor, &key, &val, op);
        if (err == MDB_NOTFOUND)
            break;
        if (err)
            goto lmdb_error;
        ret = klmdb_decode_princ(context, key.mv_data, key.mv_size,
                                 val.mv_data, val.mv_size, &entry);
        if (ret)
            goto cleanup;
        fetch_lockout(context, &key, entry);
        ret = (*func)(arg, entry);
        krb5_db_free_principal(context, entry);
        if (ret)
            goto cleanup;
    }
    ret = 0;
    goto cleanup;

lmdb_error:
    ret = klerr(context, err, _("LMDB principal iteration failure"));
cleanup:
    mdb_cursor_close(cursor);
    mdb_txn_abort(txn);
    return ret;
}

krb5_error_code
klmdb_get_policy(krb5_context context, char *name, osa_policy_ent_t *policy)
{
    krb5_error_code ret;
    klmdb_context *dbc = context->dal_handle->db_context;
    MDB_val key, val;

    *policy = NULL;
    if (dbc == NULL)
        return KRB5_KDB_DBNOTINITED;

    key.mv_data = name;
    key.mv_size = strlen(name);
    ret = fetch(context, dbc->policy_db, &key, &val);
    if (ret)
        return ret;
    return klmdb_decode_policy(context, name, strlen(name),
                               val.mv_data, val.mv_size, policy);
}

static krb5_error_code
klmdb_create_policy(krb5_context context, osa_policy_ent_t policy)
{
    krb5_error_code ret;
    klmdb_context *dbc = context->dal_handle->db_context;
    uint8_t *enc;
    size_t len;

    if (dbc == NULL)
        return KRB5_KDB_DBNOTINITED;

    ret = klmdb_encode_policy(context, policy, &enc, &len);
    if (ret)
        return ret;
    ret = put(context, dbc->policy_db, policy->name, enc, len, TRUE, FALSE);
    free(enc);
    return ret;
}

static krb5_error_code
klmdb_put_policy(krb5_context context, osa_policy_ent_t policy)
{
    krb5_error_code ret;
    klmdb_context *dbc = context->dal_handle->db_context;
    uint8_t *enc;
    size_t len;

    if (dbc == NULL)
        return KRB5_KDB_DBNOTINITED;

    ret = klmdb_encode_policy(context, policy, &enc, &len);
    if (ret)
        return ret;
    ret = put(context, dbc->policy_db, policy->name, enc, len, FALSE, TRUE);
    free(enc);
    return ret;
}

static krb5_error_code
klmdb_iter_policy(krb5_context context, char *match_entry,
                  osa_adb_iter_policy_func func, void *arg)
{
    krb5_error_code ret;
    klmdb_context *dbc = context->dal_handle->db_context;
    osa_policy_ent_t pol;
    MDB_txn *txn = NULL;
    MDB_cursor *cursor = NULL;
    MDB_val key, val;
    int err;

    if (dbc == NULL)
        return KRB5_KDB_DBNOTINITED;

    err = mdb_txn_begin(dbc->env, NULL, MDB_RDONLY, &txn);
    if (err)
        goto lmdb_error;
    err = mdb_cursor_open(txn, dbc->policy_db, &cursor);
    if (err)
        goto lmdb_error;
    for (;;) {
        err = mdb_cursor_get(cursor, &key, &val, MDB_NEXT);
        if (err == MDB_NOTFOUND)
            break;
        if (err)
            goto lmdb_error;
        ret = klmdb_decode_policy(context, key.mv_data, key.mv_size,
                                  val.mv_data, val.mv_size, &pol);
        if (ret)
            goto cleanup;
        (*func)(arg, pol);
        krb5_db_free_policy(context, pol);
    }
    ret = 0;
    goto cleanup;

lmdb_error:
    ret = klerr(context, err, _("LMDB policy iteration failure"));
cleanup:
    mdb_cursor_close(cursor);
    mdb_txn_abort(txn);
    return ret;
}

static krb5_error_code
klmdb_delete_policy(krb5_context context, char *policy)
{
    klmdb_context *dbc = context->dal_handle->db_context;

    if (dbc == NULL)
        return KRB5_KDB_DBNOTINITED;
    return del(context, dbc->env, dbc->policy_db, policy);
}

static krb5_error_code
klmdb_promote_db(krb5_context context, char *conf_section, char **db_args)
{
    krb5_error_code ret = 0;
    klmdb_context *dbc = context->dal_handle->db_context;
    int err;

    if (dbc == NULL)
        return KRB5_KDB_DBNOTINITED;
    if (dbc->load_txn == NULL)
        return EINVAL;
    err = mdb_txn_commit(dbc->load_txn);
    dbc->load_txn = NULL;
    if (err)
        ret = klerr(context, err, _("LMDB transaction commit failure"));
    klmdb_fini(context);
    return ret;
}

static krb5_error_code
klmdb_check_policy_as(krb5_context context, krb5_kdc_req *request,
                      krb5_db_entry *client, krb5_db_entry *server,
                      krb5_timestamp kdc_time, const char **status,
                      krb5_pa_data ***e_data)
{
    krb5_error_code ret;
    klmdb_context *dbc = context->dal_handle->db_context;

    if (dbc->disable_lockout)
        return 0;

    ret = klmdb_lockout_check_policy(context, client, kdc_time);
    if (ret == KRB5KDC_ERR_CLIENT_REVOKED)
        *status = "LOCKED_OUT";
    return ret;
}

static void
klmdb_audit_as_req(krb5_context context, krb5_kdc_req *request,
                   const krb5_address *local_addr,
                   const krb5_address *remote_addr, krb5_db_entry *client,
                   krb5_db_entry *server, krb5_timestamp authtime,
                   krb5_error_code status)
{
    klmdb_context *dbc = context->dal_handle->db_context;

    (void)klmdb_lockout_audit(context, client, authtime, status,
                              dbc->disable_last_success, dbc->disable_lockout);
}

krb5_error_code
klmdb_update_lockout(krb5_context context, krb5_db_entry *entry,
                     krb5_timestamp stamp, krb5_boolean zero_fail_count,
                     krb5_boolean set_last_success,
                     krb5_boolean set_last_failure)
{
    krb5_error_code ret;
    klmdb_context *dbc = context->dal_handle->db_context;
    krb5_db_entry dummy = { 0 };
    uint8_t lockout[LOCKOUT_RECORD_LEN];
    MDB_txn *txn = NULL;
    MDB_val key, val;
    char *name = NULL;
    int err;

    if (dbc == NULL)
        return KRB5_KDB_DBNOTINITED;
    if (dbc->lockout_env == NULL)
        return 0;
    if (!zero_fail_count && !set_last_success && !set_last_failure)
        return 0;

    ret = krb5_unparse_name(context, entry->princ, &name);
    if (ret)
        goto cleanup;
    key.mv_data = name;
    key.mv_size = strlen(name);

    err = mdb_txn_begin(dbc->lockout_env, NULL, 0, &txn);
    if (err)
        goto lmdb_error;
    /* Fetch base lockout info within txn so we update transactionally. */
    err = mdb_get(txn, dbc->lockout_db, &key, &val);
    if (!err && val.mv_size >= LOCKOUT_RECORD_LEN) {
        klmdb_decode_princ_lockout(context, &dummy, val.mv_data);
    } else {
        dummy.last_success = entry->last_success;
        dummy.last_failed = entry->last_failed;
        dummy.fail_auth_count = entry->fail_auth_count;
    }

    if (zero_fail_count)
        dummy.fail_auth_count = 0;
    if (set_last_success)
        dummy.last_success = stamp;
    if (set_last_failure) {
        dummy.last_failed = stamp;
        dummy.fail_auth_count++;
    }

    klmdb_encode_princ_lockout(context, &dummy, lockout);
    val.mv_data = lockout;
    val.mv_size = sizeof(lockout);
    err = mdb_put(txn, dbc->lockout_db, &key, &val, 0);
    if (err)
        goto lmdb_error;
    err = mdb_txn_commit(txn);
    txn = NULL;
    if (err)
        goto lmdb_error;
    goto cleanup;

lmdb_error:
    ret = klerr(context, err, _("LMDB lockout update failure"));
cleanup:
    krb5_free_unparsed_name(context, name);
    mdb_txn_abort(txn);
    return 0;
}

kdb_vftabl PLUGIN_SYMBOL_NAME(krb5_lmdb, kdb_function_table) = {
    .maj_ver = KRB5_KDB_DAL_MAJOR_VERSION,
    .min_ver = 0,
    .init_library = klmdb_lib_init,
    .fini_library = klmdb_lib_cleanup,
    .init_module = klmdb_open,
    .fini_module = klmdb_fini,
    .create = klmdb_create,
    .destroy = klmdb_destroy,
    .get_principal = klmdb_get_principal,
    .put_principal = klmdb_put_principal,
    .delete_principal = klmdb_delete_principal,
    .iterate = klmdb_iterate,
    .create_policy = klmdb_create_policy,
    .get_policy = klmdb_get_policy,
    .put_policy = klmdb_put_policy,
    .iter_policy = klmdb_iter_policy,
    .delete_policy = klmdb_delete_policy,
    .promote_db = klmdb_promote_db,
    .check_policy_as = klmdb_check_policy_as,
    .audit_as_req = klmdb_audit_as_req
};
