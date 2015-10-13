/* revprops.c --- everything needed to handle revprops in FSX
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

#include <assert.h>
#include <apr_md5.h>

#include "svn_pools.h"
#include "svn_hash.h"
#include "svn_dirent_uri.h"

#include "fs_x.h"
#include "revprops.h"
#include "util.h"
#include "transaction.h"

#include "private/svn_subr_private.h"
#include "private/svn_string_private.h"
#include "../libsvn_fs/fs-loader.h"

#include "svn_private_config.h"

/* Give writing processes 10 seconds to replace an existing revprop
   file with a new one. After that time, we assume that the writing
   process got aborted and that we have re-read revprops. */
#define REVPROP_CHANGE_TIMEOUT (10 * 1000000)

/* In case of an inconsistent read, close the generation file, yield,
   re-open and re-read.  This is the number of times we try this before
   giving up. */
#define GENERATION_READ_RETRY_COUNT 100

/* Maximum size of the generation number file contents (including NUL). */
#define CHECKSUMMED_NUMBER_BUFFER_LEN \
           (SVN_INT64_BUFFER_SIZE + 3 + APR_MD5_DIGESTSIZE * 2)


svn_error_t *
svn_fs_x__upgrade_pack_revprops(svn_fs_t *fs,
                                svn_fs_upgrade_notify_t notify_func,
                                void *notify_baton,
                                svn_cancel_func_t cancel_func,
                                void *cancel_baton,
                                apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
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
      svn_pool_clear(iterpool);

      revprops_pack_file_dir = svn_dirent_join(revsprops_dir,
                   apr_psprintf(iterpool,
                                "%" APR_INT64_T_FMT PATH_EXT_PACKED_SHARD,
                                shard),
                   iterpool);
      revprops_shard_path = svn_dirent_join(revsprops_dir,
                       apr_psprintf(iterpool, "%" APR_INT64_T_FMT, shard),
                       iterpool);

      SVN_ERR(svn_fs_x__pack_revprops_shard(revprops_pack_file_dir,
                                      revprops_shard_path,
                                      shard, ffd->max_files_per_dir,
                                      (int)(0.9 * ffd->revprop_pack_size),
                                      compression_level,
                                      cancel_func, cancel_baton, iterpool));
      if (notify_func)
        SVN_ERR(notify_func(notify_baton, shard,
                            svn_fs_upgrade_pack_revprops, iterpool));
    }

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_x__upgrade_cleanup_pack_revprops(svn_fs_t *fs,
                                        svn_fs_upgrade_notify_t notify_func,
                                        void *notify_baton,
                                        svn_cancel_func_t cancel_func,
                                        void *cancel_baton,
                                        apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
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
      svn_pool_clear(iterpool);

      revprops_shard_path = svn_dirent_join(revsprops_dir,
                       apr_psprintf(iterpool, "%" APR_INT64_T_FMT, shard),
                       iterpool);
      SVN_ERR(svn_fs_x__delete_revprops_shard(revprops_shard_path,
                                              shard, ffd->max_files_per_dir,
                                              cancel_func, cancel_baton,
                                              iterpool));
      if (notify_func)
        SVN_ERR(notify_func(notify_baton, shard,
                            svn_fs_upgrade_cleanup_revprops, iterpool));
    }

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

/* Revprop caching management.
 *
 * Mechanism:
 * ----------
 *
 * Revprop caching needs to be activated and will be deactivated for the
 * respective FS instance if the necessary infrastructure could not be
 * initialized.  As long as no revprops are being read or changed, revprop
 * caching imposes no overhead.
 *
 * When activated, we cache revprops using (revision, generation) pairs
 * as keys with the generation being incremented upon every revprop change.
 * Since the cache is process-local, the generation needs to be tracked
 * for at least as long as the process lives but may be reset afterwards.
 *
 * We track the revprop generation in a persistent, unbuffered file that
 * we may keep open for the lifetime of the svn_fs_t.  It is the OS'
 * responsibility to provide us with the latest contents upon read.  To
 * detect incomplete updates due to non-atomic reads, we put a MD5 checksum
 * next to the actual generation number and verify that it matches.
 *
 * Since we cannot guarantee that the OS will provide us with up-to-date
 * data buffers for open files, we re-open and re-read the file before
 * modifying it.  This will prevent lost updates.
 *
 * A race condition exists between switching to the modified revprop data
 * and bumping the generation number.  In particular, the process may crash
 * just after switching to the new revprop data and before bumping the
 * generation.  To be able to detect this scenario, we bump the generation
 * twice per revprop change: once immediately before (creating an odd number)
 * and once after the atomic switch (even generation).
 *
 * A writer holding the write lock can immediately assume a crashed writer
 * in case of an odd generation or they would not have been able to acquire
 * the lock.  A reader detecting an odd generation will use that number and
 * be forced to re-read any revprop data - usually getting the new revprops
 * already.  If the generation file modification timestamp is too old, the
 * reader will assume a crashed writer, acquire the write lock and bump
 * the generation if it is still odd.  So, for about REVPROP_CHANGE_TIMEOUT
 * after the crash, reader caches may be stale.
 */

/* If the revprop generation file in FS is open, close it.  This is a no-op
 * if the file is not open.
 */
static svn_error_t *
close_revprop_generation_file(svn_fs_t *fs,
                              apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
  if (ffd->revprop_generation_file)
    {
      SVN_ERR(svn_io_file_close(ffd->revprop_generation_file, scratch_pool));
      ffd->revprop_generation_file = NULL;
    }

  return SVN_NO_ERROR;
}

/* Make sure the revprop_generation member in FS is set.  If READ_ONLY is
 * set, open the file w/o write permission if the file is not open yet.
 * The file is kept open if it has sufficient rights (or more) but will be
 * closed and re-opened if it provided insufficient access rights.
 *
 * Call only for repos that support revprop caching.
 */
static svn_error_t *
open_revprop_generation_file(svn_fs_t *fs,
                             svn_boolean_t read_only,
                             apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
  apr_int32_t flags = read_only ? APR_READ : (APR_READ | APR_WRITE);

  /* Close the current file handle if it has insufficient rights. */
  if (   ffd->revprop_generation_file
      && (apr_file_flags_get(ffd->revprop_generation_file) & flags) != flags)
    SVN_ERR(close_revprop_generation_file(fs, scratch_pool));

  /* If not open already, open with sufficient rights. */
  if (ffd->revprop_generation_file == NULL)
    {
      const char *path = svn_fs_x__path_revprop_generation(fs, scratch_pool);
      SVN_ERR(svn_io_file_open(&ffd->revprop_generation_file, path,
                               flags, APR_OS_DEFAULT, fs->pool));
    }

  return SVN_NO_ERROR;
}

