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
 * @file svn_io_private.h
 * @brief Private IO API
 */

#ifndef SVN_IO_PRIVATE_H
#define SVN_IO_PRIVATE_H

#include <apr.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* The flags to pass to apr_stat to check for executable and/or readonly */
#if defined(WIN32) || defined(__OS2__)
#define SVN__APR_FINFO_EXECUTABLE (0)
#define SVN__APR_FINFO_READONLY (0)
#define SVN__APR_FINFO_MASK_OUT (APR_FINFO_PROT | APR_FINFO_OWNER)
#else
#define SVN__APR_FINFO_EXECUTABLE (APR_FINFO_PROT)
#define SVN__APR_FINFO_READONLY (APR_FINFO_PROT | APR_FINFO_OWNER)
#define SVN__APR_FINFO_MASK_OUT (0)
#endif


/** Set @a *executable TRUE if @a file_info is executable for the
 * user, FALSE otherwise.
 *
 * Always returns FALSE on Windows or platforms without user support.
 */
svn_error_t *
svn_io__is_finfo_executable(svn_boolean_t *executable,
                            apr_finfo_t *file_info,
                            apr_pool_t *pool);

/** Set @a *read_only TRUE if @a file_info is read-only for the user,
 * FALSE otherwise.
 */
svn_error_t *
svn_io__is_finfo_read_only(svn_boolean_t *read_only,
                           apr_finfo_t *file_info,
                           apr_pool_t *pool);


/** Buffer test handler function for a generic stream. @see svn_stream_t
 * and svn_stream__is_buffered().
 *
 * @since New in 1.7.
 */
typedef svn_boolean_t (*svn_stream__is_buffered_fn_t)(void *baton);

/** Set @a stream's buffer test function to @a is_buffered_fn
 *
 * @since New in 1.7.
 */
void
svn_stream__set_is_buffered(svn_stream_t *stream,
                            svn_stream__is_buffered_fn_t is_buffered_fn);

/** Return whether this generic @a stream uses internal buffering.
 * This may be used to work around subtle differences between buffered
 * an non-buffered APR files.  A lazy-open stream cannot report the
 * true buffering state until after the lazy open: a stream that
 * initially reports as non-buffered may report as buffered later.
 *
 * @since New in 1.7.
 */
svn_boolean_t
svn_stream__is_buffered(svn_stream_t *stream);

/** Return the underlying file, if any, associated with the stream, or
 * NULL if not available.  Accessing the file bypasses the stream.
 */
apr_file_t *
svn_stream__aprfile(svn_stream_t *stream);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* SVN_IO_PRIVATE_H */
