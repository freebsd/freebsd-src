/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/db2/kdb_db2.c */
/*
 * Copyright 1997,2006,2007-2009 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 */

/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "k5-int.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <db.h>
#include <stdio.h>
#include <errno.h>
#include <utime.h>
#include "kdb5.h"
#include "kdb_db2.h"
#include "kdb_xdr.h"
#include "policy_db.h"

#define KDB_DB2_DATABASE_NAME "database_name"

#define SUFFIX_DB ""
#define SUFFIX_LOCK ".ok"
#define SUFFIX_POLICY ".kadm5"
#define SUFFIX_POLICY_LOCK ".kadm5.lock"

/*
 * Locking:
 *
 * There are two distinct locking protocols used.  One is designed to
 * lock against processes (the admin_server, for one) which make
 * incremental changes to the database; the other is designed to lock
 * against utilities (kdb5_edit, kpropd, kdb5_convert) which replace the
 * entire database in one fell swoop.
 *
 * The first locking protocol is implemented using flock() in the
 * krb_dbl_lock() and krb_dbl_unlock routines.
 *
 * The second locking protocol is necessary because DBM "files" are
 * actually implemented as two separate files, and it is impossible to
 * atomically rename two files simultaneously.  It assumes that the
 * database is replaced only very infrequently in comparison to the time
 * needed to do a database read operation.
 *
 * A third file is used as a "version" semaphore; the modification
 * time of this file is the "version number" of the database.
 * At the start of a read operation, the reader checks the version
 * number; at the end of the read operation, it checks again.  If the
 * version number changed, or if the semaphore was nonexistant at
 * either time, the reader sleeps for a second to let things
 * stabilize, and then tries again; if it does not succeed after
 * KRB5_DBM_MAX_RETRY attempts, it gives up.
 *
 * On update, the semaphore file is deleted (if it exists) before any
 * update takes place; at the end of the update, it is replaced, with
 * a version number strictly greater than the version number which
 * existed at the start of the update.
 *
 * If the system crashes in the middle of an update, the semaphore
 * file is not automatically created on reboot; this is a feature, not
 * a bug, since the database may be inconsistant.  Note that the
 * absence of a semaphore file does not prevent another _update_ from
 * taking place later.  Database replacements take place automatically
 * only on slave servers; a crash in the middle of an update will be
 * fixed by the next slave propagation.  A crash in the middle of an
 * update on the master would be somewhat more serious, but this would
 * likely be noticed by an administrator, who could fix the problem and
 * retry the operation.
 */

/* Evaluate to true if the krb5_context c contains an initialized db2
 * context. */
#define inited(c) ((c)->dal_handle->db_context &&                       \
                   ((krb5_db2_context *)(c)->dal_handle->db_context)->  \
                   db_inited)

static krb5_error_code
get_db_opt(char *input, char **opt, char **val)
{
    char   *pos = strchr(input, '=');
    if (pos == NULL) {
        *opt = NULL;
        *val = strdup(input);
        if (*val == NULL) {
            return ENOMEM;
        }
    } else {
        *opt = malloc((pos - input) + 1);
        *val = strdup(pos + 1);
        if (!*opt || !*val) {
            free(*opt);
            *opt = NULL;
            free(*val);
            *val = NULL;
            return ENOMEM;
        }
        memcpy(*opt, input, pos - input);
        (*opt)[pos - input] = '\0';
    }
    return (0);

}

/* Restore dbctx to the uninitialized state. */
static void
ctx_clear(krb5_db2_context *dbc)
{
    /*
     * Free any dynamically allocated memory.  File descriptors and locks
     * are the caller's problem.
     */
    free(dbc->db_lf_name);
    free(dbc->db_name);
    /*
     * Clear the structure and reset the defaults.
     */
    memset(dbc, 0, sizeof(krb5_db2_context));
    dbc->db = NULL;
    dbc->db_lf_name = NULL;
    dbc->db_lf_file = -1;
    dbc->db_name = NULL;
    dbc->db_nb_locks = FALSE;
    dbc->tempdb = FALSE;
}

/* Set *dbc_out to the db2 database context for context.  If one does not
 * exist, create one in the uninitialized state. */
static krb5_error_code
ctx_get(krb5_context context, krb5_db2_context **dbc_out)
{
    krb5_db2_context *dbc;
    kdb5_dal_handle *dal_handle;

    dal_handle = context->dal_handle;

    if (dal_handle->db_context == NULL) {
        dbc = (krb5_db2_context *) malloc(sizeof(krb5_db2_context));
        if (dbc == NULL)
            return ENOMEM;
        else {
            memset(dbc, 0, sizeof(krb5_db2_context));
            ctx_clear(dbc);
            dal_handle->db_context = dbc;
        }
    }
    *dbc_out = dal_handle->db_context;
    return 0;
}

/* Using db_args and the profile, initialize the configurable parameters of the
 * DB context inside context. */
