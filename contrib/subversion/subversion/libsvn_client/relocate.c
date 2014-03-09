/*
 * relocate.c:  wrapper around wc relocation functionality.
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

#include "svn_wc.h"
#include "svn_client.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "client.h"

#include "private/svn_wc_private.h"

#include "svn_private_config.h"


/*** Code. ***/

/* Repository root and UUID for a repository. */
struct url_uuid_t
{
  const char *root;
  const char *uuid;
};

struct validator_baton_t
{
  svn_client_ctx_t *ctx;
  const char *path;
  apr_array_header_t *url_uuids;
  apr_pool_t *pool;

};


static svn_error_t *
validator_func(void *baton,
               const char *uuid,
               const char *url,
               const char *root_url,
               apr_pool_t *pool)
{
  struct validator_baton_t *b = baton;
  struct url_uuid_t *url_uuid = NULL;
  const char *disable_checks;

  apr_array_header_t *uuids = b->url_uuids;
  int i;

  for (i = 0; i < uuids->nelts; ++i)
    {
      struct url_uuid_t *uu = &APR_ARRAY_IDX(uuids, i,
                                             struct url_uuid_t);
      if (svn_uri__is_ancestor(uu->root, url))
        {
          url_uuid = uu;
          break;
        }
    }

  disable_checks = getenv("SVN_I_LOVE_CORRUPTED_WORKING_COPIES_SO_DISABLE_RELOCATE_VALIDATION");
  if (disable_checks && (strcmp(disable_checks, "yes") == 0))
    {
      /* Lie about URL_UUID's components, claiming they match the
         expectations of the validation code below.  */
      url_uuid = apr_pcalloc(pool, sizeof(*url_uuid));
      url_uuid->root = apr_pstrdup(pool, root_url);
      url_uuid->uuid = apr_pstrdup(pool, uuid);
    }

  /* We use an RA session in a subpool to get the UUID of the
     repository at the new URL so we can force the RA session to close
     by destroying the subpool. */
  if (! url_uuid)
    {
      apr_pool_t *sesspool = svn_pool_create(pool);

      url_uuid = &APR_ARRAY_PUSH(uuids, struct url_uuid_t);
      SVN_ERR(svn_client_get_repos_root(&url_uuid->root,
                                        &url_uuid->uuid,
                                        url, b->ctx,
                                        pool, sesspool));

      svn_pool_destroy(sesspool);
    }

  /* Make sure the url is a repository root if desired. */
  if (root_url
      && strcmp(root_url, url_uuid->root) != 0)
    return svn_error_createf(SVN_ERR_CLIENT_INVALID_RELOCATION, NULL,
                             _("'%s' is not the root of the repository"),
                             url);

  /* Make sure the UUIDs match. */
  if (uuid && strcmp(uuid, url_uuid->uuid) != 0)
    return svn_error_createf
      (SVN_ERR_CLIENT_INVALID_RELOCATION, NULL,
       _("The repository at '%s' has uuid '%s', but the WC has '%s'"),
       url, url_uuid->uuid, uuid);

  return SVN_NO_ERROR;
}


/* Examing the array of svn_wc_external_item2_t's EXT_DESC (parsed
   from the svn:externals property set on LOCAL_ABSPATH) and determine
   if the external working copies described by such should be
   relocated as a side-effect of the relocation of their parent
   working copy (from OLD_PARENT_REPOS_ROOT_URL to
   NEW_PARENT_REPOS_ROOT_URL).  If so, attempt said relocation.  */
