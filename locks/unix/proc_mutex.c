/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apr.h"
#include "apr_strings.h"
#include "apr_arch_proc_mutex.h"
#include "apr_arch_file_io.h" /* for apr_mkstemp() */
#include "apr_hash.h"
#include "apr_atomic.h"

APR_DECLARE(apr_status_t) apr_proc_mutex_destroy(apr_proc_mutex_t *mutex)
{
    return apr_pool_cleanup_run(mutex->pool, mutex, apr_proc_mutex_cleanup);
}

#if APR_HAS_POSIXSEM_SERIALIZE || APR_HAS_FCNTL_SERIALIZE || \
    APR_HAS_SYSVSEM_SERIALIZE
static apr_status_t proc_mutex_no_child_init(apr_proc_mutex_t **mutex,
                                             apr_pool_t *cont,
                                             const char *fname)
{
    return APR_SUCCESS;
}
#endif    

#if APR_HAS_POSIXSEM_SERIALIZE || APR_HAS_PROC_PTHREAD_SERIALIZE
static apr_status_t proc_mutex_no_perms_set(apr_proc_mutex_t *mutex,
                                            apr_fileperms_t perms,
                                            apr_uid_t uid,
                                            apr_gid_t gid)
{
    return APR_ENOTIMPL;
}
#endif    


#if APR_HAS_POSIXSEM_SERIALIZE

#ifndef SEM_FAILED
#define SEM_FAILED (-1)
#endif

static apr_status_t proc_mutex_posix_cleanup(void *mutex_)
{
    apr_proc_mutex_t *mutex = mutex_;
    
    if (sem_close(mutex->os.psem_interproc) < 0) {
        return errno;
    }

    return APR_SUCCESS;
}    

static unsigned int rshash (char *p) {
    /* hash function from Robert Sedgwicks 'Algorithms in C' book */
   unsigned int b    = 378551;
   unsigned int a    = 63689;
   unsigned int retval = 0;

   for( ; *p; p++)
   {
      retval = retval * a + (*p);
      a *= b;
   }

   return retval;
}