static krb5_error_code
configure_context(krb5_context context, char *conf_section, char **db_args)
{
    krb5_error_code status;
    krb5_db2_context *dbc;
    char **t_ptr, *opt = NULL, *val = NULL, *pval = NULL;
    profile_t profile = KRB5_DB_GET_PROFILE(context);
    int bval;

    status = ctx_get(context, &dbc);
    if (status != 0)
        return status;

    /* Allow unlockiter to be overridden by command line db_args. */
    status = profile_get_boolean(profile, KDB_MODULE_SECTION, conf_section,
                                 KRB5_CONF_UNLOCKITER, FALSE, &bval);
    if (status != 0)
        goto cleanup;
    dbc->unlockiter = bval;

    for (t_ptr = db_args; t_ptr && *t_ptr; t_ptr++) {
        free(opt);
        free(val);
        status = get_db_opt(*t_ptr, &opt, &val);
        if (opt && !strcmp(opt, "dbname")) {
            dbc->db_name = strdup(val);
            if (dbc->db_name == NULL) {
                status = ENOMEM;
                goto cleanup;
            }
        }
        else if (!opt && !strcmp(val, "temporary")) {
            dbc->tempdb = 1;
        } else if (!opt && !strcmp(val, "merge_nra")) {
            ;
        } else if (opt && !strcmp(opt, "hash")) {
            dbc->hashfirst = TRUE;
        } else if (!opt && !strcmp(val, "unlockiter")) {
            dbc->unlockiter = TRUE;
        } else if (!opt && !strcmp(val, "lockiter")) {
            dbc->unlockiter = FALSE;
        } else {
            status = EINVAL;
            k5_setmsg(context, status,
                      _("Unsupported argument \"%s\" for db2"),
                      opt ? opt : val);
            goto cleanup;
        }
    }

    if (dbc->db_name == NULL) {
        /* Check for database_name in the db_module section. */
        status = profile_get_string(profile, KDB_MODULE_SECTION, conf_section,
                                    KDB_DB2_DATABASE_NAME, NULL, &pval);
        if (status == 0 && pval == NULL) {
            /* For compatibility, check for database_name in the realm. */
            status = profile_get_string(profile, KDB_REALM_SECTION,
                                        KRB5_DB_GET_REALM(context),
                                        KDB_DB2_DATABASE_NAME,
                                        DEFAULT_KDB_FILE, &pval);
        }
        if (status != 0)
            goto cleanup;
        dbc->db_name = strdup(pval);
    }

    status = profile_get_boolean(profile, KDB_MODULE_SECTION, conf_section,
                                 KRB5_CONF_DISABLE_LAST_SUCCESS, FALSE, &bval);
    if (status != 0)
        goto cleanup;
    dbc->disable_last_success = bval;

    status = profile_get_boolean(profile, KDB_MODULE_SECTION, conf_section,
                                 KRB5_CONF_DISABLE_LOCKOUT, FALSE, &bval);
    if (status != 0)
        goto cleanup;
    dbc->disable_lockout = bval;

cleanup:
    free(opt);
    free(val);
    profile_release_string(pval);
    return status;
}

/*
 * Set *out to one of the filenames used for the DB described by dbc.  sfx
 * should be one of SUFFIX_DB, SUFFIX_LOCK, SUFFIX_POLICY, or
 * SUFFIX_POLICY_LOCK.
 */
static krb5_error_code
ctx_dbsuffix(krb5_db2_context *dbc, const char *sfx, char **out)
{
    char *result;
    const char *tilde;

    *out = NULL;
    tilde = dbc->tempdb ? "~" : "";
    if (asprintf(&result, "%s%s%s", dbc->db_name, tilde, sfx) < 0)
        return ENOMEM;
    *out = result;
    return 0;
}

/* Generate all four files corresponding to dbc. */
static krb5_error_code
ctx_allfiles(krb5_db2_context *dbc, char **dbname_out, char **lockname_out,
             char **polname_out, char **plockname_out)
{
    char *a = NULL, *b = NULL, *c = NULL, *d = NULL;

    *dbname_out = *lockname_out = *polname_out = *plockname_out = NULL;
    if (ctx_dbsuffix(dbc, SUFFIX_DB, &a))
        goto error;
    if (ctx_dbsuffix(dbc, SUFFIX_LOCK, &b))
        goto error;
    if (ctx_dbsuffix(dbc, SUFFIX_POLICY, &c))
        goto error;
    if (ctx_dbsuffix(dbc, SUFFIX_POLICY_LOCK, &d))
        goto error;
    *dbname_out = a;
    *lockname_out = b;
    *polname_out = c;
    *plockname_out = d;
    return 0;
error:
    free(a);
    free(b);
    free(c);
    free(d);
    return ENOMEM;
}

/*
 * Open the DB2 database described by dbc, using the specified flags and mode,
 * and return the resulting handle.  Try both hash and btree database types;
 * dbc->hashfirst determines which is attempted first.  If dbc->hashfirst
 * indicated the wrong type, update it to indicate the correct type.
 */
static krb5_error_code
open_db(krb5_context context, krb5_db2_context *dbc, int flags, int mode,
        DB **db_out)
{
    char *fname = NULL;
    DB *db;
    BTREEINFO bti;
    HASHINFO hashi;
    bti.flags = 0;
    bti.cachesize = 0;
    bti.psize = 4096;
    bti.lorder = 0;
    bti.minkeypage = 0;
    bti.compare = NULL;
    bti.prefix = NULL;

    *db_out = NULL;

    if (ctx_dbsuffix(dbc, SUFFIX_DB, &fname) != 0)
        return ENOMEM;

    hashi.bsize = 4096;
    hashi.cachesize = 0;
    hashi.ffactor = 40;
    hashi.hash = NULL;
    hashi.lorder = 0;
    hashi.nelem = 1;

    /* Try our best guess at the database type. */
    db = dbopen(fname, flags, mode,
                dbc->hashfirst ? DB_HASH : DB_BTREE,
                dbc->hashfirst ? (void *) &hashi : (void *) &bti);

    if (db == NULL && IS_EFTYPE(errno)) {
        db = dbopen(fname, flags, mode,
                    dbc->hashfirst ? DB_BTREE : DB_HASH,
                    dbc->hashfirst ? (void *) &bti : (void *) &hashi);
        /* If that worked, update our guess for next time. */
        if (db != NULL)
            dbc->hashfirst = !dbc->hashfirst;
    }

    /* Don't try unlocked iteration with a hash database. */
    if (db != NULL && dbc->hashfirst)
        dbc->unlockiter = FALSE;

    if (db == NULL) {
        k5_prependmsg(context, errno, _("Cannot open DB2 database '%s'"),
                      fname);
    }

    *db_out = db;
    free(fname);
    return (db == NULL) ? errno : 0;
}

