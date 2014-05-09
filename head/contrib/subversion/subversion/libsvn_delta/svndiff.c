/*
 * svndiff.c -- Encoding and decoding svndiff-format deltas.
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
#include <string.h>
#include "svn_delta.h"
#include "svn_io.h"
#include "delta.h"
#include "svn_pools.h"
#include "svn_private_config.h"
#include <zlib.h>

#include "private/svn_error_private.h"
#include "private/svn_delta_private.h"

/* The zlib compressBound function was not exported until 1.2.0. */
#if ZLIB_VERNUM >= 0x1200
#define svnCompressBound(LEN) compressBound(LEN)
#else
#define svnCompressBound(LEN) ((LEN) + ((LEN) >> 12) + ((LEN) >> 14) + 11)
#endif

/* For svndiff1, address/instruction/new data under this size will not
   be compressed using zlib as a secondary compressor.  */
#define MIN_COMPRESS_SIZE 512

/* ----- Text delta to svndiff ----- */

/* We make one of these and get it passed back to us in calls to the
   window handler.  We only use it to record the write function and
   baton passed to svn_txdelta_to_svndiff3().  */
struct encoder_baton {
  svn_stream_t *output;
  svn_boolean_t header_done;
  int version;
  int compression_level;
  apr_pool_t *pool;
};

/* This is at least as big as the largest size of an integer that
   encode_int can generate; it is sufficient for creating buffers for
   it to write into.  This assumes that integers are at most 64 bits,
   and so 10 bytes (with 7 bits of information each) are sufficient to
   represent them. */
#define MAX_ENCODED_INT_LEN 10
/* This is at least as big as the largest size for a single instruction. */
#define MAX_INSTRUCTION_LEN (2*MAX_ENCODED_INT_LEN+1)
/* This is at least as big as the largest possible instructions
   section: in theory, the instructions could be SVN_DELTA_WINDOW_SIZE
   1-byte copy-from-source instructions (though this is very unlikely). */
#define MAX_INSTRUCTION_SECTION_LEN (SVN_DELTA_WINDOW_SIZE*MAX_INSTRUCTION_LEN)

/* Encode VAL into the buffer P using the variable-length svndiff
   integer format.  Return the incremented value of P after the
   encoded bytes have been written.  P must point to a buffer of size
   at least MAX_ENCODED_INT_LEN.

   This encoding uses the high bit of each byte as a continuation bit
   and the other seven bits as data bits.  High-order data bits are
   encoded first, followed by lower-order bits, so the value can be
   reconstructed by concatenating the data bits from left to right and
   interpreting the result as a binary number.  Examples (brackets
   denote byte boundaries, spaces are for clarity only):

           1 encodes as [0 0000001]
          33 encodes as [0 0100001]
         129 encodes as [1 0000001] [0 0000001]
        2000 encodes as [1 0001111] [0 1010000]
*/
static unsigned char *
encode_int(unsigned char *p, svn_filesize_t val)
{
  int n;
  svn_filesize_t v;
  unsigned char cont;

  SVN_ERR_ASSERT_NO_RETURN(val >= 0);

  /* Figure out how many bytes we'll need.  */
  v = val >> 7;
  n = 1;
  while (v > 0)
    {
      v = v >> 7;
      n++;
    }

  SVN_ERR_ASSERT_NO_RETURN(n <= MAX_ENCODED_INT_LEN);

  /* Encode the remaining bytes; n is always the number of bytes
     coming after the one we're encoding.  */
  while (--n >= 0)
    {
      cont = ((n > 0) ? 0x1 : 0x0) << 7;
      *p++ = (unsigned char)(((val >> (n * 7)) & 0x7f) | cont);
    }

  return p;
}


/* Append an encoded integer to a string.  */
static void
append_encoded_int(svn_stringbuf_t *header, svn_filesize_t val)
{
  unsigned char buf[MAX_ENCODED_INT_LEN], *p;

  p = encode_int(buf, val);
  svn_stringbuf_appendbytes(header, (const char *)buf, p - buf);
}

/* If IN is a string that is >= MIN_COMPRESS_SIZE and the COMPRESSION_LEVEL
   is not SVN_DELTA_COMPRESSION_LEVEL_NONE, zlib compress it and places the
   result in OUT, with an integer prepended specifying the original size.
   If IN is < MIN_COMPRESS_SIZE, or if the compressed version of IN was no
   smaller than the original IN, OUT will be a copy of IN with the size
   prepended as an integer. */
