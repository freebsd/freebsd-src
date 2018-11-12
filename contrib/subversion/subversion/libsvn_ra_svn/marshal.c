/*
 * marshal.c :  Marshalling routines for Subversion protocol
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
#include <stdlib.h>

#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <apr_general.h>
#include <apr_lib.h>
#include <apr_strings.h>

#include "svn_hash.h"
#include "svn_types.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "svn_ra_svn.h"
#include "svn_private_config.h"
#include "svn_ctype.h"
#include "svn_time.h"

#include "ra_svn.h"

#include "private/svn_string_private.h"
#include "private/svn_dep_compat.h"
#include "private/svn_error_private.h"

#define svn_iswhitespace(c) ((c) == ' ' || (c) == '\n')

/* If we receive data that *claims* to be followed by a very long string,
 * we should not trust that claim right away. But everything up to 1 MB
 * should be too small to be instrumental for a DOS attack. */

#define SUSPICIOUSLY_HUGE_STRING_SIZE_THRESHOLD (0x100000)

/* Return the APR socket timeout to be used for the connection depending
 * on whether there is a blockage handler or zero copy has been activated. */
static apr_interval_time_t
get_timeout(svn_ra_svn_conn_t *conn)
{
  return conn->block_handler ? 0 : -1;
}

/* --- CONNECTION INITIALIZATION --- */

svn_ra_svn_conn_t *svn_ra_svn_create_conn3(apr_socket_t *sock,
                                           apr_file_t *in_file,
                                           apr_file_t *out_file,
                                           int compression_level,
                                           apr_size_t zero_copy_limit,
                                           apr_size_t error_check_interval,
                                           apr_pool_t *pool)
{
  svn_ra_svn_conn_t *conn;
  void *mem = apr_palloc(pool, sizeof(*conn) + SVN_RA_SVN__PAGE_SIZE);
  conn = (void*)APR_ALIGN((apr_uintptr_t)mem, SVN_RA_SVN__PAGE_SIZE);

  assert((sock && !in_file && !out_file) || (!sock && in_file && out_file));
#ifdef SVN_HAVE_SASL
  conn->sock = sock;
  conn->encrypted = FALSE;
#endif
  conn->session = NULL;
  conn->read_ptr = conn->read_buf;
  conn->read_end = conn->read_buf;
  conn->write_pos = 0;
  conn->written_since_error_check = 0;
  conn->error_check_interval = error_check_interval;
  conn->may_check_for_error = error_check_interval == 0;
  conn->block_handler = NULL;
  conn->block_baton = NULL;
  conn->capabilities = apr_hash_make(pool);
  conn->compression_level = compression_level;
  conn->zero_copy_limit = zero_copy_limit;
  conn->pool = pool;

  if (sock != NULL)
    {
      apr_sockaddr_t *sa;
      conn->stream = svn_ra_svn__stream_from_sock(sock, pool);
      if (!(apr_socket_addr_get(&sa, APR_REMOTE, sock) == APR_SUCCESS
            && apr_sockaddr_ip_get(&conn->remote_ip, sa) == APR_SUCCESS))
        conn->remote_ip = NULL;
      svn_ra_svn__stream_timeout(conn->stream, get_timeout(conn));
    }
  else
    {
      conn->stream = svn_ra_svn__stream_from_files(in_file, out_file, pool);
      conn->remote_ip = NULL;
    }

  return conn;
}

svn_ra_svn_conn_t *svn_ra_svn_create_conn2(apr_socket_t *sock,
                                           apr_file_t *in_file,
                                           apr_file_t *out_file,
                                           int compression_level,
                                           apr_pool_t *pool)
{
  return svn_ra_svn_create_conn3(sock, in_file, out_file,
                                 compression_level, 0, 0, pool);
}

/* backward-compatible implementation using the default compression level */
svn_ra_svn_conn_t *svn_ra_svn_create_conn(apr_socket_t *sock,
                                          apr_file_t *in_file,
                                          apr_file_t *out_file,
                                          apr_pool_t *pool)
{
  return svn_ra_svn_create_conn3(sock, in_file, out_file,
                                 SVN_DELTA_COMPRESSION_LEVEL_DEFAULT, 0, 0,
                                 pool);
}

svn_error_t *svn_ra_svn_set_capabilities(svn_ra_svn_conn_t *conn,
                                         const apr_array_header_t *list)
{
  int i;
  svn_ra_svn_item_t *item;
  const char *word;

  for (i = 0; i < list->nelts; i++)
    {
      item = &APR_ARRAY_IDX(list, i, svn_ra_svn_item_t);
      if (item->kind != SVN_RA_SVN_WORD)
        return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                _("Capability entry is not a word"));
      word = apr_pstrdup(conn->pool, item->u.word);
      svn_hash_sets(conn->capabilities, word, word);
    }
  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__set_shim_callbacks(svn_ra_svn_conn_t *conn,
                               svn_delta_shim_callbacks_t *shim_callbacks)
{
  conn->shim_callbacks = shim_callbacks;
  return SVN_NO_ERROR;
}

svn_boolean_t svn_ra_svn_has_capability(svn_ra_svn_conn_t *conn,
                                        const char *capability)
{
  return (svn_hash_gets(conn->capabilities, capability) != NULL);
}

int
svn_ra_svn_compression_level(svn_ra_svn_conn_t *conn)
{
  return conn->compression_level;
}

apr_size_t
svn_ra_svn_zero_copy_limit(svn_ra_svn_conn_t *conn)
{
  return conn->zero_copy_limit;
}

const char *svn_ra_svn_conn_remote_host(svn_ra_svn_conn_t *conn)
{
  return conn->remote_ip;
}

void
svn_ra_svn__set_block_handler(svn_ra_svn_conn_t *conn,
                              ra_svn_block_handler_t handler,
                              void *baton)
{
  conn->block_handler = handler;
  conn->block_baton = baton;
  svn_ra_svn__stream_timeout(conn->stream, get_timeout(conn));
}

svn_boolean_t svn_ra_svn__input_waiting(svn_ra_svn_conn_t *conn,
                                        apr_pool_t *pool)
{
  return svn_ra_svn__stream_pending(conn->stream);
}

/* --- WRITE BUFFER MANAGEMENT --- */

/* Write data to socket or output file as appropriate. */
static svn_error_t *writebuf_output(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                    const char *data, apr_size_t len)
{
  const char *end = data + len;
  apr_size_t count;
  apr_pool_t *subpool = NULL;
  svn_ra_svn__session_baton_t *session = conn->session;

  while (data < end)
    {
      count = end - data;

      if (session && session->callbacks && session->callbacks->cancel_func)
        SVN_ERR((session->callbacks->cancel_func)(session->callbacks_baton));

      SVN_ERR(svn_ra_svn__stream_write(conn->stream, data, &count));
      if (count == 0)
        {
          if (!subpool)
            subpool = svn_pool_create(pool);
          else
            svn_pool_clear(subpool);
          SVN_ERR(conn->block_handler(conn, subpool, conn->block_baton));
        }
      data += count;

      if (session)
        {
          const svn_ra_callbacks2_t *cb = session->callbacks;
          session->bytes_written += count;

          if (cb && cb->progress_func)
            (cb->progress_func)(session->bytes_written + session->bytes_read,
                                -1, cb->progress_baton, subpool);
        }
    }

  conn->written_since_error_check += len;
  conn->may_check_for_error
    = conn->written_since_error_check >= conn->error_check_interval;

  if (subpool)
    svn_pool_destroy(subpool);
  return SVN_NO_ERROR;
}

/* Write data from the write buffer out to the socket. */
static svn_error_t *writebuf_flush(svn_ra_svn_conn_t *conn, apr_pool_t *pool)
{
  apr_size_t write_pos = conn->write_pos;

  /* Clear conn->write_pos first in case the block handler does a read. */
  conn->write_pos = 0;
  SVN_ERR(writebuf_output(conn, pool, conn->write_buf, write_pos));
  return SVN_NO_ERROR;
}

static svn_error_t *writebuf_write(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                   const char *data, apr_size_t len)
{
  /* data >= 8k is sent immediately */
  if (len >= sizeof(conn->write_buf) / 2)
    {
      if (conn->write_pos > 0)
        SVN_ERR(writebuf_flush(conn, pool));

      return writebuf_output(conn, pool, data, len);
    }

  /* ensure room for the data to add */
  if (conn->write_pos + len > sizeof(conn->write_buf))
    SVN_ERR(writebuf_flush(conn, pool));

  /* buffer the new data block as well */
  memcpy(conn->write_buf + conn->write_pos, data, len);
  conn->write_pos += len;

  return SVN_NO_ERROR;
}

