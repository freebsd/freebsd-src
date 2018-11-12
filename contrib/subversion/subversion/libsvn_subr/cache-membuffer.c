/*
 * cache-membuffer.c: in-memory caching for Subversion
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

#include <assert.h>
#include <apr_md5.h>
#include <apr_thread_rwlock.h>

#include "svn_pools.h"
#include "svn_checksum.h"
#include "md5.h"
#include "svn_private_config.h"
#include "cache.h"
#include "svn_string.h"
#include "private/svn_dep_compat.h"
#include "private/svn_mutex.h"
#include "private/svn_pseudo_md5.h"

/*
 * This svn_cache__t implementation actually consists of two parts:
 * a shared (per-process) singleton membuffer cache instance and shallow
 * svn_cache__t front-end instances that each use different key spaces.
 * For data management, they all forward to the singleton membuffer cache.
 *
 * A membuffer cache consists of two parts:
 *
 * 1. A linear data buffer containing cached items in a serialized
 *    representation. There may be arbitrary gaps between entries.
 * 2. A directory of cache entries. This is organized similar to CPU
 *    data caches: for every possible key, there is exactly one group
 *    of entries that may contain the header info for an item with
 *    that given key. The result is a GROUP_SIZE-way associative cache.
 *
 * Only the start address of these two data parts are given as a native
 * pointer. All other references are expressed as offsets to these pointers.
 * With that design, it is relatively easy to share the same data structure
 * between different processes and / or to persist them on disk. These
 * out-of-process features have not been implemented, yet.
 *
 * The data buffer usage information is implicitly given by the directory
 * entries. Every USED entry has a reference to the previous and the next
 * used dictionary entry and this double-linked list is ordered by the
 * offsets of their item data within the data buffer. So removing data,
 * for instance, is done simply by unlinking it from the chain, implicitly
 * marking the entry as well as the data buffer section previously
 * associated to it as unused.
 *
 * Insertion can occur at only one, sliding position. It is marked by its
 * offset in the data buffer plus the index of the first used entry at or
 * behind that position. If this gap is too small to accommodate the new
 * item, the insertion window is extended as described below. The new entry
 * will always be inserted at the bottom end of the window and since the
 * next used entry is known, properly sorted insertion is possible.
 *
 * To make the cache perform robustly in a wide range of usage scenarios,
 * a randomized variant of LFU is used (see ensure_data_insertable for
 * details). Every item holds a read hit counter and there is a global read
 * hit counter. The more hits an entry has in relation to the average, the
 * more it is likely to be kept using a rand()-based condition. The test is
 * applied only to the entry following the insertion window. If it doesn't
 * get evicted, it is moved to the begin of that window and the window is
 * moved.
 *
 * Moreover, the entry's hits get halved to make that entry more likely to
 * be removed the next time the sliding insertion / removal window comes by.
 * As a result, frequently used entries are likely not to be dropped until
 * they get not used for a while. Also, even a cache thrashing situation
 * about 50% of the content survives every 50% of the cache being re-written
 * with new entries. For details on the fine-tuning involved, see the
 * comments in ensure_data_insertable().
 *
 * To limit the entry size and management overhead, not the actual item keys
 * but only their MD5 checksums will not be stored. This is reasonably safe
 * to do since users have only limited control over the full keys, even if
 * these contain folder paths. So, it is very hard to deliberately construct
 * colliding keys. Random checksum collisions can be shown to be extremely
 * unlikely.
 *
 * All access to the cached data needs to be serialized. Because we want
 * to scale well despite that bottleneck, we simply segment the cache into
 * a number of independent caches (segments). Items will be multiplexed based
 * on their hash key.
 */

/* APR's read-write lock implementation on Windows is horribly inefficient.
 * Even with very low contention a runtime overhead of 35% percent has been
 * measured for 'svn-bench null-export' over ra_serf.
 *
 * Use a simple mutex on Windows.  Because there is one mutex per segment,
 * large machines should (and usually can) be configured with large caches
 * such that read contention is kept low.  This is basically the situation
 * we head before 1.8.
 */
#ifdef WIN32
#  define USE_SIMPLE_MUTEX 1
#else
#  define USE_SIMPLE_MUTEX 0
#endif

/* A 16-way associative cache seems to be a good compromise between
 * performance (worst-case lookups) and efficiency-loss due to collisions.
 *
 * This value may be changed to any positive integer.
 */
#define GROUP_SIZE 16

/* For more efficient copy operations, let's align all data items properly.
 * Must be a power of 2.
 */
#define ITEM_ALIGNMENT 16

/* By default, don't create cache segments smaller than this value unless
 * the total cache size itself is smaller.
 */
#define DEFAULT_MIN_SEGMENT_SIZE APR_UINT64_C(0x2000000)

/* The minimum segment size we will allow for multi-segmented caches
 */
#define MIN_SEGMENT_SIZE APR_UINT64_C(0x10000)

/* The maximum number of segments allowed. Larger numbers reduce the size
 * of each segment, in turn reducing the max size of a cachable item.
 * Also, each segment gets its own lock object. The actual number supported
 * by the OS may therefore be lower and svn_cache__membuffer_cache_create
 * may return an error.
 */
#define MAX_SEGMENT_COUNT 0x10000

/* As of today, APR won't allocate chunks of 4GB or more. So, limit the
 * segment size to slightly below that.
 */
#define MAX_SEGMENT_SIZE APR_UINT64_C(0xffff0000)

/* We don't mark the initialization status for every group but initialize
 * a number of groups at once. That will allow for a very small init flags
 * vector that is likely to fit into the CPU caches even for fairly large
 * membuffer caches. For instance, the default of 32 means 8x32 groups per
 * byte, i.e. 8 flags/byte x 32 groups/flag x 8 entries/group x 40 index
 * bytes/entry x 8 cache bytes/index byte = 1kB init vector / 640MB cache.
 */
#define GROUP_INIT_GRANULARITY 32

/* Invalid index reference value. Equivalent to APR_UINT32_T(-1)
 */
#define NO_INDEX APR_UINT32_MAX

/* To save space in our group structure, we only use 32 bit size values
 * and, therefore, limit the size of each entry to just below 4GB.
 * Supporting larger items is not a good idea as the data transfer
 * to and from the cache would block other threads for a very long time.
 */
#define MAX_ITEM_SIZE ((apr_uint32_t)(0 - ITEM_ALIGNMENT))

/* A 16 byte key type. We use that to identify cache entries.
 * The notation as just two integer values will cause many compilers
 * to create better code.
 */
typedef apr_uint64_t entry_key_t[2];

/* Debugging / corruption detection support.
 * If you define this macro, the getter functions will performed expensive
 * checks on the item data, requested keys and entry types. If there is
 * a mismatch found in any of them when being compared with the values
 * remembered in the setter function, an error will be returned.
 */
#ifdef SVN_DEBUG_CACHE_MEMBUFFER

/* The prefix passed to svn_cache__create_membuffer_cache() effectively
 * defines the type of all items stored by that cache instance. We'll take
 * the last 7 bytes + \0 as plaintext for easy identification by the dev.
 */
#define PREFIX_TAIL_LEN 8

/* This record will be attached to any cache entry. It tracks item data
 * (content), key and type as hash values and is the baseline against which
 * the getters will compare their results to detect inconsistencies.
 */
typedef struct entry_tag_t
{
  /* MD5 checksum over the serialized the item data.
   */
  unsigned char content_hash [APR_MD5_DIGESTSIZE];

  /* Hash value of the svn_cache_t instance that wrote the item
   * (i.e. a combination of type and repository)
   */
  unsigned char prefix_hash [APR_MD5_DIGESTSIZE];

  /* Note that this only covers the variable part of the key,
   * i.e. it will be different from the full key hash used for
   * cache indexing.
   */
  unsigned char key_hash [APR_MD5_DIGESTSIZE];

  /* Last letters from of the key in human readable format
   * (ends with the type identifier, e.g. "DAG")
   */
  char prefix_tail[PREFIX_TAIL_LEN];

  /* Length of the variable key part.
   */
  apr_size_t key_len;

} entry_tag_t;

/* Per svn_cache_t instance initialization helper.
 */
static void get_prefix_tail(const char *prefix, char *prefix_tail)
{
  apr_size_t len = strlen(prefix);
  apr_size_t to_copy = len > PREFIX_TAIL_LEN-1 ? PREFIX_TAIL_LEN-1 : len;

  memset(prefix_tail, 0, PREFIX_TAIL_LEN);
  memcpy(prefix_tail, prefix + len - to_copy, to_copy);
}

/* Initialize all members of TAG except for the content hash.
 */
static svn_error_t *store_key_part(entry_tag_t *tag,
                                   entry_key_t prefix_hash,
                                   char *prefix_tail,
                                   const void *key,
                                   apr_size_t key_len,
                                   apr_pool_t *pool)
{
  svn_checksum_t *checksum;
  SVN_ERR(svn_checksum(&checksum,
                       svn_checksum_md5,
                       key,
                       key_len,
                       pool));

  memcpy(tag->prefix_hash, prefix_hash, sizeof(tag->prefix_hash));
  memcpy(tag->key_hash, checksum->digest, sizeof(tag->key_hash));
  memcpy(tag->prefix_tail, prefix_tail, sizeof(tag->prefix_tail));

  tag->key_len = key_len;

  return SVN_NO_ERROR;
}

/* Initialize the content hash member of TAG.
 */
