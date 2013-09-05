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

#include "fs.h"
#include "key-gen.h"
#include "dag.h"
#include "lock.h"
#include "tree.h"
#include "fs_fs.h"
#include "id.h"
#include "temp_serializer.h"

#include "private/svn_mergeinfo_private.h"
#include "private/svn_subr_private.h"
#include "private/svn_fs_util.h"
#include "private/svn_fspath.h"
#include "../libsvn_fs/fs-loader.h"


/* ### I believe this constant will become internal to reps-strings.c.
   ### see the comment in window_consumer() for more information. */

/* ### the comment also seems to need tweaking: the log file stuff
   ### is no longer an issue... */
/* Data written to the filesystem through the svn_fs_apply_textdelta()
   interface is cached in memory until the end of the data stream, or
   until a size trigger is hit.  Define that trigger here (in bytes).
   Setting the value to 0 will result in no filesystem buffering at
   all.  The value only really matters when dealing with file contents
   bigger than the value itself.  Above that point, large values here
   allow the filesystem to buffer more data in memory before flushing
   to the database, which increases memory usage but greatly decreases
   the amount of disk access (and log-file generation) in database.
   Smaller values will limit your overall memory consumption, but can
   drastically hurt throughput by necessitating more write operations
   to the database (which also generates more log-files).  */
#define WRITE_BUFFER_SIZE          512000



/* The root structures.

   Why do they contain different data?  Well, transactions are mutable
   enough that it isn't safe to cache the DAG node for the root
   directory or the hash of copyfrom data: somebody else might modify
   them concurrently on disk!  (Why is the DAG node cache safer than
   the root DAG node?  When cloning transaction DAG nodes in and out
   of the cache, all of the possibly-mutable data from the
   node_revision_t inside the dag_node_t is dropped.)  Additionally,
   revisions are immutable enough that their DAG node cache can be
   kept in the FS object and shared among multiple revision root
   objects.
*/
typedef struct fs_rev_root_data_t
{
  /* A dag node for the revision's root directory. */
  dag_node_t *root_dir;

  /* Cache structure for mapping const char * PATH to const char
     *COPYFROM_STRING, so that paths_changed can remember all the
     copyfrom information in the changes file.
     COPYFROM_STRING has the format "REV PATH", or is the empty string if
     the path was added without history. */
  apr_hash_t *copyfrom_cache;

} fs_rev_root_data_t;

typedef struct fs_txn_root_data_t
{
  const char *txn_id;

  /* Cache of txn DAG nodes (without their nested noderevs, because
   * it's mutable). Same keys/values as ffd->rev_node_cache. */
  svn_cache__t *txn_node_cache;
} fs_txn_root_data_t;

/* Declared here to resolve the circular dependencies. */
static svn_error_t * get_dag(dag_node_t **dag_node_p,
                             svn_fs_root_t *root,
                             const char *path,
                             svn_boolean_t needs_lock_cache,
                             apr_pool_t *pool);

static svn_fs_root_t *make_revision_root(svn_fs_t *fs, svn_revnum_t rev,
                                         dag_node_t *root_dir,
                                         apr_pool_t *pool);

static svn_error_t *make_txn_root(svn_fs_root_t **root_p,
                                  svn_fs_t *fs, const char *txn,
                                  svn_revnum_t base_rev, apr_uint32_t flags,
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

/* Each pool that has received a DAG node, will hold at least on lock on
   our cache to ensure that the node remains valid despite being allocated
   in the cache's pool.  This is the structure to represent the lock.
 */
typedef struct cache_lock_t
{
  /* pool holding the lock */
  apr_pool_t *pool;

  /* cache being locked */
  fs_fs_dag_cache_t *cache;

  /* next lock. NULL at EOL */
  struct cache_lock_t *next;

  /* previous lock. NULL at list head. Only then this==cache->first_lock */
  struct cache_lock_t *prev;
} cache_lock_t;

/* The actual cache structure.  All nodes will be allocated in POOL.
   When the number of INSERTIONS (i.e. objects created form that pool)
   exceeds a certain threshold, the pool will be cleared and the cache
   with it.

   To ensure that nodes returned from this structure remain valid, the
   cache will get locked for the lifetime of the _receiving_ pools (i.e.
   those in which we would allocate the node if there was no cache.).
   The cache will only be cleared FIRST_LOCK is 0.
 */
struct fs_fs_dag_cache_t
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

  /* List of receiving pools that are still alive. */
  cache_lock_t *first_lock;
};

/* Cleanup function to be called when a receiving pool gets cleared.
   Unlocks the cache once.
 */
static apr_status_t
unlock_cache(void *baton_void)
{
  cache_lock_t *lock = baton_void;

  /* remove lock from chain. Update the head */
  if (lock->next)
    lock->next->prev = lock->prev;
  if (lock->prev)
    lock->prev->next = lock->next;
  else
    lock->cache->first_lock = lock->next;

  return APR_SUCCESS;
}

/* Cleanup function to be called when the cache itself gets destroyed.
   In that case, we must unregister all unlock requests.
 */
static apr_status_t
unregister_locks(void *baton_void)
{
  fs_fs_dag_cache_t *cache = baton_void;
  cache_lock_t *lock;

  for (lock = cache->first_lock; lock; lock = lock->next)
    apr_pool_cleanup_kill(lock->pool,
                          lock,
                          unlock_cache);

  return APR_SUCCESS;
}

fs_fs_dag_cache_t*
svn_fs_fs__create_dag_cache(apr_pool_t *pool)
{
  fs_fs_dag_cache_t *result = apr_pcalloc(pool, sizeof(*result));
  result->pool = svn_pool_create(pool);

  apr_pool_cleanup_register(pool,
                            result,
                            unregister_locks,
                            apr_pool_cleanup_null);

  return result;
}

/* Prevent the entries in CACHE from being destroyed, for as long as the
   POOL lives.
 */
static void
lock_cache(fs_fs_dag_cache_t* cache, apr_pool_t *pool)
{
  /* we only need to lock / unlock once per pool.  Since we will often ask
     for multiple nodes with the same pool, we can reduce the overhead.
     However, if e.g. pools are being used in an alternating pattern,
     we may lock the cache more than once for the same pool (and register
     just as many cleanup actions).
   */
  cache_lock_t *lock = cache->first_lock;

  /* try to find an existing lock for POOL.
     But limit the time spent on chasing pointers.  */
  int limiter = 8;
  while (lock && --limiter)
      if (lock->pool == pool)
        return;

  /* create a new lock and put it at the beginning of the lock chain */
  lock = apr_palloc(pool, sizeof(*lock));
  lock->cache = cache;
  lock->pool = pool;
  lock->next = cache->first_lock;
  lock->prev = NULL;

  if (cache->first_lock)
    cache->first_lock->prev = lock;
  cache->first_lock = lock;

  /* instruct POOL to remove the look upon cleanup */
  apr_pool_cleanup_register(pool,
                            lock,
                            unlock_cache,
                            apr_pool_cleanup_null);
}

/* Clears the CACHE at regular intervals (destroying all cached nodes)
 */
static void
auto_clear_dag_cache(fs_fs_dag_cache_t* cache)
{
  if (cache->first_lock == NULL && cache->insertions > BUCKET_COUNT)
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
cache_lookup( fs_fs_dag_cache_t *cache
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

  return result;
}

/* 2nd level cache */

/* Find and return the DAG node cache for ROOT and the key that
   should be used for PATH. */
static void
locate_cache(svn_cache__t **cache,
             const char **key,
             svn_fs_root_t *root,
             const char *path,
             apr_pool_t *pool)
{
  if (root->is_txn_root)
    {
      fs_txn_root_data_t *frd = root->fsap_data;
      if (cache) *cache = frd->txn_node_cache;
      if (key && path) *key = path;
    }
  else
    {
      fs_fs_data_t *ffd = root->fs->fsap_data;
      if (cache) *cache = ffd->rev_node_cache;
      if (key && path) *key
        = svn_fs_fs__combine_number_and_string(root->rev, path, pool);
    }
}

/* Return NODE for PATH from ROOT's node cache, or NULL if the node
   isn't cached; read it from the FS. *NODE remains valid until either
   POOL or the FS gets cleared or destroyed (whichever comes first).

   Since locking can be expensive and POOL may be long-living, for
   nodes that will not need to survive the next call to this function,
   set NEEDS_LOCK_CACHE to FALSE. */
static svn_error_t *
dag_node_cache_get(dag_node_t **node_p,
                   svn_fs_root_t *root,
                   const char *path,
                   svn_boolean_t needs_lock_cache,
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

      fs_fs_data_t *ffd = root->fs->fsap_data;
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
              svn_fs_fs__dag_set_fs(node, root->fs);
              bucket->node = node;
            }
        }
      else
        {
          node = bucket->node;
        }

      /* if we found a node, make sure it remains valid at least as long
         as it would when allocated in POOL. */
      if (node && needs_lock_cache)
        lock_cache(ffd->dag_node_cache, pool);
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
          svn_fs_fs__dag_set_fs(node, root->fs);
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
                   apr_pool_t *pool)
{
  svn_cache__t *cache;
  const char *key;

  SVN_ERR_ASSERT(*path == '/');

  /* Do *not* attempt to dup and put the node into L1.
   * dup() is twice as expensive as an L2 lookup (which will set also L1).
   */
  locate_cache(&cache, &key, root, path, pool);

  return svn_cache__set(cache, key, node, pool);
}


/* Baton for find_descendents_in_cache. */
struct fdic_baton {
  const char *path;
  apr_array_header_t *list;
  apr_pool_t *pool;
};

/* If the given item is a descendent of BATON->PATH, push
 * it onto BATON->LIST (copying into BATON->POOL).  Implements
 * the svn_iter_apr_hash_cb_t prototype. */
static svn_error_t *
find_descendents_in_cache(void *baton,
                          const void *key,
                          apr_ssize_t klen,
                          void *val,
                          apr_pool_t *pool)
{
  struct fdic_baton *b = baton;
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
                          apr_pool_t *pool)
{
  struct fdic_baton b;
  svn_cache__t *cache;
  apr_pool_t *iterpool;
  int i;

  b.path = path;
  b.pool = svn_pool_create(pool);
  b.list = apr_array_make(b.pool, 1, sizeof(const char *));

  SVN_ERR_ASSERT(root->is_txn_root);
  locate_cache(&cache, NULL, root, NULL, b.pool);


  SVN_ERR(svn_cache__iter(NULL, cache, find_descendents_in_cache,
                          &b, b.pool));

  iterpool = svn_pool_create(b.pool);

  for (i = 0; i < b.list->nelts; i++)
    {
      const char *descendent = APR_ARRAY_IDX(b.list, i, const char *);
      svn_pool_clear(iterpool);
      SVN_ERR(svn_cache__set(cache, descendent, NULL, iterpool));
    }

  svn_pool_destroy(iterpool);
  svn_pool_destroy(b.pool);
  return SVN_NO_ERROR;
}



/* Creating transaction and revision root nodes.  */

svn_error_t *
svn_fs_fs__txn_root(svn_fs_root_t **root_p,
                    svn_fs_txn_t *txn,
                    apr_pool_t *pool)
{
  apr_uint32_t flags = 0;
  apr_hash_t *txnprops;

  /* Look for the temporary txn props representing 'flags'. */
  SVN_ERR(svn_fs_fs__txn_proplist(&txnprops, txn, pool));
  if (txnprops)
    {
      if (svn_hash_gets(txnprops, SVN_FS__PROP_TXN_CHECK_OOD))
        flags |= SVN_FS_TXN_CHECK_OOD;

      if (svn_hash_gets(txnprops, SVN_FS__PROP_TXN_CHECK_LOCKS))
        flags |= SVN_FS_TXN_CHECK_LOCKS;
    }

  return make_txn_root(root_p, txn->fs, txn->id, txn->base_rev, flags, pool);
}


svn_error_t *
svn_fs_fs__revision_root(svn_fs_root_t **root_p,
                         svn_fs_t *fs,
                         svn_revnum_t rev,
                         apr_pool_t *pool)
{
  dag_node_t *root_dir;

  SVN_ERR(svn_fs__check_fs(fs, TRUE));

  SVN_ERR(svn_fs_fs__dag_revision_root(&root_dir, fs, rev, pool));

  *root_p = make_revision_root(fs, rev, root_dir, pool);

  return SVN_NO_ERROR;
}



/* Getting dag nodes for roots.  */


/* Set *NODE_P to a freshly opened dag node referring to the root
   directory of ROOT, allocating from POOL.  */
static svn_error_t *
root_node(dag_node_t **node_p,
          svn_fs_root_t *root,
          apr_pool_t *pool)
{
  if (root->is_txn_root)
    {
      /* It's a transaction root.  Open a fresh copy.  */
      return svn_fs_fs__dag_txn_root(node_p, root->fs, root->txn, pool);
    }
  else
    {
      /* It's a revision root, so we already have its root directory
         opened.  */
      fs_rev_root_data_t *frd = root->fsap_data;
      *node_p = svn_fs_fs__dag_dup(frd->root_dir, pool);
      return SVN_NO_ERROR;
    }
}


/* Set *NODE_P to a mutable root directory for ROOT, cloning if
   necessary, allocating in POOL.  ROOT must be a transaction root.
   Use ERROR_PATH in error messages.  */
static svn_error_t *
mutable_root_node(dag_node_t **node_p,
                  svn_fs_root_t *root,
                  const char *error_path,
                  apr_pool_t *pool)
{
  if (root->is_txn_root)
    return svn_fs_fs__dag_clone_root(node_p, root->fs, root->txn, pool);
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
   node).  TXN_ID is the transaction in which these items might be
   mutable.  Allocations are taken from POOL. */
