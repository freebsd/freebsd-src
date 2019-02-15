/* lock.c :  functions for manipulating filesystem locks.
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
#include "svn_error.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_fs.h"
#include "svn_hash.h"
#include "svn_time.h"
#include "svn_utf.h"

#include <apr_uuid.h>
#include <apr_file_io.h>
#include <apr_file_info.h>

#include "lock.h"
#include "tree.h"
#include "fs_fs.h"
#include "../libsvn_fs/fs-loader.h"

#include "private/svn_fs_util.h"
#include "private/svn_fspath.h"
#include "svn_private_config.h"

/* Names of hash keys used to store a lock for writing to disk. */
#define PATH_KEY "path"
#define TOKEN_KEY "token"
#define OWNER_KEY "owner"
#define CREATION_DATE_KEY "creation_date"
#define EXPIRATION_DATE_KEY "expiration_date"
#define COMMENT_KEY "comment"
#define IS_DAV_COMMENT_KEY "is_dav_comment"
#define CHILDREN_KEY "children"

/* Number of characters from the head of a digest file name used to
   calculate a subdirectory in which to drop that file. */
#define DIGEST_SUBDIR_LEN 3



/*** Generic helper functions. ***/

/* Set *DIGEST to the MD5 hash of STR. */
static svn_error_t *
make_digest(const char **digest,
            const char *str,
            apr_pool_t *pool)
{
  svn_checksum_t *checksum;

  SVN_ERR(svn_checksum(&checksum, svn_checksum_md5, str, strlen(str), pool));

  *digest = svn_checksum_to_cstring_display(checksum, pool);
  return SVN_NO_ERROR;
}


/* Set the value of KEY (whose size is KEY_LEN, or APR_HASH_KEY_STRING
   if unknown) to an svn_string_t-ized version of VALUE (whose size is
   VALUE_LEN, or APR_HASH_KEY_STRING if unknown) in HASH.  The value
   will be allocated in POOL; KEY will not be duped.  If either KEY or VALUE
   is NULL, this function will do nothing. */
static void
hash_store(apr_hash_t *hash,
           const char *key,
           apr_ssize_t key_len,
           const char *value,
           apr_ssize_t value_len,
           apr_pool_t *pool)
{
  if (! (key && value))
    return;
  if (value_len == APR_HASH_KEY_STRING)
    value_len = strlen(value);
  apr_hash_set(hash, key, key_len,
               svn_string_ncreate(value, value_len, pool));
}


/* Fetch the value of KEY from HASH, returning only the cstring data
   of that value (if it exists). */
static const char *
hash_fetch(apr_hash_t *hash,
           const char *key,
           apr_pool_t *pool)
{
  svn_string_t *str = svn_hash_gets(hash, key);
  return str ? str->data : NULL;
}


/* SVN_ERR_FS_CORRUPT: the lockfile for PATH in FS is corrupt.  */
static svn_error_t *
err_corrupt_lockfile(const char *fs_path, const char *path)
{
  return
    svn_error_createf(
     SVN_ERR_FS_CORRUPT, 0,
     _("Corrupt lockfile for path '%s' in filesystem '%s'"),
     path, fs_path);
}


/*** Digest file handling functions. ***/

/* Return the path of the lock/entries file for which DIGEST is the
   hashed repository relative path. */
static const char *
digest_path_from_digest(const char *fs_path,
                        const char *digest,
                        apr_pool_t *pool)
{
  return svn_dirent_join_many(pool, fs_path, PATH_LOCKS_DIR,
                              apr_pstrmemdup(pool, digest, DIGEST_SUBDIR_LEN),
                              digest, NULL);
}


/* Set *DIGEST_PATH to the path to the lock/entries digest file associate
   with PATH, where PATH is the path to the lock file or lock entries file
   in FS. */
static svn_error_t *
digest_path_from_path(const char **digest_path,
                      const char *fs_path,
                      const char *path,
                      apr_pool_t *pool)
{
  const char *digest;
  SVN_ERR(make_digest(&digest, path, pool));
  *digest_path = svn_dirent_join_many(pool, fs_path, PATH_LOCKS_DIR,
                                      apr_pstrmemdup(pool, digest,
                                                     DIGEST_SUBDIR_LEN),
                                      digest, NULL);
  return SVN_NO_ERROR;
}


/* Write to DIGEST_PATH a representation of CHILDREN (which may be
   empty, if the versioned path in FS represented by DIGEST_PATH has
   no children) and LOCK (which may be NULL if that versioned path is
   lock itself locked).  Set the permissions of DIGEST_PATH to those of
   PERMS_REFERENCE.  Use POOL for all allocations.
 */
