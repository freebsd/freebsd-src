/*
 * util.c : serf utility routines for ra_serf
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

#define APR_WANT_STRFUNC
#include <apr.h>
#include <apr_want.h>
#include <apr_fnmatch.h>

#include <serf.h>
#include <serf_bucket_types.h>

#include <expat.h>

#include "svn_hash.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_private_config.h"
#include "svn_string.h"
#include "svn_xml.h"
#include "svn_props.h"
#include "svn_dirent_uri.h"

#include "../libsvn_ra/ra_loader.h"
#include "private/svn_dep_compat.h"
#include "private/svn_fspath.h"
#include "private/svn_subr_private.h"

#include "ra_serf.h"


/* Fix for older expat 1.95.x's that do not define
 * XML_STATUS_OK/XML_STATUS_ERROR
 */
#ifndef XML_STATUS_OK
#define XML_STATUS_OK    1
#define XML_STATUS_ERROR 0
#endif

#ifndef XML_VERSION_AT_LEAST
#define XML_VERSION_AT_LEAST(major,minor,patch)                  \
(((major) < XML_MAJOR_VERSION)                                       \
 || ((major) == XML_MAJOR_VERSION && (minor) < XML_MINOR_VERSION)    \
 || ((major) == XML_MAJOR_VERSION && (minor) == XML_MINOR_VERSION && \
     (patch) <= XML_MICRO_VERSION))
#endif /* APR_VERSION_AT_LEAST */

#if XML_VERSION_AT_LEAST(1, 95, 8)
#define EXPAT_HAS_STOPPARSER
#endif

/* Read/write chunks of this size into the spillbuf.  */
#define PARSE_CHUNK_SIZE 8000

/* We will store one megabyte in memory, before switching to store content
   into a temporary file.  */
#define SPILL_SIZE 1000000


/* This structure records pending data for the parser in memory blocks,
   and possibly into a temporary file if "too much" content arrives.  */
struct svn_ra_serf__pending_t {
  /* The spillbuf where we record the pending data.  */
  svn_spillbuf_t *buf;

  /* This flag is set when the network has reached EOF. The PENDING
     processing can then properly detect when parsing has completed.  */
  svn_boolean_t network_eof;
};

#define HAS_PENDING_DATA(p) ((p) != NULL && (p)->buf != NULL \
                             && svn_spillbuf__get_size((p)->buf) != 0)


struct expat_ctx_t {
  svn_ra_serf__xml_context_t *xmlctx;
  XML_Parser parser;
  svn_ra_serf__handler_t *handler;

  svn_error_t *inner_error;

  /* Do not use this pool for allocation. It is merely recorded for running
     the cleanup handler.  */
  apr_pool_t *cleanup_pool;
};


static const apr_uint32_t serf_failure_map[][2] =
{
  { SERF_SSL_CERT_NOTYETVALID,   SVN_AUTH_SSL_NOTYETVALID },
  { SERF_SSL_CERT_EXPIRED,       SVN_AUTH_SSL_EXPIRED },
  { SERF_SSL_CERT_SELF_SIGNED,   SVN_AUTH_SSL_UNKNOWNCA },
  { SERF_SSL_CERT_UNKNOWNCA,     SVN_AUTH_SSL_UNKNOWNCA }
};

/* Return a Subversion failure mask based on FAILURES, a serf SSL
   failure mask.  If anything in FAILURES is not directly mappable to
   Subversion failures, set SVN_AUTH_SSL_OTHER in the returned mask. */
static apr_uint32_t
ssl_convert_serf_failures(int failures)
{
  apr_uint32_t svn_failures = 0;
  apr_size_t i;

  for (i = 0; i < sizeof(serf_failure_map) / (2 * sizeof(apr_uint32_t)); ++i)
    {
      if (failures & serf_failure_map[i][0])
        {
          svn_failures |= serf_failure_map[i][1];
          failures &= ~serf_failure_map[i][0];
        }
    }

  /* Map any remaining failure bits to our OTHER bit. */
  if (failures)
    {
      svn_failures |= SVN_AUTH_SSL_OTHER;
    }

  return svn_failures;
}


static apr_status_t
save_error(svn_ra_serf__session_t *session,
           svn_error_t *err)
{
  if (err || session->pending_error)
    {
      session->pending_error = svn_error_compose_create(
                                  session->pending_error,
                                  err);
      return session->pending_error->apr_err;
    }

  return APR_SUCCESS;
}


/* Construct the realmstring, e.g. https://svn.collab.net:443. */
static const char *
construct_realm(svn_ra_serf__session_t *session,
                apr_pool_t *pool)
{
  const char *realm;
  apr_port_t port;

  if (session->session_url.port_str)
    {
      port = session->session_url.port;
    }
  else
    {
      port = apr_uri_port_of_scheme(session->session_url.scheme);
    }

  realm = apr_psprintf(pool, "%s://%s:%d",
                       session->session_url.scheme,
                       session->session_url.hostname,
                       port);

  return realm;
}

/* Convert a hash table containing the fields (as documented in X.509) of an
   organisation to a string ORG, allocated in POOL. ORG is as returned by
   serf_ssl_cert_issuer() and serf_ssl_cert_subject(). */
static char *
convert_organisation_to_str(apr_hash_t *org, apr_pool_t *pool)
{
  const char *org_unit = svn_hash_gets(org, "OU");
  const char *org_name = svn_hash_gets(org, "O");
  const char *locality = svn_hash_gets(org, "L");
  const char *state = svn_hash_gets(org, "ST");
  const char *country = svn_hash_gets(org, "C");
  const char *email = svn_hash_gets(org, "E");
  svn_stringbuf_t *buf = svn_stringbuf_create_empty(pool);

  if (org_unit)
    {
      svn_stringbuf_appendcstr(buf, org_unit);
      svn_stringbuf_appendcstr(buf, ", ");
    }

  if (org_name)
    {
      svn_stringbuf_appendcstr(buf, org_name);
      svn_stringbuf_appendcstr(buf, ", ");
    }

  if (locality)
    {
      svn_stringbuf_appendcstr(buf, locality);
      svn_stringbuf_appendcstr(buf, ", ");
    }

  if (state)
    {
      svn_stringbuf_appendcstr(buf, state);
      svn_stringbuf_appendcstr(buf, ", ");
    }

  if (country)
    {
      svn_stringbuf_appendcstr(buf, country);
      svn_stringbuf_appendcstr(buf, ", ");
    }

  /* Chop ', ' if any. */
  svn_stringbuf_chop(buf, 2);

  if (email)
    {
      svn_stringbuf_appendcstr(buf, "(");
      svn_stringbuf_appendcstr(buf, email);
      svn_stringbuf_appendcstr(buf, ")");
    }

  return buf->data;
}

static void append_reason(svn_stringbuf_t *errmsg, const char *reason, int *reasons)
{
  if (*reasons < 1)
    svn_stringbuf_appendcstr(errmsg, _(": "));
  else
    svn_stringbuf_appendcstr(errmsg, _(", "));
  svn_stringbuf_appendcstr(errmsg, reason);
  (*reasons)++;
}

/* This function is called on receiving a ssl certificate of a server when
   opening a https connection. It allows Subversion to override the initial
   validation done by serf.
   Serf provides us the @a baton as provided in the call to
   serf_ssl_server_cert_callback_set. The result of serf's initial validation
   of the certificate @a CERT is returned as a bitmask in FAILURES. */
static svn_error_t *
ssl_server_cert(void *baton, int failures,
                const serf_ssl_certificate_t *cert,
                apr_pool_t *scratch_pool)
{
  svn_ra_serf__connection_t *conn = baton;
  svn_auth_ssl_server_cert_info_t cert_info;
  svn_auth_cred_ssl_server_trust_t *server_creds = NULL;
  svn_auth_iterstate_t *state;
  const char *realmstring;
  apr_uint32_t svn_failures;
  apr_hash_t *issuer, *subject, *serf_cert;
  apr_array_header_t *san;
  void *creds;
  int found_matching_hostname = 0;

  /* Implicitly approve any non-server certs. */
  if (serf_ssl_cert_depth(cert) > 0)
    {
      if (failures)
        conn->server_cert_failures |= ssl_convert_serf_failures(failures);
      return APR_SUCCESS;
    }

  /* Extract the info from the certificate */
  subject = serf_ssl_cert_subject(cert, scratch_pool);
  issuer = serf_ssl_cert_issuer(cert, scratch_pool);
  serf_cert = serf_ssl_cert_certificate(cert, scratch_pool);

  cert_info.hostname = svn_hash_gets(subject, "CN");
  san = svn_hash_gets(serf_cert, "subjectAltName");
  cert_info.fingerprint = svn_hash_gets(serf_cert, "sha1");
  if (! cert_info.fingerprint)
    cert_info.fingerprint = apr_pstrdup(scratch_pool, "<unknown>");
  cert_info.valid_from = svn_hash_gets(serf_cert, "notBefore");
  if (! cert_info.valid_from)
    cert_info.valid_from = apr_pstrdup(scratch_pool, "[invalid date]");
  cert_info.valid_until = svn_hash_gets(serf_cert, "notAfter");
  if (! cert_info.valid_until)
    cert_info.valid_until = apr_pstrdup(scratch_pool, "[invalid date]");
  cert_info.issuer_dname = convert_organisation_to_str(issuer, scratch_pool);
  cert_info.ascii_cert = serf_ssl_cert_export(cert, scratch_pool);

  svn_failures = (ssl_convert_serf_failures(failures)
                  | conn->server_cert_failures);

  /* Try to find matching server name via subjectAltName first... */
  if (san) {
      int i;
      for (i = 0; i < san->nelts; i++) {
          char *s = APR_ARRAY_IDX(san, i, char*);
          if (apr_fnmatch(s, conn->session->session_url.hostname,
                          APR_FNM_PERIOD | APR_FNM_CASE_BLIND) == APR_SUCCESS)
            {
              found_matching_hostname = 1;
              cert_info.hostname = s;
              break;
            }
      }
  }

  /* Match server certificate CN with the hostname of the server */
  if (!found_matching_hostname && cert_info.hostname)
    {
      if (apr_fnmatch(cert_info.hostname, conn->session->session_url.hostname,
                      APR_FNM_PERIOD | APR_FNM_CASE_BLIND) == APR_FNM_NOMATCH)
        {
          svn_failures |= SVN_AUTH_SSL_CNMISMATCH;
        }
    }

  svn_auth_set_parameter(conn->session->wc_callbacks->auth_baton,
                         SVN_AUTH_PARAM_SSL_SERVER_FAILURES,
                         &svn_failures);

  svn_auth_set_parameter(conn->session->wc_callbacks->auth_baton,
                         SVN_AUTH_PARAM_SSL_SERVER_CERT_INFO,
                         &cert_info);

  realmstring = construct_realm(conn->session, conn->session->pool);

  SVN_ERR(svn_auth_first_credentials(&creds, &state,
                                     SVN_AUTH_CRED_SSL_SERVER_TRUST,
                                     realmstring,
                                     conn->session->wc_callbacks->auth_baton,
                                     scratch_pool));
  if (creds)
    {
      server_creds = creds;
      SVN_ERR(svn_auth_save_credentials(state, scratch_pool));
    }

  svn_auth_set_parameter(conn->session->wc_callbacks->auth_baton,
                         SVN_AUTH_PARAM_SSL_SERVER_CERT_INFO, NULL);

  if (!server_creds)
    {
      svn_stringbuf_t *errmsg;
      int reasons = 0;

      errmsg = svn_stringbuf_create(
                 _("Server SSL certificate verification failed"),
                 scratch_pool);


      if (svn_failures & SVN_AUTH_SSL_NOTYETVALID)
        append_reason(errmsg, _("certificate is not yet valid"), &reasons);

      if (svn_failures & SVN_AUTH_SSL_EXPIRED)
        append_reason(errmsg, _("certificate has expired"), &reasons);

      if (svn_failures & SVN_AUTH_SSL_CNMISMATCH)
        append_reason(errmsg,
                      _("certificate issued for a different hostname"),
                      &reasons);

      if (svn_failures & SVN_AUTH_SSL_UNKNOWNCA)
        append_reason(errmsg, _("issuer is not trusted"), &reasons);

      if (svn_failures & SVN_AUTH_SSL_OTHER)
        append_reason(errmsg, _("and other reason(s)"), &reasons);

      return svn_error_create(SVN_ERR_RA_SERF_SSL_CERT_UNTRUSTED, NULL,
                              errmsg->data);
    }

  return SVN_NO_ERROR;
}