static krb5_error_code
ctx_unlock(krb5_context context, krb5_db2_context *dbc)
{
    krb5_error_code retval, retval2;
    DB *db;

    retval = osa_adb_release_lock(dbc->policy_db);

    if (!dbc->db_locks_held) /* lock already unlocked */
        return KRB5_KDB_NOTLOCKED;

    db = dbc->db;
    if (--(dbc->db_locks_held) == 0) {
        db->close(db);
        dbc->db = NULL;
        dbc->db_lock_mode = 0;

        retval2 = krb5_lock_file(context, dbc->db_lf_file,
                                KRB5_LOCKMODE_UNLOCK);
        if (retval2)
            return retval2;
    }

    /* We may be unlocking because osa_adb_get_lock() failed. */
    if (retval == OSA_ADB_NOTLOCKED)
        return 0;
    return retval;
}

static krb5_error_code
ctx_lock(krb5_context context, krb5_db2_context *dbc, int lockmode)
{
    krb5_error_code retval;
    int kmode;

    if (lockmode == KRB5_DB_LOCKMODE_PERMANENT ||
        lockmode == KRB5_DB_LOCKMODE_EXCLUSIVE)
        kmode = KRB5_LOCKMODE_EXCLUSIVE;
    else if (lockmode == KRB5_DB_LOCKMODE_SHARED)
        kmode = KRB5_LOCKMODE_SHARED;
    else
        return EINVAL;

    if (dbc->db_locks_held == 0 || dbc->db_lock_mode < kmode) {
        /* Acquire or upgrade the lock. */
        retval = krb5_lock_file(context, dbc->db_lf_file, kmode);
        /* Check if we tried to lock something not open for write. */
        if (retval == EBADF && kmode == KRB5_LOCKMODE_EXCLUSIVE)
            return KRB5_KDB_CANTLOCK_DB;
        else if (retval == EACCES || retval == EAGAIN || retval == EWOULDBLOCK)
            return KRB5_KDB_CANTLOCK_DB;
        else if (retval)
            return retval;

        /* Open the DB (or re-open it for read/write). */
        if (dbc->db != NULL)
            dbc->db->close(dbc->db);
        retval = open_db(context, dbc,
                         kmode == KRB5_LOCKMODE_SHARED ? O_RDONLY : O_RDWR,
                         0600, &dbc->db);
        if (retval) {
            dbc->db_locks_held = 0;
            dbc->db_lock_mode = 0;
            (void) osa_adb_release_lock(dbc->policy_db);
            (void) krb5_lock_file(context, dbc->db_lf_file,
                                  KRB5_LOCKMODE_UNLOCK);
            return retval;
        }

        dbc->db_lock_mode = kmode;
    }
    dbc->db_locks_held++;

    /* Acquire or upgrade the policy lock. */
    retval = osa_adb_get_lock(dbc->policy_db, lockmode);
    if (retval) {
        (void) ctx_unlock(context, dbc);
        if (retval == OSA_ADB_NOEXCL_PERM || retval == OSA_ADB_CANTLOCK_DB ||
            retval == OSA_ADB_NOLOCKFILE)
            retval = KRB5_KDB_CANTLOCK_DB;
    }
    return retval;
}

/* Initialize the lock file and policy database fields of dbc.  The db_name and
 * tempdb fields must already be set. */
static krb5_error_code
ctx_init(krb5_db2_context *dbc)
{
    krb5_error_code retval;
    char *polname = NULL, *plockname = NULL;

    retval = ctx_dbsuffix(dbc, SUFFIX_LOCK, &dbc->db_lf_name);
    if (retval)
        return retval;

    /*
     * should be opened read/write so that write locking can work with
     * POSIX systems
     */
    if ((dbc->db_lf_file = open(dbc->db_lf_name, O_RDWR, 0666)) < 0) {
        if ((dbc->db_lf_file = open(dbc->db_lf_name, O_RDONLY, 0666)) < 0) {
            retval = errno;
            goto cleanup;
        }
    }
    set_cloexec_fd(dbc->db_lf_file);
    dbc->db_inited++;

    retval = ctx_dbsuffix(dbc, SUFFIX_POLICY, &polname);
    if (retval)
        goto cleanup;
    retval = ctx_dbsuffix(dbc, SUFFIX_POLICY_LOCK, &plockname);
    if (retval)
        goto cleanup;
    retval = osa_adb_init_db(&dbc->policy_db, polname, plockname,
                             OSA_ADB_POLICY_DB_MAGIC);

cleanup:
    free(polname);
    free(plockname);
    if (retval)
        ctx_clear(dbc);
    return retval;
}

static void
ctx_fini(krb5_db2_context *dbc)
{
    if (dbc->db_lf_file != -1)
        (void) close(dbc->db_lf_file);
    if (dbc->policy_db)
        (void) osa_adb_fini_db(dbc->policy_db, OSA_ADB_POLICY_DB_MAGIC);
    ctx_clear(dbc);
    free(dbc);
}

krb5_error_code
krb5_db2_fini(krb5_context context)
{
    if (context->dal_handle->db_context != NULL) {
        ctx_fini(context->dal_handle->db_context);
        context->dal_handle->db_context = NULL;
    }
    return 0;
}

/* Return successfully if the db2 name set in context can be opened. */
static krb5_error_code
check_openable(krb5_context context)
{
    krb5_error_code retval;
    DB     *db;
    krb5_db2_context *dbc;

    dbc = context->dal_handle->db_context;
    retval = open_db(context, dbc, O_RDONLY, 0, &db);
    if (retval)
        return retval;
    db->close(db);
    return 0;
}

/*
 * Return the last modification time of the database.
 *
 * Think about using fstat.
 */

krb5_error_code
krb5_db2_get_age(krb5_context context, char *db_name, time_t *age)
{
    krb5_db2_context *dbc;
    struct stat st;

    if (!inited(context))
        return (KRB5_KDB_DBNOTINITED);
    dbc = context->dal_handle->db_context;

    if (fstat(dbc->db_lf_file, &st) < 0)
        *age = -1;
    else
        *age = st.st_mtime;
    return 0;
}

