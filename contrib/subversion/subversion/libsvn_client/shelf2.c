/*
 * shelf2.c:  implementation of shelving v2
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
#include "private/svn_client_shelf2.h"
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
  int suffix_len = sizeof(suffix) - 1;

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
  *dir = svn_dirent_join(experimental_abspath, "shelves/v2", result_pool);

  /* Ensure the directory exists. (Other versions of svn don't create it.) */
  SVN_ERR(svn_io_make_dir_recursively(*dir, scratch_pool));

  return SVN_NO_ERROR;
}

/* Set *ABSPATH to the abspath of the file storage dir for SHELF
 * version VERSION, no matter whether it exists.
 */
static svn_error_t *
shelf_version_files_dir_abspath(const char **abspath,
                                svn_client__shelf2_t *shelf,
                                int version,
                                apr_pool_t *result_pool,
                                apr_pool_t *scratch_pool)
{
  char *codename;
  char *filename;

  SVN_ERR(shelf_name_encode(&codename, shelf->name, result_pool));
  filename = apr_psprintf(scratch_pool, "%s-%03d.d", codename, version);
  *abspath = svn_dirent_join(shelf->shelves_dir, filename, result_pool);
  return SVN_NO_ERROR;
}

/* Create a shelf-version object for a version that may or may not already
 * exist on disk.
 */
static svn_error_t *
shelf_version_create(svn_client__shelf2_version_t **new_version_p,
                     svn_client__shelf2_t *shelf,
                     int version_number,
                     apr_pool_t *result_pool)
{
  svn_client__shelf2_version_t *shelf_version
    = apr_pcalloc(result_pool, sizeof(*shelf_version));

  shelf_version->shelf = shelf;
  shelf_version->version_number = version_number;
  SVN_ERR(shelf_version_files_dir_abspath(&shelf_version->files_dir_abspath,
                                          shelf, version_number,
                                          result_pool, result_pool));
  *new_version_p = shelf_version;
  return SVN_NO_ERROR;
}

/* Set *ABSPATH to the abspath of the metadata file for SHELF_VERSION
 * node at RELPATH, no matter whether it exists.
 */
static svn_error_t *
get_metadata_abspath(char **abspath,
                     svn_client__shelf2_version_t *shelf_version,
                     const char *wc_relpath,
                     apr_pool_t *result_pool,
                     apr_pool_t *scratch_pool)
{
  wc_relpath = apr_psprintf(scratch_pool, "%s.meta", wc_relpath);
  *abspath = svn_dirent_join(shelf_version->files_dir_abspath, wc_relpath,
                             result_pool);
  return SVN_NO_ERROR;
}

/* Set *ABSPATH to the abspath of the base text file for SHELF_VERSION
 * node at RELPATH, no matter whether it exists.
 */
static svn_error_t *
get_base_file_abspath(char **base_abspath,
                      svn_client__shelf2_version_t *shelf_version,
                      const char *wc_relpath,
                      apr_pool_t *result_pool,
                      apr_pool_t *scratch_pool)
{
  wc_relpath = apr_psprintf(scratch_pool, "%s.base", wc_relpath);
  *base_abspath = svn_dirent_join(shelf_version->files_dir_abspath, wc_relpath,
                                  result_pool);
  return SVN_NO_ERROR;
}

/* Set *ABSPATH to the abspath of the working text file for SHELF_VERSION
 * node at RELPATH, no matter whether it exists.
 */
static svn_error_t *
get_working_file_abspath(char **work_abspath,
                         svn_client__shelf2_version_t *shelf_version,
                         const char *wc_relpath,
                         apr_pool_t *result_pool,
                         apr_pool_t *scratch_pool)
{
  wc_relpath = apr_psprintf(scratch_pool, "%s.work", wc_relpath);
  *work_abspath = svn_dirent_join(shelf_version->files_dir_abspath, wc_relpath,
                                  result_pool);
  return SVN_NO_ERROR;
}

/* Set *ABSPATH to the abspath of the base props file for SHELF_VERSION
 * node at RELPATH, no matter whether it exists.
 */
static svn_error_t *
get_base_props_abspath(char **base_abspath,
                       svn_client__shelf2_version_t *shelf_version,
                       const char *wc_relpath,
                       apr_pool_t *result_pool,
                       apr_pool_t *scratch_pool)
{
  wc_relpath = apr_psprintf(scratch_pool, "%s.base-props", wc_relpath);
  *base_abspath = svn_dirent_join(shelf_version->files_dir_abspath, wc_relpath,
                                  result_pool);
  return SVN_NO_ERROR;
}

/* Set *ABSPATH to the abspath of the working props file for SHELF_VERSION
 * node at RELPATH, no matter whether it exists.
 */
static svn_error_t *
get_working_props_abspath(char **work_abspath,
                          svn_client__shelf2_version_t *shelf_version,
                          const char *wc_relpath,
                          apr_pool_t *result_pool,
                          apr_pool_t *scratch_pool)
{
  wc_relpath = apr_psprintf(scratch_pool, "%s.work-props", wc_relpath);
  *work_abspath = svn_dirent_join(shelf_version->files_dir_abspath, wc_relpath,
                                  result_pool);
  return SVN_NO_ERROR;
}

