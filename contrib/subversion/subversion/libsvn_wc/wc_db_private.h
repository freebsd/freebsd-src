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
 */

/* This file is not for general consumption; it should only be used by
   wc_db.c. */
#ifndef SVN_WC__I_AM_WC_DB
#error "You should not be using these data structures directly"
#endif /* SVN_WC__I_AM_WC_DB */

#ifndef WC_DB_PRIVATE_H
#define WC_DB_PRIVATE_H

#include "wc_db.h"


struct svn_wc__db_t {
  /* We need the config whenever we run into a new WC directory, in order
     to figure out where we should look for the corresponding datastore. */
  svn_config_t *config;

  /* Should we fail with SVN_ERR_WC_UPGRADE_REQUIRED when it is
     opened, and found to be not-current?  */
  svn_boolean_t verify_format;

  /* Should we ensure the WORK_QUEUE is empty when a WCROOT is opened?  */
  svn_boolean_t enforce_empty_wq;

  /* Should we open Sqlite databases EXCLUSIVE */
  svn_boolean_t exclusive;

  /* Map a given working copy directory to its relevant data.
     const char *local_abspath -> svn_wc__db_wcroot_t *wcroot  */
  apr_hash_t *dir_data;

  /* A few members to assist with caching of kind values for paths.  See
     get_path_kind() for use. */
  struct
  {
    svn_stringbuf_t *abspath;
    svn_node_kind_t kind;
  } parse_cache;

  /* As we grow the state of this DB, allocate that state here. */
  apr_pool_t *state_pool;
};


/* Hold information about an owned lock */
typedef struct svn_wc__db_wclock_t
{
  /* Relative path of the lock root */
  const char *local_relpath;

  /* Number of levels locked (0 for infinity) */
  int levels;
} svn_wc__db_wclock_t;


/** Hold information about a WCROOT.
 *
 * This structure is referenced by all per-directory handles underneath it.
 */
typedef struct svn_wc__db_wcroot_t {
  /* Location of this wcroot in the filesystem.  */
  const char *abspath;

  /* The SQLite database containing the metadata for everything in
     this wcroot.  */
  svn_sqlite__db_t *sdb;

  /* The WCROOT.id for this directory (and all its children).  */
  apr_int64_t wc_id;

  /* The format of this wcroot's metadata storage (see wc.h). If the
     format has not (yet) been determined, this will be UNKNOWN_FORMAT.  */
  int format;

  /* Array of svn_wc__db_wclock_t structures (not pointers!).
     Typically just one or two locks maximum. */
  apr_array_header_t *owned_locks;

  /* Map a working copy directory to a cached adm_access baton.
     const char *local_abspath -> svn_wc_adm_access_t *adm_access */
  apr_hash_t *access_cache;

} svn_wc__db_wcroot_t;


/* */
svn_error_t *
svn_wc__db_close_many_wcroots(apr_hash_t *roots,
                              apr_pool_t *state_pool,
                              apr_pool_t *scratch_pool);


/* Construct a new svn_wc__db_wcroot_t. The WCROOT_ABSPATH and SDB parameters
   must have lifetime of at least RESULT_POOL.  */
svn_error_t *
svn_wc__db_pdh_create_wcroot(svn_wc__db_wcroot_t **wcroot,
                             const char *wcroot_abspath,
                             svn_sqlite__db_t *sdb,
                             apr_int64_t wc_id,
                             int format,
                             svn_boolean_t verify_format,
                             svn_boolean_t enforce_empty_wq,
                             apr_pool_t *result_pool,
                             apr_pool_t *scratch_pool);


/* For a given LOCAL_ABSPATH, figure out what sqlite database (WCROOT) to
   use and the RELPATH within that wcroot.

   *LOCAL_RELPATH will be allocated within RESULT_POOL. Temporary allocations
   will be made in SCRATCH_POOL.

   *WCROOT will be allocated within DB->STATE_POOL.

   Certain internal structures will be allocated in DB->STATE_POOL.
*/
svn_error_t *
svn_wc__db_wcroot_parse_local_abspath(svn_wc__db_wcroot_t **wcroot,
                                      const char **local_relpath,
                                      svn_wc__db_t *db,
                                      const char *local_abspath,
                                      apr_pool_t *result_pool,
                                      apr_pool_t *scratch_pool);


/* Assert that the given WCROOT is usable.
   NOTE: the expression is multiply-evaluated!!  */
