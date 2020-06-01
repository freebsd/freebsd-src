/*
 * shelf.c:  implementation of shelving
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

/* We define this here to remove any further warnings about the usage of
   experimental functions in this file. */
#define SVN_EXPERIMENTAL

#include "svn_client.h"
#include "svn_wc.h"
#include "svn_pools.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_hash.h"
#include "svn_utf.h"
#include "svn_ctype.h"
#include "svn_props.h"

#include "client.h"
#include "private/svn_client_shelf.h"
#include "private/svn_client_private.h"
#include "private/svn_wc_private.h"
#include "private/svn_sorts_private.h"
#include "svn_private_config.h"


static svn_error_t *
shelf_name_encode(char **encoded_name_p,
                  const char *name,
                  apr_pool_t *result_pool)
{
  char *encoded_name
    = apr_palloc(result_pool, strlen(name) * 2 + 1);
  char *out_pos = encoded_name;

  if (name[0] == '\0')
    return svn_error_create(SVN_ERR_BAD_CHANGELIST_NAME, NULL,
                            _("Shelf name cannot be the empty string"));

  while (*name)
    {
      apr_snprintf(out_pos, 3, "%02x", (unsigned char)(*name++));
      out_pos += 2;
    }
  *encoded_name_p = encoded_name;
  return SVN_NO_ERROR;
}

static svn_error_t *
shelf_name_decode(char **decoded_name_p,
                  const char *codename,
                  apr_pool_t *result_pool)
{
  svn_stringbuf_t *sb
    = svn_stringbuf_create_ensure(strlen(codename) / 2, result_pool);
  const char *input = codename;

  while (*input)
    {
      int c;
      int nchars;
      int nitems = sscanf(input, "%02x%n", &c, &nchars);

      if (nitems != 1 || nchars != 2)
        return svn_error_createf(SVN_ERR_BAD_CHANGELIST_NAME, NULL,
                                 _("Shelve: Bad encoded name '%s'"), codename);
      svn_stringbuf_appendbyte(sb, c);
      input += 2;
    }
  *decoded_name_p = sb->data;
  return SVN_NO_ERROR;
}

/* Set *NAME to the shelf name from FILENAME, if FILENAME names a '.current'
 * file, else to NULL. */
static svn_error_t *
shelf_name_from_filename(char **name,
                         const char *filename,
                         apr_pool_t *result_pool)
{
  size_t len = strlen(filename);
  static const char suffix[] = ".current";
  size_t suffix_len = sizeof(suffix) - 1;

  if (len > suffix_len && strcmp(filename + len - suffix_len, suffix) == 0)
    {
      char *codename = apr_pstrndup(result_pool, filename, len - suffix_len);
      SVN_ERR(shelf_name_decode(name, codename, result_pool));
    }
  else
    {
      *name = NULL;
    }
  return SVN_NO_ERROR;
}

/* Set *DIR to the shelf storage directory inside the WC's administrative
 * area. Ensure the directory exists. */
static svn_error_t *
get_shelves_dir(char **dir,
                svn_wc_context_t *wc_ctx,
                const char *local_abspath,
                apr_pool_t *result_pool,
                apr_pool_t *scratch_pool)
{
  char *experimental_abspath;

  SVN_ERR(svn_wc__get_experimental_dir(&experimental_abspath,
                                       wc_ctx, local_abspath,
                                       scratch_pool, scratch_pool));
  *dir = svn_dirent_join(experimental_abspath, "shelves/v3", result_pool);

  /* Ensure the directory exists. (Other versions of svn don't create it.) */
  SVN_ERR(svn_io_make_dir_recursively(*dir, scratch_pool));

  return SVN_NO_ERROR;
}

/* Set *ABSPATH to the abspath of the file storage dir for SHELF
 * version VERSION, no matter whether it exists.
 */
static svn_error_t *
shelf_version_files_dir_abspath(const char **abspath,
                                svn_client__shelf_t *shelf,
                                int version,
                                apr_pool_t *result_pool,
                                apr_pool_t *scratch_pool)
{
  char *codename;
  char *filename;

  SVN_ERR(shelf_name_encode(&codename, shelf->name, result_pool));
  filename = apr_psprintf(scratch_pool, "%s-%03d.wc", codename, version);
  *abspath = svn_dirent_join(shelf->shelves_dir, filename, result_pool);
  return SVN_NO_ERROR;
}

/* Create a shelf-version object for a version that may or may not already
 * exist on disk.
 */
static svn_error_t *
shelf_version_create(svn_client__shelf_version_t **new_version_p,
                     svn_client__shelf_t *shelf,
                     int version_number,
                     apr_pool_t *result_pool)
{
  svn_client__shelf_version_t *shelf_version
    = apr_pcalloc(result_pool, sizeof(*shelf_version));

  shelf_version->shelf = shelf;
  shelf_version->version_number = version_number;
  SVN_ERR(shelf_version_files_dir_abspath(&shelf_version->files_dir_abspath,
                                          shelf, version_number,
                                          result_pool, result_pool));
  *new_version_p = shelf_version;
  return SVN_NO_ERROR;
}

/* Delete the storage for SHELF:VERSION. */
static svn_error_t *
shelf_version_delete(svn_client__shelf_t *shelf,
                     int version,
                     apr_pool_t *scratch_pool)
{
  const char *files_dir_abspath;

  SVN_ERR(shelf_version_files_dir_abspath(&files_dir_abspath,
                                          shelf, version,
                                          scratch_pool, scratch_pool));
  SVN_ERR(svn_io_remove_dir2(files_dir_abspath, TRUE /*ignore_enoent*/,
                             NULL, NULL, /*cancel*/
                             scratch_pool));
  return SVN_NO_ERROR;
}

