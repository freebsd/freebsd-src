/* fs.h : interface to Subversion filesystem, private to libsvn_fs
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

#ifndef SVN_LIBSVN_FS_FS_H
#define SVN_LIBSVN_FS_FS_H

#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_network_io.h>

#include "svn_fs.h"
#include "svn_config.h"
#include "private/svn_atomic.h"
#include "private/svn_cache.h"
#include "private/svn_fs_private.h"
#include "private/svn_sqlite.h"
#include "private/svn_mutex.h"
#include "private/svn_named_atomic.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*** The filesystem structure.  ***/

/* Following are defines that specify the textual elements of the
   native filesystem directories and revision files. */

/* Names of special files in the fs_fs filesystem. */
#define PATH_FORMAT           "format"           /* Contains format number */
#define PATH_UUID             "uuid"             /* Contains UUID */
#define PATH_CURRENT          "current"          /* Youngest revision */
#define PATH_LOCK_FILE        "write-lock"       /* Revision lock file */
#define PATH_REVS_DIR         "revs"             /* Directory of revisions */
#define PATH_REVPROPS_DIR     "revprops"         /* Directory of revprops */
#define PATH_TXNS_DIR         "transactions"     /* Directory of transactions */
#define PATH_NODE_ORIGINS_DIR "node-origins"     /* Lazy node-origin cache */
#define PATH_TXN_PROTOS_DIR   "txn-protorevs"    /* Directory of proto-revs */
#define PATH_TXN_CURRENT      "txn-current"      /* File with next txn key */
#define PATH_TXN_CURRENT_LOCK "txn-current-lock" /* Lock for txn-current */
#define PATH_LOCKS_DIR        "locks"            /* Directory of locks */
#define PATH_MIN_UNPACKED_REV "min-unpacked-rev" /* Oldest revision which
                                                    has not been packed. */
#define PATH_REVPROP_GENERATION "revprop-generation"
                                                 /* Current revprop generation*/
#define PATH_MANIFEST         "manifest"         /* Manifest file name */
#define PATH_PACKED           "pack"             /* Packed revision data file */
#define PATH_EXT_PACKED_SHARD ".pack"            /* Extension for packed
                                                    shards */
/* If you change this, look at tests/svn_test_fs.c(maybe_install_fsfs_conf) */
#define PATH_CONFIG           "fsfs.conf"        /* Configuration */

/* Names of special files and file extensions for transactions */
#define PATH_CHANGES       "changes"       /* Records changes made so far */
#define PATH_TXN_PROPS     "props"         /* Transaction properties */
#define PATH_NEXT_IDS      "next-ids"      /* Next temporary ID assignments */
#define PATH_PREFIX_NODE   "node."         /* Prefix for node filename */
#define PATH_EXT_TXN       ".txn"          /* Extension of txn dir */
#define PATH_EXT_CHILDREN  ".children"     /* Extension for dir contents */
#define PATH_EXT_PROPS     ".props"        /* Extension for node props */
#define PATH_EXT_REV       ".rev"          /* Extension of protorev file */
#define PATH_EXT_REV_LOCK  ".rev-lock"     /* Extension of protorev lock file */
/* Names of files in legacy FS formats */
#define PATH_REV           "rev"           /* Proto rev file */
#define PATH_REV_LOCK      "rev-lock"      /* Proto rev (write) lock file */

/* Names of sections and options in fsfs.conf. */
#define CONFIG_SECTION_CACHES            "caches"
#define CONFIG_OPTION_FAIL_STOP          "fail-stop"
#define CONFIG_SECTION_REP_SHARING       "rep-sharing"
#define CONFIG_OPTION_ENABLE_REP_SHARING "enable-rep-sharing"
#define CONFIG_SECTION_DELTIFICATION     "deltification"
#define CONFIG_OPTION_ENABLE_DIR_DELTIFICATION   "enable-dir-deltification"
#define CONFIG_OPTION_ENABLE_PROPS_DELTIFICATION "enable-props-deltification"
#define CONFIG_OPTION_MAX_DELTIFICATION_WALK     "max-deltification-walk"
#define CONFIG_OPTION_MAX_LINEAR_DELTIFICATION   "max-linear-deltification"
#define CONFIG_SECTION_PACKED_REVPROPS   "packed-revprops"
#define CONFIG_OPTION_REVPROP_PACK_SIZE  "revprop-pack-size"
#define CONFIG_OPTION_COMPRESS_PACKED_REVPROPS  "compress-packed-revprops"

