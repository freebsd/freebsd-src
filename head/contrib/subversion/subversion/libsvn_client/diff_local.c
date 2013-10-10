/*
 * diff_local.c: comparing local trees with each other
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

#include <apr_strings.h>
#include <apr_pools.h>
#include <apr_hash.h>
#include "svn_hash.h"
#include "svn_types.h"
#include "svn_wc.h"
#include "svn_diff.h"
#include "svn_client.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_dirent_uri.h"
#include "svn_io.h"
#include "svn_pools.h"
#include "svn_props.h"
#include "svn_sorts.h"
#include "svn_subst.h"
#include "client.h"

#include "private/svn_wc_private.h"

#include "svn_private_config.h"


/* Try to get properties for LOCAL_ABSPATH and return them in the property
 * hash *PROPS. If there are no properties because LOCAL_ABSPATH is not
 * versioned, return an empty property hash. */
static svn_error_t *
get_props(apr_hash_t **props,
          const char *local_abspath,
          svn_wc_context_t *wc_ctx,
          apr_pool_t *result_pool,
          apr_pool_t *scratch_pool)
{
  svn_error_t *err;

  err = svn_wc_prop_list2(props, wc_ctx, local_abspath, result_pool,
                          scratch_pool);
  if (err)
    {
      if (err->apr_err == SVN_ERR_WC_PATH_NOT_FOUND ||
          err->apr_err == SVN_ERR_WC_NOT_WORKING_COPY)
        {
          svn_error_clear(err);
          *props = apr_hash_make(result_pool);

          /* ### Apply autoprops, like 'svn add' would? */
        }
      else
        return svn_error_trace(err);
    }

  return SVN_NO_ERROR;
}

/* Produce a diff between two arbitrary files at LOCAL_ABSPATH1 and
 * LOCAL_ABSPATH2, using the diff callbacks from CALLBACKS.
 * Use PATH as the name passed to diff callbacks.
 * FILE1_IS_EMPTY and FILE2_IS_EMPTY are used as hints which diff callback
 * function to use to compare the files (added/deleted/changed).
 *
 * If ORIGINAL_PROPS_OVERRIDE is not NULL, use it as original properties
 * instead of reading properties from LOCAL_ABSPATH1. This is required when
 * a file replaces a directory, where LOCAL_ABSPATH1 is an empty file that
 * file content must be diffed against, but properties to diff against come
 * from the replaced directory. */
