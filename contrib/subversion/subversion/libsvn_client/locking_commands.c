/*
 * locking_commands.c:  Implementation of lock and unlock.
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

/* ==================================================================== */



/*** Includes. ***/

#include "svn_client.h"
#include "svn_hash.h"
#include "client.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_xml.h"
#include "svn_pools.h"

#include "svn_private_config.h"
#include "private/svn_client_private.h"
#include "private/svn_wc_private.h"


/*** Code. ***/

/* For use with store_locks_callback, below. */
struct lock_baton
{
  const char *base_dir_abspath;
  apr_hash_t *urls_to_paths;
  svn_client_ctx_t *ctx;
  apr_pool_t *pool;
};


/* This callback is called by the ra_layer for each path locked.
 * BATON is a 'struct lock_baton *', PATH is the path being locked,
 * and LOCK is the lock itself.
 *
 * If BATON->base_dir_abspath is not null, then this function either
 * stores the LOCK on REL_URL or removes any lock tokens from REL_URL
 * (depending on whether DO_LOCK is true or false respectively), but
 * only if RA_ERR is null, or (in the unlock case) is something other
 * than SVN_ERR_FS_LOCK_OWNER_MISMATCH.
 *
 * Implements svn_ra_lock_callback_t.
 */
static svn_error_t *
store_locks_callback(void *baton,
                     const char *rel_url,
                     svn_boolean_t do_lock,
                     const svn_lock_t *lock,
                     svn_error_t *ra_err, apr_pool_t *pool)
{
  struct lock_baton *lb = baton;
  svn_wc_notify_t *notify;

  /* Create the notify struct first, so we can tweak it below. */
  notify = svn_wc_create_notify(rel_url,
                                do_lock
                                ? (ra_err
                                   ? svn_wc_notify_failed_lock
                                   : svn_wc_notify_locked)
                                : (ra_err
                                   ? svn_wc_notify_failed_unlock
                                   : svn_wc_notify_unlocked),
                                pool);
  notify->lock = lock;
  notify->err = ra_err;

  if (lb->base_dir_abspath)
    {
      char *path = svn_hash_gets(lb->urls_to_paths, rel_url);
      const char *local_abspath;

      local_abspath = svn_dirent_join(lb->base_dir_abspath, path, pool);

      /* Notify a valid working copy path */
      notify->path = local_abspath;
      notify->path_prefix = lb->base_dir_abspath;

      if (do_lock)
        {
          if (!ra_err)
            {
              SVN_ERR(svn_wc_add_lock2(lb->ctx->wc_ctx, local_abspath, lock,
                                       lb->pool));
              notify->lock_state = svn_wc_notify_lock_state_locked;
            }
          else
            notify->lock_state = svn_wc_notify_lock_state_unchanged;
        }
      else /* unlocking */
        {
          /* Remove our wc lock token either a) if we got no error, or b) if
             we got any error except for owner mismatch.  Note that the only
             errors that are handed to this callback will be locking-related
             errors. */

          if (!ra_err ||
              (ra_err && (ra_err->apr_err != SVN_ERR_FS_LOCK_OWNER_MISMATCH)))
            {
              SVN_ERR(svn_wc_remove_lock2(lb->ctx->wc_ctx, local_abspath,
                                          lb->pool));
              notify->lock_state = svn_wc_notify_lock_state_unlocked;
            }
          else
            notify->lock_state = svn_wc_notify_lock_state_unchanged;
        }
    }
  else
    notify->url = rel_url; /* Notify that path is actually a url  */

  if (lb->ctx->notify_func2)
    lb->ctx->notify_func2(lb->ctx->notify_baton2, notify, pool);

  return SVN_NO_ERROR;
}


/* This is a wrapper around svn_uri_condense_targets() and
 * svn_dirent_condense_targets() (the choice of which is made based on
 * the value of TARGETS_ARE_URIS) which takes care of the
 * single-target special case.
 *
 * Callers are expected to check for an empty *COMMON_PARENT (which
 * means, "there was nothing common") for themselves.
 */
static svn_error_t *
condense_targets(const char **common_parent,
                 apr_array_header_t **target_relpaths,
                 const apr_array_header_t *targets,
                 svn_boolean_t targets_are_uris,
                 svn_boolean_t remove_redundancies,
                 apr_pool_t *result_pool,
                 apr_pool_t *scratch_pool)
{
  if (targets_are_uris)
    {
      SVN_ERR(svn_uri_condense_targets(common_parent, target_relpaths,
                                       targets, remove_redundancies,
                                       result_pool, scratch_pool));
    }
  else
    {
      SVN_ERR(svn_dirent_condense_targets(common_parent, target_relpaths,
                                          targets, remove_redundancies,
                                          result_pool, scratch_pool));
    }

  /* svn_*_condense_targets leaves *TARGET_RELPATHS empty if TARGETS only
     had 1 member, so we special case that. */
  if (apr_is_empty_array(*target_relpaths))
    {
      const char *base_name;

      if (targets_are_uris)
        {
          svn_uri_split(common_parent, &base_name,
                        *common_parent, result_pool);
        }
      else
        {
          svn_dirent_split(common_parent, &base_name,
                           *common_parent, result_pool);
        }
      APR_ARRAY_PUSH(*target_relpaths, const char *) = base_name;
    }

  return SVN_NO_ERROR;
}