/* The format number of this filesystem.
   This is independent of the repository format number, and
   independent of any other FS back ends. */
#define SVN_FS_FS__FORMAT_NUMBER   6

/* The minimum format number that supports svndiff version 1.  */
#define SVN_FS_FS__MIN_SVNDIFF1_FORMAT 2

/* The minimum format number that supports transaction ID generation
   using a transaction sequence in the txn-current file. */
#define SVN_FS_FS__MIN_TXN_CURRENT_FORMAT 3

/* The minimum format number that supports the "layout" filesystem
   format option. */
#define SVN_FS_FS__MIN_LAYOUT_FORMAT_OPTION_FORMAT 3

/* The minimum format number that stores protorevs in a separate directory. */
#define SVN_FS_FS__MIN_PROTOREVS_DIR_FORMAT 3

/* The minimum format number that doesn't keep node and copy ID counters. */
#define SVN_FS_FS__MIN_NO_GLOBAL_IDS_FORMAT 3

/* The minimum format number that maintains minfo-here and minfo-count
   noderev fields. */
#define SVN_FS_FS__MIN_MERGEINFO_FORMAT 3

/* The minimum format number that allows rep sharing. */
#define SVN_FS_FS__MIN_REP_SHARING_FORMAT 4

/* The minimum format number that supports packed shards. */
#define SVN_FS_FS__MIN_PACKED_FORMAT 4

/* The minimum format number that stores node kinds in changed-paths lists. */
#define SVN_FS_FS__MIN_KIND_IN_CHANGED_FORMAT 4

/* 1.8 deltification options should work with any FSFS repo but to avoid
 * issues with very old servers, restrict those options to the 1.6+ format*/
#define SVN_FS_FS__MIN_DELTIFICATION_FORMAT 4

/* The 1.7-dev format, never released, that packed revprops into SQLite
   revprops.db . */
#define SVN_FS_FS__PACKED_REVPROP_SQLITE_DEV_FORMAT 5

/* The minimum format number that supports packed revprops. */
#define SVN_FS_FS__MIN_PACKED_REVPROP_FORMAT 6

/* The minimum format number that supports a configuration file (fsfs.conf) */
#define SVN_FS_FS__MIN_CONFIG_FILE 4

/* Private FSFS-specific data shared between all svn_txn_t objects that
   relate to a particular transaction in a filesystem (as identified
   by transaction id and filesystem UUID).  Objects of this type are
   allocated in their own subpool of the common pool. */
typedef struct fs_fs_shared_txn_data_t
{
  /* The next transaction in the list, or NULL if there is no following
     transaction. */
  struct fs_fs_shared_txn_data_t *next;

  /* This transaction's ID.  For repositories whose format is less
     than SVN_FS_FS__MIN_TXN_CURRENT_FORMAT, the ID is in the form
     <rev>-<uniqueifier>, where <uniqueifier> runs from 0-99999 (see
     create_txn_dir_pre_1_5() in fs_fs.c).  For newer repositories,
     the form is <rev>-<200 digit base 36 number> (see
     create_txn_dir() in fs_fs.c). */
  char txn_id[SVN_FS__TXN_MAX_LEN+1];

  /* Whether the transaction's prototype revision file is locked for
     writing by any thread in this process (including the current
     thread; recursive locks are not permitted).  This is effectively
     a non-recursive mutex. */
  svn_boolean_t being_written;

  /* The pool in which this object has been allocated; a subpool of the
     common pool. */
  apr_pool_t *pool;
} fs_fs_shared_txn_data_t;

