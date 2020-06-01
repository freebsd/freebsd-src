/*
 * wc_editor.c: editing the local modifications in the WC.
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

#include <string.h>
#include "svn_hash.h"
#include "svn_client.h"
#include "svn_delta.h"
#include "svn_dirent_uri.h"
#include "svn_error.h"
#include "svn_error_codes.h"
#include "svn_pools.h"
#include "svn_props.h"
#include "svn_wc.h"

#include <apr_md5.h>

#include "client.h"
#include "private/svn_subr_private.h"
#include "private/svn_wc_private.h"
#include "svn_private_config.h"


/* ------------------------------------------------------------------ */

/* WC Modifications Editor.
 *
 * This editor applies incoming modifications onto the current working state
 * of the working copy, to produce a new working state.
 *
 * Currently, it assumes the working state matches what the edit driver
 * expects to find, and may throw an error if not.
 *
 * For simplicity, we apply incoming edits as they arrive, rather than
 * queueing them up to apply in a batch.
 *
 * TODO:
 *   - tests
 *   - use for all existing scenarios ('svn add', 'svn propset', etc.)
 *   - Instead of 'root_dir_add' option, probably the driver should anchor
 *     at the parent dir.
 *   - Instead of 'ignore_mergeinfo' option, implement that as a wrapper.
 *   - Option to quietly accept changes that seem to be already applied
 *     in the versioned state and/or on disk.
 *     Consider 'svn add' which assumes items to be added are found on disk.
 *   - Notification.
 */

/* Everything we need to know about the edit session.
 */
struct edit_baton_t
{
  const char *anchor_abspath;
  svn_boolean_t manage_wc_write_lock;
  const char *lock_root_abspath;  /* the path locked, when locked */

  /* True => 'open_root' method will act as 'add_directory' */
  svn_boolean_t root_dir_add;
  /* True => filter out any incoming svn:mergeinfo property changes */
  svn_boolean_t ignore_mergeinfo_changes;

  svn_ra_session_t *ra_session;

  svn_wc_context_t *wc_ctx;
  svn_client_ctx_t *ctx;
  svn_wc_notify_func2_t notify_func;
  void *notify_baton;
};

/* Everything we need to know about a directory that's open for edits.
 */
struct dir_baton_t
{
  apr_pool_t *pool;

  struct edit_baton_t *eb;

  const char *local_abspath;
};

/* Join PATH onto ANCHOR_ABSPATH.
 * Throw an error if the result is outside ANCHOR_ABSPATH.
 */
static svn_error_t *
get_path(const char **local_abspath_p,
         const char *anchor_abspath,
         const char *path,
         apr_pool_t *result_pool)
{
  svn_boolean_t under_root;

  SVN_ERR(svn_dirent_is_under_root(&under_root, local_abspath_p,
                                   anchor_abspath, path, result_pool));
  if (! under_root)
    {
      return svn_error_createf(
                    SVN_ERR_WC_OBSTRUCTED_UPDATE, NULL,
                    _("Path '%s' is not in the working copy"),
                    svn_dirent_local_style(path, result_pool));
    }
  return SVN_NO_ERROR;
}

/* Create a directory on disk and add it to version control,
 * with no properties.
 */
static svn_error_t *
mkdir(const char *abspath,
      struct edit_baton_t *eb,
      apr_pool_t *scratch_pool)
{
  SVN_ERR(svn_io_make_dir_recursively(abspath, scratch_pool));
  SVN_ERR(svn_wc_add_from_disk3(eb->wc_ctx, abspath,
                                NULL /*properties*/,
                                TRUE /* skip checks */,
                                eb->notify_func, eb->notify_baton,
                                scratch_pool));
  return SVN_NO_ERROR;
}

/* Prepare to open or add a directory: initialize a new dir baton.
 *
 * If PATH is "" and PB is null, it represents the root directory of
 * the edit; otherwise PATH is not "" and PB is not null.
 */