static svn_error_t* store_content_part(entry_tag_t *tag,
                                       const char *data,
                                       apr_size_t size,
                                       apr_pool_t *pool)
{
  svn_checksum_t *checksum;
  SVN_ERR(svn_checksum(&checksum,
                       svn_checksum_md5,
                       data,
                       size,
                       pool));

  memcpy(tag->content_hash, checksum->digest, sizeof(tag->content_hash));

  return SVN_NO_ERROR;
}

/* Compare two tags and fail with an assertion upon differences.
 */
static svn_error_t* assert_equal_tags(const entry_tag_t *lhs,
                                      const entry_tag_t *rhs)
{
  SVN_ERR_ASSERT(memcmp(lhs->content_hash, rhs->content_hash,
                        sizeof(lhs->content_hash)) == 0);
  SVN_ERR_ASSERT(memcmp(lhs->prefix_hash, rhs->prefix_hash,
                        sizeof(lhs->prefix_hash)) == 0);
  SVN_ERR_ASSERT(memcmp(lhs->key_hash, rhs->key_hash,
                        sizeof(lhs->key_hash)) == 0);
  SVN_ERR_ASSERT(memcmp(lhs->prefix_tail, rhs->prefix_tail,
                        sizeof(lhs->prefix_tail)) == 0);

  SVN_ERR_ASSERT(lhs->key_len == rhs->key_len);

  return SVN_NO_ERROR;
}

/* Reoccurring code snippets.
 */

#define DEBUG_CACHE_MEMBUFFER_TAG_ARG entry_tag_t *tag,

#define DEBUG_CACHE_MEMBUFFER_TAG tag,

#define DEBUG_CACHE_MEMBUFFER_INIT_TAG                         \
  entry_tag_t _tag;                                            \
  entry_tag_t *tag = &_tag;                                    \
  SVN_ERR(store_key_part(tag,                                  \
                         cache->prefix,                        \
                         cache->prefix_tail,                   \
                         key,                                  \
                         cache->key_len == APR_HASH_KEY_STRING \
                             ? strlen((const char *) key)      \
                             : cache->key_len,                 \
                         cache->pool));

#else

/* Don't generate any checks if consistency checks have not been enabled.
 */
#define DEBUG_CACHE_MEMBUFFER_TAG_ARG
#define DEBUG_CACHE_MEMBUFFER_TAG
#define DEBUG_CACHE_MEMBUFFER_INIT_TAG

#endif /* SVN_DEBUG_CACHE_MEMBUFFER */

/* A single dictionary entry. Since all entries will be allocated once
 * during cache creation, those entries might be either used or unused.
 * An entry is used if and only if it is contained in the doubly-linked
 * list of used entries.
 */
typedef struct entry_t
{
  /* Identifying the data item. Only valid for used entries.
   */
  entry_key_t key;

  /* The offset of the cached item's serialized data within the data buffer.
   */
  apr_uint64_t offset;

  /* Size of the serialized item data. May be 0.
   * Only valid for used entries.
   */
  apr_size_t size;

  /* Number of (read) hits for this entry. Will be reset upon write.
   * Only valid for used entries.
   */
  apr_uint32_t hit_count;

  /* Reference to the next used entry in the order defined by offset.
   * NO_INDEX indicates the end of the list; this entry must be referenced
   * by the caches membuffer_cache_t.last member. NO_INDEX also implies
   * that the data buffer is not used beyond offset+size.
   * Only valid for used entries.
   */
  apr_uint32_t next;

  /* Reference to the previous used entry in the order defined by offset.
   * NO_INDEX indicates the end of the list; this entry must be referenced
   * by the caches membuffer_cache_t.first member.
   * Only valid for used entries.
   */
  apr_uint32_t previous;

#ifdef SVN_DEBUG_CACHE_MEMBUFFER
  /* Remember type, content and key hashes.
   */
  entry_tag_t tag;
#endif
} entry_t;

/* We group dictionary entries to make this GROUP-SIZE-way associative.
 */
typedef struct entry_group_t
{
  /* number of entries used [0 .. USED-1] */
  apr_uint32_t used;

  /* the actual entries */
  entry_t entries[GROUP_SIZE];
} entry_group_t;

/* The cache header structure.
 */
struct svn_membuffer_t
{
  /* Number of cache segments. Must be a power of 2.
     Please note that this structure represents only one such segment
     and that all segments must / will report the same values here. */
  apr_uint32_t segment_count;

  /* The dictionary, GROUP_SIZE * group_count entries long. Never NULL.
   */
  entry_group_t *directory;

  /* Flag array with group_count / GROUP_INIT_GRANULARITY _bit_ elements.
   * Allows for efficiently marking groups as "not initialized".
   */
  unsigned char *group_initialized;

  /* Size of dictionary in groups. Must be > 0.
   */
  apr_uint32_t group_count;

  /* Reference to the first (defined by the order content in the data
   * buffer) dictionary entry used by any data item.
   * NO_INDEX for an empty cache.
   */
  apr_uint32_t first;

  /* Reference to the last (defined by the order content in the data
   * buffer) dictionary entry used by any data item.
   * NO_INDEX for an empty cache.
   */
  apr_uint32_t last;

  /* Reference to the first (defined by the order content in the data
   * buffer) used dictionary entry behind the insertion position
   * (current_data). If NO_INDEX, the data buffer is free starting at the
   * current_data offset.
   */
  apr_uint32_t next;


  /* Pointer to the data buffer, data_size bytes long. Never NULL.
   */
  unsigned char *data;

  /* Size of data buffer in bytes. Must be > 0.
   */
  apr_uint64_t data_size;

  /* Offset in the data buffer where the next insertion shall occur.
   */
  apr_uint64_t current_data;

  /* Total number of data buffer bytes in use.
   */
  apr_uint64_t data_used;

  /* Largest entry size that we would accept.  For total cache sizes
   * less than 4TB (sic!), this is determined by the total cache size.
   */
  apr_uint64_t max_entry_size;


  /* Number of used dictionary entries, i.e. number of cached items.
   * In conjunction with hit_count, this is used calculate the average
   * hit count as part of the randomized LFU algorithm.
   */
  apr_uint32_t used_entries;

  /* Sum of (read) hit counts of all used dictionary entries.
   * In conjunction used_entries used_entries, this is used calculate
   * the average hit count as part of the randomized LFU algorithm.
   */
  apr_uint64_t hit_count;


  /* Total number of calls to membuffer_cache_get.
   * Purely statistical information that may be used for profiling.
   */
  apr_uint64_t total_reads;

  /* Total number of calls to membuffer_cache_set.
   * Purely statistical information that may be used for profiling.
   */
  apr_uint64_t total_writes;

  /* Total number of hits since the cache's creation.
   * Purely statistical information that may be used for profiling.
   */
  apr_uint64_t total_hits;

#if APR_HAS_THREADS
  /* A lock for intra-process synchronization to the cache, or NULL if
   * the cache's creator doesn't feel the cache needs to be
   * thread-safe.
   */
#  if USE_SIMPLE_MUTEX
  svn_mutex__t *lock;
#  else
  apr_thread_rwlock_t *lock;
#  endif

  /* If set, write access will wait until they get exclusive access.
   * Otherwise, they will become no-ops if the segment is currently
   * read-locked.  Only used when LOCK is an r/w lock.
   */
  svn_boolean_t allow_blocking_writes;
#endif
};

/* Align integer VALUE to the next ITEM_ALIGNMENT boundary.
 */
#define ALIGN_VALUE(value) (((value) + ITEM_ALIGNMENT-1) & -ITEM_ALIGNMENT)

/* Align POINTER value to the next ITEM_ALIGNMENT boundary.
 */
#define ALIGN_POINTER(pointer) ((void*)ALIGN_VALUE((apr_size_t)(char*)(pointer)))

/* If locking is supported for CACHE, acquire a read lock for it.
 */
static svn_error_t *
read_lock_cache(svn_membuffer_t *cache)
{
#if APR_HAS_THREADS
#  if USE_SIMPLE_MUTEX
  return svn_mutex__lock(cache->lock);
#  else
  if (cache->lock)
  {
    apr_status_t status = apr_thread_rwlock_rdlock(cache->lock);
    if (status)
      return svn_error_wrap_apr(status, _("Can't lock cache mutex"));
  }
#  endif
#endif
  return SVN_NO_ERROR;
}

/* If locking is supported for CACHE, acquire a write lock for it.
 */
static svn_error_t *
write_lock_cache(svn_membuffer_t *cache, svn_boolean_t *success)
{
#if APR_HAS_THREADS
#  if USE_SIMPLE_MUTEX

  return svn_mutex__lock(cache->lock);

#  else

  if (cache->lock)
    {
      apr_status_t status;
      if (cache->allow_blocking_writes)
        {
          status = apr_thread_rwlock_wrlock(cache->lock);
        }
      else
        {
          status = apr_thread_rwlock_trywrlock(cache->lock);
          if (SVN_LOCK_IS_BUSY(status))
            {
              *success = FALSE;
              status = APR_SUCCESS;
            }
        }

      if (status)
        return svn_error_wrap_apr(status,
                                  _("Can't write-lock cache mutex"));
    }

#  endif
#endif
  return SVN_NO_ERROR;
}

/* If locking is supported for CACHE, acquire an unconditional write lock
 * for it.
 */
static svn_error_t *
force_write_lock_cache(svn_membuffer_t *cache)
{
#if APR_HAS_THREADS
#  if USE_SIMPLE_MUTEX

  return svn_mutex__lock(cache->lock);

#  else

  apr_status_t status = apr_thread_rwlock_wrlock(cache->lock);
  if (status)
    return svn_error_wrap_apr(status,
                              _("Can't write-lock cache mutex"));

#  endif
#endif
  return SVN_NO_ERROR;
}