/*  */
static svn_error_t *
get_log_abspath(char **log_abspath,
                svn_client__shelf_t *shelf,
                apr_pool_t *result_pool,
                apr_pool_t *scratch_pool)
{
  char *codename;
  const char *filename;

  SVN_ERR(shelf_name_encode(&codename, shelf->name, result_pool));
  filename = apr_pstrcat(scratch_pool, codename, ".log", SVN_VA_NULL);
  *log_abspath = svn_dirent_join(shelf->shelves_dir, filename, result_pool);
  return SVN_NO_ERROR;
}

/* Set SHELF->revprops by reading from its storage (the '.log' file).
 * Set SHELF->revprops to empty if the storage file does not exist; this
 * is not an error.
 */
static svn_error_t *
shelf_read_revprops(svn_client__shelf_t *shelf,
                    apr_pool_t *scratch_pool)
{
  char *log_abspath;
  svn_error_t *err;
  svn_stream_t *stream;

  SVN_ERR(get_log_abspath(&log_abspath, shelf, scratch_pool, scratch_pool));

  shelf->revprops = apr_hash_make(shelf->pool);
  err = svn_stream_open_readonly(&stream, log_abspath,
                                 scratch_pool, scratch_pool);
  if (err && APR_STATUS_IS_ENOENT(err->apr_err))
    {
      svn_error_clear(err);
      return SVN_NO_ERROR;
    }
  else
    SVN_ERR(err);
  SVN_ERR(svn_hash_read2(shelf->revprops, stream, "PROPS-END", shelf->pool));
  SVN_ERR(svn_stream_close(stream));
  return SVN_NO_ERROR;
}

/* Write SHELF's revprops to its file storage.
 */
