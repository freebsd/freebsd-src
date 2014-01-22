/*
 * serf.c :  entry point for ra_serf
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



#define APR_WANT_STRFUNC
#include <apr_want.h>

#include <apr_uri.h>
#include <serf.h>

#include "svn_pools.h"
#include "svn_ra.h"
#include "svn_dav.h"
#include "svn_xml.h"
#include "../libsvn_ra/ra_loader.h"
#include "svn_config.h"
#include "svn_delta.h"
#include "svn_dirent_uri.h"
#include "svn_hash.h"
#include "svn_path.h"
#include "svn_time.h"
#include "svn_version.h"

#include "private/svn_dav_protocol.h"
#include "private/svn_dep_compat.h"
#include "private/svn_fspath.h"
#include "private/svn_subr_private.h"
#include "svn_private_config.h"

#include "ra_serf.h"


/* Implements svn_ra__vtable_t.get_version(). */
static const svn_version_t *
ra_serf_version(void)
{
  SVN_VERSION_BODY;
}

#define RA_SERF_DESCRIPTION \
    N_("Module for accessing a repository via WebDAV protocol using serf.")

#define RA_SERF_DESCRIPTION_VER \
    N_("Module for accessing a repository via WebDAV protocol using serf.\n" \
       "  - using serf %d.%d.%d")

/* Implements svn_ra__vtable_t.get_description(). */
static const char *
ra_serf_get_description(apr_pool_t *pool)
{
  int major, minor, patch;

  serf_lib_version(&major, &minor, &patch);
  return apr_psprintf(pool, _(RA_SERF_DESCRIPTION_VER), major, minor, patch);
}

/* Implements svn_ra__vtable_t.get_schemes(). */
static const char * const *
ra_serf_get_schemes(apr_pool_t *pool)
{
  static const char *serf_ssl[] = { "http", "https", NULL };
#if 0
  /* ### Temporary: to shut up a warning. */
  static const char *serf_no_ssl[] = { "http", NULL };
#endif

  /* TODO: Runtime detection. */
  return serf_ssl;
}

/* Load the setting http-auth-types from the global or server specific
   section, parse its value and set the types of authentication we should
   accept from the server. */
static svn_error_t *
load_http_auth_types(apr_pool_t *pool, svn_config_t *config,
                     const char *server_group,
                     int *authn_types)
{
  const char *http_auth_types = NULL;
  *authn_types = SERF_AUTHN_NONE;

  svn_config_get(config, &http_auth_types, SVN_CONFIG_SECTION_GLOBAL,
               SVN_CONFIG_OPTION_HTTP_AUTH_TYPES, NULL);

  if (server_group)
    {
      svn_config_get(config, &http_auth_types, server_group,
                     SVN_CONFIG_OPTION_HTTP_AUTH_TYPES, http_auth_types);
    }

  if (http_auth_types)
    {
      char *token;
      char *auth_types_list = apr_palloc(pool, strlen(http_auth_types) + 1);
      apr_collapse_spaces(auth_types_list, http_auth_types);
      while ((token = svn_cstring_tokenize(";", &auth_types_list)) != NULL)
        {
          if (svn_cstring_casecmp("basic", token) == 0)
            *authn_types |= SERF_AUTHN_BASIC;
          else if (svn_cstring_casecmp("digest", token) == 0)
            *authn_types |= SERF_AUTHN_DIGEST;
          else if (svn_cstring_casecmp("ntlm", token) == 0)
            *authn_types |= SERF_AUTHN_NTLM;
          else if (svn_cstring_casecmp("negotiate", token) == 0)
            *authn_types |= SERF_AUTHN_NEGOTIATE;
          else
            return svn_error_createf(SVN_ERR_BAD_CONFIG_VALUE, NULL,
                                     _("Invalid config: unknown %s "
                                       "'%s'"),
                                     SVN_CONFIG_OPTION_HTTP_AUTH_TYPES, token);
      }
    }
  else
    {
      /* Nothing specified by the user, so accept all types. */
      *authn_types = SERF_AUTHN_ALL;
    }

  return SVN_NO_ERROR;
}

/* Default HTTP timeout (in seconds); overridden by the 'http-timeout'
   runtime configuration variable. */
#define DEFAULT_HTTP_TIMEOUT 600

/* Private symbol for the 1.9-public SVN_CONFIG_OPTION_HTTP_CHUNKED_REQUESTS */
#define OPTION_HTTP_CHUNKED_REQUESTS "http-chunked-requests"