static svn_error_t *
writebuf_write_short_string(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                            const char *data, apr_size_t len)
{
  apr_size_t left = sizeof(conn->write_buf) - conn->write_pos;
  if (len <= left)
  {
    memcpy(conn->write_buf + conn->write_pos, data, len);
    conn->write_pos += len;
    return SVN_NO_ERROR;
  }
  else
    return writebuf_write(conn, pool, data, len);
}

static APR_INLINE svn_error_t *
writebuf_writechar(svn_ra_svn_conn_t *conn, apr_pool_t *pool, char data)
{
  if (conn->write_pos < sizeof(conn->write_buf))
  {
    conn->write_buf[conn->write_pos] = data;
    conn->write_pos++;

    return SVN_NO_ERROR;
  }
  else
  {
    char temp = data;
    return writebuf_write(conn, pool, &temp, 1);
  }
}

/* --- READ BUFFER MANAGEMENT --- */

/* Read bytes into DATA until either the read buffer is empty or
 * we reach END. */
static char *readbuf_drain(svn_ra_svn_conn_t *conn, char *data, char *end)
{
  apr_ssize_t buflen, copylen;

  buflen = conn->read_end - conn->read_ptr;
  copylen = (buflen < end - data) ? buflen : end - data;
  memcpy(data, conn->read_ptr, copylen);
  conn->read_ptr += copylen;
  return data + copylen;
}

/* Read data from socket or input file as appropriate. */
static svn_error_t *readbuf_input(svn_ra_svn_conn_t *conn, char *data,
                                  apr_size_t *len, apr_pool_t *pool)
{
  svn_ra_svn__session_baton_t *session = conn->session;

  if (session && session->callbacks && session->callbacks->cancel_func)
    SVN_ERR((session->callbacks->cancel_func)(session->callbacks_baton));

  SVN_ERR(svn_ra_svn__stream_read(conn->stream, data, len));
  if (*len == 0)
    return svn_error_create(SVN_ERR_RA_SVN_CONNECTION_CLOSED, NULL, NULL);

  if (session)
    {
      const svn_ra_callbacks2_t *cb = session->callbacks;
      session->bytes_read += *len;

      if (cb && cb->progress_func)
        (cb->progress_func)(session->bytes_read + session->bytes_written,
                            -1, cb->progress_baton, pool);
    }

  return SVN_NO_ERROR;
}

/* Treat the next LEN input bytes from CONN as "read" */
static svn_error_t *readbuf_skip(svn_ra_svn_conn_t *conn, apr_uint64_t len)
{
  do
  {
    apr_size_t buflen = conn->read_end - conn->read_ptr;
    apr_size_t copylen = (buflen < len) ? buflen : (apr_size_t)len;
    conn->read_ptr += copylen;
    len -= copylen;
    if (len == 0)
      break;

    buflen = sizeof(conn->read_buf);
    SVN_ERR(svn_ra_svn__stream_read(conn->stream, conn->read_buf, &buflen));
    if (buflen == 0)
      return svn_error_create(SVN_ERR_RA_SVN_CONNECTION_CLOSED, NULL, NULL);

    conn->read_end = conn->read_buf + buflen;
    conn->read_ptr = conn->read_buf;
  }
  while (len > 0);

  return SVN_NO_ERROR;
}

/* Read data from the socket into the read buffer, which must be empty. */
static svn_error_t *readbuf_fill(svn_ra_svn_conn_t *conn, apr_pool_t *pool)
{
  apr_size_t len;

  SVN_ERR_ASSERT(conn->read_ptr == conn->read_end);
  SVN_ERR(writebuf_flush(conn, pool));
  len = sizeof(conn->read_buf);
  SVN_ERR(readbuf_input(conn, conn->read_buf, &len, pool));
  conn->read_ptr = conn->read_buf;
  conn->read_end = conn->read_buf + len;
  return SVN_NO_ERROR;
}

static APR_INLINE svn_error_t *
readbuf_getchar(svn_ra_svn_conn_t *conn, apr_pool_t *pool, char *result)
{
  if (conn->read_ptr == conn->read_end)
    SVN_ERR(readbuf_fill(conn, pool));
  *result = *conn->read_ptr++;
  return SVN_NO_ERROR;
}

static svn_error_t *readbuf_getchar_skip_whitespace(svn_ra_svn_conn_t *conn,
                                                    apr_pool_t *pool,
                                                    char *result)
{
  do
    SVN_ERR(readbuf_getchar(conn, pool, result));
  while (svn_iswhitespace(*result));
  return SVN_NO_ERROR;
}

/* Read the next LEN bytes from CONN and copy them to *DATA. */
static svn_error_t *readbuf_read(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                 char *data, apr_size_t len)
{
  char *end = data + len;
  apr_size_t count;

  /* Copy in an appropriate amount of data from the buffer. */
  data = readbuf_drain(conn, data, end);

  /* Read large chunks directly into buffer. */
  while (end - data > (apr_ssize_t)sizeof(conn->read_buf))
    {
      SVN_ERR(writebuf_flush(conn, pool));
      count = end - data;
      SVN_ERR(readbuf_input(conn, data, &count, pool));
      data += count;
    }

  while (end > data)
    {
      /* The remaining amount to read is small; fill the buffer and
       * copy from that. */
      SVN_ERR(readbuf_fill(conn, pool));
      data = readbuf_drain(conn, data, end);
    }

  return SVN_NO_ERROR;
}

static svn_error_t *readbuf_skip_leading_garbage(svn_ra_svn_conn_t *conn,
                                                 apr_pool_t *pool)
{
  char buf[256];  /* Must be smaller than sizeof(conn->read_buf) - 1. */
  const char *p, *end;
  apr_size_t len;
  svn_boolean_t lparen = FALSE;

  SVN_ERR_ASSERT(conn->read_ptr == conn->read_end);
  while (1)
    {
      /* Read some data directly from the connection input source. */
      len = sizeof(buf);
      SVN_ERR(readbuf_input(conn, buf, &len, pool));
      end = buf + len;

      /* Scan the data for '(' WS with a very simple state machine. */
      for (p = buf; p < end; p++)
        {
          if (lparen && svn_iswhitespace(*p))
            break;
          else
            lparen = (*p == '(');
        }
      if (p < end)
        break;
    }

  /* p now points to the whitespace just after the left paren.  Fake
   * up the left paren and then copy what we have into the read
   * buffer. */
  conn->read_buf[0] = '(';
  memcpy(conn->read_buf + 1, p, end - p);
  conn->read_ptr = conn->read_buf;
  conn->read_end = conn->read_buf + 1 + (end - p);
  return SVN_NO_ERROR;
}

/* --- WRITING DATA ITEMS --- */