static svn_error_t *
write_digest_file(apr_hash_t *children,
                  svn_lock_t *lock,
                  const char *fs_path,
                  const char *digest_path,
                  const char *perms_reference,
                  apr_pool_t *pool)
{
  svn_error_t *err = SVN_NO_ERROR;
  svn_stream_t *stream;
  apr_hash_index_t *hi;
  apr_hash_t *hash = apr_hash_make(pool);
  const char *tmp_path;

  SVN_ERR(svn_fs_fs__ensure_dir_exists(svn_dirent_join(fs_path, PATH_LOCKS_DIR,
                                                       pool), fs_path, pool));
  SVN_ERR(svn_fs_fs__ensure_dir_exists(svn_dirent_dirname(digest_path, pool),
                                       fs_path, pool));

  if (lock)
    {
      const char *creation_date = NULL, *expiration_date = NULL;
      if (lock->creation_date)
        creation_date = svn_time_to_cstring(lock->creation_date, pool);
      if (lock->expiration_date)
        expiration_date = svn_time_to_cstring(lock->expiration_date, pool);
      hash_store(hash, PATH_KEY, sizeof(PATH_KEY)-1,
                 lock->path, APR_HASH_KEY_STRING, pool);
      hash_store(hash, TOKEN_KEY, sizeof(TOKEN_KEY)-1,
                 lock->token, APR_HASH_KEY_STRING, pool);
      hash_store(hash, OWNER_KEY, sizeof(OWNER_KEY)-1,
                 lock->owner, APR_HASH_KEY_STRING, pool);
      hash_store(hash, COMMENT_KEY, sizeof(COMMENT_KEY)-1,
                 lock->comment, APR_HASH_KEY_STRING, pool);
      hash_store(hash, IS_DAV_COMMENT_KEY, sizeof(IS_DAV_COMMENT_KEY)-1,
                 lock->is_dav_comment ? "1" : "0", 1, pool);
      hash_store(hash, CREATION_DATE_KEY, sizeof(CREATION_DATE_KEY)-1,
                 creation_date, APR_HASH_KEY_STRING, pool);
      hash_store(hash, EXPIRATION_DATE_KEY, sizeof(EXPIRATION_DATE_KEY)-1,
                 expiration_date, APR_HASH_KEY_STRING, pool);
    }
  if (apr_hash_count(children))
    {
      svn_stringbuf_t *children_list = svn_stringbuf_create_empty(pool);
      for (hi = apr_hash_first(pool, children); hi; hi = apr_hash_next(hi))
        {
          svn_stringbuf_appendbytes(children_list,
                                    svn__apr_hash_index_key(hi),
                                    svn__apr_hash_index_klen(hi));
          svn_stringbuf_appendbyte(children_list, '\n');
        }
      hash_store(hash, CHILDREN_KEY, sizeof(CHILDREN_KEY)-1,
                 children_list->data, children_list->len, pool);
    }

  SVN_ERR(svn_stream_open_unique(&stream, &tmp_path,
                                 svn_dirent_dirname(digest_path, pool),
                                 svn_io_file_del_none, pool, pool));
  if ((err = svn_hash_write2(hash, stream, SVN_HASH_TERMINATOR, pool)))
    {
      svn_error_clear(svn_stream_close(stream));
      return svn_error_createf(err->apr_err,
                               err,
                               _("Cannot write lock/entries hashfile '%s'"),
                               svn_dirent_local_style(tmp_path, pool));
    }

  SVN_ERR(svn_stream_close(stream));
  SVN_ERR(svn_io_file_rename(tmp_path, digest_path, pool));
  SVN_ERR(svn_io_copy_perms(perms_reference, digest_path, pool));
  return SVN_NO_ERROR;
}


/* Parse the file at DIGEST_PATH, populating the lock LOCK_P in that
   file (if it exists, and if *LOCK_P is non-NULL) and the hash of
   CHILDREN_P (if any exist, and if *CHILDREN_P is non-NULL).  Use POOL
   for all allocations.  */