static apr_status_t proc_mutex_posix_create(apr_proc_mutex_t *new_mutex,
                                            const char *fname)
{
    #define APR_POSIXSEM_NAME_MIN 13
    sem_t *psem;
    char semname[32];
    
    /*
     * This bogusness is to follow what appears to be the
     * lowest common denominator in Posix semaphore naming:
     *   - start with '/'
     *   - be at most 14 chars
     *   - be unique and not match anything on the filesystem
     *
     * Because of this, we use fname to generate a (unique) hash
     * and use that as the name of the semaphore. If no filename was
     * given, we create one based on the time. We tuck the name
     * away, since it might be useful for debugging. We use 2 hashing
     * functions to try to avoid collisions.
     *
     * To  make this as robust as possible, we initially try something
     * larger (and hopefully more unique) and gracefully fail down to the
     * LCD above.
     *
     * NOTE: Darwin (Mac OS X) seems to be the most restrictive
     * implementation. Versions previous to Darwin 6.2 had the 14
     * char limit, but later rev's allow up to 31 characters.
     *
     */
    if (fname) {
        apr_ssize_t flen = strlen(fname);
        char *p = apr_pstrndup(new_mutex->pool, fname, strlen(fname));
        unsigned int h1, h2;
        h1 = (apr_hashfunc_default((const char *)p, &flen) & 0xffffffff);
        h2 = (rshash(p) & 0xffffffff);
        apr_snprintf(semname, sizeof(semname), "/ApR.%xH%x", h1, h2);
    } else {
        apr_time_t now;
        unsigned long sec;
        unsigned long usec;
        now = apr_time_now();
        sec = apr_time_sec(now);
        usec = apr_time_usec(now);
        apr_snprintf(semname, sizeof(semname), "/ApR.%lxZ%lx", sec, usec);
    }
    do {
        psem = sem_open(semname, O_CREAT | O_EXCL, 0644, 1);
    } while (psem == (sem_t *)SEM_FAILED && errno == EINTR);
    if (psem == (sem_t *)SEM_FAILED) {
        if (errno == ENAMETOOLONG) {
            /* Oh well, good try */
            semname[APR_POSIXSEM_NAME_MIN] = '\0';
        } else {
            return errno;
        }
        do {
            psem = sem_open(semname, O_CREAT | O_EXCL, 0644, 1);
        } while (psem == (sem_t *)SEM_FAILED && errno == EINTR);
    }

    if (psem == (sem_t *)SEM_FAILED) {
        return errno;
    }
    /* Ahhh. The joys of Posix sems. Predelete it... */
    sem_unlink(semname);
    new_mutex->os.psem_interproc = psem;
    new_mutex->fname = apr_pstrdup(new_mutex->pool, semname);
    apr_pool_cleanup_register(new_mutex->pool, (void *)new_mutex,
                              apr_proc_mutex_cleanup, 
                              apr_pool_cleanup_null);
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_posix_acquire(apr_proc_mutex_t *mutex)
{
    int rc;

    do {
        rc = sem_wait(mutex->os.psem_interproc);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0) {
        return errno;
    }
    mutex->curr_locked = 1;
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_posix_tryacquire(apr_proc_mutex_t *mutex)
{
    int rc;

    do {
        rc = sem_trywait(mutex->os.psem_interproc);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0) {
        if (errno == EAGAIN) {
            return APR_EBUSY;
        }
        return errno;
    }
    mutex->curr_locked = 1;
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_posix_release(apr_proc_mutex_t *mutex)
{
    mutex->curr_locked = 0;
    if (sem_post(mutex->os.psem_interproc) < 0) {
        /* any failure is probably fatal, so no big deal to leave
         * ->curr_locked at 0. */
        return errno;
    }
    return APR_SUCCESS;
}

static const apr_proc_mutex_unix_lock_methods_t mutex_posixsem_methods =
{
#if APR_PROCESS_LOCK_IS_GLOBAL || !APR_HAS_THREADS || defined(POSIXSEM_IS_GLOBAL)
    APR_PROCESS_LOCK_MECH_IS_GLOBAL,
#else
    0,
#endif
    proc_mutex_posix_create,
    proc_mutex_posix_acquire,
    proc_mutex_posix_tryacquire,
    proc_mutex_posix_release,
    proc_mutex_posix_cleanup,
    proc_mutex_no_child_init,
    proc_mutex_no_perms_set,
    APR_LOCK_POSIXSEM,
    "posixsem"
};

#endif /* Posix sem implementation */

#if APR_HAS_SYSVSEM_SERIALIZE

static struct sembuf proc_mutex_op_on;
static struct sembuf proc_mutex_op_try;
static struct sembuf proc_mutex_op_off;

static void proc_mutex_sysv_setup(void)
{
    proc_mutex_op_on.sem_num = 0;
    proc_mutex_op_on.sem_op = -1;
    proc_mutex_op_on.sem_flg = SEM_UNDO;
    proc_mutex_op_try.sem_num = 0;
    proc_mutex_op_try.sem_op = -1;
    proc_mutex_op_try.sem_flg = SEM_UNDO | IPC_NOWAIT;
    proc_mutex_op_off.sem_num = 0;
    proc_mutex_op_off.sem_op = 1;
    proc_mutex_op_off.sem_flg = SEM_UNDO;
}

static apr_status_t proc_mutex_sysv_cleanup(void *mutex_)
{
    apr_proc_mutex_t *mutex=mutex_;
    union semun ick;
    
    if (mutex->os.crossproc != -1) {
        ick.val = 0;
        semctl(mutex->os.crossproc, 0, IPC_RMID, ick);
    }
    return APR_SUCCESS;
}    

static apr_status_t proc_mutex_sysv_create(apr_proc_mutex_t *new_mutex,
                                           const char *fname)
{
    union semun ick;
    apr_status_t rv;
    
    new_mutex->os.crossproc = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    if (new_mutex->os.crossproc == -1) {
        rv = errno;
        proc_mutex_sysv_cleanup(new_mutex);
        return rv;
    }
    ick.val = 1;
    if (semctl(new_mutex->os.crossproc, 0, SETVAL, ick) < 0) {
        rv = errno;
        proc_mutex_sysv_cleanup(new_mutex);
        new_mutex->os.crossproc = -1;
        return rv;
    }
    new_mutex->curr_locked = 0;
    apr_pool_cleanup_register(new_mutex->pool,
                              (void *)new_mutex, apr_proc_mutex_cleanup, 
                              apr_pool_cleanup_null);
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_sysv_acquire(apr_proc_mutex_t *mutex)
{
    int rc;

    do {
        rc = semop(mutex->os.crossproc, &proc_mutex_op_on, 1);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0) {
        return errno;
    }
    mutex->curr_locked = 1;
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_sysv_tryacquire(apr_proc_mutex_t *mutex)
{
    int rc;

    do {
        rc = semop(mutex->os.crossproc, &proc_mutex_op_try, 1);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0) {
        if (errno == EAGAIN) {
            return APR_EBUSY;
        }
        return errno;
    }
    mutex->curr_locked = 1;
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_sysv_release(apr_proc_mutex_t *mutex)
{
    int rc;

    mutex->curr_locked = 0;
    do {
        rc = semop(mutex->os.crossproc, &proc_mutex_op_off, 1);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0) {
        return errno;
    }
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_sysv_perms_set(apr_proc_mutex_t *mutex,
                                              apr_fileperms_t perms,
                                              apr_uid_t uid,
                                              apr_gid_t gid)
{

    union semun ick;
    struct semid_ds buf;
    buf.sem_perm.uid = uid;
    buf.sem_perm.gid = gid;
    buf.sem_perm.mode = apr_unix_perms2mode(perms);
    ick.buf = &buf;
    if (semctl(mutex->os.crossproc, 0, IPC_SET, ick) < 0) {
        return errno;
    }
    return APR_SUCCESS;
}

static const apr_proc_mutex_unix_lock_methods_t mutex_sysv_methods =
{
#if APR_PROCESS_LOCK_IS_GLOBAL || !APR_HAS_THREADS || defined(SYSVSEM_IS_GLOBAL)
    APR_PROCESS_LOCK_MECH_IS_GLOBAL,
#else
    0,
#endif
    proc_mutex_sysv_create,
    proc_mutex_sysv_acquire,
    proc_mutex_sysv_tryacquire,
    proc_mutex_sysv_release,
    proc_mutex_sysv_cleanup,
    proc_mutex_no_child_init,
    proc_mutex_sysv_perms_set,
    APR_LOCK_SYSVSEM,
    "sysvsem"
};

#endif /* SysV sem implementation */

#if APR_HAS_PROC_PTHREAD_SERIALIZE

/* The mmap()ed pthread_interproc is the native pthread_mutex_t followed
 * by a refcounter to track children using it.  We want to avoid calling
 * pthread_mutex_destroy() on the shared mutex area while it is in use by
 * another process, because this may mark the shared pthread_mutex_t as
 * invalid for everyone, including forked children (unlike "sysvsem" for
 * example), causing unexpected errors or deadlocks (PR 49504).  So the
 * last process (parent or child) referencing the mutex will effectively
 * destroy it.
 */
typedef struct {
    pthread_mutex_t mutex;
    apr_uint32_t refcount;
} proc_pthread_mutex_t;

#define proc_pthread_mutex_refcount(m) \
    (((proc_pthread_mutex_t *)(m)->os.pthread_interproc)->refcount)

static APR_INLINE int proc_pthread_mutex_inc(apr_proc_mutex_t *mutex)
{
    if (mutex->pthread_refcounting) {
        apr_atomic_inc32(&proc_pthread_mutex_refcount(mutex));
        return 1;
    }
    return 0;
}

static APR_INLINE int proc_pthread_mutex_dec(apr_proc_mutex_t *mutex)
{
    if (mutex->pthread_refcounting) {
        return apr_atomic_dec32(&proc_pthread_mutex_refcount(mutex));
    }
    return 0;
}

static apr_status_t proc_pthread_mutex_unref(void *mutex_)
{
    apr_proc_mutex_t *mutex=mutex_;
    apr_status_t rv;

    if (mutex->curr_locked == 1) {
        if ((rv = pthread_mutex_unlock(mutex->os.pthread_interproc))) {
#ifdef HAVE_ZOS_PTHREADS
            rv = errno;
#endif
            return rv;
        }
    }
    if (!proc_pthread_mutex_dec(mutex)) {
        if ((rv = pthread_mutex_destroy(mutex->os.pthread_interproc))) {
#ifdef HAVE_ZOS_PTHREADS
            rv = errno;
#endif
            return rv;
        }
    }
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_pthread_cleanup(void *mutex_)
{
    apr_proc_mutex_t *mutex=mutex_;
    apr_status_t rv;

    /* curr_locked is set to -1 until the mutex has been created */
    if (mutex->curr_locked != -1) {
        if ((rv = proc_pthread_mutex_unref(mutex))) {
            return rv;
        }
    }
    if (munmap(mutex->os.pthread_interproc, sizeof(proc_pthread_mutex_t))) {
        return errno;
    }
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_pthread_create(apr_proc_mutex_t *new_mutex,
                                              const char *fname)
{
    apr_status_t rv;
    int fd;
    pthread_mutexattr_t mattr;

    fd = open("/dev/zero", O_RDWR);
    if (fd < 0) {
        return errno;
    }

    new_mutex->os.pthread_interproc = mmap(NULL, sizeof(proc_pthread_mutex_t),
                                           PROT_READ | PROT_WRITE, MAP_SHARED,
                                           fd, 0); 
    if (new_mutex->os.pthread_interproc == MAP_FAILED) {
        new_mutex->os.pthread_interproc = NULL;
        rv = errno;
        close(fd);
        return rv;
    }
    close(fd);

    new_mutex->pthread_refcounting = 1;
    new_mutex->curr_locked = -1; /* until the mutex has been created */

    if ((rv = pthread_mutexattr_init(&mattr))) {
#ifdef HAVE_ZOS_PTHREADS
        rv = errno;
#endif
        proc_mutex_pthread_cleanup(new_mutex);
        return rv;
    }
    if ((rv = pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED))) {
#ifdef HAVE_ZOS_PTHREADS
        rv = errno;
#endif
        proc_mutex_pthread_cleanup(new_mutex);
        pthread_mutexattr_destroy(&mattr);
        return rv;
    }

#ifdef HAVE_PTHREAD_MUTEX_ROBUST
    if ((rv = pthread_mutexattr_setrobust_np(&mattr, 
                                               PTHREAD_MUTEX_ROBUST_NP))) {
#ifdef HAVE_ZOS_PTHREADS
        rv = errno;
#endif
        proc_mutex_pthread_cleanup(new_mutex);
        pthread_mutexattr_destroy(&mattr);
        return rv;
    }
    if ((rv = pthread_mutexattr_setprotocol(&mattr, PTHREAD_PRIO_INHERIT))) {
#ifdef HAVE_ZOS_PTHREADS
        rv = errno;
#endif
        proc_mutex_pthread_cleanup(new_mutex);
        pthread_mutexattr_destroy(&mattr);
        return rv;
    }
#endif /* HAVE_PTHREAD_MUTEX_ROBUST */

    if ((rv = pthread_mutex_init(new_mutex->os.pthread_interproc, &mattr))) {
#ifdef HAVE_ZOS_PTHREADS
        rv = errno;
#endif
        proc_mutex_pthread_cleanup(new_mutex);
        pthread_mutexattr_destroy(&mattr);
        return rv;
    }

    proc_pthread_mutex_refcount(new_mutex) = 1; /* first/parent reference */
    new_mutex->curr_locked = 0; /* mutex created now */

    if ((rv = pthread_mutexattr_destroy(&mattr))) {
#ifdef HAVE_ZOS_PTHREADS
        rv = errno;
#endif
        proc_mutex_pthread_cleanup(new_mutex);
        return rv;
    }

    apr_pool_cleanup_register(new_mutex->pool,
                              (void *)new_mutex,
                              apr_proc_mutex_cleanup, 
                              apr_pool_cleanup_null);
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_pthread_child_init(apr_proc_mutex_t **mutex,
                                                  apr_pool_t *pool, 
                                                  const char *fname)
{
    (*mutex)->curr_locked = 0;
    if (proc_pthread_mutex_inc(*mutex)) {
        apr_pool_cleanup_register(pool, *mutex, proc_pthread_mutex_unref, 
                                  apr_pool_cleanup_null);
    }
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_pthread_acquire(apr_proc_mutex_t *mutex)
{
    apr_status_t rv;

    if ((rv = pthread_mutex_lock(mutex->os.pthread_interproc))) {
#ifdef HAVE_ZOS_PTHREADS
        rv = errno;
#endif
#ifdef HAVE_PTHREAD_MUTEX_ROBUST
        /* Okay, our owner died.  Let's try to make it consistent again. */
        if (rv == EOWNERDEAD) {
            proc_pthread_mutex_dec(mutex);
            pthread_mutex_consistent_np(mutex->os.pthread_interproc);
        }
        else
#endif
        return rv;
    }
    mutex->curr_locked = 1;
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_pthread_tryacquire(apr_proc_mutex_t *mutex)
{
    apr_status_t rv;
 
    if ((rv = pthread_mutex_trylock(mutex->os.pthread_interproc))) {
#ifdef HAVE_ZOS_PTHREADS 
        rv = errno;
#endif
        if (rv == EBUSY) {
            return APR_EBUSY;
        }
#ifdef HAVE_PTHREAD_MUTEX_ROBUST
        /* Okay, our owner died.  Let's try to make it consistent again. */
        if (rv == EOWNERDEAD) {
            proc_pthread_mutex_dec(mutex);
            pthread_mutex_consistent_np(mutex->os.pthread_interproc);
        }
        else
#endif
        return rv;
    }
    mutex->curr_locked = 1;
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_pthread_release(apr_proc_mutex_t *mutex)
{
    apr_status_t rv;

    mutex->curr_locked = 0;
    if ((rv = pthread_mutex_unlock(mutex->os.pthread_interproc))) {
#ifdef HAVE_ZOS_PTHREADS
        rv = errno;
#endif
        return rv;
    }
    return APR_SUCCESS;
}

static const apr_proc_mutex_unix_lock_methods_t mutex_proc_pthread_methods =
{
    APR_PROCESS_LOCK_MECH_IS_GLOBAL,
    proc_mutex_pthread_create,
    proc_mutex_pthread_acquire,
    proc_mutex_pthread_tryacquire,
    proc_mutex_pthread_release,
    proc_mutex_pthread_cleanup,
    proc_mutex_pthread_child_init,
    proc_mutex_no_perms_set,
    APR_LOCK_PROC_PTHREAD,
    "pthread"
};

#endif

#if APR_HAS_FCNTL_SERIALIZE

static struct flock proc_mutex_lock_it;
static struct flock proc_mutex_unlock_it;

static apr_status_t proc_mutex_fcntl_release(apr_proc_mutex_t *);

static void proc_mutex_fcntl_setup(void)
{
    proc_mutex_lock_it.l_whence = SEEK_SET;   /* from current point */
    proc_mutex_lock_it.l_start = 0;           /* -"- */
    proc_mutex_lock_it.l_len = 0;             /* until end of file */
    proc_mutex_lock_it.l_type = F_WRLCK;      /* set exclusive/write lock */
    proc_mutex_lock_it.l_pid = 0;             /* pid not actually interesting */
    proc_mutex_unlock_it.l_whence = SEEK_SET; /* from current point */
    proc_mutex_unlock_it.l_start = 0;         /* -"- */
    proc_mutex_unlock_it.l_len = 0;           /* until end of file */
    proc_mutex_unlock_it.l_type = F_UNLCK;    /* set exclusive/write lock */
    proc_mutex_unlock_it.l_pid = 0;           /* pid not actually interesting */
}

static apr_status_t proc_mutex_fcntl_cleanup(void *mutex_)
{
    apr_status_t status = APR_SUCCESS;
    apr_proc_mutex_t *mutex=mutex_;

    if (mutex->curr_locked == 1) {
        status = proc_mutex_fcntl_release(mutex);
        if (status != APR_SUCCESS)
            return status;
    }
        
    if (mutex->interproc) {
        status = apr_file_close(mutex->interproc);
    }
    if (!mutex->interproc_closing
            && mutex->os.crossproc != -1
            && close(mutex->os.crossproc) == -1
            && status == APR_SUCCESS) {
        status = errno;
    }
    return status;
}    

static apr_status_t proc_mutex_fcntl_create(apr_proc_mutex_t *new_mutex,
                                            const char *fname)
{
    int rv;
 
    if (fname) {
        new_mutex->fname = apr_pstrdup(new_mutex->pool, fname);
        rv = apr_file_open(&new_mutex->interproc, new_mutex->fname,
                           APR_FOPEN_CREATE | APR_FOPEN_WRITE | APR_FOPEN_EXCL,
                           APR_UREAD | APR_UWRITE | APR_GREAD | APR_WREAD,
                           new_mutex->pool);
    }
    else {
        new_mutex->fname = apr_pstrdup(new_mutex->pool, "/tmp/aprXXXXXX");
        rv = apr_file_mktemp(&new_mutex->interproc, new_mutex->fname,
                             APR_FOPEN_CREATE | APR_FOPEN_WRITE | APR_FOPEN_EXCL,
                             new_mutex->pool);
    }
 
    if (rv != APR_SUCCESS) {
        return rv;
    }

    new_mutex->os.crossproc = new_mutex->interproc->filedes;
    new_mutex->interproc_closing = 1;
    new_mutex->curr_locked = 0;
    unlink(new_mutex->fname);
    apr_pool_cleanup_register(new_mutex->pool,
                              (void*)new_mutex,
                              apr_proc_mutex_cleanup, 
                              apr_pool_cleanup_null);
    return APR_SUCCESS; 
}

static apr_status_t proc_mutex_fcntl_acquire(apr_proc_mutex_t *mutex)
{
    int rc;

    do {
        rc = fcntl(mutex->os.crossproc, F_SETLKW, &proc_mutex_lock_it);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0) {
        return errno;
    }
    mutex->curr_locked=1;
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_fcntl_tryacquire(apr_proc_mutex_t *mutex)
{
    int rc;

    do {
        rc = fcntl(mutex->os.crossproc, F_SETLK, &proc_mutex_lock_it);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0) {
#if FCNTL_TRYACQUIRE_EACCES
        if (errno == EACCES) {
#else
        if (errno == EAGAIN) {
#endif
            return APR_EBUSY;
        }
        return errno;
    }
    mutex->curr_locked = 1;
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_fcntl_release(apr_proc_mutex_t *mutex)
{
    int rc;

    mutex->curr_locked=0;
    do {
        rc = fcntl(mutex->os.crossproc, F_SETLKW, &proc_mutex_unlock_it);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0) {
        return errno;
    }
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_fcntl_perms_set(apr_proc_mutex_t *mutex,
                                               apr_fileperms_t perms,
                                               apr_uid_t uid,
                                               apr_gid_t gid)
{

    if (mutex->fname) {
        if (!(perms & APR_FPROT_GSETID))
            gid = -1;
        if (fchown(mutex->os.crossproc, uid, gid) < 0) {
            return errno;
        }
    }
    return APR_SUCCESS;
}

static const apr_proc_mutex_unix_lock_methods_t mutex_fcntl_methods =
{
#if APR_PROCESS_LOCK_IS_GLOBAL || !APR_HAS_THREADS || defined(FCNTL_IS_GLOBAL)
    APR_PROCESS_LOCK_MECH_IS_GLOBAL,
#else
    0,
#endif
    proc_mutex_fcntl_create,
    proc_mutex_fcntl_acquire,
    proc_mutex_fcntl_tryacquire,
    proc_mutex_fcntl_release,
    proc_mutex_fcntl_cleanup,
    proc_mutex_no_child_init,
    proc_mutex_fcntl_perms_set,
    APR_LOCK_FCNTL,
    "fcntl"
};

#endif /* fcntl implementation */

#if APR_HAS_FLOCK_SERIALIZE

static apr_status_t proc_mutex_flock_release(apr_proc_mutex_t *);

static apr_status_t proc_mutex_flock_cleanup(void *mutex_)
{
    apr_status_t status = APR_SUCCESS;
    apr_proc_mutex_t *mutex=mutex_;

    if (mutex->curr_locked == 1) {
        status = proc_mutex_flock_release(mutex);
        if (status != APR_SUCCESS)
            return status;
    }
    if (mutex->interproc) { /* if it was opened properly */
        status = apr_file_close(mutex->interproc);
    }
    if (!mutex->interproc_closing
            && mutex->os.crossproc != -1
            && close(mutex->os.crossproc) == -1
            && status == APR_SUCCESS) {
        status = errno;
    }
    if (mutex->fname) {
        unlink(mutex->fname);
    }
    return status;
}    

static apr_status_t proc_mutex_flock_create(apr_proc_mutex_t *new_mutex,
                                            const char *fname)
{
    int rv;
 
    if (fname) {
        new_mutex->fname = apr_pstrdup(new_mutex->pool, fname);
        rv = apr_file_open(&new_mutex->interproc, new_mutex->fname,
                           APR_FOPEN_CREATE | APR_FOPEN_WRITE | APR_FOPEN_EXCL,
                           APR_UREAD | APR_UWRITE,
                           new_mutex->pool);
    }
    else {
        new_mutex->fname = apr_pstrdup(new_mutex->pool, "/tmp/aprXXXXXX");
        rv = apr_file_mktemp(&new_mutex->interproc, new_mutex->fname,
                             APR_FOPEN_CREATE | APR_FOPEN_WRITE | APR_FOPEN_EXCL,
                             new_mutex->pool);
    }
 
    if (rv != APR_SUCCESS) {
        proc_mutex_flock_cleanup(new_mutex);
        return rv;
    }

    new_mutex->os.crossproc = new_mutex->interproc->filedes;
    new_mutex->interproc_closing = 1;
    new_mutex->curr_locked = 0;
    apr_pool_cleanup_register(new_mutex->pool, (void *)new_mutex,
                              apr_proc_mutex_cleanup,
                              apr_pool_cleanup_null);
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_flock_acquire(apr_proc_mutex_t *mutex)
{
    int rc;

    do {
        rc = flock(mutex->os.crossproc, LOCK_EX);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0) {
        return errno;
    }
    mutex->curr_locked = 1;
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_flock_tryacquire(apr_proc_mutex_t *mutex)
{
    int rc;

    do {
        rc = flock(mutex->os.crossproc, LOCK_EX | LOCK_NB);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return APR_EBUSY;
        }
        return errno;
    }
    mutex->curr_locked = 1;
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_flock_release(apr_proc_mutex_t *mutex)
{
    int rc;

    mutex->curr_locked = 0;
    do {
        rc = flock(mutex->os.crossproc, LOCK_UN);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0) {
        return errno;
    }
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_flock_child_init(apr_proc_mutex_t **mutex,
                                                apr_pool_t *pool, 
                                                const char *fname)
{
    apr_proc_mutex_t *new_mutex;
    int rv;

    if (!fname) {
        fname = (*mutex)->fname;
        if (!fname) {
            return APR_SUCCESS;
        }
    }

    new_mutex = (apr_proc_mutex_t *)apr_pmemdup(pool, *mutex,
                                                sizeof(apr_proc_mutex_t));
    new_mutex->pool = pool;
    new_mutex->fname = apr_pstrdup(pool, fname);
    rv = apr_file_open(&new_mutex->interproc, new_mutex->fname,
                       APR_FOPEN_WRITE, 0, new_mutex->pool);
    if (rv != APR_SUCCESS) {
        return rv;
    }
    new_mutex->os.crossproc = new_mutex->interproc->filedes;
    new_mutex->interproc_closing = 1;

    *mutex = new_mutex;
    return APR_SUCCESS;
}

static apr_status_t proc_mutex_flock_perms_set(apr_proc_mutex_t *mutex,
                                               apr_fileperms_t perms,
                                               apr_uid_t uid,
                                               apr_gid_t gid)
{

    if (mutex->fname) {
        if (!(perms & APR_FPROT_GSETID))
            gid = -1;
        if (fchown(mutex->os.crossproc, uid, gid) < 0) {
            return errno;
        }
    }
    return APR_SUCCESS;
}

static const apr_proc_mutex_unix_lock_methods_t mutex_flock_methods =
{
#if APR_PROCESS_LOCK_IS_GLOBAL || !APR_HAS_THREADS || defined(FLOCK_IS_GLOBAL)
    APR_PROCESS_LOCK_MECH_IS_GLOBAL,
#else
    0,
#endif
    proc_mutex_flock_create,
    proc_mutex_flock_acquire,
    proc_mutex_flock_tryacquire,
    proc_mutex_flock_release,
    proc_mutex_flock_cleanup,
    proc_mutex_flock_child_init,
    proc_mutex_flock_perms_set,
    APR_LOCK_FLOCK,
    "flock"
};

#endif /* flock implementation */

void apr_proc_mutex_unix_setup_lock(void)
{
    /* setup only needed for sysvsem and fnctl */
#if APR_HAS_SYSVSEM_SERIALIZE
    proc_mutex_sysv_setup();
#endif
#if APR_HAS_FCNTL_SERIALIZE
    proc_mutex_fcntl_setup();
#endif
}

static apr_status_t proc_mutex_choose_method(apr_proc_mutex_t *new_mutex,
                                             apr_lockmech_e mech,
                                             apr_os_proc_mutex_t *ospmutex)
{
#if APR_HAS_PROC_PTHREAD_SERIALIZE
    new_mutex->os.pthread_interproc = NULL;
#endif
#if APR_HAS_POSIXSEM_SERIALIZE
    new_mutex->os.psem_interproc = NULL;
#endif
#if APR_HAS_SYSVSEM_SERIALIZE || APR_HAS_FCNTL_SERIALIZE || APR_HAS_FLOCK_SERIALIZE
    new_mutex->os.crossproc = -1;

#if APR_HAS_FCNTL_SERIALIZE || APR_HAS_FLOCK_SERIALIZE
    new_mutex->interproc = NULL;
    new_mutex->interproc_closing = 0;
#endif
#endif

    switch (mech) {
    case APR_LOCK_FCNTL:
#if APR_HAS_FCNTL_SERIALIZE
        new_mutex->meth = &mutex_fcntl_methods;
        if (ospmutex) {
            if (ospmutex->crossproc == -1) {
                return APR_EINVAL;
            }
            new_mutex->os.crossproc = ospmutex->crossproc;
        }
#else
        return APR_ENOTIMPL;
#endif
        break;
    case APR_LOCK_FLOCK:
#if APR_HAS_FLOCK_SERIALIZE
        new_mutex->meth = &mutex_flock_methods;
        if (ospmutex) {
            if (ospmutex->crossproc == -1) {
                return APR_EINVAL;
            }
            new_mutex->os.crossproc = ospmutex->crossproc;
        }
#else
        return APR_ENOTIMPL;
#endif
        break;
    case APR_LOCK_SYSVSEM:
#if APR_HAS_SYSVSEM_SERIALIZE
        new_mutex->meth = &mutex_sysv_methods;
        if (ospmutex) {
            if (ospmutex->crossproc == -1) {
                return APR_EINVAL;
            }
            new_mutex->os.crossproc = ospmutex->crossproc;
        }
#else
        return APR_ENOTIMPL;
#endif
        break;
    case APR_LOCK_POSIXSEM:
#if APR_HAS_POSIXSEM_SERIALIZE
        new_mutex->meth = &mutex_posixsem_methods;
        if (ospmutex) {
            if (ospmutex->psem_interproc == NULL) {
                return APR_EINVAL;
            }
            new_mutex->os.psem_interproc = ospmutex->psem_interproc;
        }
#else
        return APR_ENOTIMPL;
#endif
        break;
    case APR_LOCK_PROC_PTHREAD:
#if APR_HAS_PROC_PTHREAD_SERIALIZE
        new_mutex->meth = &mutex_proc_pthread_methods;
        if (ospmutex) {
            if (ospmutex->pthread_interproc == NULL) {
                return APR_EINVAL;
            }
            new_mutex->os.pthread_interproc = ospmutex->pthread_interproc;
        }
#else
        return APR_ENOTIMPL;
#endif
        break;
    case APR_LOCK_DEFAULT:
#if APR_USE_FLOCK_SERIALIZE
        new_mutex->meth = &mutex_flock_methods;
        if (ospmutex) {
            if (ospmutex->crossproc == -1) {
                return APR_EINVAL;
            }
            new_mutex->os.crossproc = ospmutex->crossproc;
        }
#elif APR_USE_SYSVSEM_SERIALIZE
        new_mutex->meth = &mutex_sysv_methods;
        if (ospmutex) {
            if (ospmutex->crossproc == -1) {
                return APR_EINVAL;
            }
            new_mutex->os.crossproc = ospmutex->crossproc;
        }
#elif APR_USE_FCNTL_SERIALIZE
        new_mutex->meth = &mutex_fcntl_methods;
        if (ospmutex) {
            if (ospmutex->crossproc == -1) {
                return APR_EINVAL;
            }
            new_mutex->os.crossproc = ospmutex->crossproc;
        }
#elif APR_USE_PROC_PTHREAD_SERIALIZE
        new_mutex->meth = &mutex_proc_pthread_methods;
        if (ospmutex) {
            if (ospmutex->pthread_interproc == NULL) {
                return APR_EINVAL;
            }
            new_mutex->os.pthread_interproc = ospmutex->pthread_interproc;
        }
#elif APR_USE_POSIXSEM_SERIALIZE
        new_mutex->meth = &mutex_posixsem_methods;
        if (ospmutex) {
            if (ospmutex->psem_interproc == NULL) {
                return APR_EINVAL;
            }
            new_mutex->os.psem_interproc = ospmutex->psem_interproc;
        }
#else
        return APR_ENOTIMPL;
#endif
        break;
    default:
        return APR_ENOTIMPL;
    }
    return APR_SUCCESS;
}

APR_DECLARE(const char *) apr_proc_mutex_defname(void)
{
    apr_status_t rv;
    apr_proc_mutex_t mutex;

    if ((rv = proc_mutex_choose_method(&mutex, APR_LOCK_DEFAULT,
                                       NULL)) != APR_SUCCESS) {
        return "unknown";
    }

    return apr_proc_mutex_name(&mutex);
}
   
static apr_status_t proc_mutex_create(apr_proc_mutex_t *new_mutex, apr_lockmech_e mech, const char *fname)
{
    apr_status_t rv;

    if ((rv = proc_mutex_choose_method(new_mutex, mech,
                                       NULL)) != APR_SUCCESS) {
        return rv;
    }

    if ((rv = new_mutex->meth->create(new_mutex, fname)) != APR_SUCCESS) {
        return rv;
    }

    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_proc_mutex_create(apr_proc_mutex_t **mutex,
                                                const char *fname,
                                                apr_lockmech_e mech,
                                                apr_pool_t *pool)
{
    apr_proc_mutex_t *new_mutex;
    apr_status_t rv;

    new_mutex = apr_pcalloc(pool, sizeof(apr_proc_mutex_t));
    new_mutex->pool = pool;

    if ((rv = proc_mutex_create(new_mutex, mech, fname)) != APR_SUCCESS)
        return rv;

    *mutex = new_mutex;
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_proc_mutex_child_init(apr_proc_mutex_t **mutex,
                                                    const char *fname,
                                                    apr_pool_t *pool)
{
    return (*mutex)->meth->child_init(mutex, pool, fname);
}

APR_DECLARE(apr_status_t) apr_proc_mutex_lock(apr_proc_mutex_t *mutex)
{
    return mutex->meth->acquire(mutex);
}

APR_DECLARE(apr_status_t) apr_proc_mutex_trylock(apr_proc_mutex_t *mutex)
{
    return mutex->meth->tryacquire(mutex);
}

APR_DECLARE(apr_status_t) apr_proc_mutex_unlock(apr_proc_mutex_t *mutex)
{
    return mutex->meth->release(mutex);
}

APR_DECLARE(apr_status_t) apr_proc_mutex_cleanup(void *mutex)
{
    return ((apr_proc_mutex_t *)mutex)->meth->cleanup(mutex);
}

APR_DECLARE(apr_lockmech_e) apr_proc_mutex_mech(apr_proc_mutex_t *mutex)
{
    return mutex->meth->mech;
}

APR_DECLARE(const char *) apr_proc_mutex_name(apr_proc_mutex_t *mutex)
{
    return mutex->meth->name;
}

APR_DECLARE(const char *) apr_proc_mutex_lockfile(apr_proc_mutex_t *mutex)
{
    /* POSIX sems use the fname field but don't use a file,
     * so be careful. */
#if APR_HAS_FLOCK_SERIALIZE
    if (mutex->meth == &mutex_flock_methods) {
        return mutex->fname;
    }
#endif
#if APR_HAS_FCNTL_SERIALIZE
    if (mutex->meth == &mutex_fcntl_methods) {
        return mutex->fname;
    }
#endif
    return NULL;
}

APR_PERMS_SET_IMPLEMENT(proc_mutex)
{
    apr_proc_mutex_t *mutex = (apr_proc_mutex_t *)theproc_mutex;
    return mutex->meth->perms_set(mutex, perms, uid, gid);
}

APR_POOL_IMPLEMENT_ACCESSOR(proc_mutex)

/* Implement OS-specific accessors defined in apr_portable.h */

APR_DECLARE(apr_status_t) apr_os_proc_mutex_get_ex(apr_os_proc_mutex_t *ospmutex, 
                                                   apr_proc_mutex_t *pmutex,
                                                   apr_lockmech_e *mech)
{
    *ospmutex = pmutex->os;
    if (mech) {
        *mech = pmutex->meth->mech;
    }
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_os_proc_mutex_get(apr_os_proc_mutex_t *ospmutex,
                                                apr_proc_mutex_t *pmutex)
{
    return apr_os_proc_mutex_get_ex(ospmutex, pmutex, NULL);
}

APR_DECLARE(apr_status_t) apr_os_proc_mutex_put_ex(apr_proc_mutex_t **pmutex,
                                                apr_os_proc_mutex_t *ospmutex,
                                                apr_lockmech_e mech,
                                                int register_cleanup,
                                                apr_pool_t *pool)
{
    apr_status_t rv;
    if (pool == NULL) {
        return APR_ENOPOOL;
    }

    if ((*pmutex) == NULL) {
        (*pmutex) = (apr_proc_mutex_t *)apr_pcalloc(pool,
                                                    sizeof(apr_proc_mutex_t));
        (*pmutex)->pool = pool;
    }
    rv = proc_mutex_choose_method(*pmutex, mech, ospmutex);
#if APR_HAS_FCNTL_SERIALIZE || APR_HAS_FLOCK_SERIALIZE
    if (rv == APR_SUCCESS) {
        rv = apr_os_file_put(&(*pmutex)->interproc, &(*pmutex)->os.crossproc,
                             0, pool);
    }
#endif

    if (rv == APR_SUCCESS && register_cleanup) {
        apr_pool_cleanup_register(pool, *pmutex, apr_proc_mutex_cleanup, 
                                  apr_pool_cleanup_null);
    }
    return rv;
}

APR_DECLARE(apr_status_t) apr_os_proc_mutex_put(apr_proc_mutex_t **pmutex,
                                                apr_os_proc_mutex_t *ospmutex,
                                                apr_pool_t *pool)
{
    return apr_os_proc_mutex_put_ex(pmutex, ospmutex, APR_LOCK_DEFAULT,
                                    0, pool);
}

