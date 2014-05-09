/*
 * options.c :  entry point for OPTIONS RA functions for ra_serf
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



#include <apr_uri.h>

#include <serf.h>

#include "svn_dirent_uri.h"
#include "svn_hash.h"
#include "svn_pools.h"
#include "svn_ra.h"
#include "svn_dav.h"
#include "svn_xml.h"

#include "../libsvn_ra/ra_loader.h"
#include "svn_private_config.h"
#include "private/svn_fspath.h"

#include "ra_serf.h"


/* In a debug build, setting this environment variable to "yes" will force
   the client to speak v1, even if the server is capable of speaking v2. */
#define SVN_IGNORE_V2_ENV_VAR "SVN_I_LIKE_LATENCY_SO_IGNORE_HTTPV2"


/*
 * This enum represents the current state of our XML parsing for an OPTIONS.
 */
enum options_state_e {
  INITIAL = 0,
  OPTIONS,
  ACTIVITY_COLLECTION,
  HREF
};

typedef struct options_context_t {
  /* pool to allocate memory from */
  apr_pool_t *pool;

  /* Have we extracted options values from the headers already?  */
  svn_boolean_t headers_processed;

  svn_ra_serf__session_t *session;
  svn_ra_serf__connection_t *conn;
  svn_ra_serf__handler_t *handler;

  svn_ra_serf__response_handler_t inner_handler;
  void *inner_baton;

  const char *activity_collection;
  svn_revnum_t youngest_rev;

} options_context_t;

#define D_ "DAV:"
#define S_ SVN_XML_NAMESPACE
static const svn_ra_serf__xml_transition_t options_ttable[] = {
  { INITIAL, D_, "options-response", OPTIONS,
    FALSE, { NULL }, FALSE },

  { OPTIONS, D_, "activity-collection-set", ACTIVITY_COLLECTION,
    FALSE, { NULL }, FALSE },

  { ACTIVITY_COLLECTION, D_, "href", HREF,
    TRUE, { NULL }, TRUE },

  { 0 }
};


/* Conforms to svn_ra_serf__xml_closed_t  */
static svn_error_t *
options_closed(svn_ra_serf__xml_estate_t *xes,
               void *baton,
               int leaving_state,
               const svn_string_t *cdata,
               apr_hash_t *attrs,
               apr_pool_t *scratch_pool)
{
  options_context_t *opt_ctx = baton;

  SVN_ERR_ASSERT(leaving_state == HREF);
  SVN_ERR_ASSERT(cdata != NULL);

  opt_ctx->activity_collection = svn_urlpath__canonicalize(cdata->data,
                                                           opt_ctx->pool);

  return SVN_NO_ERROR;
}


static svn_error_t *
create_options_body(serf_bucket_t **body_bkt,
                    void *baton,
                    serf_bucket_alloc_t *alloc,
                    apr_pool_t *pool)
{
  serf_bucket_t *body;
  body = serf_bucket_aggregate_create(alloc);
  svn_ra_serf__add_xml_header_buckets(body, alloc);
  svn_ra_serf__add_open_tag_buckets(body, alloc, "D:options",
                                    "xmlns:D", "DAV:",
                                    NULL);
  svn_ra_serf__add_tag_buckets(body, "D:activity-collection-set", NULL, alloc);
  svn_ra_serf__add_close_tag_buckets(body, alloc, "D:options");

  *body_bkt = body;
  return SVN_NO_ERROR;
}


/* We use these static pointers so we can employ pointer comparison
 * of our capabilities hash members instead of strcmp()ing all over
 * the place.
 */
/* Both server and repository support the capability. */
static const char *const capability_yes = "yes";
/* Either server or repository does not support the capability. */
static const char *const capability_no = "no";
/* Server supports the capability, but don't yet know if repository does. */
static const char *const capability_server_yes = "server-yes";


/* This implements serf_bucket_headers_do_callback_fn_t.
 */
