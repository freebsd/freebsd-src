/* util.c --- utility functions for FSX repo access
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

#include "svn_ctype.h"
#include "svn_dirent_uri.h"
#include "private/svn_string_private.h"

#include "fs_x.h"
#include "id.h"
#include "util.h"

#include "../libsvn_fs/fs-loader.h"

#include "svn_private_config.h"

/* Following are defines that specify the textual elements of the
   native filesystem directories and revision files. */

/* Notes:

To avoid opening and closing the rev-files all the time, it would
probably be advantageous to keep each rev-file open for the
lifetime of the transaction object.  I'll leave that as a later
optimization for now.

I didn't keep track of pool lifetimes at all in this code.  There
are likely some errors because of that.

*/

/* Pathname helper functions */

/* Return TRUE is REV is packed in FS, FALSE otherwise. */
svn_boolean_t
svn_fs_x__is_packed_rev(svn_fs_t *fs, svn_revnum_t rev)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;

  return (rev < ffd->min_unpacked_rev);
}

/* Return TRUE is REV is packed in FS, FALSE otherwise. */
svn_boolean_t
svn_fs_x__is_packed_revprop(svn_fs_t *fs, svn_revnum_t rev)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;

  /* rev 0 will not be packed */
  return (rev < ffd->min_unpacked_rev) && (rev != 0);
}

svn_revnum_t
svn_fs_x__packed_base_rev(svn_fs_t *fs, svn_revnum_t rev)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;

  return rev < ffd->min_unpacked_rev
       ? rev - (rev % ffd->max_files_per_dir)
       : rev;
}

svn_revnum_t
svn_fs_x__pack_size(svn_fs_t *fs, svn_revnum_t rev)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;

  return rev < ffd->min_unpacked_rev ? ffd->max_files_per_dir : 1;
}

const char *
svn_fs_x__path_format(svn_fs_t *fs,
                      apr_pool_t *result_pool)
{
  return svn_dirent_join(fs->path, PATH_FORMAT, result_pool);
}

const char *
svn_fs_x__path_uuid(svn_fs_t *fs,
                    apr_pool_t *result_pool)
{
  return svn_dirent_join(fs->path, PATH_UUID, result_pool);
}

const char *
svn_fs_x__path_current(svn_fs_t *fs,
                       apr_pool_t *result_pool)
{
  return svn_dirent_join(fs->path, PATH_CURRENT, result_pool);
}

const char *
svn_fs_x__path_txn_current(svn_fs_t *fs,
                           apr_pool_t *result_pool)
{
  return svn_dirent_join(fs->path, PATH_TXN_CURRENT,
                         result_pool);
}

const char *
svn_fs_x__path_txn_current_lock(svn_fs_t *fs,
                                apr_pool_t *result_pool)
{
  return svn_dirent_join(fs->path, PATH_TXN_CURRENT_LOCK, result_pool);
}

const char *
svn_fs_x__path_lock(svn_fs_t *fs,
                    apr_pool_t *result_pool)
{
  return svn_dirent_join(fs->path, PATH_LOCK_FILE, result_pool);
}

const char *
svn_fs_x__path_pack_lock(svn_fs_t *fs,
                         apr_pool_t *result_pool)
{
  return svn_dirent_join(fs->path, PATH_PACK_LOCK_FILE, result_pool);
}

const char *
svn_fs_x__path_revprop_generation(svn_fs_t *fs,
                                  apr_pool_t *result_pool)
{
  return svn_dirent_join(fs->path, PATH_REVPROP_GENERATION, result_pool);
}

/* Return the full path of the file FILENAME within revision REV's shard in
 * FS.  If FILENAME is NULL, return the shard directory directory itself.
 * REVPROPS indicates the parent of the shard parent folder ("revprops" or
 * "revs").  PACKED says whether we want the packed shard's name.
 *
 * Allocate the result in RESULT_POOL.
 */static const char*
