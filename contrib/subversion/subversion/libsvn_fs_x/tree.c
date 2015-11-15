/* tree.c : tree-like filesystem, built on DAG filesystem
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


/* The job of this layer is to take a filesystem with lots of node
   sharing going on --- the real DAG filesystem as it appears in the
   database --- and make it look and act like an ordinary tree
   filesystem, with no sharing.

   We do just-in-time cloning: you can walk from some unfinished
   transaction's root down into directories and files shared with
   committed revisions; as soon as you try to change something, the
   appropriate nodes get cloned (and parent directory entries updated)
   invisibly, behind your back.  Any other references you have to
   nodes that have been cloned by other changes, even made by other
   processes, are automatically updated to point to the right clones.  */


#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <apr_pools.h>
#include <apr_hash.h>

#include "svn_hash.h"
#include "svn_private_config.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_mergeinfo.h"
#include "svn_fs.h"
#include "svn_props.h"
#include "svn_sorts.h"

#include "fs.h"
#include "dag.h"
#include "lock.h"
#include "tree.h"
#include "fs_x.h"
#include "fs_id.h"
#include "temp_serializer.h"
#include "cached_data.h"
#include "transaction.h"
#include "pack.h"
#include "util.h"

#include "private/svn_mergeinfo_private.h"
#include "private/svn_subr_private.h"
#include "private/svn_fs_util.h"
#include "private/svn_fspath.h"
#include "../libsvn_fs/fs-loader.h"



/* The root structures.

   Why do they contain different data?  Well, transactions are mutable
   enough that it isn't safe to cache the DAG node for the root
   directory or the hash of copyfrom data: somebody else might modify
   them concurrently on disk!  (Why is the DAG node cache safer than
   the root DAG node?  When cloning transaction DAG nodes in and out
   of the cache, all of the possibly-mutable data from the
   svn_fs_x__noderev_t inside the dag_node_t is dropped.)  Additionally,
   revisions are immutable enough that their DAG node cache can be
   kept in the FS object and shared among multiple revision root
   objects.
*/
typedef dag_node_t fs_rev_root_data_t;

typedef struct fs_txn_root_data_t
{
  /* TXN_ID value from the main struct but as a struct instead of a string */
  svn_fs_x__txn_id_t txn_id;

  /* Cache of txn DAG nodes (without their nested noderevs, because
   * it's mutable). Same keys/values as ffd->rev_node_cache. */
  svn_cache__t *txn_node_cache;
} fs_txn_root_data_t;

/* Declared here to resolve the circular dependencies. */
static svn_error_t *
get_dag(dag_node_t **dag_node_p,
        svn_fs_root_t *root,
        const char *path,
        apr_pool_t *pool);

static svn_fs_root_t *
make_revision_root(svn_fs_t *fs,
                   svn_revnum_t rev,
                   apr_pool_t *result_pool);

static svn_error_t *
make_txn_root(svn_fs_root_t **root_p,
              svn_fs_t *fs,
              svn_fs_x__txn_id_t txn_id,
              svn_revnum_t base_rev,
              apr_uint32_t flags,
              apr_pool_t *result_pool);

static svn_error_t *
x_closest_copy(svn_fs_root_t **root_p,
               const char **path_p,
               svn_fs_root_t *root,
               const char *path,
               apr_pool_t *pool);


/*** Node Caching ***/

/* 1st level cache */

/* An entry in the first-level cache.  REVISION and PATH form the key that
   will ultimately be matched.
 */
typedef struct cache_entry_t
{
  /* hash value derived from PATH, REVISION.
     Used to short-circuit failed lookups. */
  apr_uint32_t hash_value;

  /* revision to which the NODE belongs */
  svn_revnum_t revision;

  /* path of the NODE */
  char *path;

  /* cached value of strlen(PATH). */
  apr_size_t path_len;

  /* the node allocated in the cache's pool. NULL for empty entries. */
  dag_node_t *node;
} cache_entry_t;

/* Number of entries in the cache.  Keep this low to keep pressure on the
   CPU caches low as well.  A binary value is most efficient.  If we walk
   a directory tree, we want enough entries to store nodes for all files
   without overwriting the nodes for the parent folder.  That way, there
   will be no unnecessary misses (except for a few random ones caused by
   hash collision).

   The actual number of instances may be higher but entries that got
   overwritten are no longer visible.
 */
enum { BUCKET_COUNT = 256 };

/* The actual cache structure.  All nodes will be allocated in POOL.
   When the number of INSERTIONS (i.e. objects created form that pool)
   exceeds a certain threshold, the pool will be cleared and the cache
   with it.
 */
struct svn_fs_x__dag_cache_t
{
  /* fixed number of (possibly empty) cache entries */
  cache_entry_t buckets[BUCKET_COUNT];

  /* pool used for all node allocation */
  apr_pool_t *pool;

  /* number of entries created from POOL since the last cleanup */
  apr_size_t insertions;

  /* Property lookups etc. have a very high locality (75% re-hit).
     Thus, remember the last hit location for optimistic lookup. */
  apr_size_t last_hit;

  /* Position of the last bucket hit that actually had a DAG node in it.
     LAST_HIT may refer to a bucket that matches path@rev but has not
     its NODE element set, yet.
     This value is a mere hint for optimistic lookup and any value is
     valid (as long as it is < BUCKET_COUNT). */
  apr_size_t last_non_empty;
};

svn_fs_x__dag_cache_t*
svn_fs_x__create_dag_cache(apr_pool_t *result_pool)
{
  svn_fs_x__dag_cache_t *result = apr_pcalloc(result_pool, sizeof(*result));
  result->pool = svn_pool_create(result_pool);

  return result;
}

/* Clears the CACHE at regular intervals (destroying all cached nodes)
 */
static void
auto_clear_dag_cache(svn_fs_x__dag_cache_t* cache)
{
  if (cache->insertions > BUCKET_COUNT)
    {
      svn_pool_clear(cache->pool);

      memset(cache->buckets, 0, sizeof(cache->buckets));
      cache->insertions = 0;
    }
}

/* For the given REVISION and PATH, return the respective entry in CACHE.
   If the entry is empty, its NODE member will be NULL and the caller
   may then set it to the corresponding DAG node allocated in CACHE->POOL.
 */
static cache_entry_t *
cache_lookup( svn_fs_x__dag_cache_t *cache
            , svn_revnum_t revision
            , const char *path)
{
  apr_size_t i, bucket_index;
  apr_size_t path_len = strlen(path);
  apr_uint32_t hash_value = (apr_uint32_t)revision;

#if SVN_UNALIGNED_ACCESS_IS_OK
  /* "randomizing" / distributing factor used in our hash function */
  const apr_uint32_t factor = 0xd1f3da69;
#endif

  /* optimistic lookup: hit the same bucket again? */
  cache_entry_t *result = &cache->buckets[cache->last_hit];
  if (   (result->revision == revision)
      && (result->path_len == path_len)
      && !memcmp(result->path, path, path_len))
    {
      /* Remember the position of the last node we found in this cache. */
      if (result->node)
        cache->last_non_empty = cache->last_hit;

      return result;
    }

  /* need to do a full lookup.  Calculate the hash value
     (HASH_VALUE has been initialized to REVISION). */
  i = 0;
#if SVN_UNALIGNED_ACCESS_IS_OK
  /* We relax the dependency chain between iterations by processing
     two chunks from the input per hash_value self-multiplication.
     The HASH_VALUE update latency is now 1 MUL latency + 1 ADD latency
     per 2 chunks instead of 1 chunk.
   */
  for (; i + 8 <= path_len; i += 8)
    hash_value = hash_value * factor * factor
               + (  *(const apr_uint32_t*)(path + i) * factor
                  + *(const apr_uint32_t*)(path + i + 4));
#endif

  for (; i < path_len; ++i)
    /* Help GCC to minimize the HASH_VALUE update latency by splitting the
       MUL 33 of the naive implementation: h = h * 33 + path[i].  This
       shortens the dependency chain from 1 shift + 2 ADDs to 1 shift + 1 ADD.
     */
    hash_value = hash_value * 32 + (hash_value + (unsigned char)path[i]);

  bucket_index = hash_value + (hash_value >> 16);
  bucket_index = (bucket_index + (bucket_index >> 8)) % BUCKET_COUNT;

  /* access the corresponding bucket and remember its location */
  result = &cache->buckets[bucket_index];
  cache->last_hit = bucket_index;

  /* if it is *NOT* a match,  clear the bucket, expect the caller to fill
     in the node and count it as an insertion */
  if (   (result->hash_value != hash_value)
      || (result->revision != revision)
      || (result->path_len != path_len)
      || memcmp(result->path, path, path_len))
    {
      result->hash_value = hash_value;
      result->revision = revision;
      if (result->path_len < path_len)
        result->path = apr_palloc(cache->pool, path_len + 1);
      result->path_len = path_len;
      memcpy(result->path, path, path_len + 1);

      result->node = NULL;

      cache->insertions++;
    }
  else if (result->node)
    {
      /* This bucket is valid & has a suitable DAG node in it.
         Remember its location. */
      cache->last_non_empty = bucket_index;
    }

  return result;
}

/* Optimistic lookup using the last seen non-empty location in CACHE.
   Return the node of that entry, if it is still in use and matches PATH.
   Return NULL otherwise.  Since the caller usually already knows the path
   length, provide it in PATH_LEN. */
static dag_node_t *
cache_lookup_last_path(svn_fs_x__dag_cache_t *cache,
                       const char *path,
                       apr_size_t path_len)
{
  cache_entry_t *result = &cache->buckets[cache->last_non_empty];
  assert(strlen(path) == path_len);

  if (   result->node
      && (result->path_len == path_len)
      && !memcmp(result->path, path, path_len))
    {
      return result->node;
    }

  return NULL;
}

/* 2nd level cache */

/* Find and return the DAG node cache for ROOT and the key that
   should be used for PATH.

   RESULT_POOL will only be used for allocating a new keys if necessary. */
static void
locate_cache(svn_cache__t **cache,
             const char **key,
             svn_fs_root_t *root,
             const char *path,
             apr_pool_t *result_pool)
{
  if (root->is_txn_root)
    {
      fs_txn_root_data_t *frd = root->fsap_data;

      if (cache)
        *cache = frd->txn_node_cache;
      if (key && path)
        *key = path;
    }
  else
    {
      svn_fs_x__data_t *ffd = root->fs->fsap_data;

      if (cache)
        *cache = ffd->rev_node_cache;
      if (key && path)
        *key = svn_fs_x__combine_number_and_string(root->rev, path,
                                                   result_pool);
    }
}

/* Return NODE for PATH from ROOT's node cache, or NULL if the node
   isn't cached; read it from the FS. *NODE remains valid until either
   POOL or the FS gets cleared or destroyed (whichever comes first).
 */
static svn_error_t *
dag_node_cache_get(dag_node_t **node_p,
                   svn_fs_root_t *root,
                   const char *path,
                   apr_pool_t *pool)
{
  svn_boolean_t found;
  dag_node_t *node = NULL;
  svn_cache__t *cache;
  const char *key;

  SVN_ERR_ASSERT(*path == '/');

  if (!root->is_txn_root)
    {
      /* immutable DAG node. use the global caches for it */

      svn_fs_x__data_t *ffd = root->fs->fsap_data;
      cache_entry_t *bucket;

      auto_clear_dag_cache(ffd->dag_node_cache);
      bucket = cache_lookup(ffd->dag_node_cache, root->rev, path);
      if (bucket->node == NULL)
        {
          locate_cache(&cache, &key, root, path, pool);
          SVN_ERR(svn_cache__get((void **)&node, &found, cache, key,
                                 ffd->dag_node_cache->pool));
          if (found && node)
            {
              /* Patch up the FS, since this might have come from an old FS
              * object. */
              svn_fs_x__dag_set_fs(node, root->fs);
              bucket->node = node;
            }
        }
      else
        {
          node = bucket->node;
        }
    }
  else
    {
      /* DAG is mutable / may become invalid. Use the TXN-local cache */

      locate_cache(&cache, &key, root, path, pool);

      SVN_ERR(svn_cache__get((void **) &node, &found, cache, key, pool));
      if (found && node)
        {
          /* Patch up the FS, since this might have come from an old FS
          * object. */
          svn_fs_x__dag_set_fs(node, root->fs);
        }
    }

  *node_p = node;

  return SVN_NO_ERROR;
}


/* Add the NODE for PATH to ROOT's node cache. */
static svn_error_t *
dag_node_cache_set(svn_fs_root_t *root,
                   const char *path,
                   dag_node_t *node,
                   apr_pool_t *scratch_pool)
{
  svn_cache__t *cache;
  const char *key;

  SVN_ERR_ASSERT(*path == '/');

  /* Do *not* attempt to dup and put the node into L1.
   * dup() is twice as expensive as an L2 lookup (which will set also L1).
   */
  locate_cache(&cache, &key, root, path, scratch_pool);

  return svn_cache__set(cache, key, node, scratch_pool);
}


/* Baton for find_descendants_in_cache. */
typedef struct fdic_baton_t
{
  const char *path;
  apr_array_header_t *list;
  apr_pool_t *pool;
} fdic_baton_t;

/* If the given item is a descendant of BATON->PATH, push
 * it onto BATON->LIST (copying into BATON->POOL).  Implements
 * the svn_iter_apr_hash_cb_t prototype. */
static svn_error_t *
find_descendants_in_cache(void *baton,
                          const void *key,
                          apr_ssize_t klen,
                          void *val,
                          apr_pool_t *pool)
{
  fdic_baton_t *b = baton;
  const char *item_path = key;

  if (svn_fspath__skip_ancestor(b->path, item_path))
    APR_ARRAY_PUSH(b->list, const char *) = apr_pstrdup(b->pool, item_path);

  return SVN_NO_ERROR;
}

/* Invalidate cache entries for PATH and any of its children.  This
   should *only* be called on a transaction root! */
static svn_error_t *
dag_node_cache_invalidate(svn_fs_root_t *root,
                          const char *path,
                          apr_pool_t *scratch_pool)
{
  fdic_baton_t b;
  svn_cache__t *cache;
  apr_pool_t *iterpool;
  int i;

  b.path = path;
  b.pool = svn_pool_create(scratch_pool);
  b.list = apr_array_make(b.pool, 1, sizeof(const char *));

  SVN_ERR_ASSERT(root->is_txn_root);
  locate_cache(&cache, NULL, root, NULL, b.pool);


  SVN_ERR(svn_cache__iter(NULL, cache, find_descendants_in_cache,
                          &b, b.pool));

  iterpool = svn_pool_create(b.pool);

  for (i = 0; i < b.list->nelts; i++)
    {
      const char *descendant = APR_ARRAY_IDX(b.list, i, const char *);
      svn_pool_clear(iterpool);
      SVN_ERR(svn_cache__set(cache, descendant, NULL, iterpool));
    }

  svn_pool_destroy(iterpool);
  svn_pool_destroy(b.pool);
  return SVN_NO_ERROR;
}



/* Creating transaction and revision root nodes.  */

svn_error_t *
svn_fs_x__txn_root(svn_fs_root_t **root_p,
                   svn_fs_txn_t *txn,
                   apr_pool_t *pool)
{
  apr_uint32_t flags = 0;
  apr_hash_t *txnprops;

  /* Look for the temporary txn props representing 'flags'. */
  SVN_ERR(svn_fs_x__txn_proplist(&txnprops, txn, pool));
  if (txnprops)
    {
      if (svn_hash_gets(txnprops, SVN_FS__PROP_TXN_CHECK_OOD))
        flags |= SVN_FS_TXN_CHECK_OOD;

      if (svn_hash_gets(txnprops, SVN_FS__PROP_TXN_CHECK_LOCKS))
        flags |= SVN_FS_TXN_CHECK_LOCKS;
    }

  return make_txn_root(root_p, txn->fs, svn_fs_x__txn_get_id(txn),
                       txn->base_rev, flags, pool);
}


svn_error_t *
svn_fs_x__revision_root(svn_fs_root_t **root_p,
                        svn_fs_t *fs,
                        svn_revnum_t rev,
                        apr_pool_t *pool)
{
  SVN_ERR(svn_fs__check_fs(fs, TRUE));
  SVN_ERR(svn_fs_x__ensure_revision_exists(rev, fs, pool));

  *root_p = make_revision_root(fs, rev, pool);

  return SVN_NO_ERROR;
}



/* Getting dag nodes for roots.  */

/* Return the transaction ID to a given transaction ROOT. */
static svn_fs_x__txn_id_t
root_txn_id(svn_fs_root_t *root)
{
  fs_txn_root_data_t *frd = root->fsap_data;
  assert(root->is_txn_root);

  return frd->txn_id;
}

/* Set *NODE_P to a freshly opened dag node referring to the root
   directory of ROOT, allocating from RESULT_POOL.  Use SCRATCH_POOL
   for temporary allocations.  */
static svn_error_t *
root_node(dag_node_t **node_p,
          svn_fs_root_t *root,
          apr_pool_t *result_pool,
          apr_pool_t *scratch_pool)
{
  if (root->is_txn_root)
    {
      /* It's a transaction root.  Open a fresh copy.  */
      return svn_fs_x__dag_txn_root(node_p, root->fs, root_txn_id(root),
                                    result_pool, scratch_pool);
    }
  else
    {
      /* It's a revision root, so we already have its root directory
         opened.  */
      return svn_fs_x__dag_revision_root(node_p, root->fs, root->rev,
                                         result_pool, scratch_pool);
    }
}


/* Set *NODE_P to a mutable root directory for ROOT, cloning if
   necessary, allocating in RESULT_POOL.  ROOT must be a transaction root.
   Use ERROR_PATH in error messages.  Use SCRATCH_POOL for temporaries.*/
static svn_error_t *
mutable_root_node(dag_node_t **node_p,
                  svn_fs_root_t *root,
                  const char *error_path,
                  apr_pool_t *result_pool,
                  apr_pool_t *scratch_pool)
{
  if (root->is_txn_root)
    {
      /* It's a transaction root.  Open a fresh copy.  */
      return svn_fs_x__dag_txn_root(node_p, root->fs, root_txn_id(root),
                                    result_pool, scratch_pool);
    }
  else
    /* If it's not a transaction root, we can't change its contents.  */
    return SVN_FS__ERR_NOT_MUTABLE(root->fs, root->rev, error_path);
}



/* Traversing directory paths.  */

typedef enum copy_id_inherit_t
{
  copy_id_inherit_unknown = 0,
  copy_id_inherit_self,
  copy_id_inherit_parent,
  copy_id_inherit_new

} copy_id_inherit_t;

/* A linked list representing the path from a node up to a root
   directory.  We use this for cloning, and for operations that need
   to deal with both a node and its parent directory.  For example, a
   `delete' operation needs to know that the node actually exists, but
   also needs to change the parent directory.  */
typedef struct parent_path_t
{

  /* A node along the path.  This could be the final node, one of its
     parents, or the root.  Every parent path ends with an element for
     the root directory.  */
  dag_node_t *node;

  /* The name NODE has in its parent directory.  This is zero for the
     root directory, which (obviously) has no name in its parent.  */
  char *entry;

  /* The parent of NODE, or zero if NODE is the root directory.  */
  struct parent_path_t *parent;

  /* The copy ID inheritance style. */
  copy_id_inherit_t copy_inherit;

  /* If copy ID inheritance style is copy_id_inherit_new, this is the
     path which should be implicitly copied; otherwise, this is NULL. */
  const char *copy_src_path;

} parent_path_t;

/* Return a text string describing the absolute path of parent_path
   PARENT_PATH.  It will be allocated in POOL. */
static const char *
parent_path_path(parent_path_t *parent_path,
                 apr_pool_t *pool)
{
  const char *path_so_far = "/";
  if (parent_path->parent)
    path_so_far = parent_path_path(parent_path->parent, pool);
  return parent_path->entry
    ? svn_fspath__join(path_so_far, parent_path->entry, pool)
    : path_so_far;
}