/* Delete the storage for SHELF:VERSION. */
static svn_error_t *
shelf_version_delete(svn_client__shelf2_t *shelf,
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
                svn_client__shelf2_t *shelf,
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
shelf_read_revprops(svn_client__shelf2_t *shelf,
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
shelf_write_revprops(svn_client__shelf2_t *shelf,
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
svn_client__shelf2_revprop_set(svn_client__shelf2_t *shelf,
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
svn_client__shelf2_revprop_set_all(svn_client__shelf2_t *shelf,
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
svn_client__shelf2_revprop_get(svn_string_t **prop_val,
                               svn_client__shelf2_t *shelf,
                               const char *prop_name,
                               apr_pool_t *result_pool)
{
  *prop_val = svn_hash_gets(shelf->revprops, prop_name);
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf2_revprop_list(apr_hash_t **props,
                                svn_client__shelf2_t *shelf,
                                apr_pool_t *result_pool)
{
  *props = shelf->revprops;
  return SVN_NO_ERROR;
}

/*  */
static svn_error_t *
get_current_abspath(char **current_abspath,
                    svn_client__shelf2_t *shelf,
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
shelf_read_current(svn_client__shelf2_t *shelf,
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
shelf_write_current(svn_client__shelf2_t *shelf,
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

/* Create a status struct with all fields initialized to valid values
 * representing 'uninteresting' or 'unknown' status.
 */
static svn_wc_status3_t *
status_create(apr_pool_t *result_pool)
{
  svn_wc_status3_t *s = apr_pcalloc(result_pool, sizeof(*s));

  s->filesize = SVN_INVALID_FILESIZE;
  s->versioned = TRUE;
  s->node_status = svn_wc_status_none;
  s->text_status = svn_wc_status_none;
  s->prop_status = svn_wc_status_none;
  s->revision = SVN_INVALID_REVNUM;
  s->changed_rev = SVN_INVALID_REVNUM;
  s->repos_node_status = svn_wc_status_none;
  s->repos_text_status = svn_wc_status_none;
  s->repos_prop_status = svn_wc_status_none;
  s->ood_changed_rev = SVN_INVALID_REVNUM;
  return s;
}

/* Convert from svn_node_kind_t to a single character representation. */
static char
kind_to_char(svn_node_kind_t kind)
{
  return (kind == svn_node_dir ? 'd'
            : kind == svn_node_file ? 'f'
                : kind == svn_node_symlink ? 'l'
                    : '?');
}

/* Convert to svn_node_kind_t from a single character representation. */
static svn_node_kind_t
char_to_kind(char kind)
{
  return (kind == 'd' ? svn_node_dir
            : kind == 'f' ? svn_node_file
                : kind == 'l' ? svn_node_symlink
                    : svn_node_unknown);
}

/* Return the single character representation of STATUS.
 * (Similar to subversion/svn/status.c:generate_status_code()
 * and subversion/tests/libsvn_client/client-test.c:status_to_char().) */
static char
status_to_char(enum svn_wc_status_kind status)
{
  switch (status)
    {
    case svn_wc_status_none:        return '.';
    case svn_wc_status_unversioned: return '?';
    case svn_wc_status_normal:      return ' ';
    case svn_wc_status_added:       return 'A';
    case svn_wc_status_missing:     return '!';
    case svn_wc_status_deleted:     return 'D';
    case svn_wc_status_replaced:    return 'R';
    case svn_wc_status_modified:    return 'M';
    case svn_wc_status_merged:      return 'G';
    case svn_wc_status_conflicted:  return 'C';
    case svn_wc_status_ignored:     return 'I';
    case svn_wc_status_obstructed:  return '~';
    case svn_wc_status_external:    return 'X';
    case svn_wc_status_incomplete:  return ':';
    default:                        return '*';
    }
}

static enum svn_wc_status_kind
char_to_status(char status)
{
  switch (status)
    {
    case '.': return svn_wc_status_none;
    case '?': return svn_wc_status_unversioned;
    case ' ': return svn_wc_status_normal;
    case 'A': return svn_wc_status_added;
    case '!': return svn_wc_status_missing;
    case 'D': return svn_wc_status_deleted;
    case 'R': return svn_wc_status_replaced;
    case 'M': return svn_wc_status_modified;
    case 'G': return svn_wc_status_merged;
    case 'C': return svn_wc_status_conflicted;
    case 'I': return svn_wc_status_ignored;
    case '~': return svn_wc_status_obstructed;
    case 'X': return svn_wc_status_external;
    case ':': return svn_wc_status_incomplete;
    default:  return (enum svn_wc_status_kind)0;
    }
}

/* Write a serial representation of (some fields of) STATUS to STREAM.
 */
static svn_error_t *
wc_status_serialize(svn_stream_t *stream,
                    const svn_wc_status3_t *status,
                    apr_pool_t *scratch_pool)
{
  SVN_ERR(svn_stream_printf(stream, scratch_pool, "%c %c%c%c %ld",
                            kind_to_char(status->kind),
                            status_to_char(status->node_status),
                            status_to_char(status->text_status),
                            status_to_char(status->prop_status),
                            status->revision));
  return SVN_NO_ERROR;
}

/* Read a serial representation of (some fields of) STATUS from STREAM.
 */
static svn_error_t *
wc_status_unserialize(svn_wc_status3_t *status,
                      svn_stream_t *stream,
                      apr_pool_t *result_pool)
{
  svn_stringbuf_t *sb;
  char *string;

  SVN_ERR(svn_stringbuf_from_stream(&sb, stream, 100, result_pool));
  string = sb->data;
  status->kind = char_to_kind(string[0]);
  status->node_status = char_to_status(string[2]);
  status->text_status = char_to_status(string[3]);
  status->prop_status = char_to_status(string[4]);
  sscanf(string + 6, "%ld", &status->revision);
  return SVN_NO_ERROR;
}

/* Write status to shelf storage.
 */
static svn_error_t *
status_write(svn_client__shelf2_version_t *shelf_version,
             const char *relpath,
             const svn_wc_status3_t *status,
             apr_pool_t *scratch_pool)
{
  char *file_abspath;
  svn_stream_t *stream;

  SVN_ERR(get_metadata_abspath(&file_abspath, shelf_version, relpath,
                               scratch_pool, scratch_pool));
  SVN_ERR(svn_stream_open_writable(&stream, file_abspath,
                                   scratch_pool, scratch_pool));
  SVN_ERR(wc_status_serialize(stream, status, scratch_pool));
  SVN_ERR(svn_stream_close(stream));
  return SVN_NO_ERROR;
}

/* Read status from shelf storage.
 */
static svn_error_t *
status_read(svn_wc_status3_t **status,
            svn_client__shelf2_version_t *shelf_version,
            const char *relpath,
            apr_pool_t *result_pool,
            apr_pool_t *scratch_pool)
{
  svn_wc_status3_t *s = status_create(result_pool);
  char *file_abspath;
  svn_stream_t *stream;

  SVN_ERR(get_metadata_abspath(&file_abspath, shelf_version, relpath,
                               scratch_pool, scratch_pool));
  SVN_ERR(svn_stream_open_readonly(&stream, file_abspath,
                                   scratch_pool, scratch_pool));
  SVN_ERR(wc_status_unserialize(s, stream, result_pool));
  SVN_ERR(svn_stream_close(stream));

  s->changelist = apr_psprintf(result_pool, "svn:shelf:%s",
                               shelf_version->shelf->name);
  *status = s;
  return SVN_NO_ERROR;
}

/* A visitor function type for use with shelf_status_walk().
 * The same as svn_wc_status_func4_t except relpath instead of abspath.
 * Only some fields in STATUS are available.
 */
typedef svn_error_t *(*shelf_status_visitor_t)(void *baton,
                                               const char *relpath,
                                               svn_wc_status3_t *status,
                                               apr_pool_t *scratch_pool);

/* Baton for shelved_files_walk_visitor(). */
struct shelf_status_baton_t
{
  svn_client__shelf2_version_t *shelf_version;
  const char *top_relpath;
  const char *walk_root_abspath;
  shelf_status_visitor_t walk_func;
  void *walk_baton;
};

/* Call BATON->walk_func(BATON->walk_baton, relpath, ...) for the shelved
 * 'binary' file stored at ABSPATH.
 * Implements svn_io_walk_func_t. */
static svn_error_t *
shelf_status_visitor(void *baton,
                     const char *abspath,
                     const apr_finfo_t *finfo,
                     apr_pool_t *scratch_pool)
{
  struct shelf_status_baton_t *b = baton;
  const char *relpath;

  relpath = svn_dirent_skip_ancestor(b->walk_root_abspath, abspath);
  if (finfo->filetype == APR_REG
      && (strlen(relpath) >= 5 && strcmp(relpath+strlen(relpath)-5, ".meta") == 0))
    {
      svn_wc_status3_t *s;

      relpath = apr_pstrndup(scratch_pool, relpath, strlen(relpath) - 5);
      if (!svn_relpath_skip_ancestor(b->top_relpath, relpath))
        return SVN_NO_ERROR;

      SVN_ERR(status_read(&s, b->shelf_version, relpath,
                          scratch_pool, scratch_pool));
      SVN_ERR(b->walk_func(b->walk_baton, relpath, s, scratch_pool));
    }
  return SVN_NO_ERROR;
}

/* Report the shelved status of the path SHELF_VERSION:WC_RELPATH
 * via WALK_FUNC(WALK_BATON, ...).
 */
static svn_error_t *
shelf_status_visit_path(svn_client__shelf2_version_t *shelf_version,
                        const char *wc_relpath,
                        shelf_status_visitor_t walk_func,
                        void *walk_baton,
                        apr_pool_t *scratch_pool)
{
  struct shelf_status_baton_t baton;
  char *abspath;
  apr_finfo_t finfo;

  baton.shelf_version = shelf_version;
  baton.top_relpath = wc_relpath;
  baton.walk_root_abspath = shelf_version->files_dir_abspath;
  baton.walk_func = walk_func;
  baton.walk_baton = walk_baton;
  SVN_ERR(get_metadata_abspath(&abspath, shelf_version, wc_relpath,
                               scratch_pool, scratch_pool));
  SVN_ERR(svn_io_stat(&finfo, abspath, APR_FINFO_TYPE, scratch_pool));
  SVN_ERR(shelf_status_visitor(&baton, abspath, &finfo, scratch_pool));
  return SVN_NO_ERROR;
}

/* Report the shelved status of all the shelved paths in SHELF_VERSION
 * via WALK_FUNC(WALK_BATON, ...).
 */
static svn_error_t *
shelf_status_walk(svn_client__shelf2_version_t *shelf_version,
                  const char *wc_relpath,
                  shelf_status_visitor_t walk_func,
                  void *walk_baton,
                  apr_pool_t *scratch_pool)
{
  struct shelf_status_baton_t baton;
  svn_error_t *err;

  baton.shelf_version = shelf_version;
  baton.top_relpath = wc_relpath;
  baton.walk_root_abspath = shelf_version->files_dir_abspath;
  baton.walk_func = walk_func;
  baton.walk_baton = walk_baton;
  err = svn_io_dir_walk2(baton.walk_root_abspath, 0 /*wanted*/,
                         shelf_status_visitor, &baton,
                         scratch_pool);
  if (err && APR_STATUS_IS_ENOENT(err->apr_err))
    svn_error_clear(err);
  else
    SVN_ERR(err);

  return SVN_NO_ERROR;
}

typedef struct wc_status_baton_t
{
  svn_client__shelf2_version_t *shelf_version;
  svn_wc_status_func4_t walk_func;
  void *walk_baton;
} wc_status_baton_t;

static svn_error_t *
wc_status_visitor(void *baton,
                      const char *relpath,
                      svn_wc_status3_t *status,
                      apr_pool_t *scratch_pool)
{
  struct wc_status_baton_t *b = baton;
  svn_client__shelf2_t *shelf = b->shelf_version->shelf;
  const char *abspath = svn_dirent_join(shelf->wc_root_abspath, relpath,
                                        scratch_pool);
  SVN_ERR(b->walk_func(b->walk_baton, abspath, status, scratch_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf2_version_status_walk(svn_client__shelf2_version_t *shelf_version,
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

/* A baton for use with write_changes_visitor(). */
typedef struct write_changes_baton_t {
  const char *wc_root_abspath;
  svn_client__shelf2_version_t *shelf_version;
  svn_client_ctx_t *ctx;
  svn_boolean_t any_shelved;  /* were any paths successfully shelved? */
  svn_client_status_func_t was_shelved_func;
  void *was_shelved_baton;
  svn_client_status_func_t was_not_shelved_func;
  void *was_not_shelved_baton;
  apr_pool_t *pool;  /* pool for data in 'unshelvable', etc. */
} write_changes_baton_t;

/*  */
static svn_error_t *
notify_shelved(write_changes_baton_t *wb,
               const char *wc_relpath,
               const char *local_abspath,
               const svn_wc_status3_t *wc_status,
               apr_pool_t *scratch_pool)
{
  if (wb->was_shelved_func)
    {
      svn_client_status_t *cst;

      SVN_ERR(svn_client__create_status(&cst, wb->ctx->wc_ctx, local_abspath,
                                        wc_status,
                                        scratch_pool, scratch_pool));
      SVN_ERR(wb->was_shelved_func(wb->was_shelved_baton,
                                   wc_relpath, cst, scratch_pool));
    }

  wb->any_shelved = TRUE;
  return SVN_NO_ERROR;
}

/*  */
static svn_error_t *
notify_not_shelved(write_changes_baton_t *wb,
                   const char *wc_relpath,
                   const char *local_abspath,
                   const svn_wc_status3_t *wc_status,
                   apr_pool_t *scratch_pool)
{
  if (wb->was_not_shelved_func)
    {
      svn_client_status_t *cst;

      SVN_ERR(svn_client__create_status(&cst, wb->ctx->wc_ctx, local_abspath,
                                        wc_status,
                                        scratch_pool, scratch_pool));
      SVN_ERR(wb->was_not_shelved_func(wb->was_not_shelved_baton,
                                       wc_relpath, cst, scratch_pool));
    }

  return SVN_NO_ERROR;
}

/* Read BASE_PROPS and WORK_PROPS from the WC, setting each to null if
 * the node has no base or working version (respectively).
 */
static svn_error_t *
read_props_from_wc(apr_hash_t **base_props,
                   apr_hash_t **work_props,
                   enum svn_wc_status_kind node_status,
                   const char *from_wc_abspath,
                   svn_client_ctx_t *ctx,
                   apr_pool_t *result_pool,
                   apr_pool_t *scratch_pool)
{
  if (node_status != svn_wc_status_added)
    SVN_ERR(svn_wc_get_pristine_props(base_props, ctx->wc_ctx, from_wc_abspath,
                                      result_pool, scratch_pool));
  else
    *base_props = NULL;
  if (node_status != svn_wc_status_deleted)
    SVN_ERR(svn_wc_prop_list2(work_props, ctx->wc_ctx, from_wc_abspath,
                              result_pool, scratch_pool));
  else
    *work_props = NULL;
  return SVN_NO_ERROR;
}

/* Write BASE_PROPS and WORK_PROPS to storage in SHELF_VERSION:WC_RELPATH.
 */
static svn_error_t *
write_props_to_shelf(svn_client__shelf2_version_t *shelf_version,
                     const char *wc_relpath,
                     apr_hash_t *base_props,
                     apr_hash_t *work_props,
                     apr_pool_t *scratch_pool)
{
  char *stored_props_abspath;
  svn_stream_t *stream;

  if (base_props)
    {
      SVN_ERR(get_base_props_abspath(&stored_props_abspath,
                                     shelf_version, wc_relpath,
                                     scratch_pool, scratch_pool));
      SVN_ERR(svn_stream_open_writable(&stream, stored_props_abspath,
                                       scratch_pool, scratch_pool));
      SVN_ERR(svn_hash_write2(base_props, stream, NULL, scratch_pool));
      SVN_ERR(svn_stream_close(stream));
    }

  if (work_props)
    {
      SVN_ERR(get_working_props_abspath(&stored_props_abspath,
                                        shelf_version, wc_relpath,
                                        scratch_pool, scratch_pool));
      SVN_ERR(svn_stream_open_writable(&stream, stored_props_abspath,
                                       scratch_pool, scratch_pool));
      SVN_ERR(svn_hash_write2(work_props, stream, NULL, scratch_pool));
      SVN_ERR(svn_stream_close(stream));
    }

  return SVN_NO_ERROR;
}

/* Read BASE_PROPS and WORK_PROPS from storage in SHELF_VERSION:WC_RELPATH.
 */
static svn_error_t *
read_props_from_shelf(apr_hash_t **base_props,
                      apr_hash_t **work_props,
                      enum svn_wc_status_kind node_status,
                      svn_client__shelf2_version_t *shelf_version,
                      const char *wc_relpath,
                      apr_pool_t *result_pool,
                      apr_pool_t *scratch_pool)
{
  char *stored_props_abspath;
  svn_stream_t *stream;

  if (node_status != svn_wc_status_added)
    {
      *base_props = apr_hash_make(result_pool);
      SVN_ERR(get_base_props_abspath(&stored_props_abspath,
                                     shelf_version, wc_relpath,
                                     scratch_pool, scratch_pool));
      SVN_ERR(svn_stream_open_readonly(&stream, stored_props_abspath,
                                       scratch_pool, scratch_pool));
      SVN_ERR(svn_hash_read2(*base_props, stream, NULL, scratch_pool));
      SVN_ERR(svn_stream_close(stream));
    }
  else
    *base_props = NULL;

  if (node_status != svn_wc_status_deleted)
    {
      *work_props = apr_hash_make(result_pool);
      SVN_ERR(get_working_props_abspath(&stored_props_abspath,
                                        shelf_version, wc_relpath,
                                        scratch_pool, scratch_pool));
      SVN_ERR(svn_stream_open_readonly(&stream, stored_props_abspath,
                                       scratch_pool, scratch_pool));
      SVN_ERR(svn_hash_read2(*work_props, stream, NULL, scratch_pool));
      SVN_ERR(svn_stream_close(stream));
    }
  else
    *work_props = NULL;

  return SVN_NO_ERROR;
}

/* Store metadata for any node, and base and working files if it's a file.
 *
 * Copy the WC base and working files at FROM_WC_ABSPATH to the storage
 * area in SHELF_VERSION.
 */
static svn_error_t *
store_file(const char *from_wc_abspath,
           const char *wc_relpath,
           svn_client__shelf2_version_t *shelf_version,
           const svn_wc_status3_t *status,
           svn_client_ctx_t *ctx,
           apr_pool_t *scratch_pool)
{
  char *stored_abspath;
  apr_hash_t *base_props, *work_props;

  SVN_ERR(get_working_file_abspath(&stored_abspath,
                                   shelf_version, wc_relpath,
                                   scratch_pool, scratch_pool));
  SVN_ERR(svn_io_make_dir_recursively(svn_dirent_dirname(stored_abspath,
                                                         scratch_pool),
                                      scratch_pool));
  SVN_ERR(status_write(shelf_version, wc_relpath,
                       status, scratch_pool));

  /* properties */
  SVN_ERR(read_props_from_wc(&base_props, &work_props,
                             status->node_status,
                             from_wc_abspath, ctx,
                             scratch_pool, scratch_pool));
  SVN_ERR(write_props_to_shelf(shelf_version, wc_relpath,
                               base_props, work_props,
                               scratch_pool));

  /* file text */
  if (status->kind == svn_node_file)
    {
      svn_stream_t *wc_base_stream;
      svn_node_kind_t work_kind;

      /* Copy the base file (copy-from base, if copied/moved), if present */
      SVN_ERR(svn_wc_get_pristine_contents2(&wc_base_stream,
                                            ctx->wc_ctx, from_wc_abspath,
                                            scratch_pool, scratch_pool));
      if (wc_base_stream)
        {
          char *stored_base_abspath;
          svn_stream_t *stored_base_stream;

          SVN_ERR(get_base_file_abspath(&stored_base_abspath,
                                        shelf_version, wc_relpath,
                                        scratch_pool, scratch_pool));
          SVN_ERR(svn_stream_open_writable(&stored_base_stream,
                                           stored_base_abspath,
                                           scratch_pool, scratch_pool));
          SVN_ERR(svn_stream_copy3(wc_base_stream, stored_base_stream,
                                   NULL, NULL, scratch_pool));
        }

      /* Copy the working file, if present */
      SVN_ERR(svn_io_check_path(from_wc_abspath, &work_kind, scratch_pool));
      if (work_kind == svn_node_file)
        {
          SVN_ERR(svn_io_copy_file(from_wc_abspath, stored_abspath,
                                   TRUE /*copy_perms*/, scratch_pool));
        }
    }
  return SVN_NO_ERROR;
}

/* An implementation of svn_wc_status_func4_t. */
static svn_error_t *
write_changes_visitor(void *baton,
                      const char *local_abspath,
                      const svn_wc_status3_t *status,
                      apr_pool_t *scratch_pool)
{
  write_changes_baton_t *wb = baton;
  const char *wc_relpath = svn_dirent_skip_ancestor(wb->wc_root_abspath,
                                                    local_abspath);

  /* Catch any conflict, even a tree conflict on a path that has
     node-status 'unversioned'. */
  if (status->conflicted)
    {
      SVN_ERR(notify_not_shelved(wb, wc_relpath, local_abspath,
                                 status, scratch_pool));
    }
  else switch (status->node_status)
    {
      case svn_wc_status_deleted:
      case svn_wc_status_added:
      case svn_wc_status_replaced:
        if (status->kind != svn_node_file
            || status->copied)
          {
            SVN_ERR(notify_not_shelved(wb, wc_relpath, local_abspath,
                                       status, scratch_pool));
            break;
          }
        /* fall through */
      case svn_wc_status_modified:
      {
        /* Store metadata, and base and working versions if it's a file */
        SVN_ERR(store_file(local_abspath, wc_relpath, wb->shelf_version,
                           status, wb->ctx, scratch_pool));
        SVN_ERR(notify_shelved(wb, wc_relpath, local_abspath,
                               status, scratch_pool));
        break;
      }

      case svn_wc_status_incomplete:
        if ((status->text_status != svn_wc_status_normal
             && status->text_status != svn_wc_status_none)
            || (status->prop_status != svn_wc_status_normal
                && status->prop_status != svn_wc_status_none))
          {
            /* Incomplete, but local modifications */
            SVN_ERR(notify_not_shelved(wb, wc_relpath, local_abspath,
                                       status, scratch_pool));
          }
        break;

      case svn_wc_status_conflicted:
      case svn_wc_status_missing:
      case svn_wc_status_obstructed:
        SVN_ERR(notify_not_shelved(wb, wc_relpath, local_abspath,
                                   status, scratch_pool));
        break;

      case svn_wc_status_normal:
      case svn_wc_status_ignored:
      case svn_wc_status_none:
      case svn_wc_status_external:
      case svn_wc_status_unversioned:
      default:
        break;
    }

  return SVN_NO_ERROR;
}

/* A baton for use with changelist_filter_func(). */
struct changelist_filter_baton_t {
  apr_hash_t *changelist_hash;
  svn_wc_status_func4_t status_func;
  void *status_baton;
};

/* Filter out paths that are not in the requested changelist(s).
 * Implements svn_wc_status_func4_t. */
static svn_error_t *
changelist_filter_func(void *baton,
                       const char *local_abspath,
                       const svn_wc_status3_t *status,
                       apr_pool_t *scratch_pool)
{
  struct changelist_filter_baton_t *b = baton;

  if (b->changelist_hash
      && (! status->changelist
          || ! svn_hash_gets(b->changelist_hash, status->changelist)))
    {
      return SVN_NO_ERROR;
    }

  SVN_ERR(b->status_func(b->status_baton, local_abspath, status,
                         scratch_pool));
  return SVN_NO_ERROR;
}

/*
 * Walk the WC tree(s) rooted at PATHS, to depth DEPTH, omitting paths that
 * are not in one of the CHANGELISTS (if not null).
 *
 * Call STATUS_FUNC(STATUS_BATON, ...) for each visited path.
 *
 * PATHS are absolute, or relative to CWD.
 */
static svn_error_t *
wc_walk_status_multi(const apr_array_header_t *paths,
                     svn_depth_t depth,
                     const apr_array_header_t *changelists,
                     svn_wc_status_func4_t status_func,
                     void *status_baton,
                     svn_client_ctx_t *ctx,
                     apr_pool_t *scratch_pool)
{
  struct changelist_filter_baton_t cb = {0};
  int i;

  if (changelists && changelists->nelts)
    SVN_ERR(svn_hash_from_cstring_keys(&cb.changelist_hash,
                                       changelists, scratch_pool));
  cb.status_func = status_func;
  cb.status_baton = status_baton;

  for (i = 0; i < paths->nelts; i++)
    {
      const char *path = APR_ARRAY_IDX(paths, i, const char *);

      if (svn_path_is_url(path))
        return svn_error_createf(SVN_ERR_ILLEGAL_TARGET, NULL,
                                 _("'%s' is not a local path"), path);
      SVN_ERR(svn_dirent_get_absolute(&path, path, scratch_pool));

      SVN_ERR(svn_wc_walk_status(ctx->wc_ctx, path, depth,
                                 FALSE /*get_all*/, FALSE /*no_ignore*/,
                                 FALSE /*ignore_text_mods*/,
                                 NULL /*ignore_patterns*/,
                                 changelist_filter_func, &cb,
                                 ctx->cancel_func, ctx->cancel_baton,
                                 scratch_pool));
    }

  return SVN_NO_ERROR;
}

/** Write local changes to the shelf storage.
 *
 * @a paths, @a depth, @a changelists: The selection of local paths to diff.
 *
 * @a paths are relative to CWD (or absolute).
 */
static svn_error_t *
shelf_write_changes(svn_boolean_t *any_shelved,
                    svn_client__shelf2_version_t *shelf_version,
                    const apr_array_header_t *paths,
                    svn_depth_t depth,
                    const apr_array_header_t *changelists,
                    svn_client_status_func_t shelved_func,
                    void *shelved_baton,
                    svn_client_status_func_t not_shelved_func,
                    void *not_shelved_baton,
                    const char *wc_root_abspath,
                    svn_client_ctx_t *ctx,
                    apr_pool_t *result_pool,
                    apr_pool_t *scratch_pool)
{
  write_changes_baton_t wb = { 0 };

  wb.wc_root_abspath = wc_root_abspath;
  wb.shelf_version = shelf_version;
  wb.ctx = ctx;
  wb.any_shelved = FALSE;
  wb.was_shelved_func = shelved_func;
  wb.was_shelved_baton = shelved_baton;
  wb.was_not_shelved_func = not_shelved_func;
  wb.was_not_shelved_baton = not_shelved_baton;
  wb.pool = result_pool;

  /* Walk the WC */
  SVN_ERR(wc_walk_status_multi(paths, depth, changelists,
                               write_changes_visitor, &wb,
                               ctx, scratch_pool));

  *any_shelved = wb.any_shelved;
  return SVN_NO_ERROR;
}

/* Construct a shelf object representing an empty shelf: no versions,
 * no revprops, no looking to see if such a shelf exists on disk.
 */
static svn_error_t *
shelf_construct(svn_client__shelf2_t **shelf_p,
                const char *name,
                const char *local_abspath,
                svn_client_ctx_t *ctx,
                apr_pool_t *result_pool)
{
  svn_client__shelf2_t *shelf = apr_palloc(result_pool, sizeof(*shelf));
  char *shelves_dir;

  SVN_ERR(svn_client_get_wc_root(&shelf->wc_root_abspath,
                                 local_abspath, ctx,
                                 result_pool, result_pool));
  SVN_ERR(get_shelves_dir(&shelves_dir,
                          ctx->wc_ctx, local_abspath,
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
svn_client__shelf2_open_existing(svn_client__shelf2_t **shelf_p,
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
svn_client__shelf2_open_or_create(svn_client__shelf2_t **shelf_p,
                                  const char *name,
                                  const char *local_abspath,
                                  svn_client_ctx_t *ctx,
                                  apr_pool_t *result_pool)
{
  svn_client__shelf2_t *shelf;

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
svn_client__shelf2_close(svn_client__shelf2_t *shelf,
                         apr_pool_t *scratch_pool)
{
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf2_delete(const char *name,
                          const char *local_abspath,
                          svn_boolean_t dry_run,
                          svn_client_ctx_t *ctx,
                          apr_pool_t *scratch_pool)
{
  svn_client__shelf2_t *shelf;
  int i;
  char *abspath;

  SVN_ERR(svn_client__shelf2_open_existing(&shelf, name,
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

  SVN_ERR(svn_client__shelf2_close(shelf, scratch_pool));
  return SVN_NO_ERROR;
}

/* Baton for paths_changed_visitor(). */
struct paths_changed_walk_baton_t
{
  apr_hash_t *paths_hash;
  svn_boolean_t as_abspath;
  const char *wc_root_abspath;
  apr_pool_t *pool;
};

/* Add to the list(s) in BATON, the RELPATH of a shelved 'binary' file.
 * Implements shelved_files_walk_func_t. */
static svn_error_t *
paths_changed_visitor(void *baton,
                      const char *relpath,
                      svn_wc_status3_t *s,
                      apr_pool_t *scratch_pool)
{
  struct paths_changed_walk_baton_t *b = baton;

  relpath = (b->as_abspath
             ? svn_dirent_join(b->wc_root_abspath, relpath, b->pool)
             : apr_pstrdup(b->pool, relpath));
  svn_hash_sets(b->paths_hash, relpath, relpath);
  return SVN_NO_ERROR;
}

/* Get the paths changed, relative to WC root or as abspaths, as a hash
 * and/or an array (in no particular order).
 */
static svn_error_t *
shelf_paths_changed(apr_hash_t **paths_hash_p,
                    apr_array_header_t **paths_array_p,
                    svn_client__shelf2_version_t *shelf_version,
                    svn_boolean_t as_abspath,
                    apr_pool_t *result_pool,
                    apr_pool_t *scratch_pool)
{
  svn_client__shelf2_t *shelf = shelf_version->shelf;
  apr_hash_t *paths_hash = apr_hash_make(result_pool);
  struct paths_changed_walk_baton_t baton;

  baton.paths_hash = paths_hash;
  baton.as_abspath = as_abspath;
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
svn_client__shelf2_paths_changed(apr_hash_t **affected_paths,
                                 svn_client__shelf2_version_t *shelf_version,
                                 apr_pool_t *result_pool,
                                 apr_pool_t *scratch_pool)
{
  SVN_ERR(shelf_paths_changed(affected_paths, NULL, shelf_version,
                              FALSE /*as_abspath*/,
                              result_pool, scratch_pool));
  return SVN_NO_ERROR;
}

/* Send a notification */
static svn_error_t *
send_notification(const char *local_abspath,
                  svn_wc_notify_action_t action,
                  svn_node_kind_t kind,
                  svn_wc_notify_state_t content_state,
                  svn_wc_notify_state_t prop_state,
                  svn_wc_notify_func2_t notify_func,
                  void *notify_baton,
                  apr_pool_t *scratch_pool)
{
  if (notify_func)
    {
      svn_wc_notify_t *notify
        = svn_wc_create_notify(local_abspath, action, scratch_pool);

      notify->kind = kind;
      notify->content_state = content_state;
      notify->prop_state = prop_state;
      notify_func(notify_baton, notify, scratch_pool);
    }

  return SVN_NO_ERROR;
}

/* Merge a shelved change into WC_ABSPATH.
 */
static svn_error_t *
wc_file_merge(const char *wc_abspath,
              const char *left_file,
              const char *right_file,
              /*const*/ apr_hash_t *left_props,
              /*const*/ apr_hash_t *right_props,
              svn_client_ctx_t *ctx,
              apr_pool_t *scratch_pool)
{
  svn_wc_notify_state_t property_state;
  svn_boolean_t has_local_mods;
  enum svn_wc_merge_outcome_t content_outcome;
  const char *target_label, *left_label, *right_label;
  apr_array_header_t *prop_changes;

  /* xgettext: the '.working', '.merge-left' and '.merge-right' strings
     are used to tag onto a file name in case of a merge conflict */
  target_label = apr_psprintf(scratch_pool, _(".working"));
  left_label = apr_psprintf(scratch_pool, _(".merge-left"));
  right_label = apr_psprintf(scratch_pool, _(".merge-right"));

  SVN_ERR(svn_prop_diffs(&prop_changes, right_props, left_props, scratch_pool));
  SVN_ERR(svn_wc_text_modified_p2(&has_local_mods, ctx->wc_ctx,
                                  wc_abspath, FALSE, scratch_pool));

  /* Do property merge and text merge in one step so that keyword expansion
     takes into account the new property values. */
  SVN_WC__CALL_WITH_WRITE_LOCK(
    svn_wc_merge5(&content_outcome, &property_state, ctx->wc_ctx,
                  left_file, right_file, wc_abspath,
                  left_label, right_label, target_label,
                  NULL, NULL, /*left, right conflict-versions*/
                  FALSE /*dry_run*/, NULL /*diff3_cmd*/,
                  NULL /*merge_options*/,
                  left_props, prop_changes,
                  NULL, NULL,
                  ctx->cancel_func, ctx->cancel_baton,
                  scratch_pool),
    ctx->wc_ctx, wc_abspath,
    FALSE /*lock_anchor*/, scratch_pool);

  return SVN_NO_ERROR;
}

/* Merge a shelved change (of properties) into the dir at WC_ABSPATH.
 */
static svn_error_t *
wc_dir_props_merge(const char *wc_abspath,
                   /*const*/ apr_hash_t *left_props,
                   /*const*/ apr_hash_t *right_props,
                   svn_client_ctx_t *ctx,
                   apr_pool_t *result_pool,
                   apr_pool_t *scratch_pool)
{
  apr_array_header_t *prop_changes;
  svn_wc_notify_state_t property_state;

  SVN_ERR(svn_prop_diffs(&prop_changes, right_props, left_props, scratch_pool));
  SVN_WC__CALL_WITH_WRITE_LOCK(
    svn_wc_merge_props3(&property_state, ctx->wc_ctx,
                        wc_abspath,
                        NULL, NULL, /*left, right conflict-versions*/
                        left_props, prop_changes,
                        FALSE /*dry_run*/,
                        NULL, NULL,
                        ctx->cancel_func, ctx->cancel_baton,
                        scratch_pool),
    ctx->wc_ctx, wc_abspath,
    FALSE /*lock_anchor*/, scratch_pool);

  return SVN_NO_ERROR;
}

/* Apply a shelved "delete" to TO_WC_ABSPATH.
 */
static svn_error_t *
wc_node_delete(const char *to_wc_abspath,
               svn_client_ctx_t *ctx,
               apr_pool_t *scratch_pool)
{
  SVN_WC__CALL_WITH_WRITE_LOCK(
    svn_wc_delete4(ctx->wc_ctx,
                   to_wc_abspath,
                   FALSE /*keep_local*/,
                   TRUE /*delete_unversioned_target*/,
                   NULL, NULL, NULL, NULL, /*cancel, notify*/
                   scratch_pool),
    ctx->wc_ctx, to_wc_abspath,
    TRUE /*lock_anchor*/, scratch_pool);
  return SVN_NO_ERROR;
}

/* Apply a shelved "add" to TO_WC_ABSPATH.
 * The node must already exist on disk, in a versioned parent dir.
 */
static svn_error_t *
wc_node_add(const char *to_wc_abspath,
            apr_hash_t *work_props,
            svn_client_ctx_t *ctx,
            apr_pool_t *scratch_pool)
{
  /* If it was not already versioned, schedule the node for addition.
     (Do not apply autoprops, because this isn't a user-facing "add" but
     restoring a previously saved state.) */
  SVN_WC__CALL_WITH_WRITE_LOCK(
    svn_wc_add_from_disk3(ctx->wc_ctx,
                          to_wc_abspath, work_props,
                          FALSE /* skip checks */,
                          NULL, NULL, scratch_pool),
    ctx->wc_ctx, to_wc_abspath,
    TRUE /*lock_anchor*/, scratch_pool);
  return SVN_NO_ERROR;
}

/* Baton for apply_file_visitor(). */
struct apply_files_baton_t
{
  svn_client__shelf2_version_t *shelf_version;
  svn_boolean_t test_only;  /* only check whether it would conflict */
  svn_boolean_t conflict;  /* would it conflict? */
  svn_client_ctx_t *ctx;
};

/* Copy the file RELPATH from shelf binary file storage to the WC.
 *
 * If it is not already versioned, schedule the file for addition.
 *
 * Make any missing parent directories.
 *
 * In test mode (BATON->test_only): set BATON->conflict if we can't apply
 * the change to WC at RELPATH without conflict. But in fact, just check
 * if WC at RELPATH is locally modified.
 *
 * Implements shelved_files_walk_func_t. */
static svn_error_t *
apply_file_visitor(void *baton,
                   const char *relpath,
                   svn_wc_status3_t *s,
                   apr_pool_t *scratch_pool)
{
  struct apply_files_baton_t *b = baton;
  const char *wc_root_abspath = b->shelf_version->shelf->wc_root_abspath;
  char *stored_base_abspath, *stored_work_abspath;
  apr_hash_t *base_props, *work_props;
  const char *to_wc_abspath = svn_dirent_join(wc_root_abspath, relpath,
                                              scratch_pool);
  const char *to_dir_abspath = svn_dirent_dirname(to_wc_abspath, scratch_pool);

  SVN_ERR(get_base_file_abspath(&stored_base_abspath,
                                b->shelf_version, relpath,
                                scratch_pool, scratch_pool));
  SVN_ERR(get_working_file_abspath(&stored_work_abspath,
                                   b->shelf_version, relpath,
                                   scratch_pool, scratch_pool));
  SVN_ERR(read_props_from_shelf(&base_props, &work_props,
                                s->node_status,
                                b->shelf_version, relpath,
                                scratch_pool, scratch_pool));

  if (b->test_only)
    {
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

  /* Handle 'delete' and the delete half of 'replace' */
  if (s->node_status == svn_wc_status_deleted
      || s->node_status == svn_wc_status_replaced)
    {
      SVN_ERR(wc_node_delete(to_wc_abspath, b->ctx, scratch_pool));
      if (s->node_status != svn_wc_status_replaced)
        {
          SVN_ERR(send_notification(to_wc_abspath, svn_wc_notify_update_delete,
                                    s->kind,
                                    svn_wc_notify_state_inapplicable,
                                    svn_wc_notify_state_inapplicable,
                                    b->ctx->notify_func2, b->ctx->notify_baton2,
                                    scratch_pool));
        }
    }

  /* If we can merge a file, do so. */
  if (s->node_status == svn_wc_status_modified)
    {
      if (s->kind == svn_node_dir)
        {
          SVN_ERR(wc_dir_props_merge(to_wc_abspath,
                                     base_props, work_props,
                                     b->ctx, scratch_pool, scratch_pool));
        }
      else if (s->kind == svn_node_file)
        {
          SVN_ERR(wc_file_merge(to_wc_abspath,
                                stored_base_abspath, stored_work_abspath,
                                base_props, work_props,
                                b->ctx, scratch_pool));
        }
      SVN_ERR(send_notification(to_wc_abspath, svn_wc_notify_update_update,
                                s->kind,
                                (s->kind == svn_node_dir)
                                  ? svn_wc_notify_state_inapplicable
                                  : svn_wc_notify_state_merged,
                                (s->kind == svn_node_dir)
                                  ? svn_wc_notify_state_merged
                                  : svn_wc_notify_state_unknown,
                                b->ctx->notify_func2, b->ctx->notify_baton2,
                                scratch_pool));
    }

  /* For an added file, copy it into the WC and ensure it's versioned. */
  if (s->node_status == svn_wc_status_added
      || s->node_status == svn_wc_status_replaced)
    {
      if (s->kind == svn_node_dir)
        {
          SVN_ERR(svn_io_make_dir_recursively(to_wc_abspath, scratch_pool));
        }
      else if (s->kind == svn_node_file)
        {
          SVN_ERR(svn_io_make_dir_recursively(to_dir_abspath, scratch_pool));
          SVN_ERR(svn_io_copy_file(stored_work_abspath, to_wc_abspath,
                                   TRUE /*copy_perms*/, scratch_pool));
        }
      SVN_ERR(wc_node_add(to_wc_abspath, work_props, b->ctx, scratch_pool));
      SVN_ERR(send_notification(to_wc_abspath,
                                (s->node_status == svn_wc_status_replaced)
                                  ? svn_wc_notify_update_replace
                                  : svn_wc_notify_update_add,
                                s->kind,
                                svn_wc_notify_state_inapplicable,
                                svn_wc_notify_state_inapplicable,
                                b->ctx->notify_func2, b->ctx->notify_baton2,
                                scratch_pool));
    }

  return SVN_NO_ERROR;
}

/*-------------------------------------------------------------------------*/
/* Diff */

/*  */
static svn_error_t *
file_changed(svn_client__shelf2_version_t *shelf_version,
             const char *relpath,
             svn_wc_status3_t *s,
             const svn_diff_tree_processor_t *diff_processor,
             svn_diff_source_t *left_source,
             svn_diff_source_t *right_source,
             const char *left_stored_abspath,
             const char *right_stored_abspath,
             void *dir_baton,
             apr_pool_t *scratch_pool)
{
  void *fb;
  svn_boolean_t skip = FALSE;

  SVN_ERR(diff_processor->file_opened(&fb, &skip, relpath,
                                      left_source, right_source,
                                      NULL /*copyfrom*/,
                                      dir_baton, diff_processor,
                                      scratch_pool, scratch_pool));
  if (!skip)
    {
      apr_hash_t *left_props, *right_props;
      apr_array_header_t *prop_changes;

      SVN_ERR(read_props_from_shelf(&left_props, &right_props,
                                    s->node_status, shelf_version, relpath,
                                    scratch_pool, scratch_pool));
      SVN_ERR(svn_prop_diffs(&prop_changes, right_props, left_props,
                             scratch_pool));
      SVN_ERR(diff_processor->file_changed(
                relpath,
                left_source, right_source,
                left_stored_abspath, right_stored_abspath,
                left_props, right_props,
                TRUE /*file_modified*/, prop_changes,
                fb, diff_processor, scratch_pool));
    }

  return SVN_NO_ERROR;
}

/*  */
static svn_error_t *
file_deleted(svn_client__shelf2_version_t *shelf_version,
             const char *relpath,
             svn_wc_status3_t *s,
             const svn_diff_tree_processor_t *diff_processor,
             svn_diff_source_t *left_source,
             const char *left_stored_abspath,
             void *dir_baton,
             apr_pool_t *scratch_pool)
{
  void *fb;
  svn_boolean_t skip = FALSE;

  SVN_ERR(diff_processor->file_opened(&fb, &skip, relpath,
                                      left_source, NULL, NULL /*copyfrom*/,
                                      dir_baton, diff_processor,
                                      scratch_pool, scratch_pool));
  if (!skip)
    {
      apr_hash_t *left_props, *right_props;

      SVN_ERR(read_props_from_shelf(&left_props, &right_props,
                                    s->node_status, shelf_version, relpath,
                                    scratch_pool, scratch_pool));
      SVN_ERR(diff_processor->file_deleted(relpath,
                                           left_source,
                                           left_stored_abspath,
                                           left_props,
                                           fb, diff_processor,
                                           scratch_pool));
    }

  return SVN_NO_ERROR;
}

/*  */
static svn_error_t *
file_added(svn_client__shelf2_version_t *shelf_version,
           const char *relpath,
           svn_wc_status3_t *s,
           const svn_diff_tree_processor_t *diff_processor,
           svn_diff_source_t *right_source,
           const char *right_stored_abspath,
           void *dir_baton,
           apr_pool_t *scratch_pool)
{
  void *fb;
  svn_boolean_t skip = FALSE;

  SVN_ERR(diff_processor->file_opened(&fb, &skip, relpath,
                                      NULL, right_source, NULL /*copyfrom*/,
                                      dir_baton, diff_processor,
                                      scratch_pool, scratch_pool));
  if (!skip)
    {
      apr_hash_t *left_props, *right_props;

      SVN_ERR(read_props_from_shelf(&left_props, &right_props,
                                    s->node_status, shelf_version, relpath,
                                    scratch_pool, scratch_pool));
      SVN_ERR(diff_processor->file_added(
                relpath,
                NULL /*copyfrom_source*/, right_source,
                NULL /*copyfrom_abspath*/, right_stored_abspath,
                NULL /*copyfrom_props*/, right_props,
                fb, diff_processor, scratch_pool));
    }

  return SVN_NO_ERROR;
}

/* Baton for diff_visitor(). */
struct diff_baton_t
{
  svn_client__shelf2_version_t *shelf_version;
  const char *top_relpath;  /* top of diff, relative to shelf */
  const char *walk_root_abspath;
  const svn_diff_tree_processor_t *diff_processor;
};

/* Drive BATON->diff_processor.
 * Implements svn_io_walk_func_t. */
static svn_error_t *
diff_visitor(void *baton,
             const char *abspath,
             const apr_finfo_t *finfo,
             apr_pool_t *scratch_pool)
{
  struct diff_baton_t *b = baton;
  const char *relpath;

  relpath = svn_dirent_skip_ancestor(b->walk_root_abspath, abspath);
  if (finfo->filetype == APR_REG
           && (strlen(relpath) >= 5 && strcmp(relpath+strlen(relpath)-5, ".meta") == 0))
    {
      svn_wc_status3_t *s;
      void *db = NULL;
      svn_diff_source_t *left_source;
      svn_diff_source_t *right_source;
      char *left_stored_abspath, *right_stored_abspath;

      relpath = apr_pstrndup(scratch_pool, relpath, strlen(relpath) - 5);
      if (!svn_relpath_skip_ancestor(b->top_relpath, relpath))
        return SVN_NO_ERROR;

      SVN_ERR(status_read(&s, b->shelf_version, relpath,
                          scratch_pool, scratch_pool));

      left_source = svn_diff__source_create(s->revision, scratch_pool);
      right_source = svn_diff__source_create(SVN_INVALID_REVNUM, scratch_pool);
      SVN_ERR(get_base_file_abspath(&left_stored_abspath,
                                    b->shelf_version, relpath,
                                    scratch_pool, scratch_pool));
      SVN_ERR(get_working_file_abspath(&right_stored_abspath,
                                       b->shelf_version, relpath,
                                       scratch_pool, scratch_pool));

      switch (s->node_status)
        {
        case svn_wc_status_modified:
          SVN_ERR(file_changed(b->shelf_version, relpath, s,
                               b->diff_processor,
                               left_source, right_source,
                               left_stored_abspath, right_stored_abspath,
                               db, scratch_pool));
          break;
        case svn_wc_status_added:
          SVN_ERR(file_added(b->shelf_version, relpath, s,
                             b->diff_processor,
                             right_source, right_stored_abspath,
                             db, scratch_pool));
          break;
        case svn_wc_status_deleted:
          SVN_ERR(file_deleted(b->shelf_version, relpath, s,
                               b->diff_processor,
                               left_source, left_stored_abspath,
                               db, scratch_pool));
          break;
        case svn_wc_status_replaced:
          SVN_ERR(file_deleted(b->shelf_version, relpath, s,
                               b->diff_processor,
                               left_source, left_stored_abspath,
                               db, scratch_pool));
          SVN_ERR(file_added(b->shelf_version, relpath, s,
                             b->diff_processor,
                             right_source, right_stored_abspath,
                             db, scratch_pool));
        default:
          break;
        }
    }
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf2_test_apply_file(svn_boolean_t *conflict_p,
                                   svn_client__shelf2_version_t *shelf_version,
                                   const char *file_relpath,
                                   apr_pool_t *scratch_pool)
{
  struct apply_files_baton_t baton = {0};

  baton.shelf_version = shelf_version;
  baton.test_only = TRUE;
  baton.conflict = FALSE;
  baton.ctx = shelf_version->shelf->ctx;
  SVN_ERR(shelf_status_visit_path(shelf_version, file_relpath,
                           apply_file_visitor, &baton,
                           scratch_pool));
  *conflict_p = baton.conflict;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf2_apply(svn_client__shelf2_version_t *shelf_version,
                         svn_boolean_t dry_run,
                         apr_pool_t *scratch_pool)
{
  struct apply_files_baton_t baton = {0};

  baton.shelf_version = shelf_version;
  baton.ctx = shelf_version->shelf->ctx;
  SVN_ERR(shelf_status_walk(shelf_version, "",
                            apply_file_visitor, &baton,
                            scratch_pool));

  svn_io_sleep_for_timestamps(shelf_version->shelf->wc_root_abspath,
                              scratch_pool);
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf2_unapply(svn_client__shelf2_version_t *shelf_version,
                           svn_boolean_t dry_run,
                           apr_pool_t *scratch_pool)
{
  apr_array_header_t *targets;

  SVN_ERR(shelf_paths_changed(NULL, &targets, shelf_version,
                              TRUE /*as_abspath*/,
                              scratch_pool, scratch_pool));
  if (!dry_run)
    {
      SVN_ERR(svn_client_revert4(targets, svn_depth_empty,
                                 NULL /*changelists*/,
                                 FALSE /*clear_changelists*/,
                                 FALSE /*metadata_only*/,
                                 FALSE /*added_keep_local*/,
                                 shelf_version->shelf->ctx, scratch_pool));
    }
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf2_delete_newer_versions(svn_client__shelf2_t *shelf,
                                         svn_client__shelf2_version_t *shelf_version,
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
svn_client__shelf2_diff(svn_client__shelf2_version_t *shelf_version,
                        const char *shelf_relpath,
                        svn_depth_t depth,
                        svn_boolean_t ignore_ancestry,
                        const svn_diff_tree_processor_t *diff_processor,
                        apr_pool_t *scratch_pool)
{
  struct diff_baton_t baton;

  if (shelf_version->version_number == 0)
    return SVN_NO_ERROR;

  baton.shelf_version = shelf_version;
  baton.top_relpath = shelf_relpath;
  baton.walk_root_abspath = shelf_version->files_dir_abspath;
  baton.diff_processor = diff_processor;
  SVN_ERR(svn_io_dir_walk2(baton.walk_root_abspath, 0 /*wanted*/,
                           diff_visitor, &baton,
                           scratch_pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf2_save_new_version3(svn_client__shelf2_version_t **new_version_p,
                                     svn_client__shelf2_t *shelf,
                                     const apr_array_header_t *paths,
                                     svn_depth_t depth,
                                     const apr_array_header_t *changelists,
                                     svn_client_status_func_t shelved_func,
                                     void *shelved_baton,
                                     svn_client_status_func_t not_shelved_func,
                                     void *not_shelved_baton,
                                     apr_pool_t *scratch_pool)
{
  int next_version = shelf->max_version + 1;
  svn_client__shelf2_version_t *new_shelf_version;
  svn_boolean_t any_shelved;

  SVN_ERR(shelf_version_create(&new_shelf_version,
                               shelf, next_version, scratch_pool));
  SVN_ERR(shelf_write_changes(&any_shelved,
                              new_shelf_version,
                              paths, depth, changelists,
                              shelved_func, shelved_baton,
                              not_shelved_func, not_shelved_baton,
                              shelf->wc_root_abspath,
                              shelf->ctx, scratch_pool, scratch_pool));

  if (any_shelved)
    {
      shelf->max_version = next_version;
      SVN_ERR(shelf_write_current(shelf, scratch_pool));

      if (new_version_p)
        SVN_ERR(svn_client__shelf2_version_open(new_version_p, shelf, next_version,
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
svn_client__shelf2_get_log_message(char **log_message,
                                   svn_client__shelf2_t *shelf,
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
svn_client__shelf2_set_log_message(svn_client__shelf2_t *shelf,
                                   const char *message,
                                   apr_pool_t *scratch_pool)
{
  svn_string_t *propval
    = message ? svn_string_create(message, shelf->pool) : NULL;

  SVN_ERR(svn_client__shelf2_revprop_set(shelf, SVN_PROP_REVISION_LOG, propval,
                                         scratch_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf2_list(apr_hash_t **shelf_infos,
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
          svn_client__shelf2_info_t *info
            = apr_palloc(result_pool, sizeof(*info));

          info->mtime = dirent->mtime;
          svn_hash_sets(*shelf_infos, name, info);
        }
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf2_version_open(svn_client__shelf2_version_t **shelf_version_p,
                                svn_client__shelf2_t *shelf,
                                int version_number,
                                apr_pool_t *result_pool,
                                apr_pool_t *scratch_pool)
{
  svn_client__shelf2_version_t *shelf_version;
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
svn_client__shelf2_get_newest_version(svn_client__shelf2_version_t **shelf_version_p,
                                      svn_client__shelf2_t *shelf,
                                      apr_pool_t *result_pool,
                                      apr_pool_t *scratch_pool)
{
  if (shelf->max_version == 0)
    {
      *shelf_version_p = NULL;
      return SVN_NO_ERROR;
    }

  SVN_ERR(svn_client__shelf2_version_open(shelf_version_p,
                                          shelf, shelf->max_version,
                                          result_pool, scratch_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_client__shelf2_get_all_versions(apr_array_header_t **versions_p,
                                    svn_client__shelf2_t *shelf,
                                    apr_pool_t *result_pool,
                                    apr_pool_t *scratch_pool)
{
  int i;

  *versions_p = apr_array_make(result_pool, shelf->max_version - 1,
                               sizeof(svn_client__shelf2_version_t *));

  for (i = 1; i <= shelf->max_version; i++)
    {
      svn_client__shelf2_version_t *shelf_version;

      SVN_ERR(svn_client__shelf2_version_open(&shelf_version,
                                              shelf, i,
                                              result_pool, scratch_pool));
      APR_ARRAY_PUSH(*versions_p, svn_client__shelf2_version_t *) = shelf_version;
    }
  return SVN_NO_ERROR;
}