static int
capabilities_headers_iterator_callback(void *baton,
                                       const char *key,
                                       const char *val)
{
  options_context_t *opt_ctx = baton;
  svn_ra_serf__session_t *session = opt_ctx->session;

  if (svn_cstring_casecmp(key, "dav") == 0)
    {
      /* Each header may contain multiple values, separated by commas, e.g.:
           DAV: version-control,checkout,working-resource
           DAV: merge,baseline,activity,version-controlled-collection
           DAV: http://subversion.tigris.org/xmlns/dav/svn/depth */
      apr_array_header_t *vals = svn_cstring_split(val, ",", TRUE,
                                                   opt_ctx->pool);

      /* Right now we only have a few capabilities to detect, so just
         seek for them directly.  This could be written slightly more
         efficiently, but that wouldn't be worth it until we have many
         more capabilities. */

      if (svn_cstring_match_list(SVN_DAV_NS_DAV_SVN_DEPTH, vals))
        {
          svn_hash_sets(session->capabilities,
                        SVN_RA_CAPABILITY_DEPTH, capability_yes);
        }
      if (svn_cstring_match_list(SVN_DAV_NS_DAV_SVN_MERGEINFO, vals))
        {
          /* The server doesn't know what repository we're referring
             to, so it can't just say capability_yes. */
          if (!svn_hash_gets(session->capabilities,
                             SVN_RA_CAPABILITY_MERGEINFO))
            {
              svn_hash_sets(session->capabilities, SVN_RA_CAPABILITY_MERGEINFO,
                            capability_server_yes);
            }
        }
      if (svn_cstring_match_list(SVN_DAV_NS_DAV_SVN_LOG_REVPROPS, vals))
        {
          svn_hash_sets(session->capabilities,
                        SVN_RA_CAPABILITY_LOG_REVPROPS, capability_yes);
        }
      if (svn_cstring_match_list(SVN_DAV_NS_DAV_SVN_ATOMIC_REVPROPS, vals))
        {
          svn_hash_sets(session->capabilities,
                        SVN_RA_CAPABILITY_ATOMIC_REVPROPS, capability_yes);
        }
      if (svn_cstring_match_list(SVN_DAV_NS_DAV_SVN_PARTIAL_REPLAY, vals))
        {
          svn_hash_sets(session->capabilities,
                        SVN_RA_CAPABILITY_PARTIAL_REPLAY, capability_yes);
        }
      if (svn_cstring_match_list(SVN_DAV_NS_DAV_SVN_INHERITED_PROPS, vals))
        {
          svn_hash_sets(session->capabilities,
                        SVN_RA_CAPABILITY_INHERITED_PROPS, capability_yes);
        }
      if (svn_cstring_match_list(SVN_DAV_NS_DAV_SVN_REVERSE_FILE_REVS,
                                 vals))
        {
          svn_hash_sets(session->capabilities,
                        SVN_RA_CAPABILITY_GET_FILE_REVS_REVERSE,
                        capability_yes);
        }
      if (svn_cstring_match_list(SVN_DAV_NS_DAV_SVN_EPHEMERAL_TXNPROPS, vals))
        {
          svn_hash_sets(session->capabilities,
                        SVN_RA_CAPABILITY_EPHEMERAL_TXNPROPS, capability_yes);
        }
      if (svn_cstring_match_list(SVN_DAV_NS_DAV_SVN_INLINE_PROPS, vals))
        {
          session->supports_inline_props = TRUE;
        }
      if (svn_cstring_match_list(SVN_DAV_NS_DAV_SVN_REPLAY_REV_RESOURCE, vals))
        {
          session->supports_rev_rsrc_replay = TRUE;
        }
    }

  /* SVN-specific headers -- if present, server supports HTTP protocol v2 */
  else if (strncmp(key, "SVN", 3) == 0)
    {
      /* If we've not yet seen any information about supported POST
         requests, we'll initialize the list/hash with "create-txn"
         (which we know is supported by virtue of the server speaking
         HTTPv2 at all. */
      if (! session->supported_posts)
        {
          session->supported_posts = apr_hash_make(session->pool);
          apr_hash_set(session->supported_posts, "create-txn", 10, (void *)1);
        }

      if (svn_cstring_casecmp(key, SVN_DAV_ROOT_URI_HEADER) == 0)
        {
          session->repos_root = session->session_url;
          session->repos_root.path =
            (char *)svn_fspath__canonicalize(val, session->pool);
          session->repos_root_str =
            svn_urlpath__canonicalize(
                apr_uri_unparse(session->pool, &session->repos_root, 0),
                session->pool);
        }
      else if (svn_cstring_casecmp(key, SVN_DAV_ME_RESOURCE_HEADER) == 0)
        {
#ifdef SVN_DEBUG
          char *ignore_v2_env_var = getenv(SVN_IGNORE_V2_ENV_VAR);

          if (!(ignore_v2_env_var
                && apr_strnatcasecmp(ignore_v2_env_var, "yes") == 0))
            session->me_resource = apr_pstrdup(session->pool, val);
#else
          session->me_resource = apr_pstrdup(session->pool, val);
#endif
        }
      else if (svn_cstring_casecmp(key, SVN_DAV_REV_STUB_HEADER) == 0)
        {
          session->rev_stub = apr_pstrdup(session->pool, val);
        }
      else if (svn_cstring_casecmp(key, SVN_DAV_REV_ROOT_STUB_HEADER) == 0)
        {
          session->rev_root_stub = apr_pstrdup(session->pool, val);
        }
      else if (svn_cstring_casecmp(key, SVN_DAV_TXN_STUB_HEADER) == 0)
        {
          session->txn_stub = apr_pstrdup(session->pool, val);
        }
      else if (svn_cstring_casecmp(key, SVN_DAV_TXN_ROOT_STUB_HEADER) == 0)
        {
          session->txn_root_stub = apr_pstrdup(session->pool, val);
        }
      else if (svn_cstring_casecmp(key, SVN_DAV_VTXN_STUB_HEADER) == 0)
        {
          session->vtxn_stub = apr_pstrdup(session->pool, val);
        }
      else if (svn_cstring_casecmp(key, SVN_DAV_VTXN_ROOT_STUB_HEADER) == 0)
        {
          session->vtxn_root_stub = apr_pstrdup(session->pool, val);
        }
      else if (svn_cstring_casecmp(key, SVN_DAV_REPOS_UUID_HEADER) == 0)
        {
          session->uuid = apr_pstrdup(session->pool, val);
        }
      else if (svn_cstring_casecmp(key, SVN_DAV_YOUNGEST_REV_HEADER) == 0)
        {
          opt_ctx->youngest_rev = SVN_STR_TO_REV(val);
        }
      else if (svn_cstring_casecmp(key, SVN_DAV_ALLOW_BULK_UPDATES) == 0)
        {
          session->server_allows_bulk = apr_pstrdup(session->pool, val);
        }
      else if (svn_cstring_casecmp(key, SVN_DAV_SUPPORTED_POSTS_HEADER) == 0)
        {
          /* May contain multiple values, separated by commas. */
          int i;
          apr_array_header_t *vals = svn_cstring_split(val, ",", TRUE,
                                                       opt_ctx->pool);

          for (i = 0; i < vals->nelts; i++)
            {
              const char *post_val = APR_ARRAY_IDX(vals, i, const char *);

              svn_hash_sets(session->supported_posts, post_val, (void *)1);
            }
        }
      else if (svn_cstring_casecmp(key, SVN_DAV_REPOSITORY_MERGEINFO) == 0)
        {
          if (svn_cstring_casecmp(val, "yes") == 0)
            {
              svn_hash_sets(session->capabilities, SVN_RA_CAPABILITY_MERGEINFO,
                            capability_yes);
            }
          else if (svn_cstring_casecmp(val, "no") == 0)
            {
              svn_hash_sets(session->capabilities, SVN_RA_CAPABILITY_MERGEINFO,
                            capability_no);
            }
        }
    }

  return 0;
}


