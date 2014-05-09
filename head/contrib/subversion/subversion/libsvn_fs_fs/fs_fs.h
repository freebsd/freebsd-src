/* fs_fs.h : interface to the native filesystem layer
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

#ifndef SVN_LIBSVN_FS__FS_FS_H
#define SVN_LIBSVN_FS__FS_FS_H

#include "fs.h"

/* Open the fsfs filesystem pointed to by PATH and associate it with
   filesystem object FS.  Use POOL for temporary allocations.

   ### Some parts of *FS must have been initialized beforehand; some parts
       (including FS->path) are initialized by this function. */
svn_error_t *svn_fs_fs__open(svn_fs_t *fs,
                             const char *path,
                             apr_pool_t *pool);

/* Upgrade the fsfs filesystem FS.  Use POOL for temporary allocations. */
svn_error_t *svn_fs_fs__upgrade(svn_fs_t *fs,
                                apr_pool_t *pool);

/* Verify metadata in fsfs filesystem FS.  Limit the checks to revisions
 * START to END where possible.  Indicate progress via the optional
 * NOTIFY_FUNC callback using NOTIFY_BATON.  The optional CANCEL_FUNC
 * will periodically be called with CANCEL_BATON to allow for preemption.
 * Use POOL for temporary allocations. */
svn_error_t *svn_fs_fs__verify(svn_fs_t *fs,
                               svn_revnum_t start,
                               svn_revnum_t end,
                               svn_fs_progress_notify_func_t notify_func,
                               void *notify_baton,
                               svn_cancel_func_t cancel_func,
                               void *cancel_baton,
                               apr_pool_t *pool);

/* Copy the fsfs filesystem SRC_FS at SRC_PATH into a new copy DST_FS at
 * DST_PATH. If INCREMENTAL is TRUE, do not re-copy data which already
 * exists in DST_FS. Use POOL for temporary allocations. */
svn_error_t * svn_fs_fs__hotcopy(svn_fs_t *src_fs,
                                 svn_fs_t *dst_fs,
                                 const char *src_path,
                                 const char *dst_path,
                                 svn_boolean_t incremental,
                                 svn_cancel_func_t cancel_func,
                                 void *cancel_baton,
                                 apr_pool_t *pool);

/* Recover the fsfs associated with filesystem FS.
   Use optional CANCEL_FUNC/CANCEL_BATON for cancellation support.
   Use POOL for temporary allocations. */
svn_error_t *svn_fs_fs__recover(svn_fs_t *fs,
                                svn_cancel_func_t cancel_func,
                                void *cancel_baton,
                                apr_pool_t *pool);

/* Set *NODEREV_P to the node-revision for the node ID in FS.  Do any
   allocations in POOL. */
svn_error_t *svn_fs_fs__get_node_revision(node_revision_t **noderev_p,
                                          svn_fs_t *fs,
                                          const svn_fs_id_t *id,
                                          apr_pool_t *pool);

/* Store NODEREV as the node-revision for the node whose id is ID in
   FS, after setting its is_fresh_txn_root to FRESH_TXN_ROOT.  Do any
   necessary temporary allocation in POOL. */
svn_error_t *svn_fs_fs__put_node_revision(svn_fs_t *fs,
                                          const svn_fs_id_t *id,
                                          node_revision_t *noderev,
                                          svn_boolean_t fresh_txn_root,
                                          apr_pool_t *pool);

/* Write the node-revision NODEREV into the stream OUTFILE, compatible with
   filesystem format FORMAT.  Only write mergeinfo-related metadata if
   INCLUDE_MERGEINFO is true.  Temporary allocations are from POOL. */
/* ### Currently used only by fs_fs.c */
svn_error_t *
svn_fs_fs__write_noderev(svn_stream_t *outfile,
                         node_revision_t *noderev,
                         int format,
                         svn_boolean_t include_mergeinfo,
                         apr_pool_t *pool);

/* Read a node-revision from STREAM. Set *NODEREV to the new structure,
   allocated in POOL. */
/* ### Currently used only by fs_fs.c */
svn_error_t *
svn_fs_fs__read_noderev(node_revision_t **noderev,
                        svn_stream_t *stream,
                        apr_pool_t *pool);