/* If locking is supported for CACHE, release the current lock
 * (read or write).
 */
static svn_error_t *
unlock_cache(svn_membuffer_t *cache, svn_error_t *err)
{
#if APR_HAS_THREADS
#  if USE_SIMPLE_MUTEX

  return svn_mutex__unlock(cache->lock, err);

#  else

  if (cache->lock)
  {
    apr_status_t status = apr_thread_rwlock_unlock(cache->lock);
    if (err)
      return err;

    if (status)
      return svn_error_wrap_apr(status, _("Can't unlock cache mutex"));
  }

#  endif
#endif
  return err;
}

/* If supported, guard the execution of EXPR with a read lock to cache.
 * Macro has been modeled after SVN_MUTEX__WITH_LOCK.
 */
#define WITH_READ_LOCK(cache, expr)         \
do {                                        \
  SVN_ERR(read_lock_cache(cache));          \
  SVN_ERR(unlock_cache(cache, (expr)));     \
} while (0)

/* If supported, guard the execution of EXPR with a write lock to cache.
 * Macro has been modeled after SVN_MUTEX__WITH_LOCK.
 *
 * The write lock process is complicated if we don't allow to wait for
 * the lock: If we didn't get the lock, we may still need to remove an
 * existing entry for the given key because that content is now stale.
 * Once we discovered such an entry, we unconditionally do a blocking
 * wait for the write lock.  In case no old content could be found, a
 * failing lock attempt is simply a no-op and we exit the macro.
 */
#define WITH_WRITE_LOCK(cache, expr)                            \
do {                                                            \
  svn_boolean_t got_lock = TRUE;                                \
  SVN_ERR(write_lock_cache(cache, &got_lock));                  \
  if (!got_lock)                                                \
    {                                                           \
      svn_boolean_t exists;                                     \
      SVN_ERR(entry_exists(cache, group_index, key, &exists));  \
      if (exists)                                               \
        SVN_ERR(force_write_lock_cache(cache));                 \
      else                                                      \
        break;                                                  \
    }                                                           \
  SVN_ERR(unlock_cache(cache, (expr)));                         \
} while (0)

/* Resolve a dictionary entry reference, i.e. return the entry
 * for the given IDX.
 */
static APR_INLINE entry_t *
get_entry(svn_membuffer_t *cache, apr_uint32_t idx)
{
  return &cache->directory[idx / GROUP_SIZE].entries[idx % GROUP_SIZE];
}

/* Get the entry references for the given ENTRY.
 */
static APR_INLINE apr_uint32_t
get_index(svn_membuffer_t *cache, entry_t *entry)
{
  apr_size_t group_index
    = ((char *)entry - (char *)cache->directory) / sizeof(entry_group_t);

  return (apr_uint32_t)group_index * GROUP_SIZE
       + (apr_uint32_t)(entry - cache->directory[group_index].entries);
}

/* Remove the used ENTRY from the CACHE, i.e. make it "unused".
 * In contrast to insertion, removal is possible for any entry.
 */
static void
drop_entry(svn_membuffer_t *cache, entry_t *entry)
{
  /* the group that ENTRY belongs to plus a number of useful index values
   */
  apr_uint32_t idx = get_index(cache, entry);
  apr_uint32_t group_index = idx / GROUP_SIZE;
  entry_group_t *group = &cache->directory[group_index];
  apr_uint32_t last_in_group = group_index * GROUP_SIZE + group->used - 1;

  /* Only valid to be called for used entries.
   */
  assert(idx <= last_in_group);

  /* update global cache usage counters
   */
  cache->used_entries--;
  cache->hit_count -= entry->hit_count;
  cache->data_used -= entry->size;

  /* extend the insertion window, if the entry happens to border it
   */
  if (idx == cache->next)
    cache->next = entry->next;
  else
    if (entry->next == cache->next)
      {
        /* insertion window starts right behind the entry to remove
         */
        if (entry->previous == NO_INDEX)
          {
            /* remove the first entry -> insertion may start at pos 0, now */
            cache->current_data = 0;
          }
        else
          {
            /* insertion may start right behind the previous entry */
            entry_t *previous = get_entry(cache, entry->previous);
            cache->current_data = ALIGN_VALUE(  previous->offset
                                              + previous->size);
          }
      }

  /* unlink it from the chain of used entries
   */
  if (entry->previous == NO_INDEX)
    cache->first = entry->next;
  else
    get_entry(cache, entry->previous)->next = entry->next;

  if (entry->next == NO_INDEX)
    cache->last = entry->previous;
  else
    get_entry(cache, entry->next)->previous = entry->previous;

  /* Move last entry into hole (if the removed one is not the last used).
   * We need to do this since all used entries are at the beginning of
   * the group's entries array.
   */
  if (idx < last_in_group)
    {
      /* copy the last used entry to the removed entry's index
       */
      *entry = group->entries[group->used-1];

      /* update foreign links to new index
       */
      if (last_in_group == cache->next)
        cache->next = idx;

      if (entry->previous == NO_INDEX)
        cache->first = idx;
      else
        get_entry(cache, entry->previous)->next = idx;

      if (entry->next == NO_INDEX)
        cache->last = idx;
      else
        get_entry(cache, entry->next)->previous = idx;
    }

  /* Update the number of used entries.
   */
  group->used--;
}

/* Insert ENTRY into the chain of used dictionary entries. The entry's
 * offset and size members must already have been initialized. Also,
 * the offset must match the beginning of the insertion window.
 */
static void
insert_entry(svn_membuffer_t *cache, entry_t *entry)
{
  /* the group that ENTRY belongs to plus a number of useful index values
   */
  apr_uint32_t idx = get_index(cache, entry);
  apr_uint32_t group_index = idx / GROUP_SIZE;
  entry_group_t *group = &cache->directory[group_index];
  entry_t *next = cache->next == NO_INDEX
                ? NULL
                : get_entry(cache, cache->next);

  /* The entry must start at the beginning of the insertion window.
   * It must also be the first unused entry in the group.
   */
  assert(entry->offset == cache->current_data);
  assert(idx == group_index * GROUP_SIZE + group->used);
  cache->current_data = ALIGN_VALUE(entry->offset + entry->size);

  /* update usage counters
   */
  cache->used_entries++;
  cache->data_used += entry->size;
  entry->hit_count = 0;
  group->used++;

  /* update entry chain
   */
  entry->next = cache->next;
  if (cache->first == NO_INDEX)
    {
      /* insert as the first entry and only in the chain
       */
      entry->previous = NO_INDEX;
      cache->last = idx;
      cache->first = idx;
    }
  else if (next == NULL)
    {
      /* insert as the last entry in the chain.
       * Note that it cannot also be at the beginning of the chain.
       */
      entry->previous = cache->last;
      get_entry(cache, cache->last)->next = idx;
      cache->last = idx;
    }
  else
    {
      /* insert either at the start of a non-empty list or
       * somewhere in the middle
       */
      entry->previous = next->previous;
      next->previous = idx;

      if (entry->previous != NO_INDEX)
        get_entry(cache, entry->previous)->next = idx;
      else
        cache->first = idx;
    }

  /* The current insertion position must never point outside our
   * data buffer.
   */
  assert(cache->current_data <= cache->data_size);
}

/* Map a KEY of 16 bytes to the CACHE and group that shall contain the
 * respective item.
 */
static apr_uint32_t
get_group_index(svn_membuffer_t **cache,
                entry_key_t key)
{
  svn_membuffer_t *segment0 = *cache;

  /* select the cache segment to use. they have all the same group_count */
  *cache = &segment0[key[0] & (segment0->segment_count -1)];
  return key[1] % segment0->group_count;
}

/* Reduce the hit count of ENTRY and update the accumulated hit info
 * in CACHE accordingly.
 */
static APR_INLINE void
let_entry_age(svn_membuffer_t *cache, entry_t *entry)
{
  apr_uint32_t hits_removed = (entry->hit_count + 1) >> 1;

  cache->hit_count -= hits_removed;
  entry->hit_count -= hits_removed;
}

/* Returns 0 if the entry group identified by GROUP_INDEX in CACHE has not
 * been initialized, yet. In that case, this group can not data. Otherwise,
 * a non-zero value is returned.
 */
static APR_INLINE unsigned char
is_group_initialized(svn_membuffer_t *cache, apr_uint32_t group_index)
{
  unsigned char flags
    = cache->group_initialized[group_index / (8 * GROUP_INIT_GRANULARITY)];
  unsigned char bit_mask
    = (unsigned char)(1 << ((group_index / GROUP_INIT_GRANULARITY) % 8));

  return flags & bit_mask;
}

/* Initializes the section of the directory in CACHE that contains
 * the entry group identified by GROUP_INDEX. */
static void
initialize_group(svn_membuffer_t *cache, apr_uint32_t group_index)
{
  unsigned char bit_mask;
  apr_uint32_t i;

  /* range of groups to initialize due to GROUP_INIT_GRANULARITY */
  apr_uint32_t first_index =
      (group_index / GROUP_INIT_GRANULARITY) * GROUP_INIT_GRANULARITY;
  apr_uint32_t last_index = first_index + GROUP_INIT_GRANULARITY;
  if (last_index > cache->group_count)
    last_index = cache->group_count;

  for (i = first_index; i < last_index; ++i)
    cache->directory[i].used = 0;

  /* set the "initialized" bit for these groups */
  bit_mask
    = (unsigned char)(1 << ((group_index / GROUP_INIT_GRANULARITY) % 8));
  cache->group_initialized[group_index / (8 * GROUP_INIT_GRANULARITY)]
    |= bit_mask;
}