static svn_error_t *
get_copy_inheritance(copy_id_inherit_t *inherit_p,
                     const char **copy_src_path,
                     svn_fs_t *fs,
                     parent_path_t *child,
                     const char *txn_id,
                     apr_pool_t *pool)
{
  const svn_fs_id_t *child_id, *parent_id, *copyroot_id;
  const char *child_copy_id, *parent_copy_id;
  const char *id_path = NULL;
  svn_fs_root_t *copyroot_root;
  dag_node_t *copyroot_node;
  svn_revnum_t copyroot_rev;
  const char *copyroot_path;

  SVN_ERR_ASSERT(child && child->parent && txn_id);

  /* Initialize some convenience variables. */
  child_id = svn_fs_fs__dag_get_id(child->node);
  parent_id = svn_fs_fs__dag_get_id(child->parent->node);
  child_copy_id = svn_fs_fs__id_copy_id(child_id);
  parent_copy_id = svn_fs_fs__id_copy_id(parent_id);

  /* If this child is already mutable, we have nothing to do. */
  if (svn_fs_fs__id_txn_id(child_id))
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
  if (strcmp(child_copy_id, "0") == 0)
    return SVN_NO_ERROR;

  /* Compare the copy IDs of the child and its parent.  If they are
     the same, then the child is already on the same branch as the
     parent, and should use the same mutability copy ID that the
     parent will use. */
  if (svn_fs_fs__key_compare(child_copy_id, parent_copy_id) == 0)
    return SVN_NO_ERROR;

  /* If the child is on the same branch that the parent is on, the
     child should just use the same copy ID that the parent would use.
     Else, the child needs to generate a new copy ID to use should it
     need to be made mutable.  We will claim that child is on the same
     branch as its parent if the child itself is not a branch point,
     or if it is a branch point that we are accessing via its original
     copy destination path. */
  SVN_ERR(svn_fs_fs__dag_get_copyroot(&copyroot_rev, &copyroot_path,
                                      child->node));
  SVN_ERR(svn_fs_fs__revision_root(&copyroot_root, fs, copyroot_rev, pool));
  SVN_ERR(get_dag(&copyroot_node, copyroot_root, copyroot_path, FALSE, pool));
  copyroot_id = svn_fs_fs__dag_get_id(copyroot_node);

  if (svn_fs_fs__id_compare(copyroot_id, child_id) == -1)
    return SVN_NO_ERROR;

  /* Determine if we are looking at the child via its original path or
     as a subtree item of a copied tree. */
  id_path = svn_fs_fs__dag_get_created_path(child->node);
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

/* Allocate a new parent_path_t node from POOL, referring to NODE,
   ENTRY, PARENT, and COPY_ID.  */
static parent_path_t *
make_parent_path(dag_node_t *node,
                 char *entry,
                 parent_path_t *parent,
                 apr_pool_t *pool)
{
  parent_path_t *parent_path = apr_pcalloc(pool, sizeof(*parent_path));
  parent_path->node = node;
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
  open_path_node_only = 4
} open_path_flags_t;


/* Open the node identified by PATH in ROOT, allocating in POOL.  Set
   *PARENT_PATH_P to a path from the node up to ROOT.  The resulting
   **PARENT_PATH_P value is guaranteed to contain at least one
   *element, for the root directory.  PATH must be in canonical form.

   If resulting *PARENT_PATH_P will eventually be made mutable and
   modified, or if copy ID inheritance information is otherwise
   needed, TXN_ID should be the ID of the mutability transaction.  If
   TXN_ID is NULL, no copy ID inheritance information will be
   calculated for the *PARENT_PATH_P chain.

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
          const char *txn_id,
          apr_pool_t *pool)
{
  svn_fs_t *fs = root->fs;
  dag_node_t *here = NULL; /* The directory we're currently looking at.  */
  parent_path_t *parent_path; /* The path from HERE up to the root. */
  const char *rest; /* The portion of PATH we haven't traversed yet.  */

  /* ensure a canonical path representation */
  const char *path_so_far = "/";
  apr_pool_t *iterpool = svn_pool_create(pool);

  /* callers often traverse the tree in some path-based order.  That means
     a sibling of PATH has been presently accessed.  Try to start the lookup
     directly at the parent node, if the caller did not requested the full
     parent chain. */
  const char *directory;
  assert(svn_fs__is_canonical_abspath(path));
  if (flags & open_path_node_only)
    {
      directory = svn_dirent_dirname(path, pool);
      if (directory[1] != 0) /* root nodes are covered anyway */
        SVN_ERR(dag_node_cache_get(&here, root, directory, TRUE, pool));
    }

  /* did the shortcut work? */
  if (here)
    {
      path_so_far = directory;
      rest = path + strlen(directory) + 1;
    }
  else
    {
      /* Make a parent_path item for the root node, using its own current
         copy id.  */
      SVN_ERR(root_node(&here, root, pool));
      rest = path + 1; /* skip the leading '/', it saves in iteration */
    }

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

      /* Parse out the next entry from the path.  */
      entry = svn_fs__next_entry_name(&next, rest, pool);

      /* Calculate the path traversed thus far. */
      path_so_far = svn_fspath__join(path_so_far, entry, pool);

      if (*entry == '\0')
        {
          /* Given the behavior of svn_fs__next_entry_name(), this
             happens when the path either starts or ends with a slash.
             In either case, we stay put: the current directory stays
             the same, and we add nothing to the parent path. */
          child = here;
        }
      else
        {
          copy_id_inherit_t inherit;
          const char *copy_path = NULL;
          svn_error_t *err = SVN_NO_ERROR;
          dag_node_t *cached_node = NULL;

          /* If we found a directory entry, follow it.  First, we
             check our node cache, and, failing that, we hit the DAG
             layer.  Don't bother to contact the cache for the last
             element if we already know the lookup to fail for the
             complete path. */
          if (next || !(flags & open_path_uncached))
            SVN_ERR(dag_node_cache_get(&cached_node, root, path_so_far,
                                       TRUE, pool));
          if (cached_node)
            child = cached_node;
          else
            err = svn_fs_fs__dag_open(&child, here, entry, pool, iterpool);

          /* "file not found" requires special handling.  */
          if (err && err->apr_err == SVN_ERR_FS_NOT_FOUND)
            {
              /* If this was the last path component, and the caller
                 said it was optional, then don't return an error;
                 just put a NULL node pointer in the path.  */

              svn_error_clear(err);

              if ((flags & open_path_last_optional)
                  && (! next || *next == '\0'))
                {
                  parent_path = make_parent_path(NULL, entry, parent_path,
                                                 pool);
                  break;
                }
              else
                {
                  /* Build a better error message than svn_fs_fs__dag_open
                     can provide, giving the root and full path name.  */
                  return SVN_FS__NOT_FOUND(root, path);
                }
            }

          /* Other errors we return normally.  */
          SVN_ERR(err);

          if (flags & open_path_node_only)
            {
              /* Shortcut: the caller only wan'ts the final DAG node. */
              parent_path->node = child;
            }
          else
            {
              /* Now, make a parent_path item for CHILD. */
              parent_path = make_parent_path(child, entry, parent_path, pool);
              if (txn_id)
                {
                  SVN_ERR(get_copy_inheritance(&inherit, &copy_path, fs,
                                               parent_path, txn_id, iterpool));
                  parent_path->copy_inherit = inherit;
                  parent_path->copy_src_path = apr_pstrdup(pool, copy_path);
                }
            }

          /* Cache the node we found (if it wasn't already cached). */
          if (! cached_node)
            SVN_ERR(dag_node_cache_set(root, path_so_far, child, iterpool));
        }

      /* Are we finished traversing the path?  */
      if (! next)
        break;

      /* The path isn't finished yet; we'd better be in a directory.  */
      if (svn_fs_fs__dag_node_kind(child) != svn_node_dir)
        SVN_ERR_W(SVN_FS__ERR_NOT_DIRECTORY(fs, path_so_far),
                  apr_psprintf(iterpool, _("Failure opening '%s'"), path));

      rest = next;
      here = child;
    }

  svn_pool_destroy(iterpool);
  *parent_path_p = parent_path;
  return SVN_NO_ERROR;
}


/* Make the node referred to by PARENT_PATH mutable, if it isn't
   already, allocating from POOL.  ROOT must be the root from which
   PARENT_PATH descends.  Clone any parent directories as needed.
   Adjust the dag nodes in PARENT_PATH to refer to the clones.  Use
   ERROR_PATH in error messages.  */
static svn_error_t *
make_path_mutable(svn_fs_root_t *root,
                  parent_path_t *parent_path,
                  const char *error_path,
                  apr_pool_t *pool)
{
  dag_node_t *clone;
  const char *txn_id = root->txn;

  /* Is the node mutable already?  */
  if (svn_fs_fs__dag_check_mutable(parent_path->node))
    return SVN_NO_ERROR;

  /* Are we trying to clone the root, or somebody's child node?  */
  if (parent_path->parent)
    {
      const svn_fs_id_t *parent_id, *child_id, *copyroot_id;
      const char *copy_id = NULL;
      copy_id_inherit_t inherit = parent_path->copy_inherit;
      const char *clone_path, *copyroot_path;
      svn_revnum_t copyroot_rev;
      svn_boolean_t is_parent_copyroot = FALSE;
      svn_fs_root_t *copyroot_root;
      dag_node_t *copyroot_node;

      /* We're trying to clone somebody's child.  Make sure our parent
         is mutable.  */
      SVN_ERR(make_path_mutable(root, parent_path->parent,
                                error_path, pool));

      switch (inherit)
        {
        case copy_id_inherit_parent:
          parent_id = svn_fs_fs__dag_get_id(parent_path->parent->node);
          copy_id = svn_fs_fs__id_copy_id(parent_id);
          break;

        case copy_id_inherit_new:
          SVN_ERR(svn_fs_fs__reserve_copy_id(&copy_id, root->fs, txn_id,
                                             pool));
          break;

        case copy_id_inherit_self:
          copy_id = NULL;
          break;

        case copy_id_inherit_unknown:
        default:
          SVN_ERR_MALFUNCTION(); /* uh-oh -- somebody didn't calculate copy-ID
                      inheritance data. */
        }

      /* Determine what copyroot our new child node should use. */
      SVN_ERR(svn_fs_fs__dag_get_copyroot(&copyroot_rev, &copyroot_path,
                                          parent_path->node));
      SVN_ERR(svn_fs_fs__revision_root(&copyroot_root, root->fs,
                                       copyroot_rev, pool));
      SVN_ERR(get_dag(&copyroot_node, copyroot_root, copyroot_path,
                      FALSE, pool));

      child_id = svn_fs_fs__dag_get_id(parent_path->node);
      copyroot_id = svn_fs_fs__dag_get_id(copyroot_node);
      if (strcmp(svn_fs_fs__id_node_id(child_id),
                 svn_fs_fs__id_node_id(copyroot_id)) != 0)
        is_parent_copyroot = TRUE;

      /* Now make this node mutable.  */
      clone_path = parent_path_path(parent_path->parent, pool);
      SVN_ERR(svn_fs_fs__dag_clone_child(&clone,
                                         parent_path->parent->node,
                                         clone_path,
                                         parent_path->entry,
                                         copy_id, txn_id,
                                         is_parent_copyroot,
                                         pool));

      /* Update the path cache. */
      SVN_ERR(dag_node_cache_set(root, parent_path_path(parent_path, pool),
                                 clone, pool));
    }
  else
    {
      /* We're trying to clone the root directory.  */
      SVN_ERR(mutable_root_node(&clone, root, error_path, pool));
    }

  /* Update the PARENT_PATH link to refer to the clone.  */
  parent_path->node = clone;

  return SVN_NO_ERROR;
}


/* Open the node identified by PATH in ROOT.  Set DAG_NODE_P to the
   node we find, allocated in POOL.  Return the error
   SVN_ERR_FS_NOT_FOUND if this node doesn't exist.

   Since locking can be expensive and POOL may be long-living, for
   nodes that will not need to survive the next call to this function,
   set NEEDS_LOCK_CACHE to FALSE. */
static svn_error_t *
get_dag(dag_node_t **dag_node_p,
        svn_fs_root_t *root,
        const char *path,
        svn_boolean_t needs_lock_cache,
        apr_pool_t *pool)
{
  parent_path_t *parent_path;
  dag_node_t *node = NULL;

  /* First we look for the DAG in our cache
     (if the path may be canonical). */
  if (*path == '/')
    SVN_ERR(dag_node_cache_get(&node, root, path, needs_lock_cache, pool));

  if (! node)
    {
      /* Canonicalize the input PATH. */
      if (! svn_fs__is_canonical_abspath(path))
        {
          path = svn_fs__canonicalize_abspath(path, pool);

          /* Try again with the corrected path. */
          SVN_ERR(dag_node_cache_get(&node, root, path, needs_lock_cache,
                                     pool));
        }

      if (! node)
        {
          /* Call open_path with no flags, as we want this to return an
           * error if the node for which we are searching doesn't exist. */
          SVN_ERR(open_path(&parent_path, root, path,
                            open_path_uncached | open_path_node_only,
                            NULL, pool));
          node = parent_path->node;

          /* No need to cache our find -- open_path() will do that for us. */
        }
    }

  *dag_node_p = node;
  return SVN_NO_ERROR;
}



/* Populating the `changes' table. */

/* Add a change to the changes table in FS, keyed on transaction id
   TXN_ID, and indicated that a change of kind CHANGE_KIND occurred on
   PATH (whose node revision id is--or was, in the case of a
   deletion--NODEREV_ID), and optionally that TEXT_MODs or PROP_MODs
   occurred.  If the change resulted from a copy, COPYFROM_REV and
   COPYFROM_PATH specify under which revision and path the node was
   copied from.  If this was not part of a copy, COPYFROM_REV should
   be SVN_INVALID_REVNUM.  Do all this as part of POOL.  */