static svn_error_t *
zlib_encode(const char *data,
            apr_size_t len,
            svn_stringbuf_t *out,
            int compression_level)
{
  unsigned long endlen;
  apr_size_t intlen;

  svn_stringbuf_setempty(out);
  append_encoded_int(out, len);
  intlen = out->len;

  /* Compression initialization overhead is considered to large for
     short buffers.  Also, if we don't actually want to compress data,
     ZLIB will produce an output no shorter than the input.  Hence,
     the DATA would directly appended to OUT, so we can do that directly
     without calling ZLIB before. */
  if (   (len < MIN_COMPRESS_SIZE)
      || (compression_level == SVN_DELTA_COMPRESSION_LEVEL_NONE))
    {
      svn_stringbuf_appendbytes(out, data, len);
    }
  else
    {
      int zerr;

      svn_stringbuf_ensure(out, svnCompressBound(len) + intlen);
      endlen = out->blocksize;

      zerr = compress2((unsigned char *)out->data + intlen, &endlen,
                       (const unsigned char *)data, len,
                       compression_level);
      if (zerr != Z_OK)
        return svn_error_trace(svn_error__wrap_zlib(
                                 zerr, "compress2",
                                 _("Compression of svndiff data failed")));

      /* Compression didn't help :(, just append the original text */
      if (endlen >= len)
        {
          svn_stringbuf_appendbytes(out, data, len);
          return SVN_NO_ERROR;
        }
      out->len = endlen + intlen;
      out->data[out->len] = 0;
    }
  return SVN_NO_ERROR;
}

static svn_error_t *
send_simple_insertion_window(svn_txdelta_window_t *window,
                             struct encoder_baton *eb)
{
  unsigned char headers[4 + 5 * MAX_ENCODED_INT_LEN + MAX_INSTRUCTION_LEN];
  unsigned char ibuf[MAX_INSTRUCTION_LEN];
  unsigned char *header_current;
  apr_size_t header_len;
  apr_size_t ip_len, i;
  apr_size_t len = window->new_data->len;

  /* there is only one target copy op. It must span the whole window */
  assert(window->ops[0].action_code == svn_txdelta_new);
  assert(window->ops[0].length == window->tview_len);
  assert(window->ops[0].offset == 0);

  /* write stream header if necessary */
  if (!eb->header_done)
    {
      eb->header_done = TRUE;
      headers[0] = 'S';
      headers[1] = 'V';
      headers[2] = 'N';
      headers[3] = (unsigned char)eb->version;
      header_current = headers + 4;
    }
  else
    {
      header_current = headers;
    }

  /* Encode the action code and length.  */
  if (window->tview_len >> 6 == 0)
    {
      ibuf[0] = (unsigned char)(window->tview_len + (0x2 << 6));
      ip_len = 1;
    }
  else
    {
      ibuf[0] = (0x2 << 6);
      ip_len = encode_int(ibuf + 1, window->tview_len) - ibuf;
    }

  /* encode the window header.  Please note that the source window may
   * have content despite not being used for deltification. */
  header_current = encode_int(header_current, window->sview_offset);
  header_current = encode_int(header_current, window->sview_len);
  header_current = encode_int(header_current, window->tview_len);
  header_current[0] = (unsigned char)ip_len;  /* 1 instruction */
  header_current = encode_int(&header_current[1], len);

  /* append instructions (1 to a handful of bytes) */
  for (i = 0; i < ip_len; ++i)
    header_current[i] = ibuf[i];

  header_len = header_current - headers + ip_len;

  /* Write out the window.  */
  SVN_ERR(svn_stream_write(eb->output, (const char *)headers, &header_len));
  if (len)
    SVN_ERR(svn_stream_write(eb->output, window->new_data->data, &len));

  return SVN_NO_ERROR;
}