/* Given the GROUP_INDEX that shall contain an entry with the hash key
 * TO_FIND, find that entry in the specified group.
 *
 * If FIND_EMPTY is not set, this function will return the one used entry
 * that actually matches the hash or NULL, if no such entry exists.
 *
 * If FIND_EMPTY has been set, this function will drop the one used entry
 * that actually matches the hash (i.e. make it fit to be replaced with
 * new content), an unused entry or a forcibly removed entry (if all
 * group entries are currently in use). The entries' hash value will be
 * initialized with TO_FIND.
 */
static entry_t *
find_entry(svn_membuffer_t *cache,
           apr_uint32_t group_index,
           const apr_uint64_t to_find[2],
           svn_boolean_t find_empty)
{
  entry_group_t *group;
  entry_t *entry = NULL;
  apr_size_t i;

  /* get the group that *must* contain the entry
   */
  group = &cache->directory[group_index];

  /* If the entry group has not been initialized, yet, there is no data.
   */
  if (! is_group_initialized(cache, group_index))
    {
      if (find_empty)
        {
          initialize_group(cache, group_index);
          entry = &group->entries[0];

          /* initialize entry for the new key */
          entry->key[0] = to_find[0];
          entry->key[1] = to_find[1];
        }

      return entry;
    }

  /* try to find the matching entry
   */
  for (i = 0; i < group->used; ++i)
    if (   to_find[0] == group->entries[i].key[0]
        && to_find[1] == group->entries[i].key[1])
      {
        /* found it
         */
        entry = &group->entries[i];
        if (find_empty)
          drop_entry(cache, entry);
        else
          return entry;
      }

  /* None found. Are we looking for a free entry?
   */
  if (find_empty)
    {
      /* if there is no empty entry, delete the oldest entry
       */
      if (group->used == GROUP_SIZE)
        {
          /* every entry gets the same chance of being removed.
           * Otherwise, we free the first entry, fill it and
           * remove it again on the next occasion without considering
           * the other entries in this group.
           */
          entry = &group->entries[rand() % GROUP_SIZE];
          for (i = 1; i < GROUP_SIZE; ++i)
            if (entry->hit_count > group->entries[i].hit_count)
              entry = &group->entries[i];

          /* for the entries that don't have been removed,
           * reduce their hit counts to put them at a relative
           * disadvantage the next time.
           */
          for (i = 0; i < GROUP_SIZE; ++i)
            if (entry != &group->entries[i])
              let_entry_age(cache, entry);

          drop_entry(cache, entry);
        }

      /* initialize entry for the new key
       */
      entry = &group->entries[group->used];
      entry->key[0] = to_find[0];
      entry->key[1] = to_find[1];
    }

  return entry;
}

/* Move a surviving ENTRY from just behind the insertion window to
 * its beginning and move the insertion window up accordingly.
 */
static void
move_entry(svn_membuffer_t *cache, entry_t *entry)
{
  apr_size_t size = ALIGN_VALUE(entry->size);

  /* This entry survived this cleansing run. Reset half of its
   * hit count so that its removal gets more likely in the next
   * run unless someone read / hit this entry in the meantime.
   */
  let_entry_age(cache, entry);

  /* Move the entry to the start of the empty / insertion section
   * (if it isn't there already). Size-aligned moves are legal
   * since all offsets and block sizes share this same alignment.
   * Size-aligned moves tend to be faster than non-aligned ones
   * because no "odd" bytes at the end need to special treatment.
   */
  if (entry->offset != cache->current_data)
    {
      memmove(cache->data + cache->current_data,
              cache->data + entry->offset,
              size);
      entry->offset = cache->current_data;
    }

  /* The insertion position is now directly behind this entry.
   */
  cache->current_data = entry->offset + size;
  cache->next = entry->next;

  /* The current insertion position must never point outside our
   * data buffer.
   */
  assert(cache->current_data <= cache->data_size);
}

/* If necessary, enlarge the insertion window until it is at least
 * SIZE bytes long. SIZE must not exceed the data buffer size.
 * Return TRUE if enough room could be found or made. A FALSE result
 * indicates that the respective item shall not be added.
 */
static svn_boolean_t
ensure_data_insertable(svn_membuffer_t *cache, apr_size_t size)
{
  entry_t *entry;
  apr_uint64_t average_hit_value;
  apr_uint64_t threshold;

  /* accumulated size of the entries that have been removed to make
   * room for the new one.
   */
  apr_size_t drop_size = 0;

  /* This loop will eventually terminate because every cache entry
   * would get dropped eventually:
   * - hit counts become 0 after the got kept for 32 full scans
   * - larger elements get dropped as soon as their hit count is 0
   * - smaller and smaller elements get removed as the average
   *   entry size drops (average drops by a factor of 8 per scan)
   * - after no more than 43 full scans, all elements would be removed
   *
   * Since size is < 4th of the cache size and about 50% of all
   * entries get removed by a scan, it is very unlikely that more
   * than a fractional scan will be necessary.
   */
  while (1)
    {
      /* first offset behind the insertion window
       */
      apr_uint64_t end = cache->next == NO_INDEX
                       ? cache->data_size
                       : get_entry(cache, cache->next)->offset;

      /* leave function as soon as the insertion window is large enough
       */
      if (end >= size + cache->current_data)
        return TRUE;

      /* Don't be too eager to cache data. Smaller items will fit into
       * the cache after dropping a single item. Of the larger ones, we
       * will only accept about 50%. They are also likely to get evicted
       * soon due to their notoriously low hit counts.
       *
       * As long as enough similarly or even larger sized entries already
       * exist in the cache, much less insert requests will be rejected.
       */
      if (2 * drop_size > size)
        return FALSE;

      /* try to enlarge the insertion window
       */
      if (cache->next == NO_INDEX)
        {
          /* We reached the end of the data buffer; restart at the beginning.
           * Due to the randomized nature of our LFU implementation, very
           * large data items may require multiple passes. Therefore, SIZE
           * should be restricted to significantly less than data_size.
           */
          cache->current_data = 0;
          cache->next = cache->first;
        }
      else
        {
          entry = get_entry(cache, cache->next);

          /* Keep entries that are very small. Those are likely to be data
           * headers or similar management structures. So, they are probably
           * important while not occupying much space.
           * But keep them only as long as they are a minority.
           */
          if (   (apr_uint64_t)entry->size * cache->used_entries
               < cache->data_used / 8)
            {
              move_entry(cache, entry);
            }
          else
            {
              svn_boolean_t keep;

              if (cache->hit_count > cache->used_entries)
                {
                  /* Roll the dice and determine a threshold somewhere from 0 up
                   * to 2 times the average hit count.
                   */
                  average_hit_value = cache->hit_count / cache->used_entries;
                  threshold = (average_hit_value+1) * (rand() % 4096) / 2048;

                  keep = entry->hit_count >= threshold;
                }
              else
                {
                  /* general hit count is low. Keep everything that got hit
                   * at all and assign some 50% survival chance to everything
                   * else.
                   */
                  keep = (entry->hit_count > 0) || (rand() & 1);
                }

              /* keepers or destroyers? */
              if (keep)
                {
                  move_entry(cache, entry);
                }
              else
                {
                 /* Drop the entry from the end of the insertion window, if it
                  * has been hit less than the threshold. Otherwise, keep it and
                  * move the insertion window one entry further.
                  */
                  drop_size += entry->size;
                  drop_entry(cache, entry);
                }
            }
        }
    }

  /* This will never be reached. But if it was, "can't insert" was the
   * right answer. */
}

/* Mimic apr_pcalloc in APR_POOL_DEBUG mode, i.e. handle failed allocations
 * (e.g. OOM) properly: Allocate at least SIZE bytes from POOL and zero
 * the content of the allocated memory if ZERO has been set. Return NULL
 * upon failed allocations.
 *
 * Also, satisfy our buffer alignment needs for performance reasons.
 */
static void* secure_aligned_alloc(apr_pool_t *pool,
                                  apr_size_t size,
                                  svn_boolean_t zero)
{
  void* memory = apr_palloc(pool, size + ITEM_ALIGNMENT);
  if (memory != NULL)
    {
      memory = ALIGN_POINTER(memory);
      if (zero)
        memset(memory, 0, size);
    }

  return memory;
}