/* On most operating systems apr implements file locks per process, not
   per file.  On Windows apr implements the locking as per file handle
   locks, so we don't have to add our own mutex for just in-process
   synchronization. */
/* Compare ../libsvn_subr/named_atomic.c:USE_THREAD_MUTEX */
#if APR_HAS_THREADS && !defined(WIN32)
#define SVN_FS_FS__USE_LOCK_MUTEX 1
#else
#define SVN_FS_FS__USE_LOCK_MUTEX 0
#endif

/* Private FSFS-specific data shared between all svn_fs_t objects that
   relate to a particular filesystem, as identified by filesystem UUID.
   Objects of this type are allocated in the common pool. */
typedef struct fs_fs_shared_data_t
{
  /* A list of shared transaction objects for each transaction that is
     currently active, or NULL if none are.  All access to this list,
     including the contents of the objects stored in it, is synchronised
     under TXN_LIST_LOCK. */
  fs_fs_shared_txn_data_t *txns;

  /* A free transaction object, or NULL if there is no free object.
     Access to this object is synchronised under TXN_LIST_LOCK. */
  fs_fs_shared_txn_data_t *free_txn;

  /* A lock for intra-process synchronization when accessing the TXNS list. */
  svn_mutex__t *txn_list_lock;

  /* A lock for intra-process synchronization when grabbing the
     repository write lock. */
  svn_mutex__t *fs_write_lock;

  /* A lock for intra-process synchronization when locking the
     txn-current file. */
  svn_mutex__t *txn_current_lock;

  /* The common pool, under which this object is allocated, subpools
     of which are used to allocate the transaction objects. */
  apr_pool_t *common_pool;
} fs_fs_shared_data_t;

/* Data structure for the 1st level DAG node cache. */
typedef struct fs_fs_dag_cache_t fs_fs_dag_cache_t;

/* Key type for all caches that use revision + offset / counter as key. */
typedef struct pair_cache_key_t
{
  svn_revnum_t revision;

  apr_int64_t second;
} pair_cache_key_t;

/* Private (non-shared) FSFS-specific data for each svn_fs_t object.
   Any caches in here may be NULL. */
