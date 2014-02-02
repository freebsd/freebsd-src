/*
 * commit.c :  entry point for commit RA functions for ra_serf
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

#include "svn_hash.h"
#include "svn_pools.h"
#include "svn_ra.h"
#include "svn_dav.h"
#include "svn_xml.h"
#include "svn_config.h"
#include "svn_delta.h"
#include "svn_base64.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_props.h"

#include "svn_private_config.h"
#include "private/svn_dep_compat.h"
#include "private/svn_fspath.h"
#include "private/svn_skel.h"

#include "ra_serf.h"
#include "../libsvn_ra/ra_loader.h"


/* Baton passed back with the commit editor. */
typedef struct commit_context_t {
  /* Pool for our commit. */
  apr_pool_t *pool;

  svn_ra_serf__session_t *session;
  svn_ra_serf__connection_t *conn;

  apr_hash_t *revprop_table;

  svn_commit_callback2_t callback;
  void *callback_baton;

  apr_hash_t *lock_tokens;
  svn_boolean_t keep_locks;
  apr_hash_t *deleted_entries;   /* deleted files (for delete+add detection) */

  /* HTTP v2 stuff */
  const char *txn_url;           /* txn URL (!svn/txn/TXN_NAME) */
  const char *txn_root_url;      /* commit anchor txn root URL */

  /* HTTP v1 stuff (only valid when 'txn_url' is NULL) */
  const char *activity_url;      /* activity base URL... */
  const char *baseline_url;      /* the working-baseline resource */
  const char *checked_in_url;    /* checked-in root to base CHECKOUTs from */
  const char *vcc_url;           /* vcc url */

} commit_context_t;

#define USING_HTTPV2_COMMIT_SUPPORT(commit_ctx) ((commit_ctx)->txn_url != NULL)

/* Structure associated with a PROPPATCH request. */
typedef struct proppatch_context_t {
  apr_pool_t *pool;

  const char *relpath;
  const char *path;

  commit_context_t *commit;

  /* Changed and removed properties. */
  apr_hash_t *changed_props;
  apr_hash_t *removed_props;

  /* Same, for the old value (*old_value_p). */
  apr_hash_t *previous_changed_props;
  apr_hash_t *previous_removed_props;

  /* In HTTP v2, this is the file/directory version we think we're changing. */
  svn_revnum_t base_revision;

} proppatch_context_t;

typedef struct delete_context_t {
  const char *path;

  svn_revnum_t revision;

  const char *lock_token;
  apr_hash_t *lock_token_hash;
  svn_boolean_t keep_locks;

} delete_context_t;

/* Represents a directory. */
typedef struct dir_context_t {
  /* Pool for our directory. */
  apr_pool_t *pool;

  /* The root commit we're in progress for. */
  commit_context_t *commit;

  /* URL to operate against (used for CHECKOUT and PROPPATCH before
     HTTP v2, for PROPPATCH in HTTP v2).  */
  const char *url;

  /* How many pending changes we have left in this directory. */
  unsigned int ref_count;

  /* Is this directory being added?  (Otherwise, just opened.) */
  svn_boolean_t added;

  /* Our parent */
  struct dir_context_t *parent_dir;

  /* The directory name; if "", we're the 'root' */
  const char *relpath;

  /* The basename of the directory. "" for the 'root' */
  const char *name;

  /* The base revision of the dir. */
  svn_revnum_t base_revision;

  const char *copy_path;
  svn_revnum_t copy_revision;

  /* Changed and removed properties */
  apr_hash_t *changed_props;
  apr_hash_t *removed_props;

  /* The checked-out working resource for this directory.  May be NULL; if so
     call checkout_dir() first.  */
  const char *working_url;

} dir_context_t;

/* Represents a file to be committed. */
typedef struct file_context_t {
  /* Pool for our file. */
  apr_pool_t *pool;

  /* The root commit we're in progress for. */
  commit_context_t *commit;

  /* Is this file being added?  (Otherwise, just opened.) */
  svn_boolean_t added;

  dir_context_t *parent_dir;

  const char *relpath;
  const char *name;

  /* The checked-out working resource for this file. */
  const char *working_url;

  /* The base revision of the file. */
  svn_revnum_t base_revision;

  /* Copy path and revision */
  const char *copy_path;
  svn_revnum_t copy_revision;

  /* stream */
  svn_stream_t *stream;

  /* Temporary file containing the svndiff. */
  apr_file_t *svndiff;

  /* Our base checksum as reported by the WC. */
  const char *base_checksum;

  /* Our resulting checksum as reported by the WC. */
  const char *result_checksum;

  /* Changed and removed properties. */
  apr_hash_t *changed_props;
  apr_hash_t *removed_props;

  /* URL to PUT the file at. */
  const char *url;

} file_context_t;


/* Setup routines and handlers for various requests we'll invoke. */

static svn_error_t *
return_response_err(svn_ra_serf__handler_t *handler)
{
  svn_error_t *err;

  /* We should have captured SLINE and LOCATION in the HANDLER.  */
  SVN_ERR_ASSERT(handler->handler_pool != NULL);

  /* Ye Olde Fallback Error */
  err = svn_error_compose_create(
            handler->server_error != NULL
              ? handler->server_error->error
              : SVN_NO_ERROR,
            svn_error_createf(SVN_ERR_RA_DAV_REQUEST_FAILED, NULL,
                              _("%s of '%s': %d %s"),
                              handler->method, handler->path,
                              handler->sline.code, handler->sline.reason));

  /* Try to return one of the standard errors for 301, 404, etc.,
     then look for an error embedded in the response.  */
  return svn_error_compose_create(svn_ra_serf__error_on_status(
                                    handler->sline,
                                    handler->path,
                                    handler->location),
                                  err);
}

/* Implements svn_ra_serf__request_body_delegate_t */
static svn_error_t *
create_checkout_body(serf_bucket_t **bkt,
                     void *baton,
                     serf_bucket_alloc_t *alloc,
                     apr_pool_t *pool)
{
  const char *activity_url = baton;
  serf_bucket_t *body_bkt;

  body_bkt = serf_bucket_aggregate_create(alloc);

  svn_ra_serf__add_xml_header_buckets(body_bkt, alloc);
  svn_ra_serf__add_open_tag_buckets(body_bkt, alloc, "D:checkout",
                                    "xmlns:D", "DAV:",
                                    NULL);
  svn_ra_serf__add_open_tag_buckets(body_bkt, alloc, "D:activity-set", NULL);
  svn_ra_serf__add_open_tag_buckets(body_bkt, alloc, "D:href", NULL);

  SVN_ERR_ASSERT(activity_url != NULL);
  svn_ra_serf__add_cdata_len_buckets(body_bkt, alloc,
                                     activity_url,
                                     strlen(activity_url));

  svn_ra_serf__add_close_tag_buckets(body_bkt, alloc, "D:href");
  svn_ra_serf__add_close_tag_buckets(body_bkt, alloc, "D:activity-set");
  svn_ra_serf__add_tag_buckets(body_bkt, "D:apply-to-version", NULL, alloc);
  svn_ra_serf__add_close_tag_buckets(body_bkt, alloc, "D:checkout");

  *bkt = body_bkt;
  return SVN_NO_ERROR;
}


/* Using the HTTPv1 protocol, perform a CHECKOUT of NODE_URL within the
   given COMMIT_CTX. The resulting working resource will be returned in
   *WORKING_URL, allocated from RESULT_POOL. All temporary allocations
   are performed in SCRATCH_POOL.

   ### are these URLs actually repos relpath values? or fspath? or maybe
   ### the abspath portion of the full URL.

   This function operates synchronously.

   Strictly speaking, we could perform "all" of the CHECKOUT requests
   when the commit starts, and only block when we need a specific
   answer. Or, at a minimum, send off these individual requests async
   and block when we need the answer (eg PUT or PROPPATCH).

   However: the investment to speed this up is not worthwhile, given
   that CHECKOUT (and the related round trip) is completely obviated
   in HTTPv2.
*/
static svn_error_t *
checkout_node(const char **working_url,
              const commit_context_t *commit_ctx,
              const char *node_url,
              apr_pool_t *result_pool,
              apr_pool_t *scratch_pool)
{
  svn_ra_serf__handler_t handler = { 0 };
  apr_status_t status;
  apr_uri_t uri;

  /* HANDLER_POOL is the scratch pool since we don't need to remember
     anything from the handler. We just want the working resource.  */
  handler.handler_pool = scratch_pool;
  handler.session = commit_ctx->session;
  handler.conn = commit_ctx->conn;

  handler.body_delegate = create_checkout_body;
  handler.body_delegate_baton = (/* const */ void *)commit_ctx->activity_url;
  handler.body_type = "text/xml";

  handler.response_handler = svn_ra_serf__expect_empty_body;
  handler.response_baton = &handler;

  handler.method = "CHECKOUT";
  handler.path = node_url;

  SVN_ERR(svn_ra_serf__context_run_one(&handler, scratch_pool));

  if (handler.sline.code != 201)
    return svn_error_trace(return_response_err(&handler));

  if (handler.location == NULL)
    return svn_error_create(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
                            _("No Location header received"));

  /* We only want the path portion of the Location header.
     (code.google.com sometimes returns an 'http:' scheme for an
     'https:' transaction ... we'll work around that by stripping the
     scheme, host, and port here and re-adding the correct ones
     later.  */
  status = apr_uri_parse(scratch_pool, handler.location, &uri);
  if (status)
    return svn_error_create(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
                            _("Error parsing Location header value"));

  *working_url = svn_urlpath__canonicalize(uri.path, result_pool);

  return SVN_NO_ERROR;
}