construct_shard_sub_path(svn_fs_t *fs,
                         svn_revnum_t rev,
                         svn_boolean_t revprops,
                         svn_boolean_t packed,
                         const char *filename,
                         apr_pool_t *result_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
  char buffer[SVN_INT64_BUFFER_SIZE + sizeof(PATH_EXT_PACKED_SHARD)] = { 0 };

  /* Select the appropriate parent path constant. */
  const char *parent = revprops ? PATH_REVPROPS_DIR : PATH_REVS_DIR;

  /* String containing the shard number. */
  apr_size_t len = svn__i64toa(buffer, rev / ffd->max_files_per_dir);

  /* Append the suffix.  Limit it to the buffer size (should never hit it). */
  if (packed)
    strncpy(buffer + len, PATH_EXT_PACKED_SHARD, sizeof(buffer) - len - 1);

  /* This will also work for NULL FILENAME as well. */
  return svn_dirent_join_many(result_pool, fs->path, parent, buffer,
                              filename, SVN_VA_NULL);
}

const char *
svn_fs_x__path_rev_packed(svn_fs_t *fs,
                          svn_revnum_t rev,
                          const char *kind,
                          apr_pool_t *result_pool)
{
  assert(svn_fs_x__is_packed_rev(fs, rev));
  return construct_shard_sub_path(fs, rev, FALSE, TRUE, kind, result_pool);
}

const char *
svn_fs_x__path_rev_shard(svn_fs_t *fs,
                         svn_revnum_t rev,
                         apr_pool_t *result_pool)
{
  return construct_shard_sub_path(fs, rev, FALSE, FALSE, NULL, result_pool);
}

const char *
svn_fs_x__path_rev(svn_fs_t *fs,
                   svn_revnum_t rev,
                   apr_pool_t *result_pool)
{
  char buffer[SVN_INT64_BUFFER_SIZE];
  svn__i64toa(buffer, rev);

  assert(! svn_fs_x__is_packed_rev(fs, rev));
  return construct_shard_sub_path(fs, rev, FALSE, FALSE, buffer, result_pool);
}

const char *
svn_fs_x__path_rev_absolute(svn_fs_t *fs,
                            svn_revnum_t rev,
                            apr_pool_t *result_pool)
{
  return svn_fs_x__is_packed_rev(fs, rev)
       ? svn_fs_x__path_rev_packed(fs, rev, PATH_PACKED, result_pool)
       : svn_fs_x__path_rev(fs, rev, result_pool);
}

const char *
svn_fs_x__path_revprops_shard(svn_fs_t *fs,
                              svn_revnum_t rev,
                              apr_pool_t *result_pool)
{
  return construct_shard_sub_path(fs, rev, TRUE, FALSE, NULL, result_pool);
}

const char *
svn_fs_x__path_revprops_pack_shard(svn_fs_t *fs,
                                   svn_revnum_t rev,
                                   apr_pool_t *result_pool)
{
  return construct_shard_sub_path(fs, rev, TRUE, TRUE, NULL, result_pool);
}

const char *
svn_fs_x__path_revprops(svn_fs_t *fs,
                        svn_revnum_t rev,
                        apr_pool_t *result_pool)
{
  char buffer[SVN_INT64_BUFFER_SIZE];
  svn__i64toa(buffer, rev);

  assert(! svn_fs_x__is_packed_revprop(fs, rev));
  return construct_shard_sub_path(fs, rev, TRUE, FALSE, buffer, result_pool);
}

const char *
svn_fs_x__txn_name(svn_fs_x__txn_id_t txn_id,
                   apr_pool_t *result_pool)
{
  char *p = apr_palloc(result_pool, SVN_INT64_BUFFER_SIZE);
  svn__ui64tobase36(p, txn_id);
  return p;
}

svn_error_t *
svn_fs_x__txn_by_name(svn_fs_x__txn_id_t *txn_id,
                      const char *txn_name)
{
  const char *next;
  apr_uint64_t id = svn__base36toui64(&next, txn_name);
  if (next == NULL || *next != 0 || *txn_name == 0)
    return svn_error_createf(SVN_ERR_INCORRECT_PARAMS, NULL,
                             "Malformed TXN name '%s'", txn_name);

  *txn_id = id;
  return SVN_NO_ERROR;
}

