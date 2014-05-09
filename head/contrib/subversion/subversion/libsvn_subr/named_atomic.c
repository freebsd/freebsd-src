/*
 * svn_named_atomic.c: routines for machine-wide named atomics.
 *
 * ====================================================================
 *    Licensed to the Apache Software Foundation (ASF) under one
 *    or more contributor license agreements.  See the NOTICE file
 *    distributed with this work for additional information
 *    regarding copyright ownership.  The ASF licenses this file
 *    to you under the Apache License, Version 2.0 (the
 *    "License"); you may not use this file except in compliance
 *    with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing,
 *    software distributed under the License is distributed on an
 *    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *    KIND, either express or implied.  See the License for the
 *    specific language governing permissions and limitations
 *    under the License.
 * ====================================================================
 */

#include "private/svn_named_atomic.h"

#include <apr_global_mutex.h>
#include <apr_mmap.h>

#include "svn_private_config.h"
#include "private/svn_atomic.h"
#include "private/svn_mutex.h"
#include "svn_pools.h"
#include "svn_dirent_uri.h"
#include "svn_io.h"

/* Implementation aspects.
 *
 * We use a single shared memory block (memory mapped file) that will be
 * created by the first user and merely mapped by all subsequent ones.
 * The memory block contains an short header followed by a fixed-capacity
 * array of named atomics. The number of entries currently in use is stored
 * in the header part.
 *
 * Finding / creating the MMAP object as well as adding new array entries
 * is being guarded by an APR global mutex. Since releasing the MMAP
 * structure and closing the underlying does not affect other users of the
 * same, cleanup will not be synchronized.
 *
 * The array is append-only.  Once a process mapped the block into its
 * address space, it may freely access any of the used entries.  However,
 * it must synchronize access to the volatile data within the entries.
 * On Windows and where otherwise supported by GCC, lightweight "lock-free"
 * synchronization will be used. Other targets serialize all access using
 * a global mutex.
 *
 * Atomics will be identified by their name (a short string) and lookup
 * takes linear time. But even that takes only about 10 microseconds for a
 * full array scan -- which is in the same order of magnitude than e.g. a
 * single global mutex lock / unlock pair.
 */

/* Capacity of our shared memory object, i.e. max number of named atomics
 * that may be created. Should have the form 2**N - 1.
 */
#define MAX_ATOMIC_COUNT 1023

/* We choose the size of a single named atomic object to fill a complete
 * cache line (on most architectures).  Thereby, we minimize the cache
 * sync. overhead between different CPU cores.
 */
#define CACHE_LINE_LENGTH 64

/* We need 8 bytes for the actual value and the remainder is used to
 * store the NUL-terminated name.
 *
 * Must not be smaller than SVN_NAMED_ATOMIC__MAX_NAME_LENGTH.
 */
#define MAX_NAME_LENGTH (CACHE_LINE_LENGTH - sizeof(apr_int64_t) - 1)

/* Particle that will be appended to the namespace name to form the
 * name of the mutex / lock file used for that namespace.
 */
#define MUTEX_NAME_SUFFIX ".mutex"

/* Particle that will be appended to the namespace name to form the
 * name of the shared memory file that backs that namespace.
 */
#define SHM_NAME_SUFFIX ".shm"

/* Platform-dependent implementations of our basic atomic operations.
 * NA_SYNCHRONIZE(op) will ensure that the OP gets executed atomically.
 * This will be zero-overhead if OP itself is already atomic.
 *
 * (We don't call it SYNCHRONIZE because Windows has a preprocess macro by
 * that name.)
 *
 * The default implementation will use the same mutex for initialization
 * as well as any type of data access.  This is quite expensive and we
 * can do much better on most platforms.
 */
#if defined(WIN32) && ((_WIN32_WINNT >= 0x0502) || defined(InterlockedExchangeAdd64))

/* Interlocked API / intrinsics guarantee full data synchronization
 */
#define synched_read(mem) *mem
#define synched_write(mem, value) InterlockedExchange64(mem, value)
#define synched_add(mem, delta) InterlockedExchangeAdd64(mem, delta)
#define synched_cmpxchg(mem, value, comperand) \
  InterlockedCompareExchange64(mem, value, comperand)

