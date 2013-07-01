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


#include "svn_hash.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_fs.h"
#include "svn_private_config.h"

#include <apr_uuid.h>

#include "lock.h"
#include "tree.h"
#include "err.h"
#include "bdb/locks-table.h"
#include "bdb/lock-tokens-table.h"
#include "util/fs_skels.h"
#include "../libsvn_fs/fs-loader.h"
#include "private/svn_fs_util.h"
#include "private/svn_subr_private.h"
#include "private/svn_dep_compat.h"


/* Add LOCK and its associated LOCK_TOKEN (associated with PATH) as
   part of TRAIL. */
static svn_error_t *
add_lock_and_token(svn_lock_t *lock,
                   const char *lock_token,
                   const char *path,
                   trail_t *trail)
{
  SVN_ERR(svn_fs_bdb__lock_add(trail->fs, lock_token, lock,
                               trail, trail->pool));
  return svn_fs_bdb__lock_token_add(trail->fs, path, lock_token,
                                    trail, trail->pool);
}


/* Delete LOCK_TOKEN and its corresponding lock (associated with PATH,
   whose KIND is supplied), as part of TRAIL. */
static svn_error_t *
delete_lock_and_token(const char *lock_token,
                      const char *path,
                      trail_t *trail)
{
  SVN_ERR(svn_fs_bdb__lock_delete(trail->fs, lock_token,
                                  trail, trail->pool));
  return svn_fs_bdb__lock_token_delete(trail->fs, path,
                                       trail, trail->pool);
}


struct lock_args
{
  svn_lock_t **lock_p;
  const char *path;
  const char *token;
  const char *comment;
  svn_boolean_t is_dav_comment;
  svn_boolean_t steal_lock;
  apr_time_t expiration_date;
  svn_revnum_t current_rev;
};


static svn_error_t *
txn_body_lock(void *baton, trail_t *trail)
{
  struct lock_args *args = baton;
  svn_node_kind_t kind = svn_node_file;
  svn_lock_t *existing_lock;
  svn_lock_t *lock;

  SVN_ERR(svn_fs_base__get_path_kind(&kind, args->path, trail, trail->pool));

  /* Until we implement directory locks someday, we only allow locks
     on files or non-existent paths. */
  if (kind == svn_node_dir)
    return SVN_FS__ERR_NOT_FILE(trail->fs, args->path);

  /* While our locking implementation easily supports the locking of
     nonexistent paths, we deliberately choose not to allow such madness. */
  if (kind == svn_node_none)
    {
      if (SVN_IS_VALID_REVNUM(args->current_rev))
        return svn_error_createf(
          SVN_ERR_FS_OUT_OF_DATE, NULL,
          _("Path '%s' doesn't exist in HEAD revision"),
          args->path);
      else
        return svn_error_createf(
          SVN_ERR_FS_NOT_FOUND, NULL,
          _("Path '%s' doesn't exist in HEAD revision"),
          args->path);
    }

  /* There better be a username attached to the fs. */
  if (!trail->fs->access_ctx || !trail->fs->access_ctx->username)
    return SVN_FS__ERR_NO_USER(trail->fs);

  /* Is the caller attempting to lock an out-of-date working file? */
  if (SVN_IS_VALID_REVNUM(args->current_rev))
    {
      svn_revnum_t created_rev;
      SVN_ERR(svn_fs_base__get_path_created_rev(&created_rev, args->path,
                                                trail, trail->pool));

      /* SVN_INVALID_REVNUM means the path doesn't exist.  So
         apparently somebody is trying to lock something in their
         working copy, but somebody else has deleted the thing
         from HEAD.  That counts as being 'out of date'. */
      if (! SVN_IS_VALID_REVNUM(created_rev))
        return svn_error_createf(SVN_ERR_FS_OUT_OF_DATE, NULL,
                                 "Path '%s' doesn't exist in HEAD revision",
                                 args->path);

      if (args->current_rev < created_rev)
        return svn_error_createf(SVN_ERR_FS_OUT_OF_DATE, NULL,
                                 "Lock failed: newer version of '%s' exists",
                                 args->path);
    }

  /* If the caller provided a TOKEN, we *really* need to see
     if a lock already exists with that token, and if so, verify that
     the lock's path matches PATH.  Otherwise we run the risk of
     breaking the 1-to-1 mapping of lock tokens to locked paths. */
  if (args->token)
    {
      svn_lock_t *lock_from_token;
      svn_error_t *err = svn_fs_bdb__lock_get(&lock_from_token, trail->fs,
                                              args->token, trail,
                                              trail->pool);
      if (err && ((err->apr_err == SVN_ERR_FS_LOCK_EXPIRED)
                  || (err->apr_err == SVN_ERR_FS_BAD_LOCK_TOKEN)))
        {
          svn_error_clear(err);
        }
      else
        {
          SVN_ERR(err);
          if (strcmp(lock_from_token->path, args->path) != 0)
            return svn_error_create(SVN_ERR_FS_BAD_LOCK_TOKEN, NULL,
                                    "Lock failed: token refers to existing "
                                    "lock with non-matching path.");
        }
    }

  /* Is the path already locked?

     Note that this next function call will automatically ignore any
     errors about {the path not existing as a key, the path's token
     not existing as a key, the lock just having been expired}.  And
     that's totally fine.  Any of these three errors are perfectly
     acceptable to ignore; it means that the path is now free and
     clear for locking, because the bdb funcs just cleared out both
     of the tables for us.   */
  SVN_ERR(svn_fs_base__get_lock_helper(&existing_lock, args->path,
                                       trail, trail->pool));
  if (existing_lock)
    {
      if (! args->steal_lock)
        {
          /* Sorry, the path is already locked. */
          return SVN_FS__ERR_PATH_ALREADY_LOCKED(trail->fs,
                                                 existing_lock);
        }
      else
        {
          /* ARGS->steal_lock is set, so fs_username is "stealing" the
             lock from lock->owner.  Destroy the existing lock. */
          SVN_ERR(delete_lock_and_token(existing_lock->token,
                                        existing_lock->path, trail));
        }
    }

  /* Create a new lock, and add it to the tables. */
  lock = svn_lock_create(trail->pool);
  if (args->token)
    lock->token = apr_pstrdup(trail->pool, args->token);
  else
    SVN_ERR(svn_fs_base__generate_lock_token(&(lock->token), trail->fs,
                                             trail->pool));
  lock->path = apr_pstrdup(trail->pool, args->path);
  lock->owner = apr_pstrdup(trail->pool, trail->fs->access_ctx->username);
  lock->comment = apr_pstrdup(trail->pool, args->comment);
  lock->is_dav_comment = args->is_dav_comment;
  lock->creation_date = apr_time_now();
  lock->expiration_date = args->expiration_date;
  SVN_ERR(add_lock_and_token(lock, lock->token, args->path, trail));
  *(args->lock_p) = lock;

  return SVN_NO_ERROR;
}