#define VERIFY_USABLE_WCROOT(wcroot)  SVN_ERR_ASSERT(               \
    (wcroot) != NULL && (wcroot)->format == SVN_WC__VERSION)

/* Check if the WCROOT is usable for light db operations such as path
   calculations */
#define CHECK_MINIMAL_WCROOT(wcroot, abspath, scratch_pool)             \
    do                                                                  \
    {                                                                   \
      if (wcroot == NULL)                                               \
        return svn_error_createf(SVN_ERR_WC_NOT_WORKING_COPY, NULL,     \
                    _("The node '%s' is not in a working copy."),       \
                             svn_dirent_local_style(wri_abspath,        \
                                                    scratch_pool));     \
    }                                                                   \
    while (0)

/* Calculates the depth of the relpath below "" */
APR_INLINE static int
relpath_depth(const char *relpath)
{
  int n = 1;
  if (*relpath == '\0')
    return 0;

  do
  {
    if (*relpath == '/')
      n++;
  }
  while (*(++relpath));

  return n;
}


/* */
svn_error_t *
svn_wc__db_util_fetch_wc_id(apr_int64_t *wc_id,
                            svn_sqlite__db_t *sdb,
                            apr_pool_t *scratch_pool);

/* Open a connection in *SDB to the WC database found in the WC metadata
 * directory inside DIR_ABSPATH, having the filename SDB_FNAME.
 *
 * SMODE is passed to svn_sqlite__open().
 *
 * Register MY_STATEMENTS, or if that is null, the default set of WC DB
 * statements, as the set of statements to be prepared now and executed
 * later.  MY_STATEMENTS (the strings and the array itself) is not duplicated
 * internally, and should have a lifetime at least as long as RESULT_POOL.
 * See svn_sqlite__open() for details. */
svn_error_t *
svn_wc__db_util_open_db(svn_sqlite__db_t **sdb,
                        const char *dir_abspath,
                        const char *sdb_fname,
                        svn_sqlite__mode_t smode,
                        svn_boolean_t exclusive,
                        const char *const *my_statements,
                        apr_pool_t *result_pool,
                        apr_pool_t *scratch_pool);

/* Like svn_wc__db_read_info(), but taking WCROOT+LOCAL_RELPATH instead of
   DB+LOCAL_ABSPATH, and outputting repos ids instead of URL+UUID. */
svn_error_t *
svn_wc__db_read_info_internal(svn_wc__db_status_t *status,
                              svn_node_kind_t *kind,
                              svn_revnum_t *revision,
                              const char **repos_relpath,
                              apr_int64_t *repos_id,
                              svn_revnum_t *changed_rev,
                              apr_time_t *changed_date,
                              const char **changed_author,
                              svn_depth_t *depth,
                              const svn_checksum_t **checksum,
                              const char **target,
                              const char **original_repos_relpath,
                              apr_int64_t *original_repos_id,
                              svn_revnum_t *original_revision,
                              svn_wc__db_lock_t **lock,
                              svn_filesize_t *recorded_size,
                              apr_time_t *recorded_mod_time,
                              const char **changelist,
                              svn_boolean_t *conflicted,
                              svn_boolean_t *op_root,
                              svn_boolean_t *had_props,
                              svn_boolean_t *props_mod,
                              svn_boolean_t *have_base,
                              svn_boolean_t *have_more_work,
                              svn_boolean_t *have_work,
                              svn_wc__db_wcroot_t *wcroot,
                              const char *local_relpath,
                              apr_pool_t *result_pool,
                              apr_pool_t *scratch_pool);

/* Like svn_wc__db_base_get_info(), but taking WCROOT+LOCAL_RELPATH instead of
   DB+LOCAL_ABSPATH and outputting REPOS_ID instead of URL+UUID. */
svn_error_t *
svn_wc__db_base_get_info_internal(svn_wc__db_status_t *status,
                                  svn_node_kind_t *kind,
                                  svn_revnum_t *revision,
                                  const char **repos_relpath,
                                  apr_int64_t *repos_id,
                                  svn_revnum_t *changed_rev,
                                  apr_time_t *changed_date,
                                  const char **changed_author,
                                  svn_depth_t *depth,
                                  const svn_checksum_t **checksum,
                                  const char **target,
                                  svn_wc__db_lock_t **lock,
                                  svn_boolean_t *had_props,
                                  apr_hash_t **props,
                                  svn_boolean_t *update_root,
                                  svn_wc__db_wcroot_t *wcroot,
                                  const char *local_relpath,
                                  apr_pool_t *result_pool,
                                  apr_pool_t *scratch_pool);