/* This is a wrapper around checkout_node() (which see for
   documentation) which simply retries the CHECKOUT request when it
   fails due to an SVN_ERR_APMOD_BAD_BASELINE error return from the
   server.

   See http://subversion.tigris.org/issues/show_bug.cgi?id=4127 for
   details.
*/
static svn_error_t *
retry_checkout_node(const char **working_url,
                    const commit_context_t *commit_ctx,
                    const char *node_url,
                    apr_pool_t *result_pool,
                    apr_pool_t *scratch_pool)
{
  svn_error_t *err = SVN_NO_ERROR;
  int retry_count = 5; /* Magic, arbitrary number. */

  do
    {
      svn_error_clear(err);

      err = checkout_node(working_url, commit_ctx, node_url,
                          result_pool, scratch_pool);

      /* There's a small chance of a race condition here if Apache is
         experiencing heavy commit concurrency or if the network has
         long latency.  It's possible that the value of HEAD changed
         between the time we fetched the latest baseline and the time
         we try to CHECKOUT that baseline.  If that happens, Apache
         will throw us a BAD_BASELINE error (deltaV says you can only
         checkout the latest baseline).  We just ignore that specific
         error and retry a few times, asking for the latest baseline
         again. */
      if (err && (err->apr_err != SVN_ERR_APMOD_BAD_BASELINE))
        return err;
    }
  while (err && retry_count--);

  return err;
}


static svn_error_t *
checkout_dir(dir_context_t *dir,
             apr_pool_t *scratch_pool)
{
  svn_error_t *err;
  dir_context_t *p_dir = dir;
  const char *checkout_url;
  const char **working;

  if (dir->working_url)
    {
      return SVN_NO_ERROR;
    }

  /* Is this directory or one of our parent dirs newly added?
   * If so, we're already implicitly checked out. */
  while (p_dir)
    {
      if (p_dir->added)
        {
          /* Calculate the working_url by skipping the shared ancestor bewteen
           * the parent->relpath and dir->relpath.  This is safe since an
           * add is guaranteed to have a parent that is checked out. */
          dir_context_t *parent = p_dir->parent_dir;
          const char *relpath = svn_relpath_skip_ancestor(parent->relpath,
                                                          dir->relpath);

          /* Implicitly checkout this dir now. */
          SVN_ERR_ASSERT(parent->working_url);
          dir->working_url = svn_path_url_add_component2(
                                   parent->working_url,
                                   relpath, dir->pool);
          return SVN_NO_ERROR;
        }
      p_dir = p_dir->parent_dir;
    }

  /* We could be called twice for the root: once to checkout the baseline;
   * once to checkout the directory itself if we need to do so.
   * Note: CHECKOUT_URL should live longer than HANDLER.
   */
  if (!dir->parent_dir && !dir->commit->baseline_url)
    {
      checkout_url = dir->commit->vcc_url;
      working = &dir->commit->baseline_url;
    }
  else
    {
      checkout_url = dir->url;
      working = &dir->working_url;
    }

  /* Checkout our directory into the activity URL now. */
  err = retry_checkout_node(working, dir->commit, checkout_url,
                            dir->pool, scratch_pool);
  if (err)
    {
      if (err->apr_err == SVN_ERR_FS_CONFLICT)
        SVN_ERR_W(err, apr_psprintf(scratch_pool,
                  _("Directory '%s' is out of date; try updating"),
                  svn_dirent_local_style(dir->relpath, scratch_pool)));
      return err;
    }

  return SVN_NO_ERROR;
}


/* Set *CHECKED_IN_URL to the appropriate DAV version url for
 * RELPATH (relative to the root of SESSION).
 *
 * Try to find this version url in three ways:
 * First, if SESSION->callbacks->get_wc_prop() is defined, try to read the
 * version url from the working copy properties.
 * Second, if the version url of the parent directory PARENT_VSN_URL is
 * defined, set *CHECKED_IN_URL to the concatenation of PARENT_VSN_URL with
 * RELPATH.
 * Else, fetch the version url for the root of SESSION using CONN and
 * BASE_REVISION, and set *CHECKED_IN_URL to the concatenation of that
 * with RELPATH.
 *
 * Allocate the result in RESULT_POOL, and use SCRATCH_POOL for
 * temporary allocation.
 */
static svn_error_t *
get_version_url(const char **checked_in_url,
                svn_ra_serf__session_t *session,
                const char *relpath,
                svn_revnum_t base_revision,
                const char *parent_vsn_url,
                apr_pool_t *result_pool,
                apr_pool_t *scratch_pool)
{
  const char *root_checkout;

  if (session->wc_callbacks->get_wc_prop)
    {
      const svn_string_t *current_version;

      SVN_ERR(session->wc_callbacks->get_wc_prop(
                session->wc_callback_baton,
                relpath,
                SVN_RA_SERF__WC_CHECKED_IN_URL,
                &current_version, scratch_pool));

      if (current_version)
        {
          *checked_in_url =
            svn_urlpath__canonicalize(current_version->data, result_pool);
          return SVN_NO_ERROR;
        }
    }

  if (parent_vsn_url)
    {
      root_checkout = parent_vsn_url;
    }
  else
    {
      const char *propfind_url;
      svn_ra_serf__connection_t *conn = session->conns[0];

      if (SVN_IS_VALID_REVNUM(base_revision))
        {
          /* mod_dav_svn can't handle the "Label:" header that
             svn_ra_serf__deliver_props() is going to try to use for
             this lookup, so we'll do things the hard(er) way, by
             looking up the version URL from a resource in the
             baseline collection. */
          /* ### conn==NULL for session->conns[0]. same as CONN.  */
          SVN_ERR(svn_ra_serf__get_stable_url(&propfind_url,
                                              NULL /* latest_revnum */,
                                              session, NULL /* conn */,
                                              NULL /* url */, base_revision,
                                              scratch_pool, scratch_pool));
        }
      else
        {
          propfind_url = session->session_url.path;
        }

      SVN_ERR(svn_ra_serf__fetch_dav_prop(&root_checkout,
                                          conn, propfind_url, base_revision,
                                          "checked-in",
                                          scratch_pool, scratch_pool));
      if (!root_checkout)
        return svn_error_createf(SVN_ERR_RA_DAV_REQUEST_FAILED, NULL,
                                 _("Path '%s' not present"),
                                 session->session_url.path);

      root_checkout = svn_urlpath__canonicalize(root_checkout, scratch_pool);
    }

  *checked_in_url = svn_path_url_add_component2(root_checkout, relpath,
                                                result_pool);

  return SVN_NO_ERROR;
}

static svn_error_t *
checkout_file(file_context_t *file,
              apr_pool_t *scratch_pool)
{
  svn_error_t *err;
  dir_context_t *parent_dir = file->parent_dir;
  const char *checkout_url;

  /* Is one of our parent dirs newly added?  If so, we're already
   * implicitly checked out.
   */
  while (parent_dir)
    {
      if (parent_dir->added)
        {
          /* Implicitly checkout this file now. */
          file->working_url = svn_path_url_add_component2(
                                    parent_dir->working_url,
                                    svn_relpath_skip_ancestor(
                                      parent_dir->relpath, file->relpath),
                                    file->pool);
          return SVN_NO_ERROR;
        }
      parent_dir = parent_dir->parent_dir;
    }

  SVN_ERR(get_version_url(&checkout_url,
                          file->commit->session,
                          file->relpath, file->base_revision,
                          NULL, scratch_pool, scratch_pool));

  /* Checkout our file into the activity URL now. */
  err = retry_checkout_node(&file->working_url, file->commit, checkout_url,
                            file->pool, scratch_pool);
  if (err)
    {
      if (err->apr_err == SVN_ERR_FS_CONFLICT)
        SVN_ERR_W(err, apr_psprintf(scratch_pool,
                  _("File '%s' is out of date; try updating"),
                  svn_dirent_local_style(file->relpath, scratch_pool)));
      return err;
    }

  return SVN_NO_ERROR;
}

/* Helper function for proppatch_walker() below. */
static svn_error_t *
get_encoding_and_cdata(const char **encoding_p,
                       const svn_string_t **encoded_value_p,
                       serf_bucket_alloc_t *alloc,
                       const svn_string_t *value,
                       apr_pool_t *result_pool,
                       apr_pool_t *scratch_pool)
{
  if (value == NULL)
    {
      *encoding_p = NULL;
      *encoded_value_p = NULL;
      return SVN_NO_ERROR;
    }

  /* If a property is XML-safe, XML-encode it.  Else, base64-encode
     it. */
  if (svn_xml_is_xml_safe(value->data, value->len))
    {
      svn_stringbuf_t *xml_esc = NULL;
      svn_xml_escape_cdata_string(&xml_esc, value, scratch_pool);
      *encoding_p = NULL;
      *encoded_value_p = svn_string_create_from_buf(xml_esc, result_pool);
    }
  else
    {
      *encoding_p = "base64";
      *encoded_value_p = svn_base64_encode_string2(value, TRUE, result_pool);
    }

  return SVN_NO_ERROR;
}

typedef struct walker_baton_t {
  serf_bucket_t *body_bkt;
  apr_pool_t *body_pool;

  apr_hash_t *previous_changed_props;
  apr_hash_t *previous_removed_props;

  const char *path;

  /* Hack, since change_rev_prop(old_value_p != NULL, value = NULL) uses D:set
     rather than D:remove...  (see notes/http-and-webdav/webdav-protocol) */
  enum {
    filter_all_props,
    filter_props_with_old_value,
    filter_props_without_old_value
  } filter;

  /* Is the property being deleted? */
  svn_boolean_t deleting;
} walker_baton_t;

/* If we have (recorded in WB) the old value of the property named NS:NAME,
 * then set *HAVE_OLD_VAL to TRUE and set *OLD_VAL_P to that old value
 * (which may be NULL); else set *HAVE_OLD_VAL to FALSE.  */
static svn_error_t *
derive_old_val(svn_boolean_t *have_old_val,
               const svn_string_t **old_val_p,
               walker_baton_t *wb,
               const char *ns,
               const char *name)
{
  *have_old_val = FALSE;

  if (wb->previous_changed_props)
    {
      const svn_string_t *val;
      val = svn_ra_serf__get_prop_string(wb->previous_changed_props,
                                         wb->path, ns, name);
      if (val)
        {
          *have_old_val = TRUE;
          *old_val_p = val;
        }
    }

  if (wb->previous_removed_props)
    {
      const svn_string_t *val;
      val = svn_ra_serf__get_prop_string(wb->previous_removed_props,
                                         wb->path, ns, name);
      if (val)
        {
          *have_old_val = TRUE;
          *old_val_p = NULL;
        }
    }

  return SVN_NO_ERROR;
}