/* Implements serf_ssl_need_server_cert_t for ssl_server_cert */
static apr_status_t
ssl_server_cert_cb(void *baton, int failures,
                const serf_ssl_certificate_t *cert)
{
  svn_ra_serf__connection_t *conn = baton;
  svn_ra_serf__session_t *session = conn->session;
  apr_pool_t *subpool;
  svn_error_t *err;

  subpool = svn_pool_create(session->pool);
  err = svn_error_trace(ssl_server_cert(baton, failures, cert, subpool));
  svn_pool_destroy(subpool);

  return save_error(session, err);
}

static svn_error_t *
load_authorities(svn_ra_serf__connection_t *conn, const char *authorities,
                 apr_pool_t *pool)
{
  apr_array_header_t *files = svn_cstring_split(authorities, ";",
                                                TRUE /* chop_whitespace */,
                                                pool);
  int i;

  for (i = 0; i < files->nelts; ++i)
    {
      const char *file = APR_ARRAY_IDX(files, i, const char *);
      serf_ssl_certificate_t *ca_cert;
      apr_status_t status = serf_ssl_load_cert_file(&ca_cert, file, pool);

      if (status == APR_SUCCESS)
        status = serf_ssl_trust_cert(conn->ssl_context, ca_cert);

      if (status != APR_SUCCESS)
        {
          return svn_error_createf(SVN_ERR_BAD_CONFIG_VALUE, NULL,
             _("Invalid config: unable to load certificate file '%s'"),
             svn_dirent_local_style(file, pool));
        }
    }

  return SVN_NO_ERROR;
}

static svn_error_t *
conn_setup(apr_socket_t *sock,
           serf_bucket_t **read_bkt,
           serf_bucket_t **write_bkt,
           void *baton,
           apr_pool_t *pool)
{
  svn_ra_serf__connection_t *conn = baton;

  *read_bkt = serf_context_bucket_socket_create(conn->session->context,
                                               sock, conn->bkt_alloc);

  if (conn->session->using_ssl)
    {
      /* input stream */
      *read_bkt = serf_bucket_ssl_decrypt_create(*read_bkt, conn->ssl_context,
                                                 conn->bkt_alloc);
      if (!conn->ssl_context)
        {
          conn->ssl_context = serf_bucket_ssl_encrypt_context_get(*read_bkt);

          serf_ssl_set_hostname(conn->ssl_context,
                                conn->session->session_url.hostname);

          serf_ssl_client_cert_provider_set(conn->ssl_context,
                                            svn_ra_serf__handle_client_cert,
                                            conn, conn->session->pool);
          serf_ssl_client_cert_password_set(conn->ssl_context,
                                            svn_ra_serf__handle_client_cert_pw,
                                            conn, conn->session->pool);
          serf_ssl_server_cert_callback_set(conn->ssl_context,
                                            ssl_server_cert_cb,
                                            conn);

          /* See if the user wants us to trust "default" openssl CAs. */
          if (conn->session->trust_default_ca)
            {
              serf_ssl_use_default_certificates(conn->ssl_context);
            }
          /* Are there custom CAs to load? */
          if (conn->session->ssl_authorities)
            {
              SVN_ERR(load_authorities(conn, conn->session->ssl_authorities,
                                       conn->session->pool));
            }
        }

      if (write_bkt)
        {
          /* output stream */
          *write_bkt = serf_bucket_ssl_encrypt_create(*write_bkt,
                                                      conn->ssl_context,
                                                      conn->bkt_alloc);
        }
    }

  return SVN_NO_ERROR;
}

/* svn_ra_serf__conn_setup is a callback for serf. This function
   creates a read bucket and will wrap the write bucket if SSL
   is needed. */
apr_status_t
svn_ra_serf__conn_setup(apr_socket_t *sock,
                        serf_bucket_t **read_bkt,
                        serf_bucket_t **write_bkt,
                        void *baton,
                        apr_pool_t *pool)
{
  svn_ra_serf__connection_t *conn = baton;
  svn_ra_serf__session_t *session = conn->session;
  svn_error_t *err;

  err = svn_error_trace(conn_setup(sock,
                                   read_bkt,
                                   write_bkt,
                                   baton,
                                   pool));
  return save_error(session, err);
}


/* Our default serf response acceptor.  */
static serf_bucket_t *
accept_response(serf_request_t *request,
                serf_bucket_t *stream,
                void *acceptor_baton,
                apr_pool_t *pool)
{
  serf_bucket_t *c;
  serf_bucket_alloc_t *bkt_alloc;

  bkt_alloc = serf_request_get_alloc(request);
  c = serf_bucket_barrier_create(stream, bkt_alloc);

  return serf_bucket_response_create(c, bkt_alloc);
}


/* Custom response acceptor for HEAD requests.  */
static serf_bucket_t *
accept_head(serf_request_t *request,
            serf_bucket_t *stream,
            void *acceptor_baton,
            apr_pool_t *pool)
{
  serf_bucket_t *response;

  response = accept_response(request, stream, acceptor_baton, pool);

  /* We know we shouldn't get a response body. */
  serf_bucket_response_set_head(response);

  return response;
}

static svn_error_t *
connection_closed(svn_ra_serf__connection_t *conn,
                  apr_status_t why,
                  apr_pool_t *pool)
{
  if (why)
    {
      return svn_error_wrap_apr(why, NULL);
    }

  if (conn->session->using_ssl)
    conn->ssl_context = NULL;

  return SVN_NO_ERROR;
}

void
svn_ra_serf__conn_closed(serf_connection_t *conn,
                         void *closed_baton,
                         apr_status_t why,
                         apr_pool_t *pool)
{
  svn_ra_serf__connection_t *ra_conn = closed_baton;
  svn_error_t *err;

  err = svn_error_trace(connection_closed(ra_conn, why, pool));

  (void) save_error(ra_conn->session, err);
}


/* Implementation of svn_ra_serf__handle_client_cert */
static svn_error_t *
handle_client_cert(void *data,
                   const char **cert_path,
                   apr_pool_t *pool)
{
    svn_ra_serf__connection_t *conn = data;
    svn_ra_serf__session_t *session = conn->session;
    const char *realm;
    void *creds;

    *cert_path = NULL;

    realm = construct_realm(session, session->pool);

    if (!conn->ssl_client_auth_state)
      {
        SVN_ERR(svn_auth_first_credentials(&creds,
                                           &conn->ssl_client_auth_state,
                                           SVN_AUTH_CRED_SSL_CLIENT_CERT,
                                           realm,
                                           session->wc_callbacks->auth_baton,
                                           pool));
      }
    else
      {
        SVN_ERR(svn_auth_next_credentials(&creds,
                                          conn->ssl_client_auth_state,
                                          session->pool));
      }

    if (creds)
      {
        svn_auth_cred_ssl_client_cert_t *client_creds;
        client_creds = creds;
        *cert_path = client_creds->cert_file;
      }

    return SVN_NO_ERROR;
}

/* Implements serf_ssl_need_client_cert_t for handle_client_cert */
apr_status_t svn_ra_serf__handle_client_cert(void *data,
                                             const char **cert_path)
{
  svn_ra_serf__connection_t *conn = data;
  svn_ra_serf__session_t *session = conn->session;
  svn_error_t *err;

  err = svn_error_trace(handle_client_cert(data, cert_path, session->pool));

  return save_error(session, err);
}

/* Implementation for svn_ra_serf__handle_client_cert_pw */
static svn_error_t *
handle_client_cert_pw(void *data,
                      const char *cert_path,
                      const char **password,
                      apr_pool_t *pool)
{
    svn_ra_serf__connection_t *conn = data;
    svn_ra_serf__session_t *session = conn->session;
    void *creds;

    *password = NULL;

    if (!conn->ssl_client_pw_auth_state)
      {
        SVN_ERR(svn_auth_first_credentials(&creds,
                                           &conn->ssl_client_pw_auth_state,
                                           SVN_AUTH_CRED_SSL_CLIENT_CERT_PW,
                                           cert_path,
                                           session->wc_callbacks->auth_baton,
                                           pool));
      }
    else
      {
        SVN_ERR(svn_auth_next_credentials(&creds,
                                          conn->ssl_client_pw_auth_state,
                                          pool));
      }

    if (creds)
      {
        svn_auth_cred_ssl_client_cert_pw_t *pw_creds;
        pw_creds = creds;
        *password = pw_creds->password;
      }

    return APR_SUCCESS;
}

