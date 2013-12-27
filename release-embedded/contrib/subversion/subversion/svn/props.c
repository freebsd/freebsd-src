/*
 * props.c: Utility functions for property handling
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

/* ==================================================================== */



/*** Includes. ***/

#include <stdlib.h>

#include <apr_hash.h>
#include "svn_hash.h"
#include "svn_cmdline.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_sorts.h"
#include "svn_subst.h"
#include "svn_props.h"
#include "svn_string.h"
#include "svn_opt.h"
#include "svn_xml.h"
#include "svn_base64.h"
#include "cl.h"

#include "private/svn_string_private.h"
#include "private/svn_cmdline_private.h"

#include "svn_private_config.h"


svn_error_t *
svn_cl__revprop_prepare(const svn_opt_revision_t *revision,
                        const apr_array_header_t *targets,
                        const char **URL,
                        svn_client_ctx_t *ctx,
                        apr_pool_t *pool)
{
  const char *target;

  if (revision->kind != svn_opt_revision_number
      && revision->kind != svn_opt_revision_date
      && revision->kind != svn_opt_revision_head)
    return svn_error_create
      (SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
       _("Must specify the revision as a number, a date or 'HEAD' "
         "when operating on a revision property"));

  /* There must be exactly one target at this point.  If it was optional and
     unspecified by the user, the caller has already added the implicit '.'. */
  if (targets->nelts != 1)
    return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                            _("Wrong number of targets specified"));

  /* (The docs say the target must be either a URL or implicit '.', but
     explicit WC targets are also accepted.) */
  target = APR_ARRAY_IDX(targets, 0, const char *);
  SVN_ERR(svn_client_url_from_path2(URL, target, ctx, pool, pool));
  if (*URL == NULL)
    return svn_error_create
      (SVN_ERR_UNVERSIONED_RESOURCE, NULL,
       _("Either a URL or versioned item is required"));

  return SVN_NO_ERROR;
}

void
svn_cl__check_boolean_prop_val(const char *propname, const char *propval,
                               apr_pool_t *pool)
{
  svn_stringbuf_t *propbuf;

  if (!svn_prop_is_boolean(propname))
    return;

  propbuf = svn_stringbuf_create(propval, pool);
  svn_stringbuf_strip_whitespace(propbuf);

  if (propbuf->data[0] == '\0'
      || svn_cstring_casecmp(propbuf->data, "0") == 0
      || svn_cstring_casecmp(propbuf->data, "no") == 0
      || svn_cstring_casecmp(propbuf->data, "off") == 0
      || svn_cstring_casecmp(propbuf->data, "false") == 0)
    {
      svn_error_t *err = svn_error_createf
        (SVN_ERR_BAD_PROPERTY_VALUE, NULL,
         _("To turn off the %s property, use 'svn propdel';\n"
           "setting the property to '%s' will not turn it off."),
           propname, propval);
      svn_handle_warning2(stderr, err, "svn: ");
      svn_error_clear(err);
    }
}


/* Context for sorting property names */
struct simprop_context_t
{
  svn_string_t name;    /* The name of the property we're comparing with */
  svn_membuf_t buffer;  /* Buffer for similarity testing */
};

struct simprop_t
{
  const char *propname; /* The original svn: property name */
  svn_string_t name;    /* The property name without the svn: prefix */
  unsigned int score;   /* The similarity score */
  apr_size_t diff;      /* Number of chars different from context.name */
  struct simprop_context_t *context; /* Sorting context for qsort() */
};

/* Similarity test between two property names */
static APR_INLINE unsigned int
simprop_key_diff(const svn_string_t *key, const svn_string_t *ctx,
                 svn_membuf_t *buffer, apr_size_t *diff)
{
  apr_size_t lcs;
  const unsigned int score = svn_string__similarity(key, ctx, buffer, &lcs);
  if (key->len > ctx->len)
    *diff = key->len - lcs;
  else
    *diff = ctx->len - lcs;
  return score;
}

