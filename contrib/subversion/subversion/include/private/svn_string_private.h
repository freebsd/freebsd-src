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
 * @file svn_string_private.h
 * @brief Non-public string utility functions.
 */


#ifndef SVN_STRING_PRIVATE_H
#define SVN_STRING_PRIVATE_H

#include "svn_string.h"    /* for svn_boolean_t, svn_error_t */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @defgroup svn_string String handling
 * @{
 */


/** Private functions.
 *
 * @defgroup svn_string_private Private functions
 * @{
 */


/** A self-contained memory buffer of known size.
 *
 * Intended to be used where a single variable-sized buffer is needed
 * within an iteration, a scratch pool is available and we want to
 * avoid the cost of creating another pool just for the iteration.
 */
typedef struct svn_membuf_t
{
  /** The a pool from which this buffer was originally allocated, and is not
   * necessarily specific to this buffer.  This is used only for allocating
   * more memory from when the buffer needs to grow.
   */
  apr_pool_t *pool;

  /** pointer to the memory */
  void *data;

  /** total size of buffer allocated */
  apr_size_t size;
} svn_membuf_t;


/* Initialize a memory buffer of the given size */
void
svn_membuf__create(svn_membuf_t *membuf, apr_size_t size, apr_pool_t *pool);

/* Ensure that the given memory buffer has at least the given size */
void
svn_membuf__ensure(svn_membuf_t *membuf, apr_size_t size);

/* Resize the given memory buffer, preserving its contents. */
void
svn_membuf__resize(svn_membuf_t *membuf, apr_size_t size);

/* Zero-fill the given memory */
void
svn_membuf__zero(svn_membuf_t *membuf);

/* Zero-fill the given memory buffer up to the smaller of SIZE and the
   current buffer size. */
void
svn_membuf__nzero(svn_membuf_t *membuf, apr_size_t size);

/* Inline implementation of svn_membuf__zero.
 * Note that PMEMBUF is evaluated only once.
 */
#define SVN_MEMBUF__ZERO(pmembuf)                \
  do                                             \
    {                                            \
      svn_membuf_t *const _m_b_f_ = (pmembuf);   \
      memset(_m_b_f_->data, 0, _m_b_f_->size);   \
    }                                            \
  while(0)

/* Inline implementation of svn_membuf__nzero
 * Note that PMEMBUF and PSIZE are evaluated only once.
 */
#define SVN_MEMBUF__NZERO(pmembuf, psize)        \
  do                                             \
    {                                            \
      svn_membuf_t *const _m_b_f_ = (pmembuf);   \
      const apr_size_t _s_z_ = (psize);          \
      if (_s_z_ > _m_b_f_->size)                 \
        memset(_m_b_f_->data, 0, _m_b_f_->size); \
      else                                       \
        memset(_m_b_f_->data, 0, _s_z_);         \
    }                                            \
  while(0)

#ifndef SVN_DEBUG
/* In non-debug mode, just use these inlie replacements */
#define svn_membuf__zero(B) SVN_MEMBUF__ZERO((B))
#define svn_membuf__nzero(B, S) SVN_MEMBUF__NZERO((B), (S))
#endif


/** Returns the #svn_string_t information contained in the data and
 * len members of @a strbuf. This is effectively a typecast, converting
 * @a strbuf into an #svn_string_t. This first will become invalid and must
 * not be accessed after this function returned.
 */
svn_string_t *
svn_stringbuf__morph_into_string(svn_stringbuf_t *strbuf);

/** Like apr_strtoff but provided here for backward compatibility
 *  with APR 0.9 */
apr_status_t
svn__strtoff(apr_off_t *offset, const char *buf, char **end, int base);

/** Number of chars needed to represent signed (19 places + sign + NUL) or
 * unsigned (20 places + NUL) integers as strings.
 */
#define SVN_INT64_BUFFER_SIZE 21

/** Writes the @a number as string into @a dest. The latter must provide
 * space for at least #SVN_INT64_BUFFER_SIZE characters. Returns the number
 * chars written excluding the terminating NUL.
 */
apr_size_t
svn__ui64toa(char * dest, apr_uint64_t number);

/** Writes the @a number as string into @a dest. The latter must provide
 * space for at least #SVN_INT64_BUFFER_SIZE characters. Returns the number
 * chars written excluding the terminating NUL.
 */
apr_size_t
svn__i64toa(char * dest, apr_int64_t number);

/** Returns a decimal string for @a number allocated in @a pool.  Put in
 * the @a seperator at each third place.
 */
char *
svn__ui64toa_sep(apr_uint64_t number, char seperator, apr_pool_t *pool);

/** Returns a decimal string for @a number allocated in @a pool.  Put in
 * the @a seperator at each third place.
 */
char *
svn__i64toa_sep(apr_int64_t number, char seperator, apr_pool_t *pool);

/**
 * Computes the similarity score of STRA and STRB. Returns the ratio
 * of the length of their longest common subsequence and the average
 * length of the strings, normalized to the range [0..1000].
 * The result is equivalent to Python's
 *
 *   difflib.SequenceMatcher.ratio
 *
 * Optionally sets *RLCS to the length of the longest common
 * subsequence of STRA and STRB. Using BUFFER for temporary storage,
 * requires memory proportional to the length of the shorter string.
 *
 * The LCS algorithm used is described in, e.g.,
 *
 *   http://en.wikipedia.org/wiki/Longest_common_subsequence_problem
 *
 * Q: Why another LCS when we already have one in libsvn_diff?
 * A: svn_diff__lcs is too heavyweight and too generic for the
 *    purposes of similarity testing. Whilst it would be possible
 *    to use a character-based tokenizer with it, we really only need
 *    the *length* of the LCS for the similarity score, not all the
 *    other information that svn_diff__lcs produces in order to
 *    make printing diffs possible.
 *
 * Q: Is there a limit on the length of the string parameters?
 * A: Only available memory. But note that the LCS algorithm used
 *    has O(strlen(STRA) * strlen(STRB)) worst-case performance,
 *    so do keep a rein on your enthusiasm.
 */
unsigned int
svn_cstring__similarity(const char *stra, const char *strb,
                        svn_membuf_t *buffer, apr_size_t *rlcs);

/**
 * Like svn_cstring__similarity, but accepts svn_string_t's instead
 * of NUL-terminated character strings.
 */
unsigned int
svn_string__similarity(const svn_string_t *stringa,
                       const svn_string_t *stringb,
                       svn_membuf_t *buffer, apr_size_t *rlcs);


/** @} */

/** @} */


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* SVN_STRING_PRIVATE_H */
