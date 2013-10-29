/*
 * serve.c :  Functions for serving the Subversion protocol
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




#include <limits.h> /* for UINT_MAX */
#include <stdarg.h>

#define APR_WANT_STRFUNC
#include <apr_want.h>
#include <apr_general.h>
#include <apr_lib.h>
#include <apr_strings.h>

#include "svn_compat.h"
#include "svn_private_config.h"  /* For SVN_PATH_LOCAL_SEPARATOR */
#include "svn_hash.h"
#include "svn_types.h"
#include "svn_string.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_ra.h"              /* for SVN_RA_CAPABILITY_* */
#include "svn_ra_svn.h"
#include "svn_repos.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_time.h"
#include "svn_config.h"
#include "svn_props.h"
#include "svn_mergeinfo.h"
#include "svn_user.h"

#include "private/svn_log.h"
#include "private/svn_mergeinfo_private.h"
#include "private/svn_ra_svn_private.h"
#include "private/svn_fspath.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>   /* For getpid() */
#endif

#include "server.h"

typedef struct commit_callback_baton_t {
  apr_pool_t *pool;
  svn_revnum_t *new_rev;
  const char **date;
  const char **author;
  const char **post_commit_err;
} commit_callback_baton_t;

typedef struct report_driver_baton_t {
  server_baton_t *sb;
  const char *repos_url;  /* Decoded repository URL. */
  void *report_baton;
  svn_error_t *err;
  /* so update() can distinguish checkout from update in logging */
  int entry_counter;
  svn_boolean_t only_empty_entries;
  /* for diff() logging */
  svn_revnum_t *from_rev;
} report_driver_baton_t;

typedef struct log_baton_t {
  const char *fs_path;
  svn_ra_svn_conn_t *conn;
  int stack_depth;
} log_baton_t;

typedef struct file_revs_baton_t {
  svn_ra_svn_conn_t *conn;
  apr_pool_t *pool;  /* Pool provided in the handler call. */
} file_revs_baton_t;

typedef struct fs_warning_baton_t {
  server_baton_t *server;
  svn_ra_svn_conn_t *conn;
  apr_pool_t *pool;
} fs_warning_baton_t;

typedef struct authz_baton_t {
  server_baton_t *server;
  svn_ra_svn_conn_t *conn;
} authz_baton_t;

/* Write LEN bytes of ERRSTR to LOG_FILE with svn_io_file_write(). */
static svn_error_t *
log_write(apr_file_t *log_file, const char *errstr, apr_size_t len,
          apr_pool_t *pool)
{
  return svn_io_file_write(log_file, errstr, &len, pool);
}

void
log_error(svn_error_t *err, apr_file_t *log_file, const char *remote_host,
          const char *user, const char *repos, apr_pool_t *pool)
{
  const char *timestr, *continuation;
  char errbuf[256];
  /* 8192 from MAX_STRING_LEN in from httpd-2.2.4/include/httpd.h */
  char errstr[8192];

  if (err == SVN_NO_ERROR)
    return;

  if (log_file == NULL)
    return;

  timestr = svn_time_to_cstring(apr_time_now(), pool);
  remote_host = (remote_host ? remote_host : "-");
  user = (user ? user : "-");
  repos = (repos ? repos : "-");

  continuation = "";
  while (err != NULL)
    {
      const char *message = svn_err_best_message(err, errbuf, sizeof(errbuf));
      /* based on httpd-2.2.4/server/log.c:log_error_core */
      apr_size_t len = apr_snprintf(errstr, sizeof(errstr),
                                    "%" APR_PID_T_FMT
                                    " %s %s %s %s ERR%s %s %ld %d ",
                                    getpid(), timestr, remote_host, user,
                                    repos, continuation,
                                    err->file ? err->file : "-", err->line,
                                    err->apr_err);

      len += escape_errorlog_item(errstr + len, message,
                                  sizeof(errstr) - len);
      /* Truncate for the terminator (as apr_snprintf does) */
      if (len > sizeof(errstr) - sizeof(APR_EOL_STR)) {
        len = sizeof(errstr) - sizeof(APR_EOL_STR);
      }
      strcpy(errstr + len, APR_EOL_STR);
      len += strlen(APR_EOL_STR);
      svn_error_clear(log_write(log_file, errstr, len, pool));

      continuation = "-";
      err = err->child;
    }
}

/* Call log_error with log_file, remote_host, user, and repos
   arguments from SERVER and CONN. */
static void
log_server_error(svn_error_t *err, server_baton_t *server,
                 svn_ra_svn_conn_t *conn, apr_pool_t *pool)
{
  log_error(err, server->log_file, svn_ra_svn_conn_remote_host(conn),
            server->user, server->repos_name, pool);
}

/* svn_error_create() a new error, log_server_error() it, and
   return it. */
static svn_error_t *
error_create_and_log(apr_status_t apr_err, svn_error_t *child,
                     const char *message, server_baton_t *server,
                     svn_ra_svn_conn_t *conn, apr_pool_t *pool)
{
  svn_error_t *err = svn_error_create(apr_err, child, message);
  log_server_error(err, server, conn, pool);
  return err;
}

/* Log a failure ERR, transmit ERR back to the client (as part of a
   "failure" notification), consume ERR, and flush the connection. */
static svn_error_t *
log_fail_and_flush(svn_error_t *err, server_baton_t *server,
                   svn_ra_svn_conn_t *conn, apr_pool_t *pool)
{
  svn_error_t *io_err;

  log_server_error(err, server, conn, pool);
  io_err = svn_ra_svn__write_cmd_failure(conn, pool, err);
  svn_error_clear(err);
  SVN_ERR(io_err);
  return svn_ra_svn__flush(conn, pool);
}

/* Log a client command. */
static svn_error_t *log_command(server_baton_t *b,
                                svn_ra_svn_conn_t *conn,
                                apr_pool_t *pool,
                                const char *fmt, ...)
{
  const char *remote_host, *timestr, *log, *line;
  va_list ap;
  apr_size_t nbytes;

  if (b->log_file == NULL)
    return SVN_NO_ERROR;

  remote_host = svn_ra_svn_conn_remote_host(conn);
  timestr = svn_time_to_cstring(apr_time_now(), pool);

  va_start(ap, fmt);
  log = apr_pvsprintf(pool, fmt, ap);
  va_end(ap);

  line = apr_psprintf(pool, "%" APR_PID_T_FMT
                      " %s %s %s %s %s" APR_EOL_STR,
                      getpid(), timestr,
                      (remote_host ? remote_host : "-"),
                      (b->user ? b->user : "-"), b->repos_name, log);
  nbytes = strlen(line);

  return log_write(b->log_file, line, nbytes, pool);
}

/* Log an authz failure */
static svn_error_t *
log_authz_denied(const char *path,
                 svn_repos_authz_access_t required,
                 server_baton_t *b,
                 svn_ra_svn_conn_t *conn,
                 apr_pool_t *pool)
{
  const char *timestr, *remote_host, *line;

  if (b->log_file == NULL)
    return SVN_NO_ERROR;

  if (!b->user)
    return SVN_NO_ERROR;

  timestr = svn_time_to_cstring(apr_time_now(), pool);
  remote_host = svn_ra_svn_conn_remote_host(conn);

  line = apr_psprintf(pool, "%" APR_PID_T_FMT
                      " %s %s %s %s Authorization Failed %s%s %s" APR_EOL_STR,
                      getpid(), timestr,
                      (remote_host ? remote_host : "-"),
                      (b->user ? b->user : "-"),
                      b->repos_name,
                      (required & svn_authz_recursive ? "recursive " : ""),
                      (required & svn_authz_write ? "write" : "read"),
                      (path && path[0] ? path : "/"));

  return log_write(b->log_file, line, strlen(line), pool);
}


svn_error_t *load_pwdb_config(server_baton_t *server,
                              svn_ra_svn_conn_t *conn,
                              apr_pool_t *pool)
{
  const char *pwdb_path;
  svn_error_t *err;

  svn_config_get(server->cfg, &pwdb_path, SVN_CONFIG_SECTION_GENERAL,
                 SVN_CONFIG_OPTION_PASSWORD_DB, NULL);

  server->pwdb = NULL;
  if (pwdb_path)
    {
      pwdb_path = svn_dirent_internal_style(pwdb_path, pool);
      pwdb_path = svn_dirent_join(server->base, pwdb_path, pool);

      err = svn_config_read3(&server->pwdb, pwdb_path, TRUE,
                             FALSE, FALSE, pool);
      if (err)
        {
          log_server_error(err, server, conn, pool);

          /* Because it may be possible to read the pwdb file with some
             access methods and not others, ignore errors reading the pwdb
             file and just don't present password authentication as an
             option.  Also, some authentications (e.g. --tunnel) can
             proceed without it anyway.

             ### Not entirely sure why SVN_ERR_BAD_FILENAME is checked
             ### for here.  That seems to have been introduced in r856914,
             ### and only in r870942 was the APR_EACCES check introduced. */
          if (err->apr_err != SVN_ERR_BAD_FILENAME
              && ! APR_STATUS_IS_EACCES(err->apr_err))
            {
                /* Now that we've logged the error, clear it and return a
                 * nice, generic error to the user:
                 * http://subversion.tigris.org/issues/show_bug.cgi?id=2271 */
                svn_error_clear(err);
                return svn_error_create(SVN_ERR_AUTHN_FAILED, NULL, NULL);
            }
          else
            /* Ignore SVN_ERR_BAD_FILENAME and APR_EACCES and proceed. */
            svn_error_clear(err);
        }
    }

  return SVN_NO_ERROR;
}

/* Canonicalize *ACCESS_FILE based on the type of argument.  Results are
 * placed in *ACCESS_FILE.  SERVER baton is used to convert relative paths to
 * absolute paths rooted at the server root.  REPOS_ROOT is used to calculate
 * an absolute URL for repos-relative URLs. */
static svn_error_t *
canonicalize_access_file(const char **access_file, server_baton_t *server,
                         const char *repos_root, apr_pool_t *pool)
{
  if (svn_path_is_url(*access_file))
    {
      *access_file = svn_uri_canonicalize(*access_file, pool);
    }
  else if (svn_path_is_repos_relative_url(*access_file))
    {
      const char *repos_root_url;

      SVN_ERR(svn_uri_get_file_url_from_dirent(&repos_root_url, repos_root,
                                               pool));
      SVN_ERR(svn_path_resolve_repos_relative_url(access_file, *access_file,
                                                  repos_root_url, pool));
      *access_file = svn_uri_canonicalize(*access_file, pool);
    }
  else
    {
      *access_file = svn_dirent_internal_style(*access_file, pool);
      *access_file = svn_dirent_join(server->base, *access_file, pool);
    }

  return SVN_NO_ERROR;
}

svn_error_t *load_authz_config(server_baton_t *server,
                               svn_ra_svn_conn_t *conn,
                               const char *repos_root,
                               apr_pool_t *pool)
{
  const char *authzdb_path;
  const char *groupsdb_path;
  svn_error_t *err;

  /* Read authz configuration. */
  svn_config_get(server->cfg, &authzdb_path, SVN_CONFIG_SECTION_GENERAL,
                 SVN_CONFIG_OPTION_AUTHZ_DB, NULL);

  svn_config_get(server->cfg, &groupsdb_path, SVN_CONFIG_SECTION_GENERAL,
                 SVN_CONFIG_OPTION_GROUPS_DB, NULL);

  if (authzdb_path)
    {
      const char *case_force_val;

      /* Canonicalize and add the base onto the authzdb_path (if needed). */
      err = canonicalize_access_file(&authzdb_path, server,
                                     repos_root, pool);

      /* Same for the groupsdb_path if it is present. */
      if (groupsdb_path && !err)
        err = canonicalize_access_file(&groupsdb_path, server,
                                       repos_root, pool);

      if (!err)
        err = svn_repos_authz_read2(&server->authzdb, authzdb_path,
                                    groupsdb_path, TRUE, pool);

      if (err)
        {
          log_server_error(err, server, conn, pool);
          svn_error_clear(err);
          return svn_error_create(SVN_ERR_AUTHZ_INVALID_CONFIG, NULL, NULL);
        }

      /* Are we going to be case-normalizing usernames when we consult
       * this authz file? */
      svn_config_get(server->cfg, &case_force_val, SVN_CONFIG_SECTION_GENERAL,
                     SVN_CONFIG_OPTION_FORCE_USERNAME_CASE, NULL);
      if (case_force_val)
        {
          if (strcmp(case_force_val, "upper") == 0)
            server->username_case = CASE_FORCE_UPPER;
          else if (strcmp(case_force_val, "lower") == 0)
            server->username_case = CASE_FORCE_LOWER;
          else
            server->username_case = CASE_ASIS;
        }
    }
  else
    {
      server->authzdb = NULL;
      server->username_case = CASE_ASIS;
    }

  return SVN_NO_ERROR;
}

/* Set *FS_PATH to the portion of URL that is the path within the
   repository, if URL is inside REPOS_URL (if URL is not inside
   REPOS_URL, then error, with the effect on *FS_PATH undefined).

   If the resultant fs path would be the empty string (i.e., URL and
   REPOS_URL are the same), then set *FS_PATH to "/".

   Assume that REPOS_URL and URL are already URI-decoded. */
static svn_error_t *get_fs_path(const char *repos_url, const char *url,
                                const char **fs_path)
{
  apr_size_t len;

  len = strlen(repos_url);
  if (strncmp(url, repos_url, len) != 0)
    return svn_error_createf(SVN_ERR_RA_ILLEGAL_URL, NULL,
                             "'%s' is not the same repository as '%s'",
                             url, repos_url);
  *fs_path = url + len;
  if (! **fs_path)
    *fs_path = "/";

  return SVN_NO_ERROR;
}

/* --- AUTHENTICATION AND AUTHORIZATION FUNCTIONS --- */

/* Convert TEXT to upper case if TO_UPPERCASE is TRUE, else
   converts it to lower case. */
static void convert_case(char *text, svn_boolean_t to_uppercase)
{
  char *c = text;
  while (*c)
    {
      *c = (char)(to_uppercase ? apr_toupper(*c) : apr_tolower(*c));
      ++c;
    }
}

/* Set *ALLOWED to TRUE if PATH is accessible in the REQUIRED mode to
   the user described in BATON according to the authz rules in BATON.
   Use POOL for temporary allocations only.  If no authz rules are
   present in BATON, grant access by default. */
static svn_error_t *authz_check_access(svn_boolean_t *allowed,
                                       const char *path,
                                       svn_repos_authz_access_t required,
                                       server_baton_t *b,
                                       svn_ra_svn_conn_t *conn,
                                       apr_pool_t *pool)
{
  /* If authz cannot be performed, grant access.  This is NOT the same
     as the default policy when authz is performed on a path with no
     rules.  In the latter case, the default is to deny access, and is
     set by svn_repos_authz_check_access. */
  if (!b->authzdb)
    {
      *allowed = TRUE;
      return SVN_NO_ERROR;
    }

  /* If the authz request is for the empty path (ie. ""), replace it
     with the root path.  This happens because of stripping done at
     various levels in svnserve that remove the leading / on an
     absolute path. Passing such a malformed path to the authz
     routines throws them into an infinite loop and makes them miss
     ACLs. */
  if (path)
    path = svn_fspath__canonicalize(path, pool);

  /* If we have a username, and we've not yet used it + any username
     case normalization that might be requested to determine "the
     username we used for authz purposes", do so now. */
  if (b->user && (! b->authz_user))
    {
      char *authz_user = apr_pstrdup(b->pool, b->user);
      if (b->username_case == CASE_FORCE_UPPER)
        convert_case(authz_user, TRUE);
      else if (b->username_case == CASE_FORCE_LOWER)
        convert_case(authz_user, FALSE);
      b->authz_user = authz_user;
    }

  SVN_ERR(svn_repos_authz_check_access(b->authzdb, b->authz_repos_name,
                                       path, b->authz_user, required,
                                       allowed, pool));
  if (!*allowed)
    SVN_ERR(log_authz_denied(path, required, b, conn, pool));

  return SVN_NO_ERROR;
}

