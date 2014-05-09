/*
 * checksum.c:   checksum routines
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


#include <ctype.h>

#include <apr_md5.h>
#include <apr_sha1.h>

#include "svn_checksum.h"
#include "svn_error.h"
#include "svn_ctype.h"

#include "sha1.h"
#include "md5.h"

#include "private/svn_subr_private.h"

#include "svn_private_config.h"



/* Returns the digest size of it's argument. */
#define DIGESTSIZE(k) ((k) == svn_checksum_md5 ? APR_MD5_DIGESTSIZE : \
                       (k) == svn_checksum_sha1 ? APR_SHA1_DIGESTSIZE : 0)


/* Check to see if KIND is something we recognize.  If not, return
 * SVN_ERR_BAD_CHECKSUM_KIND */
static svn_error_t *
validate_kind(svn_checksum_kind_t kind)
{
  if (kind == svn_checksum_md5 || kind == svn_checksum_sha1)
    return SVN_NO_ERROR;
  else
    return svn_error_create(SVN_ERR_BAD_CHECKSUM_KIND, NULL, NULL);
}

/* Create a svn_checksum_t with everything but the contents of the
   digest populated. */
static svn_checksum_t *
checksum_create_without_digest(svn_checksum_kind_t kind,
                               apr_size_t digest_size,
                               apr_pool_t *pool)
{
  /* Use apr_palloc() instead of apr_pcalloc() so that the digest
   * contents are only set once by the caller. */
  svn_checksum_t *checksum = apr_palloc(pool, sizeof(*checksum) + digest_size);
  checksum->digest = (unsigned char *)checksum + sizeof(*checksum);
  checksum->kind = kind;
  return checksum;
}

static svn_checksum_t *
checksum_create(svn_checksum_kind_t kind,
                apr_size_t digest_size,
                const unsigned char *digest,
                apr_pool_t *pool)
{
  svn_checksum_t *checksum = checksum_create_without_digest(kind, digest_size,
                                                            pool);
  memcpy((unsigned char *)checksum->digest, digest, digest_size);
  return checksum;
}

svn_checksum_t *
svn_checksum_create(svn_checksum_kind_t kind,
                    apr_pool_t *pool)
{
  svn_checksum_t *checksum;
  apr_size_t digest_size;

  switch (kind)
    {
      case svn_checksum_md5:
        digest_size = APR_MD5_DIGESTSIZE;
        break;
      case svn_checksum_sha1:
        digest_size = APR_SHA1_DIGESTSIZE;
        break;
      default:
        return NULL;
    }

  checksum = checksum_create_without_digest(kind, digest_size, pool);
  memset((unsigned char *) checksum->digest, 0, digest_size);
  return checksum;
}

svn_checksum_t *
svn_checksum__from_digest_md5(const unsigned char *digest,
                              apr_pool_t *result_pool)
{
  return checksum_create(svn_checksum_md5, APR_MD5_DIGESTSIZE, digest,
                         result_pool);
}

svn_checksum_t *
svn_checksum__from_digest_sha1(const unsigned char *digest,
                               apr_pool_t *result_pool)
{
  return checksum_create(svn_checksum_sha1, APR_SHA1_DIGESTSIZE, digest,
                         result_pool);
}

svn_error_t *
svn_checksum_clear(svn_checksum_t *checksum)
{
  SVN_ERR(validate_kind(checksum->kind));

  memset((unsigned char *) checksum->digest, 0, DIGESTSIZE(checksum->kind));
  return SVN_NO_ERROR;
}

svn_boolean_t
svn_checksum_match(const svn_checksum_t *checksum1,
                   const svn_checksum_t *checksum2)
{
  if (checksum1 == NULL || checksum2 == NULL)
    return TRUE;

  if (checksum1->kind != checksum2->kind)
    return FALSE;

  switch (checksum1->kind)
    {
      case svn_checksum_md5:
        return svn_md5__digests_match(checksum1->digest, checksum2->digest);
      case svn_checksum_sha1:
        return svn_sha1__digests_match(checksum1->digest, checksum2->digest);
      default:
        /* We really shouldn't get here, but if we do... */
        return FALSE;
    }
}

const char *
svn_checksum_to_cstring_display(const svn_checksum_t *checksum,
                                apr_pool_t *pool)
{
  switch (checksum->kind)
    {
      case svn_checksum_md5:
        return svn_md5__digest_to_cstring_display(checksum->digest, pool);
      case svn_checksum_sha1:
        return svn_sha1__digest_to_cstring_display(checksum->digest, pool);
      default:
        /* We really shouldn't get here, but if we do... */
        return NULL;
    }
}