/* Try to update the timestamp on dbc's lockfile. */
static void
ctx_update_age(krb5_db2_context *dbc)
{
    struct stat st;
    time_t now;
    struct utimbuf utbuf;

    now = time((time_t *) NULL);
    if (fstat(dbc->db_lf_file, &st) != 0)
        return;
    if (st.st_mtime >= now) {
        utbuf.actime = st.st_mtime + 1;
        utbuf.modtime = st.st_mtime + 1;
        (void) utime(dbc->db_lf_name, &utbuf);
    } else
        (void) utime(dbc->db_lf_name, (struct utimbuf *) NULL);
}

krb5_error_code
krb5_db2_lock(krb5_context context, int lockmode)
{
    if (!inited(context))
        return KRB5_KDB_DBNOTINITED;
    return ctx_lock(context, context->dal_handle->db_context, lockmode);
}

krb5_error_code
krb5_db2_unlock(krb5_context context)
{
    if (!inited(context))
        return KRB5_KDB_DBNOTINITED;
    return ctx_unlock(context, context->dal_handle->db_context);
}

/* Zero out and unlink filename. */
static krb5_error_code
destroy_file(char *filename)
{
    struct stat statb;
    int dowrite, j, nb, fd, retval;
    off_t pos;
    char buf[BUFSIZ], zbuf[BUFSIZ];

    fd = open(filename, O_RDWR, 0);
    if (fd < 0)
        return errno;
    set_cloexec_fd(fd);
    /* fstat() will probably not fail unless using a remote filesystem
     * (which is inappropriate for the kerberos database) so this check
     * is mostly paranoia.  */
    if (fstat(fd, &statb) == -1)
        goto error;
    /*
     * Stroll through the file, reading in BUFSIZ chunks.  If everything
     * is zero, then we're done for that block, otherwise, zero the block.
     * We would like to just blast through everything, but some DB
     * implementations make holey files and writing data to the holes
     * causes actual blocks to be allocated which is no good, since
     * we're just about to unlink it anyways.
     */
    memset(zbuf, 0, BUFSIZ);
    pos = 0;
    while (pos < statb.st_size) {
        dowrite = 0;
        nb = read(fd, buf, BUFSIZ);
        if (nb < 0)
            goto error;
        for (j = 0; j < nb; j++) {
            if (buf[j] != '\0') {
                dowrite = 1;
                break;
            }
        }
        /* For signedness */
        j = nb;
        if (dowrite) {
            lseek(fd, pos, SEEK_SET);
            nb = write(fd, zbuf, j);
            if (nb < 0)
                goto error;
        }
        pos += nb;
    }
    /* ??? Is fsync really needed?  I don't know of any non-networked
     * filesystem which will discard queued writes to disk if a file
     * is deleted after it is closed.  --jfc */
#ifndef NOFSYNC
    fsync(fd);
#endif
    close(fd);

    if (unlink(filename))
        return errno;
    return 0;

error:
    retval = errno;
    close(fd);
    return retval;
}

/* Initialize dbc by locking and creating the DB.  If the DB already exists,
 * clear it out if dbc->tempdb is set; otherwise return EEXIST. */
static krb5_error_code
ctx_create_db(krb5_context context, krb5_db2_context *dbc)
{
    krb5_error_code retval = 0;
    char *dbname = NULL, *polname = NULL, *plockname = NULL;

    retval = ctx_allfiles(dbc, &dbname, &dbc->db_lf_name, &polname,
                          &plockname);
    if (retval)
        return retval;

    dbc->db_lf_file = open(dbc->db_lf_name, O_CREAT | O_RDWR | O_TRUNC,
                           0600);
    if (dbc->db_lf_file < 0) {
        retval = errno;
        goto cleanup;
    }
    retval = krb5_lock_file(context, dbc->db_lf_file, KRB5_LOCKMODE_EXCLUSIVE);
    if (retval != 0)
        goto cleanup;
    set_cloexec_fd(dbc->db_lf_file);
    dbc->db_lock_mode = KRB5_LOCKMODE_EXCLUSIVE;
    dbc->db_locks_held = 1;

    if (dbc->tempdb) {
        /* Temporary DBs are locked for their whole lifetime.  Since we have
         * the lock, any remnant files can be safely destroyed. */
        (void) destroy_file(dbname);
        (void) unlink(polname);
        (void) unlink(plockname);
    }

    retval = open_db(context, dbc, O_RDWR | O_CREAT | O_EXCL, 0600, &dbc->db);
    if (retval)
        goto cleanup;

    /* Create the policy database, initialize a handle to it, and lock it. */
    retval = osa_adb_create_db(polname, plockname, OSA_ADB_POLICY_DB_MAGIC);
    if (retval)
        goto cleanup;
    retval = osa_adb_init_db(&dbc->policy_db, polname, plockname,
                             OSA_ADB_POLICY_DB_MAGIC);
    if (retval)
        goto cleanup;
    retval = osa_adb_get_lock(dbc->policy_db, KRB5_DB_LOCKMODE_EXCLUSIVE);
    if (retval)
        goto cleanup;

    dbc->db_inited = 1;

cleanup:
    if (retval) {
        if (dbc->db != NULL)
            dbc->db->close(dbc->db);
        if (dbc->db_locks_held > 0) {
            (void) krb5_lock_file(context, dbc->db_lf_file,
                                  KRB5_LOCKMODE_UNLOCK);
        }
        if (dbc->db_lf_file >= 0)
            close(dbc->db_lf_file);
        ctx_clear(dbc);
    }
    free(dbname);
    free(polname);
    free(plockname);
    return retval;
}