/* A custom serf_response_handler_t which is mostly a wrapper around
   the expat-based response handler -- it just notices OPTIONS response
   headers first, before handing off to the xml parser.
   Implements svn_ra_serf__response_handler_t */
static svn_error_t *
options_response_handler(serf_request_t *request,
                         serf_bucket_t *response,
                         void *baton,
                         apr_pool_t *pool)
{
  options_context_t *opt_ctx = baton;

  if (!opt_ctx->headers_processed)
    {
      svn_ra_serf__session_t *session = opt_ctx->session;
      serf_bucket_t *hdrs = serf_bucket_response_get_headers(response);

      /* Start out assuming all capabilities are unsupported. */
      svn_hash_sets(session->capabilities, SVN_RA_CAPABILITY_PARTIAL_REPLAY,
                    capability_no);
      svn_hash_sets(session->capabilities, SVN_RA_CAPABILITY_DEPTH,
                    capability_no);
      svn_hash_sets(session->capabilities, SVN_RA_CAPABILITY_MERGEINFO,
                    NULL);
      svn_hash_sets(session->capabilities, SVN_RA_CAPABILITY_LOG_REVPROPS,
                    capability_no);
      svn_hash_sets(session->capabilities, SVN_RA_CAPABILITY_ATOMIC_REVPROPS,
                    capability_no);
      svn_hash_sets(session->capabilities, SVN_RA_CAPABILITY_INHERITED_PROPS,
                    capability_no);
      svn_hash_sets(session->capabilities, SVN_RA_CAPABILITY_EPHEMERAL_TXNPROPS,
                    capability_no);
      svn_hash_sets(session->capabilities, SVN_RA_CAPABILITY_GET_FILE_REVS_REVERSE,
                    capability_no);

      /* Then see which ones we can discover. */
      serf_bucket_headers_do(hdrs, capabilities_headers_iterator_callback,
                             opt_ctx);

      /* Assume mergeinfo capability unsupported, if didn't recieve information
         about server or repository mergeinfo capability. */
      if (!svn_hash_gets(session->capabilities, SVN_RA_CAPABILITY_MERGEINFO))
        svn_hash_sets(session->capabilities, SVN_RA_CAPABILITY_MERGEINFO,
                      capability_no);

      opt_ctx->headers_processed = TRUE;
    }

  /* Execute the 'real' response handler to XML-parse the response body. */
  return opt_ctx->inner_handler(request, response, opt_ctx->inner_baton, pool);
}


