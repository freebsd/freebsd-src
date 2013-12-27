/*
 * sha1.c: SHA1 checksum routines
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

#include <apr_sha1.h>

#include "sha1.h"



/* The SHA1 digest for the empty string. */
static const unsigned char svn_sha1__empty_string_digest_array[] = {
  0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55,
  0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09
};

const unsigned char *
svn_sha1__empty_string_digest(void)
{
  return svn_sha1__empty_string_digest_array;
}


const char *
svn_sha1__digest_to_cstring_display(const unsigned char digest[],
                                   apr_pool_t *pool)
{
  static const char *hex = "0123456789abcdef";
  char *str = apr_palloc(pool, (APR_SHA1_DIGESTSIZE * 2) + 1);
  int i;

  for (i = 0; i < APR_SHA1_DIGESTSIZE; i++)
    {
      str[i*2]   = hex[digest[i] >> 4];
      str[i*2+1] = hex[digest[i] & 0x0f];
    }
  str[i*2] = '\0';

  return str;
}


const char *
svn_sha1__digest_to_cstring(const unsigned char digest[], apr_pool_t *pool)
{
  static const unsigned char zeros_digest[APR_SHA1_DIGESTSIZE] = { 0 };

  if (memcmp(digest, zeros_digest, APR_SHA1_DIGESTSIZE) != 0)
    return svn_sha1__digest_to_cstring_display(digest, pool);
  else
    return NULL;
}


svn_boolean_t
svn_sha1__digests_match(const unsigned char d1[], const unsigned char d2[])
{
  static const unsigned char zeros[APR_SHA1_DIGESTSIZE] = { 0 };

  return ((memcmp(d1, zeros, APR_SHA1_DIGESTSIZE) == 0)
          || (memcmp(d2, zeros, APR_SHA1_DIGESTSIZE) == 0)
          || (memcmp(d1, d2, APR_SHA1_DIGESTSIZE) == 0));
}