static svn_error_t *
read_digest_file(apr_hash_t **children_p,
                 svn_lock_t **lock_p,
                 const char *fs_path,
                 const char *digest_path,
                 apr_pool_t *pool)
{
  svn_error_t *err = SVN_NO_ERROR;
  svn_lock_t *lock;
  apr_hash_t *hash;
  svn_stream_t *stream;
  const char *val;

  if (lock_p)
    *lock_p = NULL;
  if (children_p)
    *children_p = apr_hash_make(pool);

  err = svn_stream_open_readonly(&stream, digest_path, pool, pool);
  if (err && APR_STATUS_IS_ENOENT(err->apr_err))
    {
      svn_error_clear(err);
      return SVN_NO_ERROR;
    }
  SVN_ERR(err);

  /* If our caller doesn't care about anything but the presence of the
     file... whatever. */
  if (! (lock_p || children_p))
    return svn_stream_close(stream);

  hash = apr_hash_make(pool);
  if ((err = svn_hash_read2(hash, stream, SVN_HASH_TERMINATOR, pool)))
    {
      svn_error_clear(svn_stream_close(stream));
      return svn_error_createf(err->apr_err,
                               err,
                               _("Can't parse lock/entries hashfile '%s'"),
                               svn_dirent_local_style(digest_path, pool));
    }
  SVN_ERR(svn_stream_close(stream));

  /* If our caller cares, see if we have a lock path in our hash. If
     so, we'll assume we have a lock here. */
  val = hash_fetch(hash, PATH_KEY, pool);
  if (val && lock_p)
    {
      const char *path = val;

      /* Create our lock and load it up. */
      lock = svn_lock_create(pool);
      lock->path = path;

      if (! ((lock->token = hash_fetch(hash, TOKEN_KEY, pool))))
        return svn_error_trace(err_corrupt_lockfile(fs_path, path));

      if (! ((lock->owner = hash_fetch(hash, OWNER_KEY, pool))))
        return svn_error_trace(err_corrupt_lockfile(fs_path, path));

      if (! ((val = hash_fetch(hash, IS_DAV_COMMENT_KEY, pool))))
        return svn_error_trace(err_corrupt_lockfile(fs_path, path));
      lock->is_dav_comment = (val[0] == '1');

      if (! ((val = hash_fetch(hash, CREATION_DATE_KEY, pool))))
        return svn_error_trace(err_corrupt_lockfile(fs_path, path));
      SVN_ERR(svn_time_from_cstring(&(lock->creation_date), val, pool));

      if ((val = hash_fetch(hash, EXPIRATION_DATE_KEY, pool)))
        SVN_ERR(svn_time_from_cstring(&(lock->expiration_date), val, pool));

      lock->comment = hash_fetch(hash, COMMENT_KEY, pool);

      *lock_p = lock;
    }

  /* If our caller cares, see if we have any children for this path. */
  val = hash_fetch(hash, CHILDREN_KEY, pool);
  if (val && children_p)
    {
      apr_array_header_t *kiddos = svn_cstring_split(val, "\n", FALSE, pool);
      int i;

      for (i = 0; i < kiddos->nelts; i++)
        {
          svn_hash_sets(*children_p, APR_ARRAY_IDX(kiddos, i, const char *),
                        (void *)1);
        }
    }
  return SVN_NO_ERROR;
}



/*** Lock helper functions (path here are still FS paths, not on-disk
     schema-supporting paths) ***/


/* Write LOCK in FS to the actual OS filesystem.

   Use PERMS_REFERENCE for the permissions of any digest files.

   Note: this takes an FS_PATH because it's called from the hotcopy logic.
 */
static svn_error_t *
set_lock(const char *fs_path,
         svn_lock_t *lock,
         const char *perms_reference,
         apr_pool_t *pool)
{
  svn_stringbuf_t *this_path = svn_stringbuf_create(lock->path, pool);
  const char *lock_digest_path = NULL;
  apr_pool_t *subpool;

  SVN_ERR_ASSERT(lock);

  /* Iterate in reverse, creating the lock for LOCK->path, and then
     just adding entries for its parent, until we reach a parent
     that's already listed in *its* parent. */
  subpool = svn_pool_create(pool);
  while (1729)
    {
      const char *digest_path, *digest_file;
      apr_hash_t *this_children;
      svn_lock_t *this_lock;

      svn_pool_clear(subpool);

      /* Calculate the DIGEST_PATH for the currently FS path, and then
         get its DIGEST_FILE basename. */
      SVN_ERR(digest_path_from_path(&digest_path, fs_path, this_path->data,
                                    subpool));
      digest_file = svn_dirent_basename(digest_path, subpool);

      SVN_ERR(read_digest_file(&this_children, &this_lock, fs_path,
                               digest_path, subpool));

      /* We're either writing a new lock (first time through only) or
         a new entry (every time but the first). */
      if (lock)
        {
          this_lock = lock;
          lock = NULL;
          lock_digest_path = apr_pstrdup(pool, digest_file);
        }
      else
        {
          /* If we already have an entry for this path, we're done. */
          if (svn_hash_gets(this_children, lock_digest_path))
            break;
          svn_hash_sets(this_children, lock_digest_path, (void *)1);
        }
      SVN_ERR(write_digest_file(this_children, this_lock, fs_path,
                                digest_path, perms_reference, subpool));

      /* Prep for next iteration, or bail if we're done. */
      if (svn_fspath__is_root(this_path->data, this_path->len))
        break;
      svn_stringbuf_set(this_path,
                        svn_fspath__dirname(this_path->data, subpool));
    }

  svn_pool_destroy(subpool);
  return SVN_NO_ERROR;
}