static svn_error_t *
do_arbitrary_files_diff(const char *local_abspath1,
                        const char *local_abspath2,
                        const char *path,
                        svn_boolean_t file1_is_empty,
                        svn_boolean_t file2_is_empty,
                        apr_hash_t *original_props_override,
                        const svn_wc_diff_callbacks4_t *callbacks,
                        void *diff_baton,
                        svn_client_ctx_t *ctx,
                        apr_pool_t *scratch_pool)
{
  apr_hash_t *original_props;
  apr_hash_t *modified_props;
  apr_array_header_t *prop_changes;
  svn_string_t *original_mime_type = NULL;
  svn_string_t *modified_mime_type = NULL;

  if (ctx->cancel_func)
    SVN_ERR(ctx->cancel_func(ctx->cancel_baton));

  /* Try to get properties from either file. It's OK if the files do not
   * have properties, or if they are unversioned. */
  if (original_props_override)
    original_props = original_props_override;
  else
    SVN_ERR(get_props(&original_props, local_abspath1, ctx->wc_ctx,
                      scratch_pool, scratch_pool));
  SVN_ERR(get_props(&modified_props, local_abspath2, ctx->wc_ctx,
                    scratch_pool, scratch_pool));

  SVN_ERR(svn_prop_diffs(&prop_changes, modified_props, original_props,
                         scratch_pool));

  /* Try to determine the mime-type of each file. */
  original_mime_type = svn_hash_gets(original_props, SVN_PROP_MIME_TYPE);
  if (!file1_is_empty && !original_mime_type)
    {
      const char *mime_type;
      SVN_ERR(svn_io_detect_mimetype2(&mime_type, local_abspath1,
                                      ctx->mimetypes_map, scratch_pool));

      if (mime_type)
        original_mime_type = svn_string_create(mime_type, scratch_pool);
    }

  modified_mime_type = svn_hash_gets(modified_props, SVN_PROP_MIME_TYPE);
  if (!file2_is_empty && !modified_mime_type)
    {
      const char *mime_type;
      SVN_ERR(svn_io_detect_mimetype2(&mime_type, local_abspath1,
                                      ctx->mimetypes_map, scratch_pool));

      if (mime_type)
        modified_mime_type = svn_string_create(mime_type, scratch_pool);
    }

  /* Produce the diff. */
  if (file1_is_empty && !file2_is_empty)
    SVN_ERR(callbacks->file_added(NULL, NULL, NULL, path,
                                  local_abspath1, local_abspath2,
                                  /* ### TODO get real revision info
                                   * for versioned files? */
                                  SVN_INVALID_REVNUM, SVN_INVALID_REVNUM,
                                  original_mime_type ?
                                    original_mime_type->data : NULL,
                                  modified_mime_type ?
                                    modified_mime_type->data : NULL,
                                  /* ### TODO get copyfrom? */
                                  NULL, SVN_INVALID_REVNUM,
                                  prop_changes, original_props,
                                  diff_baton, scratch_pool));
  else if (!file1_is_empty && file2_is_empty)
    SVN_ERR(callbacks->file_deleted(NULL, NULL, path,
                                    local_abspath1, local_abspath2,
                                    original_mime_type ?
                                      original_mime_type->data : NULL,
                                    modified_mime_type ?
                                      modified_mime_type->data : NULL,
                                    original_props,
                                    diff_baton, scratch_pool));
  else
    {
      svn_stream_t *file1;
      svn_stream_t *file2;
      svn_boolean_t same;
      svn_string_t *val;
      /* We have two files, which may or may not be the same.

         ### Our caller assumes that we should ignore symlinks here and
             handle them as normal paths. Perhaps that should change?
      */
      SVN_ERR(svn_stream_open_readonly(&file1, local_abspath1, scratch_pool,
                                       scratch_pool));

      SVN_ERR(svn_stream_open_readonly(&file2, local_abspath2, scratch_pool,
                                       scratch_pool));

      /* Wrap with normalization, etc. if necessary */
      if (original_props)
        {
          val = svn_hash_gets(original_props, SVN_PROP_EOL_STYLE);

          if (val)
            {
              svn_subst_eol_style_t style;
              const char *eol;
              svn_subst_eol_style_from_value(&style, &eol, val->data);

              /* ### Ignoring keywords */
              if (eol)
                file1 = svn_subst_stream_translated(file1, eol, TRUE,
                                                    NULL, FALSE,
                                                    scratch_pool);
            }
        }

      if (modified_props)
        {
          val = svn_hash_gets(modified_props, SVN_PROP_EOL_STYLE);

          if (val)
            {
              svn_subst_eol_style_t style;
              const char *eol;
              svn_subst_eol_style_from_value(&style, &eol, val->data);

              /* ### Ignoring keywords */
              if (eol)
                file2 = svn_subst_stream_translated(file2, eol, TRUE,
                                                    NULL, FALSE,
                                                    scratch_pool);
            }
        }

      SVN_ERR(svn_stream_contents_same2(&same, file1, file2, scratch_pool));

      if (! same || prop_changes->nelts > 0)
        {
          /* ### We should probably pass the normalized data we created using
                 the subst streams as that is what diff users expect */
          SVN_ERR(callbacks->file_changed(NULL, NULL, NULL, path,
                                          same ? NULL : local_abspath1,
                                          same ? NULL : local_abspath2,
                                          /* ### TODO get real revision info
                                           * for versioned files? */
                                          SVN_INVALID_REVNUM /* rev1 */,
                                          SVN_INVALID_REVNUM /* rev2 */,
                                          original_mime_type ?
                                            original_mime_type->data : NULL,
                                          modified_mime_type ?
                                            modified_mime_type->data : NULL,
                                          prop_changes, original_props,
                                          diff_baton, scratch_pool));
        }
    }

  return SVN_NO_ERROR;
}