svn_error_t *
svn_cache__membuffer_cache_create(svn_membuffer_t **cache,
                                  apr_size_t total_size,
                                  apr_size_t directory_size,
                                  apr_size_t segment_count,
                                  svn_boolean_t thread_safe,
                                  svn_boolean_t allow_blocking_writes,
                                  apr_pool_t *pool)
{
  svn_membuffer_t *c;

  apr_uint32_t seg;
  apr_uint32_t group_count;
  apr_uint32_t group_init_size;
  apr_uint64_t data_size;
  apr_uint64_t max_entry_size;

  /* Limit the total size (only relevant if we can address > 4GB)
   */
#if APR_SIZEOF_VOIDP > 4
  if (total_size > MAX_SEGMENT_SIZE * MAX_SEGMENT_COUNT)
    total_size = MAX_SEGMENT_SIZE * MAX_SEGMENT_COUNT;
#endif

  /* Limit the segment count
   */
  if (segment_count > MAX_SEGMENT_COUNT)
    segment_count = MAX_SEGMENT_COUNT;
  if (segment_count * MIN_SEGMENT_SIZE > total_size)
    segment_count = total_size / MIN_SEGMENT_SIZE;

  /* The segment count must be a power of two. Round it down as necessary.
   */
  while ((segment_count & (segment_count-1)) != 0)
    segment_count &= segment_count-1;

  /* if the caller hasn't provided a reasonable segment count or the above
   * limitations set it to 0, derive one from the absolute cache size
   */
  if (segment_count < 1)
    {
      /* Determine a reasonable number of cache segments. Segmentation is
       * only useful for multi-threaded / multi-core servers as it reduces
       * lock contention on these systems.
       *
       * But on these systems, we can assume that ample memory has been
       * allocated to this cache. Smaller caches should not be segmented
       * as this severely limits the maximum size of cachable items.
       *
       * Segments should not be smaller than 32MB and max. cachable item
       * size should grow as fast as segmentation.
       */

      apr_uint32_t segment_count_shift = 0;
      while (((2 * DEFAULT_MIN_SEGMENT_SIZE) << (2 * segment_count_shift))
             < total_size)
        ++segment_count_shift;

      segment_count = (apr_size_t)1 << segment_count_shift;
    }

  /* If we have an extremely large cache (>512 GB), the default segment
   * size may exceed the amount allocatable as one chunk. In that case,
   * increase segmentation until we are under the threshold.
   */
  while (   total_size / segment_count > MAX_SEGMENT_SIZE
         && segment_count < MAX_SEGMENT_COUNT)
    segment_count *= 2;

  /* allocate cache as an array of segments / cache objects */
  c = apr_palloc(pool, segment_count * sizeof(*c));

  /* Split total cache size into segments of equal size
   */
  total_size /= segment_count;
  directory_size /= segment_count;

  /* prevent pathological conditions: ensure a certain minimum cache size
   */
  if (total_size < 2 * sizeof(entry_group_t))
    total_size = 2 * sizeof(entry_group_t);

  /* adapt the dictionary size accordingly, if necessary:
   * It must hold at least one group and must not exceed the cache size.
   */
  if (directory_size > total_size - sizeof(entry_group_t))
    directory_size = total_size - sizeof(entry_group_t);
  if (directory_size < sizeof(entry_group_t))
    directory_size = sizeof(entry_group_t);

  /* limit the data size to what we can address.
   * Note that this cannot overflow since all values are of size_t.
   * Also, make it a multiple of the item placement granularity to
   * prevent subtle overflows.
   */
  data_size = ALIGN_VALUE(total_size - directory_size + 1) - ITEM_ALIGNMENT;

  /* For cache sizes > 4TB, individual cache segments will be larger
   * than 16GB allowing for >4GB entries.  But caching chunks larger
   * than 4GB is simply not supported.
   */
  max_entry_size = data_size / 4 > MAX_ITEM_SIZE
                 ? MAX_ITEM_SIZE
                 : data_size / 4;

  /* to keep the entries small, we use 32 bit indexes only
   * -> we need to ensure that no more then 4G entries exist.
   *
   * Note, that this limit could only be exceeded in a very
   * theoretical setup with about 1EB of cache.
   */
  group_count = directory_size / sizeof(entry_group_t)
                    >= (APR_UINT32_MAX / GROUP_SIZE)
              ? (APR_UINT32_MAX / GROUP_SIZE) - 1
              : (apr_uint32_t)(directory_size / sizeof(entry_group_t));

  group_init_size = 1 + group_count / (8 * GROUP_INIT_GRANULARITY);
  for (seg = 0; seg < segment_count; ++seg)
    {
      /* allocate buffers and initialize cache members
       */
      c[seg].segment_count = (apr_uint32_t)segment_count;

      c[seg].group_count = group_count;
      c[seg].directory = apr_pcalloc(pool,
                                     group_count * sizeof(entry_group_t));

      /* Allocate and initialize directory entries as "not initialized",
         hence "unused" */
      c[seg].group_initialized = apr_pcalloc(pool, group_init_size);

      c[seg].first = NO_INDEX;
      c[seg].last = NO_INDEX;
      c[seg].next = NO_INDEX;

      c[seg].data_size = data_size;
      c[seg].data = secure_aligned_alloc(pool, (apr_size_t)data_size, FALSE);
      c[seg].current_data = 0;
      c[seg].data_used = 0;
      c[seg].max_entry_size = max_entry_size;

      c[seg].used_entries = 0;
      c[seg].hit_count = 0;
      c[seg].total_reads = 0;
      c[seg].total_writes = 0;
      c[seg].total_hits = 0;

      /* were allocations successful?
       * If not, initialize a minimal cache structure.
       */
      if (c[seg].data == NULL || c[seg].directory == NULL)
        {
          /* We are OOM. There is no need to proceed with "half a cache".
           */
          return svn_error_wrap_apr(APR_ENOMEM, "OOM");
        }

#if APR_HAS_THREADS
      /* A lock for intra-process synchronization to the cache, or NULL if
       * the cache's creator doesn't feel the cache needs to be
       * thread-safe.
       */
#  if USE_SIMPLE_MUTEX

      SVN_ERR(svn_mutex__init(&c[seg].lock, thread_safe, pool));

#  else

      c[seg].lock = NULL;
      if (thread_safe)
        {
          apr_status_t status =
              apr_thread_rwlock_create(&(c[seg].lock), pool);
          if (status)
            return svn_error_wrap_apr(status, _("Can't create cache mutex"));
        }

#  endif

      /* Select the behavior of write operations.
       */
      c[seg].allow_blocking_writes = allow_blocking_writes;
#endif
    }

  /* done here
   */
  *cache = c;
  return SVN_NO_ERROR;
}

/* Look for the cache entry in group GROUP_INDEX of CACHE, identified
 * by the hash value TO_FIND and set *FOUND accordingly.
 *
 * Note: This function requires the caller to serialize access.
 * Don't call it directly, call entry_exists instead.
 */
static svn_error_t *
entry_exists_internal(svn_membuffer_t *cache,
                      apr_uint32_t group_index,
                      entry_key_t to_find,
                      svn_boolean_t *found)
{
  *found = find_entry(cache, group_index, to_find, FALSE) != NULL;
  return SVN_NO_ERROR;
}

/* Look for the cache entry in group GROUP_INDEX of CACHE, identified
 * by the hash value TO_FIND and set *FOUND accordingly.
 */
static svn_error_t *
entry_exists(svn_membuffer_t *cache,
             apr_uint32_t group_index,
             entry_key_t to_find,
             svn_boolean_t *found)
{
  WITH_READ_LOCK(cache,
                 entry_exists_internal(cache,
                                       group_index,
                                       to_find,
                                       found));

  return SVN_NO_ERROR;
}


/* Try to insert the serialized item given in BUFFER with SIZE into
 * the group GROUP_INDEX of CACHE and uniquely identify it by hash
 * value TO_FIND.
 *
 * However, there is no guarantee that it will actually be put into
 * the cache. If there is already some data associated with TO_FIND,
 * it will be removed from the cache even if the new data cannot
 * be inserted.
 *
 * Note: This function requires the caller to serialization access.
 * Don't call it directly, call membuffer_cache_get_partial instead.
 */
static svn_error_t *
membuffer_cache_set_internal(svn_membuffer_t *cache,
                             entry_key_t to_find,
                             apr_uint32_t group_index,
                             char *buffer,
                             apr_size_t size,
                             DEBUG_CACHE_MEMBUFFER_TAG_ARG
                             apr_pool_t *scratch_pool)
{
  /* first, look for a previous entry for the given key */
  entry_t *entry = find_entry(cache, group_index, to_find, FALSE);

  /* if there is an old version of that entry and the new data fits into
   * the old spot, just re-use that space. */
  if (entry && ALIGN_VALUE(entry->size) >= size && buffer)
    {
      /* Careful! We need to cast SIZE to the full width of CACHE->DATA_USED
       * lest we run into trouble with 32 bit underflow *not* treated as a
       * negative value.
       */
      cache->data_used += (apr_uint64_t)size - entry->size;
      entry->size = size;

#ifdef SVN_DEBUG_CACHE_MEMBUFFER

      /* Remember original content, type and key (hashes)
       */
      SVN_ERR(store_content_part(tag, buffer, size, scratch_pool));
      memcpy(&entry->tag, tag, sizeof(*tag));

#endif

      if (size)
        memcpy(cache->data + entry->offset, buffer, size);

      cache->total_writes++;
      return SVN_NO_ERROR;
    }

  /* if necessary, enlarge the insertion window.
   */
  if (   buffer != NULL
      && cache->max_entry_size >= size
      && ensure_data_insertable(cache, size))
    {
      /* Remove old data for this key, if that exists.
       * Get an unused entry for the key and and initialize it with
       * the serialized item's (future) position within data buffer.
       */
      entry = find_entry(cache, group_index, to_find, TRUE);
      entry->size = size;
      entry->offset = cache->current_data;

#ifdef SVN_DEBUG_CACHE_MEMBUFFER

      /* Remember original content, type and key (hashes)
       */
      SVN_ERR(store_content_part(tag, buffer, size, scratch_pool));
      memcpy(&entry->tag, tag, sizeof(*tag));

#endif

      /* Link the entry properly.
       */
      insert_entry(cache, entry);

      /* Copy the serialized item data into the cache.
       */
      if (size)
        memcpy(cache->data + entry->offset, buffer, size);

      cache->total_writes++;
    }
  else
    {
      /* if there is already an entry for this key, drop it.
       * Since ensure_data_insertable may have removed entries from
       * ENTRY's group, re-do the lookup.
       */
      entry = find_entry(cache, group_index, to_find, FALSE);
      if (entry)
        drop_entry(cache, entry);
    }

  return SVN_NO_ERROR;
}