static svn_error_t *
window_handler(svn_txdelta_window_t *window, void *baton)
{
  struct encoder_baton *eb = baton;
  apr_pool_t *pool;
  svn_stringbuf_t *instructions;
  svn_stringbuf_t *i1;
  svn_stringbuf_t *header;
  const svn_string_t *newdata;
  unsigned char ibuf[MAX_INSTRUCTION_LEN], *ip;
  const svn_txdelta_op_t *op;
  apr_size_t len;

  /* use specialized code if there is no source */
  if (window && !window->src_ops && window->num_ops == 1 && !eb->version)
    return svn_error_trace(send_simple_insertion_window(window, eb));

  /* Make sure we write the header.  */
  if (!eb->header_done)
    {
      char svnver[4] = {'S','V','N','\0'};
      len = 4;
      svnver[3] = (char)eb->version;
      SVN_ERR(svn_stream_write(eb->output, svnver, &len));
      eb->header_done = TRUE;
    }

  if (window == NULL)
    {
      svn_stream_t *output = eb->output;

      /* We're done; clean up.

         We clean our pool first. Given that the output stream was passed
         TO us, we'll assume it has a longer lifetime, and that it will not
         be affected by our pool destruction.

         The contrary point of view (close the stream first): that could
         tell our user that everything related to the output stream is done,
         and a cleanup of the user pool should occur. However, that user
         pool could include the subpool we created for our work (eb->pool),
         which would then make our call to svn_pool_destroy() puke.
       */
      svn_pool_destroy(eb->pool);

      return svn_stream_close(output);
    }

  /* create the necessary data buffers */
  pool = svn_pool_create(eb->pool);
  instructions = svn_stringbuf_create_empty(pool);
  i1 = svn_stringbuf_create_empty(pool);
  header = svn_stringbuf_create_empty(pool);

  /* Encode the instructions.  */
  for (op = window->ops; op < window->ops + window->num_ops; op++)
    {
      /* Encode the action code and length.  */
      ip = ibuf;
      switch (op->action_code)
        {
        case svn_txdelta_source: *ip = 0; break;
        case svn_txdelta_target: *ip = (0x1 << 6); break;
        case svn_txdelta_new:    *ip = (0x2 << 6); break;
        }
      if (op->length >> 6 == 0)
        *ip++ |= (unsigned char)op->length;
      else
        ip = encode_int(ip + 1, op->length);
      if (op->action_code != svn_txdelta_new)
        ip = encode_int(ip, op->offset);
      svn_stringbuf_appendbytes(instructions, (const char *)ibuf, ip - ibuf);
    }

  /* Encode the header.  */
  append_encoded_int(header, window->sview_offset);
  append_encoded_int(header, window->sview_len);
  append_encoded_int(header, window->tview_len);
  if (eb->version == 1)
    {
      SVN_ERR(zlib_encode(instructions->data, instructions->len,
                          i1, eb->compression_level));
      instructions = i1;
    }
  append_encoded_int(header, instructions->len);
  if (eb->version == 1)
    {
      svn_stringbuf_t *temp = svn_stringbuf_create_empty(pool);
      svn_string_t *tempstr = svn_string_create_empty(pool);
      SVN_ERR(zlib_encode(window->new_data->data, window->new_data->len,
                          temp, eb->compression_level));
      tempstr->data = temp->data;
      tempstr->len = temp->len;
      newdata = tempstr;
    }
  else
    newdata = window->new_data;

  append_encoded_int(header, newdata->len);

  /* Write out the window.  */
  len = header->len;
  SVN_ERR(svn_stream_write(eb->output, header->data, &len));
  if (instructions->len > 0)
    {
      len = instructions->len;
      SVN_ERR(svn_stream_write(eb->output, instructions->data, &len));
    }
  if (newdata->len > 0)
    {
      len = newdata->len;
      SVN_ERR(svn_stream_write(eb->output, newdata->data, &len));
    }

  svn_pool_destroy(pool);
  return SVN_NO_ERROR;
}

void
svn_txdelta_to_svndiff3(svn_txdelta_window_handler_t *handler,
                        void **handler_baton,
                        svn_stream_t *output,
                        int svndiff_version,
                        int compression_level,
                        apr_pool_t *pool)
{
  apr_pool_t *subpool = svn_pool_create(pool);
  struct encoder_baton *eb;

  eb = apr_palloc(subpool, sizeof(*eb));
  eb->output = output;
  eb->header_done = FALSE;
  eb->pool = subpool;
  eb->version = svndiff_version;
  eb->compression_level = compression_level;

  *handler = window_handler;
  *handler_baton = eb;
}

void
svn_txdelta_to_svndiff2(svn_txdelta_window_handler_t *handler,
                        void **handler_baton,
                        svn_stream_t *output,
                        int svndiff_version,
                        apr_pool_t *pool)
{
  svn_txdelta_to_svndiff3(handler, handler_baton, output, svndiff_version,
                          SVN_DELTA_COMPRESSION_LEVEL_DEFAULT, pool);
}

void
svn_txdelta_to_svndiff(svn_stream_t *output,
                       apr_pool_t *pool,
                       svn_txdelta_window_handler_t *handler,
                       void **handler_baton)
{
  svn_txdelta_to_svndiff3(handler, handler_baton, output, 0,
                          SVN_DELTA_COMPRESSION_LEVEL_DEFAULT, pool);
}


/* ----- svndiff to text delta ----- */

