/* pack.h : interface FSX pack functionality
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

#ifndef SVN_LIBSVN_FS__PACK_H
#define SVN_LIBSVN_FS__PACK_H

#include "fs.h"

/* Possibly pack the repository at PATH.  This just take full shards, and
   combines all the revision files into a single one, with a manifest header.
   Use optional CANCEL_FUNC/CANCEL_BATON for cancellation support.
   Use SCRATCH_POOL for temporary allocations.

   Existing filesystem references need not change.  */
svn_error_t *
svn_fs_x__pack(svn_fs_t *fs,
               svn_fs_pack_notify_t notify_func,
               void *notify_baton,
               svn_cancel_func_t cancel_func,
               void *cancel_baton,
               apr_pool_t *scratch_pool);

/**
 * For the packed revision REV in FS,  determine the offset within the
 * revision pack file and return it in REV_OFFSET.
 * Use SCRATCH_POOL for temporary allocations.
 */
svn_error_t *
svn_fs_x__get_packed_offset(apr_off_t *rev_offset,
                            svn_fs_t *fs,
                            svn_revnum_t rev,
                            apr_pool_t *scratch_pool);

/* Return the svn_dir_entry_t* objects of DIRECTORY in an APR array
 * allocated in RESULT_POOL with entries added in storage (on-disk) order.
 * FS' format will be used to pick the optimal ordering strategy.  Use
 * SCRATCH_POOL for temporary allocations.
 */
apr_array_header_t *
svn_fs_x__order_dir_entries(svn_fs_t *fs,
                            apr_hash_t *directory,
                            apr_pool_t *result_pool,
                            apr_pool_t *scratch_pool);


#endif