/* Implements serf_ssl_need_client_cert_pw_t for handle_client_cert_pw */
apr_status_t svn_ra_serf__handle_client_cert_pw(void *data,
                                                const char *cert_path,
                                                const char **password)
{
  svn_ra_serf__connection_t *conn = data;
  svn_ra_serf__session_t *session = conn->session;
  svn_error_t *err;

  err = svn_error_trace(handle_client_cert_pw(data,
                                              cert_path,
                                              password,
                                              session->pool));

  return save_error(session, err);
}


/*
 * Given a REQUEST on connection CONN, construct a request bucket for it,
 * returning the bucket in *REQ_BKT.
 *
 * If HDRS_BKT is not-NULL, it will be set to a headers_bucket that
 * corresponds to the new request.
 *
 * The request will be METHOD at URL.
 *
 * If BODY_BKT is not-NULL, it will be sent as the request body.
 *
 * If CONTENT_TYPE is not-NULL, it will be sent as the Content-Type header.
 *
 * REQUEST_POOL should live for the duration of the request. Serf will
 * construct this and provide it to the request_setup callback, so we
 * should just use that one.
 */
static svn_error_t *
setup_serf_req(serf_request_t *request,
               serf_bucket_t **req_bkt,
               serf_bucket_t **hdrs_bkt,
               svn_ra_serf__session_t *session,
               const char *method, const char *url,
               serf_bucket_t *body_bkt, const char *content_type,
               const char *accept_encoding,
               apr_pool_t *request_pool,
               apr_pool_t *scratch_pool)
{
  serf_bucket_alloc_t *allocator = serf_request_get_alloc(request);

  svn_spillbuf_t *buf;
  svn_boolean_t set_CL = session->http10 || !session->using_chunked_requests;

  if (set_CL && body_bkt != NULL)
    {
      /* Ugh. Use HTTP/1.0 to talk to the server because we don't know if
         it speaks HTTP/1.1 (and thus, chunked requests), or because the
         server actually responded as only supporting HTTP/1.0.

         We'll take the existing body_bkt, spool it into a spillbuf, and
         then wrap a bucket around that spillbuf. The spillbuf will give
         us the Content-Length value.  */
      SVN_ERR(svn_ra_serf__copy_into_spillbuf(&buf, body_bkt,
                                              request_pool,
                                              scratch_pool));
      /* Destroy original bucket since it content is already copied
         to spillbuf. */
      serf_bucket_destroy(body_bkt);

      body_bkt = svn_ra_serf__create_sb_bucket(buf, allocator,
                                               request_pool,
                                               scratch_pool);
    }

  /* Create a request bucket.  Note that this sucker is kind enough to
     add a "Host" header for us.  */
  *req_bkt = serf_request_bucket_request_create(request, method, url,
                                                body_bkt, allocator);

  /* Set the Content-Length value. This will also trigger an HTTP/1.0
     request (rather than the default chunked request).  */
  if (set_CL)
    {
      if (body_bkt == NULL)
        serf_bucket_request_set_CL(*req_bkt, 0);
      else
        serf_bucket_request_set_CL(*req_bkt, svn_spillbuf__get_size(buf));
    }

  *hdrs_bkt = serf_bucket_request_get_headers(*req_bkt);

  /* We use serf_bucket_headers_setn() because the USERAGENT has a
     lifetime longer than this bucket. Thus, there is no need to copy
     the header values.  */
  serf_bucket_headers_setn(*hdrs_bkt, "User-Agent", session->useragent);

  if (content_type)
    {
      serf_bucket_headers_setn(*hdrs_bkt, "Content-Type", content_type);
    }

  if (session->http10)
    {
      serf_bucket_headers_setn(*hdrs_bkt, "Connection", "keep-alive");
    }

  if (accept_encoding)
    {
      serf_bucket_headers_setn(*hdrs_bkt, "Accept-Encoding", accept_encoding);
    }

  /* These headers need to be sent with every request; see issue #3255
     ("mod_dav_svn does not pass client capabilities to start-commit
     hooks") for why. */
  serf_bucket_headers_setn(*hdrs_bkt, "DAV", SVN_DAV_NS_DAV_SVN_DEPTH);
  serf_bucket_headers_setn(*hdrs_bkt, "DAV", SVN_DAV_NS_DAV_SVN_MERGEINFO);
  serf_bucket_headers_setn(*hdrs_bkt, "DAV", SVN_DAV_NS_DAV_SVN_LOG_REVPROPS);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_serf__context_run_wait(svn_boolean_t *done,
                              svn_ra_serf__session_t *sess,
                              apr_pool_t *scratch_pool)
{
  apr_pool_t *iterpool;
  apr_interval_time_t waittime_left = sess->timeout;

  assert(sess->pending_error == SVN_NO_ERROR);

  iterpool = svn_pool_create(scratch_pool);
  while (!*done)
    {
      apr_status_t status;
      svn_error_t *err;
      int i;

      svn_pool_clear(iterpool);

      if (sess->cancel_func)
        SVN_ERR((*sess->cancel_func)(sess->cancel_baton));

      status = serf_context_run(sess->context,
                                SVN_RA_SERF__CONTEXT_RUN_DURATION,
                                iterpool);

      err = sess->pending_error;
      sess->pending_error = SVN_NO_ERROR;

      /* If the context duration timeout is up, we'll subtract that
         duration from the total time alloted for such things.  If
         there's no time left, we fail with a message indicating that
         the connection timed out.  */
      if (APR_STATUS_IS_TIMEUP(status))
        {
          status = 0;

          if (sess->timeout)
            {
              if (waittime_left > SVN_RA_SERF__CONTEXT_RUN_DURATION)
                {
                  waittime_left -= SVN_RA_SERF__CONTEXT_RUN_DURATION;
                }
              else
                {
                  return
                      svn_error_compose_create(
                            err,
                            svn_error_create(SVN_ERR_RA_DAV_CONN_TIMEOUT, NULL,
                                             _("Connection timed out")));
                }
            }
        }
      else
        {
          waittime_left = sess->timeout;
        }

      SVN_ERR(err);
      if (status)
        {
          if (status >= SVN_ERR_BAD_CATEGORY_START && status < SVN_ERR_LAST)
            {
              /* apr can't translate subversion errors to text */
              SVN_ERR_W(svn_error_create(status, NULL, NULL),
                        _("Error running context"));
            }

          return svn_ra_serf__wrap_err(status, _("Error running context"));
        }

      /* Debugging purposes only! */
      for (i = 0; i < sess->num_conns; i++)
        {
          serf_debug__closed_conn(sess->conns[i]->bkt_alloc);
        }
    }
  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}


svn_error_t *
svn_ra_serf__context_run_one(svn_ra_serf__handler_t *handler,
                             apr_pool_t *scratch_pool)
{
  svn_error_t *err;

  /* Create a serf request based on HANDLER.  */
  svn_ra_serf__request_create(handler);

  /* Wait until the response logic marks its DONE status.  */
  err = svn_ra_serf__context_run_wait(&handler->done, handler->session,
                                      scratch_pool);

  /* A callback invocation has been canceled. In this simple case of
     context_run_one, we can keep the ra-session operational by resetting
     the connection.

     If we don't do this, the next context run will notice that the connection
     is still in the error state and will just return SVN_ERR_CEASE_INVOCATION
     (=the last error for the connection) again  */
  if (err && err->apr_err == SVN_ERR_CEASE_INVOCATION)
    {
      apr_status_t status = serf_connection_reset(handler->conn->conn);

      if (status)
        err = svn_error_compose_create(err,
                                       svn_ra_serf__wrap_err(status, NULL));
    }

  if (handler->server_error)
    {
      err = svn_error_compose_create(err, handler->server_error->error);
      handler->server_error = NULL;
    }

  return svn_error_trace(err);
}


/*
 * Expat callback invoked on a start element tag for an error response.
 */
static svn_error_t *
start_error(svn_ra_serf__xml_parser_t *parser,
            svn_ra_serf__dav_props_t name,
            const char **attrs,
            apr_pool_t *scratch_pool)
{
  svn_ra_serf__server_error_t *ctx = parser->user_data;

  if (!ctx->in_error &&
      strcmp(name.namespace, "DAV:") == 0 &&
      strcmp(name.name, "error") == 0)
    {
      ctx->in_error = TRUE;
    }
  else if (ctx->in_error && strcmp(name.name, "human-readable") == 0)
    {
      const char *err_code;

      err_code = svn_xml_get_attr_value("errcode", attrs);
      if (err_code)
        {
          apr_int64_t val;

          SVN_ERR(svn_cstring_atoi64(&val, err_code));
          ctx->error->apr_err = (apr_status_t)val;
        }

      /* If there's no error code provided, or if the provided code is
         0 (which can happen sometimes depending on how the error is
         constructed on the server-side), just pick a generic error
         code to run with. */
      if (! ctx->error->apr_err)
        {
          ctx->error->apr_err = SVN_ERR_RA_DAV_REQUEST_FAILED;
        }

      /* Start collecting cdata. */
      svn_stringbuf_setempty(ctx->cdata);
      ctx->collect_cdata = TRUE;
    }

  return SVN_NO_ERROR;
}

/*
 * Expat callback invoked on an end element tag for a PROPFIND response.
 */
static svn_error_t *
end_error(svn_ra_serf__xml_parser_t *parser,
          svn_ra_serf__dav_props_t name,
          apr_pool_t *scratch_pool)
{
  svn_ra_serf__server_error_t *ctx = parser->user_data;

  if (ctx->in_error &&
      strcmp(name.namespace, "DAV:") == 0 &&
      strcmp(name.name, "error") == 0)
    {
      ctx->in_error = FALSE;
    }
  if (ctx->in_error && strcmp(name.name, "human-readable") == 0)
    {
      /* On the server dav_error_response_tag() will add a leading
         and trailing newline if DEBUG_CR is defined in mod_dav.h,
         so remove any such characters here. */
      svn_stringbuf_strip_whitespace(ctx->cdata);

      ctx->error->message = apr_pstrmemdup(ctx->error->pool, ctx->cdata->data,
                                           ctx->cdata->len);
      ctx->collect_cdata = FALSE;
    }

  return SVN_NO_ERROR;
}

/*
 * Expat callback invoked on CDATA elements in an error response.
 *
 * This callback can be called multiple times.
 */
static svn_error_t *
cdata_error(svn_ra_serf__xml_parser_t *parser,
            const char *data,
            apr_size_t len,
            apr_pool_t *scratch_pool)
{
  svn_ra_serf__server_error_t *ctx = parser->user_data;

  if (ctx->collect_cdata)
    {
      svn_stringbuf_appendbytes(ctx->cdata, data, len);
    }

  return SVN_NO_ERROR;
}


static apr_status_t
drain_bucket(serf_bucket_t *bucket)
{
  /* Read whatever is in the bucket, and just drop it.  */
  while (1)
    {
      apr_status_t status;
      const char *data;
      apr_size_t len;

      status = serf_bucket_read(bucket, SERF_READ_ALL_AVAIL, &data, &len);
      if (status)
        return status;
    }
}


static svn_ra_serf__server_error_t *
begin_error_parsing(svn_ra_serf__xml_start_element_t start,
                    svn_ra_serf__xml_end_element_t end,
                    svn_ra_serf__xml_cdata_chunk_handler_t cdata,
                    apr_pool_t *result_pool)
{
  svn_ra_serf__server_error_t *server_err;

  server_err = apr_pcalloc(result_pool, sizeof(*server_err));
  server_err->error = svn_error_create(APR_SUCCESS, NULL, NULL);
  server_err->contains_precondition_error = FALSE;
  server_err->cdata = svn_stringbuf_create_empty(server_err->error->pool);
  server_err->collect_cdata = FALSE;
  server_err->parser.pool = server_err->error->pool;
  server_err->parser.user_data = server_err;
  server_err->parser.start = start;
  server_err->parser.end = end;
  server_err->parser.cdata = cdata;
  server_err->parser.ignore_errors = TRUE;

  return server_err;
}

/* Implements svn_ra_serf__response_handler_t */
svn_error_t *
svn_ra_serf__handle_discard_body(serf_request_t *request,
                                 serf_bucket_t *response,
                                 void *baton,
                                 apr_pool_t *pool)
{
  apr_status_t status;

  status = drain_bucket(response);
  if (status)
    return svn_ra_serf__wrap_err(status, NULL);

  return SVN_NO_ERROR;
}

apr_status_t
svn_ra_serf__response_discard_handler(serf_request_t *request,
                                      serf_bucket_t *response,
                                      void *baton,
                                      apr_pool_t *pool)
{
  return drain_bucket(response);
}


/* Return the value of the RESPONSE's Location header if any, or NULL
   otherwise.  */
static const char *
response_get_location(serf_bucket_t *response,
                      const char *base_url,
                      apr_pool_t *result_pool,
                      apr_pool_t *scratch_pool)
{
  serf_bucket_t *headers;
  const char *location;

  headers = serf_bucket_response_get_headers(response);
  location = serf_bucket_headers_get(headers, "Location");
  if (location == NULL)
    return NULL;

  /* The RFCs say we should have received a full url in LOCATION, but
     older apache versions and many custom web handlers just return a
     relative path here...

     And we can't trust anything because it is network data.
   */
  if (*location == '/')
    {
      apr_uri_t uri;
      apr_status_t status;

      status = apr_uri_parse(scratch_pool, base_url, &uri);

      if (status != APR_SUCCESS)
        return NULL;

      /* Replace the path path with what we got */
      uri.path = (char*)svn_urlpath__canonicalize(location, scratch_pool);

      /* And make APR produce a proper full url for us */
      location = apr_uri_unparse(scratch_pool, &uri, 0);

      /* Fall through to ensure our canonicalization rules */
    }
  else if (!svn_path_is_url(location))
    {
      return NULL; /* Any other formats we should support? */
    }

  return svn_uri_canonicalize(location, result_pool);
}


/* Implements svn_ra_serf__response_handler_t */
svn_error_t *
svn_ra_serf__expect_empty_body(serf_request_t *request,
                               serf_bucket_t *response,
                               void *baton,
                               apr_pool_t *scratch_pool)
{
  svn_ra_serf__handler_t *handler = baton;
  serf_bucket_t *hdrs;
  const char *val;

  /* This function is just like handle_multistatus_only() except for the
     XML parsing callbacks. We want to look for the human-readable element.  */

  /* We should see this just once, in order to initialize SERVER_ERROR.
     At that point, the core error processing will take over. If we choose
     not to parse an error, then we'll never return here (because we
     change the response handler).  */
  SVN_ERR_ASSERT(handler->server_error == NULL);

  hdrs = serf_bucket_response_get_headers(response);
  val = serf_bucket_headers_get(hdrs, "Content-Type");
  if (val && strncasecmp(val, "text/xml", sizeof("text/xml") - 1) == 0)
    {
      svn_ra_serf__server_error_t *server_err;

      server_err = begin_error_parsing(start_error, end_error, cdata_error,
                                       handler->handler_pool);

      /* Get the parser to set our DONE flag.  */
      server_err->parser.done = &handler->done;

      handler->server_error = server_err;
    }
  else
    {
      /* The body was not text/xml, so we don't know what to do with it.
         Toss anything that arrives.  */
      handler->discard_body = TRUE;
    }

  /* Returning SVN_NO_ERROR will return APR_SUCCESS to serf, which tells it
     to call the response handler again. That will start up the XML parsing,
     or it will be dropped on the floor (per the decision above).  */
  return SVN_NO_ERROR;
}


/* Given a string like "HTTP/1.1 500 (status)" in BUF, parse out the numeric
   status code into *STATUS_CODE_OUT.  Ignores leading whitespace. */
static svn_error_t *
parse_dav_status(int *status_code_out, svn_stringbuf_t *buf,
                 apr_pool_t *scratch_pool)
{
  svn_error_t *err;
  const char *token;
  char *tok_status;
  svn_stringbuf_t *temp_buf = svn_stringbuf_dup(buf, scratch_pool);

  svn_stringbuf_strip_whitespace(temp_buf);
  token = apr_strtok(temp_buf->data, " \t\r\n", &tok_status);
  if (token)
    token = apr_strtok(NULL, " \t\r\n", &tok_status);
  if (!token)
    return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
                             _("Malformed DAV:status CDATA '%s'"),
                             buf->data);
  err = svn_cstring_atoi(status_code_out, token);
  if (err)
    return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, err,
                             _("Malformed DAV:status CDATA '%s'"),
                             buf->data);

  return SVN_NO_ERROR;
}

