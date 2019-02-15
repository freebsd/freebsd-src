/*
 * wc_db_pristine.c :  Pristine ("text base") management
 *
 * See the spec in 'notes/wc-ng/pristine-store'.
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

#define SVN_WC__I_AM_WC_DB

#include "svn_pools.h"
#include "svn_dirent_uri.h"

#include "wc.h"
#include "wc_db.h"
#include "wc-queries.h"
#include "wc_db_private.h"

#define PRISTINE_STORAGE_EXT ".svn-base"
#define PRISTINE_STORAGE_RELPATH "pristine"
#define PRISTINE_TEMPDIR_RELPATH "tmp"



/* Returns in PRISTINE_ABSPATH a new string allocated from RESULT_POOL,
   holding the local absolute path to the file location that is dedicated
   to hold CHECKSUM's pristine file, relating to the pristine store
   configured for the working copy indicated by PDH. The returned path
   does not necessarily currently exist.

   Any other allocations are made in SCRATCH_POOL. */
static svn_error_t *
get_pristine_fname(const char **pristine_abspath,
                   const char *wcroot_abspath,
                   const svn_checksum_t *sha1_checksum,
                   apr_pool_t *result_pool,
                   apr_pool_t *scratch_pool)
{
  const char *base_dir_abspath;
  const char *hexdigest = svn_checksum_to_cstring(sha1_checksum, scratch_pool);
  char subdir[3];

  /* ### code is in transition. make sure we have the proper data.  */
  SVN_ERR_ASSERT(pristine_abspath != NULL);
  SVN_ERR_ASSERT(svn_dirent_is_absolute(wcroot_abspath));
  SVN_ERR_ASSERT(sha1_checksum != NULL);
  SVN_ERR_ASSERT(sha1_checksum->kind == svn_checksum_sha1);

  base_dir_abspath = svn_dirent_join_many(scratch_pool,
                                          wcroot_abspath,
                                          svn_wc_get_adm_dir(scratch_pool),
                                          PRISTINE_STORAGE_RELPATH,
                                          NULL);

  /* We should have a valid checksum and (thus) a valid digest. */
  SVN_ERR_ASSERT(hexdigest != NULL);

  /* Get the first two characters of the digest, for the subdir. */
  subdir[0] = hexdigest[0];
  subdir[1] = hexdigest[1];
  subdir[2] = '\0';

  hexdigest = apr_pstrcat(scratch_pool, hexdigest, PRISTINE_STORAGE_EXT,
                          (char *)NULL);

  /* The file is located at DIR/.svn/pristine/XX/XXYYZZ...svn-base */
  *pristine_abspath = svn_dirent_join_many(result_pool,
                                           base_dir_abspath,
                                           subdir,
                                           hexdigest,
                                           NULL);
  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc__db_pristine_get_path(const char **pristine_abspath,
                             svn_wc__db_t *db,
                             const char *wri_abspath,
                             const svn_checksum_t *sha1_checksum,
                             apr_pool_t *result_pool,
                             apr_pool_t *scratch_pool)
{
  svn_wc__db_wcroot_t *wcroot;
  const char *local_relpath;
  svn_boolean_t present;

  SVN_ERR_ASSERT(pristine_abspath != NULL);
  SVN_ERR_ASSERT(svn_dirent_is_absolute(wri_abspath));
  SVN_ERR_ASSERT(sha1_checksum != NULL);
  /* ### Transitional: accept MD-5 and look up the SHA-1.  Return an error
   * if the pristine text is not in the store. */
  if (sha1_checksum->kind != svn_checksum_sha1)
    SVN_ERR(svn_wc__db_pristine_get_sha1(&sha1_checksum, db, wri_abspath,
                                         sha1_checksum,
                                         scratch_pool, scratch_pool));
  SVN_ERR_ASSERT(sha1_checksum->kind == svn_checksum_sha1);

  SVN_ERR(svn_wc__db_wcroot_parse_local_abspath(&wcroot, &local_relpath,
                                             db, wri_abspath,
                                             scratch_pool, scratch_pool));
  VERIFY_USABLE_WCROOT(wcroot);

  SVN_ERR(svn_wc__db_pristine_check(&present, db, wri_abspath, sha1_checksum,
                                    scratch_pool));
  if (! present)
    return svn_error_createf(SVN_ERR_WC_DB_ERROR, NULL,
                             _("The pristine text with checksum '%s' was "
                               "not found"),
                             svn_checksum_to_cstring_display(sha1_checksum,
                                                             scratch_pool));

  SVN_ERR(get_pristine_fname(pristine_abspath, wcroot->abspath,
                             sha1_checksum,
                             result_pool, scratch_pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__db_pristine_get_future_path(const char **pristine_abspath,
                                    const char *wcroot_abspath,
                                    const svn_checksum_t *sha1_checksum,
                                    apr_pool_t *result_pool,
                                    apr_pool_t *scratch_pool)
{
  SVN_ERR(get_pristine_fname(pristine_abspath, wcroot_abspath,
                             sha1_checksum,
                             result_pool, scratch_pool));
  return SVN_NO_ERROR;
}

/* Set *CONTENTS to a readable stream from which the pristine text
 * identified by SHA1_CHECKSUM and PRISTINE_ABSPATH can be read from the
 * pristine store of WCROOT.  If SIZE is not null, set *SIZE to the size
 * in bytes of that text. If that text is not in the pristine store,
 * return an error.
 *
 * Even if the pristine text is removed from the store while it is being
 * read, the stream will remain valid and readable until it is closed.
 *
 * Allocate the stream in RESULT_POOL.
 *
 * This function expects to be executed inside a SQLite txn.
 *
 * Implements 'notes/wc-ng/pristine-store' section A-3(d).
 */
static svn_error_t *
pristine_read_txn(svn_stream_t **contents,
                  svn_filesize_t *size,
                  svn_wc__db_wcroot_t *wcroot,
                  const svn_checksum_t *sha1_checksum,
                  const char *pristine_abspath,
                  apr_pool_t *result_pool,
                  apr_pool_t *scratch_pool)
{
  svn_sqlite__stmt_t *stmt;
  svn_boolean_t have_row;

  /* Check that this pristine text is present in the store.  (The presence
   * of the file is not sufficient.) */
  SVN_ERR(svn_sqlite__get_statement(&stmt, wcroot->sdb,
                                    STMT_SELECT_PRISTINE_SIZE));
  SVN_ERR(svn_sqlite__bind_checksum(stmt, 1, sha1_checksum, scratch_pool));
  SVN_ERR(svn_sqlite__step(&have_row, stmt));

  if (size)
    *size = svn_sqlite__column_int64(stmt, 0);

  SVN_ERR(svn_sqlite__reset(stmt));
  if (! have_row)
    {
      return svn_error_createf(SVN_ERR_WC_PATH_NOT_FOUND, NULL,
                               _("Pristine text '%s' not present"),
                               svn_checksum_to_cstring_display(
                                 sha1_checksum, scratch_pool));
    }

  /* Open the file as a readable stream.  It will remain readable even when
   * deleted from disk; APR guarantees that on Windows as well as Unix. */
  if (contents)
    SVN_ERR(svn_stream_open_readonly(contents, pristine_abspath,
                                     result_pool, scratch_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__db_pristine_read(svn_stream_t **contents,
                         svn_filesize_t *size,
                         svn_wc__db_t *db,
                         const char *wri_abspath,
                         const svn_checksum_t *sha1_checksum,
                         apr_pool_t *result_pool,
                         apr_pool_t *scratch_pool)
{
  svn_wc__db_wcroot_t *wcroot;
  const char *local_relpath;
  const char *pristine_abspath;

  SVN_ERR_ASSERT(contents != NULL);
  SVN_ERR_ASSERT(svn_dirent_is_absolute(wri_abspath));

  /* Some 1.6-to-1.7 wc upgrades created rows without checksums and
     updating such a row passes NULL here. */
  if (!sha1_checksum)
    return svn_error_createf(SVN_ERR_WC_CORRUPT, NULL,
                             _("Can't read '%s' from pristine store "
                               "because no checksum supplied"),
                             svn_dirent_local_style(wri_abspath, scratch_pool));

  SVN_ERR_ASSERT(sha1_checksum->kind == svn_checksum_sha1);

  SVN_ERR(svn_wc__db_wcroot_parse_local_abspath(&wcroot, &local_relpath, db,
                              wri_abspath, scratch_pool, scratch_pool));
  VERIFY_USABLE_WCROOT(wcroot);

  SVN_ERR(get_pristine_fname(&pristine_abspath, wcroot->abspath,
                             sha1_checksum,
                             scratch_pool, scratch_pool));
  SVN_WC__DB_WITH_TXN(
    pristine_read_txn(contents, size,
                      wcroot, sha1_checksum, pristine_abspath,
                      result_pool, scratch_pool),
    wcroot);

  return SVN_NO_ERROR;
}


/* Return the absolute path to the temporary directory for pristine text
   files within WCROOT. */
static char *
pristine_get_tempdir(svn_wc__db_wcroot_t *wcroot,
                     apr_pool_t *result_pool,
                     apr_pool_t *scratch_pool)
{
  return svn_dirent_join_many(result_pool, wcroot->abspath,
                              svn_wc_get_adm_dir(scratch_pool),
                              PRISTINE_TEMPDIR_RELPATH, (char *)NULL);
}

svn_error_t *
svn_wc__db_pristine_get_tempdir(const char **temp_dir_abspath,
                                svn_wc__db_t *db,
                                const char *wri_abspath,
                                apr_pool_t *result_pool,
                                apr_pool_t *scratch_pool)
{
  svn_wc__db_wcroot_t *wcroot;
  const char *local_relpath;

  SVN_ERR_ASSERT(temp_dir_abspath != NULL);
  SVN_ERR_ASSERT(svn_dirent_is_absolute(wri_abspath));

  SVN_ERR(svn_wc__db_wcroot_parse_local_abspath(&wcroot, &local_relpath, db,
                              wri_abspath, scratch_pool, scratch_pool));
  VERIFY_USABLE_WCROOT(wcroot);

  *temp_dir_abspath = pristine_get_tempdir(wcroot, result_pool, scratch_pool);
  return SVN_NO_ERROR;
}


/* Install the pristine text described by BATON into the pristine store of
 * SDB.  If it is already stored then just delete the new file
 * BATON->tempfile_abspath.
 *
 * This function expects to be executed inside a SQLite txn that has already
 * acquired a 'RESERVED' lock.
 *
 * Implements 'notes/wc-ng/pristine-store' section A-3(a).
 */
static svn_error_t *
pristine_install_txn(svn_sqlite__db_t *sdb,
                     /* The path to the source file that is to be moved into place. */
                     const char *tempfile_abspath,
                     /* The target path for the file (within the pristine store). */
                     const char *pristine_abspath,
                     /* The pristine text's SHA-1 checksum. */
                     const svn_checksum_t *sha1_checksum,
                     /* The pristine text's MD-5 checksum. */
                     const svn_checksum_t *md5_checksum,
                     apr_pool_t *scratch_pool)
{
  apr_finfo_t finfo;
  svn_sqlite__stmt_t *stmt;
  svn_boolean_t have_row;
  svn_error_t *err;

  /* If this pristine text is already present in the store, just keep it:
   * delete the new one and return. */
  SVN_ERR(svn_sqlite__get_statement(&stmt, sdb, STMT_SELECT_PRISTINE));
  SVN_ERR(svn_sqlite__bind_checksum(stmt, 1, sha1_checksum, scratch_pool));
  SVN_ERR(svn_sqlite__step(&have_row, stmt));
  SVN_ERR(svn_sqlite__reset(stmt));
  if (have_row)
    {
#ifdef SVN_DEBUG
      /* Consistency checks.  Verify both files exist and match.
       * ### We could check much more. */
      {
        apr_finfo_t finfo1, finfo2;
        SVN_ERR(svn_io_stat(&finfo1, tempfile_abspath, APR_FINFO_SIZE,
                            scratch_pool));
        SVN_ERR(svn_io_stat(&finfo2, pristine_abspath, APR_FINFO_SIZE,
                            scratch_pool));
        if (finfo1.size != finfo2.size)
          {
            return svn_error_createf(
              SVN_ERR_WC_CORRUPT_TEXT_BASE, NULL,
              _("New pristine text '%s' has different size: %ld versus %ld"),
              svn_checksum_to_cstring_display(sha1_checksum, scratch_pool),
              (long int)finfo1.size, (long int)finfo2.size);
          }
      }
#endif

      /* Remove the temp file: it's already there */
      SVN_ERR(svn_io_remove_file2(tempfile_abspath,
                                  FALSE /* ignore_enoent */, scratch_pool));
      return SVN_NO_ERROR;
    }

  /* Move the file to its target location.  (If it is already there, it is
   * an orphan file and it doesn't matter if we overwrite it.) */
  err = svn_io_file_rename(tempfile_abspath, pristine_abspath,
                           scratch_pool);

  /* Maybe the directory doesn't exist yet? */
  if (err && APR_STATUS_IS_ENOENT(err->apr_err))
    {
      svn_error_t *err2;

      err2 = svn_io_dir_make(svn_dirent_dirname(pristine_abspath,
                                                scratch_pool),
                             APR_OS_DEFAULT, scratch_pool);

      if (err2)
        /* Creating directory didn't work: Return all errors */
        return svn_error_trace(svn_error_compose_create(err, err2));
      else
        /* We could create a directory: retry install */
        svn_error_clear(err);

      SVN_ERR(svn_io_file_rename(tempfile_abspath, pristine_abspath,
                           scratch_pool));
    }
  else
    SVN_ERR(err);

  SVN_ERR(svn_io_stat(&finfo, pristine_abspath, APR_FINFO_SIZE,
                      scratch_pool));

  SVN_ERR(svn_sqlite__get_statement(&stmt, sdb,
                                    STMT_INSERT_PRISTINE));
  SVN_ERR(svn_sqlite__bind_checksum(stmt, 1, sha1_checksum, scratch_pool));
  SVN_ERR(svn_sqlite__bind_checksum(stmt, 2, md5_checksum, scratch_pool));
  SVN_ERR(svn_sqlite__bind_int64(stmt, 3, finfo.size));
  SVN_ERR(svn_sqlite__insert(NULL, stmt));

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc__db_pristine_install(svn_wc__db_t *db,
                            const char *tempfile_abspath,
                            const svn_checksum_t *sha1_checksum,
                            const svn_checksum_t *md5_checksum,
                            apr_pool_t *scratch_pool)
{
  svn_wc__db_wcroot_t *wcroot;
  const char *local_relpath;
  const char *wri_abspath;
  const char *pristine_abspath;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(tempfile_abspath));
  SVN_ERR_ASSERT(sha1_checksum != NULL);
  SVN_ERR_ASSERT(sha1_checksum->kind == svn_checksum_sha1);
  SVN_ERR_ASSERT(md5_checksum != NULL);
  SVN_ERR_ASSERT(md5_checksum->kind == svn_checksum_md5);

  /* ### this logic assumes that TEMPFILE_ABSPATH follows this pattern:
     ###   WCROOT_ABSPATH/COMPONENT/COMPONENT/TEMPFNAME
     ### if we change this (see PRISTINE_TEMPDIR_RELPATH), then this
     ### logic should change.  */
  wri_abspath = svn_dirent_dirname(
                    svn_dirent_dirname(
                        svn_dirent_dirname(tempfile_abspath, scratch_pool),
                        scratch_pool),
                    scratch_pool);

  SVN_ERR(svn_wc__db_wcroot_parse_local_abspath(&wcroot, &local_relpath, db,
                              wri_abspath, scratch_pool, scratch_pool));
  VERIFY_USABLE_WCROOT(wcroot);

  SVN_ERR(get_pristine_fname(&pristine_abspath, wcroot->abspath,
                             sha1_checksum,
                             scratch_pool, scratch_pool));

  /* Ensure the SQL txn has at least a 'RESERVED' lock before we start looking
   * at the disk, to ensure no concurrent pristine install/delete txn. */
  SVN_SQLITE__WITH_IMMEDIATE_TXN(
    pristine_install_txn(wcroot->sdb,
                         tempfile_abspath, pristine_abspath,
                         sha1_checksum, md5_checksum,
                         scratch_pool),
    wcroot->sdb);

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc__db_pristine_get_md5(const svn_checksum_t **md5_checksum,
                            svn_wc__db_t *db,
                            const char *wri_abspath,
                            const svn_checksum_t *sha1_checksum,
                            apr_pool_t *result_pool,
                            apr_pool_t *scratch_pool)
{
  svn_wc__db_wcroot_t *wcroot;
  const char *local_relpath;
  svn_sqlite__stmt_t *stmt;
  svn_boolean_t have_row;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(wri_abspath));
  SVN_ERR_ASSERT(sha1_checksum != NULL);
  SVN_ERR_ASSERT(sha1_checksum->kind == svn_checksum_sha1);

  SVN_ERR(svn_wc__db_wcroot_parse_local_abspath(&wcroot, &local_relpath, db,
                              wri_abspath, scratch_pool, scratch_pool));
  VERIFY_USABLE_WCROOT(wcroot);

  SVN_ERR(svn_sqlite__get_statement(&stmt, wcroot->sdb, STMT_SELECT_PRISTINE));
  SVN_ERR(svn_sqlite__bind_checksum(stmt, 1, sha1_checksum, scratch_pool));
  SVN_ERR(svn_sqlite__step(&have_row, stmt));
  if (!have_row)
    return svn_error_createf(SVN_ERR_WC_DB_ERROR, svn_sqlite__reset(stmt),
                             _("The pristine text with checksum '%s' was "
                               "not found"),
                             svn_checksum_to_cstring_display(sha1_checksum,
                                                             scratch_pool));

  SVN_ERR(svn_sqlite__column_checksum(md5_checksum, stmt, 0, result_pool));
  SVN_ERR_ASSERT((*md5_checksum)->kind == svn_checksum_md5);

  return svn_error_trace(svn_sqlite__reset(stmt));
}


svn_error_t *
svn_wc__db_pristine_get_sha1(const svn_checksum_t **sha1_checksum,
                             svn_wc__db_t *db,
                             const char *wri_abspath,
                             const svn_checksum_t *md5_checksum,
                             apr_pool_t *result_pool,
                             apr_pool_t *scratch_pool)
{
  svn_wc__db_wcroot_t *wcroot;
  const char *local_relpath;
  svn_sqlite__stmt_t *stmt;
  svn_boolean_t have_row;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(wri_abspath));
  SVN_ERR_ASSERT(sha1_checksum != NULL);
  SVN_ERR_ASSERT(md5_checksum->kind == svn_checksum_md5);

  SVN_ERR(svn_wc__db_wcroot_parse_local_abspath(&wcroot, &local_relpath, db,
                              wri_abspath, scratch_pool, scratch_pool));
  VERIFY_USABLE_WCROOT(wcroot);

  SVN_ERR(svn_sqlite__get_statement(&stmt, wcroot->sdb,
                                    STMT_SELECT_PRISTINE_BY_MD5));
  SVN_ERR(svn_sqlite__bind_checksum(stmt, 1, md5_checksum, scratch_pool));
  SVN_ERR(svn_sqlite__step(&have_row, stmt));
  if (!have_row)
    return svn_error_createf(SVN_ERR_WC_DB_ERROR, svn_sqlite__reset(stmt),
                             _("The pristine text with MD5 checksum '%s' was "
                               "not found"),
                             svn_checksum_to_cstring_display(md5_checksum,
                                                             scratch_pool));

  SVN_ERR(svn_sqlite__column_checksum(sha1_checksum, stmt, 0, result_pool));
  SVN_ERR_ASSERT((*sha1_checksum)->kind == svn_checksum_sha1);

  return svn_error_trace(svn_sqlite__reset(stmt));
}

/* Handle the moving of a pristine from SRC_WCROOT to DST_WCROOT. The existing
   pristine in SRC_WCROOT is described by CHECKSUM, MD5_CHECKSUM and SIZE */
static svn_error_t *
maybe_transfer_one_pristine(svn_wc__db_wcroot_t *src_wcroot,
                            svn_wc__db_wcroot_t *dst_wcroot,
                            const svn_checksum_t *checksum,
                            const svn_checksum_t *md5_checksum,
                            apr_int64_t size,
                            svn_cancel_func_t cancel_func,
                            void *cancel_baton,
                            apr_pool_t *scratch_pool)
{
  const char *pristine_abspath;
  svn_sqlite__stmt_t *stmt;
  svn_stream_t *src_stream;
  svn_stream_t *dst_stream;
  const char *tmp_abspath;
  const char *src_abspath;
  int affected_rows;
  svn_error_t *err;

  SVN_ERR(svn_sqlite__get_statement(&stmt, dst_wcroot->sdb,
                                    STMT_INSERT_OR_IGNORE_PRISTINE));
  SVN_ERR(svn_sqlite__bind_checksum(stmt, 1, checksum, scratch_pool));
  SVN_ERR(svn_sqlite__bind_checksum(stmt, 2, md5_checksum, scratch_pool));
  SVN_ERR(svn_sqlite__bind_int64(stmt, 3, size));

  SVN_ERR(svn_sqlite__update(&affected_rows, stmt));

  if (affected_rows == 0)
    return SVN_NO_ERROR;

  SVN_ERR(svn_stream_open_unique(&dst_stream, &tmp_abspath,
                                 pristine_get_tempdir(dst_wcroot,
                                                      scratch_pool,
                                                      scratch_pool),
                                 svn_io_file_del_on_pool_cleanup,
                                 scratch_pool, scratch_pool));

  SVN_ERR(get_pristine_fname(&src_abspath, src_wcroot->abspath, checksum,
                             scratch_pool, scratch_pool));

  SVN_ERR(svn_stream_open_readonly(&src_stream, src_abspath,
                                   scratch_pool, scratch_pool));

  /* ### Should we verify the SHA1 or MD5 here, or is that too expensive? */
  SVN_ERR(svn_stream_copy3(src_stream, dst_stream,
                           cancel_func, cancel_baton,
                           scratch_pool));

  SVN_ERR(get_pristine_fname(&pristine_abspath, dst_wcroot->abspath, checksum,
                             scratch_pool, scratch_pool));

  /* Move the file to its target location.  (If it is already there, it is
   * an orphan file and it doesn't matter if we overwrite it.) */
  err = svn_io_file_rename(tmp_abspath, pristine_abspath, scratch_pool);

  /* Maybe the directory doesn't exist yet? */
  if (err && APR_STATUS_IS_ENOENT(err->apr_err))
    {
      svn_error_t *err2;

      err2 = svn_io_dir_make(svn_dirent_dirname(pristine_abspath,
                                                scratch_pool),
                             APR_OS_DEFAULT, scratch_pool);

      if (err2)
        /* Creating directory didn't work: Return all errors */
        return svn_error_trace(svn_error_compose_create(err, err2));
      else
        /* We could create a directory: retry install */
        svn_error_clear(err);

      SVN_ERR(svn_io_file_rename(tmp_abspath, pristine_abspath, scratch_pool));
    }
  else
    SVN_ERR(err);

  return SVN_NO_ERROR;
}

/* Transaction implementation of svn_wc__db_pristine_transfer().
   We have a lock on DST_WCROOT.
 */
static svn_error_t *
pristine_transfer_txn(svn_wc__db_wcroot_t *src_wcroot,
                       svn_wc__db_wcroot_t *dst_wcroot,
                       const char *src_relpath,
                       svn_cancel_func_t cancel_func,
                       void *cancel_baton,
                       apr_pool_t *scratch_pool)
{
  svn_sqlite__stmt_t *stmt;
  svn_boolean_t got_row;
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);

  SVN_ERR(svn_sqlite__get_statement(&stmt, src_wcroot->sdb,
                                    STMT_SELECT_COPY_PRISTINES));
  SVN_ERR(svn_sqlite__bindf(stmt, "is", src_wcroot->wc_id, src_relpath));

  /* This obtains an sqlite read lock on src_wcroot */
  SVN_ERR(svn_sqlite__step(&got_row, stmt));

  while (got_row)
    {
      const svn_checksum_t *checksum;
      const svn_checksum_t *md5_checksum;
      apr_int64_t size;
      svn_error_t *err;

      svn_pool_clear(iterpool);

      SVN_ERR(svn_sqlite__column_checksum(&checksum, stmt, 0, iterpool));
      SVN_ERR(svn_sqlite__column_checksum(&md5_checksum, stmt, 1, iterpool));
      size = svn_sqlite__column_int64(stmt, 2);

      err = maybe_transfer_one_pristine(src_wcroot, dst_wcroot,
                                        checksum, md5_checksum, size,
                                        cancel_func, cancel_baton,
                                        iterpool);

      if (err)
        return svn_error_trace(svn_error_compose_create(
                                    err,
                                    svn_sqlite__reset(stmt)));

      SVN_ERR(svn_sqlite__step(&got_row, stmt));
    }
  SVN_ERR(svn_sqlite__reset(stmt));

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__db_pristine_transfer(svn_wc__db_t *db,
                             const char *src_local_abspath,
                             const char *dst_wri_abspath,
                             svn_cancel_func_t cancel_func,
                             void *cancel_baton,
                             apr_pool_t *scratch_pool)
{
  svn_wc__db_wcroot_t *src_wcroot, *dst_wcroot;
  const char *src_relpath, *dst_relpath;

  SVN_ERR(svn_wc__db_wcroot_parse_local_abspath(&src_wcroot, &src_relpath,
                                                db, src_local_abspath,
                                                scratch_pool, scratch_pool));
  VERIFY_USABLE_WCROOT(src_wcroot);
  SVN_ERR(svn_wc__db_wcroot_parse_local_abspath(&dst_wcroot, &dst_relpath,
                                                db, dst_wri_abspath,
                                                scratch_pool, scratch_pool));
  VERIFY_USABLE_WCROOT(dst_wcroot);

  if (src_wcroot == dst_wcroot
      || src_wcroot->sdb == dst_wcroot->sdb)
    {
      return SVN_NO_ERROR; /* Nothing to transfer */
    }

  SVN_WC__DB_WITH_TXN(
    pristine_transfer_txn(src_wcroot, dst_wcroot, src_relpath,
                          cancel_func, cancel_baton, scratch_pool),
    dst_wcroot);

  return SVN_NO_ERROR;
}




/* Remove the file at FILE_ABSPATH in such a way that we could re-create a
 * new file of the same name at any time thereafter.
 *
 * On Windows, the file will not disappear immediately from the directory if
 * it is still being read so the best thing to do is first rename it to a
 * unique name. */
static svn_error_t *
remove_file(const char *file_abspath,
            svn_wc__db_wcroot_t *wcroot,
            svn_boolean_t ignore_enoent,
            apr_pool_t *scratch_pool)
{
#ifdef WIN32
  svn_error_t *err;
  const char *temp_abspath;
  const char *temp_dir_abspath
    = pristine_get_tempdir(wcroot, scratch_pool, scratch_pool);

  /* To rename the file to a unique name in the temp dir, first create a
   * uniquely named file in the temp dir and then overwrite it. */
  SVN_ERR(svn_io_open_unique_file3(NULL, &temp_abspath, temp_dir_abspath,
                                   svn_io_file_del_none,
                                   scratch_pool, scratch_pool));
  err = svn_io_file_rename(file_abspath, temp_abspath, scratch_pool);
  if (err && ignore_enoent && APR_STATUS_IS_ENOENT(err->apr_err))
    svn_error_clear(err);
  else
    SVN_ERR(err);
  file_abspath = temp_abspath;
#endif

  SVN_ERR(svn_io_remove_file2(file_abspath, ignore_enoent, scratch_pool));

  return SVN_NO_ERROR;
}

/* If the pristine text referenced by SHA1_CHECKSUM in WCROOT/SDB, whose path
 * within the pristine store is PRISTINE_ABSPATH, has a reference count of
 * zero, delete it (both the database row and the disk file).
 *
 * This function expects to be executed inside a SQLite txn that has already
 * acquired a 'RESERVED' lock.
 */
static svn_error_t *
pristine_remove_if_unreferenced_txn(svn_sqlite__db_t *sdb,
                                    svn_wc__db_wcroot_t *wcroot,
                                    const svn_checksum_t *sha1_checksum,
                                    const char *pristine_abspath,
                                    apr_pool_t *scratch_pool)
{
  svn_sqlite__stmt_t *stmt;
  int affected_rows;

  /* Remove the DB row, if refcount is 0. */
  SVN_ERR(svn_sqlite__get_statement(&stmt, sdb,
                                    STMT_DELETE_PRISTINE_IF_UNREFERENCED));
  SVN_ERR(svn_sqlite__bind_checksum(stmt, 1, sha1_checksum, scratch_pool));
  SVN_ERR(svn_sqlite__update(&affected_rows, stmt));

  /* If we removed the DB row, then remove the file. */
  if (affected_rows > 0)
    {
      /* If the file is not present, something has gone wrong, but at this
       * point it no longer matters.  In a debug build, raise an error, but
       * in a release build, it is more helpful to ignore it and continue. */
#ifdef SVN_DEBUG
      svn_boolean_t ignore_enoent = FALSE;
#else
      svn_boolean_t ignore_enoent = TRUE;
#endif

      SVN_ERR(remove_file(pristine_abspath, wcroot, ignore_enoent,
                          scratch_pool));
    }

  return SVN_NO_ERROR;
}

/* If the pristine text referenced by SHA1_CHECKSUM in WCROOT has a
 * reference count of zero, delete it (both the database row and the disk
 * file).
 *
 * Implements 'notes/wc-ng/pristine-store' section A-3(b). */
static svn_error_t *
pristine_remove_if_unreferenced(svn_wc__db_wcroot_t *wcroot,
                                const svn_checksum_t *sha1_checksum,
                                apr_pool_t *scratch_pool)
{
  const char *pristine_abspath;

  SVN_ERR(get_pristine_fname(&pristine_abspath, wcroot->abspath,
                             sha1_checksum, scratch_pool, scratch_pool));

  /* Ensure the SQL txn has at least a 'RESERVED' lock before we start looking
   * at the disk, to ensure no concurrent pristine install/delete txn. */
  SVN_SQLITE__WITH_IMMEDIATE_TXN(
    pristine_remove_if_unreferenced_txn(
      wcroot->sdb, wcroot, sha1_checksum, pristine_abspath, scratch_pool),
    wcroot->sdb);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__db_pristine_remove(svn_wc__db_t *db,
                           const char *wri_abspath,
                           const svn_checksum_t *sha1_checksum,
                           apr_pool_t *scratch_pool)
{
  svn_wc__db_wcroot_t *wcroot;
  const char *local_relpath;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(wri_abspath));
  SVN_ERR_ASSERT(sha1_checksum != NULL);
  /* ### Transitional: accept MD-5 and look up the SHA-1.  Return an error
   * if the pristine text is not in the store. */
  if (sha1_checksum->kind != svn_checksum_sha1)
    SVN_ERR(svn_wc__db_pristine_get_sha1(&sha1_checksum, db, wri_abspath,
                                         sha1_checksum,
                                         scratch_pool, scratch_pool));
  SVN_ERR_ASSERT(sha1_checksum->kind == svn_checksum_sha1);

  SVN_ERR(svn_wc__db_wcroot_parse_local_abspath(&wcroot, &local_relpath, db,
                              wri_abspath, scratch_pool, scratch_pool));
  VERIFY_USABLE_WCROOT(wcroot);

  /* If the work queue is not empty, don't delete any pristine text because
   * the work queue may contain a reference to it. */
  {
    svn_sqlite__stmt_t *stmt;
    svn_boolean_t have_row;

    SVN_ERR(svn_sqlite__get_statement(&stmt, wcroot->sdb, STMT_LOOK_FOR_WORK));
    SVN_ERR(svn_sqlite__step(&have_row, stmt));
    SVN_ERR(svn_sqlite__reset(stmt));

    if (have_row)
      return SVN_NO_ERROR;
  }

  /* If not referenced, remove the PRISTINE table row and the file. */
  SVN_ERR(pristine_remove_if_unreferenced(wcroot, sha1_checksum, scratch_pool));

  return SVN_NO_ERROR;
}


static svn_error_t *
pristine_cleanup_wcroot(svn_wc__db_wcroot_t *wcroot,
                        apr_pool_t *scratch_pool)
{
  svn_sqlite__stmt_t *stmt;
  svn_error_t *err = NULL;

  /* Find each unreferenced pristine in the DB and remove it. */
  SVN_ERR(svn_sqlite__get_statement(&stmt, wcroot->sdb,
                                    STMT_SELECT_UNREFERENCED_PRISTINES));
  while (! err)
    {
      svn_boolean_t have_row;
      const svn_checksum_t *sha1_checksum;

      SVN_ERR(svn_sqlite__step(&have_row, stmt));
      if (! have_row)
        break;

      SVN_ERR(svn_sqlite__column_checksum(&sha1_checksum, stmt, 0,
                                          scratch_pool));
      err = pristine_remove_if_unreferenced(wcroot, sha1_checksum,
                                            scratch_pool);
    }

  return svn_error_trace(
      svn_error_compose_create(err, svn_sqlite__reset(stmt)));
}

svn_error_t *
svn_wc__db_pristine_cleanup(svn_wc__db_t *db,
                            const char *wri_abspath,
                            apr_pool_t *scratch_pool)
{
  svn_wc__db_wcroot_t *wcroot;
  const char *local_relpath;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(wri_abspath));

  SVN_ERR(svn_wc__db_wcroot_parse_local_abspath(&wcroot, &local_relpath, db,
                              wri_abspath, scratch_pool, scratch_pool));
  VERIFY_USABLE_WCROOT(wcroot);

  SVN_ERR(pristine_cleanup_wcroot(wcroot, scratch_pool));

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc__db_pristine_check(svn_boolean_t *present,
                          svn_wc__db_t *db,
                          const char *wri_abspath,
                          const svn_checksum_t *sha1_checksum,
                          apr_pool_t *scratch_pool)
{
  svn_wc__db_wcroot_t *wcroot;
  const char *local_relpath;
  svn_sqlite__stmt_t *stmt;
  svn_boolean_t have_row;

  SVN_ERR_ASSERT(svn_dirent_is_absolute(wri_abspath));
  SVN_ERR_ASSERT(sha1_checksum != NULL);

  if (sha1_checksum->kind != svn_checksum_sha1)
    {
      *present = FALSE;
      return SVN_NO_ERROR;
    }

  SVN_ERR(svn_wc__db_wcroot_parse_local_abspath(&wcroot, &local_relpath, db,
                              wri_abspath, scratch_pool, scratch_pool));
  VERIFY_USABLE_WCROOT(wcroot);

  /* A filestat is much cheaper than a sqlite transaction especially on NFS,
     so first check if there is a pristine file and then if we are allowed
     to use it. */
  {
    const char *pristine_abspath;
    svn_node_kind_t kind_on_disk;

    SVN_ERR(get_pristine_fname(&pristine_abspath, wcroot->abspath,
                               sha1_checksum, scratch_pool, scratch_pool));
    SVN_ERR(svn_io_check_path(pristine_abspath, &kind_on_disk, scratch_pool));
    if (kind_on_disk != svn_node_file)
      {
        *present = FALSE;
        return SVN_NO_ERROR;
      }
  }

  /* Check that there is an entry in the PRISTINE table. */
  SVN_ERR(svn_sqlite__get_statement(&stmt, wcroot->sdb, STMT_SELECT_PRISTINE));
  SVN_ERR(svn_sqlite__bind_checksum(stmt, 1, sha1_checksum, scratch_pool));
  SVN_ERR(svn_sqlite__step(&have_row, stmt));
  SVN_ERR(svn_sqlite__reset(stmt));

  *present = have_row;
  return SVN_NO_ERROR;
}