const char *
svn_checksum_to_cstring(const svn_checksum_t *checksum,
                        apr_pool_t *pool)
{
  if (checksum == NULL)
    return NULL;

  switch (checksum->kind)
    {
      case svn_checksum_md5:
        return svn_md5__digest_to_cstring(checksum->digest, pool);
      case svn_checksum_sha1:
        return svn_sha1__digest_to_cstring(checksum->digest, pool);
      default:
        /* We really shouldn't get here, but if we do... */
        return NULL;
    }
}


const char *
svn_checksum_serialize(const svn_checksum_t *checksum,
                       apr_pool_t *result_pool,
                       apr_pool_t *scratch_pool)
{
  const char *ckind_str;

  SVN_ERR_ASSERT_NO_RETURN(checksum->kind == svn_checksum_md5
                           || checksum->kind == svn_checksum_sha1);
  ckind_str = (checksum->kind == svn_checksum_md5 ? "$md5 $" : "$sha1$");
  return apr_pstrcat(result_pool,
                     ckind_str,
                     svn_checksum_to_cstring(checksum, scratch_pool),
                     (char *)NULL);
}


svn_error_t *
svn_checksum_deserialize(const svn_checksum_t **checksum,
                         const char *data,
                         apr_pool_t *result_pool,
                         apr_pool_t *scratch_pool)
{
  svn_checksum_kind_t ckind;
  svn_checksum_t *parsed_checksum;

  /* "$md5 $..." or "$sha1$..." */
  SVN_ERR_ASSERT(strlen(data) > 6);

  ckind = (data[1] == 'm' ? svn_checksum_md5 : svn_checksum_sha1);
  SVN_ERR(svn_checksum_parse_hex(&parsed_checksum, ckind,
                                 data + 6, result_pool));
  *checksum = parsed_checksum;

  return SVN_NO_ERROR;
}


svn_error_t *
svn_checksum_parse_hex(svn_checksum_t **checksum,
                       svn_checksum_kind_t kind,
                       const char *hex,
                       apr_pool_t *pool)
{
  int i, len;
  char is_nonzero = '\0';
  char *digest;
  static const char xdigitval[256] =
    {
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
       0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,   /* 0-9 */
      -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,   /* A-F */
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,   /* a-f */
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
      -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };

  if (hex == NULL)
    {
      *checksum = NULL;
      return SVN_NO_ERROR;
    }

  SVN_ERR(validate_kind(kind));

  *checksum = svn_checksum_create(kind, pool);
  digest = (char *)(*checksum)->digest;
  len = DIGESTSIZE(kind);

  for (i = 0; i < len; i++)
    {
      char x1 = xdigitval[(unsigned char)hex[i * 2]];
      char x2 = xdigitval[(unsigned char)hex[i * 2 + 1]];
      if (x1 == (char)-1 || x2 == (char)-1)
        return svn_error_create(SVN_ERR_BAD_CHECKSUM_PARSE, NULL, NULL);

      digest[i] = (char)((x1 << 4) | x2);
      is_nonzero |= (char)((x1 << 4) | x2);
    }

  if (!is_nonzero)
    *checksum = NULL;

  return SVN_NO_ERROR;
}

svn_checksum_t *
svn_checksum_dup(const svn_checksum_t *checksum,
                 apr_pool_t *pool)
{
  /* The duplicate of a NULL checksum is a NULL... */
  if (checksum == NULL)
    return NULL;

  /* Without this check on valid checksum kind a NULL svn_checksum_t
   * pointer is returned which could cause a core dump at an
   * indeterminate time in the future because callers are not
   * expecting a NULL pointer.  This commit forces an early abort() so
   * it's easier to track down where the issue arose. */
  switch (checksum->kind)
    {
      case svn_checksum_md5:
        return svn_checksum__from_digest_md5(checksum->digest, pool);
        break;
      case svn_checksum_sha1:
        return svn_checksum__from_digest_sha1(checksum->digest, pool);
        break;
      default:
        SVN_ERR_MALFUNCTION_NO_RETURN();
        break;
    }
}

svn_error_t *
svn_checksum(svn_checksum_t **checksum,
             svn_checksum_kind_t kind,
             const void *data,
             apr_size_t len,
             apr_pool_t *pool)
{
  apr_sha1_ctx_t sha1_ctx;

  SVN_ERR(validate_kind(kind));
  *checksum = svn_checksum_create(kind, pool);

  switch (kind)
    {
      case svn_checksum_md5:
        apr_md5((unsigned char *)(*checksum)->digest, data, len);
        break;

      case svn_checksum_sha1:
        apr_sha1_init(&sha1_ctx);
        apr_sha1_update(&sha1_ctx, data, (unsigned int)len);
        apr_sha1_final((unsigned char *)(*checksum)->digest, &sha1_ctx);
        break;

      default:
        /* We really shouldn't get here, but if we do... */
        return svn_error_create(SVN_ERR_BAD_CHECKSUM_KIND, NULL, NULL);
    }

  return SVN_NO_ERROR;
}


