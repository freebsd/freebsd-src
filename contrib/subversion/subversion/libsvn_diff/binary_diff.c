/*
 * binary_diff.c:  handling of git like binary diffs
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

#include <apr.h>

#include "svn_pools.h"
#include "svn_error.h"
#include "svn_diff.h"
#include "svn_types.h"

/* Copies the data from ORIGINAL_STREAM to a temporary file, returning both
   the original and compressed size. */
static svn_error_t *
create_compressed(apr_file_t **result,
                  svn_filesize_t *full_size,
                  svn_filesize_t *compressed_size,
                  svn_stream_t *original_stream,
                  svn_cancel_func_t cancel_func,
                  void *cancel_baton,
                  apr_pool_t *result_pool,
                  apr_pool_t *scratch_pool)
{
  svn_stream_t *compressed;
  svn_filesize_t bytes_read = 0;
  apr_finfo_t finfo;
  apr_size_t rd;

  SVN_ERR(svn_io_open_uniquely_named(result, NULL, NULL, "diffgz",
                                     NULL, svn_io_file_del_on_pool_cleanup,
                                     result_pool, scratch_pool));

  compressed = svn_stream_compressed(
                  svn_stream_from_aprfile2(*result, TRUE, scratch_pool),
                  scratch_pool);

  if (original_stream)
    do
    {
      char buffer[SVN_STREAM_CHUNK_SIZE];
      rd = sizeof(buffer);

      if (cancel_func)
        SVN_ERR(cancel_func(cancel_baton));

      SVN_ERR(svn_stream_read_full(original_stream, buffer, &rd));

      bytes_read += rd;
      SVN_ERR(svn_stream_write(compressed, buffer, &rd));
    }
    while(rd == SVN_STREAM_CHUNK_SIZE);
  else
    {
      apr_size_t zero = 0;
      SVN_ERR(svn_stream_write(compressed, NULL, &zero));
    }

  SVN_ERR(svn_stream_close(compressed)); /* Flush compression */

  *full_size = bytes_read;
  SVN_ERR(svn_io_file_info_get(&finfo, APR_FINFO_SIZE, *result, scratch_pool));
  *compressed_size = finfo.size;

  return SVN_NO_ERROR;
}

#define GIT_BASE85_CHUNKSIZE 52

/* Git Base-85 table for write_literal */
static const char b85str[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "!#$%&()*+-;<=>?@^_`{|}~";

/* Git length encoding table for write_literal */
static const char b85lenstr[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";

/* Writes out a git-like literal output of the compressed data in
   COMPRESSED_DATA to OUTPUT_STREAM, describing that its normal length is
   UNCOMPRESSED_SIZE. */
static svn_error_t *
write_literal(svn_filesize_t uncompressed_size,
              svn_stream_t *compressed_data,
              svn_stream_t *output_stream,
              svn_cancel_func_t cancel_func,
              void *cancel_baton,
              apr_pool_t *scratch_pool)
{
  apr_size_t rd;
  SVN_ERR(svn_stream_seek(compressed_data, NULL)); /* Seek to start */

  SVN_ERR(svn_stream_printf(output_stream, scratch_pool,
                            "literal %" SVN_FILESIZE_T_FMT APR_EOL_STR,
                            uncompressed_size));

  do
    {
      char chunk[GIT_BASE85_CHUNKSIZE];
      const unsigned char *next;
      apr_size_t left;

      rd = sizeof(chunk);

      if (cancel_func)
        SVN_ERR(cancel_func(cancel_baton));

      SVN_ERR(svn_stream_read_full(compressed_data, chunk, &rd));

      {
        apr_size_t one = 1;
        SVN_ERR(svn_stream_write(output_stream, &b85lenstr[rd-1], &one));
      }

      left = rd;
      next = (void*)chunk;
      while (left)
      {
        char five[5];
        unsigned info = 0;
        int n;
        apr_size_t five_sz;

        /* Push 4 bytes into the 32 bit info, when available */
        for (n = 24; n >= 0 && left; n -= 8, next++, left--)
        {
            info |= (*next) << n;
        }

        /* Write out info as base85 */
        for (n = 4; n >= 0; n--)
        {
            five[n] = b85str[info % 85];
            info /= 85;
        }

        five_sz = 5;
        SVN_ERR(svn_stream_write(output_stream, five, &five_sz));
      }

      SVN_ERR(svn_stream_puts(output_stream, APR_EOL_STR));
    }
  while (rd == GIT_BASE85_CHUNKSIZE);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_diff_output_binary(svn_stream_t *output_stream,
                       svn_stream_t *original,
                       svn_stream_t *latest,
                       svn_cancel_func_t cancel_func,
                       void *cancel_baton,
                       apr_pool_t *scratch_pool)
{
  apr_file_t *original_apr;
  svn_filesize_t original_full;
  svn_filesize_t original_deflated;
  apr_file_t *latest_apr;
  svn_filesize_t latest_full;
  svn_filesize_t latest_deflated;
  apr_pool_t *subpool = svn_pool_create(scratch_pool);

  SVN_ERR(create_compressed(&original_apr, &original_full, &original_deflated,
                            original, cancel_func, cancel_baton,
                            scratch_pool, subpool));
  svn_pool_clear(subpool);

  SVN_ERR(create_compressed(&latest_apr, &latest_full, &latest_deflated,
                            latest,  cancel_func, cancel_baton,
                            scratch_pool, subpool));
  svn_pool_clear(subpool);

  SVN_ERR(svn_stream_puts(output_stream, "GIT binary patch" APR_EOL_STR));

  /* ### git would first calculate if a git-delta latest->original would be
         shorter than the zipped data. For now lets assume that it is not
         and just dump the literal data */
  SVN_ERR(write_literal(latest_full,
                        svn_stream_from_aprfile2(latest_apr, FALSE, subpool),
                        output_stream,
                        cancel_func, cancel_baton,
                        scratch_pool));
  svn_pool_clear(subpool);
  SVN_ERR(svn_stream_puts(output_stream, APR_EOL_STR));

  /* ### git would first calculate if a git-delta original->latest would be
         shorter than the zipped data. For now lets assume that it is not
         and just dump the literal data */
  SVN_ERR(write_literal(original_full,
                        svn_stream_from_aprfile2(original_apr, FALSE, subpool),
                        output_stream,
                        cancel_func, cancel_baton,
                        scratch_pool));
  svn_pool_destroy(subpool);

  SVN_ERR(svn_stream_puts(output_stream, APR_EOL_STR));

  return SVN_NO_ERROR;
}
