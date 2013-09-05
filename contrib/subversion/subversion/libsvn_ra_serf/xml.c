/*
 * xml.c :  standard XML parsing routines for ra_serf
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
#include "../libsvn_ra/ra_loader.h"
#include "svn_config.h"
#include "svn_delta.h"
#include "svn_path.h"

#include "svn_private_config.h"
#include "private/svn_string_private.h"

#include "ra_serf.h"


struct svn_ra_serf__xml_context_t {
  /* Current state information.  */
  svn_ra_serf__xml_estate_t *current;

  /* If WAITING.NAMESPACE != NULL, wait for NAMESPACE:NAME element to be
     closed before looking for transitions from CURRENT->STATE.  */
  svn_ra_serf__dav_props_t waiting;

  /* The transition table.  */
  const svn_ra_serf__xml_transition_t *ttable;

  /* The callback information.  */
  svn_ra_serf__xml_opened_t opened_cb;
  svn_ra_serf__xml_closed_t closed_cb;
  svn_ra_serf__xml_cdata_t cdata_cb;
  void *baton;

  /* Linked list of free states.  */
  svn_ra_serf__xml_estate_t *free_states;

#ifdef SVN_DEBUG
  /* Used to verify we are not re-entering a callback, specifically to
     ensure SCRATCH_POOL is not cleared while an outer callback is
     trying to use it.  */
  svn_boolean_t within_callback;
#define START_CALLBACK(xmlctx) \
  do {                                                    \
    svn_ra_serf__xml_context_t *xmlctx__tmp = (xmlctx);   \
    SVN_ERR_ASSERT(!xmlctx__tmp->within_callback);        \
    xmlctx__tmp->within_callback = TRUE;                  \
  } while (0)
#define END_CALLBACK(xmlctx) ((xmlctx)->within_callback = FALSE)
#else
#define START_CALLBACK(xmlctx)  /* empty */
#define END_CALLBACK(xmlctx)  /* empty */
#endif /* SVN_DEBUG  */

  apr_pool_t *scratch_pool;

};

struct svn_ra_serf__xml_estate_t {
  /* The current state value.  */
  int state;

  /* The xml tag that opened this state. Waiting for the tag to close.  */
  svn_ra_serf__dav_props_t tag;

  /* Should the CLOSED_CB function be called for custom processing when
     this tag is closed?  */
  svn_boolean_t custom_close;

  /* A pool may be constructed for this state.  */
  apr_pool_t *state_pool;

  /* The namespaces extent for this state/element. This will start with
     the parent's NS_LIST, and we will push new namespaces into our
     local list. The parent will be unaffected by our locally-scoped data. */
  svn_ra_serf__ns_t *ns_list;

  /* Any collected attribute values. char * -> svn_string_t *. May be NULL
     if no attributes have been collected.  */
  apr_hash_t *attrs;

  /* Any collected cdata. May be NULL if no cdata is being collected.  */
  svn_stringbuf_t *cdata;

  /* Previous/outer state.  */
  svn_ra_serf__xml_estate_t *prev;

};


static void
define_namespaces(svn_ra_serf__ns_t **ns_list,
                  const char *const *attrs,
                  apr_pool_t *(*get_pool)(void *baton),
                  void *baton)
{
  const char *const *tmp_attrs = attrs;

  for (tmp_attrs = attrs; *tmp_attrs != NULL; tmp_attrs += 2)
    {
      if (strncmp(*tmp_attrs, "xmlns", 5) == 0)
        {
          const svn_ra_serf__ns_t *cur_ns;
          svn_boolean_t found = FALSE;
          const char *prefix;

          /* The empty prefix, or a named-prefix.  */
          if (tmp_attrs[0][5] == ':')
            prefix = &tmp_attrs[0][6];
          else
            prefix = "";

          /* Have we already defined this ns previously? */
          for (cur_ns = *ns_list; cur_ns; cur_ns = cur_ns->next)
            {
              if (strcmp(cur_ns->namespace, prefix) == 0)
                {
                  found = TRUE;
                  break;
                }
            }

          if (!found)
            {
              apr_pool_t *pool;
              svn_ra_serf__ns_t *new_ns;

              if (get_pool)
                pool = get_pool(baton);
              else
                pool = baton;
              new_ns = apr_palloc(pool, sizeof(*new_ns));
              new_ns->namespace = apr_pstrdup(pool, prefix);
              new_ns->url = apr_pstrdup(pool, tmp_attrs[1]);

              /* Push into the front of NS_LIST. Parent states will point
                 to later in the chain, so will be unaffected by
                 shadowing/other namespaces pushed onto NS_LIST.  */
              new_ns->next = *ns_list;
              *ns_list = new_ns;
            }
        }
    }
}