static svn_error_t *write_number(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                 apr_uint64_t number, char follow)
{
  apr_size_t written;

  /* SVN_INT64_BUFFER_SIZE includes space for a terminating NUL that
   * svn__ui64toa will always append. */
  if (conn->write_pos + SVN_INT64_BUFFER_SIZE >= sizeof(conn->write_buf))
    SVN_ERR(writebuf_flush(conn, pool));

  written = svn__ui64toa(conn->write_buf + conn->write_pos, number);
  conn->write_buf[conn->write_pos + written] = follow;
  conn->write_pos += written + 1;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_number(svn_ra_svn_conn_t *conn,
                         apr_pool_t *pool,
                         apr_uint64_t number)
{
  return write_number(conn, pool, number, ' ');
}

svn_error_t *
svn_ra_svn__write_string(svn_ra_svn_conn_t *conn,
                         apr_pool_t *pool,
                         const svn_string_t *str)
{
  if (str->len < 10)
    {
      SVN_ERR(writebuf_writechar(conn, pool, (char)(str->len + '0')));
      SVN_ERR(writebuf_writechar(conn, pool, ':'));
    }
  else
    SVN_ERR(write_number(conn, pool, str->len, ':'));

  SVN_ERR(writebuf_write(conn, pool, str->data, str->len));
  SVN_ERR(writebuf_writechar(conn, pool, ' '));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cstring(svn_ra_svn_conn_t *conn,
                          apr_pool_t *pool,
                          const char *s)
{
  apr_size_t len = strlen(s);

  if (len < 10)
    {
      SVN_ERR(writebuf_writechar(conn, pool, (char)(len + '0')));
      SVN_ERR(writebuf_writechar(conn, pool, ':'));
    }
  else
    SVN_ERR(write_number(conn, pool, len, ':'));

  SVN_ERR(writebuf_write(conn, pool, s, len));
  SVN_ERR(writebuf_writechar(conn, pool, ' '));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_word(svn_ra_svn_conn_t *conn,
                       apr_pool_t *pool,
                       const char *word)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, word, strlen(word)));
  SVN_ERR(writebuf_writechar(conn, pool, ' '));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_proplist(svn_ra_svn_conn_t *conn,
                           apr_pool_t *pool,
                           apr_hash_t *props)
{
  apr_pool_t *iterpool;
  apr_hash_index_t *hi;
  const void *key;
  void *val;
  const char *propname;
  svn_string_t *propval;

  if (props)
    {
      iterpool = svn_pool_create(pool);
      for (hi = apr_hash_first(pool, props); hi; hi = apr_hash_next(hi))
        {
          svn_pool_clear(iterpool);
          apr_hash_this(hi, &key, NULL, &val);
          propname = key;
          propval = val;
          SVN_ERR(svn_ra_svn__write_tuple(conn, iterpool, "cs",
                                          propname, propval));
        }
      svn_pool_destroy(iterpool);
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__start_list(svn_ra_svn_conn_t *conn,
                       apr_pool_t *pool)
{
  if (conn->write_pos + 2 <= sizeof(conn->write_buf))
    {
      conn->write_buf[conn->write_pos] = '(';
      conn->write_buf[conn->write_pos+1] = ' ';
      conn->write_pos += 2;
      return SVN_NO_ERROR;
    }

  return writebuf_write(conn, pool, "( ", 2);
}

svn_error_t *
svn_ra_svn__end_list(svn_ra_svn_conn_t *conn,
                     apr_pool_t *pool)
{
  if (conn->write_pos + 2 <= sizeof(conn->write_buf))
  {
    conn->write_buf[conn->write_pos] = ')';
    conn->write_buf[conn->write_pos+1] = ' ';
    conn->write_pos += 2;
    return SVN_NO_ERROR;
  }

  return writebuf_write(conn, pool, ") ", 2);
}

svn_error_t *
svn_ra_svn__flush(svn_ra_svn_conn_t *conn,
                  apr_pool_t *pool)
{
  SVN_ERR(writebuf_flush(conn, pool));
  conn->may_check_for_error = TRUE;

  return SVN_NO_ERROR;
}

/* --- WRITING TUPLES --- */

static svn_error_t *
vwrite_tuple_cstring(svn_ra_svn_conn_t *conn, apr_pool_t *pool, va_list *ap)
{
  const char *cstr = va_arg(*ap, const char *);
  SVN_ERR_ASSERT(cstr);
  return svn_ra_svn__write_cstring(conn, pool, cstr);
}

static svn_error_t *
vwrite_tuple_cstring_opt(svn_ra_svn_conn_t *conn, apr_pool_t *pool, va_list *ap)
{
  const char *cstr = va_arg(*ap, const char *);
  return cstr ? svn_ra_svn__write_cstring(conn, pool, cstr) : SVN_NO_ERROR;
}

static svn_error_t *
vwrite_tuple_string(svn_ra_svn_conn_t *conn, apr_pool_t *pool, va_list *ap)
{
  const svn_string_t *str = va_arg(*ap, const svn_string_t *);
  SVN_ERR_ASSERT(str);
  return svn_ra_svn__write_string(conn, pool, str);
}

static svn_error_t *
vwrite_tuple_string_opt(svn_ra_svn_conn_t *conn, apr_pool_t *pool, va_list *ap)
{
  const svn_string_t *str = va_arg(*ap, const svn_string_t *);
  return str ? svn_ra_svn__write_string(conn, pool, str) : SVN_NO_ERROR;
}

static svn_error_t *
vwrite_tuple_word(svn_ra_svn_conn_t *conn, apr_pool_t *pool, va_list *ap)
{
  const char *cstr = va_arg(*ap, const char *);
  SVN_ERR_ASSERT(cstr);
  return svn_ra_svn__write_word(conn, pool, cstr);
}

static svn_error_t *
vwrite_tuple_word_opt(svn_ra_svn_conn_t *conn, apr_pool_t *pool, va_list *ap)
{
  const char *cstr = va_arg(*ap, const char *);
  return cstr ? svn_ra_svn__write_word(conn, pool, cstr) : SVN_NO_ERROR;
}

static svn_error_t *
vwrite_tuple_revision(svn_ra_svn_conn_t *conn, apr_pool_t *pool, va_list *ap)
{
  svn_revnum_t rev = va_arg(*ap, svn_revnum_t);
  SVN_ERR_ASSERT(SVN_IS_VALID_REVNUM(rev));
  return svn_ra_svn__write_number(conn, pool, rev);
}

static svn_error_t *
vwrite_tuple_revision_opt(svn_ra_svn_conn_t *conn, apr_pool_t *pool, va_list *ap)
{
  svn_revnum_t rev = va_arg(*ap, svn_revnum_t);
  return SVN_IS_VALID_REVNUM(rev)
       ? svn_ra_svn__write_number(conn, pool, rev)
       : SVN_NO_ERROR;
}

static svn_error_t *
vwrite_tuple_number(svn_ra_svn_conn_t *conn, apr_pool_t *pool, va_list *ap)
{
  return svn_ra_svn__write_number(conn, pool, va_arg(*ap, apr_uint64_t));
}

static svn_error_t *
vwrite_tuple_boolean(svn_ra_svn_conn_t *conn, apr_pool_t *pool, va_list *ap)
{
  const char *cstr = va_arg(*ap, svn_boolean_t) ? "true" : "false";
  return svn_ra_svn__write_word(conn, pool, cstr);
}

static svn_error_t *
write_tuple_cstring(svn_ra_svn_conn_t *conn,
                    apr_pool_t *pool,
                    const char *cstr)
{
  SVN_ERR_ASSERT(cstr);
  return svn_ra_svn__write_cstring(conn, pool, cstr);
}

static svn_error_t *
write_tuple_cstring_opt(svn_ra_svn_conn_t *conn,
                        apr_pool_t *pool,
                        const char *cstr)
{
  return cstr ? svn_ra_svn__write_cstring(conn, pool, cstr) : SVN_NO_ERROR;
}

static svn_error_t *
write_tuple_string(svn_ra_svn_conn_t *conn,
                   apr_pool_t *pool,
                   const svn_string_t *str)
{
  SVN_ERR_ASSERT(str);
  return svn_ra_svn__write_string(conn, pool, str);
}

static svn_error_t *
write_tuple_string_opt(svn_ra_svn_conn_t *conn,
                       apr_pool_t *pool,
                       const svn_string_t *str)
{
  return str ? svn_ra_svn__write_string(conn, pool, str) : SVN_NO_ERROR;
}

static svn_error_t *
write_tuple_start_list(svn_ra_svn_conn_t *conn,
                       apr_pool_t *pool)
{
  return svn_ra_svn__start_list(conn, pool);
}

static svn_error_t *
write_tuple_end_list(svn_ra_svn_conn_t *conn,
                     apr_pool_t *pool)
{
  return svn_ra_svn__end_list(conn, pool);
}

static svn_error_t *
write_tuple_revision(svn_ra_svn_conn_t *conn,
                     apr_pool_t *pool,
                     svn_revnum_t rev)
{
  SVN_ERR_ASSERT(SVN_IS_VALID_REVNUM(rev));
  return svn_ra_svn__write_number(conn, pool, rev);
}

static svn_error_t *
write_tuple_revision_opt(svn_ra_svn_conn_t *conn,
                         apr_pool_t *pool,
                         svn_revnum_t rev)
{
  return SVN_IS_VALID_REVNUM(rev)
       ? svn_ra_svn__write_number(conn, pool, rev)
       : SVN_NO_ERROR;
}

static svn_error_t *
write_tuple_boolean(svn_ra_svn_conn_t *conn,
                    apr_pool_t *pool,
                    svn_boolean_t value)
{
  const char *cstr = value ? "true" : "false";
  return svn_ra_svn__write_word(conn, pool, cstr);
}

static svn_error_t *
write_tuple_depth(svn_ra_svn_conn_t *conn,
                  apr_pool_t *pool,
                  svn_depth_t depth)
{
  return svn_ra_svn__write_word(conn, pool, svn_depth_to_word(depth));
}


static svn_error_t *
write_cmd_add_node(svn_ra_svn_conn_t *conn,
                   apr_pool_t *pool,
                   const char *path,
                   const char *parent_token,
                   const char *token,
                   const char *copy_path,
                   svn_revnum_t copy_rev)
{
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(write_tuple_cstring(conn, pool, parent_token));
  SVN_ERR(write_tuple_cstring(conn, pool, token));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_cstring_opt(conn, pool, copy_path));
  SVN_ERR(write_tuple_revision_opt(conn, pool, copy_rev));
  SVN_ERR(write_tuple_end_list(conn, pool));

  return SVN_NO_ERROR;
}

static svn_error_t *
write_cmd_open_node(svn_ra_svn_conn_t *conn,
                    apr_pool_t *pool,
                    const char *path,
                    const char *parent_token,
                    const char *token,
                    svn_revnum_t rev)
{
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(write_tuple_cstring(conn, pool, parent_token));
  SVN_ERR(write_tuple_cstring(conn, pool, token));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_revision_opt(conn, pool, rev));
  SVN_ERR(write_tuple_end_list(conn, pool));

  return SVN_NO_ERROR;
}

static svn_error_t *
write_cmd_change_node_prop(svn_ra_svn_conn_t *conn,
                           apr_pool_t *pool,
                           const char *token,
                           const char *name,
                           const svn_string_t *value)
{
  SVN_ERR(write_tuple_cstring(conn, pool, token));
  SVN_ERR(write_tuple_cstring(conn, pool, name));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_string_opt(conn, pool, value));
  SVN_ERR(write_tuple_end_list(conn, pool));

  return SVN_NO_ERROR;
}