/* An svndiff parser object.  */
struct decode_baton
{
  /* Once the svndiff parser has enough data buffered to create a
     "window", it passes this window to the caller's consumer routine.  */
  svn_txdelta_window_handler_t consumer_func;
  void *consumer_baton;

  /* Pool to create subpools from; each developing window will be a
     subpool.  */
  apr_pool_t *pool;

  /* The current subpool which contains our current window-buffer.  */
  apr_pool_t *subpool;

  /* The actual svndiff data buffer, living within subpool.  */
  svn_stringbuf_t *buffer;

  /* The offset and size of the last source view, so that we can check
     to make sure the next one isn't sliding backwards.  */
  svn_filesize_t last_sview_offset;
  apr_size_t last_sview_len;

  /* We have to discard four bytes at the beginning for the header.
     This field keeps track of how many of those bytes we have read.  */
  apr_size_t header_bytes;

  /* Do we want an error to occur when we close the stream that
     indicates we didn't send the whole svndiff data?  If you plan to
     not transmit the whole svndiff data stream, you will want this to
     be FALSE. */
  svn_boolean_t error_on_early_close;

  /* svndiff version in use by delta.  */
  unsigned char version;
};


/* Decode an svndiff-encoded integer into *VAL and return a pointer to
   the byte after the integer.  The bytes to be decoded live in the
   range [P..END-1].  If these bytes do not contain a whole encoded
   integer, return NULL; in this case *VAL is undefined.

   See the comment for encode_int() earlier in this file for more detail on
   the encoding format.  */
static const unsigned char *
decode_file_offset(svn_filesize_t *val,
                   const unsigned char *p,
                   const unsigned char *end)
{
  svn_filesize_t temp = 0;

  if (p + MAX_ENCODED_INT_LEN < end)
    end = p + MAX_ENCODED_INT_LEN;
  /* Decode bytes until we're done.  */
  while (p < end)
    {
      /* Don't use svn_filesize_t here, because this might be 64 bits
       * on 32 bit targets. Optimizing compilers may or may not be
       * able to reduce that to the effective code below. */
      unsigned int c = *p++;

      temp = (temp << 7) | (c & 0x7f);
      if (c < 0x80)
      {
        *val = temp;
        return p;
      }
    }

  return NULL;
}


/* Same as above, only decode into a size variable. */
static const unsigned char *
decode_size(apr_size_t *val,
            const unsigned char *p,
            const unsigned char *end)
{
  apr_size_t temp = 0;

  if (p + MAX_ENCODED_INT_LEN < end)
    end = p + MAX_ENCODED_INT_LEN;
  /* Decode bytes until we're done.  */
  while (p < end)
    {
      apr_size_t c = *p++;

      temp = (temp << 7) | (c & 0x7f);
      if (c < 0x80)
      {
        *val = temp;
        return p;
      }
    }

  return NULL;
}

/* Decode the possibly-zlib compressed string of length INLEN that is in
   IN, into OUT.  We expect an integer is prepended to IN that specifies
   the original size, and that if encoded size == original size, that the
   remaining data is not compressed.
   In that case, we will simply return pointer into IN as data pointer for
   OUT, COPYLESS_ALLOWED has been set.  The, the caller is expected not to
   modify the contents of OUT.
   An error is returned if the decoded length exceeds the given LIMIT.
 */
static svn_error_t *
zlib_decode(const unsigned char *in, apr_size_t inLen, svn_stringbuf_t *out,
            apr_size_t limit)
{
  apr_size_t len;
  const unsigned char *oldplace = in;

  /* First thing in the string is the original length.  */
  in = decode_size(&len, in, in + inLen);
  if (in == NULL)
    return svn_error_create(SVN_ERR_SVNDIFF_INVALID_COMPRESSED_DATA, NULL,
                            _("Decompression of svndiff data failed: no size"));
  if (len > limit)
    return svn_error_create(SVN_ERR_SVNDIFF_INVALID_COMPRESSED_DATA, NULL,
                            _("Decompression of svndiff data failed: "
                              "size too large"));
  /* We need to subtract the size of the encoded original length off the
   *      still remaining input length.  */
  inLen -= (in - oldplace);
  if (inLen == len)
    {
      svn_stringbuf_ensure(out, len);
      memcpy(out->data, in, len);
      out->data[len] = 0;
      out->len = len;

      return SVN_NO_ERROR;
    }
  else
    {
      unsigned long zlen = len;
      int zerr;

      svn_stringbuf_ensure(out, len);
      zerr = uncompress((unsigned char *)out->data, &zlen, in, inLen);
      if (zerr != Z_OK)
        return svn_error_trace(svn_error__wrap_zlib(
                                 zerr, "uncompress",
                                 _("Decompression of svndiff data failed")));

      /* Zlib should not produce something that has a different size than the
         original length we stored. */
      if (zlen != len)
        return svn_error_create(SVN_ERR_SVNDIFF_INVALID_COMPRESSED_DATA,
                                NULL,
                                _("Size of uncompressed data "
                                  "does not match stored original length"));
      out->data[zlen] = 0;
      out->len = zlen;
    }
  return SVN_NO_ERROR;
}