static svn_error_t *
add_change(svn_fs_t *fs,
           const char *txn_id,
           const char *path,
           const svn_fs_id_t *noderev_id,
           svn_fs_path_change_kind_t change_kind,
           svn_boolean_t text_mod,
           svn_boolean_t prop_mod,
           svn_node_kind_t node_kind,
           svn_revnum_t copyfrom_rev,
           const char *copyfrom_path,
           apr_pool_t *pool)
{
  return svn_fs_fs__add_change(fs, txn_id,
                               svn_fs__canonicalize_abspath(path, pool),
                               noderev_id, change_kind, text_mod, prop_mod,
                               node_kind, copyfrom_rev, copyfrom_path,
                               pool);
}



/* Generic node operations.  */

/* Get the id of a node referenced by path PATH in ROOT.  Return the
   id in *ID_P allocated in POOL. */
svn_error_t *
svn_fs_fs__node_id(const svn_fs_id_t **id_p,
                   svn_fs_root_t *root,
                   const char *path,
                   apr_pool_t *pool)
{
  if ((! root->is_txn_root)
      && (path[0] == '\0' || ((path[0] == '/') && (path[1] == '\0'))))
    {
      /* Optimize the case where we don't need any db access at all.
         The root directory ("" or "/") node is stored in the
         svn_fs_root_t object, and never changes when it's a revision
         root, so we can just reach in and grab it directly. */
      fs_rev_root_data_t *frd = root->fsap_data;
      *id_p = svn_fs_fs__id_copy(svn_fs_fs__dag_get_id(frd->root_dir), pool);
    }
  else
    {
      dag_node_t *node;

      SVN_ERR(get_dag(&node, root, path, FALSE, pool));
      *id_p = svn_fs_fs__id_copy(svn_fs_fs__dag_get_id(node), pool);
    }
  return SVN_NO_ERROR;
}


svn_error_t *
svn_fs_fs__node_created_rev(svn_revnum_t *revision,
                            svn_fs_root_t *root,
                            const char *path,
                            apr_pool_t *pool)
{
  dag_node_t *node;

  SVN_ERR(get_dag(&node, root, path, FALSE, pool));
  return svn_fs_fs__dag_get_revision(revision, node, pool);
}


/* Set *CREATED_PATH to the path at which PATH under ROOT was created.
   Return a string allocated in POOL. */
static svn_error_t *
fs_node_created_path(const char **created_path,
                     svn_fs_root_t *root,
                     const char *path,
                     apr_pool_t *pool)
{
  dag_node_t *node;

  SVN_ERR(get_dag(&node, root, path, TRUE, pool));
  *created_path = svn_fs_fs__dag_get_created_path(node);

  return SVN_NO_ERROR;
}


/* Set *KIND_P to the type of node located at PATH under ROOT.
   Perform temporary allocations in POOL. */
static svn_error_t *
node_kind(svn_node_kind_t *kind_p,
          svn_fs_root_t *root,
          const char *path,
          apr_pool_t *pool)
{
  const svn_fs_id_t *node_id;
  dag_node_t *node;

  /* Get the node id. */
  SVN_ERR(svn_fs_fs__node_id(&node_id, root, path, pool));

  /* Use the node id to get the real kind. */
  SVN_ERR(svn_fs_fs__dag_get_node(&node, root->fs, node_id, pool));
  *kind_p = svn_fs_fs__dag_node_kind(node);

  return SVN_NO_ERROR;
}


/* Set *KIND_P to the type of node present at PATH under ROOT.  If
   PATH does not exist under ROOT, set *KIND_P to svn_node_none.  Use
   POOL for temporary allocation. */