/* Set *ALLOWED to TRUE if PATH is readable by the user described in
 * BATON.  Use POOL for temporary allocations only.  ROOT is not used.
 * Implements the svn_repos_authz_func_t interface.
 */
static svn_error_t *authz_check_access_cb(svn_boolean_t *allowed,
                                          svn_fs_root_t *root,
                                          const char *path,
                                          void *baton,
                                          apr_pool_t *pool)
{
  authz_baton_t *sb = baton;

  return authz_check_access(allowed, path, svn_authz_read,
                            sb->server, sb->conn, pool);
}

/* If authz is enabled in the specified BATON, return a read authorization
   function. Otherwise, return NULL. */
static svn_repos_authz_func_t authz_check_access_cb_func(server_baton_t *baton)
{
  if (baton->authzdb)
     return authz_check_access_cb;
  return NULL;
}

/* Set *ALLOWED to TRUE if the REQUIRED access to PATH is granted,
 * according to the state in BATON.  Use POOL for temporary
 * allocations only.  ROOT is not used.  Implements the
 * svn_repos_authz_callback_t interface.
 */
static svn_error_t *authz_commit_cb(svn_repos_authz_access_t required,
                                    svn_boolean_t *allowed,
                                    svn_fs_root_t *root,
                                    const char *path,
                                    void *baton,
                                    apr_pool_t *pool)
{
  authz_baton_t *sb = baton;

  return authz_check_access(allowed, path, required,
                            sb->server, sb->conn, pool);
}


enum access_type get_access(server_baton_t *b, enum authn_type auth)
{
  const char *var = (auth == AUTHENTICATED) ? SVN_CONFIG_OPTION_AUTH_ACCESS :
    SVN_CONFIG_OPTION_ANON_ACCESS;
  const char *val, *def = (auth == AUTHENTICATED) ? "write" : "read";
  enum access_type result;

  svn_config_get(b->cfg, &val, SVN_CONFIG_SECTION_GENERAL, var, def);
  result = (strcmp(val, "write") == 0 ? WRITE_ACCESS :
            strcmp(val, "read") == 0 ? READ_ACCESS : NO_ACCESS);
  return (result == WRITE_ACCESS && b->read_only) ? READ_ACCESS : result;
}

static enum access_type current_access(server_baton_t *b)
{
  return get_access(b, (b->user) ? AUTHENTICATED : UNAUTHENTICATED);
}

/* Send authentication mechs for ACCESS_TYPE to the client.  If NEEDS_USERNAME
   is true, don't send anonymous mech even if that would give the desired
   access. */
static svn_error_t *send_mechs(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                               server_baton_t *b, enum access_type required,
                               svn_boolean_t needs_username)
{
  if (!needs_username && get_access(b, UNAUTHENTICATED) >= required)
    SVN_ERR(svn_ra_svn__write_word(conn, pool, "ANONYMOUS"));
  if (b->tunnel_user && get_access(b, AUTHENTICATED) >= required)
    SVN_ERR(svn_ra_svn__write_word(conn, pool, "EXTERNAL"));
  if (b->pwdb && get_access(b, AUTHENTICATED) >= required)
    SVN_ERR(svn_ra_svn__write_word(conn, pool, "CRAM-MD5"));
  return SVN_NO_ERROR;
}

/* Context for cleanup handler. */
struct cleanup_fs_access_baton
{
  svn_fs_t *fs;
  apr_pool_t *pool;
};

/* Pool cleanup handler.  Make sure fs's access_t points to NULL when
   the command pool is destroyed. */
static apr_status_t cleanup_fs_access(void *data)
{
  svn_error_t *serr;
  struct cleanup_fs_access_baton *baton = data;

  serr = svn_fs_set_access(baton->fs, NULL);
  if (serr)
    {
      apr_status_t apr_err = serr->apr_err;
      svn_error_clear(serr);
      return apr_err;
    }

  return APR_SUCCESS;
}


/* Create an svn_fs_access_t in POOL for USER and associate it with
   B's filesystem.  Also, register a cleanup handler with POOL which
   de-associates the svn_fs_access_t from B's filesystem. */
static svn_error_t *
create_fs_access(server_baton_t *b, apr_pool_t *pool)
{
  svn_fs_access_t *fs_access;
  struct cleanup_fs_access_baton *cleanup_baton;

  if (!b->user)
    return SVN_NO_ERROR;

  SVN_ERR(svn_fs_create_access(&fs_access, b->user, pool));
  SVN_ERR(svn_fs_set_access(b->fs, fs_access));

  cleanup_baton = apr_pcalloc(pool, sizeof(*cleanup_baton));
  cleanup_baton->pool = pool;
  cleanup_baton->fs = b->fs;
  apr_pool_cleanup_register(pool, cleanup_baton, cleanup_fs_access,
                            apr_pool_cleanup_null);

  return SVN_NO_ERROR;
}

/* Authenticate, once the client has chosen a mechanism and possibly
 * sent an initial mechanism token.  On success, set *success to true
 * and b->user to the authenticated username (or NULL for anonymous).
 * On authentication failure, report failure to the client and set
 * *success to FALSE.  On communications failure, return an error.
 * If NEEDS_USERNAME is TRUE, don't allow anonymous authentication. */
static svn_error_t *auth(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                         const char *mech, const char *mecharg,
                         server_baton_t *b, enum access_type required,
                         svn_boolean_t needs_username,
                         svn_boolean_t *success)
{
  const char *user;
  *success = FALSE;

  if (get_access(b, AUTHENTICATED) >= required
      && b->tunnel_user && strcmp(mech, "EXTERNAL") == 0)
    {
      if (*mecharg && strcmp(mecharg, b->tunnel_user) != 0)
        return svn_ra_svn__write_tuple(conn, pool, "w(c)", "failure",
                                       "Requested username does not match");
      b->user = b->tunnel_user;
      SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "w()", "success"));
      *success = TRUE;
      return SVN_NO_ERROR;
    }

  if (get_access(b, UNAUTHENTICATED) >= required
      && strcmp(mech, "ANONYMOUS") == 0 && ! needs_username)
    {
      SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "w()", "success"));
      *success = TRUE;
      return SVN_NO_ERROR;
    }

  if (get_access(b, AUTHENTICATED) >= required
      && b->pwdb && strcmp(mech, "CRAM-MD5") == 0)
    {
      SVN_ERR(svn_ra_svn_cram_server(conn, pool, b->pwdb, &user, success));
      b->user = apr_pstrdup(b->pool, user);
      return SVN_NO_ERROR;
    }

  return svn_ra_svn__write_tuple(conn, pool, "w(c)", "failure",
                                "Must authenticate with listed mechanism");
}

/* Perform an authentication request using the built-in SASL implementation. */
static svn_error_t *
internal_auth_request(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                      server_baton_t *b, enum access_type required,
                      svn_boolean_t needs_username)
{
  svn_boolean_t success;
  const char *mech, *mecharg;

  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "w((!", "success"));
  SVN_ERR(send_mechs(conn, pool, b, required, needs_username));
  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "!)c)", b->realm));
  do
    {
      SVN_ERR(svn_ra_svn__read_tuple(conn, pool, "w(?c)", &mech, &mecharg));
      if (!*mech)
        break;
      SVN_ERR(auth(conn, pool, mech, mecharg, b, required, needs_username,
                   &success));
    }
  while (!success);
  return SVN_NO_ERROR;
}

/* Perform an authentication request in order to get an access level of
 * REQUIRED or higher.  Since the client may escape the authentication
 * exchange, the caller should check current_access(b) to see if
 * authentication succeeded. */
static svn_error_t *auth_request(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                 server_baton_t *b, enum access_type required,
                                 svn_boolean_t needs_username)
{
#ifdef SVN_HAVE_SASL
  if (b->use_sasl)
    return cyrus_auth_request(conn, pool, b, required, needs_username);
#endif

  return internal_auth_request(conn, pool, b, required, needs_username);
}

/* Send a trivial auth notification on CONN which lists no mechanisms,
 * indicating that authentication is unnecessary.  Usually called in
 * response to invocation of a svnserve command.
 */
static svn_error_t *trivial_auth_request(svn_ra_svn_conn_t *conn,
                                         apr_pool_t *pool, server_baton_t *b)
{
  return svn_ra_svn__write_cmd_response(conn, pool, "()c", "");
}

/* Ensure that the client has the REQUIRED access by checking the
 * access directives (both blanket and per-directory) in BATON.  If
 * PATH is NULL, then only the blanket access configuration will
 * impact the result.
 *
 * If NEEDS_USERNAME is TRUE, then a lookup is only successful if the
 * user described in BATON is authenticated and, well, has a username
 * assigned to him.
 *
 * Use POOL for temporary allocations only.
 */
static svn_boolean_t lookup_access(apr_pool_t *pool,
                                   server_baton_t *baton,
                                   svn_ra_svn_conn_t *conn,
                                   svn_repos_authz_access_t required,
                                   const char *path,
                                   svn_boolean_t needs_username)
{
  enum access_type req = (required & svn_authz_write) ?
    WRITE_ACCESS : READ_ACCESS;
  svn_boolean_t authorized;
  svn_error_t *err;

  /* Get authz's opinion on the access. */
  err = authz_check_access(&authorized, path, required, baton, conn, pool);

  /* If an error made lookup fail, deny access. */
  if (err)
    {
      log_server_error(err, baton, conn, pool);
      svn_error_clear(err);
      return FALSE;
    }

  /* If the required access is blanket-granted AND granted by authz
     AND we already have a username if one is required, then the
     lookup has succeeded. */
  if (current_access(baton) >= req
      && authorized
      && (! needs_username || baton->user))
    return TRUE;

  return FALSE;
}

/* Check that the client has the REQUIRED access by consulting the
 * authentication and authorization states stored in BATON.  If the
 * client does not have the required access credentials, attempt to
 * authenticate the client to get that access, using CONN for
 * communication.
 *
 * This function is supposed to be called to handle the authentication
 * half of a standard svn protocol reply.  If an error is returned, it
 * probably means that the server can terminate the client connection
 * with an apologetic error, as it implies an authentication failure.
 *
 * PATH and NEEDS_USERNAME are passed along to lookup_access, their
 * behaviour is documented there.
 */
static svn_error_t *must_have_access(svn_ra_svn_conn_t *conn,
                                     apr_pool_t *pool,
                                     server_baton_t *b,
                                     svn_repos_authz_access_t required,
                                     const char *path,
                                     svn_boolean_t needs_username)
{
  enum access_type req = (required & svn_authz_write) ?
    WRITE_ACCESS : READ_ACCESS;

  /* See whether the user already has the required access.  If so,
     nothing needs to be done.  Create the FS access and send a
     trivial auth request. */
  if (lookup_access(pool, b, conn, required, path, needs_username))
    {
      SVN_ERR(create_fs_access(b, pool));
      return trivial_auth_request(conn, pool, b);
    }

  /* If the required blanket access can be obtained by authenticating,
     try that.  Unfortunately, we can't tell until after
     authentication whether authz will work or not.  We force
     requiring a username because we need one to be able to check
     authz configuration again with a different user credentials than
     the first time round. */
  if (b->user == NULL
      && get_access(b, AUTHENTICATED) >= req
      && (b->tunnel_user || b->pwdb || b->use_sasl))
    SVN_ERR(auth_request(conn, pool, b, req, TRUE));

  /* Now that an authentication has been done get the new take of
     authz on the request. */
  if (! lookup_access(pool, b, conn, required, path, needs_username))
    return svn_error_create(SVN_ERR_RA_SVN_CMD_ERR,
                            error_create_and_log(SVN_ERR_RA_NOT_AUTHORIZED,
                                                 NULL, NULL, b, conn, pool),
                            NULL);

  /* Else, access is granted, and there is much rejoicing. */
  SVN_ERR(create_fs_access(b, pool));

  return SVN_NO_ERROR;
}

/* --- REPORTER COMMAND SET --- */

/* To allow for pipelining, reporter commands have no reponses.  If we
 * get an error, we ignore all subsequent reporter commands and return
 * the error finish_report, to be handled by the calling command.
 */

static svn_error_t *set_path(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                             apr_array_header_t *params, void *baton)
{
  report_driver_baton_t *b = baton;
  const char *path, *lock_token, *depth_word;
  svn_revnum_t rev;
  /* Default to infinity, for old clients that don't send depth. */
  svn_depth_t depth = svn_depth_infinity;
  svn_boolean_t start_empty;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "crb?(?c)?w",
                                 &path, &rev, &start_empty, &lock_token,
                                 &depth_word));
  if (depth_word)
    depth = svn_depth_from_word(depth_word);
  path = svn_relpath_canonicalize(path, pool);
  if (b->from_rev && strcmp(path, "") == 0)
    *b->from_rev = rev;
  if (!b->err)
    b->err = svn_repos_set_path3(b->report_baton, path, rev, depth,
                                 start_empty, lock_token, pool);
  b->entry_counter++;
  if (!start_empty)
    b->only_empty_entries = FALSE;
  return SVN_NO_ERROR;
}

static svn_error_t *delete_path(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                apr_array_header_t *params, void *baton)
{
  report_driver_baton_t *b = baton;
  const char *path;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "c", &path));
  path = svn_relpath_canonicalize(path, pool);
  if (!b->err)
    b->err = svn_repos_delete_path(b->report_baton, path, pool);
  return SVN_NO_ERROR;
}

static svn_error_t *link_path(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                              apr_array_header_t *params, void *baton)
{
  report_driver_baton_t *b = baton;
  const char *path, *url, *lock_token, *fs_path, *depth_word;
  svn_revnum_t rev;
  svn_boolean_t start_empty;
  /* Default to infinity, for old clients that don't send depth. */
  svn_depth_t depth = svn_depth_infinity;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "ccrb?(?c)?w",
                                 &path, &url, &rev, &start_empty,
                                 &lock_token, &depth_word));

  /* ### WHAT?!  The link path is an absolute URL?!  Didn't see that
     coming...   -- cmpilato  */
  path = svn_relpath_canonicalize(path, pool);
  url = svn_uri_canonicalize(url, pool);
  if (depth_word)
    depth = svn_depth_from_word(depth_word);
  if (!b->err)
    b->err = get_fs_path(svn_path_uri_decode(b->repos_url, pool),
                         svn_path_uri_decode(url, pool),
                         &fs_path);
  if (!b->err)
    b->err = svn_repos_link_path3(b->report_baton, path, fs_path, rev,
                                  depth, start_empty, lock_token, pool);
  b->entry_counter++;
  return SVN_NO_ERROR;
}

static svn_error_t *finish_report(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                  apr_array_header_t *params, void *baton)
{
  report_driver_baton_t *b = baton;

  /* No arguments to parse. */
  SVN_ERR(trivial_auth_request(conn, pool, b->sb));
  if (!b->err)
    b->err = svn_repos_finish_report(b->report_baton, pool);
  return SVN_NO_ERROR;
}

static svn_error_t *abort_report(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                 apr_array_header_t *params, void *baton)
{
  report_driver_baton_t *b = baton;

  /* No arguments to parse. */
  svn_error_clear(svn_repos_abort_report(b->report_baton, pool));
  return SVN_NO_ERROR;
}

static const svn_ra_svn_cmd_entry_t report_commands[] = {
  { "set-path",      set_path },
  { "delete-path",   delete_path },
  { "link-path",     link_path },
  { "finish-report", finish_report, TRUE },
  { "abort-report",  abort_report,  TRUE },
  { NULL }
};

/* Accept a report from the client, drive the network editor with the
 * result, and then write an empty command response.  If there is a
 * non-protocol failure, accept_report will abort the edit and return
 * a command error to be reported by handle_commands().
 *
 * If only_empty_entry is not NULL and the report contains only one
 * item, and that item is empty, set *only_empty_entry to TRUE, else
 * set it to FALSE.
 *
 * If from_rev is not NULL, set *from_rev to the revision number from
 * the set-path on ""; if somehow set-path "" never happens, set
 * *from_rev to SVN_INVALID_REVNUM.
 */
