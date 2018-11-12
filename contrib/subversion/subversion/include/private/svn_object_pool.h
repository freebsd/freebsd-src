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
 * @file svn_object_pool.h
 * @brief multithreaded object pool API
 *
 * This is the core data structure behind various object pools.  It
 * provides a thread-safe associative container for object instances of
 * the same type.
 *
 * Memory and lifetime management for the objects are handled by the pool.
 * Reference counting takes care that neither objects nor the object pool
 * get actually destroyed while other parts depend on them.  All objects
 * are thought to be recycle-able and live in their own root memory pools
 * making them (potentially) safe to be used from different threads.
 * Currently unused objects may be kept around for a while and returned
 * by the next lookup.
 *
 * Two modes are supported: shared use and exclusive use.  In shared mode,
 * any object can be handed out to multiple users and in potentially
 * different threads at the same time.  In exclusive mode, the same object
 * will only be referenced at most once.
 *
 * Object creation and access must be provided outside this structure.
 * In particular, the using container will usually wrap the actual object
 * in a meta-data struct containing key information etc and must provide
 * getters and setters for those wrapper structs.
 */



#ifndef SVN_OBJECT_POOL_H
#define SVN_OBJECT_POOL_H

#include <apr.h>        /* for apr_int64_t */
#include <apr_pools.h>  /* for apr_pool_t */
#include <apr_hash.h>   /* for apr_hash_t */

#include "svn_types.h"

#include "private/svn_mutex.h"
#include "private/svn_string_private.h"



/* The opaque object container type. */
typedef struct svn_object_pool__t svn_object_pool__t;

/* Extract the actual object from the WRAPPER using optional information
 * from BATON (provided through #svn_object_pool__lookup) and return it.
 * The result will be used with POOL and must remain valid throughout
 * POOL's lifetime.
 *
 * It is legal to return a copy, allocated in POOL, of the wrapped object.
 */
typedef void * (* svn_object_pool__getter_t)(void *wrapper,
                                             void *baton,
                                             apr_pool_t *pool);

/* Copy the information from the SOURCE object wrapper into the already
 * existing *TARGET object wrapper using POOL for allocations and BATON
 * for optional context (provided through #svn_object_pool__insert).
 */
typedef svn_error_t * (* svn_object_pool__setter_t)(void **target,
                                                    void *source,
                                                    void *baton,
                                                    apr_pool_t *pool);

/* Create a new object pool in POOL and return it in *OBJECT_POOL.
 * Objects will be extracted using GETTER and updated using SETTER.  Either
 * one (or both) may be NULL and the default implementation assumes that
 * wrapper == object and updating is a no-op.
 *
 * If THREAD_SAFE is not set, neither the object pool nor the object
 * references returned from it may be accessed from multiple threads.
 *
 * It is not legal to call any API on the object pool after POOL got
 * cleared or destroyed.  However, existing object references handed out
 * from the object pool remain valid and will keep the internal pool data
 * structures alive for as long as such object references exist.
 */
svn_error_t *
svn_object_pool__create(svn_object_pool__t **object_pool,
                        svn_object_pool__getter_t getter,
                        svn_object_pool__setter_t setter,
                        svn_boolean_t thread_safe,
                        apr_pool_t *pool);

/* Return the root pool containing the OBJECT_POOL and all sub-structures.
 */
apr_pool_t *
svn_object_pool__new_wrapper_pool(svn_object_pool__t *object_pool);

/* Return the mutex used to serialize all OBJECT_POOL access.
 */
svn_mutex__t *
svn_object_pool__mutex(svn_object_pool__t *object_pool);

/* Return the number of object instances (used or unused) in OBJECT_POOL.
 */
unsigned
svn_object_pool__count(svn_object_pool__t *object_pool);

/* In OBJECT_POOL, look for an available object by KEY and return a
 * reference to it in *OBJECT.  If none can be found, *OBJECT will be NULL.
 * BATON will be passed to OBJECT_POOL's getter function.  The reference
 * will be returned when *RESULT_POOL gets cleaned up or destroyed.
 */
svn_error_t *
svn_object_pool__lookup(void **object,
                        svn_object_pool__t *object_pool,
                        svn_membuf_t *key,
                        void *baton,
                        apr_pool_t *result_pool);

/* Store the wrapped object WRAPPER under KEY in OBJECT_POOL and return
 * a reference to the object in *OBJECT (just like lookup).
 *
 * The object must have been created in WRAPPER_POOL and the latter must
 * be a sub-pool of OBJECT_POOL's root POOL (see #svn_object_pool__pool).
 *
 * BATON will be passed to OBJECT_POOL's setter and getter functions.
 * The reference will be returned when *RESULT_POOL gets cleaned up or
 * destroyed.
 */
svn_error_t *
svn_object_pool__insert(void **object,
                        svn_object_pool__t *object_pool,
                        const svn_membuf_t *key,
                        void *wrapper,
                        void *baton,
                        apr_pool_t *wrapper_pool,
                        apr_pool_t *result_pool);

#endif /* SVN_OBJECT_POOL_H */