krb5_error_code
krb5_db2_get_principal(krb5_context context, krb5_const_principal searchfor,
                       unsigned int flags, krb5_db_entry **entry)
{
    krb5_db2_context *dbc;
    krb5_error_code retval;
    DB     *db;
    DBT     key, contents;
    krb5_data keydata, contdata;
    int     dbret;

    *entry = NULL;
    if (!inited(context))
        return KRB5_KDB_DBNOTINITED;

    dbc = context->dal_handle->db_context;

    retval = ctx_lock(context, dbc, KRB5_LOCKMODE_SHARED);
    if (retval)
        return retval;

    /* XXX deal with wildcard lookups */
    retval = krb5_encode_princ_dbkey(context, &keydata, searchfor);
    if (retval)
        goto cleanup;
    key.data = keydata.data;
    key.size = keydata.length;

    db = dbc->db;
    dbret = (*db->get)(db, &key, &contents, 0);
    retval = errno;
    krb5_free_data_contents(context, &keydata);
    switch (dbret) {
    case 1:
        retval = KRB5_KDB_NOENTRY;
        /* Fall through. */
    case -1:
    default:
        goto cleanup;
    case 0:
        contdata.data = contents.data;
        contdata.length = contents.size;
        retval = krb5_decode_princ_entry(context, &contdata, entry);
        break;
    }

cleanup:
    (void) krb5_db2_unlock(context); /* unlock read lock */
    return retval;
}

krb5_error_code
krb5_db2_put_principal(krb5_context context, krb5_db_entry *entry,
                       char **db_args)
{
    int     dbret;
    DB     *db;
    DBT     key, contents;
    krb5_data contdata, keydata;
    krb5_error_code retval;
    krb5_db2_context *dbc;

    krb5_clear_error_message (context);
    if (db_args) {
        /* DB2 does not support db_args DB arguments for principal */
        k5_setmsg(context, EINVAL, _("Unsupported argument \"%s\" for db2"),
                  db_args[0]);
        return EINVAL;
    }

    if (!inited(context))
        return KRB5_KDB_DBNOTINITED;

    dbc = context->dal_handle->db_context;
    if ((retval = ctx_lock(context, dbc, KRB5_LOCKMODE_EXCLUSIVE)))
        return retval;

    db = dbc->db;

    retval = krb5_encode_princ_entry(context, &contdata, entry);
    if (retval)
        goto cleanup;
    contents.data = contdata.data;
    contents.size = contdata.length;
    retval = krb5_encode_princ_dbkey(context, &keydata, entry->princ);
    if (retval) {
        krb5_free_data_contents(context, &contdata);
        goto cleanup;
    }

    key.data = keydata.data;
    key.size = keydata.length;
    dbret = (*db->put)(db, &key, &contents, 0);
    retval = dbret ? errno : 0;
    krb5_free_data_contents(context, &keydata);
    krb5_free_data_contents(context, &contdata);

cleanup:
    ctx_update_age(dbc);
    (void) krb5_db2_unlock(context); /* unlock database */
    return (retval);
}

krb5_error_code
krb5_db2_delete_principal(krb5_context context, krb5_const_principal searchfor)
{
    krb5_error_code retval;
    krb5_db_entry *entry;
    krb5_db2_context *dbc;
    DB     *db;
    DBT     key, contents;
    krb5_data keydata, contdata;
    int     i, dbret;

    if (!inited(context))
        return KRB5_KDB_DBNOTINITED;

    dbc = context->dal_handle->db_context;
    if ((retval = ctx_lock(context, dbc, KRB5_LOCKMODE_EXCLUSIVE)))
        return (retval);

    if ((retval = krb5_encode_princ_dbkey(context, &keydata, searchfor)))
        goto cleanup;
    key.data = keydata.data;
    key.size = keydata.length;

    db = dbc->db;
    dbret = (*db->get) (db, &key, &contents, 0);
    retval = errno;
    switch (dbret) {
    case 1:
        retval = KRB5_KDB_NOENTRY;
        /* Fall through. */
    case -1:
    default:
        goto cleankey;
    case 0:
        ;
    }
    contdata.data = contents.data;
    contdata.length = contents.size;
    retval = krb5_decode_princ_entry(context, &contdata, &entry);
    if (retval)
        goto cleankey;

    /* Clear encrypted key contents */
    for (i = 0; i < entry->n_key_data; i++) {
        if (entry->key_data[i].key_data_length[0]) {
            memset(entry->key_data[i].key_data_contents[0], 0,
                   (unsigned) entry->key_data[i].key_data_length[0]);
        }
    }

    retval = krb5_encode_princ_entry(context, &contdata, entry);
    krb5_db_free_principal(context, entry);
    if (retval)
        goto cleankey;

    contents.data = contdata.data;
    contents.size = contdata.length;
    dbret = (*db->put) (db, &key, &contents, 0);
    retval = dbret ? errno : 0;
    krb5_free_data_contents(context, &contdata);
    if (retval)
        goto cleankey;
    dbret = (*db->del) (db, &key, 0);
    retval = dbret ? errno : 0;
cleankey:
    krb5_free_data_contents(context, &keydata);

cleanup:
    ctx_update_age(dbc);
    (void) krb5_db2_unlock(context); /* unlock write lock */
    return retval;
}

typedef krb5_error_code (*ctx_iterate_cb)(krb5_pointer, krb5_db_entry *);

/* Cursor structure for ctx_iterate() */
typedef struct iter_curs {
    DBT key;
    DBT data;
    DBT keycopy;
    unsigned int startflag;
    unsigned int stepflag;
    krb5_context ctx;
    krb5_db2_context *dbc;
    int lockmode;
    krb5_boolean islocked;
} iter_curs;

/* Lock DB handle of curs, updating curs->islocked. */
static krb5_error_code
curs_lock(iter_curs *curs)
{
    krb5_error_code retval;

    retval = ctx_lock(curs->ctx, curs->dbc, curs->lockmode);
    if (retval)
        return retval;
    curs->islocked = TRUE;
    return 0;
}