static svn_error_t *
proppatch_walker(void *baton,
                 const char *ns,
                 const char *name,
                 const svn_string_t *val,
                 apr_pool_t *scratch_pool)
{
  walker_baton_t *wb = baton;
  serf_bucket_t *body_bkt = wb->body_bkt;
  serf_bucket_t *cdata_bkt;
  serf_bucket_alloc_t *alloc;
  const char *encoding;
  svn_boolean_t have_old_val;
  const svn_string_t *old_val;
  const svn_string_t *encoded_value;
  const char *prop_name;

  SVN_ERR(derive_old_val(&have_old_val, &old_val, wb, ns, name));

  /* Jump through hoops to work with D:remove and its val = (""-for-NULL)
   * representation. */
  if (wb->filter != filter_all_props)
    {
      if (wb->filter == filter_props_with_old_value && ! have_old_val)
      	return SVN_NO_ERROR;
      if (wb->filter == filter_props_without_old_value && have_old_val)
      	return SVN_NO_ERROR;
    }
  if (wb->deleting)
    val = NULL;

  alloc = body_bkt->allocator;

  SVN_ERR(get_encoding_and_cdata(&encoding, &encoded_value, alloc, val,
                                 wb->body_pool, scratch_pool));
  if (encoded_value)
    {
      cdata_bkt = SERF_BUCKET_SIMPLE_STRING_LEN(encoded_value->data,
                                                encoded_value->len,
                                                alloc);
    }
  else
    {
      cdata_bkt = NULL;
    }

  /* Use the namespace prefix instead of adding the xmlns attribute to support
     property names containing ':' */
  if (strcmp(ns, SVN_DAV_PROP_NS_SVN) == 0)
    prop_name = apr_pstrcat(wb->body_pool, "S:", name, (char *)NULL);
  else if (strcmp(ns, SVN_DAV_PROP_NS_CUSTOM) == 0)
    prop_name = apr_pstrcat(wb->body_pool, "C:", name, (char *)NULL);

  if (cdata_bkt)
    svn_ra_serf__add_open_tag_buckets(body_bkt, alloc, prop_name,
                                      "V:encoding", encoding,
                                      NULL);
  else
    svn_ra_serf__add_open_tag_buckets(body_bkt, alloc, prop_name,
                                      "V:" SVN_DAV__OLD_VALUE__ABSENT, "1",
                                      NULL);

  if (have_old_val)
    {
      const char *encoding2;
      const svn_string_t *encoded_value2;
      serf_bucket_t *cdata_bkt2;

      SVN_ERR(get_encoding_and_cdata(&encoding2, &encoded_value2,
                                     alloc, old_val,
                                     wb->body_pool, scratch_pool));

      if (encoded_value2)
        {
          cdata_bkt2 = SERF_BUCKET_SIMPLE_STRING_LEN(encoded_value2->data,
                                                     encoded_value2->len,
                                                     alloc);
        }
      else
        {
          cdata_bkt2 = NULL;
        }

      if (cdata_bkt2)
        svn_ra_serf__add_open_tag_buckets(body_bkt, alloc,
                                          "V:" SVN_DAV__OLD_VALUE,
                                          "V:encoding", encoding2,
                                          NULL);
      else
        svn_ra_serf__add_open_tag_buckets(body_bkt, alloc,
                                          "V:" SVN_DAV__OLD_VALUE,
                                          "V:" SVN_DAV__OLD_VALUE__ABSENT, "1",
                                          NULL);

      if (cdata_bkt2)
        serf_bucket_aggregate_append(body_bkt, cdata_bkt2);

      svn_ra_serf__add_close_tag_buckets(body_bkt, alloc,
                                         "V:" SVN_DAV__OLD_VALUE);
    }
  if (cdata_bkt)
    serf_bucket_aggregate_append(body_bkt, cdata_bkt);
  svn_ra_serf__add_close_tag_buckets(body_bkt, alloc, prop_name);

  return SVN_NO_ERROR;
}

/* Possible add the lock-token "If:" precondition header to HEADERS if
   an examination of COMMIT_CTX and RELPATH indicates that this is the
   right thing to do.

   Generally speaking, if the client provided a lock token for
   RELPATH, it's the right thing to do.  There is a notable instance
   where this is not the case, however.  If the file at RELPATH was
   explicitly deleted in this commit already, then mod_dav removed its
   lock token when it fielded the DELETE request, so we don't want to
   set the lock precondition again.  (See
   http://subversion.tigris.org/issues/show_bug.cgi?id=3674 for details.)
*/
static svn_error_t *
maybe_set_lock_token_header(serf_bucket_t *headers,
                            commit_context_t *commit_ctx,
                            const char *relpath,
                            apr_pool_t *pool)
{
  const char *token;

  if (! (relpath && commit_ctx->lock_tokens))
    return SVN_NO_ERROR;

  if (! svn_hash_gets(commit_ctx->deleted_entries, relpath))
    {
      token = svn_hash_gets(commit_ctx->lock_tokens, relpath);
      if (token)
        {
          const char *token_header;
          const char *token_uri;
          apr_uri_t uri = commit_ctx->session->session_url;

          /* Supplying the optional URI affects apache response when
             the lock is broken, see issue 4369.  When present any URI
             must be absolute (RFC 2518 9.4). */
          uri.path = (char *)svn_path_url_add_component2(uri.path, relpath,
                                                         pool);
          token_uri = apr_uri_unparse(pool, &uri, 0);

          token_header = apr_pstrcat(pool, "<", token_uri, "> (<", token, ">)",
                                     (char *)NULL);
          serf_bucket_headers_set(headers, "If", token_header);
        }
    }

  return SVN_NO_ERROR;
}

static svn_error_t *
setup_proppatch_headers(serf_bucket_t *headers,
                        void *baton,
                        apr_pool_t *pool)
{
  proppatch_context_t *proppatch = baton;

  if (SVN_IS_VALID_REVNUM(proppatch->base_revision))
    {
      serf_bucket_headers_set(headers, SVN_DAV_VERSION_NAME_HEADER,
                              apr_psprintf(pool, "%ld",
                                           proppatch->base_revision));
    }

  SVN_ERR(maybe_set_lock_token_header(headers, proppatch->commit,
                                      proppatch->relpath, pool));

  return SVN_NO_ERROR;
}


struct proppatch_body_baton_t {
  proppatch_context_t *proppatch;

  /* Content in the body should be allocated here, to live long enough.  */
  apr_pool_t *body_pool;
};

/* Implements svn_ra_serf__request_body_delegate_t */
static svn_error_t *
create_proppatch_body(serf_bucket_t **bkt,
                      void *baton,
                      serf_bucket_alloc_t *alloc,
                      apr_pool_t *scratch_pool)
{
  struct proppatch_body_baton_t *pbb = baton;
  proppatch_context_t *ctx = pbb->proppatch;
  serf_bucket_t *body_bkt;
  walker_baton_t wb = { 0 };

  body_bkt = serf_bucket_aggregate_create(alloc);

  svn_ra_serf__add_xml_header_buckets(body_bkt, alloc);
  svn_ra_serf__add_open_tag_buckets(body_bkt, alloc, "D:propertyupdate",
                                    "xmlns:D", "DAV:",
                                    "xmlns:V", SVN_DAV_PROP_NS_DAV,
                                    "xmlns:C", SVN_DAV_PROP_NS_CUSTOM,
                                    "xmlns:S", SVN_DAV_PROP_NS_SVN,
                                    NULL);

  wb.body_bkt = body_bkt;
  wb.body_pool = pbb->body_pool;
  wb.previous_changed_props = ctx->previous_changed_props;
  wb.previous_removed_props = ctx->previous_removed_props;
  wb.path = ctx->path;

  if (apr_hash_count(ctx->changed_props) > 0)
    {
      svn_ra_serf__add_open_tag_buckets(body_bkt, alloc, "D:set", NULL);
      svn_ra_serf__add_open_tag_buckets(body_bkt, alloc, "D:prop", NULL);

      wb.filter = filter_all_props;
      wb.deleting = FALSE;
      SVN_ERR(svn_ra_serf__walk_all_props(ctx->changed_props, ctx->path,
                                          SVN_INVALID_REVNUM,
                                          proppatch_walker, &wb,
                                          scratch_pool));

      svn_ra_serf__add_close_tag_buckets(body_bkt, alloc, "D:prop");
      svn_ra_serf__add_close_tag_buckets(body_bkt, alloc, "D:set");
    }

  if (apr_hash_count(ctx->removed_props) > 0)
    {
      svn_ra_serf__add_open_tag_buckets(body_bkt, alloc, "D:set", NULL);
      svn_ra_serf__add_open_tag_buckets(body_bkt, alloc, "D:prop", NULL);

      wb.filter = filter_props_with_old_value;
      wb.deleting = TRUE;
      SVN_ERR(svn_ra_serf__walk_all_props(ctx->removed_props, ctx->path,
                                          SVN_INVALID_REVNUM,
                                          proppatch_walker, &wb,
                                          scratch_pool));

      svn_ra_serf__add_close_tag_buckets(body_bkt, alloc, "D:prop");
      svn_ra_serf__add_close_tag_buckets(body_bkt, alloc, "D:set");
    }

  if (apr_hash_count(ctx->removed_props) > 0)
    {
      svn_ra_serf__add_open_tag_buckets(body_bkt, alloc, "D:remove", NULL);
      svn_ra_serf__add_open_tag_buckets(body_bkt, alloc, "D:prop", NULL);

      wb.filter = filter_props_without_old_value;
      wb.deleting = TRUE;
      SVN_ERR(svn_ra_serf__walk_all_props(ctx->removed_props, ctx->path,
                                          SVN_INVALID_REVNUM,
                                          proppatch_walker, &wb,
                                          scratch_pool));

      svn_ra_serf__add_close_tag_buckets(body_bkt, alloc, "D:prop");
      svn_ra_serf__add_close_tag_buckets(body_bkt, alloc, "D:remove");
    }

  svn_ra_serf__add_close_tag_buckets(body_bkt, alloc, "D:propertyupdate");

  *bkt = body_bkt;
  return SVN_NO_ERROR;
}