static svn_error_t *accept_report(svn_boolean_t *only_empty_entry,
                                  svn_revnum_t *from_rev,
                                  svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                  server_baton_t *b, svn_revnum_t rev,
                                  const char *target, const char *tgt_path,
                                  svn_boolean_t text_deltas,
                                  svn_depth_t depth,
                                  svn_boolean_t send_copyfrom_args,
                                  svn_boolean_t ignore_ancestry)
{
  const svn_delta_editor_t *editor;
  void *edit_baton, *report_baton;
  report_driver_baton_t rb;
  svn_error_t *err;
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  /* Make an svn_repos report baton.  Tell it to drive the network editor
   * when the report is complete. */
  svn_ra_svn_get_editor(&editor, &edit_baton, conn, pool, NULL, NULL);
  SVN_CMD_ERR(svn_repos_begin_report3(&report_baton, rev, b->repos,
                                      b->fs_path->data, target, tgt_path,
                                      text_deltas, depth, ignore_ancestry,
                                      send_copyfrom_args,
                                      editor, edit_baton,
                                      authz_check_access_cb_func(b),
                                      &ab, svn_ra_svn_zero_copy_limit(conn),
                                      pool));

  rb.sb = b;
  rb.repos_url = svn_path_uri_decode(b->repos_url, pool);
  rb.report_baton = report_baton;
  rb.err = NULL;
  rb.entry_counter = 0;
  rb.only_empty_entries = TRUE;
  rb.from_rev = from_rev;
  if (from_rev)
    *from_rev = SVN_INVALID_REVNUM;
  err = svn_ra_svn__handle_commands2(conn, pool, report_commands, &rb, TRUE);
  if (err)
    {
      /* Network or protocol error while handling commands. */
      svn_error_clear(rb.err);
      return err;
    }
  else if (rb.err)
    {
      /* Some failure during the reporting or editing operations. */
      SVN_CMD_ERR(rb.err);
    }
  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, ""));

  if (only_empty_entry)
    *only_empty_entry = rb.entry_counter == 1 && rb.only_empty_entries;

  return SVN_NO_ERROR;
}

/* --- MAIN COMMAND SET --- */

/* Write out a list of property diffs.  PROPDIFFS is an array of svn_prop_t
 * values. */
static svn_error_t *write_prop_diffs(svn_ra_svn_conn_t *conn,
                                     apr_pool_t *pool,
                                     const apr_array_header_t *propdiffs)
{
  int i;

  for (i = 0; i < propdiffs->nelts; ++i)
    {
      const svn_prop_t *prop = &APR_ARRAY_IDX(propdiffs, i, svn_prop_t);

      SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "c(?s)",
                                      prop->name, prop->value));
    }

  return SVN_NO_ERROR;
}

/* Write out a lock to the client. */
static svn_error_t *write_lock(svn_ra_svn_conn_t *conn,
                               apr_pool_t *pool,
                               svn_lock_t *lock)
{
  const char *cdate, *edate;

  cdate = svn_time_to_cstring(lock->creation_date, pool);
  edate = lock->expiration_date
    ? svn_time_to_cstring(lock->expiration_date, pool) : NULL;
  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "ccc(?c)c(?c)", lock->path,
                                  lock->token, lock->owner, lock->comment,
                                  cdate, edate));

  return SVN_NO_ERROR;
}

/* ### This really belongs in libsvn_repos. */
/* Get the explicit properties and/or inherited properties for a PATH in
   ROOT, with hardcoded committed-info values. */
static svn_error_t *
get_props(apr_hash_t **props,
          apr_array_header_t **iprops,
          authz_baton_t *b,
          svn_fs_root_t *root,
          const char *path,
          apr_pool_t *pool)
{
  /* Get the explicit properties. */
  if (props)
    {
      svn_string_t *str;
      svn_revnum_t crev;
      const char *cdate, *cauthor, *uuid;

      SVN_ERR(svn_fs_node_proplist(props, root, path, pool));

      /* Hardcode the values for the committed revision, date, and author. */
      SVN_ERR(svn_repos_get_committed_info(&crev, &cdate, &cauthor, root,
                                           path, pool));
      str = svn_string_create(apr_psprintf(pool, "%ld", crev),
                              pool);
      svn_hash_sets(*props, SVN_PROP_ENTRY_COMMITTED_REV, str);
      str = (cdate) ? svn_string_create(cdate, pool) : NULL;
      svn_hash_sets(*props, SVN_PROP_ENTRY_COMMITTED_DATE, str);
      str = (cauthor) ? svn_string_create(cauthor, pool) : NULL;
      svn_hash_sets(*props, SVN_PROP_ENTRY_LAST_AUTHOR, str);

      /* Hardcode the values for the UUID. */
      SVN_ERR(svn_fs_get_uuid(svn_fs_root_fs(root), &uuid, pool));
      str = (uuid) ? svn_string_create(uuid, pool) : NULL;
      svn_hash_sets(*props, SVN_PROP_ENTRY_UUID, str);
    }

  /* Get any inherited properties the user is authorized to. */
  if (iprops)
    {
      SVN_ERR(svn_repos_fs_get_inherited_props(
                iprops, root, path, NULL,
                authz_check_access_cb_func(b->server),
                b, pool, pool));
    }

  return SVN_NO_ERROR;
}

/* Set BATON->FS_PATH for the repository URL found in PARAMS. */
static svn_error_t *reparent(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                             apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  const char *url;
  const char *fs_path;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "c", &url));
  url = svn_uri_canonicalize(url, pool);
  SVN_ERR(trivial_auth_request(conn, pool, b));
  SVN_CMD_ERR(get_fs_path(svn_path_uri_decode(b->repos_url, pool),
                          svn_path_uri_decode(url, pool),
                          &fs_path));
  SVN_ERR(log_command(b, conn, pool, "%s", svn_log__reparent(fs_path, pool)));
  svn_stringbuf_set(b->fs_path, fs_path);
  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, ""));
  return SVN_NO_ERROR;
}

static svn_error_t *get_latest_rev(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                   apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  svn_revnum_t rev;

  SVN_ERR(log_command(b, conn, pool, "get-latest-rev"));

  SVN_ERR(trivial_auth_request(conn, pool, b));
  SVN_CMD_ERR(svn_fs_youngest_rev(&rev, b->fs, pool));
  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, "r", rev));
  return SVN_NO_ERROR;
}

static svn_error_t *get_dated_rev(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                  apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  svn_revnum_t rev;
  apr_time_t tm;
  const char *timestr;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "c", &timestr));
  SVN_ERR(log_command(b, conn, pool, "get-dated-rev %s", timestr));

  SVN_ERR(trivial_auth_request(conn, pool, b));
  SVN_CMD_ERR(svn_time_from_cstring(&tm, timestr, pool));
  SVN_CMD_ERR(svn_repos_dated_revision(&rev, b->repos, tm, pool));
  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, "r", rev));
  return SVN_NO_ERROR;
}

/* Common logic for change_rev_prop() and change_rev_prop2(). */
static svn_error_t *do_change_rev_prop(svn_ra_svn_conn_t *conn,
                                       server_baton_t *b,
                                       svn_revnum_t rev,
                                       const char *name,
                                       const svn_string_t *const *old_value_p,
                                       const svn_string_t *value,
                                       apr_pool_t *pool)
{
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  SVN_ERR(must_have_access(conn, pool, b, svn_authz_write, NULL, FALSE));
  SVN_ERR(log_command(b, conn, pool, "%s",
                      svn_log__change_rev_prop(rev, name, pool)));
  SVN_CMD_ERR(svn_repos_fs_change_rev_prop4(b->repos, rev, b->user,
                                            name, old_value_p, value,
                                            TRUE, TRUE,
                                            authz_check_access_cb_func(b), &ab,
                                            pool));
  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, ""));

  return SVN_NO_ERROR;
}

static svn_error_t *change_rev_prop2(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                     apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  svn_revnum_t rev;
  const char *name;
  svn_string_t *value;
  const svn_string_t *const *old_value_p;
  svn_string_t *old_value;
  svn_boolean_t dont_care;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "rc(?s)(b?s)",
                                  &rev, &name, &value,
                                  &dont_care, &old_value));

  /* Argument parsing. */
  if (dont_care)
    old_value_p = NULL;
  else
    old_value_p = (const svn_string_t *const *)&old_value;

  /* Input validation. */
  if (dont_care && old_value)
    {
      svn_error_t *err;
      err = svn_error_create(SVN_ERR_INCORRECT_PARAMS, NULL,
                             "'previous-value' and 'dont-care' cannot both be "
                             "set in 'change-rev-prop2' request");
      return log_fail_and_flush(err, b, conn, pool);
    }

  /* Do it. */
  SVN_ERR(do_change_rev_prop(conn, b, rev, name, old_value_p, value, pool));

  return SVN_NO_ERROR;
}

static svn_error_t *change_rev_prop(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                    apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  svn_revnum_t rev;
  const char *name;
  svn_string_t *value;

  /* Because the revprop value was at one time mandatory, the usual
     optional element pattern "(?s)" isn't used. */
  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "rc?s", &rev, &name, &value));

  SVN_ERR(do_change_rev_prop(conn, b, rev, name, NULL, value, pool));

  return SVN_NO_ERROR;
}

static svn_error_t *rev_proplist(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                 apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  svn_revnum_t rev;
  apr_hash_t *props;
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "r", &rev));
  SVN_ERR(log_command(b, conn, pool, "%s", svn_log__rev_proplist(rev, pool)));

  SVN_ERR(trivial_auth_request(conn, pool, b));
  SVN_CMD_ERR(svn_repos_fs_revision_proplist(&props, b->repos, rev,
                                             authz_check_access_cb_func(b), &ab,
                                             pool));
  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "w((!", "success"));
  SVN_ERR(svn_ra_svn__write_proplist(conn, pool, props));
  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "!))"));
  return SVN_NO_ERROR;
}

static svn_error_t *rev_prop(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                             apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  svn_revnum_t rev;
  const char *name;
  svn_string_t *value;
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "rc", &rev, &name));
  SVN_ERR(log_command(b, conn, pool, "%s",
                      svn_log__rev_prop(rev, name, pool)));

  SVN_ERR(trivial_auth_request(conn, pool, b));
  SVN_CMD_ERR(svn_repos_fs_revision_prop(&value, b->repos, rev, name,
                                         authz_check_access_cb_func(b), &ab,
                                         pool));
  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, "(?s)", value));
  return SVN_NO_ERROR;
}

static svn_error_t *commit_done(const svn_commit_info_t *commit_info,
                                void *baton, apr_pool_t *pool)
{
  commit_callback_baton_t *ccb = baton;

  *ccb->new_rev = commit_info->revision;
  *ccb->date = commit_info->date
    ? apr_pstrdup(ccb->pool, commit_info->date): NULL;
  *ccb->author = commit_info->author
    ? apr_pstrdup(ccb->pool, commit_info->author) : NULL;
  *ccb->post_commit_err = commit_info->post_commit_err
    ? apr_pstrdup(ccb->pool, commit_info->post_commit_err) : NULL;
  return SVN_NO_ERROR;
}

/* Add the LOCK_TOKENS (if any) to the filesystem access context,
 * checking path authorizations using the state in SB as we go.
 * LOCK_TOKENS is an array of svn_ra_svn_item_t structs.  Return a
 * client error if LOCK_TOKENS is not a list of lists.  If a lock
 * violates the authz configuration, return SVN_ERR_RA_NOT_AUTHORIZED
 * to the client.  Use POOL for temporary allocations only.
 */
static svn_error_t *add_lock_tokens(svn_ra_svn_conn_t *conn,
                                    const apr_array_header_t *lock_tokens,
                                    server_baton_t *sb,
                                    apr_pool_t *pool)
{
  int i;
  svn_fs_access_t *fs_access;

  SVN_ERR(svn_fs_get_access(&fs_access, sb->fs));

  /* If there is no access context, nowhere to add the tokens. */
  if (! fs_access)
    return SVN_NO_ERROR;

  for (i = 0; i < lock_tokens->nelts; ++i)
    {
      const char *path, *token, *full_path;
      svn_ra_svn_item_t *path_item, *token_item;
      svn_ra_svn_item_t *item = &APR_ARRAY_IDX(lock_tokens, i,
                                               svn_ra_svn_item_t);
      if (item->kind != SVN_RA_SVN_LIST)
        return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                "Lock tokens aren't a list of lists");

      path_item = &APR_ARRAY_IDX(item->u.list, 0, svn_ra_svn_item_t);
      if (path_item->kind != SVN_RA_SVN_STRING)
        return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                "Lock path isn't a string");

      token_item = &APR_ARRAY_IDX(item->u.list, 1, svn_ra_svn_item_t);
      if (token_item->kind != SVN_RA_SVN_STRING)
        return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                "Lock token isn't a string");

      path = path_item->u.string->data;
      full_path = svn_fspath__join(sb->fs_path->data,
                                   svn_relpath_canonicalize(path, pool),
                                   pool);

      if (! lookup_access(pool, sb, conn, svn_authz_write,
                          full_path, TRUE))
        return error_create_and_log(SVN_ERR_RA_NOT_AUTHORIZED, NULL, NULL,
                                    sb, conn, pool);

      token = token_item->u.string->data;
      SVN_ERR(svn_fs_access_add_lock_token2(fs_access, path, token));
    }

  return SVN_NO_ERROR;
}

/* Unlock the paths with lock tokens in LOCK_TOKENS, ignoring any errors.
   LOCK_TOKENS contains svn_ra_svn_item_t elements, assumed to be lists. */
static svn_error_t *unlock_paths(const apr_array_header_t *lock_tokens,
                                 server_baton_t *sb,
                                 svn_ra_svn_conn_t *conn,
                                 apr_pool_t *pool)
{
  int i;
  apr_pool_t *iterpool;

  iterpool = svn_pool_create(pool);

  for (i = 0; i < lock_tokens->nelts; ++i)
    {
      svn_ra_svn_item_t *item, *path_item, *token_item;
      const char *path, *token, *full_path;
      svn_error_t *err;
      svn_pool_clear(iterpool);

      item = &APR_ARRAY_IDX(lock_tokens, i, svn_ra_svn_item_t);
      path_item = &APR_ARRAY_IDX(item->u.list, 0, svn_ra_svn_item_t);
      token_item = &APR_ARRAY_IDX(item->u.list, 1, svn_ra_svn_item_t);

      path = path_item->u.string->data;
      token = token_item->u.string->data;

      full_path = svn_fspath__join(sb->fs_path->data,
                                   svn_relpath_canonicalize(path, iterpool),
                                   iterpool);

      /* The lock may have become defunct after the commit, so ignore such
         errors. */
      err = svn_repos_fs_unlock(sb->repos, full_path, token,
                                FALSE, iterpool);
      log_server_error(err, sb, conn, iterpool);
      svn_error_clear(err);
    }

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

static svn_error_t *commit(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                           apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  const char *log_msg = NULL,
             *date = NULL,
             *author = NULL,
             *post_commit_err = NULL;
  apr_array_header_t *lock_tokens;
  svn_boolean_t keep_locks;
  apr_array_header_t *revprop_list = NULL;
  apr_hash_t *revprop_table;
  const svn_delta_editor_t *editor;
  void *edit_baton;
  svn_boolean_t aborted;
  commit_callback_baton_t ccb;
  svn_revnum_t new_rev;
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  if (params->nelts == 1)
    {
      /* Clients before 1.2 don't send lock-tokens, keep-locks,
         and rev-props fields. */
      SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "c", &log_msg));
      lock_tokens = NULL;
      keep_locks = TRUE;
      revprop_list = NULL;
    }
  else
    {
      /* Clients before 1.5 don't send the rev-props field. */
      SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "clb?l", &log_msg,
                                      &lock_tokens, &keep_locks,
                                      &revprop_list));
    }

  /* The handling for locks is a little problematic, because the
     protocol won't let us send several auth requests once one has
     succeeded.  So we request write access and a username before
     adding tokens (if we have any), and subsequently fail if a lock
     violates authz. */
  SVN_ERR(must_have_access(conn, pool, b, svn_authz_write,
                           NULL,
                           (lock_tokens && lock_tokens->nelts)));

  /* Authorize the lock tokens and give them to the FS if we got
     any. */
  if (lock_tokens && lock_tokens->nelts)
    SVN_CMD_ERR(add_lock_tokens(conn, lock_tokens, b, pool));

  /* Ignore LOG_MSG, per the protocol.  See ra_svn_commit(). */
  if (revprop_list)
    SVN_ERR(svn_ra_svn__parse_proplist(revprop_list, pool, &revprop_table));
  else
    {
      revprop_table = apr_hash_make(pool);
      svn_hash_sets(revprop_table, SVN_PROP_REVISION_LOG,
                    svn_string_create(log_msg, pool));
    }

  /* Get author from the baton, making sure clients can't circumvent
     the authentication via the revision props. */
  svn_hash_sets(revprop_table, SVN_PROP_REVISION_AUTHOR,
                b->user ? svn_string_create(b->user, pool) : NULL);

  ccb.pool = pool;
  ccb.new_rev = &new_rev;
  ccb.date = &date;
  ccb.author = &author;
  ccb.post_commit_err = &post_commit_err;
  /* ### Note that svn_repos_get_commit_editor5 actually wants a decoded URL. */
  SVN_CMD_ERR(svn_repos_get_commit_editor5
              (&editor, &edit_baton, b->repos, NULL,
               svn_path_uri_decode(b->repos_url, pool),
               b->fs_path->data, revprop_table,
               commit_done, &ccb,
               authz_commit_cb, &ab, pool));
  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, ""));
  SVN_ERR(svn_ra_svn_drive_editor2(conn, pool, editor, edit_baton,
                                   &aborted, FALSE));
  if (!aborted)
    {
      SVN_ERR(log_command(b, conn, pool, "%s",
                          svn_log__commit(new_rev, pool)));
      SVN_ERR(trivial_auth_request(conn, pool, b));

      /* In tunnel mode, deltify before answering the client, because
         answering may cause the client to terminate the connection
         and thus kill the server.  But otherwise, deltify after
         answering the client, to avoid user-visible delay. */

      if (b->tunnel)
        SVN_ERR(svn_fs_deltify_revision(b->fs, new_rev, pool));

      /* Unlock the paths. */
      if (! keep_locks && lock_tokens && lock_tokens->nelts)
        SVN_ERR(unlock_paths(lock_tokens, b, conn, pool));

      SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "r(?c)(?c)(?c)",
                                      new_rev, date, author, post_commit_err));

      if (! b->tunnel)
        SVN_ERR(svn_fs_deltify_revision(b->fs, new_rev, pool));
    }
  return SVN_NO_ERROR;
}