/*
 * Expat callback invoked on a start element tag for a 207 response.
 */
static svn_error_t *
start_207(svn_ra_serf__xml_parser_t *parser,
          svn_ra_serf__dav_props_t name,
          const char **attrs,
          apr_pool_t *scratch_pool)
{
  svn_ra_serf__server_error_t *ctx = parser->user_data;

  if (!ctx->in_error &&
      strcmp(name.namespace, "DAV:") == 0 &&
      strcmp(name.name, "multistatus") == 0)
    {
      ctx->in_error = TRUE;
    }
  else if (ctx->in_error && strcmp(name.name, "responsedescription") == 0)
    {
      /* Start collecting cdata. */
      svn_stringbuf_setempty(ctx->cdata);
      ctx->collect_cdata = TRUE;
    }
  else if (ctx->in_error &&
           strcmp(name.namespace, "DAV:") == 0 &&
           strcmp(name.name, "status") == 0)
    {
      /* Start collecting cdata. */
      svn_stringbuf_setempty(ctx->cdata);
      ctx->collect_cdata = TRUE;
    }

  return SVN_NO_ERROR;
}

/*
 * Expat callback invoked on an end element tag for a 207 response.
 */
static svn_error_t *
end_207(svn_ra_serf__xml_parser_t *parser,
        svn_ra_serf__dav_props_t name,
        apr_pool_t *scratch_pool)
{
  svn_ra_serf__server_error_t *ctx = parser->user_data;

  if (ctx->in_error &&
      strcmp(name.namespace, "DAV:") == 0 &&
      strcmp(name.name, "multistatus") == 0)
    {
      ctx->in_error = FALSE;
    }
  if (ctx->in_error && strcmp(name.name, "responsedescription") == 0)
    {
      /* Remove leading newline added by DEBUG_CR on server */
      svn_stringbuf_strip_whitespace(ctx->cdata);

      ctx->collect_cdata = FALSE;
      ctx->error->message = apr_pstrmemdup(ctx->error->pool, ctx->cdata->data,
                                           ctx->cdata->len);
      if (ctx->contains_precondition_error)
        ctx->error->apr_err = SVN_ERR_FS_PROP_BASEVALUE_MISMATCH;
      else
        ctx->error->apr_err = SVN_ERR_RA_DAV_REQUEST_FAILED;
    }
  else if (ctx->in_error &&
           strcmp(name.namespace, "DAV:") == 0 &&
           strcmp(name.name, "status") == 0)
    {
      int status_code;

      ctx->collect_cdata = FALSE;

      SVN_ERR(parse_dav_status(&status_code, ctx->cdata, parser->pool));
      if (status_code == 412)
        ctx->contains_precondition_error = TRUE;
    }

  return SVN_NO_ERROR;
}

/*
 * Expat callback invoked on CDATA elements in a 207 response.
 *
 * This callback can be called multiple times.
 */
static svn_error_t *
cdata_207(svn_ra_serf__xml_parser_t *parser,
          const char *data,
          apr_size_t len,
          apr_pool_t *scratch_pool)
{
  svn_ra_serf__server_error_t *ctx = parser->user_data;

  if (ctx->collect_cdata)
    {
      svn_stringbuf_appendbytes(ctx->cdata, data, len);
    }

  return SVN_NO_ERROR;
}

/* Implements svn_ra_serf__response_handler_t */
svn_error_t *
svn_ra_serf__handle_multistatus_only(serf_request_t *request,
                                     serf_bucket_t *response,
                                     void *baton,
                                     apr_pool_t *scratch_pool)
{
  svn_ra_serf__handler_t *handler = baton;

  /* This function is just like expect_empty_body() except for the
     XML parsing callbacks. We are looking for very limited pieces of
     the multistatus response.  */

  /* We should see this just once, in order to initialize SERVER_ERROR.
     At that point, the core error processing will take over. If we choose
     not to parse an error, then we'll never return here (because we
     change the response handler).  */
  SVN_ERR_ASSERT(handler->server_error == NULL);

    {
      serf_bucket_t *hdrs;
      const char *val;

      hdrs = serf_bucket_response_get_headers(response);
      val = serf_bucket_headers_get(hdrs, "Content-Type");
      if (val && strncasecmp(val, "text/xml", sizeof("text/xml") - 1) == 0)
        {
          svn_ra_serf__server_error_t *server_err;

          server_err = begin_error_parsing(start_207, end_207, cdata_207,
                                           handler->handler_pool);

          /* Get the parser to set our DONE flag.  */
          server_err->parser.done = &handler->done;

          handler->server_error = server_err;
        }
      else
        {
          /* The body was not text/xml, so we don't know what to do with it.
             Toss anything that arrives.  */
          handler->discard_body = TRUE;
        }
    }

  /* Returning SVN_NO_ERROR will return APR_SUCCESS to serf, which tells it
     to call the response handler again. That will start up the XML parsing,
     or it will be dropped on the floor (per the decision above).  */
  return SVN_NO_ERROR;
}