static svn_error_t*
proppatch_resource(proppatch_context_t *proppatch,
                   commit_context_t *commit,
                   apr_pool_t *pool)
{
  svn_ra_serf__handler_t *handler;
  struct proppatch_body_baton_t pbb;

  handler = apr_pcalloc(pool, sizeof(*handler));
  handler->handler_pool = pool;
  handler->method = "PROPPATCH";
  handler->path = proppatch->path;
  handler->conn = commit->conn;
  handler->session = commit->session;

  handler->header_delegate = setup_proppatch_headers;
  handler->header_delegate_baton = proppatch;

  pbb.proppatch = proppatch;
  pbb.body_pool = pool;
  handler->body_delegate = create_proppatch_body;
  handler->body_delegate_baton = &pbb;

  handler->response_handler = svn_ra_serf__handle_multistatus_only;
  handler->response_baton = handler;

  SVN_ERR(svn_ra_serf__context_run_one(handler, pool));

  if (handler->sline.code != 207
      || (handler->server_error != NULL
          && handler->server_error->error != NULL))
    {
      return svn_error_create(
               SVN_ERR_RA_DAV_PROPPATCH_FAILED,
               return_response_err(handler),
               _("At least one property change failed; repository"
                 " is unchanged"));
    }

  return SVN_NO_ERROR;
}

/* Implements svn_ra_serf__request_body_delegate_t */
static svn_error_t *
create_put_body(serf_bucket_t **body_bkt,
                void *baton,
                serf_bucket_alloc_t *alloc,
                apr_pool_t *pool)
{
  file_context_t *ctx = baton;
  apr_off_t offset;

  /* We need to flush the file, make it unbuffered (so that it can be
   * zero-copied via mmap), and reset the position before attempting to
   * deliver the file.
   *
   * N.B. If we have APR 1.3+, we can unbuffer the file to let us use mmap
   * and zero-copy the PUT body.  However, on older APR versions, we can't
   * check the buffer status; but serf will fall through and create a file
   * bucket for us on the buffered svndiff handle.
   */
  apr_file_flush(ctx->svndiff);
#if APR_VERSION_AT_LEAST(1, 3, 0)
  apr_file_buffer_set(ctx->svndiff, NULL, 0);
#endif
  offset = 0;
  apr_file_seek(ctx->svndiff, APR_SET, &offset);

  *body_bkt = serf_bucket_file_create(ctx->svndiff, alloc);
  return SVN_NO_ERROR;
}

/* Implements svn_ra_serf__request_body_delegate_t */
static svn_error_t *
create_empty_put_body(serf_bucket_t **body_bkt,
                      void *baton,
                      serf_bucket_alloc_t *alloc,
                      apr_pool_t *pool)
{
  *body_bkt = SERF_BUCKET_SIMPLE_STRING("", alloc);
  return SVN_NO_ERROR;
}

static svn_error_t *
setup_put_headers(serf_bucket_t *headers,
                  void *baton,
                  apr_pool_t *pool)
{
  file_context_t *ctx = baton;

  if (SVN_IS_VALID_REVNUM(ctx->base_revision))
    {
      serf_bucket_headers_set(headers, SVN_DAV_VERSION_NAME_HEADER,
                              apr_psprintf(pool, "%ld", ctx->base_revision));
    }

  if (ctx->base_checksum)
    {
      serf_bucket_headers_set(headers, SVN_DAV_BASE_FULLTEXT_MD5_HEADER,
                              ctx->base_checksum);
    }

  if (ctx->result_checksum)
    {
      serf_bucket_headers_set(headers, SVN_DAV_RESULT_FULLTEXT_MD5_HEADER,
                              ctx->result_checksum);
    }

  SVN_ERR(maybe_set_lock_token_header(headers, ctx->commit,
                                      ctx->relpath, pool));

  return APR_SUCCESS;
}

static svn_error_t *
setup_copy_file_headers(serf_bucket_t *headers,
                        void *baton,
                        apr_pool_t *pool)
{
  file_context_t *file = baton;
  apr_uri_t uri;
  const char *absolute_uri;

  /* The Dest URI must be absolute.  Bummer. */
  uri = file->commit->session->session_url;
  uri.path = (char*)file->url;
  absolute_uri = apr_uri_unparse(pool, &uri, 0);

  serf_bucket_headers_set(headers, "Destination", absolute_uri);

  serf_bucket_headers_setn(headers, "Depth", "0");
  serf_bucket_headers_setn(headers, "Overwrite", "T");

  return SVN_NO_ERROR;
}

static svn_error_t *
setup_copy_dir_headers(serf_bucket_t *headers,
                       void *baton,
                       apr_pool_t *pool)
{
  dir_context_t *dir = baton;
  apr_uri_t uri;
  const char *absolute_uri;

  /* The Dest URI must be absolute.  Bummer. */
  uri = dir->commit->session->session_url;

  if (USING_HTTPV2_COMMIT_SUPPORT(dir->commit))
    {
      uri.path = (char *)dir->url;
    }
  else
    {
      uri.path = (char *)svn_path_url_add_component2(
                                    dir->parent_dir->working_url,
                                    dir->name, pool);
    }
  absolute_uri = apr_uri_unparse(pool, &uri, 0);

  serf_bucket_headers_set(headers, "Destination", absolute_uri);

  serf_bucket_headers_setn(headers, "Depth", "infinity");
  serf_bucket_headers_setn(headers, "Overwrite", "T");

  /* Implicitly checkout this dir now. */
  dir->working_url = apr_pstrdup(dir->pool, uri.path);

  return SVN_NO_ERROR;
}

static svn_error_t *
setup_delete_headers(serf_bucket_t *headers,
                     void *baton,
                     apr_pool_t *pool)
{
  delete_context_t *ctx = baton;

  serf_bucket_headers_set(headers, SVN_DAV_VERSION_NAME_HEADER,
                          apr_ltoa(pool, ctx->revision));

  if (ctx->lock_token_hash)
    {
      ctx->lock_token = svn_hash_gets(ctx->lock_token_hash, ctx->path);

      if (ctx->lock_token)
        {
          const char *token_header;

          token_header = apr_pstrcat(pool, "<", ctx->path, "> (<",
                                     ctx->lock_token, ">)", (char *)NULL);

          serf_bucket_headers_set(headers, "If", token_header);

          if (ctx->keep_locks)
            serf_bucket_headers_setn(headers, SVN_DAV_OPTIONS_HEADER,
                                     SVN_DAV_OPTION_KEEP_LOCKS);
        }
    }

  return SVN_NO_ERROR;
}

/* Implements svn_ra_serf__request_body_delegate_t */
static svn_error_t *
create_delete_body(serf_bucket_t **body_bkt,
                   void *baton,
                   serf_bucket_alloc_t *alloc,
                   apr_pool_t *pool)
{
  delete_context_t *ctx = baton;
  serf_bucket_t *body;

  body = serf_bucket_aggregate_create(alloc);

  svn_ra_serf__add_xml_header_buckets(body, alloc);

  svn_ra_serf__merge_lock_token_list(ctx->lock_token_hash, ctx->path,
                                     body, alloc, pool);

  *body_bkt = body;
  return SVN_NO_ERROR;
}

/* Helper function to write the svndiff stream to temporary file. */
static svn_error_t *
svndiff_stream_write(void *file_baton,
                     const char *data,
                     apr_size_t *len)
{
  file_context_t *ctx = file_baton;
  apr_status_t status;

  status = apr_file_write_full(ctx->svndiff, data, *len, NULL);
  if (status)
      return svn_error_wrap_apr(status, _("Failed writing updated file"));

  return SVN_NO_ERROR;
}



/* POST against 'me' resource handlers. */

/* Implements svn_ra_serf__request_body_delegate_t */
static svn_error_t *
create_txn_post_body(serf_bucket_t **body_bkt,
                     void *baton,
                     serf_bucket_alloc_t *alloc,
                     apr_pool_t *pool)
{
  apr_hash_t *revprops = baton;
  svn_skel_t *request_skel;
  svn_stringbuf_t *skel_str;

  request_skel = svn_skel__make_empty_list(pool);
  if (revprops)
    {
      svn_skel_t *proplist_skel;

      SVN_ERR(svn_skel__unparse_proplist(&proplist_skel, revprops, pool));
      svn_skel__prepend(proplist_skel, request_skel);
      svn_skel__prepend_str("create-txn-with-props", request_skel, pool);
      skel_str = svn_skel__unparse(request_skel, pool);
      *body_bkt = SERF_BUCKET_SIMPLE_STRING(skel_str->data, alloc);
    }
  else
    {
      *body_bkt = SERF_BUCKET_SIMPLE_STRING("( create-txn )", alloc);
    }

  return SVN_NO_ERROR;
}

/* Implements svn_ra_serf__request_header_delegate_t */
static svn_error_t *
setup_post_headers(serf_bucket_t *headers,
                   void *baton,
                   apr_pool_t *pool)
{
#ifdef SVN_DAV_SEND_VTXN_NAME
  /* Enable this to exercise the VTXN-NAME code based on a client
     supplied transaction name. */
  serf_bucket_headers_set(headers, SVN_DAV_VTXN_NAME_HEADER,
                          svn_uuid_generate(pool));
#endif

  return SVN_NO_ERROR;
}


/* Handler baton for POST request. */
typedef struct post_response_ctx_t
{
  svn_ra_serf__handler_t *handler;
  commit_context_t *commit_ctx;
} post_response_ctx_t;


/* This implements serf_bucket_headers_do_callback_fn_t.   */
static int
post_headers_iterator_callback(void *baton,
                               const char *key,
                               const char *val)
{
  post_response_ctx_t *prc = baton;
  commit_context_t *prc_cc = prc->commit_ctx;
  svn_ra_serf__session_t *sess = prc_cc->session;

  /* If we provided a UUID to the POST request, we should get back
     from the server an SVN_DAV_VTXN_NAME_HEADER header; otherwise we
     expect the SVN_DAV_TXN_NAME_HEADER.  We certainly don't expect to
     see both. */

  if (svn_cstring_casecmp(key, SVN_DAV_TXN_NAME_HEADER) == 0)
    {
      /* Build out txn and txn-root URLs using the txn name we're
         given, and store the whole lot of it in the commit context.  */
      prc_cc->txn_url =
        svn_path_url_add_component2(sess->txn_stub, val, prc_cc->pool);
      prc_cc->txn_root_url =
        svn_path_url_add_component2(sess->txn_root_stub, val, prc_cc->pool);
    }

  if (svn_cstring_casecmp(key, SVN_DAV_VTXN_NAME_HEADER) == 0)
    {
      /* Build out vtxn and vtxn-root URLs using the vtxn name we're
         given, and store the whole lot of it in the commit context.  */
      prc_cc->txn_url =
        svn_path_url_add_component2(sess->vtxn_stub, val, prc_cc->pool);
      prc_cc->txn_root_url =
        svn_path_url_add_component2(sess->vtxn_root_stub, val, prc_cc->pool);
    }

  return 0;
}