/* Return the textual representation of NUMBER and its checksum in *BUFFER.
 */
static svn_error_t *
checkedsummed_number(svn_stringbuf_t **buffer,
                     apr_int64_t number,
                     apr_pool_t *result_pool,
                     apr_pool_t *scratch_pool)
{
  svn_checksum_t *checksum;
  const char *digest;

  char str[SVN_INT64_BUFFER_SIZE];
  apr_size_t len = svn__i64toa(str, number);
  str[len] = 0;

  SVN_ERR(svn_checksum(&checksum, svn_checksum_md5, str, len, scratch_pool));
  digest = svn_checksum_to_cstring_display(checksum, scratch_pool);

  *buffer = svn_stringbuf_createf(result_pool, "%s %s\n", digest, str);

  return SVN_NO_ERROR;
}

/* Extract the generation number from the text BUFFER of LEN bytes and
 * verify it against the checksum in the same BUFFER.  If they match, return
 * the generation in *NUMBER.  Otherwise, return an error.
 * BUFFER does not need to be NUL-terminated.
 */
static svn_error_t *
verify_extract_number(apr_int64_t *number,
                      const char *buffer,
                      apr_size_t len,
                      apr_pool_t *scratch_pool)
{
  const char *digest_end = strchr(buffer, ' ');

  /* Does the buffer even contain checksum _and_ number? */
  if (digest_end != NULL)
    {
      svn_checksum_t *expected;
      svn_checksum_t *actual;

      SVN_ERR(svn_checksum_parse_hex(&expected, svn_checksum_md5, buffer,
                                     scratch_pool));
      SVN_ERR(svn_checksum(&actual, svn_checksum_md5, digest_end + 1,
                           (buffer + len) - (digest_end + 1), scratch_pool));

      if (svn_checksum_match(expected, actual))
        return svn_error_trace(svn_cstring_atoi64(number, digest_end + 1));
    }

  /* Incomplete buffer or not a match. */
  return svn_error_create(SVN_ERR_FS_INVALID_GENERATION, NULL,
                          _("Invalid generation number data."));
}

/* Read revprop generation as stored on disk for repository FS. The result is
 * returned in *CURRENT.  Call only for repos that support revprop caching.
 */
static svn_error_t *
read_revprop_generation_file(apr_int64_t *current,
                             svn_fs_t *fs,
                             apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);
  char buf[CHECKSUMMED_NUMBER_BUFFER_LEN];
  apr_size_t len;
  apr_off_t offset = 0;
  int i;
  svn_error_t *err = SVN_NO_ERROR;

  /* Retry in case of incomplete file buffer updates. */
  for (i = 0; i < GENERATION_READ_RETRY_COUNT; ++i)
    {
      svn_error_clear(err);
      svn_pool_clear(iterpool);

      /* If we can't even access the data, things are very wrong.
       * Don't retry in that case.
       */
      SVN_ERR(open_revprop_generation_file(fs, TRUE, iterpool));
      SVN_ERR(svn_io_file_seek(ffd->revprop_generation_file, APR_SET, &offset,
                              iterpool));

      len = sizeof(buf);
      SVN_ERR(svn_io_read_length_line(ffd->revprop_generation_file, buf, &len,
                                      iterpool));

      /* Some data has been read.  It will most likely be complete and
       * consistent.  Extract and verify anyway. */
      err = verify_extract_number(current, buf, len, iterpool);
      if (!err)
        break;

      /* Got unlucky and data was invalid.  Retry. */
      SVN_ERR(close_revprop_generation_file(fs, iterpool));

#if APR_HAS_THREADS
      apr_thread_yield();
#else
      apr_sleep(0);
#endif
    }

  svn_pool_destroy(iterpool);

  /* If we had to give up, propagate the error. */
  return svn_error_trace(err);
}

/* Write the CURRENT revprop generation to disk for repository FS.
 * Call only for repos that support revprop caching.
 */