static svn_error_t *get_file(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                             apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  const char *path, *full_path, *hex_digest;
  svn_revnum_t rev;
  svn_fs_root_t *root;
  svn_stream_t *contents;
  apr_hash_t *props = NULL;
  apr_array_header_t *inherited_props;
  svn_string_t write_str;
  char buf[4096];
  apr_size_t len;
  svn_boolean_t want_props, want_contents;
  apr_uint64_t wants_inherited_props;
  svn_checksum_t *checksum;
  svn_error_t *err, *write_err;
  int i;
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  /* Parse arguments. */
  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "c(?r)bb?B", &path, &rev,
                                  &want_props, &want_contents,
                                  &wants_inherited_props));

  full_path = svn_fspath__join(b->fs_path->data,
                               svn_relpath_canonicalize(path, pool), pool);

  /* Check authorizations */
  SVN_ERR(must_have_access(conn, pool, b, svn_authz_read,
                           full_path, FALSE));

  if (!SVN_IS_VALID_REVNUM(rev))
    SVN_CMD_ERR(svn_fs_youngest_rev(&rev, b->fs, pool));

  SVN_ERR(log_command(b, conn, pool, "%s",
                      svn_log__get_file(full_path, rev,
                                        want_contents, want_props, pool)));

  /* Fetch the properties and a stream for the contents. */
  SVN_CMD_ERR(svn_fs_revision_root(&root, b->fs, rev, pool));
  SVN_CMD_ERR(svn_fs_file_checksum(&checksum, svn_checksum_md5, root,
                                   full_path, TRUE, pool));
  hex_digest = svn_checksum_to_cstring_display(checksum, pool);
  if (want_props || wants_inherited_props)
    SVN_CMD_ERR(get_props(&props, &inherited_props, &ab, root, full_path,
                          pool));
  if (want_contents)
    SVN_CMD_ERR(svn_fs_file_contents(&contents, root, full_path, pool));

  /* Send successful command response with revision and props. */
  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "w((?c)r(!", "success",
                                  hex_digest, rev));
  SVN_ERR(svn_ra_svn__write_proplist(conn, pool, props));

  if (wants_inherited_props)
    {
      apr_pool_t *iterpool = svn_pool_create(pool);

      SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "!)(?!"));
      for (i = 0; i < inherited_props->nelts; i++)
        {
          svn_prop_inherited_item_t *iprop =
            APR_ARRAY_IDX(inherited_props, i, svn_prop_inherited_item_t *);

          svn_pool_clear(iterpool);
          SVN_ERR(svn_ra_svn__write_tuple(conn, iterpool, "!(c(!",
                                          iprop->path_or_url));
          SVN_ERR(svn_ra_svn__write_proplist(conn, iterpool, iprop->prop_hash));
          SVN_ERR(svn_ra_svn__write_tuple(conn, iterpool, "!))!",
                                          iprop->path_or_url));
        }
      svn_pool_destroy(iterpool);
    }

  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "!))"));

  /* Now send the file's contents. */
  if (want_contents)
    {
      err = SVN_NO_ERROR;
      while (1)
        {
          len = sizeof(buf);
          err = svn_stream_read(contents, buf, &len);
          if (err)
            break;
          if (len > 0)
            {
              write_str.data = buf;
              write_str.len = len;
              SVN_ERR(svn_ra_svn__write_string(conn, pool, &write_str));
            }
          if (len < sizeof(buf))
            {
              err = svn_stream_close(contents);
              break;
            }
        }
      write_err = svn_ra_svn__write_cstring(conn, pool, "");
      if (write_err)
        {
          svn_error_clear(err);
          return write_err;
        }
      SVN_CMD_ERR(err);
      SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, ""));
    }

  return SVN_NO_ERROR;
}

static svn_error_t *get_dir(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                            apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  const char *path, *full_path;
  svn_revnum_t rev;
  apr_hash_t *entries, *props = NULL;
  apr_array_header_t *inherited_props;
  apr_hash_index_t *hi;
  svn_fs_root_t *root;
  apr_pool_t *subpool;
  svn_boolean_t want_props, want_contents;
  apr_uint64_t wants_inherited_props;
  apr_uint64_t dirent_fields;
  apr_array_header_t *dirent_fields_list = NULL;
  svn_ra_svn_item_t *elt;
  int i;
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "c(?r)bb?l?B", &path, &rev,
                                  &want_props, &want_contents,
                                  &dirent_fields_list,
                                  &wants_inherited_props));

  if (! dirent_fields_list)
    {
      dirent_fields = SVN_DIRENT_ALL;
    }
  else
    {
      dirent_fields = 0;

      for (i = 0; i < dirent_fields_list->nelts; ++i)
        {
          elt = &APR_ARRAY_IDX(dirent_fields_list, i, svn_ra_svn_item_t);

          if (elt->kind != SVN_RA_SVN_WORD)
            return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                    "Dirent field not a string");

          if (strcmp(SVN_RA_SVN_DIRENT_KIND, elt->u.word) == 0)
            dirent_fields |= SVN_DIRENT_KIND;
          else if (strcmp(SVN_RA_SVN_DIRENT_SIZE, elt->u.word) == 0)
            dirent_fields |= SVN_DIRENT_SIZE;
          else if (strcmp(SVN_RA_SVN_DIRENT_HAS_PROPS, elt->u.word) == 0)
            dirent_fields |= SVN_DIRENT_HAS_PROPS;
          else if (strcmp(SVN_RA_SVN_DIRENT_CREATED_REV, elt->u.word) == 0)
            dirent_fields |= SVN_DIRENT_CREATED_REV;
          else if (strcmp(SVN_RA_SVN_DIRENT_TIME, elt->u.word) == 0)
            dirent_fields |= SVN_DIRENT_TIME;
          else if (strcmp(SVN_RA_SVN_DIRENT_LAST_AUTHOR, elt->u.word) == 0)
            dirent_fields |= SVN_DIRENT_LAST_AUTHOR;
        }
    }

  full_path = svn_fspath__join(b->fs_path->data,
                               svn_relpath_canonicalize(path, pool), pool);

  /* Check authorizations */
  SVN_ERR(must_have_access(conn, pool, b, svn_authz_read,
                           full_path, FALSE));

  if (!SVN_IS_VALID_REVNUM(rev))
    SVN_CMD_ERR(svn_fs_youngest_rev(&rev, b->fs, pool));

  SVN_ERR(log_command(b, conn, pool, "%s",
                      svn_log__get_dir(full_path, rev,
                                       want_contents, want_props,
                                       dirent_fields, pool)));

  /* Fetch the root of the appropriate revision. */
  SVN_CMD_ERR(svn_fs_revision_root(&root, b->fs, rev, pool));

  /* Fetch the directory's explicit and/or inherited properties
     if requested. */
  if (want_props || wants_inherited_props)
    SVN_CMD_ERR(get_props(&props, &inherited_props, &ab, root, full_path,
                          pool));

  /* Begin response ... */
  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "w(r(!", "success", rev));
  SVN_ERR(svn_ra_svn__write_proplist(conn, pool, props));
  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "!)(!"));

  /* Fetch the directory entries if requested and send them immediately. */
  if (want_contents)
    {
      /* Use epoch for a placeholder for a missing date.  */
      const char *missing_date = svn_time_to_cstring(0, pool);

      SVN_CMD_ERR(svn_fs_dir_entries(&entries, root, full_path, pool));

      /* Transform the hash table's FS entries into dirents.  This probably
       * belongs in libsvn_repos. */
      subpool = svn_pool_create(pool);
      for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi))
        {
          const char *name = svn__apr_hash_index_key(hi);
          svn_fs_dirent_t *fsent = svn__apr_hash_index_val(hi);
          const char *file_path;

          /* The fields in the entry tuple.  */
          svn_node_kind_t entry_kind = svn_node_none;
          svn_filesize_t entry_size = 0;
          svn_boolean_t has_props = FALSE;
          /* If 'created rev' was not requested, send 0.  We can't use
           * SVN_INVALID_REVNUM as the tuple field is not optional.
           * See the email thread on dev@, 2012-03-28, subject
           * "buildbot failure in ASF Buildbot on svn-slik-w2k3-x64-ra",
           * <http://svn.haxx.se/dev/archive-2012-03/0655.shtml>. */
          svn_revnum_t created_rev = 0;
          const char *cdate = NULL;
          const char *last_author = NULL;

          svn_pool_clear(subpool);

          file_path = svn_fspath__join(full_path, name, subpool);
          if (! lookup_access(subpool, b, conn, svn_authz_read,
                              file_path, FALSE))
            continue;

          if (dirent_fields & SVN_DIRENT_KIND)
              entry_kind = fsent->kind;

          if (dirent_fields & SVN_DIRENT_SIZE)
              if (entry_kind != svn_node_dir)
                SVN_CMD_ERR(svn_fs_file_length(&entry_size, root, file_path,
                                               subpool));

          if (dirent_fields & SVN_DIRENT_HAS_PROPS)
            {
              apr_hash_t *file_props;

              /* has_props */
              SVN_CMD_ERR(svn_fs_node_proplist(&file_props, root, file_path,
                                               subpool));
              has_props = (apr_hash_count(file_props) > 0);
            }

          if ((dirent_fields & SVN_DIRENT_LAST_AUTHOR)
              || (dirent_fields & SVN_DIRENT_TIME)
              || (dirent_fields & SVN_DIRENT_CREATED_REV))
            {
              /* created_rev, last_author, time */
              SVN_CMD_ERR(svn_repos_get_committed_info(&created_rev,
                                                       &cdate,
                                                       &last_author,
                                                       root,
                                                       file_path,
                                                       subpool));
            }

          /* The client does not properly handle a missing CDATE. For
             interoperability purposes, we must fill in some junk.

             See libsvn_ra_svn/client.c:ra_svn_get_dir()  */
          if (cdate == NULL)
            cdate = missing_date;

          /* Send the entry. */
          SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "cwnbr(?c)(?c)", name,
                                          svn_node_kind_to_word(entry_kind),
                                          (apr_uint64_t) entry_size,
                                          has_props, created_rev,
                                          cdate, last_author));
        }
      svn_pool_destroy(subpool);
    }

  if (wants_inherited_props)
    {
      apr_pool_t *iterpool = svn_pool_create(pool);

      SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "!)(?!"));
      for (i = 0; i < inherited_props->nelts; i++)
        {
          svn_prop_inherited_item_t *iprop =
            APR_ARRAY_IDX(inherited_props, i, svn_prop_inherited_item_t *);

          svn_pool_clear(iterpool);
          SVN_ERR(svn_ra_svn__write_tuple(conn, iterpool, "!(c(!",
                                          iprop->path_or_url));
          SVN_ERR(svn_ra_svn__write_proplist(conn, iterpool, iprop->prop_hash));
          SVN_ERR(svn_ra_svn__write_tuple(conn, iterpool, "!))!",
                                          iprop->path_or_url));
        }
      svn_pool_destroy(iterpool);
    }

  /* Finish response. */
  return svn_ra_svn__write_tuple(conn, pool, "!))");
}

static svn_error_t *update(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                           apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  svn_revnum_t rev;
  const char *target, *full_path, *depth_word;
  svn_boolean_t recurse;
  apr_uint64_t send_copyfrom_args; /* Optional; default FALSE */
  apr_uint64_t ignore_ancestry; /* Optional; default FALSE */
  /* Default to unknown.  Old clients won't send depth, but we'll
     handle that by converting recurse if necessary. */
  svn_depth_t depth = svn_depth_unknown;
  svn_boolean_t is_checkout;

  /* Parse the arguments. */
  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "(?r)cb?wB?B", &rev, &target,
                                  &recurse, &depth_word,
                                  &send_copyfrom_args, &ignore_ancestry));
  target = svn_relpath_canonicalize(target, pool);

  if (depth_word)
    depth = svn_depth_from_word(depth_word);
  else
    depth = SVN_DEPTH_INFINITY_OR_FILES(recurse);

  full_path = svn_fspath__join(b->fs_path->data, target, pool);
  /* Check authorization and authenticate the user if necessary. */
  SVN_ERR(must_have_access(conn, pool, b, svn_authz_read, full_path, FALSE));

  if (!SVN_IS_VALID_REVNUM(rev))
    SVN_CMD_ERR(svn_fs_youngest_rev(&rev, b->fs, pool));

  SVN_ERR(accept_report(&is_checkout, NULL,
                        conn, pool, b, rev, target, NULL, TRUE,
                        depth,
                        (send_copyfrom_args == TRUE) /* send_copyfrom_args */,
                        (ignore_ancestry == TRUE) /* ignore_ancestry */));
  if (is_checkout)
    {
      SVN_ERR(log_command(b, conn, pool, "%s",
                          svn_log__checkout(full_path, rev,
                                            depth, pool)));
    }
  else
    {
      SVN_ERR(log_command(b, conn, pool, "%s",
                          svn_log__update(full_path, rev, depth,
                                          send_copyfrom_args, pool)));
    }

  return SVN_NO_ERROR;
}

