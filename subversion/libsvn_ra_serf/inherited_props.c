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
  NONE = 0,
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

  /* Current CDATA values*/
  svn_stringbuf_t *curr_path;
  svn_stringbuf_t *curr_propname;
  svn_stringbuf_t *curr_propval;
  const char *curr_prop_val_encoding;

  /* Current element in IPROPS. */
  svn_prop_inherited_item_t *curr_iprop;

  /* Serf context completion flag for svn_ra_serf__context_run_wait() */
  svn_boolean_t done;

  /* Path we are finding inherited properties for.  This is relative to
     the RA session passed to svn_ra_serf__get_inherited_props. */
  const char *path;
  /* The revision of PATH*/
  svn_revnum_t revision;
} iprops_context_t;

static svn_error_t *
start_element(svn_ra_serf__xml_parser_t *parser,
              svn_ra_serf__dav_props_t name,
              const char **attrs,
              apr_pool_t *scratch_pool)
{
  iprops_context_t *iprops_ctx = parser->user_data;
  iprops_state_e state;

  state = parser->state->current_state;
  if (state == NONE
      && strcmp(name.name, SVN_DAV__INHERITED_PROPS_REPORT) == 0)
    {
      svn_ra_serf__xml_push_state(parser, IPROPS_REPORT);
    }
  else if (state == IPROPS_REPORT &&
           strcmp(name.name, SVN_DAV__IPROP_ITEM) == 0)
    {
      svn_stringbuf_setempty(iprops_ctx->curr_path);
      svn_stringbuf_setempty(iprops_ctx->curr_propname);
      svn_stringbuf_setempty(iprops_ctx->curr_propval);
      iprops_ctx->curr_prop_val_encoding = NULL;
      iprops_ctx->curr_iprop = NULL;
      svn_ra_serf__xml_push_state(parser, IPROPS_ITEM);
    }
  else if (state == IPROPS_ITEM &&
           strcmp(name.name, SVN_DAV__IPROP_PROPVAL) == 0)
    {
      const char *prop_val_encoding = svn_xml_get_attr_value("encoding",
                                                             attrs);
      iprops_ctx->curr_prop_val_encoding = apr_pstrdup(iprops_ctx->pool,
                                                       prop_val_encoding);
      svn_ra_serf__xml_push_state(parser, IPROPS_PROPVAL);
    }
  else if (state == IPROPS_ITEM &&
           strcmp(name.name, SVN_DAV__IPROP_PATH) == 0)
    {
      svn_ra_serf__xml_push_state(parser, IPROPS_PATH);
    }
  else if (state == IPROPS_ITEM &&
           strcmp(name.name, SVN_DAV__IPROP_PROPNAME) == 0)
    {
      svn_ra_serf__xml_push_state(parser, IPROPS_PROPNAME);
    }
  else if (state == IPROPS_ITEM &&
           strcmp(name.name, SVN_DAV__IPROP_PROPVAL) == 0)
    {
      svn_ra_serf__xml_push_state(parser, IPROPS_PROPVAL);
    }

  return SVN_NO_ERROR;
}