/* Set *YOUNGEST to the youngest revision in filesystem FS.  Do any
   temporary allocation in POOL. */
svn_error_t *svn_fs_fs__youngest_rev(svn_revnum_t *youngest,
                                     svn_fs_t *fs,
                                     apr_pool_t *pool);

/* Return an error iff REV does not exist in FS. */
svn_error_t *
svn_fs_fs__revision_exists(svn_revnum_t rev,
                           svn_fs_t *fs,
                           apr_pool_t *pool);

/* Set *ROOT_ID to the node-id for the root of revision REV in
   filesystem FS.  Do any allocations in POOL. */
svn_error_t *svn_fs_fs__rev_get_root(svn_fs_id_t **root_id,
                                     svn_fs_t *fs,
                                     svn_revnum_t rev,
                                     apr_pool_t *pool);

/* Set *ENTRIES to an apr_hash_t of dirent structs that contain the
   directory entries of node-revision NODEREV in filesystem FS.  The
   returned table (and its keys and values) is allocated in POOL,
   which is also used for temporary allocations. */
svn_error_t *svn_fs_fs__rep_contents_dir(apr_hash_t **entries,
                                         svn_fs_t *fs,
                                         node_revision_t *noderev,
                                         apr_pool_t *pool);

/* Set *DIRENT to the entry identified by NAME in the directory given
   by NODEREV in filesystem FS.  If no such entry exits, *DIRENT will
   be NULL. The returned object is allocated in RESULT_POOL; SCRATCH_POOL
   used for temporary allocations. */
svn_error_t *
svn_fs_fs__rep_contents_dir_entry(svn_fs_dirent_t **dirent,
                                  svn_fs_t *fs,
                                  node_revision_t *noderev,
                                  const char *name,
                                  apr_pool_t *result_pool,
                                  apr_pool_t *scratch_pool);

/* Set *CONTENTS to be a readable svn_stream_t that receives the text
   representation of node-revision NODEREV as seen in filesystem FS.
   Use POOL for temporary allocations. */
svn_error_t *svn_fs_fs__get_contents(svn_stream_t **contents,
                                     svn_fs_t *fs,
                                     node_revision_t *noderev,
                                     apr_pool_t *pool);

/* Attempt to fetch the text representation of node-revision NODEREV as
   seen in filesystem FS and pass it along with the BATON to the PROCESSOR.
   Set *SUCCESS only of the data could be provided and the processing
   had been called.
   Use POOL for all allocations.
 */
svn_error_t *
svn_fs_fs__try_process_file_contents(svn_boolean_t *success,
                                     svn_fs_t *fs,
                                     node_revision_t *noderev,
                                     svn_fs_process_contents_func_t processor,
                                     void* baton,
                                     apr_pool_t *pool);

/* Set *STREAM_P to a delta stream turning the contents of the file SOURCE into
   the contents of the file TARGET, allocated in POOL.
   If SOURCE is null, the empty string will be used. */
svn_error_t *svn_fs_fs__get_file_delta_stream(svn_txdelta_stream_t **stream_p,
                                              svn_fs_t *fs,
                                              node_revision_t *source,
                                              node_revision_t *target,
                                              apr_pool_t *pool);

/* Set *PROPLIST to be an apr_hash_t containing the property list of
   node-revision NODEREV as seen in filesystem FS.  Use POOL for
   temporary allocations. */
svn_error_t *svn_fs_fs__get_proplist(apr_hash_t **proplist,
                                     svn_fs_t *fs,
                                     node_revision_t *noderev,
                                     apr_pool_t *pool);

/* Set *PROPLIST to be an apr_hash_t containing the property list of
   revision REV as seen in filesystem FS.  Use POOL for temporary
   allocations. */
svn_error_t *svn_fs_fs__revision_proplist(apr_hash_t **proplist,
                                          svn_fs_t *fs,
                                          svn_revnum_t rev,
                                          apr_pool_t *pool);

/* Set *LENGTH to the be fulltext length of the node revision
   specified by NODEREV.  Use POOL for temporary allocations. */