static svn_error_t *switch_cmd(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                               apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  svn_revnum_t rev;
  const char *target, *depth_word;
  const char *switch_url, *switch_path;
  svn_boolean_t recurse;
  /* Default to unknown.  Old clients won't send depth, but we'll
     handle that by converting recurse if necessary. */
  svn_depth_t depth = svn_depth_unknown;
  apr_uint64_t send_copyfrom_args; /* Optional; default FALSE */
  apr_uint64_t ignore_ancestry; /* Optional; default TRUE */

  /* Parse the arguments. */
  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "(?r)cbc?w?BB", &rev, &target,
                                  &recurse, &switch_url, &depth_word,
                                  &send_copyfrom_args, &ignore_ancestry));
  target = svn_relpath_canonicalize(target, pool);
  switch_url = svn_uri_canonicalize(switch_url, pool);

  if (depth_word)
    depth = svn_depth_from_word(depth_word);
  else
    depth = SVN_DEPTH_INFINITY_OR_FILES(recurse);

  SVN_ERR(trivial_auth_request(conn, pool, b));
  if (!SVN_IS_VALID_REVNUM(rev))
    SVN_CMD_ERR(svn_fs_youngest_rev(&rev, b->fs, pool));

  SVN_CMD_ERR(get_fs_path(svn_path_uri_decode(b->repos_url, pool),
                          svn_path_uri_decode(switch_url, pool),
                          &switch_path));

  {
    const char *full_path = svn_fspath__join(b->fs_path->data, target, pool);
    SVN_ERR(log_command(b, conn, pool, "%s",
                        svn_log__switch(full_path, switch_path, rev,
                                        depth, pool)));
  }

  return accept_report(NULL, NULL,
                       conn, pool, b, rev, target, switch_path, TRUE,
                       depth,
                       (send_copyfrom_args == TRUE) /* send_copyfrom_args */,
                       (ignore_ancestry != FALSE) /* ignore_ancestry */);
}

static svn_error_t *status(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                           apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  svn_revnum_t rev;
  const char *target, *depth_word;
  svn_boolean_t recurse;
  /* Default to unknown.  Old clients won't send depth, but we'll
     handle that by converting recurse if necessary. */
  svn_depth_t depth = svn_depth_unknown;

  /* Parse the arguments. */
  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "cb?(?r)?w",
                                  &target, &recurse, &rev, &depth_word));
  target = svn_relpath_canonicalize(target, pool);

  if (depth_word)
    depth = svn_depth_from_word(depth_word);
  else
    depth = SVN_DEPTH_INFINITY_OR_EMPTY(recurse);

  SVN_ERR(trivial_auth_request(conn, pool, b));
  if (!SVN_IS_VALID_REVNUM(rev))
    SVN_CMD_ERR(svn_fs_youngest_rev(&rev, b->fs, pool));

  {
    const char *full_path = svn_fspath__join(b->fs_path->data, target, pool);
    SVN_ERR(log_command(b, conn, pool, "%s",
                        svn_log__status(full_path, rev, depth, pool)));
  }

  return accept_report(NULL, NULL, conn, pool, b, rev, target, NULL, FALSE,
                       depth, FALSE, FALSE);
}

static svn_error_t *diff(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                         apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  svn_revnum_t rev;
  const char *target, *versus_url, *versus_path, *depth_word;
  svn_boolean_t recurse, ignore_ancestry;
  svn_boolean_t text_deltas;
  /* Default to unknown.  Old clients won't send depth, but we'll
     handle that by converting recurse if necessary. */
  svn_depth_t depth = svn_depth_unknown;

  /* Parse the arguments. */
  if (params->nelts == 5)
    {
      /* Clients before 1.4 don't send the text_deltas boolean or depth. */
      SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "(?r)cbbc", &rev, &target,
                                      &recurse, &ignore_ancestry, &versus_url));
      text_deltas = TRUE;
      depth_word = NULL;
    }
  else
    {
      SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "(?r)cbbcb?w",
                                      &rev, &target, &recurse,
                                      &ignore_ancestry, &versus_url,
                                      &text_deltas, &depth_word));
    }
  target = svn_relpath_canonicalize(target, pool);
  versus_url = svn_uri_canonicalize(versus_url, pool);

  if (depth_word)
    depth = svn_depth_from_word(depth_word);
  else
    depth = SVN_DEPTH_INFINITY_OR_FILES(recurse);

  SVN_ERR(trivial_auth_request(conn, pool, b));

  if (!SVN_IS_VALID_REVNUM(rev))
    SVN_CMD_ERR(svn_fs_youngest_rev(&rev, b->fs, pool));
  SVN_CMD_ERR(get_fs_path(svn_path_uri_decode(b->repos_url, pool),
                          svn_path_uri_decode(versus_url, pool),
                          &versus_path));

  {
    const char *full_path = svn_fspath__join(b->fs_path->data, target, pool);
    svn_revnum_t from_rev;
    SVN_ERR(accept_report(NULL, &from_rev,
                          conn, pool, b, rev, target, versus_path,
                          text_deltas, depth, FALSE, ignore_ancestry));
    SVN_ERR(log_command(b, conn, pool, "%s",
                        svn_log__diff(full_path, from_rev, versus_path,
                                      rev, depth, ignore_ancestry,
                                      pool)));
  }
  return SVN_NO_ERROR;
}

/* Regardless of whether a client's capabilities indicate an
   understanding of this command (by way of SVN_RA_SVN_CAP_MERGEINFO),
   we provide a response.

   ASSUMPTION: When performing a 'merge' with two URLs at different
   revisions, the client will call this command more than once. */
static svn_error_t *get_mergeinfo(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                  apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  svn_revnum_t rev;
  apr_array_header_t *paths, *canonical_paths;
  svn_mergeinfo_catalog_t mergeinfo;
  int i;
  apr_hash_index_t *hi;
  const char *inherit_word;
  svn_mergeinfo_inheritance_t inherit;
  svn_boolean_t include_descendants;
  apr_pool_t *iterpool;
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "l(?r)wb", &paths, &rev,
                                  &inherit_word, &include_descendants));
  inherit = svn_inheritance_from_word(inherit_word);

  /* Canonicalize the paths which mergeinfo has been requested for. */
  canonical_paths = apr_array_make(pool, paths->nelts, sizeof(const char *));
  for (i = 0; i < paths->nelts; i++)
     {
        svn_ra_svn_item_t *item = &APR_ARRAY_IDX(paths, i, svn_ra_svn_item_t);
        const char *full_path;

        if (item->kind != SVN_RA_SVN_STRING)
          return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                  _("Path is not a string"));
        full_path = svn_relpath_canonicalize(item->u.string->data, pool);
        full_path = svn_fspath__join(b->fs_path->data, full_path, pool);
        APR_ARRAY_PUSH(canonical_paths, const char *) = full_path;
     }

  SVN_ERR(log_command(b, conn, pool, "%s",
                      svn_log__get_mergeinfo(canonical_paths, inherit,
                                             include_descendants,
                                             pool)));

  SVN_ERR(trivial_auth_request(conn, pool, b));
  SVN_CMD_ERR(svn_repos_fs_get_mergeinfo(&mergeinfo, b->repos,
                                         canonical_paths, rev,
                                         inherit,
                                         include_descendants,
                                         authz_check_access_cb_func(b), &ab,
                                         pool));
  SVN_ERR(svn_mergeinfo__remove_prefix_from_catalog(&mergeinfo, mergeinfo,
                                                    b->fs_path->data, pool));
  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "w((!", "success"));
  iterpool = svn_pool_create(pool);
  for (hi = apr_hash_first(pool, mergeinfo); hi; hi = apr_hash_next(hi))
    {
      const char *key = svn__apr_hash_index_key(hi);
      svn_mergeinfo_t value = svn__apr_hash_index_val(hi);
      svn_string_t *mergeinfo_string;

      svn_pool_clear(iterpool);

      SVN_ERR(svn_mergeinfo_to_string(&mergeinfo_string, value, iterpool));
      SVN_ERR(svn_ra_svn__write_tuple(conn, iterpool, "cs", key,
                                      mergeinfo_string));
    }
  svn_pool_destroy(iterpool);
  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "!))"));

  return SVN_NO_ERROR;
}

/* Send a log entry to the client. */
static svn_error_t *log_receiver(void *baton,
                                 svn_log_entry_t *log_entry,
                                 apr_pool_t *pool)
{
  log_baton_t *b = baton;
  svn_ra_svn_conn_t *conn = b->conn;
  apr_hash_index_t *h;
  svn_boolean_t invalid_revnum = FALSE;
  char action[2];
  const char *author, *date, *message;
  apr_uint64_t revprop_count;

  if (log_entry->revision == SVN_INVALID_REVNUM)
    {
      /* If the stack depth is zero, we've seen the last revision, so don't
         send it, just return. */
      if (b->stack_depth == 0)
        return SVN_NO_ERROR;

      /* Because the svn protocol won't let us send an invalid revnum, we have
         to fudge here and send an additional flag. */
      log_entry->revision = 0;
      invalid_revnum = TRUE;
      b->stack_depth--;
    }

  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "(!"));
  if (log_entry->changed_paths2)
    {
      for (h = apr_hash_first(pool, log_entry->changed_paths2); h;
                                                        h = apr_hash_next(h))
        {
          const char *path = svn__apr_hash_index_key(h);
          svn_log_changed_path2_t *change = svn__apr_hash_index_val(h);

          action[0] = change->action;
          action[1] = '\0';
          SVN_ERR(svn_ra_svn__write_tuple(
                      conn, pool, "cw(?cr)(cbb)",
                      path,
                      action,
                      change->copyfrom_path,
                      change->copyfrom_rev,
                      svn_node_kind_to_word(change->node_kind),
                      /* text_modified and props_modified are never unknown */
                      change->text_modified  == svn_tristate_true,
                      change->props_modified == svn_tristate_true));
        }
    }
  svn_compat_log_revprops_out(&author, &date, &message, log_entry->revprops);
  svn_compat_log_revprops_clear(log_entry->revprops);
  if (log_entry->revprops)
    revprop_count = apr_hash_count(log_entry->revprops);
  else
    revprop_count = 0;
  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "!)r(?c)(?c)(?c)bbn(!",
                                  log_entry->revision,
                                  author, date, message,
                                  log_entry->has_children,
                                  invalid_revnum, revprop_count));
  SVN_ERR(svn_ra_svn__write_proplist(conn, pool, log_entry->revprops));
  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "!)b",
                                  log_entry->subtractive_merge));

  if (log_entry->has_children)
    b->stack_depth++;

  return SVN_NO_ERROR;
}

static svn_error_t *log_cmd(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                            apr_array_header_t *params, void *baton)
{
  svn_error_t *err, *write_err;
  server_baton_t *b = baton;
  svn_revnum_t start_rev, end_rev;
  const char *full_path;
  svn_boolean_t send_changed_paths, strict_node, include_merged_revisions;
  apr_array_header_t *paths, *full_paths, *revprop_items, *revprops;
  char *revprop_word;
  svn_ra_svn_item_t *elt;
  int i;
  apr_uint64_t limit, include_merged_revs_param;
  log_baton_t lb;
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "l(?r)(?r)bb?n?Bwl", &paths,
                                  &start_rev, &end_rev, &send_changed_paths,
                                  &strict_node, &limit,
                                  &include_merged_revs_param,
                                  &revprop_word, &revprop_items));

  if (include_merged_revs_param == SVN_RA_SVN_UNSPECIFIED_NUMBER)
    include_merged_revisions = FALSE;
  else
    include_merged_revisions = (svn_boolean_t) include_merged_revs_param;

  if (revprop_word == NULL)
    /* pre-1.5 client */
    revprops = svn_compat_log_revprops_in(pool);
  else if (strcmp(revprop_word, "all-revprops") == 0)
    revprops = NULL;
  else if (strcmp(revprop_word, "revprops") == 0)
    {
      SVN_ERR_ASSERT(revprop_items);

      revprops = apr_array_make(pool, revprop_items->nelts,
                                sizeof(char *));
      for (i = 0; i < revprop_items->nelts; i++)
        {
          elt = &APR_ARRAY_IDX(revprop_items, i, svn_ra_svn_item_t);
          if (elt->kind != SVN_RA_SVN_STRING)
            return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                    _("Log revprop entry not a string"));
          APR_ARRAY_PUSH(revprops, const char *) = elt->u.string->data;
        }
    }
  else
    return svn_error_createf(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                             _("Unknown revprop word '%s' in log command"),
                             revprop_word);

  /* If we got an unspecified number then the user didn't send us anything,
     so we assume no limit.  If it's larger than INT_MAX then someone is
     messing with us, since we know the svn client libraries will never send
     us anything that big, so play it safe and default to no limit. */
  if (limit == SVN_RA_SVN_UNSPECIFIED_NUMBER || limit > INT_MAX)
    limit = 0;

  full_paths = apr_array_make(pool, paths->nelts, sizeof(const char *));
  for (i = 0; i < paths->nelts; i++)
    {
      elt = &APR_ARRAY_IDX(paths, i, svn_ra_svn_item_t);
      if (elt->kind != SVN_RA_SVN_STRING)
        return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                _("Log path entry not a string"));
      full_path = svn_relpath_canonicalize(elt->u.string->data, pool),
      full_path = svn_fspath__join(b->fs_path->data, full_path, pool);
      APR_ARRAY_PUSH(full_paths, const char *) = full_path;
    }
  SVN_ERR(trivial_auth_request(conn, pool, b));

  SVN_ERR(log_command(b, conn, pool, "%s",
                      svn_log__log(full_paths, start_rev, end_rev,
                                   (int) limit, send_changed_paths,
                                   strict_node, include_merged_revisions,
                                   revprops, pool)));

  /* Get logs.  (Can't report errors back to the client at this point.) */
  lb.fs_path = b->fs_path->data;
  lb.conn = conn;
  lb.stack_depth = 0;
  err = svn_repos_get_logs4(b->repos, full_paths, start_rev, end_rev,
                            (int) limit, send_changed_paths, strict_node,
                            include_merged_revisions, revprops,
                            authz_check_access_cb_func(b), &ab, log_receiver,
                            &lb, pool);

  write_err = svn_ra_svn__write_word(conn, pool, "done");
  if (write_err)
    {
      svn_error_clear(err);
      return write_err;
    }
  SVN_CMD_ERR(err);
  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, ""));
  return SVN_NO_ERROR;
}

static svn_error_t *check_path(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                               apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  svn_revnum_t rev;
  const char *path, *full_path;
  svn_fs_root_t *root;
  svn_node_kind_t kind;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "c(?r)", &path, &rev));
  full_path = svn_fspath__join(b->fs_path->data,
                               svn_relpath_canonicalize(path, pool), pool);

  /* Check authorizations */
  SVN_ERR(must_have_access(conn, pool, b, svn_authz_read,
                           full_path, FALSE));

  if (!SVN_IS_VALID_REVNUM(rev))
    SVN_CMD_ERR(svn_fs_youngest_rev(&rev, b->fs, pool));

  SVN_ERR(log_command(b, conn, pool, "check-path %s@%d",
                      svn_path_uri_encode(full_path, pool), rev));

  SVN_CMD_ERR(svn_fs_revision_root(&root, b->fs, rev, pool));
  SVN_CMD_ERR(svn_fs_check_path(&kind, root, full_path, pool));
  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, "w",
                                         svn_node_kind_to_word(kind)));
  return SVN_NO_ERROR;
}

static svn_error_t *stat_cmd(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                             apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  svn_revnum_t rev;
  const char *path, *full_path, *cdate;
  svn_fs_root_t *root;
  svn_dirent_t *dirent;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "c(?r)", &path, &rev));
  full_path = svn_fspath__join(b->fs_path->data,
                               svn_relpath_canonicalize(path, pool), pool);

  /* Check authorizations */
  SVN_ERR(must_have_access(conn, pool, b, svn_authz_read,
                           full_path, FALSE));

  if (!SVN_IS_VALID_REVNUM(rev))
    SVN_CMD_ERR(svn_fs_youngest_rev(&rev, b->fs, pool));

  SVN_ERR(log_command(b, conn, pool, "stat %s@%d",
                      svn_path_uri_encode(full_path, pool), rev));

  SVN_CMD_ERR(svn_fs_revision_root(&root, b->fs, rev, pool));
  SVN_CMD_ERR(svn_repos_stat(&dirent, root, full_path, pool));

  /* Need to return the equivalent of "(?l)", since that's what the
     client is reading.  */

  if (dirent == NULL)
    {
      SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, "()"));
      return SVN_NO_ERROR;
    }

  cdate = (dirent->time == (time_t) -1) ? NULL
    : svn_time_to_cstring(dirent->time, pool);

  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, "((wnbr(?c)(?c)))",
                                         svn_node_kind_to_word(dirent->kind),
                                         (apr_uint64_t) dirent->size,
                                         dirent->has_props, dirent->created_rev,
                                         cdate, dirent->last_author));

  return SVN_NO_ERROR;
}