#define NA_SYNCHRONIZE(_atomic,op) op;
#define NA_SYNCHRONIZE_IS_FAST TRUE

#elif SVN_HAS_ATOMIC_BUILTINS

/* GCC provides atomic intrinsics for most common CPU types
 */
#define synched_read(mem) *mem
#define synched_write(mem, value) __sync_lock_test_and_set(mem, value)
#define synched_add(mem, delta) __sync_add_and_fetch(mem, delta)
#define synched_cmpxchg(mem, value, comperand) \
  __sync_val_compare_and_swap(mem, comperand, value)

#define NA_SYNCHRONIZE(_atomic,op) op;
#define NA_SYNCHRONIZE_IS_FAST TRUE

#else

/* Default implementation
 */
static apr_int64_t
synched_read(volatile apr_int64_t *mem)
{
  return *mem;
}

static apr_int64_t
synched_write(volatile apr_int64_t *mem, apr_int64_t value)
{
  apr_int64_t old_value = *mem;
  *mem = value;

  return old_value;
}

static apr_int64_t
synched_add(volatile apr_int64_t *mem, apr_int64_t delta)
{
  return *mem += delta;
}

static apr_int64_t
synched_cmpxchg(volatile apr_int64_t *mem,
                apr_int64_t value,
                apr_int64_t comperand)
{
  apr_int64_t old_value = *mem;
  if (old_value == comperand)
    *mem = value;

  return old_value;
}

#define NA_SYNCHRONIZE(_atomic,op)\
  do{\
  SVN_ERR(lock(_atomic->mutex));\
  op;\
  SVN_ERR(unlock(_atomic->mutex,SVN_NO_ERROR));\
  }while(0)

#define NA_SYNCHRONIZE_IS_FAST FALSE

#endif

/* Structure describing a single atomic: its VALUE and NAME.
 */
struct named_atomic_data_t
{
  volatile apr_int64_t value;
  char name[MAX_NAME_LENGTH + 1];
};

/* Content of our shared memory buffer.  COUNT is the number
 * of used entries in ATOMICS.  Insertion is append-only.
 * PADDING is used to align the header information with the
 * atomics to create a favorable data alignment.
 */
struct shared_data_t
{
  volatile apr_uint32_t count;
  char padding [sizeof(struct named_atomic_data_t) - sizeof(apr_uint32_t)];

  struct named_atomic_data_t atomics[MAX_ATOMIC_COUNT];
};

/* Structure combining all objects that we need for access serialization.
 */
struct mutex_t
{
  /* Inter-process sync. is handled by through lock file. */
  apr_file_t *lock_file;

  /* Pool to be used with lock / unlock functions */
  apr_pool_t *pool;
};

/* API structure combining the atomic data and the access mutex
 */
struct svn_named_atomic__t
{
  /* pointer into the shared memory */
  struct named_atomic_data_t *data;

  /* sync. object; never NULL (even if unused) */
  struct mutex_t *mutex;
};

/* This is intended to be a singleton struct.  It contains all
 * information necessary to initialize and access the shared
 * memory.
 */
struct svn_atomic_namespace__t
{
  /* Pointer to the shared data mapped into our process */
  struct shared_data_t *data;

  /* Last time we checked, this was the number of used
   * (i.e. fully initialized) items.  I.e. we can read
   * their names without further sync. */
  volatile svn_atomic_t min_used;

  /* for each atomic in the shared memory, we hand out
   * at most one API-level object. */
  struct svn_named_atomic__t atomics[MAX_ATOMIC_COUNT];

  /* Synchronization object for this namespace */
  struct mutex_t mutex;
};

/* On most operating systems APR implements file locks per process, not
 * per file. I.e. the lock file will only sync. among processes but within
 * a process, we must use a mutex to sync the threads. */
/* Compare ../libsvn_fs_fs/fs.h:SVN_FS_FS__USE_LOCK_MUTEX */
#if APR_HAS_THREADS && !defined(WIN32)
#define USE_THREAD_MUTEX 1
#else
#define USE_THREAD_MUTEX 0
#endif

/* Used for process-local thread sync.
 */
static svn_mutex__t *thread_mutex = NULL;

#if APR_HAS_MMAP
/* Initialization flag for the above used by svn_atomic__init_once.
 */
static volatile svn_atomic_t mutex_initialized = FALSE;