static svn_error_t *
create_options_req(options_context_t **opt_ctx,
                   svn_ra_serf__session_t *session,
                   svn_ra_serf__connection_t *conn,
                   apr_pool_t *pool)
{
  options_context_t *new_ctx;
  svn_ra_serf__xml_context_t *xmlctx;
  svn_ra_serf__handler_t *handler;

  new_ctx = apr_pcalloc(pool, sizeof(*new_ctx));
  new_ctx->pool = pool;
  new_ctx->session = session;
  new_ctx->conn = conn;

  new_ctx->youngest_rev = SVN_INVALID_REVNUM;

  xmlctx = svn_ra_serf__xml_context_create(options_ttable,
                                           NULL, options_closed, NULL,
                                           new_ctx,
                                           pool);
  handler = svn_ra_serf__create_expat_handler(xmlctx, pool);

  handler->method = "OPTIONS";
  handler->path = session->session_url.path;
  handler->body_delegate = create_options_body;
  handler->body_type = "text/xml";
  handler->conn = conn;
  handler->session = session;

  new_ctx->handler = handler;

  new_ctx->inner_handler = handler->response_handler;
  new_ctx->inner_baton = handler->response_baton;
  handler->response_handler = options_response_handler;
  handler->response_baton = new_ctx;

  *opt_ctx = new_ctx;

  return SVN_NO_ERROR;
}


svn_error_t *
svn_ra_serf__v2_get_youngest_revnum(svn_revnum_t *youngest,
                                    svn_ra_serf__connection_t *conn,
                                    apr_pool_t *scratch_pool)
{
  svn_ra_serf__session_t *session = conn->session;
  options_context_t *opt_ctx;

  SVN_ERR_ASSERT(SVN_RA_SERF__HAVE_HTTPV2_SUPPORT(session));

  SVN_ERR(create_options_req(&opt_ctx, session, conn, scratch_pool));
  SVN_ERR(svn_ra_serf__context_run_one(opt_ctx->handler, scratch_pool));
  SVN_ERR(svn_ra_serf__error_on_status(opt_ctx->handler->sline,
                                       opt_ctx->handler->path,
                                       opt_ctx->handler->location));

  *youngest = opt_ctx->youngest_rev;
  SVN_ERR_ASSERT(SVN_IS_VALID_REVNUM(*youngest));

  return SVN_NO_ERROR;
}