/* Decode an instruction into OP, returning a pointer to the text
   after the instruction.  Note that if the action code is
   svn_txdelta_new, the offset field of *OP will not be set.  */
static const unsigned char *
decode_instruction(svn_txdelta_op_t *op,
                   const unsigned char *p,
                   const unsigned char *end)
{
  apr_size_t c;
  apr_size_t action;

  if (p == end)
    return NULL;

  /* We need this more than once */
  c = *p++;

  /* Decode the instruction selector.  */
  action = (c >> 6) & 0x3;
  if (action >= 0x3)
      return NULL;

  /* This relies on enum svn_delta_action values to match and never to be
     redefined. */
  op->action_code = (enum svn_delta_action)(action);

  /* Decode the length and offset.  */
  op->length = c & 0x3f;
  if (op->length == 0)
    {
      p = decode_size(&op->length, p, end);
      if (p == NULL)
        return NULL;
    }
  if (action != svn_txdelta_new)
    {
      p = decode_size(&op->offset, p, end);
      if (p == NULL)
        return NULL;
    }

  return p;
}

/* Count the instructions in the range [P..END-1] and make sure they
   are valid for the given window lengths.  Return an error if the
   instructions are invalid; otherwise set *NINST to the number of
   instructions.  */
static svn_error_t *
count_and_verify_instructions(int *ninst,
                              const unsigned char *p,
                              const unsigned char *end,
                              apr_size_t sview_len,
                              apr_size_t tview_len,
                              apr_size_t new_len)
{
  int n = 0;
  svn_txdelta_op_t op;
  apr_size_t tpos = 0, npos = 0;

  while (p < end)
    {
      p = decode_instruction(&op, p, end);

      /* Detect any malformed operations from the instruction stream. */
      if (p == NULL)
        return svn_error_createf
          (SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
           _("Invalid diff stream: insn %d cannot be decoded"), n);
      else if (op.length == 0)
        return svn_error_createf
          (SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
           _("Invalid diff stream: insn %d has length zero"), n);
      else if (op.length > tview_len - tpos)
        return svn_error_createf
          (SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
           _("Invalid diff stream: insn %d overflows the target view"), n);

      switch (op.action_code)
        {
        case svn_txdelta_source:
          if (op.length > sview_len - op.offset ||
              op.offset > sview_len)
            return svn_error_createf
              (SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
               _("Invalid diff stream: "
                 "[src] insn %d overflows the source view"), n);
          break;
        case svn_txdelta_target:
          if (op.offset >= tpos)
            return svn_error_createf
              (SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
               _("Invalid diff stream: "
                 "[tgt] insn %d starts beyond the target view position"), n);
          break;
        case svn_txdelta_new:
          if (op.length > new_len - npos)
            return svn_error_createf
              (SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
               _("Invalid diff stream: "
                 "[new] insn %d overflows the new data section"), n);
          npos += op.length;
          break;
        }
      tpos += op.length;
      n++;
    }
  if (tpos != tview_len)
    return svn_error_create(SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
                            _("Delta does not fill the target window"));
  if (npos != new_len)
    return svn_error_create(SVN_ERR_SVNDIFF_INVALID_OPS, NULL,
                            _("Delta does not contain enough new data"));

  *ninst = n;
  return SVN_NO_ERROR;
}

/* Given the five integer fields of a window header and a pointer to
   the remainder of the window contents, fill in a delta window
   structure *WINDOW.  New allocations will be performed in POOL;
   the new_data field of *WINDOW will refer directly to memory pointed
   to by DATA. */