const char *
svn_fs_x__path_txns_dir(svn_fs_t *fs,
                        apr_pool_t *result_pool)
{
  return svn_dirent_join(fs->path, PATH_TXNS_DIR, result_pool);
}

/* Return the full path of the file FILENAME within transaction TXN_ID's
 * transaction directory in FS.  If FILENAME is NULL, return the transaction
 * directory itself.
 *
 * Allocate the result in RESULT_POOL.
 */
static const char *
construct_txn_path(svn_fs_t *fs,
                   svn_fs_x__txn_id_t txn_id,
                   const char *filename,
                   apr_pool_t *result_pool)
{
  /* Construct the transaction directory name without temp. allocations. */
  char buffer[SVN_INT64_BUFFER_SIZE + sizeof(PATH_EXT_TXN)] = { 0 };
  apr_size_t len = svn__ui64tobase36(buffer, txn_id);
  strncpy(buffer + len, PATH_EXT_TXN, sizeof(buffer) - len - 1);

  /* If FILENAME is NULL, it will terminate the list of segments
     to concatenate. */
  return svn_dirent_join_many(result_pool, fs->path, PATH_TXNS_DIR,
                              buffer, filename, SVN_VA_NULL);
}

const char *
svn_fs_x__path_txn_dir(svn_fs_t *fs,
                       svn_fs_x__txn_id_t txn_id,
                       apr_pool_t *result_pool)
{
  return construct_txn_path(fs, txn_id, NULL, result_pool);
}

/* Return the name of the sha1->rep mapping file in transaction TXN_ID
 * within FS for the given SHA1 checksum.  Use POOL for allocations.
 */
const char *
svn_fs_x__path_txn_sha1(svn_fs_t *fs,
                        svn_fs_x__txn_id_t txn_id,
                        const unsigned char *sha1,
                        apr_pool_t *pool)
{
  svn_checksum_t checksum;
  checksum.digest = sha1;
  checksum.kind = svn_checksum_sha1;

  return svn_dirent_join(svn_fs_x__path_txn_dir(fs, txn_id, pool),
                         svn_checksum_to_cstring(&checksum, pool),
                         pool);
}

const char *
svn_fs_x__path_txn_changes(svn_fs_t *fs,
                           svn_fs_x__txn_id_t txn_id,
                           apr_pool_t *result_pool)
{
  return construct_txn_path(fs, txn_id, PATH_CHANGES, result_pool);
}

const char *
svn_fs_x__path_txn_props(svn_fs_t *fs,
                         svn_fs_x__txn_id_t txn_id,
                         apr_pool_t *result_pool)
{
  return construct_txn_path(fs, txn_id, PATH_TXN_PROPS, result_pool);
}

const char *
svn_fs_x__path_txn_props_final(svn_fs_t *fs,
                               svn_fs_x__txn_id_t txn_id,
                               apr_pool_t *result_pool)
{
  return construct_txn_path(fs, txn_id, PATH_TXN_PROPS_FINAL, result_pool);
}

const char*
svn_fs_x__path_l2p_proto_index(svn_fs_t *fs,
                               svn_fs_x__txn_id_t txn_id,
                               apr_pool_t *result_pool)
{
  return construct_txn_path(fs, txn_id, PATH_INDEX PATH_EXT_L2P_INDEX,
                            result_pool);
}

const char*
svn_fs_x__path_p2l_proto_index(svn_fs_t *fs,
                               svn_fs_x__txn_id_t txn_id,
                               apr_pool_t *result_pool)
{
  return construct_txn_path(fs, txn_id, PATH_INDEX PATH_EXT_P2L_INDEX,
                            result_pool);
}

const char *
svn_fs_x__path_txn_next_ids(svn_fs_t *fs,
                            svn_fs_x__txn_id_t txn_id,
                            apr_pool_t *result_pool)
{
  return construct_txn_path(fs, txn_id, PATH_NEXT_IDS, result_pool);
}

