/**
 * @copyright
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
 * @endcopyright
 *
 * @file svn_client_shelf2.h
 * @brief Subversion's client library: experimental shelving v2
 */

#ifndef SVN_CLIENT_SHELF2_H
#define SVN_CLIENT_SHELF2_H

#include <apr.h>
#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_time.h>

#include "svn_client.h"
#include "svn_types.h"
#include "svn_string.h"
#include "svn_wc.h"
#include "svn_diff.h"
#include "private/svn_diff_tree.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



/** Shelving v2, with checkpoints
 *
 * @defgroup svn_client_shelves_checkpoints Shelves and checkpoints
 * @{
 */

/** A shelf.
 *
 * @warning EXPERIMENTAL.
 */
typedef struct svn_client__shelf2_t
{
    /* Public fields (read-only for public use) */
    const char *name;
    int max_version;  /** @deprecated */

    /* Private fields */
    const char *wc_root_abspath;
    const char *shelves_dir;
    apr_hash_t *revprops;  /* non-null; allocated in POOL */
    svn_client_ctx_t *ctx;
    apr_pool_t *pool;
} svn_client__shelf2_t;

/** One version of a shelved change-set.
 *
 * @warning EXPERIMENTAL.
 */
typedef struct svn_client__shelf2_version_t
{
  /* Public fields (read-only for public use) */
  svn_client__shelf2_t *shelf;
  apr_time_t mtime;  /** time-stamp of this version */

  /* Private fields */
  const char *files_dir_abspath;  /** abspath of the storage area */
  int version_number;  /** version number starting from 1 */
} svn_client__shelf2_version_t;