/* Lock info. Used in organize_lock_targets.
   ### Maybe return this instead of the ugly hashes? */
struct wc_lock_item_t
{
  svn_revnum_t revision;
  const char *lock_token;
};

/* Set *COMMON_PARENT_URL to the nearest common parent URL of all TARGETS.
 * If TARGETS are local paths, then the entry for each path is examined
 * and *COMMON_PARENT is set to the common parent URL for all the
 * targets (as opposed to the common local path).
 *
 * If there is no common parent, either because the targets are a
 * mixture of URLs and local paths, or because they simply do not
 * share a common parent, then return SVN_ERR_UNSUPPORTED_FEATURE.
 *
 * DO_LOCK is TRUE for locking TARGETS, and FALSE for unlocking them.
 * FORCE is TRUE for breaking or stealing locks, and FALSE otherwise.
 *
 * Each key stored in *REL_TARGETS_P is a path relative to
 * *COMMON_PARENT.  If TARGETS are local paths, then: if DO_LOCK is
 * true, the value is a pointer to the corresponding base_revision
 * (allocated in POOL) for the path, else the value is the lock token
 * (or "" if no token found in the wc).
 *
 * If TARGETS is an array of urls, REL_FS_PATHS_P is set to NULL.
 * Otherwise each key in REL_FS_PATHS_P is an repository path (relative to
 * COMMON_PARENT) mapped to the target path for TARGET (relative to
 * the common parent WC path). working copy targets that they "belong" to.
 *
 * If *COMMON_PARENT is a URL, then the values are a pointer to
 * SVN_INVALID_REVNUM (allocated in pool) if DO_LOCK, else "".
 *
 * TARGETS may not be empty.
 */
