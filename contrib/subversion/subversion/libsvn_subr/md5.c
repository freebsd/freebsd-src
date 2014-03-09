/*
 * md5.c:   checksum routines
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


#include <apr_md5.h>
#include "md5.h"
#include "svn_md5.h"



/* The MD5 digest for the empty string. */
static const unsigned char svn_md5__empty_string_digest_array[] = {
  0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04,
  0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e
};

const unsigned char *
svn_md5__empty_string_digest(void)
{
  return svn_md5__empty_string_digest_array;
}


const char *
svn_md5__digest_to_cstring_display(const unsigned char digest[],
                                   apr_pool_t *pool)
{
  static const char *hex = "0123456789abcdef";
  char *str = apr_palloc(pool, (APR_MD5_DIGESTSIZE * 2) + 1);
  int i;

  for (i = 0; i < APR_MD5_DIGESTSIZE; i++)
    {
      str[i*2]   = hex[digest[i] >> 4];
      str[i*2+1] = hex[digest[i] & 0x0f];
    }
  str[i*2] = '\0';

  return str;
}


const char *
svn_md5__digest_to_cstring(const unsigned char digest[], apr_pool_t *pool)
{
  static const unsigned char zeros_digest[APR_MD5_DIGESTSIZE] = { 0 };

  if (memcmp(digest, zeros_digest, APR_MD5_DIGESTSIZE) != 0)
    return svn_md5__digest_to_cstring_display(digest, pool);
  else
    return NULL;
}


svn_boolean_t
svn_md5__digests_match(const unsigned char d1[], const unsigned char d2[])
{
  static const unsigned char zeros[APR_MD5_DIGESTSIZE] = { 0 };

  return ((memcmp(d1, zeros, APR_MD5_DIGESTSIZE) == 0)
          || (memcmp(d2, zeros, APR_MD5_DIGESTSIZE) == 0)
          || (memcmp(d1, d2, APR_MD5_DIGESTSIZE) == 0));
}

/* These are all deprecated, and just wrap the internal functions defined
   above. */
const unsigned char *
svn_md5_empty_string_digest(void)
{
  return svn_md5__empty_string_digest();
}

const char *
svn_md5_digest_to_cstring_display(const unsigned char digest[],
                                  apr_pool_t *pool)
{
  return svn_md5__digest_to_cstring_display(digest, pool);
}

const char *
svn_md5_digest_to_cstring(const unsigned char digest[], apr_pool_t *pool)
{
  return svn_md5__digest_to_cstring(digest, pool);
}

svn_boolean_t
svn_md5_digests_match(const unsigned char d1[], const unsigned char d2[])
{
  return svn_md5__digests_match(d1, d2);
}