void
svn_ra_serf__define_ns(svn_ra_serf__ns_t **ns_list,
                       const char *const *attrs,
                       apr_pool_t *result_pool)
{
  define_namespaces(ns_list, attrs, NULL /* get_pool */, result_pool);
}


/*
 * Look up NAME in the NS_LIST list for previously declared namespace
 * definitions and return a DAV_PROPS_T-tuple that has values.
 */
void
svn_ra_serf__expand_ns(svn_ra_serf__dav_props_t *returned_prop_name,
                       const svn_ra_serf__ns_t *ns_list,
                       const char *name)
{
  const char *colon;

  colon = strchr(name, ':');
  if (colon)
    {
      const svn_ra_serf__ns_t *ns;

      for (ns = ns_list; ns; ns = ns->next)
        {
          if (strncmp(ns->namespace, name, colon - name) == 0)
            {
              returned_prop_name->namespace = ns->url;
              returned_prop_name->name = colon + 1;
              return;
            }
        }
    }
  else
    {
      const svn_ra_serf__ns_t *ns;

      for (ns = ns_list; ns; ns = ns->next)
        {
          if (! ns->namespace[0])
            {
              returned_prop_name->namespace = ns->url;
              returned_prop_name->name = name;
              return;
            }
        }
    }

  /* If the prefix is not found, then the name is NOT within a
     namespace.  */
  returned_prop_name->namespace = "";
  returned_prop_name->name = name;
}


#define XML_HEADER "<?xml version=\"1.0\" encoding=\"utf-8\"?>"

void
svn_ra_serf__add_xml_header_buckets(serf_bucket_t *agg_bucket,
                                    serf_bucket_alloc_t *bkt_alloc)
{
  serf_bucket_t *tmp;

  tmp = SERF_BUCKET_SIMPLE_STRING_LEN(XML_HEADER, sizeof(XML_HEADER) - 1,
                                      bkt_alloc);
  serf_bucket_aggregate_append(agg_bucket, tmp);
}

void
svn_ra_serf__add_open_tag_buckets(serf_bucket_t *agg_bucket,
                                  serf_bucket_alloc_t *bkt_alloc,
                                  const char *tag, ...)
{
  va_list ap;
  const char *key;
  serf_bucket_t *tmp;

  tmp = SERF_BUCKET_SIMPLE_STRING_LEN("<", 1, bkt_alloc);
  serf_bucket_aggregate_append(agg_bucket, tmp);

  tmp = SERF_BUCKET_SIMPLE_STRING(tag, bkt_alloc);
  serf_bucket_aggregate_append(agg_bucket, tmp);

  va_start(ap, tag);
  while ((key = va_arg(ap, char *)) != NULL)
    {
      const char *val = va_arg(ap, const char *);
      if (val)
        {
          tmp = SERF_BUCKET_SIMPLE_STRING_LEN(" ", 1, bkt_alloc);
          serf_bucket_aggregate_append(agg_bucket, tmp);

          tmp = SERF_BUCKET_SIMPLE_STRING(key, bkt_alloc);
          serf_bucket_aggregate_append(agg_bucket, tmp);

          tmp = SERF_BUCKET_SIMPLE_STRING_LEN("=\"", 2, bkt_alloc);
          serf_bucket_aggregate_append(agg_bucket, tmp);

          tmp = SERF_BUCKET_SIMPLE_STRING(val, bkt_alloc);
          serf_bucket_aggregate_append(agg_bucket, tmp);

          tmp = SERF_BUCKET_SIMPLE_STRING_LEN("\"", 1, bkt_alloc);
          serf_bucket_aggregate_append(agg_bucket, tmp);
        }
    }
  va_end(ap);

  tmp = SERF_BUCKET_SIMPLE_STRING_LEN(">", 1, bkt_alloc);
  serf_bucket_aggregate_append(agg_bucket, tmp);
}