static svn_error_t *
organize_lock_targets(const char **common_parent_url,
                      const char **base_dir,
                      apr_hash_t **rel_targets_p,
                      apr_hash_t **rel_fs_paths_p,
                      const apr_array_header_t *targets,
                      svn_boolean_t do_lock,
                      svn_boolean_t force,
                      svn_wc_context_t *wc_ctx,
                      apr_pool_t *result_pool,
                      apr_pool_t *scratch_pool)
{
  const char *common_url = NULL;
  const char *common_dirent = NULL;
  apr_hash_t *rel_targets_ret = apr_hash_make(result_pool);
  apr_hash_t *rel_fs_paths = NULL;
  apr_array_header_t *rel_targets;
  apr_hash_t *wc_info = apr_hash_make(scratch_pool);
  svn_boolean_t url_mode;
  int i;

  SVN_ERR_ASSERT(targets->nelts);
  SVN_ERR(svn_client__assert_homogeneous_target_type(targets));

  url_mode = svn_path_is_url(APR_ARRAY_IDX(targets, 0, const char *));

  if (url_mode)
    {
      svn_revnum_t *invalid_revnum =
        apr_palloc(result_pool, sizeof(*invalid_revnum));

      *invalid_revnum = SVN_INVALID_REVNUM;

      /* Get the common parent URL and a bunch of relpaths, one per target. */
      SVN_ERR(condense_targets(&common_url, &rel_targets, targets,
                               TRUE, TRUE, result_pool, scratch_pool));
      if (! (common_url && *common_url))
        return svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                                _("No common parent found, unable to operate "
                                  "on disjoint arguments"));

      /* Create mapping of the target relpaths to either
         SVN_INVALID_REVNUM (if our caller is locking) or to an empty
         lock token string (if the caller is unlocking). */
      for (i = 0; i < rel_targets->nelts; i++)
        {
          svn_hash_sets(rel_targets_ret,
                        APR_ARRAY_IDX(rel_targets, i, const char *),
                        do_lock
                        ? (const void *)invalid_revnum
                        : (const void *)"");
        }
    }
  else
    {
      apr_array_header_t *rel_urls, *target_urls;
      apr_pool_t *iterpool = svn_pool_create(scratch_pool);

      /* Get the common parent dirent and a bunch of relpaths, one per
         target. */
      SVN_ERR(condense_targets(&common_dirent, &rel_targets, targets,
                               FALSE, TRUE, result_pool, scratch_pool));
      if (! (common_dirent && *common_dirent))
        return svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                                _("No common parent found, unable to operate "
                                  "on disjoint arguments"));

      /* Get the URL for each target (which also serves to verify that
         the dirent targets are sane).  */
      target_urls = apr_array_make(scratch_pool, rel_targets->nelts,
                                   sizeof(const char *));
      for (i = 0; i < rel_targets->nelts; i++)
        {
          const char *rel_target;
          const char *repos_relpath;
          const char *repos_root_url;
          const char *target_url;
          struct wc_lock_item_t *wli;
          const char *local_abspath;
          svn_node_kind_t kind;

          svn_pool_clear(iterpool);

          rel_target = APR_ARRAY_IDX(rel_targets, i, const char *);
          local_abspath = svn_dirent_join(common_dirent, rel_target, scratch_pool);
          wli = apr_pcalloc(scratch_pool, sizeof(*wli));

          SVN_ERR(svn_wc__node_get_base(&kind, &wli->revision, &repos_relpath,
                                        &repos_root_url, NULL,
                                        &wli->lock_token,
                                        wc_ctx, local_abspath,
                                        FALSE /* ignore_enoent */,
                                        FALSE /* show_hidden */,
                                        result_pool, iterpool));

          if (kind != svn_node_file)
            return svn_error_createf(SVN_ERR_WC_NOT_FILE, NULL,
                                     _("The node '%s' is not a file"),
                                     svn_dirent_local_style(local_abspath,
                                                            iterpool));

          svn_hash_sets(wc_info, local_abspath, wli);

          target_url = svn_path_url_add_component2(repos_root_url,
                                                   repos_relpath,
                                                   scratch_pool);

          APR_ARRAY_PUSH(target_urls, const char *) = target_url;
        }

      /* Now that we have a bunch of URLs for our dirent targets,
         condense those into a single common parent URL and a bunch of
         paths relative to that. */
      SVN_ERR(condense_targets(&common_url, &rel_urls, target_urls,
                               TRUE, FALSE, result_pool, scratch_pool));
      if (! (common_url && *common_url))
        return svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                                _("Unable to lock/unlock across multiple "
                                  "repositories"));

      /* Now we need to create a couple of different hash mappings. */
      rel_fs_paths = apr_hash_make(result_pool);
      for (i = 0; i < rel_targets->nelts; i++)
        {
          const char *rel_target, *rel_url;
          const char *local_abspath;

          svn_pool_clear(iterpool);

          /* First, we need to map our REL_URL (which is relative to
             COMMON_URL) to our REL_TARGET (which is relative to
             COMMON_DIRENT). */
          rel_target = APR_ARRAY_IDX(rel_targets, i, const char *);
          rel_url = APR_ARRAY_IDX(rel_urls, i, const char *);
          svn_hash_sets(rel_fs_paths, rel_url,
                        apr_pstrdup(result_pool, rel_target));

          /* Then, we map our REL_URL (again) to either the base
             revision of the dirent target with which it is associated
             (if our caller is locking) or to a (possible empty) lock
             token string (if the caller is unlocking). */
          local_abspath = svn_dirent_join(common_dirent, rel_target, iterpool);

          if (do_lock) /* Lock. */
            {
              svn_revnum_t *revnum;
              struct wc_lock_item_t *wli;
              revnum = apr_palloc(result_pool, sizeof(* revnum));

              wli = svn_hash_gets(wc_info, local_abspath);

              SVN_ERR_ASSERT(wli != NULL);

              *revnum = wli->revision;

              svn_hash_sets(rel_targets_ret, rel_url, revnum);
            }
          else /* Unlock. */
            {
              const char *lock_token;
              struct wc_lock_item_t *wli;

              /* If not forcing the unlock, get the lock token. */
              if (! force)
                {
                  wli = svn_hash_gets(wc_info, local_abspath);

                  SVN_ERR_ASSERT(wli != NULL);

                  if (! wli->lock_token)
                    return svn_error_createf(
                               SVN_ERR_CLIENT_MISSING_LOCK_TOKEN, NULL,
                               _("'%s' is not locked in this working copy"),
                               svn_dirent_local_style(local_abspath,
                                                      scratch_pool));

                  lock_token = wli->lock_token
                                ? apr_pstrdup(result_pool, wli->lock_token)
                                : NULL;
                }
              else
                lock_token = NULL;

              /* If breaking a lock, we shouldn't pass any lock token. */
              svn_hash_sets(rel_targets_ret, rel_url,
                            lock_token ? lock_token : "");
            }
        }

      svn_pool_destroy(iterpool);
    }

  /* Set our return variables. */
  *common_parent_url = common_url;
  *base_dir = common_dirent;
  *rel_targets_p = rel_targets_ret;
  *rel_fs_paths_p = rel_fs_paths;

  return SVN_NO_ERROR;
}

