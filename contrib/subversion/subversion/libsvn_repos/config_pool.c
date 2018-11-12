/*
 * config_pool.c :  pool of configuration objects
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
#include "svn_hash.h"
#include "svn_path.h"
#include "svn_pools.h"
#include "svn_repos.h"

#include "private/svn_dep_compat.h"
#include "private/svn_mutex.h"
#include "private/svn_subr_private.h"
#include "private/svn_repos_private.h"
#include "private/svn_object_pool.h"

#include "svn_private_config.h"


/* Our wrapper structure for parsed svn_config_t* instances.  All data in
 * CS_CFG and CI_CFG is expanded (to make it thread-safe) and considered
 * read-only.
 */
typedef struct config_object_t
{
  /* UUID of the configuration contents.
   * This is a SHA1 checksum of the parsed textual representation of CFG. */
  svn_checksum_t *key;

  /* Parsed and expanded configuration.  At least one of the following
   * must not be NULL. */

  /* Case-sensitive config. May be NULL */
  svn_config_t *cs_cfg;

  /* Case-insensitive config. May be NULL */
  svn_config_t *ci_cfg;
} config_object_t;


/* Data structure used to short-circuit the repository access for configs
 * read via URL.  After reading such a config successfully, we store key
 * repository information here and will validate it without actually opening
 * the repository.
 *
 * As this is only an optimization and may create many entries in
 * svn_repos__config_pool_t's IN_REPO_HASH_POOL index, we clean them up
 * once in a while.
 */
typedef struct in_repo_config_t
{
  /* URL used to open the configuration */
  const char *url;

  /* Path of the repository that contained URL */
  const char *repo_root;

  /* Head revision of that repository when last read */
  svn_revnum_t revision;

  /* Contents checksum of the file stored under URL@REVISION */
  svn_checksum_t *key;
} in_repo_config_t;


/* Core data structure extending the encapsulated OBJECT_POOL.  All access
 * to it must be serialized using the OBJECT_POOL->MUTEX.
 *
 * To speed up URL@HEAD lookups, we maintain IN_REPO_CONFIGS as a secondary
 * hash index.  It maps URLs as provided by the caller onto in_repo_config_t
 * instances.  If that is still up-to-date, a further lookup into CONFIG
 * may yield the desired configuration without the need to actually open
 * the respective repository.
 *
 * Unused configurations that are kept in the IN_REPO_CONFIGS hash and may
 * be cleaned up when the hash is about to grow.
 */
struct svn_repos__config_pool_t
{
  svn_object_pool__t *object_pool;

  /* URL -> in_repo_config_t* mapping.
   * This is only a partial index and will get cleared regularly. */
  apr_hash_t *in_repo_configs;

  /* allocate the IN_REPO_CONFIGS index and in_repo_config_t here */
  apr_pool_t *in_repo_hash_pool;
};


/* Return an automatic reference to the CFG member in CONFIG that will be
 * released when POOL gets cleaned up.  The case sensitivity flag in *BATON
 * selects the desired option and section name matching mode.
 */
static void *
getter(void *object,
       void *baton,
       apr_pool_t *pool)
{
  config_object_t *wrapper = object;
  svn_boolean_t *case_sensitive = baton;
  svn_config_t *config = *case_sensitive ? wrapper->cs_cfg : wrapper->ci_cfg;

  /* we need to duplicate the root structure as it contains temp. buffers */
  return config ? svn_config__shallow_copy(config, pool) : NULL;
}

/* Return a memory buffer structure allocated in POOL and containing the
 * data from CHECKSUM.
 */
static svn_membuf_t *
checksum_as_key(svn_checksum_t *checksum,
                apr_pool_t *pool)
{
  svn_membuf_t *result = apr_pcalloc(pool, sizeof(*result));
  apr_size_t size = svn_checksum_size(checksum);

  svn_membuf__create(result, size, pool);
  result->size = size; /* exact length is required! */
  memcpy(result->data, checksum->digest, size);

  return result;
}

/* Copy the configuration from the wrapper in SOURCE to the wrapper in
 * *TARGET with the case sensitivity flag in *BATON selecting the config
 * to copy.  This is usually done to add the missing case-(in)-sensitive
 * variant.  Since we must hold all data in *TARGET from the same POOL,
 * a deep copy is required.
 */