static svn_error_t *get_locations(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                  apr_array_header_t *params, void *baton)
{
  svn_error_t *err, *write_err;
  server_baton_t *b = baton;
  svn_revnum_t revision;
  apr_array_header_t *location_revisions, *loc_revs_proto;
  svn_ra_svn_item_t *elt;
  int i;
  const char *relative_path;
  svn_revnum_t peg_revision;
  apr_hash_t *fs_locations;
  const char *abs_path;
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  /* Parse the arguments. */
  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "crl", &relative_path,
                                  &peg_revision,
                                  &loc_revs_proto));
  relative_path = svn_relpath_canonicalize(relative_path, pool);

  abs_path = svn_fspath__join(b->fs_path->data, relative_path, pool);

  location_revisions = apr_array_make(pool, loc_revs_proto->nelts,
                                      sizeof(svn_revnum_t));
  for (i = 0; i < loc_revs_proto->nelts; i++)
    {
      elt = &APR_ARRAY_IDX(loc_revs_proto, i, svn_ra_svn_item_t);
      if (elt->kind != SVN_RA_SVN_NUMBER)
        return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                "Get-locations location revisions entry "
                                "not a revision number");
      revision = (svn_revnum_t)(elt->u.number);
      APR_ARRAY_PUSH(location_revisions, svn_revnum_t) = revision;
    }
  SVN_ERR(trivial_auth_request(conn, pool, b));
  SVN_ERR(log_command(b, conn, pool, "%s",
                      svn_log__get_locations(abs_path, peg_revision,
                                             location_revisions, pool)));

  /* All the parameters are fine - let's perform the query against the
   * repository. */

  /* We store both err and write_err here, so the client will get
   * the "done" even if there was an error in fetching the results. */

  err = svn_repos_trace_node_locations(b->fs, &fs_locations, abs_path,
                                       peg_revision, location_revisions,
                                       authz_check_access_cb_func(b), &ab,
                                       pool);

  /* Now, write the results to the connection. */
  if (!err)
    {
      if (fs_locations)
        {
          apr_hash_index_t *iter;

          for (iter = apr_hash_first(pool, fs_locations); iter;
              iter = apr_hash_next(iter))
            {
              const svn_revnum_t *iter_key = svn__apr_hash_index_key(iter);
              const char *iter_value = svn__apr_hash_index_val(iter);

              SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "rc",
                                              *iter_key, iter_value));
            }
        }
    }

  write_err = svn_ra_svn__write_word(conn, pool, "done");
  if (write_err)
    {
      svn_error_clear(err);
      return write_err;
    }
  SVN_CMD_ERR(err);

  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, ""));

  return SVN_NO_ERROR;
}

static svn_error_t *gls_receiver(svn_location_segment_t *segment,
                                 void *baton,
                                 apr_pool_t *pool)
{
  svn_ra_svn_conn_t *conn = baton;
  return svn_ra_svn__write_tuple(conn, pool, "rr(?c)",
                                 segment->range_start,
                                 segment->range_end,
                                 segment->path);
}

static svn_error_t *get_location_segments(svn_ra_svn_conn_t *conn,
                                          apr_pool_t *pool,
                                          apr_array_header_t *params,
                                          void *baton)
{
  svn_error_t *err, *write_err;
  server_baton_t *b = baton;
  svn_revnum_t peg_revision, start_rev, end_rev;
  const char *relative_path;
  const char *abs_path;
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  /* Parse the arguments. */
  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "c(?r)(?r)(?r)",
                                  &relative_path, &peg_revision,
                                  &start_rev, &end_rev));
  relative_path = svn_relpath_canonicalize(relative_path, pool);

  abs_path = svn_fspath__join(b->fs_path->data, relative_path, pool);

  if (SVN_IS_VALID_REVNUM(start_rev)
      && SVN_IS_VALID_REVNUM(end_rev)
      && (end_rev > start_rev))
    {
      err = svn_error_createf(SVN_ERR_INCORRECT_PARAMS, NULL,
                              "Get-location-segments end revision must not be "
                              "younger than start revision");
      return log_fail_and_flush(err, b, conn, pool);
    }

  if (SVN_IS_VALID_REVNUM(peg_revision)
      && SVN_IS_VALID_REVNUM(start_rev)
      && (start_rev > peg_revision))
    {
      err = svn_error_createf(SVN_ERR_INCORRECT_PARAMS, NULL,
                              "Get-location-segments start revision must not "
                              "be younger than peg revision");
      return log_fail_and_flush(err, b, conn, pool);
    }

  SVN_ERR(trivial_auth_request(conn, pool, b));
  SVN_ERR(log_command(baton, conn, pool, "%s",
                      svn_log__get_location_segments(abs_path, peg_revision,
                                                     start_rev, end_rev,
                                                     pool)));

  /* All the parameters are fine - let's perform the query against the
   * repository. */

  /* We store both err and write_err here, so the client will get
   * the "done" even if there was an error in fetching the results. */

  err = svn_repos_node_location_segments(b->repos, abs_path,
                                         peg_revision, start_rev, end_rev,
                                         gls_receiver, (void *)conn,
                                         authz_check_access_cb_func(b), &ab,
                                         pool);
  write_err = svn_ra_svn__write_word(conn, pool, "done");
  if (write_err)
    {
      svn_error_clear(err);
      return write_err;
    }
  SVN_CMD_ERR(err);

  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, ""));

  return SVN_NO_ERROR;
}

/* This implements svn_write_fn_t.  Write LEN bytes starting at DATA to the
   client as a string. */
static svn_error_t *svndiff_handler(void *baton, const char *data,
                                    apr_size_t *len)
{
  file_revs_baton_t *b = baton;
  svn_string_t str;

  str.data = data;
  str.len = *len;
  return svn_ra_svn__write_string(b->conn, b->pool, &str);
}

/* This implements svn_close_fn_t.  Mark the end of the data by writing an
   empty string to the client. */
static svn_error_t *svndiff_close_handler(void *baton)
{
  file_revs_baton_t *b = baton;

  SVN_ERR(svn_ra_svn__write_cstring(b->conn, b->pool, ""));
  return SVN_NO_ERROR;
}

/* This implements the svn_repos_file_rev_handler_t interface. */
static svn_error_t *file_rev_handler(void *baton, const char *path,
                                     svn_revnum_t rev, apr_hash_t *rev_props,
                                     svn_boolean_t merged_revision,
                                     svn_txdelta_window_handler_t *d_handler,
                                     void **d_baton,
                                     apr_array_header_t *prop_diffs,
                                     apr_pool_t *pool)
{
  file_revs_baton_t *frb = baton;
  svn_stream_t *stream;

  SVN_ERR(svn_ra_svn__write_tuple(frb->conn, pool, "cr(!",
                                  path, rev));
  SVN_ERR(svn_ra_svn__write_proplist(frb->conn, pool, rev_props));
  SVN_ERR(svn_ra_svn__write_tuple(frb->conn, pool, "!)(!"));
  SVN_ERR(write_prop_diffs(frb->conn, pool, prop_diffs));
  SVN_ERR(svn_ra_svn__write_tuple(frb->conn, pool, "!)b", merged_revision));

  /* Store the pool for the delta stream. */
  frb->pool = pool;

  /* Prepare for the delta or just write an empty string. */
  if (d_handler)
    {
      stream = svn_stream_create(baton, pool);
      svn_stream_set_write(stream, svndiff_handler);
      svn_stream_set_close(stream, svndiff_close_handler);

      /* If the connection does not support SVNDIFF1 or if we don't want to use
       * compression, use the non-compressing "version 0" implementation */
      if (   svn_ra_svn_compression_level(frb->conn) > 0
          && svn_ra_svn_has_capability(frb->conn, SVN_RA_SVN_CAP_SVNDIFF1))
        svn_txdelta_to_svndiff3(d_handler, d_baton, stream, 1,
                                svn_ra_svn_compression_level(frb->conn), pool);
      else
        svn_txdelta_to_svndiff3(d_handler, d_baton, stream, 0,
                                svn_ra_svn_compression_level(frb->conn), pool);
    }
  else
    SVN_ERR(svn_ra_svn__write_cstring(frb->conn, pool, ""));

  return SVN_NO_ERROR;
}

static svn_error_t *get_file_revs(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                  apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  svn_error_t *err, *write_err;
  file_revs_baton_t frb;
  svn_revnum_t start_rev, end_rev;
  const char *path;
  const char *full_path;
  apr_uint64_t include_merged_revs_param;
  svn_boolean_t include_merged_revisions;
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  /* Parse arguments. */
  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "c(?r)(?r)?B",
                                  &path, &start_rev, &end_rev,
                                  &include_merged_revs_param));
  path = svn_relpath_canonicalize(path, pool);
  SVN_ERR(trivial_auth_request(conn, pool, b));
  full_path = svn_fspath__join(b->fs_path->data, path, pool);

  if (include_merged_revs_param == SVN_RA_SVN_UNSPECIFIED_NUMBER)
    include_merged_revisions = FALSE;
  else
    include_merged_revisions = (svn_boolean_t) include_merged_revs_param;

  SVN_ERR(log_command(b, conn, pool, "%s",
                      svn_log__get_file_revs(full_path, start_rev, end_rev,
                                             include_merged_revisions,
                                             pool)));

  frb.conn = conn;
  frb.pool = NULL;

  err = svn_repos_get_file_revs2(b->repos, full_path, start_rev, end_rev,
                                 include_merged_revisions,
                                 authz_check_access_cb_func(b), &ab,
                                 file_rev_handler, &frb, pool);
  write_err = svn_ra_svn__write_word(conn, pool, "done");
  if (write_err)
    {
      svn_error_clear(err);
      return write_err;
    }
  SVN_CMD_ERR(err);
  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, ""));

  return SVN_NO_ERROR;
}

static svn_error_t *lock(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                         apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  const char *path;
  const char *comment;
  const char *full_path;
  svn_boolean_t steal_lock;
  svn_revnum_t current_rev;
  svn_lock_t *l;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "c(?c)b(?r)", &path, &comment,
                                  &steal_lock, &current_rev));
  full_path = svn_fspath__join(b->fs_path->data,
                               svn_relpath_canonicalize(path, pool), pool);

  SVN_ERR(must_have_access(conn, pool, b, svn_authz_write,
                           full_path, TRUE));
  SVN_ERR(log_command(b, conn, pool, "%s",
                      svn_log__lock_one_path(full_path, steal_lock, pool)));

  SVN_CMD_ERR(svn_repos_fs_lock(&l, b->repos, full_path, NULL, comment, 0,
                                0, /* No expiration time. */
                                current_rev, steal_lock, pool));

  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "w(!", "success"));
  SVN_ERR(write_lock(conn, pool, l));
  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "!)"));

  return SVN_NO_ERROR;
}

static svn_error_t *lock_many(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                              apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  apr_array_header_t *path_revs;
  const char *comment;
  svn_boolean_t steal_lock;
  int i;
  apr_pool_t *subpool;
  const char *path;
  const char *full_path;
  svn_revnum_t current_rev;
  apr_array_header_t *log_paths;
  svn_lock_t *l;
  svn_error_t *err = SVN_NO_ERROR, *write_err;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "(?c)bl", &comment, &steal_lock,
                                  &path_revs));

  subpool = svn_pool_create(pool);

  /* Because we can only send a single auth reply per request, we send
     a reply before parsing the lock commands.  This means an authz
     access denial will abort the processing of the locks and return
     an error. */
  SVN_ERR(must_have_access(conn, pool, b, svn_authz_write, NULL, TRUE));

  /* Loop through the lock requests. */
  log_paths = apr_array_make(pool, path_revs->nelts, sizeof(full_path));
  for (i = 0; i < path_revs->nelts; ++i)
    {
      svn_ra_svn_item_t *item = &APR_ARRAY_IDX(path_revs, i,
                                               svn_ra_svn_item_t);

      svn_pool_clear(subpool);

      if (item->kind != SVN_RA_SVN_LIST)
        return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                "Lock requests should be list of lists");

      SVN_ERR(svn_ra_svn__parse_tuple(item->u.list, pool, "c(?r)", &path,
                                      &current_rev));

      /* Allocate the full_path out of pool so it will survive for use
       * by operational logging, after this loop. */
      full_path = svn_fspath__join(b->fs_path->data,
                                   svn_relpath_canonicalize(path, subpool),
                                   pool);
      APR_ARRAY_PUSH(log_paths, const char *) = full_path;

      if (! lookup_access(pool, b, conn, svn_authz_write, full_path, TRUE))
        {
          err = error_create_and_log(SVN_ERR_RA_NOT_AUTHORIZED, NULL, NULL,
                                     b, conn, pool);
          break;
        }

      err = svn_repos_fs_lock(&l, b->repos, full_path,
                              NULL, comment, FALSE,
                              0, /* No expiration time. */
                              current_rev,
                              steal_lock, subpool);

      if (err)
        {
          if (SVN_ERR_IS_LOCK_ERROR(err))
            {
              write_err = svn_ra_svn__write_cmd_failure(conn, pool, err);
              svn_error_clear(err);
              err = NULL;
              SVN_ERR(write_err);
            }
          else
            break;
        }
      else
        {
          SVN_ERR(svn_ra_svn__write_tuple(conn, subpool, "w!", "success"));
          SVN_ERR(write_lock(conn, subpool, l));
          SVN_ERR(svn_ra_svn__write_tuple(conn, subpool, "!"));
        }
    }

  svn_pool_destroy(subpool);

  SVN_ERR(log_command(b, conn, pool, "%s",
                      svn_log__lock(log_paths, steal_lock, pool)));

  /* NOTE: err might contain a fatal locking error from the loop above. */
  write_err = svn_ra_svn__write_word(conn, pool, "done");
  if (!write_err)
    SVN_CMD_ERR(err);
  svn_error_clear(err);
  SVN_ERR(write_err);
  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, ""));

  return SVN_NO_ERROR;
}

static svn_error_t *unlock(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                           apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  const char *path, *token, *full_path;
  svn_boolean_t break_lock;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "c(?c)b", &path, &token,
                                 &break_lock));

  full_path = svn_fspath__join(b->fs_path->data,
                               svn_relpath_canonicalize(path, pool), pool);

  /* Username required unless break_lock was specified. */
  SVN_ERR(must_have_access(conn, pool, b, svn_authz_write,
                           full_path, ! break_lock));
  SVN_ERR(log_command(b, conn, pool, "%s",
                      svn_log__unlock_one_path(full_path, break_lock, pool)));

  SVN_CMD_ERR(svn_repos_fs_unlock(b->repos, full_path, token, break_lock,
                                  pool));

  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, ""));

  return SVN_NO_ERROR;
}

