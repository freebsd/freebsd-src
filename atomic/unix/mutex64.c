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

#include "apr_arch_atomic.h"
#include "apr_thread_mutex.h"

#if defined(USE_ATOMICS_GENERIC) || defined (NEED_ATOMICS_GENERIC64)

#include <stdlib.h>

#if APR_HAS_THREADS
#   define DECLARE_MUTEX_LOCKED(name, mem)  \
        apr_thread_mutex_t *name = mutex_hash(mem)
#   define MUTEX_UNLOCK(name)                                   \
        do {                                                    \
            if (apr_thread_mutex_unlock(name) != APR_SUCCESS)   \
                abort();                                        \
        } while (0)
#else
#   define DECLARE_MUTEX_LOCKED(name, mem)
#   define MUTEX_UNLOCK(name)
#   warning Be warned: using stubs for all atomic operations
#endif

#if APR_HAS_THREADS

static apr_thread_mutex_t **hash_mutex;

#define NUM_ATOMIC_HASH 7
/* shift by 2 to get rid of alignment issues */
#define ATOMIC_HASH(x) (unsigned int)(((unsigned long)(x)>>2)%(unsigned int)NUM_ATOMIC_HASH)

static apr_status_t atomic_cleanup(void *data)
{
    if (hash_mutex == data)
        hash_mutex = NULL;

    return APR_SUCCESS;
}

apr_status_t apr__atomic_generic64_init(apr_pool_t *p)
{
    int i;
    apr_status_t rv;

    if (hash_mutex != NULL)
        return APR_SUCCESS;

    hash_mutex = apr_palloc(p, sizeof(apr_thread_mutex_t*) * NUM_ATOMIC_HASH);
    apr_pool_cleanup_register(p, hash_mutex, atomic_cleanup,
                              apr_pool_cleanup_null);

    for (i = 0; i < NUM_ATOMIC_HASH; i++) {
        rv = apr_thread_mutex_create(&(hash_mutex[i]),
                                     APR_THREAD_MUTEX_DEFAULT, p);
        if (rv != APR_SUCCESS) {
           return rv;
        }
    }

    return APR_SUCCESS;
}

static APR_INLINE apr_thread_mutex_t *mutex_hash(volatile apr_uint64_t *mem)
{
    apr_thread_mutex_t *mutex = hash_mutex[ATOMIC_HASH(mem)];

    if (apr_thread_mutex_lock(mutex) != APR_SUCCESS) {
        abort();
    }

    return mutex;
}

#else

apr_status_t apr__atomic_generic64_init(apr_pool_t *p)
{
    return APR_SUCCESS;
}

#endif /* APR_HAS_THREADS */

APR_DECLARE(apr_uint64_t) apr_atomic_read64(volatile apr_uint64_t *mem)
{
    return *mem;
}

APR_DECLARE(void) apr_atomic_set64(volatile apr_uint64_t *mem, apr_uint64_t val)
{
    DECLARE_MUTEX_LOCKED(mutex, mem);

    *mem = val;

    MUTEX_UNLOCK(mutex);
}

APR_DECLARE(apr_uint64_t) apr_atomic_add64(volatile apr_uint64_t *mem, apr_uint64_t val)
{
    apr_uint64_t old_value;
    DECLARE_MUTEX_LOCKED(mutex, mem);

    old_value = *mem;
    *mem += val;

    MUTEX_UNLOCK(mutex);

    return old_value;
}

APR_DECLARE(void) apr_atomic_sub64(volatile apr_uint64_t *mem, apr_uint64_t val)
{
    DECLARE_MUTEX_LOCKED(mutex, mem);
    *mem -= val;
    MUTEX_UNLOCK(mutex);
}

APR_DECLARE(apr_uint64_t) apr_atomic_inc64(volatile apr_uint64_t *mem)
{
    return apr_atomic_add64(mem, 1);
}

APR_DECLARE(int) apr_atomic_dec64(volatile apr_uint64_t *mem)
{
    apr_uint64_t new;
    DECLARE_MUTEX_LOCKED(mutex, mem);

    (*mem)--;
    new = *mem;

    MUTEX_UNLOCK(mutex);

    return new;
}

APR_DECLARE(apr_uint64_t) apr_atomic_cas64(volatile apr_uint64_t *mem, apr_uint64_t with,
                              apr_uint64_t cmp)
{
    apr_uint64_t prev;
    DECLARE_MUTEX_LOCKED(mutex, mem);

    prev = *mem;
    if (prev == cmp) {
        *mem = with;
    }

    MUTEX_UNLOCK(mutex);

    return prev;
}

APR_DECLARE(apr_uint64_t) apr_atomic_xchg64(volatile apr_uint64_t *mem, apr_uint64_t val)
{
    apr_uint64_t prev;
    DECLARE_MUTEX_LOCKED(mutex, mem);

    prev = *mem;
    *mem = val;

    MUTEX_UNLOCK(mutex);

    return prev;
}

#endif /* USE_ATOMICS_GENERIC64 */