/* Try to insert the ITEM and use the KEY to uniquely identify it.
 * However, there is no guarantee that it will actually be put into
 * the cache. If there is already some data associated to the KEY,
 * it will be removed from the cache even if the new data cannot
 * be inserted.
 *
 * The SERIALIZER is called to transform the ITEM into a single,
 * flat data buffer. Temporary allocations may be done in POOL.
 */
static svn_error_t *
membuffer_cache_set(svn_membuffer_t *cache,
                    entry_key_t key,
                    void *item,
                    svn_cache__serialize_func_t serializer,
                    DEBUG_CACHE_MEMBUFFER_TAG_ARG
                    apr_pool_t *scratch_pool)
{
  apr_uint32_t group_index;
  void *buffer = NULL;
  apr_size_t size = 0;

  /* find the entry group that will hold the key.
   */
  group_index = get_group_index(&cache, key);

  /* Serialize data data.
   */
  if (item)
    SVN_ERR(serializer(&buffer, &size, item, scratch_pool));

  /* The actual cache data access needs to sync'ed
   */
  WITH_WRITE_LOCK(cache,
                  membuffer_cache_set_internal(cache,
                                               key,
                                               group_index,
                                               buffer,
                                               size,
                                               DEBUG_CACHE_MEMBUFFER_TAG
                                               scratch_pool));
  return SVN_NO_ERROR;
}

/* Look for the cache entry in group GROUP_INDEX of CACHE, identified
 * by the hash value TO_FIND. If no item has been stored for KEY,
 * *BUFFER will be NULL. Otherwise, return a copy of the serialized
 * data in *BUFFER and return its size in *ITEM_SIZE. Allocations will
 * be done in POOL.
 *
 * Note: This function requires the caller to serialization access.
 * Don't call it directly, call membuffer_cache_get_partial instead.
 */
static svn_error_t *
membuffer_cache_get_internal(svn_membuffer_t *cache,
                             apr_uint32_t group_index,
                             entry_key_t to_find,
                             char **buffer,
                             apr_size_t *item_size,
                             DEBUG_CACHE_MEMBUFFER_TAG_ARG
                             apr_pool_t *result_pool)
{
  entry_t *entry;
  apr_size_t size;

  /* The actual cache data access needs to sync'ed
   */
  entry = find_entry(cache, group_index, to_find, FALSE);
  cache->total_reads++;
  if (entry == NULL)
    {
      /* no such entry found.
       */
      *buffer = NULL;
      *item_size = 0;

      return SVN_NO_ERROR;
    }

  size = ALIGN_VALUE(entry->size);
  *buffer = ALIGN_POINTER(apr_palloc(result_pool, size + ITEM_ALIGNMENT-1));
  memcpy(*buffer, (const char*)cache->data + entry->offset, size);

#ifdef SVN_DEBUG_CACHE_MEMBUFFER

  /* Check for overlapping entries.
   */
  SVN_ERR_ASSERT(entry->next == NO_INDEX ||
                 entry->offset + size
                    <= get_entry(cache, entry->next)->offset);

  /* Compare original content, type and key (hashes)
   */
  SVN_ERR(store_content_part(tag, *buffer, entry->size, result_pool));
  SVN_ERR(assert_equal_tags(&entry->tag, tag));

#endif

  /* update hit statistics
   */
  entry->hit_count++;
  cache->hit_count++;
  cache->total_hits++;

  *item_size = entry->size;

  return SVN_NO_ERROR;
}

/* Look for the *ITEM identified by KEY. If no item has been stored
 * for KEY, *ITEM will be NULL. Otherwise, the DESERIALIZER is called
 * re-construct the proper object from the serialized data.
 * Allocations will be done in POOL.
 */
static svn_error_t *
membuffer_cache_get(svn_membuffer_t *cache,
                    entry_key_t key,
                    void **item,
                    svn_cache__deserialize_func_t deserializer,
                    DEBUG_CACHE_MEMBUFFER_TAG_ARG
                    apr_pool_t *result_pool)
{
  apr_uint32_t group_index;
  char *buffer;
  apr_size_t size;

  /* find the entry group that will hold the key.
   */
  group_index = get_group_index(&cache, key);
  WITH_READ_LOCK(cache,
                 membuffer_cache_get_internal(cache,
                                              group_index,
                                              key,
                                              &buffer,
                                              &size,
                                              DEBUG_CACHE_MEMBUFFER_TAG
                                              result_pool));

  /* re-construct the original data object from its serialized form.
   */
  if (buffer == NULL)
    {
      *item = NULL;
      return SVN_NO_ERROR;
    }

  return deserializer(item, buffer, size, result_pool);
}

/* Look for the cache entry in group GROUP_INDEX of CACHE, identified
 * by the hash value TO_FIND. FOUND indicates whether that entry exists.
 * If not found, *ITEM will be NULL.
 *
 * Otherwise, the DESERIALIZER is called with that entry and the BATON
 * provided and will extract the desired information. The result is set
 * in *ITEM. Allocations will be done in POOL.
 *
 * Note: This function requires the caller to serialization access.
 * Don't call it directly, call membuffer_cache_get_partial instead.
 */
static svn_error_t *
membuffer_cache_get_partial_internal(svn_membuffer_t *cache,
                                     apr_uint32_t group_index,
                                     entry_key_t to_find,
                                     void **item,
                                     svn_boolean_t *found,
                                     svn_cache__partial_getter_func_t deserializer,
                                     void *baton,
                                     DEBUG_CACHE_MEMBUFFER_TAG_ARG
                                     apr_pool_t *result_pool)
{
  entry_t *entry = find_entry(cache, group_index, to_find, FALSE);
  cache->total_reads++;
  if (entry == NULL)
    {
      *item = NULL;
      *found = FALSE;

      return SVN_NO_ERROR;
    }
  else
    {
      *found = TRUE;

      entry->hit_count++;
      cache->hit_count++;
      cache->total_hits++;

#ifdef SVN_DEBUG_CACHE_MEMBUFFER

      /* Check for overlapping entries.
       */
      SVN_ERR_ASSERT(entry->next == NO_INDEX ||
                     entry->offset + entry->size
                        <= get_entry(cache, entry->next)->offset);

      /* Compare original content, type and key (hashes)
       */
      SVN_ERR(store_content_part(tag,
                                 (const char*)cache->data + entry->offset,
                                 entry->size,
                                 result_pool));
      SVN_ERR(assert_equal_tags(&entry->tag, tag));

#endif

      return deserializer(item,
                          (const char*)cache->data + entry->offset,
                          entry->size,
                          baton,
                          result_pool);
    }
}

/* Look for the cache entry identified by KEY. FOUND indicates
 * whether that entry exists. If not found, *ITEM will be NULL. Otherwise,
 * the DESERIALIZER is called with that entry and the BATON provided
 * and will extract the desired information. The result is set in *ITEM.
 * Allocations will be done in POOL.
 */
static svn_error_t *
membuffer_cache_get_partial(svn_membuffer_t *cache,
                            entry_key_t key,
                            void **item,
                            svn_boolean_t *found,
                            svn_cache__partial_getter_func_t deserializer,
                            void *baton,
                            DEBUG_CACHE_MEMBUFFER_TAG_ARG
                            apr_pool_t *result_pool)
{
  apr_uint32_t group_index = get_group_index(&cache, key);

  WITH_READ_LOCK(cache,
                 membuffer_cache_get_partial_internal
                     (cache, group_index, key, item, found,
                      deserializer, baton, DEBUG_CACHE_MEMBUFFER_TAG
                      result_pool));

  return SVN_NO_ERROR;
}

/* Look for the cache entry in group GROUP_INDEX of CACHE, identified
 * by the hash value TO_FIND. If no entry has been found, the function
 * returns without modifying the cache.
 *
 * Otherwise, FUNC is called with that entry and the BATON provided
 * and may modify the cache entry. Allocations will be done in POOL.
 *
 * Note: This function requires the caller to serialization access.
 * Don't call it directly, call membuffer_cache_set_partial instead.
 */