typedef struct fs_fs_data_t
{
  /* The format number of this FS. */
  int format;
  /* The maximum number of files to store per directory (for sharded
     layouts) or zero (for linear layouts). */
  int max_files_per_dir;

  /* The revision that was youngest, last time we checked. */
  svn_revnum_t youngest_rev_cache;

  /* The fsfs.conf file, parsed.  Allocated in FS->pool. */
  svn_config_t *config;

  /* Caches of immutable data.  (Note that if these are created with
     svn_cache__create_memcache, the data can be shared between
     multiple svn_fs_t's for the same filesystem.) */

  /* A cache of revision root IDs, mapping from (svn_revnum_t *) to
     (svn_fs_id_t *).  (Not threadsafe.) */
  svn_cache__t *rev_root_id_cache;

  /* Caches native dag_node_t* instances and acts as a 1st level cache */
  fs_fs_dag_cache_t *dag_node_cache;

  /* DAG node cache for immutable nodes.  Maps (revision, fspath)
     to (dag_node_t *). This is the 2nd level cache for DAG nodes. */
  svn_cache__t *rev_node_cache;

  /* A cache of the contents of immutable directories; maps from
     unparsed FS ID to a apr_hash_t * mapping (const char *) dirent
     names to (svn_fs_dirent_t *). */
  svn_cache__t *dir_cache;

  /* Fulltext cache; currently only used with memcached.  Maps from
     rep key (revision/offset) to svn_string_t. */
  svn_cache__t *fulltext_cache;

  /* Access object to the atomics namespace used by revprop caching.
     Will be NULL until the first access. */
  svn_atomic_namespace__t *revprop_namespace;

  /* Access object to the revprop "generation". Will be NULL until
     the first access. */
  svn_named_atomic__t *revprop_generation;

  /* Access object to the revprop update timeout. Will be NULL until
     the first access. */
  svn_named_atomic__t *revprop_timeout;

  /* Revision property cache.  Maps from (rev,generation) to apr_hash_t. */
  svn_cache__t *revprop_cache;

  /* Node properties cache.  Maps from rep key to apr_hash_t. */
  svn_cache__t *properties_cache;

  /* Pack manifest cache; a cache mapping (svn_revnum_t) shard number to
     a manifest; and a manifest is a mapping from (svn_revnum_t) revision
     number offset within a shard to (apr_off_t) byte-offset in the
     respective pack file. */
  svn_cache__t *packed_offset_cache;

  /* Cache for txdelta_window_t objects; the key is (revFilePath, offset) */
  svn_cache__t *txdelta_window_cache;

  /* Cache for combined windows as svn_stringbuf_t objects;
     the key is (revFilePath, offset) */
  svn_cache__t *combined_window_cache;

  /* Cache for node_revision_t objects; the key is (revision, id offset) */
  svn_cache__t *node_revision_cache;

  /* Cache for change lists as APR arrays of change_t * objects; the key
     is the revision */
  svn_cache__t *changes_cache;

  /* Cache for svn_mergeinfo_t objects; the key is a combination of
     revision, inheritance flags and path. */
  svn_cache__t *mergeinfo_cache;

  /* Cache for presence of svn_mergeinfo_t on a noderev; the key is a
     combination of revision, inheritance flags and path; value is "1"
     if the node has mergeinfo, "0" if it doesn't. */
  svn_cache__t *mergeinfo_existence_cache;

  /* TRUE while the we hold a lock on the write lock file. */
  svn_boolean_t has_write_lock;

  /* If set, there are or have been more than one concurrent transaction */
  svn_boolean_t concurrent_transactions;

  /* Temporary cache for changed directories yet to be committed; maps from
     unparsed FS ID to ###x.  NULL outside transactions. */
  svn_cache__t *txn_dir_cache;

  /* Data shared between all svn_fs_t objects for a given filesystem. */
  fs_fs_shared_data_t *shared;

  /* The sqlite database used for rep caching. */
  svn_sqlite__db_t *rep_cache_db;

  /* Thread-safe boolean */
  svn_atomic_t rep_cache_db_opened;

  /* The oldest revision not in a pack file.  It also applies to revprops
   * if revprop packing has been enabled by the FSFS format version. */
  svn_revnum_t min_unpacked_rev;

  /* Whether rep-sharing is supported by the filesystem
   * and allowed by the configuration. */
  svn_boolean_t rep_sharing_allowed;

  /* File size limit in bytes up to which multiple revprops shall be packed
   * into a single file. */
  apr_int64_t revprop_pack_size;

  /* Whether packed revprop files shall be compressed. */
  svn_boolean_t compress_packed_revprops;

  /* Whether directory nodes shall be deltified just like file nodes. */
  svn_boolean_t deltify_directories;

  /* Whether nodes properties shall be deltified. */
  svn_boolean_t deltify_properties;

  /* Restart deltification histories after each multiple of this value */
  apr_int64_t max_deltification_walk;

  /* Maximum number of length of the linear part at the top of the
   * deltification history after which skip deltas will be used. */
  apr_int64_t max_linear_deltification;

  /* Pointer to svn_fs_open. */
  svn_error_t *(*svn_fs_open_)(svn_fs_t **, const char *, apr_hash_t *,
                               apr_pool_t *);
} fs_fs_data_t;


/*** Filesystem Transaction ***/
typedef struct transaction_t
{
  /* property list (const char * name, svn_string_t * value).
     may be NULL if there are no properties.  */
  apr_hash_t *proplist;

  /* node revision id of the root node.  */
  const svn_fs_id_t *root_id;

  /* node revision id of the node which is the root of the revision
     upon which this txn is base.  (unfinished only) */
  const svn_fs_id_t *base_id;

  /* copies list (const char * copy_ids), or NULL if there have been
     no copies in this transaction.  */
  apr_array_header_t *copies;

} transaction_t;