svn_error_t *
svn_ra_serf__v1_get_activity_collection(const char **activity_url,
                                        svn_ra_serf__connection_t *conn,
                                        apr_pool_t *result_pool,
                                        apr_pool_t *scratch_pool)
{
  svn_ra_serf__session_t *session = conn->session;
  options_context_t *opt_ctx;

  SVN_ERR_ASSERT(!SVN_RA_SERF__HAVE_HTTPV2_SUPPORT(session));

  SVN_ERR(create_options_req(&opt_ctx, session, conn, scratch_pool));
  SVN_ERR(svn_ra_serf__context_run_one(opt_ctx->handler, scratch_pool));

  SVN_ERR(svn_ra_serf__error_on_status(opt_ctx->handler->sline,
                                       opt_ctx->handler->path,
                                       opt_ctx->handler->location));

  *activity_url = apr_pstrdup(result_pool, opt_ctx->activity_collection);

  return SVN_NO_ERROR;

}



/** Capabilities exchange. */

svn_error_t *
svn_ra_serf__exchange_capabilities(svn_ra_serf__session_t *serf_sess,
                                   const char **corrected_url,
                                   apr_pool_t *pool)
{
  options_context_t *opt_ctx;
  svn_error_t *err;

  /* This routine automatically fills in serf_sess->capabilities */
  SVN_ERR(create_options_req(&opt_ctx, serf_sess, serf_sess->conns[0], pool));

  err = svn_ra_serf__context_run_one(opt_ctx->handler, pool);

  /* If our caller cares about server redirections, and our response
     carries such a thing, report as much.  We'll disregard ERR --
     it's most likely just a complaint about the response body not
     successfully parsing as XML or somesuch. */
  if (corrected_url && (opt_ctx->handler->sline.code == 301))
    {
      svn_error_clear(err);
      *corrected_url = opt_ctx->handler->location;
      return SVN_NO_ERROR;
    }

  SVN_ERR(svn_error_compose_create(
              svn_ra_serf__error_on_status(opt_ctx->handler->sline,
                                           serf_sess->session_url.path,
                                           opt_ctx->handler->location),
              err));

  /* Opportunistically cache any reported activity URL.  (We don't
     want to have to ask for this again later, potentially against an
     unreadable commit anchor URL.)  */
  if (opt_ctx->activity_collection)
    {
      serf_sess->activity_collection_url =
        apr_pstrdup(serf_sess->pool, opt_ctx->activity_collection);
    }

  return SVN_NO_ERROR;
}


static svn_error_t *
create_simple_options_body(serf_bucket_t **body_bkt,
                           void *baton,
                           serf_bucket_alloc_t *alloc,
                           apr_pool_t *pool)
{
  serf_bucket_t *body;
  serf_bucket_t *s;

  body = serf_bucket_aggregate_create(alloc);
  svn_ra_serf__add_xml_header_buckets(body, alloc);

  s = SERF_BUCKET_SIMPLE_STRING("<D:options xmlns:D=\"DAV:\" />", alloc);
  serf_bucket_aggregate_append(body, s);

  *body_bkt = body;
  return SVN_NO_ERROR;
}


svn_error_t *
svn_ra_serf__probe_proxy(svn_ra_serf__session_t *serf_sess,
                         apr_pool_t *scratch_pool)
{
  svn_ra_serf__handler_t *handler;

  handler = apr_pcalloc(scratch_pool, sizeof(*handler));
  handler->handler_pool = scratch_pool;
  handler->method = "OPTIONS";
  handler->path = serf_sess->session_url.path;
  handler->conn = serf_sess->conns[0];
  handler->session = serf_sess;

  /* We don't care about the response body, so discard it.  */
  handler->response_handler = svn_ra_serf__handle_discard_body;

  /* We need a simple body, in order to send it in chunked format.  */
  handler->body_delegate = create_simple_options_body;

  /* No special headers.  */

  SVN_ERR(svn_ra_serf__context_run_one(handler, scratch_pool));
  /* Some versions of nginx in reverse proxy mode will return 411. They want
     a Content-Length header, rather than chunked requests. We can keep other
     HTTP/1.1 features, but will disable the chunking.  */
  if (handler->sline.code == 411)
    {
      serf_sess->using_chunked_requests = FALSE;

      return SVN_NO_ERROR;
    }
  SVN_ERR(svn_ra_serf__error_on_status(handler->sline,
                                       handler->path,
                                       handler->location));

  return SVN_NO_ERROR;
}