void
svn_ra_serf__add_close_tag_buckets(serf_bucket_t *agg_bucket,
                                   serf_bucket_alloc_t *bkt_alloc,
                                   const char *tag)
{
  serf_bucket_t *tmp;

  tmp = SERF_BUCKET_SIMPLE_STRING_LEN("</", 2, bkt_alloc);
  serf_bucket_aggregate_append(agg_bucket, tmp);

  tmp = SERF_BUCKET_SIMPLE_STRING(tag, bkt_alloc);
  serf_bucket_aggregate_append(agg_bucket, tmp);

  tmp = SERF_BUCKET_SIMPLE_STRING_LEN(">", 1, bkt_alloc);
  serf_bucket_aggregate_append(agg_bucket, tmp);
}

void
svn_ra_serf__add_cdata_len_buckets(serf_bucket_t *agg_bucket,
                                   serf_bucket_alloc_t *bkt_alloc,
                                   const char *data, apr_size_t len)
{
  const char *end = data + len;
  const char *p = data, *q;
  serf_bucket_t *tmp_bkt;

  while (1)
    {
      /* Find a character which needs to be quoted and append bytes up
         to that point.  Strictly speaking, '>' only needs to be
         quoted if it follows "]]", but it's easier to quote it all
         the time.

         So, why are we escaping '\r' here?  Well, according to the
         XML spec, '\r\n' gets converted to '\n' during XML parsing.
         Also, any '\r' not followed by '\n' is converted to '\n'.  By
         golly, if we say we want to escape a '\r', we want to make
         sure it remains a '\r'!  */
      q = p;
      while (q < end && *q != '&' && *q != '<' && *q != '>' && *q != '\r')
        q++;


      tmp_bkt = SERF_BUCKET_SIMPLE_STRING_LEN(p, q - p, bkt_alloc);
      serf_bucket_aggregate_append(agg_bucket, tmp_bkt);

      /* We may already be a winner.  */
      if (q == end)
        break;

      /* Append the entity reference for the character.  */
      if (*q == '&')
        {
          tmp_bkt = SERF_BUCKET_SIMPLE_STRING_LEN("&amp;", sizeof("&amp;") - 1,
                                                  bkt_alloc);
          serf_bucket_aggregate_append(agg_bucket, tmp_bkt);
        }
      else if (*q == '<')
        {
          tmp_bkt = SERF_BUCKET_SIMPLE_STRING_LEN("&lt;", sizeof("&lt;") - 1,
                                                  bkt_alloc);
          serf_bucket_aggregate_append(agg_bucket, tmp_bkt);
        }
      else if (*q == '>')
        {
          tmp_bkt = SERF_BUCKET_SIMPLE_STRING_LEN("&gt;", sizeof("&gt;") - 1,
                                                  bkt_alloc);
          serf_bucket_aggregate_append(agg_bucket, tmp_bkt);
        }
      else if (*q == '\r')
        {
          tmp_bkt = SERF_BUCKET_SIMPLE_STRING_LEN("&#13;", sizeof("&#13;") - 1,
                                                  bkt_alloc);
          serf_bucket_aggregate_append(agg_bucket, tmp_bkt);
        }

      p = q + 1;
    }
}

void svn_ra_serf__add_tag_buckets(serf_bucket_t *agg_bucket, const char *tag,
                                  const char *value,
                                  serf_bucket_alloc_t *bkt_alloc)
{
  svn_ra_serf__add_open_tag_buckets(agg_bucket, bkt_alloc, tag, NULL);

  if (value)
    {
      svn_ra_serf__add_cdata_len_buckets(agg_bucket, bkt_alloc,
                                         value, strlen(value));
    }

  svn_ra_serf__add_close_tag_buckets(agg_bucket, bkt_alloc, tag);
}

void
svn_ra_serf__xml_push_state(svn_ra_serf__xml_parser_t *parser,
                            int state)
{
  svn_ra_serf__xml_state_t *new_state;

  if (!parser->free_state)
    {
      new_state = apr_palloc(parser->pool, sizeof(*new_state));
      new_state->pool = svn_pool_create(parser->pool);
    }
  else
    {
      new_state = parser->free_state;
      parser->free_state = parser->free_state->prev;

      svn_pool_clear(new_state->pool);
    }

  if (parser->state)
    {
      new_state->private = parser->state->private;
      new_state->ns_list = parser->state->ns_list;
    }
  else
    {
      new_state->private = NULL;
      new_state->ns_list = NULL;
    }

  new_state->current_state = state;

  /* Add it to the state chain. */
  new_state->prev = parser->state;
  parser->state = new_state;
}