/* A custom serf_response_handler_t which is mostly a wrapper around
   svn_ra_serf__expect_empty_body -- it just notices POST response
   headers, too.

   Implements svn_ra_serf__response_handler_t */
static svn_error_t *
post_response_handler(serf_request_t *request,
                      serf_bucket_t *response,
                      void *baton,
                      apr_pool_t *scratch_pool)
{
  post_response_ctx_t *prc = baton;
  serf_bucket_t *hdrs = serf_bucket_response_get_headers(response);

  /* Then see which ones we can discover. */
  serf_bucket_headers_do(hdrs, post_headers_iterator_callback, prc);

  /* Execute the 'real' response handler to XML-parse the repsonse body. */
  return svn_ra_serf__expect_empty_body(request, response,
                                        prc->handler, scratch_pool);
}



/* Commit baton callbacks */

static svn_error_t *
open_root(void *edit_baton,
          svn_revnum_t base_revision,
          apr_pool_t *dir_pool,
          void **root_baton)
{
  commit_context_t *ctx = edit_baton;
  svn_ra_serf__handler_t *handler;
  proppatch_context_t *proppatch_ctx;
  dir_context_t *dir;
  apr_hash_index_t *hi;
  const char *proppatch_target = NULL;

  if (SVN_RA_SERF__HAVE_HTTPV2_SUPPORT(ctx->session))
    {
      post_response_ctx_t *prc;
      const char *rel_path;
      svn_boolean_t post_with_revprops
        = (NULL != svn_hash_gets(ctx->session->supported_posts,
                                 "create-txn-with-props"));

      /* Create our activity URL now on the server. */
      handler = apr_pcalloc(ctx->pool, sizeof(*handler));
      handler->handler_pool = ctx->pool;
      handler->method = "POST";
      handler->body_type = SVN_SKEL_MIME_TYPE;
      handler->body_delegate = create_txn_post_body;
      handler->body_delegate_baton =
        post_with_revprops ? ctx->revprop_table : NULL;
      handler->header_delegate = setup_post_headers;
      handler->header_delegate_baton = NULL;
      handler->path = ctx->session->me_resource;
      handler->conn = ctx->session->conns[0];
      handler->session = ctx->session;

      prc = apr_pcalloc(ctx->pool, sizeof(*prc));
      prc->handler = handler;
      prc->commit_ctx = ctx;

      handler->response_handler = post_response_handler;
      handler->response_baton = prc;

      SVN_ERR(svn_ra_serf__context_run_one(handler, ctx->pool));

      if (handler->sline.code != 201)
        {
          apr_status_t status = SVN_ERR_RA_DAV_REQUEST_FAILED;

          switch (handler->sline.code)
            {
              case 403:
                status = SVN_ERR_RA_DAV_FORBIDDEN;
                break;
              case 404:
                status = SVN_ERR_FS_NOT_FOUND;
                break;
            }

          return svn_error_createf(status, NULL,
                                   _("%s of '%s': %d %s (%s://%s)"),
                                   handler->method, handler->path,
                                   handler->sline.code, handler->sline.reason,
                                   ctx->session->session_url.scheme,
                                   ctx->session->session_url.hostinfo);
        }
      if (! (ctx->txn_root_url && ctx->txn_url))
        {
          return svn_error_createf(
            SVN_ERR_RA_DAV_REQUEST_FAILED, NULL,
            _("POST request did not return transaction information"));
        }

      /* Fixup the txn_root_url to point to the anchor of the commit. */
      SVN_ERR(svn_ra_serf__get_relative_path(&rel_path,
                                             ctx->session->session_url.path,
                                             ctx->session, NULL, dir_pool));
      ctx->txn_root_url = svn_path_url_add_component2(ctx->txn_root_url,
                                                      rel_path, ctx->pool);

      /* Build our directory baton. */
      dir = apr_pcalloc(dir_pool, sizeof(*dir));
      dir->pool = dir_pool;
      dir->commit = ctx;
      dir->base_revision = base_revision;
      dir->relpath = "";
      dir->name = "";
      dir->changed_props = apr_hash_make(dir->pool);
      dir->removed_props = apr_hash_make(dir->pool);
      dir->url = apr_pstrdup(dir->pool, ctx->txn_root_url);

      /* If we included our revprops in the POST, we need not
         PROPPATCH them. */
      proppatch_target = post_with_revprops ? NULL : ctx->txn_url;
    }
  else
    {
      const char *activity_str = ctx->session->activity_collection_url;

      if (!activity_str)
        SVN_ERR(svn_ra_serf__v1_get_activity_collection(&activity_str,
                                                        ctx->session->conns[0],
                                                        ctx->pool,
                                                        ctx->pool));

      /* Cache the result. */
      if (activity_str)
        {
          ctx->session->activity_collection_url =
            apr_pstrdup(ctx->session->pool, activity_str);
        }
      else
        {
          return svn_error_create(SVN_ERR_RA_DAV_OPTIONS_REQ_FAILED, NULL,
                                  _("The OPTIONS response did not include the "
                                    "requested activity-collection-set value"));
        }

      ctx->activity_url =
        svn_path_url_add_component2(activity_str, svn_uuid_generate(ctx->pool),
                                    ctx->pool);

      /* Create our activity URL now on the server. */
      handler = apr_pcalloc(ctx->pool, sizeof(*handler));
      handler->handler_pool = ctx->pool;
      handler->method = "MKACTIVITY";
      handler->path = ctx->activity_url;
      handler->conn = ctx->session->conns[0];
      handler->session = ctx->session;

      handler->response_handler = svn_ra_serf__expect_empty_body;
      handler->response_baton = handler;

      SVN_ERR(svn_ra_serf__context_run_one(handler, ctx->pool));

      if (handler->sline.code != 201)
        {
          apr_status_t status = SVN_ERR_RA_DAV_REQUEST_FAILED;

          switch (handler->sline.code)
            {
              case 403:
                status = SVN_ERR_RA_DAV_FORBIDDEN;
                break;
              case 404:
                status = SVN_ERR_FS_NOT_FOUND;
                break;
            }

          return svn_error_createf(status, NULL,
                                   _("%s of '%s': %d %s (%s://%s)"),
                                   handler->method, handler->path,
                                   handler->sline.code, handler->sline.reason,
                                   ctx->session->session_url.scheme,
                                   ctx->session->session_url.hostinfo);
        }

      /* Now go fetch our VCC and baseline so we can do a CHECKOUT. */
      SVN_ERR(svn_ra_serf__discover_vcc(&(ctx->vcc_url), ctx->session,
                                        ctx->conn, ctx->pool));


      /* Build our directory baton. */
      dir = apr_pcalloc(dir_pool, sizeof(*dir));
      dir->pool = dir_pool;
      dir->commit = ctx;
      dir->base_revision = base_revision;
      dir->relpath = "";
      dir->name = "";
      dir->changed_props = apr_hash_make(dir->pool);
      dir->removed_props = apr_hash_make(dir->pool);

      SVN_ERR(get_version_url(&dir->url, dir->commit->session,
                              dir->relpath,
                              dir->base_revision, ctx->checked_in_url,
                              dir->pool, dir->pool /* scratch_pool */));
      ctx->checked_in_url = dir->url;

      /* Checkout our root dir */
      SVN_ERR(checkout_dir(dir, dir->pool /* scratch_pool */));

      proppatch_target = ctx->baseline_url;
    }

  /* Unless this is NULL -- which means we don't need to PROPPATCH the
     transaction with our revprops -- then, you know, PROPPATCH the
     transaction with our revprops.  */
  if (proppatch_target)
    {
      proppatch_ctx = apr_pcalloc(ctx->pool, sizeof(*proppatch_ctx));
      proppatch_ctx->pool = dir_pool;
      proppatch_ctx->commit = ctx;
      proppatch_ctx->path = proppatch_target;
      proppatch_ctx->changed_props = apr_hash_make(proppatch_ctx->pool);
      proppatch_ctx->removed_props = apr_hash_make(proppatch_ctx->pool);
      proppatch_ctx->base_revision = SVN_INVALID_REVNUM;

      for (hi = apr_hash_first(ctx->pool, ctx->revprop_table); hi;
           hi = apr_hash_next(hi))
        {
          const char *name = svn__apr_hash_index_key(hi);
          svn_string_t *value = svn__apr_hash_index_val(hi);
          const char *ns;

          if (strncmp(name, SVN_PROP_PREFIX, sizeof(SVN_PROP_PREFIX) - 1) == 0)
            {
              ns = SVN_DAV_PROP_NS_SVN;
              name += sizeof(SVN_PROP_PREFIX) - 1;
            }
          else
            {
              ns = SVN_DAV_PROP_NS_CUSTOM;
            }

          svn_ra_serf__set_prop(proppatch_ctx->changed_props,
                                proppatch_ctx->path,
                                ns, name, value, proppatch_ctx->pool);
        }

      SVN_ERR(proppatch_resource(proppatch_ctx, dir->commit, ctx->pool));
    }

  *root_baton = dir;

  return SVN_NO_ERROR;
}