/* Delete LOCK from FS in the actual OS filesystem. */
static svn_error_t *
delete_lock(svn_fs_t *fs,
            svn_lock_t *lock,
            apr_pool_t *pool)
{
  svn_stringbuf_t *this_path = svn_stringbuf_create(lock->path, pool);
  const char *child_to_kill = NULL;
  apr_pool_t *subpool;

  SVN_ERR_ASSERT(lock);

  /* Iterate in reverse, deleting the lock for LOCK->path, and then
     deleting its entry as it appears in each of its parents. */
  subpool = svn_pool_create(pool);
  while (1729)
    {
      const char *digest_path, *digest_file;
      apr_hash_t *this_children;
      svn_lock_t *this_lock;

      svn_pool_clear(subpool);

      /* Calculate the DIGEST_PATH for the currently FS path, and then
         get its DIGEST_FILE basename. */
      SVN_ERR(digest_path_from_path(&digest_path, fs->path, this_path->data,
                                    subpool));
      digest_file = svn_dirent_basename(digest_path, subpool);

      SVN_ERR(read_digest_file(&this_children, &this_lock, fs->path,
                               digest_path, subpool));

      /* Delete the lock (first time through only). */
      if (lock)
        {
          this_lock = NULL;
          lock = NULL;
          child_to_kill = apr_pstrdup(pool, digest_file);
        }

      if (child_to_kill)
        svn_hash_sets(this_children, child_to_kill, NULL);

      if (! (this_lock || apr_hash_count(this_children) != 0))
        {
          /* Special case:  no goodz, no file.  And remember to nix
             the entry for it in its parent. */
          SVN_ERR(svn_io_remove_file2(digest_path, FALSE, subpool));
        }
      else
        {
          const char *rev_0_path;
          SVN_ERR(svn_fs_fs__path_rev_absolute(&rev_0_path, fs, 0, pool));
          SVN_ERR(write_digest_file(this_children, this_lock, fs->path,
                                    digest_path, rev_0_path, subpool));
        }

      /* Prep for next iteration, or bail if we're done. */
      if (svn_fspath__is_root(this_path->data, this_path->len))
        break;
      svn_stringbuf_set(this_path,
                        svn_fspath__dirname(this_path->data, subpool));
    }

  svn_pool_destroy(subpool);
  return SVN_NO_ERROR;
}

/* Set *LOCK_P to the lock for PATH in FS.  HAVE_WRITE_LOCK should be
   TRUE if the caller (or one of its callers) has taken out the
   repository-wide write lock, FALSE otherwise.  If MUST_EXIST is
   not set, the function will simply return NULL in *LOCK_P instead
   of creating an SVN_FS__ERR_NO_SUCH_LOCK error in case the lock
   was not found (much faster).  Use POOL for allocations. */
static svn_error_t *
get_lock(svn_lock_t **lock_p,
         svn_fs_t *fs,
         const char *path,
         svn_boolean_t have_write_lock,
         svn_boolean_t must_exist,
         apr_pool_t *pool)
{
  svn_lock_t *lock = NULL;
  const char *digest_path;
  svn_node_kind_t kind;

  SVN_ERR(digest_path_from_path(&digest_path, fs->path, path, pool));
  SVN_ERR(svn_io_check_path(digest_path, &kind, pool));

  *lock_p = NULL;
  if (kind != svn_node_none)
    SVN_ERR(read_digest_file(NULL, &lock, fs->path, digest_path, pool));

  if (! lock)
    return must_exist ? SVN_FS__ERR_NO_SUCH_LOCK(fs, path) : SVN_NO_ERROR;

  /* Don't return an expired lock. */
  if (lock->expiration_date && (apr_time_now() > lock->expiration_date))
    {
      /* Only remove the lock if we have the write lock.
         Read operations shouldn't change the filesystem. */
      if (have_write_lock)
        SVN_ERR(delete_lock(fs, lock, pool));
      return SVN_FS__ERR_LOCK_EXPIRED(fs, lock->token);
    }

  *lock_p = lock;
  return SVN_NO_ERROR;
}


/* Set *LOCK_P to the lock for PATH in FS.  HAVE_WRITE_LOCK should be
   TRUE if the caller (or one of its callers) has taken out the
   repository-wide write lock, FALSE otherwise.  Use POOL for
   allocations. */
static svn_error_t *
get_lock_helper(svn_fs_t *fs,
                svn_lock_t **lock_p,
                const char *path,
                svn_boolean_t have_write_lock,
                apr_pool_t *pool)
{
  svn_lock_t *lock;
  svn_error_t *err;

  err = get_lock(&lock, fs, path, have_write_lock, FALSE, pool);

  /* We've deliberately decided that this function doesn't tell the
     caller *why* the lock is unavailable.  */
  if (err && ((err->apr_err == SVN_ERR_FS_NO_SUCH_LOCK)
              || (err->apr_err == SVN_ERR_FS_LOCK_EXPIRED)))
    {
      svn_error_clear(err);
      *lock_p = NULL;
      return SVN_NO_ERROR;
    }
  else
    SVN_ERR(err);

  *lock_p = lock;
  return SVN_NO_ERROR;
}