svn_error_t *
svn_fs_fs__check_path(svn_node_kind_t *kind_p,
                      svn_fs_root_t *root,
                      const char *path,
                      apr_pool_t *pool)
{
  svn_error_t *err = node_kind(kind_p, root, path, pool);
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
fs_node_prop(svn_string_t **value_p,
             svn_fs_root_t *root,
             const char *path,
             const char *propname,
             apr_pool_t *pool)
{
  dag_node_t *node;
  apr_hash_t *proplist;

  SVN_ERR(get_dag(&node, root, path, FALSE, pool));
  SVN_ERR(svn_fs_fs__dag_get_proplist(&proplist, node, pool));
  *value_p = NULL;
  if (proplist)
    *value_p = svn_hash_gets(proplist, propname);

  return SVN_NO_ERROR;
}


/* Set *TABLE_P to the entire property list of PATH under ROOT, as an
   APR hash table allocated in POOL.  The resulting property table
   maps property names to pointers to svn_string_t objects containing
   the property value. */
static svn_error_t *
fs_node_proplist(apr_hash_t **table_p,
                 svn_fs_root_t *root,
                 const char *path,
                 apr_pool_t *pool)
{
  apr_hash_t *table;
  dag_node_t *node;

  SVN_ERR(get_dag(&node, root, path, FALSE, pool));
  SVN_ERR(svn_fs_fs__dag_get_proplist(&table, node, pool));
  *table_p = table ? table : apr_hash_make(pool);

  return SVN_NO_ERROR;
}


static svn_error_t *
increment_mergeinfo_up_tree(parent_path_t *pp,
                            apr_int64_t increment,
                            apr_pool_t *pool)
{
  for (; pp; pp = pp->parent)
    SVN_ERR(svn_fs_fs__dag_increment_mergeinfo_count(pp->node,
                                                     increment,
                                                     pool));

  return SVN_NO_ERROR;
}

/* Change, add, or delete a node's property value.  The affected node
   is PATH under ROOT, the property value to modify is NAME, and VALUE
   points to either a string value to set the new contents to, or NULL
   if the property should be deleted.  Perform temporary allocations
   in POOL. */
static svn_error_t *
fs_change_node_prop(svn_fs_root_t *root,
                    const char *path,
                    const char *name,
                    const svn_string_t *value,
                    apr_pool_t *pool)
{
  parent_path_t *parent_path;
  apr_hash_t *proplist;
  const char *txn_id;

  if (! root->is_txn_root)
    return SVN_FS__NOT_TXN(root);
  txn_id = root->txn;

  path = svn_fs__canonicalize_abspath(path, pool);
  SVN_ERR(open_path(&parent_path, root, path, 0, txn_id, pool));

  /* Check (non-recursively) to see if path is locked; if so, check
     that we can use it. */
  if (root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
    SVN_ERR(svn_fs_fs__allow_locked_operation(path, root->fs, FALSE, FALSE,
                                              pool));

  SVN_ERR(make_path_mutable(root, parent_path, path, pool));
  SVN_ERR(svn_fs_fs__dag_get_proplist(&proplist, parent_path->node, pool));

  /* If there's no proplist, but we're just deleting a property, exit now. */
  if ((! proplist) && (! value))
    return SVN_NO_ERROR;

  /* Now, if there's no proplist, we know we need to make one. */
  if (! proplist)
    proplist = apr_hash_make(pool);

  if (svn_fs_fs__fs_supports_mergeinfo(root->fs)
      && strcmp (name, SVN_PROP_MERGEINFO) == 0)
    {
      apr_int64_t increment = 0;
      svn_boolean_t had_mergeinfo;
      SVN_ERR(svn_fs_fs__dag_has_mergeinfo(&had_mergeinfo, parent_path->node));

      if (value && !had_mergeinfo)
        increment = 1;
      else if (!value && had_mergeinfo)
        increment = -1;

      if (increment != 0)
        {
          SVN_ERR(increment_mergeinfo_up_tree(parent_path, increment, pool));
          SVN_ERR(svn_fs_fs__dag_set_has_mergeinfo(parent_path->node,
                                                   (value != NULL), pool));
        }
    }

  /* Set the property. */
  svn_hash_sets(proplist, name, value);

  /* Overwrite the node's proplist. */
  SVN_ERR(svn_fs_fs__dag_set_proplist(parent_path->node, proplist,
                                      pool));

  /* Make a record of this modification in the changes table. */
  return add_change(root->fs, txn_id, path,
                    svn_fs_fs__dag_get_id(parent_path->node),
                    svn_fs_path_change_modify, FALSE, TRUE,
                    svn_fs_fs__dag_node_kind(parent_path->node),
                    SVN_INVALID_REVNUM, NULL, pool);
}


/* Determine if the properties of two path/root combinations are
   different.  Set *CHANGED_P to TRUE if the properties at PATH1 under
   ROOT1 differ from those at PATH2 under ROOT2, or FALSE otherwise.
   Both roots must be in the same filesystem. */
static svn_error_t *
fs_props_changed(svn_boolean_t *changed_p,
                 svn_fs_root_t *root1,
                 const char *path1,
                 svn_fs_root_t *root2,
                 const char *path2,
                 apr_pool_t *pool)
{
  dag_node_t *node1, *node2;

  /* Check that roots are in the same fs. */
  if (root1->fs != root2->fs)
    return svn_error_create
      (SVN_ERR_FS_GENERAL, NULL,
       _("Cannot compare property value between two different filesystems"));

  SVN_ERR(get_dag(&node1, root1, path1, TRUE, pool));
  SVN_ERR(get_dag(&node2, root2, path2, TRUE, pool));
  return svn_fs_fs__dag_things_different(changed_p, NULL,
                                         node1, node2);
}



/* Merges and commits. */

/* Set *NODE to the root node of ROOT.  */
static svn_error_t *
get_root(dag_node_t **node, svn_fs_root_t *root, apr_pool_t *pool)
{
  return get_dag(node, root, "/", TRUE, pool);
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
      const char *txn_id,
      apr_int64_t *mergeinfo_increment_out,
      apr_pool_t *pool)
{
  const svn_fs_id_t *source_id, *target_id, *ancestor_id;
  apr_hash_t *s_entries, *t_entries, *a_entries;
  apr_hash_index_t *hi;
  svn_fs_t *fs;
  apr_pool_t *iterpool;
  apr_int64_t mergeinfo_increment = 0;
  svn_boolean_t fs_supports_mergeinfo;

  /* Make sure everyone comes from the same filesystem. */
  fs = svn_fs_fs__dag_get_fs(ancestor);
  if ((fs != svn_fs_fs__dag_get_fs(source))
      || (fs != svn_fs_fs__dag_get_fs(target)))
    {
      return svn_error_create
        (SVN_ERR_FS_CORRUPT, NULL,
         _("Bad merge; ancestor, source, and target not all in same fs"));
    }

  /* We have the same fs, now check it. */
  SVN_ERR(svn_fs__check_fs(fs, TRUE));

  source_id   = svn_fs_fs__dag_get_id(source);
  target_id   = svn_fs_fs__dag_get_id(target);
  ancestor_id = svn_fs_fs__dag_get_id(ancestor);

  /* It's improper to call this function with ancestor == target. */
  if (svn_fs_fs__id_eq(ancestor_id, target_id))
    {
      svn_string_t *id_str = svn_fs_fs__id_unparse(target_id, pool);
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
  if (svn_fs_fs__id_eq(ancestor_id, source_id)
      || (svn_fs_fs__id_eq(source_id, target_id)))
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

  if ((svn_fs_fs__dag_node_kind(source) != svn_node_dir)
      || (svn_fs_fs__dag_node_kind(target) != svn_node_dir)
      || (svn_fs_fs__dag_node_kind(ancestor) != svn_node_dir))
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
    node_revision_t *tgt_nr, *anc_nr, *src_nr;

    /* Get node revisions for our id's. */
    SVN_ERR(svn_fs_fs__get_node_revision(&tgt_nr, fs, target_id, pool));
    SVN_ERR(svn_fs_fs__get_node_revision(&anc_nr, fs, ancestor_id, pool));
    SVN_ERR(svn_fs_fs__get_node_revision(&src_nr, fs, source_id, pool));

    /* Now compare the prop-keys of the skels.  Note that just because
       the keys are different -doesn't- mean the proplists have
       different contents.  But merge() isn't concerned with contents;
       it doesn't do a brute-force comparison on textual contents, so
       it won't do that here either.  Checking to see if the propkey
       atoms are `equal' is enough. */
    if (! svn_fs_fs__noderev_same_rep_key(tgt_nr->prop_rep, anc_nr->prop_rep))
      return conflict_err(conflict_p, target_path);
    if (! svn_fs_fs__noderev_same_rep_key(src_nr->prop_rep, anc_nr->prop_rep))
      return conflict_err(conflict_p, target_path);
  }

  /* ### todo: it would be more efficient to simply check for a NULL
     entries hash where necessary below than to allocate an empty hash
     here, but another day, another day... */
  SVN_ERR(svn_fs_fs__dag_dir_entries(&s_entries, source, pool));
  SVN_ERR(svn_fs_fs__dag_dir_entries(&t_entries, target, pool));
  SVN_ERR(svn_fs_fs__dag_dir_entries(&a_entries, ancestor, pool));

  fs_supports_mergeinfo = svn_fs_fs__fs_supports_mergeinfo(fs);

  /* for each entry E in a_entries... */
  iterpool = svn_pool_create(pool);
  for (hi = apr_hash_first(pool, a_entries);
       hi;
       hi = apr_hash_next(hi))
    {
      svn_fs_dirent_t *s_entry, *t_entry, *a_entry;
      const char *name;
      apr_ssize_t klen;

      svn_pool_clear(iterpool);

      name = svn__apr_hash_index_key(hi);
      klen = svn__apr_hash_index_klen(hi);
      a_entry = svn__apr_hash_index_val(hi);

      s_entry = apr_hash_get(s_entries, name, klen);
      t_entry = apr_hash_get(t_entries, name, klen);

      /* No changes were made to this entry while the transaction was
         in progress, so do nothing to the target. */
      if (s_entry && svn_fs_fs__id_eq(a_entry->id, s_entry->id))
        goto end;

      /* A change was made to this entry while the transaction was in
         process, but the transaction did not touch this entry. */
      else if (t_entry && svn_fs_fs__id_eq(a_entry->id, t_entry->id))
        {
          dag_node_t *t_ent_node;
          SVN_ERR(svn_fs_fs__dag_get_node(&t_ent_node, fs,
                                          t_entry->id, iterpool));
          if (fs_supports_mergeinfo)
            {
              apr_int64_t mergeinfo_start;
              SVN_ERR(svn_fs_fs__dag_get_mergeinfo_count(&mergeinfo_start,
                                                         t_ent_node));
              mergeinfo_increment -= mergeinfo_start;
            }

          if (s_entry)
            {
              dag_node_t *s_ent_node;
              SVN_ERR(svn_fs_fs__dag_get_node(&s_ent_node, fs,
                                              s_entry->id, iterpool));

              if (fs_supports_mergeinfo)
                {
                  apr_int64_t mergeinfo_end;
                  SVN_ERR(svn_fs_fs__dag_get_mergeinfo_count(&mergeinfo_end,
                                                             s_ent_node));
                  mergeinfo_increment += mergeinfo_end;
                }

              SVN_ERR(svn_fs_fs__dag_set_entry(target, name,
                                               s_entry->id,
                                               s_entry->kind,
                                               txn_id,
                                               iterpool));
            }
          else
            {
              SVN_ERR(svn_fs_fs__dag_delete(target, name, txn_id, iterpool));
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

          /* If either SOURCE-ENTRY or TARGET-ENTRY is not a direct
             modification of ANCESTOR-ENTRY, declare a conflict. */
          if (strcmp(svn_fs_fs__id_node_id(s_entry->id),
                     svn_fs_fs__id_node_id(a_entry->id)) != 0
              || strcmp(svn_fs_fs__id_copy_id(s_entry->id),
                        svn_fs_fs__id_copy_id(a_entry->id)) != 0
              || strcmp(svn_fs_fs__id_node_id(t_entry->id),
                        svn_fs_fs__id_node_id(a_entry->id)) != 0
              || strcmp(svn_fs_fs__id_copy_id(t_entry->id),
                        svn_fs_fs__id_copy_id(a_entry->id)) != 0)
            return conflict_err(conflict_p,
                                svn_fspath__join(target_path,
                                                 a_entry->name,
                                                 iterpool));

          /* Direct modifications were made to the directory
             ANCESTOR-ENTRY in both SOURCE and TARGET.  Recursively
             merge these modifications. */
          SVN_ERR(svn_fs_fs__dag_get_node(&s_ent_node, fs,
                                          s_entry->id, iterpool));
          SVN_ERR(svn_fs_fs__dag_get_node(&t_ent_node, fs,
                                          t_entry->id, iterpool));
          SVN_ERR(svn_fs_fs__dag_get_node(&a_ent_node, fs,
                                          a_entry->id, iterpool));
          new_tpath = svn_fspath__join(target_path, t_entry->name, iterpool);
          SVN_ERR(merge(conflict_p, new_tpath,
                        t_ent_node, s_ent_node, a_ent_node,
                        txn_id,
                        &sub_mergeinfo_increment,
                        iterpool));
          if (fs_supports_mergeinfo)
            mergeinfo_increment += sub_mergeinfo_increment;
        }

      /* We've taken care of any possible implications E could have.
         Remove it from source_entries, so it's easy later to loop
         over all the source entries that didn't exist in
         ancestor_entries. */
    end:
      apr_hash_set(s_entries, name, klen, NULL);
    }

  /* For each entry E in source but not in ancestor */
  for (hi = apr_hash_first(pool, s_entries);
       hi;
       hi = apr_hash_next(hi))
    {
      svn_fs_dirent_t *s_entry, *t_entry;
      const char *name = svn__apr_hash_index_key(hi);
      apr_ssize_t klen = svn__apr_hash_index_klen(hi);
      dag_node_t *s_ent_node;

      svn_pool_clear(iterpool);

      s_entry = svn__apr_hash_index_val(hi);
      t_entry = apr_hash_get(t_entries, name, klen);

      /* If NAME exists in TARGET, declare a conflict. */
      if (t_entry)
        return conflict_err(conflict_p,
                            svn_fspath__join(target_path,
                                             t_entry->name,
                                             iterpool));

      SVN_ERR(svn_fs_fs__dag_get_node(&s_ent_node, fs,
                                      s_entry->id, iterpool));
      if (fs_supports_mergeinfo)
        {
          apr_int64_t mergeinfo_s;
          SVN_ERR(svn_fs_fs__dag_get_mergeinfo_count(&mergeinfo_s,
                                                     s_ent_node));
          mergeinfo_increment += mergeinfo_s;
        }

      SVN_ERR(svn_fs_fs__dag_set_entry
              (target, s_entry->name, s_entry->id, s_entry->kind,
               txn_id, iterpool));
    }
  svn_pool_destroy(iterpool);

  SVN_ERR(svn_fs_fs__dag_update_ancestry(target, source, pool));

  if (fs_supports_mergeinfo)
    SVN_ERR(svn_fs_fs__dag_increment_mergeinfo_count(target,
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
              apr_pool_t *pool)
{
  dag_node_t *txn_root_node;
  svn_fs_t *fs = txn->fs;
  const char *txn_id = txn->id;

  SVN_ERR(svn_fs_fs__dag_txn_root(&txn_root_node, fs, txn_id, pool));

  if (ancestor_node == NULL)
    {
      SVN_ERR(svn_fs_fs__dag_txn_base_root(&ancestor_node, fs,
                                           txn_id, pool));
    }

  if (svn_fs_fs__id_eq(svn_fs_fs__dag_get_id(ancestor_node),
                       svn_fs_fs__dag_get_id(txn_root_node)))
    {
      /* If no changes have been made in TXN since its current base,
         then it can't conflict with any changes since that base.
         The caller isn't supposed to call us in that case. */
      SVN_ERR_MALFUNCTION();
    }
  else
    SVN_ERR(merge(conflict, "/", txn_root_node,
                  source_node, ancestor_node, txn_id, NULL, pool));

  return SVN_NO_ERROR;
}


svn_error_t *
svn_fs_fs__commit_txn(const char **conflict_p,
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

      SVN_ERR(svn_fs_fs__youngest_rev(&youngish_rev, fs, iterpool));
      SVN_ERR(svn_fs_fs__revision_root(&youngish_root, fs, youngish_rev,
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
      err = svn_fs_fs__commit(new_rev, fs, txn, iterpool);
      if (err && (err->apr_err == SVN_ERR_FS_TXN_OUT_OF_DATE))
        {
          /* Did someone else finish committing a new revision while we
             were in mid-merge or mid-commit?  If so, we'll need to
             loop again to merge the new changes in, then try to
             commit again.  Or if that's not what happened, then just
             return the error. */
          svn_revnum_t youngest_rev;
          SVN_ERR(svn_fs_fs__youngest_rev(&youngest_rev, fs, iterpool));
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

  svn_fs_fs__reset_txn_caches(fs);

  svn_pool_destroy(iterpool);
  return svn_error_trace(err);
}


/* Merge changes between two nodes into a third node.  Given nodes
   SOURCE_PATH under SOURCE_ROOT, TARGET_PATH under TARGET_ROOT and
   ANCESTOR_PATH under ANCESTOR_ROOT, modify target to contain all the
   changes between the ancestor and source.  If there are conflicts,
   return SVN_ERR_FS_CONFLICT and set *CONFLICT_P to a textual
   description of the offending changes.  Perform any temporary
   allocations in POOL. */
static svn_error_t *
fs_merge(const char **conflict_p,
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
  SVN_ERR(svn_fs_fs__open_txn(&txn, ancestor_root->fs, target_root->txn,
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
svn_fs_fs__deltify(svn_fs_t *fs,
                   svn_revnum_t revision,
                   apr_pool_t *pool)
{
  /* Deltify is a no-op for fs_fs. */

  return SVN_NO_ERROR;
}



/* Directories.  */

/* Set *TABLE_P to a newly allocated APR hash table containing the
   entries of the directory at PATH in ROOT.  The keys of the table
   are entry names, as byte strings, excluding the final null
   character; the table's values are pointers to svn_fs_dirent_t
   structures.  Allocate the table and its contents in POOL. */
static svn_error_t *
fs_dir_entries(apr_hash_t **table_p,
               svn_fs_root_t *root,
               const char *path,
               apr_pool_t *pool)
{
  dag_node_t *node;

  /* Get the entries for this path in the caller's pool. */
  SVN_ERR(get_dag(&node, root, path, FALSE, pool));
  return svn_fs_fs__dag_dir_entries(table_p, node, pool);
}

/* Raise an error if PATH contains a newline because FSFS cannot handle
 * such paths. See issue #4340. */
static svn_error_t *
check_newline(const char *path, apr_pool_t *pool)
{
  char *c = strchr(path, '\n');

  if (c)
    return svn_error_createf(SVN_ERR_FS_PATH_SYNTAX, NULL,
       _("Invalid control character '0x%02x' in path '%s'"),
       (unsigned char)*c, svn_path_illegal_path_escape(path, pool));

  return SVN_NO_ERROR;
}

/* Create a new directory named PATH in ROOT.  The new directory has
   no entries, and no properties.  ROOT must be the root of a
   transaction, not a revision.  Do any necessary temporary allocation
   in POOL.  */
static svn_error_t *
fs_make_dir(svn_fs_root_t *root,
            const char *path,
            apr_pool_t *pool)
{
  parent_path_t *parent_path;
  dag_node_t *sub_dir;
  const char *txn_id = root->txn;

  SVN_ERR(check_newline(path, pool));

  path = svn_fs__canonicalize_abspath(path, pool);
  SVN_ERR(open_path(&parent_path, root, path, open_path_last_optional,
                    txn_id, pool));

  /* Check (recursively) to see if some lock is 'reserving' a path at
     that location, or even some child-path; if so, check that we can
     use it. */
  if (root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
    SVN_ERR(svn_fs_fs__allow_locked_operation(path, root->fs, TRUE, FALSE,
                                              pool));

  /* If there's already a sub-directory by that name, complain.  This
     also catches the case of trying to make a subdirectory named `/'.  */
  if (parent_path->node)
    return SVN_FS__ALREADY_EXISTS(root, path);

  /* Create the subdirectory.  */
  SVN_ERR(make_path_mutable(root, parent_path->parent, path, pool));
  SVN_ERR(svn_fs_fs__dag_make_dir(&sub_dir,
                                  parent_path->parent->node,
                                  parent_path_path(parent_path->parent,
                                                   pool),
                                  parent_path->entry,
                                  txn_id,
                                  pool));

  /* Add this directory to the path cache. */
  SVN_ERR(dag_node_cache_set(root, parent_path_path(parent_path, pool),
                             sub_dir, pool));

  /* Make a record of this modification in the changes table. */
  return add_change(root->fs, txn_id, path, svn_fs_fs__dag_get_id(sub_dir),
                    svn_fs_path_change_add, FALSE, FALSE, svn_node_dir,
                    SVN_INVALID_REVNUM, NULL, pool);
}


/* Delete the node at PATH under ROOT.  ROOT must be a transaction
   root.  Perform temporary allocations in POOL. */
static svn_error_t *
fs_delete_node(svn_fs_root_t *root,
               const char *path,
               apr_pool_t *pool)
{
  parent_path_t *parent_path;
  const char *txn_id = root->txn;
  apr_int64_t mergeinfo_count = 0;
  svn_node_kind_t kind;

  if (! root->is_txn_root)
    return SVN_FS__NOT_TXN(root);

  path = svn_fs__canonicalize_abspath(path, pool);
  SVN_ERR(open_path(&parent_path, root, path, 0, txn_id, pool));
  kind = svn_fs_fs__dag_node_kind(parent_path->node);

  /* We can't remove the root of the filesystem.  */
  if (! parent_path->parent)
    return svn_error_create(SVN_ERR_FS_ROOT_DIR, NULL,
                            _("The root directory cannot be deleted"));

  /* Check to see if path (or any child thereof) is locked; if so,
     check that we can use the existing lock(s). */
  if (root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
    SVN_ERR(svn_fs_fs__allow_locked_operation(path, root->fs, TRUE, FALSE,
                                              pool));

  /* Make the parent directory mutable, and do the deletion.  */
  SVN_ERR(make_path_mutable(root, parent_path->parent, path, pool));
  if (svn_fs_fs__fs_supports_mergeinfo(root->fs))
    SVN_ERR(svn_fs_fs__dag_get_mergeinfo_count(&mergeinfo_count,
                                               parent_path->node));
  SVN_ERR(svn_fs_fs__dag_delete(parent_path->parent->node,
                                parent_path->entry,
                                txn_id, pool));

  /* Remove this node and any children from the path cache. */
  SVN_ERR(dag_node_cache_invalidate(root, parent_path_path(parent_path, pool),
                                    pool));

  /* Update mergeinfo counts for parents */
  if (mergeinfo_count > 0)
    SVN_ERR(increment_mergeinfo_up_tree(parent_path->parent,
                                        -mergeinfo_count,
                                        pool));

  /* Make a record of this modification in the changes table. */
  return add_change(root->fs, txn_id, path,
                    svn_fs_fs__dag_get_id(parent_path->node),
                    svn_fs_path_change_delete, FALSE, FALSE, kind,
                    SVN_INVALID_REVNUM, NULL, pool);
}


/* Set *SAME_P to TRUE if FS1 and FS2 have the same UUID, else set to FALSE.
   Use POOL for temporary allocation only.
   Note: this code is duplicated between libsvn_fs_fs and libsvn_fs_base. */
static svn_error_t *
fs_same_p(svn_boolean_t *same_p,
          svn_fs_t *fs1,
          svn_fs_t *fs2,
          apr_pool_t *pool)
{
  *same_p = ! strcmp(fs1->uuid, fs2->uuid);
  return SVN_NO_ERROR;
}

/* Copy the node at FROM_PATH under FROM_ROOT to TO_PATH under
   TO_ROOT.  If PRESERVE_HISTORY is set, then the copy is recorded in
   the copies table.  Perform temporary allocations in POOL. */
static svn_error_t *
copy_helper(svn_fs_root_t *from_root,
            const char *from_path,
            svn_fs_root_t *to_root,
            const char *to_path,
            svn_boolean_t preserve_history,
            apr_pool_t *pool)
{
  dag_node_t *from_node;
  parent_path_t *to_parent_path;
  const char *txn_id = to_root->txn;
  svn_boolean_t same_p;

  /* Use an error check, not an assert, because even the caller cannot
     guarantee that a filesystem's UUID has not changed "on the fly". */
  SVN_ERR(fs_same_p(&same_p, from_root->fs, to_root->fs, pool));
  if (! same_p)
    return svn_error_createf
      (SVN_ERR_UNSUPPORTED_FEATURE, NULL,
       _("Cannot copy between two different filesystems ('%s' and '%s')"),
       from_root->fs->path, to_root->fs->path);

  if (from_root->is_txn_root)
    return svn_error_create
      (SVN_ERR_UNSUPPORTED_FEATURE, NULL,
       _("Copy from mutable tree not currently supported"));

  /* Get the NODE for FROM_PATH in FROM_ROOT.*/
  SVN_ERR(get_dag(&from_node, from_root, from_path, TRUE, pool));

  /* Build up the parent path from TO_PATH in TO_ROOT.  If the last
     component does not exist, it's not that big a deal.  We'll just
     make one there. */
  SVN_ERR(open_path(&to_parent_path, to_root, to_path,
                    open_path_last_optional, txn_id, pool));

  /* Check to see if path (or any child thereof) is locked; if so,
     check that we can use the existing lock(s). */
  if (to_root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
    SVN_ERR(svn_fs_fs__allow_locked_operation(to_path, to_root->fs,
                                              TRUE, FALSE, pool));

  /* If the destination node already exists as the same node as the
     source (in other words, this operation would result in nothing
     happening at all), just do nothing an return successfully,
     proud that you saved yourself from a tiresome task. */
  if (to_parent_path->node &&
      svn_fs_fs__id_eq(svn_fs_fs__dag_get_id(from_node),
                       svn_fs_fs__dag_get_id(to_parent_path->node)))
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
          if (svn_fs_fs__fs_supports_mergeinfo(to_root->fs))
            SVN_ERR(svn_fs_fs__dag_get_mergeinfo_count(&mergeinfo_start,
                                                       to_parent_path->node));
        }
      else
        {
          kind = svn_fs_path_change_add;
          mergeinfo_start = 0;
        }

      if (svn_fs_fs__fs_supports_mergeinfo(to_root->fs))
        SVN_ERR(svn_fs_fs__dag_get_mergeinfo_count(&mergeinfo_end,
                                                   from_node));

      /* Make sure the target node's parents are mutable.  */
      SVN_ERR(make_path_mutable(to_root, to_parent_path->parent,
                                to_path, pool));

      /* Canonicalize the copyfrom path. */
      from_canonpath = svn_fs__canonicalize_abspath(from_path, pool);

      SVN_ERR(svn_fs_fs__dag_copy(to_parent_path->parent->node,
                                  to_parent_path->entry,
                                  from_node,
                                  preserve_history,
                                  from_root->rev,
                                  from_canonpath,
                                  txn_id, pool));

      if (kind == svn_fs_path_change_replace)
        SVN_ERR(dag_node_cache_invalidate(to_root,
                                          parent_path_path(to_parent_path,
                                                           pool), pool));

      if (svn_fs_fs__fs_supports_mergeinfo(to_root->fs)
          && mergeinfo_start != mergeinfo_end)
        SVN_ERR(increment_mergeinfo_up_tree(to_parent_path->parent,
                                            mergeinfo_end - mergeinfo_start,
                                            pool));

      /* Make a record of this modification in the changes table. */
      SVN_ERR(get_dag(&new_node, to_root, to_path, TRUE, pool));
      SVN_ERR(add_change(to_root->fs, txn_id, to_path,
                         svn_fs_fs__dag_get_id(new_node), kind, FALSE, FALSE,
                         svn_fs_fs__dag_node_kind(from_node),
                         from_root->rev, from_canonpath, pool));
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
   allocations are from POOL.*/
static svn_error_t *
fs_copy(svn_fs_root_t *from_root,
        const char *from_path,
        svn_fs_root_t *to_root,
        const char *to_path,
        apr_pool_t *pool)
{
  SVN_ERR(check_newline(to_path, pool));

  return svn_error_trace(copy_helper(from_root,
                                     svn_fs__canonicalize_abspath(from_path,
                                                                  pool),
                                     to_root,
                                     svn_fs__canonicalize_abspath(to_path,
                                                                  pool),
                                     TRUE, pool));
}


/* Create a copy of FROM_PATH in FROM_ROOT named TO_PATH in TO_ROOT.
   If FROM_PATH is a directory, copy it recursively.  No history is
   preserved.  Temporary allocations are from POOL. */
static svn_error_t *
fs_revision_link(svn_fs_root_t *from_root,
                 svn_fs_root_t *to_root,
                 const char *path,
                 apr_pool_t *pool)
{
  if (! to_root->is_txn_root)
    return SVN_FS__NOT_TXN(to_root);

  path = svn_fs__canonicalize_abspath(path, pool);
  return svn_error_trace(copy_helper(from_root, path, to_root, path,
                                     FALSE, pool));
}


/* Discover the copy ancestry of PATH under ROOT.  Return a relevant
   ancestor/revision combination in *PATH_P and *REV_P.  Temporary
   allocations are in POOL. */
static svn_error_t *
fs_copied_from(svn_revnum_t *rev_p,
               const char **path_p,
               svn_fs_root_t *root,
               const char *path,
               apr_pool_t *pool)
{
  dag_node_t *node;
  const char *copyfrom_path, *copyfrom_str = NULL;
  svn_revnum_t copyfrom_rev;
  char *str, *buf;

  /* Check to see if there is a cached version of this copyfrom
     entry. */
  if (! root->is_txn_root) {
    fs_rev_root_data_t *frd = root->fsap_data;
    copyfrom_str = svn_hash_gets(frd->copyfrom_cache, path);
  }

  if (copyfrom_str)
    {
      if (*copyfrom_str == 0)
        {
          /* We have a cached entry that says there is no copyfrom
             here. */
          copyfrom_rev = SVN_INVALID_REVNUM;
          copyfrom_path = NULL;
        }
      else
        {
          /* Parse the copyfrom string for our cached entry. */
          buf = apr_pstrdup(pool, copyfrom_str);
          str = svn_cstring_tokenize(" ", &buf);
          copyfrom_rev = SVN_STR_TO_REV(str);
          copyfrom_path = buf;
        }
    }
  else
    {
      /* There is no cached entry, look it up the old-fashioned
         way. */
      SVN_ERR(get_dag(&node, root, path, TRUE, pool));
      SVN_ERR(svn_fs_fs__dag_get_copyfrom_rev(&copyfrom_rev, node));
      SVN_ERR(svn_fs_fs__dag_get_copyfrom_path(&copyfrom_path, node));
    }

  *rev_p  = copyfrom_rev;
  *path_p = copyfrom_path;

  return SVN_NO_ERROR;
}



/* Files.  */

/* Create the empty file PATH under ROOT.  Temporary allocations are
   in POOL. */
static svn_error_t *
fs_make_file(svn_fs_root_t *root,
             const char *path,
             apr_pool_t *pool)
{
  parent_path_t *parent_path;
  dag_node_t *child;
  const char *txn_id = root->txn;

  SVN_ERR(check_newline(path, pool));

  path = svn_fs__canonicalize_abspath(path, pool);
  SVN_ERR(open_path(&parent_path, root, path, open_path_last_optional,
                   txn_id, pool));

  /* If there's already a file by that name, complain.
     This also catches the case of trying to make a file named `/'.  */
  if (parent_path->node)
    return SVN_FS__ALREADY_EXISTS(root, path);

  /* Check (non-recursively) to see if path is locked;  if so, check
     that we can use it. */
  if (root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
    SVN_ERR(svn_fs_fs__allow_locked_operation(path, root->fs, FALSE, FALSE,
                                              pool));

  /* Create the file.  */
  SVN_ERR(make_path_mutable(root, parent_path->parent, path, pool));
  SVN_ERR(svn_fs_fs__dag_make_file(&child,
                                   parent_path->parent->node,
                                   parent_path_path(parent_path->parent,
                                                    pool),
                                   parent_path->entry,
                                   txn_id,
                                   pool));

  /* Add this file to the path cache. */
  SVN_ERR(dag_node_cache_set(root, parent_path_path(parent_path, pool), child,
                             pool));

  /* Make a record of this modification in the changes table. */
  return add_change(root->fs, txn_id, path, svn_fs_fs__dag_get_id(child),
                    svn_fs_path_change_add, TRUE, FALSE, svn_node_file,
                    SVN_INVALID_REVNUM, NULL, pool);
}


/* Set *LENGTH_P to the size of the file PATH under ROOT.  Temporary
   allocations are in POOL. */
static svn_error_t *
fs_file_length(svn_filesize_t *length_p,
               svn_fs_root_t *root,
               const char *path,
               apr_pool_t *pool)
{
  dag_node_t *file;

  /* First create a dag_node_t from the root/path pair. */
  SVN_ERR(get_dag(&file, root, path, FALSE, pool));

  /* Now fetch its length */
  return svn_fs_fs__dag_file_length(length_p, file, pool);
}


/* Set *CHECKSUM to the checksum of type KIND for PATH under ROOT, or
   NULL if that information isn't available.  Temporary allocations
   are from POOL. */
static svn_error_t *
fs_file_checksum(svn_checksum_t **checksum,
                 svn_checksum_kind_t kind,
                 svn_fs_root_t *root,
                 const char *path,
                 apr_pool_t *pool)
{
  dag_node_t *file;

  SVN_ERR(get_dag(&file, root, path, FALSE, pool));
  return svn_fs_fs__dag_file_checksum(checksum, file, kind, pool);
}


/* --- Machinery for svn_fs_file_contents() ---  */

/* Set *CONTENTS to a readable stream that will return the contents of
   PATH under ROOT.  The stream is allocated in POOL. */
static svn_error_t *
fs_file_contents(svn_stream_t **contents,
                 svn_fs_root_t *root,
                 const char *path,
                 apr_pool_t *pool)
{
  dag_node_t *node;
  svn_stream_t *file_stream;

  /* First create a dag_node_t from the root/path pair. */
  SVN_ERR(get_dag(&node, root, path, FALSE, pool));

  /* Then create a readable stream from the dag_node_t. */
  SVN_ERR(svn_fs_fs__dag_get_contents(&file_stream, node, pool));

  *contents = file_stream;
  return SVN_NO_ERROR;
}

/* --- End machinery for svn_fs_file_contents() ---  */


/* --- Machinery for svn_fs_try_process_file_contents() ---  */

static svn_error_t *
fs_try_process_file_contents(svn_boolean_t *success,
                             svn_fs_root_t *root,
                             const char *path,
                             svn_fs_process_contents_func_t processor,
                             void* baton,
                             apr_pool_t *pool)
{
  dag_node_t *node;
  SVN_ERR(get_dag(&node, root, path, FALSE, pool));

  return svn_fs_fs__dag_try_process_file_contents(success, node,
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
  svn_stream_t *string_stream;
  svn_stringbuf_t *target_string;

  /* MD5 digest for the base text against which a delta is to be
     applied, and for the resultant fulltext, respectively.  Either or
     both may be null, in which case ignored. */
  svn_checksum_t *base_checksum;
  svn_checksum_t *result_checksum;

  /* Pool used by db txns */
  apr_pool_t *pool;

} txdelta_baton_t;


/* ### see comment in window_consumer() regarding this function. */

/* Helper function of generic type `svn_write_fn_t'.  Implements a
   writable stream which appends to an svn_stringbuf_t. */
static svn_error_t *
write_to_string(void *baton, const char *data, apr_size_t *len)
{
  txdelta_baton_t *tb = (txdelta_baton_t *) baton;
  svn_stringbuf_appendbytes(tb->target_string, data, *len);
  return SVN_NO_ERROR;
}



/* The main window handler returned by svn_fs_apply_textdelta. */
static svn_error_t *
window_consumer(svn_txdelta_window_t *window, void *baton)
{
  txdelta_baton_t *tb = (txdelta_baton_t *) baton;

  /* Send the window right through to the custom window interpreter.
     In theory, the interpreter will then write more data to
     cb->target_string. */
  SVN_ERR(tb->interpreter(window, tb->interpreter_baton));

  /* ### the write_to_string() callback for the txdelta's output stream
     ### should be doing all the flush determination logic, not here.
     ### in a drastic case, a window could generate a LOT more than the
     ### maximum buffer size. we want to flush to the underlying target
     ### stream much sooner (e.g. also in a streamy fashion). also, by
     ### moving this logic inside the stream, the stream becomes nice
     ### and encapsulated: it holds all the logic about buffering and
     ### flushing.
     ###
     ### further: I believe the buffering should be removed from tree.c
     ### the buffering should go into the target_stream itself, which
     ### is defined by reps-string.c. Specifically, I think the
     ### rep_write_contents() function will handle the buffering and
     ### the spill to the underlying DB. by locating it there, then
     ### anybody who gets a writable stream for FS content can take
     ### advantage of the buffering capability. this will be important
     ### when we export an FS API function for writing a fulltext into
     ### the FS, rather than forcing that fulltext thru apply_textdelta.
  */

  /* Check to see if we need to purge the portion of the contents that
     have been written thus far. */
  if ((! window) || (tb->target_string->len > WRITE_BUFFER_SIZE))
    {
      apr_size_t len = tb->target_string->len;
      SVN_ERR(svn_stream_write(tb->target_stream,
                               tb->target_string->data,
                               &len));
      svn_stringbuf_setempty(tb->target_string);
    }

  /* Is the window NULL?  If so, we're done. */
  if (! window)
    {
      /* Close the internal-use stream.  ### This used to be inside of
         txn_body_fulltext_finalize_edits(), but that invoked a nested
         Berkeley DB transaction -- scandalous! */
      SVN_ERR(svn_stream_close(tb->target_stream));

      SVN_ERR(svn_fs_fs__dag_finalize_edits(tb->node, tb->result_checksum,
                                            tb->pool));
    }

  return SVN_NO_ERROR;
}

/* Helper function for fs_apply_textdelta.  BATON is of type
   txdelta_baton_t. */
static svn_error_t *
apply_textdelta(void *baton, apr_pool_t *pool)
{
  txdelta_baton_t *tb = (txdelta_baton_t *) baton;
  parent_path_t *parent_path;
  const char *txn_id = tb->root->txn;

  /* Call open_path with no flags, as we want this to return an error
     if the node for which we are searching doesn't exist. */
  SVN_ERR(open_path(&parent_path, tb->root, tb->path, 0, txn_id, pool));

  /* Check (non-recursively) to see if path is locked; if so, check
     that we can use it. */
  if (tb->root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
    SVN_ERR(svn_fs_fs__allow_locked_operation(tb->path, tb->root->fs,
                                              FALSE, FALSE, pool));

  /* Now, make sure this path is mutable. */
  SVN_ERR(make_path_mutable(tb->root, parent_path, tb->path, pool));
  tb->node = parent_path->node;

  if (tb->base_checksum)
    {
      svn_checksum_t *checksum;

      /* Until we finalize the node, its data_key points to the old
         contents, in other words, the base text. */
      SVN_ERR(svn_fs_fs__dag_file_checksum(&checksum, tb->node,
                                           tb->base_checksum->kind, pool));
      if (!svn_checksum_match(tb->base_checksum, checksum))
        return svn_checksum_mismatch_err(tb->base_checksum, checksum, pool,
                                         _("Base checksum mismatch on '%s'"),
                                         tb->path);
    }

  /* Make a readable "source" stream out of the current contents of
     ROOT/PATH; obviously, this must done in the context of a db_txn.
     The stream is returned in tb->source_stream. */
  SVN_ERR(svn_fs_fs__dag_get_contents(&(tb->source_stream),
                                      tb->node, tb->pool));

  /* Make a writable "target" stream */
  SVN_ERR(svn_fs_fs__dag_get_edit_stream(&(tb->target_stream), tb->node,
                                         tb->pool));

  /* Make a writable "string" stream which writes data to
     tb->target_string. */
  tb->target_string = svn_stringbuf_create_empty(tb->pool);
  tb->string_stream = svn_stream_create(tb, tb->pool);
  svn_stream_set_write(tb->string_stream, write_to_string);

  /* Now, create a custom window handler that uses our two streams. */
  svn_txdelta_apply(tb->source_stream,
                    tb->string_stream,
                    NULL,
                    tb->path,
                    tb->pool,
                    &(tb->interpreter),
                    &(tb->interpreter_baton));

  /* Make a record of this modification in the changes table. */
  return add_change(tb->root->fs, txn_id, tb->path,
                    svn_fs_fs__dag_get_id(tb->node),
                    svn_fs_path_change_modify, TRUE, FALSE, svn_node_file,
                    SVN_INVALID_REVNUM, NULL, pool);
}


/* Set *CONTENTS_P and *CONTENTS_BATON_P to a window handler and baton
   that will accept text delta windows to modify the contents of PATH
   under ROOT.  Allocations are in POOL. */
static svn_error_t *
fs_apply_textdelta(svn_txdelta_window_handler_t *contents_p,
                   void **contents_baton_p,
                   svn_fs_root_t *root,
                   const char *path,
                   svn_checksum_t *base_checksum,
                   svn_checksum_t *result_checksum,
                   apr_pool_t *pool)
{
  txdelta_baton_t *tb = apr_pcalloc(pool, sizeof(*tb));

  tb->root = root;
  tb->path = svn_fs__canonicalize_abspath(path, pool);
  tb->pool = pool;
  tb->base_checksum = svn_checksum_dup(base_checksum, pool);
  tb->result_checksum = svn_checksum_dup(result_checksum, pool);

  SVN_ERR(apply_textdelta(tb, pool));

  *contents_p = window_consumer;
  *contents_baton_p = tb;
  return SVN_NO_ERROR;
}

/* --- End machinery for svn_fs_apply_textdelta() ---  */

/* --- Machinery for svn_fs_apply_text() ---  */

/* Baton for svn_fs_apply_text(). */
struct text_baton_t
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
};


/* A wrapper around svn_fs_fs__dag_finalize_edits, but for
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
  struct text_baton_t *tb = baton;

  /* Psst, here's some data.  Pass it on to the -real- file stream. */
  return svn_stream_write(tb->file_stream, data, len);
}

/* Close function for the publically returned stream. */
static svn_error_t *
text_stream_closer(void *baton)
{
  struct text_baton_t *tb = baton;

  /* Close the internal-use stream.  ### This used to be inside of
     txn_body_fulltext_finalize_edits(), but that invoked a nested
     Berkeley DB transaction -- scandalous! */
  SVN_ERR(svn_stream_close(tb->file_stream));

  /* Need to tell fs that we're done sending text */
  return svn_fs_fs__dag_finalize_edits(tb->node, tb->result_checksum,
                                       tb->pool);
}


/* Helper function for fs_apply_text.  BATON is of type
   text_baton_t. */
static svn_error_t *
apply_text(void *baton, apr_pool_t *pool)
{
  struct text_baton_t *tb = baton;
  parent_path_t *parent_path;
  const char *txn_id = tb->root->txn;

  /* Call open_path with no flags, as we want this to return an error
     if the node for which we are searching doesn't exist. */
  SVN_ERR(open_path(&parent_path, tb->root, tb->path, 0, txn_id, pool));

  /* Check (non-recursively) to see if path is locked; if so, check
     that we can use it. */
  if (tb->root->txn_flags & SVN_FS_TXN_CHECK_LOCKS)
    SVN_ERR(svn_fs_fs__allow_locked_operation(tb->path, tb->root->fs,
                                              FALSE, FALSE, pool));

  /* Now, make sure this path is mutable. */
  SVN_ERR(make_path_mutable(tb->root, parent_path, tb->path, pool));
  tb->node = parent_path->node;

  /* Make a writable stream for replacing the file's text. */
  SVN_ERR(svn_fs_fs__dag_get_edit_stream(&(tb->file_stream), tb->node,
                                         tb->pool));

  /* Create a 'returnable' stream which writes to the file_stream. */
  tb->stream = svn_stream_create(tb, tb->pool);
  svn_stream_set_write(tb->stream, text_stream_writer);
  svn_stream_set_close(tb->stream, text_stream_closer);

  /* Make a record of this modification in the changes table. */
  return add_change(tb->root->fs, txn_id, tb->path,
                    svn_fs_fs__dag_get_id(tb->node),
                    svn_fs_path_change_modify, TRUE, FALSE, svn_node_file,
                    SVN_INVALID_REVNUM, NULL, pool);
}


/* Return a writable stream that will set the contents of PATH under
   ROOT.  RESULT_CHECKSUM is the MD5 checksum of the final result.
   Temporary allocations are in POOL. */
static svn_error_t *
fs_apply_text(svn_stream_t **contents_p,
              svn_fs_root_t *root,
              const char *path,
              svn_checksum_t *result_checksum,
              apr_pool_t *pool)
{
  struct text_baton_t *tb = apr_pcalloc(pool, sizeof(*tb));

  tb->root = root;
  tb->path = svn_fs__canonicalize_abspath(path, pool);
  tb->pool = pool;
  tb->result_checksum = svn_checksum_dup(result_checksum, pool);

  SVN_ERR(apply_text(tb, pool));

  *contents_p = tb->stream;
  return SVN_NO_ERROR;
}

/* --- End machinery for svn_fs_apply_text() ---  */


/* Check if the contents of PATH1 under ROOT1 are different from the
   contents of PATH2 under ROOT2.  If they are different set
   *CHANGED_P to TRUE, otherwise set it to FALSE. */
static svn_error_t *
fs_contents_changed(svn_boolean_t *changed_p,
                    svn_fs_root_t *root1,
                    const char *path1,
                    svn_fs_root_t *root2,
                    const char *path2,
                    apr_pool_t *pool)
{
  dag_node_t *node1, *node2;

  /* Check that roots are in the same fs. */
  if (root1->fs != root2->fs)
    return svn_error_create
      (SVN_ERR_FS_GENERAL, NULL,
       _("Cannot compare file contents between two different filesystems"));

  /* Check that both paths are files. */
  {
    svn_node_kind_t kind;

    SVN_ERR(svn_fs_fs__check_path(&kind, root1, path1, pool));
    if (kind != svn_node_file)
      return svn_error_createf
        (SVN_ERR_FS_GENERAL, NULL, _("'%s' is not a file"), path1);

    SVN_ERR(svn_fs_fs__check_path(&kind, root2, path2, pool));
    if (kind != svn_node_file)
      return svn_error_createf
        (SVN_ERR_FS_GENERAL, NULL, _("'%s' is not a file"), path2);
  }

  SVN_ERR(get_dag(&node1, root1, path1, TRUE, pool));
  SVN_ERR(get_dag(&node2, root2, path2, TRUE, pool));
  return svn_fs_fs__dag_things_different(NULL, changed_p,
                                         node1, node2);
}



/* Public interface to computing file text deltas.  */

static svn_error_t *
fs_get_file_delta_stream(svn_txdelta_stream_t **stream_p,
                         svn_fs_root_t *source_root,
                         const char *source_path,
                         svn_fs_root_t *target_root,
                         const char *target_path,
                         apr_pool_t *pool)
{
  dag_node_t *source_node, *target_node;

  if (source_root && source_path)
    SVN_ERR(get_dag(&source_node, source_root, source_path, TRUE, pool));
  else
    source_node = NULL;
  SVN_ERR(get_dag(&target_node, target_root, target_path, TRUE, pool));

  /* Create a delta stream that turns the source into the target.  */
  return svn_fs_fs__dag_get_file_delta_stream(stream_p, source_node,
                                              target_node, pool);
}



/* Finding Changes */

/* Set *CHANGED_PATHS_P to a newly allocated hash containing
   descriptions of the paths changed under ROOT.  The hash is keyed
   with const char * paths and has svn_fs_path_change2_t * values.  Use
   POOL for all allocations. */
static svn_error_t *
fs_paths_changed(apr_hash_t **changed_paths_p,
                 svn_fs_root_t *root,
                 apr_pool_t *pool)
{
  if (root->is_txn_root)
    return svn_fs_fs__txn_changes_fetch(changed_paths_p, root->fs, root->txn,
                                        pool);
  else
    {
      fs_rev_root_data_t *frd = root->fsap_data;
      return svn_fs_fs__paths_changed(changed_paths_p, root->fs, root->rev,
                                      frd->copyfrom_cache, pool);
    }
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
                 apr_pool_t *pool);


/* Set *HISTORY_P to an opaque node history object which represents
   PATH under ROOT.  ROOT must be a revision root.  Use POOL for all
   allocations. */
static svn_error_t *
fs_node_history(svn_fs_history_t **history_p,
                svn_fs_root_t *root,
                const char *path,
                apr_pool_t *pool)
{
  svn_node_kind_t kind;

  /* We require a revision root. */
  if (root->is_txn_root)
    return svn_error_create(SVN_ERR_FS_NOT_REVISION_ROOT, NULL, NULL);

  /* And we require that the path exist in the root. */
  SVN_ERR(svn_fs_fs__check_path(&kind, root, path, pool));
  if (kind == svn_node_none)
    return SVN_FS__NOT_FOUND(root, path);

  /* Okay, all seems well.  Build our history object and return it. */
  *history_p = assemble_history(root->fs,
                                svn_fs__canonicalize_abspath(path, pool),
                                root->rev, FALSE, NULL,
                                SVN_INVALID_REVNUM, pool);
  return SVN_NO_ERROR;
}

/* Find the youngest copyroot for path PARENT_PATH or its parents in
   filesystem FS, and store the copyroot in *REV_P and *PATH_P.
   Perform all allocations in POOL. */
static svn_error_t *
find_youngest_copyroot(svn_revnum_t *rev_p,
                       const char **path_p,
                       svn_fs_t *fs,
                       parent_path_t *parent_path,
                       apr_pool_t *pool)
{
  svn_revnum_t rev_mine;
  svn_revnum_t rev_parent = SVN_INVALID_REVNUM;
  const char *path_mine;
  const char *path_parent = NULL;

  /* First find our parent's youngest copyroot. */
  if (parent_path->parent)
    SVN_ERR(find_youngest_copyroot(&rev_parent, &path_parent, fs,
                                   parent_path->parent, pool));

  /* Find our copyroot. */
  SVN_ERR(svn_fs_fs__dag_get_copyroot(&rev_mine, &path_mine,
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


static svn_error_t *fs_closest_copy(svn_fs_root_t **root_p,
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
  svn_node_kind_t kind;

  /* Initialize return values. */
  *root_p = NULL;
  *path_p = NULL;

  path = svn_fs__canonicalize_abspath(path, pool);
  SVN_ERR(open_path(&parent_path, root, path, 0, NULL, pool));

  /* Find the youngest copyroot in the path of this node-rev, which
     will indicate the target of the innermost copy affecting the
     node-rev. */
  SVN_ERR(find_youngest_copyroot(&copy_dst_rev, &copy_dst_path,
                                 fs, parent_path, pool));
  if (copy_dst_rev == 0)  /* There are no copies affecting this node-rev. */
    return SVN_NO_ERROR;

  /* It is possible that this node was created from scratch at some
     revision between COPY_DST_REV and REV.  Make sure that PATH
     exists as of COPY_DST_REV and is related to this node-rev. */
  SVN_ERR(svn_fs_fs__revision_root(&copy_dst_root, fs, copy_dst_rev, pool));
  SVN_ERR(svn_fs_fs__check_path(&kind, copy_dst_root, path, pool));
  if (kind == svn_node_none)
    return SVN_NO_ERROR;
  SVN_ERR(open_path(&copy_dst_parent_path, copy_dst_root, path,
                    open_path_node_only, NULL, pool));
  copy_dst_node = copy_dst_parent_path->node;
  if (! svn_fs_fs__id_check_related(svn_fs_fs__dag_get_id(copy_dst_node),
                                    svn_fs_fs__dag_get_id(parent_path->node)))
    return SVN_NO_ERROR;

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
  SVN_ERR(svn_fs_fs__dag_get_revision(&created_rev, copy_dst_node, pool));
  if (created_rev == copy_dst_rev)
    {
      const svn_fs_id_t *pred;
      SVN_ERR(svn_fs_fs__dag_get_predecessor_id(&pred, copy_dst_node));
      if (! pred)
        return SVN_NO_ERROR;
    }

  /* The copy destination checks out.  Return it. */
  *root_p = copy_dst_root;
  *path_p = copy_dst_path;
  return SVN_NO_ERROR;
}


/* Set *PREV_PATH and *PREV_REV to the path and revision which
   represent the location at which PATH in FS was located immediately
   prior to REVISION iff there was a copy operation (to PATH or one of
   its parent directories) between that previous location and
   PATH@REVISION.

   If there was no such copy operation in that portion of PATH's
   history, set *PREV_PATH to NULL and *PREV_REV to SVN_INVALID_REVNUM.  */
static svn_error_t *
prev_location(const char **prev_path,
              svn_revnum_t *prev_rev,
              svn_fs_t *fs,
              svn_fs_root_t *root,
              const char *path,
              apr_pool_t *pool)
{
  const char *copy_path, *copy_src_path, *remainder_path;
  svn_fs_root_t *copy_root;
  svn_revnum_t copy_src_rev;

  /* Ask about the most recent copy which affected PATH@REVISION.  If
     there was no such copy, we're done.  */
  SVN_ERR(fs_closest_copy(&copy_root, &copy_path, root, path, pool));
  if (! copy_root)
    {
      *prev_rev = SVN_INVALID_REVNUM;
      *prev_path = NULL;
      return SVN_NO_ERROR;
    }

  /* Ultimately, it's not the path of the closest copy's source that
     we care about -- it's our own path's location in the copy source
     revision.  So we'll tack the relative path that expresses the
     difference between the copy destination and our path in the copy
     revision onto the copy source path to determine this information.

     In other words, if our path is "/branches/my-branch/foo/bar", and
     we know that the closest relevant copy was a copy of "/trunk" to
     "/branches/my-branch", then that relative path under the copy
     destination is "/foo/bar".  Tacking that onto the copy source
     path tells us that our path was located at "/trunk/foo/bar"
     before the copy.
  */
  SVN_ERR(fs_copied_from(&copy_src_rev, &copy_src_path,
                         copy_root, copy_path, pool));
  remainder_path = svn_fspath__skip_ancestor(copy_path, path);
  *prev_path = svn_fspath__join(copy_src_path, remainder_path, pool);
  *prev_rev = copy_src_rev;
  return SVN_NO_ERROR;
}


static svn_error_t *
fs_node_origin_rev(svn_revnum_t *revision,
                   svn_fs_root_t *root,
                   const char *path,
                   apr_pool_t *pool)
{
  svn_fs_t *fs = root->fs;
  const svn_fs_id_t *given_noderev_id, *cached_origin_id;
  const char *node_id, *dash;

  path = svn_fs__canonicalize_abspath(path, pool);

  /* Check the cache first. */
  SVN_ERR(svn_fs_fs__node_id(&given_noderev_id, root, path, pool));
  node_id = svn_fs_fs__id_node_id(given_noderev_id);

  /* Is it a brand new uncommitted node? */
  if (node_id[0] == '_')
    {
      *revision = SVN_INVALID_REVNUM;
      return SVN_NO_ERROR;
    }

  /* Maybe this is a new-style node ID that just has the revision
     sitting right in it. */
  dash = strchr(node_id, '-');
  if (dash && *(dash+1))
    {
      *revision = SVN_STR_TO_REV(dash + 1);
      return SVN_NO_ERROR;
    }

  /* The root node always has ID 0, created in revision 0 and will never
     use the new-style ID format. */
  if (strcmp(node_id, "0") == 0)
    {
      *revision = 0;
      return SVN_NO_ERROR;
    }

  /* OK, it's an old-style ID?  Maybe it's cached. */
  SVN_ERR(svn_fs_fs__get_node_origin(&cached_origin_id,
                                     fs,
                                     node_id,
                                     pool));
  if (cached_origin_id != NULL)
    {
      *revision = svn_fs_fs__id_rev(cached_origin_id);
      return SVN_NO_ERROR;
    }

  {
    /* Ah well, the answer isn't in the ID itself or in the cache.
       Let's actually calculate it, then. */
    svn_fs_root_t *curroot = root;
    apr_pool_t *subpool = svn_pool_create(pool);
    apr_pool_t *predidpool = svn_pool_create(pool);
    svn_stringbuf_t *lastpath = svn_stringbuf_create(path, pool);
    svn_revnum_t lastrev = SVN_INVALID_REVNUM;
    dag_node_t *node;
    const svn_fs_id_t *pred_id;

    /* Walk the closest-copy chain back to the first copy in our history.

       NOTE: We merely *assume* that this is faster than walking the
       predecessor chain, because we *assume* that copies of parent
       directories happen less often than modifications to a given item. */
    while (1)
      {
        svn_revnum_t currev;
        const char *curpath = lastpath->data;

        svn_pool_clear(subpool);

        /* Get a root pointing to LASTREV.  (The first time around,
           LASTREV is invalid, but that's cool because CURROOT is
           already initialized.)  */
        if (SVN_IS_VALID_REVNUM(lastrev))
          SVN_ERR(svn_fs_fs__revision_root(&curroot, fs, lastrev, subpool));

        /* Find the previous location using the closest-copy shortcut. */
        SVN_ERR(prev_location(&curpath, &currev, fs, curroot, curpath,
                              subpool));
        if (! curpath)
          break;

        /* Update our LASTPATH and LASTREV variables (which survive
           SUBPOOL). */
        svn_stringbuf_set(lastpath, curpath);
        lastrev = currev;
      }

    /* Walk the predecessor links back to origin. */
    SVN_ERR(svn_fs_fs__node_id(&pred_id, curroot, lastpath->data, predidpool));
    do
      {
        svn_pool_clear(subpool);
        SVN_ERR(svn_fs_fs__dag_get_node(&node, fs, pred_id, subpool));

        /* Why not just fetch the predecessor ID in PREDIDPOOL?
           Because svn_fs_fs__dag_get_predecessor_id() doesn't
           necessarily honor the passed-in pool, and might return a
           value cached in the node (which is allocated in
           SUBPOOL... maybe). */
        svn_pool_clear(predidpool);
        SVN_ERR(svn_fs_fs__dag_get_predecessor_id(&pred_id, node));
        pred_id = pred_id ? svn_fs_fs__id_copy(pred_id, predidpool) : NULL;
      }
    while (pred_id);

    /* When we get here, NODE should be the first node-revision in our
       chain. */
    SVN_ERR(svn_fs_fs__dag_get_revision(revision, node, pool));

    /* Wow, I don't want to have to do all that again.  Let's cache
       the result. */
    if (node_id[0] != '_')
      SVN_ERR(svn_fs_fs__set_node_origin(fs, node_id,
                                         svn_fs_fs__dag_get_id(node), pool));

    svn_pool_destroy(subpool);
    svn_pool_destroy(predidpool);
    return SVN_NO_ERROR;
  }
}


struct history_prev_args
{
  svn_fs_history_t **prev_history_p;
  svn_fs_history_t *history;
  svn_boolean_t cross_copies;
  apr_pool_t *pool;
};


static svn_error_t *
history_prev(void *baton, apr_pool_t *pool)
{
  struct history_prev_args *args = baton;
  svn_fs_history_t **prev_history = args->prev_history_p;
  svn_fs_history_t *history = args->history;
  fs_history_data_t *fhd = history->fsap_data;
  const char *commit_path, *src_path, *path = fhd->path;
  svn_revnum_t commit_rev, src_rev, dst_rev;
  svn_revnum_t revision = fhd->revision;
  apr_pool_t *retpool = args->pool;
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
      if (! args->cross_copies)
        return SVN_NO_ERROR;
      path = fhd->path_hint;
      revision = fhd->rev_hint;
    }

  /* Construct a ROOT for the current revision. */
  SVN_ERR(svn_fs_fs__revision_root(&root, fs, revision, pool));

  /* Open PATH/REVISION, and get its node and a bunch of other
     goodies.  */
  SVN_ERR(open_path(&parent_path, root, path, 0, NULL, pool));
  node = parent_path->node;
  commit_path = svn_fs_fs__dag_get_created_path(node);
  SVN_ERR(svn_fs_fs__dag_get_revision(&commit_rev, node, pool));

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
          *prev_history = assemble_history(fs,
                                           apr_pstrdup(retpool, commit_path),
                                           commit_rev, TRUE, NULL,
                                           SVN_INVALID_REVNUM, retpool);
          return SVN_NO_ERROR;
        }
      else
        {
          /* ... or we *have* reported on this revision, and must now
             progress toward this node's predecessor (unless there is
             no predecessor, in which case we're all done!). */
          const svn_fs_id_t *pred_id;

          SVN_ERR(svn_fs_fs__dag_get_predecessor_id(&pred_id, node));
          if (! pred_id)
            return SVN_NO_ERROR;

          /* Replace NODE and friends with the information from its
             predecessor. */
          SVN_ERR(svn_fs_fs__dag_get_node(&node, fs, pred_id, pool));
          commit_path = svn_fs_fs__dag_get_created_path(node);
          SVN_ERR(svn_fs_fs__dag_get_revision(&commit_rev, node, pool));
        }
    }

  /* Find the youngest copyroot in the path of this node, including
     itself. */
  SVN_ERR(find_youngest_copyroot(&copyroot_rev, &copyroot_path, fs,
                                 parent_path, pool));

  /* Initialize some state variables. */
  src_path = NULL;
  src_rev = SVN_INVALID_REVNUM;
  dst_rev = SVN_INVALID_REVNUM;

  if (copyroot_rev > commit_rev)
    {
      const char *remainder_path;
      const char *copy_dst, *copy_src;
      svn_fs_root_t *copyroot_root;

      SVN_ERR(svn_fs_fs__revision_root(&copyroot_root, fs, copyroot_rev,
                                       pool));
      SVN_ERR(get_dag(&node, copyroot_root, copyroot_path, FALSE, pool));
      copy_dst = svn_fs_fs__dag_get_created_path(node);

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
          SVN_ERR(svn_fs_fs__dag_get_copyfrom_rev(&src_rev, node));
          SVN_ERR(svn_fs_fs__dag_get_copyfrom_path(&copy_src, node));

          dst_rev = copyroot_rev;
          src_path = svn_fspath__join(copy_src, remainder_path, pool);
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

      *prev_history = assemble_history(fs, apr_pstrdup(retpool, path),
                                       dst_rev, ! retry,
                                       src_path, src_rev, retpool);
    }
  else
    {
      *prev_history = assemble_history(fs, apr_pstrdup(retpool, commit_path),
                                       commit_rev, TRUE, NULL,
                                       SVN_INVALID_REVNUM, retpool);
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
                apr_pool_t *pool)
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
                                        1, NULL, SVN_INVALID_REVNUM, pool);
      else if (fhd->revision > 0)
        prev_history = assemble_history(fs, "/", fhd->revision - 1,
                                        1, NULL, SVN_INVALID_REVNUM, pool);
    }
  else
    {
      struct history_prev_args args;
      prev_history = history;

      while (1)
        {
          args.prev_history_p = &prev_history;
          args.history = prev_history;
          args.cross_copies = cross_copies;
          args.pool = pool;
          SVN_ERR(history_prev(&args, pool));

          if (! prev_history)
            break;
          fhd = prev_history->fsap_data;
          if (fhd->is_interesting)
            break;
        }
    }

  *prev_history_p = prev_history;
  return SVN_NO_ERROR;
}


/* Set *PATH and *REVISION to the path and revision for the HISTORY
   object.  Use POOL for all allocations. */
static svn_error_t *
fs_history_location(const char **path,
                    svn_revnum_t *revision,
                    svn_fs_history_t *history,
                    apr_pool_t *pool)
{
  fs_history_data_t *fhd = history->fsap_data;

  *path = apr_pstrdup(pool, fhd->path);
  *revision = fhd->revision;
  return SVN_NO_ERROR;
}

static history_vtable_t history_vtable = {
  fs_history_prev,
  fs_history_location
};

/* Return a new history object (marked as "interesting") for PATH and
   REVISION, allocated in POOL, and with its members set to the values
   of the parameters provided.  Note that PATH and PATH_HINT are not
   duped into POOL -- it is the responsibility of the caller to ensure
   that this happens. */
static svn_fs_history_t *
assemble_history(svn_fs_t *fs,
                 const char *path,
                 svn_revnum_t revision,
                 svn_boolean_t is_interesting,
                 const char *path_hint,
                 svn_revnum_t rev_hint,
                 apr_pool_t *pool)
{
  svn_fs_history_t *history = apr_pcalloc(pool, sizeof(*history));
  fs_history_data_t *fhd = apr_pcalloc(pool, sizeof(*fhd));
  fhd->path = svn_fs__canonicalize_abspath(path, pool);
  fhd->revision = revision;
  fhd->is_interesting = is_interesting;
  fhd->path_hint = path_hint;
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
  apr_hash_t *entries;
  apr_hash_index_t *hi;
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);

  SVN_ERR(svn_fs_fs__dag_dir_entries(&entries, dir_dag,
                                     scratch_pool));

  for (hi = apr_hash_first(scratch_pool, entries);
       hi;
       hi = apr_hash_next(hi))
    {
      svn_fs_dirent_t *dirent = svn__apr_hash_index_val(hi);
      const char *kid_path;
      dag_node_t *kid_dag;
      svn_boolean_t has_mergeinfo, go_down;

      svn_pool_clear(iterpool);

      kid_path = svn_fspath__join(this_path, dirent->name, iterpool);
      SVN_ERR(get_dag(&kid_dag, root, kid_path, TRUE, iterpool));

      SVN_ERR(svn_fs_fs__dag_has_mergeinfo(&has_mergeinfo, kid_dag));
      SVN_ERR(svn_fs_fs__dag_has_descendants_with_mergeinfo(&go_down, kid_dag));

      if (has_mergeinfo)
        {
          /* Save this particular node's mergeinfo. */
          apr_hash_t *proplist;
          svn_mergeinfo_t kid_mergeinfo;
          svn_string_t *mergeinfo_string;
          svn_error_t *err;

          SVN_ERR(svn_fs_fs__dag_get_proplist(&proplist, kid_dag, iterpool));
          mergeinfo_string = svn_hash_gets(proplist, SVN_PROP_MERGEINFO);
          if (!mergeinfo_string)
            {
              svn_string_t *idstr = svn_fs_fs__id_unparse(dirent->id, iterpool);
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
   will be allocated in POOL..
 */
static const char *
mergeinfo_cache_key(const char *path,
                    svn_fs_root_t *rev_root,
                    svn_mergeinfo_inheritance_t inherit,
                    svn_boolean_t adjust_inherited_mergeinfo,
                    apr_pool_t *pool)
{
  apr_int64_t number = rev_root->rev;
  number = number * 4
         + (inherit == svn_mergeinfo_nearest_ancestor ? 2 : 0)
         + (adjust_inherited_mergeinfo ? 1 : 0);

  return svn_fs_fs__combine_number_and_string(number, path, pool);
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

  SVN_ERR(open_path(&parent_path, rev_root, path, 0, NULL, scratch_pool));

  if (inherit == svn_mergeinfo_nearest_ancestor && ! parent_path->parent)
    return SVN_NO_ERROR;

  if (inherit == svn_mergeinfo_nearest_ancestor)
    nearest_ancestor = parent_path->parent;
  else
    nearest_ancestor = parent_path;

  while (TRUE)
    {
      svn_boolean_t has_mergeinfo;

      SVN_ERR(svn_fs_fs__dag_has_mergeinfo(&has_mergeinfo,
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

  SVN_ERR(svn_fs_fs__dag_get_proplist(&proplist, nearest_ancestor->node,
                                      scratch_pool));
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
  fs_fs_data_t *ffd = rev_root->fs->fsap_data;
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

  SVN_ERR(get_dag(&this_dag, root, path, TRUE, scratch_pool));
  SVN_ERR(svn_fs_fs__dag_has_descendants_with_mergeinfo(&go_down,
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
fs_get_mergeinfo(svn_mergeinfo_catalog_t *catalog,
                 svn_fs_root_t *root,
                 const apr_array_header_t *paths,
                 svn_mergeinfo_inheritance_t inherit,
                 svn_boolean_t include_descendants,
                 svn_boolean_t adjust_inherited_mergeinfo,
                 apr_pool_t *result_pool,
                 apr_pool_t *scratch_pool)
{
  fs_fs_data_t *ffd = root->fs->fsap_data;

  /* We require a revision root. */
  if (root->is_txn_root)
    return svn_error_create(SVN_ERR_FS_NOT_REVISION_ROOT, NULL, NULL);

  /* We have to actually be able to find the mergeinfo metadata! */
  if (! svn_fs_fs__fs_supports_mergeinfo(root->fs))
    return svn_error_createf
      (SVN_ERR_UNSUPPORTED_FEATURE, NULL,
       _("Querying mergeinfo requires version %d of the FSFS filesystem "
         "schema; filesystem '%s' uses only version %d"),
       SVN_FS_FS__MIN_MERGEINFO_FORMAT, root->fs->path, ffd->format);

  /* Retrieve a path -> mergeinfo hash mapping. */
  return get_mergeinfos_for_paths(root, catalog, paths,
                                  inherit,
                                  include_descendants,
                                  adjust_inherited_mergeinfo,
                                  result_pool, scratch_pool);
}


/* The vtable associated with root objects. */
static root_vtable_t root_vtable = {
  fs_paths_changed,
  svn_fs_fs__check_path,
  fs_node_history,
  svn_fs_fs__node_id,
  svn_fs_fs__node_created_rev,
  fs_node_origin_rev,
  fs_node_created_path,
  fs_delete_node,
  fs_copied_from,
  fs_closest_copy,
  fs_node_prop,
  fs_node_proplist,
  fs_change_node_prop,
  fs_props_changed,
  fs_dir_entries,
  fs_make_dir,
  fs_copy,
  fs_revision_link,
  fs_file_length,
  fs_file_checksum,
  fs_file_contents,
  fs_try_process_file_contents,
  fs_make_file,
  fs_apply_textdelta,
  fs_apply_text,
  fs_contents_changed,
  fs_get_file_delta_stream,
  fs_merge,
  fs_get_mergeinfo,
};

/* Construct a new root object in FS, allocated from POOL.  */
static svn_fs_root_t *
make_root(svn_fs_t *fs,
          apr_pool_t *pool)
{
  svn_fs_root_t *root = apr_pcalloc(pool, sizeof(*root));

  root->fs = fs;
  root->pool = pool;
  root->vtable = &root_vtable;

  return root;
}


/* Construct a root object referring to the root of REVISION in FS,
   whose root directory is ROOT_DIR.  Create the new root in POOL.  */
static svn_fs_root_t *
make_revision_root(svn_fs_t *fs,
                   svn_revnum_t rev,
                   dag_node_t *root_dir,
                   apr_pool_t *pool)
{
  svn_fs_root_t *root = make_root(fs, pool);
  fs_rev_root_data_t *frd = apr_pcalloc(root->pool, sizeof(*frd));

  root->is_txn_root = FALSE;
  root->rev = rev;

  frd->root_dir = root_dir;
  frd->copyfrom_cache = svn_hash__make(root->pool);

  root->fsap_data = frd;

  return root;
}


/* Construct a root object referring to the root of the transaction
   named TXN and based on revision BASE_REV in FS, with FLAGS to
   describe transaction's behavior.  Create the new root in POOL.  */
static svn_error_t *
make_txn_root(svn_fs_root_t **root_p,
              svn_fs_t *fs,
              const char *txn,
              svn_revnum_t base_rev,
              apr_uint32_t flags,
              apr_pool_t *pool)
{
  svn_fs_root_t *root = make_root(fs, pool);
  fs_txn_root_data_t *frd = apr_pcalloc(root->pool, sizeof(*frd));

  root->is_txn_root = TRUE;
  root->txn = apr_pstrdup(root->pool, txn);
  root->txn_flags = flags;
  root->rev = base_rev;

  frd->txn_id = txn;

  /* Because this cache actually tries to invalidate elements, keep
     the number of elements per page down.

     Note that since dag_node_cache_invalidate uses svn_cache__iter,
     this *cannot* be a memcache-based cache.  */
  SVN_ERR(svn_cache__create_inprocess(&(frd->txn_node_cache),
                                      svn_fs_fs__dag_serialize,
                                      svn_fs_fs__dag_deserialize,
                                      APR_HASH_KEY_STRING,
                                      32, 20, FALSE,
                                      apr_pstrcat(pool, txn, ":TXN",
                                                  (char *)NULL),
                                      root->pool));

  /* Initialize transaction-local caches in FS.

     Note that we cannot put those caches in frd because that content
     fs root object is not available where we would need it. */
  SVN_ERR(svn_fs_fs__initialize_txn_caches(fs, txn, pool));

  root->fsap_data = frd;

  *root_p = root;
  return SVN_NO_ERROR;
}



/* Verify. */
static APR_INLINE const char *
stringify_node(dag_node_t *node,
               apr_pool_t *pool)
{
  /* ### TODO: print some PATH@REV to it, too. */
  return svn_fs_fs__id_unparse(svn_fs_fs__dag_get_id(node), pool)->data;
}

/* Check metadata sanity on NODE, and on its children.  Manually verify
   information for DAG nodes in revision REV, and trust the metadata
   accuracy for nodes belonging to older revisions. */
static svn_error_t *
verify_node(dag_node_t *node,
            svn_revnum_t rev,
            apr_pool_t *pool)
{
  svn_boolean_t has_mergeinfo;
  apr_int64_t mergeinfo_count;
  const svn_fs_id_t *pred_id;
  svn_fs_t *fs = svn_fs_fs__dag_get_fs(node);
  int pred_count;
  svn_node_kind_t kind;
  apr_pool_t *iterpool = svn_pool_create(pool);

  /* Fetch some data. */
  SVN_ERR(svn_fs_fs__dag_has_mergeinfo(&has_mergeinfo, node));
  SVN_ERR(svn_fs_fs__dag_get_mergeinfo_count(&mergeinfo_count, node));
  SVN_ERR(svn_fs_fs__dag_get_predecessor_id(&pred_id, node));
  SVN_ERR(svn_fs_fs__dag_get_predecessor_count(&pred_count, node));
  kind = svn_fs_fs__dag_node_kind(node);

  /* Sanity check. */
  if (mergeinfo_count < 0)
    return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                             "Negative mergeinfo-count %" APR_INT64_T_FMT
                             " on node '%s'",
                             mergeinfo_count, stringify_node(node, iterpool));

  /* Issue #4129. (This check will explicitly catch non-root instances too.) */
  if (pred_id)
    {
      dag_node_t *pred;
      int pred_pred_count;
      SVN_ERR(svn_fs_fs__dag_get_node(&pred, fs, pred_id, iterpool));
      SVN_ERR(svn_fs_fs__dag_get_predecessor_count(&pred_pred_count, pred));
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
      apr_hash_t *entries;
      apr_hash_index_t *hi;
      apr_int64_t children_mergeinfo = 0;

      SVN_ERR(svn_fs_fs__dag_dir_entries(&entries, node, pool));

      /* Compute CHILDREN_MERGEINFO. */
      for (hi = apr_hash_first(pool, entries);
           hi;
           hi = apr_hash_next(hi))
        {
          svn_fs_dirent_t *dirent = svn__apr_hash_index_val(hi);
          dag_node_t *child;
          svn_revnum_t child_rev;
          apr_int64_t child_mergeinfo;

          svn_pool_clear(iterpool);

          /* Compute CHILD_REV. */
          SVN_ERR(svn_fs_fs__dag_get_node(&child, fs, dirent->id, iterpool));
          SVN_ERR(svn_fs_fs__dag_get_revision(&child_rev, child, iterpool));

          if (child_rev == rev)
            SVN_ERR(verify_node(child, rev, iterpool));

          SVN_ERR(svn_fs_fs__dag_get_mergeinfo_count(&child_mergeinfo, child));
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
    }

  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__verify_root(svn_fs_root_t *root,
                       apr_pool_t *pool)
{
  svn_fs_t *fs = root->fs;
  dag_node_t *root_dir;

  /* Issue #4129: bogus pred-counts and minfo-cnt's on the root node-rev
     (and elsewhere).  This code makes more thorough checks than the
     commit-time checks in validate_root_noderev(). */

  /* Callers should disable caches by setting SVN_FS_CONFIG_FSFS_CACHE_NS;
     see r1462436.

     When this code is called in the library, we want to ensure we
     use the on-disk data --- rather than some data that was read
     in the possibly-distance past and cached since. */

  if (root->is_txn_root)
    {
      fs_txn_root_data_t *frd = root->fsap_data;
      SVN_ERR(svn_fs_fs__dag_txn_root(&root_dir, fs, frd->txn_id, pool));
    }
  else
    {
      fs_rev_root_data_t *frd = root->fsap_data;
      root_dir = frd->root_dir;
    }

  /* Recursively verify ROOT_DIR. */
  SVN_ERR(verify_node(root_dir, root->rev, pool));

  /* Verify explicitly the predecessor of the root. */
  {
    const svn_fs_id_t *pred_id;

    /* Only r0 should have no predecessor. */
    SVN_ERR(svn_fs_fs__dag_get_predecessor_id(&pred_id, root_dir));
    if (! root->is_txn_root && !!pred_id != !!root->rev)
      return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                               "r%ld's root node's predecessor is "
                               "unexpectedly '%s'",
                               root->rev,
                               (pred_id
                                ? svn_fs_fs__id_unparse(pred_id, pool)->data
                                : "(null)"));
    if (root->is_txn_root && !pred_id)
      return svn_error_createf(SVN_ERR_FS_CORRUPT, NULL,
                               "Transaction '%s''s root node's predecessor is "
                               "unexpectedly NULL",
                               root->txn);

    /* Check the predecessor's revision. */
    if (pred_id)
      {
        svn_revnum_t pred_rev = svn_fs_fs__id_rev(pred_id);
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
