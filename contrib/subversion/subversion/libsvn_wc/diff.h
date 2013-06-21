/*
 * lock.h:  routines for diffing local files and directories.
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

#ifndef SVN_LIBSVN_WC_DIFF_H
#define SVN_LIBSVN_WC_DIFF_H

#include <apr_pools.h>
#include <apr_hash.h>

#include "svn_types.h"
#include "svn_error.h"
#include "svn_wc.h"

#include "wc_db.h"
#include "private/svn_diff_tree.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Reports the file LOCAL_ABSPATH as ADDED file with relpath RELPATH to
   PROCESSOR with as parent baton PROCESSOR_PARENT_BATON.

   The node is expected to have status svn_wc__db_status_normal, or
   svn_wc__db_status_added. When DIFF_PRISTINE is TRUE, report the pristine
   version of LOCAL_ABSPATH as ADDED. In this case an
   svn_wc__db_status_deleted may shadow an added or deleted node.

   If CHANGELIST_HASH is not NULL and LOCAL_ABSPATH's changelist is not
   in the changelist, don't report the node.
 */
svn_error_t *
svn_wc__diff_local_only_file(svn_wc__db_t *db,
                             const char *local_abspath,
                             const char *relpath,
                             const svn_diff_tree_processor_t *processor,
                             void *processor_parent_baton,
                             apr_hash_t *changelist_hash,
                             svn_boolean_t diff_pristine,
                             svn_cancel_func_t cancel_func,
                             void *cancel_baton,
                             apr_pool_t *scratch_pool);

/* Reports the directory LOCAL_ABSPATH and everything below it (limited by
   DEPTH) as added with relpath RELPATH to PROCESSOR with as parent baton
   PROCESSOR_PARENT_BATON.

   The node is expected to have status svn_wc__db_status_normal, or
   svn_wc__db_status_added. When DIFF_PRISTINE is TRUE, report the pristine
   version of LOCAL_ABSPATH as ADDED. In this case an
   svn_wc__db_status_deleted may shadow an added or deleted node.

   If CHANGELIST_HASH is not NULL and LOCAL_ABSPATH's changelist is not
   in the changelist, don't report the node.
 */
svn_error_t *
svn_wc__diff_local_only_dir(svn_wc__db_t *db,
                            const char *local_abspath,
                            const char *relpath,
                            svn_depth_t depth,
                            const svn_diff_tree_processor_t *processor,
                            void *processor_parent_baton,
                            apr_hash_t *changelist_hash,
                            svn_boolean_t diff_pristine,
                            svn_cancel_func_t cancel_func,
                            void *cancel_baton,
                            apr_pool_t *scratch_pool);

/* Reports the BASE-file LOCAL_ABSPATH as deleted to PROCESSOR with relpath
   RELPATH, revision REVISION and parent baton PROCESSOR_PARENT_BATON.

   If REVISION is invalid, the revision as stored in BASE is used.

   The node is expected to have status svn_wc__db_status_normal in BASE. */
svn_error_t *
svn_wc__diff_base_only_file(svn_wc__db_t *db,
                            const char *local_abspath,
                            const char *relpath,
                            svn_revnum_t revision,
                            const svn_diff_tree_processor_t *processor,
                            void *processor_parent_baton,
                            apr_pool_t *scratch_pool);

/* Reports the BASE-directory LOCAL_ABSPATH and everything below it (limited
   by DEPTH) as deleted to PROCESSOR with relpath RELPATH and parent baton
   PROCESSOR_PARENT_BATON.

   If REVISION is invalid, the revision as stored in BASE is used.

   The node is expected to have status svn_wc__db_status_normal in BASE. */
svn_error_t *
svn_wc__diff_base_only_dir(svn_wc__db_t *db,
                           const char *local_abspath,
                           const char *relpath,
                           svn_revnum_t revision,
                           svn_depth_t depth,
                           const svn_diff_tree_processor_t *processor,
                           void *processor_parent_baton,
                           svn_cancel_func_t cancel_func,
                           void *cancel_baton,
                           apr_pool_t *scratch_pool);

/* Diff the file PATH against the text base of its BASE layer.  At this
 * stage we are dealing with a file that does exist in the working copy.
 */
svn_error_t *
svn_wc__diff_base_working_diff(svn_wc__db_t *db,
                               const char *local_abspath,
                               const char *relpath,
                               svn_revnum_t revision,
                               apr_hash_t *changelist_hash,
                               const svn_diff_tree_processor_t *processor,
                               void *processor_dir_baton,
                               svn_boolean_t diff_pristine,
                               svn_cancel_func_t cancel_func,
                               void *cancel_baton,
                               apr_pool_t *scratch_pool);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_LIBSVN_WC_DIFF_H */
