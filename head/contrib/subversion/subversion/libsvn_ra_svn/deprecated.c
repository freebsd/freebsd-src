/*
 * deprecated.c :  Public, deprecated wrappers to our private ra_svn API
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

#include "svn_ra_svn.h"

#include "private/svn_ra_svn_private.h"

svn_error_t *
svn_ra_svn_write_number(svn_ra_svn_conn_t *conn,
                        apr_pool_t *pool,
                        apr_uint64_t number)
{
  return svn_error_trace(svn_ra_svn__write_number(conn, pool, number));
}

svn_error_t *
svn_ra_svn_write_string(svn_ra_svn_conn_t *conn,
                        apr_pool_t *pool,
                        const svn_string_t *str)
{
  return svn_error_trace(svn_ra_svn__write_string(conn, pool, str));
}

svn_error_t *
svn_ra_svn_write_cstring(svn_ra_svn_conn_t *conn,
                         apr_pool_t *pool,
                         const char *s)
{
  return svn_error_trace(svn_ra_svn__write_cstring(conn, pool, s));
}

svn_error_t *
svn_ra_svn_write_word(svn_ra_svn_conn_t *conn,
                      apr_pool_t *pool,
                      const char *word)
{
  return svn_error_trace(svn_ra_svn__write_word(conn, pool, word));
}

svn_error_t *
svn_ra_svn_write_proplist(svn_ra_svn_conn_t *conn,
                          apr_pool_t *pool,
                          apr_hash_t *props)
{
  return svn_error_trace(svn_ra_svn__write_proplist(conn, pool, props));
}

svn_error_t *
svn_ra_svn_start_list(svn_ra_svn_conn_t *conn,
                      apr_pool_t *pool)
{
  return svn_error_trace(svn_ra_svn__start_list(conn, pool));
}

svn_error_t *
svn_ra_svn_end_list(svn_ra_svn_conn_t *conn,
                    apr_pool_t *pool)
{
  return svn_error_trace(svn_ra_svn__end_list(conn, pool));
}

svn_error_t *
svn_ra_svn_flush(svn_ra_svn_conn_t *conn,
                 apr_pool_t *pool)
{
  return svn_error_trace(svn_ra_svn__flush(conn, pool));
}

svn_error_t *
svn_ra_svn_write_tuple(svn_ra_svn_conn_t *conn,
                       apr_pool_t *pool,
                       const char *fmt, ...)
{
  va_list va;
  svn_error_t *err;

  va_start(va, fmt);
  err = svn_ra_svn__write_tuple(conn, pool, fmt, va);
  va_end(va);

  return svn_error_trace(err);
}

svn_error_t *
svn_ra_svn_read_item(svn_ra_svn_conn_t *conn,
                     apr_pool_t *pool,
                     svn_ra_svn_item_t **item)
{
  return svn_error_trace(svn_ra_svn__read_item(conn, pool, item));
}

svn_error_t *
svn_ra_svn_skip_leading_garbage(svn_ra_svn_conn_t *conn,
                                apr_pool_t *pool)
{
  return svn_error_trace(svn_ra_svn__skip_leading_garbage(conn, pool));
}

svn_error_t *
svn_ra_svn_parse_tuple(const apr_array_header_t *list,
                       apr_pool_t *pool,
                       const char *fmt, ...)
{
  va_list va;
  svn_error_t *err;

  va_start(va, fmt);
  err = svn_ra_svn__parse_tuple(list, pool, fmt, va);
  va_end(va);

  return svn_error_trace(err);
}

svn_error_t *
svn_ra_svn_read_tuple(svn_ra_svn_conn_t *conn,
                      apr_pool_t *pool,
                      const char *fmt, ...)
{
  va_list va;
  svn_error_t *err;

  va_start(va, fmt);
  err = svn_ra_svn__read_tuple(conn, pool, fmt, va);
  va_end(va);

  return svn_error_trace(err);
}

svn_error_t *
svn_ra_svn_parse_proplist(const apr_array_header_t *list,
                          apr_pool_t *pool,
                          apr_hash_t **props)
{
  return svn_error_trace(svn_ra_svn__parse_proplist(list, pool, props));
}

svn_error_t *
svn_ra_svn_read_cmd_response(svn_ra_svn_conn_t *conn,
                             apr_pool_t *pool,
                             const char *fmt, ...)
{
  va_list va;
  svn_error_t *err;

  va_start(va, fmt);
  err = svn_ra_svn__read_cmd_response(conn, pool, fmt, va);
  va_end(va);

  return svn_error_trace(err);
}

svn_error_t *
svn_ra_svn_handle_commands2(svn_ra_svn_conn_t *conn,
                            apr_pool_t *pool,
                            const svn_ra_svn_cmd_entry_t *commands,
                            void *baton,
                            svn_boolean_t error_on_disconnect)
{
  return svn_error_trace(svn_ra_svn__handle_commands2(conn, pool,
                                                      commands, baton,
                                                      error_on_disconnect));
}

svn_error_t *
svn_ra_svn_handle_commands(svn_ra_svn_conn_t *conn,
                           apr_pool_t *pool,
                           const svn_ra_svn_cmd_entry_t *commands,
                           void *baton)
{
  return svn_error_trace(svn_ra_svn__handle_commands2(conn, pool,
                                                      commands, baton,
                                                      FALSE));
}

svn_error_t *
svn_ra_svn_write_cmd(svn_ra_svn_conn_t *conn,
                     apr_pool_t *pool,
                     const char *cmdname,
                     const char *fmt, ...)
{
  va_list va;
  svn_error_t *err;

  SVN_ERR(svn_ra_svn__start_list(conn, pool));
  SVN_ERR(svn_ra_svn__write_word(conn, pool, cmdname));
  va_start(va, fmt);
  err = svn_ra_svn__write_tuple(conn, pool, fmt, va);
  va_end(va);
  return err ? svn_error_trace(err) : svn_ra_svn__end_list(conn, pool);
}

svn_error_t *
svn_ra_svn_write_cmd_response(svn_ra_svn_conn_t *conn,
                              apr_pool_t *pool,
                              const char *fmt, ...)
{
  va_list va;
  svn_error_t *err;

  va_start(va, fmt);
  err = svn_ra_svn__write_cmd_response(conn, pool, fmt, va);
  va_end(va);

  return svn_error_trace(err);
}


svn_error_t *
svn_ra_svn_write_cmd_failure(svn_ra_svn_conn_t *conn,
                             apr_pool_t *pool,
                             svn_error_t *err)
{
  return svn_error_trace(svn_ra_svn__write_cmd_failure(conn, pool, err));
}
