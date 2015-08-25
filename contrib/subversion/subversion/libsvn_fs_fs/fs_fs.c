/* fs_fs.c --- filesystem operations specific to fs_fs
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

#include <apr_general.h>
#include <apr_pools.h>
#include <apr_file_io.h>
#include <apr_uuid.h>
#include <apr_lib.h>
#include <apr_md5.h>
#include <apr_sha1.h>
#include <apr_strings.h>
#include <apr_thread_mutex.h>

#include "svn_pools.h"
#include "svn_fs.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_hash.h"
#include "svn_props.h"
#include "svn_sorts.h"
#include "svn_string.h"
#include "svn_time.h"
#include "svn_mergeinfo.h"
#include "svn_config.h"
#include "svn_ctype.h"
#include "svn_version.h"

#include "fs.h"
#include "tree.h"
#include "lock.h"
#include "key-gen.h"
#include "fs_fs.h"
#include "id.h"
#include "rep-cache.h"
#include "temp_serializer.h"

#include "private/svn_string_private.h"
#include "private/svn_fs_util.h"
#include "private/svn_subr_private.h"
#include "private/svn_delta_private.h"
#include "../libsvn_fs/fs-loader.h"

#include "svn_private_config.h"
#include "temp_serializer.h"

/* An arbitrary maximum path length, so clients can't run us out of memory
 * by giving us arbitrarily large paths. */
#define FSFS_MAX_PATH_LEN 4096

/* The default maximum number of files per directory to store in the
   rev and revprops directory.  The number below is somewhat arbitrary,
   and can be overridden by defining the macro while compiling; the
   figure of 1000 is reasonable for VFAT filesystems, which are by far
   the worst performers in this area. */
#ifndef SVN_FS_FS_DEFAULT_MAX_FILES_PER_DIR
#define SVN_FS_FS_DEFAULT_MAX_FILES_PER_DIR 1000
#endif

/* Begin deltification after a node history exceeded this this limit.
   Useful values are 4 to 64 with 16 being a good compromise between
   computational overhead and repository size savings.
   Should be a power of 2.
   Values < 2 will result in standard skip-delta behavior. */
#define SVN_FS_FS_MAX_LINEAR_DELTIFICATION 16

/* Finding a deltification base takes operations proportional to the
   number of changes being skipped. To prevent exploding runtime
   during commits, limit the deltification range to this value.
   Should be a power of 2 minus one.
   Values < 1 disable deltification. */
#define SVN_FS_FS_MAX_DELTIFICATION_WALK 1023

/* Give writing processes 10 seconds to replace an existing revprop
   file with a new one. After that time, we assume that the writing
   process got aborted and that we have re-read revprops. */
#define REVPROP_CHANGE_TIMEOUT (10 * 1000000)

/* The following are names of atomics that will be used to communicate
 * revprop updates across all processes on this machine. */
#define ATOMIC_REVPROP_GENERATION "rev-prop-generation"
#define ATOMIC_REVPROP_TIMEOUT    "rev-prop-timeout"
#define ATOMIC_REVPROP_NAMESPACE  "rev-prop-atomics"

/* Following are defines that specify the textual elements of the
   native filesystem directories and revision files. */

/* Headers used to describe node-revision in the revision file. */
#define HEADER_ID          "id"
#define HEADER_TYPE        "type"
#define HEADER_COUNT       "count"
#define HEADER_PROPS       "props"
#define HEADER_TEXT        "text"
#define HEADER_CPATH       "cpath"
#define HEADER_PRED        "pred"
#define HEADER_COPYFROM    "copyfrom"
#define HEADER_COPYROOT    "copyroot"
#define HEADER_FRESHTXNRT  "is-fresh-txn-root"
#define HEADER_MINFO_HERE  "minfo-here"
#define HEADER_MINFO_CNT   "minfo-cnt"

/* Kinds that a change can be. */
#define ACTION_MODIFY      "modify"
#define ACTION_ADD         "add"
#define ACTION_DELETE      "delete"
#define ACTION_REPLACE     "replace"
#define ACTION_RESET       "reset"

/* True and False flags. */
#define FLAG_TRUE          "true"
#define FLAG_FALSE         "false"

/* Kinds that a node-rev can be. */
#define KIND_FILE          "file"
#define KIND_DIR           "dir"

/* Kinds of representation. */
#define REP_PLAIN          "PLAIN"
#define REP_DELTA          "DELTA"

/* Notes:

To avoid opening and closing the rev-files all the time, it would
probably be advantageous to keep each rev-file open for the
lifetime of the transaction object.  I'll leave that as a later
optimization for now.

I didn't keep track of pool lifetimes at all in this code.  There
are likely some errors because of that.

*/

/* The vtable associated with an open transaction object. */
static txn_vtable_t txn_vtable = {
  svn_fs_fs__commit_txn,
  svn_fs_fs__abort_txn,
  svn_fs_fs__txn_prop,
  svn_fs_fs__txn_proplist,
  svn_fs_fs__change_txn_prop,
  svn_fs_fs__txn_root,
  svn_fs_fs__change_txn_props
};

/* Declarations. */

static svn_error_t *
read_min_unpacked_rev(svn_revnum_t *min_unpacked_rev,
                      const char *path,
                      apr_pool_t *pool);

static svn_error_t *
update_min_unpacked_rev(svn_fs_t *fs, apr_pool_t *pool);

static svn_error_t *
get_youngest(svn_revnum_t *youngest_p, const char *fs_path, apr_pool_t *pool);

static svn_error_t *
verify_walker(representation_t *rep,
              void *baton,
              svn_fs_t *fs,
              apr_pool_t *scratch_pool);

/* Pathname helper functions */

/* Return TRUE is REV is packed in FS, FALSE otherwise. */
static svn_boolean_t
is_packed_rev(svn_fs_t *fs, svn_revnum_t rev)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  return (rev < ffd->min_unpacked_rev);
}

/* Return TRUE is REV is packed in FS, FALSE otherwise. */
static svn_boolean_t
is_packed_revprop(svn_fs_t *fs, svn_revnum_t rev)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  /* rev 0 will not be packed */
  return (rev < ffd->min_unpacked_rev)
      && (rev != 0)
      && (ffd->format >= SVN_FS_FS__MIN_PACKED_REVPROP_FORMAT);
}

static const char *
path_format(svn_fs_t *fs, apr_pool_t *pool)
{
  return svn_dirent_join(fs->path, PATH_FORMAT, pool);
}

static APR_INLINE const char *
path_uuid(svn_fs_t *fs, apr_pool_t *pool)
{
  return svn_dirent_join(fs->path, PATH_UUID, pool);
}

const char *
svn_fs_fs__path_current(svn_fs_t *fs, apr_pool_t *pool)
{
  return svn_dirent_join(fs->path, PATH_CURRENT, pool);
}

static APR_INLINE const char *
path_txn_current(svn_fs_t *fs, apr_pool_t *pool)
{
  return svn_dirent_join(fs->path, PATH_TXN_CURRENT, pool);
}

static APR_INLINE const char *
path_txn_current_lock(svn_fs_t *fs, apr_pool_t *pool)
{
  return svn_dirent_join(fs->path, PATH_TXN_CURRENT_LOCK, pool);
}

static APR_INLINE const char *
path_lock(svn_fs_t *fs, apr_pool_t *pool)
{
  return svn_dirent_join(fs->path, PATH_LOCK_FILE, pool);
}

static const char *
path_revprop_generation(svn_fs_t *fs, apr_pool_t *pool)
{
  return svn_dirent_join(fs->path, PATH_REVPROP_GENERATION, pool);
}

static const char *
path_rev_packed(svn_fs_t *fs, svn_revnum_t rev, const char *kind,
                apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  assert(ffd->max_files_per_dir);
  assert(is_packed_rev(fs, rev));

  return svn_dirent_join_many(pool, fs->path, PATH_REVS_DIR,
                              apr_psprintf(pool,
                                           "%ld" PATH_EXT_PACKED_SHARD,
                                           rev / ffd->max_files_per_dir),
                              kind, NULL);
}

static const char *
path_rev_shard(svn_fs_t *fs, svn_revnum_t rev, apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  assert(ffd->max_files_per_dir);
  return svn_dirent_join_many(pool, fs->path, PATH_REVS_DIR,
                              apr_psprintf(pool, "%ld",
                                                 rev / ffd->max_files_per_dir),
                              NULL);
}

static const char *
path_rev(svn_fs_t *fs, svn_revnum_t rev, apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  assert(! is_packed_rev(fs, rev));

  if (ffd->max_files_per_dir)
    {
      return svn_dirent_join(path_rev_shard(fs, rev, pool),
                             apr_psprintf(pool, "%ld", rev),
                             pool);
    }

  return svn_dirent_join_many(pool, fs->path, PATH_REVS_DIR,
                              apr_psprintf(pool, "%ld", rev), NULL);
}

svn_error_t *
svn_fs_fs__path_rev_absolute(const char **path,
                             svn_fs_t *fs,
                             svn_revnum_t rev,
                             apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  if (ffd->format < SVN_FS_FS__MIN_PACKED_FORMAT
      || ! is_packed_rev(fs, rev))
    {
      *path = path_rev(fs, rev, pool);
    }
  else
    {
      *path = path_rev_packed(fs, rev, PATH_PACKED, pool);
    }

  return SVN_NO_ERROR;
}

static const char *
path_revprops_shard(svn_fs_t *fs, svn_revnum_t rev, apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  assert(ffd->max_files_per_dir);
  return svn_dirent_join_many(pool, fs->path, PATH_REVPROPS_DIR,
                              apr_psprintf(pool, "%ld",
                                           rev / ffd->max_files_per_dir),
                              NULL);
}

static const char *
path_revprops_pack_shard(svn_fs_t *fs, svn_revnum_t rev, apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  assert(ffd->max_files_per_dir);
  return svn_dirent_join_many(pool, fs->path, PATH_REVPROPS_DIR,
                              apr_psprintf(pool, "%ld" PATH_EXT_PACKED_SHARD,
                                           rev / ffd->max_files_per_dir),
                              NULL);
}

static const char *
path_revprops(svn_fs_t *fs, svn_revnum_t rev, apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  if (ffd->max_files_per_dir)
    {
      return svn_dirent_join(path_revprops_shard(fs, rev, pool),
                             apr_psprintf(pool, "%ld", rev),
                             pool);
    }

  return svn_dirent_join_many(pool, fs->path, PATH_REVPROPS_DIR,
                              apr_psprintf(pool, "%ld", rev), NULL);
}

static APR_INLINE const char *
path_txn_dir(svn_fs_t *fs, const char *txn_id, apr_pool_t *pool)
{
  SVN_ERR_ASSERT_NO_RETURN(txn_id != NULL);
  return svn_dirent_join_many(pool, fs->path, PATH_TXNS_DIR,
                              apr_pstrcat(pool, txn_id, PATH_EXT_TXN,
                                          (char *)NULL),
                              NULL);
}

/* Return the name of the sha1->rep mapping file in transaction TXN_ID
 * within FS for the given SHA1 checksum.  Use POOL for allocations.
 */
static APR_INLINE const char *
path_txn_sha1(svn_fs_t *fs, const char *txn_id, svn_checksum_t *sha1,
              apr_pool_t *pool)
{
  return svn_dirent_join(path_txn_dir(fs, txn_id, pool),
                         svn_checksum_to_cstring(sha1, pool),
                         pool);
}

static APR_INLINE const char *
path_txn_changes(svn_fs_t *fs, const char *txn_id, apr_pool_t *pool)
{
  return svn_dirent_join(path_txn_dir(fs, txn_id, pool), PATH_CHANGES, pool);
}

static APR_INLINE const char *
path_txn_props(svn_fs_t *fs, const char *txn_id, apr_pool_t *pool)
{
  return svn_dirent_join(path_txn_dir(fs, txn_id, pool), PATH_TXN_PROPS, pool);
}

static APR_INLINE const char *
path_txn_next_ids(svn_fs_t *fs, const char *txn_id, apr_pool_t *pool)
{
  return svn_dirent_join(path_txn_dir(fs, txn_id, pool), PATH_NEXT_IDS, pool);
}

static APR_INLINE const char *
path_min_unpacked_rev(svn_fs_t *fs, apr_pool_t *pool)
{
  return svn_dirent_join(fs->path, PATH_MIN_UNPACKED_REV, pool);
}


static APR_INLINE const char *
path_txn_proto_rev(svn_fs_t *fs, const char *txn_id, apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  if (ffd->format >= SVN_FS_FS__MIN_PROTOREVS_DIR_FORMAT)
    return svn_dirent_join_many(pool, fs->path, PATH_TXN_PROTOS_DIR,
                                apr_pstrcat(pool, txn_id, PATH_EXT_REV,
                                            (char *)NULL),
                                NULL);
  else
    return svn_dirent_join(path_txn_dir(fs, txn_id, pool), PATH_REV, pool);
}

static APR_INLINE const char *
path_txn_proto_rev_lock(svn_fs_t *fs, const char *txn_id, apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  if (ffd->format >= SVN_FS_FS__MIN_PROTOREVS_DIR_FORMAT)
    return svn_dirent_join_many(pool, fs->path, PATH_TXN_PROTOS_DIR,
                                apr_pstrcat(pool, txn_id, PATH_EXT_REV_LOCK,
                                            (char *)NULL),
                                NULL);
  else
    return svn_dirent_join(path_txn_dir(fs, txn_id, pool), PATH_REV_LOCK,
                           pool);
}

static const char *
path_txn_node_rev(svn_fs_t *fs, const svn_fs_id_t *id, apr_pool_t *pool)
{
  const char *txn_id = svn_fs_fs__id_txn_id(id);
  const char *node_id = svn_fs_fs__id_node_id(id);
  const char *copy_id = svn_fs_fs__id_copy_id(id);
  const char *name = apr_psprintf(pool, PATH_PREFIX_NODE "%s.%s",
                                  node_id, copy_id);

  return svn_dirent_join(path_txn_dir(fs, txn_id, pool), name, pool);
}

static APR_INLINE const char *
path_txn_node_props(svn_fs_t *fs, const svn_fs_id_t *id, apr_pool_t *pool)
{
  return apr_pstrcat(pool, path_txn_node_rev(fs, id, pool), PATH_EXT_PROPS,
                     (char *)NULL);
}

static APR_INLINE const char *
path_txn_node_children(svn_fs_t *fs, const svn_fs_id_t *id, apr_pool_t *pool)
{
  return apr_pstrcat(pool, path_txn_node_rev(fs, id, pool),
                     PATH_EXT_CHILDREN, (char *)NULL);
}

static APR_INLINE const char *
path_node_origin(svn_fs_t *fs, const char *node_id, apr_pool_t *pool)
{
  size_t len = strlen(node_id);
  const char *node_id_minus_last_char =
    (len == 1) ? "0" : apr_pstrmemdup(pool, node_id, len - 1);
  return svn_dirent_join_many(pool, fs->path, PATH_NODE_ORIGINS_DIR,
                              node_id_minus_last_char, NULL);
}

static APR_INLINE const char *
path_and_offset_of(apr_file_t *file, apr_pool_t *pool)
{
  const char *path;
  apr_off_t offset = 0;

  if (apr_file_name_get(&path, file) != APR_SUCCESS)
    path = "(unknown)";

  if (apr_file_seek(file, APR_CUR, &offset) != APR_SUCCESS)
    offset = -1;

  return apr_psprintf(pool, "%s:%" APR_OFF_T_FMT, path, offset);
}



/* Functions for working with shared transaction data. */

/* Return the transaction object for transaction TXN_ID from the
   transaction list of filesystem FS (which must already be locked via the
   txn_list_lock mutex).  If the transaction does not exist in the list,
   then create a new transaction object and return it (if CREATE_NEW is
   true) or return NULL (otherwise). */
static fs_fs_shared_txn_data_t *
get_shared_txn(svn_fs_t *fs, const char *txn_id, svn_boolean_t create_new)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  fs_fs_shared_data_t *ffsd = ffd->shared;
  fs_fs_shared_txn_data_t *txn;

  for (txn = ffsd->txns; txn; txn = txn->next)
    if (strcmp(txn->txn_id, txn_id) == 0)
      break;

  if (txn || !create_new)
    return txn;

  /* Use the transaction object from the (single-object) freelist,
     if one is available, or otherwise create a new object. */
  if (ffsd->free_txn)
    {
      txn = ffsd->free_txn;
      ffsd->free_txn = NULL;
    }
  else
    {
      apr_pool_t *subpool = svn_pool_create(ffsd->common_pool);
      txn = apr_palloc(subpool, sizeof(*txn));
      txn->pool = subpool;
    }

  assert(strlen(txn_id) < sizeof(txn->txn_id));
  apr_cpystrn(txn->txn_id, txn_id, sizeof(txn->txn_id));
  txn->being_written = FALSE;

  /* Link this transaction into the head of the list.  We will typically
     be dealing with only one active transaction at a time, so it makes
     sense for searches through the transaction list to look at the
     newest transactions first.  */
  txn->next = ffsd->txns;
  ffsd->txns = txn;

  return txn;
}

/* Free the transaction object for transaction TXN_ID, and remove it
   from the transaction list of filesystem FS (which must already be
   locked via the txn_list_lock mutex).  Do nothing if the transaction
   does not exist. */
static void
free_shared_txn(svn_fs_t *fs, const char *txn_id)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  fs_fs_shared_data_t *ffsd = ffd->shared;
  fs_fs_shared_txn_data_t *txn, *prev = NULL;

  for (txn = ffsd->txns; txn; prev = txn, txn = txn->next)
    if (strcmp(txn->txn_id, txn_id) == 0)
      break;

  if (!txn)
    return;

  if (prev)
    prev->next = txn->next;
  else
    ffsd->txns = txn->next;

  /* As we typically will be dealing with one transaction after another,
     we will maintain a single-object free list so that we can hopefully
     keep reusing the same transaction object. */
  if (!ffsd->free_txn)
    ffsd->free_txn = txn;
  else
    svn_pool_destroy(txn->pool);
}


/* Obtain a lock on the transaction list of filesystem FS, call BODY
   with FS, BATON, and POOL, and then unlock the transaction list.
   Return what BODY returned. */
static svn_error_t *
with_txnlist_lock(svn_fs_t *fs,
                  svn_error_t *(*body)(svn_fs_t *fs,
                                       const void *baton,
                                       apr_pool_t *pool),
                  const void *baton,
                  apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  fs_fs_shared_data_t *ffsd = ffd->shared;

  SVN_MUTEX__WITH_LOCK(ffsd->txn_list_lock,
                       body(fs, baton, pool));

  return SVN_NO_ERROR;
}


/* Get a lock on empty file LOCK_FILENAME, creating it in POOL. */
static svn_error_t *
get_lock_on_filesystem(const char *lock_filename,
                       apr_pool_t *pool)
{
  svn_error_t *err = svn_io_file_lock2(lock_filename, TRUE, FALSE, pool);

  if (err && APR_STATUS_IS_ENOENT(err->apr_err))
    {
      /* No lock file?  No big deal; these are just empty files
         anyway.  Create it and try again. */
      svn_error_clear(err);
      err = NULL;

      SVN_ERR(svn_io_file_create(lock_filename, "", pool));
      SVN_ERR(svn_io_file_lock2(lock_filename, TRUE, FALSE, pool));
    }

  return svn_error_trace(err);
}

/* Reset the HAS_WRITE_LOCK member in the FFD given as BATON_VOID.
   When registered with the pool holding the lock on the lock file,
   this makes sure the flag gets reset just before we release the lock. */
static apr_status_t
reset_lock_flag(void *baton_void)
{
  fs_fs_data_t *ffd = baton_void;
  ffd->has_write_lock = FALSE;
  return APR_SUCCESS;
}

/* Obtain a write lock on the file LOCK_FILENAME (protecting with
   LOCK_MUTEX if APR is threaded) in a subpool of POOL, call BODY with
   BATON and that subpool, destroy the subpool (releasing the write
   lock) and return what BODY returned.  If IS_GLOBAL_LOCK is set,
   set the HAS_WRITE_LOCK flag while we keep the write lock. */
static svn_error_t *
with_some_lock_file(svn_fs_t *fs,
                    svn_error_t *(*body)(void *baton,
                                         apr_pool_t *pool),
                    void *baton,
                    const char *lock_filename,
                    svn_boolean_t is_global_lock,
                    apr_pool_t *pool)
{
  apr_pool_t *subpool = svn_pool_create(pool);
  svn_error_t *err = get_lock_on_filesystem(lock_filename, subpool);

  if (!err)
    {
      fs_fs_data_t *ffd = fs->fsap_data;

      if (is_global_lock)
        {
          /* set the "got the lock" flag and register reset function */
          apr_pool_cleanup_register(subpool,
                                    ffd,
                                    reset_lock_flag,
                                    apr_pool_cleanup_null);
          ffd->has_write_lock = TRUE;
        }

      /* nobody else will modify the repo state
         => read HEAD & pack info once */
      if (ffd->format >= SVN_FS_FS__MIN_PACKED_FORMAT)
        SVN_ERR(update_min_unpacked_rev(fs, pool));
      SVN_ERR(get_youngest(&ffd->youngest_rev_cache, fs->path,
                           pool));
      err = body(baton, subpool);
    }

  svn_pool_destroy(subpool);

  return svn_error_trace(err);
}

svn_error_t *
svn_fs_fs__with_write_lock(svn_fs_t *fs,
                           svn_error_t *(*body)(void *baton,
                                                apr_pool_t *pool),
                           void *baton,
                           apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  fs_fs_shared_data_t *ffsd = ffd->shared;

  SVN_MUTEX__WITH_LOCK(ffsd->fs_write_lock,
                       with_some_lock_file(fs, body, baton,
                                           path_lock(fs, pool),
                                           TRUE,
                                           pool));

  return SVN_NO_ERROR;
}

/* Run BODY (with BATON and POOL) while the txn-current file
   of FS is locked. */
static svn_error_t *
with_txn_current_lock(svn_fs_t *fs,
                      svn_error_t *(*body)(void *baton,
                                           apr_pool_t *pool),
                      void *baton,
                      apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  fs_fs_shared_data_t *ffsd = ffd->shared;

  SVN_MUTEX__WITH_LOCK(ffsd->txn_current_lock,
                       with_some_lock_file(fs, body, baton,
                                           path_txn_current_lock(fs, pool),
                                           FALSE,
                                           pool));

  return SVN_NO_ERROR;
}

/* A structure used by unlock_proto_rev() and unlock_proto_rev_body(),
   which see. */
struct unlock_proto_rev_baton
{
  const char *txn_id;
  void *lockcookie;
};

/* Callback used in the implementation of unlock_proto_rev(). */
static svn_error_t *
unlock_proto_rev_body(svn_fs_t *fs, const void *baton, apr_pool_t *pool)
{
  const struct unlock_proto_rev_baton *b = baton;
  const char *txn_id = b->txn_id;
  apr_file_t *lockfile = b->lockcookie;
  fs_fs_shared_txn_data_t *txn = get_shared_txn(fs, txn_id, FALSE);
  apr_status_t apr_err;

  if (!txn)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Can't unlock unknown transaction '%s'"),
                             txn_id);
  if (!txn->being_written)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Can't unlock nonlocked transaction '%s'"),
                             txn_id);

  apr_err = apr_file_unlock(lockfile);
  if (apr_err)
    return svn_error_wrap_apr
      (apr_err,
       _("Can't unlock prototype revision lockfile for transaction '%s'"),
       txn_id);
  apr_err = apr_file_close(lockfile);
  if (apr_err)
    return svn_error_wrap_apr
      (apr_err,
       _("Can't close prototype revision lockfile for transaction '%s'"),
       txn_id);

  txn->being_written = FALSE;

  return SVN_NO_ERROR;
}

/* Unlock the prototype revision file for transaction TXN_ID in filesystem
   FS using cookie LOCKCOOKIE.  The original prototype revision file must
   have been closed _before_ calling this function.

   Perform temporary allocations in POOL. */
static svn_error_t *
unlock_proto_rev(svn_fs_t *fs, const char *txn_id, void *lockcookie,
                 apr_pool_t *pool)
{
  struct unlock_proto_rev_baton b;

  b.txn_id = txn_id;
  b.lockcookie = lockcookie;
  return with_txnlist_lock(fs, unlock_proto_rev_body, &b, pool);
}

/* Same as unlock_proto_rev(), but requires that the transaction list
   lock is already held. */
static svn_error_t *
unlock_proto_rev_list_locked(svn_fs_t *fs, const char *txn_id,
                             void *lockcookie,
                             apr_pool_t *pool)
{
  struct unlock_proto_rev_baton b;

  b.txn_id = txn_id;
  b.lockcookie = lockcookie;
  return unlock_proto_rev_body(fs, &b, pool);
}

/* A structure used by get_writable_proto_rev() and
   get_writable_proto_rev_body(), which see. */
struct get_writable_proto_rev_baton
{
  apr_file_t **file;
  void **lockcookie;
  const char *txn_id;
};

/* Callback used in the implementation of get_writable_proto_rev(). */
static svn_error_t *
get_writable_proto_rev_body(svn_fs_t *fs, const void *baton, apr_pool_t *pool)
{
  const struct get_writable_proto_rev_baton *b = baton;
  apr_file_t **file = b->file;
  void **lockcookie = b->lockcookie;
  const char *txn_id = b->txn_id;
  svn_error_t *err;
  fs_fs_shared_txn_data_t *txn = get_shared_txn(fs, txn_id, TRUE);

  /* First, ensure that no thread in this process (including this one)
     is currently writing to this transaction's proto-rev file. */
  if (txn->being_written)
    return svn_error_createf(SVN_ERR_FS_REP_BEING_WRITTEN, NULL,
                             _("Cannot write to the prototype revision file "
                               "of transaction '%s' because a previous "
                               "representation is currently being written by "
                               "this process"),
                             txn_id);


  /* We know that no thread in this process is writing to the proto-rev
     file, and by extension, that no thread in this process is holding a
     lock on the prototype revision lock file.  It is therefore safe
     for us to attempt to lock this file, to see if any other process
     is holding a lock. */

  {
    apr_file_t *lockfile;
    apr_status_t apr_err;
    const char *lockfile_path = path_txn_proto_rev_lock(fs, txn_id, pool);

    /* Open the proto-rev lockfile, creating it if necessary, as it may
       not exist if the transaction dates from before the lockfiles were
       introduced.

       ### We'd also like to use something like svn_io_file_lock2(), but
           that forces us to create a subpool just to be able to unlock
           the file, which seems a waste. */
    SVN_ERR(svn_io_file_open(&lockfile, lockfile_path,
                             APR_WRITE | APR_CREATE, APR_OS_DEFAULT, pool));

    apr_err = apr_file_lock(lockfile,
                            APR_FLOCK_EXCLUSIVE | APR_FLOCK_NONBLOCK);
    if (apr_err)
      {
        svn_error_clear(svn_io_file_close(lockfile, pool));

        if (APR_STATUS_IS_EAGAIN(apr_err))
          return svn_error_createf(SVN_ERR_FS_REP_BEING_WRITTEN, NULL,
                                   _("Cannot write to the prototype revision "
                                     "file of transaction '%s' because a "
                                     "previous representation is currently "
                                     "being written by another process"),
                                   txn_id);

        return svn_error_wrap_apr(apr_err,
                                  _("Can't get exclusive lock on file '%s'"),
                                  svn_dirent_local_style(lockfile_path, pool));
      }

    *lockcookie = lockfile;
  }

  /* We've successfully locked the transaction; mark it as such. */
  txn->being_written = TRUE;


  /* Now open the prototype revision file and seek to the end. */
  err = svn_io_file_open(file, path_txn_proto_rev(fs, txn_id, pool),
                         APR_WRITE | APR_BUFFERED, APR_OS_DEFAULT, pool);

  /* You might expect that we could dispense with the following seek
     and achieve the same thing by opening the file using APR_APPEND.
     Unfortunately, APR's buffered file implementation unconditionally
     places its initial file pointer at the start of the file (even for
     files opened with APR_APPEND), so we need this seek to reconcile
     the APR file pointer to the OS file pointer (since we need to be
     able to read the current file position later). */
  if (!err)
    {
      apr_off_t offset = 0;
      err = svn_io_file_seek(*file, APR_END, &offset, pool);
    }

  if (err)
    {
      err = svn_error_compose_create(
              err,
              unlock_proto_rev_list_locked(fs, txn_id, *lockcookie, pool));

      *lockcookie = NULL;
    }

  return svn_error_trace(err);
}

/* Get a handle to the prototype revision file for transaction TXN_ID in
   filesystem FS, and lock it for writing.  Return FILE, a file handle
   positioned at the end of the file, and LOCKCOOKIE, a cookie that
   should be passed to unlock_proto_rev() to unlock the file once FILE
   has been closed.

   If the prototype revision file is already locked, return error
   SVN_ERR_FS_REP_BEING_WRITTEN.

   Perform all allocations in POOL. */
static svn_error_t *
get_writable_proto_rev(apr_file_t **file,
                       void **lockcookie,
                       svn_fs_t *fs, const char *txn_id,
                       apr_pool_t *pool)
{
  struct get_writable_proto_rev_baton b;

  b.file = file;
  b.lockcookie = lockcookie;
  b.txn_id = txn_id;

  return with_txnlist_lock(fs, get_writable_proto_rev_body, &b, pool);
}

/* Callback used in the implementation of purge_shared_txn(). */
static svn_error_t *
purge_shared_txn_body(svn_fs_t *fs, const void *baton, apr_pool_t *pool)
{
  const char *txn_id = baton;

  free_shared_txn(fs, txn_id);
  svn_fs_fs__reset_txn_caches(fs);

  return SVN_NO_ERROR;
}

/* Purge the shared data for transaction TXN_ID in filesystem FS.
   Perform all allocations in POOL. */
static svn_error_t *
purge_shared_txn(svn_fs_t *fs, const char *txn_id, apr_pool_t *pool)
{
  return with_txnlist_lock(fs, purge_shared_txn_body, txn_id, pool);
}



/* Fetch the current offset of FILE into *OFFSET_P. */
static svn_error_t *
get_file_offset(apr_off_t *offset_p, apr_file_t *file, apr_pool_t *pool)
{
  apr_off_t offset;

  /* Note that, for buffered files, one (possibly surprising) side-effect
     of this call is to flush any unwritten data to disk. */
  offset = 0;
  SVN_ERR(svn_io_file_seek(file, APR_CUR, &offset, pool));
  *offset_p = offset;

  return SVN_NO_ERROR;
}


/* Check that BUF, a nul-terminated buffer of text from file PATH,
   contains only digits at OFFSET and beyond, raising an error if not.
   TITLE contains a user-visible description of the file, usually the
   short file name.

   Uses POOL for temporary allocation. */
static svn_error_t *
check_file_buffer_numeric(const char *buf, apr_off_t offset,
                          const char *path, const char *title,
                          apr_pool_t *pool)
{
  const char *p;

  for (p = buf + offset; *p; p++)
    if (!svn_ctype_isdigit(*p))
      return svn_error_createf(SVN_ERR_BAD_VERSION_FILE_FORMAT, NULL,
        _("%s file '%s' contains unexpected non-digit '%c' within '%s'"),
        title, svn_dirent_local_style(path, pool), *p, buf);

  return SVN_NO_ERROR;
}

/* Check that BUF, a nul-terminated buffer of text from format file PATH,
   contains only digits at OFFSET and beyond, raising an error if not.

   Uses POOL for temporary allocation. */
static svn_error_t *
check_format_file_buffer_numeric(const char *buf, apr_off_t offset,
                                 const char *path, apr_pool_t *pool)
{
  return check_file_buffer_numeric(buf, offset, path, "Format", pool);
}

/* Return the error SVN_ERR_FS_UNSUPPORTED_FORMAT if FS's format
   number is not the same as a format number supported by this
   Subversion. */
static svn_error_t *
check_format(int format)
{
  /* Blacklist.  These formats may be either younger or older than
     SVN_FS_FS__FORMAT_NUMBER, but we don't support them. */
  if (format == SVN_FS_FS__PACKED_REVPROP_SQLITE_DEV_FORMAT)
    return svn_error_createf(SVN_ERR_FS_UNSUPPORTED_FORMAT, NULL,
                             _("Found format '%d', only created by "
                               "unreleased dev builds; see "
                               "http://subversion.apache.org"
                               "/docs/release-notes/1.7#revprop-packing"),
                             format);

  /* We support all formats from 1-current simultaneously */
  if (1 <= format && format <= SVN_FS_FS__FORMAT_NUMBER)
    return SVN_NO_ERROR;

  return svn_error_createf(SVN_ERR_FS_UNSUPPORTED_FORMAT, NULL,
     _("Expected FS format between '1' and '%d'; found format '%d'"),
     SVN_FS_FS__FORMAT_NUMBER, format);
}

/* Read the format number and maximum number of files per directory
   from PATH and return them in *PFORMAT and *MAX_FILES_PER_DIR
   respectively.

   *MAX_FILES_PER_DIR is obtained from the 'layout' format option, and
   will be set to zero if a linear scheme should be used.

   Use POOL for temporary allocation. */
static svn_error_t *
read_format(int *pformat, int *max_files_per_dir,
            const char *path, apr_pool_t *pool)
{
  svn_error_t *err;
  svn_stream_t *stream;
  svn_stringbuf_t *content;
  svn_stringbuf_t *buf;
  svn_boolean_t eos = FALSE;

  err = svn_stringbuf_from_file2(&content, path, pool);
  if (err && APR_STATUS_IS_ENOENT(err->apr_err))
    {
      /* Treat an absent format file as format 1.  Do not try to
         create the format file on the fly, because the repository
         might be read-only for us, or this might be a read-only
         operation, and the spirit of FSFS is to make no changes
         whatseover in read-only operations.  See thread starting at
         http://subversion.tigris.org/servlets/ReadMsg?list=dev&msgNo=97600
         for more. */
      svn_error_clear(err);
      *pformat = 1;
      *max_files_per_dir = 0;

      return SVN_NO_ERROR;
    }
  SVN_ERR(err);

  stream = svn_stream_from_stringbuf(content, pool);
  SVN_ERR(svn_stream_readline(stream, &buf, "\n", &eos, pool));
  if (buf->len == 0 && eos)
    {
      /* Return a more useful error message. */
      return svn_error_createf(SVN_ERR_BAD_VERSION_FILE_FORMAT, NULL,
                               _("Can't read first line of format file '%s'"),
                               svn_dirent_local_style(path, pool));
    }

  /* Check that the first line contains only digits. */
  SVN_ERR(check_format_file_buffer_numeric(buf->data, 0, path, pool));
  SVN_ERR(svn_cstring_atoi(pformat, buf->data));

  /* Check that we support this format at all */
  SVN_ERR(check_format(*pformat));

  /* Set the default values for anything that can be set via an option. */
  *max_files_per_dir = 0;

  /* Read any options. */
  while (!eos)
    {
      SVN_ERR(svn_stream_readline(stream, &buf, "\n", &eos, pool));
      if (buf->len == 0)
        break;

      if (*pformat >= SVN_FS_FS__MIN_LAYOUT_FORMAT_OPTION_FORMAT &&
          strncmp(buf->data, "layout ", 7) == 0)
        {
          if (strcmp(buf->data + 7, "linear") == 0)
            {
              *max_files_per_dir = 0;
              continue;
            }

          if (strncmp(buf->data + 7, "sharded ", 8) == 0)
            {
              /* Check that the argument is numeric. */
              SVN_ERR(check_format_file_buffer_numeric(buf->data, 15, path, pool));
              SVN_ERR(svn_cstring_atoi(max_files_per_dir, buf->data + 15));
              continue;
            }
        }

      return svn_error_createf(SVN_ERR_BAD_VERSION_FILE_FORMAT, NULL,
         _("'%s' contains invalid filesystem format option '%s'"),
         svn_dirent_local_style(path, pool), buf->data);
    }

  return SVN_NO_ERROR;
}

/* Write the format number and maximum number of files per directory
   to a new format file in PATH, possibly expecting to overwrite a
   previously existing file.

   Use POOL for temporary allocation. */
static svn_error_t *
write_format(const char *path, int format, int max_files_per_dir,
             svn_boolean_t overwrite, apr_pool_t *pool)
{
  svn_stringbuf_t *sb;

  SVN_ERR_ASSERT(1 <= format && format <= SVN_FS_FS__FORMAT_NUMBER);

  sb = svn_stringbuf_createf(pool, "%d\n", format);

  if (format >= SVN_FS_FS__MIN_LAYOUT_FORMAT_OPTION_FORMAT)
    {
      if (max_files_per_dir)
        svn_stringbuf_appendcstr(sb, apr_psprintf(pool, "layout sharded %d\n",
                                                  max_files_per_dir));
      else
        svn_stringbuf_appendcstr(sb, "layout linear\n");
    }

  /* svn_io_write_version_file() does a load of magic to allow it to
     replace version files that already exist.  We only need to do
     that when we're allowed to overwrite an existing file. */
  if (! overwrite)
    {
      /* Create the file */
      SVN_ERR(svn_io_file_create(path, sb->data, pool));
    }
  else
    {
      const char *path_tmp;

      SVN_ERR(svn_io_write_unique(&path_tmp,
                                  svn_dirent_dirname(path, pool),
                                  sb->data, sb->len,
                                  svn_io_file_del_none, pool));

      /* rename the temp file as the real destination */
      SVN_ERR(svn_io_file_rename(path_tmp, path, pool));
    }

  /* And set the perms to make it read only */
  return svn_io_set_file_read_only(path, FALSE, pool);
}

svn_boolean_t
svn_fs_fs__fs_supports_mergeinfo(svn_fs_t *fs)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  return ffd->format >= SVN_FS_FS__MIN_MERGEINFO_FORMAT;
}

/* Read the configuration information of the file system at FS_PATH
 * and set the respective values in FFD.  Use POOL for allocations.
 */
static svn_error_t *
read_config(fs_fs_data_t *ffd,
            const char *fs_path,
            apr_pool_t *pool)
{
  SVN_ERR(svn_config_read3(&ffd->config,
                           svn_dirent_join(fs_path, PATH_CONFIG, pool),
                           FALSE, FALSE, FALSE, pool));

  /* Initialize ffd->rep_sharing_allowed. */
  if (ffd->format >= SVN_FS_FS__MIN_REP_SHARING_FORMAT)
    SVN_ERR(svn_config_get_bool(ffd->config, &ffd->rep_sharing_allowed,
                                CONFIG_SECTION_REP_SHARING,
                                CONFIG_OPTION_ENABLE_REP_SHARING, TRUE));
  else
    ffd->rep_sharing_allowed = FALSE;

  /* Initialize deltification settings in ffd. */
  if (ffd->format >= SVN_FS_FS__MIN_DELTIFICATION_FORMAT)
    {
      SVN_ERR(svn_config_get_bool(ffd->config, &ffd->deltify_directories,
                                  CONFIG_SECTION_DELTIFICATION,
                                  CONFIG_OPTION_ENABLE_DIR_DELTIFICATION,
                                  FALSE));
      SVN_ERR(svn_config_get_bool(ffd->config, &ffd->deltify_properties,
                                  CONFIG_SECTION_DELTIFICATION,
                                  CONFIG_OPTION_ENABLE_PROPS_DELTIFICATION,
                                  FALSE));
      SVN_ERR(svn_config_get_int64(ffd->config, &ffd->max_deltification_walk,
                                   CONFIG_SECTION_DELTIFICATION,
                                   CONFIG_OPTION_MAX_DELTIFICATION_WALK,
                                   SVN_FS_FS_MAX_DELTIFICATION_WALK));
      SVN_ERR(svn_config_get_int64(ffd->config, &ffd->max_linear_deltification,
                                   CONFIG_SECTION_DELTIFICATION,
                                   CONFIG_OPTION_MAX_LINEAR_DELTIFICATION,
                                   SVN_FS_FS_MAX_LINEAR_DELTIFICATION));
    }
  else
    {
      ffd->deltify_directories = FALSE;
      ffd->deltify_properties = FALSE;
      ffd->max_deltification_walk = SVN_FS_FS_MAX_DELTIFICATION_WALK;
      ffd->max_linear_deltification = SVN_FS_FS_MAX_LINEAR_DELTIFICATION;
    }

  /* Initialize revprop packing settings in ffd. */
  if (ffd->format >= SVN_FS_FS__MIN_PACKED_REVPROP_FORMAT)
    {
      SVN_ERR(svn_config_get_bool(ffd->config, &ffd->compress_packed_revprops,
                                  CONFIG_SECTION_PACKED_REVPROPS,
                                  CONFIG_OPTION_COMPRESS_PACKED_REVPROPS,
                                  FALSE));
      SVN_ERR(svn_config_get_int64(ffd->config, &ffd->revprop_pack_size,
                                   CONFIG_SECTION_PACKED_REVPROPS,
                                   CONFIG_OPTION_REVPROP_PACK_SIZE,
                                   ffd->compress_packed_revprops
                                       ? 0x100
                                       : 0x40));

      ffd->revprop_pack_size *= 1024;
    }
  else
    {
      ffd->revprop_pack_size = 0x10000;
      ffd->compress_packed_revprops = FALSE;
    }

  return SVN_NO_ERROR;
}

static svn_error_t *
write_config(svn_fs_t *fs,
             apr_pool_t *pool)
{
#define NL APR_EOL_STR
  static const char * const fsfs_conf_contents =
"### This file controls the configuration of the FSFS filesystem."           NL
""                                                                           NL
"[" SVN_CACHE_CONFIG_CATEGORY_MEMCACHED_SERVERS "]"                          NL
"### These options name memcached servers used to cache internal FSFS"       NL
"### data.  See http://www.danga.com/memcached/ for more information on"     NL
"### memcached.  To use memcached with FSFS, run one or more memcached"      NL
"### servers, and specify each of them as an option like so:"                NL
"# first-server = 127.0.0.1:11211"                                           NL
"# remote-memcached = mymemcached.corp.example.com:11212"                    NL
"### The option name is ignored; the value is of the form HOST:PORT."        NL
"### memcached servers can be shared between multiple repositories;"         NL
"### however, if you do this, you *must* ensure that repositories have"      NL
"### distinct UUIDs and paths, or else cached data from one repository"      NL
"### might be used by another accidentally.  Note also that memcached has"   NL
"### no authentication for reads or writes, so you must ensure that your"    NL
"### memcached servers are only accessible by trusted users."                NL
""                                                                           NL
"[" CONFIG_SECTION_CACHES "]"                                                NL
"### When a cache-related error occurs, normally Subversion ignores it"      NL
"### and continues, logging an error if the server is appropriately"         NL
"### configured (and ignoring it with file:// access).  To make"             NL
"### Subversion never ignore cache errors, uncomment this line."             NL
"# " CONFIG_OPTION_FAIL_STOP " = true"                                       NL
""                                                                           NL
"[" CONFIG_SECTION_REP_SHARING "]"                                           NL
"### To conserve space, the filesystem can optionally avoid storing"         NL
"### duplicate representations.  This comes at a slight cost in"             NL
"### performance, as maintaining a database of shared representations can"   NL
"### increase commit times.  The space savings are dependent upon the size"  NL
"### of the repository, the number of objects it contains and the amount of" NL
"### duplication between them, usually a function of the branching and"      NL
"### merging process."                                                       NL
"###"                                                                        NL
"### The following parameter enables rep-sharing in the repository.  It can" NL
"### be switched on and off at will, but for best space-saving results"      NL
"### should be enabled consistently over the life of the repository."        NL
"### 'svnadmin verify' will check the rep-cache regardless of this setting." NL
"### rep-sharing is enabled by default."                                     NL
"# " CONFIG_OPTION_ENABLE_REP_SHARING " = true"                              NL
""                                                                           NL
"[" CONFIG_SECTION_DELTIFICATION "]"                                         NL
"### To conserve space, the filesystem stores data as differences against"   NL
"### existing representations.  This comes at a slight cost in performance," NL
"### as calculating differences can increase commit times.  Reading data"    NL
"### will also create higher CPU load and the data will be fragmented."      NL
"### Since deltification tends to save significant amounts of disk space,"   NL
"### the overall I/O load can actually be lower."                            NL
"###"                                                                        NL
"### The options in this section allow for tuning the deltification"         NL
"### strategy.  Their effects on data size and server performance may vary"  NL
"### from one repository to another.  Versions prior to 1.8 will ignore"     NL
"### this section."                                                          NL
"###"                                                                        NL
"### The following parameter enables deltification for directories. It can"  NL
"### be switched on and off at will, but for best space-saving results"      NL
"### should be enabled consistently over the life of the repository."        NL
"### Repositories containing large directories will benefit greatly."        NL
"### In rarely read repositories, the I/O overhead may be significant as"    NL
"### cache hit rates will most likely be low"                                NL
"### directory deltification is disabled by default."                        NL
"# " CONFIG_OPTION_ENABLE_DIR_DELTIFICATION " = false"                       NL
"###"                                                                        NL
"### The following parameter enables deltification for properties on files"  NL
"### and directories.  Overall, this is a minor tuning option but can save"  NL
"### some disk space if you merge frequently or frequently change node"      NL
"### properties.  You should not activate this if rep-sharing has been"      NL
"### disabled because this may result in a net increase in repository size." NL
"### property deltification is disabled by default."                         NL
"# " CONFIG_OPTION_ENABLE_PROPS_DELTIFICATION " = false"                     NL
"###"                                                                        NL
"### During commit, the server may need to walk the whole change history of" NL
"### of a given node to find a suitable deltification base.  This linear"    NL
"### process can impact commit times, svnadmin load and similar operations." NL
"### This setting limits the depth of the deltification history.  If the"    NL
"### threshold has been reached, the node will be stored as fulltext and a"  NL
"### new deltification history begins."                                      NL
"### Note, this is unrelated to svn log."                                    NL
"### Very large values rarely provide significant additional savings but"    NL
"### can impact performance greatly - in particular if directory"            NL
"### deltification has been activated.  Very small values may be useful in"  NL
"### repositories that are dominated by large, changing binaries."           NL
"### Should be a power of two minus 1.  A value of 0 will effectively"       NL
"### disable deltification."                                                 NL
"### For 1.8, the default value is 1023; earlier versions have no limit."    NL
"# " CONFIG_OPTION_MAX_DELTIFICATION_WALK " = 1023"                          NL
"###"                                                                        NL
"### The skip-delta scheme used by FSFS tends to repeatably store redundant" NL
"### delta information where a simple delta against the latest version is"   NL
"### often smaller.  By default, 1.8+ will therefore use skip deltas only"   NL
"### after the linear chain of deltas has grown beyond the threshold"        NL
"### specified by this setting."                                             NL
"### Values up to 64 can result in some reduction in repository size for"    NL
"### the cost of quickly increasing I/O and CPU costs. Similarly, smaller"   NL
"### numbers can reduce those costs at the cost of more disk space.  For"    NL
"### rarely read repositories or those containing larger binaries, this may" NL
"### present a better trade-off."                                            NL
"### Should be a power of two.  A value of 1 or smaller will cause the"      NL
"### exclusive use of skip-deltas (as in pre-1.8)."                          NL
"### For 1.8, the default value is 16; earlier versions use 1."              NL
"# " CONFIG_OPTION_MAX_LINEAR_DELTIFICATION " = 16"                          NL
""                                                                           NL
"[" CONFIG_SECTION_PACKED_REVPROPS "]"                                       NL
"### This parameter controls the size (in kBytes) of packed revprop files."  NL
"### Revprops of consecutive revisions will be concatenated into a single"   NL
"### file up to but not exceeding the threshold given here.  However, each"  NL
"### pack file may be much smaller and revprops of a single revision may be" NL
"### much larger than the limit set here.  The threshold will be applied"    NL
"### before optional compression takes place."                               NL
"### Large values will reduce disk space usage at the expense of increased"  NL
"### latency and CPU usage reading and changing individual revprops.  They"  NL
"### become an advantage when revprop caching has been enabled because a"    NL
"### lot of data can be read in one go.  Values smaller than 4 kByte will"   NL
"### not improve latency any further and quickly render revprop packing"     NL
"### ineffective."                                                           NL
"### revprop-pack-size is 64 kBytes by default for non-compressed revprop"   NL
"### pack files and 256 kBytes when compression has been enabled."           NL
"# " CONFIG_OPTION_REVPROP_PACK_SIZE " = 64"                                 NL
"###"                                                                        NL
"### To save disk space, packed revprop files may be compressed.  Standard"  NL
"### revprops tend to allow for very effective compression.  Reading and"    NL
"### even more so writing, become significantly more CPU intensive.  With"   NL
"### revprop caching enabled, the overhead can be offset by reduced I/O"     NL
"### unless you often modify revprops after packing."                        NL
"### Compressing packed revprops is disabled by default."                    NL
"# " CONFIG_OPTION_COMPRESS_PACKED_REVPROPS " = false"                       NL
;
#undef NL
  return svn_io_file_create(svn_dirent_join(fs->path, PATH_CONFIG, pool),
                            fsfs_conf_contents, pool);
}

static svn_error_t *
read_min_unpacked_rev(svn_revnum_t *min_unpacked_rev,
                      const char *path,
                      apr_pool_t *pool)
{
  char buf[80];
  apr_file_t *file;
  apr_size_t len;

  SVN_ERR(svn_io_file_open(&file, path, APR_READ | APR_BUFFERED,
                           APR_OS_DEFAULT, pool));
  len = sizeof(buf);
  SVN_ERR(svn_io_read_length_line(file, buf, &len, pool));
  SVN_ERR(svn_io_file_close(file, pool));

  *min_unpacked_rev = SVN_STR_TO_REV(buf);
  return SVN_NO_ERROR;
}

static svn_error_t *
update_min_unpacked_rev(svn_fs_t *fs, apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  SVN_ERR_ASSERT(ffd->format >= SVN_FS_FS__MIN_PACKED_FORMAT);

  return read_min_unpacked_rev(&ffd->min_unpacked_rev,
                               path_min_unpacked_rev(fs, pool),
                               pool);
}

svn_error_t *
svn_fs_fs__open(svn_fs_t *fs, const char *path, apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  apr_file_t *uuid_file;
  int format, max_files_per_dir;
  char buf[APR_UUID_FORMATTED_LENGTH + 2];
  apr_size_t limit;

  fs->path = apr_pstrdup(fs->pool, path);

  /* Read the FS format number. */
  SVN_ERR(read_format(&format, &max_files_per_dir,
                      path_format(fs, pool), pool));

  /* Now we've got a format number no matter what. */
  ffd->format = format;
  ffd->max_files_per_dir = max_files_per_dir;

  /* Read in and cache the repository uuid. */
  SVN_ERR(svn_io_file_open(&uuid_file, path_uuid(fs, pool),
                           APR_READ | APR_BUFFERED, APR_OS_DEFAULT, pool));

  limit = sizeof(buf);
  SVN_ERR(svn_io_read_length_line(uuid_file, buf, &limit, pool));
  fs->uuid = apr_pstrdup(fs->pool, buf);

  SVN_ERR(svn_io_file_close(uuid_file, pool));

  /* Read the min unpacked revision. */
  if (ffd->format >= SVN_FS_FS__MIN_PACKED_FORMAT)
    SVN_ERR(update_min_unpacked_rev(fs, pool));

  /* Read the configuration file. */
  SVN_ERR(read_config(ffd, fs->path, pool));

  return get_youngest(&(ffd->youngest_rev_cache), path, pool);
}

/* Wrapper around svn_io_file_create which ignores EEXIST. */
static svn_error_t *
create_file_ignore_eexist(const char *file,
                          const char *contents,
                          apr_pool_t *pool)
{
  svn_error_t *err = svn_io_file_create(file, contents, pool);
  if (err && APR_STATUS_IS_EEXIST(err->apr_err))
    {
      svn_error_clear(err);
      err = SVN_NO_ERROR;
    }
  return svn_error_trace(err);
}

/* forward declarations */

static svn_error_t *
pack_revprops_shard(const char *pack_file_dir,
                    const char *shard_path,
                    apr_int64_t shard,
                    int max_files_per_dir,
                    apr_off_t max_pack_size,
                    int compression_level,
                    svn_cancel_func_t cancel_func,
                    void *cancel_baton,
                    apr_pool_t *scratch_pool);

static svn_error_t *
delete_revprops_shard(const char *shard_path,
                      apr_int64_t shard,
                      int max_files_per_dir,
                      svn_cancel_func_t cancel_func,
                      void *cancel_baton,
                      apr_pool_t *scratch_pool);

/* In the filesystem FS, pack all revprop shards up to min_unpacked_rev.
 * 
 * NOTE: Keep the old non-packed shards around until after the format bump.
 * Otherwise, re-running upgrade will drop the packed revprop shard but
 * have no unpacked data anymore.  Call upgrade_cleanup_pack_revprops after
 * the bump.
 * 
 * Use SCRATCH_POOL for temporary allocations.
 */
static svn_error_t *
upgrade_pack_revprops(svn_fs_t *fs,
                      apr_pool_t *scratch_pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  const char *revprops_shard_path;
  const char *revprops_pack_file_dir;
  apr_int64_t shard;
  apr_int64_t first_unpacked_shard
    =  ffd->min_unpacked_rev / ffd->max_files_per_dir;

  apr_pool_t *iterpool = svn_pool_create(scratch_pool);
  const char *revsprops_dir = svn_dirent_join(fs->path, PATH_REVPROPS_DIR,
                                              scratch_pool);
  int compression_level = ffd->compress_packed_revprops
                           ? SVN_DELTA_COMPRESSION_LEVEL_DEFAULT
                           : SVN_DELTA_COMPRESSION_LEVEL_NONE;

  /* first, pack all revprops shards to match the packed revision shards */
  for (shard = 0; shard < first_unpacked_shard; ++shard)
    {
      revprops_pack_file_dir = svn_dirent_join(revsprops_dir,
                   apr_psprintf(iterpool,
                                "%" APR_INT64_T_FMT PATH_EXT_PACKED_SHARD,
                                shard),
                   iterpool);
      revprops_shard_path = svn_dirent_join(revsprops_dir,
                       apr_psprintf(iterpool, "%" APR_INT64_T_FMT, shard),
                       iterpool);

      SVN_ERR(pack_revprops_shard(revprops_pack_file_dir, revprops_shard_path,
                                  shard, ffd->max_files_per_dir,
                                  (int)(0.9 * ffd->revprop_pack_size),
                                  compression_level,
                                  NULL, NULL, iterpool));
      svn_pool_clear(iterpool);
    }

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

/* In the filesystem FS, remove all non-packed revprop shards up to
 * min_unpacked_rev.  Use SCRATCH_POOL for temporary allocations.
 * See upgrade_pack_revprops for more info.
 */
static svn_error_t *
upgrade_cleanup_pack_revprops(svn_fs_t *fs,
                              apr_pool_t *scratch_pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  const char *revprops_shard_path;
  apr_int64_t shard;
  apr_int64_t first_unpacked_shard
    =  ffd->min_unpacked_rev / ffd->max_files_per_dir;

  apr_pool_t *iterpool = svn_pool_create(scratch_pool);
  const char *revsprops_dir = svn_dirent_join(fs->path, PATH_REVPROPS_DIR,
                                              scratch_pool);
  
  /* delete the non-packed revprops shards afterwards */
  for (shard = 0; shard < first_unpacked_shard; ++shard)
    {
      revprops_shard_path = svn_dirent_join(revsprops_dir,
                       apr_psprintf(iterpool, "%" APR_INT64_T_FMT, shard),
                       iterpool);
      SVN_ERR(delete_revprops_shard(revprops_shard_path,
                                    shard, ffd->max_files_per_dir,
                                    NULL, NULL, iterpool));
      svn_pool_clear(iterpool);
    }

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

static svn_error_t *
upgrade_body(void *baton, apr_pool_t *pool)
{
  svn_fs_t *fs = baton;
  int format, max_files_per_dir;
  const char *format_path = path_format(fs, pool);
  svn_node_kind_t kind;
  svn_boolean_t needs_revprop_shard_cleanup = FALSE;

  /* Read the FS format number and max-files-per-dir setting. */
  SVN_ERR(read_format(&format, &max_files_per_dir, format_path, pool));

  /* If the config file does not exist, create one. */
  SVN_ERR(svn_io_check_path(svn_dirent_join(fs->path, PATH_CONFIG, pool),
                            &kind, pool));
  switch (kind)
    {
    case svn_node_none:
      SVN_ERR(write_config(fs, pool));
      break;
    case svn_node_file:
      break;
    default:
      return svn_error_createf(SVN_ERR_FS_GENERAL, NULL,
                               _("'%s' is not a regular file."
                                 " Please move it out of "
                                 "the way and try again"),
                               svn_dirent_join(fs->path, PATH_CONFIG, pool));
    }

  /* If we're already up-to-date, there's nothing else to be done here. */
  if (format == SVN_FS_FS__FORMAT_NUMBER)
    return SVN_NO_ERROR;

  /* If our filesystem predates the existance of the 'txn-current
     file', make that file and its corresponding lock file. */
  if (format < SVN_FS_FS__MIN_TXN_CURRENT_FORMAT)
    {
      SVN_ERR(create_file_ignore_eexist(path_txn_current(fs, pool), "0\n",
                                        pool));
      SVN_ERR(create_file_ignore_eexist(path_txn_current_lock(fs, pool), "",
                                        pool));
    }

  /* If our filesystem predates the existance of the 'txn-protorevs'
     dir, make that directory.  */
  if (format < SVN_FS_FS__MIN_PROTOREVS_DIR_FORMAT)
    {
      /* We don't use path_txn_proto_rev() here because it expects
         we've already bumped our format. */
      SVN_ERR(svn_io_make_dir_recursively(
          svn_dirent_join(fs->path, PATH_TXN_PROTOS_DIR, pool), pool));
    }

  /* If our filesystem is new enough, write the min unpacked rev file. */
  if (format < SVN_FS_FS__MIN_PACKED_FORMAT)
    SVN_ERR(svn_io_file_create(path_min_unpacked_rev(fs, pool), "0\n", pool));

  /* If the file system supports revision packing but not revprop packing
     *and* the FS has been sharded, pack the revprops up to the point that
     revision data has been packed.  However, keep the non-packed revprop
     files around until after the format bump */
  if (   format >= SVN_FS_FS__MIN_PACKED_FORMAT
      && format < SVN_FS_FS__MIN_PACKED_REVPROP_FORMAT
      && max_files_per_dir > 0)
    {
      needs_revprop_shard_cleanup = TRUE;
      SVN_ERR(upgrade_pack_revprops(fs, pool));
    }

  /* Bump the format file. */
  SVN_ERR(write_format(format_path, SVN_FS_FS__FORMAT_NUMBER,
                       max_files_per_dir, TRUE, pool));

  /* Now, it is safe to remove the redundant revprop files. */
  if (needs_revprop_shard_cleanup)
    SVN_ERR(upgrade_cleanup_pack_revprops(fs, pool));

  /* Done */
  return SVN_NO_ERROR;
}


svn_error_t *
svn_fs_fs__upgrade(svn_fs_t *fs, apr_pool_t *pool)
{
  return svn_fs_fs__with_write_lock(fs, upgrade_body, (void *)fs, pool);
}


/* Functions for dealing with recoverable errors on mutable files
 *
 * Revprops, current, and txn-current files are mutable; that is, they
 * change as part of normal fsfs operation, in constrat to revs files, or
 * the format file, which are written once at create (or upgrade) time.
 * When more than one host writes to the same repository, we will
 * sometimes see these recoverable errors when accesssing these files.
 *
 * These errors all relate to NFS, and thus we only use this retry code if
 * ESTALE is defined.
 *
 ** ESTALE
 *
 * In NFS v3 and under, the server doesn't track opened files.  If you
 * unlink(2) or rename(2) a file held open by another process *on the
 * same host*, that host's kernel typically renames the file to
 * .nfsXXXX and automatically deletes that when it's no longer open,
 * but this behavior is not required.
 *
 * For obvious reasons, this does not work *across hosts*.  No one
 * knows about the opened file; not the server, and not the deleting
 * client.  So the file vanishes, and the reader gets stale NFS file
 * handle.
 *
 ** EIO, ENOENT
 *
 * Some client implementations (at least the 2.6.18.5 kernel that ships
 * with Ubuntu Dapper) sometimes give spurious ENOENT (only on open) or
 * even EIO errors when trying to read these files that have been renamed
 * over on some other host.
 *
 ** Solution
 *
 * Try open and read of such files in try_stringbuf_from_file().  Call
 * this function within a loop of RECOVERABLE_RETRY_COUNT iterations
 * (though, realistically, the second try will succeed).
 */

#define RECOVERABLE_RETRY_COUNT 10

/* Read the file at PATH and return its content in *CONTENT. *CONTENT will
 * not be modified unless the whole file was read successfully.
 *
 * ESTALE, EIO and ENOENT will not cause this function to return an error
 * unless LAST_ATTEMPT has been set.  If MISSING is not NULL, indicate
 * missing files (ENOENT) there.
 *
 * Use POOL for allocations.
 */
static svn_error_t *
try_stringbuf_from_file(svn_stringbuf_t **content,
                        svn_boolean_t *missing,
                        const char *path,
                        svn_boolean_t last_attempt,
                        apr_pool_t *pool)
{
  svn_error_t *err = svn_stringbuf_from_file2(content, path, pool);
  if (missing)
    *missing = FALSE;

  if (err)
    {
      *content = NULL;

      if (APR_STATUS_IS_ENOENT(err->apr_err))
        {
          if (!last_attempt)
            {
              svn_error_clear(err);
              if (missing)
                *missing = TRUE;
              return SVN_NO_ERROR;
            }
        }
#ifdef ESTALE
      else if (APR_TO_OS_ERROR(err->apr_err) == ESTALE
                || APR_TO_OS_ERROR(err->apr_err) == EIO)
        {
          if (!last_attempt)
            {
              svn_error_clear(err);
              return SVN_NO_ERROR;
            }
        }
#endif
    }

  return svn_error_trace(err);
}

/* Read the 'current' file FNAME and store the contents in *BUF.
   Allocations are performed in POOL. */
static svn_error_t *
read_content(svn_stringbuf_t **content, const char *fname, apr_pool_t *pool)
{
  int i;
  *content = NULL;

  for (i = 0; !*content && (i < RECOVERABLE_RETRY_COUNT); ++i)
    SVN_ERR(try_stringbuf_from_file(content, NULL,
                                    fname, i + 1 < RECOVERABLE_RETRY_COUNT,
                                    pool));

  if (!*content)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Can't read '%s'"),
                             svn_dirent_local_style(fname, pool));

  return SVN_NO_ERROR;
}

/* Find the youngest revision in a repository at path FS_PATH and
   return it in *YOUNGEST_P.  Perform temporary allocations in
   POOL. */
static svn_error_t *
get_youngest(svn_revnum_t *youngest_p,
             const char *fs_path,
             apr_pool_t *pool)
{
  svn_stringbuf_t *buf;
  SVN_ERR(read_content(&buf, svn_dirent_join(fs_path, PATH_CURRENT, pool),
                       pool));

  *youngest_p = SVN_STR_TO_REV(buf->data);

  return SVN_NO_ERROR;
}


svn_error_t *
svn_fs_fs__youngest_rev(svn_revnum_t *youngest_p,
                        svn_fs_t *fs,
                        apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  SVN_ERR(get_youngest(youngest_p, fs->path, pool));
  ffd->youngest_rev_cache = *youngest_p;

  return SVN_NO_ERROR;
}

/* Given a revision file FILE that has been pre-positioned at the
   beginning of a Node-Rev header block, read in that header block and
   store it in the apr_hash_t HEADERS.  All allocations will be from
   POOL. */
static svn_error_t * read_header_block(apr_hash_t **headers,
                                       svn_stream_t *stream,
                                       apr_pool_t *pool)
{
  *headers = apr_hash_make(pool);

  while (1)
    {
      svn_stringbuf_t *header_str;
      const char *name, *value;
      apr_size_t i = 0;
      svn_boolean_t eof;

      SVN_ERR(svn_stream_readline(stream, &header_str, "\n", &eof, pool));

      if (eof || header_str->len == 0)
        break; /* end of header block */

      while (header_str->data[i] != ':')
        {
          if (header_str->data[i] == '\0')
            return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                     _("Found malformed header '%s' in "
                                       "revision file"),
                                     header_str->data);
          i++;
        }

      /* Create a 'name' string and point to it. */
      header_str->data[i] = '\0';
      name = header_str->data;

      /* Skip over the NULL byte and the space following it. */
      i += 2;

      if (i > header_str->len)
        {
          /* Restore the original line for the error. */
          i -= 2;
          header_str->data[i] = ':';
          return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                   _("Found malformed header '%s' in "
                                     "revision file"),
                                   header_str->data);
        }

      value = header_str->data + i;

      /* header_str is safely in our pool, so we can use bits of it as
         key and value. */
      svn_hash_sets(*headers, name, value);
    }

  return SVN_NO_ERROR;
}

/* Return SVN_ERR_FS_NO_SUCH_REVISION if the given revision is newer
   than the current youngest revision or is simply not a valid
   revision number, else return success.

   FSFS is based around the concept that commits only take effect when
   the number in "current" is bumped.  Thus if there happens to be a rev
   or revprops file installed for a revision higher than the one recorded
   in "current" (because a commit failed between installing the rev file
   and bumping "current", or because an administrator rolled back the
   repository by resetting "current" without deleting rev files, etc), it
   ought to be completely ignored.  This function provides the check
   by which callers can make that decision. */
static svn_error_t *
ensure_revision_exists(svn_fs_t *fs,
                       svn_revnum_t rev,
                       apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  if (! SVN_IS_VALID_REVNUM(rev))
    return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
                             _("Invalid revision number '%ld'"), rev);


  /* Did the revision exist the last time we checked the current
     file? */
  if (rev <= ffd->youngest_rev_cache)
    return SVN_NO_ERROR;

  SVN_ERR(get_youngest(&(ffd->youngest_rev_cache), fs->path, pool));

  /* Check again. */
  if (rev <= ffd->youngest_rev_cache)
    return SVN_NO_ERROR;

  return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
                           _("No such revision %ld"), rev);
}

svn_error_t *
svn_fs_fs__revision_exists(svn_revnum_t rev,
                           svn_fs_t *fs,
                           apr_pool_t *pool)
{
  /* Different order of parameters. */
  SVN_ERR(ensure_revision_exists(fs, rev, pool));
  return SVN_NO_ERROR;
}

/* Open the correct revision file for REV.  If the filesystem FS has
   been packed, *FILE will be set to the packed file; otherwise, set *FILE
   to the revision file for REV.  Return SVN_ERR_FS_NO_SUCH_REVISION if the
   file doesn't exist.

   TODO: Consider returning an indication of whether this is a packed rev
         file, so the caller need not rely on is_packed_rev() which in turn
         relies on the cached FFD->min_unpacked_rev value not having changed
         since the rev file was opened.

   Use POOL for allocations. */
static svn_error_t *
open_pack_or_rev_file(apr_file_t **file,
                      svn_fs_t *fs,
                      svn_revnum_t rev,
                      apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  svn_error_t *err;
  const char *path;
  svn_boolean_t retry = FALSE;

  do
    {
      err = svn_fs_fs__path_rev_absolute(&path, fs, rev, pool);

      /* open the revision file in buffered r/o mode */
      if (! err)
        err = svn_io_file_open(file, path,
                              APR_READ | APR_BUFFERED, APR_OS_DEFAULT, pool);

      if (err && APR_STATUS_IS_ENOENT(err->apr_err))
        {
          if (ffd->format >= SVN_FS_FS__MIN_PACKED_FORMAT)
            {
              /* Could not open the file. This may happen if the
               * file once existed but got packed later. */
              svn_error_clear(err);

              /* if that was our 2nd attempt, leave it at that. */
              if (retry)
                return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
                                         _("No such revision %ld"), rev);

              /* We failed for the first time. Refresh cache & retry. */
              SVN_ERR(update_min_unpacked_rev(fs, pool));

              retry = TRUE;
            }
          else
            {
              svn_error_clear(err);
              return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
                                       _("No such revision %ld"), rev);
            }
        }
      else
        {
          retry = FALSE;
        }
    }
  while (retry);

  return svn_error_trace(err);
}

/* Reads a line from STREAM and converts it to a 64 bit integer to be
 * returned in *RESULT.  If we encounter eof, set *HIT_EOF and leave
 * *RESULT unchanged.  If HIT_EOF is NULL, EOF causes an "corrupt FS"
 * error return.
 * SCRATCH_POOL is used for temporary allocations.
 */
static svn_error_t *
read_number_from_stream(apr_int64_t *result,
                        svn_boolean_t *hit_eof,
                        svn_stream_t *stream,
                        apr_pool_t *scratch_pool)
{
  svn_stringbuf_t *sb;
  svn_boolean_t eof;
  svn_error_t *err;

  SVN_ERR(svn_stream_readline(stream, &sb, "\n", &eof, scratch_pool));
  if (hit_eof)
    *hit_eof = eof;
  else
    if (eof)
      return svn_error_create(SVN_ERR_FS_CORRUPT, NULL, _("Unexpected EOF"));

  if (!eof)
    {
      err = svn_cstring_atoi64(result, sb->data);
      if (err)
        return svn_error_createf(SVN_ERR_FS_CORRUPT, err,
                                 _("Number '%s' invalid or too large"),
                                 sb->data);
    }

  return SVN_NO_ERROR;
}

/* Given REV in FS, set *REV_OFFSET to REV's offset in the packed file.
   Use POOL for temporary allocations. */
static svn_error_t *
get_packed_offset(apr_off_t *rev_offset,
                  svn_fs_t *fs,
                  svn_revnum_t rev,
                  apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  svn_stream_t *manifest_stream;
  svn_boolean_t is_cached;
  svn_revnum_t shard;
  apr_int64_t shard_pos;
  apr_array_header_t *manifest;
  apr_pool_t *iterpool;

  shard = rev / ffd->max_files_per_dir;

  /* position of the shard within the manifest */
  shard_pos = rev % ffd->max_files_per_dir;

  /* fetch exactly that element into *rev_offset, if the manifest is found
     in the cache */
  SVN_ERR(svn_cache__get_partial((void **) rev_offset, &is_cached,
                                 ffd->packed_offset_cache, &shard,
                                 svn_fs_fs__get_sharded_offset, &shard_pos,
                                 pool));

  if (is_cached)
      return SVN_NO_ERROR;

  /* Open the manifest file. */
  SVN_ERR(svn_stream_open_readonly(&manifest_stream,
                                   path_rev_packed(fs, rev, PATH_MANIFEST,
                                                   pool),
                                   pool, pool));

  /* While we're here, let's just read the entire manifest file into an array,
     so we can cache the entire thing. */
  iterpool = svn_pool_create(pool);
  manifest = apr_array_make(pool, ffd->max_files_per_dir, sizeof(apr_off_t));
  while (1)
    {
      svn_boolean_t eof;
      apr_int64_t val;

      svn_pool_clear(iterpool);
      SVN_ERR(read_number_from_stream(&val, &eof, manifest_stream, iterpool));
      if (eof)
        break;

      APR_ARRAY_PUSH(manifest, apr_off_t) = (apr_off_t)val;
    }
  svn_pool_destroy(iterpool);

  *rev_offset = APR_ARRAY_IDX(manifest, rev % ffd->max_files_per_dir,
                              apr_off_t);

  /* Close up shop and cache the array. */
  SVN_ERR(svn_stream_close(manifest_stream));
  return svn_cache__set(ffd->packed_offset_cache, &shard, manifest, pool);
}

/* Open the revision file for revision REV in filesystem FS and store
   the newly opened file in FILE.  Seek to location OFFSET before
   returning.  Perform temporary allocations in POOL. */
static svn_error_t *
open_and_seek_revision(apr_file_t **file,
                       svn_fs_t *fs,
                       svn_revnum_t rev,
                       apr_off_t offset,
                       apr_pool_t *pool)
{
  apr_file_t *rev_file;

  SVN_ERR(ensure_revision_exists(fs, rev, pool));

  SVN_ERR(open_pack_or_rev_file(&rev_file, fs, rev, pool));

  if (is_packed_rev(fs, rev))
    {
      apr_off_t rev_offset;

      SVN_ERR(get_packed_offset(&rev_offset, fs, rev, pool));
      offset += rev_offset;
    }

  SVN_ERR(svn_io_file_seek(rev_file, APR_SET, &offset, pool));

  *file = rev_file;

  return SVN_NO_ERROR;
}

/* Open the representation for a node-revision in transaction TXN_ID
   in filesystem FS and store the newly opened file in FILE.  Seek to
   location OFFSET before returning.  Perform temporary allocations in
   POOL.  Only appropriate for file contents, nor props or directory
   contents. */
static svn_error_t *
open_and_seek_transaction(apr_file_t **file,
                          svn_fs_t *fs,
                          const char *txn_id,
                          representation_t *rep,
                          apr_pool_t *pool)
{
  apr_file_t *rev_file;
  apr_off_t offset;

  SVN_ERR(svn_io_file_open(&rev_file, path_txn_proto_rev(fs, txn_id, pool),
                           APR_READ | APR_BUFFERED, APR_OS_DEFAULT, pool));

  offset = rep->offset;
  SVN_ERR(svn_io_file_seek(rev_file, APR_SET, &offset, pool));

  *file = rev_file;

  return SVN_NO_ERROR;
}

/* Given a node-id ID, and a representation REP in filesystem FS, open
   the correct file and seek to the correction location.  Store this
   file in *FILE_P.  Perform any allocations in POOL. */
static svn_error_t *
open_and_seek_representation(apr_file_t **file_p,
                             svn_fs_t *fs,
                             representation_t *rep,
                             apr_pool_t *pool)
{
  if (! rep->txn_id)
    return open_and_seek_revision(file_p, fs, rep->revision, rep->offset,
                                  pool);
  else
    return open_and_seek_transaction(file_p, fs, rep->txn_id, rep, pool);
}

/* Parse the description of a representation from STRING and store it
   into *REP_P.  If the representation is mutable (the revision is
   given as -1), then use TXN_ID for the representation's txn_id
   field.  If MUTABLE_REP_TRUNCATED is true, then this representation
   is for property or directory contents, and no information will be
   expected except the "-1" revision number for a mutable
   representation.  Allocate *REP_P in POOL. */
static svn_error_t *
read_rep_offsets_body(representation_t **rep_p,
                      char *string,
                      const char *txn_id,
                      svn_boolean_t mutable_rep_truncated,
                      apr_pool_t *pool)
{
  representation_t *rep;
  char *str;
  apr_int64_t val;

  rep = apr_pcalloc(pool, sizeof(*rep));
  *rep_p = rep;

  str = svn_cstring_tokenize(" ", &string);
  if (str == NULL)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Malformed text representation offset line in node-rev"));


  rep->revision = SVN_STR_TO_REV(str);
  if (rep->revision == SVN_INVALID_REVNUM)
    {
      rep->txn_id = txn_id;
      if (mutable_rep_truncated)
        return SVN_NO_ERROR;
    }

  str = svn_cstring_tokenize(" ", &string);
  if (str == NULL)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Malformed text representation offset line in node-rev"));

  SVN_ERR(svn_cstring_atoi64(&val, str));
  rep->offset = (apr_off_t)val;

  str = svn_cstring_tokenize(" ", &string);
  if (str == NULL)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Malformed text representation offset line in node-rev"));

  SVN_ERR(svn_cstring_atoi64(&val, str));
  rep->size = (svn_filesize_t)val;

  str = svn_cstring_tokenize(" ", &string);
  if (str == NULL)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Malformed text representation offset line in node-rev"));

  SVN_ERR(svn_cstring_atoi64(&val, str));
  rep->expanded_size = (svn_filesize_t)val;

  /* Read in the MD5 hash. */
  str = svn_cstring_tokenize(" ", &string);
  if ((str == NULL) || (strlen(str) != (APR_MD5_DIGESTSIZE * 2)))
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Malformed text representation offset line in node-rev"));

  SVN_ERR(svn_checksum_parse_hex(&rep->md5_checksum, svn_checksum_md5, str,
                                 pool));

  /* The remaining fields are only used for formats >= 4, so check that. */
  str = svn_cstring_tokenize(" ", &string);
  if (str == NULL)
    return SVN_NO_ERROR;

  /* Read the SHA1 hash. */
  if (strlen(str) != (APR_SHA1_DIGESTSIZE * 2))
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Malformed text representation offset line in node-rev"));

  SVN_ERR(svn_checksum_parse_hex(&rep->sha1_checksum, svn_checksum_sha1, str,
                                 pool));

  /* Read the uniquifier. */
  str = svn_cstring_tokenize(" ", &string);
  if (str == NULL)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Malformed text representation offset line in node-rev"));

  rep->uniquifier = apr_pstrdup(pool, str);

  return SVN_NO_ERROR;
}

/* Wrap read_rep_offsets_body(), extracting its TXN_ID from our NODEREV_ID,
   and adding an error message. */
static svn_error_t *
read_rep_offsets(representation_t **rep_p,
                 char *string,
                 const svn_fs_id_t *noderev_id,
                 svn_boolean_t mutable_rep_truncated,
                 apr_pool_t *pool)
{
  svn_error_t *err;
  const char *txn_id;

  if (noderev_id)
    txn_id = svn_fs_fs__id_txn_id(noderev_id);
  else
    txn_id = NULL;

  err = read_rep_offsets_body(rep_p, string, txn_id, mutable_rep_truncated,
                              pool);
  if (err)
    {
      const svn_string_t *id_unparsed = svn_fs_fs__id_unparse(noderev_id, pool);
      const char *where;
      where = apr_psprintf(pool,
                           _("While reading representation offsets "
                             "for node-revision '%s':"),
                           noderev_id ? id_unparsed->data : "(null)");

      return svn_error_quick_wrap(err, where);
    }
  else
    return SVN_NO_ERROR;
}

static svn_error_t *
err_dangling_id(svn_fs_t *fs, const svn_fs_id_t *id)
{
  svn_string_t *id_str = svn_fs_fs__id_unparse(id, fs->pool);
  return svn_error_createf
    (SVN_ERR_FS_ID_NOT_FOUND, 0,
     _("Reference to non-existent node '%s' in filesystem '%s'"),
     id_str->data, fs->path);
}

/* Look up the NODEREV_P for ID in FS' node revsion cache. If noderev
 * caching has been enabled and the data can be found, IS_CACHED will
 * be set to TRUE. The noderev will be allocated from POOL.
 *
 * Non-permanent ids (e.g. ids within a TXN) will not be cached.
 */
static svn_error_t *
get_cached_node_revision_body(node_revision_t **noderev_p,
                              svn_fs_t *fs,
                              const svn_fs_id_t *id,
                              svn_boolean_t *is_cached,
                              apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  if (! ffd->node_revision_cache || svn_fs_fs__id_txn_id(id))
    {
      *is_cached = FALSE;
    }
  else
    {
      pair_cache_key_t key = { 0 };

      key.revision = svn_fs_fs__id_rev(id);
      key.second = svn_fs_fs__id_offset(id);
      SVN_ERR(svn_cache__get((void **) noderev_p,
                            is_cached,
                            ffd->node_revision_cache,
                            &key,
                            pool));
    }

  return SVN_NO_ERROR;
}

/* If noderev caching has been enabled, store the NODEREV_P for the given ID
 * in FS' node revsion cache. SCRATCH_POOL is used for temporary allcations.
 *
 * Non-permanent ids (e.g. ids within a TXN) will not be cached.
 */
static svn_error_t *
set_cached_node_revision_body(node_revision_t *noderev_p,
                              svn_fs_t *fs,
                              const svn_fs_id_t *id,
                              apr_pool_t *scratch_pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  if (ffd->node_revision_cache && !svn_fs_fs__id_txn_id(id))
    {
      pair_cache_key_t key = { 0 };

      key.revision = svn_fs_fs__id_rev(id);
      key.second = svn_fs_fs__id_offset(id);
      return svn_cache__set(ffd->node_revision_cache,
                            &key,
                            noderev_p,
                            scratch_pool);
    }

  return SVN_NO_ERROR;
}

/* Get the node-revision for the node ID in FS.
   Set *NODEREV_P to the new node-revision structure, allocated in POOL.
   See svn_fs_fs__get_node_revision, which wraps this and adds another
   error. */
static svn_error_t *
get_node_revision_body(node_revision_t **noderev_p,
                       svn_fs_t *fs,
                       const svn_fs_id_t *id,
                       apr_pool_t *pool)
{
  apr_file_t *revision_file;
  svn_error_t *err;
  svn_boolean_t is_cached = FALSE;

  /* First, try a cache lookup. If that succeeds, we are done here. */
  SVN_ERR(get_cached_node_revision_body(noderev_p, fs, id, &is_cached, pool));
  if (is_cached)
    return SVN_NO_ERROR;

  if (svn_fs_fs__id_txn_id(id))
    {
      /* This is a transaction node-rev. */
      err = svn_io_file_open(&revision_file, path_txn_node_rev(fs, id, pool),
                             APR_READ | APR_BUFFERED, APR_OS_DEFAULT, pool);
    }
  else
    {
      /* This is a revision node-rev. */
      err = open_and_seek_revision(&revision_file, fs,
                                   svn_fs_fs__id_rev(id),
                                   svn_fs_fs__id_offset(id),
                                   pool);
    }

  if (err)
    {
      if (APR_STATUS_IS_ENOENT(err->apr_err))
        {
          svn_error_clear(err);
          return svn_error_trace(err_dangling_id(fs, id));
        }

      return svn_error_trace(err);
    }

  SVN_ERR(svn_fs_fs__read_noderev(noderev_p,
                                  svn_stream_from_aprfile2(revision_file, FALSE,
                                                           pool),
                                  pool));

  /* The noderev is not in cache, yet. Add it, if caching has been enabled. */
  return set_cached_node_revision_body(*noderev_p, fs, id, pool);
}

svn_error_t *
svn_fs_fs__read_noderev(node_revision_t **noderev_p,
                        svn_stream_t *stream,
                        apr_pool_t *pool)
{
  apr_hash_t *headers;
  node_revision_t *noderev;
  char *value;
  const char *noderev_id;

  SVN_ERR(read_header_block(&headers, stream, pool));

  noderev = apr_pcalloc(pool, sizeof(*noderev));

  /* Read the node-rev id. */
  value = svn_hash_gets(headers, HEADER_ID);
  if (value == NULL)
      /* ### More information: filename/offset coordinates */
      return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                              _("Missing id field in node-rev"));

  SVN_ERR(svn_stream_close(stream));

  noderev->id = svn_fs_fs__id_parse(value, strlen(value), pool);
  noderev_id = value; /* for error messages later */

  /* Read the type. */
  value = svn_hash_gets(headers, HEADER_TYPE);

  if ((value == NULL) ||
      (strcmp(value, KIND_FILE) != 0 && strcmp(value, KIND_DIR)))
    /* ### s/kind/type/ */
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Missing kind field in node-rev '%s'"),
                             noderev_id);

  noderev->kind = (strcmp(value, KIND_FILE) == 0) ? svn_node_file
    : svn_node_dir;

  /* Read the 'count' field. */
  value = svn_hash_gets(headers, HEADER_COUNT);
  if (value)
    SVN_ERR(svn_cstring_atoi(&noderev->predecessor_count, value));
  else
    noderev->predecessor_count = 0;

  /* Get the properties location. */
  value = svn_hash_gets(headers, HEADER_PROPS);
  if (value)
    {
      SVN_ERR(read_rep_offsets(&noderev->prop_rep, value,
                               noderev->id, TRUE, pool));
    }

  /* Get the data location. */
  value = svn_hash_gets(headers, HEADER_TEXT);
  if (value)
    {
      SVN_ERR(read_rep_offsets(&noderev->data_rep, value,
                               noderev->id,
                               (noderev->kind == svn_node_dir), pool));
    }

  /* Get the created path. */
  value = svn_hash_gets(headers, HEADER_CPATH);
  if (value == NULL)
    {
      return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                               _("Missing cpath field in node-rev '%s'"),
                               noderev_id);
    }
  else
    {
      noderev->created_path = apr_pstrdup(pool, value);
    }

  /* Get the predecessor ID. */
  value = svn_hash_gets(headers, HEADER_PRED);
  if (value)
    noderev->predecessor_id = svn_fs_fs__id_parse(value, strlen(value),
                                                  pool);

  /* Get the copyroot. */
  value = svn_hash_gets(headers, HEADER_COPYROOT);
  if (value == NULL)
    {
      noderev->copyroot_path = apr_pstrdup(pool, noderev->created_path);
      noderev->copyroot_rev = svn_fs_fs__id_rev(noderev->id);
    }
  else
    {
      char *str;

      str = svn_cstring_tokenize(" ", &value);
      if (str == NULL)
        return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                 _("Malformed copyroot line in node-rev '%s'"),
                                 noderev_id);

      noderev->copyroot_rev = SVN_STR_TO_REV(str);

      if (*value == '\0')
        return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                 _("Malformed copyroot line in node-rev '%s'"),
                                 noderev_id);
      noderev->copyroot_path = apr_pstrdup(pool, value);
    }

  /* Get the copyfrom. */
  value = svn_hash_gets(headers, HEADER_COPYFROM);
  if (value == NULL)
    {
      noderev->copyfrom_path = NULL;
      noderev->copyfrom_rev = SVN_INVALID_REVNUM;
    }
  else
    {
      char *str = svn_cstring_tokenize(" ", &value);
      if (str == NULL)
        return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                 _("Malformed copyfrom line in node-rev '%s'"),
                                 noderev_id);

      noderev->copyfrom_rev = SVN_STR_TO_REV(str);

      if (*value == 0)
        return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                 _("Malformed copyfrom line in node-rev '%s'"),
                                 noderev_id);
      noderev->copyfrom_path = apr_pstrdup(pool, value);
    }

  /* Get whether this is a fresh txn root. */
  value = svn_hash_gets(headers, HEADER_FRESHTXNRT);
  noderev->is_fresh_txn_root = (value != NULL);

  /* Get the mergeinfo count. */
  value = svn_hash_gets(headers, HEADER_MINFO_CNT);
  if (value)
    SVN_ERR(svn_cstring_atoi64(&noderev->mergeinfo_count, value));
  else
    noderev->mergeinfo_count = 0;

  /* Get whether *this* node has mergeinfo. */
  value = svn_hash_gets(headers, HEADER_MINFO_HERE);
  noderev->has_mergeinfo = (value != NULL);

  *noderev_p = noderev;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__get_node_revision(node_revision_t **noderev_p,
                             svn_fs_t *fs,
                             const svn_fs_id_t *id,
                             apr_pool_t *pool)
{
  svn_error_t *err = get_node_revision_body(noderev_p, fs, id, pool);
  if (err && err->apr_err == SVN_ERR_FS_CORRUPT)
    {
      svn_string_t *id_string = svn_fs_fs__id_unparse(id, pool);
      return svn_error_createf(SVN_ERR_FS_CORRUPT, err,
                               "Corrupt node-revision '%s'",
                               id_string->data);
    }
  return svn_error_trace(err);
}


/* Return a formatted string, compatible with filesystem format FORMAT,
   that represents the location of representation REP.  If
   MUTABLE_REP_TRUNCATED is given, the rep is for props or dir contents,
   and only a "-1" revision number will be given for a mutable rep.
   If MAY_BE_CORRUPT is true, guard for NULL when constructing the string.
   Perform the allocation from POOL.  */
static const char *
representation_string(representation_t *rep,
                      int format,
                      svn_boolean_t mutable_rep_truncated,
                      svn_boolean_t may_be_corrupt,
                      apr_pool_t *pool)
{
  if (rep->txn_id && mutable_rep_truncated)
    return "-1";

#define DISPLAY_MAYBE_NULL_CHECKSUM(checksum)          \
  ((!may_be_corrupt || (checksum) != NULL)     \
   ? svn_checksum_to_cstring_display((checksum), pool) \
   : "(null)")

  if (format < SVN_FS_FS__MIN_REP_SHARING_FORMAT || rep->sha1_checksum == NULL)
    return apr_psprintf(pool, "%ld %" APR_OFF_T_FMT " %" SVN_FILESIZE_T_FMT
                        " %" SVN_FILESIZE_T_FMT " %s",
                        rep->revision, rep->offset, rep->size,
                        rep->expanded_size,
                        DISPLAY_MAYBE_NULL_CHECKSUM(rep->md5_checksum));

  return apr_psprintf(pool, "%ld %" APR_OFF_T_FMT " %" SVN_FILESIZE_T_FMT
                      " %" SVN_FILESIZE_T_FMT " %s %s %s",
                      rep->revision, rep->offset, rep->size,
                      rep->expanded_size,
                      DISPLAY_MAYBE_NULL_CHECKSUM(rep->md5_checksum),
                      DISPLAY_MAYBE_NULL_CHECKSUM(rep->sha1_checksum),
                      rep->uniquifier);

#undef DISPLAY_MAYBE_NULL_CHECKSUM

}


svn_error_t *
svn_fs_fs__write_noderev(svn_stream_t *outfile,
                         node_revision_t *noderev,
                         int format,
                         svn_boolean_t include_mergeinfo,
                         apr_pool_t *pool)
{
  SVN_ERR(svn_stream_printf(outfile, pool, HEADER_ID ": %s\n",
                            svn_fs_fs__id_unparse(noderev->id,
                                                  pool)->data));

  SVN_ERR(svn_stream_printf(outfile, pool, HEADER_TYPE ": %s\n",
                            (noderev->kind == svn_node_file) ?
                            KIND_FILE : KIND_DIR));

  if (noderev->predecessor_id)
    SVN_ERR(svn_stream_printf(outfile, pool, HEADER_PRED ": %s\n",
                              svn_fs_fs__id_unparse(noderev->predecessor_id,
                                                    pool)->data));

  SVN_ERR(svn_stream_printf(outfile, pool, HEADER_COUNT ": %d\n",
                            noderev->predecessor_count));

  if (noderev->data_rep)
    SVN_ERR(svn_stream_printf(outfile, pool, HEADER_TEXT ": %s\n",
                              representation_string(noderev->data_rep,
                                                    format,
                                                    (noderev->kind
                                                     == svn_node_dir),
                                                    FALSE,
                                                    pool)));

  if (noderev->prop_rep)
    SVN_ERR(svn_stream_printf(outfile, pool, HEADER_PROPS ": %s\n",
                              representation_string(noderev->prop_rep, format,
                                                    TRUE, FALSE, pool)));

  SVN_ERR(svn_stream_printf(outfile, pool, HEADER_CPATH ": %s\n",
                            noderev->created_path));

  if (noderev->copyfrom_path)
    SVN_ERR(svn_stream_printf(outfile, pool, HEADER_COPYFROM ": %ld"
                              " %s\n",
                              noderev->copyfrom_rev,
                              noderev->copyfrom_path));

  if ((noderev->copyroot_rev != svn_fs_fs__id_rev(noderev->id)) ||
      (strcmp(noderev->copyroot_path, noderev->created_path) != 0))
    SVN_ERR(svn_stream_printf(outfile, pool, HEADER_COPYROOT ": %ld"
                              " %s\n",
                              noderev->copyroot_rev,
                              noderev->copyroot_path));

  if (noderev->is_fresh_txn_root)
    SVN_ERR(svn_stream_puts(outfile, HEADER_FRESHTXNRT ": y\n"));

  if (include_mergeinfo)
    {
      if (noderev->mergeinfo_count > 0)
        SVN_ERR(svn_stream_printf(outfile, pool, HEADER_MINFO_CNT ": %"
                                  APR_INT64_T_FMT "\n",
                                  noderev->mergeinfo_count));

      if (noderev->has_mergeinfo)
        SVN_ERR(svn_stream_puts(outfile, HEADER_MINFO_HERE ": y\n"));
    }

  return svn_stream_puts(outfile, "\n");
}

svn_error_t *
svn_fs_fs__put_node_revision(svn_fs_t *fs,
                             const svn_fs_id_t *id,
                             node_revision_t *noderev,
                             svn_boolean_t fresh_txn_root,
                             apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  apr_file_t *noderev_file;
  const char *txn_id = svn_fs_fs__id_txn_id(id);

  noderev->is_fresh_txn_root = fresh_txn_root;

  if (! txn_id)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Attempted to write to non-transaction '%s'"),
                             svn_fs_fs__id_unparse(id, pool)->data);

  SVN_ERR(svn_io_file_open(&noderev_file, path_txn_node_rev(fs, id, pool),
                           APR_WRITE | APR_CREATE | APR_TRUNCATE
                           | APR_BUFFERED, APR_OS_DEFAULT, pool));

  SVN_ERR(svn_fs_fs__write_noderev(svn_stream_from_aprfile2(noderev_file, TRUE,
                                                            pool),
                                   noderev, ffd->format,
                                   svn_fs_fs__fs_supports_mergeinfo(fs),
                                   pool));

  SVN_ERR(svn_io_file_close(noderev_file, pool));

  return SVN_NO_ERROR;
}

/* For the in-transaction NODEREV within FS, write the sha1->rep mapping
 * file in the respective transaction, if rep sharing has been enabled etc.
 * Use POOL for temporary allocations.
 */
static svn_error_t *
store_sha1_rep_mapping(svn_fs_t *fs,
                       node_revision_t *noderev,
                       apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  /* if rep sharing has been enabled and the noderev has a data rep and
   * its SHA-1 is known, store the rep struct under its SHA1. */
  if (   ffd->rep_sharing_allowed
      && noderev->data_rep
      && noderev->data_rep->sha1_checksum)
    {
      apr_file_t *rep_file;
      const char *file_name = path_txn_sha1(fs,
                                            svn_fs_fs__id_txn_id(noderev->id),
                                            noderev->data_rep->sha1_checksum,
                                            pool);
      const char *rep_string = representation_string(noderev->data_rep,
                                                     ffd->format,
                                                     (noderev->kind
                                                      == svn_node_dir),
                                                     FALSE,
                                                     pool);
      SVN_ERR(svn_io_file_open(&rep_file, file_name,
                               APR_WRITE | APR_CREATE | APR_TRUNCATE
                               | APR_BUFFERED, APR_OS_DEFAULT, pool));

      SVN_ERR(svn_io_file_write_full(rep_file, rep_string,
                                     strlen(rep_string), NULL, pool));

      SVN_ERR(svn_io_file_close(rep_file, pool));
    }

  return SVN_NO_ERROR;
}


/* This structure is used to hold the information associated with a
   REP line. */
struct rep_args
{
  svn_boolean_t is_delta;
  svn_boolean_t is_delta_vs_empty;

  svn_revnum_t base_revision;
  apr_off_t base_offset;
  svn_filesize_t base_length;
};

/* Read the next line from file FILE and parse it as a text
   representation entry.  Return the parsed entry in *REP_ARGS_P.
   Perform all allocations in POOL. */
static svn_error_t *
read_rep_line(struct rep_args **rep_args_p,
              apr_file_t *file,
              apr_pool_t *pool)
{
  char buffer[160];
  apr_size_t limit;
  struct rep_args *rep_args;
  char *str, *last_str = buffer;
  apr_int64_t val;

  limit = sizeof(buffer);
  SVN_ERR(svn_io_read_length_line(file, buffer, &limit, pool));

  rep_args = apr_pcalloc(pool, sizeof(*rep_args));
  rep_args->is_delta = FALSE;

  if (strcmp(buffer, REP_PLAIN) == 0)
    {
      *rep_args_p = rep_args;
      return SVN_NO_ERROR;
    }

  if (strcmp(buffer, REP_DELTA) == 0)
    {
      /* This is a delta against the empty stream. */
      rep_args->is_delta = TRUE;
      rep_args->is_delta_vs_empty = TRUE;
      *rep_args_p = rep_args;
      return SVN_NO_ERROR;
    }

  rep_args->is_delta = TRUE;
  rep_args->is_delta_vs_empty = FALSE;

  /* We have hopefully a DELTA vs. a non-empty base revision. */
  str = svn_cstring_tokenize(" ", &last_str);
  if (! str || (strcmp(str, REP_DELTA) != 0))
    goto error;

  str = svn_cstring_tokenize(" ", &last_str);
  if (! str)
    goto error;
  rep_args->base_revision = SVN_STR_TO_REV(str);

  str = svn_cstring_tokenize(" ", &last_str);
  if (! str)
    goto error;
  SVN_ERR(svn_cstring_atoi64(&val, str));
  rep_args->base_offset = (apr_off_t)val;

  str = svn_cstring_tokenize(" ", &last_str);
  if (! str)
    goto error;
  SVN_ERR(svn_cstring_atoi64(&val, str));
  rep_args->base_length = (svn_filesize_t)val;

  *rep_args_p = rep_args;
  return SVN_NO_ERROR;

 error:
  return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                           _("Malformed representation header at %s"),
                           path_and_offset_of(file, pool));
}

/* Given a revision file REV_FILE, opened to REV in FS, find the Node-ID
   of the header located at OFFSET and store it in *ID_P.  Allocate
   temporary variables from POOL. */
static svn_error_t *
get_fs_id_at_offset(svn_fs_id_t **id_p,
                    apr_file_t *rev_file,
                    svn_fs_t *fs,
                    svn_revnum_t rev,
                    apr_off_t offset,
                    apr_pool_t *pool)
{
  svn_fs_id_t *id;
  apr_hash_t *headers;
  const char *node_id_str;

  SVN_ERR(svn_io_file_seek(rev_file, APR_SET, &offset, pool));

  SVN_ERR(read_header_block(&headers,
                            svn_stream_from_aprfile2(rev_file, TRUE, pool),
                            pool));

  /* In error messages, the offset is relative to the pack file,
     not to the rev file. */

  node_id_str = svn_hash_gets(headers, HEADER_ID);

  if (node_id_str == NULL)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Missing node-id in node-rev at r%ld "
                             "(offset %s)"),
                             rev,
                             apr_psprintf(pool, "%" APR_OFF_T_FMT, offset));

  id = svn_fs_fs__id_parse(node_id_str, strlen(node_id_str), pool);

  if (id == NULL)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Corrupt node-id '%s' in node-rev at r%ld "
                               "(offset %s)"),
                             node_id_str, rev,
                             apr_psprintf(pool, "%" APR_OFF_T_FMT, offset));

  *id_p = id;

  /* ### assert that the txn_id is REV/OFFSET ? */

  return SVN_NO_ERROR;
}


/* Given an open revision file REV_FILE in FS for REV, locate the trailer that
   specifies the offset to the root node-id and to the changed path
   information.  Store the root node offset in *ROOT_OFFSET and the
   changed path offset in *CHANGES_OFFSET.  If either of these
   pointers is NULL, do nothing with it.

   If PACKED is true, REV_FILE should be a packed shard file.
   ### There is currently no such parameter.  This function assumes that
       is_packed_rev(FS, REV) will indicate whether REV_FILE is a packed
       file.  Therefore FS->fsap_data->min_unpacked_rev must not have been
       refreshed since REV_FILE was opened if there is a possibility that
       revision REV may have become packed since then.
       TODO: Take an IS_PACKED parameter instead, in order to remove this
       requirement.

   Allocate temporary variables from POOL. */
static svn_error_t *
get_root_changes_offset(apr_off_t *root_offset,
                        apr_off_t *changes_offset,
                        apr_file_t *rev_file,
                        svn_fs_t *fs,
                        svn_revnum_t rev,
                        apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  apr_off_t offset;
  apr_off_t rev_offset;
  char buf[64];
  int i, num_bytes;
  const char *str;
  apr_size_t len;
  apr_seek_where_t seek_relative;

  /* Determine where to seek to in the file.

     If we've got a pack file, we want to seek to the end of the desired
     revision.  But we don't track that, so we seek to the beginning of the
     next revision.

     Unless the next revision is in a different file, in which case, we can
     just seek to the end of the pack file -- just like we do in the
     non-packed case. */
  if (is_packed_rev(fs, rev) && ((rev + 1) % ffd->max_files_per_dir != 0))
    {
      SVN_ERR(get_packed_offset(&offset, fs, rev + 1, pool));
      seek_relative = APR_SET;
    }
  else
    {
      seek_relative = APR_END;
      offset = 0;
    }

  /* Offset of the revision from the start of the pack file, if applicable. */
  if (is_packed_rev(fs, rev))
    SVN_ERR(get_packed_offset(&rev_offset, fs, rev, pool));
  else
    rev_offset = 0;

  /* We will assume that the last line containing the two offsets
     will never be longer than 64 characters. */
  SVN_ERR(svn_io_file_seek(rev_file, seek_relative, &offset, pool));

  offset -= sizeof(buf);
  SVN_ERR(svn_io_file_seek(rev_file, APR_SET, &offset, pool));

  /* Read in this last block, from which we will identify the last line. */
  len = sizeof(buf);
  SVN_ERR(svn_io_file_read(rev_file, buf, &len, pool));

  /* This cast should be safe since the maximum amount read, 64, will
     never be bigger than the size of an int. */
  num_bytes = (int) len;

  /* The last byte should be a newline. */
  if (buf[num_bytes - 1] != '\n')
    {
      return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                               _("Revision file (r%ld) lacks trailing newline"),
                               rev);
    }

  /* Look for the next previous newline. */
  for (i = num_bytes - 2; i >= 0; i--)
    {
      if (buf[i] == '\n')
        break;
    }

  if (i < 0)
    {
      return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                               _("Final line in revision file (r%ld) longer "
                                 "than 64 characters"),
                               rev);
    }

  i++;
  str = &buf[i];

  /* find the next space */
  for ( ; i < (num_bytes - 2) ; i++)
    if (buf[i] == ' ')
      break;

  if (i == (num_bytes - 2))
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Final line in revision file r%ld missing space"),
                             rev);

  if (root_offset)
    {
      apr_int64_t val;

      buf[i] = '\0';
      SVN_ERR(svn_cstring_atoi64(&val, str));
      *root_offset = rev_offset + (apr_off_t)val;
    }

  i++;
  str = &buf[i];

  /* find the next newline */
  for ( ; i < num_bytes; i++)
    if (buf[i] == '\n')
      break;

  if (changes_offset)
    {
      apr_int64_t val;

      buf[i] = '\0';
      SVN_ERR(svn_cstring_atoi64(&val, str));
      *changes_offset = rev_offset + (apr_off_t)val;
    }

  return SVN_NO_ERROR;
}

/* Move a file into place from OLD_FILENAME in the transactions
   directory to its final location NEW_FILENAME in the repository.  On
   Unix, match the permissions of the new file to the permissions of
   PERMS_REFERENCE.  Temporary allocations are from POOL.

   This function almost duplicates svn_io_file_move(), but it tries to
   guarantee a flush. */
static svn_error_t *
move_into_place(const char *old_filename,
                const char *new_filename,
                const char *perms_reference,
                apr_pool_t *pool)
{
  svn_error_t *err;

  SVN_ERR(svn_io_copy_perms(perms_reference, old_filename, pool));

  /* Move the file into place. */
  err = svn_io_file_rename(old_filename, new_filename, pool);
  if (err && APR_STATUS_IS_EXDEV(err->apr_err))
    {
      apr_file_t *file;

      /* Can't rename across devices; fall back to copying. */
      svn_error_clear(err);
      err = SVN_NO_ERROR;
      SVN_ERR(svn_io_copy_file(old_filename, new_filename, TRUE, pool));

      /* Flush the target of the copy to disk. */
      SVN_ERR(svn_io_file_open(&file, new_filename, APR_READ,
                               APR_OS_DEFAULT, pool));
      /* ### BH: Does this really guarantee a flush of the data written
         ### via a completely different handle on all operating systems?
         ###
         ### Maybe we should perform the copy ourselves instead of making
         ### apr do that and flush the real handle? */
      SVN_ERR(svn_io_file_flush_to_disk(file, pool));
      SVN_ERR(svn_io_file_close(file, pool));
    }
  if (err)
    return svn_error_trace(err);

#ifdef __linux__
  {
    /* Linux has the unusual feature that fsync() on a file is not
       enough to ensure that a file's directory entries have been
       flushed to disk; you have to fsync the directory as well.
       On other operating systems, we'd only be asking for trouble
       by trying to open and fsync a directory. */
    const char *dirname;
    apr_file_t *file;

    dirname = svn_dirent_dirname(new_filename, pool);
    SVN_ERR(svn_io_file_open(&file, dirname, APR_READ, APR_OS_DEFAULT,
                             pool));
    SVN_ERR(svn_io_file_flush_to_disk(file, pool));
    SVN_ERR(svn_io_file_close(file, pool));
  }
#endif

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__rev_get_root(svn_fs_id_t **root_id_p,
                        svn_fs_t *fs,
                        svn_revnum_t rev,
                        apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  apr_file_t *revision_file;
  apr_off_t root_offset;
  svn_fs_id_t *root_id = NULL;
  svn_boolean_t is_cached;

  SVN_ERR(ensure_revision_exists(fs, rev, pool));

  SVN_ERR(svn_cache__get((void **) root_id_p, &is_cached,
                         ffd->rev_root_id_cache, &rev, pool));
  if (is_cached)
    return SVN_NO_ERROR;

  SVN_ERR(open_pack_or_rev_file(&revision_file, fs, rev, pool));
  SVN_ERR(get_root_changes_offset(&root_offset, NULL, revision_file, fs, rev,
                                  pool));

  SVN_ERR(get_fs_id_at_offset(&root_id, revision_file, fs, rev,
                              root_offset, pool));

  SVN_ERR(svn_io_file_close(revision_file, pool));

  SVN_ERR(svn_cache__set(ffd->rev_root_id_cache, &rev, root_id, pool));

  *root_id_p = root_id;

  return SVN_NO_ERROR;
}

/* Revprop caching management.
 *
 * Mechanism:
 * ----------
 *
 * Revprop caching needs to be activated and will be deactivated for the
 * respective FS instance if the necessary infrastructure could not be
 * initialized.  In deactivated mode, there is almost no runtime overhead
 * associated with revprop caching.  As long as no revprops are being read
 * or changed, revprop caching imposes no overhead.
 *
 * When activated, we cache revprops using (revision, generation) pairs
 * as keys with the generation being incremented upon every revprop change.
 * Since the cache is process-local, the generation needs to be tracked
 * for at least as long as the process lives but may be reset afterwards.
 *
 * To track the revprop generation, we use two-layer approach. On the lower
 * level, we use named atomics to have a system-wide consistent value for
 * the current revprop generation.  However, those named atomics will only
 * remain valid for as long as at least one process / thread in the system
 * accesses revprops in the respective repository.  The underlying shared
 * memory gets cleaned up afterwards.
 *
 * On the second level, we will use a persistent file to track the latest
 * revprop generation.  It will be written upon each revprop change but
 * only be read if we are the first process to initialize the named atomics
 * with that value.
 *
 * The overhead for the second and following accesses to revprops is
 * almost zero on most systems.
 *
 *
 * Tech aspects:
 * -------------
 *
 * A problem is that we need to provide a globally available file name to
 * back the SHM implementation on OSes that need it.  We can only assume
 * write access to some file within the respective repositories.  Because
 * a given server process may access thousands of repositories during its
 * lifetime, keeping the SHM data alive for all of them is also not an
 * option.
 *
 * So, we store the new revprop generation on disk as part of each
 * setrevprop call, i.e. this write will be serialized and the write order
 * be guaranteed by the repository write lock.
 *
 * The only racy situation occurs when the data is being read again by two
 * processes concurrently but in that situation, the first process to
 * finish that procedure is guaranteed to be the only one that initializes
 * the SHM data.  Since even writers will first go through that
 * initialization phase, they will never operate on stale data.
 */

/* Read revprop generation as stored on disk for repository FS. The result
 * is returned in *CURRENT. Default to 2 if no such file is available.
 */
static svn_error_t *
read_revprop_generation_file(apr_int64_t *current,
                             svn_fs_t *fs,
                             apr_pool_t *pool)
{
  svn_error_t *err;
  apr_file_t *file;
  char buf[80];
  apr_size_t len;
  const char *path = path_revprop_generation(fs, pool);

  err = svn_io_file_open(&file, path,
                         APR_READ | APR_BUFFERED,
                         APR_OS_DEFAULT, pool);
  if (err && APR_STATUS_IS_ENOENT(err->apr_err))
    {
      svn_error_clear(err);
      *current = 2;

      return SVN_NO_ERROR;
    }
  SVN_ERR(err);

  len = sizeof(buf);
  SVN_ERR(svn_io_read_length_line(file, buf, &len, pool));

  /* Check that the first line contains only digits. */
  SVN_ERR(check_file_buffer_numeric(buf, 0, path,
                                    "Revprop Generation", pool));
  SVN_ERR(svn_cstring_atoi64(current, buf));

  return svn_io_file_close(file, pool);
}

/* Write the CURRENT revprop generation to disk for repository FS.
 */
static svn_error_t *
write_revprop_generation_file(svn_fs_t *fs,
                              apr_int64_t current,
                              apr_pool_t *pool)
{
  apr_file_t *file;
  const char *tmp_path;

  char buf[SVN_INT64_BUFFER_SIZE];
  apr_size_t len = svn__i64toa(buf, current);
  buf[len] = '\n';

  SVN_ERR(svn_io_open_unique_file3(&file, &tmp_path, fs->path,
                                   svn_io_file_del_none, pool, pool));
  SVN_ERR(svn_io_file_write_full(file, buf, len + 1, NULL, pool));
  SVN_ERR(svn_io_file_close(file, pool));

  return move_into_place(tmp_path, path_revprop_generation(fs, pool),
                         tmp_path, pool);
}

/* Make sure the revprop_namespace member in FS is set. */
static svn_error_t *
ensure_revprop_namespace(svn_fs_t *fs)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  return ffd->revprop_namespace == NULL
    ? svn_atomic_namespace__create(&ffd->revprop_namespace,
                                   svn_dirent_join(fs->path,
                                                   ATOMIC_REVPROP_NAMESPACE,
                                                   fs->pool),
                                   fs->pool)
    : SVN_NO_ERROR;
}

/* Make sure the revprop_namespace member in FS is set. */
static svn_error_t *
cleanup_revprop_namespace(svn_fs_t *fs)
{
  const char *name = svn_dirent_join(fs->path,
                                     ATOMIC_REVPROP_NAMESPACE,
                                     fs->pool);
  return svn_error_trace(svn_atomic_namespace__cleanup(name, fs->pool));
}

/* Make sure the revprop_generation member in FS is set and, if necessary,
 * initialized with the latest value stored on disk.
 */
static svn_error_t *
ensure_revprop_generation(svn_fs_t *fs, apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  SVN_ERR(ensure_revprop_namespace(fs));
  if (ffd->revprop_generation == NULL)
    {
      apr_int64_t current = 0;

      SVN_ERR(svn_named_atomic__get(&ffd->revprop_generation,
                                    ffd->revprop_namespace,
                                    ATOMIC_REVPROP_GENERATION,
                                    TRUE));

      /* If the generation is at 0, we just created a new namespace
       * (it would be at least 2 otherwise). Read the latest generation
       * from disk and if we are the first one to initialize the atomic
       * (i.e. is still 0), set it to the value just gotten.
       */
      SVN_ERR(svn_named_atomic__read(&current, ffd->revprop_generation));
      if (current == 0)
        {
          SVN_ERR(read_revprop_generation_file(&current, fs, pool));
          SVN_ERR(svn_named_atomic__cmpxchg(NULL, current, 0,
                                            ffd->revprop_generation));
        }
    }

  return SVN_NO_ERROR;
}

/* Make sure the revprop_timeout member in FS is set. */
static svn_error_t *
ensure_revprop_timeout(svn_fs_t *fs)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  SVN_ERR(ensure_revprop_namespace(fs));
  return ffd->revprop_timeout == NULL
    ? svn_named_atomic__get(&ffd->revprop_timeout,
                            ffd->revprop_namespace,
                            ATOMIC_REVPROP_TIMEOUT,
                            TRUE)
    : SVN_NO_ERROR;
}

/* Create an error object with the given MESSAGE and pass it to the
   WARNING member of FS. */
static void
log_revprop_cache_init_warning(svn_fs_t *fs,
                               svn_error_t *underlying_err,
                               const char *message)
{
  svn_error_t *err = svn_error_createf(SVN_ERR_FS_REVPROP_CACHE_INIT_FAILURE,
                                       underlying_err,
                                       message, fs->path);

  if (fs->warning)
    (fs->warning)(fs->warning_baton, err);

  svn_error_clear(err);
}

/* Test whether revprop cache and necessary infrastructure are
   available in FS. */
static svn_boolean_t
has_revprop_cache(svn_fs_t *fs, apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  svn_error_t *error;

  /* is the cache (still) enabled? */
  if (ffd->revprop_cache == NULL)
    return FALSE;

  /* is it efficient? */
  if (!svn_named_atomic__is_efficient())
    {
      /* access to it would be quite slow
       * -> disable the revprop cache for good
       */
      ffd->revprop_cache = NULL;
      log_revprop_cache_init_warning(fs, NULL,
                                     "Revprop caching for '%s' disabled"
                                     " because it would be inefficient.");

      return FALSE;
    }

  /* try to access our SHM-backed infrastructure */
  error = ensure_revprop_generation(fs, pool);
  if (error)
    {
      /* failure -> disable revprop cache for good */

      ffd->revprop_cache = NULL;
      log_revprop_cache_init_warning(fs, error,
                                     "Revprop caching for '%s' disabled "
                                     "because SHM infrastructure for revprop "
                                     "caching failed to initialize.");

      return FALSE;
    }

  return TRUE;
}

/* Baton structure for revprop_generation_fixup. */
typedef struct revprop_generation_fixup_t
{
  /* revprop generation to read */
  apr_int64_t *generation;

  /* containing the revprop_generation member to query */
  fs_fs_data_t *ffd;
} revprop_generation_upgrade_t;

/* If the revprop generation has an odd value, it means the original writer
   of the revprop got killed. We don't know whether that process as able
   to change the revprop data but we assume that it was. Therefore, we
   increase the generation in that case to basically invalidate everyones
   cache content.
   Execute this onlx while holding the write lock to the repo in baton->FFD.
 */
static svn_error_t *
revprop_generation_fixup(void *void_baton,
                         apr_pool_t *pool)
{
  revprop_generation_upgrade_t *baton = void_baton;
  assert(baton->ffd->has_write_lock);

  /* Maybe, either the original revprop writer or some other reader has
     already corrected / bumped the revprop generation.  Thus, we need
     to read it again. */
  SVN_ERR(svn_named_atomic__read(baton->generation,
                                 baton->ffd->revprop_generation));

  /* Cause everyone to re-read revprops upon their next access, if the
     last revprop write did not complete properly. */
  while (*baton->generation % 2)
    SVN_ERR(svn_named_atomic__add(baton->generation,
                                  1,
                                  baton->ffd->revprop_generation));

  return SVN_NO_ERROR;
}

/* Read the current revprop generation and return it in *GENERATION.
   Also, detect aborted / crashed writers and recover from that.
   Use the access object in FS to set the shared mem values. */
static svn_error_t *
read_revprop_generation(apr_int64_t *generation,
                        svn_fs_t *fs,
                        apr_pool_t *pool)
{
  apr_int64_t current = 0;
  fs_fs_data_t *ffd = fs->fsap_data;

  /* read the current revprop generation number */
  SVN_ERR(ensure_revprop_generation(fs, pool));
  SVN_ERR(svn_named_atomic__read(&current, ffd->revprop_generation));

  /* is an unfinished revprop write under the way? */
  if (current % 2)
    {
      apr_int64_t timeout = 0;

      /* read timeout for the write operation */
      SVN_ERR(ensure_revprop_timeout(fs));
      SVN_ERR(svn_named_atomic__read(&timeout, ffd->revprop_timeout));

      /* has the writer process been aborted,
       * i.e. has the timeout been reached?
       */
      if (apr_time_now() > timeout)
        {
          revprop_generation_upgrade_t baton;
          baton.generation = &current;
          baton.ffd = ffd;

          /* Ensure that the original writer process no longer exists by
           * acquiring the write lock to this repository.  Then, fix up
           * the revprop generation.
           */
          if (ffd->has_write_lock)
            SVN_ERR(revprop_generation_fixup(&baton, pool));
          else
            SVN_ERR(svn_fs_fs__with_write_lock(fs, revprop_generation_fixup,
                                               &baton, pool));
        }
    }

  /* return the value we just got */
  *generation = current;
  return SVN_NO_ERROR;
}

/* Set the revprop generation to the next odd number to indicate that
   there is a revprop write process under way. If that times out,
   readers shall recover from that state & re-read revprops.
   Use the access object in FS to set the shared mem value. */
static svn_error_t *
begin_revprop_change(svn_fs_t *fs, apr_pool_t *pool)
{
  apr_int64_t current;
  fs_fs_data_t *ffd = fs->fsap_data;

  /* set the timeout for the write operation */
  SVN_ERR(ensure_revprop_timeout(fs));
  SVN_ERR(svn_named_atomic__write(NULL,
                                  apr_time_now() + REVPROP_CHANGE_TIMEOUT,
                                  ffd->revprop_timeout));

  /* set the revprop generation to an odd value to indicate
   * that a write is in progress
   */
  SVN_ERR(ensure_revprop_generation(fs, pool));
  do
    {
      SVN_ERR(svn_named_atomic__add(&current,
                                    1,
                                    ffd->revprop_generation));
    }
  while (current % 2 == 0);

  return SVN_NO_ERROR;
}

/* Set the revprop generation to the next even number to indicate that
   a) readers shall re-read revprops, and
   b) the write process has been completed (no recovery required)
   Use the access object in FS to set the shared mem value. */
static svn_error_t *
end_revprop_change(svn_fs_t *fs, apr_pool_t *pool)
{
  apr_int64_t current = 1;
  fs_fs_data_t *ffd = fs->fsap_data;

  /* set the revprop generation to an even value to indicate
   * that a write has been completed
   */
  SVN_ERR(ensure_revprop_generation(fs, pool));
  do
    {
      SVN_ERR(svn_named_atomic__add(&current,
                                    1,
                                    ffd->revprop_generation));
    }
  while (current % 2);

  /* Save the latest generation to disk. FS is currently in a "locked"
   * state such that we can be sure the be the only ones to write that
   * file.
   */
  return write_revprop_generation_file(fs, current, pool);
}

/* Container for all data required to access the packed revprop file
 * for a given REVISION.  This structure will be filled incrementally
 * by read_pack_revprops() its sub-routines.
 */
typedef struct packed_revprops_t
{
  /* revision number to read (not necessarily the first in the pack) */
  svn_revnum_t revision;

  /* current revprop generation. Used when populating the revprop cache */
  apr_int64_t generation;

  /* the actual revision properties */
  apr_hash_t *properties;

  /* their size when serialized to a single string
   * (as found in PACKED_REVPROPS) */
  apr_size_t serialized_size;


  /* name of the pack file (without folder path) */
  const char *filename;

  /* packed shard folder path */
  const char *folder;

  /* sum of values in SIZES */
  apr_size_t total_size;

  /* first revision in the pack (>= MANIFEST_START) */
  svn_revnum_t start_revision;

  /* size of the revprops in PACKED_REVPROPS */
  apr_array_header_t *sizes;

  /* offset of the revprops in PACKED_REVPROPS */
  apr_array_header_t *offsets;


  /* concatenation of the serialized representation of all revprops
   * in the pack, i.e. the pack content without header and compression */
  svn_stringbuf_t *packed_revprops;

  /* First revision covered by MANIFEST.
   * Will equal the shard start revision or 1, for the 1st shard. */
  svn_revnum_t manifest_start;

  /* content of the manifest.
   * Maps long(rev - MANIFEST_START) to const char* pack file name */
  apr_array_header_t *manifest;
} packed_revprops_t;

/* Parse the serialized revprops in CONTENT and return them in *PROPERTIES.
 * Also, put them into the revprop cache, if activated, for future use.
 * Three more parameters are being used to update the revprop cache: FS is
 * our file system, the revprops belong to REVISION and the global revprop
 * GENERATION is used as well.
 *
 * The returned hash will be allocated in POOL, SCRATCH_POOL is being used
 * for temporary allocations.
 */
static svn_error_t *
parse_revprop(apr_hash_t **properties,
              svn_fs_t *fs,
              svn_revnum_t revision,
              apr_int64_t generation,
              svn_string_t *content,
              apr_pool_t *pool,
              apr_pool_t *scratch_pool)
{
  svn_stream_t *stream = svn_stream_from_string(content, scratch_pool);
  *properties = apr_hash_make(pool);

  SVN_ERR(svn_hash_read2(*properties, stream, SVN_HASH_TERMINATOR, pool));
  if (has_revprop_cache(fs, pool))
    {
      fs_fs_data_t *ffd = fs->fsap_data;
      pair_cache_key_t key = { 0 };

      key.revision = revision;
      key.second = generation;
      SVN_ERR(svn_cache__set(ffd->revprop_cache, &key, *properties,
                             scratch_pool));
    }

  return SVN_NO_ERROR;
}

/* Read the non-packed revprops for revision REV in FS, put them into the
 * revprop cache if activated and return them in *PROPERTIES.  GENERATION
 * is the current revprop generation.
 *
 * If the data could not be read due to an otherwise recoverable error,
 * leave *PROPERTIES unchanged. No error will be returned in that case.
 *
 * Allocations will be done in POOL.
 */
static svn_error_t *
read_non_packed_revprop(apr_hash_t **properties,
                        svn_fs_t *fs,
                        svn_revnum_t rev,
                        apr_int64_t generation,
                        apr_pool_t *pool)
{
  svn_stringbuf_t *content = NULL;
  apr_pool_t *iterpool = svn_pool_create(pool);
  svn_boolean_t missing = FALSE;
  int i;

  for (i = 0; i < RECOVERABLE_RETRY_COUNT && !missing && !content; ++i)
    {
      svn_pool_clear(iterpool);
      SVN_ERR(try_stringbuf_from_file(&content,
                                      &missing,
                                      path_revprops(fs, rev, iterpool),
                                      i + 1 < RECOVERABLE_RETRY_COUNT,
                                      iterpool));
    }

  if (content)
    SVN_ERR(parse_revprop(properties, fs, rev, generation,
                          svn_stringbuf__morph_into_string(content),
                          pool, iterpool));

  svn_pool_clear(iterpool);

  return SVN_NO_ERROR;
}

/* Given FS and REVPROPS->REVISION, fill the FILENAME, FOLDER and MANIFEST
 * members. Use POOL for allocating results and SCRATCH_POOL for temporaries.
 */
static svn_error_t *
get_revprop_packname(svn_fs_t *fs,
                     packed_revprops_t *revprops,
                     apr_pool_t *pool,
                     apr_pool_t *scratch_pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  svn_stringbuf_t *content = NULL;
  const char *manifest_file_path;
  int idx;

  /* read content of the manifest file */
  revprops->folder = path_revprops_pack_shard(fs, revprops->revision, pool);
  manifest_file_path = svn_dirent_join(revprops->folder, PATH_MANIFEST, pool);

  SVN_ERR(read_content(&content, manifest_file_path, pool));

  /* parse the manifest. Every line is a file name */
  revprops->manifest = apr_array_make(pool, ffd->max_files_per_dir,
                                      sizeof(const char*));

  /* Read all lines.  Since the last line ends with a newline, we will
     end up with a valid but empty string after the last entry. */
  while (content->data && *content->data)
    {
      APR_ARRAY_PUSH(revprops->manifest, const char*) = content->data;
      content->data = strchr(content->data, '\n');
      if (content->data)
        {
          *content->data = 0;
          content->data++;
        }
    }

  /* Index for our revision. Rev 0 is excluded from the first shard. */
  revprops->manifest_start = revprops->revision
                           - (revprops->revision % ffd->max_files_per_dir);
  if (revprops->manifest_start == 0)
    ++revprops->manifest_start;
  idx = (int)(revprops->revision - revprops->manifest_start);

  if (revprops->manifest->nelts <= idx)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Packed revprop manifest for r%ld too "
                               "small"), revprops->revision);

  /* Now get the file name */
  revprops->filename = APR_ARRAY_IDX(revprops->manifest, idx, const char*);

  return SVN_NO_ERROR;
}

/* Return TRUE, if revision R1 and R2 refer to the same shard in FS.
 */
static svn_boolean_t
same_shard(svn_fs_t *fs,
           svn_revnum_t r1,
           svn_revnum_t r2)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  return (r1 / ffd->max_files_per_dir) == (r2 / ffd->max_files_per_dir);
}

/* Given FS and the full packed file content in REVPROPS->PACKED_REVPROPS,
 * fill the START_REVISION, SIZES, OFFSETS members. Also, make
 * PACKED_REVPROPS point to the first serialized revprop.
 *
 * Parse the revprops for REVPROPS->REVISION and set the PROPERTIES as
 * well as the SERIALIZED_SIZE member.  If revprop caching has been
 * enabled, parse all revprops in the pack and cache them.
 */
static svn_error_t *
parse_packed_revprops(svn_fs_t *fs,
                      packed_revprops_t *revprops,
                      apr_pool_t *pool,
                      apr_pool_t *scratch_pool)
{
  svn_stream_t *stream;
  apr_int64_t first_rev, count, i;
  apr_off_t offset;
  const char *header_end;
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);

  /* decompress (even if the data is only "stored", there is still a
   * length header to remove) */
  svn_string_t *compressed
      = svn_stringbuf__morph_into_string(revprops->packed_revprops);
  svn_stringbuf_t *uncompressed = svn_stringbuf_create_empty(pool);
  SVN_ERR(svn__decompress(compressed, uncompressed, APR_SIZE_MAX));

  /* read first revision number and number of revisions in the pack */
  stream = svn_stream_from_stringbuf(uncompressed, scratch_pool);
  SVN_ERR(read_number_from_stream(&first_rev, NULL, stream, iterpool));
  SVN_ERR(read_number_from_stream(&count, NULL, stream, iterpool));

  /* Check revision range for validity. */
  if (   !same_shard(fs, revprops->revision, first_rev)
      || !same_shard(fs, revprops->revision, first_rev + count - 1)
      || count < 1)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Revprop pack for revision r%ld"
                               " contains revprops for r%ld .. r%ld"),
                             revprops->revision,
                             (svn_revnum_t)first_rev,
                             (svn_revnum_t)(first_rev + count -1));

  /* Since start & end are in the same shard, it is enough to just test
   * the FIRST_REV for being actually packed.  That will also cover the
   * special case of rev 0 never being packed. */
  if (!is_packed_revprop(fs, first_rev))
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Revprop pack for revision r%ld"
                               " starts at non-packed revisions r%ld"),
                             revprops->revision, (svn_revnum_t)first_rev);

  /* make PACKED_REVPROPS point to the first char after the header.
   * This is where the serialized revprops are. */
  header_end = strstr(uncompressed->data, "\n\n");
  if (header_end == NULL)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Header end not found"));

  offset = header_end - uncompressed->data + 2;

  revprops->packed_revprops = svn_stringbuf_create_empty(pool);
  revprops->packed_revprops->data = uncompressed->data + offset;
  revprops->packed_revprops->len = (apr_size_t)(uncompressed->len - offset);
  revprops->packed_revprops->blocksize = (apr_size_t)(uncompressed->blocksize - offset);

  /* STREAM still points to the first entry in the sizes list.
   * Init / construct REVPROPS members. */
  revprops->start_revision = (svn_revnum_t)first_rev;
  revprops->sizes = apr_array_make(pool, (int)count, sizeof(offset));
  revprops->offsets = apr_array_make(pool, (int)count, sizeof(offset));

  /* Now parse, revision by revision, the size and content of each
   * revisions' revprops. */
  for (i = 0, offset = 0, revprops->total_size = 0; i < count; ++i)
    {
      apr_int64_t size;
      svn_string_t serialized;
      apr_hash_t *properties;
      svn_revnum_t revision = (svn_revnum_t)(first_rev + i);

      /* read & check the serialized size */
      SVN_ERR(read_number_from_stream(&size, NULL, stream, iterpool));
      if (size + offset > (apr_int64_t)revprops->packed_revprops->len)
        return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                        _("Packed revprop size exceeds pack file size"));

      /* Parse this revprops list, if necessary */
      serialized.data = revprops->packed_revprops->data + offset;
      serialized.len = (apr_size_t)size;

      if (revision == revprops->revision)
        {
          SVN_ERR(parse_revprop(&revprops->properties, fs, revision,
                                revprops->generation, &serialized,
                                pool, iterpool));
          revprops->serialized_size = serialized.len;
        }
      else
        {
          /* If revprop caching is enabled, parse any revprops.
           * They will get cached as a side-effect of this. */
          if (has_revprop_cache(fs, pool))
            SVN_ERR(parse_revprop(&properties, fs, revision,
                                  revprops->generation, &serialized,
                                  iterpool, iterpool));
        }

      /* fill REVPROPS data structures */
      APR_ARRAY_PUSH(revprops->sizes, apr_off_t) = serialized.len;
      APR_ARRAY_PUSH(revprops->offsets, apr_off_t) = offset;
      revprops->total_size += serialized.len;

      offset += serialized.len;

      svn_pool_clear(iterpool);
    }

  return SVN_NO_ERROR;
}

/* In filesystem FS, read the packed revprops for revision REV into
 * *REVPROPS.  Use GENERATION to populate the revprop cache, if enabled.
 * Allocate data in POOL.
 */
static svn_error_t *
read_pack_revprop(packed_revprops_t **revprops,
                  svn_fs_t *fs,
                  svn_revnum_t rev,
                  apr_int64_t generation,
                  apr_pool_t *pool)
{
  apr_pool_t *iterpool = svn_pool_create(pool);
  svn_boolean_t missing = FALSE;
  svn_error_t *err;
  packed_revprops_t *result;
  int i;

  /* someone insisted that REV is packed. Double-check if necessary */
  if (!is_packed_revprop(fs, rev))
     SVN_ERR(update_min_unpacked_rev(fs, iterpool));

  if (!is_packed_revprop(fs, rev))
    return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
                              _("No such packed revision %ld"), rev);

  /* initialize the result data structure */
  result = apr_pcalloc(pool, sizeof(*result));
  result->revision = rev;
  result->generation = generation;

  /* try to read the packed revprops. This may require retries if we have
   * concurrent writers. */
  for (i = 0; i < RECOVERABLE_RETRY_COUNT && !result->packed_revprops; ++i)
    {
      const char *file_path;

      /* there might have been concurrent writes.
       * Re-read the manifest and the pack file.
       */
      SVN_ERR(get_revprop_packname(fs, result, pool, iterpool));
      file_path  = svn_dirent_join(result->folder,
                                   result->filename,
                                   iterpool);
      SVN_ERR(try_stringbuf_from_file(&result->packed_revprops,
                                      &missing,
                                      file_path,
                                      i + 1 < RECOVERABLE_RETRY_COUNT,
                                      pool));

      /* If we could not find the file, there was a write.
       * So, we should refresh our revprop generation info as well such
       * that others may find data we will put into the cache.  They would
       * consider it outdated, otherwise.
       */
      if (missing && has_revprop_cache(fs, pool))
        SVN_ERR(read_revprop_generation(&result->generation, fs, pool));

      svn_pool_clear(iterpool);
    }

  /* the file content should be available now */
  if (!result->packed_revprops)
    return svn_error_createf(SVN_ERR_FS_PACKED_REVPROP_READ_FAILURE, NULL,
                  _("Failed to read revprop pack file for r%ld"), rev);

  /* parse it. RESULT will be complete afterwards. */
  err = parse_packed_revprops(fs, result, pool, iterpool);
  svn_pool_destroy(iterpool);
  if (err)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, err,
                  _("Revprop pack file for r%ld is corrupt"), rev);

  *revprops = result;

  return SVN_NO_ERROR;
}

/* Read the revprops for revision REV in FS and return them in *PROPERTIES_P.
 *
 * Allocations will be done in POOL.
 */
static svn_error_t *
get_revision_proplist(apr_hash_t **proplist_p,
                      svn_fs_t *fs,
                      svn_revnum_t rev,
                      apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  apr_int64_t generation = 0;

  /* not found, yet */
  *proplist_p = NULL;

  /* should they be available at all? */
  SVN_ERR(ensure_revision_exists(fs, rev, pool));

  /* Try cache lookup first. */
  if (has_revprop_cache(fs, pool))
    {
      svn_boolean_t is_cached;
      pair_cache_key_t key = { 0 };

      SVN_ERR(read_revprop_generation(&generation, fs, pool));

      key.revision = rev;
      key.second = generation;
      SVN_ERR(svn_cache__get((void **) proplist_p, &is_cached,
                             ffd->revprop_cache, &key, pool));
      if (is_cached)
        return SVN_NO_ERROR;
    }

  /* if REV had not been packed when we began, try reading it from the
   * non-packed shard.  If that fails, we will fall through to packed
   * shard reads. */
  if (!is_packed_revprop(fs, rev))
    {
      svn_error_t *err = read_non_packed_revprop(proplist_p, fs, rev,
                                                 generation, pool);
      if (err)
        {
          if (!APR_STATUS_IS_ENOENT(err->apr_err)
              || ffd->format < SVN_FS_FS__MIN_PACKED_REVPROP_FORMAT)
            return svn_error_trace(err);

          svn_error_clear(err);
          *proplist_p = NULL; /* in case read_non_packed_revprop changed it */
        }
    }

  /* if revprop packing is available and we have not read the revprops, yet,
   * try reading them from a packed shard.  If that fails, REV is most
   * likely invalid (or its revprops highly contested). */
  if (ffd->format >= SVN_FS_FS__MIN_PACKED_REVPROP_FORMAT && !*proplist_p)
    {
      packed_revprops_t *packed_revprops;
      SVN_ERR(read_pack_revprop(&packed_revprops, fs, rev, generation, pool));
      *proplist_p = packed_revprops->properties;
    }

  /* The revprops should have been there. Did we get them? */
  if (!*proplist_p)
    return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
                             _("Could not read revprops for revision %ld"),
                             rev);

  return SVN_NO_ERROR;
}

/* Serialize the revision property list PROPLIST of revision REV in
 * filesystem FS to a non-packed file.  Return the name of that temporary
 * file in *TMP_PATH and the file path that it must be moved to in
 * *FINAL_PATH.
 *
 * Use POOL for allocations.
 */
static svn_error_t *
write_non_packed_revprop(const char **final_path,
                         const char **tmp_path,
                         svn_fs_t *fs,
                         svn_revnum_t rev,
                         apr_hash_t *proplist,
                         apr_pool_t *pool)
{
  apr_file_t *file;
  svn_stream_t *stream;
  *final_path = path_revprops(fs, rev, pool);

  /* ### do we have a directory sitting around already? we really shouldn't
     ### have to get the dirname here. */
  SVN_ERR(svn_io_open_unique_file3(&file, tmp_path,
                                   svn_dirent_dirname(*final_path, pool),
                                   svn_io_file_del_none, pool, pool));
  stream = svn_stream_from_aprfile2(file, TRUE, pool);
  SVN_ERR(svn_hash_write2(proplist, stream, SVN_HASH_TERMINATOR, pool));
  SVN_ERR(svn_stream_close(stream));

  /* Flush temporary file to disk and close it. */
  SVN_ERR(svn_io_file_flush_to_disk(file, pool));
  SVN_ERR(svn_io_file_close(file, pool));

  return SVN_NO_ERROR;
}

/* After writing the new revprop file(s), call this function to move the
 * file at TMP_PATH to FINAL_PATH and give it the permissions from
 * PERMS_REFERENCE.
 *
 * If indicated in BUMP_GENERATION, increase FS' revprop generation.
 * Finally, delete all the temporary files given in FILES_TO_DELETE.
 * The latter may be NULL.
 *
 * Use POOL for temporary allocations.
 */
static svn_error_t *
switch_to_new_revprop(svn_fs_t *fs,
                      const char *final_path,
                      const char *tmp_path,
                      const char *perms_reference,
                      apr_array_header_t *files_to_delete,
                      svn_boolean_t bump_generation,
                      apr_pool_t *pool)
{
  /* Now, we may actually be replacing revprops. Make sure that all other
     threads and processes will know about this. */
  if (bump_generation)
    SVN_ERR(begin_revprop_change(fs, pool));

  SVN_ERR(move_into_place(tmp_path, final_path, perms_reference, pool));

  /* Indicate that the update (if relevant) has been completed. */
  if (bump_generation)
    SVN_ERR(end_revprop_change(fs, pool));

  /* Clean up temporary files, if necessary. */
  if (files_to_delete)
    {
      apr_pool_t *iterpool = svn_pool_create(pool);
      int i;

      for (i = 0; i < files_to_delete->nelts; ++i)
        {
          const char *path = APR_ARRAY_IDX(files_to_delete, i, const char*);
          SVN_ERR(svn_io_remove_file2(path, TRUE, iterpool));
          svn_pool_clear(iterpool);
        }

      svn_pool_destroy(iterpool);
    }
  return SVN_NO_ERROR;
}

/* Write a pack file header to STREAM that starts at revision START_REVISION
 * and contains the indexes [START,END) of SIZES.
 */
static svn_error_t *
serialize_revprops_header(svn_stream_t *stream,
                          svn_revnum_t start_revision,
                          apr_array_header_t *sizes,
                          int start,
                          int end,
                          apr_pool_t *pool)
{
  apr_pool_t *iterpool = svn_pool_create(pool);
  int i;

  SVN_ERR_ASSERT(start < end);

  /* start revision and entry count */
  SVN_ERR(svn_stream_printf(stream, pool, "%ld\n", start_revision));
  SVN_ERR(svn_stream_printf(stream, pool, "%d\n", end - start));

  /* the sizes array */
  for (i = start; i < end; ++i)
    {
      apr_off_t size = APR_ARRAY_IDX(sizes, i, apr_off_t);
      SVN_ERR(svn_stream_printf(stream, iterpool, "%" APR_OFF_T_FMT "\n",
                                size));
    }

  /* the double newline char indicates the end of the header */
  SVN_ERR(svn_stream_printf(stream, iterpool, "\n"));

  svn_pool_clear(iterpool);
  return SVN_NO_ERROR;
}

/* Writes the a pack file to FILE.  It copies the serialized data
 * from REVPROPS for the indexes [START,END) except for index CHANGED_INDEX.
 *
 * The data for the latter is taken from NEW_SERIALIZED.  Note, that
 * CHANGED_INDEX may be outside the [START,END) range, i.e. no new data is
 * taken in that case but only a subset of the old data will be copied.
 *
 * NEW_TOTAL_SIZE is a hint for pre-allocating buffers of appropriate size.
 * POOL is used for temporary allocations.
 */
static svn_error_t *
repack_revprops(svn_fs_t *fs,
                packed_revprops_t *revprops,
                int start,
                int end,
                int changed_index,
                svn_stringbuf_t *new_serialized,
                apr_off_t new_total_size,
                apr_file_t *file,
                apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  svn_stream_t *stream;
  int i;

  /* create data empty buffers and the stream object */
  svn_stringbuf_t *uncompressed
    = svn_stringbuf_create_ensure((apr_size_t)new_total_size, pool);
  svn_stringbuf_t *compressed
    = svn_stringbuf_create_empty(pool);
  stream = svn_stream_from_stringbuf(uncompressed, pool);

  /* write the header*/
  SVN_ERR(serialize_revprops_header(stream, revprops->start_revision + start,
                                    revprops->sizes, start, end, pool));

  /* append the serialized revprops */
  for (i = start; i < end; ++i)
    if (i == changed_index)
      {
        SVN_ERR(svn_stream_write(stream,
                                 new_serialized->data,
                                 &new_serialized->len));
      }
    else
      {
        apr_size_t size
            = (apr_size_t)APR_ARRAY_IDX(revprops->sizes, i, apr_off_t);
        apr_size_t offset
            = (apr_size_t)APR_ARRAY_IDX(revprops->offsets, i, apr_off_t);

        SVN_ERR(svn_stream_write(stream,
                                 revprops->packed_revprops->data + offset,
                                 &size));
      }

  /* flush the stream buffer (if any) to our underlying data buffer */
  SVN_ERR(svn_stream_close(stream));

  /* compress / store the data */
  SVN_ERR(svn__compress(svn_stringbuf__morph_into_string(uncompressed),
                        compressed,
                        ffd->compress_packed_revprops
                          ? SVN_DELTA_COMPRESSION_LEVEL_DEFAULT
                          : SVN_DELTA_COMPRESSION_LEVEL_NONE));

  /* finally, write the content to the target file, flush and close it */
  SVN_ERR(svn_io_file_write_full(file, compressed->data, compressed->len,
                                 NULL, pool));
  SVN_ERR(svn_io_file_flush_to_disk(file, pool));
  SVN_ERR(svn_io_file_close(file, pool));

  return SVN_NO_ERROR;
}

/* Allocate a new pack file name for revisions
 *     [REVPROPS->START_REVISION + START, REVPROPS->START_REVISION + END - 1]
 * of REVPROPS->MANIFEST.  Add the name of old file to FILES_TO_DELETE,
 * auto-create that array if necessary.  Return an open file *FILE that is
 * allocated in POOL.
 */
static svn_error_t *
repack_file_open(apr_file_t **file,
                 svn_fs_t *fs,
                 packed_revprops_t *revprops,
                 int start,
                 int end,
                 apr_array_header_t **files_to_delete,
                 apr_pool_t *pool)
{
  apr_int64_t tag;
  const char *tag_string;
  svn_string_t *new_filename;
  int i;
  int manifest_offset
    = (int)(revprops->start_revision - revprops->manifest_start);

  /* get the old (= current) file name and enlist it for later deletion */
  const char *old_filename = APR_ARRAY_IDX(revprops->manifest,
                                           start + manifest_offset,
                                           const char*);

  if (*files_to_delete == NULL)
    *files_to_delete = apr_array_make(pool, 3, sizeof(const char*));

  APR_ARRAY_PUSH(*files_to_delete, const char*)
    = svn_dirent_join(revprops->folder, old_filename, pool);

  /* increase the tag part, i.e. the counter after the dot */
  tag_string = strchr(old_filename, '.');
  if (tag_string == NULL)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Packed file '%s' misses a tag"),
                             old_filename);

  SVN_ERR(svn_cstring_atoi64(&tag, tag_string + 1));
  new_filename = svn_string_createf(pool, "%ld.%" APR_INT64_T_FMT,
                                    revprops->start_revision + start,
                                    ++tag);

  /* update the manifest to point to the new file */
  for (i = start; i < end; ++i)
    APR_ARRAY_IDX(revprops->manifest, i + manifest_offset, const char*)
      = new_filename->data;

  /* open the file */
  SVN_ERR(svn_io_file_open(file, svn_dirent_join(revprops->folder,
                                                 new_filename->data,
                                                 pool),
                           APR_WRITE | APR_CREATE, APR_OS_DEFAULT, pool));

  return SVN_NO_ERROR;
}

/* For revision REV in filesystem FS, set the revision properties to
 * PROPLIST.  Return a new file in *TMP_PATH that the caller shall move
 * to *FINAL_PATH to make the change visible.  Files to be deleted will
 * be listed in *FILES_TO_DELETE which may remain unchanged / unallocated.
 * Use POOL for allocations.
 */
static svn_error_t *
write_packed_revprop(const char **final_path,
                     const char **tmp_path,
                     apr_array_header_t **files_to_delete,
                     svn_fs_t *fs,
                     svn_revnum_t rev,
                     apr_hash_t *proplist,
                     apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  packed_revprops_t *revprops;
  apr_int64_t generation = 0;
  svn_stream_t *stream;
  apr_file_t *file;
  svn_stringbuf_t *serialized;
  apr_off_t new_total_size;
  int changed_index;

  /* read the current revprop generation. This value will not change
   * while we hold the global write lock to this FS. */
  if (has_revprop_cache(fs, pool))
    SVN_ERR(read_revprop_generation(&generation, fs, pool));

  /* read contents of the current pack file */
  SVN_ERR(read_pack_revprop(&revprops, fs, rev, generation, pool));

  /* serialize the new revprops */
  serialized = svn_stringbuf_create_empty(pool);
  stream = svn_stream_from_stringbuf(serialized, pool);
  SVN_ERR(svn_hash_write2(proplist, stream, SVN_HASH_TERMINATOR, pool));
  SVN_ERR(svn_stream_close(stream));

  /* calculate the size of the new data */
  changed_index = (int)(rev - revprops->start_revision);
  new_total_size = revprops->total_size - revprops->serialized_size
                 + serialized->len
                 + (revprops->offsets->nelts + 2) * SVN_INT64_BUFFER_SIZE;

  APR_ARRAY_IDX(revprops->sizes, changed_index, apr_off_t) = serialized->len;

  /* can we put the new data into the same pack as the before? */
  if (   new_total_size < ffd->revprop_pack_size
      || revprops->sizes->nelts == 1)
    {
      /* simply replace the old pack file with new content as we do it
       * in the non-packed case */

      *final_path = svn_dirent_join(revprops->folder, revprops->filename,
                                    pool);
      SVN_ERR(svn_io_open_unique_file3(&file, tmp_path, revprops->folder,
                                       svn_io_file_del_none, pool, pool));
      SVN_ERR(repack_revprops(fs, revprops, 0, revprops->sizes->nelts,
                              changed_index, serialized, new_total_size,
                              file, pool));
    }
  else
    {
      /* split the pack file into two of roughly equal size */
      int right_count, left_count, i;

      int left = 0;
      int right = revprops->sizes->nelts - 1;
      apr_off_t left_size = 2 * SVN_INT64_BUFFER_SIZE;
      apr_off_t right_size = 2 * SVN_INT64_BUFFER_SIZE;

      /* let left and right side grow such that their size difference
       * is minimal after each step. */
      while (left <= right)
        if (  left_size + APR_ARRAY_IDX(revprops->sizes, left, apr_off_t)
            < right_size + APR_ARRAY_IDX(revprops->sizes, right, apr_off_t))
          {
            left_size += APR_ARRAY_IDX(revprops->sizes, left, apr_off_t)
                      + SVN_INT64_BUFFER_SIZE;
            ++left;
          }
        else
          {
            right_size += APR_ARRAY_IDX(revprops->sizes, right, apr_off_t)
                        + SVN_INT64_BUFFER_SIZE;
            --right;
          }

       /* since the items need much less than SVN_INT64_BUFFER_SIZE
        * bytes to represent their length, the split may not be optimal */
      left_count = left;
      right_count = revprops->sizes->nelts - left;

      /* if new_size is large, one side may exceed the pack size limit.
       * In that case, split before and after the modified revprop.*/
      if (   left_size > ffd->revprop_pack_size
          || right_size > ffd->revprop_pack_size)
        {
          left_count = changed_index;
          right_count = revprops->sizes->nelts - left_count - 1;
        }

      /* write the new, split files */
      if (left_count)
        {
          SVN_ERR(repack_file_open(&file, fs, revprops, 0,
                                   left_count, files_to_delete, pool));
          SVN_ERR(repack_revprops(fs, revprops, 0, left_count,
                                  changed_index, serialized, new_total_size,
                                  file, pool));
        }

      if (left_count + right_count < revprops->sizes->nelts)
        {
          SVN_ERR(repack_file_open(&file, fs, revprops, changed_index,
                                   changed_index + 1, files_to_delete,
                                   pool));
          SVN_ERR(repack_revprops(fs, revprops, changed_index,
                                  changed_index + 1,
                                  changed_index, serialized, new_total_size,
                                  file, pool));
        }

      if (right_count)
        {
          SVN_ERR(repack_file_open(&file, fs, revprops,
                                   revprops->sizes->nelts - right_count,
                                   revprops->sizes->nelts,
                                   files_to_delete, pool));
          SVN_ERR(repack_revprops(fs, revprops,
                                  revprops->sizes->nelts - right_count,
                                  revprops->sizes->nelts, changed_index,
                                  serialized, new_total_size, file,
                                  pool));
        }

      /* write the new manifest */
      *final_path = svn_dirent_join(revprops->folder, PATH_MANIFEST, pool);
      SVN_ERR(svn_io_open_unique_file3(&file, tmp_path, revprops->folder,
                                       svn_io_file_del_none, pool, pool));

      for (i = 0; i < revprops->manifest->nelts; ++i)
        {
          const char *filename = APR_ARRAY_IDX(revprops->manifest, i,
                                               const char*);
          SVN_ERR(svn_io_file_write_full(file, filename, strlen(filename),
                                         NULL, pool));
          SVN_ERR(svn_io_file_putc('\n', file, pool));
        }

      SVN_ERR(svn_io_file_flush_to_disk(file, pool));
      SVN_ERR(svn_io_file_close(file, pool));
    }

  return SVN_NO_ERROR;
}

/* Set the revision property list of revision REV in filesystem FS to
   PROPLIST.  Use POOL for temporary allocations. */
static svn_error_t *
set_revision_proplist(svn_fs_t *fs,
                      svn_revnum_t rev,
                      apr_hash_t *proplist,
                      apr_pool_t *pool)
{
  svn_boolean_t is_packed;
  svn_boolean_t bump_generation = FALSE;
  const char *final_path;
  const char *tmp_path;
  const char *perms_reference;
  apr_array_header_t *files_to_delete = NULL;

  SVN_ERR(ensure_revision_exists(fs, rev, pool));

  /* this info will not change while we hold the global FS write lock */
  is_packed = is_packed_revprop(fs, rev);

  /* Test whether revprops already exist for this revision.
   * Only then will we need to bump the revprop generation. */
  if (has_revprop_cache(fs, pool))
    {
      if (is_packed)
        {
          bump_generation = TRUE;
        }
      else
        {
          svn_node_kind_t kind;
          SVN_ERR(svn_io_check_path(path_revprops(fs, rev, pool), &kind,
                                    pool));
          bump_generation = kind != svn_node_none;
        }
    }

  /* Serialize the new revprop data */
  if (is_packed)
    SVN_ERR(write_packed_revprop(&final_path, &tmp_path, &files_to_delete,
                                 fs, rev, proplist, pool));
  else
    SVN_ERR(write_non_packed_revprop(&final_path, &tmp_path,
                                     fs, rev, proplist, pool));

  /* We use the rev file of this revision as the perms reference,
   * because when setting revprops for the first time, the revprop
   * file won't exist and therefore can't serve as its own reference.
   * (Whereas the rev file should already exist at this point.)
   */
  SVN_ERR(svn_fs_fs__path_rev_absolute(&perms_reference, fs, rev, pool));

  /* Now, switch to the new revprop data. */
  SVN_ERR(switch_to_new_revprop(fs, final_path, tmp_path, perms_reference,
                                files_to_delete, bump_generation, pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__revision_proplist(apr_hash_t **proplist_p,
                             svn_fs_t *fs,
                             svn_revnum_t rev,
                             apr_pool_t *pool)
{
  SVN_ERR(get_revision_proplist(proplist_p, fs, rev, pool));

  return SVN_NO_ERROR;
}

/* Represents where in the current svndiff data block each
   representation is. */
struct rep_state
{
  apr_file_t *file;
                    /* The txdelta window cache to use or NULL. */
  svn_cache__t *window_cache;
                    /* Caches un-deltified windows. May be NULL. */
  svn_cache__t *combined_cache;
  apr_off_t start;  /* The starting offset for the raw
                       svndiff/plaintext data minus header. */
  apr_off_t off;    /* The current offset into the file. */
  apr_off_t end;    /* The end offset of the raw data. */
  int ver;          /* If a delta, what svndiff version? */
  int chunk_index;
};

/* See create_rep_state, which wraps this and adds another error. */
static svn_error_t *
create_rep_state_body(struct rep_state **rep_state,
                      struct rep_args **rep_args,
                      apr_file_t **file_hint,
                      svn_revnum_t *rev_hint,
                      representation_t *rep,
                      svn_fs_t *fs,
                      apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  struct rep_state *rs = apr_pcalloc(pool, sizeof(*rs));
  struct rep_args *ra;
  unsigned char buf[4];

  /* If the hint is
   * - given,
   * - refers to a valid revision,
   * - refers to a packed revision,
   * - as does the rep we want to read, and
   * - refers to the same pack file as the rep
   * ...
   */
  if (   file_hint && rev_hint && *file_hint
      && SVN_IS_VALID_REVNUM(*rev_hint)
      && *rev_hint < ffd->min_unpacked_rev
      && rep->revision < ffd->min_unpacked_rev
      && (   (*rev_hint / ffd->max_files_per_dir)
          == (rep->revision / ffd->max_files_per_dir)))
    {
      /* ... we can re-use the same, already open file object
       */
      apr_off_t offset;
      SVN_ERR(get_packed_offset(&offset, fs, rep->revision, pool));

      offset += rep->offset;
      SVN_ERR(svn_io_file_seek(*file_hint, APR_SET, &offset, pool));

      rs->file = *file_hint;
    }
  else
    {
      /* otherwise, create a new file object
       */
      SVN_ERR(open_and_seek_representation(&rs->file, fs, rep, pool));
    }

  /* remember the current file, if suggested by the caller */
  if (file_hint)
    *file_hint = rs->file;
  if (rev_hint)
    *rev_hint = rep->revision;

  /* continue constructing RS and RA */
  rs->window_cache = ffd->txdelta_window_cache;
  rs->combined_cache = ffd->combined_window_cache;

  SVN_ERR(read_rep_line(&ra, rs->file, pool));
  SVN_ERR(get_file_offset(&rs->start, rs->file, pool));
  rs->off = rs->start;
  rs->end = rs->start + rep->size;
  *rep_state = rs;
  *rep_args = ra;

  if (!ra->is_delta)
    /* This is a plaintext, so just return the current rep_state. */
    return SVN_NO_ERROR;

  /* We are dealing with a delta, find out what version. */
  SVN_ERR(svn_io_file_read_full2(rs->file, buf, sizeof(buf),
                                 NULL, NULL, pool));
  /* ### Layering violation */
  if (! ((buf[0] == 'S') && (buf[1] == 'V') && (buf[2] == 'N')))
    return svn_error_create
      (SVN_ERR_FS_CORRUPT, NULL,
       _("Malformed svndiff data in representation"));
  rs->ver = buf[3];
  rs->chunk_index = 0;
  rs->off += 4;

  return SVN_NO_ERROR;
}

/* Read the rep args for REP in filesystem FS and create a rep_state
   for reading the representation.  Return the rep_state in *REP_STATE
   and the rep args in *REP_ARGS, both allocated in POOL.

   When reading multiple reps, i.e. a skip delta chain, you may provide
   non-NULL FILE_HINT and REV_HINT.  (If FILE_HINT is not NULL, in the first
   call it should be a pointer to NULL.)  The function will use these variables
   to store the previous call results and tries to re-use them.  This may
   result in significant savings in I/O for packed files.
 */
static svn_error_t *
create_rep_state(struct rep_state **rep_state,
                 struct rep_args **rep_args,
                 apr_file_t **file_hint,
                 svn_revnum_t *rev_hint,
                 representation_t *rep,
                 svn_fs_t *fs,
                 apr_pool_t *pool)
{
  svn_error_t *err = create_rep_state_body(rep_state, rep_args,
                                           file_hint, rev_hint,
                                           rep, fs, pool);
  if (err && err->apr_err == SVN_ERR_FS_CORRUPT)
    {
      fs_fs_data_t *ffd = fs->fsap_data;

      /* ### This always returns "-1" for transaction reps, because
         ### this particular bit of code doesn't know if the rep is
         ### stored in the protorev or in the mutable area (for props
         ### or dir contents).  It is pretty rare for FSFS to *read*
         ### from the protorev file, though, so this is probably OK.
         ### And anyone going to debug corruption errors is probably
         ### going to jump straight to this comment anyway! */
      return svn_error_createf(SVN_ERR_FS_CORRUPT, err,
                               "Corrupt representation '%s'",
                               rep
                               ? representation_string(rep, ffd->format, TRUE,
                                                       TRUE, pool)
                               : "(null)");
    }
  /* ### Call representation_string() ? */
  return svn_error_trace(err);
}

struct rep_read_baton
{
  /* The FS from which we're reading. */
  svn_fs_t *fs;

  /* If not NULL, this is the base for the first delta window in rs_list */
  svn_stringbuf_t *base_window;

  /* The state of all prior delta representations. */
  apr_array_header_t *rs_list;

  /* The plaintext state, if there is a plaintext. */
  struct rep_state *src_state;

  /* The index of the current delta chunk, if we are reading a delta. */
  int chunk_index;

  /* The buffer where we store undeltified data. */
  char *buf;
  apr_size_t buf_pos;
  apr_size_t buf_len;

  /* A checksum context for summing the data read in order to verify it.
     Note: we don't need to use the sha1 checksum because we're only doing
     data verification, for which md5 is perfectly safe.  */
  svn_checksum_ctx_t *md5_checksum_ctx;

  svn_boolean_t checksum_finalized;

  /* The stored checksum of the representation we are reading, its
     length, and the amount we've read so far.  Some of this
     information is redundant with rs_list and src_state, but it's
     convenient for the checksumming code to have it here. */
  svn_checksum_t *md5_checksum;

  svn_filesize_t len;
  svn_filesize_t off;

  /* The key for the fulltext cache for this rep, if there is a
     fulltext cache. */
  pair_cache_key_t fulltext_cache_key;
  /* The text we've been reading, if we're going to cache it. */
  svn_stringbuf_t *current_fulltext;

  /* Used for temporary allocations during the read. */
  apr_pool_t *pool;

  /* Pool used to store file handles and other data that is persistant
     for the entire stream read. */
  apr_pool_t *filehandle_pool;
};

/* Combine the name of the rev file in RS with the given OFFSET to form
 * a cache lookup key.  Allocations will be made from POOL.  May return
 * NULL if the key cannot be constructed. */
static const char*
get_window_key(struct rep_state *rs, apr_off_t offset, apr_pool_t *pool)
{
  const char *name;
  const char *last_part;
  const char *name_last;

  /* the rev file name containing the txdelta window.
   * If this fails we are in serious trouble anyways.
   * And if nobody else detects the problems, the file content checksum
   * comparison _will_ find them.
   */
  if (apr_file_name_get(&name, rs->file))
    return NULL;

  /* Handle packed files as well by scanning backwards until we find the
   * revision or pack number. */
  name_last = name + strlen(name) - 1;
  while (! svn_ctype_isdigit(*name_last))
    --name_last;

  last_part = name_last;
  while (svn_ctype_isdigit(*last_part))
    --last_part;

  /* We must differentiate between packed files (as of today, the number
   * is being followed by a dot) and non-packed files (followed by \0).
   * Otherwise, there might be overlaps in the numbering range if the
   * repo gets packed after caching the txdeltas of non-packed revs.
   * => add the first non-digit char to the packed number. */
  if (name_last[1] != '\0')
    ++name_last;

  /* copy one char MORE than the actual number to mark packed files,
   * i.e. packed revision file content uses different key space then
   * non-packed ones: keys for packed rev file content ends with a dot
   * for non-packed rev files they end with a digit. */
  name = apr_pstrndup(pool, last_part + 1, name_last - last_part);
  return svn_fs_fs__combine_number_and_string(offset, name, pool);
}

/* Read the WINDOW_P for the rep state RS from the current FSFS session's
 * cache. This will be a no-op and IS_CACHED will be set to FALSE if no
 * cache has been given. If a cache is available IS_CACHED will inform
 * the caller about the success of the lookup. Allocations (of the window
 * in particualar) will be made from POOL.
 *
 * If the information could be found, put RS and the position within the
 * rev file into the same state as if the data had just been read from it.
 */
static svn_error_t *
get_cached_window(svn_txdelta_window_t **window_p,
                  struct rep_state *rs,
                  svn_boolean_t *is_cached,
                  apr_pool_t *pool)
{
  if (! rs->window_cache)
    {
      /* txdelta window has not been enabled */
      *is_cached = FALSE;
    }
  else
    {
      /* ask the cache for the desired txdelta window */
      svn_fs_fs__txdelta_cached_window_t *cached_window;
      SVN_ERR(svn_cache__get((void **) &cached_window,
                             is_cached,
                             rs->window_cache,
                             get_window_key(rs, rs->off, pool),
                             pool));

      if (*is_cached)
        {
          /* found it. Pass it back to the caller. */
          *window_p = cached_window->window;

          /* manipulate the RS as if we just read the data */
          rs->chunk_index++;
          rs->off = cached_window->end_offset;

          /* manipulate the rev file as if we just read from it */
          SVN_ERR(svn_io_file_seek(rs->file, APR_SET, &rs->off, pool));
        }
    }

  return SVN_NO_ERROR;
}

/* Store the WINDOW read at OFFSET for the rep state RS in the current
 * FSFS session's cache. This will be a no-op if no cache has been given.
 * Temporary allocations will be made from SCRATCH_POOL. */
static svn_error_t *
set_cached_window(svn_txdelta_window_t *window,
                  struct rep_state *rs,
                  apr_off_t offset,
                  apr_pool_t *scratch_pool)
{
  if (rs->window_cache)
    {
      /* store the window and the first offset _past_ it */
      svn_fs_fs__txdelta_cached_window_t cached_window;

      cached_window.window = window;
      cached_window.end_offset = rs->off;

      /* but key it with the start offset because that is the known state
       * when we will look it up */
      return svn_cache__set(rs->window_cache,
                            get_window_key(rs, offset, scratch_pool),
                            &cached_window,
                            scratch_pool);
    }

  return SVN_NO_ERROR;
}

/* Read the WINDOW_P for the rep state RS from the current FSFS session's
 * cache. This will be a no-op and IS_CACHED will be set to FALSE if no
 * cache has been given. If a cache is available IS_CACHED will inform
 * the caller about the success of the lookup. Allocations (of the window
 * in particualar) will be made from POOL.
 */
static svn_error_t *
get_cached_combined_window(svn_stringbuf_t **window_p,
                           struct rep_state *rs,
                           svn_boolean_t *is_cached,
                           apr_pool_t *pool)
{
  if (! rs->combined_cache)
    {
      /* txdelta window has not been enabled */
      *is_cached = FALSE;
    }
  else
    {
      /* ask the cache for the desired txdelta window */
      return svn_cache__get((void **)window_p,
                            is_cached,
                            rs->combined_cache,
                            get_window_key(rs, rs->start, pool),
                            pool);
    }

  return SVN_NO_ERROR;
}

/* Store the WINDOW read at OFFSET for the rep state RS in the current
 * FSFS session's cache. This will be a no-op if no cache has been given.
 * Temporary allocations will be made from SCRATCH_POOL. */
static svn_error_t *
set_cached_combined_window(svn_stringbuf_t *window,
                           struct rep_state *rs,
                           apr_off_t offset,
                           apr_pool_t *scratch_pool)
{
  if (rs->combined_cache)
    {
      /* but key it with the start offset because that is the known state
       * when we will look it up */
      return svn_cache__set(rs->combined_cache,
                            get_window_key(rs, offset, scratch_pool),
                            window,
                            scratch_pool);
    }

  return SVN_NO_ERROR;
}

/* Build an array of rep_state structures in *LIST giving the delta
   reps from first_rep to a plain-text or self-compressed rep.  Set
   *SRC_STATE to the plain-text rep we find at the end of the chain,
   or to NULL if the final delta representation is self-compressed.
   The representation to start from is designated by filesystem FS, id
   ID, and representation REP.
   Also, set *WINDOW_P to the base window content for *LIST, if it
   could be found in cache. Otherwise, *LIST will contain the base
   representation for the whole delta chain.
   Finally, return the expanded size of the representation in
   *EXPANDED_SIZE. It will take care of cases where only the on-disk
   size is known.  */
static svn_error_t *
build_rep_list(apr_array_header_t **list,
               svn_stringbuf_t **window_p,
               struct rep_state **src_state,
               svn_filesize_t *expanded_size,
               svn_fs_t *fs,
               representation_t *first_rep,
               apr_pool_t *pool)
{
  representation_t rep;
  struct rep_state *rs = NULL;
  struct rep_args *rep_args;
  svn_boolean_t is_cached = FALSE;
  apr_file_t *last_file = NULL;
  svn_revnum_t last_revision;

  *list = apr_array_make(pool, 1, sizeof(struct rep_state *));
  rep = *first_rep;

  /* The value as stored in the data struct.
     0 is either for unknown length or actually zero length. */
  *expanded_size = first_rep->expanded_size;

  /* for the top-level rep, we need the rep_args */
  SVN_ERR(create_rep_state(&rs, &rep_args, &last_file,
                           &last_revision, &rep, fs, pool));

  /* Unknown size or empty representation?
     That implies the this being the first iteration.
     Usually size equals on-disk size, except for empty,
     compressed representations (delta, size = 4).
     Please note that for all non-empty deltas have
     a 4-byte header _plus_ some data. */
  if (*expanded_size == 0)
    if (! rep_args->is_delta || first_rep->size != 4)
      *expanded_size = first_rep->size;

  while (1)
    {
      /* fetch state, if that has not been done already */
      if (!rs)
        SVN_ERR(create_rep_state(&rs, &rep_args, &last_file,
                                &last_revision, &rep, fs, pool));

      SVN_ERR(get_cached_combined_window(window_p, rs, &is_cached, pool));
      if (is_cached)
        {
          /* We already have a reconstructed window in our cache.
             Write a pseudo rep_state with the full length. */
          rs->off = rs->start;
          rs->end = rs->start + (*window_p)->len;
          *src_state = rs;
          return SVN_NO_ERROR;
        }

      if (!rep_args->is_delta)
        {
          /* This is a plaintext, so just return the current rep_state. */
          *src_state = rs;
          return SVN_NO_ERROR;
        }

      /* Push this rep onto the list.  If it's self-compressed, we're done. */
      APR_ARRAY_PUSH(*list, struct rep_state *) = rs;
      if (rep_args->is_delta_vs_empty)
        {
          *src_state = NULL;
          return SVN_NO_ERROR;
        }

      rep.revision = rep_args->base_revision;
      rep.offset = rep_args->base_offset;
      rep.size = rep_args->base_length;
      rep.txn_id = NULL;

      rs = NULL;
    }
}


/* Create a rep_read_baton structure for node revision NODEREV in
   filesystem FS and store it in *RB_P.  If FULLTEXT_CACHE_KEY is not
   NULL, it is the rep's key in the fulltext cache, and a stringbuf
   must be allocated to store the text.  Perform all allocations in
   POOL.  If rep is mutable, it must be for file contents. */
static svn_error_t *
rep_read_get_baton(struct rep_read_baton **rb_p,
                   svn_fs_t *fs,
                   representation_t *rep,
                   pair_cache_key_t fulltext_cache_key,
                   apr_pool_t *pool)
{
  struct rep_read_baton *b;

  b = apr_pcalloc(pool, sizeof(*b));
  b->fs = fs;
  b->base_window = NULL;
  b->chunk_index = 0;
  b->buf = NULL;
  b->md5_checksum_ctx = svn_checksum_ctx_create(svn_checksum_md5, pool);
  b->checksum_finalized = FALSE;
  b->md5_checksum = svn_checksum_dup(rep->md5_checksum, pool);
  b->len = rep->expanded_size;
  b->off = 0;
  b->fulltext_cache_key = fulltext_cache_key;
  b->pool = svn_pool_create(pool);
  b->filehandle_pool = svn_pool_create(pool);

  SVN_ERR(build_rep_list(&b->rs_list, &b->base_window,
                         &b->src_state, &b->len, fs, rep,
                         b->filehandle_pool));

  if (SVN_IS_VALID_REVNUM(fulltext_cache_key.revision))
    b->current_fulltext = svn_stringbuf_create_ensure
                            ((apr_size_t)b->len,
                             b->filehandle_pool);
  else
    b->current_fulltext = NULL;

  /* Save our output baton. */
  *rb_p = b;

  return SVN_NO_ERROR;
}

/* Skip forwards to THIS_CHUNK in REP_STATE and then read the next delta
   window into *NWIN. */
static svn_error_t *
read_delta_window(svn_txdelta_window_t **nwin, int this_chunk,
                  struct rep_state *rs, apr_pool_t *pool)
{
  svn_stream_t *stream;
  svn_boolean_t is_cached;
  apr_off_t old_offset;

  SVN_ERR_ASSERT(rs->chunk_index <= this_chunk);

  /* RS->FILE may be shared between RS instances -> make sure we point
   * to the right data. */
  SVN_ERR(svn_io_file_seek(rs->file, APR_SET, &rs->off, pool));

  /* Skip windows to reach the current chunk if we aren't there yet. */
  while (rs->chunk_index < this_chunk)
    {
      SVN_ERR(svn_txdelta_skip_svndiff_window(rs->file, rs->ver, pool));
      rs->chunk_index++;
      SVN_ERR(get_file_offset(&rs->off, rs->file, pool));
      if (rs->off >= rs->end)
        return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                                _("Reading one svndiff window read "
                                  "beyond the end of the "
                                  "representation"));
    }

  /* Read the next window. But first, try to find it in the cache. */
  SVN_ERR(get_cached_window(nwin, rs, &is_cached, pool));
  if (is_cached)
    return SVN_NO_ERROR;

  /* Actually read the next window. */
  old_offset = rs->off;
  stream = svn_stream_from_aprfile2(rs->file, TRUE, pool);
  SVN_ERR(svn_txdelta_read_svndiff_window(nwin, stream, rs->ver, pool));
  rs->chunk_index++;
  SVN_ERR(get_file_offset(&rs->off, rs->file, pool));

  if (rs->off > rs->end)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Reading one svndiff window read beyond "
                              "the end of the representation"));

  /* the window has not been cached before, thus cache it now
   * (if caching is used for them at all) */
  return set_cached_window(*nwin, rs, old_offset, pool);
}

/* Read SIZE bytes from the representation RS and return it in *NWIN. */
static svn_error_t *
read_plain_window(svn_stringbuf_t **nwin, struct rep_state *rs,
                  apr_size_t size, apr_pool_t *pool)
{
  /* RS->FILE may be shared between RS instances -> make sure we point
   * to the right data. */
  SVN_ERR(svn_io_file_seek(rs->file, APR_SET, &rs->off, pool));

  /* Read the plain data. */
  *nwin = svn_stringbuf_create_ensure(size, pool);
  SVN_ERR(svn_io_file_read_full2(rs->file, (*nwin)->data, size, NULL, NULL,
                                 pool));
  (*nwin)->data[size] = 0;

  /* Update RS. */
  rs->off += (apr_off_t)size;

  return SVN_NO_ERROR;
}

/* Get the undeltified window that is a result of combining all deltas
   from the current desired representation identified in *RB with its
   base representation.  Store the window in *RESULT. */
static svn_error_t *
get_combined_window(svn_stringbuf_t **result,
                    struct rep_read_baton *rb)
{
  apr_pool_t *pool, *new_pool, *window_pool;
  int i;
  svn_txdelta_window_t *window;
  apr_array_header_t *windows;
  svn_stringbuf_t *source, *buf = rb->base_window;
  struct rep_state *rs;

  /* Read all windows that we need to combine. This is fine because
     the size of each window is relatively small (100kB) and skip-
     delta limits the number of deltas in a chain to well under 100.
     Stop early if one of them does not depend on its predecessors. */
  window_pool = svn_pool_create(rb->pool);
  windows = apr_array_make(window_pool, 0, sizeof(svn_txdelta_window_t *));
  for (i = 0; i < rb->rs_list->nelts; ++i)
    {
      rs = APR_ARRAY_IDX(rb->rs_list, i, struct rep_state *);
      SVN_ERR(read_delta_window(&window, rb->chunk_index, rs, window_pool));

      APR_ARRAY_PUSH(windows, svn_txdelta_window_t *) = window;
      if (window->src_ops == 0)
        {
          ++i;
          break;
        }
    }

  /* Combine in the windows from the other delta reps. */
  pool = svn_pool_create(rb->pool);
  for (--i; i >= 0; --i)
    {

      rs = APR_ARRAY_IDX(rb->rs_list, i, struct rep_state *);
      window = APR_ARRAY_IDX(windows, i, svn_txdelta_window_t *);

      /* Maybe, we've got a PLAIN start representation.  If we do, read
         as much data from it as the needed for the txdelta window's source
         view.
         Note that BUF / SOURCE may only be NULL in the first iteration.
         Also note that we may have short-cut reading the delta chain --
         in which case SRC_OPS is 0 and it might not be a PLAIN rep. */
      source = buf;
      if (source == NULL && rb->src_state != NULL && window->src_ops)
        SVN_ERR(read_plain_window(&source, rb->src_state, window->sview_len,
                                  pool));

      /* Combine this window with the current one. */
      new_pool = svn_pool_create(rb->pool);
      buf = svn_stringbuf_create_ensure(window->tview_len, new_pool);
      buf->len = window->tview_len;

      svn_txdelta_apply_instructions(window, source ? source->data : NULL,
                                     buf->data, &buf->len);
      if (buf->len != window->tview_len)
        return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                                _("svndiff window length is "
                                  "corrupt"));

      /* Cache windows only if the whole rep content could be read as a
         single chunk.  Only then will no other chunk need a deeper RS
         list than the cached chunk. */
      if ((rb->chunk_index == 0) && (rs->off == rs->end))
        SVN_ERR(set_cached_combined_window(buf, rs, rs->start, new_pool));

      /* Cycle pools so that we only need to hold three windows at a time. */
      svn_pool_destroy(pool);
      pool = new_pool;
    }

  svn_pool_destroy(window_pool);

  *result = buf;
  return SVN_NO_ERROR;
}

/* Returns whether or not the expanded fulltext of the file is cachable
 * based on its size SIZE.  The decision depends on the cache used by RB.
 */
static svn_boolean_t
fulltext_size_is_cachable(fs_fs_data_t *ffd, svn_filesize_t size)
{
  return (size < APR_SIZE_MAX)
      && svn_cache__is_cachable(ffd->fulltext_cache, (apr_size_t)size);
}

/* Close method used on streams returned by read_representation().
 */
static svn_error_t *
rep_read_contents_close(void *baton)
{
  struct rep_read_baton *rb = baton;

  svn_pool_destroy(rb->pool);
  svn_pool_destroy(rb->filehandle_pool);

  return SVN_NO_ERROR;
}

/* Return the next *LEN bytes of the rep and store them in *BUF. */
static svn_error_t *
get_contents(struct rep_read_baton *rb,
             char *buf,
             apr_size_t *len)
{
  apr_size_t copy_len, remaining = *len;
  char *cur = buf;
  struct rep_state *rs;

  /* Special case for when there are no delta reps, only a plain
     text. */
  if (rb->rs_list->nelts == 0)
    {
      copy_len = remaining;
      rs = rb->src_state;

      if (rb->base_window != NULL)
        {
          /* We got the desired rep directly from the cache.
             This is where we need the pseudo rep_state created
             by build_rep_list(). */
          apr_size_t offset = (apr_size_t)(rs->off - rs->start);
          if (copy_len + offset > rb->base_window->len)
            copy_len = offset < rb->base_window->len
                     ? rb->base_window->len - offset
                     : 0ul;

          memcpy (cur, rb->base_window->data + offset, copy_len);
        }
      else
        {
          if (((apr_off_t) copy_len) > rs->end - rs->off)
            copy_len = (apr_size_t) (rs->end - rs->off);
          SVN_ERR(svn_io_file_read_full2(rs->file, cur, copy_len, NULL,
                                         NULL, rb->pool));
        }

      rs->off += copy_len;
      *len = copy_len;
      return SVN_NO_ERROR;
    }

  while (remaining > 0)
    {
      /* If we have buffered data from a previous chunk, use that. */
      if (rb->buf)
        {
          /* Determine how much to copy from the buffer. */
          copy_len = rb->buf_len - rb->buf_pos;
          if (copy_len > remaining)
            copy_len = remaining;

          /* Actually copy the data. */
          memcpy(cur, rb->buf + rb->buf_pos, copy_len);
          rb->buf_pos += copy_len;
          cur += copy_len;
          remaining -= copy_len;

          /* If the buffer is all used up, clear it and empty the
             local pool. */
          if (rb->buf_pos == rb->buf_len)
            {
              svn_pool_clear(rb->pool);
              rb->buf = NULL;
            }
        }
      else
        {
          svn_stringbuf_t *sbuf = NULL;

          rs = APR_ARRAY_IDX(rb->rs_list, 0, struct rep_state *);
          if (rs->off == rs->end)
            break;

          /* Get more buffered data by evaluating a chunk. */
          SVN_ERR(get_combined_window(&sbuf, rb));

          rb->chunk_index++;
          rb->buf_len = sbuf->len;
          rb->buf = sbuf->data;
          rb->buf_pos = 0;
        }
    }

  *len = cur - buf;

  return SVN_NO_ERROR;
}

/* BATON is of type `rep_read_baton'; read the next *LEN bytes of the
   representation and store them in *BUF.  Sum as we read and verify
   the MD5 sum at the end. */
static svn_error_t *
rep_read_contents(void *baton,
                  char *buf,
                  apr_size_t *len)
{
  struct rep_read_baton *rb = baton;

  /* Get the next block of data. */
  SVN_ERR(get_contents(rb, buf, len));

  if (rb->current_fulltext)
    svn_stringbuf_appendbytes(rb->current_fulltext, buf, *len);

  /* Perform checksumming.  We want to check the checksum as soon as
     the last byte of data is read, in case the caller never performs
     a short read, but we don't want to finalize the MD5 context
     twice. */
  if (!rb->checksum_finalized)
    {
      SVN_ERR(svn_checksum_update(rb->md5_checksum_ctx, buf, *len));
      rb->off += *len;
      if (rb->off == rb->len)
        {
          svn_checksum_t *md5_checksum;

          rb->checksum_finalized = TRUE;
          SVN_ERR(svn_checksum_final(&md5_checksum, rb->md5_checksum_ctx,
                                     rb->pool));
          if (!svn_checksum_match(md5_checksum, rb->md5_checksum))
            return svn_error_create(SVN_ERR_FS_CORRUPT,
                    svn_checksum_mismatch_err(rb->md5_checksum, md5_checksum,
                        rb->pool,
                        _("Checksum mismatch while reading representation")),
                    NULL);
        }
    }

  if (rb->off == rb->len && rb->current_fulltext)
    {
      fs_fs_data_t *ffd = rb->fs->fsap_data;
      SVN_ERR(svn_cache__set(ffd->fulltext_cache, &rb->fulltext_cache_key,
                             rb->current_fulltext, rb->pool));
      rb->current_fulltext = NULL;
    }

  return SVN_NO_ERROR;
}


/* Return a stream in *CONTENTS_P that will read the contents of a
   representation stored at the location given by REP.  Appropriate
   for any kind of immutable representation, but only for file
   contents (not props or directory contents) in mutable
   representations.

   If REP is NULL, the representation is assumed to be empty, and the
   empty stream is returned.
*/
static svn_error_t *
read_representation(svn_stream_t **contents_p,
                    svn_fs_t *fs,
                    representation_t *rep,
                    apr_pool_t *pool)
{
  if (! rep)
    {
      *contents_p = svn_stream_empty(pool);
    }
  else
    {
      fs_fs_data_t *ffd = fs->fsap_data;
      pair_cache_key_t fulltext_cache_key = { 0 };
      svn_filesize_t len = rep->expanded_size ? rep->expanded_size : rep->size;
      struct rep_read_baton *rb;

      fulltext_cache_key.revision = rep->revision;
      fulltext_cache_key.second = rep->offset;
      if (ffd->fulltext_cache && SVN_IS_VALID_REVNUM(rep->revision)
          && fulltext_size_is_cachable(ffd, len))
        {
          svn_stringbuf_t *fulltext;
          svn_boolean_t is_cached;
          SVN_ERR(svn_cache__get((void **) &fulltext, &is_cached,
                                 ffd->fulltext_cache, &fulltext_cache_key,
                                 pool));
          if (is_cached)
            {
              *contents_p = svn_stream_from_stringbuf(fulltext, pool);
              return SVN_NO_ERROR;
            }
        }
      else
        fulltext_cache_key.revision = SVN_INVALID_REVNUM;

      SVN_ERR(rep_read_get_baton(&rb, fs, rep, fulltext_cache_key, pool));

      *contents_p = svn_stream_create(rb, pool);
      svn_stream_set_read(*contents_p, rep_read_contents);
      svn_stream_set_close(*contents_p, rep_read_contents_close);
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__get_contents(svn_stream_t **contents_p,
                        svn_fs_t *fs,
                        node_revision_t *noderev,
                        apr_pool_t *pool)
{
  return read_representation(contents_p, fs, noderev->data_rep, pool);
}

/* Baton used when reading delta windows. */
struct delta_read_baton
{
  struct rep_state *rs;
  svn_checksum_t *checksum;
};

/* This implements the svn_txdelta_next_window_fn_t interface. */
static svn_error_t *
delta_read_next_window(svn_txdelta_window_t **window, void *baton,
                       apr_pool_t *pool)
{
  struct delta_read_baton *drb = baton;

  if (drb->rs->off == drb->rs->end)
    {
      *window = NULL;
      return SVN_NO_ERROR;
    }

  return read_delta_window(window, drb->rs->chunk_index, drb->rs, pool);
}

/* This implements the svn_txdelta_md5_digest_fn_t interface. */
static const unsigned char *
delta_read_md5_digest(void *baton)
{
  struct delta_read_baton *drb = baton;

  if (drb->checksum->kind == svn_checksum_md5)
    return drb->checksum->digest;
  else
    return NULL;
}

svn_error_t *
svn_fs_fs__get_file_delta_stream(svn_txdelta_stream_t **stream_p,
                                 svn_fs_t *fs,
                                 node_revision_t *source,
                                 node_revision_t *target,
                                 apr_pool_t *pool)
{
  svn_stream_t *source_stream, *target_stream;

  /* Try a shortcut: if the target is stored as a delta against the source,
     then just use that delta. */
  if (source && source->data_rep && target->data_rep)
    {
      struct rep_state *rep_state;
      struct rep_args *rep_args;

      /* Read target's base rep if any. */
      SVN_ERR(create_rep_state(&rep_state, &rep_args, NULL, NULL,
                               target->data_rep, fs, pool));

      /* If that matches source, then use this delta as is.
         Note that we want an actual delta here.  E.g. a self-delta would
         not be good enough. */
      if (rep_args->is_delta
          && rep_args->base_revision == source->data_rep->revision
          && rep_args->base_offset == source->data_rep->offset)
        {
          /* Create the delta read baton. */
          struct delta_read_baton *drb = apr_pcalloc(pool, sizeof(*drb));
          drb->rs = rep_state;
          drb->checksum = svn_checksum_dup(target->data_rep->md5_checksum,
                                           pool);
          *stream_p = svn_txdelta_stream_create(drb, delta_read_next_window,
                                                delta_read_md5_digest, pool);
          return SVN_NO_ERROR;
        }
      else
        SVN_ERR(svn_io_file_close(rep_state->file, pool));
    }

  /* Read both fulltexts and construct a delta. */
  if (source)
    SVN_ERR(read_representation(&source_stream, fs, source->data_rep, pool));
  else
    source_stream = svn_stream_empty(pool);
  SVN_ERR(read_representation(&target_stream, fs, target->data_rep, pool));

  /* Because source and target stream will already verify their content,
   * there is no need to do this once more.  In particular if the stream
   * content is being fetched from cache. */
  svn_txdelta2(stream_p, source_stream, target_stream, FALSE, pool);

  return SVN_NO_ERROR;
}

/* Baton for cache_access_wrapper. Wraps the original parameters of
 * svn_fs_fs__try_process_file_content().
 */
typedef struct cache_access_wrapper_baton_t
{
  svn_fs_process_contents_func_t func;
  void* baton;
} cache_access_wrapper_baton_t;

/* Wrapper to translate between svn_fs_process_contents_func_t and
 * svn_cache__partial_getter_func_t.
 */
static svn_error_t *
cache_access_wrapper(void **out,
                     const void *data,
                     apr_size_t data_len,
                     void *baton,
                     apr_pool_t *pool)
{
  cache_access_wrapper_baton_t *wrapper_baton = baton;

  SVN_ERR(wrapper_baton->func((const unsigned char *)data,
                              data_len - 1, /* cache adds terminating 0 */
                              wrapper_baton->baton,
                              pool));

  /* non-NULL value to signal the calling cache that all went well */
  *out = baton;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__try_process_file_contents(svn_boolean_t *success,
                                     svn_fs_t *fs,
                                     node_revision_t *noderev,
                                     svn_fs_process_contents_func_t processor,
                                     void* baton,
                                     apr_pool_t *pool)
{
  representation_t *rep = noderev->data_rep;
  if (rep)
    {
      fs_fs_data_t *ffd = fs->fsap_data;
      pair_cache_key_t fulltext_cache_key = { 0 };

      fulltext_cache_key.revision = rep->revision;
      fulltext_cache_key.second = rep->offset;
      if (ffd->fulltext_cache && SVN_IS_VALID_REVNUM(rep->revision)
          && fulltext_size_is_cachable(ffd, rep->expanded_size))
        {
          cache_access_wrapper_baton_t wrapper_baton;
          void *dummy = NULL;

          wrapper_baton.func = processor;
          wrapper_baton.baton = baton;
          return svn_cache__get_partial(&dummy, success,
                                        ffd->fulltext_cache,
                                        &fulltext_cache_key,
                                        cache_access_wrapper,
                                        &wrapper_baton,
                                        pool);
        }
    }

  *success = FALSE;
  return SVN_NO_ERROR;
}

/* Fetch the contents of a directory into ENTRIES.  Values are stored
   as filename to string mappings; further conversion is necessary to
   convert them into svn_fs_dirent_t values. */
static svn_error_t *
get_dir_contents(apr_hash_t *entries,
                 svn_fs_t *fs,
                 node_revision_t *noderev,
                 apr_pool_t *pool)
{
  svn_stream_t *contents;

  if (noderev->data_rep && noderev->data_rep->txn_id)
    {
      const char *filename = path_txn_node_children(fs, noderev->id, pool);

      /* The representation is mutable.  Read the old directory
         contents from the mutable children file, followed by the
         changes we've made in this transaction. */
      SVN_ERR(svn_stream_open_readonly(&contents, filename, pool, pool));
      SVN_ERR(svn_hash_read2(entries, contents, SVN_HASH_TERMINATOR, pool));
      SVN_ERR(svn_hash_read_incremental(entries, contents, NULL, pool));
      SVN_ERR(svn_stream_close(contents));
    }
  else if (noderev->data_rep)
    {
      /* use a temporary pool for temp objects.
       * Also undeltify content before parsing it. Otherwise, we could only
       * parse it byte-by-byte.
       */
      apr_pool_t *text_pool = svn_pool_create(pool);
      apr_size_t len = noderev->data_rep->expanded_size
                     ? (apr_size_t)noderev->data_rep->expanded_size
                     : (apr_size_t)noderev->data_rep->size;
      svn_stringbuf_t *text = svn_stringbuf_create_ensure(len, text_pool);
      text->len = len;

      /* The representation is immutable.  Read it normally. */
      SVN_ERR(read_representation(&contents, fs, noderev->data_rep, text_pool));
      SVN_ERR(svn_stream_read(contents, text->data, &text->len));
      SVN_ERR(svn_stream_close(contents));

      /* de-serialize hash */
      contents = svn_stream_from_stringbuf(text, text_pool);
      SVN_ERR(svn_hash_read2(entries, contents, SVN_HASH_TERMINATOR, pool));

      svn_pool_destroy(text_pool);
    }

  return SVN_NO_ERROR;
}


static const char *
unparse_dir_entry(svn_node_kind_t kind, const svn_fs_id_t *id,
                  apr_pool_t *pool)
{
  return apr_psprintf(pool, "%s %s",
                      (kind == svn_node_file) ? KIND_FILE : KIND_DIR,
                      svn_fs_fs__id_unparse(id, pool)->data);
}

/* Given a hash ENTRIES of dirent structions, return a hash in
   *STR_ENTRIES_P, that has svn_string_t as the values in the format
   specified by the fs_fs directory contents file.  Perform
   allocations in POOL. */
static svn_error_t *
unparse_dir_entries(apr_hash_t **str_entries_p,
                    apr_hash_t *entries,
                    apr_pool_t *pool)
{
  apr_hash_index_t *hi;

  /* For now, we use a our own hash function to ensure that we get a
   * (largely) stable order when serializing the data.  It also gives
   * us some performance improvement.
   *
   * ### TODO ###
   * Use some sorted or other fixed order data container.
   */
  *str_entries_p = svn_hash__make(pool);

  for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi))
    {
      const void *key;
      apr_ssize_t klen;
      svn_fs_dirent_t *dirent = svn__apr_hash_index_val(hi);
      const char *new_val;

      apr_hash_this(hi, &key, &klen, NULL);
      new_val = unparse_dir_entry(dirent->kind, dirent->id, pool);
      apr_hash_set(*str_entries_p, key, klen,
                   svn_string_create(new_val, pool));
    }

  return SVN_NO_ERROR;
}


/* Given a hash STR_ENTRIES with values as svn_string_t as specified
   in an FSFS directory contents listing, return a hash of dirents in
   *ENTRIES_P.  Perform allocations in POOL. */
static svn_error_t *
parse_dir_entries(apr_hash_t **entries_p,
                  apr_hash_t *str_entries,
                  const char *unparsed_id,
                  apr_pool_t *pool)
{
  apr_hash_index_t *hi;

  *entries_p = apr_hash_make(pool);

  /* Translate the string dir entries into real entries. */
  for (hi = apr_hash_first(pool, str_entries); hi; hi = apr_hash_next(hi))
    {
      const char *name = svn__apr_hash_index_key(hi);
      svn_string_t *str_val = svn__apr_hash_index_val(hi);
      char *str, *last_str;
      svn_fs_dirent_t *dirent = apr_pcalloc(pool, sizeof(*dirent));

      last_str = apr_pstrdup(pool, str_val->data);
      dirent->name = apr_pstrdup(pool, name);

      str = svn_cstring_tokenize(" ", &last_str);
      if (str == NULL)
        return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                 _("Directory entry corrupt in '%s'"),
                                 unparsed_id);

      if (strcmp(str, KIND_FILE) == 0)
        {
          dirent->kind = svn_node_file;
        }
      else if (strcmp(str, KIND_DIR) == 0)
        {
          dirent->kind = svn_node_dir;
        }
      else
        {
          return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                   _("Directory entry corrupt in '%s'"),
                                   unparsed_id);
        }

      str = svn_cstring_tokenize(" ", &last_str);
      if (str == NULL)
          return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                   _("Directory entry corrupt in '%s'"),
                                   unparsed_id);

      dirent->id = svn_fs_fs__id_parse(str, strlen(str), pool);

      svn_hash_sets(*entries_p, dirent->name, dirent);
    }

  return SVN_NO_ERROR;
}

/* Return the cache object in FS responsible to storing the directory
 * the NODEREV. If none exists, return NULL. */
static svn_cache__t *
locate_dir_cache(svn_fs_t *fs,
                 node_revision_t *noderev)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  return svn_fs_fs__id_txn_id(noderev->id)
      ? ffd->txn_dir_cache
      : ffd->dir_cache;
}

svn_error_t *
svn_fs_fs__rep_contents_dir(apr_hash_t **entries_p,
                            svn_fs_t *fs,
                            node_revision_t *noderev,
                            apr_pool_t *pool)
{
  const char *unparsed_id = NULL;
  apr_hash_t *unparsed_entries, *parsed_entries;

  /* find the cache we may use */
  svn_cache__t *cache = locate_dir_cache(fs, noderev);
  if (cache)
    {
      svn_boolean_t found;

      unparsed_id = svn_fs_fs__id_unparse(noderev->id, pool)->data;
      SVN_ERR(svn_cache__get((void **) entries_p, &found, cache,
                             unparsed_id, pool));
      if (found)
        return SVN_NO_ERROR;
    }

  /* Read in the directory hash. */
  unparsed_entries = apr_hash_make(pool);
  SVN_ERR(get_dir_contents(unparsed_entries, fs, noderev, pool));
  SVN_ERR(parse_dir_entries(&parsed_entries, unparsed_entries,
                            unparsed_id, pool));

  /* Update the cache, if we are to use one. */
  if (cache)
    SVN_ERR(svn_cache__set(cache, unparsed_id, parsed_entries, pool));

  *entries_p = parsed_entries;
  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__rep_contents_dir_entry(svn_fs_dirent_t **dirent,
                                  svn_fs_t *fs,
                                  node_revision_t *noderev,
                                  const char *name,
                                  apr_pool_t *result_pool,
                                  apr_pool_t *scratch_pool)
{
  svn_boolean_t found = FALSE;

  /* find the cache we may use */
  svn_cache__t *cache = locate_dir_cache(fs, noderev);
  if (cache)
    {
      const char *unparsed_id =
        svn_fs_fs__id_unparse(noderev->id, scratch_pool)->data;

      /* Cache lookup. */
      SVN_ERR(svn_cache__get_partial((void **)dirent,
                                     &found,
                                     cache,
                                     unparsed_id,
                                     svn_fs_fs__extract_dir_entry,
                                     (void*)name,
                                     result_pool));
    }

  /* fetch data from disk if we did not find it in the cache */
  if (! found)
    {
      apr_hash_t *entries;
      svn_fs_dirent_t *entry;
      svn_fs_dirent_t *entry_copy = NULL;

      /* read the dir from the file system. It will probably be put it
         into the cache for faster lookup in future calls. */
      SVN_ERR(svn_fs_fs__rep_contents_dir(&entries, fs, noderev,
                                          scratch_pool));

      /* find desired entry and return a copy in POOL, if found */
      entry = svn_hash_gets(entries, name);
      if (entry != NULL)
        {
          entry_copy = apr_palloc(result_pool, sizeof(*entry_copy));
          entry_copy->name = apr_pstrdup(result_pool, entry->name);
          entry_copy->id = svn_fs_fs__id_copy(entry->id, result_pool);
          entry_copy->kind = entry->kind;
        }

      *dirent = entry_copy;
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__get_proplist(apr_hash_t **proplist_p,
                        svn_fs_t *fs,
                        node_revision_t *noderev,
                        apr_pool_t *pool)
{
  apr_hash_t *proplist;
  svn_stream_t *stream;

  if (noderev->prop_rep && noderev->prop_rep->txn_id)
    {
      const char *filename = path_txn_node_props(fs, noderev->id, pool);
      proplist = apr_hash_make(pool);

      SVN_ERR(svn_stream_open_readonly(&stream, filename, pool, pool));
      SVN_ERR(svn_hash_read2(proplist, stream, SVN_HASH_TERMINATOR, pool));
      SVN_ERR(svn_stream_close(stream));
    }
  else if (noderev->prop_rep)
    {
      fs_fs_data_t *ffd = fs->fsap_data;
      representation_t *rep = noderev->prop_rep;
      pair_cache_key_t key = { 0 };

      key.revision = rep->revision;
      key.second = rep->offset;
      if (ffd->properties_cache && SVN_IS_VALID_REVNUM(rep->revision))
        {
          svn_boolean_t is_cached;
          SVN_ERR(svn_cache__get((void **) proplist_p, &is_cached,
                                 ffd->properties_cache, &key, pool));
          if (is_cached)
            return SVN_NO_ERROR;
        }

      proplist = apr_hash_make(pool);
      SVN_ERR(read_representation(&stream, fs, noderev->prop_rep, pool));
      SVN_ERR(svn_hash_read2(proplist, stream, SVN_HASH_TERMINATOR, pool));
      SVN_ERR(svn_stream_close(stream));

      if (ffd->properties_cache && SVN_IS_VALID_REVNUM(rep->revision))
        SVN_ERR(svn_cache__set(ffd->properties_cache, &key, proplist, pool));
    }
  else
    {
      /* return an empty prop list if the node doesn't have any props */
      proplist = apr_hash_make(pool);
    }

  *proplist_p = proplist;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__file_length(svn_filesize_t *length,
                       node_revision_t *noderev,
                       apr_pool_t *pool)
{
  if (noderev->data_rep)
    *length = noderev->data_rep->expanded_size;
  else
    *length = 0;

  return SVN_NO_ERROR;
}

svn_boolean_t
svn_fs_fs__noderev_same_rep_key(representation_t *a,
                                representation_t *b)
{
  if (a == b)
    return TRUE;

  if (a == NULL || b == NULL)
    return FALSE;

  if (a->offset != b->offset)
    return FALSE;

  if (a->revision != b->revision)
    return FALSE;

  if (a->uniquifier == b->uniquifier)
    return TRUE;

  if (a->uniquifier == NULL || b->uniquifier == NULL)
    return FALSE;

  return strcmp(a->uniquifier, b->uniquifier) == 0;
}

svn_error_t *
svn_fs_fs__file_checksum(svn_checksum_t **checksum,
                         node_revision_t *noderev,
                         svn_checksum_kind_t kind,
                         apr_pool_t *pool)
{
  if (noderev->data_rep)
    {
      switch(kind)
        {
          case svn_checksum_md5:
            *checksum = svn_checksum_dup(noderev->data_rep->md5_checksum,
                                         pool);
            break;
          case svn_checksum_sha1:
            *checksum = svn_checksum_dup(noderev->data_rep->sha1_checksum,
                                         pool);
            break;
          default:
            *checksum = NULL;
        }
    }
  else
    *checksum = NULL;

  return SVN_NO_ERROR;
}

representation_t *
svn_fs_fs__rep_copy(representation_t *rep,
                    apr_pool_t *pool)
{
  representation_t *rep_new;

  if (rep == NULL)
    return NULL;

  rep_new = apr_pcalloc(pool, sizeof(*rep_new));

  memcpy(rep_new, rep, sizeof(*rep_new));
  rep_new->md5_checksum = svn_checksum_dup(rep->md5_checksum, pool);
  rep_new->sha1_checksum = svn_checksum_dup(rep->sha1_checksum, pool);
  rep_new->uniquifier = apr_pstrdup(pool, rep->uniquifier);

  return rep_new;
}

/* Merge the internal-use-only CHANGE into a hash of public-FS
   svn_fs_path_change2_t CHANGES, collapsing multiple changes into a
   single summarical (is that real word?) change per path.  Also keep
   the COPYFROM_CACHE up to date with new adds and replaces.  */
static svn_error_t *
fold_change(apr_hash_t *changes,
            const change_t *change,
            apr_hash_t *copyfrom_cache)
{
  apr_pool_t *pool = apr_hash_pool_get(changes);
  svn_fs_path_change2_t *old_change, *new_change;
  const char *path;
  apr_size_t path_len = strlen(change->path);

  if ((old_change = apr_hash_get(changes, change->path, path_len)))
    {
      /* This path already exists in the hash, so we have to merge
         this change into the already existing one. */

      /* Sanity check:  only allow NULL node revision ID in the
         `reset' case. */
      if ((! change->noderev_id) && (change->kind != svn_fs_path_change_reset))
        return svn_error_create
          (SVN_ERR_FS_CORRUPT, NULL,
           _("Missing required node revision ID"));

      /* Sanity check: we should be talking about the same node
         revision ID as our last change except where the last change
         was a deletion. */
      if (change->noderev_id
          && (! svn_fs_fs__id_eq(old_change->node_rev_id, change->noderev_id))
          && (old_change->change_kind != svn_fs_path_change_delete))
        return svn_error_create
          (SVN_ERR_FS_CORRUPT, NULL,
           _("Invalid change ordering: new node revision ID "
             "without delete"));

      /* Sanity check: an add, replacement, or reset must be the first
         thing to follow a deletion. */
      if ((old_change->change_kind == svn_fs_path_change_delete)
          && (! ((change->kind == svn_fs_path_change_replace)
                 || (change->kind == svn_fs_path_change_reset)
                 || (change->kind == svn_fs_path_change_add))))
        return svn_error_create
          (SVN_ERR_FS_CORRUPT, NULL,
           _("Invalid change ordering: non-add change on deleted path"));

      /* Sanity check: an add can't follow anything except
         a delete or reset.  */
      if ((change->kind == svn_fs_path_change_add)
          && (old_change->change_kind != svn_fs_path_change_delete)
          && (old_change->change_kind != svn_fs_path_change_reset))
        return svn_error_create
          (SVN_ERR_FS_CORRUPT, NULL,
           _("Invalid change ordering: add change on preexisting path"));

      /* Now, merge that change in. */
      switch (change->kind)
        {
        case svn_fs_path_change_reset:
          /* A reset here will simply remove the path change from the
             hash. */
          old_change = NULL;
          break;

        case svn_fs_path_change_delete:
          if (old_change->change_kind == svn_fs_path_change_add)
            {
              /* If the path was introduced in this transaction via an
                 add, and we are deleting it, just remove the path
                 altogether. */
              old_change = NULL;
            }
          else
            {
              /* A deletion overrules all previous changes. */
              old_change->change_kind = svn_fs_path_change_delete;
              old_change->text_mod = change->text_mod;
              old_change->prop_mod = change->prop_mod;
              old_change->copyfrom_rev = SVN_INVALID_REVNUM;
              old_change->copyfrom_path = NULL;
            }
          break;

        case svn_fs_path_change_add:
        case svn_fs_path_change_replace:
          /* An add at this point must be following a previous delete,
             so treat it just like a replace. */
          old_change->change_kind = svn_fs_path_change_replace;
          old_change->node_rev_id = svn_fs_fs__id_copy(change->noderev_id,
                                                       pool);
          old_change->text_mod = change->text_mod;
          old_change->prop_mod = change->prop_mod;
          if (change->copyfrom_rev == SVN_INVALID_REVNUM)
            {
              old_change->copyfrom_rev = SVN_INVALID_REVNUM;
              old_change->copyfrom_path = NULL;
            }
          else
            {
              old_change->copyfrom_rev = change->copyfrom_rev;
              old_change->copyfrom_path = apr_pstrdup(pool,
                                                      change->copyfrom_path);
            }
          break;

        case svn_fs_path_change_modify:
        default:
          if (change->text_mod)
            old_change->text_mod = TRUE;
          if (change->prop_mod)
            old_change->prop_mod = TRUE;
          break;
        }

      /* Point our new_change to our (possibly modified) old_change. */
      new_change = old_change;
    }
  else
    {
      /* This change is new to the hash, so make a new public change
         structure from the internal one (in the hash's pool), and dup
         the path into the hash's pool, too. */
      new_change = apr_pcalloc(pool, sizeof(*new_change));
      new_change->node_rev_id = svn_fs_fs__id_copy(change->noderev_id, pool);
      new_change->change_kind = change->kind;
      new_change->text_mod = change->text_mod;
      new_change->prop_mod = change->prop_mod;
      /* In FSFS, copyfrom_known is *always* true, since we've always
       * stored copyfroms in changed paths lists. */
      new_change->copyfrom_known = TRUE;
      if (change->copyfrom_rev != SVN_INVALID_REVNUM)
        {
          new_change->copyfrom_rev = change->copyfrom_rev;
          new_change->copyfrom_path = apr_pstrdup(pool, change->copyfrom_path);
        }
      else
        {
          new_change->copyfrom_rev = SVN_INVALID_REVNUM;
          new_change->copyfrom_path = NULL;
        }
    }

  if (new_change)
    new_change->node_kind = change->node_kind;

  /* Add (or update) this path.

     Note: this key might already be present, and it would be nice to
     re-use its value, but there is no way to fetch it. The API makes no
     guarantees that this (new) key will not be retained. Thus, we (again)
     copy the key into the target pool to ensure a proper lifetime.  */
  path = apr_pstrmemdup(pool, change->path, path_len);
  apr_hash_set(changes, path, path_len, new_change);

  /* Update the copyfrom cache, if any. */
  if (copyfrom_cache)
    {
      apr_pool_t *copyfrom_pool = apr_hash_pool_get(copyfrom_cache);
      const char *copyfrom_string = NULL, *copyfrom_key = path;
      if (new_change)
        {
          if (SVN_IS_VALID_REVNUM(new_change->copyfrom_rev))
            copyfrom_string = apr_psprintf(copyfrom_pool, "%ld %s",
                                           new_change->copyfrom_rev,
                                           new_change->copyfrom_path);
          else
            copyfrom_string = "";
        }
      /* We need to allocate a copy of the key in the copyfrom_pool if
       * we're not doing a deletion and if it isn't already there. */
      if (   copyfrom_string
          && (   ! apr_hash_count(copyfrom_cache)
              || ! apr_hash_get(copyfrom_cache, copyfrom_key, path_len)))
        copyfrom_key = apr_pstrmemdup(copyfrom_pool, copyfrom_key, path_len);

      apr_hash_set(copyfrom_cache, copyfrom_key, path_len,
                   copyfrom_string);
    }

  return SVN_NO_ERROR;
}

/* The 256 is an arbitrary size large enough to hold the node id and the
 * various flags. */
#define MAX_CHANGE_LINE_LEN FSFS_MAX_PATH_LEN + 256

/* Read the next entry in the changes record from file FILE and store
   the resulting change in *CHANGE_P.  If there is no next record,
   store NULL there.  Perform all allocations from POOL. */
static svn_error_t *
read_change(change_t **change_p,
            apr_file_t *file,
            apr_pool_t *pool)
{
  char buf[MAX_CHANGE_LINE_LEN];
  apr_size_t len = sizeof(buf);
  change_t *change;
  char *str, *last_str = buf, *kind_str;
  svn_error_t *err;

  /* Default return value. */
  *change_p = NULL;

  err = svn_io_read_length_line(file, buf, &len, pool);

  /* Check for a blank line. */
  if (err || (len == 0))
    {
      if (err && APR_STATUS_IS_EOF(err->apr_err))
        {
          svn_error_clear(err);
          return SVN_NO_ERROR;
        }
      if ((len == 0) && (! err))
        return SVN_NO_ERROR;
      return svn_error_trace(err);
    }

  change = apr_pcalloc(pool, sizeof(*change));

  /* Get the node-id of the change. */
  str = svn_cstring_tokenize(" ", &last_str);
  if (str == NULL)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Invalid changes line in rev-file"));

  change->noderev_id = svn_fs_fs__id_parse(str, strlen(str), pool);
  if (change->noderev_id == NULL)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Invalid changes line in rev-file"));

  /* Get the change type. */
  str = svn_cstring_tokenize(" ", &last_str);
  if (str == NULL)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Invalid changes line in rev-file"));

  /* Don't bother to check the format number before looking for
   * node-kinds: just read them if you find them. */
  change->node_kind = svn_node_unknown;
  kind_str = strchr(str, '-');
  if (kind_str)
    {
      /* Cap off the end of "str" (the action). */
      *kind_str = '\0';
      kind_str++;
      if (strcmp(kind_str, KIND_FILE) == 0)
        change->node_kind = svn_node_file;
      else if (strcmp(kind_str, KIND_DIR) == 0)
        change->node_kind = svn_node_dir;
      else
        return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                                _("Invalid changes line in rev-file"));
    }

  if (strcmp(str, ACTION_MODIFY) == 0)
    {
      change->kind = svn_fs_path_change_modify;
    }
  else if (strcmp(str, ACTION_ADD) == 0)
    {
      change->kind = svn_fs_path_change_add;
    }
  else if (strcmp(str, ACTION_DELETE) == 0)
    {
      change->kind = svn_fs_path_change_delete;
    }
  else if (strcmp(str, ACTION_REPLACE) == 0)
    {
      change->kind = svn_fs_path_change_replace;
    }
  else if (strcmp(str, ACTION_RESET) == 0)
    {
      change->kind = svn_fs_path_change_reset;
    }
  else
    {
      return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                              _("Invalid change kind in rev file"));
    }

  /* Get the text-mod flag. */
  str = svn_cstring_tokenize(" ", &last_str);
  if (str == NULL)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Invalid changes line in rev-file"));

  if (strcmp(str, FLAG_TRUE) == 0)
    {
      change->text_mod = TRUE;
    }
  else if (strcmp(str, FLAG_FALSE) == 0)
    {
      change->text_mod = FALSE;
    }
  else
    {
      return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                              _("Invalid text-mod flag in rev-file"));
    }

  /* Get the prop-mod flag. */
  str = svn_cstring_tokenize(" ", &last_str);
  if (str == NULL)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Invalid changes line in rev-file"));

  if (strcmp(str, FLAG_TRUE) == 0)
    {
      change->prop_mod = TRUE;
    }
  else if (strcmp(str, FLAG_FALSE) == 0)
    {
      change->prop_mod = FALSE;
    }
  else
    {
      return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                              _("Invalid prop-mod flag in rev-file"));
    }

  /* Get the changed path. */
  change->path = apr_pstrdup(pool, last_str);


  /* Read the next line, the copyfrom line. */
  len = sizeof(buf);
  SVN_ERR(svn_io_read_length_line(file, buf, &len, pool));

  if (len == 0)
    {
      change->copyfrom_rev = SVN_INVALID_REVNUM;
      change->copyfrom_path = NULL;
    }
  else
    {
      last_str = buf;
      str = svn_cstring_tokenize(" ", &last_str);
      if (! str)
        return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                                _("Invalid changes line in rev-file"));
      change->copyfrom_rev = SVN_STR_TO_REV(str);

      if (! last_str)
        return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                                _("Invalid changes line in rev-file"));

      change->copyfrom_path = apr_pstrdup(pool, last_str);
    }

  *change_p = change;

  return SVN_NO_ERROR;
}

/* Examine all the changed path entries in CHANGES and store them in
   *CHANGED_PATHS.  Folding is done to remove redundant or unnecessary
   *data.  Store a hash of paths to copyfrom "REV PATH" strings in
   COPYFROM_HASH if it is non-NULL.  If PREFOLDED is true, assume that
   the changed-path entries have already been folded (by
   write_final_changed_path_info) and may be out of order, so we shouldn't
   remove children of replaced or deleted directories.  Do all
   allocations in POOL. */
static svn_error_t *
process_changes(apr_hash_t *changed_paths,
                apr_hash_t *copyfrom_cache,
                apr_array_header_t *changes,
                svn_boolean_t prefolded,
                apr_pool_t *pool)
{
  apr_pool_t *iterpool = svn_pool_create(pool);
  int i;

  /* Read in the changes one by one, folding them into our local hash
     as necessary. */

  for (i = 0; i < changes->nelts; ++i)
    {
      change_t *change = APR_ARRAY_IDX(changes, i, change_t *);

      SVN_ERR(fold_change(changed_paths, change, copyfrom_cache));

      /* Now, if our change was a deletion or replacement, we have to
         blow away any changes thus far on paths that are (or, were)
         children of this path.
         ### i won't bother with another iteration pool here -- at
         most we talking about a few extra dups of paths into what
         is already a temporary subpool.
      */

      if (((change->kind == svn_fs_path_change_delete)
           || (change->kind == svn_fs_path_change_replace))
          && ! prefolded)
        {
          apr_hash_index_t *hi;

          /* a potential child path must contain at least 2 more chars
             (the path separator plus at least one char for the name).
             Also, we should not assume that all paths have been normalized
             i.e. some might have trailing path separators.
          */
          apr_ssize_t change_path_len = strlen(change->path);
          apr_ssize_t min_child_len = change_path_len == 0
                                    ? 1
                                    : change->path[change_path_len-1] == '/'
                                        ? change_path_len + 1
                                        : change_path_len + 2;

          /* CAUTION: This is the inner loop of an O(n^2) algorithm.
             The number of changes to process may be >> 1000.
             Therefore, keep the inner loop as tight as possible.
          */
          for (hi = apr_hash_first(iterpool, changed_paths);
               hi;
               hi = apr_hash_next(hi))
            {
              /* KEY is the path. */
              const void *path;
              apr_ssize_t klen;
              apr_hash_this(hi, &path, &klen, NULL);

              /* If we come across a child of our path, remove it.
                 Call svn_dirent_is_child only if there is a chance that
                 this is actually a sub-path.
               */
              if (   klen >= min_child_len
                  && svn_dirent_is_child(change->path, path, iterpool))
                apr_hash_set(changed_paths, path, klen, NULL);
            }
        }

      /* Clear the per-iteration subpool. */
      svn_pool_clear(iterpool);
    }

  /* Destroy the per-iteration subpool. */
  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

/* Fetch all the changes from FILE and store them in *CHANGES.  Do all
   allocations in POOL. */
static svn_error_t *
read_all_changes(apr_array_header_t **changes,
                 apr_file_t *file,
                 apr_pool_t *pool)
{
  change_t *change;

  /* pre-allocate enough room for most change lists
     (will be auto-expanded as necessary) */
  *changes = apr_array_make(pool, 30, sizeof(change_t *));

  SVN_ERR(read_change(&change, file, pool));
  while (change)
    {
      APR_ARRAY_PUSH(*changes, change_t*) = change;
      SVN_ERR(read_change(&change, file, pool));
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__txn_changes_fetch(apr_hash_t **changed_paths_p,
                             svn_fs_t *fs,
                             const char *txn_id,
                             apr_pool_t *pool)
{
  apr_file_t *file;
  apr_hash_t *changed_paths = apr_hash_make(pool);
  apr_array_header_t *changes;
  apr_pool_t *scratch_pool = svn_pool_create(pool);

  SVN_ERR(svn_io_file_open(&file, path_txn_changes(fs, txn_id, pool),
                           APR_READ | APR_BUFFERED, APR_OS_DEFAULT, pool));

  SVN_ERR(read_all_changes(&changes, file, scratch_pool));
  SVN_ERR(process_changes(changed_paths, NULL, changes, FALSE, pool));
  svn_pool_destroy(scratch_pool);

  SVN_ERR(svn_io_file_close(file, pool));

  *changed_paths_p = changed_paths;

  return SVN_NO_ERROR;
}

/* Fetch the list of change in revision REV in FS and return it in *CHANGES.
 * Allocate the result in POOL.
 */
static svn_error_t *
get_changes(apr_array_header_t **changes,
            svn_fs_t *fs,
            svn_revnum_t rev,
            apr_pool_t *pool)
{
  apr_off_t changes_offset;
  apr_file_t *revision_file;
  svn_boolean_t found;
  fs_fs_data_t *ffd = fs->fsap_data;

  /* try cache lookup first */

  if (ffd->changes_cache)
    {
      SVN_ERR(svn_cache__get((void **) changes, &found, ffd->changes_cache,
                             &rev, pool));
      if (found)
        return SVN_NO_ERROR;
    }

  /* read changes from revision file */

  SVN_ERR(ensure_revision_exists(fs, rev, pool));

  SVN_ERR(open_pack_or_rev_file(&revision_file, fs, rev, pool));

  SVN_ERR(get_root_changes_offset(NULL, &changes_offset, revision_file, fs,
                                  rev, pool));

  SVN_ERR(svn_io_file_seek(revision_file, APR_SET, &changes_offset, pool));
  SVN_ERR(read_all_changes(changes, revision_file, pool));

  SVN_ERR(svn_io_file_close(revision_file, pool));

  /* cache for future reference */

  if (ffd->changes_cache)
    SVN_ERR(svn_cache__set(ffd->changes_cache, &rev, *changes, pool));

  return SVN_NO_ERROR;
}


svn_error_t *
svn_fs_fs__paths_changed(apr_hash_t **changed_paths_p,
                         svn_fs_t *fs,
                         svn_revnum_t rev,
                         apr_hash_t *copyfrom_cache,
                         apr_pool_t *pool)
{
  apr_hash_t *changed_paths;
  apr_array_header_t *changes;
  apr_pool_t *scratch_pool = svn_pool_create(pool);

  SVN_ERR(get_changes(&changes, fs, rev, scratch_pool));

  changed_paths = svn_hash__make(pool);

  SVN_ERR(process_changes(changed_paths, copyfrom_cache, changes,
                          TRUE, pool));
  svn_pool_destroy(scratch_pool);

  *changed_paths_p = changed_paths;

  return SVN_NO_ERROR;
}

/* Copy a revision node-rev SRC into the current transaction TXN_ID in
   the filesystem FS.  This is only used to create the root of a transaction.
   Allocations are from POOL.  */
static svn_error_t *
create_new_txn_noderev_from_rev(svn_fs_t *fs,
                                const char *txn_id,
                                svn_fs_id_t *src,
                                apr_pool_t *pool)
{
  node_revision_t *noderev;
  const char *node_id, *copy_id;

  SVN_ERR(svn_fs_fs__get_node_revision(&noderev, fs, src, pool));

  if (svn_fs_fs__id_txn_id(noderev->id))
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Copying from transactions not allowed"));

  noderev->predecessor_id = noderev->id;
  noderev->predecessor_count++;
  noderev->copyfrom_path = NULL;
  noderev->copyfrom_rev = SVN_INVALID_REVNUM;

  /* For the transaction root, the copyroot never changes. */

  node_id = svn_fs_fs__id_node_id(noderev->id);
  copy_id = svn_fs_fs__id_copy_id(noderev->id);
  noderev->id = svn_fs_fs__id_txn_create(node_id, copy_id, txn_id, pool);

  return svn_fs_fs__put_node_revision(fs, noderev->id, noderev, TRUE, pool);
}

/* A structure used by get_and_increment_txn_key_body(). */
struct get_and_increment_txn_key_baton {
  svn_fs_t *fs;
  char *txn_id;
  apr_pool_t *pool;
};

/* Callback used in the implementation of create_txn_dir().  This gets
   the current base 36 value in PATH_TXN_CURRENT and increments it.
   It returns the original value by the baton. */
static svn_error_t *
get_and_increment_txn_key_body(void *baton, apr_pool_t *pool)
{
  struct get_and_increment_txn_key_baton *cb = baton;
  const char *txn_current_filename = path_txn_current(cb->fs, pool);
  const char *tmp_filename;
  char next_txn_id[MAX_KEY_SIZE+3];
  apr_size_t len;

  svn_stringbuf_t *buf;
  SVN_ERR(read_content(&buf, txn_current_filename, cb->pool));

  /* remove trailing newlines */
  svn_stringbuf_strip_whitespace(buf);
  cb->txn_id = buf->data;
  len = buf->len;

  /* Increment the key and add a trailing \n to the string so the
     txn-current file has a newline in it. */
  svn_fs_fs__next_key(cb->txn_id, &len, next_txn_id);
  next_txn_id[len] = '\n';
  ++len;
  next_txn_id[len] = '\0';

  SVN_ERR(svn_io_write_unique(&tmp_filename,
                              svn_dirent_dirname(txn_current_filename, pool),
                              next_txn_id, len, svn_io_file_del_none, pool));
  SVN_ERR(move_into_place(tmp_filename, txn_current_filename,
                          txn_current_filename, pool));

  return SVN_NO_ERROR;
}

/* Create a unique directory for a transaction in FS based on revision
   REV.  Return the ID for this transaction in *ID_P.  Use a sequence
   value in the transaction ID to prevent reuse of transaction IDs. */
static svn_error_t *
create_txn_dir(const char **id_p, svn_fs_t *fs, svn_revnum_t rev,
               apr_pool_t *pool)
{
  struct get_and_increment_txn_key_baton cb;
  const char *txn_dir;

  /* Get the current transaction sequence value, which is a base-36
     number, from the txn-current file, and write an
     incremented value back out to the file.  Place the revision
     number the transaction is based off into the transaction id. */
  cb.pool = pool;
  cb.fs = fs;
  SVN_ERR(with_txn_current_lock(fs,
                                get_and_increment_txn_key_body,
                                &cb,
                                pool));
  *id_p = apr_psprintf(pool, "%ld-%s", rev, cb.txn_id);

  txn_dir = svn_dirent_join_many(pool,
                                 fs->path,
                                 PATH_TXNS_DIR,
                                 apr_pstrcat(pool, *id_p, PATH_EXT_TXN,
                                             (char *)NULL),
                                 NULL);

  return svn_io_dir_make(txn_dir, APR_OS_DEFAULT, pool);
}

/* Create a unique directory for a transaction in FS based on revision
   REV.  Return the ID for this transaction in *ID_P.  This
   implementation is used in svn 1.4 and earlier repositories and is
   kept in 1.5 and greater to support the --pre-1.4-compatible and
   --pre-1.5-compatible repository creation options.  Reused
   transaction IDs are possible with this implementation. */
static svn_error_t *
create_txn_dir_pre_1_5(const char **id_p, svn_fs_t *fs, svn_revnum_t rev,
                       apr_pool_t *pool)
{
  unsigned int i;
  apr_pool_t *subpool;
  const char *unique_path, *prefix;

  /* Try to create directories named "<txndir>/<rev>-<uniqueifier>.txn". */
  prefix = svn_dirent_join_many(pool, fs->path, PATH_TXNS_DIR,
                                apr_psprintf(pool, "%ld", rev), NULL);

  subpool = svn_pool_create(pool);
  for (i = 1; i <= 99999; i++)
    {
      svn_error_t *err;

      svn_pool_clear(subpool);
      unique_path = apr_psprintf(subpool, "%s-%u" PATH_EXT_TXN, prefix, i);
      err = svn_io_dir_make(unique_path, APR_OS_DEFAULT, subpool);
      if (! err)
        {
          /* We succeeded.  Return the basename minus the ".txn" extension. */
          const char *name = svn_dirent_basename(unique_path, subpool);
          *id_p = apr_pstrndup(pool, name,
                               strlen(name) - strlen(PATH_EXT_TXN));
          svn_pool_destroy(subpool);
          return SVN_NO_ERROR;
        }
      if (! APR_STATUS_IS_EEXIST(err->apr_err))
        return svn_error_trace(err);
      svn_error_clear(err);
    }

  return svn_error_createf(SVN_ERR_IO_UNIQUE_NAMES_EXHAUSTED,
                           NULL,
                           _("Unable to create transaction directory "
                             "in '%s' for revision %ld"),
                           svn_dirent_local_style(fs->path, pool),
                           rev);
}

svn_error_t *
svn_fs_fs__create_txn(svn_fs_txn_t **txn_p,
                      svn_fs_t *fs,
                      svn_revnum_t rev,
                      apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  svn_fs_txn_t *txn;
  svn_fs_id_t *root_id;

  txn = apr_pcalloc(pool, sizeof(*txn));

  /* Get the txn_id. */
  if (ffd->format >= SVN_FS_FS__MIN_TXN_CURRENT_FORMAT)
    SVN_ERR(create_txn_dir(&txn->id, fs, rev, pool));
  else
    SVN_ERR(create_txn_dir_pre_1_5(&txn->id, fs, rev, pool));

  txn->fs = fs;
  txn->base_rev = rev;

  txn->vtable = &txn_vtable;
  *txn_p = txn;

  /* Create a new root node for this transaction. */
  SVN_ERR(svn_fs_fs__rev_get_root(&root_id, fs, rev, pool));
  SVN_ERR(create_new_txn_noderev_from_rev(fs, txn->id, root_id, pool));

  /* Create an empty rev file. */
  SVN_ERR(svn_io_file_create(path_txn_proto_rev(fs, txn->id, pool), "",
                             pool));

  /* Create an empty rev-lock file. */
  SVN_ERR(svn_io_file_create(path_txn_proto_rev_lock(fs, txn->id, pool), "",
                             pool));

  /* Create an empty changes file. */
  SVN_ERR(svn_io_file_create(path_txn_changes(fs, txn->id, pool), "",
                             pool));

  /* Create the next-ids file. */
  return svn_io_file_create(path_txn_next_ids(fs, txn->id, pool), "0 0\n",
                            pool);
}

/* Store the property list for transaction TXN_ID in PROPLIST.
   Perform temporary allocations in POOL. */
static svn_error_t *
get_txn_proplist(apr_hash_t *proplist,
                 svn_fs_t *fs,
                 const char *txn_id,
                 apr_pool_t *pool)
{
  svn_stream_t *stream;

  /* Check for issue #3696. (When we find and fix the cause, we can change
   * this to an assertion.) */
  if (txn_id == NULL)
    return svn_error_create(SVN_ERR_INCORRECT_PARAMS, NULL,
                            _("Internal error: a null transaction id was "
                              "passed to get_txn_proplist()"));

  /* Open the transaction properties file. */
  SVN_ERR(svn_stream_open_readonly(&stream, path_txn_props(fs, txn_id, pool),
                                   pool, pool));

  /* Read in the property list. */
  SVN_ERR(svn_hash_read2(proplist, stream, SVN_HASH_TERMINATOR, pool));

  return svn_stream_close(stream);
}

svn_error_t *
svn_fs_fs__change_txn_prop(svn_fs_txn_t *txn,
                           const char *name,
                           const svn_string_t *value,
                           apr_pool_t *pool)
{
  apr_array_header_t *props = apr_array_make(pool, 1, sizeof(svn_prop_t));
  svn_prop_t prop;

  prop.name = name;
  prop.value = value;
  APR_ARRAY_PUSH(props, svn_prop_t) = prop;

  return svn_fs_fs__change_txn_props(txn, props, pool);
}

svn_error_t *
svn_fs_fs__change_txn_props(svn_fs_txn_t *txn,
                            const apr_array_header_t *props,
                            apr_pool_t *pool)
{
  const char *txn_prop_filename;
  svn_stringbuf_t *buf;
  svn_stream_t *stream;
  apr_hash_t *txn_prop = apr_hash_make(pool);
  int i;
  svn_error_t *err;

  err = get_txn_proplist(txn_prop, txn->fs, txn->id, pool);
  /* Here - and here only - we need to deal with the possibility that the
     transaction property file doesn't yet exist.  The rest of the
     implementation assumes that the file exists, but we're called to set the
     initial transaction properties as the transaction is being created. */
  if (err && (APR_STATUS_IS_ENOENT(err->apr_err)))
    svn_error_clear(err);
  else if (err)
    return svn_error_trace(err);

  for (i = 0; i < props->nelts; i++)
    {
      svn_prop_t *prop = &APR_ARRAY_IDX(props, i, svn_prop_t);

      svn_hash_sets(txn_prop, prop->name, prop->value);
    }

  /* Create a new version of the file and write out the new props. */
  /* Open the transaction properties file. */
  buf = svn_stringbuf_create_ensure(1024, pool);
  stream = svn_stream_from_stringbuf(buf, pool);
  SVN_ERR(svn_hash_write2(txn_prop, stream, SVN_HASH_TERMINATOR, pool));
  SVN_ERR(svn_stream_close(stream));
  SVN_ERR(svn_io_write_unique(&txn_prop_filename,
                              path_txn_dir(txn->fs, txn->id, pool),
                              buf->data,
                              buf->len,
                              svn_io_file_del_none,
                              pool));
  return svn_io_file_rename(txn_prop_filename,
                            path_txn_props(txn->fs, txn->id, pool),
                            pool);
}

svn_error_t *
svn_fs_fs__get_txn(transaction_t **txn_p,
                   svn_fs_t *fs,
                   const char *txn_id,
                   apr_pool_t *pool)
{
  transaction_t *txn;
  node_revision_t *noderev;
  svn_fs_id_t *root_id;

  txn = apr_pcalloc(pool, sizeof(*txn));
  txn->proplist = apr_hash_make(pool);

  SVN_ERR(get_txn_proplist(txn->proplist, fs, txn_id, pool));
  root_id = svn_fs_fs__id_txn_create("0", "0", txn_id, pool);

  SVN_ERR(svn_fs_fs__get_node_revision(&noderev, fs, root_id, pool));

  txn->root_id = svn_fs_fs__id_copy(noderev->id, pool);
  txn->base_id = svn_fs_fs__id_copy(noderev->predecessor_id, pool);
  txn->copies = NULL;

  *txn_p = txn;

  return SVN_NO_ERROR;
}

/* Write out the currently available next node_id NODE_ID and copy_id
   COPY_ID for transaction TXN_ID in filesystem FS.  The next node-id is
   used both for creating new unique nodes for the given transaction, as
   well as uniquifying representations.  Perform temporary allocations in
   POOL. */
static svn_error_t *
write_next_ids(svn_fs_t *fs,
               const char *txn_id,
               const char *node_id,
               const char *copy_id,
               apr_pool_t *pool)
{
  apr_file_t *file;
  svn_stream_t *out_stream;

  SVN_ERR(svn_io_file_open(&file, path_txn_next_ids(fs, txn_id, pool),
                           APR_WRITE | APR_TRUNCATE,
                           APR_OS_DEFAULT, pool));

  out_stream = svn_stream_from_aprfile2(file, TRUE, pool);

  SVN_ERR(svn_stream_printf(out_stream, pool, "%s %s\n", node_id, copy_id));

  SVN_ERR(svn_stream_close(out_stream));
  return svn_io_file_close(file, pool);
}

/* Find out what the next unique node-id and copy-id are for
   transaction TXN_ID in filesystem FS.  Store the results in *NODE_ID
   and *COPY_ID.  The next node-id is used both for creating new unique
   nodes for the given transaction, as well as uniquifying representations.
   Perform all allocations in POOL. */
static svn_error_t *
read_next_ids(const char **node_id,
              const char **copy_id,
              svn_fs_t *fs,
              const char *txn_id,
              apr_pool_t *pool)
{
  apr_file_t *file;
  char buf[MAX_KEY_SIZE*2+3];
  apr_size_t limit;
  char *str, *last_str = buf;

  SVN_ERR(svn_io_file_open(&file, path_txn_next_ids(fs, txn_id, pool),
                           APR_READ | APR_BUFFERED, APR_OS_DEFAULT, pool));

  limit = sizeof(buf);
  SVN_ERR(svn_io_read_length_line(file, buf, &limit, pool));

  SVN_ERR(svn_io_file_close(file, pool));

  /* Parse this into two separate strings. */

  str = svn_cstring_tokenize(" ", &last_str);
  if (! str)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("next-id file corrupt"));

  *node_id = apr_pstrdup(pool, str);

  str = svn_cstring_tokenize(" ", &last_str);
  if (! str)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("next-id file corrupt"));

  *copy_id = apr_pstrdup(pool, str);

  return SVN_NO_ERROR;
}

/* Get a new and unique to this transaction node-id for transaction
   TXN_ID in filesystem FS.  Store the new node-id in *NODE_ID_P.
   Node-ids are guaranteed to be unique to this transction, but may
   not necessarily be sequential.  Perform all allocations in POOL. */
static svn_error_t *
get_new_txn_node_id(const char **node_id_p,
                    svn_fs_t *fs,
                    const char *txn_id,
                    apr_pool_t *pool)
{
  const char *cur_node_id, *cur_copy_id;
  char *node_id;
  apr_size_t len;

  /* First read in the current next-ids file. */
  SVN_ERR(read_next_ids(&cur_node_id, &cur_copy_id, fs, txn_id, pool));

  node_id = apr_pcalloc(pool, strlen(cur_node_id) + 2);

  len = strlen(cur_node_id);
  svn_fs_fs__next_key(cur_node_id, &len, node_id);

  SVN_ERR(write_next_ids(fs, txn_id, node_id, cur_copy_id, pool));

  *node_id_p = apr_pstrcat(pool, "_", cur_node_id, (char *)NULL);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__create_node(const svn_fs_id_t **id_p,
                       svn_fs_t *fs,
                       node_revision_t *noderev,
                       const char *copy_id,
                       const char *txn_id,
                       apr_pool_t *pool)
{
  const char *node_id;
  const svn_fs_id_t *id;

  /* Get a new node-id for this node. */
  SVN_ERR(get_new_txn_node_id(&node_id, fs, txn_id, pool));

  id = svn_fs_fs__id_txn_create(node_id, copy_id, txn_id, pool);

  noderev->id = id;

  SVN_ERR(svn_fs_fs__put_node_revision(fs, noderev->id, noderev, FALSE, pool));

  *id_p = id;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__purge_txn(svn_fs_t *fs,
                     const char *txn_id,
                     apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;

  /* Remove the shared transaction object associated with this transaction. */
  SVN_ERR(purge_shared_txn(fs, txn_id, pool));
  /* Remove the directory associated with this transaction. */
  SVN_ERR(svn_io_remove_dir2(path_txn_dir(fs, txn_id, pool), FALSE,
                             NULL, NULL, pool));
  if (ffd->format >= SVN_FS_FS__MIN_PROTOREVS_DIR_FORMAT)
    {
      /* Delete protorev and its lock, which aren't in the txn
         directory.  It's OK if they don't exist (for example, if this
         is post-commit and the proto-rev has been moved into
         place). */
      SVN_ERR(svn_io_remove_file2(path_txn_proto_rev(fs, txn_id, pool),
                                  TRUE, pool));
      SVN_ERR(svn_io_remove_file2(path_txn_proto_rev_lock(fs, txn_id, pool),
                                  TRUE, pool));
    }
  return SVN_NO_ERROR;
}


svn_error_t *
svn_fs_fs__abort_txn(svn_fs_txn_t *txn,
                     apr_pool_t *pool)
{
  SVN_ERR(svn_fs__check_fs(txn->fs, TRUE));

  /* Now, purge the transaction. */
  SVN_ERR_W(svn_fs_fs__purge_txn(txn->fs, txn->id, pool),
            apr_psprintf(pool, _("Transaction '%s' cleanup failed"),
                         txn->id));

  return SVN_NO_ERROR;
}


svn_error_t *
svn_fs_fs__set_entry(svn_fs_t *fs,
                     const char *txn_id,
                     node_revision_t *parent_noderev,
                     const char *name,
                     const svn_fs_id_t *id,
                     svn_node_kind_t kind,
                     apr_pool_t *pool)
{
  representation_t *rep = parent_noderev->data_rep;
  const char *filename = path_txn_node_children(fs, parent_noderev->id, pool);
  apr_file_t *file;
  svn_stream_t *out;
  fs_fs_data_t *ffd = fs->fsap_data;
  apr_pool_t *subpool = svn_pool_create(pool);

  if (!rep || !rep->txn_id)
    {
      const char *unique_suffix;
      apr_hash_t *entries;

      /* Before we can modify the directory, we need to dump its old
         contents into a mutable representation file. */
      SVN_ERR(svn_fs_fs__rep_contents_dir(&entries, fs, parent_noderev,
                                          subpool));
      SVN_ERR(unparse_dir_entries(&entries, entries, subpool));
      SVN_ERR(svn_io_file_open(&file, filename,
                               APR_WRITE | APR_CREATE | APR_BUFFERED,
                               APR_OS_DEFAULT, pool));
      out = svn_stream_from_aprfile2(file, TRUE, pool);
      SVN_ERR(svn_hash_write2(entries, out, SVN_HASH_TERMINATOR, subpool));

      svn_pool_clear(subpool);

      /* Mark the node-rev's data rep as mutable. */
      rep = apr_pcalloc(pool, sizeof(*rep));
      rep->revision = SVN_INVALID_REVNUM;
      rep->txn_id = txn_id;

      if (ffd->format >= SVN_FS_FS__MIN_REP_SHARING_FORMAT)
        {
          SVN_ERR(get_new_txn_node_id(&unique_suffix, fs, txn_id, pool));
          rep->uniquifier = apr_psprintf(pool, "%s/%s", txn_id, unique_suffix);
        }

      parent_noderev->data_rep = rep;
      SVN_ERR(svn_fs_fs__put_node_revision(fs, parent_noderev->id,
                                           parent_noderev, FALSE, pool));
    }
  else
    {
      /* The directory rep is already mutable, so just open it for append. */
      SVN_ERR(svn_io_file_open(&file, filename, APR_WRITE | APR_APPEND,
                               APR_OS_DEFAULT, pool));
      out = svn_stream_from_aprfile2(file, TRUE, pool);
    }

  /* if we have a directory cache for this transaction, update it */
  if (ffd->txn_dir_cache)
    {
      /* build parameters: (name, new entry) pair */
      const char *key =
          svn_fs_fs__id_unparse(parent_noderev->id, subpool)->data;
      replace_baton_t baton;

      baton.name = name;
      baton.new_entry = NULL;

      if (id)
        {
          baton.new_entry = apr_pcalloc(subpool, sizeof(*baton.new_entry));
          baton.new_entry->name = name;
          baton.new_entry->kind = kind;
          baton.new_entry->id = id;
        }

      /* actually update the cached directory (if cached) */
      SVN_ERR(svn_cache__set_partial(ffd->txn_dir_cache, key,
                                     svn_fs_fs__replace_dir_entry, &baton,
                                     subpool));
    }
  svn_pool_clear(subpool);

  /* Append an incremental hash entry for the entry change. */
  if (id)
    {
      const char *val = unparse_dir_entry(kind, id, subpool);

      SVN_ERR(svn_stream_printf(out, subpool, "K %" APR_SIZE_T_FMT "\n%s\n"
                                "V %" APR_SIZE_T_FMT "\n%s\n",
                                strlen(name), name,
                                strlen(val), val));
    }
  else
    {
      SVN_ERR(svn_stream_printf(out, subpool, "D %" APR_SIZE_T_FMT "\n%s\n",
                                strlen(name), name));
    }

  SVN_ERR(svn_io_file_close(file, subpool));
  svn_pool_destroy(subpool);
  return SVN_NO_ERROR;
}

/* Write a single change entry, path PATH, change CHANGE, and copyfrom
   string COPYFROM, into the file specified by FILE.  Only include the
   node kind field if INCLUDE_NODE_KIND is true.  All temporary
   allocations are in POOL. */
static svn_error_t *
write_change_entry(apr_file_t *file,
                   const char *path,
                   svn_fs_path_change2_t *change,
                   svn_boolean_t include_node_kind,
                   apr_pool_t *pool)
{
  const char *idstr, *buf;
  const char *change_string = NULL;
  const char *kind_string = "";

  switch (change->change_kind)
    {
    case svn_fs_path_change_modify:
      change_string = ACTION_MODIFY;
      break;
    case svn_fs_path_change_add:
      change_string = ACTION_ADD;
      break;
    case svn_fs_path_change_delete:
      change_string = ACTION_DELETE;
      break;
    case svn_fs_path_change_replace:
      change_string = ACTION_REPLACE;
      break;
    case svn_fs_path_change_reset:
      change_string = ACTION_RESET;
      break;
    default:
      return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                               _("Invalid change type %d"),
                               change->change_kind);
    }

  if (change->node_rev_id)
    idstr = svn_fs_fs__id_unparse(change->node_rev_id, pool)->data;
  else
    idstr = ACTION_RESET;

  if (include_node_kind)
    {
      SVN_ERR_ASSERT(change->node_kind == svn_node_dir
                     || change->node_kind == svn_node_file);
      kind_string = apr_psprintf(pool, "-%s",
                                 change->node_kind == svn_node_dir
                                 ? KIND_DIR : KIND_FILE);
    }
  buf = apr_psprintf(pool, "%s %s%s %s %s %s\n",
                     idstr, change_string, kind_string,
                     change->text_mod ? FLAG_TRUE : FLAG_FALSE,
                     change->prop_mod ? FLAG_TRUE : FLAG_FALSE,
                     path);

  SVN_ERR(svn_io_file_write_full(file, buf, strlen(buf), NULL, pool));

  if (SVN_IS_VALID_REVNUM(change->copyfrom_rev))
    {
      buf = apr_psprintf(pool, "%ld %s", change->copyfrom_rev,
                         change->copyfrom_path);
      SVN_ERR(svn_io_file_write_full(file, buf, strlen(buf), NULL, pool));
    }

  return svn_io_file_write_full(file, "\n", 1, NULL, pool);
}

svn_error_t *
svn_fs_fs__add_change(svn_fs_t *fs,
                      const char *txn_id,
                      const char *path,
                      const svn_fs_id_t *id,
                      svn_fs_path_change_kind_t change_kind,
                      svn_boolean_t text_mod,
                      svn_boolean_t prop_mod,
                      svn_node_kind_t node_kind,
                      svn_revnum_t copyfrom_rev,
                      const char *copyfrom_path,
                      apr_pool_t *pool)
{
  apr_file_t *file;
  svn_fs_path_change2_t *change;

  SVN_ERR(svn_io_file_open(&file, path_txn_changes(fs, txn_id, pool),
                           APR_APPEND | APR_WRITE | APR_CREATE
                           | APR_BUFFERED, APR_OS_DEFAULT, pool));

  change = svn_fs__path_change_create_internal(id, change_kind, pool);
  change->text_mod = text_mod;
  change->prop_mod = prop_mod;
  change->node_kind = node_kind;
  change->copyfrom_rev = copyfrom_rev;
  change->copyfrom_path = apr_pstrdup(pool, copyfrom_path);

  SVN_ERR(write_change_entry(file, path, change, TRUE, pool));

  return svn_io_file_close(file, pool);
}

/* This baton is used by the representation writing streams.  It keeps
   track of the checksum information as well as the total size of the
   representation so far. */
struct rep_write_baton
{
  /* The FS we are writing to. */
  svn_fs_t *fs;

  /* Actual file to which we are writing. */
  svn_stream_t *rep_stream;

  /* A stream from the delta combiner.  Data written here gets
     deltified, then eventually written to rep_stream. */
  svn_stream_t *delta_stream;

  /* Where is this representation header stored. */
  apr_off_t rep_offset;

  /* Start of the actual data. */
  apr_off_t delta_start;

  /* How many bytes have been written to this rep already. */
  svn_filesize_t rep_size;

  /* The node revision for which we're writing out info. */
  node_revision_t *noderev;

  /* Actual output file. */
  apr_file_t *file;
  /* Lock 'cookie' used to unlock the output file once we've finished
     writing to it. */
  void *lockcookie;

  svn_checksum_ctx_t *md5_checksum_ctx;
  svn_checksum_ctx_t *sha1_checksum_ctx;

  apr_pool_t *pool;

  apr_pool_t *parent_pool;
};

/* Handler for the write method of the representation writable stream.
   BATON is a rep_write_baton, DATA is the data to write, and *LEN is
   the length of this data. */
static svn_error_t *
rep_write_contents(void *baton,
                   const char *data,
                   apr_size_t *len)
{
  struct rep_write_baton *b = baton;

  SVN_ERR(svn_checksum_update(b->md5_checksum_ctx, data, *len));
  SVN_ERR(svn_checksum_update(b->sha1_checksum_ctx, data, *len));
  b->rep_size += *len;

  /* If we are writing a delta, use that stream. */
  if (b->delta_stream)
    return svn_stream_write(b->delta_stream, data, len);
  else
    return svn_stream_write(b->rep_stream, data, len);
}

/* Given a node-revision NODEREV in filesystem FS, return the
   representation in *REP to use as the base for a text representation
   delta if PROPS is FALSE.  If PROPS has been set, a suitable props
   base representation will be returned.  Perform temporary allocations
   in *POOL. */
static svn_error_t *
choose_delta_base(representation_t **rep,
                  svn_fs_t *fs,
                  node_revision_t *noderev,
                  svn_boolean_t props,
                  apr_pool_t *pool)
{
  int count;
  int walk;
  node_revision_t *base;
  fs_fs_data_t *ffd = fs->fsap_data;
  svn_boolean_t maybe_shared_rep = FALSE;

  /* If we have no predecessors, then use the empty stream as a
     base. */
  if (! noderev->predecessor_count)
    {
      *rep = NULL;
      return SVN_NO_ERROR;
    }

  /* Flip the rightmost '1' bit of the predecessor count to determine
     which file rev (counting from 0) we want to use.  (To see why
     count & (count - 1) unsets the rightmost set bit, think about how
     you decrement a binary number.) */
  count = noderev->predecessor_count;
  count = count & (count - 1);

  /* We use skip delta for limiting the number of delta operations
     along very long node histories.  Close to HEAD however, we create
     a linear history to minimize delta size.  */
  walk = noderev->predecessor_count - count;
  if (walk < (int)ffd->max_linear_deltification)
    count = noderev->predecessor_count - 1;

  /* Finding the delta base over a very long distance can become extremely
     expensive for very deep histories, possibly causing client timeouts etc.
     OTOH, this is a rare operation and its gains are minimal. Lets simply
     start deltification anew close every other 1000 changes or so.  */
  if (walk > (int)ffd->max_deltification_walk)
    {
      *rep = NULL;
      return SVN_NO_ERROR;
    }

  /* Walk back a number of predecessors equal to the difference
     between count and the original predecessor count.  (For example,
     if noderev has ten predecessors and we want the eighth file rev,
     walk back two predecessors.) */
  base = noderev;
  while ((count++) < noderev->predecessor_count)
    {
      SVN_ERR(svn_fs_fs__get_node_revision(&base, fs,
                                           base->predecessor_id, pool));

      /* If there is a shared rep along the way, we need to limit the
       * length of the deltification chain.
       *
       * Please note that copied nodes - such as branch directories - will
       * look the same (false positive) while reps shared within the same
       * revision will not be caught (false negative).
       */
      if (props)
        {
          if (   base->prop_rep
              && svn_fs_fs__id_rev(base->id) > base->prop_rep->revision)
            maybe_shared_rep = TRUE;
        }
      else
        {
          if (   base->data_rep
              && svn_fs_fs__id_rev(base->id) > base->data_rep->revision)
            maybe_shared_rep = TRUE;
        }
    }

  /* return a suitable base representation */
  *rep = props ? base->prop_rep : base->data_rep;

  /* if we encountered a shared rep, it's parent chain may be different
   * from the node-rev parent chain. */
  if (*rep && maybe_shared_rep)
    {
      /* Check whether the length of the deltification chain is acceptable.
       * Otherwise, shared reps may form a non-skipping delta chain in
       * extreme cases. */
      apr_pool_t *sub_pool = svn_pool_create(pool);
      representation_t base_rep = **rep;

      /* Some reasonable limit, depending on how acceptable longer linear
       * chains are in this repo.  Also, allow for some minimal chain. */
      int max_chain_length = 2 * (int)ffd->max_linear_deltification + 2;

      /* re-use open files between iterations */
      svn_revnum_t rev_hint = SVN_INVALID_REVNUM;
      apr_file_t *file_hint = NULL;

      /* follow the delta chain towards the end but for at most
       * MAX_CHAIN_LENGTH steps. */
      for (; max_chain_length; --max_chain_length)
        {
          struct rep_state *rep_state;
          struct rep_args *rep_args;

          SVN_ERR(create_rep_state_body(&rep_state,
                                        &rep_args,
                                        &file_hint,
                                        &rev_hint,
                                        &base_rep,
                                        fs,
                                        sub_pool));
          if (!rep_args->is_delta  || !rep_args->base_revision)
            break;

          base_rep.revision = rep_args->base_revision;
          base_rep.offset = rep_args->base_offset;
          base_rep.size = rep_args->base_length;
          base_rep.txn_id = NULL;
        }

      /* start new delta chain if the current one has grown too long */
      if (max_chain_length == 0)
        *rep = NULL;

      svn_pool_destroy(sub_pool);
    }

  /* verify that the reps don't form a degenerated '*/
  return SVN_NO_ERROR;
}

/* Something went wrong and the pool for the rep write is being
   cleared before we've finished writing the rep.  So we need
   to remove the rep from the protorevfile and we need to unlock
   the protorevfile. */
static apr_status_t
rep_write_cleanup(void *data)
{
  struct rep_write_baton *b = data;
  const char *txn_id = svn_fs_fs__id_txn_id(b->noderev->id);
  svn_error_t *err;

  /* Truncate and close the protorevfile. */
  err = svn_io_file_trunc(b->file, b->rep_offset, b->pool);
  err = svn_error_compose_create(err, svn_io_file_close(b->file, b->pool));

  /* Remove our lock regardless of any preceeding errors so that the
     being_written flag is always removed and stays consistent with the
     file lock which will be removed no matter what since the pool is
     going away. */
  err = svn_error_compose_create(err, unlock_proto_rev(b->fs, txn_id,
                                                       b->lockcookie, b->pool));
  if (err)
    {
      apr_status_t rc = err->apr_err;
      svn_error_clear(err);
      return rc;
    }

  return APR_SUCCESS;
}


/* Get a rep_write_baton and store it in *WB_P for the representation
   indicated by NODEREV in filesystem FS.  Perform allocations in
   POOL.  Only appropriate for file contents, not for props or
   directory contents. */
static svn_error_t *
rep_write_get_baton(struct rep_write_baton **wb_p,
                    svn_fs_t *fs,
                    node_revision_t *noderev,
                    apr_pool_t *pool)
{
  struct rep_write_baton *b;
  apr_file_t *file;
  representation_t *base_rep;
  svn_stream_t *source;
  const char *header;
  svn_txdelta_window_handler_t wh;
  void *whb;
  fs_fs_data_t *ffd = fs->fsap_data;
  int diff_version = ffd->format >= SVN_FS_FS__MIN_SVNDIFF1_FORMAT ? 1 : 0;

  b = apr_pcalloc(pool, sizeof(*b));

  b->sha1_checksum_ctx = svn_checksum_ctx_create(svn_checksum_sha1, pool);
  b->md5_checksum_ctx = svn_checksum_ctx_create(svn_checksum_md5, pool);

  b->fs = fs;
  b->parent_pool = pool;
  b->pool = svn_pool_create(pool);
  b->rep_size = 0;
  b->noderev = noderev;

  /* Open the prototype rev file and seek to its end. */
  SVN_ERR(get_writable_proto_rev(&file, &b->lockcookie,
                                 fs, svn_fs_fs__id_txn_id(noderev->id),
                                 b->pool));

  b->file = file;
  b->rep_stream = svn_stream_from_aprfile2(file, TRUE, b->pool);

  SVN_ERR(get_file_offset(&b->rep_offset, file, b->pool));

  /* Get the base for this delta. */
  SVN_ERR(choose_delta_base(&base_rep, fs, noderev, FALSE, b->pool));
  SVN_ERR(read_representation(&source, fs, base_rep, b->pool));

  /* Write out the rep header. */
  if (base_rep)
    {
      header = apr_psprintf(b->pool, REP_DELTA " %ld %" APR_OFF_T_FMT " %"
                            SVN_FILESIZE_T_FMT "\n",
                            base_rep->revision, base_rep->offset,
                            base_rep->size);
    }
  else
    {
      header = REP_DELTA "\n";
    }
  SVN_ERR(svn_io_file_write_full(file, header, strlen(header), NULL,
                                 b->pool));

  /* Now determine the offset of the actual svndiff data. */
  SVN_ERR(get_file_offset(&b->delta_start, file, b->pool));

  /* Cleanup in case something goes wrong. */
  apr_pool_cleanup_register(b->pool, b, rep_write_cleanup,
                            apr_pool_cleanup_null);

  /* Prepare to write the svndiff data. */
  svn_txdelta_to_svndiff3(&wh,
                          &whb,
                          b->rep_stream,
                          diff_version,
                          SVN_DELTA_COMPRESSION_LEVEL_DEFAULT,
                          pool);

  b->delta_stream = svn_txdelta_target_push(wh, whb, source, b->pool);

  *wb_p = b;

  return SVN_NO_ERROR;
}

/* For the hash REP->SHA1, try to find an already existing representation
   in FS and return it in *OUT_REP.  If no such representation exists or
   if rep sharing has been disabled for FS, NULL will be returned.  Since
   there may be new duplicate representations within the same uncommitted
   revision, those can be passed in REPS_HASH (maps a sha1 digest onto
   representation_t*), otherwise pass in NULL for REPS_HASH.
   POOL will be used for allocations. The lifetime of the returned rep is
   limited by both, POOL and REP lifetime.
 */
static svn_error_t *
get_shared_rep(representation_t **old_rep,
               svn_fs_t *fs,
               representation_t *rep,
               apr_hash_t *reps_hash,
               apr_pool_t *pool)
{
  svn_error_t *err;
  fs_fs_data_t *ffd = fs->fsap_data;

  /* Return NULL, if rep sharing has been disabled. */
  *old_rep = NULL;
  if (!ffd->rep_sharing_allowed)
    return SVN_NO_ERROR;

  /* Check and see if we already have a representation somewhere that's
     identical to the one we just wrote out.  Start with the hash lookup
     because it is cheepest. */
  if (reps_hash)
    *old_rep = apr_hash_get(reps_hash,
                            rep->sha1_checksum->digest,
                            APR_SHA1_DIGESTSIZE);

  /* If we haven't found anything yet, try harder and consult our DB. */
  if (*old_rep == NULL)
    {
      err = svn_fs_fs__get_rep_reference(old_rep, fs, rep->sha1_checksum,
                                         pool);
      /* ### Other error codes that we shouldn't mask out? */
      if (err == SVN_NO_ERROR)
        {
          if (*old_rep)
            SVN_ERR(verify_walker(*old_rep, NULL, fs, pool));
        }
      else if (err->apr_err == SVN_ERR_FS_CORRUPT
               || SVN_ERROR_IN_CATEGORY(err->apr_err,
                                        SVN_ERR_MALFUNC_CATEGORY_START))
        {
          /* Fatal error; don't mask it.

             In particular, this block is triggered when the rep-cache refers
             to revisions in the future.  We signal that as a corruption situation
             since, once those revisions are less than youngest (because of more
             commits), the rep-cache would be invalid.
           */
          SVN_ERR(err);
        }
      else
        {
          /* Something's wrong with the rep-sharing index.  We can continue
             without rep-sharing, but warn.
           */
          (fs->warning)(fs->warning_baton, err);
          svn_error_clear(err);
          *old_rep = NULL;
        }
    }

  /* look for intra-revision matches (usually data reps but not limited
     to them in case props happen to look like some data rep)
   */
  if (*old_rep == NULL && rep->txn_id)
    {
      svn_node_kind_t kind;
      const char *file_name
        = path_txn_sha1(fs, rep->txn_id, rep->sha1_checksum, pool);

      /* in our txn, is there a rep file named with the wanted SHA1?
         If so, read it and use that rep.
       */
      SVN_ERR(svn_io_check_path(file_name, &kind, pool));
      if (kind == svn_node_file)
        {
          svn_stringbuf_t *rep_string;
          SVN_ERR(svn_stringbuf_from_file2(&rep_string, file_name, pool));
          SVN_ERR(read_rep_offsets_body(old_rep, rep_string->data,
                                        rep->txn_id, FALSE, pool));
        }
    }

  /* Add information that is missing in the cached data. */
  if (*old_rep)
    {
      /* Use the old rep for this content. */
      (*old_rep)->md5_checksum = rep->md5_checksum;
      (*old_rep)->uniquifier = rep->uniquifier;
    }

  return SVN_NO_ERROR;
}

/* Close handler for the representation write stream.  BATON is a
   rep_write_baton.  Writes out a new node-rev that correctly
   references the representation we just finished writing. */
static svn_error_t *
rep_write_contents_close(void *baton)
{
  struct rep_write_baton *b = baton;
  const char *unique_suffix;
  representation_t *rep;
  representation_t *old_rep;
  apr_off_t offset;
  fs_fs_data_t *ffd = b->fs->fsap_data;

  rep = apr_pcalloc(b->parent_pool, sizeof(*rep));
  rep->offset = b->rep_offset;

  /* Close our delta stream so the last bits of svndiff are written
     out. */
  if (b->delta_stream)
    SVN_ERR(svn_stream_close(b->delta_stream));

  /* Determine the length of the svndiff data. */
  SVN_ERR(get_file_offset(&offset, b->file, b->pool));
  rep->size = offset - b->delta_start;

  /* Fill in the rest of the representation field. */
  rep->expanded_size = b->rep_size;
  rep->txn_id = svn_fs_fs__id_txn_id(b->noderev->id);

  if (ffd->format >= SVN_FS_FS__MIN_REP_SHARING_FORMAT)
    {
      SVN_ERR(get_new_txn_node_id(&unique_suffix, b->fs, rep->txn_id, b->pool));
      rep->uniquifier = apr_psprintf(b->parent_pool, "%s/%s", rep->txn_id,
                                     unique_suffix);
    }
  rep->revision = SVN_INVALID_REVNUM;

  /* Finalize the checksum. */
  SVN_ERR(svn_checksum_final(&rep->md5_checksum, b->md5_checksum_ctx,
                              b->parent_pool));
  SVN_ERR(svn_checksum_final(&rep->sha1_checksum, b->sha1_checksum_ctx,
                              b->parent_pool));

  /* Check and see if we already have a representation somewhere that's
     identical to the one we just wrote out. */
  SVN_ERR(get_shared_rep(&old_rep, b->fs, rep, NULL, b->parent_pool));

  if (old_rep)
    {
      /* We need to erase from the protorev the data we just wrote. */
      SVN_ERR(svn_io_file_trunc(b->file, b->rep_offset, b->pool));

      /* Use the old rep for this content. */
      b->noderev->data_rep = old_rep;
    }
  else
    {
      /* Write out our cosmetic end marker. */
      SVN_ERR(svn_stream_puts(b->rep_stream, "ENDREP\n"));

      b->noderev->data_rep = rep;
    }

  /* Remove cleanup callback. */
  apr_pool_cleanup_kill(b->pool, b, rep_write_cleanup);

  /* Write out the new node-rev information. */
  SVN_ERR(svn_fs_fs__put_node_revision(b->fs, b->noderev->id, b->noderev, FALSE,
                                       b->pool));
  if (!old_rep)
    SVN_ERR(store_sha1_rep_mapping(b->fs, b->noderev, b->pool));

  SVN_ERR(svn_io_file_close(b->file, b->pool));
  SVN_ERR(unlock_proto_rev(b->fs, rep->txn_id, b->lockcookie, b->pool));
  svn_pool_destroy(b->pool);

  return SVN_NO_ERROR;
}

/* Store a writable stream in *CONTENTS_P that will receive all data
   written and store it as the file data representation referenced by
   NODEREV in filesystem FS.  Perform temporary allocations in
   POOL.  Only appropriate for file data, not props or directory
   contents. */
static svn_error_t *
set_representation(svn_stream_t **contents_p,
                   svn_fs_t *fs,
                   node_revision_t *noderev,
                   apr_pool_t *pool)
{
  struct rep_write_baton *wb;

  if (! svn_fs_fs__id_txn_id(noderev->id))
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Attempted to write to non-transaction '%s'"),
                             svn_fs_fs__id_unparse(noderev->id, pool)->data);

  SVN_ERR(rep_write_get_baton(&wb, fs, noderev, pool));

  *contents_p = svn_stream_create(wb, pool);
  svn_stream_set_write(*contents_p, rep_write_contents);
  svn_stream_set_close(*contents_p, rep_write_contents_close);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__set_contents(svn_stream_t **stream,
                        svn_fs_t *fs,
                        node_revision_t *noderev,
                        apr_pool_t *pool)
{
  if (noderev->kind != svn_node_file)
    return svn_error_create(SVN_ERR_FS_NOT_FILE, NULL,
                            _("Can't set text contents of a directory"));

  return set_representation(stream, fs, noderev, pool);
}

svn_error_t *
svn_fs_fs__create_successor(const svn_fs_id_t **new_id_p,
                            svn_fs_t *fs,
                            const svn_fs_id_t *old_idp,
                            node_revision_t *new_noderev,
                            const char *copy_id,
                            const char *txn_id,
                            apr_pool_t *pool)
{
  const svn_fs_id_t *id;

  if (! copy_id)
    copy_id = svn_fs_fs__id_copy_id(old_idp);
  id = svn_fs_fs__id_txn_create(svn_fs_fs__id_node_id(old_idp), copy_id,
                                txn_id, pool);

  new_noderev->id = id;

  if (! new_noderev->copyroot_path)
    {
      new_noderev->copyroot_path = apr_pstrdup(pool,
                                               new_noderev->created_path);
      new_noderev->copyroot_rev = svn_fs_fs__id_rev(new_noderev->id);
    }

  SVN_ERR(svn_fs_fs__put_node_revision(fs, new_noderev->id, new_noderev, FALSE,
                                       pool));

  *new_id_p = id;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__set_proplist(svn_fs_t *fs,
                        node_revision_t *noderev,
                        apr_hash_t *proplist,
                        apr_pool_t *pool)
{
  const char *filename = path_txn_node_props(fs, noderev->id, pool);
  apr_file_t *file;
  svn_stream_t *out;

  /* Dump the property list to the mutable property file. */
  SVN_ERR(svn_io_file_open(&file, filename,
                           APR_WRITE | APR_CREATE | APR_TRUNCATE
                           | APR_BUFFERED, APR_OS_DEFAULT, pool));
  out = svn_stream_from_aprfile2(file, TRUE, pool);
  SVN_ERR(svn_hash_write2(proplist, out, SVN_HASH_TERMINATOR, pool));
  SVN_ERR(svn_io_file_close(file, pool));

  /* Mark the node-rev's prop rep as mutable, if not already done. */
  if (!noderev->prop_rep || !noderev->prop_rep->txn_id)
    {
      noderev->prop_rep = apr_pcalloc(pool, sizeof(*noderev->prop_rep));
      noderev->prop_rep->txn_id = svn_fs_fs__id_txn_id(noderev->id);
      SVN_ERR(svn_fs_fs__put_node_revision(fs, noderev->id, noderev, FALSE, pool));
    }

  return SVN_NO_ERROR;
}

/* Read the 'current' file for filesystem FS and store the next
   available node id in *NODE_ID, and the next available copy id in
   *COPY_ID.  Allocations are performed from POOL. */
static svn_error_t *
get_next_revision_ids(const char **node_id,
                      const char **copy_id,
                      svn_fs_t *fs,
                      apr_pool_t *pool)
{
  char *buf;
  char *str;
  svn_stringbuf_t *content;

  SVN_ERR(read_content(&content, svn_fs_fs__path_current(fs, pool), pool));
  buf = content->data;

  str = svn_cstring_tokenize(" ", &buf);
  if (! str)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Corrupt 'current' file"));

  str = svn_cstring_tokenize(" ", &buf);
  if (! str)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Corrupt 'current' file"));

  *node_id = apr_pstrdup(pool, str);

  str = svn_cstring_tokenize(" \n", &buf);
  if (! str)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Corrupt 'current' file"));

  *copy_id = apr_pstrdup(pool, str);

  return SVN_NO_ERROR;
}

/* This baton is used by the stream created for write_hash_rep. */
struct write_hash_baton
{
  svn_stream_t *stream;

  apr_size_t size;

  svn_checksum_ctx_t *md5_ctx;
  svn_checksum_ctx_t *sha1_ctx;
};

/* The handler for the write_hash_rep stream.  BATON is a
   write_hash_baton, DATA has the data to write and *LEN is the number
   of bytes to write. */
static svn_error_t *
write_hash_handler(void *baton,
                   const char *data,
                   apr_size_t *len)
{
  struct write_hash_baton *whb = baton;

  SVN_ERR(svn_checksum_update(whb->md5_ctx, data, *len));
  SVN_ERR(svn_checksum_update(whb->sha1_ctx, data, *len));

  SVN_ERR(svn_stream_write(whb->stream, data, len));
  whb->size += *len;

  return SVN_NO_ERROR;
}

/* Write out the hash HASH as a text representation to file FILE.  In
   the process, record position, the total size of the dump and MD5 as
   well as SHA1 in REP.   If rep sharing has been enabled and REPS_HASH
   is not NULL, it will be used in addition to the on-disk cache to find
   earlier reps with the same content.  When such existing reps can be
   found, we will truncate the one just written from the file and return
   the existing rep.  Perform temporary allocations in POOL. */
static svn_error_t *
write_hash_rep(representation_t *rep,
               apr_file_t *file,
               apr_hash_t *hash,
               svn_fs_t *fs,
               apr_hash_t *reps_hash,
               apr_pool_t *pool)
{
  svn_stream_t *stream;
  struct write_hash_baton *whb;
  representation_t *old_rep;

  SVN_ERR(get_file_offset(&rep->offset, file, pool));

  whb = apr_pcalloc(pool, sizeof(*whb));

  whb->stream = svn_stream_from_aprfile2(file, TRUE, pool);
  whb->size = 0;
  whb->md5_ctx = svn_checksum_ctx_create(svn_checksum_md5, pool);
  whb->sha1_ctx = svn_checksum_ctx_create(svn_checksum_sha1, pool);

  stream = svn_stream_create(whb, pool);
  svn_stream_set_write(stream, write_hash_handler);

  SVN_ERR(svn_stream_puts(whb->stream, "PLAIN\n"));

  SVN_ERR(svn_hash_write2(hash, stream, SVN_HASH_TERMINATOR, pool));

  /* Store the results. */
  SVN_ERR(svn_checksum_final(&rep->md5_checksum, whb->md5_ctx, pool));
  SVN_ERR(svn_checksum_final(&rep->sha1_checksum, whb->sha1_ctx, pool));

  /* Check and see if we already have a representation somewhere that's
     identical to the one we just wrote out. */
  SVN_ERR(get_shared_rep(&old_rep, fs, rep, reps_hash, pool));

  if (old_rep)
    {
      /* We need to erase from the protorev the data we just wrote. */
      SVN_ERR(svn_io_file_trunc(file, rep->offset, pool));

      /* Use the old rep for this content. */
      memcpy(rep, old_rep, sizeof (*rep));
    }
  else
    {
      /* Write out our cosmetic end marker. */
      SVN_ERR(svn_stream_puts(whb->stream, "ENDREP\n"));

      /* update the representation */
      rep->size = whb->size;
      rep->expanded_size = whb->size;
    }

  return SVN_NO_ERROR;
}

/* Write out the hash HASH pertaining to the NODEREV in FS as a deltified
   text representation to file FILE.  In the process, record the total size
   and the md5 digest in REP.  If rep sharing has been enabled and REPS_HASH
   is not NULL, it will be used in addition to the on-disk cache to find
   earlier reps with the same content.  When such existing reps can be found,
   we will truncate the one just written from the file and return the existing
   rep.  If PROPS is set, assume that we want to a props representation as
   the base for our delta.  Perform temporary allocations in POOL. */
static svn_error_t *
write_hash_delta_rep(representation_t *rep,
                     apr_file_t *file,
                     apr_hash_t *hash,
                     svn_fs_t *fs,
                     node_revision_t *noderev,
                     apr_hash_t *reps_hash,
                     svn_boolean_t props,
                     apr_pool_t *pool)
{
  svn_txdelta_window_handler_t diff_wh;
  void *diff_whb;

  svn_stream_t *file_stream;
  svn_stream_t *stream;
  representation_t *base_rep;
  representation_t *old_rep;
  svn_stream_t *source;
  const char *header;

  apr_off_t rep_end = 0;
  apr_off_t delta_start = 0;

  struct write_hash_baton *whb;
  fs_fs_data_t *ffd = fs->fsap_data;
  int diff_version = ffd->format >= SVN_FS_FS__MIN_SVNDIFF1_FORMAT ? 1 : 0;

  /* Get the base for this delta. */
  SVN_ERR(choose_delta_base(&base_rep, fs, noderev, props, pool));
  SVN_ERR(read_representation(&source, fs, base_rep, pool));

  SVN_ERR(get_file_offset(&rep->offset, file, pool));

  /* Write out the rep header. */
  if (base_rep)
    {
      header = apr_psprintf(pool, REP_DELTA " %ld %" APR_OFF_T_FMT " %"
                            SVN_FILESIZE_T_FMT "\n",
                            base_rep->revision, base_rep->offset,
                            base_rep->size);
    }
  else
    {
      header = REP_DELTA "\n";
    }
  SVN_ERR(svn_io_file_write_full(file, header, strlen(header), NULL,
                                 pool));

  SVN_ERR(get_file_offset(&delta_start, file, pool));
  file_stream = svn_stream_from_aprfile2(file, TRUE, pool);

  /* Prepare to write the svndiff data. */
  svn_txdelta_to_svndiff3(&diff_wh,
                          &diff_whb,
                          file_stream,
                          diff_version,
                          SVN_DELTA_COMPRESSION_LEVEL_DEFAULT,
                          pool);

  whb = apr_pcalloc(pool, sizeof(*whb));
  whb->stream = svn_txdelta_target_push(diff_wh, diff_whb, source, pool);
  whb->size = 0;
  whb->md5_ctx = svn_checksum_ctx_create(svn_checksum_md5, pool);
  whb->sha1_ctx = svn_checksum_ctx_create(svn_checksum_sha1, pool);

  /* serialize the hash */
  stream = svn_stream_create(whb, pool);
  svn_stream_set_write(stream, write_hash_handler);

  SVN_ERR(svn_hash_write2(hash, stream, SVN_HASH_TERMINATOR, pool));
  SVN_ERR(svn_stream_close(whb->stream));

  /* Store the results. */
  SVN_ERR(svn_checksum_final(&rep->md5_checksum, whb->md5_ctx, pool));
  SVN_ERR(svn_checksum_final(&rep->sha1_checksum, whb->sha1_ctx, pool));

  /* Check and see if we already have a representation somewhere that's
     identical to the one we just wrote out. */
  SVN_ERR(get_shared_rep(&old_rep, fs, rep, reps_hash, pool));

  if (old_rep)
    {
      /* We need to erase from the protorev the data we just wrote. */
      SVN_ERR(svn_io_file_trunc(file, rep->offset, pool));

      /* Use the old rep for this content. */
      memcpy(rep, old_rep, sizeof (*rep));
    }
  else
    {
      /* Write out our cosmetic end marker. */
      SVN_ERR(get_file_offset(&rep_end, file, pool));
      SVN_ERR(svn_stream_puts(file_stream, "ENDREP\n"));

      /* update the representation */
      rep->expanded_size = whb->size;
      rep->size = rep_end - delta_start;
    }

  return SVN_NO_ERROR;
}

/* Sanity check ROOT_NODEREV, a candidate for being the root node-revision
   of (not yet committed) revision REV in FS.  Use POOL for temporary
   allocations.

   If you change this function, consider updating svn_fs_fs__verify() too.
 */
static svn_error_t *
validate_root_noderev(svn_fs_t *fs,
                      node_revision_t *root_noderev,
                      svn_revnum_t rev,
                      apr_pool_t *pool)
{
  svn_revnum_t head_revnum = rev-1;
  int head_predecessor_count;

  SVN_ERR_ASSERT(rev > 0);

  /* Compute HEAD_PREDECESSOR_COUNT. */
  {
    svn_fs_root_t *head_revision;
    const svn_fs_id_t *head_root_id;
    node_revision_t *head_root_noderev;

    /* Get /@HEAD's noderev. */
    SVN_ERR(svn_fs_fs__revision_root(&head_revision, fs, head_revnum, pool));
    SVN_ERR(svn_fs_fs__node_id(&head_root_id, head_revision, "/", pool));
    SVN_ERR(svn_fs_fs__get_node_revision(&head_root_noderev, fs, head_root_id,
                                         pool));

    head_predecessor_count = head_root_noderev->predecessor_count;
  }

  /* Check that the root noderev's predecessor count equals REV.

     This kind of corruption was seen on svn.apache.org (both on
     the root noderev and on other fspaths' noderevs); see
     issue #4129.

     Normally (rev == root_noderev->predecessor_count), but here we
     use a more roundabout check that should only trigger on new instances
     of the corruption, rather then trigger on each and every new commit
     to a repository that has triggered the bug somewhere in its root
     noderev's history.
   */
  if (root_noderev->predecessor_count != -1
      && (root_noderev->predecessor_count - head_predecessor_count)
         != (rev - head_revnum))
    {
      return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                               _("predecessor count for "
                                 "the root node-revision is wrong: "
                                 "found (%d+%ld != %d), committing r%ld"),
                                 head_predecessor_count,
                                 rev - head_revnum, /* This is equal to 1. */
                                 root_noderev->predecessor_count,
                                 rev);
    }

  return SVN_NO_ERROR;
}

/* Copy a node-revision specified by id ID in fileystem FS from a
   transaction into the proto-rev-file FILE.  Set *NEW_ID_P to a
   pointer to the new node-id which will be allocated in POOL.
   If this is a directory, copy all children as well.

   START_NODE_ID and START_COPY_ID are
   the first available node and copy ids for this filesystem, for older
   FS formats.

   REV is the revision number that this proto-rev-file will represent.

   INITIAL_OFFSET is the offset of the proto-rev-file on entry to
   commit_body.

   If REPS_TO_CACHE is not NULL, append to it a copy (allocated in
   REPS_POOL) of each data rep that is new in this revision.

   If REPS_HASH is not NULL, append copies (allocated in REPS_POOL)
   of the representations of each property rep that is new in this
   revision.

   AT_ROOT is true if the node revision being written is the root
   node-revision.  It is only controls additional sanity checking
   logic.

   Temporary allocations are also from POOL. */
static svn_error_t *
write_final_rev(const svn_fs_id_t **new_id_p,
                apr_file_t *file,
                svn_revnum_t rev,
                svn_fs_t *fs,
                const svn_fs_id_t *id,
                const char *start_node_id,
                const char *start_copy_id,
                apr_off_t initial_offset,
                apr_array_header_t *reps_to_cache,
                apr_hash_t *reps_hash,
                apr_pool_t *reps_pool,
                svn_boolean_t at_root,
                apr_pool_t *pool)
{
  node_revision_t *noderev;
  apr_off_t my_offset;
  char my_node_id_buf[MAX_KEY_SIZE + 2];
  char my_copy_id_buf[MAX_KEY_SIZE + 2];
  const svn_fs_id_t *new_id;
  const char *node_id, *copy_id, *my_node_id, *my_copy_id;
  fs_fs_data_t *ffd = fs->fsap_data;

  *new_id_p = NULL;

  /* Check to see if this is a transaction node. */
  if (! svn_fs_fs__id_txn_id(id))
    return SVN_NO_ERROR;

  SVN_ERR(svn_fs_fs__get_node_revision(&noderev, fs, id, pool));

  if (noderev->kind == svn_node_dir)
    {
      apr_pool_t *subpool;
      apr_hash_t *entries, *str_entries;
      apr_array_header_t *sorted_entries;
      int i;

      /* This is a directory.  Write out all the children first. */
      subpool = svn_pool_create(pool);

      SVN_ERR(svn_fs_fs__rep_contents_dir(&entries, fs, noderev, pool));
      /* For the sake of the repository administrator sort the entries
         so that the final file is deterministic and repeatable,
         however the rest of the FSFS code doesn't require any
         particular order here. */
      sorted_entries = svn_sort__hash(entries, svn_sort_compare_items_lexically,
                                      pool);
      for (i = 0; i < sorted_entries->nelts; ++i)
        {
          svn_fs_dirent_t *dirent = APR_ARRAY_IDX(sorted_entries, i,
                                                  svn_sort__item_t).value;

          svn_pool_clear(subpool);
          SVN_ERR(write_final_rev(&new_id, file, rev, fs, dirent->id,
                                  start_node_id, start_copy_id, initial_offset,
                                  reps_to_cache, reps_hash, reps_pool, FALSE,
                                  subpool));
          if (new_id && (svn_fs_fs__id_rev(new_id) == rev))
            dirent->id = svn_fs_fs__id_copy(new_id, pool);
        }
      svn_pool_destroy(subpool);

      if (noderev->data_rep && noderev->data_rep->txn_id)
        {
          /* Write out the contents of this directory as a text rep. */
          SVN_ERR(unparse_dir_entries(&str_entries, entries, pool));

          noderev->data_rep->txn_id = NULL;
          noderev->data_rep->revision = rev;

          if (ffd->deltify_directories)
            SVN_ERR(write_hash_delta_rep(noderev->data_rep, file,
                                         str_entries, fs, noderev, NULL,
                                         FALSE, pool));
          else
            SVN_ERR(write_hash_rep(noderev->data_rep, file, str_entries,
                                   fs, NULL, pool));
        }
    }
  else
    {
      /* This is a file.  We should make sure the data rep, if it
         exists in a "this" state, gets rewritten to our new revision
         num. */

      if (noderev->data_rep && noderev->data_rep->txn_id)
        {
          noderev->data_rep->txn_id = NULL;
          noderev->data_rep->revision = rev;

          /* See issue 3845.  Some unknown mechanism caused the
             protorev file to get truncated, so check for that
             here.  */
          if (noderev->data_rep->offset + noderev->data_rep->size
              > initial_offset)
            return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                                    _("Truncated protorev file detected"));
        }
    }

  /* Fix up the property reps. */
  if (noderev->prop_rep && noderev->prop_rep->txn_id)
    {
      apr_hash_t *proplist;
      SVN_ERR(svn_fs_fs__get_proplist(&proplist, fs, noderev, pool));

      noderev->prop_rep->txn_id = NULL;
      noderev->prop_rep->revision = rev;

      if (ffd->deltify_properties)
        SVN_ERR(write_hash_delta_rep(noderev->prop_rep, file,
                                     proplist, fs, noderev, reps_hash,
                                     TRUE, pool));
      else
        SVN_ERR(write_hash_rep(noderev->prop_rep, file, proplist,
                               fs, reps_hash, pool));
    }


  /* Convert our temporary ID into a permanent revision one. */
  SVN_ERR(get_file_offset(&my_offset, file, pool));

  node_id = svn_fs_fs__id_node_id(noderev->id);
  if (*node_id == '_')
    {
      if (ffd->format >= SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT)
        my_node_id = apr_psprintf(pool, "%s-%ld", node_id + 1, rev);
      else
        {
          svn_fs_fs__add_keys(start_node_id, node_id + 1, my_node_id_buf);
          my_node_id = my_node_id_buf;
        }
    }
  else
    my_node_id = node_id;

  copy_id = svn_fs_fs__id_copy_id(noderev->id);
  if (*copy_id == '_')
    {
      if (ffd->format >= SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT)
        my_copy_id = apr_psprintf(pool, "%s-%ld", copy_id + 1, rev);
      else
        {
          svn_fs_fs__add_keys(start_copy_id, copy_id + 1, my_copy_id_buf);
          my_copy_id = my_copy_id_buf;
        }
    }
  else
    my_copy_id = copy_id;

  if (noderev->copyroot_rev == SVN_INVALID_REVNUM)
    noderev->copyroot_rev = rev;

  new_id = svn_fs_fs__id_rev_create(my_node_id, my_copy_id, rev, my_offset,
                                    pool);

  noderev->id = new_id;

  if (ffd->rep_sharing_allowed)
    {
      /* Save the data representation's hash in the rep cache. */
      if (   noderev->data_rep && noderev->kind == svn_node_file
          && noderev->data_rep->revision == rev)
        {
          SVN_ERR_ASSERT(reps_to_cache && reps_pool);
          APR_ARRAY_PUSH(reps_to_cache, representation_t *)
            = svn_fs_fs__rep_copy(noderev->data_rep, reps_pool);
        }

      if (noderev->prop_rep && noderev->prop_rep->revision == rev)
        {
          /* Add new property reps to hash and on-disk cache. */
          representation_t *copy
            = svn_fs_fs__rep_copy(noderev->prop_rep, reps_pool);

          SVN_ERR_ASSERT(reps_to_cache && reps_pool);
          APR_ARRAY_PUSH(reps_to_cache, representation_t *) = copy;

          apr_hash_set(reps_hash,
                        copy->sha1_checksum->digest,
                        APR_SHA1_DIGESTSIZE,
                        copy);
        }
    }

  /* don't serialize SHA1 for dirs to disk (waste of space) */
  if (noderev->data_rep && noderev->kind == svn_node_dir)
    noderev->data_rep->sha1_checksum = NULL;

  /* don't serialize SHA1 for props to disk (waste of space) */
  if (noderev->prop_rep)
    noderev->prop_rep->sha1_checksum = NULL;

  /* Workaround issue #4031: is-fresh-txn-root in revision files. */
  noderev->is_fresh_txn_root = FALSE;

  /* Write out our new node-revision. */
  if (at_root)
    SVN_ERR(validate_root_noderev(fs, noderev, rev, pool));

  SVN_ERR(svn_fs_fs__write_noderev(svn_stream_from_aprfile2(file, TRUE, pool),
                                   noderev, ffd->format,
                                   svn_fs_fs__fs_supports_mergeinfo(fs),
                                   pool));

  /* Return our ID that references the revision file. */
  *new_id_p = noderev->id;

  return SVN_NO_ERROR;
}

/* Write the changed path info from transaction TXN_ID in filesystem
   FS to the permanent rev-file FILE.  *OFFSET_P is set the to offset
   in the file of the beginning of this information.  Perform
   temporary allocations in POOL. */
static svn_error_t *
write_final_changed_path_info(apr_off_t *offset_p,
                              apr_file_t *file,
                              svn_fs_t *fs,
                              const char *txn_id,
                              apr_pool_t *pool)
{
  apr_hash_t *changed_paths;
  apr_off_t offset;
  apr_pool_t *iterpool = svn_pool_create(pool);
  fs_fs_data_t *ffd = fs->fsap_data;
  svn_boolean_t include_node_kinds =
      ffd->format >= SVN_FS_FS__MIN_KIND_IN_CHANGED_FORMAT;
  apr_array_header_t *sorted_changed_paths;
  int i;

  SVN_ERR(get_file_offset(&offset, file, pool));

  SVN_ERR(svn_fs_fs__txn_changes_fetch(&changed_paths, fs, txn_id, pool));
  /* For the sake of the repository administrator sort the changes so
     that the final file is deterministic and repeatable, however the
     rest of the FSFS code doesn't require any particular order here. */
  sorted_changed_paths = svn_sort__hash(changed_paths,
                                        svn_sort_compare_items_lexically, pool);

  /* Iterate through the changed paths one at a time, and convert the
     temporary node-id into a permanent one for each change entry. */
  for (i = 0; i < sorted_changed_paths->nelts; ++i)
    {
      node_revision_t *noderev;
      const svn_fs_id_t *id;
      svn_fs_path_change2_t *change;
      const char *path;

      svn_pool_clear(iterpool);

      change = APR_ARRAY_IDX(sorted_changed_paths, i, svn_sort__item_t).value;
      path = APR_ARRAY_IDX(sorted_changed_paths, i, svn_sort__item_t).key;

      id = change->node_rev_id;

      /* If this was a delete of a mutable node, then it is OK to
         leave the change entry pointing to the non-existent temporary
         node, since it will never be used. */
      if ((change->change_kind != svn_fs_path_change_delete) &&
          (! svn_fs_fs__id_txn_id(id)))
        {
          SVN_ERR(svn_fs_fs__get_node_revision(&noderev, fs, id, iterpool));

          /* noderev has the permanent node-id at this point, so we just
             substitute it for the temporary one. */
          change->node_rev_id = noderev->id;
        }

      /* Write out the new entry into the final rev-file. */
      SVN_ERR(write_change_entry(file, path, change, include_node_kinds,
                                 iterpool));
    }

  svn_pool_destroy(iterpool);

  *offset_p = offset;

  return SVN_NO_ERROR;
}

/* Atomically update the 'current' file to hold the specifed REV,
   NEXT_NODE_ID, and NEXT_COPY_ID.  (The two next-ID parameters are
   ignored and may be NULL if the FS format does not use them.)
   Perform temporary allocations in POOL. */
static svn_error_t *
write_current(svn_fs_t *fs, svn_revnum_t rev, const char *next_node_id,
              const char *next_copy_id, apr_pool_t *pool)
{
  char *buf;
  const char *tmp_name, *name;
  fs_fs_data_t *ffd = fs->fsap_data;

  /* Now we can just write out this line. */
  if (ffd->format >= SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT)
    buf = apr_psprintf(pool, "%ld\n", rev);
  else
    buf = apr_psprintf(pool, "%ld %s %s\n", rev, next_node_id, next_copy_id);

  name = svn_fs_fs__path_current(fs, pool);
  SVN_ERR(svn_io_write_unique(&tmp_name,
                              svn_dirent_dirname(name, pool),
                              buf, strlen(buf),
                              svn_io_file_del_none, pool));

  return move_into_place(tmp_name, name, name, pool);
}

/* Open a new svn_fs_t handle to FS, set that handle's concept of "current
   youngest revision" to NEW_REV, and call svn_fs_fs__verify_root() on
   NEW_REV's revision root.

   Intended to be called as the very last step in a commit before 'current'
   is bumped.  This implies that we are holding the write lock. */
static svn_error_t *
verify_as_revision_before_current_plus_plus(svn_fs_t *fs,
                                            svn_revnum_t new_rev,
                                            apr_pool_t *pool)
{
#ifdef SVN_DEBUG
  fs_fs_data_t *ffd = fs->fsap_data;
  svn_fs_t *ft; /* fs++ == ft */
  svn_fs_root_t *root;
  fs_fs_data_t *ft_ffd;
  apr_hash_t *fs_config;

  SVN_ERR_ASSERT(ffd->svn_fs_open_);

  /* make sure FT does not simply return data cached by other instances
   * but actually retrieves it from disk at least once.
   */
  fs_config = apr_hash_make(pool);
  svn_hash_sets(fs_config, SVN_FS_CONFIG_FSFS_CACHE_NS,
                           svn_uuid_generate(pool));
  SVN_ERR(ffd->svn_fs_open_(&ft, fs->path,
                            fs_config,
                            pool));
  ft_ffd = ft->fsap_data;
  /* Don't let FT consult rep-cache.db, either. */
  ft_ffd->rep_sharing_allowed = FALSE;

  /* Time travel! */
  ft_ffd->youngest_rev_cache = new_rev;

  SVN_ERR(svn_fs_fs__revision_root(&root, ft, new_rev, pool));
  SVN_ERR_ASSERT(root->is_txn_root == FALSE && root->rev == new_rev);
  SVN_ERR_ASSERT(ft_ffd->youngest_rev_cache == new_rev);
  SVN_ERR(svn_fs_fs__verify_root(root, pool));
#endif /* SVN_DEBUG */

  return SVN_NO_ERROR;
}

/* Update the 'current' file to hold the correct next node and copy_ids
   from transaction TXN_ID in filesystem FS.  The current revision is
   set to REV.  Perform temporary allocations in POOL. */
static svn_error_t *
write_final_current(svn_fs_t *fs,
                    const char *txn_id,
                    svn_revnum_t rev,
                    const char *start_node_id,
                    const char *start_copy_id,
                    apr_pool_t *pool)
{
  const char *txn_node_id, *txn_copy_id;
  char new_node_id[MAX_KEY_SIZE + 2];
  char new_copy_id[MAX_KEY_SIZE + 2];
  fs_fs_data_t *ffd = fs->fsap_data;

  if (ffd->format >= SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT)
    return write_current(fs, rev, NULL, NULL, pool);

  /* To find the next available ids, we add the id that used to be in
     the 'current' file, to the next ids from the transaction file. */
  SVN_ERR(read_next_ids(&txn_node_id, &txn_copy_id, fs, txn_id, pool));

  svn_fs_fs__add_keys(start_node_id, txn_node_id, new_node_id);
  svn_fs_fs__add_keys(start_copy_id, txn_copy_id, new_copy_id);

  return write_current(fs, rev, new_node_id, new_copy_id, pool);
}

/* Verify that the user registed with FS has all the locks necessary to
   permit all the changes associate with TXN_NAME.
   The FS write lock is assumed to be held by the caller. */
static svn_error_t *
verify_locks(svn_fs_t *fs,
             const char *txn_name,
             apr_pool_t *pool)
{
  apr_pool_t *subpool = svn_pool_create(pool);
  apr_hash_t *changes;
  apr_hash_index_t *hi;
  apr_array_header_t *changed_paths;
  svn_stringbuf_t *last_recursed = NULL;
  int i;

  /* Fetch the changes for this transaction. */
  SVN_ERR(svn_fs_fs__txn_changes_fetch(&changes, fs, txn_name, pool));

  /* Make an array of the changed paths, and sort them depth-first-ily.  */
  changed_paths = apr_array_make(pool, apr_hash_count(changes) + 1,
                                 sizeof(const char *));
  for (hi = apr_hash_first(pool, changes); hi; hi = apr_hash_next(hi))
    APR_ARRAY_PUSH(changed_paths, const char *) = svn__apr_hash_index_key(hi);
  qsort(changed_paths->elts, changed_paths->nelts,
        changed_paths->elt_size, svn_sort_compare_paths);

  /* Now, traverse the array of changed paths, verify locks.  Note
     that if we need to do a recursive verification a path, we'll skip
     over children of that path when we get to them. */
  for (i = 0; i < changed_paths->nelts; i++)
    {
      const char *path;
      svn_fs_path_change2_t *change;
      svn_boolean_t recurse = TRUE;

      svn_pool_clear(subpool);
      path = APR_ARRAY_IDX(changed_paths, i, const char *);

      /* If this path has already been verified as part of a recursive
         check of one of its parents, no need to do it again.  */
      if (last_recursed
          && svn_dirent_is_child(last_recursed->data, path, subpool))
        continue;

      /* Fetch the change associated with our path.  */
      change = svn_hash_gets(changes, path);

      /* What does it mean to succeed at lock verification for a given
         path?  For an existing file or directory getting modified
         (text, props), it means we hold the lock on the file or
         directory.  For paths being added or removed, we need to hold
         the locks for that path and any children of that path.

         WHEW!  We have no reliable way to determine the node kind
         of deleted items, but fortunately we are going to do a
         recursive check on deleted paths regardless of their kind.  */
      if (change->change_kind == svn_fs_path_change_modify)
        recurse = FALSE;
      SVN_ERR(svn_fs_fs__allow_locked_operation(path, fs, recurse, TRUE,
                                                subpool));

      /* If we just did a recursive check, remember the path we
         checked (so children can be skipped).  */
      if (recurse)
        {
          if (! last_recursed)
            last_recursed = svn_stringbuf_create(path, pool);
          else
            svn_stringbuf_set(last_recursed, path);
        }
    }
  svn_pool_destroy(subpool);
  return SVN_NO_ERROR;
}

/* Baton used for commit_body below. */
struct commit_baton {
  svn_revnum_t *new_rev_p;
  svn_fs_t *fs;
  svn_fs_txn_t *txn;
  apr_array_header_t *reps_to_cache;
  apr_hash_t *reps_hash;
  apr_pool_t *reps_pool;
};

/* The work-horse for svn_fs_fs__commit, called with the FS write lock.
   This implements the svn_fs_fs__with_write_lock() 'body' callback
   type.  BATON is a 'struct commit_baton *'. */
static svn_error_t *
commit_body(void *baton, apr_pool_t *pool)
{
  struct commit_baton *cb = baton;
  fs_fs_data_t *ffd = cb->fs->fsap_data;
  const char *old_rev_filename, *rev_filename, *proto_filename;
  const char *revprop_filename, *final_revprop;
  const svn_fs_id_t *root_id, *new_root_id;
  const char *start_node_id = NULL, *start_copy_id = NULL;
  svn_revnum_t old_rev, new_rev;
  apr_file_t *proto_file;
  void *proto_file_lockcookie;
  apr_off_t initial_offset, changed_path_offset;
  char *buf;
  apr_hash_t *txnprops;
  apr_array_header_t *txnprop_list;
  svn_prop_t prop;
  svn_string_t date;

  /* Get the current youngest revision. */
  SVN_ERR(svn_fs_fs__youngest_rev(&old_rev, cb->fs, pool));

  /* Check to make sure this transaction is based off the most recent
     revision. */
  if (cb->txn->base_rev != old_rev)
    return svn_error_create(SVN_ERR_FS_TXN_OUT_OF_DATE, NULL,
                            _("Transaction out of date"));

  /* Locks may have been added (or stolen) between the calling of
     previous svn_fs.h functions and svn_fs_commit_txn(), so we need
     to re-examine every changed-path in the txn and re-verify all
     discovered locks. */
  SVN_ERR(verify_locks(cb->fs, cb->txn->id, pool));

  /* Get the next node_id and copy_id to use. */
  if (ffd->format < SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT)
    SVN_ERR(get_next_revision_ids(&start_node_id, &start_copy_id, cb->fs,
                                  pool));

  /* We are going to be one better than this puny old revision. */
  new_rev = old_rev + 1;

  /* Get a write handle on the proto revision file. */
  SVN_ERR(get_writable_proto_rev(&proto_file, &proto_file_lockcookie,
                                 cb->fs, cb->txn->id, pool));
  SVN_ERR(get_file_offset(&initial_offset, proto_file, pool));

  /* Write out all the node-revisions and directory contents. */
  root_id = svn_fs_fs__id_txn_create("0", "0", cb->txn->id, pool);
  SVN_ERR(write_final_rev(&new_root_id, proto_file, new_rev, cb->fs, root_id,
                          start_node_id, start_copy_id, initial_offset,
                          cb->reps_to_cache, cb->reps_hash, cb->reps_pool,
                          TRUE, pool));

  /* Write the changed-path information. */
  SVN_ERR(write_final_changed_path_info(&changed_path_offset, proto_file,
                                        cb->fs, cb->txn->id, pool));

  /* Write the final line. */
  buf = apr_psprintf(pool, "\n%" APR_OFF_T_FMT " %" APR_OFF_T_FMT "\n",
                     svn_fs_fs__id_offset(new_root_id),
                     changed_path_offset);
  SVN_ERR(svn_io_file_write_full(proto_file, buf, strlen(buf), NULL,
                                 pool));
  SVN_ERR(svn_io_file_flush_to_disk(proto_file, pool));
  SVN_ERR(svn_io_file_close(proto_file, pool));

  /* We don't unlock the prototype revision file immediately to avoid a
     race with another caller writing to the prototype revision file
     before we commit it. */

  /* Remove any temporary txn props representing 'flags'. */
  SVN_ERR(svn_fs_fs__txn_proplist(&txnprops, cb->txn, pool));
  txnprop_list = apr_array_make(pool, 3, sizeof(svn_prop_t));
  prop.value = NULL;

  if (svn_hash_gets(txnprops, SVN_FS__PROP_TXN_CHECK_OOD))
    {
      prop.name = SVN_FS__PROP_TXN_CHECK_OOD;
      APR_ARRAY_PUSH(txnprop_list, svn_prop_t) = prop;
    }

  if (svn_hash_gets(txnprops, SVN_FS__PROP_TXN_CHECK_LOCKS))
    {
      prop.name = SVN_FS__PROP_TXN_CHECK_LOCKS;
      APR_ARRAY_PUSH(txnprop_list, svn_prop_t) = prop;
    }

  if (! apr_is_empty_array(txnprop_list))
    SVN_ERR(svn_fs_fs__change_txn_props(cb->txn, txnprop_list, pool));

  /* Create the shard for the rev and revprop file, if we're sharding and
     this is the first revision of a new shard.  We don't care if this
     fails because the shard already existed for some reason. */
  if (ffd->max_files_per_dir && new_rev % ffd->max_files_per_dir == 0)
    {
      /* Create the revs shard. */
        {
          const char *new_dir = path_rev_shard(cb->fs, new_rev, pool);
          svn_error_t *err = svn_io_dir_make(new_dir, APR_OS_DEFAULT, pool);
          if (err && !APR_STATUS_IS_EEXIST(err->apr_err))
            return svn_error_trace(err);
          svn_error_clear(err);
          SVN_ERR(svn_io_copy_perms(svn_dirent_join(cb->fs->path,
                                                    PATH_REVS_DIR,
                                                    pool),
                                    new_dir, pool));
        }

      /* Create the revprops shard. */
      SVN_ERR_ASSERT(! is_packed_revprop(cb->fs, new_rev));
        {
          const char *new_dir = path_revprops_shard(cb->fs, new_rev, pool);
          svn_error_t *err = svn_io_dir_make(new_dir, APR_OS_DEFAULT, pool);
          if (err && !APR_STATUS_IS_EEXIST(err->apr_err))
            return svn_error_trace(err);
          svn_error_clear(err);
          SVN_ERR(svn_io_copy_perms(svn_dirent_join(cb->fs->path,
                                                    PATH_REVPROPS_DIR,
                                                    pool),
                                    new_dir, pool));
        }
    }

  /* Move the finished rev file into place. */
  SVN_ERR(svn_fs_fs__path_rev_absolute(&old_rev_filename,
                                       cb->fs, old_rev, pool));
  rev_filename = path_rev(cb->fs, new_rev, pool);
  proto_filename = path_txn_proto_rev(cb->fs, cb->txn->id, pool);
  SVN_ERR(move_into_place(proto_filename, rev_filename, old_rev_filename,
                          pool));

  /* Now that we've moved the prototype revision file out of the way,
     we can unlock it (since further attempts to write to the file
     will fail as it no longer exists).  We must do this so that we can
     remove the transaction directory later. */
  SVN_ERR(unlock_proto_rev(cb->fs, cb->txn->id, proto_file_lockcookie, pool));

  /* Update commit time to ensure that svn:date revprops remain ordered. */
  date.data = svn_time_to_cstring(apr_time_now(), pool);
  date.len = strlen(date.data);

  SVN_ERR(svn_fs_fs__change_txn_prop(cb->txn, SVN_PROP_REVISION_DATE,
                                     &date, pool));

  /* Move the revprops file into place. */
  SVN_ERR_ASSERT(! is_packed_revprop(cb->fs, new_rev));
  revprop_filename = path_txn_props(cb->fs, cb->txn->id, pool);
  final_revprop = path_revprops(cb->fs, new_rev, pool);
  SVN_ERR(move_into_place(revprop_filename, final_revprop,
                          old_rev_filename, pool));

  /* Update the 'current' file. */
  SVN_ERR(verify_as_revision_before_current_plus_plus(cb->fs, new_rev, pool));
  SVN_ERR(write_final_current(cb->fs, cb->txn->id, new_rev, start_node_id,
                              start_copy_id, pool));

  /* At this point the new revision is committed and globally visible
     so let the caller know it succeeded by giving it the new revision
     number, which fulfills svn_fs_commit_txn() contract.  Any errors
     after this point do not change the fact that a new revision was
     created. */
  *cb->new_rev_p = new_rev;

  ffd->youngest_rev_cache = new_rev;

  /* Remove this transaction directory. */
  SVN_ERR(svn_fs_fs__purge_txn(cb->fs, cb->txn->id, pool));

  return SVN_NO_ERROR;
}

/* Add the representations in REPS_TO_CACHE (an array of representation_t *)
 * to the rep-cache database of FS. */
static svn_error_t *
write_reps_to_cache(svn_fs_t *fs,
                    const apr_array_header_t *reps_to_cache,
                    apr_pool_t *scratch_pool)
{
  int i;

  for (i = 0; i < reps_to_cache->nelts; i++)
    {
      representation_t *rep = APR_ARRAY_IDX(reps_to_cache, i, representation_t *);

      /* FALSE because we don't care if another parallel commit happened to
       * collide with us.  (Non-parallel collisions will not be detected.) */
      SVN_ERR(svn_fs_fs__set_rep_reference(fs, rep, FALSE, scratch_pool));
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__commit(svn_revnum_t *new_rev_p,
                  svn_fs_t *fs,
                  svn_fs_txn_t *txn,
                  apr_pool_t *pool)
{
  struct commit_baton cb;
  fs_fs_data_t *ffd = fs->fsap_data;

  cb.new_rev_p = new_rev_p;
  cb.fs = fs;
  cb.txn = txn;

  if (ffd->rep_sharing_allowed)
    {
      cb.reps_to_cache = apr_array_make(pool, 5, sizeof(representation_t *));
      cb.reps_hash = apr_hash_make(pool);
      cb.reps_pool = pool;
    }
  else
    {
      cb.reps_to_cache = NULL;
      cb.reps_hash = NULL;
      cb.reps_pool = NULL;
    }

  SVN_ERR(svn_fs_fs__with_write_lock(fs, commit_body, &cb, pool));

  /* At this point, *NEW_REV_P has been set, so errors below won't affect
     the success of the commit.  (See svn_fs_commit_txn().)  */

  if (ffd->rep_sharing_allowed)
    {
      SVN_ERR(svn_fs_fs__open_rep_cache(fs, pool));

      /* Write new entries to the rep-sharing database.
       *
       * We use an sqlite transaction to speed things up;
       * see <http://www.sqlite.org/faq.html#q19>.
       */
      SVN_SQLITE__WITH_TXN(
        write_reps_to_cache(fs, cb.reps_to_cache, pool),
        ffd->rep_cache_db);
    }

  return SVN_NO_ERROR;
}


svn_error_t *
svn_fs_fs__reserve_copy_id(const char **copy_id_p,
                           svn_fs_t *fs,
                           const char *txn_id,
                           apr_pool_t *pool)
{
  const char *cur_node_id, *cur_copy_id;
  char *copy_id;
  apr_size_t len;

  /* First read in the current next-ids file. */
  SVN_ERR(read_next_ids(&cur_node_id, &cur_copy_id, fs, txn_id, pool));

  copy_id = apr_pcalloc(pool, strlen(cur_copy_id) + 2);

  len = strlen(cur_copy_id);
  svn_fs_fs__next_key(cur_copy_id, &len, copy_id);

  SVN_ERR(write_next_ids(fs, txn_id, cur_node_id, copy_id, pool));

  *copy_id_p = apr_pstrcat(pool, "_", cur_copy_id, (char *)NULL);

  return SVN_NO_ERROR;
}

/* Write out the zeroth revision for filesystem FS. */
static svn_error_t *
write_revision_zero(svn_fs_t *fs)
{
  const char *path_revision_zero = path_rev(fs, 0, fs->pool);
  apr_hash_t *proplist;
  svn_string_t date;

  /* Write out a rev file for revision 0. */
  SVN_ERR(svn_io_file_create(path_revision_zero,
                             "PLAIN\nEND\nENDREP\n"
                             "id: 0.0.r0/17\n"
                             "type: dir\n"
                             "count: 0\n"
                             "text: 0 0 4 4 "
                             "2d2977d1c96f487abe4a1e202dd03b4e\n"
                             "cpath: /\n"
                             "\n\n17 107\n", fs->pool));
  SVN_ERR(svn_io_set_file_read_only(path_revision_zero, FALSE, fs->pool));

  /* Set a date on revision 0. */
  date.data = svn_time_to_cstring(apr_time_now(), fs->pool);
  date.len = strlen(date.data);
  proplist = apr_hash_make(fs->pool);
  svn_hash_sets(proplist, SVN_PROP_REVISION_DATE, &date);
  return set_revision_proplist(fs, 0, proplist, fs->pool);
}

svn_error_t *
svn_fs_fs__create(svn_fs_t *fs,
                  const char *path,
                  apr_pool_t *pool)
{
  int format = SVN_FS_FS__FORMAT_NUMBER;
  fs_fs_data_t *ffd = fs->fsap_data;

  fs->path = apr_pstrdup(pool, path);
  /* See if compatibility with older versions was explicitly requested. */
  if (fs->config)
    {
      if (svn_hash_gets(fs->config, SVN_FS_CONFIG_PRE_1_4_COMPATIBLE))
        format = 1;
      else if (svn_hash_gets(fs->config, SVN_FS_CONFIG_PRE_1_5_COMPATIBLE))
        format = 2;
      else if (svn_hash_gets(fs->config, SVN_FS_CONFIG_PRE_1_6_COMPATIBLE))
        format = 3;
      else if (svn_hash_gets(fs->config, SVN_FS_CONFIG_PRE_1_8_COMPATIBLE))
        format = 4;
    }
  ffd->format = format;

  /* Override the default linear layout if this is a new-enough format. */
  if (format >= SVN_FS_FS__MIN_LAYOUT_FORMAT_OPTION_FORMAT)
    ffd->max_files_per_dir = SVN_FS_FS_DEFAULT_MAX_FILES_PER_DIR;

  /* Create the revision data directories. */
  if (ffd->max_files_per_dir)
    SVN_ERR(svn_io_make_dir_recursively(path_rev_shard(fs, 0, pool), pool));
  else
    SVN_ERR(svn_io_make_dir_recursively(svn_dirent_join(path, PATH_REVS_DIR,
                                                        pool),
                                        pool));

  /* Create the revprops directory. */
  if (ffd->max_files_per_dir)
    SVN_ERR(svn_io_make_dir_recursively(path_revprops_shard(fs, 0, pool),
                                        pool));
  else
    SVN_ERR(svn_io_make_dir_recursively(svn_dirent_join(path,
                                                        PATH_REVPROPS_DIR,
                                                        pool),
                                        pool));

  /* Create the transaction directory. */
  SVN_ERR(svn_io_make_dir_recursively(svn_dirent_join(path, PATH_TXNS_DIR,
                                                      pool),
                                      pool));

  /* Create the protorevs directory. */
  if (format >= SVN_FS_FS__MIN_PROTOREVS_DIR_FORMAT)
    SVN_ERR(svn_io_make_dir_recursively(svn_dirent_join(path, PATH_TXN_PROTOS_DIR,
                                                      pool),
                                        pool));

  /* Create the 'current' file. */
  SVN_ERR(svn_io_file_create(svn_fs_fs__path_current(fs, pool),
                             (format >= SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT
                              ? "0\n" : "0 1 1\n"),
                             pool));
  SVN_ERR(svn_io_file_create(path_lock(fs, pool), "", pool));
  SVN_ERR(svn_fs_fs__set_uuid(fs, NULL, pool));

  SVN_ERR(write_revision_zero(fs));

  /* Create the fsfs.conf file if supported.  Older server versions would
     simply ignore the file but that might result in a different behavior
     than with the later releases.  Also, hotcopy would ignore, i.e. not
     copy, a fsfs.conf with old formats. */
  if (ffd->format >= SVN_FS_FS__MIN_CONFIG_FILE)
    SVN_ERR(write_config(fs, pool));

  SVN_ERR(read_config(ffd, fs->path, pool));

  /* Create the min unpacked rev file. */
  if (ffd->format >= SVN_FS_FS__MIN_PACKED_FORMAT)
    SVN_ERR(svn_io_file_create(path_min_unpacked_rev(fs, pool), "0\n", pool));

  /* Create the txn-current file if the repository supports
     the transaction sequence file. */
  if (format >= SVN_FS_FS__MIN_TXN_CURRENT_FORMAT)
    {
      SVN_ERR(svn_io_file_create(path_txn_current(fs, pool),
                                 "0\n", pool));
      SVN_ERR(svn_io_file_create(path_txn_current_lock(fs, pool),
                                 "", pool));
    }

  /* This filesystem is ready.  Stamp it with a format number. */
  SVN_ERR(write_format(path_format(fs, pool),
                       ffd->format, ffd->max_files_per_dir, FALSE, pool));

  ffd->youngest_rev_cache = 0;
  return SVN_NO_ERROR;
}

/* Part of the recovery procedure.  Return the largest revision *REV in
   filesystem FS.  Use POOL for temporary allocation. */
static svn_error_t *
recover_get_largest_revision(svn_fs_t *fs, svn_revnum_t *rev, apr_pool_t *pool)
{
  /* Discovering the largest revision in the filesystem would be an
     expensive operation if we did a readdir() or searched linearly,
     so we'll do a form of binary search.  left is a revision that we
     know exists, right a revision that we know does not exist. */
  apr_pool_t *iterpool;
  svn_revnum_t left, right = 1;

  iterpool = svn_pool_create(pool);
  /* Keep doubling right, until we find a revision that doesn't exist. */
  while (1)
    {
      svn_error_t *err;
      apr_file_t *file;

      err = open_pack_or_rev_file(&file, fs, right, iterpool);
      svn_pool_clear(iterpool);

      if (err && err->apr_err == SVN_ERR_FS_NO_SUCH_REVISION)
        {
          svn_error_clear(err);
          break;
        }
      else
        SVN_ERR(err);

      right <<= 1;
    }

  left = right >> 1;

  /* We know that left exists and right doesn't.  Do a normal bsearch to find
     the last revision. */
  while (left + 1 < right)
    {
      svn_revnum_t probe = left + ((right - left) / 2);
      svn_error_t *err;
      apr_file_t *file;

      err = open_pack_or_rev_file(&file, fs, probe, iterpool);
      svn_pool_clear(iterpool);

      if (err && err->apr_err == SVN_ERR_FS_NO_SUCH_REVISION)
        {
          svn_error_clear(err);
          right = probe;
        }
      else
        {
          SVN_ERR(err);
          left = probe;
        }
    }

  svn_pool_destroy(iterpool);

  /* left is now the largest revision that exists. */
  *rev = left;
  return SVN_NO_ERROR;
}

/* A baton for reading a fixed amount from an open file.  For
   recover_find_max_ids() below. */
struct recover_read_from_file_baton
{
  apr_file_t *file;
  apr_pool_t *pool;
  apr_off_t remaining;
};

/* A stream read handler used by recover_find_max_ids() below.
   Read and return at most BATON->REMAINING bytes from the stream,
   returning nothing after that to indicate EOF. */
static svn_error_t *
read_handler_recover(void *baton, char *buffer, apr_size_t *len)
{
  struct recover_read_from_file_baton *b = baton;
  svn_filesize_t bytes_to_read = *len;

  if (b->remaining == 0)
    {
      /* Return a successful read of zero bytes to signal EOF. */
      *len = 0;
      return SVN_NO_ERROR;
    }

  if (bytes_to_read > b->remaining)
    bytes_to_read = b->remaining;
  b->remaining -= bytes_to_read;

  return svn_io_file_read_full2(b->file, buffer, (apr_size_t) bytes_to_read,
                                len, NULL, b->pool);
}

/* Part of the recovery procedure.  Read the directory noderev at offset
   OFFSET of file REV_FILE (the revision file of revision REV of
   filesystem FS), and set MAX_NODE_ID and MAX_COPY_ID to be the node-id
   and copy-id of that node, if greater than the current value stored
   in either.  Recurse into any child directories that were modified in
   this revision.

   MAX_NODE_ID and MAX_COPY_ID must be arrays of at least MAX_KEY_SIZE.

   Perform temporary allocation in POOL. */
static svn_error_t *
recover_find_max_ids(svn_fs_t *fs, svn_revnum_t rev,
                     apr_file_t *rev_file, apr_off_t offset,
                     char *max_node_id, char *max_copy_id,
                     apr_pool_t *pool)
{
  apr_hash_t *headers;
  char *value;
  representation_t *data_rep;
  struct rep_args *ra;
  struct recover_read_from_file_baton baton;
  svn_stream_t *stream;
  apr_hash_t *entries;
  apr_hash_index_t *hi;
  apr_pool_t *iterpool;

  SVN_ERR(svn_io_file_seek(rev_file, APR_SET, &offset, pool));
  SVN_ERR(read_header_block(&headers, svn_stream_from_aprfile2(rev_file, TRUE,
                                                               pool),
                            pool));

  /* Check that this is a directory.  It should be. */
  value = svn_hash_gets(headers, HEADER_TYPE);
  if (value == NULL || strcmp(value, KIND_DIR) != 0)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Recovery encountered a non-directory node"));

  /* Get the data location.  No data location indicates an empty directory. */
  value = svn_hash_gets(headers, HEADER_TEXT);
  if (!value)
    return SVN_NO_ERROR;
  SVN_ERR(read_rep_offsets(&data_rep, value, NULL, FALSE, pool));

  /* If the directory's data representation wasn't changed in this revision,
     we've already scanned the directory's contents for noderevs, so we don't
     need to again.  This will occur if a property is changed on a directory
     without changing the directory's contents. */
  if (data_rep->revision != rev)
    return SVN_NO_ERROR;

  /* We could use get_dir_contents(), but this is much cheaper.  It does
     rely on directory entries being stored as PLAIN reps, though. */
  offset = data_rep->offset;
  SVN_ERR(svn_io_file_seek(rev_file, APR_SET, &offset, pool));
  SVN_ERR(read_rep_line(&ra, rev_file, pool));
  if (ra->is_delta)
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Recovery encountered a deltified directory "
                              "representation"));

  /* Now create a stream that's allowed to read only as much data as is
     stored in the representation. */
  baton.file = rev_file;
  baton.pool = pool;
  baton.remaining = data_rep->expanded_size
                  ? data_rep->expanded_size
                  : data_rep->size;
  stream = svn_stream_create(&baton, pool);
  svn_stream_set_read(stream, read_handler_recover);

  /* Now read the entries from that stream. */
  entries = apr_hash_make(pool);
  SVN_ERR(svn_hash_read2(entries, stream, SVN_HASH_TERMINATOR, pool));
  SVN_ERR(svn_stream_close(stream));

  /* Now check each of the entries in our directory to find new node and
     copy ids, and recurse into new subdirectories. */
  iterpool = svn_pool_create(pool);
  for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi))
    {
      char *str_val;
      char *str;
      svn_node_kind_t kind;
      svn_fs_id_t *id;
      const char *node_id, *copy_id;
      apr_off_t child_dir_offset;
      const svn_string_t *path = svn__apr_hash_index_val(hi);

      svn_pool_clear(iterpool);

      str_val = apr_pstrdup(iterpool, path->data);

      str = svn_cstring_tokenize(" ", &str_val);
      if (str == NULL)
        return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                                _("Directory entry corrupt"));

      if (strcmp(str, KIND_FILE) == 0)
        kind = svn_node_file;
      else if (strcmp(str, KIND_DIR) == 0)
        kind = svn_node_dir;
      else
        {
          return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                                  _("Directory entry corrupt"));
        }

      str = svn_cstring_tokenize(" ", &str_val);
      if (str == NULL)
        return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                                _("Directory entry corrupt"));

      id = svn_fs_fs__id_parse(str, strlen(str), iterpool);

      if (svn_fs_fs__id_rev(id) != rev)
        {
          /* If the node wasn't modified in this revision, we've already
             checked the node and copy id. */
          continue;
        }

      node_id = svn_fs_fs__id_node_id(id);
      copy_id = svn_fs_fs__id_copy_id(id);

      if (svn_fs_fs__key_compare(node_id, max_node_id) > 0)
        {
          SVN_ERR_ASSERT(strlen(node_id) < MAX_KEY_SIZE);
          apr_cpystrn(max_node_id, node_id, MAX_KEY_SIZE);
        }
      if (svn_fs_fs__key_compare(copy_id, max_copy_id) > 0)
        {
          SVN_ERR_ASSERT(strlen(copy_id) < MAX_KEY_SIZE);
          apr_cpystrn(max_copy_id, copy_id, MAX_KEY_SIZE);
        }

      if (kind == svn_node_file)
        continue;

      child_dir_offset = svn_fs_fs__id_offset(id);
      SVN_ERR(recover_find_max_ids(fs, rev, rev_file, child_dir_offset,
                                   max_node_id, max_copy_id, iterpool));
    }
  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

/* Return TRUE, if for REVISION in FS, we can find the revprop pack file.
 * Use POOL for temporary allocations.
 * Set *MISSING, if the reason is a missing manifest or pack file.
 */
static svn_boolean_t
packed_revprop_available(svn_boolean_t *missing,
                         svn_fs_t *fs,
                         svn_revnum_t revision,
                         apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  svn_stringbuf_t *content = NULL;

  /* try to read the manifest file */
  const char *folder = path_revprops_pack_shard(fs, revision, pool);
  const char *manifest_path = svn_dirent_join(folder, PATH_MANIFEST, pool);

  svn_error_t *err = try_stringbuf_from_file(&content,
                                             missing,
                                             manifest_path,
                                             FALSE,
                                             pool);

  /* if the manifest cannot be read, consider the pack files inaccessible
   * even if the file itself exists. */
  if (err)
    {
      svn_error_clear(err);
      return FALSE;
    }

  if (*missing)
    return FALSE;

  /* parse manifest content until we find the entry for REVISION.
   * Revision 0 is never packed. */
  revision = revision < ffd->max_files_per_dir
           ? revision - 1
           : revision % ffd->max_files_per_dir;
  while (content->data)
    {
      char *next = strchr(content->data, '\n');
      if (next)
        {
          *next = 0;
          ++next;
        }

      if (revision-- == 0)
        {
          /* the respective pack file must exist (and be a file) */
          svn_node_kind_t kind;
          err = svn_io_check_path(svn_dirent_join(folder, content->data,
                                                  pool),
                                  &kind, pool);
          if (err)
            {
              svn_error_clear(err);
              return FALSE;
            }

          *missing = kind == svn_node_none;
          return kind == svn_node_file;
        }

      content->data = next;
    }

  return FALSE;
}

/* Baton used for recover_body below. */
struct recover_baton {
  svn_fs_t *fs;
  svn_cancel_func_t cancel_func;
  void *cancel_baton;
};

/* The work-horse for svn_fs_fs__recover, called with the FS
   write lock.  This implements the svn_fs_fs__with_write_lock()
   'body' callback type.  BATON is a 'struct recover_baton *'. */
static svn_error_t *
recover_body(void *baton, apr_pool_t *pool)
{
  struct recover_baton *b = baton;
  svn_fs_t *fs = b->fs;
  fs_fs_data_t *ffd = fs->fsap_data;
  svn_revnum_t max_rev;
  char next_node_id_buf[MAX_KEY_SIZE], next_copy_id_buf[MAX_KEY_SIZE];
  char *next_node_id = NULL, *next_copy_id = NULL;
  svn_revnum_t youngest_rev;
  svn_node_kind_t youngest_revprops_kind;

  /* Lose potentially corrupted data in temp files */
  SVN_ERR(cleanup_revprop_namespace(fs));

  /* We need to know the largest revision in the filesystem. */
  SVN_ERR(recover_get_largest_revision(fs, &max_rev, pool));

  /* Get the expected youngest revision */
  SVN_ERR(get_youngest(&youngest_rev, fs->path, pool));

  /* Policy note:

     Since the revprops file is written after the revs file, the true
     maximum available revision is the youngest one for which both are
     present.  That's probably the same as the max_rev we just found,
     but if it's not, we could, in theory, repeatedly decrement
     max_rev until we find a revision that has both a revs and
     revprops file, then write db/current with that.

     But we choose not to.  If a repository is so corrupt that it's
     missing at least one revprops file, we shouldn't assume that the
     youngest revision for which both the revs and revprops files are
     present is healthy.  In other words, we're willing to recover
     from a missing or out-of-date db/current file, because db/current
     is truly redundant -- it's basically a cache so we don't have to
     find max_rev each time, albeit a cache with unusual semantics,
     since it also officially defines when a revision goes live.  But
     if we're missing more than the cache, it's time to back out and
     let the admin reconstruct things by hand: correctness at that
     point may depend on external things like checking a commit email
     list, looking in particular working copies, etc.

     This policy matches well with a typical naive backup scenario.
     Say you're rsyncing your FSFS repository nightly to the same
     location.  Once revs and revprops are written, you've got the
     maximum rev; if the backup should bomb before db/current is
     written, then db/current could stay arbitrarily out-of-date, but
     we can still recover.  It's a small window, but we might as well
     do what we can. */

  /* Even if db/current were missing, it would be created with 0 by
     get_youngest(), so this conditional remains valid. */
  if (youngest_rev > max_rev)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Expected current rev to be <= %ld "
                               "but found %ld"), max_rev, youngest_rev);

  /* We only need to search for maximum IDs for old FS formats which
     se global ID counters. */
  if (ffd->format < SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT)
    {
      /* Next we need to find the maximum node id and copy id in use across the
         filesystem.  Unfortunately, the only way we can get this information
         is to scan all the noderevs of all the revisions and keep track as
         we go along. */
      svn_revnum_t rev;
      apr_pool_t *iterpool = svn_pool_create(pool);
      char max_node_id[MAX_KEY_SIZE] = "0", max_copy_id[MAX_KEY_SIZE] = "0";
      apr_size_t len;

      for (rev = 0; rev <= max_rev; rev++)
        {
          apr_file_t *rev_file;
          apr_off_t root_offset;

          svn_pool_clear(iterpool);

          if (b->cancel_func)
            SVN_ERR(b->cancel_func(b->cancel_baton));

          SVN_ERR(open_pack_or_rev_file(&rev_file, fs, rev, iterpool));
          SVN_ERR(get_root_changes_offset(&root_offset, NULL, rev_file, fs, rev,
                                          iterpool));
          SVN_ERR(recover_find_max_ids(fs, rev, rev_file, root_offset,
                                       max_node_id, max_copy_id, iterpool));
          SVN_ERR(svn_io_file_close(rev_file, iterpool));
        }
      svn_pool_destroy(iterpool);

      /* Now that we finally have the maximum revision, node-id and copy-id, we
         can bump the two ids to get the next of each. */
      len = strlen(max_node_id);
      svn_fs_fs__next_key(max_node_id, &len, next_node_id_buf);
      next_node_id = next_node_id_buf;
      len = strlen(max_copy_id);
      svn_fs_fs__next_key(max_copy_id, &len, next_copy_id_buf);
      next_copy_id = next_copy_id_buf;
    }

  /* Before setting current, verify that there is a revprops file
     for the youngest revision.  (Issue #2992) */
  SVN_ERR(svn_io_check_path(path_revprops(fs, max_rev, pool),
                            &youngest_revprops_kind, pool));
  if (youngest_revprops_kind == svn_node_none)
    {
      svn_boolean_t missing = TRUE;
      if (!packed_revprop_available(&missing, fs, max_rev, pool))
        {
          if (missing)
            {
              return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                      _("Revision %ld has a revs file but no "
                                        "revprops file"),
                                      max_rev);
            }
          else
            {
              return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                      _("Revision %ld has a revs file but the "
                                        "revprops file is inaccessible"),
                                      max_rev);
            }
          }
    }
  else if (youngest_revprops_kind != svn_node_file)
    {
      return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                               _("Revision %ld has a non-file where its "
                                 "revprops file should be"),
                               max_rev);
    }

  /* Prune younger-than-(newfound-youngest) revisions from the rep
     cache if sharing is enabled taking care not to create the cache
     if it does not exist. */
  if (ffd->rep_sharing_allowed)
    {
      svn_boolean_t rep_cache_exists;

      SVN_ERR(svn_fs_fs__exists_rep_cache(&rep_cache_exists, fs, pool));
      if (rep_cache_exists)
        SVN_ERR(svn_fs_fs__del_rep_reference(fs, max_rev, pool));
    }

  /* Now store the discovered youngest revision, and the next IDs if
     relevant, in a new 'current' file. */
  return write_current(fs, max_rev, next_node_id, next_copy_id, pool);
}

/* This implements the fs_library_vtable_t.recover() API. */
svn_error_t *
svn_fs_fs__recover(svn_fs_t *fs,
                   svn_cancel_func_t cancel_func, void *cancel_baton,
                   apr_pool_t *pool)
{
  struct recover_baton b;

  /* We have no way to take out an exclusive lock in FSFS, so we're
     restricted as to the types of recovery we can do.  Luckily,
     we just want to recreate the 'current' file, and we can do that just
     by blocking other writers. */
  b.fs = fs;
  b.cancel_func = cancel_func;
  b.cancel_baton = cancel_baton;
  return svn_fs_fs__with_write_lock(fs, recover_body, &b, pool);
}

svn_error_t *
svn_fs_fs__set_uuid(svn_fs_t *fs,
                    const char *uuid,
                    apr_pool_t *pool)
{
  char *my_uuid;
  apr_size_t my_uuid_len;
  const char *tmp_path;
  const char *uuid_path = path_uuid(fs, pool);

  if (! uuid)
    uuid = svn_uuid_generate(pool);

  /* Make sure we have a copy in FS->POOL, and append a newline. */
  my_uuid = apr_pstrcat(fs->pool, uuid, "\n", (char *)NULL);
  my_uuid_len = strlen(my_uuid);

  SVN_ERR(svn_io_write_unique(&tmp_path,
                              svn_dirent_dirname(uuid_path, pool),
                              my_uuid, my_uuid_len,
                              svn_io_file_del_none, pool));

  /* We use the permissions of the 'current' file, because the 'uuid'
     file does not exist during repository creation. */
  SVN_ERR(move_into_place(tmp_path, uuid_path,
                          svn_fs_fs__path_current(fs, pool), pool));

  /* Remove the newline we added, and stash the UUID. */
  my_uuid[my_uuid_len - 1] = '\0';
  fs->uuid = my_uuid;

  return SVN_NO_ERROR;
}

/** Node origin lazy cache. */

/* If directory PATH does not exist, create it and give it the same
   permissions as FS_path.*/
svn_error_t *
svn_fs_fs__ensure_dir_exists(const char *path,
                             const char *fs_path,
                             apr_pool_t *pool)
{
  svn_error_t *err = svn_io_dir_make(path, APR_OS_DEFAULT, pool);
  if (err && APR_STATUS_IS_EEXIST(err->apr_err))
    {
      svn_error_clear(err);
      return SVN_NO_ERROR;
    }
  SVN_ERR(err);

  /* We successfully created a new directory.  Dup the permissions
     from FS->path. */
  return svn_io_copy_perms(fs_path, path, pool);
}

/* Set *NODE_ORIGINS to a hash mapping 'const char *' node IDs to
   'svn_string_t *' node revision IDs.  Use POOL for allocations. */
static svn_error_t *
get_node_origins_from_file(svn_fs_t *fs,
                           apr_hash_t **node_origins,
                           const char *node_origins_file,
                           apr_pool_t *pool)
{
  apr_file_t *fd;
  svn_error_t *err;
  svn_stream_t *stream;

  *node_origins = NULL;
  err = svn_io_file_open(&fd, node_origins_file,
                         APR_READ, APR_OS_DEFAULT, pool);
  if (err && APR_STATUS_IS_ENOENT(err->apr_err))
    {
      svn_error_clear(err);
      return SVN_NO_ERROR;
    }
  SVN_ERR(err);

  stream = svn_stream_from_aprfile2(fd, FALSE, pool);
  *node_origins = apr_hash_make(pool);
  SVN_ERR(svn_hash_read2(*node_origins, stream, SVN_HASH_TERMINATOR, pool));
  return svn_stream_close(stream);
}

svn_error_t *
svn_fs_fs__get_node_origin(const svn_fs_id_t **origin_id,
                           svn_fs_t *fs,
                           const char *node_id,
                           apr_pool_t *pool)
{
  apr_hash_t *node_origins;

  *origin_id = NULL;
  SVN_ERR(get_node_origins_from_file(fs, &node_origins,
                                     path_node_origin(fs, node_id, pool),
                                     pool));
  if (node_origins)
    {
      svn_string_t *origin_id_str =
        svn_hash_gets(node_origins, node_id);
      if (origin_id_str)
        *origin_id = svn_fs_fs__id_parse(origin_id_str->data,
                                         origin_id_str->len, pool);
    }
  return SVN_NO_ERROR;
}


/* Helper for svn_fs_fs__set_node_origin.  Takes a NODE_ID/NODE_REV_ID
   pair and adds it to the NODE_ORIGINS_PATH file.  */
static svn_error_t *
set_node_origins_for_file(svn_fs_t *fs,
                          const char *node_origins_path,
                          const char *node_id,
                          svn_string_t *node_rev_id,
                          apr_pool_t *pool)
{
  const char *path_tmp;
  svn_stream_t *stream;
  apr_hash_t *origins_hash;
  svn_string_t *old_node_rev_id;

  SVN_ERR(svn_fs_fs__ensure_dir_exists(svn_dirent_join(fs->path,
                                                       PATH_NODE_ORIGINS_DIR,
                                                       pool),
                                       fs->path, pool));

  /* Read the previously existing origins (if any), and merge our
     update with it. */
  SVN_ERR(get_node_origins_from_file(fs, &origins_hash,
                                     node_origins_path, pool));
  if (! origins_hash)
    origins_hash = apr_hash_make(pool);

  old_node_rev_id = svn_hash_gets(origins_hash, node_id);

  if (old_node_rev_id && !svn_string_compare(node_rev_id, old_node_rev_id))
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Node origin for '%s' exists with a different "
                               "value (%s) than what we were about to store "
                               "(%s)"),
                             node_id, old_node_rev_id->data, node_rev_id->data);

  svn_hash_sets(origins_hash, node_id, node_rev_id);

  /* Sure, there's a race condition here.  Two processes could be
     trying to add different cache elements to the same file at the
     same time, and the entries added by the first one to write will
     be lost.  But this is just a cache of reconstructible data, so
     we'll accept this problem in return for not having to deal with
     locking overhead. */

  /* Create a temporary file, write out our hash, and close the file. */
  SVN_ERR(svn_stream_open_unique(&stream, &path_tmp,
                                 svn_dirent_dirname(node_origins_path, pool),
                                 svn_io_file_del_none, pool, pool));
  SVN_ERR(svn_hash_write2(origins_hash, stream, SVN_HASH_TERMINATOR, pool));
  SVN_ERR(svn_stream_close(stream));

  /* Rename the temp file as the real destination */
  return svn_io_file_rename(path_tmp, node_origins_path, pool);
}


svn_error_t *
svn_fs_fs__set_node_origin(svn_fs_t *fs,
                           const char *node_id,
                           const svn_fs_id_t *node_rev_id,
                           apr_pool_t *pool)
{
  svn_error_t *err;
  const char *filename = path_node_origin(fs, node_id, pool);

  err = set_node_origins_for_file(fs, filename,
                                  node_id,
                                  svn_fs_fs__id_unparse(node_rev_id, pool),
                                  pool);
  if (err && APR_STATUS_IS_EACCES(err->apr_err))
    {
      /* It's just a cache; stop trying if I can't write. */
      svn_error_clear(err);
      err = NULL;
    }
  return svn_error_trace(err);
}


svn_error_t *
svn_fs_fs__list_transactions(apr_array_header_t **names_p,
                             svn_fs_t *fs,
                             apr_pool_t *pool)
{
  const char *txn_dir;
  apr_hash_t *dirents;
  apr_hash_index_t *hi;
  apr_array_header_t *names;
  apr_size_t ext_len = strlen(PATH_EXT_TXN);

  names = apr_array_make(pool, 1, sizeof(const char *));

  /* Get the transactions directory. */
  txn_dir = svn_dirent_join(fs->path, PATH_TXNS_DIR, pool);

  /* Now find a listing of this directory. */
  SVN_ERR(svn_io_get_dirents3(&dirents, txn_dir, TRUE, pool, pool));

  /* Loop through all the entries and return anything that ends with '.txn'. */
  for (hi = apr_hash_first(pool, dirents); hi; hi = apr_hash_next(hi))
    {
      const char *name = svn__apr_hash_index_key(hi);
      apr_ssize_t klen = svn__apr_hash_index_klen(hi);
      const char *id;

      /* The name must end with ".txn" to be considered a transaction. */
      if ((apr_size_t) klen <= ext_len
          || (strcmp(name + klen - ext_len, PATH_EXT_TXN)) != 0)
        continue;

      /* Truncate the ".txn" extension and store the ID. */
      id = apr_pstrndup(pool, name, strlen(name) - ext_len);
      APR_ARRAY_PUSH(names, const char *) = id;
    }

  *names_p = names;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__open_txn(svn_fs_txn_t **txn_p,
                    svn_fs_t *fs,
                    const char *name,
                    apr_pool_t *pool)
{
  svn_fs_txn_t *txn;
  svn_node_kind_t kind;
  transaction_t *local_txn;

  /* First check to see if the directory exists. */
  SVN_ERR(svn_io_check_path(path_txn_dir(fs, name, pool), &kind, pool));

  /* Did we find it? */
  if (kind != svn_node_dir)
    return svn_error_createf(SVN_ERR_FS_NO_SUCH_TRANSACTION, NULL,
                             _("No such transaction '%s'"),
                             name);

  txn = apr_pcalloc(pool, sizeof(*txn));

  /* Read in the root node of this transaction. */
  txn->id = apr_pstrdup(pool, name);
  txn->fs = fs;

  SVN_ERR(svn_fs_fs__get_txn(&local_txn, fs, name, pool));

  txn->base_rev = svn_fs_fs__id_rev(local_txn->base_id);

  txn->vtable = &txn_vtable;
  *txn_p = txn;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__txn_proplist(apr_hash_t **table_p,
                        svn_fs_txn_t *txn,
                        apr_pool_t *pool)
{
  apr_hash_t *proplist = apr_hash_make(pool);
  SVN_ERR(get_txn_proplist(proplist, txn->fs, txn->id, pool));
  *table_p = proplist;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__delete_node_revision(svn_fs_t *fs,
                                const svn_fs_id_t *id,
                                apr_pool_t *pool)
{
  node_revision_t *noderev;

  SVN_ERR(svn_fs_fs__get_node_revision(&noderev, fs, id, pool));

  /* Delete any mutable property representation. */
  if (noderev->prop_rep && noderev->prop_rep->txn_id)
    SVN_ERR(svn_io_remove_file2(path_txn_node_props(fs, id, pool), FALSE,
                                pool));

  /* Delete any mutable data representation. */
  if (noderev->data_rep && noderev->data_rep->txn_id
      && noderev->kind == svn_node_dir)
    {
      fs_fs_data_t *ffd = fs->fsap_data;
      SVN_ERR(svn_io_remove_file2(path_txn_node_children(fs, id, pool), FALSE,
                                  pool));

      /* remove the corresponding entry from the cache, if such exists */
      if (ffd->txn_dir_cache)
        {
          const char *key = svn_fs_fs__id_unparse(id, pool)->data;
          SVN_ERR(svn_cache__set(ffd->txn_dir_cache, key, NULL, pool));
        }
    }

  return svn_io_remove_file2(path_txn_node_rev(fs, id, pool), FALSE, pool);
}



/*** Revisions ***/

svn_error_t *
svn_fs_fs__revision_prop(svn_string_t **value_p,
                         svn_fs_t *fs,
                         svn_revnum_t rev,
                         const char *propname,
                         apr_pool_t *pool)
{
  apr_hash_t *table;

  SVN_ERR(svn_fs__check_fs(fs, TRUE));
  SVN_ERR(svn_fs_fs__revision_proplist(&table, fs, rev, pool));

  *value_p = svn_hash_gets(table, propname);

  return SVN_NO_ERROR;
}


/* Baton used for change_rev_prop_body below. */
struct change_rev_prop_baton {
  svn_fs_t *fs;
  svn_revnum_t rev;
  const char *name;
  const svn_string_t *const *old_value_p;
  const svn_string_t *value;
};

/* The work-horse for svn_fs_fs__change_rev_prop, called with the FS
   write lock.  This implements the svn_fs_fs__with_write_lock()
   'body' callback type.  BATON is a 'struct change_rev_prop_baton *'. */
static svn_error_t *
change_rev_prop_body(void *baton, apr_pool_t *pool)
{
  struct change_rev_prop_baton *cb = baton;
  apr_hash_t *table;

  SVN_ERR(svn_fs_fs__revision_proplist(&table, cb->fs, cb->rev, pool));

  if (cb->old_value_p)
    {
      const svn_string_t *wanted_value = *cb->old_value_p;
      const svn_string_t *present_value = svn_hash_gets(table, cb->name);
      if ((!wanted_value != !present_value)
          || (wanted_value && present_value
              && !svn_string_compare(wanted_value, present_value)))
        {
          /* What we expected isn't what we found. */
          return svn_error_createf(SVN_ERR_FS_PROP_BASEVALUE_MISMATCH, NULL,
                                   _("revprop '%s' has unexpected value in "
                                     "filesystem"),
                                   cb->name);
        }
      /* Fall through. */
    }
  svn_hash_sets(table, cb->name, cb->value);

  return set_revision_proplist(cb->fs, cb->rev, table, pool);
}

svn_error_t *
svn_fs_fs__change_rev_prop(svn_fs_t *fs,
                           svn_revnum_t rev,
                           const char *name,
                           const svn_string_t *const *old_value_p,
                           const svn_string_t *value,
                           apr_pool_t *pool)
{
  struct change_rev_prop_baton cb;

  SVN_ERR(svn_fs__check_fs(fs, TRUE));

  cb.fs = fs;
  cb.rev = rev;
  cb.name = name;
  cb.old_value_p = old_value_p;
  cb.value = value;

  return svn_fs_fs__with_write_lock(fs, change_rev_prop_body, &cb, pool);
}



/*** Transactions ***/

svn_error_t *
svn_fs_fs__get_txn_ids(const svn_fs_id_t **root_id_p,
                       const svn_fs_id_t **base_root_id_p,
                       svn_fs_t *fs,
                       const char *txn_name,
                       apr_pool_t *pool)
{
  transaction_t *txn;
  SVN_ERR(svn_fs_fs__get_txn(&txn, fs, txn_name, pool));
  *root_id_p = txn->root_id;
  *base_root_id_p = txn->base_id;
  return SVN_NO_ERROR;
}


/* Generic transaction operations.  */

svn_error_t *
svn_fs_fs__txn_prop(svn_string_t **value_p,
                    svn_fs_txn_t *txn,
                    const char *propname,
                    apr_pool_t *pool)
{
  apr_hash_t *table;
  svn_fs_t *fs = txn->fs;

  SVN_ERR(svn_fs__check_fs(fs, TRUE));
  SVN_ERR(svn_fs_fs__txn_proplist(&table, txn, pool));

  *value_p = svn_hash_gets(table, propname);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__begin_txn(svn_fs_txn_t **txn_p,
                     svn_fs_t *fs,
                     svn_revnum_t rev,
                     apr_uint32_t flags,
                     apr_pool_t *pool)
{
  svn_string_t date;
  svn_prop_t prop;
  apr_array_header_t *props = apr_array_make(pool, 3, sizeof(svn_prop_t));

  SVN_ERR(svn_fs__check_fs(fs, TRUE));

  SVN_ERR(svn_fs_fs__create_txn(txn_p, fs, rev, pool));

  /* Put a datestamp on the newly created txn, so we always know
     exactly how old it is.  (This will help sysadmins identify
     long-abandoned txns that may need to be manually removed.)  When
     a txn is promoted to a revision, this property will be
     automatically overwritten with a revision datestamp. */
  date.data = svn_time_to_cstring(apr_time_now(), pool);
  date.len = strlen(date.data);

  prop.name = SVN_PROP_REVISION_DATE;
  prop.value = &date;
  APR_ARRAY_PUSH(props, svn_prop_t) = prop;

  /* Set temporary txn props that represent the requested 'flags'
     behaviors. */
  if (flags & SVN_FS_TXN_CHECK_OOD)
    {
      prop.name = SVN_FS__PROP_TXN_CHECK_OOD;
      prop.value = svn_string_create("true", pool);
      APR_ARRAY_PUSH(props, svn_prop_t) = prop;
    }

  if (flags & SVN_FS_TXN_CHECK_LOCKS)
    {
      prop.name = SVN_FS__PROP_TXN_CHECK_LOCKS;
      prop.value = svn_string_create("true", pool);
      APR_ARRAY_PUSH(props, svn_prop_t) = prop;
    }

  return svn_fs_fs__change_txn_props(*txn_p, props, pool);
}


/****** Packing FSFS shards *********/

/* Write a file FILENAME in directory FS_PATH, containing a single line
 * with the number REVNUM in ASCII decimal.  Move the file into place
 * atomically, overwriting any existing file.
 *
 * Similar to write_current(). */
static svn_error_t *
write_revnum_file(const char *fs_path,
                  const char *filename,
                  svn_revnum_t revnum,
                  apr_pool_t *scratch_pool)
{
  const char *final_path, *tmp_path;
  svn_stream_t *tmp_stream;

  final_path = svn_dirent_join(fs_path, filename, scratch_pool);
  SVN_ERR(svn_stream_open_unique(&tmp_stream, &tmp_path, fs_path,
                                   svn_io_file_del_none,
                                   scratch_pool, scratch_pool));
  SVN_ERR(svn_stream_printf(tmp_stream, scratch_pool, "%ld\n", revnum));
  SVN_ERR(svn_stream_close(tmp_stream));
  SVN_ERR(move_into_place(tmp_path, final_path, final_path, scratch_pool));
  return SVN_NO_ERROR;
}

/* Pack the revision SHARD containing exactly MAX_FILES_PER_DIR revisions
 * from SHARD_PATH into the PACK_FILE_DIR, using POOL for allocations.
 * CANCEL_FUNC and CANCEL_BATON are what you think they are.
 *
 * If for some reason we detect a partial packing already performed, we
 * remove the pack file and start again.
 */
static svn_error_t *
pack_rev_shard(const char *pack_file_dir,
               const char *shard_path,
               apr_int64_t shard,
               int max_files_per_dir,
               svn_cancel_func_t cancel_func,
               void *cancel_baton,
               apr_pool_t *pool)
{
  const char *pack_file_path, *manifest_file_path;
  svn_stream_t *pack_stream, *manifest_stream;
  svn_revnum_t start_rev, end_rev, rev;
  apr_off_t next_offset;
  apr_pool_t *iterpool;

  /* Some useful paths. */
  pack_file_path = svn_dirent_join(pack_file_dir, PATH_PACKED, pool);
  manifest_file_path = svn_dirent_join(pack_file_dir, PATH_MANIFEST, pool);

  /* Remove any existing pack file for this shard, since it is incomplete. */
  SVN_ERR(svn_io_remove_dir2(pack_file_dir, TRUE, cancel_func, cancel_baton,
                             pool));

  /* Create the new directory and pack and manifest files. */
  SVN_ERR(svn_io_dir_make(pack_file_dir, APR_OS_DEFAULT, pool));
  SVN_ERR(svn_stream_open_writable(&pack_stream, pack_file_path, pool,
                                    pool));
  SVN_ERR(svn_stream_open_writable(&manifest_stream, manifest_file_path,
                                   pool, pool));

  start_rev = (svn_revnum_t) (shard * max_files_per_dir);
  end_rev = (svn_revnum_t) ((shard + 1) * (max_files_per_dir) - 1);
  next_offset = 0;
  iterpool = svn_pool_create(pool);

  /* Iterate over the revisions in this shard, squashing them together. */
  for (rev = start_rev; rev <= end_rev; rev++)
    {
      svn_stream_t *rev_stream;
      apr_finfo_t finfo;
      const char *path;

      svn_pool_clear(iterpool);

      /* Get the size of the file. */
      path = svn_dirent_join(shard_path, apr_psprintf(iterpool, "%ld", rev),
                             iterpool);
      SVN_ERR(svn_io_stat(&finfo, path, APR_FINFO_SIZE, iterpool));

      /* Update the manifest. */
      SVN_ERR(svn_stream_printf(manifest_stream, iterpool, "%" APR_OFF_T_FMT
                                "\n", next_offset));
      next_offset += finfo.size;

      /* Copy all the bits from the rev file to the end of the pack file. */
      SVN_ERR(svn_stream_open_readonly(&rev_stream, path, iterpool, iterpool));
      SVN_ERR(svn_stream_copy3(rev_stream, svn_stream_disown(pack_stream,
                                                             iterpool),
                          cancel_func, cancel_baton, iterpool));
    }

  SVN_ERR(svn_stream_close(manifest_stream));
  SVN_ERR(svn_stream_close(pack_stream));
  SVN_ERR(svn_io_copy_perms(shard_path, pack_file_dir, iterpool));
  SVN_ERR(svn_io_set_file_read_only(pack_file_path, FALSE, iterpool));
  SVN_ERR(svn_io_set_file_read_only(manifest_file_path, FALSE, iterpool));

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

/* Copy revprop files for revisions [START_REV, END_REV) from SHARD_PATH
 * to the pack file at PACK_FILE_NAME in PACK_FILE_DIR.
 *
 * The file sizes have already been determined and written to SIZES.
 * Please note that this function will be executed while the filesystem
 * has been locked and that revprops files will therefore not be modified
 * while the pack is in progress.
 *
 * COMPRESSION_LEVEL defines how well the resulting pack file shall be
 * compressed or whether is shall be compressed at all.  TOTAL_SIZE is
 * a hint on which initial buffer size we should use to hold the pack file
 * content.
 *
 * CANCEL_FUNC and CANCEL_BATON are used as usual. Temporary allocations
 * are done in SCRATCH_POOL.
 */
static svn_error_t *
copy_revprops(const char *pack_file_dir,
              const char *pack_filename,
              const char *shard_path,
              svn_revnum_t start_rev,
              svn_revnum_t end_rev,
              apr_array_header_t *sizes,
              apr_size_t total_size,
              int compression_level,
              svn_cancel_func_t cancel_func,
              void *cancel_baton,
              apr_pool_t *scratch_pool)
{
  svn_stream_t *pack_stream;
  apr_file_t *pack_file;
  svn_revnum_t rev;
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);
  svn_stream_t *stream;

  /* create empty data buffer and a write stream on top of it */
  svn_stringbuf_t *uncompressed
    = svn_stringbuf_create_ensure(total_size, scratch_pool);
  svn_stringbuf_t *compressed
    = svn_stringbuf_create_empty(scratch_pool);
  pack_stream = svn_stream_from_stringbuf(uncompressed, scratch_pool);

  /* write the pack file header */
  SVN_ERR(serialize_revprops_header(pack_stream, start_rev, sizes, 0,
                                    sizes->nelts, iterpool));

  /* Some useful paths. */
  SVN_ERR(svn_io_file_open(&pack_file, svn_dirent_join(pack_file_dir,
                                                       pack_filename,
                                                       scratch_pool),
                           APR_WRITE | APR_CREATE, APR_OS_DEFAULT,
                           scratch_pool));

  /* Iterate over the revisions in this shard, squashing them together. */
  for (rev = start_rev; rev <= end_rev; rev++)
    {
      const char *path;

      svn_pool_clear(iterpool);

      /* Construct the file name. */
      path = svn_dirent_join(shard_path, apr_psprintf(iterpool, "%ld", rev),
                             iterpool);

      /* Copy all the bits from the non-packed revprop file to the end of
       * the pack file. */
      SVN_ERR(svn_stream_open_readonly(&stream, path, iterpool, iterpool));
      SVN_ERR(svn_stream_copy3(stream, pack_stream,
                               cancel_func, cancel_baton, iterpool));
    }

  /* flush stream buffers to content buffer */
  SVN_ERR(svn_stream_close(pack_stream));

  /* compress the content (or just store it for COMPRESSION_LEVEL 0) */
  SVN_ERR(svn__compress(svn_stringbuf__morph_into_string(uncompressed),
                        compressed, compression_level));

  /* write the pack file content to disk */
  stream = svn_stream_from_aprfile2(pack_file, FALSE, scratch_pool);
  SVN_ERR(svn_stream_write(stream, compressed->data, &compressed->len));
  SVN_ERR(svn_stream_close(stream));

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

/* For the revprop SHARD at SHARD_PATH with exactly MAX_FILES_PER_DIR
 * revprop files in it, create a packed shared at PACK_FILE_DIR.
 *
 * COMPRESSION_LEVEL defines how well the resulting pack file shall be
 * compressed or whether is shall be compressed at all.  Individual pack
 * file containing more than one revision will be limited to a size of
 * MAX_PACK_SIZE bytes before compression.
 *
 * CANCEL_FUNC and CANCEL_BATON are used in the usual way.  Temporary
 * allocations are done in SCRATCH_POOL.
 */
static svn_error_t *
pack_revprops_shard(const char *pack_file_dir,
                    const char *shard_path,
                    apr_int64_t shard,
                    int max_files_per_dir,
                    apr_off_t max_pack_size,
                    int compression_level,
                    svn_cancel_func_t cancel_func,
                    void *cancel_baton,
                    apr_pool_t *scratch_pool)
{
  const char *manifest_file_path, *pack_filename = NULL;
  svn_stream_t *manifest_stream;
  svn_revnum_t start_rev, end_rev, rev;
  apr_off_t total_size;
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);
  apr_array_header_t *sizes;

  /* Some useful paths. */
  manifest_file_path = svn_dirent_join(pack_file_dir, PATH_MANIFEST,
                                       scratch_pool);

  /* Remove any existing pack file for this shard, since it is incomplete. */
  SVN_ERR(svn_io_remove_dir2(pack_file_dir, TRUE, cancel_func, cancel_baton,
                             scratch_pool));

  /* Create the new directory and manifest file stream. */
  SVN_ERR(svn_io_dir_make(pack_file_dir, APR_OS_DEFAULT, scratch_pool));
  SVN_ERR(svn_stream_open_writable(&manifest_stream, manifest_file_path,
                                   scratch_pool, scratch_pool));

  /* revisions to handle. Special case: revision 0 */
  start_rev = (svn_revnum_t) (shard * max_files_per_dir);
  end_rev = (svn_revnum_t) ((shard + 1) * (max_files_per_dir) - 1);
  if (start_rev == 0)
    ++start_rev;

  /* initialize the revprop size info */
  sizes = apr_array_make(scratch_pool, max_files_per_dir, sizeof(apr_off_t));
  total_size = 2 * SVN_INT64_BUFFER_SIZE;

  /* Iterate over the revisions in this shard, determine their size and
   * squashing them together into pack files. */
  for (rev = start_rev; rev <= end_rev; rev++)
    {
      apr_finfo_t finfo;
      const char *path;

      svn_pool_clear(iterpool);

      /* Get the size of the file. */
      path = svn_dirent_join(shard_path, apr_psprintf(iterpool, "%ld", rev),
                             iterpool);
      SVN_ERR(svn_io_stat(&finfo, path, APR_FINFO_SIZE, iterpool));

      /* if we already have started a pack file and this revprop cannot be
       * appended to it, write the previous pack file. */
      if (sizes->nelts != 0 &&
          total_size + SVN_INT64_BUFFER_SIZE + finfo.size > max_pack_size)
        {
          SVN_ERR(copy_revprops(pack_file_dir, pack_filename, shard_path,
                                start_rev, rev-1, sizes, (apr_size_t)total_size,
                                compression_level, cancel_func, cancel_baton,
                                iterpool));

          /* next pack file starts empty again */
          apr_array_clear(sizes);
          total_size = 2 * SVN_INT64_BUFFER_SIZE;
          start_rev = rev;
        }

      /* Update the manifest. Allocate a file name for the current pack
       * file if it is a new one */
      if (sizes->nelts == 0)
        pack_filename = apr_psprintf(scratch_pool, "%ld.0", rev);

      SVN_ERR(svn_stream_printf(manifest_stream, iterpool, "%s\n",
                                pack_filename));

      /* add to list of files to put into the current pack file */
      APR_ARRAY_PUSH(sizes, apr_off_t) = finfo.size;
      total_size += SVN_INT64_BUFFER_SIZE + finfo.size;
    }

  /* write the last pack file */
  if (sizes->nelts != 0)
    SVN_ERR(copy_revprops(pack_file_dir, pack_filename, shard_path,
                          start_rev, rev-1, sizes, (apr_size_t)total_size,
                          compression_level, cancel_func, cancel_baton,
                          iterpool));

  /* flush the manifest file and update permissions */
  SVN_ERR(svn_stream_close(manifest_stream));
  SVN_ERR(svn_io_copy_perms(shard_path, pack_file_dir, iterpool));

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

/* Delete the non-packed revprop SHARD at SHARD_PATH with exactly
 * MAX_FILES_PER_DIR revprop files in it.  If this is shard 0, keep the
 * revprop file for revision 0.
 *
 * CANCEL_FUNC and CANCEL_BATON are used in the usual way.  Temporary
 * allocations are done in SCRATCH_POOL.
 */
static svn_error_t *
delete_revprops_shard(const char *shard_path,
                      apr_int64_t shard,
                      int max_files_per_dir,
                      svn_cancel_func_t cancel_func,
                      void *cancel_baton,
                      apr_pool_t *scratch_pool)
{
  if (shard == 0)
    {
      apr_pool_t *iterpool = svn_pool_create(scratch_pool);
      int i;

      /* delete all files except the one for revision 0 */
      for (i = 1; i < max_files_per_dir; ++i)
        {
          const char *path = svn_dirent_join(shard_path,
                                       apr_psprintf(iterpool, "%d", i),
                                       iterpool);
          if (cancel_func)
            SVN_ERR((*cancel_func)(cancel_baton));

          SVN_ERR(svn_io_remove_file2(path, TRUE, iterpool));
          svn_pool_clear(iterpool);
        }

      svn_pool_destroy(iterpool);
    }
  else
    SVN_ERR(svn_io_remove_dir2(shard_path, TRUE,
                               cancel_func, cancel_baton, scratch_pool));

  return SVN_NO_ERROR;
}

/* In the file system at FS_PATH, pack the SHARD in REVS_DIR and
 * REVPROPS_DIR containing exactly MAX_FILES_PER_DIR revisions, using POOL
 * for allocations.  REVPROPS_DIR will be NULL if revprop packing is not
 * supported.  COMPRESSION_LEVEL and MAX_PACK_SIZE will be ignored in that
 * case.
 *
 * CANCEL_FUNC and CANCEL_BATON are what you think they are; similarly
 * NOTIFY_FUNC and NOTIFY_BATON.
 *
 * If for some reason we detect a partial packing already performed, we
 * remove the pack file and start again.
 */
static svn_error_t *
pack_shard(const char *revs_dir,
           const char *revsprops_dir,
           const char *fs_path,
           apr_int64_t shard,
           int max_files_per_dir,
           apr_off_t max_pack_size,
           int compression_level,
           svn_fs_pack_notify_t notify_func,
           void *notify_baton,
           svn_cancel_func_t cancel_func,
           void *cancel_baton,
           apr_pool_t *pool)
{
  const char *rev_shard_path, *rev_pack_file_dir;
  const char *revprops_shard_path, *revprops_pack_file_dir;

  /* Notify caller we're starting to pack this shard. */
  if (notify_func)
    SVN_ERR(notify_func(notify_baton, shard, svn_fs_pack_notify_start,
                        pool));

  /* Some useful paths. */
  rev_pack_file_dir = svn_dirent_join(revs_dir,
                  apr_psprintf(pool,
                               "%" APR_INT64_T_FMT PATH_EXT_PACKED_SHARD,
                               shard),
                  pool);
  rev_shard_path = svn_dirent_join(revs_dir,
                           apr_psprintf(pool, "%" APR_INT64_T_FMT, shard),
                           pool);

  /* pack the revision content */
  SVN_ERR(pack_rev_shard(rev_pack_file_dir, rev_shard_path,
                         shard, max_files_per_dir,
                         cancel_func, cancel_baton, pool));

  /* if enabled, pack the revprops in an equivalent way */
  if (revsprops_dir)
    {
      revprops_pack_file_dir = svn_dirent_join(revsprops_dir,
                   apr_psprintf(pool,
                                "%" APR_INT64_T_FMT PATH_EXT_PACKED_SHARD,
                                shard),
                   pool);
      revprops_shard_path = svn_dirent_join(revsprops_dir,
                           apr_psprintf(pool, "%" APR_INT64_T_FMT, shard),
                           pool);

      SVN_ERR(pack_revprops_shard(revprops_pack_file_dir, revprops_shard_path,
                                  shard, max_files_per_dir,
                                  (int)(0.9 * max_pack_size),
                                  compression_level,
                                  cancel_func, cancel_baton, pool));
    }

  /* Update the min-unpacked-rev file to reflect our newly packed shard.
   * (This doesn't update ffd->min_unpacked_rev.  That will be updated by
   * update_min_unpacked_rev() when necessary.) */
  SVN_ERR(write_revnum_file(fs_path, PATH_MIN_UNPACKED_REV,
                            (svn_revnum_t)((shard + 1) * max_files_per_dir),
                            pool));

  /* Finally, remove the existing shard directories. */
  SVN_ERR(svn_io_remove_dir2(rev_shard_path, TRUE,
                             cancel_func, cancel_baton, pool));
  if (revsprops_dir)
    SVN_ERR(delete_revprops_shard(revprops_shard_path,
                                  shard, max_files_per_dir,
                                  cancel_func, cancel_baton, pool));

  /* Notify caller we're starting to pack this shard. */
  if (notify_func)
    SVN_ERR(notify_func(notify_baton, shard, svn_fs_pack_notify_end,
                        pool));

  return SVN_NO_ERROR;
}

struct pack_baton
{
  svn_fs_t *fs;
  svn_fs_pack_notify_t notify_func;
  void *notify_baton;
  svn_cancel_func_t cancel_func;
  void *cancel_baton;
};


/* The work-horse for svn_fs_fs__pack, called with the FS write lock.
   This implements the svn_fs_fs__with_write_lock() 'body' callback
   type.  BATON is a 'struct pack_baton *'.

   WARNING: if you add a call to this function, please note:
     The code currently assumes that any piece of code running with
     the write-lock set can rely on the ffd->min_unpacked_rev and
     ffd->min_unpacked_revprop caches to be up-to-date (and, by
     extension, on not having to use a retry when calling
     svn_fs_fs__path_rev_absolute() and friends).  If you add a call
     to this function, consider whether you have to call
     update_min_unpacked_rev().
     See this thread: http://thread.gmane.org/1291206765.3782.3309.camel@edith
 */
static svn_error_t *
pack_body(void *baton,
          apr_pool_t *pool)
{
  struct pack_baton *pb = baton;
  fs_fs_data_t ffd = {0};
  apr_int64_t completed_shards;
  apr_int64_t i;
  svn_revnum_t youngest;
  apr_pool_t *iterpool;
  const char *rev_data_path;
  const char *revprops_data_path = NULL;

  /* read repository settings */
  SVN_ERR(read_format(&ffd.format, &ffd.max_files_per_dir,
                      path_format(pb->fs, pool), pool));
  SVN_ERR(check_format(ffd.format));
  SVN_ERR(read_config(&ffd, pb->fs->path, pool));

  /* If the repository isn't a new enough format, we don't support packing.
     Return a friendly error to that effect. */
  if (ffd.format < SVN_FS_FS__MIN_PACKED_FORMAT)
    return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
      _("FSFS format (%d) too old to pack; please upgrade the filesystem."),
      ffd.format);

  /* If we aren't using sharding, we can't do any packing, so quit. */
  if (!ffd.max_files_per_dir)
    return SVN_NO_ERROR;

  SVN_ERR(read_min_unpacked_rev(&ffd.min_unpacked_rev,
                                path_min_unpacked_rev(pb->fs, pool),
                                pool));

  SVN_ERR(get_youngest(&youngest, pb->fs->path, pool));
  completed_shards = (youngest + 1) / ffd.max_files_per_dir;

  /* See if we've already completed all possible shards thus far. */
  if (ffd.min_unpacked_rev == (completed_shards * ffd.max_files_per_dir))
    return SVN_NO_ERROR;

  rev_data_path = svn_dirent_join(pb->fs->path, PATH_REVS_DIR, pool);
  if (ffd.format >= SVN_FS_FS__MIN_PACKED_REVPROP_FORMAT)
    revprops_data_path = svn_dirent_join(pb->fs->path, PATH_REVPROPS_DIR,
                                         pool);

  iterpool = svn_pool_create(pool);
  for (i = ffd.min_unpacked_rev / ffd.max_files_per_dir;
       i < completed_shards;
       i++)
    {
      svn_pool_clear(iterpool);

      if (pb->cancel_func)
        SVN_ERR(pb->cancel_func(pb->cancel_baton));

      SVN_ERR(pack_shard(rev_data_path, revprops_data_path,
                         pb->fs->path, i, ffd.max_files_per_dir,
                         ffd.revprop_pack_size,
                         ffd.compress_packed_revprops
                           ? SVN_DELTA_COMPRESSION_LEVEL_DEFAULT
                           : SVN_DELTA_COMPRESSION_LEVEL_NONE,
                         pb->notify_func, pb->notify_baton,
                         pb->cancel_func, pb->cancel_baton, iterpool));
    }

  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__pack(svn_fs_t *fs,
                svn_fs_pack_notify_t notify_func,
                void *notify_baton,
                svn_cancel_func_t cancel_func,
                void *cancel_baton,
                apr_pool_t *pool)
{
  struct pack_baton pb = { 0 };
  pb.fs = fs;
  pb.notify_func = notify_func;
  pb.notify_baton = notify_baton;
  pb.cancel_func = cancel_func;
  pb.cancel_baton = cancel_baton;
  return svn_fs_fs__with_write_lock(fs, pack_body, &pb, pool);
}


/** Verifying. **/

/* Baton type expected by verify_walker().  The purpose is to reuse open
 * rev / pack file handles between calls.  Its contents need to be cleaned
 * periodically to limit resource usage.
 */
typedef struct verify_walker_baton_t
{
  /* number of calls to verify_walker() since the last clean */
  int iteration_count;

  /* number of files opened since the last clean */
  int file_count;

  /* progress notification callback to invoke periodically (may be NULL) */
  svn_fs_progress_notify_func_t notify_func;

  /* baton to use with NOTIFY_FUNC */
  void *notify_baton;

  /* remember the last revision for which we called notify_func */
  svn_revnum_t last_notified_revision;

  /* current file handle (or NULL) */
  apr_file_t *file_hint;

  /* corresponding revision (or SVN_INVALID_REVNUM) */
  svn_revnum_t rev_hint;

  /* pool to use for the file handles etc. */
  apr_pool_t *pool;
} verify_walker_baton_t;

/* Used by svn_fs_fs__verify().
   Implements svn_fs_fs__walk_rep_reference().walker.  */
static svn_error_t *
verify_walker(representation_t *rep,
              void *baton,
              svn_fs_t *fs,
              apr_pool_t *scratch_pool)
{
  struct rep_state *rs;
  struct rep_args *rep_args;

  if (baton)
    {
      verify_walker_baton_t *walker_baton = baton;
      apr_file_t * previous_file;

      /* notify and free resources periodically */
      if (   walker_baton->iteration_count > 1000
          || walker_baton->file_count > 16)
        {
          if (   walker_baton->notify_func
              && rep->revision != walker_baton->last_notified_revision)
            {
              walker_baton->notify_func(rep->revision,
                                        walker_baton->notify_baton,
                                        scratch_pool);
              walker_baton->last_notified_revision = rep->revision;
            }

          svn_pool_clear(walker_baton->pool);

          walker_baton->iteration_count = 0;
          walker_baton->file_count = 0;
          walker_baton->file_hint = NULL;
          walker_baton->rev_hint = SVN_INVALID_REVNUM;
        }

      /* access the repo data */
      previous_file = walker_baton->file_hint;
      SVN_ERR(create_rep_state(&rs, &rep_args, &walker_baton->file_hint,
                               &walker_baton->rev_hint, rep, fs,
                               walker_baton->pool));

      /* update resource usage counters */
      walker_baton->iteration_count++;
      if (previous_file != walker_baton->file_hint)
        walker_baton->file_count++;
    }
  else
    {
      /* ### Should this be using read_rep_line() directly? */
      SVN_ERR(create_rep_state(&rs, &rep_args, NULL, NULL, rep, fs,
                               scratch_pool));
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__verify(svn_fs_t *fs,
                  svn_revnum_t start,
                  svn_revnum_t end,
                  svn_fs_progress_notify_func_t notify_func,
                  void *notify_baton,
                  svn_cancel_func_t cancel_func,
                  void *cancel_baton,
                  apr_pool_t *pool)
{
  fs_fs_data_t *ffd = fs->fsap_data;
  svn_boolean_t exists;
  svn_revnum_t youngest = ffd->youngest_rev_cache; /* cache is current */

  if (ffd->format < SVN_FS_FS__MIN_REP_SHARING_FORMAT)
    return SVN_NO_ERROR;

  /* Input validation. */
  if (! SVN_IS_VALID_REVNUM(start))
    start = 0;
  if (! SVN_IS_VALID_REVNUM(end))
    end = youngest;
  SVN_ERR(ensure_revision_exists(fs, start, pool));
  SVN_ERR(ensure_revision_exists(fs, end, pool));

  /* rep-cache verification. */
  SVN_ERR(svn_fs_fs__exists_rep_cache(&exists, fs, pool));
  if (exists)
    {
      /* provide a baton to allow the reuse of open file handles between
         iterations (saves 2/3 of OS level file operations). */
      verify_walker_baton_t *baton = apr_pcalloc(pool, sizeof(*baton));
      baton->rev_hint = SVN_INVALID_REVNUM;
      baton->pool = svn_pool_create(pool);
      baton->last_notified_revision = SVN_INVALID_REVNUM;
      baton->notify_func = notify_func;
      baton->notify_baton = notify_baton;

      /* tell the user that we are now ready to do *something* */
      if (notify_func)
        notify_func(SVN_INVALID_REVNUM, notify_baton, baton->pool);

      /* Do not attempt to walk the rep-cache database if its file does
         not exist,  since doing so would create it --- which may confuse
         the administrator.   Don't take any lock. */
      SVN_ERR(svn_fs_fs__walk_rep_reference(fs, start, end,
                                            verify_walker, baton,
                                            cancel_func, cancel_baton,
                                            pool));

      /* walker resource cleanup */
      svn_pool_destroy(baton->pool);
    }

  return SVN_NO_ERROR;
}


/** Hotcopy. **/

/* Like svn_io_dir_file_copy(), but doesn't copy files that exist at
 * the destination and do not differ in terms of kind, size, and mtime. */
static svn_error_t *
hotcopy_io_dir_file_copy(const char *src_path,
                         const char *dst_path,
                         const char *file,
                         apr_pool_t *scratch_pool)
{
  const svn_io_dirent2_t *src_dirent;
  const svn_io_dirent2_t *dst_dirent;
  const char *src_target;
  const char *dst_target;

  /* Does the destination already exist? If not, we must copy it. */
  dst_target = svn_dirent_join(dst_path, file, scratch_pool);
  SVN_ERR(svn_io_stat_dirent2(&dst_dirent, dst_target, FALSE, TRUE,
                              scratch_pool, scratch_pool));
  if (dst_dirent->kind != svn_node_none)
    {
      /* If the destination's stat information indicates that the file
       * is equal to the source, don't bother copying the file again. */
      src_target = svn_dirent_join(src_path, file, scratch_pool);
      SVN_ERR(svn_io_stat_dirent2(&src_dirent, src_target, FALSE, FALSE,
                                  scratch_pool, scratch_pool));
      if (src_dirent->kind == dst_dirent->kind &&
          src_dirent->special == dst_dirent->special &&
          src_dirent->filesize == dst_dirent->filesize &&
          src_dirent->mtime <= dst_dirent->mtime)
        return SVN_NO_ERROR;
    }

  return svn_error_trace(svn_io_dir_file_copy(src_path, dst_path, file,
                                              scratch_pool));
}

/* Set *NAME_P to the UTF-8 representation of directory entry NAME.
 * NAME is in the internal encoding used by APR; PARENT is in
 * UTF-8 and in internal (not local) style.
 *
 * Use PARENT only for generating an error string if the conversion
 * fails because NAME could not be represented in UTF-8.  In that
 * case, return a two-level error in which the outer error's message
 * mentions PARENT, but the inner error's message does not mention
 * NAME (except possibly in hex) since NAME may not be printable.
 * Such a compound error at least allows the user to go looking in the
 * right directory for the problem.
 *
 * If there is any other error, just return that error directly.
 *
 * If there is any error, the effect on *NAME_P is undefined.
 *
 * *NAME_P and NAME may refer to the same storage.
 */
static svn_error_t *
entry_name_to_utf8(const char **name_p,
                   const char *name,
                   const char *parent,
                   apr_pool_t *pool)
{
  svn_error_t *err = svn_path_cstring_to_utf8(name_p, name, pool);
  if (err && err->apr_err == APR_EINVAL)
    {
      return svn_error_createf(err->apr_err, err,
                               _("Error converting entry "
                                 "in directory '%s' to UTF-8"),
                               svn_dirent_local_style(parent, pool));
    }
  return err;
}

/* Like svn_io_copy_dir_recursively() but doesn't copy regular files that
 * exist in the destination and do not differ from the source in terms of
 * kind, size, and mtime. */
static svn_error_t *
hotcopy_io_copy_dir_recursively(const char *src,
                                const char *dst_parent,
                                const char *dst_basename,
                                svn_boolean_t copy_perms,
                                svn_cancel_func_t cancel_func,
                                void *cancel_baton,
                                apr_pool_t *pool)
{
  svn_node_kind_t kind;
  apr_status_t status;
  const char *dst_path;
  apr_dir_t *this_dir;
  apr_finfo_t this_entry;
  apr_int32_t flags = APR_FINFO_TYPE | APR_FINFO_NAME;

  /* Make a subpool for recursion */
  apr_pool_t *subpool = svn_pool_create(pool);

  /* The 'dst_path' is simply dst_parent/dst_basename */
  dst_path = svn_dirent_join(dst_parent, dst_basename, pool);

  /* Sanity checks:  SRC and DST_PARENT are directories, and
     DST_BASENAME doesn't already exist in DST_PARENT. */
  SVN_ERR(svn_io_check_path(src, &kind, subpool));
  if (kind != svn_node_dir)
    return svn_error_createf(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
                             _("Source '%s' is not a directory"),
                             svn_dirent_local_style(src, pool));

  SVN_ERR(svn_io_check_path(dst_parent, &kind, subpool));
  if (kind != svn_node_dir)
    return svn_error_createf(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
                             _("Destination '%s' is not a directory"),
                             svn_dirent_local_style(dst_parent, pool));

  SVN_ERR(svn_io_check_path(dst_path, &kind, subpool));

  /* Create the new directory. */
  /* ### TODO: copy permissions (needs apr_file_attrs_get()) */
  SVN_ERR(svn_io_make_dir_recursively(dst_path, pool));

  /* Loop over the dirents in SRC.  ('.' and '..' are auto-excluded) */
  SVN_ERR(svn_io_dir_open(&this_dir, src, subpool));

  for (status = apr_dir_read(&this_entry, flags, this_dir);
       status == APR_SUCCESS;
       status = apr_dir_read(&this_entry, flags, this_dir))
    {
      if ((this_entry.name[0] == '.')
          && ((this_entry.name[1] == '\0')
              || ((this_entry.name[1] == '.')
                  && (this_entry.name[2] == '\0'))))
        {
          continue;
        }
      else
        {
          const char *entryname_utf8;

          if (cancel_func)
            SVN_ERR(cancel_func(cancel_baton));

          SVN_ERR(entry_name_to_utf8(&entryname_utf8, this_entry.name,
                                     src, subpool));
          if (this_entry.filetype == APR_REG) /* regular file */
            {
              SVN_ERR(hotcopy_io_dir_file_copy(src, dst_path, entryname_utf8,
                                               subpool));
            }
          else if (this_entry.filetype == APR_LNK) /* symlink */
            {
              const char *src_target = svn_dirent_join(src, entryname_utf8,
                                                       subpool);
              const char *dst_target = svn_dirent_join(dst_path,
                                                       entryname_utf8,
                                                       subpool);
              SVN_ERR(svn_io_copy_link(src_target, dst_target,
                                       subpool));
            }
          else if (this_entry.filetype == APR_DIR) /* recurse */
            {
              const char *src_target;

              /* Prevent infinite recursion by filtering off our
                 newly created destination path. */
              if (strcmp(src, dst_parent) == 0
                  && strcmp(entryname_utf8, dst_basename) == 0)
                continue;

              src_target = svn_dirent_join(src, entryname_utf8, subpool);
              SVN_ERR(hotcopy_io_copy_dir_recursively(src_target,
                                                      dst_path,
                                                      entryname_utf8,
                                                      copy_perms,
                                                      cancel_func,
                                                      cancel_baton,
                                                      subpool));
            }
          /* ### support other APR node types someday?? */

        }
    }

  if (! (APR_STATUS_IS_ENOENT(status)))
    return svn_error_wrap_apr(status, _("Can't read directory '%s'"),
                              svn_dirent_local_style(src, pool));

  status = apr_dir_close(this_dir);
  if (status)
    return svn_error_wrap_apr(status, _("Error closing directory '%s'"),
                              svn_dirent_local_style(src, pool));

  /* Free any memory used by recursion */
  svn_pool_destroy(subpool);

  return SVN_NO_ERROR;
}

/* Copy an un-packed revision or revprop file for revision REV from SRC_SUBDIR
 * to DST_SUBDIR. Assume a sharding layout based on MAX_FILES_PER_DIR.
 * Use SCRATCH_POOL for temporary allocations. */
static svn_error_t *
hotcopy_copy_shard_file(const char *src_subdir,
                        const char *dst_subdir,
                        svn_revnum_t rev,
                        int max_files_per_dir,
                        apr_pool_t *scratch_pool)
{
  const char *src_subdir_shard = src_subdir,
             *dst_subdir_shard = dst_subdir;

  if (max_files_per_dir)
    {
      const char *shard = apr_psprintf(scratch_pool, "%ld",
                                       rev / max_files_per_dir);
      src_subdir_shard = svn_dirent_join(src_subdir, shard, scratch_pool);
      dst_subdir_shard = svn_dirent_join(dst_subdir, shard, scratch_pool);

      if (rev % max_files_per_dir == 0)
        {
          SVN_ERR(svn_io_make_dir_recursively(dst_subdir_shard, scratch_pool));
          SVN_ERR(svn_io_copy_perms(dst_subdir, dst_subdir_shard,
                                    scratch_pool));
        }
    }

  SVN_ERR(hotcopy_io_dir_file_copy(src_subdir_shard, dst_subdir_shard,
                                   apr_psprintf(scratch_pool, "%ld", rev),
                                   scratch_pool));
  return SVN_NO_ERROR;
}


/* Copy a packed shard containing revision REV, and which contains
 * MAX_FILES_PER_DIR revisions, from SRC_FS to DST_FS.
 * Update *DST_MIN_UNPACKED_REV in case the shard is new in DST_FS.
 * Do not re-copy data which already exists in DST_FS.
 * Use SCRATCH_POOL for temporary allocations. */
static svn_error_t *
hotcopy_copy_packed_shard(svn_revnum_t *dst_min_unpacked_rev,
                          svn_fs_t *src_fs,
                          svn_fs_t *dst_fs,
                          svn_revnum_t rev,
                          int max_files_per_dir,
                          apr_pool_t *scratch_pool)
{
  const char *src_subdir;
  const char *dst_subdir;
  const char *packed_shard;
  const char *src_subdir_packed_shard;
  svn_revnum_t revprop_rev;
  apr_pool_t *iterpool;
  fs_fs_data_t *src_ffd = src_fs->fsap_data;

  /* Copy the packed shard. */
  src_subdir = svn_dirent_join(src_fs->path, PATH_REVS_DIR, scratch_pool);
  dst_subdir = svn_dirent_join(dst_fs->path, PATH_REVS_DIR, scratch_pool);
  packed_shard = apr_psprintf(scratch_pool, "%ld" PATH_EXT_PACKED_SHARD,
                              rev / max_files_per_dir);
  src_subdir_packed_shard = svn_dirent_join(src_subdir, packed_shard,
                                            scratch_pool);
  SVN_ERR(hotcopy_io_copy_dir_recursively(src_subdir_packed_shard,
                                          dst_subdir, packed_shard,
                                          TRUE /* copy_perms */,
                                          NULL /* cancel_func */, NULL,
                                          scratch_pool));

  /* Copy revprops belonging to revisions in this pack. */
  src_subdir = svn_dirent_join(src_fs->path, PATH_REVPROPS_DIR, scratch_pool);
  dst_subdir = svn_dirent_join(dst_fs->path, PATH_REVPROPS_DIR, scratch_pool);

  if (   src_ffd->format < SVN_FS_FS__MIN_PACKED_REVPROP_FORMAT
      || src_ffd->min_unpacked_rev < rev + max_files_per_dir)
    {
      /* copy unpacked revprops rev by rev */
      iterpool = svn_pool_create(scratch_pool);
      for (revprop_rev = rev;
           revprop_rev < rev + max_files_per_dir;
           revprop_rev++)
        {
          svn_pool_clear(iterpool);

          SVN_ERR(hotcopy_copy_shard_file(src_subdir, dst_subdir,
                                          revprop_rev, max_files_per_dir,
                                          iterpool));
        }
      svn_pool_destroy(iterpool);
    }
  else
    {
      /* revprop for revision 0 will never be packed */
      if (rev == 0)
        SVN_ERR(hotcopy_copy_shard_file(src_subdir, dst_subdir,
                                        0, max_files_per_dir,
                                        scratch_pool));

      /* packed revprops folder */
      packed_shard = apr_psprintf(scratch_pool, "%ld" PATH_EXT_PACKED_SHARD,
                                  rev / max_files_per_dir);
      src_subdir_packed_shard = svn_dirent_join(src_subdir, packed_shard,
                                                scratch_pool);
      SVN_ERR(hotcopy_io_copy_dir_recursively(src_subdir_packed_shard,
                                              dst_subdir, packed_shard,
                                              TRUE /* copy_perms */,
                                              NULL /* cancel_func */, NULL,
                                              scratch_pool));
    }

  /* If necessary, update the min-unpacked rev file in the hotcopy. */
  if (*dst_min_unpacked_rev < rev + max_files_per_dir)
    {
      *dst_min_unpacked_rev = rev + max_files_per_dir;
      SVN_ERR(write_revnum_file(dst_fs->path, PATH_MIN_UNPACKED_REV,
                                *dst_min_unpacked_rev,
                                scratch_pool));
    }

  return SVN_NO_ERROR;
}

/* If NEW_YOUNGEST is younger than *DST_YOUNGEST, update the 'current'
 * file in DST_FS and set *DST_YOUNGEST to NEW_YOUNGEST.
 * Use SCRATCH_POOL for temporary allocations. */
static svn_error_t *
hotcopy_update_current(svn_revnum_t *dst_youngest,
                       svn_fs_t *dst_fs,
                       svn_revnum_t new_youngest,
                       apr_pool_t *scratch_pool)
{
  char next_node_id[MAX_KEY_SIZE] = "0";
  char next_copy_id[MAX_KEY_SIZE] = "0";
  fs_fs_data_t *dst_ffd = dst_fs->fsap_data;

  if (*dst_youngest >= new_youngest)
    return SVN_NO_ERROR;

  /* If necessary, get new current next_node and next_copy IDs. */
  if (dst_ffd->format < SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT)
    {
      apr_off_t root_offset;
      apr_file_t *rev_file;
      char max_node_id[MAX_KEY_SIZE] = "0";
      char max_copy_id[MAX_KEY_SIZE] = "0";
      apr_size_t len;

      if (dst_ffd->format >= SVN_FS_FS__MIN_PACKED_FORMAT)
        SVN_ERR(update_min_unpacked_rev(dst_fs, scratch_pool));

      SVN_ERR(open_pack_or_rev_file(&rev_file, dst_fs, new_youngest,
                                    scratch_pool));
      SVN_ERR(get_root_changes_offset(&root_offset, NULL, rev_file,
                                      dst_fs, new_youngest, scratch_pool));
      SVN_ERR(recover_find_max_ids(dst_fs, new_youngest, rev_file,
                                   root_offset, max_node_id, max_copy_id,
                                   scratch_pool));
      SVN_ERR(svn_io_file_close(rev_file, scratch_pool));

      /* We store the _next_ ids. */
      len = strlen(max_node_id);
      svn_fs_fs__next_key(max_node_id, &len, next_node_id);
      len = strlen(max_copy_id);
      svn_fs_fs__next_key(max_copy_id, &len, next_copy_id);
    }

  /* Update 'current'. */
  SVN_ERR(write_current(dst_fs, new_youngest, next_node_id, next_copy_id,
                        scratch_pool));

  *dst_youngest = new_youngest;

  return SVN_NO_ERROR;
}


/* Remove revision or revprop files between START_REV (inclusive) and
 * END_REV (non-inclusive) from folder DST_SUBDIR in DST_FS.  Assume
 * sharding as per MAX_FILES_PER_DIR.
 * Use SCRATCH_POOL for temporary allocations. */
static svn_error_t *
hotcopy_remove_files(svn_fs_t *dst_fs,
                     const char *dst_subdir,
                     svn_revnum_t start_rev,
                     svn_revnum_t end_rev,
                     int max_files_per_dir,
                     apr_pool_t *scratch_pool)
{
  const char *shard;
  const char *dst_subdir_shard;
  svn_revnum_t rev;
  apr_pool_t *iterpool;

  /* Pre-compute paths for initial shard. */
  shard = apr_psprintf(scratch_pool, "%ld", start_rev / max_files_per_dir);
  dst_subdir_shard = svn_dirent_join(dst_subdir, shard, scratch_pool);

  iterpool = svn_pool_create(scratch_pool);
  for (rev = start_rev; rev < end_rev; rev++)
    {
      const char *path;
      svn_pool_clear(iterpool);

      /* If necessary, update paths for shard. */
      if (rev != start_rev && rev % max_files_per_dir == 0)
        {
          shard = apr_psprintf(iterpool, "%ld", rev / max_files_per_dir);
          dst_subdir_shard = svn_dirent_join(dst_subdir, shard, scratch_pool);
        }

      /* remove files for REV */
      path = svn_dirent_join(dst_subdir_shard,
                             apr_psprintf(iterpool, "%ld", rev),
                             iterpool);

      /* Make the rev file writable and remove it. */
      SVN_ERR(svn_io_set_file_read_write(path, TRUE, iterpool));
      SVN_ERR(svn_io_remove_file2(path, TRUE, iterpool));
    }

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

/* Remove revisions between START_REV (inclusive) and END_REV (non-inclusive)
 * from DST_FS. Assume sharding as per MAX_FILES_PER_DIR.
 * Use SCRATCH_POOL for temporary allocations. */
static svn_error_t *
hotcopy_remove_rev_files(svn_fs_t *dst_fs,
                         svn_revnum_t start_rev,
                         svn_revnum_t end_rev,
                         int max_files_per_dir,
                         apr_pool_t *scratch_pool)
{
  SVN_ERR_ASSERT(start_rev <= end_rev);
  SVN_ERR(hotcopy_remove_files(dst_fs,
                               svn_dirent_join(dst_fs->path,
                                               PATH_REVS_DIR,
                                               scratch_pool),
                               start_rev, end_rev,
                               max_files_per_dir, scratch_pool));

  return SVN_NO_ERROR;
}

/* Remove revision properties between START_REV (inclusive) and END_REV
 * (non-inclusive) from DST_FS. Assume sharding as per MAX_FILES_PER_DIR.
 * Use SCRATCH_POOL for temporary allocations.  Revision 0 revprops will
 * not be deleted. */
static svn_error_t *
hotcopy_remove_revprop_files(svn_fs_t *dst_fs,
                             svn_revnum_t start_rev,
                             svn_revnum_t end_rev,
                             int max_files_per_dir,
                             apr_pool_t *scratch_pool)
{
  SVN_ERR_ASSERT(start_rev <= end_rev);

  /* don't delete rev 0 props */
  SVN_ERR(hotcopy_remove_files(dst_fs,
                               svn_dirent_join(dst_fs->path,
                                               PATH_REVPROPS_DIR,
                                               scratch_pool),
                               start_rev ? start_rev : 1, end_rev,
                               max_files_per_dir, scratch_pool));

  return SVN_NO_ERROR;
}

/* Verify that DST_FS is a suitable destination for an incremental
 * hotcopy from SRC_FS. */
static svn_error_t *
hotcopy_incremental_check_preconditions(svn_fs_t *src_fs,
                                        svn_fs_t *dst_fs,
                                        apr_pool_t *pool)
{
  fs_fs_data_t *src_ffd = src_fs->fsap_data;
  fs_fs_data_t *dst_ffd = dst_fs->fsap_data;

  /* We only support incremental hotcopy between the same format. */
  if (src_ffd->format != dst_ffd->format)
    return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
      _("The FSFS format (%d) of the hotcopy source does not match the "
        "FSFS format (%d) of the hotcopy destination; please upgrade "
        "both repositories to the same format"),
      src_ffd->format, dst_ffd->format);

  /* Make sure the UUID of source and destination match up.
   * We don't want to copy over a different repository. */
  if (strcmp(src_fs->uuid, dst_fs->uuid) != 0)
    return svn_error_create(SVN_ERR_RA_UUID_MISMATCH, NULL,
                            _("The UUID of the hotcopy source does "
                              "not match the UUID of the hotcopy "
                              "destination"));

  /* Also require same shard size. */
  if (src_ffd->max_files_per_dir != dst_ffd->max_files_per_dir)
    return svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                            _("The sharding layout configuration "
                              "of the hotcopy source does not match "
                              "the sharding layout configuration of "
                              "the hotcopy destination"));
  return SVN_NO_ERROR;
}

/* Remove folder PATH.  Ignore errors due to the sub-tree not being empty.
 * CANCEL_FUNC and CANCEL_BATON do the usual thing.
 * Use POOL for temporary allocations.
 */
static svn_error_t *
remove_folder(const char *path,
              svn_cancel_func_t cancel_func,
              void *cancel_baton,
              apr_pool_t *pool)
{
  svn_error_t *err = svn_io_remove_dir2(path, TRUE,
                                        cancel_func, cancel_baton, pool);

  if (err && APR_STATUS_IS_ENOTEMPTY(err->apr_err))
    {
      svn_error_clear(err);
      err = SVN_NO_ERROR;
    }

  return svn_error_trace(err);
}

/* Baton for hotcopy_body(). */
struct hotcopy_body_baton {
  svn_fs_t *src_fs;
  svn_fs_t *dst_fs;
  svn_boolean_t incremental;
  svn_cancel_func_t cancel_func;
  void *cancel_baton;
} hotcopy_body_baton;

/* Perform a hotcopy, either normal or incremental.
 *
 * Normal hotcopy assumes that the destination exists as an empty
 * directory. It behaves like an incremental hotcopy except that
 * none of the copied files already exist in the destination.
 *
 * An incremental hotcopy copies only changed or new files to the destination,
 * and removes files from the destination no longer present in the source.
 * While the incremental hotcopy is running, readers should still be able
 * to access the destintation repository without error and should not see
 * revisions currently in progress of being copied. Readers are able to see
 * new fully copied revisions even if the entire incremental hotcopy procedure
 * has not yet completed.
 *
 * Writers are blocked out completely during the entire incremental hotcopy
 * process to ensure consistency. This function assumes that the repository
 * write-lock is held.
 */
static svn_error_t *
hotcopy_body(void *baton, apr_pool_t *pool)
{
  struct hotcopy_body_baton *hbb = baton;
  svn_fs_t *src_fs = hbb->src_fs;
  fs_fs_data_t *src_ffd = src_fs->fsap_data;
  svn_fs_t *dst_fs = hbb->dst_fs;
  fs_fs_data_t *dst_ffd = dst_fs->fsap_data;
  int max_files_per_dir = src_ffd->max_files_per_dir;
  svn_boolean_t incremental = hbb->incremental;
  svn_cancel_func_t cancel_func = hbb->cancel_func;
  void* cancel_baton = hbb->cancel_baton;
  svn_revnum_t src_youngest;
  svn_revnum_t dst_youngest;
  svn_revnum_t rev;
  svn_revnum_t src_min_unpacked_rev;
  svn_revnum_t dst_min_unpacked_rev;
  const char *src_subdir;
  const char *dst_subdir;
  const char *revprop_src_subdir;
  const char *revprop_dst_subdir;
  apr_pool_t *iterpool;
  svn_node_kind_t kind;

  /* Try to copy the config.
   *
   * ### We try copying the config file before doing anything else,
   * ### because higher layers will abort the hotcopy if we throw
   * ### an error from this function, and that renders the hotcopy
   * ### unusable anyway. */
  if (src_ffd->format >= SVN_FS_FS__MIN_CONFIG_FILE)
    {
      svn_error_t *err;

      err = svn_io_dir_file_copy(src_fs->path, dst_fs->path, PATH_CONFIG,
                                 pool);
      if (err)
        {
          if (APR_STATUS_IS_ENOENT(err->apr_err))
            {
              /* 1.6.0 to 1.6.11 did not copy the configuration file during
               * hotcopy. So if we're hotcopying a repository which has been
               * created as a hotcopy itself, it's possible that fsfs.conf
               * does not exist. Ask the user to re-create it.
               *
               * ### It would be nice to make this a non-fatal error,
               * ### but this function does not get an svn_fs_t object
               * ### so we have no way of just printing a warning via
               * ### the fs->warning() callback. */

              const char *msg;
              const char *src_abspath;
              const char *dst_abspath;
              const char *config_relpath;
              svn_error_t *err2;

              config_relpath = svn_dirent_join(src_fs->path, PATH_CONFIG, pool);
              err2 = svn_dirent_get_absolute(&src_abspath, src_fs->path, pool);
              if (err2)
                return svn_error_trace(svn_error_compose_create(err, err2));
              err2 = svn_dirent_get_absolute(&dst_abspath, dst_fs->path, pool);
              if (err2)
                return svn_error_trace(svn_error_compose_create(err, err2));

              /* ### hack: strip off the 'db/' directory from paths so
               * ### they make sense to the user */
              src_abspath = svn_dirent_dirname(src_abspath, pool);
              dst_abspath = svn_dirent_dirname(dst_abspath, pool);

              msg = apr_psprintf(pool,
                                 _("Failed to create hotcopy at '%s'. "
                                   "The file '%s' is missing from the source "
                                   "repository. Please create this file, for "
                                   "instance by running 'svnadmin upgrade %s'"),
                                 dst_abspath, config_relpath, src_abspath);
              return svn_error_quick_wrap(err, msg);
            }
          else
            return svn_error_trace(err);
        }
    }

  if (cancel_func)
    SVN_ERR(cancel_func(cancel_baton));

  /* Find the youngest revision in the source and destination.
   * We only support hotcopies from sources with an equal or greater amount
   * of revisions than the destination.
   * This also catches the case where users accidentally swap the
   * source and destination arguments. */
  SVN_ERR(get_youngest(&src_youngest, src_fs->path, pool));
  if (incremental)
    {
      SVN_ERR(get_youngest(&dst_youngest, dst_fs->path, pool));
      if (src_youngest < dst_youngest)
        return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                 _("The hotcopy destination already contains more revisions "
                   "(%lu) than the hotcopy source contains (%lu); are source "
                   "and destination swapped?"),
                  dst_youngest, src_youngest);
    }
  else
    dst_youngest = 0;

  if (cancel_func)
    SVN_ERR(cancel_func(cancel_baton));

  /* Copy the min unpacked rev, and read its value. */
  if (src_ffd->format >= SVN_FS_FS__MIN_PACKED_FORMAT)
    {
      const char *min_unpacked_rev_path;

      min_unpacked_rev_path = svn_dirent_join(src_fs->path,
                                              PATH_MIN_UNPACKED_REV,
                                              pool);
      SVN_ERR(read_min_unpacked_rev(&src_min_unpacked_rev,
                                    min_unpacked_rev_path,
                                    pool));

      min_unpacked_rev_path = svn_dirent_join(dst_fs->path,
                                              PATH_MIN_UNPACKED_REV,
                                              pool);
      SVN_ERR(read_min_unpacked_rev(&dst_min_unpacked_rev,
                                    min_unpacked_rev_path,
                                    pool));

      /* We only support packs coming from the hotcopy source.
       * The destination should not be packed independently from
       * the source. This also catches the case where users accidentally
       * swap the source and destination arguments. */
      if (src_min_unpacked_rev < dst_min_unpacked_rev)
        return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                                 _("The hotcopy destination already contains "
                                   "more packed revisions (%lu) than the "
                                   "hotcopy source contains (%lu)"),
                                   dst_min_unpacked_rev - 1,
                                   src_min_unpacked_rev - 1);

      SVN_ERR(svn_io_dir_file_copy(src_fs->path, dst_fs->path,
                                   PATH_MIN_UNPACKED_REV, pool));
    }
  else
    {
      src_min_unpacked_rev = 0;
      dst_min_unpacked_rev = 0;
    }

  if (cancel_func)
    SVN_ERR(cancel_func(cancel_baton));

  /*
   * Copy the necessary rev files.
   */

  src_subdir = svn_dirent_join(src_fs->path, PATH_REVS_DIR, pool);
  dst_subdir = svn_dirent_join(dst_fs->path, PATH_REVS_DIR, pool);
  SVN_ERR(svn_io_make_dir_recursively(dst_subdir, pool));

  iterpool = svn_pool_create(pool);
  /* First, copy packed shards. */
  for (rev = 0; rev < src_min_unpacked_rev; rev += max_files_per_dir)
    {
      svn_pool_clear(iterpool);

      if (cancel_func)
        SVN_ERR(cancel_func(cancel_baton));

      /* Copy the packed shard. */
      SVN_ERR(hotcopy_copy_packed_shard(&dst_min_unpacked_rev,
                                        src_fs, dst_fs,
                                        rev, max_files_per_dir,
                                        iterpool));

      /* If necessary, update 'current' to the most recent packed rev,
       * so readers can see new revisions which arrived in this pack. */
      SVN_ERR(hotcopy_update_current(&dst_youngest, dst_fs,
                                     rev + max_files_per_dir - 1,
                                     iterpool));

      /* Remove revision files which are now packed. */
      if (incremental)
        {
          SVN_ERR(hotcopy_remove_rev_files(dst_fs, rev,
                                           rev + max_files_per_dir,
                                           max_files_per_dir, iterpool));
          if (dst_ffd->format >= SVN_FS_FS__MIN_PACKED_REVPROP_FORMAT)
            SVN_ERR(hotcopy_remove_revprop_files(dst_fs, rev,
                                                 rev + max_files_per_dir,
                                                 max_files_per_dir,
                                                 iterpool));
        }

      /* Now that all revisions have moved into the pack, the original
       * rev dir can be removed. */
      SVN_ERR(remove_folder(path_rev_shard(dst_fs, rev, iterpool),
                            cancel_func, cancel_baton, iterpool));
      if (rev > 0 && dst_ffd->format >= SVN_FS_FS__MIN_PACKED_REVPROP_FORMAT)
        SVN_ERR(remove_folder(path_revprops_shard(dst_fs, rev, iterpool),
                              cancel_func, cancel_baton, iterpool));
    }

  if (cancel_func)
    SVN_ERR(cancel_func(cancel_baton));

  /* Now, copy pairs of non-packed revisions and revprop files.
   * If necessary, update 'current' after copying all files from a shard. */
  SVN_ERR_ASSERT(rev == src_min_unpacked_rev);
  SVN_ERR_ASSERT(src_min_unpacked_rev == dst_min_unpacked_rev);
  revprop_src_subdir = svn_dirent_join(src_fs->path, PATH_REVPROPS_DIR, pool);
  revprop_dst_subdir = svn_dirent_join(dst_fs->path, PATH_REVPROPS_DIR, pool);
  SVN_ERR(svn_io_make_dir_recursively(revprop_dst_subdir, pool));
  for (; rev <= src_youngest; rev++)
    {
      svn_error_t *err;

      svn_pool_clear(iterpool);

      if (cancel_func)
        SVN_ERR(cancel_func(cancel_baton));

      /* Copy the rev file. */
      err = hotcopy_copy_shard_file(src_subdir, dst_subdir,
                                    rev, max_files_per_dir,
                                    iterpool);
      if (err)
        {
          if (APR_STATUS_IS_ENOENT(err->apr_err) &&
              src_ffd->format >= SVN_FS_FS__MIN_PACKED_FORMAT)
            {
              svn_error_clear(err);

              /* The source rev file does not exist. This can happen if the
               * source repository is being packed concurrently with this
               * hotcopy operation.
               *
               * If the new revision is now packed, and the youngest revision
               * we're interested in is not inside this pack, try to copy the
               * pack instead.
               *
               * If the youngest revision ended up being packed, don't try
               * to be smart and work around this. Just abort the hotcopy. */
              SVN_ERR(update_min_unpacked_rev(src_fs, pool));
              if (is_packed_rev(src_fs, rev))
                {
                  if (is_packed_rev(src_fs, src_youngest))
                    return svn_error_createf(
                             SVN_ERR_FS_NO_SUCH_REVISION, NULL,
                             _("The assumed HEAD revision (%lu) of the "
                               "hotcopy source has been packed while the "
                               "hotcopy was in progress; please restart "
                               "the hotcopy operation"),
                             src_youngest);

                  SVN_ERR(hotcopy_copy_packed_shard(&dst_min_unpacked_rev,
                                                    src_fs, dst_fs,
                                                    rev, max_files_per_dir,
                                                    iterpool));
                  rev = dst_min_unpacked_rev;
                  continue;
                }
              else
                return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
                                         _("Revision %lu disappeared from the "
                                           "hotcopy source while hotcopy was "
                                           "in progress"), rev);
            }
          else
            return svn_error_trace(err);
        }

      /* Copy the revprop file. */
      SVN_ERR(hotcopy_copy_shard_file(revprop_src_subdir,
                                      revprop_dst_subdir,
                                      rev, max_files_per_dir,
                                      iterpool));

      /* After completing a full shard, update 'current'. */
      if (max_files_per_dir && rev % max_files_per_dir == 0)
        SVN_ERR(hotcopy_update_current(&dst_youngest, dst_fs, rev, iterpool));
    }
  svn_pool_destroy(iterpool);

  if (cancel_func)
    SVN_ERR(cancel_func(cancel_baton));

  /* We assume that all revisions were copied now, i.e. we didn't exit the
   * above loop early. 'rev' was last incremented during exit of the loop. */
  SVN_ERR_ASSERT(rev == src_youngest + 1);

  /* All revisions were copied. Update 'current'. */
  SVN_ERR(hotcopy_update_current(&dst_youngest, dst_fs, src_youngest, pool));

  /* Replace the locks tree.
   * This is racy in case readers are currently trying to list locks in
   * the destination. However, we need to get rid of stale locks.
   * This is the simplest way of doing this, so we accept this small race. */
  dst_subdir = svn_dirent_join(dst_fs->path, PATH_LOCKS_DIR, pool);
  SVN_ERR(svn_io_remove_dir2(dst_subdir, TRUE, cancel_func, cancel_baton,
                             pool));
  src_subdir = svn_dirent_join(src_fs->path, PATH_LOCKS_DIR, pool);
  SVN_ERR(svn_io_check_path(src_subdir, &kind, pool));
  if (kind == svn_node_dir)
    SVN_ERR(svn_io_copy_dir_recursively(src_subdir, dst_fs->path,
                                        PATH_LOCKS_DIR, TRUE,
                                        cancel_func, cancel_baton, pool));

  /* Now copy the node-origins cache tree. */
  src_subdir = svn_dirent_join(src_fs->path, PATH_NODE_ORIGINS_DIR, pool);
  SVN_ERR(svn_io_check_path(src_subdir, &kind, pool));
  if (kind == svn_node_dir)
    SVN_ERR(hotcopy_io_copy_dir_recursively(src_subdir, dst_fs->path,
                                            PATH_NODE_ORIGINS_DIR, TRUE,
                                            cancel_func, cancel_baton, pool));

  /*
   * NB: Data copied below is only read by writers, not readers.
   *     Writers are still locked out at this point.
   */

  if (dst_ffd->format >= SVN_FS_FS__MIN_REP_SHARING_FORMAT)
    {
      /* Copy the rep cache and then remove entries for revisions
       * younger than the destination's youngest revision. */
      src_subdir = svn_dirent_join(src_fs->path, REP_CACHE_DB_NAME, pool);
      dst_subdir = svn_dirent_join(dst_fs->path, REP_CACHE_DB_NAME, pool);
      SVN_ERR(svn_io_check_path(src_subdir, &kind, pool));
      if (kind == svn_node_file)
        {
          SVN_ERR(svn_sqlite__hotcopy(src_subdir, dst_subdir, pool));
          SVN_ERR(svn_fs_fs__del_rep_reference(dst_fs, dst_youngest, pool));
        }
    }

  /* Copy the txn-current file. */
  if (dst_ffd->format >= SVN_FS_FS__MIN_TXN_CURRENT_FORMAT)
    SVN_ERR(svn_io_dir_file_copy(src_fs->path, dst_fs->path,
                                 PATH_TXN_CURRENT, pool));

  /* If a revprop generation file exists in the source filesystem,
   * reset it to zero (since this is on a different path, it will not
   * overlap with data already in cache).  Also, clean up stale files
   * used for the named atomics implementation. */
  SVN_ERR(svn_io_check_path(path_revprop_generation(src_fs, pool),
                            &kind, pool));
  if (kind == svn_node_file)
    SVN_ERR(write_revprop_generation_file(dst_fs, 0, pool));

  SVN_ERR(cleanup_revprop_namespace(dst_fs));

  /* Hotcopied FS is complete. Stamp it with a format file. */
  SVN_ERR(write_format(svn_dirent_join(dst_fs->path, PATH_FORMAT, pool),
                       dst_ffd->format, max_files_per_dir, TRUE, pool));

  return SVN_NO_ERROR;
}


/* Set up shared data between SRC_FS and DST_FS. */
static void
hotcopy_setup_shared_fs_data(svn_fs_t *src_fs, svn_fs_t *dst_fs)
{
  fs_fs_data_t *src_ffd = src_fs->fsap_data;
  fs_fs_data_t *dst_ffd = dst_fs->fsap_data;

  /* The common pool and mutexes are shared between src and dst filesystems.
   * During hotcopy we only grab the mutexes for the destination, so there
   * is no risk of dead-lock. We don't write to the src filesystem. Shared
   * data for the src_fs has already been initialised in fs_hotcopy(). */
  dst_ffd->shared = src_ffd->shared;
}

/* Create an empty filesystem at DST_FS at DST_PATH with the same
 * configuration as SRC_FS (uuid, format, and other parameters).
 * After creation DST_FS has no revisions, not even revision zero. */
static svn_error_t *
hotcopy_create_empty_dest(svn_fs_t *src_fs,
                          svn_fs_t *dst_fs,
                          const char *dst_path,
                          apr_pool_t *pool)
{
  fs_fs_data_t *src_ffd = src_fs->fsap_data;
  fs_fs_data_t *dst_ffd = dst_fs->fsap_data;

  dst_fs->path = apr_pstrdup(pool, dst_path);

  dst_ffd->max_files_per_dir = src_ffd->max_files_per_dir;
  dst_ffd->config = src_ffd->config;
  dst_ffd->format = src_ffd->format;

  /* Create the revision data directories. */
  if (dst_ffd->max_files_per_dir)
    SVN_ERR(svn_io_make_dir_recursively(path_rev_shard(dst_fs, 0, pool),
                                        pool));
  else
    SVN_ERR(svn_io_make_dir_recursively(svn_dirent_join(dst_path,
                                                        PATH_REVS_DIR, pool),
                                        pool));

  /* Create the revprops directory. */
  if (src_ffd->max_files_per_dir)
    SVN_ERR(svn_io_make_dir_recursively(path_revprops_shard(dst_fs, 0, pool),
                                        pool));
  else
    SVN_ERR(svn_io_make_dir_recursively(svn_dirent_join(dst_path,
                                                        PATH_REVPROPS_DIR,
                                                        pool),
                                        pool));

  /* Create the transaction directory. */
  SVN_ERR(svn_io_make_dir_recursively(svn_dirent_join(dst_path, PATH_TXNS_DIR,
                                                      pool),
                                      pool));

  /* Create the protorevs directory. */
  if (dst_ffd->format >= SVN_FS_FS__MIN_PROTOREVS_DIR_FORMAT)
    SVN_ERR(svn_io_make_dir_recursively(svn_dirent_join(dst_path,
                                                        PATH_TXN_PROTOS_DIR,
                                                        pool),
                                        pool));

  /* Create the 'current' file. */
  SVN_ERR(svn_io_file_create(svn_fs_fs__path_current(dst_fs, pool),
                             (dst_ffd->format >=
                                SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT
                                ? "0\n" : "0 1 1\n"),
                             pool));

  /* Create lock file and UUID. */
  SVN_ERR(svn_io_file_create(path_lock(dst_fs, pool), "", pool));
  SVN_ERR(svn_fs_fs__set_uuid(dst_fs, src_fs->uuid, pool));

  /* Create the min unpacked rev file. */
  if (dst_ffd->format >= SVN_FS_FS__MIN_PACKED_FORMAT)
    SVN_ERR(svn_io_file_create(path_min_unpacked_rev(dst_fs, pool),
                                                     "0\n", pool));
  /* Create the txn-current file if the repository supports
     the transaction sequence file. */
  if (dst_ffd->format >= SVN_FS_FS__MIN_TXN_CURRENT_FORMAT)
    {
      SVN_ERR(svn_io_file_create(path_txn_current(dst_fs, pool),
                                 "0\n", pool));
      SVN_ERR(svn_io_file_create(path_txn_current_lock(dst_fs, pool),
                                 "", pool));
    }

  dst_ffd->youngest_rev_cache = 0;

  hotcopy_setup_shared_fs_data(src_fs, dst_fs);
  SVN_ERR(svn_fs_fs__initialize_caches(dst_fs, pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__hotcopy(svn_fs_t *src_fs,
                   svn_fs_t *dst_fs,
                   const char *src_path,
                   const char *dst_path,
                   svn_boolean_t incremental,
                   svn_cancel_func_t cancel_func,
                   void *cancel_baton,
                   apr_pool_t *pool)
{
  struct hotcopy_body_baton hbb;

  if (cancel_func)
    SVN_ERR(cancel_func(cancel_baton));

  SVN_ERR(svn_fs_fs__open(src_fs, src_path, pool));

  if (incremental)
    {
      const char *dst_format_abspath;
      svn_node_kind_t dst_format_kind;

      /* Check destination format to be sure we know how to incrementally
       * hotcopy to the destination FS. */
      dst_format_abspath = svn_dirent_join(dst_path, PATH_FORMAT, pool);
      SVN_ERR(svn_io_check_path(dst_format_abspath, &dst_format_kind, pool));
      if (dst_format_kind == svn_node_none)
        {
          /* Destination doesn't exist yet. Perform a normal hotcopy to a
           * empty destination using the same configuration as the source. */
          SVN_ERR(hotcopy_create_empty_dest(src_fs, dst_fs, dst_path, pool));
        }
      else
        {
          /* Check the existing repository. */
          SVN_ERR(svn_fs_fs__open(dst_fs, dst_path, pool));
          SVN_ERR(hotcopy_incremental_check_preconditions(src_fs, dst_fs,
                                                          pool));
          hotcopy_setup_shared_fs_data(src_fs, dst_fs);
          SVN_ERR(svn_fs_fs__initialize_caches(dst_fs, pool));
        }
    }
  else
    {
      /* Start out with an empty destination using the same configuration
       * as the source. */
      SVN_ERR(hotcopy_create_empty_dest(src_fs, dst_fs, dst_path, pool));
    }

  if (cancel_func)
    SVN_ERR(cancel_func(cancel_baton));

  hbb.src_fs = src_fs;
  hbb.dst_fs = dst_fs;
  hbb.incremental = incremental;
  hbb.cancel_func = cancel_func;
  hbb.cancel_baton = cancel_baton;
  SVN_ERR(svn_fs_fs__with_write_lock(dst_fs, hotcopy_body, &hbb, pool));

  return SVN_NO_ERROR;
}
