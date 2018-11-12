/*
 * inherited_props.c : ra_serf implementation of svn_ra_get_inherited_props
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


#include <apr_tables.h>
#include <apr_xml.h>

#include "svn_hash.h"
#include "svn_path.h"
#include "svn_ra.h"
#include "svn_string.h"
#include "svn_xml.h"
#include "svn_props.h"
#include "svn_base64.h"

#include "private/svn_dav_protocol.h"
#include "../libsvn_ra/ra_loader.h"
#include "svn_private_config.h"
#include "ra_serf.h"


/* The current state of our XML parsing. */
typedef enum iprops_state_e {
  INITIAL = 0,
  IPROPS_REPORT,
  IPROPS_ITEM,
  IPROPS_PATH,
  IPROPS_PROPNAME,
  IPROPS_PROPVAL
} iprops_state_e;

/* Struct for accumulating inherited props. */
typedef struct iprops_context_t {
  /* The depth-first ordered array of svn_prop_inherited_item_t *
     structures we are building. */
  apr_array_header_t *iprops;

  /* Pool in which to allocate elements of IPROPS. */
  apr_pool_t *pool;

  /* The repository's root URL. */
  const char *repos_root_url;

  /* Current property name */
  svn_stringbuf_t *curr_propname;

  /* Current element in IPROPS. */
  svn_prop_inherited_item_t *curr_iprop;

  /* Path we are finding inherited properties for.  This is relative to
     the RA session passed to svn_ra_serf__get_inherited_props. */
  const char *path;
  /* The revision of PATH*/
  svn_revnum_t revision;
} iprops_context_t;

#define S_ SVN_XML_NAMESPACE
static const svn_ra_serf__xml_transition_t iprops_table[] = {
  { INITIAL, S_, SVN_DAV__INHERITED_PROPS_REPORT, IPROPS_REPORT,
    FALSE, { NULL }, FALSE },

  { IPROPS_REPORT, S_, SVN_DAV__IPROP_ITEM, IPROPS_ITEM,
    FALSE, { NULL }, TRUE },

  { IPROPS_ITEM, S_, SVN_DAV__IPROP_PATH, IPROPS_PATH,
    TRUE, { NULL }, TRUE },

  { IPROPS_ITEM, S_, SVN_DAV__IPROP_PROPNAME, IPROPS_PROPNAME,
    TRUE, { NULL }, TRUE },

  { IPROPS_ITEM, S_, SVN_DAV__IPROP_PROPVAL, IPROPS_PROPVAL,
    TRUE, { "?V:encoding", NULL }, TRUE },

  { 0 }
};

/* Conforms to svn_ra_serf__xml_opened_t */
static svn_error_t *
iprops_opened(svn_ra_serf__xml_estate_t *xes,
              void *baton,
              int entered_state,
              const svn_ra_serf__dav_props_t *tag,
              apr_pool_t *scratch_pool)
{
  iprops_context_t *iprops_ctx = baton;

  if (entered_state == IPROPS_ITEM)
    {
      svn_stringbuf_setempty(iprops_ctx->curr_propname);

      iprops_ctx->curr_iprop = apr_pcalloc(iprops_ctx->pool,
                                           sizeof(*iprops_ctx->curr_iprop));

      iprops_ctx->curr_iprop->prop_hash = apr_hash_make(iprops_ctx->pool);
    }
  return SVN_NO_ERROR;
}

/* Conforms to svn_ra_serf__xml_closed_t  */
static svn_error_t *
iprops_closed(svn_ra_serf__xml_estate_t *xes,
              void *baton,
              int leaving_state,
              const svn_string_t *cdata,
              apr_hash_t *attrs,
              apr_pool_t *scratch_pool)
{
  iprops_context_t *iprops_ctx = baton;

  if (leaving_state == IPROPS_ITEM)
    {
      APR_ARRAY_PUSH(iprops_ctx->iprops, svn_prop_inherited_item_t *) =
        iprops_ctx->curr_iprop;

      iprops_ctx->curr_iprop = NULL;
    }
  else if (leaving_state == IPROPS_PATH)
    {
      /* Every <iprop-item> has a single <iprop-path> */
      if (iprops_ctx->curr_iprop->path_or_url)
        return svn_error_create(SVN_ERR_XML_MALFORMED, NULL, NULL);

      iprops_ctx->curr_iprop->path_or_url =
        svn_path_url_add_component2(iprops_ctx->repos_root_url,
                                    cdata->data,
                                    iprops_ctx->pool);
    }
  else if (leaving_state == IPROPS_PROPNAME)
    {
      if (iprops_ctx->curr_propname->len)
        return svn_error_create(SVN_ERR_XML_MALFORMED, NULL, NULL);

      /* Store propname for value */
      svn_stringbuf_set(iprops_ctx->curr_propname, cdata->data);
    }
  else if (leaving_state == IPROPS_PROPVAL)
    {
      const char *encoding;
      const svn_string_t *val_str;

      if (! iprops_ctx->curr_propname->len)
        return svn_error_create(SVN_ERR_XML_MALFORMED, NULL, NULL);

      encoding = svn_hash_gets(attrs, "V:encoding");

      if (encoding)
        {
          if (strcmp(encoding, "base64") != 0)
            return svn_error_createf(SVN_ERR_XML_MALFORMED,
                                     NULL,
                                     _("Got unrecognized encoding '%s'"),
                                     encoding);

          /* Decode into the right pool.  */
          val_str = svn_base64_decode_string(cdata, iprops_ctx->pool);
        }
      else
        {
          /* Copy into the right pool.  */
          val_str = svn_string_dup(cdata, iprops_ctx->pool);
        }

      svn_hash_sets(iprops_ctx->curr_iprop->prop_hash,
                    apr_pstrdup(iprops_ctx->pool,
                                iprops_ctx->curr_propname->data),
                    val_str);
      /* Clear current propname. */
      svn_stringbuf_setempty(iprops_ctx->curr_propname);
    }
  else
    SVN_ERR_MALFUNCTION(); /* Invalid transition table */

  return SVN_NO_ERROR;
}