svn_error_t *svn_fs_fs__file_length(svn_filesize_t *length,
                                    node_revision_t *noderev,
                                    apr_pool_t *pool);

/* Return TRUE if the representation keys in A and B both point to the
   same representation, else return FALSE. */
svn_boolean_t svn_fs_fs__noderev_same_rep_key(representation_t *a,
                                              representation_t *b);


/* Return a copy of the representation REP allocated from POOL. */
representation_t *svn_fs_fs__rep_copy(representation_t *rep,
                                      apr_pool_t *pool);


/* Return the recorded checksum of type KIND for the text representation
   of NODREV into CHECKSUM, allocating from POOL.  If no stored checksum is
   available, put all NULL into CHECKSUM. */
svn_error_t *svn_fs_fs__file_checksum(svn_checksum_t **checksum,
                                      node_revision_t *noderev,
                                      svn_checksum_kind_t kind,
                                      apr_pool_t *pool);

/* Find the paths which were changed in revision REV of filesystem FS
   and store them in *CHANGED_PATHS_P.  Cached copyfrom information
   will be stored in *COPYFROM_CACHE.  Get any temporary allocations
   from POOL. */
svn_error_t *svn_fs_fs__paths_changed(apr_hash_t **changed_paths_p,
                                      svn_fs_t *fs,
                                      svn_revnum_t rev,
                                      apr_hash_t *copyfrom_cache,
                                      apr_pool_t *pool);

/* Create a new transaction in filesystem FS, based on revision REV,
   and store it in *TXN_P.  Allocate all necessary variables from
   POOL. */
svn_error_t *svn_fs_fs__create_txn(svn_fs_txn_t **txn_p,
                                   svn_fs_t *fs,
                                   svn_revnum_t rev,
                                   apr_pool_t *pool);

/* Set the transaction property NAME to the value VALUE in transaction
   TXN.  Perform temporary allocations from POOL. */
svn_error_t *svn_fs_fs__change_txn_prop(svn_fs_txn_t *txn,
                                        const char *name,
                                        const svn_string_t *value,
                                        apr_pool_t *pool);

/* Change transaction properties in transaction TXN based on PROPS.
   Perform temporary allocations from POOL. */
svn_error_t *svn_fs_fs__change_txn_props(svn_fs_txn_t *txn,
                                         const apr_array_header_t *props,
                                         apr_pool_t *pool);

/* Return whether or not the given FS supports mergeinfo metadata. */
svn_boolean_t svn_fs_fs__fs_supports_mergeinfo(svn_fs_t *fs);

/* Store a transaction record in *TXN_P for the transaction identified
   by TXN_ID in filesystem FS.  Allocate everything from POOL. */
svn_error_t *svn_fs_fs__get_txn(transaction_t **txn_p,
                                svn_fs_t *fs,
                                const char *txn_id,
                                apr_pool_t *pool);

/* Abort the existing transaction TXN, performing any temporary
   allocations in POOL. */
svn_error_t *svn_fs_fs__abort_txn(svn_fs_txn_t *txn, apr_pool_t *pool);

/* Create an entirely new mutable node in the filesystem FS, whose
   node-revision is NODEREV.  Set *ID_P to the new node revision's ID.
   Use POOL for any temporary allocation.  COPY_ID is the copy_id to
   use in the node revision ID.  TXN_ID is the Subversion transaction
   under which this occurs. */
svn_error_t *svn_fs_fs__create_node(const svn_fs_id_t **id_p,
                                    svn_fs_t *fs,
                                    node_revision_t *noderev,
                                    const char *copy_id,
                                    const char *txn_id,
                                    apr_pool_t *pool);

/* Remove all references to the transaction TXN_ID from filesystem FS.
   Temporary allocations are from POOL. */
svn_error_t *svn_fs_fs__purge_txn(svn_fs_t *fs,
                                  const char *txn_id,
                                  apr_pool_t *pool);

/* Add or set in filesystem FS, transaction TXN_ID, in directory
   PARENT_NODEREV a directory entry for NAME pointing to ID of type
   KIND.  Allocations are done in POOL. */