static svn_error_t *
setter(void **target,
       void *source,
       void *baton,
       apr_pool_t *pool)
{
  svn_boolean_t *case_sensitive = baton;
  config_object_t *target_cfg = *(config_object_t **)target;
  config_object_t *source_cfg = source;

  /* Maybe, we created a variant with different case sensitivity? */
  if (*case_sensitive && target_cfg->cs_cfg == NULL)
    {
      SVN_ERR(svn_config_dup(&target_cfg->cs_cfg, source_cfg->cs_cfg, pool));
      svn_config__set_read_only(target_cfg->cs_cfg, pool);
    }
  else if (!*case_sensitive && target_cfg->ci_cfg == NULL)
    {
      SVN_ERR(svn_config_dup(&target_cfg->ci_cfg, source_cfg->ci_cfg, pool));
      svn_config__set_read_only(target_cfg->ci_cfg, pool);
    }

  return SVN_NO_ERROR;
}

/* Set *CFG to the configuration passed in as text in CONTENTS and *KEY to
 * the corresponding object pool key.  If no such configuration exists in
 * CONFIG_POOL, yet, parse CONTENTS and cache the result.  CASE_SENSITIVE
 * controls option and section name matching.
 *
 * RESULT_POOL determines the lifetime of the returned reference and
 * SCRATCH_POOL is being used for temporary allocations.
 */
static svn_error_t *
auto_parse(svn_config_t **cfg,
           svn_membuf_t **key,
           svn_repos__config_pool_t *config_pool,
           svn_stringbuf_t *contents,
           svn_boolean_t case_sensitive,
           apr_pool_t *result_pool,
           apr_pool_t *scratch_pool)
{
  svn_checksum_t *checksum;
  config_object_t *config_object;
  apr_pool_t *cfg_pool;

  /* calculate SHA1 over the whole file contents */
  SVN_ERR(svn_stream_close
              (svn_stream_checksummed2
                  (svn_stream_from_stringbuf(contents, scratch_pool),
                   &checksum, NULL, svn_checksum_sha1, TRUE, scratch_pool)));

  /* return reference to suitable config object if that already exists */
  *key = checksum_as_key(checksum, result_pool);
  SVN_ERR(svn_object_pool__lookup((void **)cfg, config_pool->object_pool,
                                  *key, &case_sensitive, result_pool));
  if (*cfg)
    return SVN_NO_ERROR;

  /* create a pool for the new config object and parse the data into it  */
  cfg_pool = svn_object_pool__new_wrapper_pool(config_pool->object_pool);

  config_object = apr_pcalloc(cfg_pool, sizeof(*config_object));

  SVN_ERR(svn_config_parse(case_sensitive ? &config_object->cs_cfg
                                          : &config_object->ci_cfg,
                           svn_stream_from_stringbuf(contents, scratch_pool),
                           case_sensitive, case_sensitive, cfg_pool));

  /* switch config data to r/o mode to guarantee thread-safe access */
  svn_config__set_read_only(case_sensitive ? config_object->cs_cfg
                                           : config_object->ci_cfg,
                            cfg_pool);

  /* add config in pool, handle loads races and return the right config */
  SVN_ERR(svn_object_pool__insert((void **)cfg, config_pool->object_pool,
                                  *key, config_object, &case_sensitive,
                                  cfg_pool, result_pool));

  return SVN_NO_ERROR;
}

/* Store a URL@REVISION to CHECKSUM, REPOS_ROOT in CONFIG_POOL.
 */
static svn_error_t *
add_checksum(svn_repos__config_pool_t *config_pool,
             const char *url,
             const char *repos_root,
             svn_revnum_t revision,
             svn_checksum_t *checksum)
{
  apr_size_t path_len = strlen(url);
  apr_pool_t *pool = config_pool->in_repo_hash_pool;
  in_repo_config_t *config = apr_hash_get(config_pool->in_repo_configs,
                                          url, path_len);
  if (config)
    {
      /* update the existing entry */
      memcpy((void *)config->key->digest, checksum->digest,
             svn_checksum_size(checksum));
      config->revision = revision;

      /* duplicate the string only if necessary */
      if (strcmp(config->repo_root, repos_root))
        config->repo_root = apr_pstrdup(pool, repos_root);
    }
  else
    {
      /* insert a new entry.
       * Limit memory consumption by cyclically clearing pool and hash. */
      if (2 * svn_object_pool__count(config_pool->object_pool)
          < apr_hash_count(config_pool->in_repo_configs))
        {
          svn_pool_clear(pool);
          config_pool->in_repo_configs = svn_hash__make(pool);
        }

      /* construct the new entry */
      config = apr_pcalloc(pool, sizeof(*config));
      config->key = svn_checksum_dup(checksum, pool);
      config->url = apr_pstrmemdup(pool, url, path_len);
      config->repo_root = apr_pstrdup(pool, repos_root);
      config->revision = revision;

      /* add to index */
      apr_hash_set(config_pool->in_repo_configs, url, path_len, config);
    }

  return SVN_NO_ERROR;
}