/* Conforms to Expat's XML_StartElementHandler  */
static void
start_xml(void *userData, const char *raw_name, const char **attrs)
{
  svn_ra_serf__xml_parser_t *parser = userData;
  svn_ra_serf__dav_props_t name;
  apr_pool_t *scratch_pool;
  svn_error_t *err;

  if (parser->error)
    return;

  if (!parser->state)
    svn_ra_serf__xml_push_state(parser, 0);

  /* ### get a real scratch_pool  */
  scratch_pool = parser->state->pool;

  svn_ra_serf__define_ns(&parser->state->ns_list, attrs, parser->state->pool);

  svn_ra_serf__expand_ns(&name, parser->state->ns_list, raw_name);

  err = parser->start(parser, name, attrs, scratch_pool);
  if (err && !SERF_BUCKET_READ_ERROR(err->apr_err))
    err = svn_error_create(SVN_ERR_RA_SERF_WRAPPED_ERROR, err, NULL);

  parser->error = err;
}


/* Conforms to Expat's XML_EndElementHandler  */
static void
end_xml(void *userData, const char *raw_name)
{
  svn_ra_serf__xml_parser_t *parser = userData;
  svn_ra_serf__dav_props_t name;
  svn_error_t *err;
  apr_pool_t *scratch_pool;

  if (parser->error)
    return;

  /* ### get a real scratch_pool  */
  scratch_pool = parser->state->pool;

  svn_ra_serf__expand_ns(&name, parser->state->ns_list, raw_name);

  err = parser->end(parser, name, scratch_pool);
  if (err && !SERF_BUCKET_READ_ERROR(err->apr_err))
    err = svn_error_create(SVN_ERR_RA_SERF_WRAPPED_ERROR, err, NULL);

  parser->error = err;
}


/* Conforms to Expat's XML_CharacterDataHandler  */
static void
cdata_xml(void *userData, const char *data, int len)
{
  svn_ra_serf__xml_parser_t *parser = userData;
  svn_error_t *err;
  apr_pool_t *scratch_pool;

  if (parser->error)
    return;

  if (!parser->state)
    svn_ra_serf__xml_push_state(parser, 0);

  /* ### get a real scratch_pool  */
  scratch_pool = parser->state->pool;

  err = parser->cdata(parser, data, len, scratch_pool);
  if (err && !SERF_BUCKET_READ_ERROR(err->apr_err))
    err = svn_error_create(SVN_ERR_RA_SERF_WRAPPED_ERROR, err, NULL);

  parser->error = err;
}

/* Flip the requisite bits in CTX to indicate that processing of the
   response is complete, adding the current "done item" to the list of
   completed items. */
static void
add_done_item(svn_ra_serf__xml_parser_t *ctx)
{
  /* Make sure we don't add to DONE_LIST twice.  */
  if (!*ctx->done)
    {
      *ctx->done = TRUE;
      if (ctx->done_list)
        {
          ctx->done_item->data = ctx->user_data;
          ctx->done_item->next = *ctx->done_list;
          *ctx->done_list = ctx->done_item;
        }
    }
}


static svn_error_t *
write_to_pending(svn_ra_serf__xml_parser_t *ctx,
                 const char *data,
                 apr_size_t len,
                 apr_pool_t *scratch_pool)
{
  if (ctx->pending == NULL)
    {
      ctx->pending = apr_pcalloc(ctx->pool, sizeof(*ctx->pending));
      ctx->pending->buf = svn_spillbuf__create(PARSE_CHUNK_SIZE,
                                               SPILL_SIZE,
                                               ctx->pool);
    }

  /* Copy the data into one or more chunks in the spill buffer.  */
  return svn_error_trace(svn_spillbuf__write(ctx->pending->buf,
                                             data, len,
                                             scratch_pool));
}


static svn_error_t *
inject_to_parser(svn_ra_serf__xml_parser_t *ctx,
                 const char *data,
                 apr_size_t len,
                 const serf_status_line *sl)
{
  int xml_status;

  xml_status = XML_Parse(ctx->xmlp, data, (int) len, 0);

  if (! ctx->ignore_errors)
    {
      SVN_ERR(ctx->error);

      if (xml_status != XML_STATUS_OK)
        {
          if (sl == NULL)
            return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
                                     _("XML parsing failed"));

          return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
                                   _("XML parsing failed: (%d %s)"),
                                   sl->code, sl->reason);
        }
     }

  return SVN_NO_ERROR;
}

/* Apr pool cleanup handler to release an XML_Parser in success and error
   conditions */
static apr_status_t
xml_parser_cleanup(void *baton)
{
  XML_Parser *xmlp = baton;

  if (*xmlp)
    {
      (void) XML_ParserFree(*xmlp);
      *xmlp = NULL;
    }

  return APR_SUCCESS;
}

/* Limit the amount of pending content to parse at once to < 100KB per
   iteration. This number is chosen somewhat arbitrarely. Making it lower
   will have a drastical negative impact on performance, whereas increasing it
   increases the risk for connection timeouts.
 */
#define PENDING_TO_PARSE PARSE_CHUNK_SIZE * 5

svn_error_t *
svn_ra_serf__process_pending(svn_ra_serf__xml_parser_t *parser,
                             svn_boolean_t *network_eof,
                             apr_pool_t *scratch_pool)
{
  svn_boolean_t pending_empty = FALSE;
  apr_size_t cur_read = 0;

  /* Fast path exit: already paused, nothing to do, or already done.  */
  if (parser->paused || parser->pending == NULL || *parser->done)
    {
      *network_eof = parser->pending ? parser->pending->network_eof : FALSE;
      return SVN_NO_ERROR;
    }

  /* Parsing the pending conten in the spillbuf will result in many disc i/o
     operations. This can be so slow that we don't run the network event
     processing loop often enough, resulting in timed out connections.

     So we limit the amounts of bytes parsed per iteration.
   */
  while (cur_read < PENDING_TO_PARSE)
    {
      const char *data;
      apr_size_t len;

      /* Get a block of content, stopping the loop when we run out.  */
      SVN_ERR(svn_spillbuf__read(&data, &len, parser->pending->buf,
                             scratch_pool));
      if (data)
        {
          /* Inject the content into the XML parser.  */
          SVN_ERR(inject_to_parser(parser, data, len, NULL));

          /* If the XML parsing callbacks paused us, then we're done for now.  */
          if (parser->paused)
            break;

          cur_read += len;
        }
      else
        {
          /* The buffer is empty. */
          pending_empty = TRUE;
          break;
        }
    }

  /* If the PENDING structures are empty *and* we consumed all content from
     the network, then we're completely done with the parsing.  */
  if (pending_empty &&
      parser->pending->network_eof)
    {
      int xml_status;
      SVN_ERR_ASSERT(parser->xmlp != NULL);

      /* Tell the parser that no more content will be parsed. */
      xml_status = XML_Parse(parser->xmlp, NULL, 0, 1);

      apr_pool_cleanup_run(parser->pool, &parser->xmlp, xml_parser_cleanup);
      parser->xmlp = NULL;

      if (! parser->ignore_errors)
        {
          SVN_ERR(parser->error);

          if (xml_status != XML_STATUS_OK)
            {
              return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
                                       _("XML parsing failed"));
            }
        }

      add_done_item(parser);
    }

  *network_eof = parser->pending ? parser->pending->network_eof : FALSE;

  return SVN_NO_ERROR;
}
#undef PENDING_TO_PARSE


/* ### this is still broken conceptually. just shifting incrementally... */
static svn_error_t *
handle_server_error(serf_request_t *request,
                    serf_bucket_t *response,
                    apr_pool_t *scratch_pool)
{
  svn_ra_serf__server_error_t server_err = { 0 };
  serf_bucket_t *hdrs;
  const char *val;
  apr_status_t err;

  hdrs = serf_bucket_response_get_headers(response);
  val = serf_bucket_headers_get(hdrs, "Content-Type");
  if (val && strncasecmp(val, "text/xml", sizeof("text/xml") - 1) == 0)
    {
      /* ### we should figure out how to reuse begin_error_parsing  */

      server_err.error = svn_error_create(APR_SUCCESS, NULL, NULL);
      server_err.contains_precondition_error = FALSE;
      server_err.cdata = svn_stringbuf_create_empty(scratch_pool);
      server_err.collect_cdata = FALSE;
      server_err.parser.pool = server_err.error->pool;
      server_err.parser.user_data = &server_err;
      server_err.parser.start = start_error;
      server_err.parser.end = end_error;
      server_err.parser.cdata = cdata_error;
      server_err.parser.done = &server_err.done;
      server_err.parser.ignore_errors = TRUE;

      /* We don't care about any errors except for SERVER_ERR.ERROR  */
      svn_error_clear(svn_ra_serf__handle_xml_parser(request,
                                                     response,
                                                     &server_err.parser,
                                                     scratch_pool));

      /* ### checking DONE is silly. the above only parses whatever has
         ### been received at the network interface. totally wrong. but
         ### it is what we have for now (maintaining historical code),
         ### until we fully migrate.  */
      if (server_err.done && server_err.error->apr_err == APR_SUCCESS)
        {
          svn_error_clear(server_err.error);
          server_err.error = SVN_NO_ERROR;
        }

      return svn_error_trace(server_err.error);
    }

  /* The only error that we will return is from the XML response body.
     Otherwise, ignore the entire body but allow SUCCESS/EOF/EAGAIN to
     surface. */
  err = drain_bucket(response);
  if (err && !SERF_BUCKET_READ_ERROR(err))
    return svn_ra_serf__wrap_err(err, NULL);

  return SVN_NO_ERROR;
}