/* Baton for locks_walker(). */
struct walk_locks_baton {
  svn_fs_get_locks_callback_t get_locks_func;
  void *get_locks_baton;
  svn_fs_t *fs;
};

/* Implements walk_digests_callback_t. */
static svn_error_t *
locks_walker(void *baton,
             const char *fs_path,
             const char *digest_path,
             apr_hash_t *children,
             svn_lock_t *lock,
             svn_boolean_t have_write_lock,
             apr_pool_t *pool)
{
  struct walk_locks_baton *wlb = baton;

  if (lock)
    {
      /* Don't report an expired lock. */
      if (lock->expiration_date == 0
          || (apr_time_now() <= lock->expiration_date))
        {
          if (wlb->get_locks_func)
            SVN_ERR(wlb->get_locks_func(wlb->get_locks_baton, lock, pool));
        }
      else
        {
          /* Only remove the lock if we have the write lock.
             Read operations shouldn't change the filesystem. */
          if (have_write_lock)
            SVN_ERR(delete_lock(wlb->fs, lock, pool));
        }
    }

  return SVN_NO_ERROR;
}

/* Callback type for walk_digest_files().
 *
 * CHILDREN and LOCK come from a read_digest_file(digest_path) call.
 */
typedef svn_error_t *(*walk_digests_callback_t)(void *baton,
                                                const char *fs_path,
                                                const char *digest_path,
                                                apr_hash_t *children,
                                                svn_lock_t *lock,
                                                svn_boolean_t have_write_lock,
                                                apr_pool_t *pool);

/* A recursive function that calls WALK_DIGESTS_FUNC/WALK_DIGESTS_BATON for
   all lock digest files in and under PATH in FS.
   HAVE_WRITE_LOCK should be true if the caller (directly or indirectly)
   has the FS write lock. */
static svn_error_t *
walk_digest_files(const char *fs_path,
                  const char *digest_path,
                  walk_digests_callback_t walk_digests_func,
                  void *walk_digests_baton,
                  svn_boolean_t have_write_lock,
                  apr_pool_t *pool)
{
  apr_hash_index_t *hi;
  apr_hash_t *children;
  apr_pool_t *subpool;
  svn_lock_t *lock;

  /* First, send up any locks in the current digest file. */
  SVN_ERR(read_digest_file(&children, &lock, fs_path, digest_path, pool));

  SVN_ERR(walk_digests_func(walk_digests_baton, fs_path, digest_path,
                            children, lock,
                            have_write_lock, pool));

  /* Now, recurse on this thing's child entries (if any; bail otherwise). */
  if (! apr_hash_count(children))
    return SVN_NO_ERROR;
  subpool = svn_pool_create(pool);
  for (hi = apr_hash_first(pool, children); hi; hi = apr_hash_next(hi))
    {
      const char *digest = svn__apr_hash_index_key(hi);
      svn_pool_clear(subpool);
      SVN_ERR(walk_digest_files
              (fs_path, digest_path_from_digest(fs_path, digest, subpool),
               walk_digests_func, walk_digests_baton, have_write_lock, subpool));
    }
  svn_pool_destroy(subpool);
  return SVN_NO_ERROR;
}

/* A recursive function that calls GET_LOCKS_FUNC/GET_LOCKS_BATON for
   all locks in and under PATH in FS.
   HAVE_WRITE_LOCK should be true if the caller (directly or indirectly)
   has the FS write lock. */
static svn_error_t *
walk_locks(svn_fs_t *fs,
           const char *digest_path,
           svn_fs_get_locks_callback_t get_locks_func,
           void *get_locks_baton,
           svn_boolean_t have_write_lock,
           apr_pool_t *pool)
{
  struct walk_locks_baton wlb;

  wlb.get_locks_func = get_locks_func;
  wlb.get_locks_baton = get_locks_baton;
  wlb.fs = fs;
  SVN_ERR(walk_digest_files(fs->path, digest_path, locks_walker, &wlb,
                            have_write_lock, pool));
  return SVN_NO_ERROR;
}


/* Utility function:  verify that a lock can be used.  Interesting
   errors returned from this function:

      SVN_ERR_FS_NO_USER: No username attached to FS.
      SVN_ERR_FS_LOCK_OWNER_MISMATCH: FS's username doesn't match LOCK's owner.
      SVN_ERR_FS_BAD_LOCK_TOKEN: FS doesn't hold matching lock-token for LOCK.
 */
static svn_error_t *
verify_lock(svn_fs_t *fs,
            svn_lock_t *lock,
            apr_pool_t *pool)
{
  if ((! fs->access_ctx) || (! fs->access_ctx->username))
    return svn_error_createf
      (SVN_ERR_FS_NO_USER, NULL,
       _("Cannot verify lock on path '%s'; no username available"),
       lock->path);

  else if (strcmp(fs->access_ctx->username, lock->owner) != 0)
    return svn_error_createf
      (SVN_ERR_FS_LOCK_OWNER_MISMATCH, NULL,
       _("User '%s' does not own lock on path '%s' (currently locked by '%s')"),
       fs->access_ctx->username, lock->path, lock->owner);

  else if (svn_hash_gets(fs->access_ctx->lock_tokens, lock->token) == NULL)
    return svn_error_createf
      (SVN_ERR_FS_BAD_LOCK_TOKEN, NULL,
       _("Cannot verify lock on path '%s'; no matching lock-token available"),
       lock->path);

  return SVN_NO_ERROR;
}


