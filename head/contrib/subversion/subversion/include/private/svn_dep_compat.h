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
 * @file svn_compat.h
 * @brief Compatibility macros and functions.
 * @since New in 1.5.0.
 */

#ifndef SVN_DEP_COMPAT_H
#define SVN_DEP_COMPAT_H

#include <apr_version.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Check at compile time if the APR version is at least a certain
 * level.
 * @param major The major version component of the version checked
 * for (e.g., the "1" of "1.3.0").
 * @param minor The minor version component of the version checked
 * for (e.g., the "3" of "1.3.0").
 * @param patch The patch level component of the version checked
 * for (e.g., the "0" of "1.3.0").
 *
 * @since New in 1.5.
 */
#ifndef APR_VERSION_AT_LEAST /* Introduced in APR 1.3.0 */
#define APR_VERSION_AT_LEAST(major,minor,patch)                  \
(((major) < APR_MAJOR_VERSION)                                       \
 || ((major) == APR_MAJOR_VERSION && (minor) < APR_MINOR_VERSION)    \
 || ((major) == APR_MAJOR_VERSION && (minor) == APR_MINOR_VERSION && \
     (patch) <= APR_PATCH_VERSION))
#endif /* APR_VERSION_AT_LEAST */

/**
 * If we don't have a recent enough APR, emulate the behavior of the
 * apr_array_clear() API.
 */
#if !APR_VERSION_AT_LEAST(1,3,0)
#define apr_array_clear(arr)         (arr)->nelts = 0
#endif

#if !APR_VERSION_AT_LEAST(1,3,0)
/* Equivalent to the apr_hash_clear() function in APR >= 1.3.0.  Used to
 * implement the 'apr_hash_clear' macro if the version of APR that
 * we build against does not provide the apr_hash_clear() function. */
void svn_hash__clear(struct apr_hash_t *ht);

/**
 * If we don't have a recent enough APR, emulate the behavior of the
 * apr_hash_clear() API.
 */
#define apr_hash_clear(ht)           svn_hash__clear(ht)
#endif

#if !APR_VERSION_AT_LEAST(1,0,0)
#define APR_UINT64_C(val) UINT64_C(val)
#define APR_FPROT_OS_DEFAULT APR_OS_DEFAULT
#endif

#if !APR_VERSION_AT_LEAST(1,3,0)
#define APR_UINT16_MAX  0xFFFFU
#define APR_INT16_MAX   0x7FFF
#define APR_INT16_MIN   (-APR_INT16_MAX-1)
#define APR_UINT32_MAX 0xFFFFFFFFU
#define APR_INT32_MAX  0x7FFFFFFF
#define APR_INT32_MIN (-APR_INT32_MAX-1)
#define APR_UINT64_MAX APR_UINT64_C(0xFFFFFFFFFFFFFFFF)
#define APR_INT64_MAX   APR_INT64_C(0x7FFFFFFFFFFFFFFF)
#define APR_INT64_MIN (-APR_INT64_MAX-1)
#define APR_SIZE_MAX (~(apr_size_t)0)

#if APR_SIZEOF_VOIDP == 8
typedef apr_uint64_t apr_uintptr_t;
#else
typedef apr_uint32_t apr_uintptr_t;
#endif
#endif /* !APR_VERSION_AT_LEAST(1,3,0) */

/**
 * Work around a platform dependency issue. apr_thread_rwlock_trywrlock()
 * will make APR_STATUS_IS_EBUSY() return TRUE if the lock could not be
 * acquired under Unix. Under Windows, this will not work. So, provide
 * a more portable substitute.
 *
 * @since New in 1.8.
 */
#ifdef WIN32
#define SVN_LOCK_IS_BUSY(x) \
    (APR_STATUS_IS_EBUSY(x) || (x) == APR_FROM_OS_ERROR(WAIT_TIMEOUT))
#else
#define SVN_LOCK_IS_BUSY(x) APR_STATUS_IS_EBUSY(x)
#endif

/**
 * Check at compile time if the Serf version is at least a certain
 * level.
 * @param major The major version component of the version checked
 * for (e.g., the "1" of "1.3.0").
 * @param minor The minor version component of the version checked
 * for (e.g., the "3" of "1.3.0").
 * @param patch The patch level component of the version checked
 * for (e.g., the "0" of "1.3.0").
 *
 * @since New in 1.5.
 */
#ifndef SERF_VERSION_AT_LEAST /* Introduced in Serf 0.1.1 */
#define SERF_VERSION_AT_LEAST(major,minor,patch)                       \
(((major) < SERF_MAJOR_VERSION)                                        \
 || ((major) == SERF_MAJOR_VERSION && (minor) < SERF_MINOR_VERSION)    \
 || ((major) == SERF_MAJOR_VERSION && (minor) == SERF_MINOR_VERSION && \
     (patch) <= SERF_PATCH_VERSION))
#endif /* SERF_VERSION_AT_LEAST */

/**
 * By default, if libsvn is built against one version of SQLite
 * and then run using an older version, svn will error out:
 *
 *     svn: Couldn't perform atomic initialization
 *     svn: SQLite compiled for 3.7.4, but running with 3.7.3
 *
 * That can be annoying when building on a modern system in order
 * to deploy on a less modern one.  So these constants allow one
 * to specify how old the system being deployed on might be.
 * For example,
 *
 *     EXTRA_CFLAGS += -DSVN_SQLITE_MIN_VERSION_NUMBER=3007003
 *     EXTRA_CFLAGS += '-DSVN_SQLITE_MIN_VERSION="3.7.3"'
 *
 * turns on code that works around infelicities in older versions
 * as far back as 3.7.3 and relaxes the check at initialization time
 * to permit them.
 *
 * @since New in 1.8.
 */
#ifndef SVN_SQLITE_MIN_VERSION_NUMBER
#define SVN_SQLITE_MIN_VERSION_NUMBER SQLITE_VERSION_NUMBER
#define SVN_SQLITE_MIN_VERSION SQLITE_VERSION
#endif /* SVN_SQLITE_MIN_VERSION_NUMBER */

/**
 * Check at compile time if the SQLite version is at least a certain
 * level.
 * @param major The major version component of the version checked
 * for (e.g., the "1" of "1.3.0").
 * @param minor The minor version component of the version checked
 * for (e.g., the "3" of "1.3.0").
 * @param patch The patch level component of the version checked
 * for (e.g., the "0" of "1.3.0").
 *
 * @since New in 1.6.
 */
#ifndef SQLITE_VERSION_AT_LEAST
#define SQLITE_VERSION_AT_LEAST(major,minor,patch)                     \
((major*1000000 + minor*1000 + patch) <= SVN_SQLITE_MIN_VERSION_NUMBER)
#endif /* SQLITE_VERSION_AT_LEAST */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_DEP_COMPAT_H */