/** Open an existing shelf or create a new shelf.
 *
 * Create a new shelf (containing no versions) if a shelf named @a name
 * is not found.
 *
 * The shelf should be closed after use by calling svn_client_shelf_close().
 *
 * @a local_abspath is any path in the WC and is used to find the WC root.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_open_or_create(svn_client__shelf2_t **shelf_p,
                                  const char *name,
                                  const char *local_abspath,
                                  svn_client_ctx_t *ctx,
                                  apr_pool_t *result_pool);

/** Open an existing shelf named @a name, or error if it doesn't exist.
 *
 * The shelf should be closed after use by calling svn_client_shelf_close().
 *
 * @a local_abspath is any path in the WC and is used to find the WC root.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_open_existing(svn_client__shelf2_t **shelf_p,
                                 const char *name,
                                 const char *local_abspath,
                                 svn_client_ctx_t *ctx,
                                 apr_pool_t *result_pool);

/** Close @a shelf.
 *
 * If @a shelf is NULL, do nothing; otherwise @a shelf must be an open shelf.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_close(svn_client__shelf2_t *shelf,
                         apr_pool_t *scratch_pool);

/** Delete the shelf named @a name, or error if it doesn't exist.
 *
 * @a local_abspath is any path in the WC and is used to find the WC root.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_delete(const char *name,
                          const char *local_abspath,
                          svn_boolean_t dry_run,
                          svn_client_ctx_t *ctx,
                          apr_pool_t *scratch_pool);

/** Save the local modifications found by @a paths, @a depth,
 * @a changelists as a new version of @a shelf.
 *
 * If any paths are shelved, create a new shelf-version and return the new
 * shelf-version in @a *new_version_p, else set @a *new_version_p to null.
 * @a new_version_p may be null if that output is not wanted; a new shelf-
 * version is still saved and may be found through @a shelf.
 *
 * @a paths are relative to the CWD, or absolute.
 *
 * For each successfully shelved path: call @a shelved_func (if not null)
 * with @a shelved_baton.
 *
 * If any paths cannot be shelved: if @a not_shelved_func is given, call
 * it with @a not_shelved_baton for each such path, and still create a new
 * shelf-version if any paths are shelved.
 *
 * This function does not revert the changes from the WC; use
 * svn_client_shelf_unapply() for that.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_save_new_version3(svn_client__shelf2_version_t **new_version_p,
                                     svn_client__shelf2_t *shelf,
                                     const apr_array_header_t *paths,
                                     svn_depth_t depth,
                                     const apr_array_header_t *changelists,
                                     svn_client_status_func_t shelved_func,
                                     void *shelved_baton,
                                     svn_client_status_func_t not_shelved_func,
                                     void *not_shelved_baton,
                                     apr_pool_t *scratch_pool);

/** Delete all newer versions of @a shelf newer than @a shelf_version.
 *
 * If @a shelf_version is null, delete all versions of @a shelf. (The
 * shelf will still exist, with any log message and other revprops, but
 * with no versions in it.)
 *
 * Leave the shelf's log message and other revprops unchanged.
 *
 * Any #svn_client_shelf_version_t object that refers to a deleted version
 * will become invalid: attempting to use it will give undefined behaviour.
 * The given @a shelf_version will remain valid.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_delete_newer_versions(svn_client__shelf2_t *shelf,
                                         svn_client__shelf2_version_t *shelf_version,
                                         apr_pool_t *scratch_pool);

/** Return in @a shelf_version an existing version of @a shelf, given its
 * @a version_number. Error if that version doesn't exist.
 *
 * There is no need to "close" it after use.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_version_open(svn_client__shelf2_version_t **shelf_version_p,
                                svn_client__shelf2_t *shelf,
                                int version_number,
                                apr_pool_t *result_pool,
                                apr_pool_t *scratch_pool);

/** Return in @a shelf_version the newest version of @a shelf.
 *
 * Set @a shelf_version to null if no versions exist.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_get_newest_version(svn_client__shelf2_version_t **shelf_version_p,
                                      svn_client__shelf2_t *shelf,
                                      apr_pool_t *result_pool,
                                      apr_pool_t *scratch_pool);

/** Return in @a versions_p an array of (#svn_client_shelf_version_t *)
 * containing all versions of @a shelf.
 *
 * The versions will be in chronological order, oldest to newest.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_get_all_versions(apr_array_header_t **versions_p,
                                    svn_client__shelf2_t *shelf,
                                    apr_pool_t *result_pool,
                                    apr_pool_t *scratch_pool);

/** Apply @a shelf_version to the WC.
 *
 * If @a dry_run is true, try applying the shelf-version to the WC and
 * report the full set of notifications about successes and conflicts,
 * but leave the WC untouched.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_apply(svn_client__shelf2_version_t *shelf_version,
                         svn_boolean_t dry_run,
                         apr_pool_t *scratch_pool);

/** Test whether we can successfully apply the changes for @a file_relpath
 * in @a shelf_version to the WC.
 *
 * Set @a *conflict_p to true if the changes conflict with the WC state,
 * else to false.
 *
 * If @a file_relpath is not found in @a shelf_version, set @a *conflict_p
 * to FALSE.
 *
 * @a file_relpath is relative to the WC root.
 *
 * A conflict means the shelf cannot be applied successfully to the WC
 * because the change to be applied is not compatible with the current
 * working state of the WC file. Examples are a text conflict, or the
 * file does not exist or is a directory, or the shelf is trying to add
 * the file but it already exists, or trying to delete it but it does not
 * exist.
 *
 * Return an error only if something is broken, e.g. unable to read data
 * from the specified shelf-version.
 *
 * Leave the WC untouched.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_test_apply_file(svn_boolean_t *conflict_p,
                                   svn_client__shelf2_version_t *shelf_version,
                                   const char *file_relpath,
                                   apr_pool_t *scratch_pool);

/** Reverse-apply @a shelf_version to the WC.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_unapply(svn_client__shelf2_version_t *shelf_version,
                           svn_boolean_t dry_run,
                           apr_pool_t *scratch_pool);

/** Set @a *affected_paths to a hash with one entry for each path affected
 * by the @a shelf_version.
 *
 * The hash key is the path of the affected file, relative to the WC root.
 *
 * (Future possibility: When moves and copies are supported, the hash key
 * is the old path and value is the new path.)
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_paths_changed(apr_hash_t **affected_paths,
                                 svn_client__shelf2_version_t *shelf_version,
                                 apr_pool_t *result_pool,
                                 apr_pool_t *scratch_pool);

/** Set @a shelf's revprop @a prop_name to @a prop_val.
 *
 * This can be used to set or change the shelf's log message
 * (property name "svn:log" or #SVN_PROP_REVISION_LOG).
 *
 * If @a prop_val is NULL, delete the property (if present).
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_revprop_set(svn_client__shelf2_t *shelf,
                               const char *prop_name,
                               const svn_string_t *prop_val,
                               apr_pool_t *scratch_pool);

/** Set @a shelf's revprops to @a revprop_table.
 *
 * This deletes all previous revprops.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_revprop_set_all(svn_client__shelf2_t *shelf,
                                   apr_hash_t *revprop_table,
                                   apr_pool_t *scratch_pool);

/** Get @a shelf's revprop @a prop_name into @a *prop_val.
 *
 * If the property is not present, set @a *prop_val to NULL.
 *
 * This can be used to get the shelf's log message
 * (property name "svn:log" or #SVN_PROP_REVISION_LOG).
 *
 * The lifetime of the result is limited to that of @a shelf and/or
 * of @a result_pool.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_revprop_get(svn_string_t **prop_val,
                               svn_client__shelf2_t *shelf,
                               const char *prop_name,
                               apr_pool_t *result_pool);

/** Get @a shelf's revprops into @a props.
 *
 * The lifetime of the result is limited to that of @a shelf and/or
 * of @a result_pool.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_revprop_list(apr_hash_t **props,
                                svn_client__shelf2_t *shelf,
                                apr_pool_t *result_pool);

/** Set the log message in @a shelf to @a log_message.
 *
 * If @a log_message is null, delete the log message.
 *
 * Similar to svn_client_shelf_revprop_set(... SVN_PROP_REVISION_LOG ...).
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_set_log_message(svn_client__shelf2_t *shelf,
                                   const char *log_message,
                                   apr_pool_t *scratch_pool);

/** Get the log message in @a shelf into @a *log_message.
 *
 * Set @a *log_message to NULL if there is no log message.
 *
 * Similar to svn_client_shelf_revprop_get(... SVN_PROP_REVISION_LOG ...).
 *
 * The result is allocated in @a result_pool.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_get_log_message(char **log_message,
                                   svn_client__shelf2_t *shelf,
                                   apr_pool_t *result_pool);

/** Information about a shelf.
 *
 * @warning EXPERIMENTAL.
 */