void svn_ra_serf__xml_pop_state(svn_ra_serf__xml_parser_t *parser)
{
  svn_ra_serf__xml_state_t *cur_state;

  cur_state = parser->state;
  parser->state = cur_state->prev;
  cur_state->prev = parser->free_state;
  parser->free_state = cur_state;
}


/* Return a pool for XES to use for self-alloc (and other specifics).  */
static apr_pool_t *
xes_pool(const svn_ra_serf__xml_estate_t *xes)
{
  /* Move up through parent states looking for one with a pool. This
     will always terminate since the initial state has a pool.  */
  while (xes->state_pool == NULL)
    xes = xes->prev;
  return xes->state_pool;
}


static void
ensure_pool(svn_ra_serf__xml_estate_t *xes)
{
  if (xes->state_pool == NULL)
    xes->state_pool = svn_pool_create(xes_pool(xes));
}


/* This callback is used by define_namespaces() to wait until a pool is
   required before constructing it.  */
static apr_pool_t *
lazy_create_pool(void *baton)
{
  svn_ra_serf__xml_estate_t *xes = baton;

  ensure_pool(xes);
  return xes->state_pool;
}

void
svn_ra_serf__xml_context_destroy(
  svn_ra_serf__xml_context_t *xmlctx)
{
  svn_pool_destroy(xmlctx->scratch_pool);
}

svn_ra_serf__xml_context_t *
svn_ra_serf__xml_context_create(
  const svn_ra_serf__xml_transition_t *ttable,
  svn_ra_serf__xml_opened_t opened_cb,
  svn_ra_serf__xml_closed_t closed_cb,
  svn_ra_serf__xml_cdata_t cdata_cb,
  void *baton,
  apr_pool_t *result_pool)
{
  svn_ra_serf__xml_context_t *xmlctx;
  svn_ra_serf__xml_estate_t *xes;

  xmlctx = apr_pcalloc(result_pool, sizeof(*xmlctx));
  xmlctx->ttable = ttable;
  xmlctx->opened_cb = opened_cb;
  xmlctx->closed_cb = closed_cb;
  xmlctx->cdata_cb = cdata_cb;
  xmlctx->baton = baton;
  xmlctx->scratch_pool = svn_pool_create(result_pool);

  xes = apr_pcalloc(result_pool, sizeof(*xes));
  /* XES->STATE == 0  */

  /* Child states may use this pool to allocate themselves. If a child
     needs to collect information, then it will construct a subpool and
     will use that to allocate itself and its collected data.  */
  xes->state_pool = result_pool;

  xmlctx->current = xes;

  return xmlctx;
}


apr_hash_t *
svn_ra_serf__xml_gather_since(svn_ra_serf__xml_estate_t *xes,
                              int stop_state)
{
  apr_hash_t *data;
  apr_pool_t *pool;

  ensure_pool(xes);
  pool = xes->state_pool;

  data = apr_hash_make(pool);

  for (; xes != NULL; xes = xes->prev)
    {
      if (xes->attrs != NULL)
        {
          apr_hash_index_t *hi;

          for (hi = apr_hash_first(pool, xes->attrs); hi;
               hi = apr_hash_next(hi))
            {
              const void *key;
              apr_ssize_t klen;
              void *val;

              /* Parent name/value lifetimes are at least as long as POOL.  */
              apr_hash_this(hi, &key, &klen, &val);
              apr_hash_set(data, key, klen, val);
            }
        }

      if (xes->state == stop_state)
        break;
    }

  return data;
}


void
svn_ra_serf__xml_note(svn_ra_serf__xml_estate_t *xes,
                      int state,
                      const char *name,
                      const char *value)
{
  svn_ra_serf__xml_estate_t *scan;

  for (scan = xes; scan != NULL && scan->state != state; scan = scan->prev)
    /* pass */ ;

  SVN_ERR_ASSERT_NO_RETURN(scan != NULL);

  /* Make sure the target state has a pool.  */
  ensure_pool(scan);

  /* ... and attribute storage.  */
  if (scan->attrs == NULL)
    scan->attrs = apr_hash_make(scan->state_pool);

  /* In all likelihood, NAME is a string constant. But we can't really
     be sure. And it isn't like we're storing a billion of these into
     the state pool.  */
  svn_hash_sets(scan->attrs,
                apr_pstrdup(scan->state_pool, name),
                apr_pstrdup(scan->state_pool, value));
}