static svn_error_t *
write_cmd_absent_node(svn_ra_svn_conn_t *conn,
                      apr_pool_t *pool,
                      const char *path,
                      const char *token)
{
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(write_tuple_cstring(conn, pool, token));

  return SVN_NO_ERROR;
}




static svn_error_t *vwrite_tuple(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                 const char *fmt, va_list *ap)
{
  svn_boolean_t opt = FALSE;

  if (*fmt == '!')
    fmt++;
  else
    SVN_ERR(svn_ra_svn__start_list(conn, pool));
  for (; *fmt; fmt++)
    {
      if (*fmt == 'c')
        SVN_ERR(opt ? vwrite_tuple_cstring_opt(conn, pool, ap)
                    : vwrite_tuple_cstring(conn, pool, ap));
      else if (*fmt == 's')
        SVN_ERR(opt ? vwrite_tuple_string_opt(conn, pool, ap)
                    : vwrite_tuple_string(conn, pool, ap));
      else if (*fmt == '(' && !opt)
        SVN_ERR(write_tuple_start_list(conn, pool));
      else if (*fmt == ')')
        {
          SVN_ERR(write_tuple_end_list(conn, pool));
          opt = FALSE;
        }
      else if (*fmt == '?')
        opt = TRUE;
      else if (*fmt == 'w')
        SVN_ERR(opt ? vwrite_tuple_word_opt(conn, pool, ap)
                    : vwrite_tuple_word(conn, pool, ap));
      else if (*fmt == 'r')
        SVN_ERR(opt ? vwrite_tuple_revision_opt(conn, pool, ap)
                    : vwrite_tuple_revision(conn, pool, ap));
      else if (*fmt == 'n' && !opt)
        SVN_ERR(vwrite_tuple_number(conn, pool, ap));
      else if (*fmt == 'b' && !opt)
        SVN_ERR(vwrite_tuple_boolean(conn, pool, ap));
      else if (*fmt == '!' && !*(fmt + 1))
        return SVN_NO_ERROR;
      else
        SVN_ERR_MALFUNCTION();
    }
  SVN_ERR(svn_ra_svn__end_list(conn, pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_tuple(svn_ra_svn_conn_t *conn,
                        apr_pool_t *pool,
                        const char *fmt, ...)
{
  svn_error_t *err;
  va_list ap;

  va_start(ap, fmt);
  err = vwrite_tuple(conn, pool, fmt, &ap);
  va_end(ap);
  return err;
}

/* --- READING DATA ITEMS --- */

/* Read LEN bytes from CONN into already-allocated structure ITEM.
 * Afterwards, *ITEM is of type 'SVN_RA_SVN_STRING', and its string
 * data is allocated in POOL. */
static svn_error_t *read_string(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                svn_ra_svn_item_t *item, apr_uint64_t len64)
{
  svn_stringbuf_t *stringbuf;
  apr_size_t len = (apr_size_t)len64;
  apr_size_t readbuf_len;
  char *dest;

  /* We can't store strings longer than the maximum size of apr_size_t,
   * so check for wrapping */
  if (len64 > APR_SIZE_MAX)
    return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                            _("String length larger than maximum"));

  /* Read the string in chunks.  The chunk size is large enough to avoid
   * re-allocation in typical cases, and small enough to ensure we do not
   * pre-allocate an unreasonable amount of memory if (perhaps due to
   * network data corruption or a DOS attack), we receive a bogus claim that
   * a very long string is going to follow.  In that case, we start small
   * and wait for all that data to actually show up.  This does not fully
   * prevent DOS attacks but makes them harder (you have to actually send
   * gigabytes of data). */
  readbuf_len = len < SUSPICIOUSLY_HUGE_STRING_SIZE_THRESHOLD
                    ? len
                    : SUSPICIOUSLY_HUGE_STRING_SIZE_THRESHOLD;
  stringbuf = svn_stringbuf_create_ensure(readbuf_len, pool);
  dest = stringbuf->data;

  /* Read remaining string data directly into the string structure.
   * Do it iteratively, if necessary.  */
  while (readbuf_len)
    {
      SVN_ERR(readbuf_read(conn, pool, dest, readbuf_len));

      stringbuf->len += readbuf_len;
      len -= readbuf_len;

      /* Early exit. In most cases, strings can be read in the first
       * iteration. */
      if (len == 0)
        break;

      /* Prepare next iteration: determine length of chunk to read
       * and re-alloc the string buffer. */
      readbuf_len
        = len < SUSPICIOUSLY_HUGE_STRING_SIZE_THRESHOLD
              ? len
              : SUSPICIOUSLY_HUGE_STRING_SIZE_THRESHOLD;

      svn_stringbuf_ensure(stringbuf, stringbuf->len + readbuf_len);
      dest = stringbuf->data + stringbuf->len;
    }

  /* zero-terminate the string */
  stringbuf->data[stringbuf->len] = '\0';

  /* Return the string properly wrapped into an RA_SVN item. */
  item->kind = SVN_RA_SVN_STRING;
  item->u.string = svn_stringbuf__morph_into_string(stringbuf);

  return SVN_NO_ERROR;
}

/* Given the first non-whitespace character FIRST_CHAR, read an item
 * into the already allocated structure ITEM.  LEVEL should be set
 * to 0 for the first call and is used to enforce a recurssion limit
 * on the parser. */
static svn_error_t *read_item(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                              svn_ra_svn_item_t *item, char first_char,
                              int level)
{
  char c = first_char;
  apr_uint64_t val;
  svn_stringbuf_t *str;
  svn_ra_svn_item_t *listitem;

  if (++level >= 64)
    return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                            _("Too many nested items"));


  /* Determine the item type and read it in.  Make sure that c is the
   * first character at the end of the item so we can test to make
   * sure it's whitespace. */
  if (svn_ctype_isdigit(c))
    {
      /* It's a number or a string.  Read the number part, either way. */
      val = c - '0';
      while (1)
        {
          apr_uint64_t prev_val = val;
          SVN_ERR(readbuf_getchar(conn, pool, &c));
          if (!svn_ctype_isdigit(c))
            break;
          val = val * 10 + (c - '0');
          /* val wrapped past maximum value? */
          if (prev_val >= (APR_UINT64_MAX / 10) && (val / 10) != prev_val)
            return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                    _("Number is larger than maximum"));
        }
      if (c == ':')
        {
          /* It's a string. */
          SVN_ERR(read_string(conn, pool, item, val));
          SVN_ERR(readbuf_getchar(conn, pool, &c));
        }
      else
        {
          /* It's a number. */
          item->kind = SVN_RA_SVN_NUMBER;
          item->u.number = val;
        }
    }
  else if (svn_ctype_isalpha(c))
    {
      /* It's a word. */
      str = svn_stringbuf_create_ensure(16, pool);
      svn_stringbuf_appendbyte(str, c);
      while (1)
        {
          SVN_ERR(readbuf_getchar(conn, pool, &c));
          if (!svn_ctype_isalnum(c) && c != '-')
            break;
          svn_stringbuf_appendbyte(str, c);
        }
      item->kind = SVN_RA_SVN_WORD;
      item->u.word = str->data;
    }
  else if (c == '(')
    {
      /* Read in the list items. */
      item->kind = SVN_RA_SVN_LIST;
      item->u.list = apr_array_make(pool, 4, sizeof(svn_ra_svn_item_t));
      while (1)
        {
          SVN_ERR(readbuf_getchar_skip_whitespace(conn, pool, &c));
          if (c == ')')
            break;
          listitem = apr_array_push(item->u.list);
          SVN_ERR(read_item(conn, pool, listitem, c, level));
        }
      SVN_ERR(readbuf_getchar(conn, pool, &c));
    }

  if (!svn_iswhitespace(c))
    return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                            _("Malformed network data"));
  return SVN_NO_ERROR;
}