svn_error_t *
svn_fs_base__lock(svn_lock_t **lock,
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
  struct lock_args args;

  SVN_ERR(svn_fs__check_fs(fs, TRUE));

  args.lock_p = lock;
  args.path = svn_fs__canonicalize_abspath(path, pool);
  args.token = token;
  args.comment = comment;
  args.is_dav_comment = is_dav_comment;
  args.steal_lock = steal_lock;
  args.expiration_date = expiration_date;
  args.current_rev = current_rev;

  return svn_fs_base__retry_txn(fs, txn_body_lock, &args, FALSE, pool);
}


svn_error_t *
svn_fs_base__generate_lock_token(const char **token,
                                 svn_fs_t *fs,
                                 apr_pool_t *pool)
{
  /* Notice that 'fs' is currently unused.  But perhaps someday, we'll
     want to use the fs UUID + some incremented number?  For now, we
     generate a URI that matches the DAV RFC.  We could change this to
     some other URI scheme someday, if we wish. */
  *token = apr_pstrcat(pool, "opaquelocktoken:",
                       svn_uuid_generate(pool), (char *)NULL);
  return SVN_NO_ERROR;
}


struct unlock_args
{
  const char *path;
  const char *token;
  svn_boolean_t break_lock;
};


static svn_error_t *
txn_body_unlock(void *baton, trail_t *trail)
{
  struct unlock_args *args = baton;
  const char *lock_token;
  svn_lock_t *lock;

  /* This could return SVN_ERR_FS_BAD_LOCK_TOKEN or SVN_ERR_FS_LOCK_EXPIRED. */
  SVN_ERR(svn_fs_bdb__lock_token_get(&lock_token, trail->fs, args->path,
                                     trail, trail->pool));

  /* If not breaking the lock, we need to do some more checking. */
  if (!args->break_lock)
    {
      /* Sanity check: The lock token must exist, and must match. */
      if (args->token == NULL)
        return svn_fs_base__err_no_lock_token(trail->fs, args->path);
      else if (strcmp(lock_token, args->token) != 0)
        return SVN_FS__ERR_NO_SUCH_LOCK(trail->fs, args->path);

      SVN_ERR(svn_fs_bdb__lock_get(&lock, trail->fs, lock_token,
                                   trail, trail->pool));

      /* There better be a username attached to the fs. */
      if (!trail->fs->access_ctx || !trail->fs->access_ctx->username)
        return SVN_FS__ERR_NO_USER(trail->fs);

      /* And that username better be the same as the lock's owner. */
      if (strcmp(trail->fs->access_ctx->username, lock->owner) != 0)
        return SVN_FS__ERR_LOCK_OWNER_MISMATCH(
           trail->fs,
           trail->fs->access_ctx->username,
           lock->owner);
    }

  /* Remove a row from each of the locking tables. */
  return delete_lock_and_token(lock_token, args->path, trail);
}