static svn_error_t *
dir_open_or_add(struct dir_baton_t **child_dir_baton,
                const char *path,
                struct dir_baton_t *pb,
                struct edit_baton_t *eb,
                apr_pool_t *dir_pool)
{
  struct dir_baton_t *db = apr_pcalloc(dir_pool, sizeof(*db));

  db->pool = dir_pool;
  db->eb = eb;

  SVN_ERR(get_path(&db->local_abspath,
                   eb->anchor_abspath, path, dir_pool));

  *child_dir_baton = db;
  return SVN_NO_ERROR;
}

/*  */
static svn_error_t *
release_write_lock(struct edit_baton_t *eb,
                   apr_pool_t *scratch_pool)
{
  if (eb->lock_root_abspath)
    {
      SVN_ERR(svn_wc__release_write_lock(
                eb->ctx->wc_ctx, eb->lock_root_abspath, scratch_pool));
      eb->lock_root_abspath = NULL;
    }
  return SVN_NO_ERROR;
}

/*  */
static apr_status_t
pool_cleanup_handler(void *root_baton)
{
  struct dir_baton_t *db = root_baton;
  struct edit_baton_t *eb = db->eb;

  svn_error_clear(release_write_lock(eb, db->pool));
  return APR_SUCCESS;
}

/* svn_delta_editor_t function */
static svn_error_t *
edit_open(void *edit_baton,
          svn_revnum_t base_revision,
          apr_pool_t *result_pool,
          void **root_baton)
{
  struct edit_baton_t *eb = edit_baton;
  struct dir_baton_t *db;

  SVN_ERR(dir_open_or_add(&db, "", NULL, eb, result_pool));

  /* Acquire a WC write lock */
  if (eb->manage_wc_write_lock)
    {
      apr_pool_cleanup_register(db->pool, db,
                                pool_cleanup_handler,
                                apr_pool_cleanup_null);
      SVN_ERR(svn_wc__acquire_write_lock(&eb->lock_root_abspath,
                                         eb->ctx->wc_ctx,
                                         eb->anchor_abspath,
                                         FALSE /*lock_anchor*/,
                                         db->pool, db->pool));
    }

  if (eb->root_dir_add)
    {
      SVN_ERR(mkdir(db->local_abspath, eb, result_pool));
    }

  *root_baton = db;
  return SVN_NO_ERROR;
}

/* svn_delta_editor_t function */
static svn_error_t *
edit_close_or_abort(void *edit_baton,
                    apr_pool_t *scratch_pool)
{
  SVN_ERR(release_write_lock(edit_baton, scratch_pool));
  return SVN_NO_ERROR;
}

/*  */
static svn_error_t *
delete_entry(const char *path,
             svn_revnum_t revision,
             void *parent_baton,
             apr_pool_t *scratch_pool)
{
  struct dir_baton_t *pb = parent_baton;
  struct edit_baton_t *eb = pb->eb;
  const char *local_abspath;

  SVN_ERR(get_path(&local_abspath,
                   eb->anchor_abspath, path, scratch_pool));
  SVN_ERR(svn_wc_delete4(eb->wc_ctx, local_abspath,
                         FALSE /*keep_local*/,
                         TRUE /*delete_unversioned*/,
                         NULL, NULL, /*cancellation*/
                         eb->notify_func, eb->notify_baton, scratch_pool));

  return SVN_NO_ERROR;
}

/* An svn_delta_editor_t function. */
static svn_error_t *
dir_open(const char *path,
         void *parent_baton,
         svn_revnum_t base_revision,
         apr_pool_t *result_pool,
         void **child_baton)
{
  struct dir_baton_t *pb = parent_baton;
  struct edit_baton_t *eb = pb->eb;
  struct dir_baton_t *db;

  SVN_ERR(dir_open_or_add(&db, path, pb, eb, result_pool));

  *child_baton = db;
  return SVN_NO_ERROR;
}