static svn_error_t *
membuffer_cache_set_partial_internal(svn_membuffer_t *cache,
                                     apr_uint32_t group_index,
                                     entry_key_t to_find,
                                     svn_cache__partial_setter_func_t func,
                                     void *baton,
                                     DEBUG_CACHE_MEMBUFFER_TAG_ARG
                                     apr_pool_t *scratch_pool)
{
  /* cache item lookup
   */
  entry_t *entry = find_entry(cache, group_index, to_find, FALSE);
  cache->total_reads++;

  /* this function is a no-op if the item is not in cache
   */
  if (entry != NULL)
    {
      svn_error_t *err;

      /* access the serialized cache item */
      char *data = (char*)cache->data + entry->offset;
      char *orig_data = data;
      apr_size_t size = entry->size;

      entry->hit_count++;
      cache->hit_count++;
      cache->total_writes++;

#ifdef SVN_DEBUG_CACHE_MEMBUFFER

      /* Check for overlapping entries.
       */
      SVN_ERR_ASSERT(entry->next == NO_INDEX ||
                     entry->offset + size
                        <= get_entry(cache, entry->next)->offset);

      /* Compare original content, type and key (hashes)
       */
      SVN_ERR(store_content_part(tag, data, size, scratch_pool));
      SVN_ERR(assert_equal_tags(&entry->tag, tag));

#endif

      /* modify it, preferably in-situ.
       */
      err = func((void **)&data, &size, baton, scratch_pool);

      if (err)
        {
          /* Something somewhere when wrong while FUNC was modifying the
           * changed item. Thus, it might have become invalid /corrupted.
           * We better drop that.
           */
          drop_entry(cache, entry);
        }
      else
        {
          /* if the modification caused a re-allocation, we need to remove
           * the old entry and to copy the new data back into cache.
           */
          if (data != orig_data)
            {
              /* Remove the old entry and try to make space for the new one.
               */
              drop_entry(cache, entry);
              if (   (cache->max_entry_size >= size)
                  && ensure_data_insertable(cache, size))
                {
                  /* Write the new entry.
                   */
                  entry = find_entry(cache, group_index, to_find, TRUE);
                  entry->size = size;
                  entry->offset = cache->current_data;
                  if (size)
                    memcpy(cache->data + entry->offset, data, size);

                  /* Link the entry properly.
                   */
                  insert_entry(cache, entry);
                }
            }

#ifdef SVN_DEBUG_CACHE_MEMBUFFER

          /* Remember original content, type and key (hashes)
           */
          SVN_ERR(store_content_part(tag, data, size, scratch_pool));
          memcpy(&entry->tag, tag, sizeof(*tag));

#endif
        }
    }

  return SVN_NO_ERROR;
}

/* Look for the cache entry identified by KEY. If no entry
 * has been found, the function returns without modifying the cache.
 * Otherwise, FUNC is called with that entry and the BATON provided
 * and may modify the cache entry. Allocations will be done in POOL.
 */
static svn_error_t *
membuffer_cache_set_partial(svn_membuffer_t *cache,
                            entry_key_t key,
                            svn_cache__partial_setter_func_t func,
                            void *baton,
                            DEBUG_CACHE_MEMBUFFER_TAG_ARG
                            apr_pool_t *scratch_pool)
{
  /* cache item lookup
   */
  apr_uint32_t group_index = get_group_index(&cache, key);
  WITH_WRITE_LOCK(cache,
                  membuffer_cache_set_partial_internal
                     (cache, group_index, key, func, baton,
                      DEBUG_CACHE_MEMBUFFER_TAG
                      scratch_pool));

  /* done here -> unlock the cache
   */
  return SVN_NO_ERROR;
}

/* Implement the svn_cache__t interface on top of a shared membuffer cache.
 *
 * Because membuffer caches tend to be very large, there will be rather few
 * of them (usually only one). Thus, the same instance shall be used as the
 * backend to many application-visible svn_cache__t instances. This should
 * also achieve global resource usage fairness.
 *
 * To accommodate items from multiple resources, the individual keys must be
 * unique over all sources. This is achieved by simply adding a prefix key
 * that unambiguously identifies the item's context (e.g. path to the
 * respective repository). The prefix will be set upon construction of the
 * svn_cache__t instance.
 */

/* Internal cache structure (used in svn_cache__t.cache_internal) basically
 * holding the additional parameters needed to call the respective membuffer
 * functions.
 */
typedef struct svn_membuffer_cache_t
{
  /* this is where all our data will end up in
   */
  svn_membuffer_t *membuffer;

  /* use this conversion function when inserting an item into the memcache
   */
  svn_cache__serialize_func_t serializer;

  /* use this conversion function when reading an item from the memcache
   */
  svn_cache__deserialize_func_t deserializer;

  /* Prepend this byte sequence to any key passed to us.
   * This makes (very likely) our keys different from all keys used
   * by other svn_membuffer_cache_t instances.
   */
  entry_key_t prefix;

  /* A copy of the unmodified prefix. It is being used as a user-visible
   * ID for this cache instance.
   */
  const char* full_prefix;

  /* length of the keys that will be passed to us through the
   * svn_cache_t interface. May be APR_HASH_KEY_STRING.
   */
  apr_ssize_t key_len;

  /* Temporary buffer containing the hash key for the current access
   */
  entry_key_t combined_key;

  /* a pool for temporary allocations during get() and set()
   */
  apr_pool_t *pool;

  /* an internal counter that is used to clear the pool from time to time
   * but not too frequently.
   */
  int alloc_counter;

  /* if enabled, this will serialize the access to this instance.
   */
  svn_mutex__t *mutex;
#ifdef SVN_DEBUG_CACHE_MEMBUFFER

  /* Invariant tag info for all items stored by this cache instance.
   */
  char prefix_tail[PREFIX_TAIL_LEN];

#endif
} svn_membuffer_cache_t;

/* After an estimated ALLOCATIONS_PER_POOL_CLEAR allocations, we should
 * clear the svn_membuffer_cache_t.pool to keep memory consumption in check.
 */
#define ALLOCATIONS_PER_POOL_CLEAR 10


/* Basically calculate a hash value for KEY of length KEY_LEN, combine it
 * with the CACHE->PREFIX and write the result in CACHE->COMBINED_KEY.
 */
static void
combine_key(svn_membuffer_cache_t *cache,
            const void *key,
            apr_ssize_t key_len)
{
  if (key_len == APR_HASH_KEY_STRING)
    key_len = strlen((const char *) key);

  if (key_len < 16)
    {
      apr_uint32_t data[4] = { 0 };
      memcpy(data, key, key_len);

      svn__pseudo_md5_15((apr_uint32_t *)cache->combined_key, data);
    }
  else if (key_len < 32)
    {
      apr_uint32_t data[8] = { 0 };
      memcpy(data, key, key_len);

      svn__pseudo_md5_31((apr_uint32_t *)cache->combined_key, data);
    }
  else if (key_len < 64)
    {
      apr_uint32_t data[16] = { 0 };
      memcpy(data, key, key_len);

      svn__pseudo_md5_63((apr_uint32_t *)cache->combined_key, data);
    }
  else
    {
      apr_md5((unsigned char*)cache->combined_key, key, key_len);
    }

  cache->combined_key[0] ^= cache->prefix[0];
  cache->combined_key[1] ^= cache->prefix[1];
}

/* Implement svn_cache__vtable_t.get (not thread-safe)
 */
static svn_error_t *
svn_membuffer_cache_get(void **value_p,
                        svn_boolean_t *found,
                        void *cache_void,
                        const void *key,
                        apr_pool_t *result_pool)
{
  svn_membuffer_cache_t *cache = cache_void;

  DEBUG_CACHE_MEMBUFFER_INIT_TAG

  /* special case */
  if (key == NULL)
    {
      *value_p = NULL;
      *found = FALSE;

      return SVN_NO_ERROR;
    }

  /* construct the full, i.e. globally unique, key by adding
   * this cache instances' prefix
   */
  combine_key(cache, key, cache->key_len);

  /* Look the item up. */
  SVN_ERR(membuffer_cache_get(cache->membuffer,
                              cache->combined_key,
                              value_p,
                              cache->deserializer,
                              DEBUG_CACHE_MEMBUFFER_TAG
                              result_pool));

  /* return result */
  *found = *value_p != NULL;
  return SVN_NO_ERROR;
}

/* Implement svn_cache__vtable_t.set (not thread-safe)
 */
static svn_error_t *
svn_membuffer_cache_set(void *cache_void,
                        const void *key,
                        void *value,
                        apr_pool_t *scratch_pool)
{
  svn_membuffer_cache_t *cache = cache_void;

  DEBUG_CACHE_MEMBUFFER_INIT_TAG

  /* special case */
  if (key == NULL)
    return SVN_NO_ERROR;

  /* we do some allocations below, so increase the allocation counter
   * by a slightly larger amount. Free allocated memory every now and then.
   */
  cache->alloc_counter += 3;
  if (cache->alloc_counter > ALLOCATIONS_PER_POOL_CLEAR)
    {
      svn_pool_clear(cache->pool);
      cache->alloc_counter = 0;
    }

  /* construct the full, i.e. globally unique, key by adding
   * this cache instances' prefix
   */
  combine_key(cache, key, cache->key_len);

  /* (probably) add the item to the cache. But there is no real guarantee
   * that the item will actually be cached afterwards.
   */
  return membuffer_cache_set(cache->membuffer,
                             cache->combined_key,
                             value,
                             cache->serializer,
                             DEBUG_CACHE_MEMBUFFER_TAG
                             cache->pool);
}

/* Implement svn_cache__vtable_t.iter as "not implemented"
 */
static svn_error_t *
svn_membuffer_cache_iter(svn_boolean_t *completed,
                          void *cache_void,
                          svn_iter_apr_hash_cb_t user_cb,
                          void *user_baton,
                          apr_pool_t *scratch_pool)
{
  return svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                          _("Can't iterate a membuffer-based cache"));
}

/* Implement svn_cache__vtable_t.get_partial (not thread-safe)
 */
static svn_error_t *
svn_membuffer_cache_get_partial(void **value_p,
                                svn_boolean_t *found,
                                void *cache_void,
                                const void *key,
                                svn_cache__partial_getter_func_t func,
                                void *baton,
                                apr_pool_t *result_pool)
{
  svn_membuffer_cache_t *cache = cache_void;

  DEBUG_CACHE_MEMBUFFER_INIT_TAG

  if (key == NULL)
    {
      *value_p = NULL;
      *found = FALSE;

      return SVN_NO_ERROR;
    }

  combine_key(cache, key, cache->key_len);
  SVN_ERR(membuffer_cache_get_partial(cache->membuffer,
                                      cache->combined_key,
                                      value_p,
                                      found,
                                      func,
                                      baton,
                                      DEBUG_CACHE_MEMBUFFER_TAG
                                      result_pool));

  return SVN_NO_ERROR;
}