static svn_error_t *
write_revprop_generation_file(svn_fs_t *fs,
                              apr_int64_t current,
                              apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
  svn_stringbuf_t *buffer;
  apr_off_t offset = 0;

  SVN_ERR(checkedsummed_number(&buffer, current, scratch_pool, scratch_pool));

  SVN_ERR(open_revprop_generation_file(fs, FALSE, scratch_pool));
  SVN_ERR(svn_io_file_seek(ffd->revprop_generation_file, APR_SET, &offset,
                           scratch_pool));
  SVN_ERR(svn_io_file_write_full(ffd->revprop_generation_file, buffer->data,
                                 buffer->len, NULL, scratch_pool));
  SVN_ERR(svn_io_file_flush_to_disk(ffd->revprop_generation_file,
                                    scratch_pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_x__reset_revprop_generation_file(svn_fs_t *fs,
                                        apr_pool_t *scratch_pool)
{
  const char *path = svn_fs_x__path_revprop_generation(fs, scratch_pool);
  svn_stringbuf_t *buffer;

  /* Unconditionally close the revprop generation file.
   * Don't care about FS formats. This ensures consistent internal state. */
  SVN_ERR(close_revprop_generation_file(fs, scratch_pool));

  /* Unconditionally remove any old revprop generation file.
   * Don't care about FS formats.  This ensures consistent on-disk state
   * for old format repositories. */
  SVN_ERR(svn_io_remove_file2(path, TRUE, scratch_pool));

  /* Write the initial revprop generation file contents, if supported by
   * the current format.  This ensures consistent on-disk state for new
   * format repositories. */
  SVN_ERR(checkedsummed_number(&buffer, 0, scratch_pool, scratch_pool));
  SVN_ERR(svn_io_write_atomic(path, buffer->data, buffer->len, NULL,
                              scratch_pool));

  /* ffd->revprop_generation_file will be re-opened on demand. */

  return SVN_NO_ERROR;
}

/* Create an error object with the given MESSAGE and pass it to the
   WARNING member of FS. Clears UNDERLYING_ERR. */
static void
log_revprop_cache_init_warning(svn_fs_t *fs,
                               svn_error_t *underlying_err,
                               const char *message,
                               apr_pool_t *scratch_pool)
{
  svn_error_t *err = svn_error_createf(
                       SVN_ERR_FS_REVPROP_CACHE_INIT_FAILURE,
                       underlying_err, message,
                       svn_dirent_local_style(fs->path, scratch_pool));

  if (fs->warning)
    (fs->warning)(fs->warning_baton, err);

  svn_error_clear(err);
}

/* Test whether revprop cache and necessary infrastructure are
   available in FS. */
static svn_boolean_t
has_revprop_cache(svn_fs_t *fs,
                  apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
  svn_error_t *error;

  /* is the cache (still) enabled? */
  if (ffd->revprop_cache == NULL)
    return FALSE;

  /* try initialize our file-backed infrastructure */
  error = open_revprop_generation_file(fs, TRUE, scratch_pool);
  if (error)
    {
      /* failure -> disable revprop cache for good */

      ffd->revprop_cache = NULL;
      log_revprop_cache_init_warning(fs, error,
                                     "Revprop caching for '%s' disabled "
                                     "because infrastructure for revprop "
                                     "caching failed to initialize.",
                                     scratch_pool);

      return FALSE;
    }

  return TRUE;
}

/* Baton structure for revprop_generation_fixup. */
typedef struct revprop_generation_fixup_t
{
  /* revprop generation to read */
  apr_int64_t *generation;

  /* file system context */
  svn_fs_t *fs;
} revprop_generation_upgrade_t;

/* If the revprop generation has an odd value, it means the original writer
   of the revprop got killed. We don't know whether that process as able
   to change the revprop data but we assume that it was. Therefore, we
   increase the generation in that case to basically invalidate everyone's
   cache content.
   Execute this only while holding the write lock to the repo in baton->FFD.
 */
static svn_error_t *
revprop_generation_fixup(void *void_baton,
                         apr_pool_t *scratch_pool)
{
  revprop_generation_upgrade_t *baton = void_baton;
  svn_fs_x__data_t *ffd = baton->fs->fsap_data;
  assert(ffd->has_write_lock);

  /* Make sure we don't operate on stale OS buffers. */
  SVN_ERR(close_revprop_generation_file(baton->fs, scratch_pool));

  /* Maybe, either the original revprop writer or some other reader has
     already corrected / bumped the revprop generation.  Thus, we need
     to read it again.  However, we will now be the only ones changing
     the file contents due to us holding the write lock. */
  SVN_ERR(read_revprop_generation_file(baton->generation, baton->fs,
                                       scratch_pool));

  /* Cause everyone to re-read revprops upon their next access, if the
     last revprop write did not complete properly. */
  if (*baton->generation % 2)
    {
      ++*baton->generation;
      SVN_ERR(write_revprop_generation_file(baton->fs,
                                            *baton->generation,
                                            scratch_pool));
    }

  return SVN_NO_ERROR;
}

/* Read the current revprop generation and return it in *GENERATION.
   Also, detect aborted / crashed writers and recover from that.
   Use the access object in FS to set the shared mem values. */
static svn_error_t *
read_revprop_generation(apr_int64_t *generation,
                        svn_fs_t *fs,
                        apr_pool_t *scratch_pool)
{
  apr_int64_t current = 0;
  svn_fs_x__data_t *ffd = fs->fsap_data;

  /* read the current revprop generation number */
  SVN_ERR(read_revprop_generation_file(&current, fs, scratch_pool));

  /* is an unfinished revprop write under the way? */
  if (current % 2)
    {
      svn_boolean_t timeout = FALSE;

      /* Has the writer process been aborted?
       * Either by timeout or by us being the writer now.
       */
      if (!ffd->has_write_lock)
        {
          apr_time_t mtime;
          SVN_ERR(svn_io_file_affected_time(&mtime,
                        svn_fs_x__path_revprop_generation(fs, scratch_pool),
                        scratch_pool));
          timeout = apr_time_now() > mtime + REVPROP_CHANGE_TIMEOUT;
        }

      if (ffd->has_write_lock || timeout)
        {
          revprop_generation_upgrade_t baton;
          baton.generation = &current;
          baton.fs = fs;

          /* Ensure that the original writer process no longer exists by
           * acquiring the write lock to this repository.  Then, fix up
           * the revprop generation.
           */
          if (ffd->has_write_lock)
            SVN_ERR(revprop_generation_fixup(&baton, scratch_pool));
          else
            SVN_ERR(svn_fs_x__with_write_lock(fs, revprop_generation_fixup,
                                              &baton, scratch_pool));
        }
    }

  /* return the value we just got */
  *generation = current;
  return SVN_NO_ERROR;
}

/* Set the revprop generation in FS to the next odd number to indicate
   that there is a revprop write process under way.  Return that value
   in *GENERATION.  If the change times out, readers shall recover from
   that state & re-read revprops.
   This is a no-op for repo formats that don't support revprop caching. */
static svn_error_t *
begin_revprop_change(apr_int64_t *generation,
                     svn_fs_t *fs,
                     apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
  SVN_ERR_ASSERT(ffd->has_write_lock);

  /* Close and re-open to make sure we read the latest data. */
  SVN_ERR(close_revprop_generation_file(fs, scratch_pool));
  SVN_ERR(open_revprop_generation_file(fs, FALSE, scratch_pool));

  /* Set the revprop generation to an odd value to indicate
   * that a write is in progress.
   */
  SVN_ERR(read_revprop_generation(generation, fs, scratch_pool));
  ++*generation;
  SVN_ERR(write_revprop_generation_file(fs, *generation, scratch_pool));

  return SVN_NO_ERROR;
}

/* Set the revprop generation in FS to the next even generation after
   the odd value in GENERATION to indicate that
   a) readers shall re-read revprops, and
   b) the write process has been completed (no recovery required).
   This is a no-op for repo formats that don't support revprop caching. */
static svn_error_t *
end_revprop_change(svn_fs_t *fs,
                   apr_int64_t generation,
                   apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
  SVN_ERR_ASSERT(ffd->has_write_lock);
  SVN_ERR_ASSERT(generation % 2);

  /* Set the revprop generation to an even value to indicate
   * that a write has been completed.  Since we held the write
   * lock, nobody else could have updated the file contents.
   */
  SVN_ERR(write_revprop_generation_file(fs, generation + 1, scratch_pool));

  return SVN_NO_ERROR;
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
 * The returned hash will be allocated in RESULT_POOL, SCRATCH_POOL is
 * being used for temporary allocations.
 */
static svn_error_t *
parse_revprop(apr_hash_t **properties,
              svn_fs_t *fs,
              svn_revnum_t revision,
              apr_int64_t generation,
              svn_string_t *content,
              apr_pool_t *result_pool,
              apr_pool_t *scratch_pool)
{
  svn_stream_t *stream = svn_stream_from_string(content, scratch_pool);
  *properties = apr_hash_make(result_pool);

  SVN_ERR(svn_hash_read2(*properties, stream, SVN_HASH_TERMINATOR,
                         result_pool));
  if (has_revprop_cache(fs, scratch_pool))
    {
      svn_fs_x__data_t *ffd = fs->fsap_data;
      svn_fs_x__pair_cache_key_t key = { 0 };

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
 * Allocate *PROPERTIES in RESULT_POOL and temporaries in SCRATCH_POOL.
 */
static svn_error_t *
read_non_packed_revprop(apr_hash_t **properties,
                        svn_fs_t *fs,
                        svn_revnum_t rev,
                        apr_int64_t generation,
                        apr_pool_t *result_pool,
                        apr_pool_t *scratch_pool)
{
  svn_stringbuf_t *content = NULL;
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);
  svn_boolean_t missing = FALSE;
  int i;

  for (i = 0;
       i < SVN_FS_X__RECOVERABLE_RETRY_COUNT && !missing && !content;
       ++i)
    {
      svn_pool_clear(iterpool);
      SVN_ERR(svn_fs_x__try_stringbuf_from_file(&content,
                                  &missing,
                                  svn_fs_x__path_revprops(fs, rev, iterpool),
                                  i + 1 < SVN_FS_X__RECOVERABLE_RETRY_COUNT,
                                  iterpool));
    }

  if (content)
    SVN_ERR(parse_revprop(properties, fs, rev, generation,
                          svn_stringbuf__morph_into_string(content),
                          result_pool, iterpool));

  svn_pool_clear(iterpool);

  return SVN_NO_ERROR;
}

/* Return the minimum length of any packed revprop file name in REVPROPS. */
static apr_size_t
get_min_filename_len(packed_revprops_t *revprops)
{
  char number_buffer[SVN_INT64_BUFFER_SIZE];

  /* The revprop filenames have the format <REV>.<COUNT> - with <REV> being
   * at least the first rev in the shard and <COUNT> having at least one
   * digit.  Thus, the minimum is 2 + #decimal places in the start rev.
   */
  return svn__i64toa(number_buffer, revprops->manifest_start) + 2;
}

/* Given FS and REVPROPS->REVISION, fill the FILENAME, FOLDER and MANIFEST
 * members. Use RESULT_POOL for allocating results and SCRATCH_POOL for
 * temporaries.
 */
static svn_error_t *
get_revprop_packname(svn_fs_t *fs,
                     packed_revprops_t *revprops,
                     apr_pool_t *result_pool,
                     apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
  svn_stringbuf_t *content = NULL;
  const char *manifest_file_path;
  int idx, rev_count;
  char *buffer, *buffer_end;
  const char **filenames, **filenames_end;
  apr_size_t min_filename_len;

  /* Determine the dimensions. Rev 0 is excluded from the first shard. */
  rev_count = ffd->max_files_per_dir;
  revprops->manifest_start
    = revprops->revision - (revprops->revision % rev_count);
  if (revprops->manifest_start == 0)
    {
      ++revprops->manifest_start;
      --rev_count;
    }

  revprops->manifest = apr_array_make(result_pool, rev_count,
                                      sizeof(const char*));

  /* No line in the file can be less than this number of chars long. */
  min_filename_len = get_min_filename_len(revprops);

  /* Read the content of the manifest file */
  revprops->folder
    = svn_fs_x__path_revprops_pack_shard(fs, revprops->revision, result_pool);
  manifest_file_path = svn_dirent_join(revprops->folder, PATH_MANIFEST,
                                       result_pool);

  SVN_ERR(svn_fs_x__read_content(&content, manifest_file_path, result_pool));

  /* There CONTENT must have a certain minimal size and there no
   * unterminated lines at the end of the file.  Both guarantees also
   * simplify the parser loop below.
   */
  if (   content->len < rev_count * (min_filename_len + 1)
      || content->data[content->len - 1] != '\n')
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Packed revprop manifest for r%ld not "
                               "properly terminated"), revprops->revision);

  /* Chop (parse) the manifest CONTENT into filenames, one per line.
   * We only have to replace all newlines with NUL and add all line
   * starts to REVPROPS->MANIFEST.
   *
   * There must be exactly REV_COUNT lines and that is the number of
   * lines we parse from BUFFER to FILENAMES.  Set the end pointer for
   * the source BUFFER such that BUFFER+MIN_FILENAME_LEN is still valid
   * BUFFER_END is always valid due to CONTENT->LEN > MIN_FILENAME_LEN.
   *
   * Please note that this loop is performance critical for e.g. 'svn log'.
   * It is run 1000x per revprop access, i.e. per revision and about
   * 50 million times per sec (and CPU core).
   */
  for (filenames = (const char **)revprops->manifest->elts,
       filenames_end = filenames + rev_count,
       buffer = content->data,
       buffer_end = buffer + content->len - min_filename_len;
       (filenames < filenames_end) && (buffer < buffer_end);
       ++filenames)
    {
      /* BUFFER always points to the start of the next line / filename. */
      *filenames = buffer;

      /* Find the next EOL.  This is guaranteed to stay within the CONTENT
       * buffer because we left enough room after BUFFER_END and we know
       * we will always see a newline as the last non-NUL char. */
      buffer += min_filename_len;
      while (*buffer != '\n')
        ++buffer;

      /* Found EOL.  Turn it into the filename terminator and move BUFFER
       * to the start of the next line or CONTENT buffer end. */
      *buffer = '\0';
      ++buffer;
    }

  /* We must have reached the end of both buffers. */
  if (buffer < content->data + content->len)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Packed revprop manifest for r%ld "
                               "has too many entries"), revprops->revision);

  if (filenames < filenames_end)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Packed revprop manifest for r%ld "
                               "has too few entries"), revprops->revision);

  /* The target array has now exactly one entry per revision. */
  revprops->manifest->nelts = rev_count;

  /* Now get the file name */
  idx = (int)(revprops->revision - revprops->manifest_start);
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
  svn_fs_x__data_t *ffd = fs->fsap_data;
  return (r1 / ffd->max_files_per_dir) == (r2 / ffd->max_files_per_dir);
}