struct arbitrary_diff_walker_baton {
  /* The root directories of the trees being compared. */
  const char *root1_abspath;
  const char *root2_abspath;

  /* TRUE if recursing within an added subtree of root2_abspath that
   * does not exist in root1_abspath. */
  svn_boolean_t recursing_within_added_subtree;

  /* TRUE if recursing within an administrative (.i.e. .svn) directory. */
  svn_boolean_t recursing_within_adm_dir;

  /* The absolute path of the adm dir if RECURSING_WITHIN_ADM_DIR is TRUE.
   * Else this is NULL.*/
  const char *adm_dir_abspath;

  /* A path to an empty file used for diffs that add/delete files. */
  const char *empty_file_abspath;

  const svn_wc_diff_callbacks4_t *callbacks;
  void *diff_baton;
  svn_client_ctx_t *ctx;
  apr_pool_t *pool;
} arbitrary_diff_walker_baton;

/* Forward declaration needed because this function has a cyclic
 * dependency with do_arbitrary_dirs_diff(). */
static svn_error_t *
arbitrary_diff_walker(void *baton, const char *local_abspath,
                      const apr_finfo_t *finfo,
                      apr_pool_t *scratch_pool);

/* Another forward declaration. */
static svn_error_t *
arbitrary_diff_this_dir(struct arbitrary_diff_walker_baton *b,
                        const char *local_abspath,
                        svn_depth_t depth,
                        apr_pool_t *scratch_pool);

/* Produce a diff of depth DEPTH between two arbitrary directories at
 * LOCAL_ABSPATH1 and LOCAL_ABSPATH2, using the provided diff callbacks
 * to show file changes and, for versioned nodes, property changes.
 *
 * If ROOT_ABSPATH1 and ROOT_ABSPATH2 are not NULL, show paths in diffs
 * relative to these roots, rather than relative to LOCAL_ABSPATH1 and
 * LOCAL_ABSPATH2. This is needed when crawling a subtree that exists
 * only within LOCAL_ABSPATH2. */
static svn_error_t *
do_arbitrary_dirs_diff(const char *local_abspath1,
                       const char *local_abspath2,
                       const char *root_abspath1,
                       const char *root_abspath2,
                       svn_depth_t depth,
                       const svn_wc_diff_callbacks4_t *callbacks,
                       void *diff_baton,
                       svn_client_ctx_t *ctx,
                       apr_pool_t *scratch_pool)
{
  apr_file_t *empty_file;
  svn_node_kind_t kind1;

  struct arbitrary_diff_walker_baton b;

  /* If LOCAL_ABSPATH1 is not a directory, crawl LOCAL_ABSPATH2 instead
   * and compare it to LOCAL_ABSPATH1, showing only additions.
   * This case can only happen during recursion from arbitrary_diff_walker(),
   * because do_arbitrary_nodes_diff() prevents this from happening at
   * the root of the comparison. */
  SVN_ERR(svn_io_check_resolved_path(local_abspath1, &kind1, scratch_pool));
  b.recursing_within_added_subtree = (kind1 != svn_node_dir);

  b.root1_abspath = root_abspath1 ? root_abspath1 : local_abspath1;
  b.root2_abspath = root_abspath2 ? root_abspath2 : local_abspath2;
  b.recursing_within_adm_dir = FALSE;
  b.adm_dir_abspath = NULL;
  b.callbacks = callbacks;
  b.diff_baton = diff_baton;
  b.ctx = ctx;
  b.pool = scratch_pool;

  SVN_ERR(svn_io_open_unique_file3(&empty_file, &b.empty_file_abspath,
                                   NULL, svn_io_file_del_on_pool_cleanup,
                                   scratch_pool, scratch_pool));

  if (depth <= svn_depth_immediates)
    SVN_ERR(arbitrary_diff_this_dir(&b, local_abspath1, depth, scratch_pool));
  else if (depth == svn_depth_infinity)
    SVN_ERR(svn_io_dir_walk2(b.recursing_within_added_subtree ? local_abspath2
                                                              : local_abspath1,
                             0, arbitrary_diff_walker, &b, scratch_pool));
  return SVN_NO_ERROR;
}