/* Return the FS path for the parent path chain object CHILD relative
   to its ANCESTOR in the same chain, allocated in POOL.  */
static const char *
parent_path_relpath(parent_path_t *child,
                    parent_path_t *ancestor,
                    apr_pool_t *pool)
{
  const char *path_so_far = "";
  parent_path_t *this_node = child;
  while (this_node != ancestor)
    {
      assert(this_node != NULL);
      path_so_far = svn_relpath_join(this_node->entry, path_so_far, pool);
      this_node = this_node->parent;
    }
  return path_so_far;
}



/* Choose a copy ID inheritance method *INHERIT_P to be used in the
   event that immutable node CHILD in FS needs to be made mutable.  If
   the inheritance method is copy_id_inherit_new, also return a
   *COPY_SRC_PATH on which to base the new copy ID (else return NULL
   for that path).  CHILD must have a parent (it cannot be the root
   node).  Allocations are taken from POOL. */
static svn_error_t *
get_copy_inheritance(copy_id_inherit_t *inherit_p,
                     const char **copy_src_path,
                     svn_fs_t *fs,
                     parent_path_t *child,
                     apr_pool_t *pool)
{
  svn_fs_x__id_t child_copy_id, parent_copy_id;
  svn_boolean_t related;
  const char *id_path = NULL;
  svn_fs_root_t *copyroot_root;
  dag_node_t *copyroot_node;
  svn_revnum_t copyroot_rev;
  const char *copyroot_path;

  SVN_ERR_ASSERT(child && child->parent);

  /* Initialize some convenience variables. */
  SVN_ERR(svn_fs_x__dag_get_copy_id(&child_copy_id, child->node));
  SVN_ERR(svn_fs_x__dag_get_copy_id(&parent_copy_id, child->parent->node));

  /* If this child is already mutable, we have nothing to do. */
  if (svn_fs_x__dag_check_mutable(child->node))
    {
      *inherit_p = copy_id_inherit_self;
      *copy_src_path = NULL;
      return SVN_NO_ERROR;
    }

  /* From this point on, we'll assume that the child will just take
     its copy ID from its parent. */
  *inherit_p = copy_id_inherit_parent;
  *copy_src_path = NULL;

  /* Special case: if the child's copy ID is '0', use the parent's
     copy ID. */
  if (svn_fs_x__id_is_root(&child_copy_id))
    return SVN_NO_ERROR;

  /* Compare the copy IDs of the child and its parent.  If they are
     the same, then the child is already on the same branch as the
     parent, and should use the same mutability copy ID that the
     parent will use. */
  if (svn_fs_x__id_eq(&child_copy_id, &parent_copy_id))
    return SVN_NO_ERROR;

  /* If the child is on the same branch that the parent is on, the
     child should just use the same copy ID that the parent would use.
     Else, the child needs to generate a new copy ID to use should it
     need to be made mutable.  We will claim that child is on the same
     branch as its parent if the child itself is not a branch point,
     or if it is a branch point that we are accessing via its original
     copy destination path. */
  SVN_ERR(svn_fs_x__dag_get_copyroot(&copyroot_rev, &copyroot_path,
                                     child->node));
  SVN_ERR(svn_fs_x__revision_root(&copyroot_root, fs, copyroot_rev, pool));
  SVN_ERR(get_dag(&copyroot_node, copyroot_root, copyroot_path, pool));

  SVN_ERR(svn_fs_x__dag_related_node(&related, copyroot_node, child->node));
  if (!related)
    return SVN_NO_ERROR;

  /* Determine if we are looking at the child via its original path or
     as a subtree item of a copied tree. */
  id_path = svn_fs_x__dag_get_created_path(child->node);
  if (strcmp(id_path, parent_path_path(child, pool)) == 0)
    {
      *inherit_p = copy_id_inherit_self;
      return SVN_NO_ERROR;
    }

  /* We are pretty sure that the child node is an unedited nested
     branched node.  When it needs to be made mutable, it should claim
     a new copy ID. */
  *inherit_p = copy_id_inherit_new;
  *copy_src_path = id_path;
  return SVN_NO_ERROR;
}

/* Allocate a new parent_path_t node from RESULT_POOL, referring to NODE,
   ENTRY, PARENT, and COPY_ID.  */
static parent_path_t *
make_parent_path(dag_node_t *node,
                 char *entry,
                 parent_path_t *parent,
                 apr_pool_t *result_pool)
{
  parent_path_t *parent_path = apr_pcalloc(result_pool, sizeof(*parent_path));
  if (node)
    parent_path->node = svn_fs_x__dag_copy_into_pool(node, result_pool);
  parent_path->entry = entry;
  parent_path->parent = parent;
  parent_path->copy_inherit = copy_id_inherit_unknown;
  parent_path->copy_src_path = NULL;
  return parent_path;
}


/* Flags for open_path.  */
typedef enum open_path_flags_t {

  /* The last component of the PATH need not exist.  (All parent
     directories must exist, as usual.)  If the last component doesn't
     exist, simply leave the `node' member of the bottom parent_path
     component zero.  */
  open_path_last_optional = 1,

  /* When this flag is set, don't bother to lookup the DAG node in
     our caches because we already tried this.  Ignoring this flag
     has no functional impact.  */
  open_path_uncached = 2,

  /* The caller does not care about the parent node chain but only
     the final DAG node. */
  open_path_node_only = 4,

  /* The caller wants a NULL path object instead of an error if the
     path cannot be found. */
  open_path_allow_null = 8
} open_path_flags_t;

/* Try a short-cut for the open_path() function using the last node accessed.
 * If that ROOT is that nodes's "created rev" and PATH of PATH_LEN chars is
 * its "created path", return the node in *NODE_P.  Set it to NULL otherwise.
 *
 * This function is used to support ra_serf-style access patterns where we
 * are first asked for path@rev and then for path@c_rev of the same node.
 * The shortcut works by ignoring the "rev" part of the cache key and then
 * checking whether we got lucky.  Lookup and verification are both quick
 * plus there are many early outs for common types of mismatch.
 */
static svn_error_t *
try_match_last_node(dag_node_t **node_p,
                    svn_fs_root_t *root,
                    const char *path,
                    apr_size_t path_len,
                    apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = root->fs->fsap_data;

  /* Optimistic lookup: if the last node returned from the cache applied to
     the same PATH, return it in NODE. */
  dag_node_t *node
    = cache_lookup_last_path(ffd->dag_node_cache, path, path_len);

  /* Did we get a bucket with a committed node? */
  if (node && !svn_fs_x__dag_check_mutable(node))
    {
      /* Get the path&rev pair at which this node was created.
         This is repository location for which this node is _known_ to be
         the right lookup result irrespective of how we found it. */
      const char *created_path
        = svn_fs_x__dag_get_created_path(node);
      svn_revnum_t revision = svn_fs_x__dag_get_revision(node);

      /* Is it an exact match? */
      if (revision == root->rev && strcmp(created_path, path) == 0)
        {
          /* Cache it under its full path@rev access path. */
          SVN_ERR(dag_node_cache_set(root, path, node, scratch_pool));

          *node_p = node;
          return SVN_NO_ERROR;
        }
    }

  *node_p = NULL;
  return SVN_NO_ERROR;
}


/* Open the node identified by PATH in ROOT, allocating in POOL.  Set
   *PARENT_PATH_P to a path from the node up to ROOT.  The resulting
   **PARENT_PATH_P value is guaranteed to contain at least one
   *element, for the root directory.  PATH must be in canonical form.

   If resulting *PARENT_PATH_P will eventually be made mutable and
   modified, or if copy ID inheritance information is otherwise needed,
   IS_TXN_PATH must be set.  If IS_TXN_PATH is FALSE, no copy ID
   inheritance information will be calculated for the *PARENT_PATH_P chain.

   If FLAGS & open_path_last_optional is zero, return the error
   SVN_ERR_FS_NOT_FOUND if the node PATH refers to does not exist.  If
   non-zero, require all the parent directories to exist as normal,
   but if the final path component doesn't exist, simply return a path
   whose bottom `node' member is zero.  This option is useful for
   callers that create new nodes --- we find the parent directory for
   them, and tell them whether the entry exists already.

   The remaining bits in FLAGS are hints that allow this function
   to take shortcuts based on knowledge that the caller provides,
   such as the caller is not actually being interested in PARENT_PATH_P,
   but only in (*PARENT_PATH_P)->NODE.

   NOTE: Public interfaces which only *read* from the filesystem
   should not call this function directly, but should instead use
   get_dag().
*/
static svn_error_t *
open_path(parent_path_t **parent_path_p,
          svn_fs_root_t *root,
          const char *path,
          int flags,
          svn_boolean_t is_txn_path,
          apr_pool_t *pool)
{
  svn_fs_t *fs = root->fs;
  dag_node_t *here = NULL; /* The directory we're currently looking at.  */
  parent_path_t *parent_path; /* The path from HERE up to the root. */
  const char *rest = NULL; /* The portion of PATH we haven't traversed yet. */
  apr_pool_t *iterpool = svn_pool_create(pool);

  /* path to the currently processed entry without trailing '/'.
     We will reuse this across iterations by simply putting a NUL terminator
     at the respective position and replacing that with a '/' in the next
     iteration.  This is correct as we assert() PATH to be canonical. */
  svn_stringbuf_t *path_so_far = svn_stringbuf_create(path, pool);
  apr_size_t path_len = path_so_far->len;

  /* Callers often traverse the DAG in some path-based order or along the
     history segments.  That allows us to try a few guesses about where to
     find the next item.  This is only useful if the caller didn't request
     the full parent chain. */
  assert(svn_fs__is_canonical_abspath(path));
  path_so_far->len = 0; /* "" */
  if (flags & open_path_node_only)
    {
      const char *directory;

      /* First attempt: Assume that we access the DAG for the same path as
         in the last lookup but for a different revision that happens to be
         the last revision that touched the respective node.  This is a
         common pattern when e.g. checking out over ra_serf.  Note that this
         will only work for committed data as the revision info for nodes in
         txns is bogus.

         This shortcut is quick and will exit this function upon success.
         So, try it first. */
      if (!root->is_txn_root)
        {
          dag_node_t *node;
          SVN_ERR(try_match_last_node(&node, root, path, path_len, iterpool));

          /* Did the shortcut work? */
          if (node)
            {
              /* Construct and return the result. */
              svn_pool_destroy(iterpool);

              parent_path = make_parent_path(node, 0, 0, pool);
              parent_path->copy_inherit = copy_id_inherit_self;
              *parent_path_p = parent_path;

              return SVN_NO_ERROR;
            }
        }

      /* Second attempt: Try starting the lookup immediately at the parent
         node.  We will often have recently accessed either a sibling or
         said parent DIRECTORY itself for the same revision. */
      directory = svn_dirent_dirname(path, pool);
      if (directory[1] != 0) /* root nodes are covered anyway */
        {
          SVN_ERR(dag_node_cache_get(&here, root, directory, pool));

          /* Did the shortcut work? */
          if (here)
            {
              apr_size_t dirname_len = strlen(directory);
              path_so_far->len = dirname_len;
              rest = path + dirname_len + 1;
            }
        }
    }

  /* did the shortcut work? */
  if (!here)
    {
      /* Make a parent_path item for the root node, using its own current
         copy id.  */
      SVN_ERR(root_node(&here, root, pool, iterpool));
      rest = path + 1; /* skip the leading '/', it saves in iteration */
    }

  path_so_far->data[path_so_far->len] = '\0';
  parent_path = make_parent_path(here, 0, 0, pool);
  parent_path->copy_inherit = copy_id_inherit_self;

  /* Whenever we are at the top of this loop:
     - HERE is our current directory,
     - ID is the node revision ID of HERE,
     - REST is the path we're going to find in HERE, and
     - PARENT_PATH includes HERE and all its parents.  */
  for (;;)
    {
      const char *next;
      char *entry;
      dag_node_t *child;

      svn_pool_clear(iterpool);

      /* The NODE in PARENT_PATH always lives in POOL, i.e. it will
       * survive the cleanup of ITERPOOL and the DAG cache.*/
      here = parent_path->node;

      /* Parse out the next entry from the path.  */
      entry = svn_fs__next_entry_name(&next, rest, pool);

      /* Update the path traversed thus far. */
      path_so_far->data[path_so_far->len] = '/';
      path_so_far->len += strlen(entry) + 1;
      path_so_far->data[path_so_far->len] = '\0';

      /* Given the behavior of svn_fs__next_entry_name(), ENTRY may be an
         empty string when the path either starts or ends with a slash.
         In either case, we stay put: the current directory stays the
         same, and we add nothing to the parent path.  We only need to
         process non-empty path segments. */
      if (*entry != '\0')
        {
          copy_id_inherit_t inherit;
          const char *copy_path = NULL;
          dag_node_t *cached_node = NULL;

          /* If we found a directory entry, follow it.  First, we
             check our node cache, and, failing that, we hit the DAG
             layer.  Don't bother to contact the cache for the last
             element if we already know the lookup to fail for the
             complete path. */
          if (next || !(flags & open_path_uncached))
            SVN_ERR(dag_node_cache_get(&cached_node, root, path_so_far->data,
                                       pool));
          if (cached_node)
            child = cached_node;
          else
            SVN_ERR(svn_fs_x__dag_open(&child, here, entry, pool, iterpool));

          /* "file not found" requires special handling.  */
          if (child == NULL)
            {
              /* If this was the last path component, and the caller
                 said it was optional, then don't return an error;
                 just put a NULL node pointer in the path.  */

              if ((flags & open_path_last_optional)
                  && (! next || *next == '\0'))
                {
                  parent_path = make_parent_path(NULL, entry, parent_path,
                                                 pool);
                  break;
                }
              else if (flags & open_path_allow_null)
                {
                  parent_path = NULL;
                  break;
                }
              else
                {
                  /* Build a better error message than svn_fs_x__dag_open
                     can provide, giving the root and full path name.  */
                  return SVN_FS__NOT_FOUND(root, path);
                }
            }

          if (flags & open_path_node_only)
            {
              /* Shortcut: the caller only wants the final DAG node. */
              parent_path->node = svn_fs_x__dag_copy_into_pool(child, pool);
            }
          else
            {
              /* Now, make a parent_path item for CHILD. */
              parent_path = make_parent_path(child, entry, parent_path, pool);
              if (is_txn_path)
                {
                  SVN_ERR(get_copy_inheritance(&inherit, &copy_path, fs,
                                               parent_path, iterpool));
                  parent_path->copy_inherit = inherit;
                  parent_path->copy_src_path = apr_pstrdup(pool, copy_path);
                }
            }

          /* Cache the node we found (if it wasn't already cached). */
          if (! cached_node)
            SVN_ERR(dag_node_cache_set(root, path_so_far->data, child,
                                       iterpool));
        }

      /* Are we finished traversing the path?  */
      if (! next)
        break;

      /* The path isn't finished yet; we'd better be in a directory.  */
      if (svn_fs_x__dag_node_kind(child) != svn_node_dir)
        SVN_ERR_W(SVN_FS__ERR_NOT_DIRECTORY(fs, path_so_far->data),
                  apr_psprintf(iterpool, _("Failure opening '%s'"), path));

      rest = next;
    }

  svn_pool_destroy(iterpool);
  *parent_path_p = parent_path;
  return SVN_NO_ERROR;
}


/* Make the node referred to by PARENT_PATH mutable, if it isn't already,
   allocating from RESULT_POOL.  ROOT must be the root from which
   PARENT_PATH descends.  Clone any parent directories as needed.
   Adjust the dag nodes in PARENT_PATH to refer to the clones.  Use
   ERROR_PATH in error messages.  Use SCRATCH_POOL for temporaries. */
static svn_error_t *
make_path_mutable(svn_fs_root_t *root,
                  parent_path_t *parent_path,
                  const char *error_path,
                  apr_pool_t *result_pool,
                  apr_pool_t *scratch_pool)
{
  dag_node_t *clone;
  svn_fs_x__txn_id_t txn_id = root_txn_id(root);

  /* Is the node mutable already?  */
  if (svn_fs_x__dag_check_mutable(parent_path->node))
    return SVN_NO_ERROR;

  /* Are we trying to clone the root, or somebody's child node?  */
  if (parent_path->parent)
    {
      svn_fs_x__id_t copy_id = { SVN_INVALID_REVNUM, 0 };
      svn_fs_x__id_t *copy_id_ptr = &copy_id;
      copy_id_inherit_t inherit = parent_path->copy_inherit;
      const char *clone_path, *copyroot_path;
      svn_revnum_t copyroot_rev;
      svn_boolean_t is_parent_copyroot = FALSE;
      svn_fs_root_t *copyroot_root;
      dag_node_t *copyroot_node;
      svn_boolean_t related;

      /* We're trying to clone somebody's child.  Make sure our parent
         is mutable.  */
      SVN_ERR(make_path_mutable(root, parent_path->parent,
                                error_path, result_pool, scratch_pool));

      switch (inherit)
        {
        case copy_id_inherit_parent:
          SVN_ERR(svn_fs_x__dag_get_copy_id(&copy_id,
                                            parent_path->parent->node));
          break;

        case copy_id_inherit_new:
          SVN_ERR(svn_fs_x__reserve_copy_id(&copy_id, root->fs, txn_id,
                                            scratch_pool));
          break;

        case copy_id_inherit_self:
          copy_id_ptr = NULL;
          break;

        case copy_id_inherit_unknown:
        default:
          SVN_ERR_MALFUNCTION(); /* uh-oh -- somebody didn't calculate copy-ID
                      inheritance data. */
        }

      /* Determine what copyroot our new child node should use. */
      SVN_ERR(svn_fs_x__dag_get_copyroot(&copyroot_rev, &copyroot_path,
                                          parent_path->node));
      SVN_ERR(svn_fs_x__revision_root(&copyroot_root, root->fs,
                                      copyroot_rev, scratch_pool));
      SVN_ERR(get_dag(&copyroot_node, copyroot_root, copyroot_path,
                      result_pool));

      SVN_ERR(svn_fs_x__dag_related_node(&related, copyroot_node,
                                         parent_path->node));
      if (!related)
        is_parent_copyroot = TRUE;

      /* Now make this node mutable.  */
      clone_path = parent_path_path(parent_path->parent, scratch_pool);
      SVN_ERR(svn_fs_x__dag_clone_child(&clone,
                                        parent_path->parent->node,
                                        clone_path,
                                        parent_path->entry,
                                        copy_id_ptr, txn_id,
                                        is_parent_copyroot,
                                        result_pool,
                                        scratch_pool));

      /* Update the path cache. */
      SVN_ERR(dag_node_cache_set(root,
                                 parent_path_path(parent_path, scratch_pool),
                                 clone, scratch_pool));
    }
  else
    {
      /* We're trying to clone the root directory.  */
      SVN_ERR(mutable_root_node(&clone, root, error_path, result_pool,
                                scratch_pool));
    }

  /* Update the PARENT_PATH link to refer to the clone.  */
  parent_path->node = clone;

  return SVN_NO_ERROR;
}


/* Open the node identified by PATH in ROOT.  Set DAG_NODE_P to the
   node we find, allocated in POOL.  Return the error
   SVN_ERR_FS_NOT_FOUND if this node doesn't exist.
 */