/* Implements svn_ra_serf__response_handler_t */
svn_error_t *
svn_ra_serf__handle_xml_parser(serf_request_t *request,
                               serf_bucket_t *response,
                               void *baton,
                               apr_pool_t *pool)
{
  serf_status_line sl;
  apr_status_t status;
  svn_ra_serf__xml_parser_t *ctx = baton;
  svn_error_t *err;

  /* ### get the HANDLER rather than fetching this.  */
  status = serf_bucket_response_status(response, &sl);
  if (SERF_BUCKET_READ_ERROR(status))
    {
      return svn_ra_serf__wrap_err(status, NULL);
    }

  /* Woo-hoo.  Nothing here to see.  */
  if (sl.code == 404 && !ctx->ignore_errors)
    {
      err = handle_server_error(request, response, pool);

      if (err && APR_STATUS_IS_EOF(err->apr_err))
        add_done_item(ctx);

      return svn_error_trace(err);
    }

  if (!ctx->xmlp)
    {
      ctx->xmlp = XML_ParserCreate(NULL);
      apr_pool_cleanup_register(ctx->pool, &ctx->xmlp, xml_parser_cleanup,
                                apr_pool_cleanup_null);
      XML_SetUserData(ctx->xmlp, ctx);
      XML_SetElementHandler(ctx->xmlp, start_xml, end_xml);
      if (ctx->cdata)
        {
          XML_SetCharacterDataHandler(ctx->xmlp, cdata_xml);
        }
    }

  while (1)
    {
      const char *data;
      apr_size_t len;

      status = serf_bucket_read(response, PARSE_CHUNK_SIZE, &data, &len);
      if (SERF_BUCKET_READ_ERROR(status))
        {
          return svn_ra_serf__wrap_err(status, NULL);
        }

      /* Note: once the callbacks invoked by inject_to_parser() sets the
         PAUSED flag, then it will not be cleared. write_to_pending() will
         only save the content. Logic outside of serf_context_run() will
         clear that flag, as appropriate, along with processing the
         content that we have placed into the PENDING buffer.

         We want to save arriving content into the PENDING structures if
         the parser has been paused, or we already have data in there (so
         the arriving data is appended, rather than injected out of order)  */
      if (ctx->paused || HAS_PENDING_DATA(ctx->pending))
        {
          err = write_to_pending(ctx, data, len, pool);
        }
      else
        {
          err = inject_to_parser(ctx, data, len, &sl);
          if (err)
            {
              /* Should have no errors if IGNORE_ERRORS is set.  */
              SVN_ERR_ASSERT(!ctx->ignore_errors);
            }
        }
      if (err)
        {
          SVN_ERR_ASSERT(ctx->xmlp != NULL);

          apr_pool_cleanup_run(ctx->pool, &ctx->xmlp, xml_parser_cleanup);
          add_done_item(ctx);
          return svn_error_trace(err);
        }

      if (APR_STATUS_IS_EAGAIN(status))
        {
          return svn_ra_serf__wrap_err(status, NULL);
        }

      if (APR_STATUS_IS_EOF(status))
        {
          if (ctx->pending != NULL)
            ctx->pending->network_eof = TRUE;

          /* We just hit the end of the network content. If we have nothing
             in the PENDING structures, then we're completely done.  */
          if (!HAS_PENDING_DATA(ctx->pending))
            {
              int xml_status;
              SVN_ERR_ASSERT(ctx->xmlp != NULL);

              xml_status = XML_Parse(ctx->xmlp, NULL, 0, 1);

              apr_pool_cleanup_run(ctx->pool, &ctx->xmlp, xml_parser_cleanup);

              if (! ctx->ignore_errors)
                {
                  SVN_ERR(ctx->error);

                  if (xml_status != XML_STATUS_OK)
                    {
                      return svn_error_create(
                                    SVN_ERR_XML_MALFORMED, NULL,
                                    _("The XML response contains invalid XML"));
                    }
                }

              add_done_item(ctx);
            }

          return svn_ra_serf__wrap_err(status, NULL);
        }

      /* feed me! */
    }
  /* not reached */
}


apr_status_t
svn_ra_serf__credentials_callback(char **username, char **password,
                                  serf_request_t *request, void *baton,
                                  int code, const char *authn_type,
                                  const char *realm,
                                  apr_pool_t *pool)
{
  svn_ra_serf__handler_t *handler = baton;
  svn_ra_serf__session_t *session = handler->session;
  void *creds;
  svn_auth_cred_simple_t *simple_creds;
  svn_error_t *err;

  if (code == 401)
    {
      /* Use svn_auth_first_credentials if this is the first time we ask for
         credentials during this session OR if the last time we asked
         session->auth_state wasn't set (eg. if the credentials provider was
         cancelled by the user). */
      if (!session->auth_state)
        {
          err = svn_auth_first_credentials(&creds,
                                           &session->auth_state,
                                           SVN_AUTH_CRED_SIMPLE,
                                           realm,
                                           session->wc_callbacks->auth_baton,
                                           session->pool);
        }
      else
        {
          err = svn_auth_next_credentials(&creds,
                                          session->auth_state,
                                          session->pool);
        }

      if (err)
        {
          (void) save_error(session, err);
          return err->apr_err;
        }

      session->auth_attempts++;

      if (!creds || session->auth_attempts > 4)
        {
          /* No more credentials. */
          (void) save_error(session,
                            svn_error_create(
                              SVN_ERR_AUTHN_FAILED, NULL,
                              _("No more credentials or we tried too many "
                                "times.\nAuthentication failed")));
          return SVN_ERR_AUTHN_FAILED;
        }

      simple_creds = creds;
      *username = apr_pstrdup(pool, simple_creds->username);
      *password = apr_pstrdup(pool, simple_creds->password);
    }
  else
    {
      *username = apr_pstrdup(pool, session->proxy_username);
      *password = apr_pstrdup(pool, session->proxy_password);

      session->proxy_auth_attempts++;

      if (!session->proxy_username || session->proxy_auth_attempts > 4)
        {
          /* No more credentials. */
          (void) save_error(session,
                            svn_error_create(
                              SVN_ERR_AUTHN_FAILED, NULL,
                              _("Proxy authentication failed")));
          return SVN_ERR_AUTHN_FAILED;
        }
    }

  handler->conn->last_status_code = code;

  return APR_SUCCESS;
}

/* Wait for HTTP response status and headers, and invoke HANDLER->
   response_handler() to carry out operation-specific processing.
   Afterwards, check for connection close.

   SERF_STATUS allows returning errors to serf without creating a
   subversion error object.
   */
static svn_error_t *
handle_response(serf_request_t *request,
                serf_bucket_t *response,
                svn_ra_serf__handler_t *handler,
                apr_status_t *serf_status,
                apr_pool_t *scratch_pool)
{
  apr_status_t status;
  svn_error_t *err;

  /* ### need to verify whether this already gets init'd on every
     ### successful exit. for an error-exit, it will (properly) be
     ### ignored by the caller.  */
  *serf_status = APR_SUCCESS;

  if (!response)
    {
      /* Uh-oh. Our connection died.  */
      if (handler->response_error)
        {
          /* Give a handler chance to prevent request requeue. */
          SVN_ERR(handler->response_error(request, response, 0,
                                          handler->response_error_baton));

          svn_ra_serf__request_create(handler);
        }
      /* Response error callback is not configured. Requeue another request
         for this handler only if we didn't started to process body.
         Return error otherwise. */
      else if (!handler->reading_body)
        {
          svn_ra_serf__request_create(handler);
        }
      else
        {
          return svn_error_createf(SVN_ERR_RA_DAV_REQUEST_FAILED, NULL,
                                    _("%s request on '%s' failed"),
                                   handler->method, handler->path);
        }

      return SVN_NO_ERROR;
    }

  /* If we're reading the body, then skip all this preparation.  */
  if (handler->reading_body)
    goto process_body;

  /* Copy the Status-Line info into HANDLER, if we don't yet have it.  */
  if (handler->sline.version == 0)
    {
      serf_status_line sl;

      status = serf_bucket_response_status(response, &sl);
      if (status != APR_SUCCESS)
        {
          /* The response line is not (yet) ready, or some other error.  */
          *serf_status = status;
          return SVN_NO_ERROR; /* Handled by serf */
        }

      /* If we got APR_SUCCESS, then we should have Status-Line info.  */
      SVN_ERR_ASSERT(sl.version != 0);

      handler->sline = sl;
      handler->sline.reason = apr_pstrdup(handler->handler_pool, sl.reason);

      /* HTTP/1.1? (or later)  */
      if (sl.version != SERF_HTTP_10)
        handler->session->http10 = FALSE;
    }

  /* Keep reading from the network until we've read all the headers.  */
  status = serf_bucket_response_wait_for_headers(response);
  if (status)
    {
      /* The typical "error" will be APR_EAGAIN, meaning that more input
         from the network is required to complete the reading of the
         headers.  */
      if (!APR_STATUS_IS_EOF(status))
        {
          /* Either the headers are not (yet) complete, or there really
             was an error.  */
          *serf_status = status;
          return SVN_NO_ERROR;
        }

      /* wait_for_headers() will return EOF if there is no body in this
         response, or if we completely read the body. The latter is not
         true since we would have set READING_BODY to get the body read,
         and we would not be back to this code block.

         It can also return EOF if we truly hit EOF while (say) processing
         the headers. aka Badness.  */

      /* Cases where a lack of a response body (via EOF) is okay:
       *  - A HEAD request
       *  - 204/304 response
       *
       * Otherwise, if we get an EOF here, something went really wrong: either
       * the server closed on us early or we're reading too much.  Either way,
       * scream loudly.
       */
      if (strcmp(handler->method, "HEAD") != 0
          && handler->sline.code != 204
          && handler->sline.code != 304)
        {
          err = svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA,
                                  svn_ra_serf__wrap_err(status, NULL),
                                  _("Premature EOF seen from server"
                                    " (http status=%d)"),
                                  handler->sline.code);

          /* In case anything else arrives... discard it.  */
          handler->discard_body = TRUE;

          return err;
        }
    }

  /* ... and set up the header fields in HANDLER.  */
  handler->location = response_get_location(response,
                                            handler->session->session_url_str,
                                            handler->handler_pool,
                                            scratch_pool);

  /* On the last request, we failed authentication. We succeeded this time,
     so let's save away these credentials.  */
  if (handler->conn->last_status_code == 401 && handler->sline.code < 400)
    {
      SVN_ERR(svn_auth_save_credentials(handler->session->auth_state,
                                        handler->session->pool));
      handler->session->auth_attempts = 0;
      handler->session->auth_state = NULL;
    }
  handler->conn->last_status_code = handler->sline.code;

  if (handler->sline.code == 405
      || handler->sline.code == 408
      || handler->sline.code == 409
      || handler->sline.code >= 500)
    {
      /* 405 Method Not allowed.
         408 Request Timeout
         409 Conflict: can indicate a hook error.
         5xx (Internal) Server error. */
      serf_bucket_t *hdrs;
      const char *val;

      hdrs = serf_bucket_response_get_headers(response);
      val = serf_bucket_headers_get(hdrs, "Content-Type");
      if (val && strncasecmp(val, "text/xml", sizeof("text/xml") - 1) == 0)
        {
          svn_ra_serf__server_error_t *server_err;

          server_err = begin_error_parsing(start_error, end_error, cdata_error,
                                           handler->handler_pool);
          /* Get the parser to set our DONE flag.  */
          server_err->parser.done = &handler->done;

          handler->server_error = server_err;
        }
      else
        {
          handler->discard_body = TRUE;

          if (!handler->session->pending_error)
            {
              apr_status_t apr_err = SVN_ERR_RA_DAV_REQUEST_FAILED;

              /* 405 == Method Not Allowed (Occurs when trying to lock a working
                copy path which no longer exists at HEAD in the repository. */
              if (handler->sline.code == 405
                  && strcmp(handler->method, "LOCK") == 0)
                apr_err = SVN_ERR_FS_OUT_OF_DATE;

              handler->session->pending_error =
                  svn_error_createf(apr_err, NULL,
                                    _("%s request on '%s' failed: %d %s"),
                                   handler->method, handler->path,
                                   handler->sline.code, handler->sline.reason);
            }
        }
    }

  /* Stop processing the above, on every packet arrival.  */
  handler->reading_body = TRUE;

 process_body:

  /* We've been instructed to ignore the body. Drain whatever is present.  */
  if (handler->discard_body)
    {
      *serf_status = drain_bucket(response);

      /* If the handler hasn't set done (which it shouldn't have) and
         we now have the EOF, go ahead and set it so that we can stop
         our context loops.
       */
      if (!handler->done && APR_STATUS_IS_EOF(*serf_status))
          handler->done = TRUE;

      return SVN_NO_ERROR;
    }

  /* If we are supposed to parse the body as a server_error, then do
     that now.  */
  if (handler->server_error != NULL)
    {
      err = svn_ra_serf__handle_xml_parser(request, response,
                                           &handler->server_error->parser,
                                           scratch_pool);

      /* If we do not receive an error or it is a non-transient error, return
         immediately.

         APR_EOF will be returned when parsing is complete.

         APR_EAGAIN & WAIT_CONN may be intermittently returned as we proceed through
         parsing and the network has no more data right now.  If we receive that,
         clear the error and return - allowing serf to wait for more data.
         */
      if (!err || SERF_BUCKET_READ_ERROR(err->apr_err))
        return svn_error_trace(err);

      if (!APR_STATUS_IS_EOF(err->apr_err))
        {
          *serf_status = err->apr_err;
          svn_error_clear(err);
          return SVN_NO_ERROR;
        }

      /* Clear the EOF. We don't need it.  */
      svn_error_clear(err);

      /* If the parsing is done, and we did not extract an error, then
         simply toss everything, and anything else that might arrive.
         The higher-level code will need to investigate HANDLER->SLINE,
         as we have no further information for them.  */
      if (handler->done
          && handler->server_error->error->apr_err == APR_SUCCESS)
        {
          svn_error_clear(handler->server_error->error);

          /* Stop parsing for a server error.  */
          handler->server_error = NULL;

          /* If anything arrives after this, then just discard it.  */
          handler->discard_body = TRUE;
        }

      *serf_status = APR_EOF;
      return SVN_NO_ERROR;
    }

  /* Pass the body along to the registered response handler.  */
  err = handler->response_handler(request, response,
                                  handler->response_baton,
                                  scratch_pool);

  if (err
      && (!SERF_BUCKET_READ_ERROR(err->apr_err)
          || APR_STATUS_IS_ECONNRESET(err->apr_err)
          || APR_STATUS_IS_ECONNABORTED(err->apr_err)))
    {
      /* These errors are special cased in serf
         ### We hope no handler returns these by accident. */
      *serf_status = err->apr_err;
      svn_error_clear(err);
      return SVN_NO_ERROR;
    }

  return svn_error_trace(err);
}