/* Given the first non-whitespace character FIRST_CHAR, read the first
 * command (word) encountered in CONN into *ITEM.  If ITEM is NULL, skip
 * to the end of the current list.  Use POOL for allocations. */
static svn_error_t *
read_command_only(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                  const char **item, char first_char)
{
  char c = first_char;

  /* Determine the item type and read it in.  Make sure that c is the
  * first character at the end of the item so we can test to make
  * sure it's whitespace. */
  if (svn_ctype_isdigit(c))
    {
      /* It's a number or a string.  Read the number part, either way. */
      apr_uint64_t val, prev_val=0;
      val = c - '0';
      while (1)
        {
          prev_val = val;
          SVN_ERR(readbuf_getchar(conn, pool, &c));
          if (!svn_ctype_isdigit(c))
            break;
          val = val * 10 + (c - '0');
          if (prev_val >= (APR_UINT64_MAX / 10)) /* > maximum value? */
            return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                    _("Number is larger than maximum"));
        }
      if (c == ':')
        {
          /* It's a string. */
          SVN_ERR(readbuf_skip(conn, val));
          SVN_ERR(readbuf_getchar(conn, pool, &c));
        }
    }
  else if (svn_ctype_isalpha(c))
    {
      /* It's a word. */
      if (item)
        {
          /* This is the word we want to read */

          char *buf = apr_palloc(pool, 32);
          apr_size_t len = 1;
          buf[0] = c;

          while (1)
            {
              SVN_ERR(readbuf_getchar(conn, pool, &c));
              if (!svn_ctype_isalnum(c) && c != '-')
                break;
              buf[len] = c;
              if (++len == 32)
                return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                        _("Word too long"));
            }
          buf[len] = 0;
          *item = buf;
        }
      else
        {
          /* we don't need the actual word, just skip it */
          do
          {
            SVN_ERR(readbuf_getchar(conn, pool, &c));
          }
          while (svn_ctype_isalnum(c) || c == '-');
        }
    }
  else if (c == '(')
    {
      /* Read in the list items. */
      while (1)
        {
          SVN_ERR(readbuf_getchar_skip_whitespace(conn, pool, &c));
          if (c == ')')
            break;

          if (item && *item == NULL)
            SVN_ERR(read_command_only(conn, pool, item, c));
          else
            SVN_ERR(read_command_only(conn, pool, NULL, c));
        }
      SVN_ERR(readbuf_getchar(conn, pool, &c));
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__read_item(svn_ra_svn_conn_t *conn,
                      apr_pool_t *pool,
                      svn_ra_svn_item_t **item)
{
  char c;

  /* Allocate space, read the first character, and then do the rest of
   * the work.  This makes sense because of the way lists are read. */
  *item = apr_palloc(pool, sizeof(**item));
  SVN_ERR(readbuf_getchar_skip_whitespace(conn, pool, &c));
  return read_item(conn, pool, *item, c, 0);
}

svn_error_t *
svn_ra_svn__skip_leading_garbage(svn_ra_svn_conn_t *conn,
                                 apr_pool_t *pool)
{
  return readbuf_skip_leading_garbage(conn, pool);
}

/* --- READING AND PARSING TUPLES --- */

/* Parse a tuple of svn_ra_svn_item_t *'s.  Advance *FMT to the end of the
 * tuple specification and advance AP by the corresponding arguments. */
static svn_error_t *vparse_tuple(const apr_array_header_t *items, apr_pool_t *pool,
                                 const char **fmt, va_list *ap)
{
  int count, nesting_level;
  svn_ra_svn_item_t *elt;

  for (count = 0; **fmt && count < items->nelts; (*fmt)++, count++)
    {
      /* '?' just means the tuple may stop; skip past it. */
      if (**fmt == '?')
        (*fmt)++;
      elt = &APR_ARRAY_IDX(items, count, svn_ra_svn_item_t);
      if (**fmt == 'n' && elt->kind == SVN_RA_SVN_NUMBER)
        *va_arg(*ap, apr_uint64_t *) = elt->u.number;
      else if (**fmt == 'r' && elt->kind == SVN_RA_SVN_NUMBER)
        *va_arg(*ap, svn_revnum_t *) = (svn_revnum_t) elt->u.number;
      else if (**fmt == 's' && elt->kind == SVN_RA_SVN_STRING)
        *va_arg(*ap, svn_string_t **) = elt->u.string;
      else if (**fmt == 'c' && elt->kind == SVN_RA_SVN_STRING)
        *va_arg(*ap, const char **) = elt->u.string->data;
      else if (**fmt == 'w' && elt->kind == SVN_RA_SVN_WORD)
        *va_arg(*ap, const char **) = elt->u.word;
      else if (**fmt == 'b' && elt->kind == SVN_RA_SVN_WORD)
        {
          if (strcmp(elt->u.word, "true") == 0)
            *va_arg(*ap, svn_boolean_t *) = TRUE;
          else if (strcmp(elt->u.word, "false") == 0)
            *va_arg(*ap, svn_boolean_t *) = FALSE;
          else
            break;
        }
      else if (**fmt == 'B' && elt->kind == SVN_RA_SVN_WORD)
        {
          if (strcmp(elt->u.word, "true") == 0)
            *va_arg(*ap, apr_uint64_t *) = TRUE;
          else if (strcmp(elt->u.word, "false") == 0)
            *va_arg(*ap, apr_uint64_t *) = FALSE;
          else
            break;
        }
      else if (**fmt == 'l' && elt->kind == SVN_RA_SVN_LIST)
        *va_arg(*ap, apr_array_header_t **) = elt->u.list;
      else if (**fmt == '(' && elt->kind == SVN_RA_SVN_LIST)
        {
          (*fmt)++;
          SVN_ERR(vparse_tuple(elt->u.list, pool, fmt, ap));
        }
      else if (**fmt == ')')
        return SVN_NO_ERROR;
      else
        break;
    }
  if (**fmt == '?')
    {
      nesting_level = 0;
      for (; **fmt; (*fmt)++)
        {
          switch (**fmt)
            {
            case '?':
              break;
            case 'r':
              *va_arg(*ap, svn_revnum_t *) = SVN_INVALID_REVNUM;
              break;
            case 's':
              *va_arg(*ap, svn_string_t **) = NULL;
              break;
            case 'c':
            case 'w':
              *va_arg(*ap, const char **) = NULL;
              break;
            case 'l':
              *va_arg(*ap, apr_array_header_t **) = NULL;
              break;
            case 'B':
            case 'n':
              *va_arg(*ap, apr_uint64_t *) = SVN_RA_SVN_UNSPECIFIED_NUMBER;
              break;
            case '(':
              nesting_level++;
              break;
            case ')':
              if (--nesting_level < 0)
                return SVN_NO_ERROR;
              break;
            default:
              SVN_ERR_MALFUNCTION();
            }
        }
    }
  if (**fmt && **fmt != ')')
    return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                            _("Malformed network data"));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__parse_tuple(const apr_array_header_t *list,
                        apr_pool_t *pool,
                        const char *fmt, ...)
{
  svn_error_t *err;
  va_list ap;

  va_start(ap, fmt);
  err = vparse_tuple(list, pool, &fmt, &ap);
  va_end(ap);
  return err;
}

svn_error_t *
svn_ra_svn__read_tuple(svn_ra_svn_conn_t *conn,
                       apr_pool_t *pool,
                       const char *fmt, ...)
{
  va_list ap;
  svn_ra_svn_item_t *item;
  svn_error_t *err;

  SVN_ERR(svn_ra_svn__read_item(conn, pool, &item));
  if (item->kind != SVN_RA_SVN_LIST)
    return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                            _("Malformed network data"));
  va_start(ap, fmt);
  err = vparse_tuple(item->u.list, pool, &fmt, &ap);
  va_end(ap);
  return err;
}