static svn_error_t *
relocate_externals(const char *local_abspath,
                   apr_array_header_t *ext_desc,
                   const char *old_parent_repos_root_url,
                   const char *new_parent_repos_root_url,
                   svn_client_ctx_t *ctx,
                   apr_pool_t *scratch_pool)
{
  apr_pool_t *iterpool;
  int i;

  /* Parse an externals definition into an array of external items. */

  iterpool = svn_pool_create(scratch_pool);

  for (i = 0; i < ext_desc->nelts; i++)
    {
      svn_wc_external_item2_t *ext_item =
        APR_ARRAY_IDX(ext_desc, i, svn_wc_external_item2_t *);
      const char *target_repos_root_url;
      const char *target_abspath;
      svn_error_t *err;

      svn_pool_clear(iterpool);

      /* If this external isn't pulled in via a relative URL, ignore
         it.  There's no sense in relocating a working copy only to
         have the next 'svn update' try to point it back to another
         location. */
      if (! ((strncmp("../", ext_item->url, 3) == 0) ||
             (strncmp("^/", ext_item->url, 2) == 0)))
        continue;

      /* If the external working copy's not-yet-relocated repos root
         URL matches the primary working copy's pre-relocated
         repository root URL, try to relocate that external, too.
         You might wonder why this check is needed, given that we're
         already limiting ourselves to externals pulled via URLs
         relative to their primary working copy.  Well, it's because
         you can use "../" to "crawl up" above one repository's URL
         space and down into another one.  */
      SVN_ERR(svn_dirent_get_absolute(&target_abspath,
                                      svn_dirent_join(local_abspath,
                                                      ext_item->target_dir,
                                                      iterpool),
                                      iterpool));
      err = svn_client_get_repos_root(&target_repos_root_url, NULL /* uuid */,
                                      target_abspath, ctx, iterpool, iterpool);

      /* Ignore externals that aren't present in the working copy.
       * This can happen if an external is deleted from disk accidentally,
       * or if an external is configured on a locally added directory. */
      if (err && err->apr_err == SVN_ERR_WC_PATH_NOT_FOUND)
        {
          svn_error_clear(err);
          continue;
        }
      else
        SVN_ERR(err);

      if (strcmp(target_repos_root_url, old_parent_repos_root_url) == 0)
        SVN_ERR(svn_client_relocate2(target_abspath,
                                     old_parent_repos_root_url,
                                     new_parent_repos_root_url,
                                     FALSE, ctx, iterpool));
    }

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_client_relocate2(const char *wcroot_dir,
                     const char *from_prefix,
                     const char *to_prefix,
                     svn_boolean_t ignore_externals,
                     svn_client_ctx_t *ctx,
                     apr_pool_t *pool)
{
  struct validator_baton_t vb;
  const char *local_abspath;
  apr_hash_t *externals_hash = NULL;
  apr_hash_index_t *hi;
  apr_pool_t *iterpool = NULL;
  const char *old_repos_root_url, *new_repos_root_url;

  /* Populate our validator callback baton, and call the relocate code. */
  vb.ctx = ctx;
  vb.path = wcroot_dir;
  vb.url_uuids = apr_array_make(pool, 1, sizeof(struct url_uuid_t));
  vb.pool = pool;

  if (svn_path_is_url(wcroot_dir))
    return svn_error_createf(SVN_ERR_ILLEGAL_TARGET, NULL,
                             _("'%s' is not a local path"),
                             wcroot_dir);

  SVN_ERR(svn_dirent_get_absolute(&local_abspath, wcroot_dir, pool));

  /* If we're ignoring externals, just relocate and get outta here. */
  if (ignore_externals)
    {
      return svn_error_trace(svn_wc_relocate4(ctx->wc_ctx, local_abspath,
                                              from_prefix, to_prefix,
                                              validator_func, &vb, pool));
    }

  /* Fetch our current root URL. */
  SVN_ERR(svn_client_get_repos_root(&old_repos_root_url, NULL /* uuid */,
                                    local_abspath, ctx, pool, pool));

  /* Perform the relocation. */
  SVN_ERR(svn_wc_relocate4(ctx->wc_ctx, local_abspath, from_prefix, to_prefix,
                           validator_func, &vb, pool));

  /* Now fetch new current root URL. */
  SVN_ERR(svn_client_get_repos_root(&new_repos_root_url, NULL /* uuid */,
                                    local_abspath, ctx, pool, pool));


  /* Relocate externals, too (if any). */
  SVN_ERR(svn_wc__externals_gather_definitions(&externals_hash, NULL,
                                               ctx->wc_ctx, local_abspath,
                                               svn_depth_infinity,
                                               pool, pool));
  if (! apr_hash_count(externals_hash))
    return SVN_NO_ERROR;

  iterpool = svn_pool_create(pool);

  for (hi = apr_hash_first(pool, externals_hash);
       hi != NULL;
       hi = apr_hash_next(hi))
    {
      const char *this_abspath = svn__apr_hash_index_key(hi);
      const char *value = svn__apr_hash_index_val(hi);
      apr_array_header_t *ext_desc;

      svn_pool_clear(iterpool);

      SVN_ERR(svn_wc_parse_externals_description3(&ext_desc, this_abspath,
                                                  value, FALSE,
                                                  iterpool));
      if (ext_desc->nelts)
        SVN_ERR(relocate_externals(this_abspath, ext_desc, old_repos_root_url,
                                   new_repos_root_url, ctx, iterpool));
    }

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}