static svn_error_t *
get_dag(dag_node_t **dag_node_p,
        svn_fs_root_t *root,
        const char *path,
        apr_pool_t *pool)
{
  parent_path_t *parent_path;
  dag_node_t *node = NULL;

  /* First we look for the DAG in our cache
     (if the path may be canonical). */
  if (*path == '/')
    SVN_ERR(dag_node_cache_get(&node, root, path, pool));

  if (! node)
    {
      /* Canonicalize the input PATH.  As it turns out, >95% of all paths
       * seen here during e.g. svnadmin verify are non-canonical, i.e.
       * miss the leading '/'.  Unconditional canonicalization has a net
       * performance benefit over previously checking path for being
       * canonical. */
      path = svn_fs__canonicalize_abspath(path, pool);
      SVN_ERR(dag_node_cache_get(&node, root, path, pool));

      if (! node)
        {
          /* Call open_path with no flags, as we want this to return an
           * error if the node for which we are searching doesn't exist. */
          SVN_ERR(open_path(&parent_path, root, path,
                            open_path_uncached | open_path_node_only,
                            FALSE, pool));
          node = parent_path->node;

          /* No need to cache our find -- open_path() will do that for us. */
        }
    }

  *dag_node_p = svn_fs_x__dag_copy_into_pool(node, pool);
  return SVN_NO_ERROR;
}



/* Populating the `changes' table. */

/* Add a change to the changes table in FS, keyed on transaction id
   TXN_ID, and indicated that a change of kind CHANGE_KIND occurred on
   PATH (whose node revision id is--or was, in the case of a
   deletion--NODEREV_ID), and optionally that TEXT_MODs, PROP_MODs or
   MERGEINFO_MODs occurred.  If the change resulted from a copy,
   COPYFROM_REV and COPYFROM_PATH specify under which revision and path
   the node was copied from.  If this was not part of a copy, COPYFROM_REV
   should be SVN_INVALID_REVNUM.  Use SCRATCH_POOL for temporary allocations.
 */
static svn_error_t *
add_change(svn_fs_t *fs,
           svn_fs_x__txn_id_t txn_id,
           const char *path,
           const svn_fs_x__id_t *noderev_id,
           svn_fs_path_change_kind_t change_kind,
           svn_boolean_t text_mod,
           svn_boolean_t prop_mod,
           svn_boolean_t mergeinfo_mod,
           svn_node_kind_t node_kind,
           svn_revnum_t copyfrom_rev,
           const char *copyfrom_path,
           apr_pool_t *scratch_pool)
{
  return svn_fs_x__add_change(fs, txn_id,
                              svn_fs__canonicalize_abspath(path,
                                                           scratch_pool),
                              noderev_id, change_kind,
                              text_mod, prop_mod, mergeinfo_mod,
                              node_kind, copyfrom_rev, copyfrom_path,
                              scratch_pool);
}



/* Generic node operations.  */

/* Get the id of a node referenced by path PATH in ROOT.  Return the
   id in *ID_P allocated in POOL. */
static svn_error_t *
x_node_id(const svn_fs_id_t **id_p,
          svn_fs_root_t *root,
          const char *path,
          apr_pool_t *pool)
{
  svn_fs_x__id_t noderev_id;

  if ((! root->is_txn_root)
      && (path[0] == '\0' || ((path[0] == '/') && (path[1] == '\0'))))
    {
      /* Optimize the case where we don't need any db access at all.
         The root directory ("" or "/") node is stored in the
         svn_fs_root_t object, and never changes when it's a revision
         root, so we can just reach in and grab it directly. */
      svn_fs_x__init_rev_root(&noderev_id, root->rev);
    }
  else
    {
      dag_node_t *node;

      SVN_ERR(get_dag(&node, root, path, pool));
      noderev_id = *svn_fs_x__dag_get_id(node);
    }

  *id_p = svn_fs_x__id_create(svn_fs_x__id_create_context(root->fs, pool),
                              &noderev_id, pool);

  return SVN_NO_ERROR;
}

static svn_error_t *
x_node_relation(svn_fs_node_relation_t *relation,
                svn_fs_root_t *root_a,
                const char *path_a,
                svn_fs_root_t *root_b,
                const char *path_b,
                apr_pool_t *scratch_pool)
{
  dag_node_t *node;
  svn_fs_x__id_t noderev_id_a, noderev_id_b, node_id_a, node_id_b;

  /* Root paths are a common special case. */
  svn_boolean_t a_is_root_dir
    = (path_a[0] == '\0') || ((path_a[0] == '/') && (path_a[1] == '\0'));
  svn_boolean_t b_is_root_dir
    = (path_b[0] == '\0') || ((path_b[0] == '/') && (path_b[1] == '\0'));

  /* Path from different repository are always unrelated. */
  if (root_a->fs != root_b->fs)
    {
      *relation = svn_fs_node_unrelated;
      return SVN_NO_ERROR;
    }

  /* Are both (!) root paths? Then, they are related and we only test how
   * direct the relation is. */
  if (a_is_root_dir && b_is_root_dir)
    {
      svn_boolean_t different_txn
        = root_a->is_txn_root && root_b->is_txn_root
            && strcmp(root_a->txn, root_b->txn);

      /* For txn roots, root->REV is the base revision of that TXN. */
      *relation = (   (root_a->rev == root_b->rev)
                   && (root_a->is_txn_root == root_b->is_txn_root)
                   && !different_txn)
                ? svn_fs_node_unchanged
                : svn_fs_node_common_ancestor;
      return SVN_NO_ERROR;
    }

  /* We checked for all separations between ID spaces (repos, txn).
   * Now, we can simply test for the ID values themselves. */
  SVN_ERR(get_dag(&node, root_a, path_a, scratch_pool));
  noderev_id_a = *svn_fs_x__dag_get_id(node);
  SVN_ERR(svn_fs_x__dag_get_node_id(&node_id_a, node));

  SVN_ERR(get_dag(&node, root_b, path_b, scratch_pool));
  noderev_id_b = *svn_fs_x__dag_get_id(node);
  SVN_ERR(svn_fs_x__dag_get_node_id(&node_id_b, node));

  /* In FSX, even in-txn IDs are globally unique.
   * So, we can simply compare them. */
  if (svn_fs_x__id_eq(&noderev_id_a, &noderev_id_b))
    *relation = svn_fs_node_unchanged;
  else if (svn_fs_x__id_eq(&node_id_a, &node_id_b))
    *relation = svn_fs_node_common_ancestor;
  else
    *relation = svn_fs_node_unrelated;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_x__node_created_rev(svn_revnum_t *revision,
                           svn_fs_root_t *root,
                           const char *path,
                           apr_pool_t *scratch_pool)
{
  dag_node_t *node;

  SVN_ERR(get_dag(&node, root, path, scratch_pool));
  *revision = svn_fs_x__dag_get_revision(node);

  return SVN_NO_ERROR;
}


/* Set *CREATED_PATH to the path at which PATH under ROOT was created.
   Return a string allocated in POOL. */
static svn_error_t *
x_node_created_path(const char **created_path,
                    svn_fs_root_t *root,
                    const char *path,
                    apr_pool_t *pool)
{
  dag_node_t *node;

  SVN_ERR(get_dag(&node, root, path, pool));
  *created_path = svn_fs_x__dag_get_created_path(node);

  return SVN_NO_ERROR;
}


/* Set *KIND_P to the type of node located at PATH under ROOT.
   Perform temporary allocations in SCRATCH_POOL. */
static svn_error_t *
node_kind(svn_node_kind_t *kind_p,
          svn_fs_root_t *root,
          const char *path,
          apr_pool_t *scratch_pool)
{
  dag_node_t *node;

  /* Get the node id. */
  SVN_ERR(get_dag(&node, root, path, scratch_pool));

  /* Use the node id to get the real kind. */
  *kind_p = svn_fs_x__dag_node_kind(node);

  return SVN_NO_ERROR;
}


/* Set *KIND_P to the type of node present at PATH under ROOT.  If
   PATH does not exist under ROOT, set *KIND_P to svn_node_none.  Use
   SCRATCH_POOL for temporary allocation. */
svn_error_t *
svn_fs_x__check_path(svn_node_kind_t *kind_p,
                     svn_fs_root_t *root,
                     const char *path,
                     apr_pool_t *scratch_pool)
{
  svn_error_t *err = node_kind(kind_p, root, path, scratch_pool);
  if (err &&
      ((err->apr_err == SVN_ERR_FS_NOT_FOUND)
       || (err->apr_err == SVN_ERR_FS_NOT_DIRECTORY)))
    {
      svn_error_clear(err);
      err = SVN_NO_ERROR;
      *kind_p = svn_node_none;
    }

  return svn_error_trace(err);
}

/* Set *VALUE_P to the value of the property named PROPNAME of PATH in
   ROOT.  If the node has no property by that name, set *VALUE_P to
   zero.  Allocate the result in POOL. */
static svn_error_t *
x_node_prop(svn_string_t **value_p,
            svn_fs_root_t *root,
            const char *path,
            const char *propname,
            apr_pool_t *pool)
{
  dag_node_t *node;
  apr_hash_t *proplist;
  apr_pool_t *scratch_pool = svn_pool_create(pool);

  SVN_ERR(get_dag(&node, root, path,  pool));
  SVN_ERR(svn_fs_x__dag_get_proplist(&proplist, node, pool, scratch_pool));
  *value_p = NULL;
  if (proplist)
    *value_p = svn_hash_gets(proplist, propname);

  svn_pool_destroy(scratch_pool);
  return SVN_NO_ERROR;
}


/* Set *TABLE_P to the entire property list of PATH under ROOT, as an
   APR hash table allocated in POOL.  The resulting property table
   maps property names to pointers to svn_string_t objects containing
   the property value. */
static svn_error_t *
x_node_proplist(apr_hash_t **table_p,
                svn_fs_root_t *root,
                const char *path,
                apr_pool_t *pool)
{
  dag_node_t *node;
  apr_pool_t *scratch_pool = svn_pool_create(pool);

  SVN_ERR(get_dag(&node, root, path, pool));
  SVN_ERR(svn_fs_x__dag_get_proplist(table_p, node, pool, scratch_pool));

  svn_pool_destroy(scratch_pool);
  return SVN_NO_ERROR;
}

static svn_error_t *
x_node_has_props(svn_boolean_t *has_props,
                 svn_fs_root_t *root,
                 const char *path,
                 apr_pool_t *scratch_pool)
{
  apr_hash_t *props;

  SVN_ERR(x_node_proplist(&props, root, path, scratch_pool));

  *has_props = (0 < apr_hash_count(props));

  return SVN_NO_ERROR;
}

static svn_error_t *
increment_mergeinfo_up_tree(parent_path_t *pp,
                            apr_int64_t increment,
                            apr_pool_t *scratch_pool)
{
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);

  for (; pp; pp = pp->parent)
    {
      svn_pool_clear(iterpool);
      SVN_ERR(svn_fs_x__dag_increment_mergeinfo_count(pp->node,
                                                      increment,
                                                      iterpool));
    }

  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}

/* Change, add, or delete a node's property value.  The affected node
   is PATH under ROOT, the property value to modify is NAME, and VALUE
   points to either a string value to set the new contents to, or NULL
   if the property should be deleted.  Perform temporary allocations
   in SCRATCH_POOL. */
static svn_error_t *
x_change_node_prop(svn_fs_root_t *root,
                   const char *path,
                   const char *name,
                   const svn_string_t *value,
                   apr_pool_t *scratch_pool)
{
  parent_path_t *parent_path;
  apr_hash_t *proplist;
  svn_fs_x__txn_id_t txn_id;
  svn_boolean_t mergeinfo_mod = FALSE;
  apr_pool_t *subpool = svn_pool_create(scratch_pool);

  if (! root->is_txn_root)
    return SVN_FS__NOT_TXN(root);
  txn_id = root_txn_id(root);

  path = svn_fs__canonicalize_abspath(path, subpool);
  SVN_ERR(open_path(&parent_path, root, path, 0, TRUE, subpool));

  /* Check (non-recursively) to see if path is locked; if so, check
     that we can use it. */
  if (root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
    SVN_ERR(svn_fs_x__allow_locked_operation(path, root->fs, FALSE, FALSE,
                                             subpool));

  SVN_ERR(make_path_mutable(root, parent_path, path, subpool, subpool));
  SVN_ERR(svn_fs_x__dag_get_proplist(&proplist, parent_path->node, subpool,
                                     subpool));

  /* If there's no proplist, but we're just deleting a property, exit now. */
  if ((! proplist) && (! value))
    return SVN_NO_ERROR;

  /* Now, if there's no proplist, we know we need to make one. */
  if (! proplist)
    proplist = apr_hash_make(subpool);

  if (strcmp(name, SVN_PROP_MERGEINFO) == 0)
    {
      apr_int64_t increment = 0;
      svn_boolean_t had_mergeinfo;
      SVN_ERR(svn_fs_x__dag_has_mergeinfo(&had_mergeinfo, parent_path->node));

      if (value && !had_mergeinfo)
        increment = 1;
      else if (!value && had_mergeinfo)
        increment = -1;

      if (increment != 0)
        {
          SVN_ERR(increment_mergeinfo_up_tree(parent_path, increment, subpool));
          SVN_ERR(svn_fs_x__dag_set_has_mergeinfo(parent_path->node,
                                                  (value != NULL), subpool));
        }

      mergeinfo_mod = TRUE;
    }

  /* Set the property. */
  svn_hash_sets(proplist, name, value);

  /* Overwrite the node's proplist. */
  SVN_ERR(svn_fs_x__dag_set_proplist(parent_path->node, proplist,
                                     subpool));

  /* Make a record of this modification in the changes table. */
  SVN_ERR(add_change(root->fs, txn_id, path,
                     svn_fs_x__dag_get_id(parent_path->node),
                     svn_fs_path_change_modify, FALSE, TRUE, mergeinfo_mod,
                     svn_fs_x__dag_node_kind(parent_path->node),
                     SVN_INVALID_REVNUM, NULL, subpool));

  svn_pool_destroy(subpool);
  return SVN_NO_ERROR;
}


/* Determine if the properties of two path/root combinations are
   different.  Set *CHANGED_P to TRUE if the properties at PATH1 under
   ROOT1 differ from those at PATH2 under ROOT2, or FALSE otherwise.
   Both roots must be in the same filesystem. */
static svn_error_t *
x_props_changed(svn_boolean_t *changed_p,
                svn_fs_root_t *root1,
                const char *path1,
                svn_fs_root_t *root2,
                const char *path2,
                svn_boolean_t strict,
                apr_pool_t *scratch_pool)
{
  dag_node_t *node1, *node2;
  apr_pool_t *subpool = svn_pool_create(scratch_pool);

  /* Check that roots are in the same fs. */
  if (root1->fs != root2->fs)
    return svn_error_create
      (SVN_ERR_FS_GENERAL, NULL,
       _("Cannot compare property value between two different filesystems"));

  SVN_ERR(get_dag(&node1, root1, path1, subpool));
  SVN_ERR(get_dag(&node2, root2, path2, subpool));
  SVN_ERR(svn_fs_x__dag_things_different(changed_p, NULL, node1, node2,
                                         strict, subpool));
  svn_pool_destroy(subpool);

  return SVN_NO_ERROR;
}



/* Merges and commits. */

/* Set *NODE to the root node of ROOT.  */
static svn_error_t *
get_root(dag_node_t **node, svn_fs_root_t *root, apr_pool_t *pool)
{
  return get_dag(node, root, "/", pool);
}


/* Set the contents of CONFLICT_PATH to PATH, and return an
   SVN_ERR_FS_CONFLICT error that indicates that there was a conflict
   at PATH.  Perform all allocations in POOL (except the allocation of
   CONFLICT_PATH, which should be handled outside this function).  */
static svn_error_t *
conflict_err(svn_stringbuf_t *conflict_path,
             const char *path)
{
  svn_stringbuf_set(conflict_path, path);
  return svn_error_createf(SVN_ERR_FS_CONFLICT, NULL,
                           _("Conflict at '%s'"), path);
}

/* Compare the directory representations at nodes LHS and RHS in FS and set
 * *CHANGED to TRUE, if at least one entry has been added or removed them.
 * Use SCRATCH_POOL for temporary allocations.
 */
static svn_error_t *
compare_dir_structure(svn_boolean_t *changed,
                      svn_fs_t *fs,
                      dag_node_t *lhs,
                      dag_node_t *rhs,
                      apr_pool_t *scratch_pool)
{
  apr_array_header_t *lhs_entries;
  apr_array_header_t *rhs_entries;
  int i;
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);

  SVN_ERR(svn_fs_x__dag_dir_entries(&lhs_entries, lhs, scratch_pool,
                                    iterpool));
  SVN_ERR(svn_fs_x__dag_dir_entries(&rhs_entries, rhs, scratch_pool,
                                    iterpool));

  /* different number of entries -> some addition / removal */
  if (lhs_entries->nelts != rhs_entries->nelts)
    {
      svn_pool_destroy(iterpool);
      *changed = TRUE;

      return SVN_NO_ERROR;
    }

  /* Since directories are sorted by name, we can simply compare their
     entries one-by-one without binary lookup etc. */
  for (i = 0; i < lhs_entries->nelts; ++i)
    {
      svn_fs_x__dirent_t *lhs_entry
        = APR_ARRAY_IDX(lhs_entries, i, svn_fs_x__dirent_t *);
      svn_fs_x__dirent_t *rhs_entry
        = APR_ARRAY_IDX(rhs_entries, i, svn_fs_x__dirent_t *);

      if (strcmp(lhs_entry->name, rhs_entry->name) == 0)
        {
          svn_boolean_t same_history;
          dag_node_t *lhs_node, *rhs_node;

          /* Unchanged entry? */
          if (!svn_fs_x__id_eq(&lhs_entry->id, &rhs_entry->id))
            continue;

          /* We get here rarely. */
          svn_pool_clear(iterpool);

          /* Modified but not copied / replaced or anything? */
          SVN_ERR(svn_fs_x__dag_get_node(&lhs_node, fs, &lhs_entry->id,
                                         iterpool, iterpool));
          SVN_ERR(svn_fs_x__dag_get_node(&rhs_node, fs, &rhs_entry->id,
                                         iterpool, iterpool));
          SVN_ERR(svn_fs_x__dag_same_line_of_history(&same_history,
                                                     lhs_node, rhs_node));
          if (same_history)
            continue;
        }

      /* This is a different entry. */
      *changed = TRUE;
      svn_pool_destroy(iterpool);

      return SVN_NO_ERROR;
    }

  svn_pool_destroy(iterpool);
  *changed = FALSE;

  return SVN_NO_ERROR;
}

/* Merge changes between ANCESTOR and SOURCE into TARGET.  ANCESTOR
 * and TARGET must be distinct node revisions.  TARGET_PATH should
 * correspond to TARGET's full path in its filesystem, and is used for
 * reporting conflict location.
 *
 * SOURCE, TARGET, and ANCESTOR are generally directories; this
 * function recursively merges the directories' contents.  If any are
 * files, this function simply returns an error whenever SOURCE,
 * TARGET, and ANCESTOR are all distinct node revisions.
 *
 * If there are differences between ANCESTOR and SOURCE that conflict
 * with changes between ANCESTOR and TARGET, this function returns an
 * SVN_ERR_FS_CONFLICT error, and updates CONFLICT_P to the name of the
 * conflicting node in TARGET, with TARGET_PATH prepended as a path.
 *
 * If there are no conflicting differences, CONFLICT_P is updated to
 * the empty string.
 *
 * CONFLICT_P must point to a valid svn_stringbuf_t.
 *
 * Do any necessary temporary allocation in POOL.
 */