const char *
svn_fs_x__path_min_unpacked_rev(svn_fs_t *fs,
                                apr_pool_t *result_pool)
{
  return svn_dirent_join(fs->path, PATH_MIN_UNPACKED_REV, result_pool);
}

const char *
svn_fs_x__path_txn_proto_revs(svn_fs_t *fs,
                              apr_pool_t *result_pool)
{
  return svn_dirent_join(fs->path, PATH_TXN_PROTOS_DIR, result_pool);
}

const char *
svn_fs_x__path_txn_item_index(svn_fs_t *fs,
                              svn_fs_x__txn_id_t txn_id,
                              apr_pool_t *result_pool)
{
  return construct_txn_path(fs, txn_id, PATH_TXN_ITEM_INDEX, result_pool);
}

/* Return the full path of the proto-rev file / lock file for transaction
 * TXN_ID in FS.  The SUFFIX determines what file (rev / lock) it will be.
 *
 * Allocate the result in RESULT_POOL.
 */
static const char *
construct_proto_rev_path(svn_fs_t *fs,
                         svn_fs_x__txn_id_t txn_id,
                         const char *suffix,
                         apr_pool_t *result_pool)
{
  /* Construct the file name without temp. allocations. */
  char buffer[SVN_INT64_BUFFER_SIZE + sizeof(PATH_EXT_REV_LOCK)] = { 0 };
  apr_size_t len = svn__ui64tobase36(buffer, txn_id);
  strncpy(buffer + len, suffix, sizeof(buffer) - len - 1);

  /* If FILENAME is NULL, it will terminate the list of segments
     to concatenate. */
  return svn_dirent_join_many(result_pool, fs->path, PATH_TXN_PROTOS_DIR,
                              buffer, SVN_VA_NULL);
}

const char *
svn_fs_x__path_txn_proto_rev(svn_fs_t *fs,
                             svn_fs_x__txn_id_t txn_id,
                             apr_pool_t *result_pool)
{
  return construct_proto_rev_path(fs, txn_id, PATH_EXT_REV, result_pool);
}

const char *
svn_fs_x__path_txn_proto_rev_lock(svn_fs_t *fs,
                                  svn_fs_x__txn_id_t txn_id,
                                  apr_pool_t *result_pool)
{
  return construct_proto_rev_path(fs, txn_id, PATH_EXT_REV_LOCK, result_pool);
}

/* Return the full path of the noderev-related file with the extension SUFFIX
 * for noderev *ID in transaction TXN_ID in FS.
 *
 * Allocate the result in RESULT_POOL and temporaries in SCRATCH_POOL.
 */
static const char *
construct_txn_node_path(svn_fs_t *fs,
                        const svn_fs_x__id_t *id,
                        const char *suffix,
                        apr_pool_t *result_pool,
                        apr_pool_t *scratch_pool)
{
  const char *filename = svn_fs_x__id_unparse(id, result_pool)->data;
  apr_int64_t txn_id = svn_fs_x__get_txn_id(id->change_set);

  return svn_dirent_join(svn_fs_x__path_txn_dir(fs, txn_id, scratch_pool),
                         apr_psprintf(scratch_pool, PATH_PREFIX_NODE "%s%s",
                                      filename, suffix),
                         result_pool);
}

const char *
svn_fs_x__path_txn_node_rev(svn_fs_t *fs,
                            const svn_fs_x__id_t *id,
                            apr_pool_t *result_pool,
                            apr_pool_t *scratch_pool)
{
  return construct_txn_node_path(fs, id, "", result_pool, scratch_pool);
}

const char *
svn_fs_x__path_txn_node_props(svn_fs_t *fs,
                              const svn_fs_x__id_t *id,
                              apr_pool_t *result_pool,
                              apr_pool_t *scratch_pool)
{
  return construct_txn_node_path(fs, id, PATH_EXT_PROPS, result_pool,
                                 scratch_pool);
}

const char *
svn_fs_x__path_txn_node_children(svn_fs_t *fs,
                                 const svn_fs_x__id_t *id,
                                 apr_pool_t *result_pool,
                                 apr_pool_t *scratch_pool)
{
  return construct_txn_node_path(fs, id, PATH_EXT_CHILDREN, result_pool,
                                 scratch_pool);
}

