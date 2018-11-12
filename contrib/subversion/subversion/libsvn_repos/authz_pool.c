/*
 * authz_pool.c :  pool of authorization objects
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



#include "svn_checksum.h"
#include "svn_config.h"
#include "svn_error.h"
#include "svn_pools.h"

#include "private/svn_dep_compat.h"
#include "private/svn_mutex.h"
#include "private/svn_object_pool.h"
#include "private/svn_subr_private.h"
#include "private/svn_repos_private.h"
#include "private/svn_string_private.h"
#include "private/svn_subr_private.h"

#include "repos.h"

/* Currently this structure is just a wrapper around a svn_config_t.
 */
struct svn_authz_t
{
  svn_config_t *cfg;
};

/* The wrapper object structure that we store in the object pool.  It
 * combines the authz with the underlying config structures and their
 * identifying keys.
 */
typedef struct authz_object_t
{
  /* key = concatenation of AUTHZ_KEY and GROUPS_KEY */
  svn_membuf_t *key;

  /* keys used to identify AUTHZ_CFG and GROUPS_CFG */
  svn_membuf_t *authz_key;
  svn_membuf_t *groups_key;

  /* r/o references to configurations from the configuration pool.
     GROUPS_CFG may be NULL. */
  svn_config_t *authz_cfg;
  svn_config_t *groups_cfg;

  /* Case-sensitive config. */
  svn_authz_t *authz;
} authz_object_t;

/* Root data structure simply adding the config_pool to the basic object pool.
 */
struct svn_repos__authz_pool_t
{
  /* authz_object_t object storage */
  svn_object_pool__t *object_pool;

  /* factory and storage of (shared) configuration objects */
  svn_repos__config_pool_t *config_pool;
};

/* Return a combination of AUTHZ_KEY and GROUPS_KEY, allocated in POOL.
 * GROUPS_KEY may be NULL.
 */
static svn_membuf_t *
construct_key(svn_membuf_t *authz_key,
              svn_membuf_t *groups_key,
              apr_pool_t *pool)
{
  svn_membuf_t *result = apr_pcalloc(pool, sizeof(*result));
  apr_size_t size;
  if (groups_key)
    {
      size = authz_key->size + groups_key->size;
      svn_membuf__create(result,size, pool);
      memcpy(result->data, authz_key->data, authz_key->size);
      memcpy((char *)result->data + authz_key->size,
             groups_key->data, groups_key->size);
    }
  else
    {
      size = authz_key->size;
      svn_membuf__create(result, size, pool);
      memcpy(result->data, authz_key->data, authz_key->size);
    }

  result->size = size;
  return result;
}

/* Implement svn_object_pool__getter_t on authz_object_t structures.
 */
static void *
getter(void *object,
       void *baton,
       apr_pool_t *pool)
{
  return ((authz_object_t *)object)->authz;
}

/* API implementation */

svn_error_t *
svn_repos__authz_pool_create(svn_repos__authz_pool_t **authz_pool,
                             svn_repos__config_pool_t *config_pool,
                             svn_boolean_t thread_safe,
                             apr_pool_t *pool)
{
  svn_repos__authz_pool_t *result;
  svn_object_pool__t *object_pool;

  /* there is no setter as we don't need to update existing authz */
  SVN_ERR(svn_object_pool__create(&object_pool, getter, NULL, thread_safe,
                                  pool));

  result = apr_pcalloc(pool, sizeof(*result));
  result->object_pool = object_pool;
  result->config_pool = config_pool;

  *authz_pool = result;
  return SVN_NO_ERROR;
}

svn_error_t *
svn_repos__authz_pool_get(svn_authz_t **authz_p,
                          svn_repos__authz_pool_t *authz_pool,
                          const char *path,
                          const char *groups_path,
                          svn_boolean_t must_exist,
                          svn_repos_t *preferred_repos,
                          apr_pool_t *pool)
{
  apr_pool_t *authz_ref_pool
    = svn_object_pool__new_wrapper_pool(authz_pool->object_pool);
  authz_object_t *authz_ref
    = apr_pcalloc(authz_ref_pool, sizeof(*authz_ref));
  svn_boolean_t have_all_keys;

  /* read the configurations */
  SVN_ERR(svn_repos__config_pool_get(&authz_ref->authz_cfg,
                                     &authz_ref->authz_key,
                                     authz_pool->config_pool,
                                     path, must_exist, TRUE,
                                     preferred_repos, authz_ref_pool));
  have_all_keys = authz_ref->authz_key != NULL;

  if (groups_path)
    {
      SVN_ERR(svn_repos__config_pool_get(&authz_ref->groups_cfg,
                                         &authz_ref->groups_key,
                                         authz_pool->config_pool,
                                         groups_path, must_exist, TRUE,
                                         preferred_repos, authz_ref_pool));
      have_all_keys &= authz_ref->groups_key != NULL;
    }

  /* fall back to standard implementation in case we don't have all the
   * facts (i.e. keys). */
  if (!have_all_keys)
    return svn_error_trace(svn_repos_authz_read2(authz_p, path, groups_path,
                                                 must_exist, pool));

  /* all keys are known and lookup is unambigious. */
  authz_ref->key = construct_key(authz_ref->authz_key,
                                 authz_ref->groups_key,
                                 authz_ref_pool);

  SVN_ERR(svn_object_pool__lookup((void **)authz_p, authz_pool->object_pool,
                                  authz_ref->key, NULL, pool));
  if (*authz_p)
    {
      svn_pool_destroy(authz_ref_pool);
      return SVN_NO_ERROR;
    }

  authz_ref->authz = apr_palloc(authz_ref_pool, sizeof(*authz_ref->authz));
  authz_ref->authz->cfg = authz_ref->authz_cfg;

  if (groups_path)
    {
      /* Easy out: we prohibit local groups in the authz file when global
         groups are being used. */
      if (svn_config_has_section(authz_ref->authz->cfg,
                                 SVN_CONFIG_SECTION_GROUPS))
        return svn_error_createf(SVN_ERR_AUTHZ_INVALID_CONFIG, NULL,
                                 "Error reading authz file '%s' with "
                                 "groups file '%s':"
                                 "Authz file cannot contain any groups "
                                 "when global groups are being used.",
                                 path, groups_path);

      /* We simply need to add the [Groups] section to the authz config.
       */
      svn_config__shallow_replace_section(authz_ref->authz->cfg,
                                          authz_ref->groups_cfg,
                                          SVN_CONFIG_SECTION_GROUPS);
    }

  /* Make sure there are no errors in the configuration. */
  SVN_ERR(svn_repos__authz_validate(authz_ref->authz, authz_ref_pool));

  SVN_ERR(svn_object_pool__insert((void **)authz_p, authz_pool->object_pool,
                                  authz_ref->key, authz_ref, NULL,
                                  authz_ref_pool, pool));

  return SVN_NO_ERROR;
}