static svn_error_t *
delete_entry(const char *path,
             svn_revnum_t revision,
             void *parent_baton,
             apr_pool_t *pool)
{
  dir_context_t *dir = parent_baton;
  delete_context_t *delete_ctx;
  svn_ra_serf__handler_t *handler;
  const char *delete_target;
  svn_error_t *err;

  if (USING_HTTPV2_COMMIT_SUPPORT(dir->commit))
    {
      delete_target = svn_path_url_add_component2(dir->commit->txn_root_url,
                                                  path, dir->pool);
    }
  else
    {
      /* Ensure our directory has been checked out */
      SVN_ERR(checkout_dir(dir, pool /* scratch_pool */));
      delete_target = svn_path_url_add_component2(dir->working_url,
                                                  svn_relpath_basename(path,
                                                                       NULL),
                                                  pool);
    }

  /* DELETE our entry */
  delete_ctx = apr_pcalloc(pool, sizeof(*delete_ctx));
  delete_ctx->path = apr_pstrdup(pool, path);
  delete_ctx->revision = revision;
  delete_ctx->lock_token_hash = dir->commit->lock_tokens;
  delete_ctx->keep_locks = dir->commit->keep_locks;

  handler = apr_pcalloc(pool, sizeof(*handler));
  handler->handler_pool = pool;
  handler->session = dir->commit->session;
  handler->conn = dir->commit->conn;

  handler->response_handler = svn_ra_serf__expect_empty_body;
  handler->response_baton = handler;

  handler->header_delegate = setup_delete_headers;
  handler->header_delegate_baton = delete_ctx;

  handler->method = "DELETE";
  handler->path = delete_target;

  err = svn_ra_serf__context_run_one(handler, pool);

  if (err &&
      (err->apr_err == SVN_ERR_FS_BAD_LOCK_TOKEN ||
       err->apr_err == SVN_ERR_FS_NO_LOCK_TOKEN ||
       err->apr_err == SVN_ERR_FS_LOCK_OWNER_MISMATCH ||
       err->apr_err == SVN_ERR_FS_PATH_ALREADY_LOCKED))
    {
      svn_error_clear(err);

      /* An error has been registered on the connection. Reset the thing
         so that we can use it again.  */
      serf_connection_reset(handler->conn->conn);

      handler->body_delegate = create_delete_body;
      handler->body_delegate_baton = delete_ctx;
      handler->body_type = "text/xml";

      SVN_ERR(svn_ra_serf__context_run_one(handler, pool));
    }
  else if (err)
    {
      return err;
    }

  /* 204 No Content: item successfully deleted */
  if (handler->sline.code != 204)
    {
      return svn_error_trace(return_response_err(handler));
    }

  svn_hash_sets(dir->commit->deleted_entries,
                apr_pstrdup(dir->commit->pool, path), (void *)1);

  return SVN_NO_ERROR;
}

static svn_error_t *
add_directory(const char *path,
              void *parent_baton,
              const char *copyfrom_path,
              svn_revnum_t copyfrom_revision,
              apr_pool_t *dir_pool,
              void **child_baton)
{
  dir_context_t *parent = parent_baton;
  dir_context_t *dir;
  svn_ra_serf__handler_t *handler;
  apr_status_t status;
  const char *mkcol_target;

  dir = apr_pcalloc(dir_pool, sizeof(*dir));

  dir->pool = dir_pool;
  dir->parent_dir = parent;
  dir->commit = parent->commit;
  dir->added = TRUE;
  dir->base_revision = SVN_INVALID_REVNUM;
  dir->copy_revision = copyfrom_revision;
  dir->copy_path = copyfrom_path;
  dir->relpath = apr_pstrdup(dir->pool, path);
  dir->name = svn_relpath_basename(dir->relpath, NULL);
  dir->changed_props = apr_hash_make(dir->pool);
  dir->removed_props = apr_hash_make(dir->pool);

  if (USING_HTTPV2_COMMIT_SUPPORT(dir->commit))
    {
      dir->url = svn_path_url_add_component2(parent->commit->txn_root_url,
                                             path, dir->pool);
      mkcol_target = dir->url;
    }
  else
    {
      /* Ensure our parent is checked out. */
      SVN_ERR(checkout_dir(parent, dir->pool /* scratch_pool */));

      dir->url = svn_path_url_add_component2(parent->commit->checked_in_url,
                                             dir->name, dir->pool);
      mkcol_target = svn_path_url_add_component2(
                               parent->working_url,
                               dir->name, dir->pool);
    }

  handler = apr_pcalloc(dir->pool, sizeof(*handler));
  handler->handler_pool = dir->pool;
  handler->conn = dir->commit->conn;
  handler->session = dir->commit->session;

  handler->response_handler = svn_ra_serf__expect_empty_body;
  handler->response_baton = handler;
  if (!dir->copy_path)
    {
      handler->method = "MKCOL";
      handler->path = mkcol_target;
    }
  else
    {
      apr_uri_t uri;
      const char *req_url;

      status = apr_uri_parse(dir->pool, dir->copy_path, &uri);
      if (status)
        {
          return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
                                   _("Unable to parse URL '%s'"),
                                   dir->copy_path);
        }

      /* ### conn==NULL for session->conns[0]. same as commit->conn.  */
      SVN_ERR(svn_ra_serf__get_stable_url(&req_url, NULL /* latest_revnum */,
                                          dir->commit->session,
                                          NULL /* conn */,
                                          uri.path, dir->copy_revision,
                                          dir_pool, dir_pool));

      handler->method = "COPY";
      handler->path = req_url;

      handler->header_delegate = setup_copy_dir_headers;
      handler->header_delegate_baton = dir;
    }

  SVN_ERR(svn_ra_serf__context_run_one(handler, dir->pool));

  switch (handler->sline.code)
    {
      case 201: /* Created:    item was successfully copied */
      case 204: /* No Content: item successfully replaced an existing target */
        break;

      case 403:
        return svn_error_createf(SVN_ERR_RA_DAV_FORBIDDEN, NULL,
                                _("Access to '%s' forbidden"),
                                 handler->path);
      default:
        return svn_error_createf(SVN_ERR_RA_DAV_REQUEST_FAILED, NULL,
                                 _("Adding directory failed: %s on %s "
                                   "(%d %s)"),
                                 handler->method, handler->path,
                                 handler->sline.code, handler->sline.reason);
    }

  *child_baton = dir;

  return SVN_NO_ERROR;
}

static svn_error_t *
open_directory(const char *path,
               void *parent_baton,
               svn_revnum_t base_revision,
               apr_pool_t *dir_pool,
               void **child_baton)
{
  dir_context_t *parent = parent_baton;
  dir_context_t *dir;

  dir = apr_pcalloc(dir_pool, sizeof(*dir));

  dir->pool = dir_pool;

  dir->parent_dir = parent;
  dir->commit = parent->commit;

  dir->added = FALSE;
  dir->base_revision = base_revision;
  dir->relpath = apr_pstrdup(dir->pool, path);
  dir->name = svn_relpath_basename(dir->relpath, NULL);
  dir->changed_props = apr_hash_make(dir->pool);
  dir->removed_props = apr_hash_make(dir->pool);

  if (USING_HTTPV2_COMMIT_SUPPORT(dir->commit))
    {
      dir->url = svn_path_url_add_component2(parent->commit->txn_root_url,
                                             path, dir->pool);
    }
  else
    {
      SVN_ERR(get_version_url(&dir->url,
                              dir->commit->session,
                              dir->relpath, dir->base_revision,
                              dir->commit->checked_in_url,
                              dir->pool, dir->pool /* scratch_pool */));
    }
  *child_baton = dir;

  return SVN_NO_ERROR;
}

static svn_error_t *
change_dir_prop(void *dir_baton,
                const char *name,
                const svn_string_t *value,
                apr_pool_t *pool)
{
  dir_context_t *dir = dir_baton;
  const char *ns;
  const char *proppatch_target;


  if (USING_HTTPV2_COMMIT_SUPPORT(dir->commit))
    {
      proppatch_target = dir->url;
    }
  else
    {
      /* Ensure we have a checked out dir. */
      SVN_ERR(checkout_dir(dir, pool /* scratch_pool */));

      proppatch_target = dir->working_url;
    }

  name = apr_pstrdup(dir->pool, name);
  if (strncmp(name, SVN_PROP_PREFIX, sizeof(SVN_PROP_PREFIX) - 1) == 0)
    {
      ns = SVN_DAV_PROP_NS_SVN;
      name += sizeof(SVN_PROP_PREFIX) - 1;
    }
  else
    {
      ns = SVN_DAV_PROP_NS_CUSTOM;
    }

  if (value)
    {
      value = svn_string_dup(value, dir->pool);
      svn_ra_serf__set_prop(dir->changed_props, proppatch_target,
                            ns, name, value, dir->pool);
    }
  else
    {
      value = svn_string_create_empty(dir->pool);
      svn_ra_serf__set_prop(dir->removed_props, proppatch_target,
                            ns, name, value, dir->pool);
    }

  return SVN_NO_ERROR;
}

static svn_error_t *
close_directory(void *dir_baton,
                apr_pool_t *pool)
{
  dir_context_t *dir = dir_baton;

  /* Huh?  We're going to be called before the texts are sent.  Ugh.
   * Therefore, just wave politely at our caller.
   */

  /* PROPPATCH our prop change and pass it along.  */
  if (apr_hash_count(dir->changed_props) ||
      apr_hash_count(dir->removed_props))
    {
      proppatch_context_t *proppatch_ctx;

      proppatch_ctx = apr_pcalloc(pool, sizeof(*proppatch_ctx));
      proppatch_ctx->pool = pool;
      proppatch_ctx->commit = dir->commit;
      proppatch_ctx->relpath = dir->relpath;
      proppatch_ctx->changed_props = dir->changed_props;
      proppatch_ctx->removed_props = dir->removed_props;
      proppatch_ctx->base_revision = dir->base_revision;

      if (USING_HTTPV2_COMMIT_SUPPORT(dir->commit))
        {
          proppatch_ctx->path = dir->url;
        }
      else
        {
          proppatch_ctx->path = dir->working_url;
        }

      SVN_ERR(proppatch_resource(proppatch_ctx, dir->commit, dir->pool));
    }

  return SVN_NO_ERROR;
}