/* Key comparator for qsort for simprop_t */
static int
simprop_compare(const void *pkeya, const void *pkeyb)
{
  struct simprop_t *const keya = *(struct simprop_t *const *)pkeya;
  struct simprop_t *const keyb = *(struct simprop_t *const *)pkeyb;
  struct simprop_context_t *const context = keya->context;

  if (keya->score == -1)
    keya->score = simprop_key_diff(&keya->name, &context->name,
                                   &context->buffer, &keya->diff);
  if (keyb->score == -1)
    keyb->score = simprop_key_diff(&keyb->name, &context->name,
                                   &context->buffer, &keyb->diff);

  return (keya->score < keyb->score ? 1
          : (keya->score > keyb->score ? -1
             : (keya->diff > keyb->diff ? 1
                : (keya->diff < keyb->diff ? -1 : 0))));
}


static const char*
force_prop_option_message(svn_cl__prop_use_t prop_use, const char *prop_name,
                          apr_pool_t *scratch_pool)
{
  switch (prop_use)
    {
    case svn_cl__prop_use_set:
      return apr_psprintf(
          scratch_pool,
          _("(To set the '%s' property, re-run with '--force'.)"),
          prop_name);
    case svn_cl__prop_use_edit:
      return apr_psprintf(
          scratch_pool,
          _("(To edit the '%s' property, re-run with '--force'.)"),
          prop_name);
    case svn_cl__prop_use_use:
    default:
      return apr_psprintf(
          scratch_pool,
          _("(To use the '%s' property, re-run with '--force'.)"),
          prop_name);
    }
}

static const char*
wrong_prop_error_message(svn_cl__prop_use_t prop_use, const char *prop_name,
                         apr_pool_t *scratch_pool)
{
  switch (prop_use)
    {
    case svn_cl__prop_use_set:
      return apr_psprintf(
          scratch_pool,
          _("'%s' is not a valid %s property name;"
            " re-run with '--force' to set it"),
          prop_name, SVN_PROP_PREFIX);
    case svn_cl__prop_use_edit:
      return apr_psprintf(
          scratch_pool,
          _("'%s' is not a valid %s property name;"
            " re-run with '--force' to edit it"),
          prop_name, SVN_PROP_PREFIX);
    case svn_cl__prop_use_use:
    default:
      return apr_psprintf(
          scratch_pool,
          _("'%s' is not a valid %s property name;"
            " re-run with '--force' to use it"),
          prop_name, SVN_PROP_PREFIX);
    }
}