static svn_error_t *unlock_many(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  svn_boolean_t break_lock;
  apr_array_header_t *unlock_tokens;
  int i;
  apr_pool_t *subpool;
  const char *path;
  const char *full_path;
  apr_array_header_t *log_paths;
  const char *token;
  svn_error_t *err = SVN_NO_ERROR, *write_err;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "bl", &break_lock,
                                  &unlock_tokens));

  /* Username required unless break_lock was specified. */
  SVN_ERR(must_have_access(conn, pool, b, svn_authz_write, NULL, ! break_lock));

  subpool = svn_pool_create(pool);

  /* Loop through the unlock requests. */
  log_paths = apr_array_make(pool, unlock_tokens->nelts, sizeof(full_path));
  for (i = 0; i < unlock_tokens->nelts; i++)
    {
      svn_ra_svn_item_t *item = &APR_ARRAY_IDX(unlock_tokens, i,
                                               svn_ra_svn_item_t);

      svn_pool_clear(subpool);

      if (item->kind != SVN_RA_SVN_LIST)
        return svn_error_create(SVN_ERR_RA_SVN_MALFORMED_DATA, NULL,
                                "Unlock request should be a list of lists");

      SVN_ERR(svn_ra_svn__parse_tuple(item->u.list, subpool, "c(?c)", &path,
                                      &token));

      /* Allocate the full_path out of pool so it will survive for use
       * by operational logging, after this loop. */
      full_path = svn_fspath__join(b->fs_path->data,
                                   svn_relpath_canonicalize(path, subpool),
                                   pool);
      APR_ARRAY_PUSH(log_paths, const char *) = full_path;

      if (! lookup_access(subpool, b, conn, svn_authz_write, full_path,
                          ! break_lock))
        return svn_error_create(SVN_ERR_RA_SVN_CMD_ERR,
                                error_create_and_log(SVN_ERR_RA_NOT_AUTHORIZED,
                                                     NULL, NULL,
                                                     b, conn, pool),
                                NULL);

      err = svn_repos_fs_unlock(b->repos, full_path, token, break_lock,
                                subpool);
      if (err)
        {
          if (SVN_ERR_IS_UNLOCK_ERROR(err))
            {
              write_err = svn_ra_svn__write_cmd_failure(conn, pool, err);
              svn_error_clear(err);
              err = NULL;
              SVN_ERR(write_err);
            }
          else
            break;
        }
      else
        SVN_ERR(svn_ra_svn__write_tuple(conn, subpool, "w(c)", "success",
                                        path));
    }

  svn_pool_destroy(subpool);

  SVN_ERR(log_command(b, conn, pool, "%s",
                      svn_log__unlock(log_paths, break_lock, pool)));

  /* NOTE: err might contain a fatal unlocking error from the loop above. */
  write_err = svn_ra_svn__write_word(conn, pool, "done");
  if (! write_err)
    SVN_CMD_ERR(err);
  svn_error_clear(err);
  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, ""));

  return SVN_NO_ERROR;
}

static svn_error_t *get_lock(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                             apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  const char *path;
  const char *full_path;
  svn_lock_t *l;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "c", &path));

  full_path = svn_fspath__join(b->fs_path->data,
                               svn_relpath_canonicalize(path, pool), pool);

  SVN_ERR(must_have_access(conn, pool, b, svn_authz_read,
                           full_path, FALSE));
  SVN_ERR(log_command(b, conn, pool, "get-lock %s",
                      svn_path_uri_encode(full_path, pool)));

  SVN_CMD_ERR(svn_fs_get_lock(&l, b->fs, full_path, pool));

  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "w((!", "success"));
  if (l)
    SVN_ERR(write_lock(conn, pool, l));
  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "!))"));

  return SVN_NO_ERROR;
}

static svn_error_t *get_locks(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                              apr_array_header_t *params, void *baton)
{
  server_baton_t *b = baton;
  const char *path;
  const char *full_path;
  const char *depth_word;
  svn_depth_t depth;
  apr_hash_t *locks;
  apr_hash_index_t *hi;
  svn_error_t *err;
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "c?(?w)", &path, &depth_word));

  depth = depth_word ? svn_depth_from_word(depth_word) : svn_depth_infinity;
  if ((depth != svn_depth_empty) &&
      (depth != svn_depth_files) &&
      (depth != svn_depth_immediates) &&
      (depth != svn_depth_infinity))
    {
      err = svn_error_create(SVN_ERR_INCORRECT_PARAMS, NULL,
                             "Invalid 'depth' specified in get-locks request");
      return log_fail_and_flush(err, b, conn, pool);
    }

  full_path = svn_fspath__join(b->fs_path->data,
                               svn_relpath_canonicalize(path, pool), pool);

  SVN_ERR(trivial_auth_request(conn, pool, b));

  SVN_ERR(log_command(b, conn, pool, "get-locks %s",
                      svn_path_uri_encode(full_path, pool)));
  SVN_CMD_ERR(svn_repos_fs_get_locks2(&locks, b->repos, full_path, depth,
                                      authz_check_access_cb_func(b), &ab,
                                      pool));

  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "w((!", "success"));
  for (hi = apr_hash_first(pool, locks); hi; hi = apr_hash_next(hi))
    {
      svn_lock_t *l = svn__apr_hash_index_val(hi);

      SVN_ERR(write_lock(conn, pool, l));
    }
  SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "!))"));

  return SVN_NO_ERROR;
}

static svn_error_t *replay_one_revision(svn_ra_svn_conn_t *conn,
                                        server_baton_t *b,
                                        svn_revnum_t rev,
                                        svn_revnum_t low_water_mark,
                                        svn_boolean_t send_deltas,
                                        apr_pool_t *pool)
{
  const svn_delta_editor_t *editor;
  void *edit_baton;
  svn_fs_root_t *root;
  svn_error_t *err;
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  SVN_ERR(log_command(b, conn, pool,
                      svn_log__replay(b->fs_path->data, rev, pool)));

  svn_ra_svn_get_editor(&editor, &edit_baton, conn, pool, NULL, NULL);

  err = svn_fs_revision_root(&root, b->fs, rev, pool);

  if (! err)
    err = svn_repos_replay2(root, b->fs_path->data, low_water_mark,
                            send_deltas, editor, edit_baton,
                            authz_check_access_cb_func(b), &ab, pool);

  if (err)
    svn_error_clear(editor->abort_edit(edit_baton, pool));
  SVN_CMD_ERR(err);

  return svn_ra_svn__write_cmd_finish_replay(conn, pool);
}

static svn_error_t *replay(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                           apr_array_header_t *params, void *baton)
{
  svn_revnum_t rev, low_water_mark;
  svn_boolean_t send_deltas;
  server_baton_t *b = baton;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "rrb", &rev, &low_water_mark,
                                 &send_deltas));

  SVN_ERR(trivial_auth_request(conn, pool, b));

  SVN_ERR(replay_one_revision(conn, b, rev, low_water_mark,
                              send_deltas, pool));

  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, ""));

  return SVN_NO_ERROR;
}

static svn_error_t *replay_range(svn_ra_svn_conn_t *conn, apr_pool_t *pool,
                                 apr_array_header_t *params, void *baton)
{
  svn_revnum_t start_rev, end_rev, rev, low_water_mark;
  svn_boolean_t send_deltas;
  server_baton_t *b = baton;
  apr_pool_t *iterpool;
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "rrrb", &start_rev,
                                 &end_rev, &low_water_mark,
                                 &send_deltas));

  SVN_ERR(trivial_auth_request(conn, pool, b));

  iterpool = svn_pool_create(pool);
  for (rev = start_rev; rev <= end_rev; rev++)
    {
      apr_hash_t *props;

      svn_pool_clear(iterpool);

      SVN_CMD_ERR(svn_repos_fs_revision_proplist(&props, b->repos, rev,
                                                 authz_check_access_cb_func(b),
                                                 &ab,
                                                 iterpool));
      SVN_ERR(svn_ra_svn__write_tuple(conn, iterpool, "w(!", "revprops"));
      SVN_ERR(svn_ra_svn__write_proplist(conn, iterpool, props));
      SVN_ERR(svn_ra_svn__write_tuple(conn, iterpool, "!)"));

      SVN_ERR(replay_one_revision(conn, b, rev, low_water_mark,
                                  send_deltas, iterpool));

    }
  svn_pool_destroy(iterpool);

  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, ""));

  return SVN_NO_ERROR;
}

static svn_error_t *
get_deleted_rev(svn_ra_svn_conn_t *conn,
                apr_pool_t *pool,
                apr_array_header_t *params,
                void *baton)
{
  server_baton_t *b = baton;
  const char *path, *full_path;
  svn_revnum_t peg_revision;
  svn_revnum_t end_revision;
  svn_revnum_t revision_deleted;

  SVN_ERR(svn_ra_svn__parse_tuple(params, pool, "crr",
                                 &path, &peg_revision, &end_revision));
  full_path = svn_fspath__join(b->fs_path->data,
                               svn_relpath_canonicalize(path, pool), pool);
  SVN_ERR(log_command(b, conn, pool, "get-deleted-rev"));
  SVN_ERR(trivial_auth_request(conn, pool, b));
  SVN_ERR(svn_repos_deleted_rev(b->fs, full_path, peg_revision, end_revision,
                                &revision_deleted, pool));
  SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, "r", revision_deleted));
  return SVN_NO_ERROR;
}

static svn_error_t *
get_inherited_props(svn_ra_svn_conn_t *conn,
                    apr_pool_t *pool,
                    apr_array_header_t *params,
                    void *baton)
{
  server_baton_t *b = baton;
  const char *path, *full_path;
  svn_revnum_t rev;
  svn_fs_root_t *root;
  apr_array_header_t *inherited_props;
  int i;
  apr_pool_t *iterpool = svn_pool_create(pool);
  authz_baton_t ab;

  ab.server = b;
  ab.conn = conn;

  /* Parse arguments. */
  SVN_ERR(svn_ra_svn__parse_tuple(params, iterpool, "c(?r)", &path, &rev));

  full_path = svn_fspath__join(b->fs_path->data,
                               svn_relpath_canonicalize(path, iterpool),
                               pool);

  /* Check authorizations */
  SVN_ERR(must_have_access(conn, iterpool, b, svn_authz_read,
                           full_path, FALSE));

  if (!SVN_IS_VALID_REVNUM(rev))
    SVN_CMD_ERR(svn_fs_youngest_rev(&rev, b->fs, pool));

  SVN_ERR(log_command(b, conn, pool, "%s",
                      svn_log__get_inherited_props(full_path, rev,
                                                   iterpool)));

  /* Fetch the properties and a stream for the contents. */
  SVN_CMD_ERR(svn_fs_revision_root(&root, b->fs, rev, iterpool));
  SVN_CMD_ERR(get_props(NULL, &inherited_props, &ab, root, full_path, pool));

  /* Send successful command response with revision and props. */
  SVN_ERR(svn_ra_svn__write_tuple(conn, iterpool, "w(!", "success"));

  SVN_ERR(svn_ra_svn__write_tuple(conn, iterpool, "!(?!"));

  for (i = 0; i < inherited_props->nelts; i++)
    {
      svn_prop_inherited_item_t *iprop =
        APR_ARRAY_IDX(inherited_props, i, svn_prop_inherited_item_t *);

      svn_pool_clear(iterpool);
      SVN_ERR(svn_ra_svn__write_tuple(conn, iterpool, "!(c(!",
                                      iprop->path_or_url));
      SVN_ERR(svn_ra_svn__write_proplist(conn, iterpool, iprop->prop_hash));
      SVN_ERR(svn_ra_svn__write_tuple(conn, iterpool, "!))!",
                                      iprop->path_or_url));
    }

  SVN_ERR(svn_ra_svn__write_tuple(conn, iterpool, "!))"));
  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}

static const svn_ra_svn_cmd_entry_t main_commands[] = {
  { "reparent",        reparent },
  { "get-latest-rev",  get_latest_rev },
  { "get-dated-rev",   get_dated_rev },
  { "change-rev-prop", change_rev_prop },
  { "change-rev-prop2",change_rev_prop2 },
  { "rev-proplist",    rev_proplist },
  { "rev-prop",        rev_prop },
  { "commit",          commit },
  { "get-file",        get_file },
  { "get-dir",         get_dir },
  { "update",          update },
  { "switch",          switch_cmd },
  { "status",          status },
  { "diff",            diff },
  { "get-mergeinfo",   get_mergeinfo },
  { "log",             log_cmd },
  { "check-path",      check_path },
  { "stat",            stat_cmd },
  { "get-locations",   get_locations },
  { "get-location-segments",   get_location_segments },
  { "get-file-revs",   get_file_revs },
  { "lock",            lock },
  { "lock-many",       lock_many },
  { "unlock",          unlock },
  { "unlock-many",     unlock_many },
  { "get-lock",        get_lock },
  { "get-locks",       get_locks },
  { "replay",          replay },
  { "replay-range",    replay_range },
  { "get-deleted-rev", get_deleted_rev },
  { "get-iprops",      get_inherited_props },
  { NULL }
};

/* Skip past the scheme part of a URL, including the tunnel specification
 * if present.  Return NULL if the scheme part is invalid for ra_svn. */
static const char *skip_scheme_part(const char *url)
{
  if (strncmp(url, "svn", 3) != 0)
    return NULL;
  url += 3;
  if (*url == '+')
    url += strcspn(url, ":");
  if (strncmp(url, "://", 3) != 0)
    return NULL;
  return url + 3;
}

/* Check that PATH is a valid repository path, meaning it doesn't contain any
   '..' path segments.
   NOTE: This is similar to svn_path_is_backpath_present, but that function
   assumes the path separator is '/'.  This function also checks for
   segments delimited by the local path separator. */
static svn_boolean_t
repos_path_valid(const char *path)
{
  const char *s = path;

  while (*s)
    {
      /* Scan for the end of the segment. */
      while (*path && *path != '/' && *path != SVN_PATH_LOCAL_SEPARATOR)
        ++path;

      /* Check for '..'. */
#ifdef WIN32
      /* On Windows, don't allow sequences of more than one character
         consisting of just dots and spaces.  Win32 functions treat
         paths such as ".. " and "......." inconsistently.  Make sure
         no one can escape out of the root. */
      if (path - s >= 2 && strspn(s, ". ") == (size_t)(path - s))
        return FALSE;
#else  /* ! WIN32 */
      if (path - s == 2 && s[0] == '.' && s[1] == '.')
        return FALSE;
#endif

      /* Skip all separators. */
      while (*path && (*path == '/' || *path == SVN_PATH_LOCAL_SEPARATOR))
        ++path;
      s = path;
    }

  return TRUE;
}

/* Look for the repository given by URL, using ROOT as the virtual
 * repository root.  If we find one, fill in the repos, fs, cfg,
 * repos_url, and fs_path fields of B.  Set B->repos's client
 * capabilities to CAPABILITIES, which must be at least as long-lived
 * as POOL, and whose elements are SVN_RA_CAPABILITY_*.
 */