/* Unlock DB handle of curs, updating curs->islocked. */
static void
curs_unlock(iter_curs *curs)
{
    ctx_unlock(curs->ctx, curs->dbc);
    curs->islocked = FALSE;
}

/* Set up curs and lock DB. */
static krb5_error_code
curs_init(iter_curs *curs, krb5_context ctx, krb5_db2_context *dbc,
          krb5_flags iterflags)
{
    int isrecurse = iterflags & KRB5_DB_ITER_RECURSE;
    unsigned int prevflag = R_PREV;
    unsigned int nextflag = R_NEXT;

    curs->keycopy.size = 0;
    curs->keycopy.data = NULL;
    curs->islocked = FALSE;
    curs->ctx = ctx;
    curs->dbc = dbc;

    if (iterflags & KRB5_DB_ITER_WRITE)
        curs->lockmode = KRB5_LOCKMODE_EXCLUSIVE;
    else
        curs->lockmode = KRB5_LOCKMODE_SHARED;

    if (isrecurse) {
#ifdef R_RNEXT
        if (dbc->hashfirst) {
            k5_setmsg(ctx, EINVAL, _("Recursive iteration is not supported "
                                     "for hash databases"));
            return EINVAL;
        }
        prevflag = R_RPREV;
        nextflag = R_RNEXT;
#else
        k5_setmsg(ctx, EINVAL, _("Recursive iteration not supported "
                                 "in this version of libdb"));
        return EINVAL;
#endif
    }
    if (iterflags & KRB5_DB_ITER_REV) {
        curs->startflag = R_LAST;
        curs->stepflag = prevflag;
    } else {
        curs->startflag = R_FIRST;
        curs->stepflag = nextflag;
    }
    return curs_lock(curs);
}

/* Get initial entry. */
static int
curs_start(iter_curs *curs)
{
    DB *db = curs->dbc->db;

    return db->seq(db, &curs->key, &curs->data, curs->startflag);
}

/* Save iteration state so DB can be unlocked/closed. */
static krb5_error_code
curs_save(iter_curs *curs)
{
    if (!curs->dbc->unlockiter)
        return 0;

    curs->keycopy.data = malloc(curs->key.size);
    if (curs->keycopy.data == NULL)
        return ENOMEM;

    curs->keycopy.size = curs->key.size;
    memcpy(curs->keycopy.data, curs->key.data, curs->key.size);
    return 0;
}

/* Free allocated cursor resources */
static void
curs_free(iter_curs *curs)
{
    free(curs->keycopy.data);
    curs->keycopy.size = 0;
    curs->keycopy.data = NULL;
}

/* Move one step of iteration (forwards or backwards as requested).  Free
 * curs->keycopy as a side effect, if needed. */
static int
curs_step(iter_curs *curs)
{
    int dbret;
    krb5_db2_context *dbc = curs->dbc;

    if (dbc->unlockiter) {
        /* Reacquire libdb cursor using saved copy of key. */
        curs->key = curs->keycopy;
        dbret = dbc->db->seq(dbc->db, &curs->key, &curs->data, R_CURSOR);
        curs_free(curs);
        if (dbret)
            return dbret;
    }
    return dbc->db->seq(dbc->db, &curs->key, &curs->data, curs->stepflag);
}

/* Run one invocation of the callback, unlocking the mutex and possibly the DB
 * around the invocation. */
static krb5_error_code
curs_run_cb(iter_curs *curs, ctx_iterate_cb func, krb5_pointer func_arg)
{
    krb5_db2_context *dbc = curs->dbc;
    krb5_error_code retval, lockerr;
    krb5_db_entry *entry;
    krb5_context ctx = curs->ctx;
    krb5_data contdata;

    contdata = make_data(curs->data.data, curs->data.size);
    retval = krb5_decode_princ_entry(ctx, &contdata, &entry);
    if (retval)
        return retval;
    /* Save libdb key across possible DB closure. */
    retval = curs_save(curs);
    if (retval)
        return retval;

    if (dbc->unlockiter)
        curs_unlock(curs);

    k5_mutex_unlock(krb5_db2_mutex);
    retval = (*func)(func_arg, entry);
    krb5_db_free_principal(ctx, entry);
    k5_mutex_lock(krb5_db2_mutex);
    if (dbc->unlockiter) {
        lockerr = curs_lock(curs);
        if (lockerr)
            return lockerr;
    }
    return retval;
}

/* Free cursor resources and unlock the DB if needed. */
static void
curs_fini(iter_curs *curs)
{
    curs_free(curs);
    if (curs->islocked)
        curs_unlock(curs);
}

static krb5_error_code
ctx_iterate(krb5_context context, krb5_db2_context *dbc,
            ctx_iterate_cb func, krb5_pointer func_arg, krb5_flags iterflags)
{
    krb5_error_code retval;
    int dbret;
    iter_curs curs;

    retval = curs_init(&curs, context, dbc, iterflags);
    if (retval)
        return retval;
    dbret = curs_start(&curs);
    while (dbret == 0) {
        retval = curs_run_cb(&curs, func, func_arg);
        if (retval)
            goto cleanup;
        dbret = curs_step(&curs);
    }
    switch (dbret) {
    case 1:
    case 0:
        break;
    case -1:
    default:
        retval = errno;
    }
cleanup:
    curs_fini(&curs);
    return retval;
}

krb5_error_code
krb5_db2_iterate(krb5_context context, char *match_expr, ctx_iterate_cb func,
                 krb5_pointer func_arg, krb5_flags iterflags)
{
    if (!inited(context))
        return KRB5_KDB_DBNOTINITED;
    return ctx_iterate(context, context->dal_handle->db_context, func,
                       func_arg, iterflags);
}