svn_error_t *
svn_ra_svn__read_command_only(svn_ra_svn_conn_t *conn,
                              apr_pool_t *pool,
                              const char **command)
{
  char c;
  SVN_ERR(readbuf_getchar_skip_whitespace(conn, pool, &c));

  *command = NULL;
  return read_command_only(conn, pool, command, c);
}


svn_error_t *
svn_ra_svn__parse_proplist(const apr_array_header_t *list,
                           apr_pool_t *pool,
                           apr_hash_t **props)
{
  char *name;
  svn_string_t *value;
  svn_ra_svn_item_t *elt;
  int i;

  *props = apr_hash_make(pool);
  for (i = 0; i < list->nelts; i++)
    {
      elt = &APR_ARRAY_IDX(list, i, svn_ra_svn_item_t);
      if (elt->kind != SVN_RA_SVN_LIST)
        return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                _("Proplist element not a list"));
      SVN_ERR(svn_ra_svn__parse_tuple(elt->u.list, pool, "cs",
                                      &name, &value));
      svn_hash_sets(*props, name, value);
    }

  return SVN_NO_ERROR;
}


/* --- READING AND WRITING COMMANDS AND RESPONSES --- */

svn_error_t *svn_ra_svn__locate_real_error_child(svn_error_t *err)
{
  svn_error_t *this_link;

  SVN_ERR_ASSERT(err);

  for (this_link = err;
       this_link && (this_link->apr_err == SVN_ERR_RA_SVN_CMD_ERR);
       this_link = this_link->child)
    ;

  SVN_ERR_ASSERT(this_link);
  return this_link;
}

svn_error_t *svn_ra_svn__handle_failure_status(const apr_array_header_t *params,
                                               apr_pool_t *pool)
{
  const char *message, *file;
  svn_error_t *err = NULL;
  svn_ra_svn_item_t *elt;
  int i;
  apr_uint64_t apr_err, line;
  apr_pool_t *subpool = svn_pool_create(pool);

  if (params->nelts == 0)
    return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                            _("Empty error list"));

  /* Rebuild the error list from the end, to avoid reversing the order. */
  for (i = params->nelts - 1; i >= 0; i--)
    {
      svn_pool_clear(subpool);
      elt = &APR_ARRAY_IDX(params, i, svn_ra_svn_item_t);
      if (elt->kind != SVN_RA_SVN_LIST)
        return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                _("Malformed error list"));
      SVN_ERR(svn_ra_svn__parse_tuple(elt->u.list, subpool, "nccn",
                                      &apr_err, &message, &file, &line));
      /* The message field should have been optional, but we can't
         easily change that, so "" means a nonexistent message. */
      if (!*message)
        message = NULL;

      /* Skip over links in the error chain that were intended only to
         exist on the server (to wrap real errors intended for the
         client) but accidentally got included in the server's actual
         response. */
      if ((apr_status_t)apr_err != SVN_ERR_RA_SVN_CMD_ERR)
        {
          err = svn_error_create((apr_status_t)apr_err, err, message);
          err->file = apr_pstrdup(err->pool, file);
          err->line = (long)line;
        }
    }

  svn_pool_destroy(subpool);

  /* If we get here, then we failed to find a real error in the error
     chain that the server proported to be sending us.  That's bad. */
  if (! err)
    err = svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                           _("Malformed error list"));

  return err;
}

svn_error_t *
svn_ra_svn__read_cmd_response(svn_ra_svn_conn_t *conn,
                              apr_pool_t *pool,
                              const char *fmt, ...)
{
  va_list ap;
  const char *status;
  apr_array_header_t *params;
  svn_error_t *err;

  SVN_ERR(svn_ra_svn__read_tuple(conn, pool, "wl", &status, &params));
  if (strcmp(status, "success") == 0)
    {
      va_start(ap, fmt);
      err = vparse_tuple(params, pool, &fmt, &ap);
      va_end(ap);
      return err;
    }
  else if (strcmp(status, "failure") == 0)
    {
      return svn_ra_svn__handle_failure_status(params, pool);
    }

  return svn_error_createf(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                           _("Unknown status '%s' in command response"),
                           status);
}