svn_error_t *
svn_fs_base__unlock(svn_fs_t *fs,
                    const char *path,
                    const char *token,
                    svn_boolean_t break_lock,
                    apr_pool_t *pool)
{
  struct unlock_args args;

  SVN_ERR(svn_fs__check_fs(fs, TRUE));

  args.path = svn_fs__canonicalize_abspath(path, pool);
  args.token = token;
  args.break_lock = break_lock;
  return svn_fs_base__retry_txn(fs, txn_body_unlock, &args, TRUE, pool);
}


svn_error_t *
svn_fs_base__get_lock_helper(svn_lock_t **lock_p,
                             const char *path,
                             trail_t *trail,
                             apr_pool_t *pool)
{
  const char *lock_token;
  svn_error_t *err;

  err = svn_fs_bdb__lock_token_get(&lock_token, trail->fs, path,
                                   trail, pool);

  /* We've deliberately decided that this function doesn't tell the
     caller *why* the lock is unavailable.  */
  if (err && ((err->apr_err == SVN_ERR_FS_NO_SUCH_LOCK)
              || (err->apr_err == SVN_ERR_FS_LOCK_EXPIRED)
              || (err->apr_err == SVN_ERR_FS_BAD_LOCK_TOKEN)))
    {
      svn_error_clear(err);
      *lock_p = NULL;
      return SVN_NO_ERROR;
    }
  else
    SVN_ERR(err);

  /* Same situation here.  */
  err = svn_fs_bdb__lock_get(lock_p, trail->fs, lock_token, trail, pool);
  if (err && ((err->apr_err == SVN_ERR_FS_LOCK_EXPIRED)
              || (err->apr_err == SVN_ERR_FS_BAD_LOCK_TOKEN)))
    {
      svn_error_clear(err);
      *lock_p = NULL;
      return SVN_NO_ERROR;
    }
  else
    SVN_ERR(err);

  return svn_error_trace(err);
}


struct lock_token_get_args
{
  svn_lock_t **lock_p;
  const char *path;
};


static svn_error_t *
txn_body_get_lock(void *baton, trail_t *trail)
{
  struct lock_token_get_args *args = baton;
  return svn_fs_base__get_lock_helper(args->lock_p, args->path,
                                      trail, trail->pool);
}


svn_error_t *
svn_fs_base__get_lock(svn_lock_t **lock,
                      svn_fs_t *fs,
                      const char *path,
                      apr_pool_t *pool)
{
  struct lock_token_get_args args;

  SVN_ERR(svn_fs__check_fs(fs, TRUE));

  args.path = svn_fs__canonicalize_abspath(path, pool);
  args.lock_p = lock;
  return svn_fs_base__retry_txn(fs, txn_body_get_lock, &args, FALSE, pool);
}

/* Implements `svn_fs_get_locks_callback_t', spooling lock information
   to a stream as the filesystem provides it.  BATON is an 'svn_stream_t *'
   object pointing to the stream.  We'll write the spool stream with a
   format like so:

      SKEL1_LEN "\n" SKEL1 "\n" SKEL2_LEN "\n" SKEL2 "\n" ...

   where each skel is a lock skel (the same format we use to store
   locks in the `locks' table). */
static svn_error_t *
spool_locks_info(void *baton,
                 svn_lock_t *lock,
                 apr_pool_t *pool)
{
  svn_skel_t *lock_skel;
  svn_stream_t *stream = baton;
  const char *skel_len;
  svn_stringbuf_t *skel_buf;
  apr_size_t len;

  SVN_ERR(svn_fs_base__unparse_lock_skel(&lock_skel, lock, pool));
  skel_buf = svn_skel__unparse(lock_skel, pool);
  skel_len = apr_psprintf(pool, "%" APR_SIZE_T_FMT "\n", skel_buf->len);
  len = strlen(skel_len);
  SVN_ERR(svn_stream_write(stream, skel_len, &len));
  len = skel_buf->len;
  SVN_ERR(svn_stream_write(stream, skel_buf->data, &len));
  len = 1;
  return svn_stream_write(stream, "\n", &len);
}