static svn_error_t *
add_file(const char *path,
         void *parent_baton,
         const char *copy_path,
         svn_revnum_t copy_revision,
         apr_pool_t *file_pool,
         void **file_baton)
{
  dir_context_t *dir = parent_baton;
  file_context_t *new_file;
  const char *deleted_parent = path;

  new_file = apr_pcalloc(file_pool, sizeof(*new_file));
  new_file->pool = file_pool;

  dir->ref_count++;

  new_file->parent_dir = dir;
  new_file->commit = dir->commit;
  new_file->relpath = apr_pstrdup(new_file->pool, path);
  new_file->name = svn_relpath_basename(new_file->relpath, NULL);
  new_file->added = TRUE;
  new_file->base_revision = SVN_INVALID_REVNUM;
  new_file->copy_path = copy_path;
  new_file->copy_revision = copy_revision;
  new_file->changed_props = apr_hash_make(new_file->pool);
  new_file->removed_props = apr_hash_make(new_file->pool);

  /* Ensure that the file doesn't exist by doing a HEAD on the
     resource.  If we're using HTTP v2, we'll just look into the
     transaction root tree for this thing.  */
  if (USING_HTTPV2_COMMIT_SUPPORT(dir->commit))
    {
      new_file->url = svn_path_url_add_component2(dir->commit->txn_root_url,
                                                  path, new_file->pool);
    }
  else
    {
      /* Ensure our parent directory has been checked out */
      SVN_ERR(checkout_dir(dir, new_file->pool /* scratch_pool */));

      new_file->url =
        svn_path_url_add_component2(dir->working_url,
                                    new_file->name, new_file->pool);
    }

  while (deleted_parent && deleted_parent[0] != '\0')
    {
      if (svn_hash_gets(dir->commit->deleted_entries, deleted_parent))
        {
          break;
        }
      deleted_parent = svn_relpath_dirname(deleted_parent, file_pool);
    }

  if (! ((dir->added && !dir->copy_path) ||
         (deleted_parent && deleted_parent[0] != '\0')))
    {
      svn_ra_serf__handler_t *handler;

      handler = apr_pcalloc(new_file->pool, sizeof(*handler));
      handler->handler_pool = new_file->pool;
      handler->session = new_file->commit->session;
      handler->conn = new_file->commit->conn;
      handler->method = "HEAD";
      handler->path = svn_path_url_add_component2(
        dir->commit->session->session_url.path,
        path, new_file->pool);
      handler->response_handler = svn_ra_serf__expect_empty_body;
      handler->response_baton = handler;

      SVN_ERR(svn_ra_serf__context_run_one(handler, new_file->pool));

      if (handler->sline.code != 404)
        {
          if (handler->sline.code != 200)
            {
              svn_error_t *err;

              err = svn_ra_serf__error_on_status(handler->sline,
                                                 handler->path,
                                                 handler->location);

              SVN_ERR(err);
            }

          return svn_error_createf(SVN_ERR_FS_ALREADY_EXISTS, NULL,
                                   _("File '%s' already exists"), path);
        }
    }

  *file_baton = new_file;

  return SVN_NO_ERROR;
}

static svn_error_t *
open_file(const char *path,
          void *parent_baton,
          svn_revnum_t base_revision,
          apr_pool_t *file_pool,
          void **file_baton)
{
  dir_context_t *parent = parent_baton;
  file_context_t *new_file;

  new_file = apr_pcalloc(file_pool, sizeof(*new_file));
  new_file->pool = file_pool;

  parent->ref_count++;

  new_file->parent_dir = parent;
  new_file->commit = parent->commit;
  new_file->relpath = apr_pstrdup(new_file->pool, path);
  new_file->name = svn_relpath_basename(new_file->relpath, NULL);
  new_file->added = FALSE;
  new_file->base_revision = base_revision;
  new_file->changed_props = apr_hash_make(new_file->pool);
  new_file->removed_props = apr_hash_make(new_file->pool);

  if (USING_HTTPV2_COMMIT_SUPPORT(parent->commit))
    {
      new_file->url = svn_path_url_add_component2(parent->commit->txn_root_url,
                                                  path, new_file->pool);
    }
  else
    {
      /* CHECKOUT the file into our activity. */
      SVN_ERR(checkout_file(new_file, new_file->pool /* scratch_pool */));

      new_file->url = new_file->working_url;
    }

  *file_baton = new_file;

  return SVN_NO_ERROR;
}

static svn_error_t *
apply_textdelta(void *file_baton,
                const char *base_checksum,
                apr_pool_t *pool,
                svn_txdelta_window_handler_t *handler,
                void **handler_baton)
{
  file_context_t *ctx = file_baton;

  /* Store the stream in a temporary file; we'll give it to serf when we
   * close this file.
   *
   * TODO: There should be a way we can stream the request body instead of
   * writing to a temporary file (ugh). A special svn stream serf bucket
   * that returns EAGAIN until we receive the done call?  But, when
   * would we run through the serf context?  Grr.
   *
   * ctx->pool is the same for all files in the commit that send a
   * textdelta so this file is explicitly closed in close_file to
   * avoid too many simultaneously open files.
   */

  SVN_ERR(svn_io_open_unique_file3(&ctx->svndiff, NULL, NULL,
                                   svn_io_file_del_on_pool_cleanup,
                                   ctx->pool, pool));

  ctx->stream = svn_stream_create(ctx, pool);
  svn_stream_set_write(ctx->stream, svndiff_stream_write);

  svn_txdelta_to_svndiff3(handler, handler_baton, ctx->stream, 0,
                          SVN_DELTA_COMPRESSION_LEVEL_DEFAULT, pool);

  if (base_checksum)
    ctx->base_checksum = apr_pstrdup(ctx->pool, base_checksum);

  return SVN_NO_ERROR;
}

static svn_error_t *
change_file_prop(void *file_baton,
                 const char *name,
                 const svn_string_t *value,
                 apr_pool_t *pool)
{
  file_context_t *file = file_baton;
  const char *ns;

  name = apr_pstrdup(file->pool, name);

  if (strncmp(name, SVN_PROP_PREFIX, sizeof(SVN_PROP_PREFIX) - 1) == 0)
    {
      ns = SVN_DAV_PROP_NS_SVN;
      name += sizeof(SVN_PROP_PREFIX) - 1;
    }
  else
    {
      ns = SVN_DAV_PROP_NS_CUSTOM;
    }

  if (value)
    {
      value = svn_string_dup(value, file->pool);
      svn_ra_serf__set_prop(file->changed_props, file->url,
                            ns, name, value, file->pool);
    }
  else
    {
      value = svn_string_create_empty(file->pool);

      svn_ra_serf__set_prop(file->removed_props, file->url,
                            ns, name, value, file->pool);
    }

  return SVN_NO_ERROR;
}

static svn_error_t *
close_file(void *file_baton,
           const char *text_checksum,
           apr_pool_t *scratch_pool)
{
  file_context_t *ctx = file_baton;
  svn_boolean_t put_empty_file = FALSE;
  apr_status_t status;

  ctx->result_checksum = text_checksum;

  if (ctx->copy_path)
    {
      svn_ra_serf__handler_t *handler;
      apr_uri_t uri;
      const char *req_url;

      status = apr_uri_parse(scratch_pool, ctx->copy_path, &uri);
      if (status)
        {
          return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
                                   _("Unable to parse URL '%s'"),
                                   ctx->copy_path);
        }

      /* ### conn==NULL for session->conns[0]. same as commit->conn.  */
      SVN_ERR(svn_ra_serf__get_stable_url(&req_url, NULL /* latest_revnum */,
                                          ctx->commit->session,
                                          NULL /* conn */,
                                          uri.path, ctx->copy_revision,
                                          scratch_pool, scratch_pool));

      handler = apr_pcalloc(scratch_pool, sizeof(*handler));
      handler->handler_pool = scratch_pool;
      handler->method = "COPY";
      handler->path = req_url;
      handler->conn = ctx->commit->conn;
      handler->session = ctx->commit->session;

      handler->response_handler = svn_ra_serf__expect_empty_body;
      handler->response_baton = handler;

      handler->header_delegate = setup_copy_file_headers;
      handler->header_delegate_baton = ctx;

      SVN_ERR(svn_ra_serf__context_run_one(handler, scratch_pool));

      if (handler->sline.code != 201 && handler->sline.code != 204)
        {
          return svn_error_trace(return_response_err(handler));
        }
    }

  /* If we got no stream of changes, but this is an added-without-history
   * file, make a note that we'll be PUTting a zero-byte file to the server.
   */
  if ((!ctx->stream) && ctx->added && (!ctx->copy_path))
    put_empty_file = TRUE;

  /* If we had a stream of changes, push them to the server... */
  if (ctx->stream || put_empty_file)
    {
      svn_ra_serf__handler_t *handler;

      handler = apr_pcalloc(scratch_pool, sizeof(*handler));
      handler->handler_pool = scratch_pool;
      handler->method = "PUT";
      handler->path = ctx->url;
      handler->conn = ctx->commit->conn;
      handler->session = ctx->commit->session;

      handler->response_handler = svn_ra_serf__expect_empty_body;
      handler->response_baton = handler;

      if (put_empty_file)
        {
          handler->body_delegate = create_empty_put_body;
          handler->body_delegate_baton = ctx;
          handler->body_type = "text/plain";
        }
      else
        {
          handler->body_delegate = create_put_body;
          handler->body_delegate_baton = ctx;
          handler->body_type = SVN_SVNDIFF_MIME_TYPE;
        }

      handler->header_delegate = setup_put_headers;
      handler->header_delegate_baton = ctx;

      SVN_ERR(svn_ra_serf__context_run_one(handler, scratch_pool));

      if (handler->sline.code != 204 && handler->sline.code != 201)
        {
          return svn_error_trace(return_response_err(handler));
        }
    }

  if (ctx->svndiff)
    SVN_ERR(svn_io_file_close(ctx->svndiff, scratch_pool));

  /* If we had any prop changes, push them via PROPPATCH. */
  if (apr_hash_count(ctx->changed_props) ||
      apr_hash_count(ctx->removed_props))
    {
      proppatch_context_t *proppatch;

      proppatch = apr_pcalloc(ctx->pool, sizeof(*proppatch));
      proppatch->pool = ctx->pool;
      proppatch->relpath = ctx->relpath;
      proppatch->path = ctx->url;
      proppatch->commit = ctx->commit;
      proppatch->changed_props = ctx->changed_props;
      proppatch->removed_props = ctx->removed_props;
      proppatch->base_revision = ctx->base_revision;

      SVN_ERR(proppatch_resource(proppatch, ctx->commit, ctx->pool));
    }

  return SVN_NO_ERROR;
}

