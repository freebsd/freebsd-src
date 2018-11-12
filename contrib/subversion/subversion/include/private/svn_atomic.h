/**
 * @copyright
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
 * @endcopyright
 *
 * @file svn_atomic.h
 * @brief Macros and functions for atomic operations
 */

#ifndef SVN_ATOMIC_H
#define SVN_ATOMIC_H

#include <apr_version.h>
#include <apr_atomic.h>

#include "svn_error.h"
#include "private/svn_dep_compat.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @name Macro definitions for atomic types and operations
 *
 * @note These are necessary because the apr_atomic API changed somewhat
 *       between apr-0.x and apr-1.x.
 * @{
 */

/** The type used by all the other atomic operations. */
#define svn_atomic_t apr_uint32_t

/** Atomically read an #svn_atomic_t from memory. */
#define svn_atomic_read(mem) apr_atomic_read32((mem))

/** Atomically set an #svn_atomic_t in memory. */
#define svn_atomic_set(mem, val) apr_atomic_set32((mem), (val))

/** Atomically increment an #svn_atomic_t. */
#define svn_atomic_inc(mem) apr_atomic_inc32(mem)

/** Atomically decrement an #svn_atomic_t. */
#define svn_atomic_dec(mem) apr_atomic_dec32(mem)

/**
 * Atomic compare-and-swap.
 *
 * Compare the value that @a mem points to with @a cmp. If they are
 * the same swap the value with @a with.
 *
 * @note svn_atomic_cas should not be combined with the other
 *       svn_atomic operations.  A comment in apr_atomic.h explains
 *       that on some platforms, the CAS function is implemented in a
 *       way that is incompatible with the other atomic operations.
 */
#define svn_atomic_cas(mem, with, cmp) \
    apr_atomic_cas32((mem), (with), (cmp))
/** @} */

/**
 * Call an initialization function in a thread-safe manner.
 *
 * @a global_status must be a pointer to a global, zero-initialized
 * #svn_atomic_t. @a init_func is a pointer to the function that performs
 * the actual initialization. @a baton and and @a pool are passed on to the
 * init_func for its use.
 *
 * @since New in 1.5.
 */
svn_error_t *
svn_atomic__init_once(volatile svn_atomic_t *global_status,
                      svn_error_t *(*init_func)(void*,apr_pool_t*),
                      void *baton,
                      apr_pool_t* pool);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_ATOMIC_H */
