/*
 * deprecated.c:  holding file for all deprecated APIs.
 *                "we can't lose 'em, but we can shun 'em!"
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

/* We define this here to remove any further warnings about the usage of
   deprecated functions in this file. */
#define SVN_DEPRECATED

#include "svn_fs.h"


/*** From fs-loader.c ***/
svn_error_t *
svn_fs_upgrade(const char *path, apr_pool_t *pool)
{
  return svn_error_trace(svn_fs_upgrade2(path, NULL, NULL, NULL, NULL, pool));
}

svn_error_t *
svn_fs_hotcopy2(const char *src_path, const char *dest_path,
                svn_boolean_t clean, svn_boolean_t incremental,
                svn_cancel_func_t cancel_func, void *cancel_baton,
                apr_pool_t *scratch_pool)
{
  return svn_error_trace(svn_fs_hotcopy3(src_path, dest_path, clean,
                                         incremental, NULL, NULL,
                                         cancel_func, cancel_baton,
                                         scratch_pool));
}

svn_error_t *
svn_fs_hotcopy(const char *src_path, const char *dest_path,
               svn_boolean_t clean, apr_pool_t *pool)
{
  return svn_error_trace(svn_fs_hotcopy2(src_path, dest_path, clean,
                                         FALSE, NULL, NULL, pool));
}

svn_error_t *
svn_fs_begin_txn(svn_fs_txn_t **txn_p, svn_fs_t *fs, svn_revnum_t rev,
                 apr_pool_t *pool)
{
  return svn_error_trace(svn_fs_begin_txn2(txn_p, fs, rev, 0, pool));
}

svn_error_t *
svn_fs_change_rev_prop(svn_fs_t *fs, svn_revnum_t rev, const char *name,
                       const svn_string_t *value, apr_pool_t *pool)
{
  return svn_error_trace(
           svn_fs_change_rev_prop2(fs, rev, name, NULL, value, pool));
}

svn_error_t *
svn_fs_get_locks(svn_fs_t *fs, const char *path,
                 svn_fs_get_locks_callback_t get_locks_func,
                 void *get_locks_baton, apr_pool_t *pool)
{
  return svn_error_trace(svn_fs_get_locks2(fs, path, svn_depth_infinity,
                                           get_locks_func, get_locks_baton,
                                           pool));
}

/*** From access.c ***/
svn_error_t *
svn_fs_access_add_lock_token(svn_fs_access_t *access_ctx,
                             const char *token)
{
  return svn_fs_access_add_lock_token2(access_ctx, (const char *) 1, token);
}
