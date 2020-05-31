/*
* layout.c:  code to list and update the working copy layout
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

#include "svn_hash.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_wc.h"
#include "svn_client.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "client.h"

#include "svn_private_config.h"
#include "private/svn_wc_private.h"

struct layout_item_t
{
  const char *local_abspath;
  const char *url;
  svn_revnum_t revision;
  svn_depth_t depth;
  struct layout_item_t *ancestor;
  apr_pool_t *pool;
};

struct client_layout_baton_t
{
  const char *root_abspath;
  svn_wc_context_t *wc_ctx;
  const char *repos_root_url;

  struct layout_item_t *stack;
  apr_pool_t *root_pool;

  svn_client__layout_func_t layout;
  void *layout_baton;
};


static svn_error_t *
layout_set_path(void *report_baton,
                const char *path,
                svn_revnum_t revision,
                svn_depth_t depth,
                svn_boolean_t start_empty,
                const char *lock_token,
                apr_pool_t *pool)
{
  struct client_layout_baton_t *lb = report_baton;
  const char *local_abspath = svn_dirent_join(lb->root_abspath, path, pool);
  struct layout_item_t *it;
  apr_pool_t *item_pool;
  svn_depth_t expected_depth;

  while (lb->stack
          && !svn_dirent_is_ancestor(lb->stack->local_abspath, local_abspath))
    {
      it = lb->stack;
      lb->stack = it->ancestor;
      svn_pool_destroy(it->pool);
    }

  item_pool = svn_pool_create(lb->stack ? lb->stack->pool
                                        : lb->root_pool);

  it = apr_pcalloc(item_pool, sizeof(*it));
  it->pool = item_pool;
  it->local_abspath = apr_pstrdup(item_pool, local_abspath);
  it->depth = depth;
  it->revision = revision;
  if (lb->stack)
    {
      it->url = svn_path_url_add_component2(
                     lb->stack->url,
                     svn_dirent_skip_ancestor(lb->stack->local_abspath,
                                              local_abspath),
                     item_pool);
    }
  else
    {
      const char *repos_relpath, *repos_root_url;

      SVN_ERR(svn_wc__node_get_base(NULL, NULL, &repos_relpath,
                                    &repos_root_url, NULL, NULL,
                                    lb->wc_ctx, local_abspath,
                                    FALSE /* ignore_enoent */,
                                    pool, pool));

      lb->repos_root_url = apr_pstrdup(lb->root_pool, repos_root_url);
      it->url = svn_path_url_add_component2(repos_root_url, repos_relpath,
                                            item_pool);
    }
  it->ancestor = lb->stack;
  lb->stack = it;

  if (!it->ancestor)
    expected_depth = depth;
  else if (it->ancestor->depth == svn_depth_infinity)
    expected_depth = svn_depth_infinity;
  else
    expected_depth = svn_depth_empty;

  return svn_error_trace(lb->layout(lb->layout_baton,
                                    it->local_abspath,
                                    lb->repos_root_url,
                                    FALSE /* not-present */,
                                    FALSE /* url changed */,
                                    it->url,
                                    it->ancestor
                                      ? it->ancestor->revision != it->revision
                                      : FALSE,
                                    it->revision,
                                    (depth != expected_depth),
                                    it->depth,
                                    pool));
    }