/* Similar to svn_wc__db_base_get_info(), but taking WCROOT+LOCAL_RELPATH
 * instead of DB+LOCAL_ABSPATH, an explicit op-depth of the node to get
 * information about, and outputting REPOS_ID instead of URL+UUID, and
 * without the LOCK or UPDATE_ROOT outputs.
 *
 * OR
 *
 * Similar to svn_wc__db_base_get_info_internal(), but taking an explicit
 * op-depth OP_DEPTH of the node to get information about, and without the
 * LOCK or UPDATE_ROOT outputs.
 *
 * ### [JAF] TODO: Harmonize svn_wc__db_base_get_info[_internal] with
 * svn_wc__db_depth_get_info -- common API, common implementation.
 */
svn_error_t *
svn_wc__db_depth_get_info(svn_wc__db_status_t *status,
                          svn_node_kind_t *kind,
                          svn_revnum_t *revision,
                          const char **repos_relpath,
                          apr_int64_t *repos_id,
                          svn_revnum_t *changed_rev,
                          apr_time_t *changed_date,
                          const char **changed_author,
                          svn_depth_t *depth,
                          const svn_checksum_t **checksum,
                          const char **target,
                          svn_boolean_t *had_props,
                          apr_hash_t **props,
                          svn_wc__db_wcroot_t *wcroot,
                          const char *local_relpath,
                          int op_depth,
                          apr_pool_t *result_pool,
                          apr_pool_t *scratch_pool);

/* Look up REPOS_ID in SDB and set *REPOS_ROOT_URL and/or *REPOS_UUID to
   its root URL and UUID respectively.  If REPOS_ID is INVALID_REPOS_ID,
   use NULL for both URL and UUID.  Either or both output parameters may be
   NULL if not wanted.  */
svn_error_t *
svn_wc__db_fetch_repos_info(const char **repos_root_url,
                            const char **repos_uuid,
                            svn_sqlite__db_t *sdb,
                            apr_int64_t repos_id,
                            apr_pool_t *result_pool);

/* Like svn_wc__db_read_conflict(), but with WCROOT+LOCAL_RELPATH instead of
   DB+LOCAL_ABSPATH, and outputting relpaths instead of abspaths. */
svn_error_t *
svn_wc__db_read_conflict_internal(svn_skel_t **conflict,
                                  svn_wc__db_wcroot_t *wcroot,
                                  const char *local_relpath,
                                  apr_pool_t *result_pool,
                                  apr_pool_t *scratch_pool);

/* Like svn_wc__db_op_mark_conflict(), but with WCROOT+LOCAL_RELPATH instead of
   DB+LOCAL_ABSPATH. */
svn_error_t *
svn_wc__db_mark_conflict_internal(svn_wc__db_wcroot_t *wcroot,
                                  const char *local_relpath,
                                  const svn_skel_t *conflict_skel,
                                  apr_pool_t *scratch_pool);


/* Transaction handling */

/* A callback which supplies WCROOTs and LOCAL_RELPATHs. */
typedef svn_error_t *(*svn_wc__db_txn_callback_t)(void *baton,
                                          svn_wc__db_wcroot_t *wcroot,
                                          const char *local_relpath,
                                          apr_pool_t *scratch_pool);


/* Run CB_FUNC in a SQLite transaction with CB_BATON, using WCROOT and
   LOCAL_RELPATH.  If callbacks require additional information, they may
   provide it using CB_BATON. */
svn_error_t *
svn_wc__db_with_txn(svn_wc__db_wcroot_t *wcroot,
                    const char *local_relpath,
                    svn_wc__db_txn_callback_t cb_func,
                    void *cb_baton,
                    apr_pool_t *scratch_pool);

/* Evaluate the expression EXPR within a transaction.
 *
 * Begin a transaction in WCROOT's DB; evaluate the expression EXPR, which would
 * typically be a function call that does some work in DB; finally commit
 * the transaction if EXPR evaluated to SVN_NO_ERROR, otherwise roll back
 * the transaction.
 */
#define SVN_WC__DB_WITH_TXN(expr, wcroot) \
  SVN_SQLITE__WITH_LOCK(expr, (wcroot)->sdb)