/* Set *CFG to the configuration stored in URL@HEAD and cache it in
 * CONFIG_POOL.  CASE_SENSITIVE controls
 * option and section name matching.  If PREFERRED_REPOS is given,
 * use that if it also matches URL.
 *
 * RESULT_POOL determines the lifetime of the returned reference and
 * SCRATCH_POOL is being used for temporary allocations.
 */
static svn_error_t *
find_repos_config(svn_config_t **cfg,
                  svn_membuf_t **key,
                  svn_repos__config_pool_t *config_pool,
                  const char *url,
                  svn_boolean_t case_sensitive,
                  svn_repos_t *preferred_repos,
                  apr_pool_t *result_pool,
                  apr_pool_t *scratch_pool)
{
  svn_repos_t *repos = NULL;
  svn_fs_t *fs;
  svn_fs_root_t *root;
  svn_revnum_t youngest_rev;
  svn_node_kind_t node_kind;
  const char *dirent;
  svn_stream_t *stream;
  const char *fs_path;
  const char *repos_root_dirent;
  svn_checksum_t *checksum;
  svn_stringbuf_t *contents;

  *cfg = NULL;
  SVN_ERR(svn_uri_get_dirent_from_file_url(&dirent, url, scratch_pool));

  /* maybe we can use the preferred repos instance instead of creating a
   * new one */
  if (preferred_repos)
    {
      repos_root_dirent = svn_repos_path(preferred_repos, scratch_pool);
      if (!svn_dirent_is_absolute(repos_root_dirent))
        SVN_ERR(svn_dirent_get_absolute(&repos_root_dirent,
                                        repos_root_dirent,
                                        scratch_pool));

      if (svn_dirent_is_ancestor(repos_root_dirent, dirent))
        repos = preferred_repos;
    }

  /* open repos if no suitable preferred repos was provided. */
  if (!repos)
    {
      /* Search for a repository in the full path. */
      repos_root_dirent = svn_repos_find_root_path(dirent, scratch_pool);

      /* Attempt to open a repository at repos_root_dirent. */
      SVN_ERR(svn_repos_open3(&repos, repos_root_dirent, NULL,
                              scratch_pool, scratch_pool));
    }

  fs_path = &dirent[strlen(repos_root_dirent)];

  /* Get the filesystem. */
  fs = svn_repos_fs(repos);

  /* Find HEAD and the revision root */
  SVN_ERR(svn_fs_youngest_rev(&youngest_rev, fs, scratch_pool));
  SVN_ERR(svn_fs_revision_root(&root, fs, youngest_rev, scratch_pool));

  /* Fetch checksum and see whether we already have a matching config */
  SVN_ERR(svn_fs_file_checksum(&checksum, svn_checksum_sha1, root, fs_path,
                               FALSE, scratch_pool));
  if (checksum)
    {
      *key = checksum_as_key(checksum, scratch_pool);
      SVN_ERR(svn_object_pool__lookup((void **)cfg, config_pool->object_pool,
                                      *key, &case_sensitive, result_pool));
    }

  /* not parsed, yet? */
  if (!*cfg)
    {
      svn_filesize_t length;

      /* fetch the file contents */
      SVN_ERR(svn_fs_check_path(&node_kind, root, fs_path, scratch_pool));
      if (node_kind != svn_node_file)
        return SVN_NO_ERROR;

      SVN_ERR(svn_fs_file_length(&length, root, fs_path, scratch_pool));
      SVN_ERR(svn_fs_file_contents(&stream, root, fs_path, scratch_pool));
      SVN_ERR(svn_stringbuf_from_stream(&contents, stream,
                                        (apr_size_t)length, scratch_pool));

      /* handle it like ordinary file contents and cache it */
      SVN_ERR(auto_parse(cfg, key, config_pool, contents, case_sensitive,
                         result_pool, scratch_pool));
    }

  /* store the (path,rev) -> checksum mapping as well */
  if (*cfg && checksum)
    SVN_MUTEX__WITH_LOCK(svn_object_pool__mutex(config_pool->object_pool),
                         add_checksum(config_pool, url, repos_root_dirent,
                                      youngest_rev, checksum));

  return SVN_NO_ERROR;
}