static svn_error_t *
dir_add(const char *path,
        void *parent_baton,
        const char *copyfrom_path,
        svn_revnum_t copyfrom_revision,
        apr_pool_t *result_pool,
        void **child_baton)
{
  struct dir_baton_t *pb = parent_baton;
  struct edit_baton_t *eb = pb->eb;
  struct dir_baton_t *db;
  /* ### Our caller should be providing a scratch pool */
  apr_pool_t *scratch_pool = svn_pool_create(result_pool);

  SVN_ERR(dir_open_or_add(&db, path, pb, eb, result_pool));

  if (copyfrom_path && SVN_IS_VALID_REVNUM(copyfrom_revision))
    {
      SVN_ERR(svn_client__repos_to_wc_copy_internal(NULL /*timestamp_sleep*/,
                                           svn_node_dir,
                                           copyfrom_path,
                                           copyfrom_revision,
                                           db->local_abspath,
                                           db->eb->ra_session,
                                           db->eb->ctx,
                                           scratch_pool));
    }
  else
    {
      SVN_ERR(mkdir(db->local_abspath, eb, result_pool));
    }

  *child_baton = db;
  svn_pool_destroy(scratch_pool);
  return SVN_NO_ERROR;
}

static svn_error_t *
dir_change_prop(void *dir_baton,
                const char *name,
                const svn_string_t *value,
                apr_pool_t *scratch_pool)
{
  struct dir_baton_t *db = dir_baton;
  struct edit_baton_t *eb = db->eb;

  if (svn_property_kind2(name) != svn_prop_regular_kind
      || (eb->ignore_mergeinfo_changes && ! strcmp(name, SVN_PROP_MERGEINFO)))
    {
      /* We can't handle DAV, ENTRY and merge specific props here */
      return SVN_NO_ERROR;
    }

  SVN_ERR(svn_wc_prop_set4(eb->wc_ctx, db->local_abspath, name, value,
                           svn_depth_empty, FALSE, NULL,
                           NULL, NULL, /* Cancellation */
                           NULL, NULL, /* Notification */
                           scratch_pool));

  return SVN_NO_ERROR;
}

static svn_error_t *
dir_close(void *dir_baton,
          apr_pool_t *scratch_pool)
{
  return SVN_NO_ERROR;
}

/* Everything we need to know about a file that's open for edits.
 */
struct file_baton_t
{
  apr_pool_t *pool;

  struct edit_baton_t *eb;

  const char *local_abspath;

  /* fields for the transfer of text changes */
  const char *writing_file;
  unsigned char digest[APR_MD5_DIGESTSIZE];  /* MD5 digest of new fulltext */
  svn_stream_t *wc_file_read_stream, *tmp_file_write_stream;
  const char *tmp_path;
};

/* Create a new file on disk and add it to version control.
 *
 * The file is empty and has no properties.
 */
static svn_error_t *
mkfile(const char *abspath,
       struct edit_baton_t *eb,
       apr_pool_t *scratch_pool)
{
  SVN_ERR(svn_io_file_create_empty(abspath, scratch_pool));
  SVN_ERR(svn_wc_add_from_disk3(eb->wc_ctx, abspath,
                                NULL /*properties*/,
                                TRUE /* skip checks */,
                                eb->notify_func, eb->notify_baton,
                                scratch_pool));
  return SVN_NO_ERROR;
}

/*  */
static svn_error_t *
file_open_or_add(const char *path,
                 void *parent_baton,
                 struct file_baton_t **file_baton,
                 apr_pool_t *file_pool)
{
  struct dir_baton_t *pb = parent_baton;
  struct edit_baton_t *eb = pb->eb;
  struct file_baton_t *fb = apr_pcalloc(file_pool, sizeof(*fb));

  fb->pool = file_pool;
  fb->eb = eb;
  SVN_ERR(get_path(&fb->local_abspath,
                   eb->anchor_abspath, path, fb->pool));

  *file_baton = fb;
  return SVN_NO_ERROR;
}

static svn_error_t *
file_open(const char *path,
          void *parent_baton,
          svn_revnum_t base_revision,
          apr_pool_t *result_pool,
          void **file_baton)
{
  struct file_baton_t *fb;

  SVN_ERR(file_open_or_add(path, parent_baton, &fb, result_pool));

  *file_baton = fb;
  return SVN_NO_ERROR;
}