/* Implements serf_response_handler_t for handle_response. Storing
   errors in handler->session->pending_error if appropriate. */
static apr_status_t
handle_response_cb(serf_request_t *request,
                   serf_bucket_t *response,
                   void *baton,
                   apr_pool_t *scratch_pool)
{
  svn_ra_serf__handler_t *handler = baton;
  svn_error_t *err;
  apr_status_t inner_status;
  apr_status_t outer_status;

  err = svn_error_trace(handle_response(request, response,
                                        handler, &inner_status,
                                        scratch_pool));

  /* Select the right status value to return.  */
  outer_status = save_error(handler->session, err);
  if (!outer_status)
    outer_status = inner_status;

  /* Make sure the DONE flag is set properly.  */
  if (APR_STATUS_IS_EOF(outer_status) || APR_STATUS_IS_EOF(inner_status))
    handler->done = TRUE;

  return outer_status;
}

/* Perform basic request setup, with special handling for HEAD requests,
   and finer-grained callbacks invoked (if non-NULL) to produce the request
   headers and body. */
static svn_error_t *
setup_request(serf_request_t *request,
              svn_ra_serf__handler_t *handler,
              serf_bucket_t **req_bkt,
              apr_pool_t *request_pool,
              apr_pool_t *scratch_pool)
{
  serf_bucket_t *body_bkt;
  serf_bucket_t *headers_bkt;
  const char *accept_encoding;

  if (handler->body_delegate)
    {
      serf_bucket_alloc_t *bkt_alloc = serf_request_get_alloc(request);

      /* ### should pass the scratch_pool  */
      SVN_ERR(handler->body_delegate(&body_bkt, handler->body_delegate_baton,
                                     bkt_alloc, request_pool));
    }
  else
    {
      body_bkt = NULL;
    }

  if (handler->custom_accept_encoding)
    {
      accept_encoding = NULL;
    }
  else if (handler->session->using_compression)
    {
      /* Accept gzip compression if enabled. */
      accept_encoding = "gzip";
    }
  else
    {
      accept_encoding = NULL;
    }

  SVN_ERR(setup_serf_req(request, req_bkt, &headers_bkt,
                         handler->session, handler->method, handler->path,
                         body_bkt, handler->body_type, accept_encoding,
                         request_pool, scratch_pool));

  if (handler->header_delegate)
    {
      /* ### should pass the scratch_pool  */
      SVN_ERR(handler->header_delegate(headers_bkt,
                                       handler->header_delegate_baton,
                                       request_pool));
    }

  return APR_SUCCESS;
}

/* Implements the serf_request_setup_t interface (which sets up both a
   request and its response handler callback). Handles errors for
   setup_request_cb */
static apr_status_t
setup_request_cb(serf_request_t *request,
              void *setup_baton,
              serf_bucket_t **req_bkt,
              serf_response_acceptor_t *acceptor,
              void **acceptor_baton,
              serf_response_handler_t *s_handler,
              void **s_handler_baton,
              apr_pool_t *pool)
{
  svn_ra_serf__handler_t *handler = setup_baton;
  svn_error_t *err;

  /* ### construct a scratch_pool? serf gives us a pool that will live for
     ### the duration of the request.  */
  apr_pool_t *scratch_pool = pool;

  if (strcmp(handler->method, "HEAD") == 0)
    *acceptor = accept_head;
  else
    *acceptor = accept_response;
  *acceptor_baton = handler->session;

  *s_handler = handle_response_cb;
  *s_handler_baton = handler;

  err = svn_error_trace(setup_request(request, handler, req_bkt,
                                      pool /* request_pool */, scratch_pool));

  return save_error(handler->session, err);
}

void
svn_ra_serf__request_create(svn_ra_serf__handler_t *handler)
{
  SVN_ERR_ASSERT_NO_RETURN(handler->handler_pool != NULL);

  /* In case HANDLER is re-queued, reset the various transient fields.

     ### prior to recent changes, HANDLER was constant. maybe we should
     ### break out these processing fields, apart from the request
     ### definition.  */
  handler->done = FALSE;
  handler->server_error = NULL;
  handler->sline.version = 0;
  handler->location = NULL;
  handler->reading_body = FALSE;
  handler->discard_body = FALSE;

  /* ### do we ever alter the >response_handler?  */

  /* ### do we need to hold onto the returned request object, or just
     ### not worry about it (the serf ctx will manage it).  */
  (void) serf_connection_request_create(handler->conn->conn,
                                        setup_request_cb, handler);
}


