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

#ifdef USE_ATOMICS_BUILTINS

APR_DECLARE(apr_uint64_t) apr_atomic_read64(volatile apr_uint64_t *mem)
{
    return *mem;
}

APR_DECLARE(void) apr_atomic_set64(volatile apr_uint64_t *mem, apr_uint64_t val)
{
    *mem = val;
}

APR_DECLARE(apr_uint64_t) apr_atomic_add64(volatile apr_uint64_t *mem, apr_uint64_t val)
{
    return __sync_fetch_and_add(mem, val);
}

APR_DECLARE(void) apr_atomic_sub64(volatile apr_uint64_t *mem, apr_uint64_t val)
{
    __sync_fetch_and_sub(mem, val);
}

APR_DECLARE(apr_uint64_t) apr_atomic_inc64(volatile apr_uint64_t *mem)
{
    return __sync_fetch_and_add(mem, 1);
}

APR_DECLARE(int) apr_atomic_dec64(volatile apr_uint64_t *mem)
{
    return __sync_sub_and_fetch(mem, 1);
}

APR_DECLARE(apr_uint64_t) apr_atomic_cas64(volatile apr_uint64_t *mem, apr_uint64_t with,
                                           apr_uint64_t cmp)
{
    return __sync_val_compare_and_swap(mem, cmp, with);
}

APR_DECLARE(apr_uint64_t) apr_atomic_xchg64(volatile apr_uint64_t *mem, apr_uint64_t val)
{
    __sync_synchronize();

    return __sync_lock_test_and_set(mem, val);
}

#endif /* USE_ATOMICS_BUILTINS */