/* Produce a diff of depth DEPTH for the directory at LOCAL_ABSPATH,
 * using information from the arbitrary_diff_walker_baton B.
 * LOCAL_ABSPATH is the path being crawled and can be on either side
 * of the diff depending on baton->recursing_within_added_subtree. */
static svn_error_t *
arbitrary_diff_this_dir(struct arbitrary_diff_walker_baton *b,
                        const char *local_abspath,
                        svn_depth_t depth,
                        apr_pool_t *scratch_pool)
{
  const char *local_abspath1;
  const char *local_abspath2;
  svn_node_kind_t kind1;
  svn_node_kind_t kind2;
  const char *child_relpath;
  apr_hash_t *dirents1;
  apr_hash_t *dirents2;
  apr_hash_t *merged_dirents;
  apr_array_header_t *sorted_dirents;
  int i;
  apr_pool_t *iterpool;

  if (b->recursing_within_adm_dir)
    {
      if (svn_dirent_skip_ancestor(b->adm_dir_abspath, local_abspath))
        return SVN_NO_ERROR;
      else
        {
          b->recursing_within_adm_dir = FALSE;
          b->adm_dir_abspath = NULL;
        }
    }
  else if (svn_wc_is_adm_dir(svn_dirent_basename(local_abspath, NULL),
                             scratch_pool))
    {
      b->recursing_within_adm_dir = TRUE;
      b->adm_dir_abspath = apr_pstrdup(b->pool, local_abspath);
      return SVN_NO_ERROR;
    }

  if (b->recursing_within_added_subtree)
    child_relpath = svn_dirent_skip_ancestor(b->root2_abspath, local_abspath);
  else
    child_relpath = svn_dirent_skip_ancestor(b->root1_abspath, local_abspath);
  if (!child_relpath)
    return SVN_NO_ERROR;

  local_abspath1 = svn_dirent_join(b->root1_abspath, child_relpath,
                                   scratch_pool);
  SVN_ERR(svn_io_check_resolved_path(local_abspath1, &kind1, scratch_pool));

  local_abspath2 = svn_dirent_join(b->root2_abspath, child_relpath,
                                   scratch_pool);
  SVN_ERR(svn_io_check_resolved_path(local_abspath2, &kind2, scratch_pool));

  if (depth > svn_depth_empty)
    {
      if (kind1 == svn_node_dir)
        SVN_ERR(svn_io_get_dirents3(&dirents1, local_abspath1,
                                    TRUE, /* only_check_type */
                                    scratch_pool, scratch_pool));
      else
        dirents1 = apr_hash_make(scratch_pool);
    }

  if (kind2 == svn_node_dir)
    {
      apr_hash_t *original_props;
      apr_hash_t *modified_props;
      apr_array_header_t *prop_changes;

      /* Show any property changes for this directory. */
      SVN_ERR(get_props(&original_props, local_abspath1, b->ctx->wc_ctx,
                        scratch_pool, scratch_pool));
      SVN_ERR(get_props(&modified_props, local_abspath2, b->ctx->wc_ctx,
                        scratch_pool, scratch_pool));
      SVN_ERR(svn_prop_diffs(&prop_changes, modified_props, original_props,
                             scratch_pool));
      if (prop_changes->nelts > 0)
        SVN_ERR(b->callbacks->dir_props_changed(NULL, NULL, child_relpath,
                                                FALSE /* was_added */,
                                                prop_changes, original_props,
                                                b->diff_baton,
                                                scratch_pool));

      if (depth > svn_depth_empty)
        {
          /* Read directory entries. */
          SVN_ERR(svn_io_get_dirents3(&dirents2, local_abspath2,
                                      TRUE, /* only_check_type */
                                      scratch_pool, scratch_pool));
        }
    }
  else if (depth > svn_depth_empty)
    dirents2 = apr_hash_make(scratch_pool);

  if (depth <= svn_depth_empty)
    return SVN_NO_ERROR;

  /* Compare dirents1 to dirents2 and show added/deleted/changed files. */
  merged_dirents = apr_hash_merge(scratch_pool, dirents1, dirents2,
                                  NULL, NULL);
  sorted_dirents = svn_sort__hash(merged_dirents,
                                  svn_sort_compare_items_as_paths,
                                  scratch_pool);
  iterpool = svn_pool_create(scratch_pool);
  for (i = 0; i < sorted_dirents->nelts; i++)
    {
      svn_sort__item_t elt = APR_ARRAY_IDX(sorted_dirents, i, svn_sort__item_t);
      const char *name = elt.key;
      svn_io_dirent2_t *dirent1;
      svn_io_dirent2_t *dirent2;
      const char *child1_abspath;
      const char *child2_abspath;

      svn_pool_clear(iterpool);

      if (b->ctx->cancel_func)
        SVN_ERR(b->ctx->cancel_func(b->ctx->cancel_baton));

      if (strcmp(name, SVN_WC_ADM_DIR_NAME) == 0)
        continue;

      dirent1 = svn_hash_gets(dirents1, name);
      if (!dirent1)
        {
          dirent1 = svn_io_dirent2_create(iterpool);
          dirent1->kind = svn_node_none;
        }
      dirent2 = svn_hash_gets(dirents2, name);
      if (!dirent2)
        {
          dirent2 = svn_io_dirent2_create(iterpool);
          dirent2->kind = svn_node_none;
        }

      child1_abspath = svn_dirent_join(local_abspath1, name, iterpool);
      child2_abspath = svn_dirent_join(local_abspath2, name, iterpool);

      if (dirent1->special)
        SVN_ERR(svn_io_check_resolved_path(child1_abspath, &dirent1->kind,
                                           iterpool));
      if (dirent2->special)
        SVN_ERR(svn_io_check_resolved_path(child1_abspath, &dirent2->kind,
                                           iterpool));

      if (dirent1->kind == svn_node_dir &&
          dirent2->kind == svn_node_dir)
        {
          if (depth == svn_depth_immediates)
            {
              /* Not using the walker, so show property diffs on these dirs. */
              SVN_ERR(do_arbitrary_dirs_diff(child1_abspath, child2_abspath,
                                             b->root1_abspath, b->root2_abspath,
                                             svn_depth_empty,
                                             b->callbacks, b->diff_baton,
                                             b->ctx, iterpool));
            }
          else
            {
              /* Either the walker will visit these directories (with
               * depth=infinity) and they will be processed as 'this dir'
               * later, or we're showing file children only (depth=files). */
              continue;
            }

        }

      /* Files that exist only in dirents1. */
      if (dirent1->kind == svn_node_file &&
          (dirent2->kind == svn_node_dir || dirent2->kind == svn_node_none))
        SVN_ERR(do_arbitrary_files_diff(child1_abspath, b->empty_file_abspath,
                                        svn_relpath_join(child_relpath, name,
                                                         iterpool),
                                        FALSE, TRUE, NULL,
                                        b->callbacks, b->diff_baton,
                                        b->ctx, iterpool));

      /* Files that exist only in dirents2. */
      if (dirent2->kind == svn_node_file &&
          (dirent1->kind == svn_node_dir || dirent1->kind == svn_node_none))
        {
          apr_hash_t *original_props;

          SVN_ERR(get_props(&original_props, child1_abspath, b->ctx->wc_ctx,
                            scratch_pool, scratch_pool));
          SVN_ERR(do_arbitrary_files_diff(b->empty_file_abspath, child2_abspath,
                                          svn_relpath_join(child_relpath, name,
                                                           iterpool),
                                          TRUE, FALSE, original_props,
                                          b->callbacks, b->diff_baton,
                                          b->ctx, iterpool));
        }

      /* Files that exist in dirents1 and dirents2. */
      if (dirent1->kind == svn_node_file && dirent2->kind == svn_node_file)
        SVN_ERR(do_arbitrary_files_diff(child1_abspath, child2_abspath,
                                        svn_relpath_join(child_relpath, name,
                                                         iterpool),
                                        FALSE, FALSE, NULL,
                                        b->callbacks, b->diff_baton,
                                        b->ctx, scratch_pool));

      /* Directories that only exist in dirents2. These aren't crawled
       * by this walker so we have to crawl them separately. */
      if (depth > svn_depth_files &&
          dirent2->kind == svn_node_dir &&
          (dirent1->kind == svn_node_file || dirent1->kind == svn_node_none))
        SVN_ERR(do_arbitrary_dirs_diff(child1_abspath, child2_abspath,
                                       b->root1_abspath, b->root2_abspath,
                                       depth <= svn_depth_immediates
                                         ? svn_depth_empty
                                         : svn_depth_infinity ,
                                       b->callbacks, b->diff_baton,
                                       b->ctx, iterpool));
    }

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

/* An implementation of svn_io_walk_func_t.
 * Note: LOCAL_ABSPATH is the path being crawled and can be on either side
 * of the diff depending on baton->recursing_within_added_subtree. */
static svn_error_t *
arbitrary_diff_walker(void *baton, const char *local_abspath,
                      const apr_finfo_t *finfo,
                      apr_pool_t *scratch_pool)
{
  struct arbitrary_diff_walker_baton *b = baton;

  if (b->ctx->cancel_func)
    SVN_ERR(b->ctx->cancel_func(b->ctx->cancel_baton));

  if (finfo->filetype != APR_DIR)
    return SVN_NO_ERROR;

  SVN_ERR(arbitrary_diff_this_dir(b, local_abspath, svn_depth_infinity,
                                  scratch_pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__arbitrary_nodes_diff(const char *local_abspath1,
                                 const char *local_abspath2,
                                 svn_depth_t depth,
                                 const svn_wc_diff_callbacks4_t *callbacks,
                                 void *diff_baton,
                                 svn_client_ctx_t *ctx,
                                 apr_pool_t *scratch_pool)
{
  svn_node_kind_t kind1;
  svn_node_kind_t kind2;

  SVN_ERR(svn_io_check_resolved_path(local_abspath1, &kind1, scratch_pool));
  SVN_ERR(svn_io_check_resolved_path(local_abspath2, &kind2, scratch_pool));

  if (kind1 != kind2)
    return svn_error_createf(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
                             _("'%s' is not the same node kind as '%s'"),
                             svn_dirent_local_style(local_abspath1,
                                                    scratch_pool),
                             svn_dirent_local_style(local_abspath2,
                                                    scratch_pool));

  if (depth == svn_depth_unknown)
    depth = svn_depth_infinity;

  if (kind1 == svn_node_file)
    SVN_ERR(do_arbitrary_files_diff(local_abspath1, local_abspath2,
                                    svn_dirent_basename(local_abspath1,
                                                        scratch_pool),
                                    FALSE, FALSE, NULL,
                                    callbacks, diff_baton,
                                    ctx, scratch_pool));
  else if (kind1 == svn_node_dir)
    SVN_ERR(do_arbitrary_dirs_diff(local_abspath1, local_abspath2,
                                   NULL, NULL, depth,
                                   callbacks, diff_baton,
                                   ctx, scratch_pool));
  else
    return svn_error_createf(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
                             _("'%s' is not a file or directory"),
                             kind1 == svn_node_none
                               ? svn_dirent_local_style(local_abspath1,
                                                        scratch_pool)
                               : svn_dirent_local_style(local_abspath2,
                                                        scratch_pool));
  return SVN_NO_ERROR;
}