static svn_error_t *
load_config(svn_ra_serf__session_t *session,
            apr_hash_t *config_hash,
            apr_pool_t *pool)
{
  svn_config_t *config, *config_client;
  const char *server_group;
  const char *proxy_host = NULL;
  const char *port_str = NULL;
  const char *timeout_str = NULL;
  const char *exceptions;
  apr_port_t proxy_port;
  svn_tristate_t chunked_requests;

  if (config_hash)
    {
      config = svn_hash_gets(config_hash, SVN_CONFIG_CATEGORY_SERVERS);
      config_client = svn_hash_gets(config_hash, SVN_CONFIG_CATEGORY_CONFIG);
    }
  else
    {
      config = NULL;
      config_client = NULL;
    }

  SVN_ERR(svn_config_get_bool(config, &session->using_compression,
                              SVN_CONFIG_SECTION_GLOBAL,
                              SVN_CONFIG_OPTION_HTTP_COMPRESSION, TRUE));
  svn_config_get(config, &timeout_str, SVN_CONFIG_SECTION_GLOBAL,
                 SVN_CONFIG_OPTION_HTTP_TIMEOUT, NULL);

  if (session->wc_callbacks->auth_baton)
    {
      if (config_client)
        {
          svn_auth_set_parameter(session->wc_callbacks->auth_baton,
                                 SVN_AUTH_PARAM_CONFIG_CATEGORY_CONFIG,
                                 config_client);
        }
      if (config)
        {
          svn_auth_set_parameter(session->wc_callbacks->auth_baton,
                                 SVN_AUTH_PARAM_CONFIG_CATEGORY_SERVERS,
                                 config);
        }
    }

  /* Use the default proxy-specific settings if and only if
     "http-proxy-exceptions" is not set to exclude this host. */
  svn_config_get(config, &exceptions, SVN_CONFIG_SECTION_GLOBAL,
                 SVN_CONFIG_OPTION_HTTP_PROXY_EXCEPTIONS, "");
  if (! svn_cstring_match_glob_list(session->session_url.hostname,
                                    svn_cstring_split(exceptions, ",",
                                                      TRUE, pool)))
    {
      svn_config_get(config, &proxy_host, SVN_CONFIG_SECTION_GLOBAL,
                     SVN_CONFIG_OPTION_HTTP_PROXY_HOST, NULL);
      svn_config_get(config, &port_str, SVN_CONFIG_SECTION_GLOBAL,
                     SVN_CONFIG_OPTION_HTTP_PROXY_PORT, NULL);
      svn_config_get(config, &session->proxy_username,
                     SVN_CONFIG_SECTION_GLOBAL,
                     SVN_CONFIG_OPTION_HTTP_PROXY_USERNAME, NULL);
      svn_config_get(config, &session->proxy_password,
                     SVN_CONFIG_SECTION_GLOBAL,
                     SVN_CONFIG_OPTION_HTTP_PROXY_PASSWORD, NULL);
    }

  /* Load the global ssl settings, if set. */
  SVN_ERR(svn_config_get_bool(config, &session->trust_default_ca,
                              SVN_CONFIG_SECTION_GLOBAL,
                              SVN_CONFIG_OPTION_SSL_TRUST_DEFAULT_CA,
                              TRUE));
  svn_config_get(config, &session->ssl_authorities, SVN_CONFIG_SECTION_GLOBAL,
                 SVN_CONFIG_OPTION_SSL_AUTHORITY_FILES, NULL);

  /* If set, read the flag that tells us to do bulk updates or not. Defaults
     to skelta updates. */
  SVN_ERR(svn_config_get_tristate(config, &session->bulk_updates,
                                  SVN_CONFIG_SECTION_GLOBAL,
                                  SVN_CONFIG_OPTION_HTTP_BULK_UPDATES,
                                  "auto",
                                  svn_tristate_unknown));

  /* Load the maximum number of parallel session connections. */
  SVN_ERR(svn_config_get_int64(config, &session->max_connections,
                               SVN_CONFIG_SECTION_GLOBAL,
                               SVN_CONFIG_OPTION_HTTP_MAX_CONNECTIONS,
                               SVN_CONFIG_DEFAULT_OPTION_HTTP_MAX_CONNECTIONS));

  /* Should we use chunked transfer encoding. */ 
  SVN_ERR(svn_config_get_tristate(config, &chunked_requests,
                                  SVN_CONFIG_SECTION_GLOBAL,
                                  OPTION_HTTP_CHUNKED_REQUESTS,
                                  "auto", svn_tristate_unknown));

  if (config)
    server_group = svn_config_find_group(config,
                                         session->session_url.hostname,
                                         SVN_CONFIG_SECTION_GROUPS, pool);
  else
    server_group = NULL;

  if (server_group)
    {
      SVN_ERR(svn_config_get_bool(config, &session->using_compression,
                                  server_group,
                                  SVN_CONFIG_OPTION_HTTP_COMPRESSION,
                                  session->using_compression));
      svn_config_get(config, &timeout_str, server_group,
                     SVN_CONFIG_OPTION_HTTP_TIMEOUT, timeout_str);

      svn_auth_set_parameter(session->wc_callbacks->auth_baton,
                             SVN_AUTH_PARAM_SERVER_GROUP, server_group);

      /* Load the group proxy server settings, overriding global
         settings.  We intentionally ignore 'http-proxy-exceptions'
         here because, well, if this site was an exception, why is
         there a per-server proxy configuration for it?  */
      svn_config_get(config, &proxy_host, server_group,
                     SVN_CONFIG_OPTION_HTTP_PROXY_HOST, proxy_host);
      svn_config_get(config, &port_str, server_group,
                     SVN_CONFIG_OPTION_HTTP_PROXY_PORT, port_str);
      svn_config_get(config, &session->proxy_username, server_group,
                     SVN_CONFIG_OPTION_HTTP_PROXY_USERNAME,
                     session->proxy_username);
      svn_config_get(config, &session->proxy_password, server_group,
                     SVN_CONFIG_OPTION_HTTP_PROXY_PASSWORD,
                     session->proxy_password);

      /* Load the group ssl settings. */
      SVN_ERR(svn_config_get_bool(config, &session->trust_default_ca,
                                  server_group,
                                  SVN_CONFIG_OPTION_SSL_TRUST_DEFAULT_CA,
                                  session->trust_default_ca));
      svn_config_get(config, &session->ssl_authorities, server_group,
                     SVN_CONFIG_OPTION_SSL_AUTHORITY_FILES,
                     session->ssl_authorities);

      /* Load the group bulk updates flag. */
      SVN_ERR(svn_config_get_tristate(config, &session->bulk_updates,
                                      server_group,
                                      SVN_CONFIG_OPTION_HTTP_BULK_UPDATES,
                                      "auto",
                                      session->bulk_updates));

      /* Load the maximum number of parallel session connections,
         overriding global values. */
      SVN_ERR(svn_config_get_int64(config, &session->max_connections,
                                   server_group,
                                   SVN_CONFIG_OPTION_HTTP_MAX_CONNECTIONS,
                                   session->max_connections));

      /* Should we use chunked transfer encoding. */ 
      SVN_ERR(svn_config_get_tristate(config, &chunked_requests,
                                      server_group,
                                      OPTION_HTTP_CHUNKED_REQUESTS,
                                      "auto", chunked_requests));
    }

  /* Don't allow the http-max-connections value to be larger than our
     compiled-in limit, or to be too small to operate.  Broken
     functionality and angry administrators are equally undesirable. */
  if (session->max_connections > SVN_RA_SERF__MAX_CONNECTIONS_LIMIT)
    session->max_connections = SVN_RA_SERF__MAX_CONNECTIONS_LIMIT;
  if (session->max_connections < 2)
    session->max_connections = 2;

  /* Parse the connection timeout value, if any. */
  session->timeout = apr_time_from_sec(DEFAULT_HTTP_TIMEOUT);
  if (timeout_str)
    {
      char *endstr;
      const long int timeout = strtol(timeout_str, &endstr, 10);

      if (*endstr)
        return svn_error_create(SVN_ERR_BAD_CONFIG_VALUE, NULL,
                                _("Invalid config: illegal character in "
                                  "timeout value"));
      if (timeout < 0)
        return svn_error_create(SVN_ERR_BAD_CONFIG_VALUE, NULL,
                                _("Invalid config: negative timeout value"));
      session->timeout = apr_time_from_sec(timeout);
    }
  SVN_ERR_ASSERT(session->timeout >= 0);

  /* Convert the proxy port value, if any. */
  if (port_str)
    {
      char *endstr;
      const long int port = strtol(port_str, &endstr, 10);

      if (*endstr)
        return svn_error_create(SVN_ERR_RA_ILLEGAL_URL, NULL,
                                _("Invalid URL: illegal character in proxy "
                                  "port number"));
      if (port < 0)
        return svn_error_create(SVN_ERR_RA_ILLEGAL_URL, NULL,
                                _("Invalid URL: negative proxy port number"));
      if (port > 65535)
        return svn_error_create(SVN_ERR_RA_ILLEGAL_URL, NULL,
                                _("Invalid URL: proxy port number greater "
                                  "than maximum TCP port number 65535"));
      proxy_port = (apr_port_t) port;
    }
  else
    {
      proxy_port = 80;
    }

  if (proxy_host)
    {
      apr_sockaddr_t *proxy_addr;
      apr_status_t status;

      status = apr_sockaddr_info_get(&proxy_addr, proxy_host,
                                     APR_UNSPEC, proxy_port, 0,
                                     session->pool);
      if (status)
        {
          return svn_ra_serf__wrap_err(
                   status, _("Could not resolve proxy server '%s'"),
                   proxy_host);
        }
      session->using_proxy = TRUE;
      serf_config_proxy(session->context, proxy_addr);
    }
  else
    {
      session->using_proxy = FALSE;
    }

  /* Setup detect_chunking and using_chunked_requests based on
   * the chunked_requests tristate */
  if (chunked_requests == svn_tristate_unknown)
    {
      session->detect_chunking = TRUE;
      session->using_chunked_requests = TRUE;
    }
  else if (chunked_requests == svn_tristate_true)
    {
      session->detect_chunking = FALSE;
      session->using_chunked_requests = TRUE;
    }
  else /* chunked_requests == svn_tristate_false */
    {
      session->detect_chunking = FALSE;
      session->using_chunked_requests = FALSE;
    }

  /* Setup authentication. */
  SVN_ERR(load_http_auth_types(pool, config, server_group,
                               &session->authn_types));
  serf_config_authn_types(session->context, session->authn_types);
  serf_config_credentials_callback(session->context,
                                   svn_ra_serf__credentials_callback);

  return SVN_NO_ERROR;
}
#undef DEFAULT_HTTP_TIMEOUT

