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
#include "apr_pools.h"
#include "apr_random.h"
#include "apr_thread_proc.h"
#include <assert.h>
#include <stdlib.h>

APR_DECLARE(void) apr_random_init(apr_random_t *g,apr_pool_t *p,
                                  apr_crypto_hash_t *pool_hash,
                                  apr_crypto_hash_t *key_hash,
                                  apr_crypto_hash_t *prng_hash)
{
    (void)g;
    (void)p;
    (void)pool_hash;
    (void)key_hash;
    (void)prng_hash;
}

APR_DECLARE(void) apr_random_after_fork(apr_proc_t *proc)
{
    (void)proc;
}

APR_DECLARE(apr_random_t *) apr_random_standard_new(apr_pool_t *p)
{
    /* apr_random_t is an opaque struct type. */
    return (void *)0x1;
}

APR_DECLARE(void) apr_random_add_entropy(apr_random_t *g,const void *entropy_,
                                         apr_size_t bytes)
{
    (void)g;
    (void)entropy_;
    (void)bytes;
}

APR_DECLARE(apr_status_t) apr_random_secure_bytes(apr_random_t *g,
                                                  void *random,
                                                  apr_size_t bytes)
{
    (void)g;
    arc4random_buf(random, bytes);
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_random_insecure_bytes(apr_random_t *g,
                                                    void *random,
                                                    apr_size_t bytes)
{
    (void)g;
    arc4random_buf(random, bytes);
    return APR_SUCCESS;
}

APR_DECLARE(void) apr_random_barrier(apr_random_t *g)
{
    (void)g;
}

APR_DECLARE(apr_status_t) apr_random_secure_ready(apr_random_t *r)
{
    (void)r;
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_random_insecure_ready(apr_random_t *r)
{
    (void)r;
    return APR_SUCCESS;
}