/* Initialize the thread sync. structures.
 * To be called by svn_atomic__init_once.
 */
static svn_error_t *
init_thread_mutex(void *baton, apr_pool_t *pool)
{
  /* let the mutex live as long as the APR */
  apr_pool_t *global_pool = svn_pool_create(NULL);

  return svn_mutex__init(&thread_mutex, USE_THREAD_MUTEX, global_pool);
}
#endif /* APR_HAS_MMAP */

/* Utility that acquires our global mutex and converts error types.
 */
static svn_error_t *
lock(struct mutex_t *mutex)
{
  svn_error_t *err;

  /* Get lock on the filehandle. */
  SVN_ERR(svn_mutex__lock(thread_mutex));
  err = svn_io_lock_open_file(mutex->lock_file, TRUE, FALSE, mutex->pool);

  return err
    ? svn_mutex__unlock(thread_mutex, err)
    : err;
}

/* Utility that releases the lock previously acquired via lock().  If the
 * unlock succeeds and OUTER_ERR is not NULL, OUTER_ERR will be returned.
 * Otherwise, return the result of the unlock operation.
 */
static svn_error_t *
unlock(struct mutex_t *mutex, svn_error_t * outer_err)
{
  svn_error_t *unlock_err
      = svn_io_unlock_open_file(mutex->lock_file, mutex->pool);
  return svn_mutex__unlock(thread_mutex,
                           svn_error_compose_create(outer_err,
                                                    unlock_err));
}

#if APR_HAS_MMAP
/* The last user to close a particular namespace should also remove the
 * lock file.  Failure to do so, however, does not affect further uses
 * of the same namespace.
 */
static apr_status_t
delete_lock_file(void *arg)
{
  struct mutex_t *mutex = arg;
  const char *lock_name = NULL;

  /* locks have already been cleaned up. Simply close the file */
  apr_status_t status = apr_file_close(mutex->lock_file);

  /* Remove the file from disk. This will fail if there ares still other
   * users of this lock file, i.e. namespace. */
  apr_file_name_get(&lock_name, mutex->lock_file);
  if (lock_name)
    apr_file_remove(lock_name, mutex->pool);

  return status;
}
#endif /* APR_HAS_MMAP */

/* Validate the ATOMIC parameter, i.e it's address.  Correct code will
 * never need this but if someone should accidentally to use a NULL or
 * incomplete structure, let's catch that here instead of segfaulting.
 */
static svn_error_t *
validate(svn_named_atomic__t *atomic)
{
  return atomic && atomic->data && atomic->mutex
    ? SVN_NO_ERROR
    : svn_error_create(SVN_ERR_BAD_ATOMIC, 0, _("Not a valid atomic"));
}

/* Auto-initialize and return in *ATOMIC the API-level object for the
 * atomic with index I within NS. */
static void
return_atomic(svn_named_atomic__t **atomic,
              svn_atomic_namespace__t *ns,
              int i)
{
  *atomic = &ns->atomics[i];
  if (ns->atomics[i].data == NULL)
    {
      (*atomic)->mutex = &ns->mutex;
      (*atomic)->data = &ns->data->atomics[i];
    }
}

/* Implement API */

svn_boolean_t
svn_named_atomic__is_supported(void)
{
#if !APR_HAS_MMAP
  return FALSE;
#elif !defined(_WIN32)
  return TRUE;
#else
  static svn_tristate_t result = svn_tristate_unknown;

  if (result == svn_tristate_unknown)
    {
      /* APR SHM implementation requires the creation of global objects */
      HANDLE handle = CreateFileMappingA(INVALID_HANDLE_VALUE,
                                         NULL,
                                         PAGE_READONLY,
                                         0,
                                         1,
                                         "Global\\__RandomXZY_svn");
      if (handle != NULL)
        {
          CloseHandle(handle);
          result = svn_tristate_true;
        }
      else
        result = svn_tristate_false;
    }

  return result == svn_tristate_true;
#endif /* _WIN32 */
}

svn_boolean_t
svn_named_atomic__is_efficient(void)
{
  return NA_SYNCHRONIZE_IS_FAST;
}

