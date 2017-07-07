/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 * $Header$
 */

#include        <k5-int.h>
#include        <sys/file.h>
#include        <fcntl.h>
#include        <unistd.h>
#include        "policy_db.h"
#include        <stdlib.h>
#include        <db.h>

struct _locklist {
    osa_adb_lock_ent lockinfo;
    struct _locklist *next;
};

krb5_error_code
osa_adb_create_db(char *filename, char *lockfilename, int magic)
{
    int lf;
    DB *db;
    BTREEINFO btinfo;

    memset(&btinfo, 0, sizeof(btinfo));
    btinfo.flags = 0;
    btinfo.cachesize = 0;
    btinfo.psize = 4096;
    btinfo.lorder = 0;
    btinfo.minkeypage = 0;
    btinfo.compare = NULL;
    btinfo.prefix = NULL;
    db = dbopen(filename, O_RDWR | O_CREAT | O_EXCL, 0600, DB_BTREE, &btinfo);
    if (db == NULL)
        return errno;
    if (db->close(db) < 0)
        return errno;

    /* only create the lock file if we successfully created the db */
    lf = THREEPARAMOPEN(lockfilename, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (lf == -1)
        return errno;
    (void) close(lf);

    return OSA_ADB_OK;
}

krb5_error_code
osa_adb_destroy_db(char *filename, char *lockfilename, int magic)
{
    /* the admin databases do not contain security-critical data */
    if (unlink(filename) < 0 ||
        unlink(lockfilename) < 0)
        return errno;
    return OSA_ADB_OK;
}

krb5_error_code
osa_adb_init_db(osa_adb_db_t *dbp, char *filename, char *lockfilename,
                int magic)
{
    osa_adb_db_t db;
    static struct _locklist *locklist = NULL;
    struct _locklist *lockp;
    krb5_error_code code;

    if (dbp == NULL || filename == NULL)
        return EINVAL;

    db = (osa_adb_princ_t) malloc(sizeof(osa_adb_db_ent));
    if (db == NULL)
        return ENOMEM;

    memset(db, 0, sizeof(*db));
    db->info.hash = NULL;
    db->info.bsize = 256;
    db->info.ffactor = 8;
    db->info.nelem = 25000;
    db->info.lorder = 0;

    db->btinfo.flags = 0;
    db->btinfo.cachesize = 0;
    db->btinfo.psize = 4096;
    db->btinfo.lorder = 0;
    db->btinfo.minkeypage = 0;
    db->btinfo.compare = NULL;
    db->btinfo.prefix = NULL;
    /*
     * A process is allowed to open the same database multiple times
     * and access it via different handles.  If the handles use
     * distinct lockinfo structures, things get confused: lock(A),
     * lock(B), release(B) will result in the kernel unlocking the
     * lock file but handle A will still think the file is locked.
     * Therefore, all handles using the same lock file must share a
     * single lockinfo structure.
     *
     * It is not sufficient to have a single lockinfo structure,
     * however, because a single process may also wish to open
     * multiple different databases simultaneously, with different
     * lock files.  This code used to use a single static lockinfo
     * structure, which means that the second database opened used
     * the first database's lock file.  This was Bad.
     *
     * We now maintain a linked list of lockinfo structures, keyed by
     * lockfilename.  An entry is added when this function is called
     * with a new lockfilename, and all subsequent calls with that
     * lockfilename use the existing entry, updating the refcnt.
     * When the database is closed with fini_db(), the refcnt is
     * decremented, and when it is zero the lockinfo structure is
     * freed and reset.  The entry in the linked list, however, is
     * never removed; it will just be reinitialized the next time
     * init_db is called with the right lockfilename.
     */

    /* find or create the lockinfo structure for lockfilename */
    lockp = locklist;
    while (lockp) {
        if (strcmp(lockp->lockinfo.filename, lockfilename) == 0)
            break;
        else
            lockp = lockp->next;
    }
    if (lockp == NULL) {
        /* doesn't exist, create it, add to list */
        lockp = (struct _locklist *) malloc(sizeof(*lockp));
        if (lockp == NULL) {
            free(db);
            return ENOMEM;
        }
        memset(lockp, 0, sizeof(*lockp));
        lockp->lockinfo.filename = strdup(lockfilename);
        if (lockp->lockinfo.filename == NULL) {
            free(lockp);
            free(db);
            return ENOMEM;
        }
        lockp->next = locklist;
        locklist = lockp;
    }

    /* now initialize lockp->lockinfo if necessary */
    if (lockp->lockinfo.lockfile == NULL) {
        if ((code = krb5int_init_context_kdc(&lockp->lockinfo.context))) {
            free(db);
            return((krb5_error_code) code);
        }

        /*
         * needs be open read/write so that write locking can work with
         * POSIX systems
         */
        if ((lockp->lockinfo.lockfile = fopen(lockfilename, "r+")) == NULL) {
            /*
             * maybe someone took away write permission so we could only
             * get shared locks?
             */
            if ((lockp->lockinfo.lockfile = fopen(lockfilename, "r"))
                == NULL) {
                free(db);
                return OSA_ADB_NOLOCKFILE;
            }
        }
        set_cloexec_file(lockp->lockinfo.lockfile);
        lockp->lockinfo.lockmode = lockp->lockinfo.lockcnt = 0;
    }

    /* lockp is set, lockinfo is initialized, update the reference count */
    db->lock = &lockp->lockinfo;
    db->lock->refcnt++;

    db->opencnt = 0;
    db->filename = strdup(filename);
    db->magic = magic;

    *dbp = db;

    return OSA_ADB_OK;
}

krb5_error_code
osa_adb_fini_db(osa_adb_db_t db, int magic)
{
    if (db->magic != magic)
        return EINVAL;
    if (db->lock->refcnt == 0) {
        /* barry says this can't happen */
        return OSA_ADB_FAILURE;
    } else {
        db->lock->refcnt--;
    }

    if (db->lock->refcnt == 0) {
        /*
         * Don't free db->lock->filename, it is used as a key to
         * find the lockinfo entry in the linked list.  If the
         * lockfile doesn't exist, we must be closing the database
         * after trashing it.  This has to be allowed, so don't
         * generate an error.
         */
        if (db->lock->lockmode != KRB5_DB_LOCKMODE_PERMANENT)
            (void) fclose(db->lock->lockfile);
        db->lock->lockfile = NULL;
        krb5_free_context(db->lock->context);
    }

    db->magic = 0;
    free(db->filename);
    free(db);
    return OSA_ADB_OK;
}

krb5_error_code
osa_adb_get_lock(osa_adb_db_t db, int mode)
{
    int perm, krb5_mode, ret = 0;

    if (db->lock->lockmode >= mode) {
        /* No need to upgrade lock, just incr refcnt and return */
        db->lock->lockcnt++;
        return(OSA_ADB_OK);
    }

    perm = 0;
    switch (mode) {
    case KRB5_DB_LOCKMODE_PERMANENT:
        perm = 1;
    case KRB5_DB_LOCKMODE_EXCLUSIVE:
        krb5_mode = KRB5_LOCKMODE_EXCLUSIVE;
        break;
    case KRB5_DB_LOCKMODE_SHARED:
        krb5_mode = KRB5_LOCKMODE_SHARED;
        break;
    default:
        return(EINVAL);
    }

    ret = krb5_lock_file(db->lock->context, fileno(db->lock->lockfile),
                         krb5_mode);
    if (ret == EBADF && mode == KRB5_DB_LOCKMODE_EXCLUSIVE)
        return OSA_ADB_NOEXCL_PERM;
    else if (ret == EACCES || ret == EAGAIN || ret == EWOULDBLOCK)
        return OSA_ADB_CANTLOCK_DB;
    else if (ret != 0)
        return ret;

    /*
     * If the file no longer exists, someone acquired a permanent
     * lock.  If that process terminates its exclusive lock is lost,
     * but if we already had the file open we can (probably) lock it
     * even though it has been unlinked.  So we need to insist that
     * it exist.
     */
    if (access(db->lock->filename, F_OK) < 0) {
        (void) krb5_lock_file(db->lock->context,
                              fileno(db->lock->lockfile),
                              KRB5_LOCKMODE_UNLOCK);
        return OSA_ADB_NOLOCKFILE;
    }

    /* we have the shared/exclusive lock */

    if (perm) {
        if (unlink(db->lock->filename) < 0) {
            /* somehow we can't delete the file, but we already */
            /* have the lock, so release it and return */

            ret = errno;
            (void) krb5_lock_file(db->lock->context,
                                  fileno(db->lock->lockfile),
                                  KRB5_LOCKMODE_UNLOCK);

            /* maybe we should return CANTLOCK_DB.. but that would */
            /* look just like the db was already locked */
            return ret;
        }

        /* this releases our exclusive lock.. which is okay because */
        /* now no one else can get one either */
        (void) fclose(db->lock->lockfile);
    }

    db->lock->lockmode = mode;
    db->lock->lockcnt++;
    return OSA_ADB_OK;
}

krb5_error_code
osa_adb_release_lock(osa_adb_db_t db)
{
    int ret, fd;

    if (!db->lock->lockcnt)            /* lock already unlocked */
        return OSA_ADB_NOTLOCKED;

    if (--db->lock->lockcnt == 0) {
        if (db->lock->lockmode == KRB5_DB_LOCKMODE_PERMANENT) {
            /* now we need to create the file since it does not exist */
            fd = THREEPARAMOPEN(db->lock->filename,O_RDWR | O_CREAT | O_EXCL,
                                0600);
            if (fd < 0)
                return OSA_ADB_NOLOCKFILE;
            set_cloexec_fd(fd);
            if ((db->lock->lockfile = fdopen(fd, "w+")) == NULL)
                return OSA_ADB_NOLOCKFILE;
        } else if ((ret = krb5_lock_file(db->lock->context,
                                         fileno(db->lock->lockfile),
                                         KRB5_LOCKMODE_UNLOCK)))
            return ret;

        db->lock->lockmode = 0;
    }
    return OSA_ADB_OK;
}

krb5_error_code
osa_adb_open_and_lock(osa_adb_princ_t db, int locktype)
{
    int ret;

    ret = osa_adb_get_lock(db, locktype);
    if (ret != OSA_ADB_OK)
        return ret;
    if (db->opencnt)
        goto open_ok;

    db->db = dbopen(db->filename, O_RDWR, 0600, DB_BTREE, &db->btinfo);
    if (db->db != NULL)
        goto open_ok;
    if (IS_EFTYPE(errno)) {
        db->db = dbopen(db->filename, O_RDWR, 0600, DB_HASH, &db->info);
        if (db->db != NULL)
            goto open_ok;
    } else {
        (void) osa_adb_release_lock(db);
        if (errno == EINVAL)
            return OSA_ADB_BAD_DB;
        return errno;
    }
open_ok:
    db->opencnt++;
    return OSA_ADB_OK;
}

krb5_error_code
osa_adb_close_and_unlock(osa_adb_princ_t db)
{
    if (--db->opencnt)
        return osa_adb_release_lock(db);
    if(db->db != NULL && db->db->close(db->db) == -1) {
        (void) osa_adb_release_lock(db);
        return OSA_ADB_FAILURE;
    }

    db->db = NULL;

    return(osa_adb_release_lock(db));
}