apr_pool_t *
svn_ra_serf__xml_state_pool(svn_ra_serf__xml_estate_t *xes)
{
  /* If they asked for a pool, then ensure that we have one to provide.  */
  ensure_pool(xes);

  return xes->state_pool;
}


svn_error_t *
svn_ra_serf__xml_cb_start(svn_ra_serf__xml_context_t *xmlctx,
                          const char *raw_name,
                          const char *const *attrs)
{
  svn_ra_serf__xml_estate_t *current = xmlctx->current;
  svn_ra_serf__dav_props_t elemname;
  const svn_ra_serf__xml_transition_t *scan;
  apr_pool_t *new_pool;
  svn_ra_serf__xml_estate_t *new_xes;

  /* If we're waiting for an element to close, then just ignore all
     other element-opens.  */
  if (xmlctx->waiting.namespace != NULL)
    return SVN_NO_ERROR;

  /* Look for xmlns: attributes. Lazily create the state pool if any
     were found.  */
  define_namespaces(&current->ns_list, attrs, lazy_create_pool, current);

  svn_ra_serf__expand_ns(&elemname, current->ns_list, raw_name);

  for (scan = xmlctx->ttable; scan->ns != NULL; ++scan)
    {
      if (scan->from_state != current->state)
        continue;

      /* Wildcard tag match.  */
      if (*scan->name == '*')
        break;

      /* Found a specific transition.  */
      if (strcmp(elemname.name, scan->name) == 0
          && strcmp(elemname.namespace, scan->ns) == 0)
        break;
    }
  if (scan->ns == NULL)
    {
      if (current->state == 0)
        {
          return svn_error_createf(
                        SVN_ERR_RA_DAV_MALFORMED_DATA, NULL,
                        _("XML Parsing failed: Unexpected root element '%s'"),
                        elemname.name);
        }

      xmlctx->waiting = elemname;
      /* ### return?  */
      return SVN_NO_ERROR;
    }

  /* We should not be told to collect cdata if the closed_cb will not
     be called.  */
  SVN_ERR_ASSERT(!scan->collect_cdata || scan->custom_close);

  /* Found a transition. Make it happen.  */

  /* ### todo. push state  */

  /* ### how to use free states?  */
  /* This state should be allocated in the extent pool. If we will be
     collecting information for this state, then construct a subpool.

     ### potentially optimize away the subpool if none of the
     ### attributes are present. subpools are cheap, tho...  */
  new_pool = xes_pool(current);
  if (scan->collect_cdata || scan->collect_attrs[0])
    {
      new_pool = svn_pool_create(new_pool);

      /* Prep the new state.  */
      new_xes = apr_pcalloc(new_pool, sizeof(*new_xes));
      new_xes->state_pool = new_pool;

      /* If we're supposed to collect cdata, then set up a buffer for
         this. The existence of this buffer will instruct our cdata
         callback to collect the cdata.  */
      if (scan->collect_cdata)
        new_xes->cdata = svn_stringbuf_create_empty(new_pool);

      if (scan->collect_attrs[0] != NULL)
        {
          const char *const *saveattr = &scan->collect_attrs[0];

          new_xes->attrs = apr_hash_make(new_pool);
          for (; *saveattr != NULL; ++saveattr)
            {
              const char *name;
              const char *value;

              if (**saveattr == '?')
                {
                  name = *saveattr + 1;
                  value = svn_xml_get_attr_value(name, attrs);
                }
              else
                {
                  name = *saveattr;
                  value = svn_xml_get_attr_value(name, attrs);
                  if (value == NULL)
                    return svn_error_createf(SVN_ERR_XML_ATTRIB_NOT_FOUND,
                                             NULL,
                                             _("Missing XML attribute: '%s'"),
                                             name);
                }

              if (value)
                svn_hash_sets(new_xes->attrs, name,
                              apr_pstrdup(new_pool, value));
            }
        }
    }
  else
    {
      /* Prep the new state.  */
      new_xes = apr_pcalloc(new_pool, sizeof(*new_xes));
      /* STATE_POOL remains NULL.  */
    }

  /* Some basic copies to set up the new estate.  */
  new_xes->state = scan->to_state;
  new_xes->tag.name = apr_pstrdup(new_pool, elemname.name);
  new_xes->tag.namespace = apr_pstrdup(new_pool, elemname.namespace);
  new_xes->custom_close = scan->custom_close;

  /* Start with the parent's namespace set.  */
  new_xes->ns_list = current->ns_list;

  /* The new state is prepared. Make it current.  */
  new_xes->prev = current;
  xmlctx->current = new_xes;

  if (xmlctx->opened_cb)
    {
      START_CALLBACK(xmlctx);
      SVN_ERR(xmlctx->opened_cb(new_xes, xmlctx->baton,
                                new_xes->state, &new_xes->tag,
                                xmlctx->scratch_pool));
      END_CALLBACK(xmlctx);
      svn_pool_clear(xmlctx->scratch_pool);
    }

  return SVN_NO_ERROR;
}