/* Given FS and the full packed file content in REVPROPS->PACKED_REVPROPS,
 * fill the START_REVISION member, and make PACKED_REVPROPS point to the
 * first serialized revprop.  If READ_ALL is set, initialize the SIZES
 * and OFFSETS members as well.
 *
 * Parse the revprops for REVPROPS->REVISION and set the PROPERTIES as
 * well as the SERIALIZED_SIZE member.  If revprop caching has been
 * enabled, parse all revprops in the pack and cache them.
 */
static svn_error_t *
parse_packed_revprops(svn_fs_t *fs,
                      packed_revprops_t *revprops,
                      svn_boolean_t read_all,
                      apr_pool_t *result_pool,
                      apr_pool_t *scratch_pool)
{
  svn_stream_t *stream;
  apr_int64_t first_rev, count, i;
  apr_off_t offset;
  const char *header_end;
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);
  svn_boolean_t cache_all = has_revprop_cache(fs, scratch_pool);

  /* decompress (even if the data is only "stored", there is still a
   * length header to remove) */
  svn_stringbuf_t *compressed = revprops->packed_revprops;
  svn_stringbuf_t *uncompressed = svn_stringbuf_create_empty(result_pool);
  SVN_ERR(svn__decompress(compressed, uncompressed, APR_SIZE_MAX));

  /* read first revision number and number of revisions in the pack */
  stream = svn_stream_from_stringbuf(uncompressed, scratch_pool);
  SVN_ERR(svn_fs_x__read_number_from_stream(&first_rev, NULL, stream,
                                            iterpool));
  SVN_ERR(svn_fs_x__read_number_from_stream(&count, NULL, stream, iterpool));

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
  if (!svn_fs_x__is_packed_revprop(fs, first_rev))
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

  revprops->packed_revprops = svn_stringbuf_create_empty(result_pool);
  revprops->packed_revprops->data = uncompressed->data + offset;
  revprops->packed_revprops->len = (apr_size_t)(uncompressed->len - offset);
  revprops->packed_revprops->blocksize = (apr_size_t)(uncompressed->blocksize - offset);

  /* STREAM still points to the first entry in the sizes list. */
  revprops->start_revision = (svn_revnum_t)first_rev;
  if (read_all)
    {
      /* Init / construct REVPROPS members. */
      revprops->sizes = apr_array_make(result_pool, (int)count,
                                       sizeof(offset));
      revprops->offsets = apr_array_make(result_pool, (int)count,
                                         sizeof(offset));
    }

  /* Now parse, revision by revision, the size and content of each
   * revisions' revprops. */
  for (i = 0, offset = 0, revprops->total_size = 0; i < count; ++i)
    {
      apr_int64_t size;
      svn_string_t serialized;
      svn_revnum_t revision = (svn_revnum_t)(first_rev + i);
      svn_pool_clear(iterpool);

      /* read & check the serialized size */
      SVN_ERR(svn_fs_x__read_number_from_stream(&size, NULL, stream,
                                                iterpool));
      if (size + offset > (apr_int64_t)revprops->packed_revprops->len)
        return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                        _("Packed revprop size exceeds pack file size"));

      /* Parse this revprops list, if necessary */
      serialized.data = revprops->packed_revprops->data + offset;
      serialized.len = (apr_size_t)size;

      if (revision == revprops->revision)
        {
          /* Parse (and possibly cache) the one revprop list we care about. */
          SVN_ERR(parse_revprop(&revprops->properties, fs, revision,
                                revprops->generation, &serialized,
                                result_pool, iterpool));
          revprops->serialized_size = serialized.len;

          /* If we only wanted the revprops for REVISION then we are done. */
          if (!read_all && !cache_all)
            break;
        }
      else if (cache_all)
        {
          /* Parse and cache all other revprop lists. */
          apr_hash_t *properties;
          SVN_ERR(parse_revprop(&properties, fs, revision,
                                revprops->generation, &serialized,
                                iterpool, iterpool));
        }

      if (read_all)
        {
          /* fill REVPROPS data structures */
          APR_ARRAY_PUSH(revprops->sizes, apr_off_t) = serialized.len;
          APR_ARRAY_PUSH(revprops->offsets, apr_off_t) = offset;
        }
      revprops->total_size += serialized.len;

      offset += serialized.len;
    }

  return SVN_NO_ERROR;
}