/*** Representation ***/
/* If you add fields to this, check to see if you need to change
 * svn_fs_fs__rep_copy. */
typedef struct representation_t
{
  /* Checksums for the contents produced by this representation.
     This checksum is for the contents the rep shows to consumers,
     regardless of how the rep stores the data under the hood.  It is
     independent of the storage (fulltext, delta, whatever).

     If checksum is NULL, then for compatibility behave as though this
     checksum matches the expected checksum.

     The md5 checksum is always filled, unless this is rep which was
     retrieved from the rep-cache.  The sha1 checksum is only computed on
     a write, for use with rep-sharing; it may be read from an existing
     representation, but otherwise it is NULL. */
  svn_checksum_t *md5_checksum;
  svn_checksum_t *sha1_checksum;

  /* Revision where this representation is located. */
  svn_revnum_t revision;

  /* Offset into the revision file where it is located. */
  apr_off_t offset;

  /* The size of the representation in bytes as seen in the revision
     file. */
  svn_filesize_t size;

  /* The size of the fulltext of the representation. If this is 0,
   * the fulltext size is equal to representation size in the rev file, */
  svn_filesize_t expanded_size;

  /* Is this representation a transaction? */
  const char *txn_id;

  /* For rep-sharing, we need a way of uniquifying node-revs which share the
     same representation (see svn_fs_fs__noderev_same_rep_key() ).  So, we
     store the original txn of the node rev (not the rep!), along with some
     intra-node uniqification content.

     May be NULL, in which case, it is considered to match other NULL
     values.*/
  const char *uniquifier;
} representation_t;


/*** Node-Revision ***/
/* If you add fields to this, check to see if you need to change
 * copy_node_revision in dag.c. */
typedef struct node_revision_t
{
  /* node kind */
  svn_node_kind_t kind;

  /* The node-id for this node-rev. */
  const svn_fs_id_t *id;

  /* predecessor node revision id, or NULL if there is no predecessor
     for this node revision */
  const svn_fs_id_t *predecessor_id;

  /* If this node-rev is a copy, where was it copied from? */
  const char *copyfrom_path;
  svn_revnum_t copyfrom_rev;

  /* Helper for history tracing, root of the parent tree from whence
     this node-rev was copied. */
  svn_revnum_t copyroot_rev;
  const char *copyroot_path;

  /* number of predecessors this node revision has (recursively), or
     -1 if not known (for backward compatibility). */
  int predecessor_count;

  /* representation key for this node's properties.  may be NULL if
     there are no properties.  */
  representation_t *prop_rep;

  /* representation for this node's data.  may be NULL if there is
     no data. */
  representation_t *data_rep;

  /* path at which this node first came into existence.  */
  const char *created_path;

  /* is this the unmodified root of a transaction? */
  svn_boolean_t is_fresh_txn_root;

  /* Number of nodes with svn:mergeinfo properties that are
     descendants of this node (including it itself) */
  apr_int64_t mergeinfo_count;

  /* Does this node itself have svn:mergeinfo? */
  svn_boolean_t has_mergeinfo;

} node_revision_t;


/*** Change ***/
typedef struct change_t
{
  /* Path of the change. */
  const char *path;

  /* Node revision ID of the change. */
  const svn_fs_id_t *noderev_id;

  /* The kind of change. */
  svn_fs_path_change_kind_t kind;

  /* Text or property mods? */
  svn_boolean_t text_mod;
  svn_boolean_t prop_mod;

  /* Node kind (possibly svn_node_unknown). */
  svn_node_kind_t node_kind;

  /* Copyfrom revision and path. */
  svn_revnum_t copyfrom_rev;
  const char * copyfrom_path;

} change_t;


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_LIBSVN_FS_FS_H */