/* Evaluate the expressions EXPR1..EXPR4 within a transaction, returning the
 * first error if an error occurs.
 *
 * Begin a transaction in WCROOT's DB; evaluate the expressions, which would
 * typically be  function calls that do some work in DB; finally commit
 * the transaction if EXPR evaluated to SVN_NO_ERROR, otherwise roll back
 * the transaction.
 */
#define SVN_WC__DB_WITH_TXN4(expr1, expr2, expr3, expr4, wcroot) \
  SVN_SQLITE__WITH_LOCK4(expr1, expr2, expr3, expr4, (wcroot)->sdb)


/* Return CHILDREN mapping const char * names to svn_node_kind_t * for the
   children of LOCAL_RELPATH at OP_DEPTH. */
svn_error_t *
svn_wc__db_get_children_op_depth(apr_hash_t **children,
                                 svn_wc__db_wcroot_t *wcroot,
                                 const char *local_relpath,
                                 int op_depth,
                                 apr_pool_t *result_pool,
                                 apr_pool_t *scratch_pool);


/* Extend any delete of the parent of LOCAL_RELPATH to LOCAL_RELPATH.

   ### What about KIND and OP_DEPTH?  KIND ought to be redundant; I'm
       discussing on dev@ whether we can let that be null for presence
       == base-deleted.  OP_DEPTH is the op-depth of what, and why?
       It is used to select the lowest working node higher than OP_DEPTH,
       so, in terms of the API, OP_DEPTH means ...?

   Given a wc:

              0         1         2         3         4
              normal
   A          normal
   A/B        normal              normal
   A/B/C                          not-pres  normal
   A/B/C/D                                            normal

   That is checkout, delete A/B, copy a replacement A/B, delete copied
   child A/B/C, add replacement A/B/C, add A/B/C/D.

   Now an update that adds base nodes for A/B/C, A/B/C/D and A/B/C/D/E
   must extend the A/B deletion:

              0         1         2         3         4
              normal
   A          normal
   A/B        normal              normal
   A/B/C      normal              not-pres  normal
   A/B/C/D    normal              base-del            normal
   A/B/C/D/E  normal              base-del

   When adding a node if the parent has a higher working node then the
   parent node is deleted (or replaced) and the delete must be extended
   to cover new node.

   In the example above A/B/C/D and A/B/C/D/E are the nodes that get
   the extended delete, A/B/C is already deleted.
 */
svn_error_t *
svn_wc__db_extend_parent_delete(svn_wc__db_wcroot_t *wcroot,
                                const char *local_relpath,
                                svn_node_kind_t kind,
                                int op_depth,
                                apr_pool_t *scratch_pool);

svn_error_t *
svn_wc__db_retract_parent_delete(svn_wc__db_wcroot_t *wcroot,
                                 const char *local_relpath,
                                 int op_depth,
                                 apr_pool_t *scratch_pool);

svn_error_t *
svn_wc__db_op_depth_moved_to(const char **move_dst_relpath,
                             const char **move_dst_op_root_relpath,
                             const char **move_src_root_relpath,
                             const char **move_src_op_root_relpath,
                             int op_depth,
                             svn_wc__db_wcroot_t *wcroot,
                             const char *local_relpath,
                             apr_pool_t *result_pool,
                             apr_pool_t *scratch_pool);

/* Do a post-drive revision bump for the moved-away destination for
   any move sources under LOCAL_RELPATH.  This is called from within
   the revision bump transaction after the tree at LOCAL_RELPATH has
   been bumped. */
svn_error_t *
svn_wc__db_bump_moved_away(svn_wc__db_wcroot_t *wcroot,
                           const char *local_relpath,
                           svn_depth_t depth,
                           svn_wc__db_t *db,
                           apr_pool_t *scratch_pool);

/* Unbreak the move from LOCAL_RELPATH on op-depth in WCROOT, by making
   the destination a normal copy */
svn_error_t *
svn_wc__db_resolve_break_moved_away_internal(svn_wc__db_wcroot_t *wcroot,
                                             const char *local_relpath,
                                             int op_depth,
                                             apr_pool_t *scratch_pool);

svn_error_t *
svn_wc__db_update_move_list_notify(svn_wc__db_wcroot_t *wcroot,
                                   svn_revnum_t old_revision,
                                   svn_revnum_t new_revision,
                                   svn_wc_notify_func2_t notify_func,
                                   void *notify_baton,
                                   apr_pool_t *scratch_pool);

#endif /* WC_DB_PRIVATE_H */