svn_error_t *svn_fs_fs__set_entry(svn_fs_t *fs,
                                  const char *txn_id,
                                  node_revision_t *parent_noderev,
                                  const char *name,
                                  const svn_fs_id_t *id,
                                  svn_node_kind_t kind,
                                  apr_pool_t *pool);

/* Add a change to the changes record for filesystem FS in transaction
   TXN_ID.  Mark path PATH, having node-id ID, as changed according to
   the type in CHANGE_KIND.  If the text representation was changed
   set TEXT_MOD to TRUE, and likewise for PROP_MOD.  If this change
   was the result of a copy, set COPYFROM_REV and COPYFROM_PATH to the
   revision and path of the copy source, otherwise they should be set
   to SVN_INVALID_REVNUM and NULL.  Perform any temporary allocations
   from POOL. */
svn_error_t *svn_fs_fs__add_change(svn_fs_t *fs,
                                   const char *txn_id,
                                   const char *path,
                                   const svn_fs_id_t *id,
                                   svn_fs_path_change_kind_t change_kind,
                                   svn_boolean_t text_mod,
                                   svn_boolean_t prop_mod,
                                   svn_node_kind_t node_kind,
                                   svn_revnum_t copyfrom_rev,
                                   const char *copyfrom_path,
                                   apr_pool_t *pool);

/* Return a writable stream in *STREAM that allows storing the text
   representation of node-revision NODEREV in filesystem FS.
   Allocations are from POOL. */
svn_error_t *svn_fs_fs__set_contents(svn_stream_t **stream,
                                     svn_fs_t *fs,
                                     node_revision_t *noderev,
                                     apr_pool_t *pool);

/* Create a node revision in FS which is an immediate successor of
   OLD_ID, whose contents are NEW_NR.  Set *NEW_ID_P to the new node
   revision's ID.  Use POOL for any temporary allocation.

   COPY_ID, if non-NULL, is a key into the `copies' table, and
   indicates that this new node is being created as the result of a
   copy operation, and specifically which operation that was.  If
   COPY_ID is NULL, then re-use the copy ID from the predecessor node.

   TXN_ID is the Subversion transaction under which this occurs.

   After this call, the deltification code assumes that the new node's
   contents will change frequently, and will avoid representing other
   nodes as deltas against this node's contents.  */
svn_error_t *svn_fs_fs__create_successor(const svn_fs_id_t **new_id_p,
                                         svn_fs_t *fs,
                                         const svn_fs_id_t *old_idp,
                                         node_revision_t *new_noderev,
                                         const char *copy_id,
                                         const char *txn_id,
                                         apr_pool_t *pool);

/* Write a new property list PROPLIST for node-revision NODEREV in
   filesystem FS.  Perform any temporary allocations in POOL. */
svn_error_t *svn_fs_fs__set_proplist(svn_fs_t *fs,
                                     node_revision_t *noderev,
                                     apr_hash_t *proplist,
                                     apr_pool_t *pool);

/* Commit the transaction TXN in filesystem FS and return its new
   revision number in *REV.  If the transaction is out of date, return
   the error SVN_ERR_FS_TXN_OUT_OF_DATE.  Use POOL for temporary
   allocations. */
svn_error_t *svn_fs_fs__commit(svn_revnum_t *new_rev_p,
                               svn_fs_t *fs,
                               svn_fs_txn_t *txn,
                               apr_pool_t *pool);

/* Return the next available copy_id in *COPY_ID for the transaction
   TXN_ID in filesystem FS.  Allocate space in POOL. */
svn_error_t *svn_fs_fs__reserve_copy_id(const char **copy_id,
                                        svn_fs_t *fs,
                                        const char *txn_id,
                                        apr_pool_t *pool);

/* Create a fs_fs fileysystem referenced by FS at path PATH.  Get any
   temporary allocations from POOL.

   ### Some parts of *FS must have been initialized beforehand; some parts
       (including FS->path) are initialized by this function. */
svn_error_t *svn_fs_fs__create(svn_fs_t *fs,
                               const char *path,
                               apr_pool_t *pool);

/* Set the uuid of repository FS to UUID, if UUID is not NULL;
   otherwise, set the uuid of FS to a newly generated UUID.  Perform
   temporary allocations in POOL. */