svn_error_t *
svn_ra_serf__discover_vcc(const char **vcc_url,
                          svn_ra_serf__session_t *session,
                          svn_ra_serf__connection_t *conn,
                          apr_pool_t *pool)
{
  const char *path;
  const char *relative_path;
  const char *uuid;

  /* If we've already got the information our caller seeks, just return it.  */
  if (session->vcc_url && session->repos_root_str)
    {
      *vcc_url = session->vcc_url;
      return SVN_NO_ERROR;
    }

  /* If no connection is provided, use the default one. */
  if (! conn)
    {
      conn = session->conns[0];
    }

  path = session->session_url.path;
  *vcc_url = NULL;
  uuid = NULL;

  do
    {
      apr_hash_t *props;
      svn_error_t *err;

      err = svn_ra_serf__fetch_node_props(&props, conn,
                                          path, SVN_INVALID_REVNUM,
                                          base_props, pool, pool);
      if (! err)
        {
          apr_hash_t *ns_props;

          ns_props = apr_hash_get(props, "DAV:", 4);
          *vcc_url = svn_prop_get_value(ns_props,
                                        "version-controlled-configuration");

          ns_props = svn_hash_gets(props, SVN_DAV_PROP_NS_DAV);
          relative_path = svn_prop_get_value(ns_props,
                                             "baseline-relative-path");
          uuid = svn_prop_get_value(ns_props, "repository-uuid");
          break;
        }
      else
        {
          if ((err->apr_err != SVN_ERR_FS_NOT_FOUND) &&
              (err->apr_err != SVN_ERR_RA_DAV_FORBIDDEN))
            {
              return svn_error_trace(err);  /* found a _real_ error */
            }
          else
            {
              /* This happens when the file is missing in HEAD. */
              svn_error_clear(err);

              /* Okay, strip off a component from PATH. */
              path = svn_urlpath__dirname(path, pool);

              /* An error occurred on conns. serf 0.4.0 remembers that
                 the connection had a problem. We need to reset it, in
                 order to use it again.  */
              serf_connection_reset(conn->conn);
            }
        }
    }
  while ((path[0] != '\0')
         && (! (path[0] == '/' && path[1] == '\0')));

  if (!*vcc_url)
    {
      return svn_error_create(SVN_ERR_RA_DAV_OPTIONS_REQ_FAILED, NULL,
                              _("The PROPFIND response did not include the "
                                "requested version-controlled-configuration "
                                "value"));
    }

  /* Store our VCC in our cache. */
  if (!session->vcc_url)
    {
      session->vcc_url = apr_pstrdup(session->pool, *vcc_url);
    }

  /* Update our cached repository root URL. */
  if (!session->repos_root_str)
    {
      svn_stringbuf_t *url_buf;

      url_buf = svn_stringbuf_create(path, pool);

      svn_path_remove_components(url_buf,
                                 svn_path_component_count(relative_path));

      /* Now recreate the root_url. */
      session->repos_root = session->session_url;
      session->repos_root.path =
        (char *)svn_fspath__canonicalize(url_buf->data, session->pool);
      session->repos_root_str =
        svn_urlpath__canonicalize(apr_uri_unparse(session->pool,
                                                  &session->repos_root, 0),
                                  session->pool);
    }

  /* Store the repository UUID in the cache. */
  if (!session->uuid)
    {
      session->uuid = apr_pstrdup(session->pool, uuid);
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_serf__get_relative_path(const char **rel_path,
                               const char *orig_path,
                               svn_ra_serf__session_t *session,
                               svn_ra_serf__connection_t *conn,
                               apr_pool_t *pool)
{
  const char *decoded_root, *decoded_orig;

  if (! session->repos_root.path)
    {
      const char *vcc_url;

      /* This should only happen if we haven't detected HTTP v2
         support from the server.  */
      assert(! SVN_RA_SERF__HAVE_HTTPV2_SUPPORT(session));

      /* We don't actually care about the VCC_URL, but this API
         promises to populate the session's root-url cache, and that's
         what we really want. */
      SVN_ERR(svn_ra_serf__discover_vcc(&vcc_url, session,
                                        conn ? conn : session->conns[0],
                                        pool));
    }

  decoded_root = svn_path_uri_decode(session->repos_root.path, pool);
  decoded_orig = svn_path_uri_decode(orig_path, pool);
  *rel_path = svn_urlpath__skip_ancestor(decoded_root, decoded_orig);
  SVN_ERR_ASSERT(*rel_path != NULL);
  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_serf__report_resource(const char **report_target,
                             svn_ra_serf__session_t *session,
                             svn_ra_serf__connection_t *conn,
                             apr_pool_t *pool)
{
  /* If we have HTTP v2 support, we want to report against the 'me'
     resource. */
  if (SVN_RA_SERF__HAVE_HTTPV2_SUPPORT(session))
    *report_target = apr_pstrdup(pool, session->me_resource);

  /* Otherwise, we'll use the default VCC. */
  else
    SVN_ERR(svn_ra_serf__discover_vcc(report_target, session, conn, pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_serf__error_on_status(serf_status_line sline,
                             const char *path,
                             const char *location)
{
  switch(sline.code)
    {
      case 301:
      case 302:
      case 307:
        return svn_error_createf(SVN_ERR_RA_DAV_RELOCATED, NULL,
                                 (sline.code == 301)
                                 ? _("Repository moved permanently to '%s';"
                                     " please relocate")
                                 : _("Repository moved temporarily to '%s';"
                                     " please relocate"), location);
      case 403:
        return svn_error_createf(SVN_ERR_RA_DAV_FORBIDDEN, NULL,
                                 _("Access to '%s' forbidden"), path);

      case 404:
        return svn_error_createf(SVN_ERR_FS_NOT_FOUND, NULL,
                                 _("'%s' path not found"), path);
      case 423:
        return svn_error_createf(SVN_ERR_FS_NO_LOCK_TOKEN, NULL,
                                 _("'%s': no lock token available"), path);

      case 411:
        return svn_error_createf(SVN_ERR_RA_DAV_REQUEST_FAILED, NULL,
                    _("DAV request failed: 411 Content length required. The "
                      "server or an intermediate proxy does not accept "
                      "chunked encoding. Try setting 'http-chunked-requests' "
                      "to 'auto' or 'no' in your client configuration."));
      case 501:
        return svn_error_createf(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                                 _("The requested feature is not supported by "
                                   "'%s'"), path);
    }

  if (sline.code >= 300)
    return svn_error_createf(SVN_ERR_RA_DAV_REQUEST_FAILED, NULL,
                             _("Unexpected HTTP status %d '%s' on '%s'\n"),
                             sline.code, sline.reason, path);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_serf__register_editor_shim_callbacks(svn_ra_session_t *ra_session,
                                    svn_delta_shim_callbacks_t *callbacks)
{
  svn_ra_serf__session_t *session = ra_session->priv;

  session->shim_callbacks = callbacks;
  return SVN_NO_ERROR;
}


/* Conforms to Expat's XML_StartElementHandler  */
static void
expat_start(void *userData, const char *raw_name, const char **attrs)
{
  struct expat_ctx_t *ectx = userData;

  if (ectx->inner_error != NULL)
    return;

  ectx->inner_error = svn_error_trace(
                        svn_ra_serf__xml_cb_start(ectx->xmlctx,
                                                  raw_name, attrs));

#ifdef EXPAT_HAS_STOPPARSER
  if (ectx->inner_error)
    (void) XML_StopParser(ectx->parser, 0 /* resumable */);
#endif
}


/* Conforms to Expat's XML_EndElementHandler  */
static void
expat_end(void *userData, const char *raw_name)
{
  struct expat_ctx_t *ectx = userData;

  if (ectx->inner_error != NULL)
    return;

  ectx->inner_error = svn_error_trace(
                        svn_ra_serf__xml_cb_end(ectx->xmlctx, raw_name));

#ifdef EXPAT_HAS_STOPPARSER
  if (ectx->inner_error)
    (void) XML_StopParser(ectx->parser, 0 /* resumable */);
#endif
}


/* Conforms to Expat's XML_CharacterDataHandler  */
static void
expat_cdata(void *userData, const char *data, int len)
{
  struct expat_ctx_t *ectx = userData;

  if (ectx->inner_error != NULL)
    return;

  ectx->inner_error = svn_error_trace(
                        svn_ra_serf__xml_cb_cdata(ectx->xmlctx, data, len));

#ifdef EXPAT_HAS_STOPPARSER
  if (ectx->inner_error)
    (void) XML_StopParser(ectx->parser, 0 /* resumable */);
#endif
}


/* Implements svn_ra_serf__response_handler_t */
static svn_error_t *
expat_response_handler(serf_request_t *request,
                       serf_bucket_t *response,
                       void *baton,
                       apr_pool_t *scratch_pool)
{
  struct expat_ctx_t *ectx = baton;

  if (!ectx->parser)
    {
      ectx->parser = XML_ParserCreate(NULL);
      apr_pool_cleanup_register(ectx->cleanup_pool, &ectx->parser,
                                xml_parser_cleanup, apr_pool_cleanup_null);
      XML_SetUserData(ectx->parser, ectx);
      XML_SetElementHandler(ectx->parser, expat_start, expat_end);
      XML_SetCharacterDataHandler(ectx->parser, expat_cdata);
    }

  /* ### TODO: sline.code < 200 should really be handled by the core */
  if ((ectx->handler->sline.code < 200) || (ectx->handler->sline.code >= 300))
    {
      /* By deferring to expect_empty_body(), it will make a choice on
         how to handle the body. Whatever the decision, the core handler
         will take over, and we will not be called again.  */
      return svn_error_trace(svn_ra_serf__expect_empty_body(
                               request, response, ectx->handler,
                               scratch_pool));
    }

  while (1)
    {
      apr_status_t status;
      const char *data;
      apr_size_t len;
      int expat_status;

      status = serf_bucket_read(response, PARSE_CHUNK_SIZE, &data, &len);
      if (SERF_BUCKET_READ_ERROR(status))
        return svn_ra_serf__wrap_err(status, NULL);

#if 0
      /* ### move restart/skip into the core handler  */
      ectx->handler->read_size += len;
#endif

      /* ### move PAUSED behavior to a new response handler that can feed
         ### an inner handler, or can pause for a while.  */

      /* ### should we have an IGNORE_ERRORS flag like the v1 parser?  */

      expat_status = XML_Parse(ectx->parser, data, (int)len, 0 /* isFinal */);

      /* We need to check INNER_ERROR first. This is an error from the
         callbacks that has been "dropped off" for us to retrieve. On
         current Expat parsers, we stop the parser when an error occurs,
         so we want to ignore EXPAT_STATUS (which reports the stoppage).

         If an error is not present, THEN we go ahead and look for parsing
         errors.  */
      if (ectx->inner_error)
        {
          apr_pool_cleanup_run(ectx->cleanup_pool, &ectx->parser,
                               xml_parser_cleanup);
          return svn_error_trace(ectx->inner_error);
        }
      if (expat_status == XML_STATUS_ERROR)
        return svn_error_createf(SVN_ERR_XML_MALFORMED,
                                 ectx->inner_error,
                                 _("The %s response contains invalid XML"
                                   " (%d %s)"),
                                 ectx->handler->method,
                                 ectx->handler->sline.code,
                                 ectx->handler->sline.reason);

      /* The parsing went fine. What has the bucket told us?  */

      if (APR_STATUS_IS_EOF(status))
        {
          /* Tell expat we've reached the end of the content. Ignore the
             return status. We just don't care.  */
          (void) XML_Parse(ectx->parser, NULL, 0, 1 /* isFinal */);

          svn_ra_serf__xml_context_destroy(ectx->xmlctx);
          apr_pool_cleanup_run(ectx->cleanup_pool, &ectx->parser,
                               xml_parser_cleanup);

          /* ### should check XMLCTX to see if it has returned to the
             ### INITIAL state. we may have ended early...  */
        }

      if (status && !SERF_BUCKET_READ_ERROR(status))
        {
          return svn_ra_serf__wrap_err(status, NULL);
        }
    }

  /* NOTREACHED */
}


svn_ra_serf__handler_t *
svn_ra_serf__create_expat_handler(svn_ra_serf__xml_context_t *xmlctx,
                                  apr_pool_t *result_pool)
{
  svn_ra_serf__handler_t *handler;
  struct expat_ctx_t *ectx;

  ectx = apr_pcalloc(result_pool, sizeof(*ectx));
  ectx->xmlctx = xmlctx;
  ectx->parser = NULL;
  ectx->cleanup_pool = result_pool;


  handler = apr_pcalloc(result_pool, sizeof(*handler));
  handler->handler_pool = result_pool;
  handler->response_handler = expat_response_handler;
  handler->response_baton = ectx;

  ectx->handler = handler;

  return handler;
}