static svn_error_t *
shelf_write_revprops(svn_client__shelf_t *shelf,
                     apr_pool_t *scratch_pool)
{
  char *log_abspath;
  apr_file_t *file;
  svn_stream_t *stream;

  SVN_ERR(get_log_abspath(&log_abspath, shelf, scratch_pool, scratch_pool));

  SVN_ERR(svn_io_file_open(&file, log_abspath,
                           APR_FOPEN_WRITE | APR_FOPEN_CREATE | APR_FOPEN_TRUNCATE,
                           APR_FPROT_OS_DEFAULT, scratch_pool));
  stream = svn_stream_from_aprfile2(file, FALSE /*disown*/, scratch_pool);

  SVN_ERR(svn_hash_write2(shelf->revprops, stream, "PROPS-END", scratch_pool));
  SVN_ERR(svn_stream_close(stream));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_revprop_set(svn_client__shelf_t *shelf,
                             const char *prop_name,
                             const svn_string_t *prop_val,
                             apr_pool_t *scratch_pool)
{
  svn_hash_sets(shelf->revprops, apr_pstrdup(shelf->pool, prop_name),
                svn_string_dup(prop_val, shelf->pool));
  SVN_ERR(shelf_write_revprops(shelf, scratch_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_revprop_set_all(svn_client__shelf_t *shelf,
                                 apr_hash_t *revprop_table,
                                 apr_pool_t *scratch_pool)
{
  if (revprop_table)
    shelf->revprops = svn_prop_hash_dup(revprop_table, shelf->pool);
  else
    shelf->revprops = apr_hash_make(shelf->pool);

  SVN_ERR(shelf_write_revprops(shelf, scratch_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_revprop_get(svn_string_t **prop_val,
                             svn_client__shelf_t *shelf,
                             const char *prop_name,
                             apr_pool_t *result_pool)
{
  *prop_val = svn_hash_gets(shelf->revprops, prop_name);
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_revprop_list(apr_hash_t **props,
                              svn_client__shelf_t *shelf,
                              apr_pool_t *result_pool)
{
  *props = shelf->revprops;
  return SVN_NO_ERROR;
}

/*  */
static svn_error_t *
get_current_abspath(char **current_abspath,
                    svn_client__shelf_t *shelf,
                    apr_pool_t *result_pool)
{
  char *codename;
  char *filename;

  SVN_ERR(shelf_name_encode(&codename, shelf->name, result_pool));
  filename = apr_psprintf(result_pool, "%s.current", codename);
  *current_abspath = svn_dirent_join(shelf->shelves_dir, filename, result_pool);
  return SVN_NO_ERROR;
}

/* Read SHELF->max_version from its storage (the '.current' file).
 * Set SHELF->max_version to -1 if that file does not exist.
 */
static svn_error_t *
shelf_read_current(svn_client__shelf_t *shelf,
                   apr_pool_t *scratch_pool)
{
  char *current_abspath;
  svn_error_t *err;

  SVN_ERR(get_current_abspath(&current_abspath, shelf, scratch_pool));
  err = svn_io_read_version_file(&shelf->max_version,
                                 current_abspath, scratch_pool);
  if (err)
    {
      shelf->max_version = -1;
      svn_error_clear(err);
      return SVN_NO_ERROR;
    }
  return SVN_NO_ERROR;
}

/*  */
static svn_error_t *
shelf_write_current(svn_client__shelf_t *shelf,
                    apr_pool_t *scratch_pool)
{
  char *current_abspath;

  SVN_ERR(get_current_abspath(&current_abspath, shelf, scratch_pool));
  SVN_ERR(svn_io_write_version_file(current_abspath, shelf->max_version,
                                    scratch_pool));
  return SVN_NO_ERROR;
}

/*-------------------------------------------------------------------------*/
/* Status Reporting */

/* Adjust a status STATUS_IN obtained from the shelf storage WC, to add
 * shelf-related metadata:
 *  - changelist: 'svn:shelf:SHELFNAME'
 */
static svn_error_t *
status_augment(svn_wc_status3_t **status_p,
               const svn_wc_status3_t *status_in,
               svn_client__shelf_version_t *shelf_version,
               apr_pool_t *result_pool)
{
  *status_p = svn_wc_dup_status3(status_in, result_pool);
  (*status_p)->changelist = apr_psprintf(result_pool, "svn:shelf:%s",
                                         shelf_version->shelf->name);
  return SVN_NO_ERROR;
}

/* Read status from shelf storage.
 */
static svn_error_t *
status_read(svn_wc_status3_t **status,
            svn_client__shelf_version_t *shelf_version,
            const char *relpath,
            apr_pool_t *result_pool,
            apr_pool_t *scratch_pool)
{
  svn_client_ctx_t *ctx = shelf_version->shelf->ctx;
  char *abspath
    = svn_dirent_join(shelf_version->files_dir_abspath, relpath,
                      scratch_pool);

  SVN_ERR(svn_wc_status3(status, ctx->wc_ctx, abspath,
                         result_pool, scratch_pool));
  SVN_ERR(status_augment(status, *status, shelf_version, result_pool));
  return SVN_NO_ERROR;
}

/* A visitor function type for use with shelf_status_walk().
 * The same as svn_wc_status_func4_t except relpath instead of abspath.
 */
typedef svn_error_t *(*shelf_status_visitor_t)(void *baton,
                                               const char *relpath,
                                               const svn_wc_status3_t *status,
                                               apr_pool_t *scratch_pool);

/* Baton for shelved_files_walk_visitor(). */
struct shelf_status_baton_t
{
  svn_client__shelf_version_t *shelf_version;
  shelf_status_visitor_t walk_func;
  void *walk_baton;
};

/* Convert a svn_wc_status_func4_t callback invocation to call a
 * shelf_status_visitor_t callback.
 *
 * Call BATON->walk_func(BATON->walk_baton, relpath, ...) for the shelved
 * storage path ABSPATH, converting ABSPATH to a WC-relative path, and
 * augmenting the STATUS.
 *
 * The opposite of wc_status_visitor().
 *
 * Implements svn_wc_status_func4_t. */
static svn_error_t *
shelf_status_visitor(void *baton,
                     const char *abspath,
                     const svn_wc_status3_t *status,
                     apr_pool_t *scratch_pool)
{
  struct shelf_status_baton_t *b = baton;
  const char *relpath;
  svn_wc_status3_t *new_status;

  relpath = svn_dirent_skip_ancestor(b->shelf_version->files_dir_abspath,
                                     abspath);
  SVN_ERR(status_augment(&new_status, status, b->shelf_version, scratch_pool));
  SVN_ERR(b->walk_func(b->walk_baton, relpath, new_status, scratch_pool));
  return SVN_NO_ERROR;
}

/* Report the shelved status of the path SHELF_VERSION:WC_RELPATH
 * via WALK_FUNC(WALK_BATON, ...).
 */
static svn_error_t *
shelf_status_visit_path(svn_client__shelf_version_t *shelf_version,
                        const char *wc_relpath,
                        shelf_status_visitor_t walk_func,
                        void *walk_baton,
                        apr_pool_t *scratch_pool)
{
  svn_wc_status3_t *status;

  SVN_ERR(status_read(&status, shelf_version, wc_relpath,
                      scratch_pool, scratch_pool));
  SVN_ERR(walk_func(walk_baton, wc_relpath, status, scratch_pool));
  return SVN_NO_ERROR;
}

/* Report the shelved status of all the shelved paths in SHELF_VERSION
 * at and under WC_RELPATH, via WALK_FUNC(WALK_BATON, ...).
 */
static svn_error_t *
shelf_status_walk(svn_client__shelf_version_t *shelf_version,
                  const char *wc_relpath,
                  shelf_status_visitor_t walk_func,
                  void *walk_baton,
                  apr_pool_t *scratch_pool)
{
  svn_client_ctx_t *ctx = shelf_version->shelf->ctx;
  char *walk_root_abspath
    = svn_dirent_join(shelf_version->files_dir_abspath, wc_relpath,
                      scratch_pool);
  struct shelf_status_baton_t baton;
  svn_error_t *err;

  baton.shelf_version = shelf_version;
  baton.walk_func = walk_func;
  baton.walk_baton = walk_baton;
  err = svn_wc_walk_status(ctx->wc_ctx, walk_root_abspath,
                           svn_depth_infinity,
                           FALSE /*get_all*/,
                           TRUE /*no_ignore*/,
                           FALSE /*ignore_text_mods*/,
                           NULL /*ignore_patterns: use the defaults*/,
                           shelf_status_visitor, &baton,
                           NULL, NULL, /*cancellation*/
                           scratch_pool);
  if (err && APR_STATUS_IS_ENOENT(err->apr_err))
    svn_error_clear(err);
  else
    SVN_ERR(err);

  return SVN_NO_ERROR;
}

/* Baton for wc_status_visitor(). */
typedef struct wc_status_baton_t
{
  svn_client__shelf_version_t *shelf_version;
  svn_wc_status_func4_t walk_func;
  void *walk_baton;
} wc_status_baton_t;

/* Convert a shelf_status_visitor_t callback invocation to call a
 * svn_wc_status_func4_t callback.
 *
 * Call BATON->walk_func(BATON->walk_baton, abspath, ...) for the WC-
 * relative path RELPATH, converting RELPATH to an abspath in the user's WC.
 *
 * The opposite of shelf_status_visitor().
 *
 * Implements shelf_status_visitor_t. */
static svn_error_t *
wc_status_visitor(void *baton,
                  const char *relpath,
                  const svn_wc_status3_t *status,
                  apr_pool_t *scratch_pool)
{
  struct wc_status_baton_t *b = baton;
  svn_client__shelf_t *shelf = b->shelf_version->shelf;
  const char *abspath = svn_dirent_join(shelf->wc_root_abspath, relpath,
                                        scratch_pool);
  SVN_ERR(b->walk_func(b->walk_baton, abspath, status, scratch_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_version_status_walk(svn_client__shelf_version_t *shelf_version,
                                     const char *wc_relpath,
                                     svn_wc_status_func4_t walk_func,
                                     void *walk_baton,
                                     apr_pool_t *scratch_pool)
{
  wc_status_baton_t baton;

  baton.shelf_version = shelf_version;
  baton.walk_func = walk_func;
  baton.walk_baton = walk_baton;
  SVN_ERR(shelf_status_walk(shelf_version, wc_relpath,
                            wc_status_visitor, &baton,
                            scratch_pool));
  return SVN_NO_ERROR;
}

/*-------------------------------------------------------------------------*/
/* Shelf Storage */

/* Construct a shelf object representing an empty shelf: no versions,
 * no revprops, no looking to see if such a shelf exists on disk.
 */
static svn_error_t *
shelf_construct(svn_client__shelf_t **shelf_p,
                const char *name,
                const char *local_abspath,
                svn_client_ctx_t *ctx,
                apr_pool_t *result_pool)
{
  svn_client__shelf_t *shelf = apr_palloc(result_pool, sizeof(*shelf));
  char *shelves_dir;

  SVN_ERR(svn_client_get_wc_root(&shelf->wc_root_abspath,
                                 local_abspath, ctx,
                                 result_pool, result_pool));
  SVN_ERR(get_shelves_dir(&shelves_dir, ctx->wc_ctx, local_abspath,
                          result_pool, result_pool));
  shelf->shelves_dir = shelves_dir;
  shelf->ctx = ctx;
  shelf->pool = result_pool;

  shelf->name = apr_pstrdup(result_pool, name);
  shelf->revprops = apr_hash_make(result_pool);
  shelf->max_version = 0;

  *shelf_p = shelf;
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_open_existing(svn_client__shelf_t **shelf_p,
                               const char *name,
                               const char *local_abspath,
                               svn_client_ctx_t *ctx,
                               apr_pool_t *result_pool)
{
  SVN_ERR(shelf_construct(shelf_p, name,
                          local_abspath, ctx, result_pool));
  SVN_ERR(shelf_read_revprops(*shelf_p, result_pool));
  SVN_ERR(shelf_read_current(*shelf_p, result_pool));
  if ((*shelf_p)->max_version < 0)
    {
      return svn_error_createf(SVN_ERR_ILLEGAL_TARGET, NULL,
                               _("Shelf '%s' not found"),
                               name);
    }
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_open_or_create(svn_client__shelf_t **shelf_p,
                                const char *name,
                                const char *local_abspath,
                                svn_client_ctx_t *ctx,
                                apr_pool_t *result_pool)
{
  svn_client__shelf_t *shelf;

  SVN_ERR(shelf_construct(&shelf, name,
                          local_abspath, ctx, result_pool));
  SVN_ERR(shelf_read_revprops(shelf, result_pool));
  SVN_ERR(shelf_read_current(shelf, result_pool));
  if (shelf->max_version < 0)
    {
      shelf->max_version = 0;
      SVN_ERR(shelf_write_current(shelf, result_pool));
    }
  *shelf_p = shelf;
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_close(svn_client__shelf_t *shelf,
                       apr_pool_t *scratch_pool)
{
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_delete(const char *name,
                        const char *local_abspath,
                        svn_boolean_t dry_run,
                        svn_client_ctx_t *ctx,
                        apr_pool_t *scratch_pool)
{
  svn_client__shelf_t *shelf;
  int i;
  char *abspath;

  SVN_ERR(svn_client__shelf_open_existing(&shelf, name,
                                         local_abspath, ctx, scratch_pool));

  /* Remove the versions. */
  for (i = shelf->max_version; i > 0; i--)
    {
      SVN_ERR(shelf_version_delete(shelf, i, scratch_pool));
    }

  /* Remove the other files */
  SVN_ERR(get_log_abspath(&abspath, shelf, scratch_pool, scratch_pool));
  SVN_ERR(svn_io_remove_file2(abspath, TRUE /*ignore_enoent*/, scratch_pool));
  SVN_ERR(get_current_abspath(&abspath, shelf, scratch_pool));
  SVN_ERR(svn_io_remove_file2(abspath, TRUE /*ignore_enoent*/, scratch_pool));

  SVN_ERR(svn_client__shelf_close(shelf, scratch_pool));
  return SVN_NO_ERROR;
}

/* Baton for paths_changed_visitor(). */
struct paths_changed_walk_baton_t
{
  apr_hash_t *paths_hash;
  const char *wc_root_abspath;
  apr_pool_t *pool;
};

/* Add to the list(s) in BATON, the RELPATH of a shelved 'binary' file.
 * Implements shelved_files_walk_func_t. */
static svn_error_t *
paths_changed_visitor(void *baton,
                      const char *relpath,
                      const svn_wc_status3_t *s,
                      apr_pool_t *scratch_pool)
{
  struct paths_changed_walk_baton_t *b = baton;

  relpath = apr_pstrdup(b->pool, relpath);
  svn_hash_sets(b->paths_hash, relpath, relpath);
  return SVN_NO_ERROR;
}

/* Get the paths changed, relative to WC root or as abspaths, as a hash
 * and/or an array (in no particular order).
 */
static svn_error_t *
shelf_paths_changed(apr_hash_t **paths_hash_p,
                    apr_array_header_t **paths_array_p,
                    svn_client__shelf_version_t *shelf_version,
                    apr_pool_t *result_pool,
                    apr_pool_t *scratch_pool)
{
  svn_client__shelf_t *shelf = shelf_version->shelf;
  apr_hash_t *paths_hash = apr_hash_make(result_pool);
  struct paths_changed_walk_baton_t baton;

  baton.paths_hash = paths_hash;
  baton.wc_root_abspath = shelf->wc_root_abspath;
  baton.pool = result_pool;
  SVN_ERR(shelf_status_walk(shelf_version, "",
                            paths_changed_visitor, &baton,
                            scratch_pool));

  if (paths_hash_p)
    *paths_hash_p = paths_hash;
  if (paths_array_p)
    SVN_ERR(svn_hash_keys(paths_array_p, paths_hash, result_pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_paths_changed(apr_hash_t **affected_paths,
                               svn_client__shelf_version_t *shelf_version,
                               apr_pool_t *result_pool,
                               apr_pool_t *scratch_pool)
{
  SVN_ERR(shelf_paths_changed(affected_paths, NULL, shelf_version,
                              result_pool, scratch_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_replay(svn_client__shelf_version_t *shelf_version,
                         const char *top_relpath,
                         const svn_delta_editor_t *editor,
                         void *edit_baton,
                         svn_wc_notify_func2_t notify_func,
                         void *notify_baton,
                         apr_pool_t *scratch_pool)
{
  svn_client_ctx_t *ctx = shelf_version->shelf->ctx;
  apr_array_header_t *src_targets = apr_array_make(scratch_pool, 1,
                                                   sizeof(char *));
  const char *src_wc_abspath
    = svn_dirent_join(shelf_version->files_dir_abspath, top_relpath, scratch_pool);

  APR_ARRAY_PUSH(src_targets, const char *) = src_wc_abspath;
  SVN_ERR(svn_client__wc_replay(src_wc_abspath,
                                src_targets, svn_depth_infinity, NULL,
                                editor, edit_baton,
                                notify_func, notify_baton,
                                ctx, scratch_pool));
  return SVN_NO_ERROR;
}

/* Baton for test_apply_file_visitor(). */
struct test_apply_files_baton_t
{
  svn_client__shelf_version_t *shelf_version;
  svn_boolean_t conflict;  /* would it conflict? */
  svn_client_ctx_t *ctx;
};

/* Ideally, set BATON->conflict if we can't apply a change to WC
 * at RELPATH without conflict. But in fact, just check
 * if WC at RELPATH is locally modified.
 *
 * Implements shelved_files_walk_func_t. */
static svn_error_t *
test_apply_file_visitor(void *baton,
                        const char *relpath,
                        const svn_wc_status3_t *s,
                        apr_pool_t *scratch_pool)
{
  struct test_apply_files_baton_t *b = baton;
  const char *wc_root_abspath = b->shelf_version->shelf->wc_root_abspath;
  const char *to_wc_abspath = svn_dirent_join(wc_root_abspath, relpath,
                                              scratch_pool);
  svn_wc_status3_t *status;

  SVN_ERR(svn_wc_status3(&status, b->ctx->wc_ctx, to_wc_abspath,
                         scratch_pool, scratch_pool));
  switch (status->node_status)
    {
    case svn_wc_status_normal:
    case svn_wc_status_none:
      break;
    default:
      b->conflict = TRUE;
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_test_apply_file(svn_boolean_t *conflict_p,
                                 svn_client__shelf_version_t *shelf_version,
                                 const char *file_relpath,
                                 apr_pool_t *scratch_pool)
{
  struct test_apply_files_baton_t baton = {0};

  baton.shelf_version = shelf_version;
  baton.conflict = FALSE;
  baton.ctx = shelf_version->shelf->ctx;
  SVN_ERR(shelf_status_visit_path(shelf_version, file_relpath,
                           test_apply_file_visitor, &baton,
                           scratch_pool));
  *conflict_p = baton.conflict;

  return SVN_NO_ERROR;
}

static svn_error_t *
wc_mods_editor(const svn_delta_editor_t **editor_p,
               void **edit_baton_p,
               const char *dst_wc_abspath,
               svn_wc_notify_func2_t notify_func,
               void *notify_baton,
               svn_client_ctx_t *ctx,
               apr_pool_t *result_pool,
               apr_pool_t *scratch_pool)
{
  svn_client__pathrev_t *base;
  const char *dst_wc_url;
  svn_ra_session_t *ra_session;

  /* We'll need an RA session to obtain the base of any copies */
  SVN_ERR(svn_client__wc_node_get_base(&base,
                                       dst_wc_abspath, ctx->wc_ctx,
                                       scratch_pool, scratch_pool));
  dst_wc_url = base->url;
  SVN_ERR(svn_client_open_ra_session2(&ra_session,
                                      dst_wc_url, dst_wc_abspath,
                                      ctx, result_pool, scratch_pool));
  SVN_ERR(svn_client__wc_editor(editor_p, edit_baton_p,
                                dst_wc_abspath,
                                notify_func, notify_baton,
                                ra_session, ctx, result_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_mods_editor(const svn_delta_editor_t **editor_p,
                              void **edit_baton_p,
                              svn_client__shelf_version_t *shelf_version,
                              svn_wc_notify_func2_t notify_func,
                              void *notify_baton,
                              svn_client_ctx_t *ctx,
                              apr_pool_t *result_pool)
{
  SVN_ERR(wc_mods_editor(editor_p, edit_baton_p,
                         shelf_version->files_dir_abspath,
                         notify_func, notify_baton,
                         ctx, result_pool, result_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_apply(svn_client__shelf_version_t *shelf_version,
                       svn_boolean_t dry_run,
                       apr_pool_t *scratch_pool)
{
  svn_client__shelf_t *shelf = shelf_version->shelf;
  const svn_delta_editor_t *editor;
  void *edit_baton;

  SVN_ERR(wc_mods_editor(&editor, &edit_baton,
                         shelf->wc_root_abspath,
                         NULL, NULL, /*notification*/
                         shelf->ctx, scratch_pool, scratch_pool));

  SVN_ERR(svn_client__shelf_replay(shelf_version, "",
                                   editor, edit_baton,
                                   shelf->ctx->notify_func2, shelf->ctx->notify_baton2,
                                   scratch_pool));

  svn_io_sleep_for_timestamps(shelf->wc_root_abspath,
                              scratch_pool);
  return SVN_NO_ERROR;
}

/* Baton for paths_changed_visitor(). */
struct unapply_walk_baton_t
{
  const char *wc_root_abspath;
  svn_boolean_t dry_run;
  svn_boolean_t use_commit_times;
  svn_client_ctx_t *ctx;
  apr_pool_t *pool;
};

/* Revert the change at RELPATH in the user's WC.
 * Implements shelved_files_walk_func_t. */
static svn_error_t *
unapply_visitor(void *baton,
                const char *relpath,
                const svn_wc_status3_t *s,
                apr_pool_t *scratch_pool)
{
  struct unapply_walk_baton_t *b = baton;
  const char *abspath = svn_dirent_join(b->wc_root_abspath, relpath,
                                        scratch_pool);

  if (!b->dry_run)
    {
      apr_array_header_t *targets
        = apr_array_make(scratch_pool, 1, sizeof(char *));
      svn_depth_t depth;

      APR_ARRAY_PUSH(targets, const char *) = abspath;

      /* If the local modification is a "delete" then revert it all
         (recursively). Otherwise we'd have to walk paths in
         top-down order to revert a delete, whereas we need bottom-up
         order to revert children of an added directory. */
      if (s->node_status == svn_wc_status_deleted
          || s->node_status == svn_wc_status_replaced
          || s->node_status == svn_wc_status_added)
        depth = svn_depth_infinity;
      else
        depth = svn_depth_empty;
      SVN_ERR(svn_wc_revert6(b->ctx->wc_ctx,
                             abspath,
                             depth,
                             b->use_commit_times,
                             NULL /*changelists*/,
                             FALSE /*clear_changelists*/,
                             FALSE /*metadata_only*/,
                             FALSE /*added_keep_local*/,
                             b->ctx->cancel_func, b->ctx->cancel_baton,
                             NULL, NULL, /*notification*/
                             scratch_pool));
    }
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_unapply(svn_client__shelf_version_t *shelf_version,
                         svn_boolean_t dry_run,
                         apr_pool_t *scratch_pool)
{
  svn_client_ctx_t *ctx = shelf_version->shelf->ctx;
  svn_client__shelf_t *shelf = shelf_version->shelf;
  struct unapply_walk_baton_t baton;
  svn_config_t *cfg;

  baton.wc_root_abspath = shelf->wc_root_abspath;
  baton.dry_run = dry_run;
  baton.ctx = ctx;
  baton.pool = scratch_pool;

  cfg = ctx->config ? svn_hash_gets(ctx->config, SVN_CONFIG_CATEGORY_CONFIG)
                    : NULL;
  SVN_ERR(svn_config_get_bool(cfg, &baton.use_commit_times,
                              SVN_CONFIG_SECTION_MISCELLANY,
                              SVN_CONFIG_OPTION_USE_COMMIT_TIMES, FALSE));

  SVN_WC__CALL_WITH_WRITE_LOCK(
    shelf_status_walk(shelf_version, "",
                      unapply_visitor, &baton,
                      scratch_pool),
    ctx->wc_ctx, shelf_version->shelf->wc_root_abspath,
    FALSE /*lock_anchor*/, scratch_pool);
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_delete_newer_versions(svn_client__shelf_t *shelf,
                                       svn_client__shelf_version_t *shelf_version,
                                       apr_pool_t *scratch_pool)
{
  int previous_version = shelf_version ? shelf_version->version_number : 0;
  int i;

  /* Delete any newer checkpoints */
  for (i = shelf->max_version; i > previous_version; i--)
    {
      SVN_ERR(shelf_version_delete(shelf, i, scratch_pool));
    }

  shelf->max_version = previous_version;
  SVN_ERR(shelf_write_current(shelf, scratch_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_diff(svn_client__shelf_version_t *shelf_version,
                       const char *shelf_relpath,
                       svn_depth_t depth,
                       svn_boolean_t ignore_ancestry,
                       const svn_diff_tree_processor_t *diff_processor,
                       apr_pool_t *scratch_pool)
{
  svn_client_ctx_t *ctx = shelf_version->shelf->ctx;
  char *local_abspath
    = svn_dirent_join(shelf_version->files_dir_abspath, shelf_relpath,
                      scratch_pool);

  if (shelf_version->version_number == 0)
    return SVN_NO_ERROR;

  SVN_ERR(svn_wc__diff7(FALSE /*anchor_at_given_paths*/,
                        ctx->wc_ctx, local_abspath,
                        depth,
                        ignore_ancestry,
                        NULL /*changelists*/,
                        diff_processor,
                        NULL, NULL, /*cancellation*/
                        scratch_pool, scratch_pool));
  return SVN_NO_ERROR;
}

/* Populate the storage a new shelf-version object NEW_SHELF_VERSION,
 * by creating a shelf storage WC with its base state copied from the
 * 'real' WC.
 */
static svn_error_t *
shelf_copy_base(svn_client__shelf_version_t *new_shelf_version,
                apr_pool_t *scratch_pool)
{
  svn_client_ctx_t *ctx = new_shelf_version->shelf->ctx;
  const char *users_wc_abspath = new_shelf_version->shelf->wc_root_abspath;
  svn_client__pathrev_t *users_wc_root_base;
  svn_opt_revision_t users_wc_root_rev;
  svn_ra_session_t *ra_session = NULL;
  svn_boolean_t sleep_here = FALSE;

  SVN_ERR(svn_client__wc_node_get_base(&users_wc_root_base,
                                       users_wc_abspath, ctx->wc_ctx,
                                       scratch_pool, scratch_pool));

  /* ### We need to read and recreate the mixed-rev, switched-URL,
     mixed-depth WC state; but for a rough start we'll just use
     HEAD, unswitched, depth-infinity. */
  users_wc_root_rev.kind = svn_opt_revision_head;

  /* ### TODO: Create an RA session that reads from the user's WC.
     For a rough start, we'll just let 'checkout' read from the repo. */

  SVN_ERR(svn_client__checkout_internal(NULL /*result_rev*/, &sleep_here,
                                        users_wc_root_base->url,
                                        new_shelf_version->files_dir_abspath,
                                        &users_wc_root_rev, &users_wc_root_rev,
                                        svn_depth_infinity,
                                        TRUE /*ignore_externals*/,
                                        FALSE /*allow_unver_obstructions*/,
                                        ra_session,
                                        ctx, scratch_pool));
  /* ### hopefully we won't eventually need to sleep_here... */
  if (sleep_here)
    svn_io_sleep_for_timestamps(new_shelf_version->files_dir_abspath,
                                scratch_pool);
  return SVN_NO_ERROR;
}

/*  */
struct shelf_save_notifer_baton_t
{
  svn_client__shelf_version_t *shelf_version;
  svn_wc_notify_func2_t notify_func;
  void *notify_baton;
  svn_client_status_func_t shelved_func;
  void *shelved_baton;
  svn_boolean_t any_shelved;
};

/*  */
static void
shelf_save_notifier(void *baton,
                    const svn_wc_notify_t *notify,
                    apr_pool_t *pool)
{
  struct shelf_save_notifer_baton_t *nb = baton;
  const char *wc_relpath
    = svn_dirent_skip_ancestor(nb->shelf_version->shelf->wc_root_abspath,
                               notify->path);
  svn_client_status_t *cst = NULL;
#if 0
  svn_wc_status3_t *wc_status;

  svn_error_clear(status_read(&wc_status, nb->shelf_version, wc_relpath,
                              pool, pool));
  svn_error_clear(svn_client__create_status(
                    &cst, nb->shelf_version->shelf->ctx->wc_ctx,
                    notify->path, wc_status, pool, pool));
#endif
  svn_error_clear(nb->shelved_func(nb->shelved_baton, wc_relpath, cst, pool));
  nb->any_shelved = TRUE;

  nb->notify_func(nb->notify_baton, notify, pool);
}

svn_error_t *
svn_client__shelf_save_new_version3(svn_client__shelf_version_t **new_version_p,
                                   svn_client__shelf_t *shelf,
                                   const apr_array_header_t *paths,
                                   svn_depth_t depth,
                                   const apr_array_header_t *changelists,
                                   svn_client_status_func_t shelved_func,
                                   void *shelved_baton,
                                   svn_client_status_func_t not_shelved_func,
                                   void *not_shelved_baton,
                                   apr_pool_t *scratch_pool)
{
  svn_client_ctx_t *ctx = shelf->ctx;
  int next_version = shelf->max_version + 1;
  svn_client__shelf_version_t *new_shelf_version;
  struct shelf_save_notifer_baton_t nb;
  const svn_delta_editor_t *editor;
  void *edit_baton;

  SVN_ERR(shelf_version_create(&new_shelf_version,
                               shelf, next_version, scratch_pool));
  SVN_ERR(shelf_copy_base(new_shelf_version, scratch_pool));

  nb.shelf_version = new_shelf_version;
  nb.notify_func = ctx->notify_func2;
  nb.notify_baton = ctx->notify_baton2;
  nb.shelved_func = shelved_func;
  nb.shelved_baton = shelved_baton;
  nb.any_shelved = FALSE;
  SVN_ERR(svn_client__shelf_mods_editor(&editor, &edit_baton,
                                        new_shelf_version,
                                        NULL, NULL, /*notification*/
                                        ctx, scratch_pool));
  SVN_ERR(svn_client__wc_replay(shelf->wc_root_abspath,
                                paths, depth, changelists,
                                editor, edit_baton,
                                shelf_save_notifier, &nb,
                                ctx, scratch_pool));

  if (nb.any_shelved)
    {
      shelf->max_version = next_version;
      SVN_ERR(shelf_write_current(shelf, scratch_pool));

      if (new_version_p)
        SVN_ERR(svn_client__shelf_version_open(new_version_p, shelf, next_version,
                                               scratch_pool, scratch_pool));
    }
  else
    {
      if (new_version_p)
        *new_version_p = NULL;
    }
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_get_log_message(char **log_message,
                                 svn_client__shelf_t *shelf,
                                 apr_pool_t *result_pool)
{
  svn_string_t *propval = svn_hash_gets(shelf->revprops, SVN_PROP_REVISION_LOG);

  if (propval)
    *log_message = apr_pstrdup(result_pool, propval->data);
  else
    *log_message = NULL;
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_set_log_message(svn_client__shelf_t *shelf,
                                 const char *message,
                                 apr_pool_t *scratch_pool)
{
  svn_string_t *propval
    = message ? svn_string_create(message, shelf->pool) : NULL;

  SVN_ERR(svn_client__shelf_revprop_set(shelf, SVN_PROP_REVISION_LOG, propval,
                                       scratch_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_list(apr_hash_t **shelf_infos,
                      const char *local_abspath,
                      svn_client_ctx_t *ctx,
                      apr_pool_t *result_pool,
                      apr_pool_t *scratch_pool)
{
  const char *wc_root_abspath;
  char *shelves_dir;
  apr_hash_t *dirents;
  apr_hash_index_t *hi;

  SVN_ERR(svn_wc__get_wcroot(&wc_root_abspath, ctx->wc_ctx, local_abspath,
                             scratch_pool, scratch_pool));
  SVN_ERR(get_shelves_dir(&shelves_dir, ctx->wc_ctx, local_abspath,
                          scratch_pool, scratch_pool));
  SVN_ERR(svn_io_get_dirents3(&dirents, shelves_dir, FALSE /*only_check_type*/,
                              result_pool, scratch_pool));

  *shelf_infos = apr_hash_make(result_pool);

  /* Remove non-shelves */
  for (hi = apr_hash_first(scratch_pool, dirents); hi; hi = apr_hash_next(hi))
    {
      const char *filename = apr_hash_this_key(hi);
      svn_io_dirent2_t *dirent = apr_hash_this_val(hi);
      char *name = NULL;

      svn_error_clear(shelf_name_from_filename(&name, filename, result_pool));
      if (name && dirent->kind == svn_node_file)
        {
          svn_client__shelf_info_t *info
            = apr_palloc(result_pool, sizeof(*info));

          info->mtime = dirent->mtime;
          svn_hash_sets(*shelf_infos, name, info);
        }
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_version_open(svn_client__shelf_version_t **shelf_version_p,
                              svn_client__shelf_t *shelf,
                              int version_number,
                              apr_pool_t *result_pool,
                              apr_pool_t *scratch_pool)
{
  svn_client__shelf_version_t *shelf_version;
  const svn_io_dirent2_t *dirent;

  SVN_ERR(shelf_version_create(&shelf_version,
                               shelf, version_number, result_pool));
  SVN_ERR(svn_io_stat_dirent2(&dirent,
                              shelf_version->files_dir_abspath,
                              FALSE /*verify_truename*/,
                              TRUE /*ignore_enoent*/,
                              result_pool, scratch_pool));
  if (dirent->kind == svn_node_none)
    {
      return svn_error_createf(SVN_ERR_ILLEGAL_TARGET, NULL,
                               _("Shelf '%s' version %d not found"),
                               shelf->name, version_number);
    }
  shelf_version->mtime = dirent->mtime;
  *shelf_version_p = shelf_version;
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_get_newest_version(svn_client__shelf_version_t **shelf_version_p,
                                    svn_client__shelf_t *shelf,
                                    apr_pool_t *result_pool,
                                    apr_pool_t *scratch_pool)
{
  if (shelf->max_version == 0)
    {
      *shelf_version_p = NULL;
      return SVN_NO_ERROR;
    }

  SVN_ERR(svn_client__shelf_version_open(shelf_version_p,
                                        shelf, shelf->max_version,
                                        result_pool, scratch_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf_get_all_versions(apr_array_header_t **versions_p,
                                  svn_client__shelf_t *shelf,
                                  apr_pool_t *result_pool,
                                  apr_pool_t *scratch_pool)
{
  int i;

  *versions_p = apr_array_make(result_pool, shelf->max_version - 1,
                               sizeof(svn_client__shelf_version_t *));

  for (i = 1; i <= shelf->max_version; i++)
    {
      svn_client__shelf_version_t *shelf_version;

      SVN_ERR(svn_client__shelf_version_open(&shelf_version,
                                            shelf, i,
                                            result_pool, scratch_pool));
      APR_ARRAY_PUSH(*versions_p, svn_client__shelf_version_t *) = shelf_version;
    }
  return SVN_NO_ERROR;
}