svn_checksum_t *
svn_checksum_empty_checksum(svn_checksum_kind_t kind,
                            apr_pool_t *pool)
{
  switch (kind)
    {
      case svn_checksum_md5:
        return svn_checksum__from_digest_md5(svn_md5__empty_string_digest(),
                                             pool);

      case svn_checksum_sha1:
        return svn_checksum__from_digest_sha1(svn_sha1__empty_string_digest(),
                                              pool);

      default:
        /* We really shouldn't get here, but if we do... */
        SVN_ERR_MALFUNCTION_NO_RETURN();
    }
}

struct svn_checksum_ctx_t
{
  void *apr_ctx;
  svn_checksum_kind_t kind;
};

svn_checksum_ctx_t *
svn_checksum_ctx_create(svn_checksum_kind_t kind,
                        apr_pool_t *pool)
{
  svn_checksum_ctx_t *ctx = apr_palloc(pool, sizeof(*ctx));

  ctx->kind = kind;
  switch (kind)
    {
      case svn_checksum_md5:
        ctx->apr_ctx = apr_palloc(pool, sizeof(apr_md5_ctx_t));
        apr_md5_init(ctx->apr_ctx);
        break;

      case svn_checksum_sha1:
        ctx->apr_ctx = apr_palloc(pool, sizeof(apr_sha1_ctx_t));
        apr_sha1_init(ctx->apr_ctx);
        break;

      default:
        SVN_ERR_MALFUNCTION_NO_RETURN();
    }

  return ctx;
}

svn_error_t *
svn_checksum_update(svn_checksum_ctx_t *ctx,
                    const void *data,
                    apr_size_t len)
{
  switch (ctx->kind)
    {
      case svn_checksum_md5:
        apr_md5_update(ctx->apr_ctx, data, len);
        break;

      case svn_checksum_sha1:
        apr_sha1_update(ctx->apr_ctx, data, (unsigned int)len);
        break;

      default:
        /* We really shouldn't get here, but if we do... */
        return svn_error_create(SVN_ERR_BAD_CHECKSUM_KIND, NULL, NULL);
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_checksum_final(svn_checksum_t **checksum,
                   const svn_checksum_ctx_t *ctx,
                   apr_pool_t *pool)
{
  *checksum = svn_checksum_create(ctx->kind, pool);

  switch (ctx->kind)
    {
      case svn_checksum_md5:
        apr_md5_final((unsigned char *)(*checksum)->digest, ctx->apr_ctx);
        break;

      case svn_checksum_sha1:
        apr_sha1_final((unsigned char *)(*checksum)->digest, ctx->apr_ctx);
        break;

      default:
        /* We really shouldn't get here, but if we do... */
        return svn_error_create(SVN_ERR_BAD_CHECKSUM_KIND, NULL, NULL);
    }

  return SVN_NO_ERROR;
}

apr_size_t
svn_checksum_size(const svn_checksum_t *checksum)
{
  return DIGESTSIZE(checksum->kind);
}

svn_error_t *
svn_checksum_mismatch_err(const svn_checksum_t *expected,
                          const svn_checksum_t *actual,
                          apr_pool_t *scratch_pool,
                          const char *fmt,
                          ...)
{
  va_list ap;
  const char *desc;

  va_start(ap, fmt);
  desc = apr_pvsprintf(scratch_pool, fmt, ap);
  va_end(ap);

  return svn_error_createf(SVN_ERR_CHECKSUM_MISMATCH, NULL,
                           _("%s:\n"
                             "   expected:  %s\n"
                             "     actual:  %s\n"),
                desc,
                svn_checksum_to_cstring_display(expected, scratch_pool),
                svn_checksum_to_cstring_display(actual, scratch_pool));
}

svn_boolean_t
svn_checksum_is_empty_checksum(svn_checksum_t *checksum)
{
  /* By definition, the NULL checksum matches all others, including the
     empty one. */
  if (!checksum)
    return TRUE;

  switch (checksum->kind)
    {
      case svn_checksum_md5:
        return svn_md5__digests_match(checksum->digest,
                                      svn_md5__empty_string_digest());

      case svn_checksum_sha1:
        return svn_sha1__digests_match(checksum->digest,
                                       svn_sha1__empty_string_digest());

      default:
        /* We really shouldn't get here, but if we do... */
        SVN_ERR_MALFUNCTION_NO_RETURN();
    }
}
