/*
 * svn_dirent_uri_private.h : private definitions for dirents and URIs
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

#ifndef SVN_DIRENT_URI_PRIVATE_H
#define SVN_DIRENT_URI_PRIVATE_H

#include <apr_pools.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Convert @a relpath from the local style to the canonical internal style.
 * "Local style" means native path separators and "." for the empty path.
 *
 * Allocates the results in @a result_pool. Uses @a scratch_pool for
 * temporary allocations.
 *
 * @since New in 1.7 (as svn_relpath__internal_style()).
 * @since Name and signature changed in 1.12.
 */
svn_error_t *
svn_relpath__make_internal(const char **internal_style_relpath,
                           const char *relpath,
                           apr_pool_t *result_pool,
                           apr_pool_t *scratch_pool);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_DIRENT_URI_PRIVATE_H */