static svn_error_t *
decode_window(svn_txdelta_window_t *window, svn_filesize_t sview_offset,
              apr_size_t sview_len, apr_size_t tview_len, apr_size_t inslen,
              apr_size_t newlen, const unsigned char *data, apr_pool_t *pool,
              unsigned int version)
{
  const unsigned char *insend;
  int ninst;
  apr_size_t npos;
  svn_txdelta_op_t *ops, *op;
  svn_string_t *new_data = apr_palloc(pool, sizeof(*new_data));

  window->sview_offset = sview_offset;
  window->sview_len = sview_len;
  window->tview_len = tview_len;

  insend = data + inslen;

  if (version == 1)
    {
      svn_stringbuf_t *instout = svn_stringbuf_create_empty(pool);
      svn_stringbuf_t *ndout = svn_stringbuf_create_empty(pool);

      /* these may in fact simply return references to insend */

      SVN_ERR(zlib_decode(insend, newlen, ndout,
                          SVN_DELTA_WINDOW_SIZE));
      SVN_ERR(zlib_decode(data, insend - data, instout,
                          MAX_INSTRUCTION_SECTION_LEN));

      newlen = ndout->len;
      data = (unsigned char *)instout->data;
      insend = (unsigned char *)instout->data + instout->len;

      new_data->data = (const char *) ndout->data;
      new_data->len = newlen;
    }
  else
    {
      new_data->data = (const char *) insend;
      new_data->len = newlen;
    }

  /* Count the instructions and make sure they are all valid.  */
  SVN_ERR(count_and_verify_instructions(&ninst, data, insend,
                                        sview_len, tview_len, newlen));

  /* Allocate a buffer for the instructions and decode them. */
  ops = apr_palloc(pool, ninst * sizeof(*ops));
  npos = 0;
  window->src_ops = 0;
  for (op = ops; op < ops + ninst; op++)
    {
      data = decode_instruction(op, data, insend);
      if (op->action_code == svn_txdelta_source)
        ++window->src_ops;
      else if (op->action_code == svn_txdelta_new)
        {
          op->offset = npos;
          npos += op->length;
        }
    }
  SVN_ERR_ASSERT(data == insend);

  window->ops = ops;
  window->num_ops = ninst;
  window->new_data = new_data;

  return SVN_NO_ERROR;
}

static svn_error_t *
write_handler(void *baton,
              const char *buffer,
              apr_size_t *len)
{
  struct decode_baton *db = (struct decode_baton *) baton;
  const unsigned char *p, *end;
  svn_filesize_t sview_offset;
  apr_size_t sview_len, tview_len, inslen, newlen, remaining;
  apr_size_t buflen = *len;

  /* Chew up four bytes at the beginning for the header.  */
  if (db->header_bytes < 4)
    {
      apr_size_t nheader = 4 - db->header_bytes;
      if (nheader > buflen)
        nheader = buflen;
      if (memcmp(buffer, "SVN\0" + db->header_bytes, nheader) == 0)
        db->version = 0;
      else if (memcmp(buffer, "SVN\1" + db->header_bytes, nheader) == 0)
        db->version = 1;
      else
        return svn_error_create(SVN_ERR_SVNDIFF_INVALID_HEADER, NULL,
                                _("Svndiff has invalid header"));
      buflen -= nheader;
      buffer += nheader;
      db->header_bytes += nheader;
    }

  /* Concatenate the old with the new.  */
  svn_stringbuf_appendbytes(db->buffer, buffer, buflen);

  /* We have a buffer of svndiff data that might be good for:

     a) an integral number of windows' worth of data - this is a
        trivial case.  Make windows from our data and ship them off.

     b) a non-integral number of windows' worth of data - we shall
        consume the integral portion of the window data, and then
        somewhere in the following loop the decoding of the svndiff
        data will run out of stuff to decode, and will simply return
        SVN_NO_ERROR, anxiously awaiting more data.
  */

  while (1)
    {
      apr_pool_t *newpool;
      svn_txdelta_window_t window;

      /* Read the header, if we have enough bytes for that.  */
      p = (const unsigned char *) db->buffer->data;
      end = (const unsigned char *) db->buffer->data + db->buffer->len;

      p = decode_file_offset(&sview_offset, p, end);
      if (p == NULL)
        return SVN_NO_ERROR;

      p = decode_size(&sview_len, p, end);
      if (p == NULL)
        return SVN_NO_ERROR;

      p = decode_size(&tview_len, p, end);
      if (p == NULL)
        return SVN_NO_ERROR;

      p = decode_size(&inslen, p, end);
      if (p == NULL)
        return SVN_NO_ERROR;

      p = decode_size(&newlen, p, end);
      if (p == NULL)
        return SVN_NO_ERROR;

      if (tview_len > SVN_DELTA_WINDOW_SIZE ||
          sview_len > SVN_DELTA_WINDOW_SIZE ||
          /* for svndiff1, newlen includes the original length */
          newlen > SVN_DELTA_WINDOW_SIZE + MAX_ENCODED_INT_LEN ||
          inslen > MAX_INSTRUCTION_SECTION_LEN)
        return svn_error_create(SVN_ERR_SVNDIFF_CORRUPT_WINDOW, NULL,
                                _("Svndiff contains a too-large window"));

      /* Check for integer overflow.  */
      if (sview_offset < 0 || inslen + newlen < inslen
          || sview_len + tview_len < sview_len
          || (apr_size_t)sview_offset + sview_len < (apr_size_t)sview_offset)
        return svn_error_create(SVN_ERR_SVNDIFF_CORRUPT_WINDOW, NULL,
                                _("Svndiff contains corrupt window header"));

      /* Check for source windows which slide backwards.  */
      if (sview_len > 0
          && (sview_offset < db->last_sview_offset
              || (sview_offset + sview_len
                  < db->last_sview_offset + db->last_sview_len)))
        return svn_error_create
          (SVN_ERR_SVNDIFF_BACKWARD_VIEW, NULL,
           _("Svndiff has backwards-sliding source views"));

      /* Wait for more data if we don't have enough bytes for the
         whole window.  */
      if ((apr_size_t) (end - p) < inslen + newlen)
        return SVN_NO_ERROR;

      /* Decode the window and send it off. */
      SVN_ERR(decode_window(&window, sview_offset, sview_len, tview_len,
                            inslen, newlen, p, db->subpool,
                            db->version));
      SVN_ERR(db->consumer_func(&window, db->consumer_baton));

      /* Make a new subpool and buffer, saving aside the remaining
         data in the old buffer.  */
      newpool = svn_pool_create(db->pool);
      p += inslen + newlen;
      remaining = db->buffer->data + db->buffer->len - (const char *) p;
      db->buffer =
        svn_stringbuf_ncreate((const char *) p, remaining, newpool);

      /* Remember the offset and length of the source view for next time.  */
      db->last_sview_offset = sview_offset;
      db->last_sview_len = sview_len;

      /* We've copied stuff out of the old pool. Toss that pool and use
         our new pool.
         ### might be nice to avoid the copy and just use svn_pool_clear
         ### to get rid of whatever the "other stuff" is. future project...
      */
      svn_pool_destroy(db->subpool);
      db->subpool = newpool;
    }

  /* NOTREACHED */
}