static svn_error_t *
end_element(svn_ra_serf__xml_parser_t *parser,
            svn_ra_serf__dav_props_t name,
            apr_pool_t *scratch_pool)
{
  iprops_context_t *iprops_ctx = parser->user_data;
  iprops_state_e state;

  state = parser->state->current_state;

    if (state == IPROPS_REPORT &&
      strcmp(name.name, SVN_DAV__INHERITED_PROPS_REPORT) == 0)
    {
      svn_ra_serf__xml_pop_state(parser);
    }
  else if (state == IPROPS_PATH
           && strcmp(name.name, SVN_DAV__IPROP_PATH) == 0)
    {
      iprops_ctx->curr_iprop = apr_palloc(
        iprops_ctx->pool, sizeof(svn_prop_inherited_item_t));

      iprops_ctx->curr_iprop->path_or_url =
        svn_path_url_add_component2(iprops_ctx->repos_root_url,
                                    iprops_ctx->curr_path->data,
                                    iprops_ctx->pool);
      iprops_ctx->curr_iprop->prop_hash = apr_hash_make(iprops_ctx->pool);
      svn_ra_serf__xml_pop_state(parser);
    }
  else if (state == IPROPS_PROPVAL
           && strcmp(name.name, SVN_DAV__IPROP_PROPVAL) == 0)
    {
      const svn_string_t *prop_val;

      if (iprops_ctx->curr_prop_val_encoding)
        {
          svn_string_t encoded_prop_val;

          if (strcmp(iprops_ctx->curr_prop_val_encoding, "base64") != 0)
            return svn_error_create(SVN_ERR_XML_MALFORMED, NULL, NULL);

          encoded_prop_val.data = iprops_ctx->curr_propval->data;
          encoded_prop_val.len = iprops_ctx->curr_propval->len;
          prop_val = svn_base64_decode_string(&encoded_prop_val,
                                              iprops_ctx->pool);
        }
      else
        {
          prop_val = svn_string_create_from_buf(iprops_ctx->curr_propval,
                                                iprops_ctx->pool);
        }

      svn_hash_sets(iprops_ctx->curr_iprop->prop_hash,
                    apr_pstrdup(iprops_ctx->pool,
                                iprops_ctx->curr_propname->data),
                    prop_val);
      /* Clear current propname and propval in the event there are
         multiple properties on the current path. */
      svn_stringbuf_setempty(iprops_ctx->curr_propname);
      svn_stringbuf_setempty(iprops_ctx->curr_propval);
      svn_ra_serf__xml_pop_state(parser);
    }
  else if (state == IPROPS_PROPNAME
           && strcmp(name.name, SVN_DAV__IPROP_PROPNAME) == 0)
    {
      svn_ra_serf__xml_pop_state(parser);
    }
  else if (state == IPROPS_ITEM
           && strcmp(name.name, SVN_DAV__IPROP_ITEM) == 0)
    {
      APR_ARRAY_PUSH(iprops_ctx->iprops, svn_prop_inherited_item_t *) =
        iprops_ctx->curr_iprop;
      svn_ra_serf__xml_pop_state(parser);
    }
  return SVN_NO_ERROR;
}


static svn_error_t *
cdata_handler(svn_ra_serf__xml_parser_t *parser,
              const char *data,
              apr_size_t len,
              apr_pool_t *scratch_pool)
{
  iprops_context_t *iprops_ctx = parser->user_data;
  iprops_state_e state = parser->state->current_state;

  switch (state)
    {
    case IPROPS_PATH:
      svn_stringbuf_appendbytes(iprops_ctx->curr_path, data, len);
      break;

    case IPROPS_PROPNAME:
      svn_stringbuf_appendbytes(iprops_ctx->curr_propname, data, len);
      break;

    case IPROPS_PROPVAL:
      svn_stringbuf_appendbytes(iprops_ctx->curr_propval, data, len);
      break;

    default:
      break;
    }

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
  svn_ra_serf__xml_parser_t *parser_ctx;
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
  iprops_ctx->done = FALSE;
  iprops_ctx->repos_root_url = session->repos_root_str;
  iprops_ctx->pool = result_pool;
  iprops_ctx->curr_path = svn_stringbuf_create_empty(scratch_pool);
  iprops_ctx->curr_propname = svn_stringbuf_create_empty(scratch_pool);
  iprops_ctx->curr_propval = svn_stringbuf_create_empty(scratch_pool);
  iprops_ctx->curr_iprop = NULL;
  iprops_ctx->iprops = apr_array_make(result_pool, 1,
                                       sizeof(svn_prop_inherited_item_t *));
  iprops_ctx->path = path;
  iprops_ctx->revision = revision;

  handler = apr_pcalloc(scratch_pool, sizeof(*handler));

  handler->method = "REPORT";
  handler->path = req_url;
  handler->conn = session->conns[0];
  handler->session = session;
  handler->body_delegate = create_iprops_body;
  handler->body_delegate_baton = iprops_ctx;
  handler->body_type = "text/xml";
  handler->handler_pool = scratch_pool;

  parser_ctx = apr_pcalloc(scratch_pool, sizeof(*parser_ctx));

  parser_ctx->pool = scratch_pool;
  parser_ctx->user_data = iprops_ctx;
  parser_ctx->start = start_element;
  parser_ctx->end = end_element;
  parser_ctx->cdata = cdata_handler;
  parser_ctx->done = &iprops_ctx->done;

  handler->response_handler = svn_ra_serf__handle_xml_parser;
  handler->response_baton = parser_ctx;

  err = svn_ra_serf__context_run_one(handler, scratch_pool);
  SVN_ERR(svn_error_compose_create(
                    svn_ra_serf__error_on_status(handler->sline,
                                                 handler->path,
                                                 handler->location),
                    err));

  if (iprops_ctx->done)
    *iprops = iprops_ctx->iprops;

  return SVN_NO_ERROR;
}
