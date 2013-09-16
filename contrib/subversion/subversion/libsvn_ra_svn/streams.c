/*
 * streams.c :  stream encapsulation routines for the ra_svn protocol
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



#include <apr_general.h>
#include <apr_network_io.h>
#include <apr_poll.h>

#include "svn_types.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "svn_io.h"
#include "svn_private_config.h"

#include "ra_svn.h"

struct svn_ra_svn__stream_st {
  svn_stream_t *stream;
  void *baton;
  ra_svn_pending_fn_t pending_fn;
  ra_svn_timeout_fn_t timeout_fn;
};

typedef struct sock_baton_t {
  apr_socket_t *sock;
  apr_pool_t *pool;
} sock_baton_t;

typedef struct file_baton_t {
  apr_file_t *in_file;
  apr_file_t *out_file;
  apr_pool_t *pool;
} file_baton_t;

/* Returns TRUE if PFD has pending data, FALSE otherwise. */
static svn_boolean_t pending(apr_pollfd_t *pfd, apr_pool_t *pool)
{
  apr_status_t status;
  int n;

  pfd->p = pool;
  pfd->reqevents = APR_POLLIN;
  status = apr_poll(pfd, 1, &n, 0);
  return (status == APR_SUCCESS && n);
}

/* Functions to implement a file backed svn_ra_svn__stream_t. */

/* Implements svn_read_fn_t */
static svn_error_t *
file_read_cb(void *baton, char *buffer, apr_size_t *len)
{
  file_baton_t *b = baton;
  apr_status_t status = apr_file_read(b->in_file, buffer, len);

  if (status && !APR_STATUS_IS_EOF(status))
    return svn_error_wrap_apr(status, _("Can't read from connection"));
  if (*len == 0)
    return svn_error_create(SVN_ERR_RA_SVN_CONNECTION_CLOSED, NULL, NULL);
  return SVN_NO_ERROR;
}

/* Implements svn_write_fn_t */
static svn_error_t *
file_write_cb(void *baton, const char *buffer, apr_size_t *len)
{
  file_baton_t *b = baton;
  apr_status_t status = apr_file_write(b->out_file, buffer, len);
  if (status)
    return svn_error_wrap_apr(status, _("Can't write to connection"));
  return SVN_NO_ERROR;
}

/* Implements ra_svn_timeout_fn_t */
static void
file_timeout_cb(void *baton, apr_interval_time_t interval)
{
  file_baton_t *b = baton;
  apr_file_pipe_timeout_set(b->out_file, interval);
}

/* Implements ra_svn_pending_fn_t */
static svn_boolean_t
file_pending_cb(void *baton)
{
  file_baton_t *b = baton;
  apr_pollfd_t pfd;

  pfd.desc_type = APR_POLL_FILE;
  pfd.desc.f = b->in_file;

  return pending(&pfd, b->pool);
}

svn_ra_svn__stream_t *
svn_ra_svn__stream_from_files(apr_file_t *in_file,
                              apr_file_t *out_file,
                              apr_pool_t *pool)
{
  file_baton_t *b = apr_palloc(pool, sizeof(*b));

  b->in_file = in_file;
  b->out_file = out_file;
  b->pool = pool;

  return svn_ra_svn__stream_create(b, file_read_cb, file_write_cb,
                                   file_timeout_cb, file_pending_cb,
                                   pool);
}

/* Functions to implement a socket backed svn_ra_svn__stream_t. */

/* Implements svn_read_fn_t */
static svn_error_t *
sock_read_cb(void *baton, char *buffer, apr_size_t *len)
{
  sock_baton_t *b = baton;
  apr_status_t status;
  apr_interval_time_t interval;

  status = apr_socket_timeout_get(b->sock, &interval);
  if (status)
    return svn_error_wrap_apr(status, _("Can't get socket timeout"));

  /* Always block on read.
   * During pipelining, we set the timeout to 0 for some write
   * operations so that we can try them without blocking. If APR had
   * separate timeouts for read and write, we would only set the
   * write timeout, but it doesn't. So here, we revert back to blocking.
   */
  apr_socket_timeout_set(b->sock, -1);
  status = apr_socket_recv(b->sock, buffer, len);
  apr_socket_timeout_set(b->sock, interval);

  if (status && !APR_STATUS_IS_EOF(status))
    return svn_error_wrap_apr(status, _("Can't read from connection"));
  if (*len == 0)
    return svn_error_create(SVN_ERR_RA_SVN_CONNECTION_CLOSED, NULL, NULL);
  return SVN_NO_ERROR;
}

/* Implements svn_write_fn_t */
static svn_error_t *
sock_write_cb(void *baton, const char *buffer, apr_size_t *len)
{
  sock_baton_t *b = baton;
  apr_status_t status = apr_socket_send(b->sock, buffer, len);
  if (status)
    return svn_error_wrap_apr(status, _("Can't write to connection"));
  return SVN_NO_ERROR;
}

/* Implements ra_svn_timeout_fn_t */
static void
sock_timeout_cb(void *baton, apr_interval_time_t interval)
{
  sock_baton_t *b = baton;
  apr_socket_timeout_set(b->sock, interval);
}

/* Implements ra_svn_pending_fn_t */
static svn_boolean_t
sock_pending_cb(void *baton)
{
  sock_baton_t *b = baton;
  apr_pollfd_t pfd;

  pfd.desc_type = APR_POLL_SOCKET;
  pfd.desc.s = b->sock;

  return pending(&pfd, b->pool);
}

svn_ra_svn__stream_t *
svn_ra_svn__stream_from_sock(apr_socket_t *sock,
                             apr_pool_t *pool)
{
  sock_baton_t *b = apr_palloc(pool, sizeof(*b));

  b->sock = sock;
  b->pool = pool;

  return svn_ra_svn__stream_create(b, sock_read_cb, sock_write_cb,
                                   sock_timeout_cb, sock_pending_cb,
                                   pool);
}

svn_ra_svn__stream_t *
svn_ra_svn__stream_create(void *baton,
                          svn_read_fn_t read_cb,
                          svn_write_fn_t write_cb,
                          ra_svn_timeout_fn_t timeout_cb,
                          ra_svn_pending_fn_t pending_cb,
                          apr_pool_t *pool)
{
  svn_ra_svn__stream_t *s = apr_palloc(pool, sizeof(*s));
  s->stream = svn_stream_empty(pool);
  svn_stream_set_baton(s->stream, baton);
  if (read_cb)
    svn_stream_set_read(s->stream, read_cb);
  if (write_cb)
    svn_stream_set_write(s->stream, write_cb);
  s->baton = baton;
  s->timeout_fn = timeout_cb;
  s->pending_fn = pending_cb;
  return s;
}

svn_error_t *
svn_ra_svn__stream_write(svn_ra_svn__stream_t *stream,
                         const char *data, apr_size_t *len)
{
  return svn_stream_write(stream->stream, data, len);
}

svn_error_t *
svn_ra_svn__stream_read(svn_ra_svn__stream_t *stream, char *data,
                        apr_size_t *len)
{
  return svn_stream_read(stream->stream, data, len);
}

void
svn_ra_svn__stream_timeout(svn_ra_svn__stream_t *stream,
                           apr_interval_time_t interval)
{
  stream->timeout_fn(stream->baton, interval);
}

svn_boolean_t
svn_ra_svn__stream_pending(svn_ra_svn__stream_t *stream)
{
  return stream->pending_fn(stream->baton);
}