static svn_error_t *find_repos(const char *url, const char *root,
                               server_baton_t *b,
                               svn_ra_svn_conn_t *conn,
                               const apr_array_header_t *capabilities,
                               apr_pool_t *pool)
{
  const char *path, *full_path, *repos_root, *fs_path, *hooks_env;
  svn_stringbuf_t *url_buf;

  /* Skip past the scheme and authority part. */
  path = skip_scheme_part(url);
  if (path == NULL)
    return svn_error_createf(SVN_ERR_BAD_URL, NULL,
                             "Non-svn URL passed to svn server: '%s'", url);

  if (! b->vhost)
    {
      path = strchr(path, '/');
      if (path == NULL)
        path = "";
    }
  path = svn_relpath_canonicalize(path, pool);
  path = svn_path_uri_decode(path, pool);

  /* Ensure that it isn't possible to escape the root by disallowing
     '..' segments. */
  if (!repos_path_valid(path))
    return svn_error_create(SVN_ERR_BAD_FILENAME, NULL,
                            "Couldn't determine repository path");

  /* Join the server-configured root with the client path. */
  full_path = svn_dirent_join(svn_dirent_canonicalize(root, pool),
                              path, pool);

  /* Search for a repository in the full path. */
  repos_root = svn_repos_find_root_path(full_path, pool);
  if (!repos_root)
    return svn_error_createf(SVN_ERR_RA_SVN_REPOS_NOT_FOUND, NULL,
                             "No repository found in '%s'", url);

  /* Open the repository and fill in b with the resulting information. */
  SVN_ERR(svn_repos_open2(&b->repos, repos_root, b->fs_config, pool));
  SVN_ERR(svn_repos_remember_client_capabilities(b->repos, capabilities));
  b->fs = svn_repos_fs(b->repos);
  fs_path = full_path + strlen(repos_root);
  b->fs_path = svn_stringbuf_create(*fs_path ? fs_path : "/", pool);
  url_buf = svn_stringbuf_create(url, pool);
  svn_path_remove_components(url_buf,
                             svn_path_component_count(b->fs_path->data));
  b->repos_url = url_buf->data;
  b->authz_repos_name = svn_dirent_is_child(root, repos_root, pool);
  if (b->authz_repos_name == NULL)
    b->repos_name = svn_dirent_basename(repos_root, pool);
  else
    b->repos_name = b->authz_repos_name;
  b->repos_name = svn_path_uri_encode(b->repos_name, pool);

  /* If the svnserve configuration has not been loaded then load it from the
   * repository. */
  if (NULL == b->cfg)
    {
      b->base = svn_repos_conf_dir(b->repos, pool);

      SVN_ERR(svn_config_read3(&b->cfg, svn_repos_svnserve_conf(b->repos, pool),
                               FALSE, /* must_exist */
                               FALSE, /* section_names_case_sensitive */
                               FALSE, /* option_names_case_sensitive */
                               pool));
      SVN_ERR(load_pwdb_config(b, conn, pool));
      SVN_ERR(load_authz_config(b, conn, repos_root, pool));
    }
  /* svnserve.conf has been loaded via the --config-file option so need
   * to load pwdb and authz. */
  else
    {
      SVN_ERR(load_pwdb_config(b, conn, pool));
      SVN_ERR(load_authz_config(b, conn, repos_root, pool));
    }

#ifdef SVN_HAVE_SASL
  /* Should we use Cyrus SASL? */
  SVN_ERR(svn_config_get_bool(b->cfg, &b->use_sasl, SVN_CONFIG_SECTION_SASL,
                              SVN_CONFIG_OPTION_USE_SASL, FALSE));
#endif

  /* Use the repository UUID as the default realm. */
  SVN_ERR(svn_fs_get_uuid(b->fs, &b->realm, pool));
  svn_config_get(b->cfg, &b->realm, SVN_CONFIG_SECTION_GENERAL,
                 SVN_CONFIG_OPTION_REALM, b->realm);

  /* Make sure it's possible for the client to authenticate.  Note
     that this doesn't take into account any authz configuration read
     above, because we can't know about access it grants until paths
     are given by the client. */
  if (get_access(b, UNAUTHENTICATED) == NO_ACCESS
      && (get_access(b, AUTHENTICATED) == NO_ACCESS
          || (!b->tunnel_user && !b->pwdb && !b->use_sasl)))
    return error_create_and_log(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
                                 "No access allowed to this repository",
                                 b, conn, pool);

  /* Configure hook script environment variables. */
  svn_config_get(b->cfg, &hooks_env, SVN_CONFIG_SECTION_GENERAL,
                 SVN_CONFIG_OPTION_HOOKS_ENV, NULL);
  if (hooks_env)
    hooks_env = svn_dirent_internal_style(hooks_env, pool);
  SVN_ERR(svn_repos_hooks_setenv(b->repos, hooks_env, pool));

  return SVN_NO_ERROR;
}

/* Compute the authentication name EXTERNAL should be able to get, if any. */
static const char *get_tunnel_user(serve_params_t *params, apr_pool_t *pool)
{
  /* Only offer EXTERNAL for connections tunneled over a login agent. */
  if (!params->tunnel)
    return NULL;

  /* If a tunnel user was provided on the command line, use that. */
  if (params->tunnel_user)
    return params->tunnel_user;

  return svn_user_get_name(pool);
}

static void
fs_warning_func(void *baton, svn_error_t *err)
{
  fs_warning_baton_t *b = baton;
  log_server_error(err, b->server, b->conn, b->pool);
  /* TODO: Keep log_pool in the server baton, cleared after every log? */
  svn_pool_clear(b->pool);
}

/* Return the normalized repository-relative path for the given PATH
 * (may be a URL, full path or relative path) and fs contained in the
 * server baton BATON. Allocate the result in POOL.
 */
static const char *
get_normalized_repo_rel_path(void *baton,
                             const char *path,
                             apr_pool_t *pool)
{
  server_baton_t *sb = baton;

  if (svn_path_is_url(path))
    {
      /* This is a copyfrom URL. */
      path = svn_uri_skip_ancestor(sb->repos_url, path, pool);
      path = svn_fspath__canonicalize(path, pool);
    }
  else
    {
      /* This is a base-relative path. */
      if ((path)[0] != '/')
        /* Get an absolute path for use in the FS. */
        path = svn_fspath__join(sb->fs_path->data, path, pool);
    }

  return path;
}

/* Get the revision root for REVISION in fs given by server baton BATON
 * and return it in *FS_ROOT. Use HEAD if REVISION is SVN_INVALID_REVNUM.
 * Use POOL for allocations.
 */
static svn_error_t *
get_revision_root(svn_fs_root_t **fs_root,
                  void *baton,
                  svn_revnum_t revision,
                  apr_pool_t *pool)
{
  server_baton_t *sb = baton;

  if (!SVN_IS_VALID_REVNUM(revision))
    SVN_ERR(svn_fs_youngest_rev(&revision, sb->fs, pool));

  SVN_ERR(svn_fs_revision_root(fs_root, sb->fs, revision, pool));

  return SVN_NO_ERROR;
}

static svn_error_t *
fetch_props_func(apr_hash_t **props,
                 void *baton,
                 const char *path,
                 svn_revnum_t base_revision,
                 apr_pool_t *result_pool,
                 apr_pool_t *scratch_pool)
{
  svn_fs_root_t *fs_root;
  svn_error_t *err;

  path = get_normalized_repo_rel_path(baton, path, scratch_pool);
  SVN_ERR(get_revision_root(&fs_root, baton, base_revision, scratch_pool));

  err = svn_fs_node_proplist(props, fs_root, path, result_pool);
  if (err && err->apr_err == SVN_ERR_FS_NOT_FOUND)
    {
      svn_error_clear(err);
      *props = apr_hash_make(result_pool);
      return SVN_NO_ERROR;
    }
  else if (err)
    return svn_error_trace(err);

  return SVN_NO_ERROR;
}

static svn_error_t *
fetch_kind_func(svn_node_kind_t *kind,
                void *baton,
                const char *path,
                svn_revnum_t base_revision,
                apr_pool_t *scratch_pool)
{
  svn_fs_root_t *fs_root;

  path = get_normalized_repo_rel_path(baton, path, scratch_pool);
  SVN_ERR(get_revision_root(&fs_root, baton, base_revision, scratch_pool));

  SVN_ERR(svn_fs_check_path(kind, fs_root, path, scratch_pool));

  return SVN_NO_ERROR;
}

static svn_error_t *
fetch_base_func(const char **filename,
                void *baton,
                const char *path,
                svn_revnum_t base_revision,
                apr_pool_t *result_pool,
                apr_pool_t *scratch_pool)
{
  svn_stream_t *contents;
  svn_stream_t *file_stream;
  const char *tmp_filename;
  svn_fs_root_t *fs_root;
  svn_error_t *err;

  path = get_normalized_repo_rel_path(baton, path, scratch_pool);
  SVN_ERR(get_revision_root(&fs_root, baton, base_revision, scratch_pool));

  err = svn_fs_file_contents(&contents, fs_root, path, scratch_pool);
  if (err && err->apr_err == SVN_ERR_FS_NOT_FOUND)
    {
      svn_error_clear(err);
      *filename = NULL;
      return SVN_NO_ERROR;
    }
  else if (err)
    return svn_error_trace(err);
  SVN_ERR(svn_stream_open_unique(&file_stream, &tmp_filename, NULL,
                                 svn_io_file_del_on_pool_cleanup,
                                 scratch_pool, scratch_pool));
  SVN_ERR(svn_stream_copy3(contents, file_stream, NULL, NULL, scratch_pool));

  *filename = apr_pstrdup(result_pool, tmp_filename);

  return SVN_NO_ERROR;
}

svn_error_t *serve(svn_ra_svn_conn_t *conn, serve_params_t *params,
                   apr_pool_t *pool)
{
  svn_error_t *err, *io_err;
  apr_uint64_t ver;
  const char *uuid, *client_url, *ra_client_string, *client_string;
  apr_array_header_t *caplist, *cap_words;
  server_baton_t b;
  fs_warning_baton_t warn_baton;
  svn_stringbuf_t *cap_log = svn_stringbuf_create_empty(pool);

  b.tunnel = params->tunnel;
  b.tunnel_user = get_tunnel_user(params, pool);
  b.read_only = params->read_only;
  b.user = NULL;
  b.username_case = params->username_case;
  b.authz_user = NULL;
  b.base = params->base;
  b.cfg = params->cfg;
  b.pwdb = NULL;
  b.authzdb = NULL;
  b.realm = NULL;
  b.log_file = params->log_file;
  b.pool = pool;
  b.use_sasl = FALSE;
  b.vhost = params->vhost;

  /* construct FS configuration parameters */
  b.fs_config = apr_hash_make(pool);
  svn_hash_sets(b.fs_config, SVN_FS_CONFIG_FSFS_CACHE_DELTAS,
                params->cache_txdeltas ? "1" :"0");
  svn_hash_sets(b.fs_config, SVN_FS_CONFIG_FSFS_CACHE_FULLTEXTS,
                params->cache_fulltexts ? "1" :"0");
  svn_hash_sets(b.fs_config, SVN_FS_CONFIG_FSFS_CACHE_REVPROPS,
                params->cache_revprops ? "1" :"0");

  /* Send greeting.  We don't support version 1 any more, so we can
   * send an empty mechlist. */
  if (params->compression_level > 0)
    SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, "nn()(wwwwwwwwwww)",
                                           (apr_uint64_t) 2, (apr_uint64_t) 2,
                                           SVN_RA_SVN_CAP_EDIT_PIPELINE,
                                           SVN_RA_SVN_CAP_SVNDIFF1,
                                           SVN_RA_SVN_CAP_ABSENT_ENTRIES,
                                           SVN_RA_SVN_CAP_COMMIT_REVPROPS,
                                           SVN_RA_SVN_CAP_DEPTH,
                                           SVN_RA_SVN_CAP_LOG_REVPROPS,
                                           SVN_RA_SVN_CAP_ATOMIC_REVPROPS,
                                           SVN_RA_SVN_CAP_PARTIAL_REPLAY,
                                           SVN_RA_SVN_CAP_INHERITED_PROPS,
                                           SVN_RA_SVN_CAP_EPHEMERAL_TXNPROPS,
                                           SVN_RA_SVN_CAP_GET_FILE_REVS_REVERSE
                                           ));
  else
    SVN_ERR(svn_ra_svn__write_cmd_response(conn, pool, "nn()(wwwwwwwwww)",
                                           (apr_uint64_t) 2, (apr_uint64_t) 2,
                                           SVN_RA_SVN_CAP_EDIT_PIPELINE,
                                           SVN_RA_SVN_CAP_ABSENT_ENTRIES,
                                           SVN_RA_SVN_CAP_COMMIT_REVPROPS,
                                           SVN_RA_SVN_CAP_DEPTH,
                                           SVN_RA_SVN_CAP_LOG_REVPROPS,
                                           SVN_RA_SVN_CAP_ATOMIC_REVPROPS,
                                           SVN_RA_SVN_CAP_PARTIAL_REPLAY,
                                           SVN_RA_SVN_CAP_INHERITED_PROPS,
                                           SVN_RA_SVN_CAP_EPHEMERAL_TXNPROPS,
                                           SVN_RA_SVN_CAP_GET_FILE_REVS_REVERSE
                                           ));

  /* Read client response, which we assume to be in version 2 format:
   * version, capability list, and client URL; then we do an auth
   * request. */
  SVN_ERR(svn_ra_svn__read_tuple(conn, pool, "nlc?c(?c)",
                                 &ver, &caplist, &client_url,
                                 &ra_client_string,
                                 &client_string));
  if (ver != 2)
    return SVN_NO_ERROR;

  client_url = svn_uri_canonicalize(client_url, pool);
  SVN_ERR(svn_ra_svn_set_capabilities(conn, caplist));

  /* All released versions of Subversion support edit-pipeline,
   * so we do not accept connections from clients that do not. */
  if (! svn_ra_svn_has_capability(conn, SVN_RA_SVN_CAP_EDIT_PIPELINE))
    return SVN_NO_ERROR;

  /* find_repos needs the capabilities as a list of words (eventually
     they get handed to the start-commit hook).  While we could add a
     new interface to re-retrieve them from conn and convert the
     result to a list, it's simpler to just convert caplist by hand
     here, since we already have it and turning 'svn_ra_svn_item_t's
     into 'const char *'s is pretty easy.

     We only record capabilities we care about.  The client may report
     more (because it doesn't know what the server cares about). */
  {
    int i;
    svn_ra_svn_item_t *item;

    cap_words = apr_array_make(pool, 1, sizeof(const char *));
    for (i = 0; i < caplist->nelts; i++)
      {
        item = &APR_ARRAY_IDX(caplist, i, svn_ra_svn_item_t);
        /* ra_svn_set_capabilities() already type-checked for us */
        if (strcmp(item->u.word, SVN_RA_SVN_CAP_MERGEINFO) == 0)
          {
            APR_ARRAY_PUSH(cap_words, const char *)
              = SVN_RA_CAPABILITY_MERGEINFO;
          }
        /* Save for operational log. */
        if (cap_log->len > 0)
          svn_stringbuf_appendcstr(cap_log, " ");
        svn_stringbuf_appendcstr(cap_log, item->u.word);
      }
  }

  err = find_repos(client_url, params->root, &b, conn, cap_words, pool);
  if (!err)
    {
      SVN_ERR(auth_request(conn, pool, &b, READ_ACCESS, FALSE));
      if (current_access(&b) == NO_ACCESS)
        err = error_create_and_log(SVN_ERR_RA_NOT_AUTHORIZED, NULL,
                                   "Not authorized for access",
                                   &b, conn, pool);
    }
  if (err)
    {
      log_error(err, b.log_file, svn_ra_svn_conn_remote_host(conn),
                b.user, NULL, pool);
      io_err = svn_ra_svn__write_cmd_failure(conn, pool, err);
      svn_error_clear(err);
      SVN_ERR(io_err);
      return svn_ra_svn__flush(conn, pool);
    }

  /* Log the open. */
  if (ra_client_string == NULL || ra_client_string[0] == '\0')
    ra_client_string = "-";
  else
    ra_client_string = svn_path_uri_encode(ra_client_string, pool);
  if (client_string == NULL || client_string[0] == '\0')
    client_string = "-";
  else
    client_string = svn_path_uri_encode(client_string, pool);
  SVN_ERR(log_command(&b, conn, pool,
                      "open %" APR_UINT64_T_FMT " cap=(%s) %s %s %s",
                      ver, cap_log->data,
                      svn_path_uri_encode(b.fs_path->data, pool),
                      ra_client_string, client_string));

  warn_baton.server = &b;
  warn_baton.conn = conn;
  warn_baton.pool = svn_pool_create(pool);
  svn_fs_set_warning_func(b.fs, fs_warning_func, &warn_baton);

  SVN_ERR(svn_fs_get_uuid(b.fs, &uuid, pool));

  /* We can't claim mergeinfo capability until we know whether the
     repository supports mergeinfo (i.e., is not a 1.4 repository),
     but we don't get the repository url from the client until after
     we've already sent the initial list of server capabilities.  So
     we list repository capabilities here, in our first response after
     the client has sent the url. */
  {
    svn_boolean_t supports_mergeinfo;
    SVN_ERR(svn_repos_has_capability(b.repos, &supports_mergeinfo,
                                     SVN_REPOS_CAPABILITY_MERGEINFO, pool));

    SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "w(cc(!",
                                    "success", uuid, b.repos_url));
    if (supports_mergeinfo)
      SVN_ERR(svn_ra_svn__write_word(conn, pool, SVN_RA_SVN_CAP_MERGEINFO));
    SVN_ERR(svn_ra_svn__write_tuple(conn, pool, "!))"));
  }

  /* Set up editor shims. */
  {
    svn_delta_shim_callbacks_t *callbacks =
                                svn_delta_shim_callbacks_default(pool);

    callbacks->fetch_base_func = fetch_base_func;
    callbacks->fetch_props_func = fetch_props_func;
    callbacks->fetch_kind_func = fetch_kind_func;
    callbacks->fetch_baton = &b;

    SVN_ERR(svn_ra_svn__set_shim_callbacks(conn, callbacks));
  }

  return svn_ra_svn__handle_commands2(conn, pool, main_commands, &b, FALSE);
}
