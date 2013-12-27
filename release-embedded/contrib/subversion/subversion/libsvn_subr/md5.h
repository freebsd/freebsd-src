/*
 * md5.h: Converting and comparing MD5 checksums
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

#ifndef SVN_LIBSVN_SUBR_MD5_H
#define SVN_LIBSVN_SUBR_MD5_H

#include <apr_pools.h>

#include "svn_types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



/* The MD5 digest for the empty string. */
const unsigned char *
svn_md5__empty_string_digest(void);


/* Return the hex representation of DIGEST, which must be
 * APR_MD5_DIGESTSIZE bytes long, allocating the string in POOL.
 */
const char *
svn_md5__digest_to_cstring_display(const unsigned char digest[],
                                   apr_pool_t *pool);


/* Return the hex representation of DIGEST, which must be
 * APR_MD5_DIGESTSIZE bytes long, allocating the string in POOL.
 * If DIGEST is all zeros, then return NULL.
 */
const char *
svn_md5__digest_to_cstring(const unsigned char digest[],
                           apr_pool_t *pool);


/** Compare digests D1 and D2, each APR_MD5_DIGESTSIZE bytes long.
 * If neither is all zeros, and they do not match, then return FALSE;
 * else return TRUE.
 */
svn_boolean_t
svn_md5__digests_match(const unsigned char d1[],
                        const unsigned char d2[]);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_LIBSVN_SUBR_MD5_H */