krb5_boolean
krb5_db2_set_lockmode(krb5_context context, krb5_boolean mode)
{
    krb5_boolean old;
    krb5_db2_context *dbc;

    dbc = context->dal_handle->db_context;
    old = mode;
    if (dbc) {
        old = dbc->db_nb_locks;
        dbc->db_nb_locks = mode;
    }
    return old;
}

/*
 *     DAL API functions
 */
krb5_error_code
krb5_db2_lib_init()
{
    return 0;
}

krb5_error_code
krb5_db2_lib_cleanup()
{
    /* right now, no cleanup required */
    return 0;
}

krb5_error_code
krb5_db2_open(krb5_context context, char *conf_section, char **db_args,
              int mode)
{
    krb5_error_code status = 0;

    krb5_clear_error_message(context);
    if (inited(context))
        return 0;

    status = configure_context(context, conf_section, db_args);
    if (status != 0)
        return status;

    status = check_openable(context);
    if (status != 0)
        return status;

    return ctx_init(context->dal_handle->db_context);
}

krb5_error_code
krb5_db2_create(krb5_context context, char *conf_section, char **db_args)
{
    krb5_error_code status = 0;
    krb5_db2_context *dbc;

    krb5_clear_error_message(context);
    if (inited(context))
        return 0;

    status = configure_context(context, conf_section, db_args);
    if (status != 0)
        return status;

    dbc = context->dal_handle->db_context;
    status = ctx_create_db(context, dbc);
    if (status != 0)
        return status;

    if (!dbc->tempdb)
        krb5_db2_unlock(context);

    return 0;
}

krb5_error_code
krb5_db2_destroy(krb5_context context, char *conf_section, char **db_args)
{
    krb5_error_code status;
    krb5_db2_context *dbc;
    char *dbname = NULL, *lockname = NULL, *polname = NULL, *plockname = NULL;

    if (inited(context)) {
        status = krb5_db2_fini(context);
        if (status != 0)
            return status;
    }

    krb5_clear_error_message(context);
    status = configure_context(context, conf_section, db_args);
    if (status != 0)
        return status;

    status = check_openable(context);
    if (status != 0)
        return status;

    dbc = context->dal_handle->db_context;

    status = ctx_allfiles(dbc, &dbname, &lockname, &polname, &plockname);
    if (status)
        goto cleanup;
    status = destroy_file(dbname);
    if (status)
        goto cleanup;
    status = unlink(lockname);
    if (status)
        goto cleanup;
    status = osa_adb_destroy_db(polname, plockname, OSA_ADB_POLICY_DB_MAGIC);
    if (status)
        return status;

    status = krb5_db2_fini(context);

cleanup:
    free(dbname);
    free(lockname);
    free(polname);
    free(plockname);
    return status;
}

/* policy functions */
krb5_error_code
krb5_db2_create_policy(krb5_context context, osa_policy_ent_t policy)
{
    krb5_db2_context *dbc = context->dal_handle->db_context;

    return osa_adb_create_policy(dbc->policy_db, policy);
}

krb5_error_code
krb5_db2_get_policy(krb5_context context,
                    char *name, osa_policy_ent_t *policy)
{
    krb5_db2_context *dbc = context->dal_handle->db_context;

    return osa_adb_get_policy(dbc->policy_db, name, policy);
}

krb5_error_code
krb5_db2_put_policy(krb5_context context, osa_policy_ent_t policy)
{
    krb5_db2_context *dbc = context->dal_handle->db_context;

    return osa_adb_put_policy(dbc->policy_db, policy);
}

krb5_error_code
krb5_db2_iter_policy(krb5_context context,
                     char *match_entry,
                     osa_adb_iter_policy_func func, void *data)
{
    krb5_db2_context *dbc = context->dal_handle->db_context;

    return osa_adb_iter_policy(dbc->policy_db, func, data);
}

krb5_error_code
krb5_db2_delete_policy(krb5_context context, char *policy)
{
    krb5_db2_context *dbc = context->dal_handle->db_context;

    return osa_adb_destroy_policy(dbc->policy_db, policy);
}

void
krb5_db2_free_policy(krb5_context context, osa_policy_ent_t entry)
{
    osa_free_policy_ent(entry);
}


/*
 * Merge non-replicated attributes from src into dst, setting
 * changed to non-zero if dst was changed.
 *
 * Non-replicated attributes are: last_success, last_failed,
 * fail_auth_count, and any negative TL data values.
 */
static krb5_error_code
krb5_db2_merge_principal(krb5_context context,
                         krb5_db_entry *src,
                         krb5_db_entry *dst,
                         int *changed)
{
    *changed = 0;

    if (dst->last_success != src->last_success) {
        dst->last_success = src->last_success;
        (*changed)++;
    }

    if (dst->last_failed != src->last_failed) {
        dst->last_failed = src->last_failed;
        (*changed)++;
    }

    if (dst->fail_auth_count != src->fail_auth_count) {
        dst->fail_auth_count = src->fail_auth_count;
        (*changed)++;
    }

    return 0;
}

struct nra_context {
    krb5_context kcontext;
    krb5_db2_context *db_context;
};

/*
 * Iteration callback merges non-replicated attributes from
 * old database.
 */
static krb5_error_code
krb5_db2_merge_nra_iterator(krb5_pointer ptr, krb5_db_entry *entry)
{
    struct nra_context *nra = (struct nra_context *)ptr;
    kdb5_dal_handle *dal_handle = nra->kcontext->dal_handle;
    krb5_error_code retval;
    int changed;
    krb5_db_entry *s_entry;
    krb5_db2_context *dst_db;

    memset(&s_entry, 0, sizeof(s_entry));

    dst_db = dal_handle->db_context;
    dal_handle->db_context = nra->db_context;

    /* look up the new principal in the old DB */
    retval = krb5_db2_get_principal(nra->kcontext, entry->princ, 0, &s_entry);
    if (retval != 0) {
        /* principal may be newly created, so ignore */
        dal_handle->db_context = dst_db;
        return 0;
    }

    /* merge non-replicated attributes from the old entry in */
    krb5_db2_merge_principal(nra->kcontext, s_entry, entry, &changed);

    dal_handle->db_context = dst_db;

    /* if necessary, commit the modified new entry to the new DB */
    if (changed) {
        retval = krb5_db2_put_principal(nra->kcontext, entry, NULL);
    } else {
        retval = 0;
    }

    krb5_db_free_principal(nra->kcontext, s_entry);
    return retval;
}