static void
svn_ra_serf__progress(void *progress_baton, apr_off_t read, apr_off_t written)
{
  const svn_ra_serf__session_t *serf_sess = progress_baton;
  if (serf_sess->progress_func)
    {
      serf_sess->progress_func(read + written, -1,
                               serf_sess->progress_baton,
                               serf_sess->pool);
    }
}

/** Our User-Agent string. */
static const char *
get_user_agent_string(apr_pool_t *pool)
{
  int major, minor, patch;
  serf_lib_version(&major, &minor, &patch);

  return apr_psprintf(pool, "SVN/%s (%s) serf/%d.%d.%d",
                      SVN_VER_NUMBER, SVN_BUILD_TARGET,
                      major, minor, patch);
}

/* Implements svn_ra__vtable_t.open_session(). */
static svn_error_t *
svn_ra_serf__open(svn_ra_session_t *session,
                  const char **corrected_url,
                  const char *session_URL,
                  const svn_ra_callbacks2_t *callbacks,
                  void *callback_baton,
                  apr_hash_t *config,
                  apr_pool_t *pool)
{
  apr_status_t status;
  svn_ra_serf__session_t *serf_sess;
  apr_uri_t url;
  const char *client_string = NULL;
  svn_error_t *err;

  if (corrected_url)
    *corrected_url = NULL;

  serf_sess = apr_pcalloc(pool, sizeof(*serf_sess));
  serf_sess->pool = svn_pool_create(pool);
  serf_sess->wc_callbacks = callbacks;
  serf_sess->wc_callback_baton = callback_baton;
  serf_sess->progress_func = callbacks->progress_func;
  serf_sess->progress_baton = callbacks->progress_baton;
  serf_sess->cancel_func = callbacks->cancel_func;
  serf_sess->cancel_baton = callback_baton;

  /* todo: reuse serf context across sessions */
  serf_sess->context = serf_context_create(serf_sess->pool);

  SVN_ERR(svn_ra_serf__blncache_create(&serf_sess->blncache,
                                       serf_sess->pool));


  status = apr_uri_parse(serf_sess->pool, session_URL, &url);
  if (status)
    {
      return svn_error_createf(SVN_ERR_RA_ILLEGAL_URL, NULL,
                               _("Illegal URL '%s'"),
                               session_URL);
    }
  /* Depending the version of apr-util in use, for root paths url.path
     will be NULL or "", where serf requires "/". */
  if (url.path == NULL || url.path[0] == '\0')
    {
      url.path = apr_pstrdup(serf_sess->pool, "/");
    }
  if (!url.port)
    {
      url.port = apr_uri_port_of_scheme(url.scheme);
    }
  serf_sess->session_url = url;
  serf_sess->session_url_str = apr_pstrdup(serf_sess->pool, session_URL);
  serf_sess->using_ssl = (svn_cstring_casecmp(url.scheme, "https") == 0);

  serf_sess->supports_deadprop_count = svn_tristate_unknown;

  serf_sess->capabilities = apr_hash_make(serf_sess->pool);

  /* We have to assume that the server only supports HTTP/1.0. Once it's clear
     HTTP/1.1 is supported, we can upgrade. */
  serf_sess->http10 = TRUE;

  /* If we switch to HTTP/1.1, then we will use chunked requests. We may disable
     this, if we find an intervening proxy does not support chunked requests.  */
  serf_sess->using_chunked_requests = TRUE;

  SVN_ERR(load_config(serf_sess, config, serf_sess->pool));

  serf_sess->conns[0] = apr_pcalloc(serf_sess->pool,
                                    sizeof(*serf_sess->conns[0]));
  serf_sess->conns[0]->bkt_alloc =
          serf_bucket_allocator_create(serf_sess->pool, NULL, NULL);
  serf_sess->conns[0]->session = serf_sess;
  serf_sess->conns[0]->last_status_code = -1;

  /* create the user agent string */
  if (callbacks->get_client_string)
    SVN_ERR(callbacks->get_client_string(callback_baton, &client_string, pool));

  if (client_string)
    serf_sess->useragent = apr_pstrcat(pool, get_user_agent_string(pool), " ",
                                       client_string, (char *)NULL);
  else
    serf_sess->useragent = get_user_agent_string(pool);

  /* go ahead and tell serf about the connection. */
  status =
    serf_connection_create2(&serf_sess->conns[0]->conn,
                            serf_sess->context,
                            url,
                            svn_ra_serf__conn_setup, serf_sess->conns[0],
                            svn_ra_serf__conn_closed, serf_sess->conns[0],
                            serf_sess->pool);
  if (status)
    return svn_ra_serf__wrap_err(status, NULL);

  /* Set the progress callback. */
  serf_context_set_progress_cb(serf_sess->context, svn_ra_serf__progress,
                               serf_sess);

  serf_sess->num_conns = 1;

  session->priv = serf_sess;

  err = svn_ra_serf__exchange_capabilities(serf_sess, corrected_url, pool);

  /* serf should produce a usable error code instead of APR_EGENERAL */
  if (err && err->apr_err == APR_EGENERAL)
    err = svn_error_createf(SVN_ERR_RA_DAV_REQUEST_FAILED, err,
                            _("Connection to '%s' failed"), session_URL);
  SVN_ERR(err);

  /* We have set up a useful connection (that doesn't indication a redirect).
     If we've been told there is possibly a worrisome proxy in our path to the
     server AND we switched to HTTP/1.1 (chunked requests), then probe for
     problems in any proxy.  */
  if ((corrected_url == NULL || *corrected_url == NULL)
      && serf_sess->detect_chunking && !serf_sess->http10)
    SVN_ERR(svn_ra_serf__probe_proxy(serf_sess, pool));

  return SVN_NO_ERROR;
}