/* In filesystem FS, read the packed revprops for revision REV into
 * *REVPROPS.  Use GENERATION to populate the revprop cache, if enabled.
 * If you want to modify revprop contents / update REVPROPS, READ_ALL
 * must be set.  Otherwise, only the properties of REV are being provided.
 *
 * Allocate *PROPERTIES in RESULT_POOL and temporaries in SCRATCH_POOL.
 */
static svn_error_t *
read_pack_revprop(packed_revprops_t **revprops,
                  svn_fs_t *fs,
                  svn_revnum_t rev,
                  apr_int64_t generation,
                  svn_boolean_t read_all,
                  apr_pool_t *result_pool,
                  apr_pool_t *scratch_pool)
{
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);
  svn_boolean_t missing = FALSE;
  svn_error_t *err;
  packed_revprops_t *result;
  int i;

  /* someone insisted that REV is packed. Double-check if necessary */
  if (!svn_fs_x__is_packed_revprop(fs, rev))
     SVN_ERR(svn_fs_x__update_min_unpacked_rev(fs, iterpool));

  if (!svn_fs_x__is_packed_revprop(fs, rev))
    return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
                              _("No such packed revision %ld"), rev);

  /* initialize the result data structure */
  result = apr_pcalloc(result_pool, sizeof(*result));
  result->revision = rev;
  result->generation = generation;

  /* try to read the packed revprops. This may require retries if we have
   * concurrent writers. */
  for (i = 0;
       i < SVN_FS_X__RECOVERABLE_RETRY_COUNT && !result->packed_revprops;
       ++i)
    {
      const char *file_path;
      svn_pool_clear(iterpool);

      /* there might have been concurrent writes.
       * Re-read the manifest and the pack file.
       */
      SVN_ERR(get_revprop_packname(fs, result, result_pool, iterpool));
      file_path  = svn_dirent_join(result->folder,
                                   result->filename,
                                   iterpool);
      SVN_ERR(svn_fs_x__try_stringbuf_from_file(&result->packed_revprops,
                                &missing,
                                file_path,
                                i + 1 < SVN_FS_X__RECOVERABLE_RETRY_COUNT,
                                result_pool));

      /* If we could not find the file, there was a write.
       * So, we should refresh our revprop generation info as well such
       * that others may find data we will put into the cache.  They would
       * consider it outdated, otherwise.
       */
      if (missing && has_revprop_cache(fs, iterpool))
        SVN_ERR(read_revprop_generation(&result->generation, fs, iterpool));
    }

  /* the file content should be available now */
  if (!result->packed_revprops)
    return svn_error_createf(SVN_ERR_FS_PACKED_REVPROP_READ_FAILURE, NULL,
                  _("Failed to read revprop pack file for r%ld"), rev);

  /* parse it. RESULT will be complete afterwards. */
  err = parse_packed_revprops(fs, result, read_all, result_pool, iterpool);
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
svn_error_t *
svn_fs_x__get_revision_proplist(apr_hash_t **proplist_p,
                                svn_fs_t *fs,
                                svn_revnum_t rev,
                                svn_boolean_t bypass_cache,
                                apr_pool_t *result_pool,
                                apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
  apr_int64_t generation = 0;

  /* not found, yet */
  *proplist_p = NULL;

  /* should they be available at all? */
  SVN_ERR(svn_fs_x__ensure_revision_exists(rev, fs, scratch_pool));

  /* Try cache lookup first. */
  if (!bypass_cache && has_revprop_cache(fs, scratch_pool))
    {
      svn_boolean_t is_cached;
      svn_fs_x__pair_cache_key_t key = { 0 };

      SVN_ERR(read_revprop_generation(&generation, fs, scratch_pool));

      key.revision = rev;
      key.second = generation;
      SVN_ERR(svn_cache__get((void **) proplist_p, &is_cached,
                             ffd->revprop_cache, &key, result_pool));
      if (is_cached)
        return SVN_NO_ERROR;
    }

  /* if REV had not been packed when we began, try reading it from the
   * non-packed shard.  If that fails, we will fall through to packed
   * shard reads. */
  if (!svn_fs_x__is_packed_revprop(fs, rev))
    {
      svn_error_t *err = read_non_packed_revprop(proplist_p, fs, rev,
                                                 generation, result_pool,
                                                 scratch_pool);
      if (err)
        {
          if (!APR_STATUS_IS_ENOENT(err->apr_err))
            return svn_error_trace(err);

          svn_error_clear(err);
          *proplist_p = NULL; /* in case read_non_packed_revprop changed it */
        }
    }

  /* if revprop packing is available and we have not read the revprops, yet,
   * try reading them from a packed shard.  If that fails, REV is most
   * likely invalid (or its revprops highly contested). */
  if (!*proplist_p)
    {
      packed_revprops_t *revprops;
      SVN_ERR(read_pack_revprop(&revprops, fs, rev, generation, FALSE,
                                result_pool, scratch_pool));
      *proplist_p = revprops->properties;
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
 * Allocate *FINAL_PATH and *TMP_PATH in RESULT_POOL.  Use SCRATCH_POOL
 * for temporary allocations.
 */
static svn_error_t *
write_non_packed_revprop(const char **final_path,
                         const char **tmp_path,
                         svn_fs_t *fs,
                         svn_revnum_t rev,
                         apr_hash_t *proplist,
                         apr_pool_t *result_pool,
                         apr_pool_t *scratch_pool)
{
  svn_stream_t *stream;
  *final_path = svn_fs_x__path_revprops(fs, rev, result_pool);

  /* ### do we have a directory sitting around already? we really shouldn't
     ### have to get the dirname here. */
  SVN_ERR(svn_stream_open_unique(&stream, tmp_path,
                                 svn_dirent_dirname(*final_path,
                                                    scratch_pool),
                                 svn_io_file_del_none,
                                 result_pool, scratch_pool));
  SVN_ERR(svn_hash_write2(proplist, stream, SVN_HASH_TERMINATOR,
                          scratch_pool));
  SVN_ERR(svn_stream_close(stream));

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
 * Use SCRATCH_POOL for temporary allocations.
 */
static svn_error_t *
switch_to_new_revprop(svn_fs_t *fs,
                      const char *final_path,
                      const char *tmp_path,
                      const char *perms_reference,
                      apr_array_header_t *files_to_delete,
                      svn_boolean_t bump_generation,
                      apr_pool_t *scratch_pool)
{
  apr_int64_t generation;

  /* Now, we may actually be replacing revprops. Make sure that all other
     threads and processes will know about this. */
  if (bump_generation)
    SVN_ERR(begin_revprop_change(&generation, fs, scratch_pool));

  SVN_ERR(svn_fs_x__move_into_place(tmp_path, final_path, perms_reference,
                                    scratch_pool));

  /* Indicate that the update (if relevant) has been completed. */
  if (bump_generation)
    SVN_ERR(end_revprop_change(fs, generation, scratch_pool));

  /* Clean up temporary files, if necessary. */
  if (files_to_delete)
    {
      apr_pool_t *iterpool = svn_pool_create(scratch_pool);
      int i;

      for (i = 0; i < files_to_delete->nelts; ++i)
        {
          const char *path = APR_ARRAY_IDX(files_to_delete, i, const char*);

          svn_pool_clear(iterpool);
          SVN_ERR(svn_io_remove_file2(path, TRUE, iterpool));
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
                          apr_pool_t *scratch_pool)
{
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);
  int i;

  SVN_ERR_ASSERT(start < end);

  /* start revision and entry count */
  SVN_ERR(svn_stream_printf(stream, scratch_pool, "%ld\n", start_revision));
  SVN_ERR(svn_stream_printf(stream, scratch_pool, "%d\n", end - start));

  /* the sizes array */
  for (i = start; i < end; ++i)
    {
      /* Non-standard pool usage.
       *
       * We only allocate a few bytes each iteration -- even with a
       * million iterations we would still be in good shape memory-wise.
       */
      apr_off_t size = APR_ARRAY_IDX(sizes, i, apr_off_t);
      SVN_ERR(svn_stream_printf(stream, iterpool, "%" APR_OFF_T_FMT "\n",
                                size));
    }

  /* the double newline char indicates the end of the header */
  SVN_ERR(svn_stream_printf(stream, iterpool, "\n"));

  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}

/* Writes the a pack file to FILE_STREAM.  It copies the serialized data
 * from REVPROPS for the indexes [START,END) except for index CHANGED_INDEX.
 *
 * The data for the latter is taken from NEW_SERIALIZED.  Note, that
 * CHANGED_INDEX may be outside the [START,END) range, i.e. no new data is
 * taken in that case but only a subset of the old data will be copied.
 *
 * NEW_TOTAL_SIZE is a hint for pre-allocating buffers of appropriate size.
 * SCRATCH_POOL is used for temporary allocations.
 */
static svn_error_t *
repack_revprops(svn_fs_t *fs,
                packed_revprops_t *revprops,
                int start,
                int end,
                int changed_index,
                svn_stringbuf_t *new_serialized,
                apr_off_t new_total_size,
                svn_stream_t *file_stream,
                apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
  svn_stream_t *stream;
  int i;

  /* create data empty buffers and the stream object */
  svn_stringbuf_t *uncompressed
    = svn_stringbuf_create_ensure((apr_size_t)new_total_size, scratch_pool);
  svn_stringbuf_t *compressed
    = svn_stringbuf_create_empty(scratch_pool);
  stream = svn_stream_from_stringbuf(uncompressed, scratch_pool);

  /* write the header*/
  SVN_ERR(serialize_revprops_header(stream, revprops->start_revision + start,
                                    revprops->sizes, start, end,
                                    scratch_pool));

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
  SVN_ERR(svn__compress(uncompressed,
                        compressed,
                        ffd->compress_packed_revprops
                          ? SVN_DELTA_COMPRESSION_LEVEL_DEFAULT
                          : SVN_DELTA_COMPRESSION_LEVEL_NONE));

  /* finally, write the content to the target stream and close it */
  SVN_ERR(svn_stream_write(file_stream, compressed->data, &compressed->len));
  SVN_ERR(svn_stream_close(file_stream));

  return SVN_NO_ERROR;
}

/* Allocate a new pack file name for revisions
 *     [REVPROPS->START_REVISION + START, REVPROPS->START_REVISION + END - 1]
 * of REVPROPS->MANIFEST.  Add the name of old file to FILES_TO_DELETE,
 * auto-create that array if necessary.  Return an open file stream to
 * the new file in *STREAM allocated in RESULT_POOL.  Allocate the paths
 * in *FILES_TO_DELETE from the same pool that contains the array itself.
 *
 * Use SCRATCH_POOL for temporary allocations.
 */
static svn_error_t *
repack_stream_open(svn_stream_t **stream,
                   svn_fs_t *fs,
                   packed_revprops_t *revprops,
                   int start,
                   int end,
                   apr_array_header_t **files_to_delete,
                   apr_pool_t *result_pool,
                   apr_pool_t *scratch_pool)
{
  apr_int64_t tag;
  const char *tag_string;
  svn_string_t *new_filename;
  int i;
  apr_file_t *file;
  int manifest_offset
    = (int)(revprops->start_revision - revprops->manifest_start);

  /* get the old (= current) file name and enlist it for later deletion */
  const char *old_filename = APR_ARRAY_IDX(revprops->manifest,
                                           start + manifest_offset,
                                           const char*);

  if (*files_to_delete == NULL)
    *files_to_delete = apr_array_make(result_pool, 3, sizeof(const char*));

  APR_ARRAY_PUSH(*files_to_delete, const char*)
    = svn_dirent_join(revprops->folder, old_filename,
                      (*files_to_delete)->pool);

  /* increase the tag part, i.e. the counter after the dot */
  tag_string = strchr(old_filename, '.');
  if (tag_string == NULL)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Packed file '%s' misses a tag"),
                             old_filename);

  SVN_ERR(svn_cstring_atoi64(&tag, tag_string + 1));
  new_filename = svn_string_createf((*files_to_delete)->pool,
                                    "%ld.%" APR_INT64_T_FMT,
                                    revprops->start_revision + start,
                                    ++tag);

  /* update the manifest to point to the new file */
  for (i = start; i < end; ++i)
    APR_ARRAY_IDX(revprops->manifest, i + manifest_offset, const char*)
      = new_filename->data;

  /* create a file stream for the new file */
  SVN_ERR(svn_io_file_open(&file, svn_dirent_join(revprops->folder,
                                                  new_filename->data,
                                                  scratch_pool),
                           APR_WRITE | APR_CREATE, APR_OS_DEFAULT,
                           result_pool));
  *stream = svn_stream_from_aprfile2(file, FALSE, result_pool);

  return SVN_NO_ERROR;
}

/* For revision REV in filesystem FS, set the revision properties to
 * PROPLIST.  Return a new file in *TMP_PATH that the caller shall move
 * to *FINAL_PATH to make the change visible.  Files to be deleted will
 * be listed in *FILES_TO_DELETE which may remain unchanged / unallocated.
 *
 * Allocate output values in RESULT_POOL and temporaries from SCRATCH_POOL.
 */
static svn_error_t *
write_packed_revprop(const char **final_path,
                     const char **tmp_path,
                     apr_array_header_t **files_to_delete,
                     svn_fs_t *fs,
                     svn_revnum_t rev,
                     apr_hash_t *proplist,
                     apr_pool_t *result_pool,
                     apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
  packed_revprops_t *revprops;
  apr_int64_t generation = 0;
  svn_stream_t *stream;
  svn_stringbuf_t *serialized;
  apr_off_t new_total_size;
  int changed_index;

  /* read the current revprop generation. This value will not change
   * while we hold the global write lock to this FS. */
  if (has_revprop_cache(fs, scratch_pool))
    SVN_ERR(read_revprop_generation(&generation, fs, scratch_pool));

  /* read contents of the current pack file */
  SVN_ERR(read_pack_revprop(&revprops, fs, rev, generation, TRUE,
                            scratch_pool, scratch_pool));

  /* serialize the new revprops */
  serialized = svn_stringbuf_create_empty(scratch_pool);
  stream = svn_stream_from_stringbuf(serialized, scratch_pool);
  SVN_ERR(svn_hash_write2(proplist, stream, SVN_HASH_TERMINATOR,
                          scratch_pool));
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
                                    result_pool);
      SVN_ERR(svn_stream_open_unique(&stream, tmp_path, revprops->folder,
                                     svn_io_file_del_none, result_pool,
                                     scratch_pool));
      SVN_ERR(repack_revprops(fs, revprops, 0, revprops->sizes->nelts,
                              changed_index, serialized, new_total_size,
                              stream, scratch_pool));
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

      /* Allocate this here such that we can call the repack functions with
       * the scratch pool alone. */
      if (*files_to_delete == NULL)
        *files_to_delete = apr_array_make(result_pool, 3,
                                          sizeof(const char*));

      /* write the new, split files */
      if (left_count)
        {
          SVN_ERR(repack_stream_open(&stream, fs, revprops, 0,
                                     left_count, files_to_delete,
                                     scratch_pool, scratch_pool));
          SVN_ERR(repack_revprops(fs, revprops, 0, left_count,
                                  changed_index, serialized, new_total_size,
                                  stream, scratch_pool));
        }

      if (left_count + right_count < revprops->sizes->nelts)
        {
          SVN_ERR(repack_stream_open(&stream, fs, revprops, changed_index,
                                     changed_index + 1, files_to_delete,
                                     scratch_pool, scratch_pool));
          SVN_ERR(repack_revprops(fs, revprops, changed_index,
                                  changed_index + 1,
                                  changed_index, serialized, new_total_size,
                                  stream, scratch_pool));
        }

      if (right_count)
        {
          SVN_ERR(repack_stream_open(&stream, fs, revprops,
                                     revprops->sizes->nelts - right_count,
                                     revprops->sizes->nelts,
                                     files_to_delete, scratch_pool,
                                     scratch_pool));
          SVN_ERR(repack_revprops(fs, revprops,
                                  revprops->sizes->nelts - right_count,
                                  revprops->sizes->nelts, changed_index,
                                  serialized, new_total_size, stream,
                                  scratch_pool));
        }

      /* write the new manifest */
      *final_path = svn_dirent_join(revprops->folder, PATH_MANIFEST,
                                    result_pool);
      SVN_ERR(svn_stream_open_unique(&stream, tmp_path, revprops->folder,
                                     svn_io_file_del_none, result_pool,
                                     scratch_pool));

      for (i = 0; i < revprops->manifest->nelts; ++i)
        {
          const char *filename = APR_ARRAY_IDX(revprops->manifest, i,
                                               const char*);
          SVN_ERR(svn_stream_printf(stream, scratch_pool, "%s\n", filename));
        }

      SVN_ERR(svn_stream_close(stream));
    }

  return SVN_NO_ERROR;
}

/* Set the revision property list of revision REV in filesystem FS to
   PROPLIST.  Use SCRATCH_POOL for temporary allocations. */
svn_error_t *
svn_fs_x__set_revision_proplist(svn_fs_t *fs,
                                svn_revnum_t rev,
                                apr_hash_t *proplist,
                                apr_pool_t *scratch_pool)
{
  svn_boolean_t is_packed;
  svn_boolean_t bump_generation = FALSE;
  const char *final_path;
  const char *tmp_path;
  const char *perms_reference;
  apr_array_header_t *files_to_delete = NULL;

  SVN_ERR(svn_fs_x__ensure_revision_exists(rev, fs, scratch_pool));

  /* this info will not change while we hold the global FS write lock */
  is_packed = svn_fs_x__is_packed_revprop(fs, rev);

  /* Test whether revprops already exist for this revision.
   * Only then will we need to bump the revprop generation.
   * The fact that they did not yet exist is never cached. */
  if (is_packed)
    {
      bump_generation = TRUE;
    }
  else
    {
      svn_node_kind_t kind;
      SVN_ERR(svn_io_check_path(svn_fs_x__path_revprops(fs, rev,
                                                        scratch_pool),
                                &kind, scratch_pool));
      bump_generation = kind != svn_node_none;
    }

  /* Serialize the new revprop data */
  if (is_packed)
    SVN_ERR(write_packed_revprop(&final_path, &tmp_path, &files_to_delete,
                                 fs, rev, proplist, scratch_pool,
                                 scratch_pool));
  else
    SVN_ERR(write_non_packed_revprop(&final_path, &tmp_path,
                                     fs, rev, proplist, scratch_pool,
                                     scratch_pool));

  /* We use the rev file of this revision as the perms reference,
   * because when setting revprops for the first time, the revprop
   * file won't exist and therefore can't serve as its own reference.
   * (Whereas the rev file should already exist at this point.)
   */
  perms_reference = svn_fs_x__path_rev_absolute(fs, rev, scratch_pool);

  /* Now, switch to the new revprop data. */
  SVN_ERR(switch_to_new_revprop(fs, final_path, tmp_path, perms_reference,
                                files_to_delete, bump_generation,
                                scratch_pool));

  return SVN_NO_ERROR;
}

/* Return TRUE, if for REVISION in FS, we can find the revprop pack file.
 * Use SCRATCH_POOL for temporary allocations.
 * Set *MISSING, if the reason is a missing manifest or pack file.
 */
svn_boolean_t
svn_fs_x__packed_revprop_available(svn_boolean_t *missing,
                                   svn_fs_t *fs,
                                   svn_revnum_t revision,
                                   apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
  svn_stringbuf_t *content = NULL;

  /* try to read the manifest file */
  const char *folder = svn_fs_x__path_revprops_pack_shard(fs, revision,
                                                          scratch_pool);
  const char *manifest_path = svn_dirent_join(folder, PATH_MANIFEST,
                                              scratch_pool);

  svn_error_t *err = svn_fs_x__try_stringbuf_from_file(&content,
                                                       missing,
                                                       manifest_path,
                                                       FALSE,
                                                       scratch_pool);

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
                                                  scratch_pool),
                                  &kind, scratch_pool);
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


