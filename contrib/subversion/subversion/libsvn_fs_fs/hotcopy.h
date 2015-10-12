/* hotcopy.h : interface to the native filesystem layer
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

#ifndef SVN_LIBSVN_FS__HOTCOPY_H
#define SVN_LIBSVN_FS__HOTCOPY_H

#include "fs.h"

/* Create an empty copy of the fsfs filesystem SRC_FS into a new DST_FS at
 * DST_PATH.  If INCREMENTAL is TRUE, perform a few pre-checks only if
 * a repo already exists at DST_PATH. Use POOL for temporary allocations. */
svn_error_t *
svn_fs_fs__hotcopy_prepare_target(svn_fs_t *src_fs,
                                  svn_fs_t *dst_fs,
                                  const char *dst_path,
                                  svn_boolean_t incremental,
                                  apr_pool_t *pool);

/* Copy the fsfs filesystem SRC_FS into DST_FS. If INCREMENTAL is TRUE, do
 * not re-copy data which already exists in DST_FS.  Indicate progress via
 * the optional NOTIFY_FUNC callback using NOTIFY_BATON.  Use POOL for
 * temporary allocations. */
svn_error_t * svn_fs_fs__hotcopy(svn_fs_t *src_fs,
                                 svn_fs_t *dst_fs,
                                 svn_boolean_t incremental,
                                 svn_fs_hotcopy_notify_t notify_func,
                                 void *notify_baton,
                                 svn_cancel_func_t cancel_func,
                                 void *cancel_baton,
                                 apr_pool_t *pool);

#endif