struct locks_get_args
{
  const char *path;
  svn_depth_t depth;
  svn_stream_t *stream;
};


static svn_error_t *
txn_body_get_locks(void *baton, trail_t *trail)
{
  struct locks_get_args *args = baton;
  return svn_fs_bdb__locks_get(trail->fs, args->path, args->depth,
                               spool_locks_info, args->stream,
                               trail, trail->pool);
}


svn_error_t *
svn_fs_base__get_locks(svn_fs_t *fs,
                       const char *path,
                       svn_depth_t depth,
                       svn_fs_get_locks_callback_t get_locks_func,
                       void *get_locks_baton,
                       apr_pool_t *pool)
{
  struct locks_get_args args;
  svn_stream_t *stream;
  svn_stringbuf_t *buf;
  svn_boolean_t eof;
  apr_pool_t *iterpool = svn_pool_create(pool);

  SVN_ERR(svn_fs__check_fs(fs, TRUE));

  args.path = svn_fs__canonicalize_abspath(path, pool);
  args.depth = depth;
  /* Enough for 100+ locks if the comments are small. */
  args.stream = svn_stream__from_spillbuf(4 * 1024  /* blocksize */,
                                          64 * 1024 /* maxsize */,
                                          pool);
  SVN_ERR(svn_fs_base__retry_txn(fs, txn_body_get_locks, &args, FALSE, pool));

  /* Read the stream calling GET_LOCKS_FUNC(). */
  stream = args.stream;

  while (1)
    {
      apr_size_t len, skel_len;
      char c, *skel_buf;
      svn_skel_t *lock_skel;
      svn_lock_t *lock;
      apr_uint64_t ui64;
      svn_error_t *err;

      svn_pool_clear(iterpool);

      /* Read a skel length line and parse it for the skel's length.  */
      SVN_ERR(svn_stream_readline(stream, &buf, "\n", &eof, iterpool));
      if (eof)
        break;
      err = svn_cstring_strtoui64(&ui64, buf->data, 0, APR_SIZE_MAX, 10);
      if (err)
        return svn_error_create(SVN_ERR_MALFORMED_FILE, err, NULL);
      skel_len = (apr_size_t)ui64;

      /* Now read that much into a buffer. */
      skel_buf = apr_palloc(pool, skel_len + 1);
      SVN_ERR(svn_stream_read(stream, skel_buf, &skel_len));
      skel_buf[skel_len] = '\0';

      /* Read the extra newline that follows the skel. */
      len = 1;
      SVN_ERR(svn_stream_read(stream, &c, &len));
      if (c != '\n')
        return svn_error_create(SVN_ERR_MALFORMED_FILE, NULL, NULL);

      /* Parse the skel into a lock, and notify the caller. */
      lock_skel = svn_skel__parse(skel_buf, skel_len, iterpool);
      SVN_ERR(svn_fs_base__parse_lock_skel(&lock, lock_skel, iterpool));
      SVN_ERR(get_locks_func(get_locks_baton, lock, iterpool));
    }

  SVN_ERR(svn_stream_close(stream));
  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}



/* Utility function:  verify that a lock can be used.

   If no username is attached to the FS, return SVN_ERR_FS_NO_USER.

   If the FS username doesn't match LOCK's owner, return
   SVN_ERR_FS_LOCK_OWNER_MISMATCH.

   If FS hasn't been supplied with a matching lock-token for LOCK,
   return SVN_ERR_FS_BAD_LOCK_TOKEN.

   Otherwise return SVN_NO_ERROR.
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


/* The main routine for lock enforcement, used throughout libsvn_fs_base. */
svn_error_t *
svn_fs_base__allow_locked_operation(const char *path,
                                    svn_boolean_t recurse,
                                    trail_t *trail,
                                    apr_pool_t *pool)
{
  if (recurse)
    {
      /* Discover all locks at or below the path. */
      SVN_ERR(svn_fs_bdb__locks_get(trail->fs, path, svn_depth_infinity,
                                    get_locks_callback,
                                    trail->fs, trail, pool));
    }
  else
    {
      svn_lock_t *lock;

      /* Discover any lock attached to the path. */
      SVN_ERR(svn_fs_base__get_lock_helper(&lock, path, trail, pool));
      if (lock)
        SVN_ERR(verify_lock(trail->fs, lock, pool));
    }
  return SVN_NO_ERROR;
}