/****** Packing FSX shards *********/

svn_error_t *
svn_fs_x__copy_revprops(const char *pack_file_dir,
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
  SVN_ERR(svn__compress(uncompressed, compressed, compression_level));

  /* write the pack file content to disk */
  stream = svn_stream_from_aprfile2(pack_file, FALSE, scratch_pool);
  SVN_ERR(svn_stream_write(stream, compressed->data, &compressed->len));
  SVN_ERR(svn_stream_close(stream));

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_x__pack_revprops_shard(const char *pack_file_dir,
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
    /* Special special case: if max_files_per_dir is 1, then at this point
       start_rev == 1 and end_rev == 0 (!).  Fortunately, everything just
       works. */

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
          SVN_ERR(svn_fs_x__copy_revprops(pack_file_dir, pack_filename,
                                          shard_path, start_rev, rev-1,
                                          sizes, (apr_size_t)total_size,
                                          compression_level, cancel_func,
                                          cancel_baton, iterpool));

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
    SVN_ERR(svn_fs_x__copy_revprops(pack_file_dir, pack_filename, shard_path,
                                    start_rev, rev-1, sizes,
                                    (apr_size_t)total_size, compression_level,
                                    cancel_func, cancel_baton, iterpool));

  /* flush the manifest file and update permissions */
  SVN_ERR(svn_stream_close(manifest_stream));
  SVN_ERR(svn_io_copy_perms(shard_path, pack_file_dir, iterpool));

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_x__delete_revprops_shard(const char *shard_path,
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
          const char *path;
          svn_pool_clear(iterpool);

          path = svn_dirent_join(shard_path,
                                 apr_psprintf(iterpool, "%d", i),
                                 iterpool);
          if (cancel_func)
            SVN_ERR((*cancel_func)(cancel_baton));

          SVN_ERR(svn_io_remove_file2(path, TRUE, iterpool));
        }

      svn_pool_destroy(iterpool);
    }
  else
    SVN_ERR(svn_io_remove_dir2(shard_path, TRUE,
                               cancel_func, cancel_baton, scratch_pool));

  return SVN_NO_ERROR;
}