static svn_error_t *
file_add(const char *path,
         void *parent_baton,
         const char *copyfrom_path,
         svn_revnum_t copyfrom_revision,
         apr_pool_t *result_pool,
         void **file_baton)
{
  struct file_baton_t *fb;

  SVN_ERR(file_open_or_add(path, parent_baton, &fb, result_pool));

  if (copyfrom_path && SVN_IS_VALID_REVNUM(copyfrom_revision))
    {
      SVN_ERR(svn_client__repos_to_wc_copy_internal(NULL /*timestamp_sleep*/,
                                           svn_node_file,
                                           copyfrom_path,
                                           copyfrom_revision,
                                           fb->local_abspath,
                                           fb->eb->ra_session,
                                           fb->eb->ctx, fb->pool));
    }
  else
    {
      SVN_ERR(mkfile(fb->local_abspath, fb->eb, result_pool));
    }

  *file_baton = fb;
  return SVN_NO_ERROR;
}

static svn_error_t *
file_change_prop(void *file_baton,
                 const char *name,
                 const svn_string_t *value,
                 apr_pool_t *scratch_pool)
{
  struct file_baton_t *fb = file_baton;
  struct edit_baton_t *eb = fb->eb;

  if (svn_property_kind2(name) != svn_prop_regular_kind
      || (eb->ignore_mergeinfo_changes && ! strcmp(name, SVN_PROP_MERGEINFO)))
    {
      /* We can't handle DAV, ENTRY and merge specific props here */
      return SVN_NO_ERROR;
    }

  SVN_ERR(svn_wc_prop_set4(eb->wc_ctx, fb->local_abspath, name, value,
                           svn_depth_empty, FALSE, NULL,
                           NULL, NULL, /* Cancellation */
                           NULL, NULL, /* Notification */
                           scratch_pool));

  return SVN_NO_ERROR;
}

static svn_error_t *
file_textdelta(void *file_baton,
               const char *base_checksum,
               apr_pool_t *result_pool,
               svn_txdelta_window_handler_t *handler,
               void **handler_baton)
{
  struct file_baton_t *fb = file_baton;
  const char *target_dir = svn_dirent_dirname(fb->local_abspath, fb->pool);
  svn_error_t *err;

  SVN_ERR_ASSERT(! fb->writing_file);

  err = svn_stream_open_readonly(&fb->wc_file_read_stream, fb->local_abspath,
                                 fb->pool, fb->pool);
  if (err && APR_STATUS_IS_ENOENT(err->apr_err))
    {
      svn_error_clear(err);
      fb->wc_file_read_stream = svn_stream_empty(fb->pool);
    }
  else
    SVN_ERR(err);

  SVN_ERR(svn_stream_open_unique(&fb->tmp_file_write_stream, &fb->writing_file,
                                 target_dir, svn_io_file_del_none,
                                 fb->pool, fb->pool));

  svn_txdelta_apply(fb->wc_file_read_stream,
                    fb->tmp_file_write_stream,
                    fb->digest,
                    fb->local_abspath,
                    fb->pool,
                    /* Provide the handler directly */
                    handler, handler_baton);

  return SVN_NO_ERROR;
}