typedef struct svn_client__shelf2_info_t
{
  apr_time_t mtime;  /* mtime of the latest change */
} svn_client__shelf2_info_t;

/** Set @a *shelf_infos to a hash, keyed by shelf name, of pointers to
 * @c svn_client_shelf_info_t structures, one for each shelf in the
 * given WC.
 *
 * @a local_abspath is any path in the WC and is used to find the WC root.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_list(apr_hash_t **shelf_infos,
                        const char *local_abspath,
                        svn_client_ctx_t *ctx,
                        apr_pool_t *result_pool,
                        apr_pool_t *scratch_pool);

/* Report the shelved status of all the shelved paths in SHELF_VERSION
 * via WALK_FUNC(WALK_BATON, ...).
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_version_status_walk(svn_client__shelf2_version_t *shelf_version,
                                       const char *wc_relpath,
                                       svn_wc_status_func4_t walk_func,
                                       void *walk_baton,
                                       apr_pool_t *scratch_pool);

/** Output the subtree of @a shelf_version rooted at @a shelf_relpath
 * as a diff to @a diff_processor.
 *
 * ### depth and ignore_ancestry are currently ignored.
 *
 * @warning EXPERIMENTAL.
 */
SVN_EXPERIMENTAL
svn_error_t *
svn_client__shelf2_diff(svn_client__shelf2_version_t *shelf_version,
                        const char *shelf_relpath,
                        svn_depth_t depth,
                        svn_boolean_t ignore_ancestry,
                        const svn_diff_tree_processor_t *diff_processor,
                        apr_pool_t *scratch_pool);

/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* SVN_CLIENT_SHELF2_H */