/* Minimal svn_stream_t write handler, doing nothing */
static svn_error_t *
noop_write_handler(void *baton,
                   const char *buffer,
                   apr_size_t *len)
{
  return SVN_NO_ERROR;
}

static svn_error_t *
close_handler(void *baton)
{
  struct decode_baton *db = (struct decode_baton *) baton;
  svn_error_t *err;

  /* Make sure that we're at a plausible end of stream, returning an
     error if we are expected to do so.  */
  if ((db->error_on_early_close)
      && (db->header_bytes < 4 || db->buffer->len != 0))
    return svn_error_create(SVN_ERR_SVNDIFF_UNEXPECTED_END, NULL,
                            _("Unexpected end of svndiff input"));

  /* Tell the window consumer that we're done, and clean up.  */
  err = db->consumer_func(NULL, db->consumer_baton);
  svn_pool_destroy(db->pool);
  return err;
}


svn_stream_t *
svn_txdelta_parse_svndiff(svn_txdelta_window_handler_t handler,
                          void *handler_baton,
                          svn_boolean_t error_on_early_close,
                          apr_pool_t *pool)
{
  apr_pool_t *subpool = svn_pool_create(pool);
  struct decode_baton *db = apr_palloc(pool, sizeof(*db));
  svn_stream_t *stream;

  db->consumer_func = handler;
  db->consumer_baton = handler_baton;
  db->pool = subpool;
  db->subpool = svn_pool_create(subpool);
  db->buffer = svn_stringbuf_create_empty(db->subpool);
  db->last_sview_offset = 0;
  db->last_sview_len = 0;
  db->header_bytes = 0;
  db->error_on_early_close = error_on_early_close;
  stream = svn_stream_create(db, pool);

  if (handler != svn_delta_noop_window_handler)
    {
      svn_stream_set_write(stream, write_handler);
      svn_stream_set_close(stream, close_handler);
    }
  else
    {
      /* And else we just ignore everything as efficiently as we can.
         by only hooking a no-op handler */
      svn_stream_set_write(stream, noop_write_handler);
    }
  return stream;
}


/* Routines for reading one svndiff window at a time. */