svn_error_t *
svn_fs_x__check_file_buffer_numeric(const char *buf,
                                    apr_off_t offset,
                                    const char *path,
                                    const char *title,
                                    apr_pool_t *scratch_pool)
{
  const char *p;

  for (p = buf + offset; *p; p++)
    if (!svn_ctype_isdigit(*p))
      return svn_error_createf(SVN_ERR_BAD_VERSION_FILE_FORMAT, NULL,
        _("%s file '%s' contains unexpected non-digit '%c' within '%s'"),
        title, svn_dirent_local_style(path, scratch_pool), *p, buf);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_x__read_min_unpacked_rev(svn_revnum_t *min_unpacked_rev,
                                svn_fs_t *fs,
                                apr_pool_t *scratch_pool)
{
  char buf[80];
  apr_file_t *file;
  apr_size_t len;

  SVN_ERR(svn_io_file_open(&file,
                           svn_fs_x__path_min_unpacked_rev(fs, scratch_pool),
                           APR_READ | APR_BUFFERED,
                           APR_OS_DEFAULT,
                           scratch_pool));
  len = sizeof(buf);
  SVN_ERR(svn_io_read_length_line(file, buf, &len, scratch_pool));
  SVN_ERR(svn_io_file_close(file, scratch_pool));

  SVN_ERR(svn_revnum_parse(min_unpacked_rev, buf, NULL));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_x__update_min_unpacked_rev(svn_fs_t *fs,
                                  apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = fs->fsap_data;
  return svn_fs_x__read_min_unpacked_rev(&ffd->min_unpacked_rev, fs,
                                         scratch_pool);
}

/* Write a file FILENAME in directory FS_PATH, containing a single line
 * with the number REVNUM in ASCII decimal.  Move the file into place
 * atomically, overwriting any existing file.
 *
 * Similar to write_current(). */
svn_error_t *
svn_fs_x__write_min_unpacked_rev(svn_fs_t *fs,
                                 svn_revnum_t revnum,
                                 apr_pool_t *scratch_pool)
{
  const char *final_path;
  char buf[SVN_INT64_BUFFER_SIZE];
  apr_size_t len = svn__i64toa(buf, revnum);
  buf[len] = '\n';

  final_path = svn_fs_x__path_min_unpacked_rev(fs, scratch_pool);

  SVN_ERR(svn_io_write_atomic(final_path, buf, len + 1,
                              final_path /* copy_perms */, scratch_pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_x__read_current(svn_revnum_t *rev,
                       svn_fs_t *fs,
                       apr_pool_t *scratch_pool)
{
  const char *str;
  svn_stringbuf_t *content;
  SVN_ERR(svn_fs_x__read_content(&content,
                                 svn_fs_x__path_current(fs, scratch_pool),
                                 scratch_pool));
  SVN_ERR(svn_revnum_parse(rev, content->data, &str));
  if (*str != '\n')
    return svn_error_create(SVN_ERR_FS_CORRUPT, NULL,
                            _("Corrupt 'current' file"));

  return SVN_NO_ERROR;
}

/* Atomically update the 'current' file to hold the specifed REV.
   Perform temporary allocations in SCRATCH_POOL. */
svn_error_t *
svn_fs_x__write_current(svn_fs_t *fs,
                        svn_revnum_t rev,
                        apr_pool_t *scratch_pool)
{
  char *buf;
  const char *tmp_name, *name;

  /* Now we can just write out this line. */
  buf = apr_psprintf(scratch_pool, "%ld\n", rev);

  name = svn_fs_x__path_current(fs, scratch_pool);
  SVN_ERR(svn_io_write_unique(&tmp_name,
                              svn_dirent_dirname(name, scratch_pool),
                              buf, strlen(buf),
                              svn_io_file_del_none, scratch_pool));

  return svn_fs_x__move_into_place(tmp_name, name, name, scratch_pool);
}


svn_error_t *
svn_fs_x__try_stringbuf_from_file(svn_stringbuf_t **content,
                                  svn_boolean_t *missing,
                                  const char *path,
                                  svn_boolean_t last_attempt,
                                  apr_pool_t *result_pool)
{
  svn_error_t *err = svn_stringbuf_from_file2(content, path, result_pool);
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

/* Fetch the current offset of FILE into *OFFSET_P. */
svn_error_t *
svn_fs_x__get_file_offset(apr_off_t *offset_p,
                          apr_file_t *file,
                          apr_pool_t *scratch_pool)
{
  apr_off_t offset;

  /* Note that, for buffered files, one (possibly surprising) side-effect
     of this call is to flush any unwritten data to disk. */
  offset = 0;
  SVN_ERR(svn_io_file_seek(file, APR_CUR, &offset, scratch_pool));
  *offset_p = offset;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_x__read_content(svn_stringbuf_t **content,
                       const char *fname,
                       apr_pool_t *result_pool)
{
  int i;
  *content = NULL;

  for (i = 0; !*content && (i < SVN_FS_X__RECOVERABLE_RETRY_COUNT); ++i)
    SVN_ERR(svn_fs_x__try_stringbuf_from_file(content, NULL,
                           fname, i + 1 < SVN_FS_X__RECOVERABLE_RETRY_COUNT,
                           result_pool));

  if (!*content)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             _("Can't read '%s'"),
                             svn_dirent_local_style(fname, result_pool));

  return SVN_NO_ERROR;
}

/* Reads a line from STREAM and converts it to a 64 bit integer to be
 * returned in *RESULT.  If we encounter eof, set *HIT_EOF and leave
 * *RESULT unchanged.  If HIT_EOF is NULL, EOF causes an "corrupt FS"
 * error return.
 * SCRATCH_POOL is used for temporary allocations.
 */
svn_error_t *
svn_fs_x__read_number_from_stream(apr_int64_t *result,
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


/* Move a file into place from OLD_FILENAME in the transactions
   directory to its final location NEW_FILENAME in the repository.  On
   Unix, match the permissions of the new file to the permissions of
   PERMS_REFERENCE.  Temporary allocations are from SCRATCH_POOL.

   This function almost duplicates svn_io_file_move(), but it tries to
   guarantee a flush. */
svn_error_t *
svn_fs_x__move_into_place(const char *old_filename,
                          const char *new_filename,
                          const char *perms_reference,
                          apr_pool_t *scratch_pool)
{
  svn_error_t *err;

  SVN_ERR(svn_io_copy_perms(perms_reference, old_filename, scratch_pool));

  /* Move the file into place. */
  err = svn_io_file_rename(old_filename, new_filename, scratch_pool);
  if (err && APR_STATUS_IS_EXDEV(err->apr_err))
    {
      apr_file_t *file;

      /* Can't rename across devices; fall back to copying. */
      svn_error_clear(err);
      err = SVN_NO_ERROR;
      SVN_ERR(svn_io_copy_file(old_filename, new_filename, TRUE,
                               scratch_pool));

      /* Flush the target of the copy to disk. */
      SVN_ERR(svn_io_file_open(&file, new_filename, APR_READ,
                               APR_OS_DEFAULT, scratch_pool));
      /* ### BH: Does this really guarantee a flush of the data written
         ### via a completely different handle on all operating systems?
         ###
         ### Maybe we should perform the copy ourselves instead of making
         ### apr do that and flush the real handle? */
      SVN_ERR(svn_io_file_flush_to_disk(file, scratch_pool));
      SVN_ERR(svn_io_file_close(file, scratch_pool));
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

    dirname = svn_dirent_dirname(new_filename, scratch_pool);
    SVN_ERR(svn_io_file_open(&file, dirname, APR_READ, APR_OS_DEFAULT,
                             scratch_pool));
    SVN_ERR(svn_io_file_flush_to_disk(file, scratch_pool));
    SVN_ERR(svn_io_file_close(file, scratch_pool));
  }
#endif

  return SVN_NO_ERROR;
}