static svn_error_t *
file_close(void *file_baton,
           const char *text_checksum,
           apr_pool_t *scratch_pool)
{
  struct file_baton_t *fb = file_baton;

  /* If we have text changes, write them to disk */
  if (fb->writing_file)
    {
      SVN_ERR(svn_stream_close(fb->wc_file_read_stream));
      SVN_ERR(svn_io_file_rename2(fb->writing_file, fb->local_abspath,
                                  FALSE /*flush*/, scratch_pool));
    }

  if (text_checksum)
    {
      svn_checksum_t *expected_checksum;
      svn_checksum_t *actual_checksum;

      SVN_ERR(svn_checksum_parse_hex(&expected_checksum, svn_checksum_md5,
                                     text_checksum, fb->pool));
      actual_checksum = svn_checksum__from_digest_md5(fb->digest, fb->pool);

      if (! svn_checksum_match(expected_checksum, actual_checksum))
        return svn_error_trace(
                    svn_checksum_mismatch_err(expected_checksum,
                                              actual_checksum,
                                              fb->pool,
                                         _("Checksum mismatch for '%s'"),
                                              svn_dirent_local_style(
                                                    fb->local_abspath,
                                                    fb->pool)));
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__wc_editor_internal(const svn_delta_editor_t **editor_p,
                               void **edit_baton_p,
                               const char *dst_abspath,
                               svn_boolean_t root_dir_add,
                               svn_boolean_t ignore_mergeinfo_changes,
                               svn_boolean_t manage_wc_write_lock,
                               svn_wc_notify_func2_t notify_func,
                               void *notify_baton,
                               svn_ra_session_t *ra_session,
                               svn_client_ctx_t *ctx,
                               apr_pool_t *result_pool)
{
  svn_delta_editor_t *editor = svn_delta_default_editor(result_pool);
  struct edit_baton_t *eb = apr_pcalloc(result_pool, sizeof(*eb));

  eb->anchor_abspath = apr_pstrdup(result_pool, dst_abspath);
  eb->manage_wc_write_lock = manage_wc_write_lock;
  eb->lock_root_abspath = NULL;
  eb->root_dir_add = root_dir_add;
  eb->ignore_mergeinfo_changes = ignore_mergeinfo_changes;

  eb->ra_session = ra_session;
  eb->wc_ctx = ctx->wc_ctx;
  eb->ctx = ctx;
  eb->notify_func = notify_func;
  eb->notify_baton  = notify_baton;

  editor->open_root = edit_open;
  editor->close_edit = edit_close_or_abort;
  editor->abort_edit = edit_close_or_abort;

  editor->delete_entry = delete_entry;

  editor->open_directory = dir_open;
  editor->add_directory = dir_add;
  editor->change_dir_prop = dir_change_prop;
  editor->close_directory = dir_close;

  editor->open_file = file_open;
  editor->add_file = file_add;
  editor->change_file_prop = file_change_prop;
  editor->apply_textdelta = file_textdelta;
  editor->close_file = file_close;

  *editor_p = editor;
  *edit_baton_p = eb;
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__wc_editor(const svn_delta_editor_t **editor_p,
                      void **edit_baton_p,
                      const char *dst_abspath,
                      svn_wc_notify_func2_t notify_func,
                      void *notify_baton,
                      svn_ra_session_t *ra_session,
                      svn_client_ctx_t *ctx,
                      apr_pool_t *result_pool)
{
  SVN_ERR(svn_client__wc_editor_internal(editor_p, edit_baton_p,
                                         dst_abspath,
                                         FALSE /*root_dir_add*/,
                                         FALSE /*ignore_mergeinfo_changes*/,
                                         TRUE /*manage_wc_write_lock*/,
                                         notify_func, notify_baton,
                                         ra_session,
                                         ctx, result_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__wc_copy_mods(const char *src_wc_abspath,
                         const char *dst_wc_abspath,
                         svn_wc_notify_func2_t notify_func,
                         void *notify_baton,
                         svn_client_ctx_t *ctx,
                         apr_pool_t *scratch_pool)
{
  svn_client__pathrev_t *base;
  const char *dst_wc_url;
  svn_ra_session_t *ra_session;
  const svn_delta_editor_t *editor;
  void *edit_baton;
  apr_array_header_t *src_targets = apr_array_make(scratch_pool, 1,
                                                   sizeof(char *));

  /* We'll need an RA session to obtain the base of any copies */
  SVN_ERR(svn_client__wc_node_get_base(&base,
                                       src_wc_abspath, ctx->wc_ctx,
                                       scratch_pool, scratch_pool));
  dst_wc_url = base->url;
  SVN_ERR(svn_client_open_ra_session2(&ra_session,
                                      dst_wc_url, dst_wc_abspath,
                                      ctx, scratch_pool, scratch_pool));
  SVN_ERR(svn_client__wc_editor(&editor, &edit_baton,
                                dst_wc_abspath,
                                NULL, NULL, /*notification*/
                                ra_session, ctx, scratch_pool));

  APR_ARRAY_PUSH(src_targets, const char *) = src_wc_abspath;
  SVN_ERR(svn_client__wc_replay(src_wc_abspath,
                                src_targets, svn_depth_infinity, NULL,
                                editor, edit_baton,
                                notify_func, notify_baton,
                                ctx, scratch_pool));

  return SVN_NO_ERROR;
}