/* This implements the svn_fs_get_locks_callback_t interface, where
   BATON is just an svn_fs_t object. */
static svn_error_t *
get_locks_callback(void *baton,
                   svn_lock_t *lock,
                   apr_pool_t *pool)
{
  return verify_lock(baton, lock, pool);
}


/* The main routine for lock enforcement, used throughout libsvn_fs_fs. */
svn_error_t *
svn_fs_fs__allow_locked_operation(const char *path,
                                  svn_fs_t *fs,
                                  svn_boolean_t recurse,
                                  svn_boolean_t have_write_lock,
                                  apr_pool_t *pool)
{
  path = svn_fs__canonicalize_abspath(path, pool);
  if (recurse)
    {
      /* Discover all locks at or below the path. */
      const char *digest_path;
      SVN_ERR(digest_path_from_path(&digest_path, fs->path, path, pool));
      SVN_ERR(walk_locks(fs, digest_path, get_locks_callback,
                         fs, have_write_lock, pool));
    }
  else
    {
      /* Discover and verify any lock attached to the path. */
      svn_lock_t *lock;
      SVN_ERR(get_lock_helper(fs, &lock, path, have_write_lock, pool));
      if (lock)
        SVN_ERR(verify_lock(fs, lock, pool));
    }
  return SVN_NO_ERROR;
}

/* Baton used for lock_body below. */
struct lock_baton {
  svn_lock_t **lock_p;
  svn_fs_t *fs;
  const char *path;
  const char *token;
  const char *comment;
  svn_boolean_t is_dav_comment;
  apr_time_t expiration_date;
  svn_revnum_t current_rev;
  svn_boolean_t steal_lock;
  apr_pool_t *pool;
};


/* This implements the svn_fs_fs__with_write_lock() 'body' callback
   type, and assumes that the write lock is held.
   BATON is a 'struct lock_baton *'. */