/* Read one byte from STREAM into *BYTE. */
static svn_error_t *
read_one_byte(unsigned char *byte, svn_stream_t *stream)
{
  char c;
  apr_size_t len = 1;

  SVN_ERR(svn_stream_read(stream, &c, &len));
  if (len == 0)
    return svn_error_create(SVN_ERR_SVNDIFF_UNEXPECTED_END, NULL,
                            _("Unexpected end of svndiff input"));
  *byte = (unsigned char) c;
  return SVN_NO_ERROR;
}

/* Read and decode one integer from STREAM into *SIZE. */
static svn_error_t *
read_one_size(apr_size_t *size, svn_stream_t *stream)
{
  unsigned char c;

  *size = 0;
  while (1)
    {
      SVN_ERR(read_one_byte(&c, stream));
      *size = (*size << 7) | (c & 0x7f);
      if (!(c & 0x80))
        break;
    }
  return SVN_NO_ERROR;
}

/* Read a window header from STREAM and check it for integer overflow. */
static svn_error_t *
read_window_header(svn_stream_t *stream, svn_filesize_t *sview_offset,
                   apr_size_t *sview_len, apr_size_t *tview_len,
                   apr_size_t *inslen, apr_size_t *newlen)
{
  unsigned char c;

  /* Read the source view offset by hand, since it's not an apr_size_t. */
  *sview_offset = 0;
  while (1)
    {
      SVN_ERR(read_one_byte(&c, stream));
      *sview_offset = (*sview_offset << 7) | (c & 0x7f);
      if (!(c & 0x80))
        break;
    }

  /* Read the four size fields. */
  SVN_ERR(read_one_size(sview_len, stream));
  SVN_ERR(read_one_size(tview_len, stream));
  SVN_ERR(read_one_size(inslen, stream));
  SVN_ERR(read_one_size(newlen, stream));

  if (*tview_len > SVN_DELTA_WINDOW_SIZE ||
      *sview_len > SVN_DELTA_WINDOW_SIZE ||
      /* for svndiff1, newlen includes the original length */
      *newlen > SVN_DELTA_WINDOW_SIZE + MAX_ENCODED_INT_LEN ||
      *inslen > MAX_INSTRUCTION_SECTION_LEN)
    return svn_error_create(SVN_ERR_SVNDIFF_CORRUPT_WINDOW, NULL,
                            _("Svndiff contains a too-large window"));

  /* Check for integer overflow.  */
  if (*sview_offset < 0 || *inslen + *newlen < *inslen
      || *sview_len + *tview_len < *sview_len
      || (apr_size_t)*sview_offset + *sview_len < (apr_size_t)*sview_offset)
    return svn_error_create(SVN_ERR_SVNDIFF_CORRUPT_WINDOW, NULL,
                            _("Svndiff contains corrupt window header"));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_txdelta_read_svndiff_window(svn_txdelta_window_t **window,
                                svn_stream_t *stream,
                                int svndiff_version,
                                apr_pool_t *pool)
{
  svn_filesize_t sview_offset;
  apr_size_t sview_len, tview_len, inslen, newlen, len;
  unsigned char *buf;

  SVN_ERR(read_window_header(stream, &sview_offset, &sview_len, &tview_len,
                             &inslen, &newlen));
  len = inslen + newlen;
  buf = apr_palloc(pool, len);
  SVN_ERR(svn_stream_read(stream, (char*)buf, &len));
  if (len < inslen + newlen)
    return svn_error_create(SVN_ERR_SVNDIFF_UNEXPECTED_END, NULL,
                            _("Unexpected end of svndiff input"));
  *window = apr_palloc(pool, sizeof(**window));
  return decode_window(*window, sview_offset, sview_len, tview_len, inslen,
                       newlen, buf, pool, svndiff_version);
}


svn_error_t *
svn_txdelta_skip_svndiff_window(apr_file_t *file,
                                int svndiff_version,
                                apr_pool_t *pool)
{
  svn_stream_t *stream = svn_stream_from_aprfile2(file, TRUE, pool);
  svn_filesize_t sview_offset;
  apr_size_t sview_len, tview_len, inslen, newlen;
  apr_off_t offset;

  SVN_ERR(read_window_header(stream, &sview_offset, &sview_len, &tview_len,
                             &inslen, &newlen));

  offset = inslen + newlen;
  return svn_io_file_seek(file, APR_CUR, &offset, pool);
}


svn_error_t *
svn__compress(svn_string_t *in,
              svn_stringbuf_t *out,
              int compression_level)
{
  return zlib_encode(in->data, in->len, out, compression_level);
}

svn_error_t *
svn__decompress(svn_string_t *in,
                svn_stringbuf_t *out,
                apr_size_t limit)
{
  return zlib_decode((const unsigned char*)in->data, in->len, out, limit);
}