svn_error_t *svn_fs_fs__set_uuid(svn_fs_t *fs,
                                 const char *uuid,
                                 apr_pool_t *pool);

/* Set *NAMES_P to an array of names which are all the active
   transactions in filesystem FS.  Allocate the array from POOL. */
svn_error_t *svn_fs_fs__list_transactions(apr_array_header_t **names_p,
                                          svn_fs_t *fs,
                                          apr_pool_t *pool);

/* Open the transaction named NAME in filesystem FS.  Set *TXN_P to
 * the transaction. If there is no such transaction, return
` * SVN_ERR_FS_NO_SUCH_TRANSACTION.  Allocate the new transaction in
 * POOL. */
svn_error_t *svn_fs_fs__open_txn(svn_fs_txn_t **txn_p,
                                 svn_fs_t *fs,
                                 const char *name,
                                 apr_pool_t *pool);

/* Return the property list from transaction TXN and store it in
   *PROPLIST.  Allocate the property list from POOL. */
svn_error_t *svn_fs_fs__txn_proplist(apr_hash_t **proplist,
                                     svn_fs_txn_t *txn,
                                     apr_pool_t *pool);

/* Delete the mutable node-revision referenced by ID, along with any
   mutable props or directory contents associated with it.  Perform
   temporary allocations in POOL. */
svn_error_t *svn_fs_fs__delete_node_revision(svn_fs_t *fs,
                                             const svn_fs_id_t *id,
                                             apr_pool_t *pool);


/* Find the paths which were changed in transaction TXN_ID of
   filesystem FS and store them in *CHANGED_PATHS_P.
   Get any temporary allocations from POOL. */
svn_error_t *svn_fs_fs__txn_changes_fetch(apr_hash_t **changes,
                                          svn_fs_t *fs,
                                          const char *txn_id,
                                          apr_pool_t *pool);


/* Set *PATH to the path of REV in FS, whether in a pack file or not.
   Allocate *PATH in POOL.

   Note: If the caller does not have the write lock on FS, then the path is
   not guaranteed to be correct or to remain correct after the function
   returns, because the revision might become packed before or after this
   call.  If a file exists at that path, then it is correct; if not, then
   the caller should call update_min_unpacked_rev() and re-try once. */
svn_error_t *
svn_fs_fs__path_rev_absolute(const char **path,
                             svn_fs_t *fs,
                             svn_revnum_t rev,
                             apr_pool_t *pool);

/* Return the path to the 'current' file in FS.
   Perform allocation in POOL. */
const char *
svn_fs_fs__path_current(svn_fs_t *fs, apr_pool_t *pool);

/* Obtain a write lock on the filesystem FS in a subpool of POOL, call
   BODY with BATON and that subpool, destroy the subpool (releasing the write
   lock) and return what BODY returned. */
svn_error_t *
svn_fs_fs__with_write_lock(svn_fs_t *fs,
                           svn_error_t *(*body)(void *baton,
                                                apr_pool_t *pool),
                           void *baton,
                           apr_pool_t *pool);

/* Find the value of the property named PROPNAME in transaction TXN.
   Return the contents in *VALUE_P.  The contents will be allocated
   from POOL. */
svn_error_t *svn_fs_fs__revision_prop(svn_string_t **value_p, svn_fs_t *fs,
                                      svn_revnum_t rev,
                                      const char *propname,
                                      apr_pool_t *pool);

/* Change, add, or delete a property on a revision REV in filesystem
   FS.  NAME gives the name of the property, and value, if non-NULL,
   gives the new contents of the property.  If value is NULL, then the
   property will be deleted.  If OLD_VALUE_P is not NULL, do nothing unless the
   preexisting value is *OLD_VALUE_P.  Do any temporary allocation in POOL.  */
svn_error_t *svn_fs_fs__change_rev_prop(svn_fs_t *fs, svn_revnum_t rev,
                                        const char *name,
                                        const svn_string_t *const *old_value_p,
                                        const svn_string_t *value,
                                        apr_pool_t *pool);