static svn_error_t *
lock_body(void *baton, apr_pool_t *pool)
{
  struct lock_baton *lb = baton;
  svn_node_kind_t kind;
  svn_lock_t *existing_lock;
  svn_lock_t *lock;
  svn_fs_root_t *root;
  svn_revnum_t youngest;
  const char *rev_0_path;

  /* Until we implement directory locks someday, we only allow locks
     on files or non-existent paths. */
  /* Use fs->vtable->foo instead of svn_fs_foo to avoid circular
     library dependencies, which are not portable. */
  SVN_ERR(lb->fs->vtable->youngest_rev(&youngest, lb->fs, pool));
  SVN_ERR(lb->fs->vtable->revision_root(&root, lb->fs, youngest, pool));
  SVN_ERR(svn_fs_fs__check_path(&kind, root, lb->path, pool));
  if (kind == svn_node_dir)
    return SVN_FS__ERR_NOT_FILE(lb->fs, lb->path);

  /* While our locking implementation easily supports the locking of
     nonexistent paths, we deliberately choose not to allow such madness. */
  if (kind == svn_node_none)
    {
      if (SVN_IS_VALID_REVNUM(lb->current_rev))
        return svn_error_createf(
          SVN_ERR_FS_OUT_OF_DATE, NULL,
          _("Path '%s' doesn't exist in HEAD revision"),
          lb->path);
      else
        return svn_error_createf(
          SVN_ERR_FS_NOT_FOUND, NULL,
          _("Path '%s' doesn't exist in HEAD revision"),
          lb->path);
    }

  /* We need to have a username attached to the fs. */
  if (!lb->fs->access_ctx || !lb->fs->access_ctx->username)
    return SVN_FS__ERR_NO_USER(lb->fs);

  /* Is the caller attempting to lock an out-of-date working file? */
  if (SVN_IS_VALID_REVNUM(lb->current_rev))
    {
      svn_revnum_t created_rev;
      SVN_ERR(svn_fs_fs__node_created_rev(&created_rev, root, lb->path,
                                          pool));

      /* SVN_INVALID_REVNUM means the path doesn't exist.  So
         apparently somebody is trying to lock something in their
         working copy, but somebody else has deleted the thing
         from HEAD.  That counts as being 'out of date'. */
      if (! SVN_IS_VALID_REVNUM(created_rev))
        return svn_error_createf
          (SVN_ERR_FS_OUT_OF_DATE, NULL,
           _("Path '%s' doesn't exist in HEAD revision"), lb->path);

      if (lb->current_rev < created_rev)
        return svn_error_createf
          (SVN_ERR_FS_OUT_OF_DATE, NULL,
           _("Lock failed: newer version of '%s' exists"), lb->path);
    }

  /* If the caller provided a TOKEN, we *really* need to see
     if a lock already exists with that token, and if so, verify that
     the lock's path matches PATH.  Otherwise we run the risk of
     breaking the 1-to-1 mapping of lock tokens to locked paths. */
  /* ### TODO:  actually do this check.  This is tough, because the
     schema doesn't supply a lookup-by-token mechanism. */

  /* Is the path already locked?

     Note that this next function call will automatically ignore any
     errors about {the path not existing as a key, the path's token
     not existing as a key, the lock just having been expired}.  And
     that's totally fine.  Any of these three errors are perfectly
     acceptable to ignore; it means that the path is now free and
     clear for locking, because the fsfs funcs just cleared out both
     of the tables for us.   */
  SVN_ERR(get_lock_helper(lb->fs, &existing_lock, lb->path, TRUE, pool));
  if (existing_lock)
    {
      if (! lb->steal_lock)
        {
          /* Sorry, the path is already locked. */
          return SVN_FS__ERR_PATH_ALREADY_LOCKED(lb->fs, existing_lock);
        }
      else
        {
          /* STEAL_LOCK was passed, so fs_username is "stealing" the
             lock from lock->owner.  Destroy the existing lock. */
          SVN_ERR(delete_lock(lb->fs, existing_lock, pool));
        }
    }

  /* Create our new lock, and add it to the tables.
     Ensure that the lock is created in the correct pool. */
  lock = svn_lock_create(lb->pool);
  if (lb->token)
    lock->token = apr_pstrdup(lb->pool, lb->token);
  else
    SVN_ERR(svn_fs_fs__generate_lock_token(&(lock->token), lb->fs,
                                           lb->pool));
  lock->path = apr_pstrdup(lb->pool, lb->path);
  lock->owner = apr_pstrdup(lb->pool, lb->fs->access_ctx->username);
  lock->comment = apr_pstrdup(lb->pool, lb->comment);
  lock->is_dav_comment = lb->is_dav_comment;
  lock->creation_date = apr_time_now();
  lock->expiration_date = lb->expiration_date;
  SVN_ERR(svn_fs_fs__path_rev_absolute(&rev_0_path, lb->fs, 0, pool));
  SVN_ERR(set_lock(lb->fs->path, lock, rev_0_path, pool));
  *lb->lock_p = lock;

  return SVN_NO_ERROR;
}

/* Baton used for unlock_body below. */
struct unlock_baton {
  svn_fs_t *fs;
  const char *path;
  const char *token;
  svn_boolean_t break_lock;
};

/* This implements the svn_fs_fs__with_write_lock() 'body' callback
   type, and assumes that the write lock is held.
   BATON is a 'struct unlock_baton *'. */
static svn_error_t *
unlock_body(void *baton, apr_pool_t *pool)
{
  struct unlock_baton *ub = baton;
  svn_lock_t *lock;

  /* This could return SVN_ERR_FS_BAD_LOCK_TOKEN or SVN_ERR_FS_LOCK_EXPIRED. */
  SVN_ERR(get_lock(&lock, ub->fs, ub->path, TRUE, TRUE, pool));

  /* Unless breaking the lock, we do some checks. */
  if (! ub->break_lock)
    {
      /* Sanity check:  the incoming token should match lock->token. */
      if (strcmp(ub->token, lock->token) != 0)
        return SVN_FS__ERR_NO_SUCH_LOCK(ub->fs, lock->path);

      /* There better be a username attached to the fs. */
      if (! (ub->fs->access_ctx && ub->fs->access_ctx->username))
        return SVN_FS__ERR_NO_USER(ub->fs);

      /* And that username better be the same as the lock's owner. */
      if (strcmp(ub->fs->access_ctx->username, lock->owner) != 0)
        return SVN_FS__ERR_LOCK_OWNER_MISMATCH(
           ub->fs, ub->fs->access_ctx->username, lock->owner);
    }

  /* Remove lock and lock token files. */
  return delete_lock(ub->fs, lock, pool);
}


/*** Public API implementations ***/

svn_error_t *
svn_fs_fs__lock(svn_lock_t **lock_p,
                svn_fs_t *fs,
                const char *path,
                const char *token,
                const char *comment,
                svn_boolean_t is_dav_comment,
                apr_time_t expiration_date,
                svn_revnum_t current_rev,
                svn_boolean_t steal_lock,
                apr_pool_t *pool)
{
  struct lock_baton lb;

  SVN_ERR(svn_fs__check_fs(fs, TRUE));
  path = svn_fs__canonicalize_abspath(path, pool);

  lb.lock_p = lock_p;
  lb.fs = fs;
  lb.path = path;
  lb.token = token;
  lb.comment = comment;
  lb.is_dav_comment = is_dav_comment;
  lb.expiration_date = expiration_date;
  lb.current_rev = current_rev;
  lb.steal_lock = steal_lock;
  lb.pool = pool;

  return svn_fs_fs__with_write_lock(fs, lock_body, &lb, pool);
}