/* Implements svn_ra__vtable_t.reparent(). */
static svn_error_t *
svn_ra_serf__reparent(svn_ra_session_t *ra_session,
                      const char *url,
                      apr_pool_t *pool)
{
  svn_ra_serf__session_t *session = ra_session->priv;
  apr_uri_t new_url;
  apr_status_t status;

  /* If it's the URL we already have, wave our hands and do nothing. */
  if (strcmp(session->session_url_str, url) == 0)
    {
      return SVN_NO_ERROR;
    }

  if (!session->repos_root_str)
    {
      const char *vcc_url;
      SVN_ERR(svn_ra_serf__discover_vcc(&vcc_url, session, NULL, pool));
    }

  if (!svn_uri__is_ancestor(session->repos_root_str, url))
    {
      return svn_error_createf(
          SVN_ERR_RA_ILLEGAL_URL, NULL,
          _("URL '%s' is not a child of the session's repository root "
            "URL '%s'"), url, session->repos_root_str);
    }

  status = apr_uri_parse(pool, url, &new_url);
  if (status)
    {
      return svn_error_createf(SVN_ERR_RA_ILLEGAL_URL, NULL,
                               _("Illegal repository URL '%s'"), url);
    }

  /* Depending the version of apr-util in use, for root paths url.path
     will be NULL or "", where serf requires "/". */
  /* ### Maybe we should use a string buffer for these strings so we
     ### don't allocate memory in the session on every reparent? */
  if (new_url.path == NULL || new_url.path[0] == '\0')
    {
      session->session_url.path = apr_pstrdup(session->pool, "/");
    }
  else
    {
      session->session_url.path = apr_pstrdup(session->pool, new_url.path);
    }
  session->session_url_str = apr_pstrdup(session->pool, url);

  return SVN_NO_ERROR;
}

/* Implements svn_ra__vtable_t.get_session_url(). */
static svn_error_t *
svn_ra_serf__get_session_url(svn_ra_session_t *ra_session,
                             const char **url,
                             apr_pool_t *pool)
{
  svn_ra_serf__session_t *session = ra_session->priv;
  *url = apr_pstrdup(pool, session->session_url_str);
  return SVN_NO_ERROR;
}

/* Implements svn_ra__vtable_t.get_latest_revnum(). */
static svn_error_t *
svn_ra_serf__get_latest_revnum(svn_ra_session_t *ra_session,
                               svn_revnum_t *latest_revnum,
                               apr_pool_t *pool)
{
  svn_ra_serf__session_t *session = ra_session->priv;

  return svn_error_trace(svn_ra_serf__get_youngest_revnum(
                           latest_revnum, session, pool));
}