svn_error_t *
svn_ra_serf__has_capability(svn_ra_session_t *ra_session,
                            svn_boolean_t *has,
                            const char *capability,
                            apr_pool_t *pool)
{
  svn_ra_serf__session_t *serf_sess = ra_session->priv;
  const char *cap_result;

  /* This capability doesn't rely on anything server side. */
  if (strcmp(capability, SVN_RA_CAPABILITY_COMMIT_REVPROPS) == 0)
    {
      *has = TRUE;
      return SVN_NO_ERROR;
    }

  cap_result = svn_hash_gets(serf_sess->capabilities, capability);

  /* If any capability is unknown, they're all unknown, so ask. */
  if (cap_result == NULL)
    SVN_ERR(svn_ra_serf__exchange_capabilities(serf_sess, NULL, pool));

  /* Try again, now that we've fetched the capabilities. */
  cap_result = svn_hash_gets(serf_sess->capabilities, capability);

  /* Some capabilities depend on the repository as well as the server. */
  if (cap_result == capability_server_yes)
    {
      if (strcmp(capability, SVN_RA_CAPABILITY_MERGEINFO) == 0)
        {
          /* Handle mergeinfo specially.  Mergeinfo depends on the
             repository as well as the server, but the server routine
             that answered our svn_ra_serf__exchange_capabilities() call above
             didn't even know which repository we were interested in
             -- it just told us whether the server supports mergeinfo.
             If the answer was 'no', there's no point checking the
             particular repository; but if it was 'yes', we still must
             change it to 'no' iff the repository itself doesn't
             support mergeinfo. */
          svn_mergeinfo_catalog_t ignored;
          svn_error_t *err;
          apr_array_header_t *paths = apr_array_make(pool, 1,
                                                     sizeof(char *));
          APR_ARRAY_PUSH(paths, const char *) = "";

          err = svn_ra_serf__get_mergeinfo(ra_session, &ignored, paths, 0,
                                           FALSE, FALSE, pool);

          if (err)
            {
              if (err->apr_err == SVN_ERR_UNSUPPORTED_FEATURE)
                {
                  svn_error_clear(err);
                  cap_result = capability_no;
                }
              else if (err->apr_err == SVN_ERR_FS_NOT_FOUND)
                {
                  /* Mergeinfo requests use relative paths, and
                     anyway we're in r0, so this is a likely error,
                     but it means the repository supports mergeinfo! */
                  svn_error_clear(err);
                  cap_result = capability_yes;
                }
              else
                return err;
            }
          else
            cap_result = capability_yes;

          svn_hash_sets(serf_sess->capabilities,
                        SVN_RA_CAPABILITY_MERGEINFO,  cap_result);
        }
      else
        {
          return svn_error_createf
            (SVN_ERR_UNKNOWN_CAPABILITY, NULL,
             _("Don't know how to handle '%s' for capability '%s'"),
             capability_server_yes, capability);
        }
    }

  if (cap_result == capability_yes)
    {
      *has = TRUE;
    }
  else if (cap_result == capability_no)
    {
      *has = FALSE;
    }
  else if (cap_result == NULL)
    {
      return svn_error_createf
        (SVN_ERR_UNKNOWN_CAPABILITY, NULL,
         _("Don't know anything about capability '%s'"), capability);
    }
  else  /* "can't happen" */
    {
      /* Well, let's hope it's a string. */
      return svn_error_createf
        (SVN_ERR_RA_DAV_OPTIONS_REQ_FAILED, NULL,
         _("Attempt to fetch capability '%s' resulted in '%s'"),
         capability, cap_result);
    }

  return SVN_NO_ERROR;
}
