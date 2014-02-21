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
 * @file svn_pseudo_md5.h
 * @brief Subversion hash sum calculation for runtime data (only)
 */

#ifndef SVN_PSEUDO_MD5_H
#define SVN_PSEUDO_MD5_H

#include <apr.h>        /* for apr_uint32_t */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/**
 * Calculates a hash sum for 15 bytes in @a x and returns it in @a digest.
 * The most significant byte in @a x must be 0 (independent of being on a
 * little or big endian machine).
 *
 * @note Use for runtime data hashing only.
 *
 * @note The output is NOT an MD5 digest shares has the same basic
 *       cryptographic properties.  Collisions with proper MD5 on the same
 *       or other input data is equally unlikely as any MD5 collision.
 */
void svn__pseudo_md5_15(apr_uint32_t digest[4],
                        const apr_uint32_t x[4]);

/**
 * Calculates a hash sum for 31 bytes in @a x and returns it in @a digest.
 * The most significant byte in @a x must be 0 (independent of being on a
 * little or big endian machine).
 *
 * @note Use for runtime data hashing only.
 *
 * @note The output is NOT an MD5 digest shares has the same basic
 *       cryptographic properties.  Collisions with proper MD5 on the same
 *       or other input data is equally unlikely as any MD5 collision.
 */
void svn__pseudo_md5_31(apr_uint32_t digest[4],
                        const apr_uint32_t x[8]);

/**
 * Calculates a hash sum for 63 bytes in @a x and returns it in @a digest.
 * The most significant byte in @a x must be 0 (independent of being on a
 * little or big endian machine).
 *
 * @note Use for runtime data hashing only.
 *
 * @note The output is NOT an MD5 digest shares has the same basic
 *       cryptographic properties.  Collisions with proper MD5 on the same
 *       or other input data is equally unlikely as any MD5 collision.
 */
void svn__pseudo_md5_63(apr_uint32_t digest[4],
                        const apr_uint32_t x[16]);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_PSEUDO_MD5_H */