static svn_error_t *
merge(svn_stringbuf_t *conflict_p,
      const char *target_path,
      dag_node_t *target,
      dag_node_t *source,
      dag_node_t *ancestor,
      svn_fs_x__txn_id_t txn_id,
      apr_int64_t *mergeinfo_increment_out,
      apr_pool_t *pool)
{
  const svn_fs_x__id_t *source_id, *target_id, *ancestor_id;
  apr_array_header_t *s_entries, *t_entries, *a_entries;
  int i, s_idx = -1, t_idx = -1;
  svn_fs_t *fs;
  apr_pool_t *iterpool;
  apr_int64_t mergeinfo_increment = 0;

  /* Make sure everyone comes from the same filesystem. */
  fs = svn_fs_x__dag_get_fs(ancestor);
  if ((fs != svn_fs_x__dag_get_fs(source))
      || (fs != svn_fs_x__dag_get_fs(target)))
    {
      return svn_error_create
        (SVN_ERR_FS_CORRUPT, NULL,
         _("Bad merge; ancestor, source, and target not all in same fs"));
    }

  /* We have the same fs, now check it. */
  SVN_ERR(svn_fs__check_fs(fs, TRUE));

  source_id   = svn_fs_x__dag_get_id(source);
  target_id   = svn_fs_x__dag_get_id(target);
  ancestor_id = svn_fs_x__dag_get_id(ancestor);

  /* It's improper to call this function with ancestor == target. */
  if (svn_fs_x__id_eq(ancestor_id, target_id))
    {
      svn_string_t *id_str = svn_fs_x__id_unparse(target_id, pool);
      return svn_error_createf
        (SVN_ERR_FS_GENERAL, NULL,
         _("Bad merge; target '%s' has id '%s', same as ancestor"),
         target_path, id_str->data);
    }

  svn_stringbuf_setempty(conflict_p);

  /* Base cases:
   * Either no change made in source, or same change as made in target.
   * Both mean nothing to merge here.
   */
  if (svn_fs_x__id_eq(ancestor_id, source_id)
      || (svn_fs_x__id_eq(source_id, target_id)))
    return SVN_NO_ERROR;

  /* Else proceed, knowing all three are distinct node revisions.
   *
   * How to merge from this point:
   *
   * if (not all 3 are directories)
   *   {
   *     early exit with conflict;
   *   }
   *
   * // Property changes may only be made to up-to-date
   * // directories, because once the client commits the prop
   * // change, it bumps the directory's revision, and therefore
   * // must be able to depend on there being no other changes to
   * // that directory in the repository.
   * if (target's property list differs from ancestor's)
   *    conflict;
   *
   * For each entry NAME in the directory ANCESTOR:
   *
   *   Let ANCESTOR-ENTRY, SOURCE-ENTRY, and TARGET-ENTRY be the IDs of
   *   the name within ANCESTOR, SOURCE, and TARGET respectively.
   *   (Possibly null if NAME does not exist in SOURCE or TARGET.)
   *
   *   If ANCESTOR-ENTRY == SOURCE-ENTRY, then:
   *     No changes were made to this entry while the transaction was in
   *     progress, so do nothing to the target.
   *
   *   Else if ANCESTOR-ENTRY == TARGET-ENTRY, then:
   *     A change was made to this entry while the transaction was in
   *     process, but the transaction did not touch this entry.  Replace
   *     TARGET-ENTRY with SOURCE-ENTRY.
   *
   *   Else:
   *     Changes were made to this entry both within the transaction and
   *     to the repository while the transaction was in progress.  They
   *     must be merged or declared to be in conflict.
   *
   *     If SOURCE-ENTRY and TARGET-ENTRY are both null, that's a
   *     double delete; flag a conflict.
   *
   *     If any of the three entries is of type file, declare a conflict.
   *
   *     If either SOURCE-ENTRY or TARGET-ENTRY is not a direct
   *     modification of ANCESTOR-ENTRY (determine by comparing the
   *     node-id fields), declare a conflict.  A replacement is
   *     incompatible with a modification or other replacement--even
   *     an identical replacement.
   *
   *     Direct modifications were made to the directory ANCESTOR-ENTRY
   *     in both SOURCE and TARGET.  Recursively merge these
   *     modifications.
   *
   * For each leftover entry NAME in the directory SOURCE:
   *
   *   If NAME exists in TARGET, declare a conflict.  Even if SOURCE and
   *   TARGET are adding exactly the same thing, two additions are not
   *   auto-mergeable with each other.
   *
   *   Add NAME to TARGET with the entry from SOURCE.
   *
   * Now that we are done merging the changes from SOURCE into the
   * directory TARGET, update TARGET's predecessor to be SOURCE.
   */

  if ((svn_fs_x__dag_node_kind(source) != svn_node_dir)
      || (svn_fs_x__dag_node_kind(target) != svn_node_dir)
      || (svn_fs_x__dag_node_kind(ancestor) != svn_node_dir))
    {
      return conflict_err(conflict_p, target_path);
    }


  /* Possible early merge failure: if target and ancestor have
     different property lists, then the merge should fail.
     Propchanges can *only* be committed on an up-to-date directory.
     ### TODO: see issue #418 about the inelegance of this.

     Another possible, similar, early merge failure: if source and
     ancestor have different property lists (meaning someone else
     changed directory properties while our commit transaction was
     happening), the merge should fail.  See issue #2751.
  */
  {
    svn_fs_x__noderev_t *tgt_nr, *anc_nr, *src_nr;
    svn_boolean_t same;
    apr_pool_t *scratch_pool;

    /* Get node revisions for our id's. */
    scratch_pool = svn_pool_create(pool);
    SVN_ERR(svn_fs_x__get_node_revision(&tgt_nr, fs, target_id,
                                        pool, scratch_pool));
    svn_pool_clear(scratch_pool);
    SVN_ERR(svn_fs_x__get_node_revision(&anc_nr, fs, ancestor_id,
                                        pool, scratch_pool));
    svn_pool_clear(scratch_pool);
    SVN_ERR(svn_fs_x__get_node_revision(&src_nr, fs, source_id,
                                        pool, scratch_pool));
    svn_pool_destroy(scratch_pool);

    /* Now compare the prop-keys of the skels.  Note that just because
       the keys are different -doesn't- mean the proplists have
       different contents. */
    SVN_ERR(svn_fs_x__prop_rep_equal(&same, fs, src_nr, anc_nr, TRUE, pool));
    if (! same)
      return conflict_err(conflict_p, target_path);

    /* The directory entries got changed in the repository but the directory
       properties did not. */
    SVN_ERR(svn_fs_x__prop_rep_equal(&same, fs, tgt_nr, anc_nr, TRUE, pool));
    if (! same)
      {
        /* There is an incoming prop change for this directory.
           We will accept it only if the directory changes were mere updates
           to its entries, i.e. there were no additions or removals.
           Those could cause update problems to the working copy. */
        svn_boolean_t changed;
        SVN_ERR(compare_dir_structure(&changed, fs, source, ancestor, pool));

        if (changed)
          return conflict_err(conflict_p, target_path);
      }
  }

  /* ### todo: it would be more efficient to simply check for a NULL
     entries hash where necessary below than to allocate an empty hash
     here, but another day, another day... */
  iterpool = svn_pool_create(pool);
  SVN_ERR(svn_fs_x__dag_dir_entries(&s_entries, source, pool, iterpool));
  SVN_ERR(svn_fs_x__dag_dir_entries(&t_entries, target, pool, iterpool));
  SVN_ERR(svn_fs_x__dag_dir_entries(&a_entries, ancestor, pool, iterpool));

  /* for each entry E in a_entries... */
  for (i = 0; i < a_entries->nelts; ++i)
    {
      svn_fs_x__dirent_t *s_entry, *t_entry, *a_entry;
      svn_pool_clear(iterpool);

      a_entry = APR_ARRAY_IDX(a_entries, i, svn_fs_x__dirent_t *);
      s_entry = svn_fs_x__find_dir_entry(s_entries, a_entry->name, &s_idx);
      t_entry = svn_fs_x__find_dir_entry(t_entries, a_entry->name, &t_idx);

      /* No changes were made to this entry while the transaction was
         in progress, so do nothing to the target. */
      if (s_entry && svn_fs_x__id_eq(&a_entry->id, &s_entry->id))
        continue;

      /* A change was made to this entry while the transaction was in
         process, but the transaction did not touch this entry. */
      else if (t_entry && svn_fs_x__id_eq(&a_entry->id, &t_entry->id))
        {
          apr_int64_t mergeinfo_start;
          apr_int64_t mergeinfo_end;

          dag_node_t *t_ent_node;
          SVN_ERR(svn_fs_x__dag_get_node(&t_ent_node, fs, &t_entry->id,
                                         iterpool, iterpool));
          SVN_ERR(svn_fs_x__dag_get_mergeinfo_count(&mergeinfo_start,
                                                    t_ent_node));
          mergeinfo_increment -= mergeinfo_start;

          if (s_entry)
            {
              dag_node_t *s_ent_node;
              SVN_ERR(svn_fs_x__dag_get_node(&s_ent_node, fs, &s_entry->id,
                                             iterpool, iterpool));

              SVN_ERR(svn_fs_x__dag_get_mergeinfo_count(&mergeinfo_end,
                                                        s_ent_node));
              mergeinfo_increment += mergeinfo_end;

              SVN_ERR(svn_fs_x__dag_set_entry(target, a_entry->name,
                                              &s_entry->id,
                                              s_entry->kind,
                                              txn_id,
                                              iterpool));
            }
          else
            {
              SVN_ERR(svn_fs_x__dag_delete(target, a_entry->name, txn_id,
                                           iterpool));
            }
        }

      /* Changes were made to this entry both within the transaction
         and to the repository while the transaction was in progress.
         They must be merged or declared to be in conflict. */
      else
        {
          dag_node_t *s_ent_node, *t_ent_node, *a_ent_node;
          const char *new_tpath;
          apr_int64_t sub_mergeinfo_increment;
          svn_boolean_t s_a_same, t_a_same;

          /* If SOURCE-ENTRY and TARGET-ENTRY are both null, that's a
             double delete; if one of them is null, that's a delete versus
             a modification. In any of these cases, flag a conflict. */
          if (s_entry == NULL || t_entry == NULL)
            return conflict_err(conflict_p,
                                svn_fspath__join(target_path,
                                                 a_entry->name,
                                                 iterpool));

          /* If any of the three entries is of type file, flag a conflict. */
          if (s_entry->kind == svn_node_file
              || t_entry->kind == svn_node_file
              || a_entry->kind == svn_node_file)
            return conflict_err(conflict_p,
                                svn_fspath__join(target_path,
                                                 a_entry->name,
                                                 iterpool));

          /* Fetch DAG nodes to efficiently access ID parts. */
          SVN_ERR(svn_fs_x__dag_get_node(&s_ent_node, fs, &s_entry->id,
                                         iterpool, iterpool));
          SVN_ERR(svn_fs_x__dag_get_node(&t_ent_node, fs, &t_entry->id,
                                         iterpool, iterpool));
          SVN_ERR(svn_fs_x__dag_get_node(&a_ent_node, fs, &a_entry->id,
                                         iterpool, iterpool));

          /* If either SOURCE-ENTRY or TARGET-ENTRY is not a direct
             modification of ANCESTOR-ENTRY, declare a conflict. */
          SVN_ERR(svn_fs_x__dag_same_line_of_history(&s_a_same, s_ent_node,
                                                     a_ent_node));
          SVN_ERR(svn_fs_x__dag_same_line_of_history(&t_a_same, t_ent_node,
                                                     a_ent_node));
          if (!s_a_same || !t_a_same)
            return conflict_err(conflict_p,
                                svn_fspath__join(target_path,
                                                 a_entry->name,
                                                 iterpool));

          /* Direct modifications were made to the directory
             ANCESTOR-ENTRY in both SOURCE and TARGET.  Recursively
             merge these modifications. */
          new_tpath = svn_fspath__join(target_path, t_entry->name, iterpool);
          SVN_ERR(merge(conflict_p, new_tpath,
                        t_ent_node, s_ent_node, a_ent_node,
                        txn_id,
                        &sub_mergeinfo_increment,
                        iterpool));
          mergeinfo_increment += sub_mergeinfo_increment;
        }
    }

  /* For each entry E in source but not in ancestor */
  for (i = 0; i < s_entries->nelts; ++i)
    {
      svn_fs_x__dirent_t *a_entry, *s_entry, *t_entry;
      dag_node_t *s_ent_node;
      apr_int64_t mergeinfo_s;

      svn_pool_clear(iterpool);

      s_entry = APR_ARRAY_IDX(s_entries, i, svn_fs_x__dirent_t *);
      a_entry = svn_fs_x__find_dir_entry(a_entries, s_entry->name, &s_idx);
      t_entry = svn_fs_x__find_dir_entry(t_entries, s_entry->name, &t_idx);

      /* Process only entries in source that are NOT in ancestor. */
      if (a_entry)
        continue;

      /* If NAME exists in TARGET, declare a conflict. */
      if (t_entry)
        return conflict_err(conflict_p,
                            svn_fspath__join(target_path,
                                             t_entry->name,
                                             iterpool));

      SVN_ERR(svn_fs_x__dag_get_node(&s_ent_node, fs, &s_entry->id,
                                     iterpool, iterpool));
      SVN_ERR(svn_fs_x__dag_get_mergeinfo_count(&mergeinfo_s, s_ent_node));
      mergeinfo_increment += mergeinfo_s;

      SVN_ERR(svn_fs_x__dag_set_entry
              (target, s_entry->name, &s_entry->id, s_entry->kind,
               txn_id, iterpool));
    }
  svn_pool_destroy(iterpool);

  SVN_ERR(svn_fs_x__dag_update_ancestry(target, source, pool));

  SVN_ERR(svn_fs_x__dag_increment_mergeinfo_count(target,
                                                  mergeinfo_increment,
                                                  pool));

  if (mergeinfo_increment_out)
    *mergeinfo_increment_out = mergeinfo_increment;

  return SVN_NO_ERROR;
}

/* Merge changes between an ancestor and SOURCE_NODE into
   TXN.  The ancestor is either ANCESTOR_NODE, or if
   that is null, TXN's base node.

   If the merge is successful, TXN's base will become
   SOURCE_NODE, and its root node will have a new ID, a
   successor of SOURCE_NODE.

   If a conflict results, update *CONFLICT to the path in the txn that
   conflicted; see the CONFLICT_P parameter of merge() for details. */
static svn_error_t *
merge_changes(dag_node_t *ancestor_node,
              dag_node_t *source_node,
              svn_fs_txn_t *txn,
              svn_stringbuf_t *conflict,
              apr_pool_t *scratch_pool)
{
  dag_node_t *txn_root_node;
  svn_fs_t *fs = txn->fs;
  svn_fs_x__txn_id_t txn_id = svn_fs_x__txn_get_id(txn);
  svn_boolean_t related;

  SVN_ERR(svn_fs_x__dag_txn_root(&txn_root_node, fs, txn_id, scratch_pool,
                                 scratch_pool));

  if (ancestor_node == NULL)
    {
      svn_revnum_t base_rev;
      SVN_ERR(svn_fs_x__get_base_rev(&base_rev, fs, txn_id, scratch_pool));
      SVN_ERR(svn_fs_x__dag_revision_root(&ancestor_node, fs, base_rev,
                                          scratch_pool, scratch_pool));
    }

  SVN_ERR(svn_fs_x__dag_related_node(&related, ancestor_node, txn_root_node));
  if (!related)
    {
      /* If no changes have been made in TXN since its current base,
         then it can't conflict with any changes since that base.
         The caller isn't supposed to call us in that case. */
      SVN_ERR_MALFUNCTION();
    }
  else
    SVN_ERR(merge(conflict, "/", txn_root_node,
                  source_node, ancestor_node, txn_id, NULL, scratch_pool));

  return SVN_NO_ERROR;
}


svn_error_t *
svn_fs_x__commit_txn(const char **conflict_p,
                     svn_revnum_t *new_rev,
                     svn_fs_txn_t *txn,
                     apr_pool_t *pool)
{
  /* How do commits work in Subversion?
   *
   * When you're ready to commit, here's what you have:
   *
   *    1. A transaction, with a mutable tree hanging off it.
   *    2. A base revision, against which TXN_TREE was made.
   *    3. A latest revision, which may be newer than the base rev.
   *
   * The problem is that if latest != base, then one can't simply
   * attach the txn root as the root of the new revision, because that
   * would lose all the changes between base and latest.  It is also
   * not acceptable to insist that base == latest; in a busy
   * repository, commits happen too fast to insist that everyone keep
   * their entire tree up-to-date at all times.  Non-overlapping
   * changes should not interfere with each other.
   *
   * The solution is to merge the changes between base and latest into
   * the txn tree [see the function merge()].  The txn tree is the
   * only one of the three trees that is mutable, so it has to be the
   * one to adjust.
   *
   * You might have to adjust it more than once, if a new latest
   * revision gets committed while you were merging in the previous
   * one.  For example:
   *
   *    1. Jane starts txn T, based at revision 6.
   *    2. Someone commits (or already committed) revision 7.
   *    3. Jane's starts merging the changes between 6 and 7 into T.
   *    4. Meanwhile, someone commits revision 8.
   *    5. Jane finishes the 6-->7 merge.  T could now be committed
   *       against a latest revision of 7, if only that were still the
   *       latest.  Unfortunately, 8 is now the latest, so...
   *    6. Jane starts merging the changes between 7 and 8 into T.
   *    7. Meanwhile, no one commits any new revisions.  Whew.
   *    8. Jane commits T, creating revision 9, whose tree is exactly
   *       T's tree, except immutable now.
   *
   * Lather, rinse, repeat.
   */

  svn_error_t *err = SVN_NO_ERROR;
  svn_stringbuf_t *conflict = svn_stringbuf_create_empty(pool);
  svn_fs_t *fs = txn->fs;
  svn_fs_x__data_t *ffd = fs->fsap_data;

  /* Limit memory usage when the repository has a high commit rate and
     needs to run the following while loop multiple times.  The memory
     growth without an iteration pool is very noticeable when the
     transaction modifies a node that has 20,000 sibling nodes. */
  apr_pool_t *iterpool = svn_pool_create(pool);

  /* Initialize output params. */
  *new_rev = SVN_INVALID_REVNUM;
  if (conflict_p)
    *conflict_p = NULL;

  while (1729)
    {
      svn_revnum_t youngish_rev;
      svn_fs_root_t *youngish_root;
      dag_node_t *youngish_root_node;

      svn_pool_clear(iterpool);

      /* Get the *current* youngest revision.  We call it "youngish"
         because new revisions might get committed after we've
         obtained it. */

      SVN_ERR(svn_fs_x__youngest_rev(&youngish_rev, fs, iterpool));
      SVN_ERR(svn_fs_x__revision_root(&youngish_root, fs, youngish_rev,
                                      iterpool));

      /* Get the dag node for the youngest revision.  Later we'll use
         it as the SOURCE argument to a merge, and if the merge
         succeeds, this youngest root node will become the new base
         root for the svn txn that was the target of the merge (but
         note that the youngest rev may have changed by then -- that's
         why we're careful to get this root in its own bdb txn
         here). */
      SVN_ERR(get_root(&youngish_root_node, youngish_root, iterpool));

      /* Try to merge.  If the merge succeeds, the base root node of
         TARGET's txn will become the same as youngish_root_node, so
         any future merges will only be between that node and whatever
         the root node of the youngest rev is by then. */
      err = merge_changes(NULL, youngish_root_node, txn, conflict, iterpool);
      if (err)
        {
          if ((err->apr_err == SVN_ERR_FS_CONFLICT) && conflict_p)
            *conflict_p = conflict->data;
          goto cleanup;
        }
      txn->base_rev = youngish_rev;

      /* Try to commit. */
      err = svn_fs_x__commit(new_rev, fs, txn, iterpool);
      if (err && (err->apr_err == SVN_ERR_FS_TXN_OUT_OF_DATE))
        {
          /* Did someone else finish committing a new revision while we
             were in mid-merge or mid-commit?  If so, we'll need to
             loop again to merge the new changes in, then try to
             commit again.  Or if that's not what happened, then just
             return the error. */
          svn_revnum_t youngest_rev;
          SVN_ERR(svn_fs_x__youngest_rev(&youngest_rev, fs, iterpool));
          if (youngest_rev == youngish_rev)
            goto cleanup;
          else
            svn_error_clear(err);
        }
      else if (err)
        {
          goto cleanup;
        }
      else
        {
          err = SVN_NO_ERROR;
          goto cleanup;
        }
    }

 cleanup:

  svn_pool_destroy(iterpool);

  SVN_ERR(err);

  if (ffd->pack_after_commit)
    {
      SVN_ERR(svn_fs_x__pack(fs, NULL, NULL, NULL, NULL, pool));
    }

  return SVN_NO_ERROR;
}