/* Implements svn_ra__vtable_t.rev_proplist(). */
static svn_error_t *
svn_ra_serf__rev_proplist(svn_ra_session_t *ra_session,
                          svn_revnum_t rev,
                          apr_hash_t **ret_props,
                          apr_pool_t *pool)
{
  svn_ra_serf__session_t *session = ra_session->priv;
  apr_hash_t *props;
  const char *propfind_path;

  if (SVN_RA_SERF__HAVE_HTTPV2_SUPPORT(session))
    {
      propfind_path = apr_psprintf(pool, "%s/%ld", session->rev_stub, rev);

      /* svn_ra_serf__retrieve_props() wants to added the revision as
         a Label to the PROPFIND, which isn't really necessary when
         querying a rev-stub URI.  *Shrug*  Probably okay to leave the
         Label, but whatever. */
      rev = SVN_INVALID_REVNUM;
    }
  else
    {
      /* Use the VCC as the propfind target path. */
      SVN_ERR(svn_ra_serf__discover_vcc(&propfind_path, session, NULL, pool));
    }

  /* ### fix: fetch hash of *just* the PATH@REV props. no nested hash.  */
  SVN_ERR(svn_ra_serf__retrieve_props(&props, session, session->conns[0],
                                      propfind_path, rev, "0", all_props,
                                      pool, pool));

  SVN_ERR(svn_ra_serf__select_revprops(ret_props, propfind_path, rev, props,
                                       pool, pool));

  return SVN_NO_ERROR;
}

/* Implements svn_ra__vtable_t.rev_prop(). */
static svn_error_t *
svn_ra_serf__rev_prop(svn_ra_session_t *session,
                      svn_revnum_t rev,
                      const char *name,
                      svn_string_t **value,
                      apr_pool_t *pool)
{
  apr_hash_t *props;

  SVN_ERR(svn_ra_serf__rev_proplist(session, rev, &props, pool));

  *value = svn_hash_gets(props, name);

  return SVN_NO_ERROR;
}

static svn_error_t *
fetch_path_props(apr_hash_t **props,
                 svn_ra_serf__session_t *session,
                 const char *session_relpath,
                 svn_revnum_t revision,
                 const svn_ra_serf__dav_props_t *desired_props,
                 apr_pool_t *result_pool,
                 apr_pool_t *scratch_pool)
{
  const char *url;

  url = session->session_url.path;

  /* If we have a relative path, append it. */
  if (session_relpath)
    url = svn_path_url_add_component2(url, session_relpath, scratch_pool);

  /* If we were given a specific revision, get a URL that refers to that
     specific revision (rather than floating with HEAD).  */
  if (SVN_IS_VALID_REVNUM(revision))
    {
      SVN_ERR(svn_ra_serf__get_stable_url(&url, NULL /* latest_revnum */,
                                          session, NULL /* conn */,
                                          url, revision,
                                          scratch_pool, scratch_pool));
    }

  /* URL is stable, so we use SVN_INVALID_REVNUM since it is now irrelevant.
     Or we started with SVN_INVALID_REVNUM and URL may be floating.  */
  SVN_ERR(svn_ra_serf__fetch_node_props(props, session->conns[0],
                                        url, SVN_INVALID_REVNUM,
                                        desired_props,
                                        result_pool, scratch_pool));

  return SVN_NO_ERROR;
}

/* Implements svn_ra__vtable_t.check_path(). */
static svn_error_t *
svn_ra_serf__check_path(svn_ra_session_t *ra_session,
                        const char *rel_path,
                        svn_revnum_t revision,
                        svn_node_kind_t *kind,
                        apr_pool_t *pool)
{
  svn_ra_serf__session_t *session = ra_session->priv;
  apr_hash_t *props;

  svn_error_t *err = fetch_path_props(&props, session, rel_path,
                                      revision, check_path_props,
                                      pool, pool);

  if (err && err->apr_err == SVN_ERR_FS_NOT_FOUND)
    {
      svn_error_clear(err);
      *kind = svn_node_none;
    }
  else
    {
      /* Any other error, raise to caller. */
      if (err)
        return svn_error_trace(err);

      SVN_ERR(svn_ra_serf__get_resource_type(kind, props));
    }

  return SVN_NO_ERROR;
}


struct dirent_walker_baton_t {
  /* Update the fields in this entry.  */
  svn_dirent_t *entry;

  svn_tristate_t *supports_deadprop_count;

  /* If allocations are necessary, then use this pool.  */
  apr_pool_t *result_pool;
};

static svn_error_t *
dirent_walker(void *baton,
              const char *ns,
              const char *name,
              const svn_string_t *val,
              apr_pool_t *scratch_pool)
{
  struct dirent_walker_baton_t *dwb = baton;

  if (strcmp(ns, SVN_DAV_PROP_NS_CUSTOM) == 0)
    {
      dwb->entry->has_props = TRUE;
    }
  else if (strcmp(ns, SVN_DAV_PROP_NS_SVN) == 0)
    {
      dwb->entry->has_props = TRUE;
    }
  else if (strcmp(ns, SVN_DAV_PROP_NS_DAV) == 0)
    {
      if(strcmp(name, "deadprop-count") == 0)
        {
          if (*val->data)
            {
              apr_int64_t deadprop_count;
              SVN_ERR(svn_cstring_atoi64(&deadprop_count, val->data));
              dwb->entry->has_props = deadprop_count > 0;
              if (dwb->supports_deadprop_count)
                *dwb->supports_deadprop_count = svn_tristate_true;
            }
          else if (dwb->supports_deadprop_count)
            *dwb->supports_deadprop_count = svn_tristate_false;
        }
    }
  else if (strcmp(ns, "DAV:") == 0)
    {
      if (strcmp(name, SVN_DAV__VERSION_NAME) == 0)
        {
          dwb->entry->created_rev = SVN_STR_TO_REV(val->data);
        }
      else if (strcmp(name, "creator-displayname") == 0)
        {
          dwb->entry->last_author = val->data;
        }
      else if (strcmp(name, SVN_DAV__CREATIONDATE) == 0)
        {
          SVN_ERR(svn_time_from_cstring(&dwb->entry->time,
                                        val->data,
                                        dwb->result_pool));
        }
      else if (strcmp(name, "getcontentlength") == 0)
        {
          /* 'getcontentlength' property is empty for directories. */
          if (val->len)
            {
              SVN_ERR(svn_cstring_atoi64(&dwb->entry->size, val->data));
            }
        }
      else if (strcmp(name, "resourcetype") == 0)
        {
          if (strcmp(val->data, "collection") == 0)
            {
              dwb->entry->kind = svn_node_dir;
            }
          else
            {
              dwb->entry->kind = svn_node_file;
            }
        }
    }

  return SVN_NO_ERROR;
}