/* Implement svn_cache__vtable_t.set_partial (not thread-safe)
 */
static svn_error_t *
svn_membuffer_cache_set_partial(void *cache_void,
                                const void *key,
                                svn_cache__partial_setter_func_t func,
                                void *baton,
                                apr_pool_t *scratch_pool)
{
  svn_membuffer_cache_t *cache = cache_void;

  DEBUG_CACHE_MEMBUFFER_INIT_TAG

  if (key != NULL)
    {
      combine_key(cache, key, cache->key_len);
      SVN_ERR(membuffer_cache_set_partial(cache->membuffer,
                                          cache->combined_key,
                                          func,
                                          baton,
                                          DEBUG_CACHE_MEMBUFFER_TAG
                                          scratch_pool));
    }
  return SVN_NO_ERROR;
}

/* Implement svn_cache__vtable_t.is_cachable
 * (thread-safe even without mutex)
 */
static svn_boolean_t
svn_membuffer_cache_is_cachable(void *cache_void, apr_size_t size)
{
  /* Don't allow extremely large element sizes. Otherwise, the cache
   * might by thrashed by a few extremely large entries. And the size
   * must be small enough to be stored in a 32 bit value.
   */
  svn_membuffer_cache_t *cache = cache_void;
  return size <= cache->membuffer->max_entry_size;
}

/* Add statistics of SEGMENT to INFO.
 */
static svn_error_t *
svn_membuffer_get_segment_info(svn_membuffer_t *segment,
                               svn_cache__info_t *info)
{
  info->data_size += segment->data_size;
  info->used_size += segment->data_used;
  info->total_size += segment->data_size +
      segment->group_count * GROUP_SIZE * sizeof(entry_t);

  info->used_entries += segment->used_entries;
  info->total_entries += segment->group_count * GROUP_SIZE;

  return SVN_NO_ERROR;
}

/* Implement svn_cache__vtable_t.get_info
 * (thread-safe even without mutex)
 */
static svn_error_t *
svn_membuffer_cache_get_info(void *cache_void,
                             svn_cache__info_t *info,
                             svn_boolean_t reset,
                             apr_pool_t *result_pool)
{
  svn_membuffer_cache_t *cache = cache_void;
  apr_uint32_t i;

  /* cache front-end specific data */

  info->id = apr_pstrdup(result_pool, cache->full_prefix);

  /* collect info from shared cache back-end */

  info->data_size = 0;
  info->used_size = 0;
  info->total_size = 0;

  info->used_entries = 0;
  info->total_entries = 0;

  for (i = 0; i < cache->membuffer->segment_count; ++i)
    {
      svn_membuffer_t *segment = cache->membuffer + i;
      WITH_READ_LOCK(segment,
                     svn_membuffer_get_segment_info(segment, info));
    }

  return SVN_NO_ERROR;
}


/* the v-table for membuffer-based caches (single-threaded access)
 */
static svn_cache__vtable_t membuffer_cache_vtable = {
  svn_membuffer_cache_get,
  svn_membuffer_cache_set,
  svn_membuffer_cache_iter,
  svn_membuffer_cache_is_cachable,
  svn_membuffer_cache_get_partial,
  svn_membuffer_cache_set_partial,
  svn_membuffer_cache_get_info
};

/* Implement svn_cache__vtable_t.get and serialize all cache access.
 */
static svn_error_t *
svn_membuffer_cache_get_synced(void **value_p,
                               svn_boolean_t *found,
                               void *cache_void,
                               const void *key,
                               apr_pool_t *result_pool)
{
  svn_membuffer_cache_t *cache = cache_void;
  SVN_MUTEX__WITH_LOCK(cache->mutex,
                       svn_membuffer_cache_get(value_p,
                                               found,
                                               cache_void,
                                               key,
                                               result_pool));

  return SVN_NO_ERROR;
}

/* Implement svn_cache__vtable_t.set and serialize all cache access.
 */
static svn_error_t *
svn_membuffer_cache_set_synced(void *cache_void,
                               const void *key,
                               void *value,
                               apr_pool_t *scratch_pool)
{
  svn_membuffer_cache_t *cache = cache_void;
  SVN_MUTEX__WITH_LOCK(cache->mutex,
                       svn_membuffer_cache_set(cache_void,
                                               key,
                                               value,
                                               scratch_pool));

  return SVN_NO_ERROR;
}

/* Implement svn_cache__vtable_t.get_partial and serialize all cache access.
 */
static svn_error_t *
svn_membuffer_cache_get_partial_synced(void **value_p,
                                       svn_boolean_t *found,
                                       void *cache_void,
                                       const void *key,
                                       svn_cache__partial_getter_func_t func,
                                       void *baton,
                                       apr_pool_t *result_pool)
{
  svn_membuffer_cache_t *cache = cache_void;
  SVN_MUTEX__WITH_LOCK(cache->mutex,
                       svn_membuffer_cache_get_partial(value_p,
                                                       found,
                                                       cache_void,
                                                       key,
                                                       func,
                                                       baton,
                                                       result_pool));

  return SVN_NO_ERROR;
}

/* Implement svn_cache__vtable_t.set_partial and serialize all cache access.
 */
static svn_error_t *
svn_membuffer_cache_set_partial_synced(void *cache_void,
                                       const void *key,
                                       svn_cache__partial_setter_func_t func,
                                       void *baton,
                                       apr_pool_t *scratch_pool)
{
  svn_membuffer_cache_t *cache = cache_void;
  SVN_MUTEX__WITH_LOCK(cache->mutex,
                       svn_membuffer_cache_set_partial(cache_void,
                                                       key,
                                                       func,
                                                       baton,
                                                       scratch_pool));

  return SVN_NO_ERROR;
}

/* the v-table for membuffer-based caches with multi-threading support)
 */
static svn_cache__vtable_t membuffer_cache_synced_vtable = {
  svn_membuffer_cache_get_synced,
  svn_membuffer_cache_set_synced,
  svn_membuffer_cache_iter,               /* no sync required */
  svn_membuffer_cache_is_cachable,        /* no sync required */
  svn_membuffer_cache_get_partial_synced,
  svn_membuffer_cache_set_partial_synced,
  svn_membuffer_cache_get_info            /* no sync required */
};

/* standard serialization function for svn_stringbuf_t items.
 * Implements svn_cache__serialize_func_t.
 */
static svn_error_t *
serialize_svn_stringbuf(void **buffer,
                        apr_size_t *buffer_size,
                        void *item,
                        apr_pool_t *result_pool)
{
  svn_stringbuf_t *value_str = item;

  *buffer = value_str->data;
  *buffer_size = value_str->len + 1;

  return SVN_NO_ERROR;
}

/* standard de-serialization function for svn_stringbuf_t items.
 * Implements svn_cache__deserialize_func_t.
 */
static svn_error_t *
deserialize_svn_stringbuf(void **item,
                          void *buffer,
                          apr_size_t buffer_size,
                          apr_pool_t *result_pool)
{
  svn_stringbuf_t *value_str = apr_palloc(result_pool, sizeof(svn_stringbuf_t));

  value_str->pool = result_pool;
  value_str->blocksize = buffer_size;
  value_str->data = buffer;
  value_str->len = buffer_size-1;
  *item = value_str;

  return SVN_NO_ERROR;
}

/* Construct a svn_cache__t object on top of a shared memcache.
 */
svn_error_t *
svn_cache__create_membuffer_cache(svn_cache__t **cache_p,
                                  svn_membuffer_t *membuffer,
                                  svn_cache__serialize_func_t serializer,
                                  svn_cache__deserialize_func_t deserializer,
                                  apr_ssize_t klen,
                                  const char *prefix,
                                  svn_boolean_t thread_safe,
                                  apr_pool_t *pool)
{
  svn_checksum_t *checksum;

  /* allocate the cache header structures
   */
  svn_cache__t *wrapper = apr_pcalloc(pool, sizeof(*wrapper));
  svn_membuffer_cache_t *cache = apr_palloc(pool, sizeof(*cache));

  /* initialize our internal cache header
   */
  cache->membuffer = membuffer;
  cache->serializer = serializer
                    ? serializer
                    : serialize_svn_stringbuf;
  cache->deserializer = deserializer
                      ? deserializer
                      : deserialize_svn_stringbuf;
  cache->full_prefix = apr_pstrdup(pool, prefix);
  cache->key_len = klen;
  cache->pool = svn_pool_create(pool);
  cache->alloc_counter = 0;

  SVN_ERR(svn_mutex__init(&cache->mutex, thread_safe, pool));

  /* for performance reasons, we don't actually store the full prefix but a
   * hash value of it
   */
  SVN_ERR(svn_checksum(&checksum,
                       svn_checksum_md5,
                       prefix,
                       strlen(prefix),
                       pool));
  memcpy(cache->prefix, checksum->digest, sizeof(cache->prefix));

#ifdef SVN_DEBUG_CACHE_MEMBUFFER

  /* Initialize cache debugging support.
   */
  get_prefix_tail(prefix, cache->prefix_tail);

#endif

  /* initialize the generic cache wrapper
   */
  wrapper->vtable = thread_safe ? &membuffer_cache_synced_vtable
                                : &membuffer_cache_vtable;
  wrapper->cache_internal = cache;
  wrapper->error_handler = 0;
  wrapper->error_baton = 0;

  *cache_p = wrapper;
  return SVN_NO_ERROR;
}