svn_error_t *
svn_ra_svn__handle_commands2(svn_ra_svn_conn_t *conn,
                             apr_pool_t *pool,
                             const svn_ra_svn_cmd_entry_t *commands,
                             void *baton,
                             svn_boolean_t error_on_disconnect)
{
  apr_pool_t *subpool = svn_pool_create(pool);
  apr_pool_t *iterpool = svn_pool_create(subpool);
  const char *cmdname;
  const svn_ra_svn_cmd_entry_t *command;
  svn_error_t *err, *write_err;
  apr_array_header_t *params;
  apr_hash_t *cmd_hash = apr_hash_make(subpool);

  for (command = commands; command->cmdname; command++)
    svn_hash_sets(cmd_hash, command->cmdname, command);

  while (1)
    {
      svn_pool_clear(iterpool);
      err = svn_ra_svn__read_tuple(conn, iterpool, "wl", &cmdname, &params);
      if (err)
        {
          if (!error_on_disconnect
              && err->apr_err == SVN_ERR_RA_SVN_CONNECTION_CLOSED)
            {
              svn_error_clear(err);
              svn_pool_destroy(subpool);
              return SVN_NO_ERROR;
            }
          return err;
        }
      command = svn_hash_gets(cmd_hash, cmdname);

      if (command)
        err = (*command->handler)(conn, iterpool, params, baton);
      else
        {
          err = svn_error_createf(SVN_ERR_RA_SVN_UNKNOWN_CMD, NULL,
                                  _("Unknown editor command '%s'"), cmdname);
          err = svn_error_create(SVN_ERR_RA_SVN_CMD_ERR, err, NULL);
        }

      if (err && err->apr_err == SVN_ERR_RA_SVN_CMD_ERR)
        {
          write_err = svn_ra_svn__write_cmd_failure(
                          conn, iterpool,
                          svn_ra_svn__locate_real_error_child(err));
          svn_error_clear(err);
          if (write_err)
            return write_err;
        }
      else if (err)
        return err;

      if (command && command->terminate)
        break;
    }
  svn_pool_destroy(iterpool);
  svn_pool_destroy(subpool);
  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_target_rev(svn_ra_svn_conn_t *conn,
                                 apr_pool_t *pool,
                                 svn_revnum_t rev)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( target-rev ( ", 15));
  SVN_ERR(write_tuple_revision(conn, pool, rev));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_open_root(svn_ra_svn_conn_t *conn,
                                apr_pool_t *pool,
                                svn_revnum_t rev,
                                const char *token)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( open-root ( ", 14));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_revision_opt(conn, pool, rev));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(write_tuple_cstring(conn, pool, token));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_delete_entry(svn_ra_svn_conn_t *conn,
                                   apr_pool_t *pool,
                                   const char *path,
                                   svn_revnum_t rev,
                                   const char *token)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( delete-entry ( ", 17));
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_revision_opt(conn, pool, rev));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(write_tuple_cstring(conn, pool, token));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_add_dir(svn_ra_svn_conn_t *conn,
                              apr_pool_t *pool,
                              const char *path,
                              const char *parent_token,
                              const char *token,
                              const char *copy_path,
                              svn_revnum_t copy_rev)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( add-dir ( ", 12));
  SVN_ERR(write_cmd_add_node(conn, pool, path, parent_token, token,
                              copy_path, copy_rev));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_open_dir(svn_ra_svn_conn_t *conn,
                               apr_pool_t *pool,
                               const char *path,
                               const char *parent_token,
                               const char *token,
                               svn_revnum_t rev)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( open-dir ( ", 13));
  SVN_ERR(write_cmd_open_node(conn, pool, path, parent_token, token, rev));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_change_dir_prop(svn_ra_svn_conn_t *conn,
                                      apr_pool_t *pool,
                                      const char *token,
                                      const char *name,
                                      const svn_string_t *value)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( change-dir-prop ( ", 20));
  SVN_ERR(write_cmd_change_node_prop(conn, pool, token, name, value));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_close_dir(svn_ra_svn_conn_t *conn,
                                apr_pool_t *pool,
                                const char *token)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( close-dir ( ", 14));
  SVN_ERR(write_tuple_cstring(conn, pool, token));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_absent_dir(svn_ra_svn_conn_t *conn,
                                 apr_pool_t *pool,
                                 const char *path,
                                 const char *parent_token)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( absent-dir ( ", 15));
  SVN_ERR(write_cmd_absent_node(conn, pool, path, parent_token));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_add_file(svn_ra_svn_conn_t *conn,
                               apr_pool_t *pool,
                               const char *path,
                               const char *parent_token,
                               const char *token,
                               const char *copy_path,
                               svn_revnum_t copy_rev)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( add-file ( ", 13));
  SVN_ERR(write_cmd_add_node(conn, pool, path, parent_token, token,
                              copy_path, copy_rev));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_open_file(svn_ra_svn_conn_t *conn,
                                apr_pool_t *pool,
                                const char *path,
                                const char *parent_token,
                                const char *token,
                                svn_revnum_t rev)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( open-file ( ", 14));
  SVN_ERR(write_cmd_open_node(conn, pool, path, parent_token, token, rev));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_change_file_prop(svn_ra_svn_conn_t *conn,
                                       apr_pool_t *pool,
                                       const char *token,
                                       const char *name,
                                       const svn_string_t *value)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( change-file-prop ( ", 21));
  SVN_ERR(write_cmd_change_node_prop(conn, pool, token, name, value));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_close_file(svn_ra_svn_conn_t *conn,
                                 apr_pool_t *pool,
                                 const char *token,
                                 const char *text_checksum)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( close-file ( ", 15));
  SVN_ERR(write_tuple_cstring(conn, pool, token));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_cstring_opt(conn, pool, text_checksum));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_absent_file(svn_ra_svn_conn_t *conn,
                                  apr_pool_t *pool,
                                  const char *path,
                                  const char *parent_token)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( absent-file ( ", 16));
  SVN_ERR(write_cmd_absent_node(conn, pool, path, parent_token));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_textdelta_chunk(svn_ra_svn_conn_t *conn,
                                      apr_pool_t *pool,
                                      const char *token,
                                      const svn_string_t *chunk)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( textdelta-chunk ( ", 20));
  SVN_ERR(write_tuple_cstring(conn, pool, token));
  SVN_ERR(write_tuple_string(conn, pool, chunk));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_textdelta_end(svn_ra_svn_conn_t *conn,
                                    apr_pool_t *pool,
                                    const char *token)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( textdelta-end ( ", 18));
  SVN_ERR(write_tuple_cstring(conn, pool, token));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_apply_textdelta(svn_ra_svn_conn_t *conn,
                                      apr_pool_t *pool,
                                      const char *token,
                                      const char *base_checksum)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( apply-textdelta ( ", 20));
  SVN_ERR(write_tuple_cstring(conn, pool, token));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_cstring_opt(conn, pool, base_checksum));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_close_edit(svn_ra_svn_conn_t *conn,
                                 apr_pool_t *pool)
{
  return writebuf_write_short_string(conn, pool, "( close-edit ( ) ) ", 19);
}

svn_error_t *
svn_ra_svn__write_cmd_abort_edit(svn_ra_svn_conn_t *conn,
                                 apr_pool_t *pool)
{
  return writebuf_write_short_string(conn, pool, "( abort-edit ( ) ) ", 19);
}

svn_error_t *
svn_ra_svn__write_cmd_set_path(svn_ra_svn_conn_t *conn,
                               apr_pool_t *pool,
                               const char *path,
                               svn_revnum_t rev,
                               svn_boolean_t start_empty,
                               const char *lock_token,
                               svn_depth_t depth)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( set-path ( ", 13));
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(write_tuple_revision(conn, pool, rev));
  SVN_ERR(write_tuple_boolean(conn, pool, start_empty));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_cstring_opt(conn, pool, lock_token));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(write_tuple_depth(conn, pool, depth));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_delete_path(svn_ra_svn_conn_t *conn,
                                  apr_pool_t *pool,
                                  const char *path)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( delete-path ( ", 16));
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_link_path(svn_ra_svn_conn_t *conn,
                                apr_pool_t *pool,
                                const char *path,
                                const char *url,
                                svn_revnum_t rev,
                                svn_boolean_t start_empty,
                                const char *lock_token,
                                svn_depth_t depth)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( link-path ( ", 14));
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(write_tuple_cstring(conn, pool, url));
  SVN_ERR(write_tuple_revision(conn, pool, rev));
  SVN_ERR(write_tuple_boolean(conn, pool, start_empty));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_cstring_opt(conn, pool,lock_token));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(write_tuple_depth(conn, pool, depth));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_finish_report(svn_ra_svn_conn_t *conn,
                                    apr_pool_t *pool)
{
  return writebuf_write_short_string(conn, pool, "( finish-report ( ) ) ", 22);
}

svn_error_t *
svn_ra_svn__write_cmd_abort_report(svn_ra_svn_conn_t *conn,
                                   apr_pool_t *pool)
{
  return writebuf_write_short_string(conn, pool, "( abort-report ( ) ) ", 21);
}

svn_error_t *
svn_ra_svn__write_cmd_reparent(svn_ra_svn_conn_t *conn,
                               apr_pool_t *pool,
                               const char *url)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( reparent ( ", 13));
  SVN_ERR(write_tuple_cstring(conn, pool, url));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_get_latest_rev(svn_ra_svn_conn_t *conn,
                                   apr_pool_t *pool)
{
  return writebuf_write_short_string(conn, pool, "( get-latest-rev ( ) ) ", 23);
}

svn_error_t *
svn_ra_svn__write_cmd_get_dated_rev(svn_ra_svn_conn_t *conn,
                                    apr_pool_t *pool,
                                    apr_time_t tm)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( get-dated-rev ( ", 18));
  SVN_ERR(write_tuple_cstring(conn, pool, svn_time_to_cstring(tm, pool)));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_change_rev_prop2(svn_ra_svn_conn_t *conn,
                                       apr_pool_t *pool,
                                       svn_revnum_t rev,
                                       const char *name,
                                       const svn_string_t *value,
                                       svn_boolean_t dont_care,
                                       const svn_string_t *old_value)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( change-rev-prop2 ( ", 21));
  SVN_ERR(write_tuple_revision(conn, pool, rev));
  SVN_ERR(write_tuple_cstring(conn, pool, name));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_string_opt(conn, pool, value));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_boolean(conn, pool, dont_care));
  SVN_ERR(write_tuple_string_opt(conn, pool, old_value));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_change_rev_prop(svn_ra_svn_conn_t *conn,
                                      apr_pool_t *pool,
                                      svn_revnum_t rev,
                                      const char *name,
                                      const svn_string_t *value)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( change-rev-prop ( ", 20));
  SVN_ERR(write_tuple_revision(conn, pool, rev));
  SVN_ERR(write_tuple_cstring(conn, pool, name));
  SVN_ERR(write_tuple_string_opt(conn, pool, value));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_rev_proplist(svn_ra_svn_conn_t *conn,
                                   apr_pool_t *pool,
                                   svn_revnum_t rev)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( rev-proplist ( ", 17));
  SVN_ERR(write_tuple_revision(conn, pool, rev));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_rev_prop(svn_ra_svn_conn_t *conn,
                               apr_pool_t *pool,
                               svn_revnum_t rev,
                               const char *name)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( rev-prop ( ", 13));
  SVN_ERR(write_tuple_revision(conn, pool, rev));
  SVN_ERR(write_tuple_cstring(conn, pool, name));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_get_file(svn_ra_svn_conn_t *conn,
                               apr_pool_t *pool,
                               const char *path,
                               svn_revnum_t rev,
                               svn_boolean_t props,
                               svn_boolean_t stream)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( get-file ( ", 13));
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_revision_opt(conn, pool, rev));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(write_tuple_boolean(conn, pool, props));
  SVN_ERR(write_tuple_boolean(conn, pool, stream));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_update(svn_ra_svn_conn_t *conn,
                             apr_pool_t *pool,
                             svn_revnum_t rev,
                             const char *target,
                             svn_boolean_t recurse,
                             svn_depth_t depth,
                             svn_boolean_t send_copyfrom_args,
                             svn_boolean_t ignore_ancestry)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( update ( ", 11));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_revision_opt(conn, pool, rev));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(write_tuple_cstring(conn, pool, target));
  SVN_ERR(write_tuple_boolean(conn, pool, recurse));
  SVN_ERR(write_tuple_depth(conn, pool, depth));
  SVN_ERR(write_tuple_boolean(conn, pool, send_copyfrom_args));
  SVN_ERR(write_tuple_boolean(conn, pool, ignore_ancestry));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_switch(svn_ra_svn_conn_t *conn,
                             apr_pool_t *pool,
                             svn_revnum_t rev,
                             const char *target,
                             svn_boolean_t recurse,
                             const char *switch_url,
                             svn_depth_t depth,
                             svn_boolean_t send_copyfrom_args,
                             svn_boolean_t ignore_ancestry)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( switch ( ", 11));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_revision_opt(conn, pool, rev));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(write_tuple_cstring(conn, pool, target));
  SVN_ERR(write_tuple_boolean(conn, pool, recurse));
  SVN_ERR(write_tuple_cstring(conn, pool, switch_url));
  SVN_ERR(write_tuple_depth(conn, pool, depth));
  SVN_ERR(write_tuple_boolean(conn, pool, send_copyfrom_args));
  SVN_ERR(write_tuple_boolean(conn, pool, ignore_ancestry));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_status(svn_ra_svn_conn_t *conn,
                             apr_pool_t *pool,
                             const char *target,
                             svn_boolean_t recurse,
                             svn_revnum_t rev,
                             svn_depth_t depth)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( status ( ", 11));
  SVN_ERR(write_tuple_cstring(conn, pool, target));
  SVN_ERR(write_tuple_boolean(conn, pool, recurse));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_revision_opt(conn, pool, rev));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(write_tuple_depth(conn, pool, depth));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_diff(svn_ra_svn_conn_t *conn,
                           apr_pool_t *pool,
                           svn_revnum_t rev,
                           const char *target,
                           svn_boolean_t recurse,
                           svn_boolean_t ignore_ancestry,
                           const char *versus_url,
                           svn_boolean_t text_deltas,
                           svn_depth_t depth)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( diff ( ", 9));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_revision_opt(conn, pool, rev));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(write_tuple_cstring(conn, pool, target));
  SVN_ERR(write_tuple_boolean(conn, pool, recurse));
  SVN_ERR(write_tuple_boolean(conn, pool, ignore_ancestry));
  SVN_ERR(write_tuple_cstring(conn, pool, versus_url));
  SVN_ERR(write_tuple_boolean(conn, pool, text_deltas));
  SVN_ERR(write_tuple_depth(conn, pool, depth));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_check_path(svn_ra_svn_conn_t *conn,
                                 apr_pool_t *pool,
                                 const char *path,
                                 svn_revnum_t rev)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( check-path ( ", 15));
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_revision_opt(conn, pool, rev));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_stat(svn_ra_svn_conn_t *conn,
                           apr_pool_t *pool,
                           const char *path,
                           svn_revnum_t rev)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( stat ( ", 9));
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_revision_opt(conn, pool, rev));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_get_file_revs(svn_ra_svn_conn_t *conn,
                                    apr_pool_t *pool,
                                    const char *path,
                                    svn_revnum_t start,
                                    svn_revnum_t end,
                                    svn_boolean_t include_merged_revisions)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( get-file-revs ( ", 18));
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_revision_opt(conn, pool, start));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_revision_opt(conn, pool, end));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(write_tuple_boolean(conn, pool, include_merged_revisions));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_lock(svn_ra_svn_conn_t *conn,
                           apr_pool_t *pool,
                           const char *path,
                           const char *comment,
                           svn_boolean_t steal_lock,
                           svn_revnum_t revnum)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( lock ( ", 9));
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_cstring_opt(conn, pool, comment));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(write_tuple_boolean(conn, pool, steal_lock));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_revision_opt(conn, pool, revnum));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_unlock(svn_ra_svn_conn_t *conn,
                             apr_pool_t *pool,
                             const char *path,
                             const char *token,
                             svn_boolean_t break_lock)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( unlock ( ", 11));
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_cstring_opt(conn, pool, token));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(write_tuple_boolean(conn, pool, break_lock));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_get_lock(svn_ra_svn_conn_t *conn,
                               apr_pool_t *pool,
                               const char *path)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( get-lock ( ", 13));
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_get_locks(svn_ra_svn_conn_t *conn,
                                apr_pool_t *pool,
                                const char *path,
                                svn_depth_t depth)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( get-locks ( ", 14));
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_depth(conn, pool, depth));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_replay(svn_ra_svn_conn_t *conn,
                             apr_pool_t *pool,
                             svn_revnum_t rev,
                             svn_revnum_t low_water_mark,
                             svn_boolean_t send_deltas)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( replay ( ", 11));
  SVN_ERR(write_tuple_revision(conn, pool, rev));
  SVN_ERR(write_tuple_revision(conn, pool, low_water_mark));
  SVN_ERR(write_tuple_boolean(conn, pool, send_deltas));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_replay_range(svn_ra_svn_conn_t *conn,
                                   apr_pool_t *pool,
                                   svn_revnum_t start_revision,
                                   svn_revnum_t end_revision,
                                   svn_revnum_t low_water_mark,
                                   svn_boolean_t send_deltas)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( replay-range ( ", 17));
  SVN_ERR(write_tuple_revision(conn, pool, start_revision));
  SVN_ERR(write_tuple_revision(conn, pool, end_revision));
  SVN_ERR(write_tuple_revision(conn, pool, low_water_mark));
  SVN_ERR(write_tuple_boolean(conn, pool, send_deltas));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_get_deleted_rev(svn_ra_svn_conn_t *conn,
                                      apr_pool_t *pool,
                                      const char *path,
                                      svn_revnum_t peg_revision,
                                      svn_revnum_t end_revision)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( get-deleted-rev ( ", 20));
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(write_tuple_revision(conn, pool, peg_revision));
  SVN_ERR(write_tuple_revision(conn, pool, end_revision));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_get_iprops(svn_ra_svn_conn_t *conn,
                                 apr_pool_t *pool,
                                 const char *path,
                                 svn_revnum_t revision)
{
  SVN_ERR(writebuf_write_short_string(conn, pool, "( get-iprops ( ", 15));
  SVN_ERR(write_tuple_cstring(conn, pool, path));
  SVN_ERR(write_tuple_start_list(conn, pool));
  SVN_ERR(write_tuple_revision_opt(conn, pool, revision));
  SVN_ERR(write_tuple_end_list(conn, pool));
  SVN_ERR(writebuf_write_short_string(conn, pool, ") ) ", 4));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_svn__write_cmd_finish_replay(svn_ra_svn_conn_t *conn,
                                    apr_pool_t *pool)
{
  return writebuf_write_short_string(conn, pool, "( finish-replay ( ) ) ", 22);
}

svn_error_t *svn_ra_svn__write_cmd_response(svn_ra_svn_conn_t *conn,
                                            apr_pool_t *pool,
                                            const char *fmt, ...)
{
  va_list ap;
  svn_error_t *err;

  SVN_ERR(writebuf_write_short_string(conn, pool, "( success ", 10));
  va_start(ap, fmt);
  err = vwrite_tuple(conn, pool, fmt, &ap);
  va_end(ap);
  return err ? svn_error_trace(err) : svn_ra_svn__end_list(conn, pool);
}

svn_error_t *svn_ra_svn__write_cmd_failure(svn_ra_svn_conn_t *conn,
                                           apr_pool_t *pool, svn_error_t *err)
{
  char buffer[128];
  SVN_ERR(writebuf_write_short_string(conn, pool, "( failure ( ", 12));
  for (; err; err = err->child)
    {
      const char *msg;

#ifdef SVN_ERR__TRACING
      if (svn_error__is_tracing_link(err))
        msg = err->message;
      else
#endif
        msg = svn_err_best_message(err, buffer, sizeof(buffer));

      /* The message string should have been optional, but we can't
         easily change that, so marshal nonexistent messages as "". */
      SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "nccn",
                                      (apr_uint64_t) err->apr_err,
                                      msg ? msg : "",
                                      err->file ? err->file : "",
                                      (apr_uint64_t) err->line));
    }
  return writebuf_write_short_string(conn, pool, ") ) ", 4);
}