struct path_dirent_visitor_t {
  apr_hash_t *full_paths;
  apr_hash_t *base_paths;
  const char *orig_path;
  svn_tristate_t supports_deadprop_count;
  apr_pool_t *result_pool;
};

static svn_error_t *
path_dirent_walker(void *baton,
                   const char *path, apr_ssize_t path_len,
                   const char *ns, apr_ssize_t ns_len,
                   const char *name, apr_ssize_t name_len,
                   const svn_string_t *val,
                   apr_pool_t *pool)
{
  struct path_dirent_visitor_t *dirents = baton;
  struct dirent_walker_baton_t dwb;
  svn_dirent_t *entry;

  /* Skip our original path. */
  if (strcmp(path, dirents->orig_path) == 0)
    {
      return SVN_NO_ERROR;
    }

  entry = apr_hash_get(dirents->full_paths, path, path_len);

  if (!entry)
    {
      const char *base_name;

      entry = svn_dirent_create(pool);

      apr_hash_set(dirents->full_paths, path, path_len, entry);

      base_name = svn_path_uri_decode(svn_urlpath__basename(path, pool),
                                      pool);

      svn_hash_sets(dirents->base_paths, base_name, entry);
    }

  dwb.entry = entry;
  dwb.supports_deadprop_count = &dirents->supports_deadprop_count;
  dwb.result_pool = dirents->result_pool;
  return svn_error_trace(dirent_walker(&dwb, ns, name, val, pool));
}

static const svn_ra_serf__dav_props_t *
get_dirent_props(apr_uint32_t dirent_fields,
                 svn_ra_serf__session_t *session,
                 apr_pool_t *pool)
{
  svn_ra_serf__dav_props_t *prop;
  apr_array_header_t *props = apr_array_make
    (pool, 7, sizeof(svn_ra_serf__dav_props_t));

  if (session->supports_deadprop_count != svn_tristate_false
      || ! (dirent_fields & SVN_DIRENT_HAS_PROPS))
    {
      if (dirent_fields & SVN_DIRENT_KIND)
        {
          prop = apr_array_push(props);
          prop->namespace = "DAV:";
          prop->name = "resourcetype";
        }

      if (dirent_fields & SVN_DIRENT_SIZE)
        {
          prop = apr_array_push(props);
          prop->namespace = "DAV:";
          prop->name = "getcontentlength";
        }

      if (dirent_fields & SVN_DIRENT_HAS_PROPS)
        {
          prop = apr_array_push(props);
          prop->namespace = SVN_DAV_PROP_NS_DAV;
          prop->name = "deadprop-count";
        }

      if (dirent_fields & SVN_DIRENT_CREATED_REV)
        {
          svn_ra_serf__dav_props_t *p = apr_array_push(props);
          p->namespace = "DAV:";
          p->name = SVN_DAV__VERSION_NAME;
        }

      if (dirent_fields & SVN_DIRENT_TIME)
        {
          prop = apr_array_push(props);
          prop->namespace = "DAV:";
          prop->name = SVN_DAV__CREATIONDATE;
        }

      if (dirent_fields & SVN_DIRENT_LAST_AUTHOR)
        {
          prop = apr_array_push(props);
          prop->namespace = "DAV:";
          prop->name = "creator-displayname";
        }
    }
  else
    {
      /* We found an old subversion server that can't handle
         the deadprop-count property in the way we expect.

         The neon behavior is to retrieve all properties in this case */
      prop = apr_array_push(props);
      prop->namespace = "DAV:";
      prop->name = "allprop";
    }

  prop = apr_array_push(props);
  prop->namespace = NULL;
  prop->name = NULL;

  return (svn_ra_serf__dav_props_t *) props->elts;
}

/* Implements svn_ra__vtable_t.stat(). */
static svn_error_t *
svn_ra_serf__stat(svn_ra_session_t *ra_session,
                  const char *rel_path,
                  svn_revnum_t revision,
                  svn_dirent_t **dirent,
                  apr_pool_t *pool)
{
  svn_ra_serf__session_t *session = ra_session->priv;
  apr_hash_t *props;
  svn_error_t *err;
  struct dirent_walker_baton_t dwb;
  svn_tristate_t deadprop_count = svn_tristate_unknown;

  err = fetch_path_props(&props,
                         session, rel_path, revision,
                         get_dirent_props(SVN_DIRENT_ALL, session, pool),
                         pool, pool);
  if (err)
    {
      if (err->apr_err == SVN_ERR_FS_NOT_FOUND)
        {
          svn_error_clear(err);
          *dirent = NULL;
          return SVN_NO_ERROR;
        }
      else
        return svn_error_trace(err);
    }

  dwb.entry = svn_dirent_create(pool);
  dwb.supports_deadprop_count = &deadprop_count;
  dwb.result_pool = pool;
  SVN_ERR(svn_ra_serf__walk_node_props(props, dirent_walker, &dwb, pool));

  if (deadprop_count == svn_tristate_false
      && session->supports_deadprop_count == svn_tristate_unknown
      && !dwb.entry->has_props)
    {
      /* We have to requery as the server didn't give us the right
         information */
      session->supports_deadprop_count = svn_tristate_false;

      SVN_ERR(fetch_path_props(&props,
                               session, rel_path, SVN_INVALID_REVNUM,
                               get_dirent_props(SVN_DIRENT_ALL, session, pool),
                               pool, pool));

      SVN_ERR(svn_ra_serf__walk_node_props(props, dirent_walker, &dwb, pool));
    }

  if (deadprop_count != svn_tristate_unknown)
    session->supports_deadprop_count = deadprop_count;

  *dirent = dwb.entry;

  return SVN_NO_ERROR;
}