svn_error_t *
svn_fs_fs__generate_lock_token(const char **token,
                               svn_fs_t *fs,
                               apr_pool_t *pool)
{
  SVN_ERR(svn_fs__check_fs(fs, TRUE));

  /* Notice that 'fs' is currently unused.  But perhaps someday, we'll
     want to use the fs UUID + some incremented number?  For now, we
     generate a URI that matches the DAV RFC.  We could change this to
     some other URI scheme someday, if we wish. */
  *token = apr_pstrcat(pool, "opaquelocktoken:",
                       svn_uuid_generate(pool), (char *)NULL);
  return SVN_NO_ERROR;
}


svn_error_t *
svn_fs_fs__unlock(svn_fs_t *fs,
                  const char *path,
                  const char *token,
                  svn_boolean_t break_lock,
                  apr_pool_t *pool)
{
  struct unlock_baton ub;

  SVN_ERR(svn_fs__check_fs(fs, TRUE));
  path = svn_fs__canonicalize_abspath(path, pool);

  ub.fs = fs;
  ub.path = path;
  ub.token = token;
  ub.break_lock = break_lock;

  return svn_fs_fs__with_write_lock(fs, unlock_body, &ub, pool);
}


svn_error_t *
svn_fs_fs__get_lock(svn_lock_t **lock_p,
                    svn_fs_t *fs,
                    const char *path,
                    apr_pool_t *pool)
{
  SVN_ERR(svn_fs__check_fs(fs, TRUE));
  path = svn_fs__canonicalize_abspath(path, pool);
  return get_lock_helper(fs, lock_p, path, FALSE, pool);
}


/* Baton for get_locks_filter_func(). */
typedef struct get_locks_filter_baton_t
{
  const char *path;
  svn_depth_t requested_depth;
  svn_fs_get_locks_callback_t get_locks_func;
  void *get_locks_baton;

} get_locks_filter_baton_t;


/* A wrapper for the GET_LOCKS_FUNC passed to svn_fs_fs__get_locks()
   which filters out locks on paths that aren't within
   BATON->requested_depth of BATON->path before called
   BATON->get_locks_func() with BATON->get_locks_baton.

   NOTE: See issue #3660 for details about how the FSFS lock
   management code is inconsistent.  Until that inconsistency is
   resolved, we take this filtering approach rather than honoring
   depth requests closer to the crawling code.  In other words, once
   we decide how to resolve issue #3660, there might be a more
   performant way to honor the depth passed to svn_fs_fs__get_locks().  */
static svn_error_t *
get_locks_filter_func(void *baton,
                      svn_lock_t *lock,
                      apr_pool_t *pool)
{
  get_locks_filter_baton_t *b = baton;

  /* Filter out unwanted paths.  Since Subversion only allows
     locks on files, we can treat depth=immediates the same as
     depth=files for filtering purposes.  Meaning, we'll keep
     this lock if:

     a) its path is the very path we queried, or
     b) we've asked for a fully recursive answer, or
     c) we've asked for depth=files or depth=immediates, and this
        lock is on an immediate child of our query path.
  */
  if ((strcmp(b->path, lock->path) == 0)
      || (b->requested_depth == svn_depth_infinity))
    {
      SVN_ERR(b->get_locks_func(b->get_locks_baton, lock, pool));
    }
  else if ((b->requested_depth == svn_depth_files) ||
           (b->requested_depth == svn_depth_immediates))
    {
      const char *rel_uri = svn_fspath__skip_ancestor(b->path, lock->path);
      if (rel_uri && (svn_path_component_count(rel_uri) == 1))
        SVN_ERR(b->get_locks_func(b->get_locks_baton, lock, pool));
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__get_locks(svn_fs_t *fs,
                     const char *path,
                     svn_depth_t depth,
                     svn_fs_get_locks_callback_t get_locks_func,
                     void *get_locks_baton,
                     apr_pool_t *pool)
{
  const char *digest_path;
  get_locks_filter_baton_t glfb;

  SVN_ERR(svn_fs__check_fs(fs, TRUE));
  path = svn_fs__canonicalize_abspath(path, pool);

  glfb.path = path;
  glfb.requested_depth = depth;
  glfb.get_locks_func = get_locks_func;
  glfb.get_locks_baton = get_locks_baton;

  /* Get the top digest path in our tree of interest, and then walk it. */
  SVN_ERR(digest_path_from_path(&digest_path, fs->path, path, pool));
  SVN_ERR(walk_locks(fs, digest_path, get_locks_filter_func, &glfb,
                     FALSE, pool));
  return SVN_NO_ERROR;
}