svn_error_t *
svn_atomic_namespace__create(svn_atomic_namespace__t **ns,
                             const char *name,
                             apr_pool_t *result_pool)
{
#if !APR_HAS_MMAP
  return svn_error_create(APR_ENOTIMPL, NULL, NULL);
#else
  apr_status_t apr_err;
  svn_error_t *err;
  apr_file_t *file;
  apr_mmap_t *mmap;
  const char *shm_name, *lock_name;
  apr_finfo_t finfo;

  apr_pool_t *subpool = svn_pool_create(result_pool);

  /* allocate the namespace data structure
   */
  svn_atomic_namespace__t *new_ns = apr_pcalloc(result_pool, sizeof(**ns));

  /* construct the names of the system objects that we need
   */
  shm_name = apr_pstrcat(subpool, name, SHM_NAME_SUFFIX, NULL);
  lock_name = apr_pstrcat(subpool, name, MUTEX_NAME_SUFFIX, NULL);

  /* initialize the lock objects
   */
  SVN_ERR(svn_atomic__init_once(&mutex_initialized, init_thread_mutex, NULL,
                                result_pool));

  new_ns->mutex.pool = result_pool;
  SVN_ERR(svn_io_file_open(&new_ns->mutex.lock_file, lock_name,
                           APR_READ | APR_WRITE | APR_CREATE,
                           APR_OS_DEFAULT,
                           result_pool));

  /* Make sure the last user of our lock file will actually remove it.
   * Please note that only the last file handle begin closed will actually
   * remove the underlying file (see docstring for apr_file_remove).
   */
  apr_pool_cleanup_register(result_pool, &new_ns->mutex,
                            delete_lock_file,
                            apr_pool_cleanup_null);

  /* Prevent concurrent initialization.
   */
  SVN_ERR(lock(&new_ns->mutex));

  /* First, make sure that the underlying file exists.  If it doesn't
   * exist, create one and initialize its content.
   */
  err = svn_io_file_open(&file, shm_name,
                          APR_READ | APR_WRITE | APR_CREATE,
                          APR_OS_DEFAULT,
                          result_pool);
  if (!err)
    {
      err = svn_io_stat(&finfo, shm_name, APR_FINFO_SIZE, subpool);
      if (!err && finfo.size < sizeof(struct shared_data_t))
        {
           /* Zero all counters, values and names.
            */
           struct shared_data_t initial_data;
           memset(&initial_data, 0, sizeof(initial_data));
           err = svn_io_file_write_full(file, &initial_data,
                                        sizeof(initial_data), NULL,
                                        subpool);
        }
    }

  /* Now, map it into memory.
   */
  if (!err)
    {
      apr_err = apr_mmap_create(&mmap, file, 0, sizeof(*new_ns->data),
                                APR_MMAP_READ | APR_MMAP_WRITE , result_pool);
      if (!apr_err)
        new_ns->data = mmap->mm;
      else
        err = svn_error_createf(apr_err, NULL,
                                _("MMAP failed for file '%s'"), shm_name);
    }

  svn_pool_destroy(subpool);

  if (!err && new_ns->data)
    {
      /* Detect severe cases of corruption (i.e. when some outsider messed
       * with our data file)
       */
      if (new_ns->data->count > MAX_ATOMIC_COUNT)
        return svn_error_create(SVN_ERR_CORRUPTED_ATOMIC_STORAGE, 0,
                       _("Number of atomics in namespace is too large."));

      /* Cache the number of existing, complete entries.  There can't be
       * incomplete ones from other processes because we hold the mutex.
       * Our process will also not access this information since we are
       * either being called from within svn_atomic__init_once or by
       * svn_atomic_namespace__create for a new object.
       */
      new_ns->min_used = new_ns->data->count;
      *ns = new_ns;
    }

  /* Unlock to allow other processes may access the shared memory as well.
   */
  return unlock(&new_ns->mutex, err);
#endif /* APR_HAS_MMAP */
}