static svn_error_t *
layout_link_path(void *report_baton,
                 const char *path,
                 const char *url,
                 svn_revnum_t revision,
                 svn_depth_t depth,
                 svn_boolean_t start_empty,
                 const char *lock_token,
                 apr_pool_t *pool)
{
  struct client_layout_baton_t *lb = report_baton;
  const char *local_abspath = svn_dirent_join(lb->root_abspath, path, pool);
  struct layout_item_t *it;
  apr_pool_t *item_pool;
  svn_depth_t expected_depth;

  SVN_ERR_ASSERT(lb->stack); /* Always below root entry */

  while (!svn_dirent_is_ancestor(lb->stack->local_abspath, local_abspath))
    {
      it = lb->stack;
      lb->stack = it->ancestor;
      svn_pool_destroy(it->pool);
    }

  item_pool = svn_pool_create(lb->stack ? lb->stack->pool
                                        : lb->root_pool);

  it = apr_pcalloc(item_pool, sizeof(*it));
  it->pool = item_pool;
  it->local_abspath = apr_pstrdup(item_pool, local_abspath);
  it->depth = depth;
  it->revision = revision;
  it->url = apr_pstrdup(item_pool, url);

  it->ancestor = lb->stack;
  lb->stack = it;

  if (it->ancestor->depth == svn_depth_infinity)
    expected_depth = svn_depth_infinity;
  else
    expected_depth = svn_depth_empty;

  return svn_error_trace(lb->layout(lb->layout_baton,
                                    it->local_abspath,
                                    lb->repos_root_url,
                                    FALSE /* not-present */,
                                    TRUE /* url changed */,
                                    it->url,
                                    it->ancestor
                                      ? it->ancestor->revision != it->revision
                                      : FALSE,
                                    it->revision,
                                    (depth != expected_depth),
                                    it->depth,
                                    pool));
}

static svn_error_t *
layout_delete_path(void *report_baton,
                   const char *path,
                   apr_pool_t *pool)
{
  struct client_layout_baton_t *lb = report_baton;
  const char *local_abspath = svn_dirent_join(lb->root_abspath, path, pool);
  struct layout_item_t *it;

  SVN_ERR_ASSERT(lb->stack); /* Always below root entry */

  while (!svn_dirent_is_ancestor(lb->stack->local_abspath, local_abspath))
    {
      it = lb->stack;
      lb->stack = it->ancestor;
      svn_pool_destroy(it->pool);
    }

  return svn_error_trace(lb->layout(lb->layout_baton,
                                    local_abspath,
                                    lb->repos_root_url,
                                    TRUE /* not-present */,
                                    FALSE /* url changed */,
                                    NULL /* no-url */,
                                    FALSE /* revision changed */,
                                    SVN_INVALID_REVNUM,
                                    FALSE /* depth changed */,
                                    svn_depth_unknown,
                                    pool));
}

static svn_error_t *
layout_finish_report(void *report_baton,
                     apr_pool_t *pool)
{
  /*struct client_layout_baton_t *lb = report_baton;*/
  return SVN_NO_ERROR;
}

static svn_error_t *
layout_abort_report(void *report_baton,
                     apr_pool_t *pool)
{
  /*struct client_layout_baton_t *lb = report_baton;*/
  return SVN_NO_ERROR;
}

static const svn_ra_reporter3_t layout_reporter =
{
  layout_set_path,
  layout_delete_path,
  layout_link_path,
  layout_finish_report,
  layout_abort_report
};

svn_error_t *
svn_client__layout_list(const char *local_abspath,
                        svn_client__layout_func_t layout,
                        void *layout_baton,
                        svn_client_ctx_t *ctx,
                        apr_pool_t *scratch_pool)
{
  struct client_layout_baton_t lb;

  lb.root_abspath = local_abspath;
  lb.root_pool = scratch_pool;
  lb.wc_ctx = ctx->wc_ctx;
  lb.repos_root_url = NULL; /* Filled in later */
  lb.stack = NULL;

  lb.layout = layout;
  lb.layout_baton = layout_baton;

  /* Drive the reporter structure, describing the revisions within
     LOCAL_ABSPATH.  */
  SVN_ERR(svn_wc_crawl_revisions5(ctx->wc_ctx, local_abspath,
                                  &layout_reporter, &lb,
                                  FALSE /* restore_files */,
                                  svn_depth_infinity,
                                  TRUE /* honor_depth_exclude */,
                                  FALSE /* depth_compatibility_trick */,
                                  FALSE /* use_commit_times */,
                                  ctx->cancel_func, ctx->cancel_baton,
                                  ctx->notify_func2, ctx->notify_baton2,
                                  scratch_pool));
  return SVN_NO_ERROR;
}