/* Fetch lock tokens from the repository for the paths in PATH_TOKENS,
   setting the values to the fetched tokens, allocated in pool. */
static svn_error_t *
fetch_tokens(svn_ra_session_t *ra_session, apr_hash_t *path_tokens,
             apr_pool_t *pool)
{
  apr_hash_index_t *hi;
  apr_pool_t *iterpool = svn_pool_create(pool);

  for (hi = apr_hash_first(pool, path_tokens); hi; hi = apr_hash_next(hi))
    {
      const char *path = svn__apr_hash_index_key(hi);
      svn_lock_t *lock;

      svn_pool_clear(iterpool);

      SVN_ERR(svn_ra_get_lock(ra_session, &lock, path, iterpool));

      if (! lock)
        return svn_error_createf
          (SVN_ERR_CLIENT_MISSING_LOCK_TOKEN, NULL,
           _("'%s' is not locked"), path);

      svn_hash_sets(path_tokens, path, apr_pstrdup(pool, lock->token));
    }

  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}


svn_error_t *
svn_client_lock(const apr_array_header_t *targets,
                const char *comment,
                svn_boolean_t steal_lock,
                svn_client_ctx_t *ctx,
                apr_pool_t *pool)
{
  const char *base_dir;
  const char *base_dir_abspath = NULL;
  const char *common_parent_url;
  svn_ra_session_t *ra_session;
  apr_hash_t *path_revs, *urls_to_paths;
  struct lock_baton cb;

  if (apr_is_empty_array(targets))
    return SVN_NO_ERROR;

  /* Enforce that the comment be xml-escapable. */
  if (comment)
    {
      if (! svn_xml_is_xml_safe(comment, strlen(comment)))
        return svn_error_create
          (SVN_ERR_XML_UNESCAPABLE_DATA, NULL,
           _("Lock comment contains illegal characters"));
    }

  SVN_ERR(organize_lock_targets(&common_parent_url, &base_dir, &path_revs,
                                &urls_to_paths, targets, TRUE, steal_lock,
                                ctx->wc_ctx, pool, pool));

  /* Open an RA session to the common parent of TARGETS. */
  if (base_dir)
    SVN_ERR(svn_dirent_get_absolute(&base_dir_abspath, base_dir, pool));
  SVN_ERR(svn_client_open_ra_session2(&ra_session, common_parent_url,
                                      base_dir_abspath, ctx, pool, pool));

  cb.base_dir_abspath = base_dir_abspath;
  cb.urls_to_paths = urls_to_paths;
  cb.ctx = ctx;
  cb.pool = pool;

  /* Lock the paths. */
  SVN_ERR(svn_ra_lock(ra_session, path_revs, comment,
                      steal_lock, store_locks_callback, &cb, pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_client_unlock(const apr_array_header_t *targets,
                  svn_boolean_t break_lock,
                  svn_client_ctx_t *ctx,
                  apr_pool_t *pool)
{
  const char *base_dir;
  const char *base_dir_abspath = NULL;
  const char *common_parent_url;
  svn_ra_session_t *ra_session;
  apr_hash_t *path_tokens, *urls_to_paths;
  struct lock_baton cb;

  if (apr_is_empty_array(targets))
    return SVN_NO_ERROR;

  SVN_ERR(organize_lock_targets(&common_parent_url, &base_dir, &path_tokens,
                                &urls_to_paths, targets, FALSE, break_lock,
                                ctx->wc_ctx, pool, pool));

  /* Open an RA session. */
  if (base_dir)
    SVN_ERR(svn_dirent_get_absolute(&base_dir_abspath, base_dir, pool));
  SVN_ERR(svn_client_open_ra_session2(&ra_session, common_parent_url,
                                      base_dir_abspath, ctx, pool, pool));

  /* If break_lock is not set, lock tokens are required by the server.
     If the targets were all URLs, ensure that we provide lock tokens,
     so the repository will only check that the user owns the
     locks. */
  if (! base_dir && !break_lock)
    SVN_ERR(fetch_tokens(ra_session, path_tokens, pool));

  cb.base_dir_abspath = base_dir_abspath;
  cb.urls_to_paths = urls_to_paths;
  cb.ctx = ctx;
  cb.pool = pool;

  /* Unlock the paths. */
  SVN_ERR(svn_ra_unlock(ra_session, path_tokens, break_lock,
                        store_locks_callback, &cb, pool));

  return SVN_NO_ERROR;
}