/* Given the URL, search the CONFIG_POOL for an entry that maps it URL to
 * a content checksum and is still up-to-date.  If this could be found,
 * return the object's *KEY.  Use POOL for allocations.
 *
 * Requires external serialization on CONFIG_POOL.
 *
 * Note that this is only the URL(+rev) -> Checksum lookup and does not
 * guarantee that there is actually a config object available for *KEY.
 */
static svn_error_t *
key_by_url(svn_membuf_t **key,
           svn_repos__config_pool_t *config_pool,
           const char *url,
           apr_pool_t *pool)
{
  svn_error_t *err;
  svn_stringbuf_t *contents;
  apr_int64_t current;

  /* hash lookup url -> sha1 -> config */
  in_repo_config_t *config = svn_hash_gets(config_pool->in_repo_configs, url);
  *key = NULL;
  if (!config)
    return SVN_NO_ERROR;

  /* found *some* reference to a configuration.
   * Verify that it is still current.  Will fail for BDB repos. */
  err = svn_stringbuf_from_file2(&contents,
                                 svn_dirent_join(config->repo_root,
                                                 "db/current", pool),
                                 pool);
  if (!err)
    err = svn_cstring_atoi64(&current, contents->data);

  if (err)
    svn_error_clear(err);
  else if (current == config->revision)
    *key = checksum_as_key(config->key, pool);

  return SVN_NO_ERROR;
}

/* API implementation */

svn_error_t *
svn_repos__config_pool_create(svn_repos__config_pool_t **config_pool,
                              svn_boolean_t thread_safe,
                              apr_pool_t *pool)
{
  svn_repos__config_pool_t *result;
  svn_object_pool__t *object_pool;

  SVN_ERR(svn_object_pool__create(&object_pool, getter, setter,
                                  thread_safe, pool));

  /* construct the config pool in our private ROOT_POOL to survive POOL
   * cleanup and to prevent threading issues with the allocator */
  result = apr_pcalloc(pool, sizeof(*result));

  result->object_pool = object_pool;
  result->in_repo_hash_pool = svn_pool_create(pool);
  result->in_repo_configs = svn_hash__make(result->in_repo_hash_pool);

  *config_pool = result;
  return SVN_NO_ERROR;
}

svn_error_t *
svn_repos__config_pool_get(svn_config_t **cfg,
                           svn_membuf_t **key,
                           svn_repos__config_pool_t *config_pool,
                           const char *path,
                           svn_boolean_t must_exist,
                           svn_boolean_t case_sensitive,
                           svn_repos_t *preferred_repos,
                           apr_pool_t *pool)
{
  svn_error_t *err = SVN_NO_ERROR;
  apr_pool_t *scratch_pool = svn_pool_create(pool);

  /* make sure we always have a *KEY object */
  svn_membuf_t *local_key = NULL;
  if (key == NULL)
    key = &local_key;
  else
    *key = NULL;

  if (svn_path_is_url(path))
    {
      /* Read config file from repository.
       * Attempt a quick lookup first. */
      SVN_MUTEX__WITH_LOCK(svn_object_pool__mutex(config_pool->object_pool),
                           key_by_url(key, config_pool, path, pool));
      if (*key)
        {
          SVN_ERR(svn_object_pool__lookup((void **)cfg,
                                          config_pool->object_pool,
                                          *key, &case_sensitive, pool));
          if (*cfg)
            {
              svn_pool_destroy(scratch_pool);
              return SVN_NO_ERROR;
            }
        }

      /* Read and cache the configuration.  This may fail. */
      err = find_repos_config(cfg, key, config_pool, path, case_sensitive,
                              preferred_repos, pool, scratch_pool);
      if (err || !*cfg)
        {
          /* let the standard implementation handle all the difficult cases */
          svn_error_clear(err);
          err = svn_repos__retrieve_config(cfg, path, must_exist,
                                           case_sensitive, pool);
        }
    }
  else
    {
      /* Outside of repo file.  Read it. */
      svn_stringbuf_t *contents;
      err = svn_stringbuf_from_file2(&contents, path, scratch_pool);
      if (err)
        {
          /* let the standard implementation handle all the difficult cases */
          svn_error_clear(err);
          err = svn_config_read3(cfg, path, must_exist, case_sensitive,
                                 case_sensitive, pool);
        }
      else
        {
          /* parsing and caching will always succeed */
          err = auto_parse(cfg, key, config_pool, contents, case_sensitive,
                           pool, scratch_pool);
        }
    }

  svn_pool_destroy(scratch_pool);

  return err;
}