/* Merge changes between two nodes into a third node.  Given nodes
   SOURCE_PATH under SOURCE_ROOT, TARGET_PATH under TARGET_ROOT and
   ANCESTOR_PATH under ANCESTOR_ROOT, modify target to contain all the
   changes between the ancestor and source.  If there are conflicts,
   return SVN_ERR_FS_CONFLICT and set *CONFLICT_P to a textual
   description of the offending changes.  Perform any temporary
   allocations in POOL. */
static svn_error_t *
x_merge(const char **conflict_p,
        svn_fs_root_t *source_root,
        const char *source_path,
        svn_fs_root_t *target_root,
        const char *target_path,
        svn_fs_root_t *ancestor_root,
        const char *ancestor_path,
        apr_pool_t *pool)
{
  dag_node_t *source, *ancestor;
  svn_fs_txn_t *txn;
  svn_error_t *err;
  svn_stringbuf_t *conflict = svn_stringbuf_create_empty(pool);

  if (! target_root->is_txn_root)
    return SVN_FS__NOT_TXN(target_root);

  /* Paranoia. */
  if ((source_root->fs != ancestor_root->fs)
      || (target_root->fs != ancestor_root->fs))
    {
      return svn_error_create
        (SVN_ERR_FS_CORRUPT, NULL,
         _("Bad merge; ancestor, source, and target not all in same fs"));
    }

  /* ### kff todo: is there any compelling reason to get the nodes in
     one db transaction?  Right now we don't; txn_body_get_root() gets
     one node at a time.  This will probably need to change:

     Jim Blandy <jimb@zwingli.cygnus.com> writes:
     > svn_fs_merge needs to be a single transaction, to protect it against
     > people deleting parents of nodes it's working on, etc.
  */

  /* Get the ancestor node. */
  SVN_ERR(get_root(&ancestor, ancestor_root, pool));

  /* Get the source node. */
  SVN_ERR(get_root(&source, source_root, pool));

  /* Open a txn for the txn root into which we're merging. */
  SVN_ERR(svn_fs_x__open_txn(&txn, ancestor_root->fs, target_root->txn,
                             pool));

  /* Merge changes between ANCESTOR and SOURCE into TXN. */
  err = merge_changes(ancestor, source, txn, conflict, pool);
  if (err)
    {
      if ((err->apr_err == SVN_ERR_FS_CONFLICT) && conflict_p)
        *conflict_p = conflict->data;
      return svn_error_trace(err);
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_x__deltify(svn_fs_t *fs,
                  svn_revnum_t revision,
                  apr_pool_t *scratch_pool)
{
  /* Deltify is a no-op for fs_x. */

  return SVN_NO_ERROR;
}



/* Directories.  */

/* Set *TABLE_P to a newly allocated APR hash table containing the
   entries of the directory at PATH in ROOT.  The keys of the table
   are entry names, as byte strings, excluding the final null
   character; the table's values are pointers to svn_fs_svn_fs_x__dirent_t
   structures.  Allocate the table and its contents in POOL. */
static svn_error_t *
x_dir_entries(apr_hash_t **table_p,
              svn_fs_root_t *root,
              const char *path,
              apr_pool_t *pool)
{
  dag_node_t *node;
  apr_hash_t *hash = svn_hash__make(pool);
  apr_array_header_t *table;
  int i;
  svn_fs_x__id_context_t *context = NULL;
  apr_pool_t *scratch_pool = svn_pool_create(pool);

  /* Get the entries for this path in the caller's pool. */
  SVN_ERR(get_dag(&node, root, path, scratch_pool));
  SVN_ERR(svn_fs_x__dag_dir_entries(&table, node, scratch_pool,
                                    scratch_pool));

  if (table->nelts)
    context = svn_fs_x__id_create_context(root->fs, pool);

  /* Convert directory array to hash. */
  for (i = 0; i < table->nelts; ++i)
    {
      svn_fs_x__dirent_t *entry
        = APR_ARRAY_IDX(table, i, svn_fs_x__dirent_t *);
      apr_size_t len = strlen(entry->name);

      svn_fs_dirent_t *api_dirent = apr_pcalloc(pool, sizeof(*api_dirent));
      api_dirent->name = apr_pstrmemdup(pool, entry->name, len);
      api_dirent->kind = entry->kind;
      api_dirent->id = svn_fs_x__id_create(context, &entry->id, pool);

      apr_hash_set(hash, api_dirent->name, len, api_dirent);
    }

  *table_p = hash;
  svn_pool_destroy(scratch_pool);

  return SVN_NO_ERROR;
}

static svn_error_t *
x_dir_optimal_order(apr_array_header_t **ordered_p,
                    svn_fs_root_t *root,
                    apr_hash_t *entries,
                    apr_pool_t *result_pool,
                    apr_pool_t *scratch_pool)
{
  *ordered_p = svn_fs_x__order_dir_entries(root->fs, entries, result_pool,
                                           scratch_pool);

  return SVN_NO_ERROR;
}

/* Create a new directory named PATH in ROOT.  The new directory has
   no entries, and no properties.  ROOT must be the root of a
   transaction, not a revision.  Do any necessary temporary allocation
   in SCRATCH_POOL.  */
static svn_error_t *
x_make_dir(svn_fs_root_t *root,
           const char *path,
           apr_pool_t *scratch_pool)
{
  parent_path_t *parent_path;
  dag_node_t *sub_dir;
  svn_fs_x__txn_id_t txn_id = root_txn_id(root);
  apr_pool_t *subpool = svn_pool_create(scratch_pool);

  path = svn_fs__canonicalize_abspath(path, subpool);
  SVN_ERR(open_path(&parent_path, root, path, open_path_last_optional,
                    TRUE, subpool));

  /* Check (recursively) to see if some lock is 'reserving' a path at
     that location, or even some child-path; if so, check that we can
     use it. */
  if (root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
    SVN_ERR(svn_fs_x__allow_locked_operation(path, root->fs, TRUE, FALSE,
                                             subpool));

  /* If there's already a sub-directory by that name, complain.  This
     also catches the case of trying to make a subdirectory named `/'.  */
  if (parent_path->node)
    return SVN_FS__ALREADY_EXISTS(root, path);

  /* Create the subdirectory.  */
  SVN_ERR(make_path_mutable(root, parent_path->parent, path, subpool,
                            subpool));
  SVN_ERR(svn_fs_x__dag_make_dir(&sub_dir,
                                 parent_path->parent->node,
                                 parent_path_path(parent_path->parent,
                                                  subpool),
                                 parent_path->entry,
                                 txn_id,
                                 subpool, subpool));

  /* Add this directory to the path cache. */
  SVN_ERR(dag_node_cache_set(root, parent_path_path(parent_path, subpool),
                             sub_dir, subpool));

  /* Make a record of this modification in the changes table. */
  SVN_ERR(add_change(root->fs, txn_id, path, svn_fs_x__dag_get_id(sub_dir),
                     svn_fs_path_change_add, FALSE, FALSE, FALSE,
                     svn_node_dir, SVN_INVALID_REVNUM, NULL, subpool));

  svn_pool_destroy(subpool);
  return SVN_NO_ERROR;
}


/* Delete the node at PATH under ROOT.  ROOT must be a transaction
   root.  Perform temporary allocations in SCRATCH_POOL. */
static svn_error_t *
x_delete_node(svn_fs_root_t *root,
              const char *path,
              apr_pool_t *scratch_pool)
{
  parent_path_t *parent_path;
  svn_fs_x__txn_id_t txn_id;
  apr_int64_t mergeinfo_count = 0;
  svn_node_kind_t kind;
  apr_pool_t *subpool = svn_pool_create(scratch_pool);

  if (! root->is_txn_root)
    return SVN_FS__NOT_TXN(root);

  txn_id = root_txn_id(root);
  path = svn_fs__canonicalize_abspath(path, subpool);
  SVN_ERR(open_path(&parent_path, root, path, 0, TRUE, subpool));
  kind = svn_fs_x__dag_node_kind(parent_path->node);

  /* We can't remove the root of the filesystem.  */
  if (! parent_path->parent)
    return svn_error_create(SVN_ERR_FS_ROOT_DIR, NULL,
                            _("The root directory cannot be deleted"));

  /* Check to see if path (or any child thereof) is locked; if so,
     check that we can use the existing lock(s). */
  if (root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
    SVN_ERR(svn_fs_x__allow_locked_operation(path, root->fs, TRUE, FALSE,
                                             subpool));

  /* Make the parent directory mutable, and do the deletion.  */
  SVN_ERR(make_path_mutable(root, parent_path->parent, path, subpool,
                            subpool));
  SVN_ERR(svn_fs_x__dag_get_mergeinfo_count(&mergeinfo_count,
                                            parent_path->node));
  SVN_ERR(svn_fs_x__dag_delete(parent_path->parent->node,
                               parent_path->entry,
                               txn_id, subpool));

  /* Remove this node and any children from the path cache. */
  SVN_ERR(dag_node_cache_invalidate(root, parent_path_path(parent_path,
                                                           subpool),
                                    subpool));

  /* Update mergeinfo counts for parents */
  if (mergeinfo_count > 0)
    SVN_ERR(increment_mergeinfo_up_tree(parent_path->parent,
                                        -mergeinfo_count,
                                        subpool));

  /* Make a record of this modification in the changes table. */
  SVN_ERR(add_change(root->fs, txn_id, path,
                     svn_fs_x__dag_get_id(parent_path->node),
                     svn_fs_path_change_delete, FALSE, FALSE, FALSE, kind,
                     SVN_INVALID_REVNUM, NULL, subpool));

  svn_pool_destroy(subpool);
  return SVN_NO_ERROR;
}


/* Set *SAME_P to TRUE if FS1 and FS2 have the same UUID, else set to FALSE.
   Use SCRATCH_POOL for temporary allocation only.
   Note: this code is duplicated between libsvn_fs_x and libsvn_fs_base. */
static svn_error_t *
x_same_p(svn_boolean_t *same_p,
         svn_fs_t *fs1,
         svn_fs_t *fs2,
         apr_pool_t *scratch_pool)
{
  *same_p = ! strcmp(fs1->uuid, fs2->uuid);
  return SVN_NO_ERROR;
}

/* Copy the node at FROM_PATH under FROM_ROOT to TO_PATH under
   TO_ROOT.  If PRESERVE_HISTORY is set, then the copy is recorded in
   the copies table.  Perform temporary allocations in SCRATCH_POOL. */
static svn_error_t *
copy_helper(svn_fs_root_t *from_root,
            const char *from_path,
            svn_fs_root_t *to_root,
            const char *to_path,
            svn_boolean_t preserve_history,
            apr_pool_t *scratch_pool)
{
  dag_node_t *from_node;
  parent_path_t *to_parent_path;
  svn_fs_x__txn_id_t txn_id = root_txn_id(to_root);
  svn_boolean_t same_p;

  /* Use an error check, not an assert, because even the caller cannot
     guarantee that a filesystem's UUID has not changed "on the fly". */
  SVN_ERR(x_same_p(&same_p, from_root->fs, to_root->fs, scratch_pool));
  if (! same_p)
    return svn_error_createf
      (SVN_ERR_UNSUPPORTED_FEATURE, NULL,
       _("Cannot copy between two different filesystems ('%s' and '%s')"),
       from_root->fs->path, to_root->fs->path);

  /* more things that we can't do ATM */
  if (from_root->is_txn_root)
    return svn_error_create
      (SVN_ERR_UNSUPPORTED_FEATURE, NULL,
       _("Copy from mutable tree not currently supported"));

  if (! to_root->is_txn_root)
    return svn_error_create
      (SVN_ERR_UNSUPPORTED_FEATURE, NULL,
       _("Copy immutable tree not supported"));

  /* Get the NODE for FROM_PATH in FROM_ROOT.*/
  SVN_ERR(get_dag(&from_node, from_root, from_path, scratch_pool));

  /* Build up the parent path from TO_PATH in TO_ROOT.  If the last
     component does not exist, it's not that big a deal.  We'll just
     make one there. */
  SVN_ERR(open_path(&to_parent_path, to_root, to_path,
                    open_path_last_optional, TRUE, scratch_pool));

  /* Check to see if path (or any child thereof) is locked; if so,
     check that we can use the existing lock(s). */
  if (to_root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
    SVN_ERR(svn_fs_x__allow_locked_operation(to_path, to_root->fs,
                                             TRUE, FALSE, scratch_pool));

  /* If the destination node already exists as the same node as the
     source (in other words, this operation would result in nothing
     happening at all), just do nothing an return successfully,
     proud that you saved yourself from a tiresome task. */
  if (to_parent_path->node &&
      svn_fs_x__id_eq(svn_fs_x__dag_get_id(from_node),
                      svn_fs_x__dag_get_id(to_parent_path->node)))
    return SVN_NO_ERROR;

  if (! from_root->is_txn_root)
    {
      svn_fs_path_change_kind_t kind;
      dag_node_t *new_node;
      const char *from_canonpath;
      apr_int64_t mergeinfo_start;
      apr_int64_t mergeinfo_end;

      /* If TO_PATH already existed prior to the copy, note that this
         operation is a replacement, not an addition. */
      if (to_parent_path->node)
        {
          kind = svn_fs_path_change_replace;
          SVN_ERR(svn_fs_x__dag_get_mergeinfo_count(&mergeinfo_start,
                                                    to_parent_path->node));
        }
      else
        {
          kind = svn_fs_path_change_add;
          mergeinfo_start = 0;
        }

      SVN_ERR(svn_fs_x__dag_get_mergeinfo_count(&mergeinfo_end, from_node));

      /* Make sure the target node's parents are mutable.  */
      SVN_ERR(make_path_mutable(to_root, to_parent_path->parent,
                                to_path, scratch_pool, scratch_pool));

      /* Canonicalize the copyfrom path. */
      from_canonpath = svn_fs__canonicalize_abspath(from_path, scratch_pool);

      SVN_ERR(svn_fs_x__dag_copy(to_parent_path->parent->node,
                                 to_parent_path->entry,
                                 from_node,
                                 preserve_history,
                                 from_root->rev,
                                 from_canonpath,
                                 txn_id, scratch_pool));

      if (kind != svn_fs_path_change_add)
        SVN_ERR(dag_node_cache_invalidate(to_root,
                                          parent_path_path(to_parent_path,
                                                           scratch_pool),
                                          scratch_pool));

      if (mergeinfo_start != mergeinfo_end)
        SVN_ERR(increment_mergeinfo_up_tree(to_parent_path->parent,
                                            mergeinfo_end - mergeinfo_start,
                                            scratch_pool));

      /* Make a record of this modification in the changes table. */
      SVN_ERR(get_dag(&new_node, to_root, to_path, scratch_pool));
      SVN_ERR(add_change(to_root->fs, txn_id, to_path,
                         svn_fs_x__dag_get_id(new_node), kind, FALSE,
                         FALSE, FALSE, svn_fs_x__dag_node_kind(from_node),
                         from_root->rev, from_canonpath, scratch_pool));
    }
  else
    {
      /* See IZ Issue #436 */
      /* Copying from transaction roots not currently available.

         ### cmpilato todo someday: make this not so. :-) Note that
         when copying from mutable trees, you have to make sure that
         you aren't creating a cyclic graph filesystem, and a simple
         referencing operation won't cut it.  Currently, we should not
         be able to reach this clause, and the interface reports that
         this only works from immutable trees anyway, but JimB has
         stated that this requirement need not be necessary in the
         future. */

      SVN_ERR_MALFUNCTION();
    }

  return SVN_NO_ERROR;
}


/* Create a copy of FROM_PATH in FROM_ROOT named TO_PATH in TO_ROOT.
   If FROM_PATH is a directory, copy it recursively.  Temporary
   allocations are from SCRATCH_POOL.*/
static svn_error_t *
x_copy(svn_fs_root_t *from_root,
       const char *from_path,
       svn_fs_root_t *to_root,
       const char *to_path,
       apr_pool_t *scratch_pool)
{
  apr_pool_t *subpool = svn_pool_create(scratch_pool);

  SVN_ERR(copy_helper(from_root,
                      svn_fs__canonicalize_abspath(from_path, subpool),
                      to_root,
                      svn_fs__canonicalize_abspath(to_path, subpool),
                      TRUE, subpool));

  svn_pool_destroy(subpool);

  return SVN_NO_ERROR;
}


/* Create a copy of FROM_PATH in FROM_ROOT named TO_PATH in TO_ROOT.
   If FROM_PATH is a directory, copy it recursively.  No history is
   preserved.  Temporary allocations are from SCRATCH_POOL. */
static svn_error_t *
x_revision_link(svn_fs_root_t *from_root,
                svn_fs_root_t *to_root,
                const char *path,
                apr_pool_t *scratch_pool)
{
  apr_pool_t *subpool;

  if (! to_root->is_txn_root)
    return SVN_FS__NOT_TXN(to_root);

  subpool = svn_pool_create(scratch_pool);

  path = svn_fs__canonicalize_abspath(path, subpool);
  SVN_ERR(copy_helper(from_root, path, to_root, path, FALSE, subpool));

  svn_pool_destroy(subpool);

  return SVN_NO_ERROR;
}


/* Discover the copy ancestry of PATH under ROOT.  Return a relevant
   ancestor/revision combination in *PATH_P and *REV_P.  Temporary
   allocations are in POOL. */
static svn_error_t *
x_copied_from(svn_revnum_t *rev_p,
              const char **path_p,
              svn_fs_root_t *root,
              const char *path,
              apr_pool_t *pool)
{
  dag_node_t *node;

  /* There is no cached entry, look it up the old-fashioned
      way. */
  SVN_ERR(get_dag(&node, root, path, pool));
  SVN_ERR(svn_fs_x__dag_get_copyfrom_rev(rev_p, node));
  SVN_ERR(svn_fs_x__dag_get_copyfrom_path(path_p, node));

  return SVN_NO_ERROR;
}



/* Files.  */

/* Create the empty file PATH under ROOT.  Temporary allocations are
   in SCRATCH_POOL. */
static svn_error_t *
x_make_file(svn_fs_root_t *root,
            const char *path,
            apr_pool_t *scratch_pool)
{
  parent_path_t *parent_path;
  dag_node_t *child;
  svn_fs_x__txn_id_t txn_id = root_txn_id(root);
  apr_pool_t *subpool = svn_pool_create(scratch_pool);

  path = svn_fs__canonicalize_abspath(path, subpool);
  SVN_ERR(open_path(&parent_path, root, path, open_path_last_optional,
                    TRUE, subpool));

  /* If there's already a file by that name, complain.
     This also catches the case of trying to make a file named `/'.  */
  if (parent_path->node)
    return SVN_FS__ALREADY_EXISTS(root, path);

  /* Check (non-recursively) to see if path is locked;  if so, check
     that we can use it. */
  if (root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
    SVN_ERR(svn_fs_x__allow_locked_operation(path, root->fs, FALSE, FALSE,
                                             subpool));

  /* Create the file.  */
  SVN_ERR(make_path_mutable(root, parent_path->parent, path, subpool,
                            subpool));
  SVN_ERR(svn_fs_x__dag_make_file(&child,
                                  parent_path->parent->node,
                                  parent_path_path(parent_path->parent,
                                                   subpool),
                                  parent_path->entry,
                                  txn_id,
                                  subpool, subpool));

  /* Add this file to the path cache. */
  SVN_ERR(dag_node_cache_set(root, parent_path_path(parent_path, subpool),
                             child, subpool));

  /* Make a record of this modification in the changes table. */
  SVN_ERR(add_change(root->fs, txn_id, path, svn_fs_x__dag_get_id(child),
                     svn_fs_path_change_add, TRUE, FALSE, FALSE,
                     svn_node_file, SVN_INVALID_REVNUM, NULL, subpool));

  svn_pool_destroy(subpool);
  return SVN_NO_ERROR;
}


/* Set *LENGTH_P to the size of the file PATH under ROOT.  Temporary
   allocations are in SCRATCH_POOL. */
static svn_error_t *
x_file_length(svn_filesize_t *length_p,
              svn_fs_root_t *root,
              const char *path,
              apr_pool_t *scratch_pool)
{
  dag_node_t *file;

  /* First create a dag_node_t from the root/path pair. */
  SVN_ERR(get_dag(&file, root, path, scratch_pool));

  /* Now fetch its length */
  return svn_fs_x__dag_file_length(length_p, file);
}


/* Set *CHECKSUM to the checksum of type KIND for PATH under ROOT, or
   NULL if that information isn't available.  Temporary allocations
   are from POOL. */
static svn_error_t *
x_file_checksum(svn_checksum_t **checksum,
                svn_checksum_kind_t kind,
                svn_fs_root_t *root,
                const char *path,
                apr_pool_t *pool)
{
  dag_node_t *file;

  SVN_ERR(get_dag(&file, root, path, pool));
  return svn_fs_x__dag_file_checksum(checksum, file, kind, pool);
}


/* --- Machinery for svn_fs_file_contents() ---  */

/* Set *CONTENTS to a readable stream that will return the contents of
   PATH under ROOT.  The stream is allocated in POOL. */
static svn_error_t *
x_file_contents(svn_stream_t **contents,
                svn_fs_root_t *root,
                const char *path,
                apr_pool_t *pool)
{
  dag_node_t *node;
  svn_stream_t *file_stream;

  /* First create a dag_node_t from the root/path pair. */
  SVN_ERR(get_dag(&node, root, path, pool));

  /* Then create a readable stream from the dag_node_t. */
  SVN_ERR(svn_fs_x__dag_get_contents(&file_stream, node, pool));

  *contents = file_stream;
  return SVN_NO_ERROR;
}

/* --- End machinery for svn_fs_file_contents() ---  */


/* --- Machinery for svn_fs_try_process_file_contents() ---  */

static svn_error_t *
x_try_process_file_contents(svn_boolean_t *success,
                            svn_fs_root_t *root,
                            const char *path,
                            svn_fs_process_contents_func_t processor,
                            void* baton,
                            apr_pool_t *pool)
{
  dag_node_t *node;
  SVN_ERR(get_dag(&node, root, path, pool));

  return svn_fs_x__dag_try_process_file_contents(success, node,
                                                 processor, baton, pool);
}

/* --- End machinery for svn_fs_try_process_file_contents() ---  */


/* --- Machinery for svn_fs_apply_textdelta() ---  */


/* Local baton type for all the helper functions below. */
typedef struct txdelta_baton_t
{
  /* This is the custom-built window consumer given to us by the delta
     library;  it uniquely knows how to read data from our designated
     "source" stream, interpret the window, and write data to our
     designated "target" stream (in this case, our repos file.) */
  svn_txdelta_window_handler_t interpreter;
  void *interpreter_baton;

  /* The original file info */
  svn_fs_root_t *root;
  const char *path;

  /* Derived from the file info */
  dag_node_t *node;

  svn_stream_t *source_stream;
  svn_stream_t *target_stream;

  /* MD5 digest for the base text against which a delta is to be
     applied, and for the resultant fulltext, respectively.  Either or
     both may be null, in which case ignored. */
  svn_checksum_t *base_checksum;
  svn_checksum_t *result_checksum;

  /* Pool used by db txns */
  apr_pool_t *pool;

} txdelta_baton_t;


/* The main window handler returned by svn_fs_apply_textdelta. */
static svn_error_t *
window_consumer(svn_txdelta_window_t *window, void *baton)
{
  txdelta_baton_t *tb = (txdelta_baton_t *) baton;

  /* Send the window right through to the custom window interpreter.
     In theory, the interpreter will then write more data to
     cb->target_string. */
  SVN_ERR(tb->interpreter(window, tb->interpreter_baton));

  /* Is the window NULL?  If so, we're done.  The stream has already been
     closed by the interpreter. */
  if (! window)
    SVN_ERR(svn_fs_x__dag_finalize_edits(tb->node, tb->result_checksum,
                                         tb->pool));

  return SVN_NO_ERROR;
}

/* Helper function for fs_apply_textdelta.  BATON is of type
   txdelta_baton_t. */
static svn_error_t *
apply_textdelta(void *baton,
                apr_pool_t *scratch_pool)
{
  txdelta_baton_t *tb = (txdelta_baton_t *) baton;
  parent_path_t *parent_path;
  svn_fs_x__txn_id_t txn_id = root_txn_id(tb->root);

  /* Call open_path with no flags, as we want this to return an error
     if the node for which we are searching doesn't exist. */
  SVN_ERR(open_path(&parent_path, tb->root, tb->path, 0, TRUE, scratch_pool));

  /* Check (non-recursively) to see if path is locked; if so, check
     that we can use it. */
  if (tb->root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
    SVN_ERR(svn_fs_x__allow_locked_operation(tb->path, tb->root->fs,
                                             FALSE, FALSE, scratch_pool));

  /* Now, make sure this path is mutable. */
  SVN_ERR(make_path_mutable(tb->root, parent_path, tb->path, scratch_pool,
                            scratch_pool));
  tb->node = svn_fs_x__dag_dup(parent_path->node, tb->pool);

  if (tb->base_checksum)
    {
      svn_checksum_t *checksum;

      /* Until we finalize the node, its data_key points to the old
         contents, in other words, the base text. */
      SVN_ERR(svn_fs_x__dag_file_checksum(&checksum, tb->node,
                                          tb->base_checksum->kind,
                                          scratch_pool));
      if (!svn_checksum_match(tb->base_checksum, checksum))
        return svn_checksum_mismatch_err(tb->base_checksum, checksum,
                                         scratch_pool,
                                         _("Base checksum mismatch on '%s'"),
                                         tb->path);
    }

  /* Make a readable "source" stream out of the current contents of
     ROOT/PATH; obviously, this must done in the context of a db_txn.
     The stream is returned in tb->source_stream. */
  SVN_ERR(svn_fs_x__dag_get_contents(&(tb->source_stream),
                                     tb->node, tb->pool));

  /* Make a writable "target" stream */
  SVN_ERR(svn_fs_x__dag_get_edit_stream(&(tb->target_stream), tb->node,
                                        tb->pool));

  /* Now, create a custom window handler that uses our two streams. */
  svn_txdelta_apply(tb->source_stream,
                    tb->target_stream,
                    NULL,
                    tb->path,
                    tb->pool,
                    &(tb->interpreter),
                    &(tb->interpreter_baton));

  /* Make a record of this modification in the changes table. */
  return add_change(tb->root->fs, txn_id, tb->path,
                    svn_fs_x__dag_get_id(tb->node),
                    svn_fs_path_change_modify, TRUE, FALSE, FALSE,
                    svn_node_file, SVN_INVALID_REVNUM, NULL, scratch_pool);
}


/* Set *CONTENTS_P and *CONTENTS_BATON_P to a window handler and baton
   that will accept text delta windows to modify the contents of PATH
   under ROOT.  Allocations are in POOL. */
static svn_error_t *
x_apply_textdelta(svn_txdelta_window_handler_t *contents_p,
                  void **contents_baton_p,
                  svn_fs_root_t *root,
                  const char *path,
                  svn_checksum_t *base_checksum,
                  svn_checksum_t *result_checksum,
                  apr_pool_t *pool)
{
  apr_pool_t *scratch_pool = svn_pool_create(pool);
  txdelta_baton_t *tb = apr_pcalloc(pool, sizeof(*tb));

  tb->root = root;
  tb->path = svn_fs__canonicalize_abspath(path, pool);
  tb->pool = pool;
  tb->base_checksum = svn_checksum_dup(base_checksum, pool);
  tb->result_checksum = svn_checksum_dup(result_checksum, pool);

  SVN_ERR(apply_textdelta(tb, scratch_pool));

  *contents_p = window_consumer;
  *contents_baton_p = tb;

  svn_pool_destroy(scratch_pool);
  return SVN_NO_ERROR;
}

/* --- End machinery for svn_fs_apply_textdelta() ---  */

/* --- Machinery for svn_fs_apply_text() ---  */

/* Baton for svn_fs_apply_text(). */
typedef struct text_baton_t
{
  /* The original file info */
  svn_fs_root_t *root;
  const char *path;

  /* Derived from the file info */
  dag_node_t *node;

  /* The returned stream that will accept the file's new contents. */
  svn_stream_t *stream;

  /* The actual fs stream that the returned stream will write to. */
  svn_stream_t *file_stream;

  /* MD5 digest for the final fulltext written to the file.  May
     be null, in which case ignored. */
  svn_checksum_t *result_checksum;

  /* Pool used by db txns */
  apr_pool_t *pool;
} text_baton_t;


/* A wrapper around svn_fs_x__dag_finalize_edits, but for
 * fulltext data, not text deltas.  Closes BATON->file_stream.
 *
 * Note: If you're confused about how this function relates to another
 * of similar name, think of it this way:
 *
 * svn_fs_apply_textdelta() ==> ... ==> txn_body_txdelta_finalize_edits()
 * svn_fs_apply_text()      ==> ... ==> txn_body_fulltext_finalize_edits()
 */

/* Write function for the publically returned stream. */
static svn_error_t *
text_stream_writer(void *baton,
                   const char *data,
                   apr_size_t *len)
{
  text_baton_t *tb = baton;

  /* Psst, here's some data.  Pass it on to the -real- file stream. */
  return svn_stream_write(tb->file_stream, data, len);
}

/* Close function for the publically returned stream. */
static svn_error_t *
text_stream_closer(void *baton)
{
  text_baton_t *tb = baton;

  /* Close the internal-use stream.  ### This used to be inside of
     txn_body_fulltext_finalize_edits(), but that invoked a nested
     Berkeley DB transaction -- scandalous! */
  SVN_ERR(svn_stream_close(tb->file_stream));

  /* Need to tell fs that we're done sending text */
  return svn_fs_x__dag_finalize_edits(tb->node, tb->result_checksum,
                                       tb->pool);
}


/* Helper function for fs_apply_text.  BATON is of type
   text_baton_t. */
static svn_error_t *
apply_text(void *baton,
           apr_pool_t *scratch_pool)
{
  text_baton_t *tb = baton;
  parent_path_t *parent_path;
  svn_fs_x__txn_id_t txn_id = root_txn_id(tb->root);

  /* Call open_path with no flags, as we want this to return an error
     if the node for which we are searching doesn't exist. */
  SVN_ERR(open_path(&parent_path, tb->root, tb->path, 0, TRUE, scratch_pool));

  /* Check (non-recursively) to see if path is locked; if so, check
     that we can use it. */
  if (tb->root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
    SVN_ERR(svn_fs_x__allow_locked_operation(tb->path, tb->root->fs,
                                             FALSE, FALSE, scratch_pool));

  /* Now, make sure this path is mutable. */
  SVN_ERR(make_path_mutable(tb->root, parent_path, tb->path, scratch_pool,
                            scratch_pool));
  tb->node = svn_fs_x__dag_dup(parent_path->node, tb->pool);

  /* Make a writable stream for replacing the file's text. */
  SVN_ERR(svn_fs_x__dag_get_edit_stream(&(tb->file_stream), tb->node,
                                        tb->pool));

  /* Create a 'returnable' stream which writes to the file_stream. */
  tb->stream = svn_stream_create(tb, tb->pool);
  svn_stream_set_write(tb->stream, text_stream_writer);
  svn_stream_set_close(tb->stream, text_stream_closer);

  /* Make a record of this modification in the changes table. */
  return add_change(tb->root->fs, txn_id, tb->path,
                    svn_fs_x__dag_get_id(tb->node),
                    svn_fs_path_change_modify, TRUE, FALSE, FALSE,
                    svn_node_file, SVN_INVALID_REVNUM, NULL, scratch_pool);
}


/* Return a writable stream that will set the contents of PATH under
   ROOT.  RESULT_CHECKSUM is the MD5 checksum of the final result.
   Temporary allocations are in POOL. */
static svn_error_t *
x_apply_text(svn_stream_t **contents_p,
             svn_fs_root_t *root,
             const char *path,
             svn_checksum_t *result_checksum,
             apr_pool_t *pool)
{
  apr_pool_t *scratch_pool = svn_pool_create(pool);
  text_baton_t *tb = apr_pcalloc(pool, sizeof(*tb));

  tb->root = root;
  tb->path = svn_fs__canonicalize_abspath(path, pool);
  tb->pool = pool;
  tb->result_checksum = svn_checksum_dup(result_checksum, pool);

  SVN_ERR(apply_text(tb, scratch_pool));

  *contents_p = tb->stream;

  svn_pool_destroy(scratch_pool);
  return SVN_NO_ERROR;
}

/* --- End machinery for svn_fs_apply_text() ---  */


/* Check if the contents of PATH1 under ROOT1 are different from the
   contents of PATH2 under ROOT2.  If they are different set
   *CHANGED_P to TRUE, otherwise set it to FALSE. */
static svn_error_t *
x_contents_changed(svn_boolean_t *changed_p,
                   svn_fs_root_t *root1,
                   const char *path1,
                   svn_fs_root_t *root2,
                   const char *path2,
                   svn_boolean_t strict,
                   apr_pool_t *scratch_pool)
{
  dag_node_t *node1, *node2;
  apr_pool_t *subpool = svn_pool_create(scratch_pool);

  /* Check that roots are in the same fs. */
  if (root1->fs != root2->fs)
    return svn_error_create
      (SVN_ERR_FS_GENERAL, NULL,
       _("Cannot compare file contents between two different filesystems"));

  /* Check that both paths are files. */
  {
    svn_node_kind_t kind;

    SVN_ERR(svn_fs_x__check_path(&kind, root1, path1, subpool));
    if (kind != svn_node_file)
      return svn_error_createf
        (SVN_ERR_FS_GENERAL, NULL, _("'%s' is not a file"), path1);

    SVN_ERR(svn_fs_x__check_path(&kind, root2, path2, subpool));
    if (kind != svn_node_file)
      return svn_error_createf
        (SVN_ERR_FS_GENERAL, NULL, _("'%s' is not a file"), path2);
  }

  SVN_ERR(get_dag(&node1, root1, path1, subpool));
  SVN_ERR(get_dag(&node2, root2, path2, subpool));
  SVN_ERR(svn_fs_x__dag_things_different(NULL, changed_p, node1, node2,
                                         strict, subpool));

  svn_pool_destroy(subpool);
  return SVN_NO_ERROR;
}



/* Public interface to computing file text deltas.  */

static svn_error_t *
x_get_file_delta_stream(svn_txdelta_stream_t **stream_p,
                        svn_fs_root_t *source_root,
                        const char *source_path,
                        svn_fs_root_t *target_root,
                        const char *target_path,
                        apr_pool_t *pool)
{
  dag_node_t *source_node, *target_node;
  apr_pool_t *scratch_pool = svn_pool_create(pool);

  if (source_root && source_path)
    SVN_ERR(get_dag(&source_node, source_root, source_path, scratch_pool));
  else
    source_node = NULL;
  SVN_ERR(get_dag(&target_node, target_root, target_path, scratch_pool));

  /* Create a delta stream that turns the source into the target.  */
  SVN_ERR(svn_fs_x__dag_get_file_delta_stream(stream_p, source_node,
                                              target_node, pool,
                                              scratch_pool));

  svn_pool_destroy(scratch_pool);
  return SVN_NO_ERROR;
}



/* Finding Changes */

/* Copy CHANGE into a FS API object allocated in RESULT_POOL and return
   it in *RESULT_P.  Pass CONTEXT to the ID API object being created. */
static svn_error_t *
construct_fs_path_change(svn_fs_path_change2_t **result_p,
                         svn_fs_x__id_context_t *context,
                         svn_fs_x__change_t *change,
                         apr_pool_t *result_pool)
{
  const svn_fs_id_t *id
    = svn_fs_x__id_create(context, &change->noderev_id, result_pool);
  svn_fs_path_change2_t *result
    = svn_fs__path_change_create_internal(id, change->change_kind,
                                          result_pool);

  result->text_mod = change->text_mod;
  result->prop_mod = change->prop_mod;
  result->node_kind = change->node_kind;

  result->copyfrom_known = change->copyfrom_known;
  result->copyfrom_rev = change->copyfrom_rev;
  result->copyfrom_path = change->copyfrom_path;

  result->mergeinfo_mod = change->mergeinfo_mod;

  *result_p = result;

  return SVN_NO_ERROR;
}

/* Set *CHANGED_PATHS_P to a newly allocated hash containing
   descriptions of the paths changed under ROOT.  The hash is keyed
   with const char * paths and has svn_fs_path_change2_t * values.  Use
   POOL for all allocations. */
static svn_error_t *
x_paths_changed(apr_hash_t **changed_paths_p,
                svn_fs_root_t *root,
                apr_pool_t *pool)
{
  apr_hash_t *changed_paths;
  svn_fs_path_change2_t *path_change;
  svn_fs_x__id_context_t *context
    = svn_fs_x__id_create_context(root->fs, pool);

  if (root->is_txn_root)
    {
      apr_hash_index_t *hi;
      SVN_ERR(svn_fs_x__txn_changes_fetch(&changed_paths, root->fs,
                                          root_txn_id(root), pool));
      for (hi = apr_hash_first(pool, changed_paths);
           hi;
           hi = apr_hash_next(hi))
        {
          svn_fs_x__change_t *change = apr_hash_this_val(hi);
          SVN_ERR(construct_fs_path_change(&path_change, context, change,
                                           pool));
          apr_hash_set(changed_paths,
                       apr_hash_this_key(hi), apr_hash_this_key_len(hi),
                       path_change);
        }
    }
  else
    {
      apr_array_header_t *changes;
      int i;

      SVN_ERR(svn_fs_x__get_changes(&changes, root->fs, root->rev, pool));

      changed_paths = svn_hash__make(pool);
      for (i = 0; i < changes->nelts; ++i)
        {
          svn_fs_x__change_t *change = APR_ARRAY_IDX(changes, i,
                                                     svn_fs_x__change_t *);
          SVN_ERR(construct_fs_path_change(&path_change, context, change,
                                           pool));
          apr_hash_set(changed_paths, change->path.data, change->path.len,
                       path_change);
        }
    }

  *changed_paths_p = changed_paths;

  return SVN_NO_ERROR;
}



/* Our coolio opaque history object. */
typedef struct fs_history_data_t
{
  /* filesystem object */
  svn_fs_t *fs;

  /* path and revision of historical location */
  const char *path;
  svn_revnum_t revision;

  /* internal-use hints about where to resume the history search. */
  const char *path_hint;
  svn_revnum_t rev_hint;

  /* FALSE until the first call to svn_fs_history_prev(). */
  svn_boolean_t is_interesting;
} fs_history_data_t;

static svn_fs_history_t *
assemble_history(svn_fs_t *fs,
                 const char *path,
                 svn_revnum_t revision,
                 svn_boolean_t is_interesting,
                 const char *path_hint,
                 svn_revnum_t rev_hint,
                 apr_pool_t *result_pool);


/* Set *HISTORY_P to an opaque node history object which represents
   PATH under ROOT.  ROOT must be a revision root.  Use POOL for all
   allocations. */
static svn_error_t *
x_node_history(svn_fs_history_t **history_p,
               svn_fs_root_t *root,
               const char *path,
               apr_pool_t *result_pool,
               apr_pool_t *scratch_pool)
{
  svn_node_kind_t kind;

  /* We require a revision root. */
  if (root->is_txn_root)
    return svn_error_create(SVN_ERR_FS_NOT_REVISION_ROOT, NULL, NULL);

  /* And we require that the path exist in the root. */
  SVN_ERR(svn_fs_x__check_path(&kind, root, path, scratch_pool));
  if (kind == svn_node_none)
    return SVN_FS__NOT_FOUND(root, path);

  /* Okay, all seems well.  Build our history object and return it. */
  *history_p = assemble_history(root->fs, path, root->rev, FALSE, NULL,
                                SVN_INVALID_REVNUM, result_pool);
  return SVN_NO_ERROR;
}

/* Find the youngest copyroot for path PARENT_PATH or its parents in
   filesystem FS, and store the copyroot in *REV_P and *PATH_P. */
static svn_error_t *
find_youngest_copyroot(svn_revnum_t *rev_p,
                       const char **path_p,
                       svn_fs_t *fs,
                       parent_path_t *parent_path)
{
  svn_revnum_t rev_mine;
  svn_revnum_t rev_parent = SVN_INVALID_REVNUM;
  const char *path_mine;
  const char *path_parent = NULL;

  /* First find our parent's youngest copyroot. */
  if (parent_path->parent)
    SVN_ERR(find_youngest_copyroot(&rev_parent, &path_parent, fs,
                                   parent_path->parent));

  /* Find our copyroot. */
  SVN_ERR(svn_fs_x__dag_get_copyroot(&rev_mine, &path_mine,
                                     parent_path->node));

  /* If a parent and child were copied to in the same revision, prefer
     the child copy target, since it is the copy relevant to the
     history of the child. */
  if (rev_mine >= rev_parent)
    {
      *rev_p = rev_mine;
      *path_p = path_mine;
    }
  else
    {
      *rev_p = rev_parent;
      *path_p = path_parent;
    }

  return SVN_NO_ERROR;
}


static svn_error_t *
x_closest_copy(svn_fs_root_t **root_p,
               const char **path_p,
               svn_fs_root_t *root,
               const char *path,
               apr_pool_t *pool)
{
  svn_fs_t *fs = root->fs;
  parent_path_t *parent_path, *copy_dst_parent_path;
  svn_revnum_t copy_dst_rev, created_rev;
  const char *copy_dst_path;
  svn_fs_root_t *copy_dst_root;
  dag_node_t *copy_dst_node;
  svn_boolean_t related;
  apr_pool_t *scratch_pool = svn_pool_create(pool);

  /* Initialize return values. */
  *root_p = NULL;
  *path_p = NULL;

  path = svn_fs__canonicalize_abspath(path, scratch_pool);
  SVN_ERR(open_path(&parent_path, root, path, 0, FALSE, scratch_pool));

  /* Find the youngest copyroot in the path of this node-rev, which
     will indicate the target of the innermost copy affecting the
     node-rev. */
  SVN_ERR(find_youngest_copyroot(&copy_dst_rev, &copy_dst_path,
                                 fs, parent_path));
  if (copy_dst_rev == 0)  /* There are no copies affecting this node-rev. */
    {
      svn_pool_destroy(scratch_pool);
      return SVN_NO_ERROR;
    }

  /* It is possible that this node was created from scratch at some
     revision between COPY_DST_REV and REV.  Make sure that PATH
     exists as of COPY_DST_REV and is related to this node-rev. */
  SVN_ERR(svn_fs_x__revision_root(&copy_dst_root, fs, copy_dst_rev, pool));
  SVN_ERR(open_path(&copy_dst_parent_path, copy_dst_root, path,
                    open_path_node_only | open_path_allow_null, FALSE,
                    scratch_pool));
  if (copy_dst_parent_path == NULL)
    {
      svn_pool_destroy(scratch_pool);
      return SVN_NO_ERROR;
    }

  copy_dst_node = copy_dst_parent_path->node;
  SVN_ERR(svn_fs_x__dag_related_node(&related, copy_dst_node,
                                     parent_path->node));
  if (!related)
    {
      svn_pool_destroy(scratch_pool);
      return SVN_NO_ERROR;
    }

  /* One final check must be done here.  If you copy a directory and
     create a new entity somewhere beneath that directory in the same
     txn, then we can't claim that the copy affected the new entity.
     For example, if you do:

        copy dir1 dir2
        create dir2/new-thing
        commit

     then dir2/new-thing was not affected by the copy of dir1 to dir2.
     We detect this situation by asking if PATH@COPY_DST_REV's
     created-rev is COPY_DST_REV, and that node-revision has no
     predecessors, then there is no relevant closest copy.
  */
  created_rev = svn_fs_x__dag_get_revision(copy_dst_node);
  if (created_rev == copy_dst_rev)
    {
      svn_fs_x__id_t pred;
      SVN_ERR(svn_fs_x__dag_get_predecessor_id(&pred, copy_dst_node));
      if (!svn_fs_x__id_used(&pred))
        {
          svn_pool_destroy(scratch_pool);
          return SVN_NO_ERROR;
        }
    }

  /* The copy destination checks out.  Return it. */
  *root_p = copy_dst_root;
  *path_p = apr_pstrdup(pool, copy_dst_path);

  svn_pool_destroy(scratch_pool);
  return SVN_NO_ERROR;
}


static svn_error_t *
x_node_origin_rev(svn_revnum_t *revision,
                  svn_fs_root_t *root,
                  const char *path,
                  apr_pool_t *scratch_pool)
{
  svn_fs_x__id_t node_id;
  dag_node_t *node;

  path = svn_fs__canonicalize_abspath(path, scratch_pool);

  SVN_ERR(get_dag(&node, root, path, scratch_pool));
  SVN_ERR(svn_fs_x__dag_get_node_id(&node_id, node));

  *revision = svn_fs_x__get_revnum(node_id.change_set);

  return SVN_NO_ERROR;
}


static svn_error_t *
history_prev(svn_fs_history_t **prev_history,
             svn_fs_history_t *history,
             svn_boolean_t cross_copies,
             apr_pool_t *result_pool,
             apr_pool_t *scratch_pool)
{
  fs_history_data_t *fhd = history->fsap_data;
  const char *commit_path, *src_path, *path = fhd->path;
  svn_revnum_t commit_rev, src_rev, dst_rev;
  svn_revnum_t revision = fhd->revision;
  svn_fs_t *fs = fhd->fs;
  parent_path_t *parent_path;
  dag_node_t *node;
  svn_fs_root_t *root;
  svn_boolean_t reported = fhd->is_interesting;
  svn_revnum_t copyroot_rev;
  const char *copyroot_path;

  /* Initialize our return value. */
  *prev_history = NULL;

  /* If our last history report left us hints about where to pickup
     the chase, then our last report was on the destination of a
     copy.  If we are crossing copies, start from those locations,
     otherwise, we're all done here.  */
  if (fhd->path_hint && SVN_IS_VALID_REVNUM(fhd->rev_hint))
    {
      reported = FALSE;
      if (! cross_copies)
        return SVN_NO_ERROR;
      path = fhd->path_hint;
      revision = fhd->rev_hint;
    }

  /* Construct a ROOT for the current revision. */
  SVN_ERR(svn_fs_x__revision_root(&root, fs, revision, scratch_pool));

  /* Open PATH/REVISION, and get its node and a bunch of other
     goodies.  */
  SVN_ERR(open_path(&parent_path, root, path, 0, FALSE, scratch_pool));
  node = parent_path->node;
  commit_path = svn_fs_x__dag_get_created_path(node);
  commit_rev = svn_fs_x__dag_get_revision(node);

  /* The Subversion filesystem is written in such a way that a given
     line of history may have at most one interesting history point
     per filesystem revision.  Either that node was edited (and
     possibly copied), or it was copied but not edited.  And a copy
     source cannot be from the same revision as its destination.  So,
     if our history revision matches its node's commit revision, we
     know that ... */
  if (revision == commit_rev)
    {
      if (! reported)
        {
          /* ... we either have not yet reported on this revision (and
             need now to do so) ... */
          *prev_history = assemble_history(fs, commit_path,
                                           commit_rev, TRUE, NULL,
                                           SVN_INVALID_REVNUM, result_pool);
          return SVN_NO_ERROR;
        }
      else
        {
          /* ... or we *have* reported on this revision, and must now
             progress toward this node's predecessor (unless there is
             no predecessor, in which case we're all done!). */
          svn_fs_x__id_t pred_id;

          SVN_ERR(svn_fs_x__dag_get_predecessor_id(&pred_id, node));
          if (!svn_fs_x__id_used(&pred_id))
            return SVN_NO_ERROR;

          /* Replace NODE and friends with the information from its
             predecessor. */
          SVN_ERR(svn_fs_x__dag_get_node(&node, fs, &pred_id, scratch_pool,
                                         scratch_pool));
          commit_path = svn_fs_x__dag_get_created_path(node);
          commit_rev = svn_fs_x__dag_get_revision(node);
        }
    }

  /* Find the youngest copyroot in the path of this node, including
     itself. */
  SVN_ERR(find_youngest_copyroot(&copyroot_rev, &copyroot_path, fs,
                                 parent_path));

  /* Initialize some state variables. */
  src_path = NULL;
  src_rev = SVN_INVALID_REVNUM;
  dst_rev = SVN_INVALID_REVNUM;

  if (copyroot_rev > commit_rev)
    {
      const char *remainder_path;
      const char *copy_dst, *copy_src;
      svn_fs_root_t *copyroot_root;

      SVN_ERR(svn_fs_x__revision_root(&copyroot_root, fs, copyroot_rev,
                                       scratch_pool));
      SVN_ERR(get_dag(&node, copyroot_root, copyroot_path, scratch_pool));
      copy_dst = svn_fs_x__dag_get_created_path(node);

      /* If our current path was the very destination of the copy,
         then our new current path will be the copy source.  If our
         current path was instead the *child* of the destination of
         the copy, then figure out its previous location by taking its
         path relative to the copy destination and appending that to
         the copy source.  Finally, if our current path doesn't meet
         one of these other criteria ... ### for now just fallback to
         the old copy hunt algorithm. */
      remainder_path = svn_fspath__skip_ancestor(copy_dst, path);

      if (remainder_path)
        {
          /* If we get here, then our current path is the destination
             of, or the child of the destination of, a copy.  Fill
             in the return values and get outta here.  */
          SVN_ERR(svn_fs_x__dag_get_copyfrom_rev(&src_rev, node));
          SVN_ERR(svn_fs_x__dag_get_copyfrom_path(&copy_src, node));

          dst_rev = copyroot_rev;
          src_path = svn_fspath__join(copy_src, remainder_path, scratch_pool);
        }
    }

  /* If we calculated a copy source path and revision, we'll make a
     'copy-style' history object. */
  if (src_path && SVN_IS_VALID_REVNUM(src_rev))
    {
      svn_boolean_t retry = FALSE;

      /* It's possible for us to find a copy location that is the same
         as the history point we've just reported.  If that happens,
         we simply need to take another trip through this history
         search. */
      if ((dst_rev == revision) && reported)
        retry = TRUE;

      *prev_history = assemble_history(fs, path, dst_rev, ! retry,
                                       src_path, src_rev, result_pool);
    }
  else
    {
      *prev_history = assemble_history(fs, commit_path, commit_rev, TRUE,
                                       NULL, SVN_INVALID_REVNUM, result_pool);
    }

  return SVN_NO_ERROR;
}


/* Implement svn_fs_history_prev, set *PREV_HISTORY_P to a new
   svn_fs_history_t object that represents the predecessory of
   HISTORY.  If CROSS_COPIES is true, *PREV_HISTORY_P may be related
   only through a copy operation.  Perform all allocations in POOL. */
static svn_error_t *
fs_history_prev(svn_fs_history_t **prev_history_p,
                svn_fs_history_t *history,
                svn_boolean_t cross_copies,
                apr_pool_t *result_pool,
                apr_pool_t *scratch_pool)
{
  svn_fs_history_t *prev_history = NULL;
  fs_history_data_t *fhd = history->fsap_data;
  svn_fs_t *fs = fhd->fs;

  /* Special case: the root directory changes in every single
     revision, no exceptions.  And, the root can't be the target (or
     child of a target -- duh) of a copy.  So, if that's our path,
     then we need only decrement our revision by 1, and there you go. */
  if (strcmp(fhd->path, "/") == 0)
    {
      if (! fhd->is_interesting)
        prev_history = assemble_history(fs, "/", fhd->revision,
                                        1, NULL, SVN_INVALID_REVNUM,
                                        result_pool);
      else if (fhd->revision > 0)
        prev_history = assemble_history(fs, "/", fhd->revision - 1,
                                        1, NULL, SVN_INVALID_REVNUM,
                                        result_pool);
    }
  else
    {
      apr_pool_t *iterpool = svn_pool_create(scratch_pool);
      prev_history = history;

      while (1)
        {
          svn_pool_clear(iterpool);
          SVN_ERR(history_prev(&prev_history, prev_history, cross_copies,
                               result_pool, iterpool));

          if (! prev_history)
            break;
          fhd = prev_history->fsap_data;
          if (fhd->is_interesting)
            break;
        }

      svn_pool_destroy(iterpool);
    }

  *prev_history_p = prev_history;
  return SVN_NO_ERROR;
}


/* Set *PATH and *REVISION to the path and revision for the HISTORY
   object.  Allocate *PATH in RESULT_POOL. */
static svn_error_t *
fs_history_location(const char **path,
                    svn_revnum_t *revision,
                    svn_fs_history_t *history,
                    apr_pool_t *result_pool)
{
  fs_history_data_t *fhd = history->fsap_data;

  *path = apr_pstrdup(result_pool, fhd->path);
  *revision = fhd->revision;
  return SVN_NO_ERROR;
}

static history_vtable_t history_vtable = {
  fs_history_prev,
  fs_history_location
};

/* Return a new history object (marked as "interesting") for PATH and
   REVISION, allocated in RESULT_POOL, and with its members set to the
   values of the parameters provided.  Note that PATH and PATH_HINT get
   normalized and duplicated in RESULT_POOL. */
static svn_fs_history_t *
assemble_history(svn_fs_t *fs,
                 const char *path,
                 svn_revnum_t revision,
                 svn_boolean_t is_interesting,
                 const char *path_hint,
                 svn_revnum_t rev_hint,
                 apr_pool_t *result_pool)
{
  svn_fs_history_t *history = apr_pcalloc(result_pool, sizeof(*history));
  fs_history_data_t *fhd = apr_pcalloc(result_pool, sizeof(*fhd));
  fhd->path = svn_fs__canonicalize_abspath(path, result_pool);
  fhd->revision = revision;
  fhd->is_interesting = is_interesting;
  fhd->path_hint = path_hint
                 ? svn_fs__canonicalize_abspath(path_hint, result_pool)
                 : NULL;
  fhd->rev_hint = rev_hint;
  fhd->fs = fs;

  history->vtable = &history_vtable;
  history->fsap_data = fhd;
  return history;
}


/* mergeinfo queries */


/* DIR_DAG is a directory DAG node which has mergeinfo in its
   descendants.  This function iterates over its children.  For each
   child with immediate mergeinfo, it adds its mergeinfo to
   RESULT_CATALOG.  appropriate arguments.  For each child with
   descendants with mergeinfo, it recurses.  Note that it does *not*
   call the action on the path for DIR_DAG itself.

   POOL is used for temporary allocations, including the mergeinfo
   hashes passed to actions; RESULT_POOL is used for the mergeinfo added
   to RESULT_CATALOG.
 */
static svn_error_t *
crawl_directory_dag_for_mergeinfo(svn_fs_root_t *root,
                                  const char *this_path,
                                  dag_node_t *dir_dag,
                                  svn_mergeinfo_catalog_t result_catalog,
                                  apr_pool_t *result_pool,
                                  apr_pool_t *scratch_pool)
{
  apr_array_header_t *entries;
  int i;
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);

  SVN_ERR(svn_fs_x__dag_dir_entries(&entries, dir_dag, scratch_pool,
                                    iterpool));
  for (i = 0; i < entries->nelts; ++i)
    {
      svn_fs_x__dirent_t *dirent = APR_ARRAY_IDX(entries, i, svn_fs_x__dirent_t *);
      const char *kid_path;
      dag_node_t *kid_dag;
      svn_boolean_t has_mergeinfo, go_down;

      svn_pool_clear(iterpool);

      kid_path = svn_fspath__join(this_path, dirent->name, iterpool);
      SVN_ERR(get_dag(&kid_dag, root, kid_path, iterpool));

      SVN_ERR(svn_fs_x__dag_has_mergeinfo(&has_mergeinfo, kid_dag));
      SVN_ERR(svn_fs_x__dag_has_descendants_with_mergeinfo(&go_down, kid_dag));

      if (has_mergeinfo)
        {
          /* Save this particular node's mergeinfo. */
          apr_hash_t *proplist;
          svn_mergeinfo_t kid_mergeinfo;
          svn_string_t *mergeinfo_string;
          svn_error_t *err;

          SVN_ERR(svn_fs_x__dag_get_proplist(&proplist, kid_dag, iterpool,
                                             iterpool));
          mergeinfo_string = svn_hash_gets(proplist, SVN_PROP_MERGEINFO);
          if (!mergeinfo_string)
            {
              svn_string_t *idstr
                = svn_fs_x__id_unparse(&dirent->id, iterpool);
              return svn_error_createf
                (SVN_ERR_FS_CORRUPT, NULL,
                 _("Node-revision #'%s' claims to have mergeinfo but doesn't"),
                 idstr->data);
            }

          /* Issue #3896: If a node has syntactically invalid mergeinfo, then
             treat it as if no mergeinfo is present rather than raising a parse
             error. */
          err = svn_mergeinfo_parse(&kid_mergeinfo,
                                    mergeinfo_string->data,
                                    result_pool);
          if (err)
            {
              if (err->apr_err == SVN_ERR_MERGEINFO_PARSE_ERROR)
                svn_error_clear(err);
              else
                return svn_error_trace(err);
              }
          else
            {
              svn_hash_sets(result_catalog, apr_pstrdup(result_pool, kid_path),
                            kid_mergeinfo);
            }
        }

      if (go_down)
        SVN_ERR(crawl_directory_dag_for_mergeinfo(root,
                                                  kid_path,
                                                  kid_dag,
                                                  result_catalog,
                                                  result_pool,
                                                  iterpool));
    }

  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}

/* Return the cache key as a combination of REV_ROOT->REV, the inheritance
   flags INHERIT and ADJUST_INHERITED_MERGEINFO, and the PATH.  The result
   will be allocated in RESULT_POOL.
 */
static const char *
mergeinfo_cache_key(const char *path,
                    svn_fs_root_t *rev_root,
                    svn_mergeinfo_inheritance_t inherit,
                    svn_boolean_t adjust_inherited_mergeinfo,
                    apr_pool_t *result_pool)
{
  apr_int64_t number = rev_root->rev;
  number = number * 4
         + (inherit == svn_mergeinfo_nearest_ancestor ? 2 : 0)
         + (adjust_inherited_mergeinfo ? 1 : 0);

  return svn_fs_x__combine_number_and_string(number, path, result_pool);
}

/* Calculates the mergeinfo for PATH under REV_ROOT using inheritance
   type INHERIT.  Returns it in *MERGEINFO, or NULL if there is none.
   The result is allocated in RESULT_POOL; SCRATCH_POOL is
   used for temporary allocations.
 */
static svn_error_t *
get_mergeinfo_for_path_internal(svn_mergeinfo_t *mergeinfo,
                                svn_fs_root_t *rev_root,
                                const char *path,
                                svn_mergeinfo_inheritance_t inherit,
                                svn_boolean_t adjust_inherited_mergeinfo,
                                apr_pool_t *result_pool,
                                apr_pool_t *scratch_pool)
{
  parent_path_t *parent_path, *nearest_ancestor;
  apr_hash_t *proplist;
  svn_string_t *mergeinfo_string;

  path = svn_fs__canonicalize_abspath(path, scratch_pool);

  SVN_ERR(open_path(&parent_path, rev_root, path, 0, FALSE, scratch_pool));

  if (inherit == svn_mergeinfo_nearest_ancestor && ! parent_path->parent)
    return SVN_NO_ERROR;

  if (inherit == svn_mergeinfo_nearest_ancestor)
    nearest_ancestor = parent_path->parent;
  else
    nearest_ancestor = parent_path;

  while (TRUE)
    {
      svn_boolean_t has_mergeinfo;

      SVN_ERR(svn_fs_x__dag_has_mergeinfo(&has_mergeinfo,
                                          nearest_ancestor->node));
      if (has_mergeinfo)
        break;

      /* No need to loop if we're looking for explicit mergeinfo. */
      if (inherit == svn_mergeinfo_explicit)
        {
          return SVN_NO_ERROR;
        }

      nearest_ancestor = nearest_ancestor->parent;

      /* Run out?  There's no mergeinfo. */
      if (!nearest_ancestor)
        {
          return SVN_NO_ERROR;
        }
    }

  SVN_ERR(svn_fs_x__dag_get_proplist(&proplist, nearest_ancestor->node,
                                     scratch_pool, scratch_pool));
  mergeinfo_string = svn_hash_gets(proplist, SVN_PROP_MERGEINFO);
  if (!mergeinfo_string)
    return svn_error_createf
      (SVN_ERR_FS_CORRUPT, NULL,
       _("Node-revision '%s@%ld' claims to have mergeinfo but doesn't"),
       parent_path_path(nearest_ancestor, scratch_pool), rev_root->rev);

  /* Parse the mergeinfo; store the result in *MERGEINFO. */
  {
    /* Issue #3896: If a node has syntactically invalid mergeinfo, then
       treat it as if no mergeinfo is present rather than raising a parse
       error. */
    svn_error_t *err = svn_mergeinfo_parse(mergeinfo,
                                           mergeinfo_string->data,
                                           result_pool);
    if (err)
      {
        if (err->apr_err == SVN_ERR_MERGEINFO_PARSE_ERROR)
          {
            svn_error_clear(err);
            err = NULL;
            *mergeinfo = NULL;
          }
        return svn_error_trace(err);
      }
  }

  /* If our nearest ancestor is the very path we inquired about, we
     can return the mergeinfo results directly.  Otherwise, we're
     inheriting the mergeinfo, so we need to a) remove non-inheritable
     ranges and b) telescope the merged-from paths. */
  if (adjust_inherited_mergeinfo && (nearest_ancestor != parent_path))
    {
      svn_mergeinfo_t tmp_mergeinfo;

      SVN_ERR(svn_mergeinfo_inheritable2(&tmp_mergeinfo, *mergeinfo,
                                         NULL, SVN_INVALID_REVNUM,
                                         SVN_INVALID_REVNUM, TRUE,
                                         scratch_pool, scratch_pool));
      SVN_ERR(svn_fs__append_to_merged_froms(mergeinfo, tmp_mergeinfo,
                                             parent_path_relpath(
                                               parent_path, nearest_ancestor,
                                               scratch_pool),
                                             result_pool));
    }

  return SVN_NO_ERROR;
}

/* Caching wrapper around get_mergeinfo_for_path_internal().
 */
static svn_error_t *
get_mergeinfo_for_path(svn_mergeinfo_t *mergeinfo,
                       svn_fs_root_t *rev_root,
                       const char *path,
                       svn_mergeinfo_inheritance_t inherit,
                       svn_boolean_t adjust_inherited_mergeinfo,
                       apr_pool_t *result_pool,
                       apr_pool_t *scratch_pool)
{
  svn_fs_x__data_t *ffd = rev_root->fs->fsap_data;
  const char *cache_key;
  svn_boolean_t found = FALSE;
  svn_stringbuf_t *mergeinfo_exists;

  *mergeinfo = NULL;

  cache_key = mergeinfo_cache_key(path, rev_root, inherit,
                                  adjust_inherited_mergeinfo, scratch_pool);
  if (ffd->mergeinfo_existence_cache)
    {
      SVN_ERR(svn_cache__get((void **)&mergeinfo_exists, &found,
                             ffd->mergeinfo_existence_cache,
                             cache_key, result_pool));
      if (found && mergeinfo_exists->data[0] == '1')
        SVN_ERR(svn_cache__get((void **)mergeinfo, &found,
                              ffd->mergeinfo_cache,
                              cache_key, result_pool));
    }

  if (! found)
    {
      SVN_ERR(get_mergeinfo_for_path_internal(mergeinfo, rev_root, path,
                                              inherit,
                                              adjust_inherited_mergeinfo,
                                              result_pool, scratch_pool));
      if (ffd->mergeinfo_existence_cache)
        {
          mergeinfo_exists = svn_stringbuf_create(*mergeinfo ? "1" : "0",
                                                  scratch_pool);
          SVN_ERR(svn_cache__set(ffd->mergeinfo_existence_cache,
                                 cache_key, mergeinfo_exists, scratch_pool));
          if (*mergeinfo)
            SVN_ERR(svn_cache__set(ffd->mergeinfo_cache,
                                  cache_key, *mergeinfo, scratch_pool));
        }
    }

  return SVN_NO_ERROR;
}

/* Adds mergeinfo for each descendant of PATH (but not PATH itself)
   under ROOT to RESULT_CATALOG.  Returned values are allocated in
   RESULT_POOL; temporary values in POOL. */
static svn_error_t *
add_descendant_mergeinfo(svn_mergeinfo_catalog_t result_catalog,
                         svn_fs_root_t *root,
                         const char *path,
                         apr_pool_t *result_pool,
                         apr_pool_t *scratch_pool)
{
  dag_node_t *this_dag;
  svn_boolean_t go_down;

  SVN_ERR(get_dag(&this_dag, root, path, scratch_pool));
  SVN_ERR(svn_fs_x__dag_has_descendants_with_mergeinfo(&go_down,
                                                       this_dag));
  if (go_down)
    SVN_ERR(crawl_directory_dag_for_mergeinfo(root,
                                              path,
                                              this_dag,
                                              result_catalog,
                                              result_pool,
                                              scratch_pool));
  return SVN_NO_ERROR;
}


/* Get the mergeinfo for a set of paths, returned in
   *MERGEINFO_CATALOG.  Returned values are allocated in
   POOL, while temporary values are allocated in a sub-pool. */
static svn_error_t *
get_mergeinfos_for_paths(svn_fs_root_t *root,
                         svn_mergeinfo_catalog_t *mergeinfo_catalog,
                         const apr_array_header_t *paths,
                         svn_mergeinfo_inheritance_t inherit,
                         svn_boolean_t include_descendants,
                         svn_boolean_t adjust_inherited_mergeinfo,
                         apr_pool_t *result_pool,
                         apr_pool_t *scratch_pool)
{
  svn_mergeinfo_catalog_t result_catalog = svn_hash__make(result_pool);
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);
  int i;

  for (i = 0; i < paths->nelts; i++)
    {
      svn_error_t *err;
      svn_mergeinfo_t path_mergeinfo;
      const char *path = APR_ARRAY_IDX(paths, i, const char *);

      svn_pool_clear(iterpool);

      err = get_mergeinfo_for_path(&path_mergeinfo, root, path,
                                   inherit, adjust_inherited_mergeinfo,
                                   result_pool, iterpool);
      if (err)
        {
          if (err->apr_err == SVN_ERR_MERGEINFO_PARSE_ERROR)
            {
              svn_error_clear(err);
              err = NULL;
              path_mergeinfo = NULL;
            }
          else
            {
              return svn_error_trace(err);
            }
        }

      if (path_mergeinfo)
        svn_hash_sets(result_catalog, path, path_mergeinfo);
      if (include_descendants)
        SVN_ERR(add_descendant_mergeinfo(result_catalog, root, path,
                                         result_pool, scratch_pool));
    }
  svn_pool_destroy(iterpool);

  *mergeinfo_catalog = result_catalog;
  return SVN_NO_ERROR;
}


/* Implements svn_fs_get_mergeinfo. */
static svn_error_t *
x_get_mergeinfo(svn_mergeinfo_catalog_t *catalog,
                svn_fs_root_t *root,
                const apr_array_header_t *paths,
                svn_mergeinfo_inheritance_t inherit,
                svn_boolean_t include_descendants,
                svn_boolean_t adjust_inherited_mergeinfo,
                apr_pool_t *result_pool,
                apr_pool_t *scratch_pool)
{
  /* We require a revision root. */
  if (root->is_txn_root)
    return svn_error_create(SVN_ERR_FS_NOT_REVISION_ROOT, NULL, NULL);

  /* Retrieve a path -> mergeinfo hash mapping. */
  return get_mergeinfos_for_paths(root, catalog, paths,
                                  inherit,
                                  include_descendants,
                                  adjust_inherited_mergeinfo,
                                  result_pool, scratch_pool);
}


/* The vtable associated with root objects. */
static root_vtable_t root_vtable = {
  x_paths_changed,
  svn_fs_x__check_path,
  x_node_history,
  x_node_id,
  x_node_relation,
  svn_fs_x__node_created_rev,
  x_node_origin_rev,
  x_node_created_path,
  x_delete_node,
  x_copy,
  x_revision_link,
  x_copied_from,
  x_closest_copy,
  x_node_prop,
  x_node_proplist,
  x_node_has_props,
  x_change_node_prop,
  x_props_changed,
  x_dir_entries,
  x_dir_optimal_order,
  x_make_dir,
  x_file_length,
  x_file_checksum,
  x_file_contents,
  x_try_process_file_contents,
  x_make_file,
  x_apply_textdelta,
  x_apply_text,
  x_contents_changed,
  x_get_file_delta_stream,
  x_merge,
  x_get_mergeinfo,
};

/* Construct a new root object in FS, allocated from RESULT_POOL.  */
static svn_fs_root_t *
make_root(svn_fs_t *fs,
          apr_pool_t *result_pool)
{
  svn_fs_root_t *root = apr_pcalloc(result_pool, sizeof(*root));

  root->fs = fs;
  root->pool = result_pool;
  root->vtable = &root_vtable;

  return root;
}


/* Construct a root object referring to the root of revision REV in FS.
   Create the new root in RESULT_POOL.  */
static svn_fs_root_t *
make_revision_root(svn_fs_t *fs,
                   svn_revnum_t rev,
                   apr_pool_t *result_pool)
{
  svn_fs_root_t *root = make_root(fs, result_pool);

  root->is_txn_root = FALSE;
  root->rev = rev;

  return root;
}


/* Construct a root object referring to the root of the transaction
   named TXN and based on revision BASE_REV in FS, with FLAGS to
   describe transaction's behavior.  Create the new root in RESULT_POOL.  */
static svn_error_t *
make_txn_root(svn_fs_root_t **root_p,
              svn_fs_t *fs,
              svn_fs_x__txn_id_t txn_id,
              svn_revnum_t base_rev,
              apr_uint32_t flags,
              apr_pool_t *result_pool)
{
  svn_fs_root_t *root = make_root(fs, result_pool);
  fs_txn_root_data_t *frd = apr_pcalloc(root->pool, sizeof(*frd));
  frd->txn_id = txn_id;

  root->is_txn_root = TRUE;
  root->txn = svn_fs_x__txn_name(txn_id, root->pool);
  root->txn_flags = flags;
  root->rev = base_rev;

  /* Because this cache actually tries to invalidate elements, keep
     the number of elements per page down.

     Note that since dag_node_cache_invalidate uses svn_cache__iter,
     this *cannot* be a memcache-based cache.  */
  SVN_ERR(svn_cache__create_inprocess(&(frd->txn_node_cache),
                                      svn_fs_x__dag_serialize,
                                      svn_fs_x__dag_deserialize,
                                      APR_HASH_KEY_STRING,
                                      32, 20, FALSE,
                                      root->txn,
                                      root->pool));

  root->fsap_data = frd;

  *root_p = root;
  return SVN_NO_ERROR;
}



/* Verify. */
static const char *
stringify_node(dag_node_t *node,
               apr_pool_t *result_pool)
{
  /* ### TODO: print some PATH@REV to it, too. */
  return svn_fs_x__id_unparse(svn_fs_x__dag_get_id(node), result_pool)->data;
}

/* Check metadata sanity on NODE, and on its children.  Manually verify
   information for DAG nodes in revision REV, and trust the metadata
   accuracy for nodes belonging to older revisions.  To detect cycles,
   provide all parent dag_node_t * in PARENT_NODES. */
static svn_error_t *
verify_node(dag_node_t *node,
            svn_revnum_t rev,
            apr_array_header_t *parent_nodes,
            apr_pool_t *scratch_pool)
{
  svn_boolean_t has_mergeinfo;
  apr_int64_t mergeinfo_count;
  svn_fs_x__id_t pred_id;
  svn_fs_t *fs = svn_fs_x__dag_get_fs(node);
  int pred_count;
  svn_node_kind_t kind;
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);
  int i;

  /* Detect (non-)DAG cycles. */
  for (i = 0; i < parent_nodes->nelts; ++i)
    {
      dag_node_t *parent = APR_ARRAY_IDX(parent_nodes, i, dag_node_t *);
      if (svn_fs_x__id_eq(svn_fs_x__dag_get_id(parent),
                          svn_fs_x__dag_get_id(node)))
        return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                "Node is its own direct or indirect parent '%s'",
                                stringify_node(node, iterpool));
    }

  /* Fetch some data. */
  SVN_ERR(svn_fs_x__dag_has_mergeinfo(&has_mergeinfo, node));
  SVN_ERR(svn_fs_x__dag_get_mergeinfo_count(&mergeinfo_count, node));
  SVN_ERR(svn_fs_x__dag_get_predecessor_id(&pred_id, node));
  SVN_ERR(svn_fs_x__dag_get_predecessor_count(&pred_count, node));
  kind = svn_fs_x__dag_node_kind(node);

  /* Sanity check. */
  if (mergeinfo_count < 0)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             "Negative mergeinfo-count %" APR_INT64_T_FMT
                             " on node '%s'",
                             mergeinfo_count, stringify_node(node, iterpool));

  /* Issue #4129. (This check will explicitly catch non-root instances too.) */
  if (svn_fs_x__id_used(&pred_id))
    {
      dag_node_t *pred;
      int pred_pred_count;
      SVN_ERR(svn_fs_x__dag_get_node(&pred, fs, &pred_id, iterpool,
                                     iterpool));
      SVN_ERR(svn_fs_x__dag_get_predecessor_count(&pred_pred_count, pred));
      if (pred_pred_count+1 != pred_count)
        return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                 "Predecessor count mismatch: "
                                 "%s has %d, but %s has %d",
                                 stringify_node(node, iterpool), pred_count,
                                 stringify_node(pred, iterpool),
                                 pred_pred_count);
    }

  /* Kind-dependent verifications. */
  if (kind == svn_node_none)
    {
      return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                               "Node '%s' has kind 'none'",
                               stringify_node(node, iterpool));
    }
  if (kind == svn_node_file)
    {
      if (has_mergeinfo != mergeinfo_count) /* comparing int to bool */
        return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                 "File node '%s' has inconsistent mergeinfo: "
                                 "has_mergeinfo=%d, "
                                 "mergeinfo_count=%" APR_INT64_T_FMT,
                                 stringify_node(node, iterpool),
                                 has_mergeinfo, mergeinfo_count);
    }
  if (kind == svn_node_dir)
    {
      apr_array_header_t *entries;
      apr_int64_t children_mergeinfo = 0;
      APR_ARRAY_PUSH(parent_nodes, dag_node_t*) = node;

      SVN_ERR(svn_fs_x__dag_dir_entries(&entries, node, scratch_pool,
                                        iterpool));

      /* Compute CHILDREN_MERGEINFO. */
      for (i = 0; i < entries->nelts; ++i)
        {
          svn_fs_x__dirent_t *dirent
            = APR_ARRAY_IDX(entries, i, svn_fs_x__dirent_t *);
          dag_node_t *child;
          apr_int64_t child_mergeinfo;

          svn_pool_clear(iterpool);

          /* Compute CHILD_REV. */
          if (svn_fs_x__get_revnum(dirent->id.change_set) == rev)
            {
              SVN_ERR(svn_fs_x__dag_get_node(&child, fs, &dirent->id,
                                             iterpool, iterpool));
              SVN_ERR(verify_node(child, rev, parent_nodes, iterpool));
              SVN_ERR(svn_fs_x__dag_get_mergeinfo_count(&child_mergeinfo,
                                                        child));
            }
          else
            {
              SVN_ERR(svn_fs_x__get_mergeinfo_count(&child_mergeinfo, fs,
                                                    &dirent->id, iterpool));
            }

          children_mergeinfo += child_mergeinfo;
        }

      /* Side-effect of issue #4129. */
      if (children_mergeinfo+has_mergeinfo != mergeinfo_count)
        return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                 "Mergeinfo-count discrepancy on '%s': "
                                 "expected %" APR_INT64_T_FMT "+%d, "
                                 "counted %" APR_INT64_T_FMT,
                                 stringify_node(node, iterpool),
                                 mergeinfo_count, has_mergeinfo,
                                 children_mergeinfo);

      /* If we don't make it here, there was an error / corruption.
       * In that case, nobody will need PARENT_NODES anymore. */
      apr_array_pop(parent_nodes);
    }

  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_x__verify_root(svn_fs_root_t *root,
                      apr_pool_t *scratch_pool)
{
  dag_node_t *root_dir;
  apr_array_header_t *parent_nodes;

  /* Issue #4129: bogus pred-counts and minfo-cnt's on the root node-rev
     (and elsewhere).  This code makes more thorough checks than the
     commit-time checks in validate_root_noderev(). */

  /* Callers should disable caches by setting SVN_FS_CONFIG_FSX_CACHE_NS;
     see r1462436.

     When this code is called in the library, we want to ensure we
     use the on-disk data --- rather than some data that was read
     in the possibly-distance past and cached since. */
  SVN_ERR(root_node(&root_dir, root, scratch_pool, scratch_pool));

  /* Recursively verify ROOT_DIR. */
  parent_nodes = apr_array_make(scratch_pool, 16, sizeof(dag_node_t *));
  SVN_ERR(verify_node(root_dir, root->rev, parent_nodes, scratch_pool));

  /* Verify explicitly the predecessor of the root. */
  {
    svn_fs_x__id_t pred_id;
    svn_boolean_t has_predecessor;

    /* Only r0 should have no predecessor. */
    SVN_ERR(svn_fs_x__dag_get_predecessor_id(&pred_id, root_dir));
    has_predecessor = svn_fs_x__id_used(&pred_id);
    if (!root->is_txn_root && has_predecessor != !!root->rev)
      return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                               "r%ld's root node's predecessor is "
                               "unexpectedly '%s'",
                               root->rev,
                               (has_predecessor
                                 ? svn_fs_x__id_unparse(&pred_id,
                                                        scratch_pool)->data
                                 : "(null)"));
    if (root->is_txn_root && !has_predecessor)
      return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                               "Transaction '%s''s root node's predecessor is "
                               "unexpectedly NULL",
                               root->txn);

    /* Check the predecessor's revision. */
    if (has_predecessor)
      {
        svn_revnum_t pred_rev = svn_fs_x__get_revnum(pred_id.change_set);
        if (! root->is_txn_root && pred_rev+1 != root->rev)
          /* Issue #4129. */
          return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                   "r%ld's root node's predecessor is r%ld"
                                   " but should be r%ld",
                                   root->rev, pred_rev, root->rev - 1);
        if (root->is_txn_root && pred_rev != root->rev)
          return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                                   "Transaction '%s''s root node's predecessor"
                                   " is r%ld"
                                   " but should be r%ld",
                                   root->txn, pred_rev, root->rev);
      }
  }

  return SVN_NO_ERROR;
}
