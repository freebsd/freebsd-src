/* load-index-cmd.c -- implements the dump-index sub-command.
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

#include "svn_pools.h"

#include "private/svn_fs_fs_private.h"
#include "private/svn_sorts_private.h"

#include "index.h"
#include "util.h"
#include "transaction.h"

/* A svn_sort__array compatible comparator function, sorting the
 * svn_fs_fs__p2l_entry_t** given in LHS, RHS by offset. */
static int
compare_p2l_entry_revision(const void *lhs,
                           const void *rhs)
{
  const svn_fs_fs__p2l_entry_t *lhs_entry
    =*(const svn_fs_fs__p2l_entry_t **)lhs;
  const svn_fs_fs__p2l_entry_t *rhs_entry
    =*(const svn_fs_fs__p2l_entry_t **)rhs;

  if (lhs_entry->offset < rhs_entry->offset)
    return -1;

  return lhs_entry->offset == rhs_entry->offset ? 0 : 1;
}

svn_error_t *
svn_fs_fs__load_index(svn_fs_t *fs,
                      svn_revnum_t revision,
                      apr_array_header_t *entries,
                      apr_pool_t *scratch_pool)
{
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);

  /* Check the FS format number. */
  if (! svn_fs_fs__use_log_addressing(fs))
    return svn_error_create(SVN_ERR_FS_UNSUPPORTED_FORMAT, NULL, NULL);

  /* P2L index must be written in offset order.
   * Sort ENTRIES accordingly. */
  svn_sort__array(entries, compare_p2l_entry_revision);

  /* Treat an empty array as a no-op instead error. */
  if (entries->nelts != 0)
    {
      const char *l2p_proto_index;
      const char *p2l_proto_index;
      svn_fs_fs__revision_file_t *rev_file;

      /* Open rev / pack file & trim indexes + footer off it. */
      SVN_ERR(svn_fs_fs__open_pack_or_rev_file_writable(&rev_file, fs,
                                                        revision, iterpool,
                                                        iterpool));
      SVN_ERR(svn_fs_fs__auto_read_footer(rev_file));
      SVN_ERR(svn_io_file_trunc(rev_file->file, rev_file->l2p_offset,
                                iterpool));

      /* Create proto index files for the new index data
       * (will be cleaned up automatically with iterpool). */
      SVN_ERR(svn_fs_fs__p2l_index_from_p2l_entries(&p2l_proto_index, fs,
                                                    rev_file, entries,
                                                    iterpool, iterpool));
      SVN_ERR(svn_fs_fs__l2p_index_from_p2l_entries(&l2p_proto_index, fs,
                                                    entries, iterpool,
                                                    iterpool));

      /* Combine rev data with new index data. */
      SVN_ERR(svn_fs_fs__add_index_data(fs, rev_file->file, l2p_proto_index,
                                        p2l_proto_index,
                                        rev_file->start_revision, iterpool));
    }

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}