svn_error_t *
svn_ra_serf__xml_cb_end(svn_ra_serf__xml_context_t *xmlctx,
                        const char *raw_name)
{
  svn_ra_serf__xml_estate_t *xes = xmlctx->current;
  svn_ra_serf__dav_props_t elemname;

  svn_ra_serf__expand_ns(&elemname, xes->ns_list, raw_name);

  if (xmlctx->waiting.namespace != NULL)
    {
      /* If this element is not the closer, then keep waiting... */
      if (strcmp(elemname.name, xmlctx->waiting.name) != 0
          || strcmp(elemname.namespace, xmlctx->waiting.namespace) != 0)
        return SVN_NO_ERROR;

      /* Found it. Stop waiting, and go back for more.  */
      xmlctx->waiting.namespace = NULL;
      return SVN_NO_ERROR;
    }

  /* We should be looking at the same tag that opened the current state.

     Unknown elements are simply skipped, so we wouldn't reach this check.

     Known elements push a new state for a given tag. Some other elemname
     would imply closing an ancestor tag (where did ours go?) or a spurious
     tag closure.  */
  if (strcmp(elemname.name, xes->tag.name) != 0
      || strcmp(elemname.namespace, xes->tag.namespace) != 0)
    return svn_error_create(SVN_ERR_XML_MALFORMED, NULL,
                            _("The response contains invalid XML"));

  if (xes->custom_close)
    {
      const svn_string_t *cdata;

      if (xes->cdata)
        {
          cdata = svn_stringbuf__morph_into_string(xes->cdata);
#ifdef SVN_DEBUG
          /* We might toss the pool holding this structure, but it could also
             be within a parent pool. In any case, for safety's sake, disable
             the stringbuf against future Badness.  */
          xes->cdata->pool = NULL;
#endif
        }
      else
        cdata = NULL;

      START_CALLBACK(xmlctx);
      SVN_ERR(xmlctx->closed_cb(xes, xmlctx->baton, xes->state,
                                cdata, xes->attrs,
                                xmlctx->scratch_pool));
      END_CALLBACK(xmlctx);
      svn_pool_clear(xmlctx->scratch_pool);
    }

  /* Pop the state.  */
  xmlctx->current = xes->prev;

  /* ### not everything should go on the free state list. XES may go
     ### away with the state pool.  */
  xes->prev = xmlctx->free_states;
  xmlctx->free_states = xes;

  /* If there is a STATE_POOL, then toss it. This will get rid of as much
     memory as possible. Potentially the XES (if we didn't create a pool
     right away, then XES may be in a parent pool).  */
  if (xes->state_pool)
    svn_pool_destroy(xes->state_pool);

  return SVN_NO_ERROR;
}


svn_error_t *
svn_ra_serf__xml_cb_cdata(svn_ra_serf__xml_context_t *xmlctx,
                          const char *data,
                          apr_size_t len)
{
  /* If we are waiting for a closing tag, then we are uninterested in
     the cdata. Just return.  */
  if (xmlctx->waiting.namespace != NULL)
    return SVN_NO_ERROR;

  /* If the current state is collecting cdata, then copy the cdata.  */
  if (xmlctx->current->cdata != NULL)
    {
      svn_stringbuf_appendbytes(xmlctx->current->cdata, data, len);
    }
  /* ... else if a CDATA_CB has been supplied, then invoke it for
     all states.  */
  else if (xmlctx->cdata_cb != NULL)
    {
      START_CALLBACK(xmlctx);
      SVN_ERR(xmlctx->cdata_cb(xmlctx->current,
                               xmlctx->baton,
                               xmlctx->current->state,
                               data, len,
                               xmlctx->scratch_pool));
      END_CALLBACK(xmlctx);
      svn_pool_clear(xmlctx->scratch_pool);
    }

  return SVN_NO_ERROR;
}

