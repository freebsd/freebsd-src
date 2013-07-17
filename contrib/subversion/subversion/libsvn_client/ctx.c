/*
 * ctx.c:  initialization function for client context
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

#include <apr_pools.h>
#include "svn_hash.h"
#include "svn_client.h"
#include "svn_error.h"

#include "private/svn_wc_private.h"


/*** Code. ***/

/* Call the notify_func of the context provided by BATON, if non-NULL. */
static void
call_notify_func(void *baton, const svn_wc_notify_t *n, apr_pool_t *pool)
{
  const svn_client_ctx_t *ctx = baton;

  if (ctx->notify_func)
    ctx->notify_func(ctx->notify_baton, n->path, n->action, n->kind,
                     n->mime_type, n->content_state, n->prop_state,
                     n->revision);
}

static svn_error_t *
call_conflict_func(svn_wc_conflict_result_t **result,
                   const svn_wc_conflict_description2_t *conflict,
                   void *baton,
                   apr_pool_t *result_pool,
                   apr_pool_t *scratch_pool)
{
  svn_client_ctx_t *ctx = baton;

  if (ctx->conflict_func)
    {
      const svn_wc_conflict_description_t *cd;

      cd = svn_wc__cd2_to_cd(conflict, scratch_pool);
      SVN_ERR(ctx->conflict_func(result, cd, ctx->conflict_baton,
                                 result_pool));
    }
  else
    {
      /* We have to set a result; so we postpone */
      *result = svn_wc_create_conflict_result(svn_wc_conflict_choose_postpone,
                                              NULL, result_pool);
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_client_create_context2(svn_client_ctx_t **ctx,
                           apr_hash_t *cfg_hash,
                           apr_pool_t *pool)
{
  svn_config_t *cfg_config;

  *ctx = apr_pcalloc(pool, sizeof(svn_client_ctx_t));

  (*ctx)->notify_func2 = call_notify_func;
  (*ctx)->notify_baton2 = *ctx;

  (*ctx)->conflict_func2 = call_conflict_func;
  (*ctx)->conflict_baton2 = *ctx;

  (*ctx)->config = cfg_hash;

  if (cfg_hash)
    cfg_config = svn_hash_gets(cfg_hash, SVN_CONFIG_CATEGORY_CONFIG);
  else
    cfg_config = NULL;

  SVN_ERR(svn_wc_context_create(&(*ctx)->wc_ctx, cfg_config, pool,
                                pool));

  return SVN_NO_ERROR;
}

svn_error_t *
svn_client_create_context(svn_client_ctx_t **ctx,
                          apr_pool_t *pool)
{
  return svn_client_create_context2(ctx, NULL, pool);
}