static svn_error_t *
create_iprops_body(serf_bucket_t **bkt,
                   void *baton,
                   serf_bucket_alloc_t *alloc,
                   apr_pool_t *pool)
{
  iprops_context_t *iprops_ctx = baton;
  serf_bucket_t *body_bkt;

  body_bkt = serf_bucket_aggregate_create(alloc);

  svn_ra_serf__add_open_tag_buckets(body_bkt, alloc,
                                    "S:" SVN_DAV__INHERITED_PROPS_REPORT,
                                    "xmlns:S", SVN_XML_NAMESPACE,
                                    NULL);
  svn_ra_serf__add_tag_buckets(body_bkt,
                               "S:" SVN_DAV__REVISION,
                               apr_ltoa(pool, iprops_ctx->revision),
                               alloc);
  svn_ra_serf__add_tag_buckets(body_bkt, "S:" SVN_DAV__PATH,
                               iprops_ctx->path, alloc);
  svn_ra_serf__add_close_tag_buckets(body_bkt, alloc,
                                     "S:" SVN_DAV__INHERITED_PROPS_REPORT);
  *bkt = body_bkt;
  return SVN_NO_ERROR;
}

/* Request a inherited-props-report from the URL attached to RA_SESSION,
   and fill the IPROPS array hash with the results.  */
svn_error_t *
svn_ra_serf__get_inherited_props(svn_ra_session_t *ra_session,
                                 apr_array_header_t **iprops,
                                 const char *path,
                                 svn_revnum_t revision,
                                 apr_pool_t *result_pool,
                                 apr_pool_t *scratch_pool)
{
  svn_error_t *err;
  iprops_context_t *iprops_ctx;
  svn_ra_serf__session_t *session = ra_session->priv;
  svn_ra_serf__handler_t *handler;
  svn_ra_serf__xml_context_t *xmlctx;
  const char *req_url;

  SVN_ERR(svn_ra_serf__get_stable_url(&req_url,
                                      NULL /* latest_revnum */,
                                      session,
                                      NULL /* conn */,
                                      NULL /* url */,
                                      revision,
                                      result_pool, scratch_pool));

  SVN_ERR_ASSERT(session->repos_root_str);

  iprops_ctx = apr_pcalloc(scratch_pool, sizeof(*iprops_ctx));
  iprops_ctx->repos_root_url = session->repos_root_str;
  iprops_ctx->pool = result_pool;
  iprops_ctx->curr_propname = svn_stringbuf_create_empty(scratch_pool);
  iprops_ctx->curr_iprop = NULL;
  iprops_ctx->iprops = apr_array_make(result_pool, 1,
                                       sizeof(svn_prop_inherited_item_t *));
  iprops_ctx->path = path;
  iprops_ctx->revision = revision;

  xmlctx = svn_ra_serf__xml_context_create(iprops_table,
                                           iprops_opened, iprops_closed, NULL,
                                           iprops_ctx,
                                           scratch_pool);
  handler = svn_ra_serf__create_expat_handler(xmlctx, scratch_pool);

  handler->method = "REPORT";
  handler->path = req_url;
  handler->conn = session->conns[0];
  handler->session = session;
  handler->body_delegate = create_iprops_body;
  handler->body_delegate_baton = iprops_ctx;
  handler->body_type = "text/xml";
  handler->handler_pool = scratch_pool;

  err = svn_ra_serf__context_run_one(handler, scratch_pool);
  SVN_ERR(svn_error_compose_create(
                    svn_ra_serf__error_on_status(handler->sline,
                                                 handler->path,
                                                 handler->location),
                    err));

  *iprops = iprops_ctx->iprops;

  return SVN_NO_ERROR;
}