static svn_error_t *
close_edit(void *edit_baton,
           apr_pool_t *pool)
{
  commit_context_t *ctx = edit_baton;
  const char *merge_target =
    ctx->activity_url ? ctx->activity_url : ctx->txn_url;
  const svn_commit_info_t *commit_info;
  int response_code;

  /* MERGE our activity */
  SVN_ERR(svn_ra_serf__run_merge(&commit_info, &response_code,
                                 ctx->session,
                                 ctx->session->conns[0],
                                 merge_target,
                                 ctx->lock_tokens,
                                 ctx->keep_locks,
                                 pool, pool));

  if (response_code != 200)
    {
      return svn_error_createf(SVN_ERR_RA_DAV_REQUEST_FAILED, NULL,
                               _("MERGE request failed: returned %d "
                                 "(during commit)"),
                               response_code);
    }

  /* Inform the WC that we did a commit.  */
  if (ctx->callback)
    SVN_ERR(ctx->callback(commit_info, ctx->callback_baton, pool));

  /* If we're using activities, DELETE our completed activity.  */
  if (ctx->activity_url)
    {
      svn_ra_serf__handler_t *handler;

      handler = apr_pcalloc(pool, sizeof(*handler));
      handler->handler_pool = pool;
      handler->method = "DELETE";
      handler->path = ctx->activity_url;
      handler->conn = ctx->conn;
      handler->session = ctx->session;

      handler->response_handler = svn_ra_serf__expect_empty_body;
      handler->response_baton = handler;

      SVN_ERR(svn_ra_serf__context_run_one(handler, pool));

      SVN_ERR_ASSERT(handler->sline.code == 204);
    }

  return SVN_NO_ERROR;
}

static svn_error_t *
abort_edit(void *edit_baton,
           apr_pool_t *pool)
{
  commit_context_t *ctx = edit_baton;
  svn_ra_serf__handler_t *handler;

  /* If an activity or transaction wasn't even created, don't bother
     trying to delete it. */
  if (! (ctx->activity_url || ctx->txn_url))
    return SVN_NO_ERROR;

  /* An error occurred on conns[0]. serf 0.4.0 remembers that the connection
     had a problem. We need to reset it, in order to use it again.  */
  serf_connection_reset(ctx->session->conns[0]->conn);

  /* DELETE our aborted activity */
  handler = apr_pcalloc(pool, sizeof(*handler));
  handler->handler_pool = pool;
  handler->method = "DELETE";
  handler->conn = ctx->session->conns[0];
  handler->session = ctx->session;

  handler->response_handler = svn_ra_serf__expect_empty_body;
  handler->response_baton = handler;

  if (USING_HTTPV2_COMMIT_SUPPORT(ctx)) /* HTTP v2 */
    handler->path = ctx->txn_url;
  else
    handler->path = ctx->activity_url;

  SVN_ERR(svn_ra_serf__context_run_one(handler, pool));

  /* 204 if deleted,
     403 if DELETE was forbidden (indicates MKACTIVITY was forbidden too),
     404 if the activity wasn't found. */
  if (handler->sline.code != 204
      && handler->sline.code != 403
      && handler->sline.code != 404
      )
    {
      return svn_error_createf(SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
                               _("DELETE returned unexpected status: %d"),
                               handler->sline.code);
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_serf__get_commit_editor(svn_ra_session_t *ra_session,
                               const svn_delta_editor_t **ret_editor,
                               void **edit_baton,
                               apr_hash_t *revprop_table,
                               svn_commit_callback2_t callback,
                               void *callback_baton,
                               apr_hash_t *lock_tokens,
                               svn_boolean_t keep_locks,
                               apr_pool_t *pool)
{
  svn_ra_serf__session_t *session = ra_session->priv;
  svn_delta_editor_t *editor;
  commit_context_t *ctx;
  const char *repos_root;
  const char *base_relpath;
  svn_boolean_t supports_ephemeral_props;

  ctx = apr_pcalloc(pool, sizeof(*ctx));

  ctx->pool = pool;

  ctx->session = session;
  ctx->conn = session->conns[0];

  ctx->revprop_table = svn_prop_hash_dup(revprop_table, pool);

  /* If the server supports ephemeral properties, add some carrying
     interesting version information. */
  SVN_ERR(svn_ra_serf__has_capability(ra_session, &supports_ephemeral_props,
                                      SVN_RA_CAPABILITY_EPHEMERAL_TXNPROPS,
                                      pool));
  if (supports_ephemeral_props)
    {
      svn_hash_sets(ctx->revprop_table,
                    apr_pstrdup(pool, SVN_PROP_TXN_CLIENT_COMPAT_VERSION),
                    svn_string_create(SVN_VER_NUMBER, pool));
      svn_hash_sets(ctx->revprop_table,
                    apr_pstrdup(pool, SVN_PROP_TXN_USER_AGENT),
                    svn_string_create(session->useragent, pool));
    }

  ctx->callback = callback;
  ctx->callback_baton = callback_baton;

  ctx->lock_tokens = lock_tokens;
  ctx->keep_locks = keep_locks;

  ctx->deleted_entries = apr_hash_make(ctx->pool);

  editor = svn_delta_default_editor(pool);
  editor->open_root = open_root;
  editor->delete_entry = delete_entry;
  editor->add_directory = add_directory;
  editor->open_directory = open_directory;
  editor->change_dir_prop = change_dir_prop;
  editor->close_directory = close_directory;
  editor->add_file = add_file;
  editor->open_file = open_file;
  editor->apply_textdelta = apply_textdelta;
  editor->change_file_prop = change_file_prop;
  editor->close_file = close_file;
  editor->close_edit = close_edit;
  editor->abort_edit = abort_edit;

  *ret_editor = editor;
  *edit_baton = ctx;

  SVN_ERR(svn_ra_serf__get_repos_root(ra_session, &repos_root, pool));
  base_relpath = svn_uri_skip_ancestor(repos_root, session->session_url_str,
                                       pool);

  SVN_ERR(svn_editor__insert_shims(ret_editor, edit_baton, *ret_editor,
                                   *edit_baton, repos_root, base_relpath,
                                   session->shim_callbacks, pool, pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_ra_serf__change_rev_prop(svn_ra_session_t *ra_session,
                             svn_revnum_t rev,
                             const char *name,
                             const svn_string_t *const *old_value_p,
                             const svn_string_t *value,
                             apr_pool_t *pool)
{
  svn_ra_serf__session_t *session = ra_session->priv;
  proppatch_context_t *proppatch_ctx;
  commit_context_t *commit;
  const char *proppatch_target;
  const char *ns;
  svn_error_t *err;

  if (old_value_p)
    {
      svn_boolean_t capable;
      SVN_ERR(svn_ra_serf__has_capability(ra_session, &capable,
                                          SVN_RA_CAPABILITY_ATOMIC_REVPROPS,
                                          pool));

      /* How did you get past the same check in svn_ra_change_rev_prop2()? */
      SVN_ERR_ASSERT(capable);
    }

  commit = apr_pcalloc(pool, sizeof(*commit));

  commit->pool = pool;

  commit->session = session;
  commit->conn = session->conns[0];

  if (SVN_RA_SERF__HAVE_HTTPV2_SUPPORT(session))
    {
      proppatch_target = apr_psprintf(pool, "%s/%ld", session->rev_stub, rev);
    }
  else
    {
      const char *vcc_url;

      SVN_ERR(svn_ra_serf__discover_vcc(&vcc_url, commit->session,
                                        commit->conn, pool));

      SVN_ERR(svn_ra_serf__fetch_dav_prop(&proppatch_target,
                                          commit->conn, vcc_url, rev,
                                          "href",
                                          pool, pool));
    }

  if (strncmp(name, SVN_PROP_PREFIX, sizeof(SVN_PROP_PREFIX) - 1) == 0)
    {
      ns = SVN_DAV_PROP_NS_SVN;
      name += sizeof(SVN_PROP_PREFIX) - 1;
    }
  else
    {
      ns = SVN_DAV_PROP_NS_CUSTOM;
    }

  /* PROPPATCH our log message and pass it along.  */
  proppatch_ctx = apr_pcalloc(pool, sizeof(*proppatch_ctx));
  proppatch_ctx->pool = pool;
  proppatch_ctx->commit = commit;
  proppatch_ctx->path = proppatch_target;
  proppatch_ctx->changed_props = apr_hash_make(proppatch_ctx->pool);
  proppatch_ctx->removed_props = apr_hash_make(proppatch_ctx->pool);
  if (old_value_p)
    {
      proppatch_ctx->previous_changed_props = apr_hash_make(proppatch_ctx->pool);
      proppatch_ctx->previous_removed_props = apr_hash_make(proppatch_ctx->pool);
    }
  proppatch_ctx->base_revision = SVN_INVALID_REVNUM;

  if (old_value_p && *old_value_p)
    {
      svn_ra_serf__set_prop(proppatch_ctx->previous_changed_props,
                            proppatch_ctx->path,
                            ns, name, *old_value_p, proppatch_ctx->pool);
    }
  else if (old_value_p)
    {
      svn_string_t *dummy_value = svn_string_create_empty(proppatch_ctx->pool);

      svn_ra_serf__set_prop(proppatch_ctx->previous_removed_props,
                            proppatch_ctx->path,
                            ns, name, dummy_value, proppatch_ctx->pool);
    }

  if (value)
    {
      svn_ra_serf__set_prop(proppatch_ctx->changed_props, proppatch_ctx->path,
                            ns, name, value, proppatch_ctx->pool);
    }
  else
    {
      value = svn_string_create_empty(proppatch_ctx->pool);

      svn_ra_serf__set_prop(proppatch_ctx->removed_props, proppatch_ctx->path,
                            ns, name, value, proppatch_ctx->pool);
    }

  err = proppatch_resource(proppatch_ctx, commit, proppatch_ctx->pool);
  if (err)
    return
      svn_error_create
      (SVN_ERR_RA_DAV_REQUEST_FAILED, err,
       _("DAV request failed; it's possible that the repository's "
         "pre-revprop-change hook either failed or is non-existent"));

  return SVN_NO_ERROR;
}