/*
 * Merge non-replicated attributes (that is, lockout-related
 * attributes and negative TL data types) from the real database
 * into the temporary one.
 */
static krb5_error_code
ctx_merge_nra(krb5_context context, krb5_db2_context *dbc_temp,
              krb5_db2_context *dbc_real)
{
    struct nra_context nra;

    nra.kcontext = context;
    nra.db_context = dbc_real;
    return ctx_iterate(context, dbc_temp, krb5_db2_merge_nra_iterator, &nra, 0);
}

/*
 * In the filesystem, promote the temporary database described by dbc_temp to
 * the real database described by dbc_real.  Both must be exclusively locked.
 */
static krb5_error_code
ctx_promote(krb5_context context, krb5_db2_context *dbc_temp,
            krb5_db2_context *dbc_real)
{
    krb5_error_code retval;
    char *tdb = NULL, *tlock = NULL, *tpol = NULL, *tplock = NULL;
    char *rdb = NULL, *rlock = NULL, *rpol = NULL, *rplock = NULL;

    /* Generate all filenames of interest (including a few we don't need). */
    retval = ctx_allfiles(dbc_temp, &tdb, &tlock, &tpol, &tplock);
    if (retval)
        return retval;
    retval = ctx_allfiles(dbc_real, &rdb, &rlock, &rpol, &rplock);
    if (retval)
        goto cleanup;

    /* Rename the principal and policy databases into place. */
    if (rename(tdb, rdb)) {
        retval = errno;
        goto cleanup;
    }
    if (rename(tpol, rpol)) {
        retval = errno;
        goto cleanup;
    }

    ctx_update_age(dbc_real);

    /* Release and remove the temporary DB lockfiles. */
    (void) unlink(tlock);
    (void) unlink(tplock);

cleanup:
    free(tdb);
    free(tlock);
    free(tpol);
    free(tplock);
    free(rdb);
    free(rlock);
    free(rpol);
    free(rplock);
    return retval;
}

krb5_error_code
krb5_db2_promote_db(krb5_context context, char *conf_section, char **db_args)
{
    krb5_error_code retval;
    krb5_boolean merge_nra = FALSE, real_locked = FALSE;
    krb5_db2_context *dbc_temp, *dbc_real = NULL;
    char **db_argp;

    /* context must be initialized with an exclusively locked temp DB. */
    if (!inited(context))
        return KRB5_KDB_DBNOTINITED;
    dbc_temp = context->dal_handle->db_context;
    if (dbc_temp->db_lock_mode != KRB5_LOCKMODE_EXCLUSIVE)
        return KRB5_KDB_NOTLOCKED;
    if (!dbc_temp->tempdb)
        return EINVAL;

    /* Check db_args for whether we should merge non-replicated attributes. */
    for (db_argp = db_args; *db_argp; db_argp++) {
        if (!strcmp(*db_argp, "merge_nra")) {
            merge_nra = TRUE;
            break;
        }
    }

    /* Make a db2 context for the real DB. */
    dbc_real = k5alloc(sizeof(*dbc_real), &retval);
    if (dbc_real == NULL)
        return retval;
    ctx_clear(dbc_real);

    /* Try creating the real DB. */
    dbc_real->db_name = strdup(dbc_temp->db_name);
    if (dbc_real->db_name == NULL)
        goto cleanup;
    dbc_real->tempdb = FALSE;
    retval = ctx_create_db(context, dbc_real);
    if (retval == EEXIST) {
        /* The real database already exists, so open and lock it. */
        dbc_real->db_name = strdup(dbc_temp->db_name);
        if (dbc_real->db_name == NULL)
            goto cleanup;
        dbc_real->tempdb = FALSE;
        retval = ctx_init(dbc_real);
        if (retval)
            goto cleanup;
        retval = ctx_lock(context, dbc_real, KRB5_DB_LOCKMODE_EXCLUSIVE);
        if (retval)
            goto cleanup;
    } else if (retval)
        goto cleanup;
    real_locked = TRUE;

    if (merge_nra) {
        retval = ctx_merge_nra(context, dbc_temp, dbc_real);
        if (retval)
            goto cleanup;
    }

    /* Perform filesystem manipulations for the promotion. */
    retval = ctx_promote(context, dbc_temp, dbc_real);
    if (retval)
        goto cleanup;

    /* Unlock and finalize context since the temp DB is gone. */
    (void) krb5_db2_unlock(context);
    krb5_db2_fini(context);

cleanup:
    if (real_locked)
        (void) ctx_unlock(context, dbc_real);
    if (dbc_real)
        ctx_fini(dbc_real);
    return retval;
}

krb5_error_code
krb5_db2_check_policy_as(krb5_context kcontext, krb5_kdc_req *request,
                         krb5_db_entry *client, krb5_db_entry *server,
                         krb5_timestamp kdc_time, const char **status,
                         krb5_pa_data ***e_data)
{
    krb5_error_code retval;

    retval = krb5_db2_lockout_check_policy(kcontext, client, kdc_time);
    if (retval == KRB5KDC_ERR_CLIENT_REVOKED)
        *status = "LOCKED_OUT";
    return retval;
}

void
krb5_db2_audit_as_req(krb5_context kcontext, krb5_kdc_req *request,
                      krb5_db_entry *client, krb5_db_entry *server,
                      krb5_timestamp authtime, krb5_error_code error_code)
{
    (void) krb5_db2_lockout_audit(kcontext, client, authtime, error_code);
}