svn_error_t *
svn_cl__check_svn_prop_name(const char *propname,
                            svn_boolean_t revprop,
                            svn_cl__prop_use_t prop_use,
                            apr_pool_t *scratch_pool)
{
  static const char *const nodeprops[] =
    {
      SVN_PROP_NODE_ALL_PROPS
    };
  static const apr_size_t nodeprops_len = sizeof(nodeprops)/sizeof(*nodeprops);

  static const char *const revprops[] =
    {
      SVN_PROP_REVISION_ALL_PROPS
    };
  static const apr_size_t revprops_len = sizeof(revprops)/sizeof(*revprops);

  const char *const *const proplist = (revprop ? revprops : nodeprops);
  const apr_size_t numprops = (revprop ? revprops_len : nodeprops_len);

  struct simprop_t **propkeys;
  struct simprop_t *propbuf;
  apr_size_t i;

  struct simprop_context_t context;
  svn_string_t prefix;

  context.name.data = propname;
  context.name.len = strlen(propname);
  prefix.data = SVN_PROP_PREFIX;
  prefix.len = strlen(SVN_PROP_PREFIX);

  svn_membuf__create(&context.buffer, 0, scratch_pool);

  /* First, check if the name is even close to being in the svn: namespace.
     It must contain a colon in the right place, and we only allow
     one-char typos or a single transposition. */
  if (context.name.len < prefix.len
      || context.name.data[prefix.len - 1] != prefix.data[prefix.len - 1])
    return SVN_NO_ERROR;        /* Wrong prefix, ignore */
  else
    {
      apr_size_t lcs;
      const apr_size_t name_len = context.name.len;
      context.name.len = prefix.len; /* Only check up to the prefix length */
      svn_string__similarity(&context.name, &prefix, &context.buffer, &lcs);
      context.name.len = name_len; /* Restore the original propname length */
      if (lcs < prefix.len - 1)
        return SVN_NO_ERROR;    /* Wrong prefix, ignore */

      /* If the prefix is slightly different, the rest must be
         identical in order to trigger the error. */
      if (lcs == prefix.len - 1)
        {
          for (i = 0; i < numprops; ++i)
            {
              if (0 == strcmp(proplist[i] + prefix.len, propname + prefix.len))
                return svn_error_createf(
                  SVN_ERR_CLIENT_PROPERTY_NAME, NULL,
                  _("'%s' is not a valid %s property name;"
                    " did you mean '%s'?\n%s"),
                  propname, SVN_PROP_PREFIX, proplist[i],
                  force_prop_option_message(prop_use, propname, scratch_pool));
            }
          return SVN_NO_ERROR;
        }
    }

  /* Now find the closest match from amongst the set of reserved
     node or revision property names. Skip the prefix while matching,
     we already know that it's the same and looking at it would only
     skew the results. */
  propkeys = apr_palloc(scratch_pool,
                        numprops * sizeof(struct simprop_t*));
  propbuf = apr_palloc(scratch_pool,
                       numprops * sizeof(struct simprop_t));
  context.name.data += prefix.len;
  context.name.len -= prefix.len;
  for (i = 0; i < numprops; ++i)
    {
      propkeys[i] = &propbuf[i];
      propbuf[i].propname = proplist[i];
      propbuf[i].name.data = proplist[i] + prefix.len;
      propbuf[i].name.len = strlen(propbuf[i].name.data);
      propbuf[i].score = (unsigned int)-1;
      propbuf[i].context = &context;
    }

  qsort(propkeys, numprops, sizeof(*propkeys), simprop_compare);

  if (0 == propkeys[0]->diff)
    return SVN_NO_ERROR;        /* We found an exact match. */

  /* See if we can suggest a sane alternative spelling */
  for (i = 0; i < numprops; ++i)
    if (propkeys[i]->score < 666) /* 2/3 similarity required */
      break;

  switch (i)
    {
    case 0:
      /* The best alternative isn't good enough */
      return svn_error_create(
        SVN_ERR_CLIENT_PROPERTY_NAME, NULL,
        wrong_prop_error_message(prop_use, propname, scratch_pool));

    case 1:
      /* There is only one good candidate */
      return svn_error_createf(
        SVN_ERR_CLIENT_PROPERTY_NAME, NULL,
        _("'%s' is not a valid %s property name; did you mean '%s'?\n%s"),
        propname, SVN_PROP_PREFIX, propkeys[0]->propname,
        force_prop_option_message(prop_use, propname, scratch_pool));

    case 2:
      /* Suggest a list of the most likely candidates */
      return svn_error_createf(
        SVN_ERR_CLIENT_PROPERTY_NAME, NULL,
        _("'%s' is not a valid %s property name\n"
          "Did you mean '%s' or '%s'?\n%s"),
        propname, SVN_PROP_PREFIX,
        propkeys[0]->propname, propkeys[1]->propname,
        force_prop_option_message(prop_use, propname, scratch_pool));

    default:
      /* Never suggest more than three candidates */
      return svn_error_createf(
        SVN_ERR_CLIENT_PROPERTY_NAME, NULL,
        _("'%s' is not a valid %s property name\n"
          "Did you mean '%s', '%s' or '%s'?\n%s"),
        propname, SVN_PROP_PREFIX,
        propkeys[0]->propname, propkeys[1]->propname, propkeys[2]->propname,
        force_prop_option_message(prop_use, propname, scratch_pool));
    }
}