/* Retrieve information about the Subversion transaction SVN_TXN from
   the `transactions' table of FS, allocating from POOL.  Set
   *ROOT_ID_P to the ID of the transaction's root directory.  Set
   *BASE_ROOT_ID_P to the ID of the root directory of the
   transaction's base revision.

   If there is no such transaction, SVN_ERR_FS_NO_SUCH_TRANSACTION is
   the error returned.

   Returns SVN_ERR_FS_TRANSACTION_NOT_MUTABLE if TXN_NAME refers to a
   transaction that has already been committed.

   Allocate *ROOT_ID_P and *BASE_ROOT_ID_P in POOL.  */
svn_error_t *svn_fs_fs__get_txn_ids(const svn_fs_id_t **root_id_p,
                                    const svn_fs_id_t **base_root_id_p,
                                    svn_fs_t *fs,
                                    const char *txn_name,
                                    apr_pool_t *pool);

/* Begin a new transaction in filesystem FS, based on existing
   revision REV.  The new transaction is returned in *TXN_P.  Allocate
   the new transaction structure from POOL. */
svn_error_t *svn_fs_fs__begin_txn(svn_fs_txn_t **txn_p, svn_fs_t *fs,
                                  svn_revnum_t rev, apr_uint32_t flags,
                                  apr_pool_t *pool);

/* Find the value of the property named PROPNAME in transaction TXN.
   Return the contents in *VALUE_P.  The contents will be allocated
   from POOL. */
svn_error_t *svn_fs_fs__txn_prop(svn_string_t **value_p, svn_fs_txn_t *txn,
                                 const char *propname, apr_pool_t *pool);

/* If directory PATH does not exist, create it and give it the same
   permissions as FS_PATH.*/
svn_error_t *svn_fs_fs__ensure_dir_exists(const char *path,
                                          const char *fs_path,
                                          apr_pool_t *pool);

/* Update the node origin index for FS, recording the mapping from
   NODE_ID to NODE_REV_ID.  Use POOL for any temporary allocations.

   Because this is just an "optional" cache, this function does not
   return an error if the underlying storage is readonly; it still
   returns an error for other error conditions.
 */
svn_error_t *
svn_fs_fs__set_node_origin(svn_fs_t *fs,
                           const char *node_id,
                           const svn_fs_id_t *node_rev_id,
                           apr_pool_t *pool);

/* Set *ORIGIN_ID to the node revision ID from which the history of
   all nodes in FS whose "Node ID" is NODE_ID springs, as determined
   by a look in the index.  ORIGIN_ID needs to be parsed in an
   FS-backend-specific way.  Use POOL for allocations.

   If there is no entry for NODE_ID in the cache, return NULL
   in *ORIGIN_ID. */
svn_error_t *
svn_fs_fs__get_node_origin(const svn_fs_id_t **origin_id,
                           svn_fs_t *fs,
                           const char *node_id,
                           apr_pool_t *pool);


/* Initialize all session-local caches in FS according to the global
   cache settings. Use POOL for allocations.

   Please note that it is permissible for this function to set some
   or all of these caches to NULL, regardless of any setting. */
svn_error_t *
svn_fs_fs__initialize_caches(svn_fs_t *fs, apr_pool_t *pool);

/* Initialize all transaction-local caches in FS according to the global
   cache settings and make TXN_ID part of their key space. Use POOL for
   allocations.

   Please note that it is permissible for this function to set some or all
   of these caches to NULL, regardless of any setting. */
svn_error_t *
svn_fs_fs__initialize_txn_caches(svn_fs_t *fs,
                                 const char *txn_id,
                                 apr_pool_t *pool);

/* Resets the svn_cache__t structures local to the current transaction in FS.
   Calling it more than once per txn or from outside any txn is allowed. */
void
svn_fs_fs__reset_txn_caches(svn_fs_t *fs);

/* Possibly pack the repository at PATH.  This just take full shards, and
   combines all the revision files into a single one, with a manifest header.
   Use optional CANCEL_FUNC/CANCEL_BATON for cancellation support.

   Existing filesystem references need not change.  */
svn_error_t *
svn_fs_fs__pack(svn_fs_t *fs,
                svn_fs_pack_notify_t notify_func,
                void *notify_baton,
                svn_cancel_func_t cancel_func,
                void *cancel_baton,
                apr_pool_t *pool);


#endif