svn_error_t *
svn_atomic_namespace__cleanup(const char *name,
                              apr_pool_t *pool)
{
  const char *shm_name, *lock_name;

  /* file names used for the specified namespace */
  shm_name = apr_pstrcat(pool, name, SHM_NAME_SUFFIX, NULL);
  lock_name = apr_pstrcat(pool, name, MUTEX_NAME_SUFFIX, NULL);

  /* remove these files if they exist */
  SVN_ERR(svn_io_remove_file2(shm_name, TRUE, pool));
  SVN_ERR(svn_io_remove_file2(lock_name, TRUE, pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_named_atomic__get(svn_named_atomic__t **atomic,
                      svn_atomic_namespace__t *ns,
                      const char *name,
                      svn_boolean_t auto_create)
{
  apr_uint32_t i, count;
  svn_error_t *error = SVN_NO_ERROR;
  apr_size_t len = strlen(name);

  /* Check parameters and make sure we return a NULL atomic
   * in case of failure.
   */
  *atomic = NULL;
  if (len > SVN_NAMED_ATOMIC__MAX_NAME_LENGTH)
    return svn_error_create(SVN_ERR_BAD_ATOMIC, 0,
                            _("Atomic's name is too long."));

  /* If no namespace has been provided, bail out.
   */
  if (ns == NULL || ns->data == NULL)
    return svn_error_create(SVN_ERR_BAD_ATOMIC, 0,
                            _("Namespace has not been initialized."));

  /* Optimistic lookup.
   * Because we never change the name of existing atomics and may only
   * append new ones, we can safely compare the name of existing ones
   * with the name that we are looking for.
   */
  for (i = 0, count = svn_atomic_read(&ns->min_used); i < count; ++i)
    if (strncmp(ns->data->atomics[i].name, name, len + 1) == 0)
      {
        return_atomic(atomic, ns, i);
        return SVN_NO_ERROR;
      }

  /* Try harder:
   * Serialize all lookup and insert the item, if necessary and allowed.
   */
  SVN_ERR(lock(&ns->mutex));

  /* We only need to check for new entries.
   */
  for (i = count; i < ns->data->count; ++i)
    if (strncmp(ns->data->atomics[i].name, name, len + 1) == 0)
      {
        return_atomic(atomic, ns, i);

        /* Update our cached number of complete entries. */
        svn_atomic_set(&ns->min_used, ns->data->count);

        return unlock(&ns->mutex, error);
      }

  /* Not found.  Append a new entry, if allowed & possible.
   */
  if (auto_create)
    {
      if (ns->data->count < MAX_ATOMIC_COUNT)
        {
          ns->data->atomics[ns->data->count].value = 0;
          memcpy(ns->data->atomics[ns->data->count].name,
                 name,
                 len+1);

          return_atomic(atomic, ns, ns->data->count);
          ++ns->data->count;
        }
        else
          error = svn_error_create(SVN_ERR_BAD_ATOMIC, 0,
                                  _("Out of slots for named atomic."));
    }

  /* We are mainly done here.  Let others continue their work.
   */
  SVN_ERR(unlock(&ns->mutex, error));

  /* Only now can we be sure that a full memory barrier has been set
   * and that the new entry has been written to memory in full.
   */
  svn_atomic_set(&ns->min_used, ns->data->count);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_named_atomic__read(apr_int64_t *value,
                       svn_named_atomic__t *atomic)
{
  SVN_ERR(validate(atomic));
  NA_SYNCHRONIZE(atomic, *value = synched_read(&atomic->data->value));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_named_atomic__write(apr_int64_t *old_value,
                        apr_int64_t new_value,
                        svn_named_atomic__t *atomic)
{
  apr_int64_t temp;

  SVN_ERR(validate(atomic));
  NA_SYNCHRONIZE(atomic, temp = synched_write(&atomic->data->value, new_value));

  if (old_value)
    *old_value = temp;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_named_atomic__add(apr_int64_t *new_value,
                      apr_int64_t delta,
                      svn_named_atomic__t *atomic)
{
  apr_int64_t temp;

  SVN_ERR(validate(atomic));
  NA_SYNCHRONIZE(atomic, temp = synched_add(&atomic->data->value, delta));

  if (new_value)
    *new_value = temp;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_named_atomic__cmpxchg(apr_int64_t *old_value,
                          apr_int64_t new_value,
                          apr_int64_t comperand,
                          svn_named_atomic__t *atomic)
{
  apr_int64_t temp;

  SVN_ERR(validate(atomic));
  NA_SYNCHRONIZE(atomic, temp = synched_cmpxchg(&atomic->data->value,
                                                new_value,
                                                comperand));

  if (old_value)
    *old_value = temp;

  return SVN_NO_ERROR;
}