/* Reads the 'resourcetype' property from the list PROPS and checks if the
 * resource at PATH@REVISION really is a directory. Returns
 * SVN_ERR_FS_NOT_DIRECTORY if not.
 */
static svn_error_t *
resource_is_directory(apr_hash_t *props)
{
  svn_node_kind_t kind;

  SVN_ERR(svn_ra_serf__get_resource_type(&kind, props));

  if (kind != svn_node_dir)
    {
      return svn_error_create(SVN_ERR_FS_NOT_DIRECTORY, NULL,
                              _("Can't get entries of non-directory"));
    }

  return SVN_NO_ERROR;
}

/* Implements svn_ra__vtable_t.get_dir(). */
static svn_error_t *
svn_ra_serf__get_dir(svn_ra_session_t *ra_session,
                     apr_hash_t **dirents,
                     svn_revnum_t *fetched_rev,
                     apr_hash_t **ret_props,
                     const char *rel_path,
                     svn_revnum_t revision,
                     apr_uint32_t dirent_fields,
                     apr_pool_t *pool)
{
  svn_ra_serf__session_t *session = ra_session->priv;
  const char *path;

  path = session->session_url.path;

  /* If we have a relative path, URI encode and append it. */
  if (rel_path)
    {
      path = svn_path_url_add_component2(path, rel_path, pool);
    }

  /* If the user specified a peg revision other than HEAD, we have to fetch
     the baseline collection url for that revision. If not, we can use the
     public url. */
  if (SVN_IS_VALID_REVNUM(revision) || fetched_rev)
    {
      SVN_ERR(svn_ra_serf__get_stable_url(&path, fetched_rev,
                                          session, NULL /* conn */,
                                          path, revision,
                                          pool, pool));
      revision = SVN_INVALID_REVNUM;
    }
  /* REVISION is always SVN_INVALID_REVNUM  */
  SVN_ERR_ASSERT(!SVN_IS_VALID_REVNUM(revision));

  /* If we're asked for children, fetch them now. */
  if (dirents)
    {
      struct path_dirent_visitor_t dirent_walk;
      apr_hash_t *props;
      const char *rtype;

      /* Always request node kind to check that path is really a
       * directory.
       */
      dirent_fields |= SVN_DIRENT_KIND;
      SVN_ERR(svn_ra_serf__retrieve_props(&props, session, session->conns[0],
                                          path, SVN_INVALID_REVNUM, "1",
                                          get_dirent_props(dirent_fields,
                                                           session, pool),
                                          pool, pool));

      /* Check if the path is really a directory. */
      rtype = svn_ra_serf__get_prop(props, path, "DAV:", "resourcetype");
      if (rtype == NULL || strcmp(rtype, "collection") != 0)
        return svn_error_create(SVN_ERR_FS_NOT_DIRECTORY, NULL,
                                _("Can't get entries of non-directory"));

      /* We're going to create two hashes to help the walker along.
       * We're going to return the 2nd one back to the caller as it
       * will have the basenames it expects.
       */
      dirent_walk.full_paths = apr_hash_make(pool);
      dirent_walk.base_paths = apr_hash_make(pool);
      dirent_walk.orig_path = svn_urlpath__canonicalize(path, pool);
      dirent_walk.supports_deadprop_count = svn_tristate_unknown;
      dirent_walk.result_pool = pool;

      SVN_ERR(svn_ra_serf__walk_all_paths(props, SVN_INVALID_REVNUM,
                                          path_dirent_walker, &dirent_walk,
                                          pool));

      if (dirent_walk.supports_deadprop_count == svn_tristate_false
          && session->supports_deadprop_count == svn_tristate_unknown
          && dirent_fields & SVN_DIRENT_HAS_PROPS)
        {
          /* We have to requery as the server didn't give us the right
             information */
          session->supports_deadprop_count = svn_tristate_false;
          SVN_ERR(svn_ra_serf__retrieve_props(&props, session,
                                              session->conns[0],
                                              path, SVN_INVALID_REVNUM, "1",
                                              get_dirent_props(dirent_fields,
                                                               session, pool),
                                              pool, pool));

          apr_hash_clear(dirent_walk.full_paths);
          apr_hash_clear(dirent_walk.base_paths);

          SVN_ERR(svn_ra_serf__walk_all_paths(props, SVN_INVALID_REVNUM,
                                              path_dirent_walker,
                                              &dirent_walk, pool));
        }

      *dirents = dirent_walk.base_paths;

      if (dirent_walk.supports_deadprop_count != svn_tristate_unknown)
        session->supports_deadprop_count = dirent_walk.supports_deadprop_count;
    }

  /* If we're asked for the directory properties, fetch them too. */
  if (ret_props)
    {
      apr_hash_t *props;

      SVN_ERR(svn_ra_serf__fetch_node_props(&props, session->conns[0],
                                            path, SVN_INVALID_REVNUM,
                                            all_props,
                                            pool, pool));

      /* Check if the path is really a directory. */
      SVN_ERR(resource_is_directory(props));

      /* ### flatten_props() does not copy PROPVALUE, but fetch_node_props()
         ### put them into POOL, so we're okay.  */
      SVN_ERR(svn_ra_serf__flatten_props(ret_props, props, pool, pool));
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_serf__get_repos_root(svn_ra_session_t *ra_session,
                            const char **url,
                            apr_pool_t *pool)
{
  svn_ra_serf__session_t *session = ra_session->priv;

  if (!session->repos_root_str)
    {
      const char *vcc_url;
      SVN_ERR(svn_ra_serf__discover_vcc(&vcc_url, session, NULL, pool));
    }

  *url = session->repos_root_str;
  return SVN_NO_ERROR;
}

/* TODO: to fetch the uuid from the repository, we need:
   1. a path that exists in HEAD
   2. a path that's readable

   get_uuid handles the case where a path doesn't exist in HEAD and also the
   case where the root of the repository is not readable.
   However, it does not handle the case where we're fetching path not existing
   in HEAD of a repository with unreadable root directory.

   Implements svn_ra__vtable_t.get_uuid().
 */
static svn_error_t *
svn_ra_serf__get_uuid(svn_ra_session_t *ra_session,
                      const char **uuid,
                      apr_pool_t *pool)
{
  svn_ra_serf__session_t *session = ra_session->priv;

  if (!session->uuid)
    {
      const char *vcc_url;

      /* We should never get here if we have HTTP v2 support, because
         any server with that support should be transmitting the
         UUID in the initial OPTIONS response.  */
      SVN_ERR_ASSERT(! SVN_RA_SERF__HAVE_HTTPV2_SUPPORT(session));

      /* We're not interested in vcc_url and relative_url, but this call also
         stores the repository's uuid in the session. */
      SVN_ERR(svn_ra_serf__discover_vcc(&vcc_url, session, NULL, pool));
      if (!session->uuid)
        {
          return svn_error_create(SVN_ERR_RA_DAV_RESPONSE_HEADER_BADNESS, NULL,
                                  _("The UUID property was not found on the "
                                    "resource or any of its parents"));
        }
    }

  *uuid = session->uuid;

  return SVN_NO_ERROR;
}


static const svn_ra__vtable_t serf_vtable = {
  ra_serf_version,
  ra_serf_get_description,
  ra_serf_get_schemes,
  svn_ra_serf__open,
  svn_ra_serf__reparent,
  svn_ra_serf__get_session_url,
  svn_ra_serf__get_latest_revnum,
  svn_ra_serf__get_dated_revision,
  svn_ra_serf__change_rev_prop,
  svn_ra_serf__rev_proplist,
  svn_ra_serf__rev_prop,
  svn_ra_serf__get_commit_editor,
  svn_ra_serf__get_file,
  svn_ra_serf__get_dir,
  svn_ra_serf__get_mergeinfo,
  svn_ra_serf__do_update,
  svn_ra_serf__do_switch,
  svn_ra_serf__do_status,
  svn_ra_serf__do_diff,
  svn_ra_serf__get_log,
  svn_ra_serf__check_path,
  svn_ra_serf__stat,
  svn_ra_serf__get_uuid,
  svn_ra_serf__get_repos_root,
  svn_ra_serf__get_locations,
  svn_ra_serf__get_location_segments,
  svn_ra_serf__get_file_revs,
  svn_ra_serf__lock,
  svn_ra_serf__unlock,
  svn_ra_serf__get_lock,
  svn_ra_serf__get_locks,
  svn_ra_serf__replay,
  svn_ra_serf__has_capability,
  svn_ra_serf__replay_range,
  svn_ra_serf__get_deleted_rev,
  svn_ra_serf__register_editor_shim_callbacks,
  svn_ra_serf__get_inherited_props
};

svn_error_t *
svn_ra_serf__init(const svn_version_t *loader_version,
                  const svn_ra__vtable_t **vtable,
                  apr_pool_t *pool)
{
  static const svn_version_checklist_t checklist[] =
    {
      { "svn_subr",  svn_subr_version },
      { "svn_delta", svn_delta_version },
      { NULL, NULL }
    };
  int serf_major;
  int serf_minor;
  int serf_patch;

  SVN_ERR(svn_ver_check_list2(ra_serf_version(), checklist, svn_ver_equal));

  /* Simplified version check to make sure we can safely use the
     VTABLE parameter. The RA loader does a more exhaustive check. */
  if (loader_version->major != SVN_VER_MAJOR)
    {
      return svn_error_createf(
         SVN_ERR_VERSION_MISMATCH, NULL,
         _("Unsupported RA loader version (%d) for ra_serf"),
         loader_version->major);
    }

  /* Make sure that we have loaded a compatible library: the MAJOR must
     match, and the minor must be at *least* what we compiled against.
     The patch level is simply ignored.  */
  serf_lib_version(&serf_major, &serf_minor, &serf_patch);
  if (serf_major != SERF_MAJOR_VERSION
      || serf_minor < SERF_MINOR_VERSION)
    {
      return svn_error_createf(
         /* ### should return a unique error  */
         SVN_ERR_VERSION_MISMATCH, NULL,
         _("ra_serf was compiled for serf %d.%d.%d but loaded "
           "an incompatible %d.%d.%d library"),
         SERF_MAJOR_VERSION, SERF_MINOR_VERSION, SERF_PATCH_VERSION,
         serf_major, serf_minor, serf_patch);
    }

  *vtable = &serf_vtable;

  return SVN_NO_ERROR;
}

/* Compatibility wrapper for pre-1.2 subversions.  Needed? */
#define NAME "ra_serf"
#define DESCRIPTION RA_SERF_DESCRIPTION
#define VTBL serf_vtable
#define INITFUNC svn_ra_serf__init
#define COMPAT_INITFUNC svn_ra_serf_init
#include "../libsvn_ra/wrapper_template.h"
