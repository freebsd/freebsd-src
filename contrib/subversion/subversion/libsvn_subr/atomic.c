/* atomic.c : perform atomic initialization
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

#include <apr_time.h>
#include "private/svn_atomic.h"

/* Magic values for atomic initialization */
#define SVN_ATOMIC_UNINITIALIZED 0
#define SVN_ATOMIC_START_INIT    1
#define SVN_ATOMIC_INIT_FAILED   2
#define SVN_ATOMIC_INITIALIZED   3

svn_error_t*
svn_atomic__init_once(volatile svn_atomic_t *global_status,
                      svn_error_t *(*init_func)(void*,apr_pool_t*),
                      void *baton,
                      apr_pool_t* pool)
{
  /* !! Don't use localizable strings in this function, because these
     !! might cause deadlocks. This function can be used to initialize
     !! libraries that are used for generating error messages. */

  /* We have to call init_func exactly once.  Because APR
     doesn't have statically-initialized mutexes, we implement a poor
     man's spinlock using svn_atomic_cas. */
  svn_atomic_t status = svn_atomic_cas(global_status,
                                       SVN_ATOMIC_START_INIT,
                                       SVN_ATOMIC_UNINITIALIZED);

  if (status == SVN_ATOMIC_UNINITIALIZED)
    {
      svn_error_t *err = init_func(baton, pool);
      if (err)
        {
#if APR_HAS_THREADS
          /* Tell other threads that the initialization failed. */
          svn_atomic_cas(global_status,
                         SVN_ATOMIC_INIT_FAILED,
                         SVN_ATOMIC_START_INIT);
#endif
          return svn_error_create(SVN_ERR_ATOMIC_INIT_FAILURE, err,
                                  "Couldn't perform atomic initialization");
        }
      svn_atomic_cas(global_status,
                     SVN_ATOMIC_INITIALIZED,
                     SVN_ATOMIC_START_INIT);
    }
#if APR_HAS_THREADS
  /* Wait for whichever thread is performing initialization to finish. */
  /* XXX FIXME: Should we have a maximum wait here, like we have in
                the Windows file IO spinner? */
  else while (status != SVN_ATOMIC_INITIALIZED)
    {
      if (status == SVN_ATOMIC_INIT_FAILED)
        return svn_error_create(SVN_ERR_ATOMIC_INIT_FAILURE, NULL,
                                "Couldn't perform atomic initialization");

      apr_sleep(APR_USEC_PER_SEC / 1000);
      status = svn_atomic_cas(global_status,
                              SVN_ATOMIC_UNINITIALIZED,
                              SVN_ATOMIC_UNINITIALIZED);
    }
#endif /* APR_HAS_THREADS */

  return SVN_NO_ERROR;
}
