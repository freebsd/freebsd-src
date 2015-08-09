/*
 * conflicts.c: routines for managing conflict data.
 *            NOTE: this code doesn't know where the conflict is
 *            actually stored.
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



#include <string.h>

#include <apr_pools.h>
#include <apr_tables.h>
#include <apr_hash.h>
#include <apr_errno.h>

#include "svn_hash.h"
#include "svn_types.h"
#include "svn_pools.h"
#include "svn_string.h"
#include "svn_error.h"
#include "svn_dirent_uri.h"
#include "svn_wc.h"
#include "svn_io.h"
#include "svn_diff.h"

#include "wc.h"
#include "wc_db.h"
#include "conflicts.h"
#include "workqueue.h"
#include "props.h"

#include "private/svn_wc_private.h"
#include "private/svn_skel.h"
#include "private/svn_string_private.h"

#include "svn_private_config.h"

/* --------------------------------------------------------------------
 * Conflict skel management
 */

svn_skel_t *
svn_wc__conflict_skel_create(apr_pool_t *result_pool)
{
  svn_skel_t *conflict_skel = svn_skel__make_empty_list(result_pool);

  /* Add empty CONFLICTS list */
  svn_skel__prepend(svn_skel__make_empty_list(result_pool), conflict_skel);

  /* Add empty WHY list */
  svn_skel__prepend(svn_skel__make_empty_list(result_pool), conflict_skel);

  return conflict_skel;
}

svn_error_t *
svn_wc__conflict_skel_is_complete(svn_boolean_t *complete,
                                  const svn_skel_t *conflict_skel)
{
  *complete = FALSE;

  if (svn_skel__list_length(conflict_skel) < 2)
    return svn_error_create(SVN_ERR_INCOMPLETE_DATA, NULL,
                            _("Not a conflict skel"));

  if (svn_skel__list_length(conflict_skel->children) < 2)
    return SVN_NO_ERROR; /* WHY is not set */

  if (svn_skel__list_length(conflict_skel->children->next) == 0)
    return SVN_NO_ERROR; /* No conflict set */

  *complete = TRUE;
  return SVN_NO_ERROR;
}

/* Serialize a svn_wc_conflict_version_t before the existing data in skel */
static svn_error_t *
conflict__prepend_location(svn_skel_t *skel,
                           const svn_wc_conflict_version_t *location,
                           svn_boolean_t allow_NULL,
                           apr_pool_t *result_pool,
                           apr_pool_t *scratch_pool)
{
  svn_skel_t *loc;
  SVN_ERR_ASSERT(location || allow_NULL);

  if (!location)
    {
      svn_skel__prepend(svn_skel__make_empty_list(result_pool), skel);
      return SVN_NO_ERROR;
    }

  /* ("subversion" repos_root_url repos_uuid repos_relpath rev kind) */
  loc = svn_skel__make_empty_list(result_pool);

  svn_skel__prepend_str(svn_node_kind_to_word(location->node_kind),
                        loc, result_pool);

  svn_skel__prepend_int(location->peg_rev, loc, result_pool);

  svn_skel__prepend_str(apr_pstrdup(result_pool, location->path_in_repos), loc,
                        result_pool);

  if (!location->repos_uuid) /* Can theoretically be NULL */
    svn_skel__prepend(svn_skel__make_empty_list(result_pool), loc);
  else
    svn_skel__prepend_str(location->repos_uuid, loc, result_pool);

  svn_skel__prepend_str(apr_pstrdup(result_pool, location->repos_url), loc,
                        result_pool);

  svn_skel__prepend_str(SVN_WC__CONFLICT_SRC_SUBVERSION, loc, result_pool);

  svn_skel__prepend(loc, skel);
  return SVN_NO_ERROR;
}

/* Deserialize a svn_wc_conflict_version_t from the skel.
   Set *LOCATION to NULL when the data is not a svn_wc_conflict_version_t. */
static svn_error_t *
conflict__read_location(svn_wc_conflict_version_t **location,
                        const svn_skel_t *skel,
                        apr_pool_t *result_pool,
                        apr_pool_t *scratch_pool)
{
  const char *repos_root_url;
  const char *repos_uuid;
  const char *repos_relpath;
  svn_revnum_t revision;
  apr_int64_t v;
  svn_node_kind_t node_kind;  /* note that 'none' is a legitimate value */
  const char *kind_str;

  const svn_skel_t *c = skel->children;

  if (!svn_skel__matches_atom(c, SVN_WC__CONFLICT_SRC_SUBVERSION))
    {
      *location = NULL;
      return SVN_NO_ERROR;
    }
  c = c->next;

  repos_root_url = apr_pstrmemdup(result_pool, c->data, c->len);
  c = c->next;

  if (c->is_atom)
    repos_uuid = apr_pstrmemdup(result_pool, c->data, c->len);
  else
    repos_uuid = NULL;
  c = c->next;

  repos_relpath = apr_pstrmemdup(result_pool, c->data, c->len);
  c = c->next;

  SVN_ERR(svn_skel__parse_int(&v, c, scratch_pool));
  revision = (svn_revnum_t)v;
  c = c->next;

  kind_str = apr_pstrmemdup(scratch_pool, c->data, c->len);
  node_kind = svn_node_kind_from_word(kind_str);

  *location = svn_wc_conflict_version_create2(repos_root_url,
                                              repos_uuid,
                                              repos_relpath,
                                              revision,
                                              node_kind,
                                              result_pool);
  return SVN_NO_ERROR;
}

/* Get the operation part of CONFLICT_SKELL or NULL if no operation is set
   at this time */
static svn_error_t *
conflict__get_operation(svn_skel_t **why,
                        const svn_skel_t *conflict_skel)
{
  SVN_ERR_ASSERT(conflict_skel
                 && conflict_skel->children
                 && conflict_skel->children->next
                 && !conflict_skel->children->next->is_atom);

  *why = conflict_skel->children;

  if (!(*why)->children)
    *why = NULL; /* Operation is not set yet */

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc__conflict_skel_set_op_update(svn_skel_t *conflict_skel,
                                    const svn_wc_conflict_version_t *original,
                                    const svn_wc_conflict_version_t *target,
                                    apr_pool_t *result_pool,
                                    apr_pool_t *scratch_pool)
{
  svn_skel_t *why;
  svn_skel_t *origins;

  SVN_ERR_ASSERT(conflict_skel
                 && conflict_skel->children
                 && conflict_skel->children->next
                 && !conflict_skel->children->next->is_atom);

  SVN_ERR(conflict__get_operation(&why, conflict_skel));

  SVN_ERR_ASSERT(why == NULL); /* No operation set */

  why = conflict_skel->children;

  origins = svn_skel__make_empty_list(result_pool);

  SVN_ERR(conflict__prepend_location(origins, target, TRUE,
                                     result_pool, scratch_pool));
  SVN_ERR(conflict__prepend_location(origins, original, TRUE,
                                     result_pool, scratch_pool));

  svn_skel__prepend(origins, why);
  svn_skel__prepend_str(SVN_WC__CONFLICT_OP_UPDATE, why, result_pool);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__conflict_skel_set_op_switch(svn_skel_t *conflict_skel,
                                    const svn_wc_conflict_version_t *original,
                                    const svn_wc_conflict_version_t *target,
                                    apr_pool_t *result_pool,
                                    apr_pool_t *scratch_pool)
{
  svn_skel_t *why;
  svn_skel_t *origins;

  SVN_ERR_ASSERT(conflict_skel
                 && conflict_skel->children
                 && conflict_skel->children->next
                 && !conflict_skel->children->next->is_atom);

  SVN_ERR(conflict__get_operation(&why, conflict_skel));

  SVN_ERR_ASSERT(why == NULL); /* No operation set */

  why = conflict_skel->children;

  origins = svn_skel__make_empty_list(result_pool);

  SVN_ERR(conflict__prepend_location(origins, target, TRUE,
                                     result_pool, scratch_pool));
  SVN_ERR(conflict__prepend_location(origins, original, TRUE,
                                     result_pool, scratch_pool));

  svn_skel__prepend(origins, why);
  svn_skel__prepend_str(SVN_WC__CONFLICT_OP_SWITCH, why, result_pool);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__conflict_skel_set_op_merge(svn_skel_t *conflict_skel,
                                   const svn_wc_conflict_version_t *left,
                                   const svn_wc_conflict_version_t *right,
                                   apr_pool_t *result_pool,
                                   apr_pool_t *scratch_pool)
{
  svn_skel_t *why;
  svn_skel_t *origins;

  SVN_ERR_ASSERT(conflict_skel
                 && conflict_skel->children
                 && conflict_skel->children->next
                 && !conflict_skel->children->next->is_atom);

  SVN_ERR(conflict__get_operation(&why, conflict_skel));

  SVN_ERR_ASSERT(why == NULL); /* No operation set */

  why = conflict_skel->children;

  origins = svn_skel__make_empty_list(result_pool);

  SVN_ERR(conflict__prepend_location(origins, right, TRUE,
                                     result_pool, scratch_pool));

  SVN_ERR(conflict__prepend_location(origins, left, TRUE,
                                     result_pool, scratch_pool));

  svn_skel__prepend(origins, why);
  svn_skel__prepend_str(SVN_WC__CONFLICT_OP_MERGE, why, result_pool);

  return SVN_NO_ERROR;
}

/* Gets the conflict data of the specified type CONFLICT_TYPE from
   CONFLICT_SKEL, or NULL if no such conflict is recorded */
static svn_error_t *
conflict__get_conflict(svn_skel_t **conflict,
                       const svn_skel_t *conflict_skel,
                       const char *conflict_type)
{
  svn_skel_t *c;

  SVN_ERR_ASSERT(conflict_skel
                 && conflict_skel->children
                 && conflict_skel->children->next
                 && !conflict_skel->children->next->is_atom);

  for(c = conflict_skel->children->next->children;
      c;
      c = c->next)
    {
      if (svn_skel__matches_atom(c->children, conflict_type))
        {
          *conflict = c;
          return SVN_NO_ERROR;
        }
    }

  *conflict = NULL;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__conflict_skel_add_text_conflict(svn_skel_t *conflict_skel,
                                        svn_wc__db_t *db,
                                        const char *wri_abspath,
                                        const char *mine_abspath,
                                        const char *their_old_abspath,
                                        const char *their_abspath,
                                        apr_pool_t *result_pool,
                                        apr_pool_t *scratch_pool)
{
  svn_skel_t *text_conflict;
  svn_skel_t *markers;

  SVN_ERR(conflict__get_conflict(&text_conflict, conflict_skel,
                                 SVN_WC__CONFLICT_KIND_TEXT));

  SVN_ERR_ASSERT(!text_conflict); /* ### Use proper error? */

  /* Current skel format
     ("text"
      (OLD MINE OLD-THEIRS THEIRS)) */

  text_conflict = svn_skel__make_empty_list(result_pool);
  markers = svn_skel__make_empty_list(result_pool);

if (their_abspath)
    {
      const char *their_relpath;

      SVN_ERR(svn_wc__db_to_relpath(&their_relpath,
                                    db, wri_abspath, their_abspath,
                                    result_pool, scratch_pool));
      svn_skel__prepend_str(their_relpath, markers, result_pool);
    }
  else
    svn_skel__prepend(svn_skel__make_empty_list(result_pool), markers);

  if (mine_abspath)
    {
      const char *mine_relpath;

      SVN_ERR(svn_wc__db_to_relpath(&mine_relpath,
                                    db, wri_abspath, mine_abspath,
                                    result_pool, scratch_pool));
      svn_skel__prepend_str(mine_relpath, markers, result_pool);
    }
  else
    svn_skel__prepend(svn_skel__make_empty_list(result_pool), markers);

  if (their_old_abspath)
    {
      const char *original_relpath;

      SVN_ERR(svn_wc__db_to_relpath(&original_relpath,
                                    db, wri_abspath, their_old_abspath,
                                    result_pool, scratch_pool));
      svn_skel__prepend_str(original_relpath, markers, result_pool);
    }
  else
    svn_skel__prepend(svn_skel__make_empty_list(result_pool), markers);

  svn_skel__prepend(markers, text_conflict);
  svn_skel__prepend_str(SVN_WC__CONFLICT_KIND_TEXT, text_conflict,
                        result_pool);

  /* And add it to the conflict skel */
  svn_skel__prepend(text_conflict, conflict_skel->children->next);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__conflict_skel_add_prop_conflict(svn_skel_t *conflict_skel,
                                        svn_wc__db_t *db,
                                        const char *wri_abspath,
                                        const char *marker_abspath,
                                        const apr_hash_t *mine_props,
                                        const apr_hash_t *their_old_props,
                                        const apr_hash_t *their_props,
                                        const apr_hash_t *conflicted_prop_names,
                                        apr_pool_t *result_pool,
                                        apr_pool_t *scratch_pool)
{
  svn_skel_t *prop_conflict;
  svn_skel_t *props;
  svn_skel_t *conflict_names;
  svn_skel_t *markers;
  apr_hash_index_t *hi;

  SVN_ERR(conflict__get_conflict(&prop_conflict, conflict_skel,
                                 SVN_WC__CONFLICT_KIND_PROP));

  SVN_ERR_ASSERT(!prop_conflict); /* ### Use proper error? */

  /* This function currently implements:
     ("prop"
      ("marker_relpath")
      prop-conflicted_prop_names
      old-props
      mine-props
      their-props)
     NULL lists are recorded as "" */
  /* ### Seems that this may not match what we read out.  Read-out of
   * 'theirs-old' comes as NULL. */

  prop_conflict = svn_skel__make_empty_list(result_pool);

  if (their_props)
    {
      SVN_ERR(svn_skel__unparse_proplist(&props, their_props, result_pool));
      svn_skel__prepend(props, prop_conflict);
    }
  else
    svn_skel__prepend_str("", prop_conflict, result_pool); /* No their_props */

  if (mine_props)
    {
      SVN_ERR(svn_skel__unparse_proplist(&props, mine_props, result_pool));
      svn_skel__prepend(props, prop_conflict);
    }
  else
    svn_skel__prepend_str("", prop_conflict, result_pool); /* No mine_props */

  if (their_old_props)
    {
      SVN_ERR(svn_skel__unparse_proplist(&props, their_old_props,
                                         result_pool));
      svn_skel__prepend(props, prop_conflict);
    }
  else
    svn_skel__prepend_str("", prop_conflict, result_pool); /* No old_props */

  conflict_names = svn_skel__make_empty_list(result_pool);
  for (hi = apr_hash_first(scratch_pool, (apr_hash_t *)conflicted_prop_names);
       hi;
       hi = apr_hash_next(hi))
    {
      svn_skel__prepend_str(apr_pstrdup(result_pool,
                                        svn__apr_hash_index_key(hi)),
                            conflict_names,
                            result_pool);
    }
  svn_skel__prepend(conflict_names, prop_conflict);

  markers = svn_skel__make_empty_list(result_pool);

  if (marker_abspath)
    {
      const char *marker_relpath;
      SVN_ERR(svn_wc__db_to_relpath(&marker_relpath, db, wri_abspath,
                                    marker_abspath,
                                    result_pool, scratch_pool));

      svn_skel__prepend_str(marker_relpath, markers, result_pool);
    }
/*else // ### set via svn_wc__conflict_create_markers
    svn_skel__prepend(svn_skel__make_empty_list(result_pool), markers);*/

  svn_skel__prepend(markers, prop_conflict);

  svn_skel__prepend_str(SVN_WC__CONFLICT_KIND_PROP, prop_conflict, result_pool);

  /* And add it to the conflict skel */
  svn_skel__prepend(prop_conflict, conflict_skel->children->next);

  return SVN_NO_ERROR;
}

/* A map for svn_wc_conflict_reason_t values. */
static const svn_token_map_t local_change_map[] =
{
  { "edited",           svn_wc_conflict_reason_edited },
  { "obstructed",       svn_wc_conflict_reason_obstructed },
  { "deleted",          svn_wc_conflict_reason_deleted },
  { "missing",          svn_wc_conflict_reason_missing },
  { "unversioned",      svn_wc_conflict_reason_unversioned },
  { "added",            svn_wc_conflict_reason_added },
  { "replaced",         svn_wc_conflict_reason_replaced },
  { "moved-away",       svn_wc_conflict_reason_moved_away },
  { "moved-here",       svn_wc_conflict_reason_moved_here },
  { NULL }
};

static const svn_token_map_t incoming_change_map[] =
{
  { "edited",           svn_wc_conflict_action_edit },
  { "added",            svn_wc_conflict_action_add },
  { "deleted",          svn_wc_conflict_action_delete },
  { "replaced",         svn_wc_conflict_action_replace },
  { NULL }
};

svn_error_t *
svn_wc__conflict_skel_add_tree_conflict(svn_skel_t *conflict_skel,
                                        svn_wc__db_t *db,
                                        const char *wri_abspath,
                                        svn_wc_conflict_reason_t local_change,
                                        svn_wc_conflict_action_t incoming_change,
                                        const char *move_src_op_root_abspath,
                                        apr_pool_t *result_pool,
                                        apr_pool_t *scratch_pool)
{
  svn_skel_t *tree_conflict;
  svn_skel_t *markers;

  SVN_ERR(conflict__get_conflict(&tree_conflict, conflict_skel,
                                 SVN_WC__CONFLICT_KIND_TREE));

  SVN_ERR_ASSERT(!tree_conflict); /* ### Use proper error? */

  SVN_ERR_ASSERT(local_change == svn_wc_conflict_reason_moved_away
                 || !move_src_op_root_abspath); /* ### Use proper error? */

  tree_conflict = svn_skel__make_empty_list(result_pool);

  if (local_change == svn_wc_conflict_reason_moved_away
      && move_src_op_root_abspath)
    {
      const char *move_src_op_root_relpath;

      SVN_ERR(svn_wc__db_to_relpath(&move_src_op_root_relpath,
                                    db, wri_abspath,
                                    move_src_op_root_abspath,
                                    result_pool, scratch_pool));

      svn_skel__prepend_str(move_src_op_root_relpath, tree_conflict,
                            result_pool);
    }

  svn_skel__prepend_str(
                svn_token__to_word(incoming_change_map, incoming_change),
                tree_conflict, result_pool);

  svn_skel__prepend_str(
                svn_token__to_word(local_change_map, local_change),
                tree_conflict, result_pool);

  /* Tree conflicts have no marker files */
  markers = svn_skel__make_empty_list(result_pool);
  svn_skel__prepend(markers, tree_conflict);

  svn_skel__prepend_str(SVN_WC__CONFLICT_KIND_TREE, tree_conflict,
                        result_pool);

  /* And add it to the conflict skel */
  svn_skel__prepend(tree_conflict, conflict_skel->children->next);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__conflict_skel_resolve(svn_boolean_t *completely_resolved,
                              svn_skel_t *conflict_skel,
                              svn_wc__db_t *db,
                              const char *wri_abspath,
                              svn_boolean_t resolve_text,
                              const char *resolve_prop,
                              svn_boolean_t resolve_tree,
                              apr_pool_t *result_pool,
                              apr_pool_t *scratch_pool)
{
  svn_skel_t *op;
  svn_skel_t **pconflict;
  SVN_ERR(conflict__get_operation(&op, conflict_skel));

  if (!op)
    return svn_error_create(SVN_ERR_INCOMPLETE_DATA, NULL,
                            _("Not a completed conflict skel"));

  /* We are going to drop items from a linked list. Instead of keeping
     a pointer to the item we want to drop we store a pointer to the
     pointer of what we may drop, to allow setting it to the next item. */

  pconflict = &(conflict_skel->children->next->children);
  while (*pconflict)
    {
      svn_skel_t *c = (*pconflict)->children;

      if (resolve_text
          && svn_skel__matches_atom(c, SVN_WC__CONFLICT_KIND_TEXT))
        {
          /* Remove the text conflict from the linked list */
          *pconflict = (*pconflict)->next;
          continue;
        }
      else if (resolve_prop
               && svn_skel__matches_atom(c, SVN_WC__CONFLICT_KIND_PROP))
        {
          svn_skel_t **ppropnames = &(c->next->next->children);

          if (resolve_prop[0] == '\0')
            *ppropnames = NULL; /* remove all conflicted property names */
          else
            while (*ppropnames)
              {
                if (svn_skel__matches_atom(*ppropnames, resolve_prop))
                  {
                    *ppropnames = (*ppropnames)->next;
                    break;
                  }
                ppropnames = &((*ppropnames)->next);
              }

          /* If no conflicted property names left */
          if (!c->next->next->children)
            {
              /* Remove the propery conflict skel from the linked list */
             *pconflict = (*pconflict)->next;
             continue;
            }
        }
      else if (resolve_tree
               && svn_skel__matches_atom(c, SVN_WC__CONFLICT_KIND_TREE))
        {
          /* Remove the tree conflict from the linked list */
          *pconflict = (*pconflict)->next;
          continue;
        }

      pconflict = &((*pconflict)->next);
    }

  if (completely_resolved)
    {
      /* Nice, we can just call the complete function */
      svn_boolean_t complete_conflict;
      SVN_ERR(svn_wc__conflict_skel_is_complete(&complete_conflict,
                                                conflict_skel));

      *completely_resolved = !complete_conflict;
    }
  return SVN_NO_ERROR;
}


/* A map for svn_wc_operation_t values. */
static const svn_token_map_t operation_map[] =
{
  { "",   svn_wc_operation_none },
  { SVN_WC__CONFLICT_OP_UPDATE, svn_wc_operation_update },
  { SVN_WC__CONFLICT_OP_SWITCH, svn_wc_operation_switch },
  { SVN_WC__CONFLICT_OP_MERGE,  svn_wc_operation_merge },
  { NULL }
};

svn_error_t *
svn_wc__conflict_read_info(svn_wc_operation_t *operation,
                           const apr_array_header_t **locations,
                           svn_boolean_t *text_conflicted,
                           svn_boolean_t *prop_conflicted,
                           svn_boolean_t *tree_conflicted,
                           svn_wc__db_t *db,
                           const char *wri_abspath,
                           const svn_skel_t *conflict_skel,
                           apr_pool_t *result_pool,
                           apr_pool_t *scratch_pool)
{
  svn_skel_t *op;
  const svn_skel_t *c;

  SVN_ERR(conflict__get_operation(&op, conflict_skel));

  if (!op)
    return svn_error_create(SVN_ERR_INCOMPLETE_DATA, NULL,
                            _("Not a completed conflict skel"));

  c = op->children;
  if (operation)
    {
      int value = svn_token__from_mem(operation_map, c->data, c->len);

      if (value != SVN_TOKEN_UNKNOWN)
        *operation = value;
      else
        *operation = svn_wc_operation_none;
    }
  c = c->next;

  if (locations && c->children)
    {
      const svn_skel_t *loc_skel;
      svn_wc_conflict_version_t *loc;
      apr_array_header_t *locs = apr_array_make(result_pool, 2, sizeof(loc));

      for (loc_skel = c->children; loc_skel; loc_skel = loc_skel->next)
        {
          SVN_ERR(conflict__read_location(&loc, loc_skel, result_pool,
                                          scratch_pool));

          APR_ARRAY_PUSH(locs, svn_wc_conflict_version_t *) = loc;
        }

      *locations = locs;
    }
  else if (locations)
    *locations = NULL;

  if (text_conflicted)
    {
      svn_skel_t *c_skel;
      SVN_ERR(conflict__get_conflict(&c_skel, conflict_skel,
                                     SVN_WC__CONFLICT_KIND_TEXT));

      *text_conflicted = (c_skel != NULL);
    }

  if (prop_conflicted)
    {
      svn_skel_t *c_skel;
      SVN_ERR(conflict__get_conflict(&c_skel, conflict_skel,
                                     SVN_WC__CONFLICT_KIND_PROP));

      *prop_conflicted = (c_skel != NULL);
    }

  if (tree_conflicted)
    {
      svn_skel_t *c_skel;
      SVN_ERR(conflict__get_conflict(&c_skel, conflict_skel,
                                     SVN_WC__CONFLICT_KIND_TREE));

      *tree_conflicted = (c_skel != NULL);
    }

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc__conflict_read_text_conflict(const char **mine_abspath,
                                    const char **their_old_abspath,
                                    const char **their_abspath,
                                    svn_wc__db_t *db,
                                    const char *wri_abspath,
                                    const svn_skel_t *conflict_skel,
                                    apr_pool_t *result_pool,
                                    apr_pool_t *scratch_pool)
{
  svn_skel_t *text_conflict;
  const svn_skel_t *m;

  SVN_ERR(conflict__get_conflict(&text_conflict, conflict_skel,
                                 SVN_WC__CONFLICT_KIND_TEXT));

  if (!text_conflict)
    return svn_error_create(SVN_ERR_WC_MISSING, NULL, _("Conflict not set"));

  m = text_conflict->children->next->children;

  if (their_old_abspath)
    {
      if (m->is_atom)
        {
          const char *original_relpath;

          original_relpath = apr_pstrmemdup(scratch_pool, m->data, m->len);
          SVN_ERR(svn_wc__db_from_relpath(their_old_abspath,
                                          db, wri_abspath, original_relpath,
                                          result_pool, scratch_pool));
        }
      else
        *their_old_abspath = NULL;
    }
  m = m->next;

  if (mine_abspath)
    {
      if (m->is_atom)
        {
          const char *mine_relpath;

          mine_relpath = apr_pstrmemdup(scratch_pool, m->data, m->len);
          SVN_ERR(svn_wc__db_from_relpath(mine_abspath,
                                          db, wri_abspath, mine_relpath,
                                          result_pool, scratch_pool));
        }
      else
        *mine_abspath = NULL;
    }
  m = m->next;

  if (their_abspath)
    {
      if (m->is_atom)
        {
          const char *their_relpath;

          their_relpath = apr_pstrmemdup(scratch_pool, m->data, m->len);
          SVN_ERR(svn_wc__db_from_relpath(their_abspath,
                                          db, wri_abspath, their_relpath,
                                          result_pool, scratch_pool));
        }
      else
        *their_abspath = NULL;
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__conflict_read_prop_conflict(const char **marker_abspath,
                                    apr_hash_t **mine_props,
                                    apr_hash_t **their_old_props,
                                    apr_hash_t **their_props,
                                    apr_hash_t **conflicted_prop_names,
                                    svn_wc__db_t *db,
                                    const char *wri_abspath,
                                    const svn_skel_t *conflict_skel,
                                    apr_pool_t *result_pool,
                                    apr_pool_t *scratch_pool)
{
  svn_skel_t *prop_conflict;
  const svn_skel_t *c;

  SVN_ERR(conflict__get_conflict(&prop_conflict, conflict_skel,
                                 SVN_WC__CONFLICT_KIND_PROP));

  if (!prop_conflict)
    return svn_error_create(SVN_ERR_WC_MISSING, NULL, _("Conflict not set"));

  c = prop_conflict->children;

  c = c->next; /* Skip "prop" */

  /* Get marker file */
  if (marker_abspath)
    {
      const char *marker_relpath;

      if (c->children && c->children->is_atom)
        {
          marker_relpath = apr_pstrmemdup(result_pool, c->children->data,
                                        c->children->len);

          SVN_ERR(svn_wc__db_from_relpath(marker_abspath, db, wri_abspath,
                                          marker_relpath,
                                          result_pool, scratch_pool));
        }
      else
        *marker_abspath = NULL;
    }
  c = c->next;

  /* Get conflicted properties */
  if (conflicted_prop_names)
    {
      const svn_skel_t *name;
      *conflicted_prop_names = apr_hash_make(result_pool);

      for (name = c->children; name; name = name->next)
        {
          svn_hash_sets(*conflicted_prop_names,
                        apr_pstrmemdup(result_pool, name->data, name->len),
                        "");
        }
    }
  c = c->next;

  /* Get original properties */
  if (their_old_props)
    {
      if (c->is_atom)
        *their_old_props = apr_hash_make(result_pool);
      else
        SVN_ERR(svn_skel__parse_proplist(their_old_props, c, result_pool));
    }
  c = c->next;

  /* Get mine properties */
  if (mine_props)
    {
      if (c->is_atom)
        *mine_props = apr_hash_make(result_pool);
      else
        SVN_ERR(svn_skel__parse_proplist(mine_props, c, result_pool));
    }
  c = c->next;

  /* Get their properties */
  if (their_props)
    {
      if (c->is_atom)
        *their_props = apr_hash_make(result_pool);
      else
        SVN_ERR(svn_skel__parse_proplist(their_props, c, result_pool));
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__conflict_read_tree_conflict(svn_wc_conflict_reason_t *local_change,
                                    svn_wc_conflict_action_t *incoming_change,
                                    const char **move_src_op_root_abspath,
                                    svn_wc__db_t *db,
                                    const char *wri_abspath,
                                    const svn_skel_t *conflict_skel,
                                    apr_pool_t *result_pool,
                                    apr_pool_t *scratch_pool)
{
  svn_skel_t *tree_conflict;
  const svn_skel_t *c;
  svn_boolean_t is_moved_away = FALSE;

  SVN_ERR(conflict__get_conflict(&tree_conflict, conflict_skel,
                                 SVN_WC__CONFLICT_KIND_TREE));

  if (!tree_conflict)
    return svn_error_create(SVN_ERR_WC_MISSING, NULL, _("Conflict not set"));

  c = tree_conflict->children;

  c = c->next; /* Skip "tree" */

  c = c->next; /* Skip markers */

  {
    int value = svn_token__from_mem(local_change_map, c->data, c->len);

    if (local_change)
      {
        if (value != SVN_TOKEN_UNKNOWN)
          *local_change = value;
        else
          *local_change = svn_wc_conflict_reason_edited;
      }

      is_moved_away = (value == svn_wc_conflict_reason_moved_away);
    }
  c = c->next;

  if (incoming_change)
    {
      int value = svn_token__from_mem(incoming_change_map, c->data, c->len);

      if (value != SVN_TOKEN_UNKNOWN)
        *incoming_change = value;
      else
        *incoming_change = svn_wc_conflict_action_edit;
    }

  c = c->next;

  if (move_src_op_root_abspath)
    {
      /* Only set for update and switch tree conflicts */
      if (c && is_moved_away)
        {
          const char *move_src_op_root_relpath
                            = apr_pstrmemdup(scratch_pool, c->data, c->len);

          SVN_ERR(svn_wc__db_from_relpath(move_src_op_root_abspath,
                                          db, wri_abspath,
                                          move_src_op_root_relpath,
                                          result_pool, scratch_pool));
        }
      else
        *move_src_op_root_abspath = NULL;
    }

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__conflict_read_markers(const apr_array_header_t **markers,
                              svn_wc__db_t *db,
                              const char *wri_abspath,
                              const svn_skel_t *conflict_skel,
                              apr_pool_t *result_pool,
                              apr_pool_t *scratch_pool)
{
  const svn_skel_t *conflict;
  apr_array_header_t *list = NULL;

  SVN_ERR_ASSERT(conflict_skel != NULL);

  /* Walk the conflicts */
  for (conflict = conflict_skel->children->next->children;
       conflict;
       conflict = conflict->next)
    {
      const svn_skel_t *marker;

      /* Get the list of markers stored per conflict */
      for (marker = conflict->children->next->children;
           marker;
           marker = marker->next)
        {
          /* Skip placeholders */
          if (! marker->is_atom)
            continue;

          if (! list)
            list = apr_array_make(result_pool, 4, sizeof(const char *));

          SVN_ERR(svn_wc__db_from_relpath(
                        &APR_ARRAY_PUSH(list, const char*),
                        db, wri_abspath,
                        apr_pstrmemdup(scratch_pool, marker->data,
                                       marker->len),
                        result_pool, scratch_pool));
        }
    }
  *markers = list;

  return SVN_NO_ERROR;
}

/* --------------------------------------------------------------------
 */
/* Helper for svn_wc__conflict_create_markers */
static svn_skel_t *
prop_conflict_skel_new(apr_pool_t *result_pool)
{
  svn_skel_t *operation = svn_skel__make_empty_list(result_pool);
  svn_skel_t *result = svn_skel__make_empty_list(result_pool);

  svn_skel__prepend(operation, result);
  return result;
}


/* Helper for prop_conflict_skel_add */
static void
prepend_prop_value(const svn_string_t *value,
                   svn_skel_t *skel,
                   apr_pool_t *result_pool)
{
  svn_skel_t *value_skel = svn_skel__make_empty_list(result_pool);

  if (value != NULL)
    {
      const void *dup = apr_pmemdup(result_pool, value->data, value->len);

      svn_skel__prepend(svn_skel__mem_atom(dup, value->len, result_pool),
                        value_skel);
    }

  svn_skel__prepend(value_skel, skel);
}


/* Helper for svn_wc__conflict_create_markers */
static svn_error_t *
prop_conflict_skel_add(
  svn_skel_t *skel,
  const char *prop_name,
  const svn_string_t *original_value,
  const svn_string_t *mine_value,
  const svn_string_t *incoming_value,
  const svn_string_t *incoming_base_value,
  apr_pool_t *result_pool,
  apr_pool_t *scratch_pool)
{
  svn_skel_t *prop_skel = svn_skel__make_empty_list(result_pool);

  /* ### check that OPERATION has been filled in.  */

  /* See notes/wc-ng/conflict-storage  */
  prepend_prop_value(incoming_base_value, prop_skel, result_pool);
  prepend_prop_value(incoming_value, prop_skel, result_pool);
  prepend_prop_value(mine_value, prop_skel, result_pool);
  prepend_prop_value(original_value, prop_skel, result_pool);
  svn_skel__prepend_str(apr_pstrdup(result_pool, prop_name), prop_skel,
                        result_pool);
  svn_skel__prepend_str(SVN_WC__CONFLICT_KIND_PROP, prop_skel, result_pool);

  /* Now we append PROP_SKEL to the end of the provided conflict SKEL.  */
  svn_skel__append(skel, prop_skel);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__conflict_create_markers(svn_skel_t **work_items,
                                svn_wc__db_t *db,
                                const char *local_abspath,
                                svn_skel_t *conflict_skel,
                                apr_pool_t *result_pool,
                                apr_pool_t *scratch_pool)
{
  svn_boolean_t prop_conflicted;
  svn_wc_operation_t operation;
  *work_items = NULL;

  SVN_ERR(svn_wc__conflict_read_info(&operation, NULL,
                                     NULL, &prop_conflicted, NULL,
                                     db, local_abspath,
                                     conflict_skel,
                                     scratch_pool, scratch_pool));

  if (prop_conflicted)
    {
      const char *marker_abspath = NULL;
      svn_node_kind_t kind;
      const char *marker_dir;
      const char *marker_name;
      const char *marker_relpath;

      /* Ok, currently we have to do a few things for property conflicts:
         - Create a marker file
         - Create a WQ item that sets the marker name
         - Create a WQ item that fills the marker with the expected data

         This can be simplified once we really store conflict_skel in wc.db */

      SVN_ERR(svn_io_check_path(local_abspath, &kind, scratch_pool));

      if (kind == svn_node_dir)
        {
          marker_dir = local_abspath;
          marker_name = SVN_WC__THIS_DIR_PREJ;
        }
      else
        svn_dirent_split(&marker_dir, &marker_name, local_abspath,
                         scratch_pool);

      SVN_ERR(svn_io_open_uniquely_named(NULL, &marker_abspath,
                                         marker_dir,
                                         marker_name,
                                         SVN_WC__PROP_REJ_EXT,
                                         svn_io_file_del_none,
                                         scratch_pool, scratch_pool));

      SVN_ERR(svn_wc__db_to_relpath(&marker_relpath, db, local_abspath,
                                    marker_abspath, result_pool, result_pool));

      /* And store the marker in the skel */
      {
        svn_skel_t *prop_conflict;
        SVN_ERR(conflict__get_conflict(&prop_conflict, conflict_skel,
                                       SVN_WC__CONFLICT_KIND_PROP));

        svn_skel__prepend_str(marker_relpath, prop_conflict->children->next,
                            result_pool);
      }

      /* Store the data in the WQ item in the same format used as 1.7.
         Once we store the data in DB it is easier to just read it back
         from the workqueue */
      {
        svn_skel_t *prop_data;
        apr_hash_index_t *hi;
        apr_hash_t *old_props;
        apr_hash_t *mine_props;
        apr_hash_t *their_original_props;
        apr_hash_t *their_props;
        apr_hash_t *conflicted_props;

        SVN_ERR(svn_wc__conflict_read_prop_conflict(NULL,
                                                    &mine_props,
                                                    &their_original_props,
                                                    &their_props,
                                                    &conflicted_props,
                                                    db, local_abspath,
                                                    conflict_skel,
                                                    scratch_pool,
                                                    scratch_pool));

        if (operation == svn_wc_operation_merge)
          SVN_ERR(svn_wc__db_read_pristine_props(&old_props, db, local_abspath,
                                                 scratch_pool, scratch_pool));
        else
          old_props = their_original_props;

        prop_data = prop_conflict_skel_new(result_pool);

        for (hi = apr_hash_first(scratch_pool, conflicted_props);
             hi;
             hi = apr_hash_next(hi))
          {
            const char *propname = svn__apr_hash_index_key(hi);

            SVN_ERR(prop_conflict_skel_add(
                            prop_data, propname,
                            old_props
                                    ? svn_hash_gets(old_props, propname)
                                    : NULL,
                            mine_props
                                    ? svn_hash_gets(mine_props, propname)
                                    : NULL,
                            their_props
                                    ? svn_hash_gets(their_props, propname)
                                      : NULL,
                            their_original_props
                                    ? svn_hash_gets(their_original_props, propname)
                                      : NULL,
                            result_pool, scratch_pool));
          }

        SVN_ERR(svn_wc__wq_build_prej_install(work_items,
                                              db, local_abspath,
                                              prop_data,
                                              scratch_pool, scratch_pool));
      }
    }

  return SVN_NO_ERROR;
}

/* Helper function for the three apply_* functions below, used when
 * merging properties together.
 *
 * Given property PROPNAME on LOCAL_ABSPATH, and four possible property
 * values, generate four tmpfiles and pass them to CONFLICT_FUNC callback.
 * This gives the client an opportunity to interactively resolve the
 * property conflict.
 *
 * BASE_VAL/WORKING_VAL represent the current state of the working
 * copy, and INCOMING_OLD_VAL/INCOMING_NEW_VAL represents the incoming
 * propchange.  Any of these values might be NULL, indicating either
 * non-existence or intent-to-delete.
 *
 * If the callback isn't available, or if it responds with
 * 'choose_postpone', then set *CONFLICT_REMAINS to TRUE and return.
 *
 * If the callback responds with a choice of 'base', 'theirs', 'mine',
 * or 'merged', then install the proper value into ACTUAL_PROPS and
 * set *CONFLICT_REMAINS to FALSE.
 */
static svn_error_t *
generate_propconflict(svn_boolean_t *conflict_remains,
                      svn_wc__db_t *db,
                      const char *local_abspath,
                      svn_wc_operation_t operation,
                      const svn_wc_conflict_version_t *left_version,
                      const svn_wc_conflict_version_t *right_version,
                      const char *propname,
                      const svn_string_t *base_val,
                      const svn_string_t *working_val,
                      const svn_string_t *incoming_old_val,
                      const svn_string_t *incoming_new_val,
                      svn_wc_conflict_resolver_func2_t conflict_func,
                      void *conflict_baton,
                      apr_pool_t *scratch_pool)
{
  svn_wc_conflict_result_t *result = NULL;
  svn_wc_conflict_description2_t *cdesc;
  const char *dirpath = svn_dirent_dirname(local_abspath, scratch_pool);
  svn_node_kind_t kind;
  const svn_string_t *new_value = NULL;

  SVN_ERR(svn_wc__db_read_kind(&kind, db, local_abspath,
                               FALSE /* allow_missing */,
                               FALSE /* show_deleted */,
                               FALSE /* show_hidden */,
                               scratch_pool));

  if (kind == svn_node_none)
    return svn_error_createf(SVN_ERR_WC_PATH_NOT_FOUND, NULL,
                             _("The node '%s' was not found."),
                             svn_dirent_local_style(local_abspath,
                                                    scratch_pool));

  cdesc = svn_wc_conflict_description_create_prop2(
                local_abspath,
                (kind == svn_node_dir) ? svn_node_dir : svn_node_file,
                propname, scratch_pool);

  cdesc->operation = operation;
  cdesc->src_left_version = left_version;
  cdesc->src_right_version = right_version;

  /* Create a tmpfile for each of the string_t's we've got.  */
  if (working_val)
    {
      const char *file_name;

      SVN_ERR(svn_io_write_unique(&file_name, dirpath, working_val->data,
                                  working_val->len,
                                  svn_io_file_del_on_pool_cleanup,
                                  scratch_pool));
      cdesc->my_abspath = svn_dirent_join(dirpath, file_name, scratch_pool);
    }

  if (incoming_new_val)
    {
      const char *file_name;

      SVN_ERR(svn_io_write_unique(&file_name, dirpath, incoming_new_val->data,
                                  incoming_new_val->len,
                                  svn_io_file_del_on_pool_cleanup,
                                  scratch_pool));
      cdesc->their_abspath = svn_dirent_join(dirpath, file_name, scratch_pool);
    }

  if (!base_val && !incoming_old_val)
    {
      /* If base and old are both NULL, then that's fine, we just let
         base_file stay NULL as-is.  Both agents are attempting to add a
         new property.  */
    }

  else if ((base_val && !incoming_old_val)
           || (!base_val && incoming_old_val))
    {
      /* If only one of base and old are defined, then we've got a
         situation where one agent is attempting to add the property
         for the first time, and the other agent is changing a
         property it thinks already exists.  In this case, we return
         whichever older-value happens to be defined, so that the
         conflict-callback can still attempt a 3-way merge. */

      const svn_string_t *conflict_base_val = base_val ? base_val
                                                       : incoming_old_val;
      const char *file_name;

      SVN_ERR(svn_io_write_unique(&file_name, dirpath,
                                  conflict_base_val->data,
                                  conflict_base_val->len,
                                  svn_io_file_del_on_pool_cleanup,
                                  scratch_pool));
      cdesc->base_abspath = svn_dirent_join(dirpath, file_name, scratch_pool);
    }

  else  /* base and old are both non-NULL */
    {
      const svn_string_t *conflict_base_val;
      const char *file_name;

      if (! svn_string_compare(base_val, incoming_old_val))
        {
          /* What happens if 'base' and 'old' don't match up?  In an
             ideal situation, they would.  But if they don't, this is
             a classic example of a patch 'hunk' failing to apply due
             to a lack of context.  For example: imagine that the user
             is busy changing the property from a value of "cat" to
             "dog", but the incoming propchange wants to change the
             same property value from "red" to "green".  Total context
             mismatch.

             HOWEVER: we can still pass one of the two base values as
             'base_file' to the callback anyway.  It's still useful to
             present the working and new values to the user to
             compare. */

          if (working_val && svn_string_compare(base_val, working_val))
            conflict_base_val = incoming_old_val;
          else
            conflict_base_val = base_val;
        }
      else
        {
          conflict_base_val = base_val;
        }

      SVN_ERR(svn_io_write_unique(&file_name, dirpath, conflict_base_val->data,
                                  conflict_base_val->len,
                                  svn_io_file_del_on_pool_cleanup, scratch_pool));
      cdesc->base_abspath = svn_dirent_join(dirpath, file_name, scratch_pool);

      if (working_val && incoming_new_val)
        {
          svn_stream_t *mergestream;
          svn_diff_t *diff;
          svn_diff_file_options_t *options =
            svn_diff_file_options_create(scratch_pool);

          SVN_ERR(svn_stream_open_unique(&mergestream, &cdesc->merged_file,
                                         NULL, svn_io_file_del_on_pool_cleanup,
                                         scratch_pool, scratch_pool));
          SVN_ERR(svn_diff_mem_string_diff3(&diff, conflict_base_val,
                                            working_val,
                                            incoming_new_val, options, scratch_pool));
          SVN_ERR(svn_diff_mem_string_output_merge2
                  (mergestream, diff, conflict_base_val, working_val,
                   incoming_new_val, NULL, NULL, NULL, NULL,
                   svn_diff_conflict_display_modified_latest, scratch_pool));
          SVN_ERR(svn_stream_close(mergestream));
        }
    }

  if (!incoming_old_val && incoming_new_val)
    cdesc->action = svn_wc_conflict_action_add;
  else if (incoming_old_val && !incoming_new_val)
    cdesc->action = svn_wc_conflict_action_delete;
  else
    cdesc->action = svn_wc_conflict_action_edit;

  if (base_val && !working_val)
    cdesc->reason = svn_wc_conflict_reason_deleted;
  else if (!base_val && working_val)
    cdesc->reason = svn_wc_conflict_reason_obstructed;
  else
    cdesc->reason = svn_wc_conflict_reason_edited;

  /* Invoke the interactive conflict callback. */
  {
    SVN_ERR(conflict_func(&result, cdesc, conflict_baton, scratch_pool,
                          scratch_pool));
  }
  if (result == NULL)
    {
      *conflict_remains = TRUE;
      return svn_error_create(SVN_ERR_WC_CONFLICT_RESOLVER_FAILURE,
                              NULL, _("Conflict callback violated API:"
                                      " returned no results"));
    }


  switch (result->choice)
    {
      default:
      case svn_wc_conflict_choose_postpone:
        {
          *conflict_remains = TRUE;
          break;
        }
      case svn_wc_conflict_choose_mine_full:
        {
          /* No need to change actual_props; it already contains working_val */
          *conflict_remains = FALSE;
          new_value = working_val;
          break;
        }
      /* I think _mine_full and _theirs_full are appropriate for prop
         behavior as well as the text behavior.  There should even be
         analogous behaviors for _mine and _theirs when those are
         ready, namely: fold in all non-conflicting prop changes, and
         then choose _mine side or _theirs side for conflicting ones. */
      case svn_wc_conflict_choose_theirs_full:
        {
          *conflict_remains = FALSE;
          new_value = incoming_new_val;
          break;
        }
      case svn_wc_conflict_choose_base:
        {
          *conflict_remains = FALSE;
          new_value = base_val;
          break;
        }
      case svn_wc_conflict_choose_merged:
        {
          svn_stringbuf_t *merged_stringbuf;

          if (!cdesc->merged_file && !result->merged_file)
            return svn_error_create
                (SVN_ERR_WC_CONFLICT_RESOLVER_FAILURE,
                 NULL, _("Conflict callback violated API:"
                         " returned no merged file"));

          SVN_ERR(svn_stringbuf_from_file2(&merged_stringbuf,
                                           result->merged_file ?
                                                result->merged_file :
                                                cdesc->merged_file,
                                           scratch_pool));
          new_value = svn_stringbuf__morph_into_string(merged_stringbuf);
          *conflict_remains = FALSE;
          break;
        }
    }

  if (!*conflict_remains)
    {
      apr_hash_t *props;

      /* For now, just set the property values. This should really do some of the
         more advanced things from svn_wc_prop_set() */

      SVN_ERR(svn_wc__db_read_props(&props, db, local_abspath, scratch_pool,
                                    scratch_pool));

      svn_hash_sets(props, propname, new_value);

      SVN_ERR(svn_wc__db_op_set_props(db, local_abspath, props,
                                      FALSE, NULL, NULL,
                                      scratch_pool));
    }

  return SVN_NO_ERROR;
}

/* Resolve the text conflict on DB/LOCAL_ABSPATH in the manner specified
 * by CHOICE.
 *
 * Set *WORK_ITEMS to new work items that will make the on-disk changes
 * needed to complete the resolution (but not to mark it as resolved).
 * Set *IS_RESOLVED to true if the conflicts are resolved; otherwise
 * (which is only if CHOICE is 'postpone') to false.
 *
 * LEFT_ABSPATH, RIGHT_ABSPATH, and DETRANSLATED_TARGET are the
 * input files to the 3-way merge that will be performed if CHOICE is
 * 'theirs-conflict' or 'mine-conflict'.  LEFT_ABSPATH is also the file
 * that will be used if CHOICE is 'base', and RIGHT_ABSPATH if CHOICE is
 * 'theirs-full'.  MERGED_ABSPATH will be used if CHOICE is 'merged'.
 *
 * DETRANSLATED_TARGET is the detranslated version of 'mine' (see
 * detranslate_wc_file() above).  MERGE_OPTIONS are passed to the
 * diff3 implementation in case a 3-way merge has to be carried out.
 */
static svn_error_t *
eval_text_conflict_func_result(svn_skel_t **work_items,
                               svn_boolean_t *is_resolved,
                               svn_wc__db_t *db,
                               const char *local_abspath,
                               svn_wc_conflict_choice_t choice,
                               const apr_array_header_t *merge_options,
                               const char *left_abspath,
                               const char *right_abspath,
                               const char *merged_abspath,
                               const char *detranslated_target,
                               apr_pool_t *result_pool,
                               apr_pool_t *scratch_pool)
{
  const char *install_from_abspath = NULL;
  svn_boolean_t remove_source = FALSE;

  *work_items = NULL;

  switch (choice)
    {
      /* If the callback wants to use one of the fulltexts
         to resolve the conflict, so be it.*/
      case svn_wc_conflict_choose_base:
        {
          install_from_abspath = left_abspath;
          *is_resolved = TRUE;
          break;
        }
      case svn_wc_conflict_choose_theirs_full:
        {
          install_from_abspath = right_abspath;
          *is_resolved = TRUE;
          break;
        }
      case svn_wc_conflict_choose_mine_full:
        {
          install_from_abspath = detranslated_target;
          *is_resolved = TRUE;
          break;
        }
      case svn_wc_conflict_choose_theirs_conflict:
      case svn_wc_conflict_choose_mine_conflict:
        {
          const char *chosen_abspath;
          const char *temp_dir;
          svn_stream_t *chosen_stream;
          svn_diff_t *diff;
          svn_diff_conflict_display_style_t style;
          svn_diff_file_options_t *diff3_options;

          diff3_options = svn_diff_file_options_create(scratch_pool);

          if (merge_options)
             SVN_ERR(svn_diff_file_options_parse(diff3_options,
                                                 merge_options,
                                                 scratch_pool));

          style = choice == svn_wc_conflict_choose_theirs_conflict
                    ? svn_diff_conflict_display_latest
                    : svn_diff_conflict_display_modified;

          SVN_ERR(svn_wc__db_temp_wcroot_tempdir(&temp_dir, db,
                                                 local_abspath,
                                                 scratch_pool, scratch_pool));
          SVN_ERR(svn_stream_open_unique(&chosen_stream, &chosen_abspath,
                                         temp_dir, svn_io_file_del_none,
                                         scratch_pool, scratch_pool));

          SVN_ERR(svn_diff_file_diff3_2(&diff,
                                        left_abspath,
                                        detranslated_target, right_abspath,
                                        diff3_options, scratch_pool));
          SVN_ERR(svn_diff_file_output_merge2(chosen_stream, diff,
                                              left_abspath,
                                              detranslated_target,
                                              right_abspath,
                                              /* markers ignored */
                                              NULL, NULL,
                                              NULL, NULL,
                                              style,
                                              scratch_pool));
          SVN_ERR(svn_stream_close(chosen_stream));

          install_from_abspath = chosen_abspath;
          remove_source = TRUE;
          *is_resolved = TRUE;
          break;
        }

        /* For the case of 3-way file merging, we don't
           really distinguish between these return values;
           if the callback claims to have "generally
           resolved" the situation, we still interpret
           that as "OK, we'll assume the merged version is
           good to use". */
      case svn_wc_conflict_choose_merged:
        {
          install_from_abspath = merged_abspath;
          *is_resolved = TRUE;
          break;
        }
      case svn_wc_conflict_choose_postpone:
      default:
        {
          /* Assume conflict remains. */
          *is_resolved = FALSE;
          return SVN_NO_ERROR;
        }
    }

  if (install_from_abspath == NULL)
    return svn_error_createf(SVN_ERR_WC_CONFLICT_RESOLVER_FAILURE, NULL,
                             _("Conflict on '%s' could not be resolved "
                               "because the chosen version of the file "
                               "is not available."),
                             svn_dirent_local_style(local_abspath,
                                                    scratch_pool));

  {
    svn_skel_t *work_item;

    SVN_ERR(svn_wc__wq_build_file_install(&work_item,
                                          db, local_abspath,
                                          install_from_abspath,
                                          FALSE /* use_commit_times */,
                                          FALSE /* record_fileinfo */,
                                          result_pool, scratch_pool));
    *work_items = svn_wc__wq_merge(*work_items, work_item, result_pool);

    SVN_ERR(svn_wc__wq_build_sync_file_flags(&work_item, db, local_abspath,
                                             result_pool, scratch_pool));
    *work_items = svn_wc__wq_merge(*work_items, work_item, result_pool);

    if (remove_source)
      {
        SVN_ERR(svn_wc__wq_build_file_remove(&work_item,
                                             db, local_abspath,
                                             install_from_abspath,
                                             result_pool, scratch_pool));
        *work_items = svn_wc__wq_merge(*work_items, work_item, result_pool);
      }
  }

  return SVN_NO_ERROR;
}


/* Create a new file in the same directory as LOCAL_ABSPATH, with the
   same basename as LOCAL_ABSPATH, with a ".edited" extension, and set
   *WORK_ITEM to a new work item that will copy and translate from the file
   SOURCE_ABSPATH to that new file.  It will be translated from repository-
   normal form to working-copy form according to the versioned properties
   of LOCAL_ABSPATH that are current when the work item is executed.

   DB should have a write lock for the directory containing SOURCE.

   Allocate *WORK_ITEM in RESULT_POOL. */
static svn_error_t *
save_merge_result(svn_skel_t **work_item,
                  svn_wc__db_t *db,
                  const char *local_abspath,
                  const char *source_abspath,
                  apr_pool_t *result_pool,
                  apr_pool_t *scratch_pool)
{
  const char *edited_copy_abspath;
  const char *dir_abspath;
  const char *filename;

  svn_dirent_split(&dir_abspath, &filename, local_abspath, scratch_pool);

  /* ### Should use preserved-conflict-file-exts. */
  /* Create the .edited file within this file's DIR_ABSPATH  */
  SVN_ERR(svn_io_open_uniquely_named(NULL,
                                     &edited_copy_abspath,
                                     dir_abspath,
                                     filename,
                                     ".edited",
                                     svn_io_file_del_none,
                                     scratch_pool, scratch_pool));
  SVN_ERR(svn_wc__wq_build_file_copy_translated(work_item,
                                                db, local_abspath,
                                                source_abspath,
                                                edited_copy_abspath,
                                                result_pool, scratch_pool));
  return SVN_NO_ERROR;
}


/* Call the conflict resolver callback for a text conflict, and resolve
 * the conflict if it tells us to do so.
 *
 * Assume that there is a text conflict on the path DB/LOCAL_ABSPATH.
 *
 * Call CONFLICT_FUNC with CONFLICT_BATON to find out whether and how
 * it wants to resolve the conflict.  Pass it a conflict description
 * containing OPERATION, LEFT/RIGHT_ABSPATH, LEFT/RIGHT_VERSION,
 * RESULT_TARGET and DETRANSLATED_TARGET.
 *
 * If the callback returns a resolution other than 'postpone', then
 * perform that requested resolution and prepare to mark the conflict
 * as resolved.
 *
 * Return *WORK_ITEMS that will do the on-disk work required to complete
 * the resolution (but not to mark the conflict as resolved), and set
 * *WAS_RESOLVED to true, if it was resolved.  Set *WORK_ITEMS to NULL
 * and *WAS_RESOLVED to FALSE otherwise.
 *
 * RESULT_TARGET is the path to the merged file produced by the internal
 * or external 3-way merge, which may contain conflict markers, in
 * repository normal form.  DETRANSLATED_TARGET is the 'mine' version of
 * the file, also in RNF.
 */
static svn_error_t *
resolve_text_conflict(svn_skel_t **work_items,
                      svn_boolean_t *was_resolved,
                      svn_wc__db_t *db,
                      const char *local_abspath,
                      const apr_array_header_t *merge_options,
                      svn_wc_operation_t operation,
                      const char *left_abspath,
                      const char *right_abspath,
                      const svn_wc_conflict_version_t *left_version,
                      const svn_wc_conflict_version_t *right_version,
                      const char *result_target,
                      const char *detranslated_target,
                      svn_wc_conflict_resolver_func2_t conflict_func,
                      void *conflict_baton,
                      apr_pool_t *result_pool,
                      apr_pool_t *scratch_pool)
{
  svn_wc_conflict_result_t *result;
  svn_skel_t *work_item;
  svn_wc_conflict_description2_t *cdesc;
  apr_hash_t *props;
  const char *mime_type;

  *work_items = NULL;
  *was_resolved = FALSE;

  /* Give the conflict resolution callback a chance to clean
     up the conflicts before we mark the file 'conflicted' */

  SVN_ERR(svn_wc__db_read_props(&props, db, local_abspath,
                              scratch_pool, scratch_pool));

  cdesc = svn_wc_conflict_description_create_text2(local_abspath,
                                                   scratch_pool);
  mime_type = svn_prop_get_value(props, SVN_PROP_MIME_TYPE);
  cdesc->is_binary = mime_type ? svn_mime_type_is_binary(mime_type) : FALSE;
  cdesc->mime_type = mime_type;
  cdesc->base_abspath = left_abspath;
  cdesc->their_abspath = right_abspath;
  cdesc->my_abspath = detranslated_target;
  cdesc->merged_file = result_target;
  cdesc->operation = operation;
  cdesc->src_left_version = left_version;
  cdesc->src_right_version = right_version;

  SVN_ERR(conflict_func(&result, cdesc, conflict_baton, scratch_pool,
                        scratch_pool));
  if (result == NULL)
    return svn_error_create(SVN_ERR_WC_CONFLICT_RESOLVER_FAILURE, NULL,
                            _("Conflict callback violated API:"
                              " returned no results"));

  if (result->save_merged)
    {
      SVN_ERR(save_merge_result(work_items,
                                db, local_abspath,
                                /* Look for callback's own
                                    merged-file first: */
                                result->merged_file
                                  ? result->merged_file
                                  : result_target,
                                result_pool, scratch_pool));
    }

  if (result->choice != svn_wc_conflict_choose_postpone)
    {
      SVN_ERR(eval_text_conflict_func_result(&work_item,
                                             was_resolved,
                                             db, local_abspath,
                                             result->choice,
                                             merge_options,
                                             left_abspath,
                                             right_abspath,
                                             /* ### Sure this is an abspath? */
                                             result->merged_file
                                               ? result->merged_file
                                               : result_target,
                                             detranslated_target,
                                             result_pool, scratch_pool));
      *work_items = svn_wc__wq_merge(*work_items, work_item, result_pool);
    }
  else
    *was_resolved = FALSE;

  return SVN_NO_ERROR;
}


static svn_error_t *
setup_tree_conflict_desc(svn_wc_conflict_description2_t **desc,
                         svn_wc__db_t *db,
                         const char *local_abspath,
                         svn_wc_operation_t operation,
                         const svn_wc_conflict_version_t *left_version,
                         const svn_wc_conflict_version_t *right_version,
                         svn_wc_conflict_reason_t local_change,
                         svn_wc_conflict_action_t incoming_change,
                         apr_pool_t *result_pool,
                         apr_pool_t *scratch_pool)
{
  svn_node_kind_t tc_kind;

  if (left_version)
    tc_kind = left_version->node_kind;
  else if (right_version)
    tc_kind = right_version->node_kind;
  else
    tc_kind = svn_node_file; /* Avoid assertion */

  *desc = svn_wc_conflict_description_create_tree2(local_abspath, tc_kind,
                                                   operation,
                                                   left_version, right_version,
                                                   result_pool);
  (*desc)->reason = local_change;
  (*desc)->action = incoming_change;

  return SVN_NO_ERROR;
}


svn_error_t *
svn_wc__conflict_invoke_resolver(svn_wc__db_t *db,
                                 const char *local_abspath,
                                 const svn_skel_t *conflict_skel,
                                 const apr_array_header_t *merge_options,
                                 svn_wc_conflict_resolver_func2_t resolver_func,
                                 void *resolver_baton,
                                 svn_cancel_func_t cancel_func,
                                 void *cancel_baton,
                                 apr_pool_t *scratch_pool)
{
  svn_boolean_t text_conflicted;
  svn_boolean_t prop_conflicted;
  svn_boolean_t tree_conflicted;
  svn_wc_operation_t operation;
  const apr_array_header_t *locations;
  const svn_wc_conflict_version_t *left_version = NULL;
  const svn_wc_conflict_version_t *right_version = NULL;

  SVN_ERR(svn_wc__conflict_read_info(&operation, &locations,
                                     &text_conflicted, &prop_conflicted,
                                     &tree_conflicted,
                                     db, local_abspath, conflict_skel,
                                     scratch_pool, scratch_pool));

  if (locations && locations->nelts > 0)
    left_version = APR_ARRAY_IDX(locations, 0, const svn_wc_conflict_version_t *);

  if (locations && locations->nelts > 1)
    right_version = APR_ARRAY_IDX(locations, 1, const svn_wc_conflict_version_t *);

  /* Quick and dirty compatibility wrapper. My guess would be that most resolvers
     would want to look at all properties at the same time.

     ### svn currently only invokes this from the merge code to collect the list of
     ### conflicted paths. Eventually this code will be the base for 'svn resolve'
     ### and at that time the test coverage will improve
     */
  if (prop_conflicted)
    {
      apr_hash_t *old_props;
      apr_hash_t *mine_props;
      apr_hash_t *their_props;
      apr_hash_t *old_their_props;
      apr_hash_t *conflicted;
      apr_pool_t *iterpool;
      apr_hash_index_t *hi;
      svn_boolean_t mark_resolved = TRUE;

      SVN_ERR(svn_wc__conflict_read_prop_conflict(NULL,
                                                  &mine_props,
                                                  &old_their_props,
                                                  &their_props,
                                                  &conflicted,
                                                  db, local_abspath,
                                                  conflict_skel,
                                                  scratch_pool, scratch_pool));

      if (operation == svn_wc_operation_merge)
        SVN_ERR(svn_wc__db_read_pristine_props(&old_props, db, local_abspath,
                                               scratch_pool, scratch_pool));
      else
        old_props = old_their_props;

      iterpool = svn_pool_create(scratch_pool);

      for (hi = apr_hash_first(scratch_pool, conflicted);
           hi;
           hi = apr_hash_next(hi))
        {
          const char *propname = svn__apr_hash_index_key(hi);
          svn_boolean_t conflict_remains = TRUE;

          svn_pool_clear(iterpool);

          if (cancel_func)
            SVN_ERR(cancel_func(cancel_baton));

          SVN_ERR(generate_propconflict(&conflict_remains,
                                        db, local_abspath,
                                        operation,
                                        left_version,
                                        right_version,
                                        propname,
                                        old_props
                                          ? svn_hash_gets(old_props, propname)
                                          : NULL,
                                        mine_props
                                          ? svn_hash_gets(mine_props, propname)
                                          : NULL,
                                        old_their_props
                                          ? svn_hash_gets(old_their_props, propname)
                                          : NULL,
                                        their_props
                                          ? svn_hash_gets(their_props, propname)
                                          : NULL,
                                        resolver_func, resolver_baton,
                                        iterpool));

          if (conflict_remains)
            mark_resolved = FALSE;
        }

      if (mark_resolved)
        {
          SVN_ERR(svn_wc__mark_resolved_prop_conflicts(db, local_abspath,
                                                       scratch_pool));
        }
    }

  if (text_conflicted)
    {
      const char *mine_abspath;
      const char *their_original_abspath;
      const char *their_abspath;
      svn_skel_t *work_items;
      svn_boolean_t was_resolved;

      SVN_ERR(svn_wc__conflict_read_text_conflict(&mine_abspath,
                                                  &their_original_abspath,
                                                  &their_abspath,
                                                  db, local_abspath,
                                                  conflict_skel,
                                                  scratch_pool, scratch_pool));

      SVN_ERR(resolve_text_conflict(&work_items, &was_resolved,
                                    db, local_abspath,
                                    merge_options,
                                    operation,
                                    their_original_abspath, their_abspath,
                                    left_version, right_version,
                                    local_abspath, mine_abspath,
                                    resolver_func, resolver_baton,
                                    scratch_pool, scratch_pool));

      if (was_resolved)
        {
          if (work_items)
            {
              SVN_ERR(svn_wc__db_wq_add(db, local_abspath, work_items,
                                        scratch_pool));
              SVN_ERR(svn_wc__wq_run(db, local_abspath,
                                     cancel_func, cancel_baton,
                                     scratch_pool));
            }
          SVN_ERR(svn_wc__mark_resolved_text_conflict(db, local_abspath,
                                                      scratch_pool));
        }
    }

  if (tree_conflicted)
    {
      svn_wc_conflict_reason_t local_change;
      svn_wc_conflict_action_t incoming_change;
      svn_wc_conflict_result_t *result;
      svn_wc_conflict_description2_t *desc;

      SVN_ERR(svn_wc__conflict_read_tree_conflict(&local_change,
                                                  &incoming_change,
                                                  NULL,
                                                  db, local_abspath,
                                                  conflict_skel,
                                                  scratch_pool, scratch_pool));

      SVN_ERR(setup_tree_conflict_desc(&desc,
                                       db, local_abspath,
                                       operation, left_version, right_version,
                                       local_change, incoming_change,
                                       scratch_pool, scratch_pool));

      /* Tell the resolver func about this conflict. */
      SVN_ERR(resolver_func(&result, desc, resolver_baton, scratch_pool,
                            scratch_pool));

      /* Ignore the result. We cannot apply it here since this code runs
       * during an update or merge operation. Tree conflicts are always
       * postponed and resolved after the operation has completed. */
    }

  return SVN_NO_ERROR;
}

/* Read all property conflicts contained in CONFLICT_SKEL into
 * individual conflict descriptions, and append those descriptions
 * to the CONFLICTS array.
 *
 * If NOT create_tempfiles, always create a legacy property conflict
 * descriptor.
 *
 * Use NODE_KIND, OPERATION and shallow copies of LEFT_VERSION and
 * RIGHT_VERSION, rather than reading them from CONFLICT_SKEL.
 *
 * Allocate results in RESULT_POOL. SCRATCH_POOL is used for temporary
 * allocations. */
static svn_error_t *
read_prop_conflicts(apr_array_header_t *conflicts,
                    svn_wc__db_t *db,
                    const char *local_abspath,
                    svn_skel_t *conflict_skel,
                    svn_boolean_t create_tempfiles,
                    svn_node_kind_t node_kind,
                    svn_wc_operation_t operation,
                    const svn_wc_conflict_version_t *left_version,
                    const svn_wc_conflict_version_t *right_version,
                    apr_pool_t *result_pool,
                    apr_pool_t *scratch_pool)
{
  const char *prop_reject_file;
  apr_hash_t *my_props;
  apr_hash_t *their_old_props;
  apr_hash_t *their_props;
  apr_hash_t *conflicted_props;
  apr_hash_index_t *hi;
  apr_pool_t *iterpool;

  SVN_ERR(svn_wc__conflict_read_prop_conflict(&prop_reject_file,
                                              &my_props,
                                              &their_old_props,
                                              &their_props,
                                              &conflicted_props,
                                              db, local_abspath,
                                              conflict_skel,
                                              scratch_pool, scratch_pool));

  if ((! create_tempfiles) || apr_hash_count(conflicted_props) == 0)
    {
      /* Legacy prop conflict with only a .reject file. */
      svn_wc_conflict_description2_t *desc;

      desc  = svn_wc_conflict_description_create_prop2(local_abspath,
                                                       node_kind,
                                                       "", result_pool);

      /* ### This should be changed. The prej file should be stored
       * ### separately from the other files. We need to rev the
       * ### conflict description struct for this. */
      desc->their_abspath = apr_pstrdup(result_pool, prop_reject_file);

      desc->operation = operation;
      desc->src_left_version = left_version;
      desc->src_right_version = right_version;

      APR_ARRAY_PUSH(conflicts, svn_wc_conflict_description2_t*) = desc;

      return SVN_NO_ERROR;
    }

  iterpool = svn_pool_create(scratch_pool);
  for (hi = apr_hash_first(scratch_pool, conflicted_props);
       hi;
       hi = apr_hash_next(hi))
    {
      const char *propname = svn__apr_hash_index_key(hi);
      svn_string_t *old_value;
      svn_string_t *my_value;
      svn_string_t *their_value;
      svn_wc_conflict_description2_t *desc;

      svn_pool_clear(iterpool);

      desc  = svn_wc_conflict_description_create_prop2(local_abspath,
                                                       node_kind,
                                                       propname,
                                                       result_pool);

      desc->operation = operation;
      desc->src_left_version = left_version;
      desc->src_right_version = right_version;

      desc->property_name = apr_pstrdup(result_pool, propname);

      my_value = svn_hash_gets(my_props, propname);
      their_value = svn_hash_gets(their_props, propname);
      old_value = svn_hash_gets(their_old_props, propname);

      /* Compute the incoming side of the conflict ('action'). */
      if (their_value == NULL)
        desc->action = svn_wc_conflict_action_delete;
      else if (old_value == NULL)
        desc->action = svn_wc_conflict_action_add;
      else
        desc->action = svn_wc_conflict_action_edit;

      /* Compute the local side of the conflict ('reason'). */
      if (my_value == NULL)
        desc->reason = svn_wc_conflict_reason_deleted;
      else if (old_value == NULL)
        desc->reason = svn_wc_conflict_reason_added;
      else
        desc->reason = svn_wc_conflict_reason_edited;

      /* ### This should be changed. The prej file should be stored
       * ### separately from the other files. We need to rev the
       * ### conflict description struct for this. */
      desc->their_abspath = apr_pstrdup(result_pool, prop_reject_file);

      /* ### This should be changed. The conflict description for
       * ### props should contain these values as svn_string_t,
       * ### rather than in temporary files. We need to rev the
       * ### conflict description struct for this. */
      if (my_value)
        {
          svn_stream_t *s;
          apr_size_t len;

          SVN_ERR(svn_stream_open_unique(&s, &desc->my_abspath, NULL,
                                         svn_io_file_del_on_pool_cleanup,
                                         result_pool, iterpool));
          len = my_value->len;
          SVN_ERR(svn_stream_write(s, my_value->data, &len));
          SVN_ERR(svn_stream_close(s));
        }

      if (their_value)
        {
          svn_stream_t *s;
          apr_size_t len;

          /* ### Currently, their_abspath is used for the prop reject file.
           * ### Put their value into merged instead...
           * ### We need to rev the conflict description struct to fix this. */
          SVN_ERR(svn_stream_open_unique(&s, &desc->merged_file, NULL,
                                         svn_io_file_del_on_pool_cleanup,
                                         result_pool, iterpool));
          len = their_value->len;
          SVN_ERR(svn_stream_write(s, their_value->data, &len));
          SVN_ERR(svn_stream_close(s));
        }

      if (old_value)
        {
          svn_stream_t *s;
          apr_size_t len;

          SVN_ERR(svn_stream_open_unique(&s, &desc->base_abspath, NULL,
                                         svn_io_file_del_on_pool_cleanup,
                                         result_pool, iterpool));
          len = old_value->len;
          SVN_ERR(svn_stream_write(s, old_value->data, &len));
          SVN_ERR(svn_stream_close(s));
        }

      APR_ARRAY_PUSH(conflicts, svn_wc_conflict_description2_t*) = desc;
    }
  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__read_conflicts(const apr_array_header_t **conflicts,
                       svn_wc__db_t *db,
                       const char *local_abspath,
                       svn_boolean_t create_tempfiles,
                       apr_pool_t *result_pool,
                       apr_pool_t *scratch_pool)
{
  svn_skel_t *conflict_skel;
  apr_array_header_t *cflcts;
  svn_boolean_t prop_conflicted;
  svn_boolean_t text_conflicted;
  svn_boolean_t tree_conflicted;
  svn_wc_operation_t operation;
  const apr_array_header_t *locations;
  const svn_wc_conflict_version_t *left_version = NULL;
  const svn_wc_conflict_version_t *right_version = NULL;

  SVN_ERR(svn_wc__db_read_conflict(&conflict_skel, db, local_abspath,
                                   scratch_pool, scratch_pool));

  if (!conflict_skel)
    {
      /* Some callers expect not NULL */
      *conflicts = apr_array_make(result_pool, 0,
                                  sizeof(svn_wc_conflict_description2_t*));;
      return SVN_NO_ERROR;
    }

  SVN_ERR(svn_wc__conflict_read_info(&operation, &locations, &text_conflicted,
                                     &prop_conflicted, &tree_conflicted,
                                     db, local_abspath, conflict_skel,
                                     result_pool, scratch_pool));

  cflcts = apr_array_make(result_pool, 4,
                          sizeof(svn_wc_conflict_description2_t*));

  if (locations && locations->nelts > 0)
    left_version = APR_ARRAY_IDX(locations, 0, const svn_wc_conflict_version_t *);
  if (locations && locations->nelts > 1)
    right_version = APR_ARRAY_IDX(locations, 1, const svn_wc_conflict_version_t *);

  if (prop_conflicted)
    {
      svn_node_kind_t node_kind
        = left_version ? left_version->node_kind : svn_node_unknown;

      SVN_ERR(read_prop_conflicts(cflcts, db, local_abspath, conflict_skel,
                                  create_tempfiles, node_kind,
                                  operation, left_version, right_version,
                                  result_pool, scratch_pool));
    }

  if (text_conflicted)
    {
      apr_hash_t *props;
      const char *mime_type;
      svn_wc_conflict_description2_t *desc;
      desc  = svn_wc_conflict_description_create_text2(local_abspath,
                                                       result_pool);

      desc->operation = operation;
      desc->src_left_version = left_version;
      desc->src_right_version = right_version;

      SVN_ERR(svn_wc__db_read_props(&props, db, local_abspath,
                                    scratch_pool, scratch_pool));
      mime_type = svn_prop_get_value(props, SVN_PROP_MIME_TYPE);
      desc->is_binary = mime_type ? svn_mime_type_is_binary(mime_type) : FALSE;
      desc->mime_type = mime_type;

      SVN_ERR(svn_wc__conflict_read_text_conflict(&desc->my_abspath,
                                                  &desc->base_abspath,
                                                  &desc->their_abspath,
                                                  db, local_abspath,
                                                  conflict_skel,
                                                  result_pool, scratch_pool));

      desc->merged_file = apr_pstrdup(result_pool, local_abspath);

      APR_ARRAY_PUSH(cflcts, svn_wc_conflict_description2_t*) = desc;
    }

  if (tree_conflicted)
    {
      svn_wc_conflict_reason_t local_change;
      svn_wc_conflict_action_t incoming_change;
      svn_wc_conflict_description2_t *desc;

      SVN_ERR(svn_wc__conflict_read_tree_conflict(&local_change,
                                                  &incoming_change,
                                                  NULL,
                                                  db, local_abspath,
                                                  conflict_skel,
                                                  scratch_pool, scratch_pool));

      SVN_ERR(setup_tree_conflict_desc(&desc,
                                       db, local_abspath,
                                       operation, left_version, right_version,
                                       local_change, incoming_change,
                                       result_pool, scratch_pool));

      APR_ARRAY_PUSH(cflcts, const svn_wc_conflict_description2_t *) = desc;
    }

  *conflicts = cflcts;
  return SVN_NO_ERROR;
}


/*** Resolving a conflict automatically ***/

/* Prepare to delete an artifact file at ARTIFACT_FILE_ABSPATH in the
 * working copy at DB/WRI_ABSPATH.
 *
 * Set *WORK_ITEMS to a new work item that, when run, will delete the
 * artifact file; or to NULL if there is no file to delete.
 *
 * Set *FILE_FOUND to TRUE if the artifact file is found on disk and its
 * node kind is 'file'; otherwise do not change *FILE_FOUND.  FILE_FOUND
 * may be NULL if not required.
 */
static svn_error_t *
remove_artifact_file_if_exists(svn_skel_t **work_items,
                               svn_boolean_t *file_found,
                               svn_wc__db_t *db,
                               const char *wri_abspath,
                               const char *artifact_file_abspath,
                               apr_pool_t *result_pool,
                               apr_pool_t *scratch_pool)
{
  *work_items = NULL;
  if (artifact_file_abspath)
    {
      svn_node_kind_t node_kind;

      SVN_ERR(svn_io_check_path(artifact_file_abspath, &node_kind,
                                scratch_pool));
      if (node_kind == svn_node_file)
        {
          SVN_ERR(svn_wc__wq_build_file_remove(work_items,
                                               db, wri_abspath,
                                               artifact_file_abspath,
                                               result_pool, scratch_pool));
          if (file_found)
            *file_found = TRUE;
        }
    }

  return SVN_NO_ERROR;
}

/*
 * Resolve the text conflict found in DB/LOCAL_ABSPATH according
 * to CONFLICT_CHOICE.
 *
 * It is not an error if there is no text conflict. If a text conflict
 * existed and was resolved, set *DID_RESOLVE to TRUE, else set it to FALSE.
 *
 * Note: When there are no conflict markers to remove there is no existing
 * text conflict; just a database containing old information, which we should
 * remove to avoid checking all the time. Resolving a text conflict by
 * removing all the marker files is a fully supported scenario since
 * Subversion 1.0.
 */
static svn_error_t *
resolve_text_conflict_on_node(svn_boolean_t *did_resolve,
                              svn_wc__db_t *db,
                              const char *local_abspath,
                              svn_wc_conflict_choice_t conflict_choice,
                              const char *merged_file,
                              svn_cancel_func_t cancel_func,
                              void *cancel_baton,
                              apr_pool_t *scratch_pool)
{
  const char *conflict_old = NULL;
  const char *conflict_new = NULL;
  const char *conflict_working = NULL;
  const char *auto_resolve_src;
  svn_skel_t *work_item;
  svn_skel_t *work_items = NULL;
  svn_skel_t *conflicts;
  svn_wc_operation_t operation;
  svn_boolean_t text_conflicted;

  *did_resolve = FALSE;

  SVN_ERR(svn_wc__db_read_conflict(&conflicts, db, local_abspath,
                                   scratch_pool, scratch_pool));
  if (!conflicts)
    return SVN_NO_ERROR;

  SVN_ERR(svn_wc__conflict_read_info(&operation, NULL, &text_conflicted,
                                     NULL, NULL, db, local_abspath, conflicts,
                                     scratch_pool, scratch_pool));
  if (!text_conflicted)
    return SVN_NO_ERROR;

  SVN_ERR(svn_wc__conflict_read_text_conflict(&conflict_working,
                                              &conflict_old,
                                              &conflict_new,
                                              db, local_abspath, conflicts,
                                              scratch_pool, scratch_pool));

  /* Handle automatic conflict resolution before the temporary files are
   * deleted, if necessary. */
  switch (conflict_choice)
    {
    case svn_wc_conflict_choose_base:
      auto_resolve_src = conflict_old;
      break;
    case svn_wc_conflict_choose_mine_full:
      auto_resolve_src = conflict_working;
      break;
    case svn_wc_conflict_choose_theirs_full:
      auto_resolve_src = conflict_new;
      break;
    case svn_wc_conflict_choose_merged:
      auto_resolve_src = merged_file;
      break;
    case svn_wc_conflict_choose_theirs_conflict:
    case svn_wc_conflict_choose_mine_conflict:
      {
        if (conflict_old && conflict_working && conflict_new)
          {
            const char *temp_dir;
            svn_stream_t *tmp_stream = NULL;
            svn_diff_t *diff;
            svn_diff_conflict_display_style_t style =
              conflict_choice == svn_wc_conflict_choose_theirs_conflict
              ? svn_diff_conflict_display_latest
              : svn_diff_conflict_display_modified;

            SVN_ERR(svn_wc__db_temp_wcroot_tempdir(&temp_dir, db,
                                                   local_abspath,
                                                   scratch_pool,
                                                   scratch_pool));
            SVN_ERR(svn_stream_open_unique(&tmp_stream,
                                           &auto_resolve_src,
                                           temp_dir,
                                           svn_io_file_del_on_pool_cleanup,
                                           scratch_pool, scratch_pool));

            SVN_ERR(svn_diff_file_diff3_2(&diff,
                                          conflict_old,
                                          conflict_working,
                                          conflict_new,
                                          svn_diff_file_options_create(
                                            scratch_pool),
                                          scratch_pool));
            SVN_ERR(svn_diff_file_output_merge2(tmp_stream, diff,
                                                conflict_old,
                                                conflict_working,
                                                conflict_new,
                                                /* markers ignored */
                                                NULL, NULL, NULL, NULL,
                                                style,
                                                scratch_pool));
            SVN_ERR(svn_stream_close(tmp_stream));
          }
        else
          auto_resolve_src = NULL;
        break;
      }
    default:
      return svn_error_create(SVN_ERR_INCORRECT_PARAMS, NULL,
                              _("Invalid 'conflict_result' argument"));
    }

  if (auto_resolve_src)
    {
      SVN_ERR(svn_wc__wq_build_file_copy_translated(
                &work_item, db, local_abspath,
                auto_resolve_src, local_abspath, scratch_pool, scratch_pool));
      work_items = svn_wc__wq_merge(work_items, work_item, scratch_pool);

      SVN_ERR(svn_wc__wq_build_sync_file_flags(&work_item, db,
                                               local_abspath,
                                               scratch_pool, scratch_pool));
      work_items = svn_wc__wq_merge(work_items, work_item, scratch_pool);
    }

  /* Legacy behavior: Only report text conflicts as resolved when at least
     one conflict marker file exists.

     If not the UI shows the conflict as already resolved
     (and in this case we just remove the in-db conflict) */

  SVN_ERR(remove_artifact_file_if_exists(&work_item, did_resolve,
                                         db, local_abspath, conflict_old,
                                         scratch_pool, scratch_pool));
  work_items = svn_wc__wq_merge(work_items, work_item, scratch_pool);

  SVN_ERR(remove_artifact_file_if_exists(&work_item, did_resolve,
                                         db, local_abspath, conflict_new,
                                         scratch_pool, scratch_pool));
  work_items = svn_wc__wq_merge(work_items, work_item, scratch_pool);

  SVN_ERR(remove_artifact_file_if_exists(&work_item, did_resolve,
                                         db, local_abspath, conflict_working,
                                         scratch_pool, scratch_pool));
  work_items = svn_wc__wq_merge(work_items, work_item, scratch_pool);

  SVN_ERR(svn_wc__db_op_mark_resolved(db, local_abspath,
                                      TRUE, FALSE, FALSE,
                                      work_items, scratch_pool));
  SVN_ERR(svn_wc__wq_run(db, local_abspath, cancel_func, cancel_baton,
                         scratch_pool));

  return SVN_NO_ERROR;
}

/*
 * Resolve the property conflicts found in DB/LOCAL_ABSPATH according
 * to CONFLICT_CHOICE.
 *
 * It is not an error if there is no prop conflict. If a prop conflict
 * existed and was resolved, set *DID_RESOLVE to TRUE, else set it to FALSE.
 *
 * Note: When there are no conflict markers on-disk to remove there is
 * no existing text conflict (unless we are still in the process of
 * creating the text conflict and we didn't register a marker file yet).
 * In this case the database contains old information, which we should
 * remove to avoid checking the next time. Resolving a property conflict
 * by just removing the marker file is a fully supported scenario since
 * Subversion 1.0.
 *
 * ### TODO [JAF] The '*_full' and '*_conflict' choices should differ.
 *     In my opinion, 'mine_full'/'theirs_full' should select
 *     the entire set of properties from 'mine' or 'theirs' respectively,
 *     while 'mine_conflict'/'theirs_conflict' should select just the
 *     properties that are in conflict.  Or, '_full' should select the
 *     entire property whereas '_conflict' should do a text merge within
 *     each property, selecting hunks.  Or all three kinds of behaviour
 *     should be available (full set of props, full value of conflicting
 *     props, or conflicting text hunks).
 * ### BH: If we make *_full select the full set of properties, we should
 *     check if we shouldn't make it also select the full text for files.
 *
 * ### TODO [JAF] All this complexity should not be down here in libsvn_wc
 *     but in a layer above.
 *
 * ### TODO [JAF] Options for 'base' should be like options for 'mine' and
 *     for 'theirs' -- choose full set of props, full value of conflicting
 *     props, or conflicting text hunks.
 *
 */
static svn_error_t *
resolve_prop_conflict_on_node(svn_boolean_t *did_resolve,
                              svn_wc__db_t *db,
                              const char *local_abspath,
                              const char *conflicted_propname,
                              svn_wc_conflict_choice_t conflict_choice,
                              const char *merged_file,
                              svn_cancel_func_t cancel_func,
                              void *cancel_baton,
                              apr_pool_t *scratch_pool)
{
  const char *prop_reject_file;
  apr_hash_t *mine_props;
  apr_hash_t *their_old_props;
  apr_hash_t *their_props;
  apr_hash_t *conflicted_props;
  apr_hash_t *old_props;
  apr_hash_t *resolve_from = NULL;
  svn_skel_t *work_items = NULL;
  svn_skel_t *conflicts;
  svn_wc_operation_t operation;
  svn_boolean_t prop_conflicted;

  *did_resolve = FALSE;

  SVN_ERR(svn_wc__db_read_conflict(&conflicts, db, local_abspath,
                                   scratch_pool, scratch_pool));

  if (!conflicts)
    return SVN_NO_ERROR;

  SVN_ERR(svn_wc__conflict_read_info(&operation, NULL, NULL, &prop_conflicted,
                                     NULL, db, local_abspath, conflicts,
                                     scratch_pool, scratch_pool));
  if (!prop_conflicted)
    return SVN_NO_ERROR;

  SVN_ERR(svn_wc__conflict_read_prop_conflict(&prop_reject_file,
                                              &mine_props, &their_old_props,
                                              &their_props, &conflicted_props,
                                              db, local_abspath, conflicts,
                                              scratch_pool, scratch_pool));

  if (operation == svn_wc_operation_merge)
      SVN_ERR(svn_wc__db_read_pristine_props(&old_props, db, local_abspath,
                                             scratch_pool, scratch_pool));
    else
      old_props = their_old_props;

  /* We currently handle *_conflict as *_full as this argument is currently
     always applied for all conflicts on a node at the same time. Giving
     an error would break some tests that assumed that this would just
     resolve property conflicts to working.

     An alternative way to handle these conflicts would be to just copy all
     property state from mine/theirs on the _full option instead of just the
     conflicted properties. In some ways this feels like a sensible option as
     that would take both properties and text from mine/theirs, but when not
     both properties and text are conflicted we would fail in doing so.
   */
  switch (conflict_choice)
    {
    case svn_wc_conflict_choose_base:
      resolve_from = their_old_props ? their_old_props : old_props;
      break;
    case svn_wc_conflict_choose_mine_full:
    case svn_wc_conflict_choose_mine_conflict:
      resolve_from = mine_props;
      break;
    case svn_wc_conflict_choose_theirs_full:
    case svn_wc_conflict_choose_theirs_conflict:
      resolve_from = their_props;
      break;
    case svn_wc_conflict_choose_merged:
      if (merged_file && conflicted_propname[0] != '\0')
        {
          apr_hash_t *actual_props;
          svn_stream_t *stream;
          svn_string_t *merged_propval;

          SVN_ERR(svn_wc__db_read_props(&actual_props, db, local_abspath,
                                        scratch_pool, scratch_pool));
          resolve_from = actual_props;

          SVN_ERR(svn_stream_open_readonly(&stream, merged_file,
                                           scratch_pool, scratch_pool));
          SVN_ERR(svn_string_from_stream(&merged_propval, stream,
                                         scratch_pool, scratch_pool));
          svn_hash_sets(resolve_from, conflicted_propname, merged_propval);
        }
      else
        resolve_from = NULL;
      break;
    default:
      return svn_error_create(SVN_ERR_INCORRECT_PARAMS, NULL,
                              _("Invalid 'conflict_result' argument"));
    }

  if (conflicted_props && apr_hash_count(conflicted_props) && resolve_from)
    {
      apr_hash_index_t *hi;
      apr_hash_t *actual_props;

      SVN_ERR(svn_wc__db_read_props(&actual_props, db, local_abspath,
                                    scratch_pool, scratch_pool));

      for (hi = apr_hash_first(scratch_pool, conflicted_props);
           hi;
           hi = apr_hash_next(hi))
        {
          const char *propname = svn__apr_hash_index_key(hi);
          svn_string_t *new_value = NULL;

          new_value = svn_hash_gets(resolve_from, propname);

          svn_hash_sets(actual_props, propname, new_value);
        }
      SVN_ERR(svn_wc__db_op_set_props(db, local_abspath, actual_props,
                                      FALSE, NULL, NULL,
                                      scratch_pool));
    }

  /* Legacy behavior: Only report property conflicts as resolved when the
     property reject file exists

     If not the UI shows the conflict as already resolved
     (and in this case we just remove the in-db conflict) */

  {
    svn_skel_t *work_item;

    SVN_ERR(remove_artifact_file_if_exists(&work_item, did_resolve,
                                           db, local_abspath, prop_reject_file,
                                           scratch_pool, scratch_pool));
    work_items = svn_wc__wq_merge(work_items, work_item, scratch_pool);
  }

  SVN_ERR(svn_wc__db_op_mark_resolved(db, local_abspath, FALSE, TRUE, FALSE,
                                      work_items, scratch_pool));
  SVN_ERR(svn_wc__wq_run(db, local_abspath, cancel_func, cancel_baton,
                         scratch_pool));

  return SVN_NO_ERROR;
}

/*
 * Resolve the tree conflict found in DB/LOCAL_ABSPATH according to
 * CONFLICT_CHOICE.
 *
 * It is not an error if there is no tree conflict. If a tree conflict
 * existed and was resolved, set *DID_RESOLVE to TRUE, else set it to FALSE.
 *
 * It is not an error if there is no tree conflict.
 */
static svn_error_t *
resolve_tree_conflict_on_node(svn_boolean_t *did_resolve,
                              svn_wc__db_t *db,
                              const char *local_abspath,
                              svn_wc_conflict_choice_t conflict_choice,
                              svn_wc_notify_func2_t notify_func,
                              void *notify_baton,
                              svn_cancel_func_t cancel_func,
                              void *cancel_baton,
                              apr_pool_t *scratch_pool)
{
  svn_wc_conflict_reason_t reason;
  svn_wc_conflict_action_t action;
  svn_skel_t *conflicts;
  svn_wc_operation_t operation;
  svn_boolean_t tree_conflicted;

  *did_resolve = FALSE;

  SVN_ERR(svn_wc__db_read_conflict(&conflicts, db, local_abspath,
                                   scratch_pool, scratch_pool));
  if (!conflicts)
    return SVN_NO_ERROR;

  SVN_ERR(svn_wc__conflict_read_info(&operation, NULL, NULL, NULL,
                                     &tree_conflicted, db, local_abspath,
                                     conflicts, scratch_pool, scratch_pool));
  if (!tree_conflicted)
    return SVN_NO_ERROR;

  SVN_ERR(svn_wc__conflict_read_tree_conflict(&reason, &action, NULL,
                                              db, local_abspath,
                                              conflicts,
                                              scratch_pool, scratch_pool));

  if (operation == svn_wc_operation_update
      || operation == svn_wc_operation_switch)
    {
      if (reason == svn_wc_conflict_reason_deleted ||
          reason == svn_wc_conflict_reason_replaced)
        {
          if (conflict_choice == svn_wc_conflict_choose_merged)
            {
              /* Break moves for any children moved out of this directory,
               * and leave this directory deleted. */
              SVN_ERR(svn_wc__db_resolve_break_moved_away_children(
                        db, local_abspath, notify_func, notify_baton,
                        scratch_pool));
              *did_resolve = TRUE;
            }
          else if (conflict_choice == svn_wc_conflict_choose_mine_conflict)
            {
              /* Raised moved-away conflicts on any children moved out of
               * this directory, and leave this directory deleted.
               * The newly conflicted moved-away children will be updated
               * if they are resolved with 'mine_conflict' as well. */
              SVN_ERR(svn_wc__db_resolve_delete_raise_moved_away(
                        db, local_abspath, notify_func, notify_baton,
                        scratch_pool));
              *did_resolve = TRUE;
            }
          else
            return svn_error_createf(SVN_ERR_WC_CONFLICT_RESOLVER_FAILURE,
                                     NULL,
                                     _("Tree conflict can only be resolved to "
                                       "'working' or 'mine-conflict' state; "
                                       "'%s' not resolved"),
                                     svn_dirent_local_style(local_abspath,
                                                            scratch_pool));
        }
      else if (reason == svn_wc_conflict_reason_moved_away
              && action == svn_wc_conflict_action_edit)
        {
          /* After updates, we can resolve local moved-away
           * vs. any incoming change, either by updating the
           * moved-away node (mine-conflict) or by breaking the
           * move (theirs-conflict). */
          if (conflict_choice == svn_wc_conflict_choose_mine_conflict)
            {
              SVN_ERR(svn_wc__db_update_moved_away_conflict_victim(
                        db, local_abspath,
                        notify_func, notify_baton,
                        cancel_func, cancel_baton,
                        scratch_pool));
              *did_resolve = TRUE;
            }
          else if (conflict_choice == svn_wc_conflict_choose_merged)
            {
              /* We must break the move if the user accepts the current
               * working copy state instead of updating the move.
               * Else the move would be left in an invalid state. */

              /* ### This breaks the move but leaves the conflict
                 ### involving the move until
                 ### svn_wc__db_op_mark_resolved. */
              SVN_ERR(svn_wc__db_resolve_break_moved_away(db, local_abspath,
                                                          notify_func,
                                                          notify_baton,
                                                          scratch_pool));
              *did_resolve = TRUE;
            }
          else
            return svn_error_createf(SVN_ERR_WC_CONFLICT_RESOLVER_FAILURE,
                                     NULL,
                                     _("Tree conflict can only be resolved to "
                                       "'working' or 'mine-conflict' state; "
                                       "'%s' not resolved"),
                                     svn_dirent_local_style(local_abspath,
                                                            scratch_pool));
        }
    }

  if (! *did_resolve && conflict_choice != svn_wc_conflict_choose_merged)
    {
      /* For other tree conflicts, there is no way to pick
       * theirs-full or mine-full, etc. Throw an error if the
       * user expects us to be smarter than we really are. */
      return svn_error_createf(SVN_ERR_WC_CONFLICT_RESOLVER_FAILURE,
                               NULL,
                               _("Tree conflict can only be "
                                 "resolved to 'working' state; "
                                 "'%s' not resolved"),
                               svn_dirent_local_style(local_abspath,
                                                      scratch_pool));
    }

  SVN_ERR(svn_wc__db_op_mark_resolved(db, local_abspath, FALSE, FALSE, TRUE,
                                      NULL, scratch_pool));
  SVN_ERR(svn_wc__wq_run(db, local_abspath, cancel_func, cancel_baton,
                         scratch_pool));
  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__mark_resolved_text_conflict(svn_wc__db_t *db,
                                    const char *local_abspath,
                                    apr_pool_t *scratch_pool)
{
  svn_boolean_t ignored_result;

  return svn_error_trace(resolve_text_conflict_on_node(
                           &ignored_result,
                           db, local_abspath,
                           svn_wc_conflict_choose_merged, NULL,
                           NULL, NULL,
                           scratch_pool));
}

svn_error_t *
svn_wc__mark_resolved_prop_conflicts(svn_wc__db_t *db,
                                     const char *local_abspath,
                                     apr_pool_t *scratch_pool)
{
  svn_boolean_t ignored_result;

  return svn_error_trace(resolve_prop_conflict_on_node(
                           &ignored_result,
                           db, local_abspath, "",
                           svn_wc_conflict_choose_merged, NULL,
                           NULL, NULL,
                           scratch_pool));
}


/* Baton for conflict_status_walker */
struct conflict_status_walker_baton
{
  svn_wc__db_t *db;
  svn_boolean_t resolve_text;
  const char *resolve_prop;
  svn_boolean_t resolve_tree;
  svn_wc_conflict_choice_t conflict_choice;
  svn_wc_conflict_resolver_func2_t conflict_func;
  void *conflict_baton;
  svn_cancel_func_t cancel_func;
  void *cancel_baton;
  svn_wc_notify_func2_t notify_func;
  void *notify_baton;
};

/* Implements svn_wc_status4_t to walk all conflicts to resolve.
 */
static svn_error_t *
conflict_status_walker(void *baton,
                       const char *local_abspath,
                       const svn_wc_status3_t *status,
                       apr_pool_t *scratch_pool)
{
  struct conflict_status_walker_baton *cswb = baton;
  svn_wc__db_t *db = cswb->db;

  const apr_array_header_t *conflicts;
  apr_pool_t *iterpool;
  int i;
  svn_boolean_t resolved = FALSE;

  if (!status->conflicted)
    return SVN_NO_ERROR;

  iterpool = svn_pool_create(scratch_pool);

  SVN_ERR(svn_wc__read_conflicts(&conflicts, db, local_abspath, TRUE,
                                 scratch_pool, iterpool));

  for (i = 0; i < conflicts->nelts; i++)
    {
      const svn_wc_conflict_description2_t *cd;
      svn_boolean_t did_resolve;
      svn_wc_conflict_choice_t my_choice = cswb->conflict_choice;
      const char *merged_file = NULL;

      cd = APR_ARRAY_IDX(conflicts, i, const svn_wc_conflict_description2_t *);

      if ((cd->kind == svn_wc_conflict_kind_property && !cswb->resolve_prop)
          || (cd->kind == svn_wc_conflict_kind_text && !cswb->resolve_text)
          || (cd->kind == svn_wc_conflict_kind_tree && !cswb->resolve_tree))
        {
          continue; /* Easy out. Don't call resolver func and ignore result */
        }

      svn_pool_clear(iterpool);

      if (my_choice == svn_wc_conflict_choose_unspecified)
        {
          svn_wc_conflict_result_t *result;

          if (!cswb->conflict_func)
            return svn_error_create(SVN_ERR_WC_CONFLICT_RESOLVER_FAILURE, NULL,
                                    _("No conflict-callback and no "
                                      "pre-defined conflict-choice provided"));

          SVN_ERR(cswb->conflict_func(&result, cd, cswb->conflict_baton,
                                      iterpool, iterpool));

          my_choice = result->choice;
          merged_file = result->merged_file;
          /* ### Bug: ignores result->save_merged */
        }


      if (my_choice == svn_wc_conflict_choose_postpone)
        continue;

      switch (cd->kind)
        {
          case svn_wc_conflict_kind_tree:
            if (!cswb->resolve_tree)
              break;
            SVN_ERR(resolve_tree_conflict_on_node(&did_resolve,
                                                  db,
                                                  local_abspath,
                                                  my_choice,
                                                  cswb->notify_func,
                                                  cswb->notify_baton,
                                                  cswb->cancel_func,
                                                  cswb->cancel_baton,
                                                  iterpool));

            resolved = TRUE;
            break;

          case svn_wc_conflict_kind_text:
            if (!cswb->resolve_text)
              break;

            SVN_ERR(resolve_text_conflict_on_node(&did_resolve,
                                                  db,
                                                  local_abspath,
                                                  my_choice,
                                                  merged_file,
                                                  cswb->cancel_func,
                                                  cswb->cancel_baton,
                                                  iterpool));

            if (did_resolve)
              resolved = TRUE;
            break;

          case svn_wc_conflict_kind_property:
            if (!cswb->resolve_prop)
              break;

            if (*cswb->resolve_prop != '\0' &&
                strcmp(cswb->resolve_prop, cd->property_name) != 0)
              {
                break; /* This is not the property we want to resolve. */
              }

            SVN_ERR(resolve_prop_conflict_on_node(&did_resolve,
                                                  db,
                                                  local_abspath,
                                                  cd->property_name,
                                                  my_choice,
                                                  merged_file,
                                                  cswb->cancel_func,
                                                  cswb->cancel_baton,
                                                  iterpool));

            if (did_resolve)
              resolved = TRUE;
            break;

          default:
            /* We can't resolve other conflict types */
            break;
        }
    }

  /* Notify */
  if (cswb->notify_func && resolved)
    cswb->notify_func(cswb->notify_baton,
                      svn_wc_create_notify(local_abspath,
                                           svn_wc_notify_resolved,
                                           iterpool),
                      iterpool);

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc__resolve_conflicts(svn_wc_context_t *wc_ctx,
                          const char *local_abspath,
                          svn_depth_t depth,
                          svn_boolean_t resolve_text,
                          const char *resolve_prop,
                          svn_boolean_t resolve_tree,
                          svn_wc_conflict_choice_t conflict_choice,
                          svn_wc_conflict_resolver_func2_t conflict_func,
                          void *conflict_baton,
                          svn_cancel_func_t cancel_func,
                          void *cancel_baton,
                          svn_wc_notify_func2_t notify_func,
                          void *notify_baton,
                          apr_pool_t *scratch_pool)
{
  svn_node_kind_t kind;
  svn_boolean_t conflicted;
  struct conflict_status_walker_baton cswb;

  /* ### the underlying code does NOT support resolving individual
     ### properties. bail out if the caller tries it.  */
  if (resolve_prop != NULL && *resolve_prop != '\0')
    return svn_error_create(SVN_ERR_INCORRECT_PARAMS, NULL,
                            U_("Resolving a single property is not (yet) "
                               "supported."));

  /* ### Just a versioned check? */
  /* Conflicted is set to allow invoking on actual only nodes */
  SVN_ERR(svn_wc__db_read_info(NULL, &kind, NULL, NULL, NULL, NULL, NULL,
                               NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                               NULL, NULL, NULL, NULL, NULL, NULL, &conflicted,
                               NULL, NULL, NULL, NULL, NULL, NULL,
                               wc_ctx->db, local_abspath,
                               scratch_pool, scratch_pool));

  /* When the implementation still used the entry walker, depth
     unknown was translated to infinity. */
  if (kind != svn_node_dir)
    depth = svn_depth_empty;
  else if (depth == svn_depth_unknown)
    depth = svn_depth_infinity;

  cswb.db = wc_ctx->db;
  cswb.resolve_text = resolve_text;
  cswb.resolve_prop = resolve_prop;
  cswb.resolve_tree = resolve_tree;
  cswb.conflict_choice = conflict_choice;

  cswb.conflict_func = conflict_func;
  cswb.conflict_baton = conflict_baton;

  cswb.cancel_func = cancel_func;
  cswb.cancel_baton = cancel_baton;

  cswb.notify_func = notify_func;
  cswb.notify_baton = notify_baton;

  if (notify_func)
    notify_func(notify_baton,
                svn_wc_create_notify(local_abspath,
                                    svn_wc_notify_conflict_resolver_starting,
                                    scratch_pool),
                scratch_pool);

  SVN_ERR(svn_wc_walk_status(wc_ctx,
                             local_abspath,
                             depth,
                             FALSE /* get_all */,
                             FALSE /* no_ignore */,
                             TRUE /* ignore_text_mods */,
                             NULL /* ignore_patterns */,
                             conflict_status_walker, &cswb,
                             cancel_func, cancel_baton,
                             scratch_pool));

  if (notify_func)
    notify_func(notify_baton,
                svn_wc_create_notify(local_abspath,
                                    svn_wc_notify_conflict_resolver_done,
                                    scratch_pool),
                scratch_pool);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_wc_resolved_conflict5(svn_wc_context_t *wc_ctx,
                          const char *local_abspath,
                          svn_depth_t depth,
                          svn_boolean_t resolve_text,
                          const char *resolve_prop,
                          svn_boolean_t resolve_tree,
                          svn_wc_conflict_choice_t conflict_choice,
                          svn_cancel_func_t cancel_func,
                          void *cancel_baton,
                          svn_wc_notify_func2_t notify_func,
                          void *notify_baton,
                          apr_pool_t *scratch_pool)
{
  return svn_error_trace(svn_wc__resolve_conflicts(wc_ctx, local_abspath,
                                                   depth, resolve_text,
                                                   resolve_prop, resolve_tree,
                                                   conflict_choice,
                                                   NULL, NULL,
                                                   cancel_func, cancel_baton,
                                                   notify_func, notify_baton,
                                                   scratch_pool));
}

/* Constructor for the result-structure returned by conflict callbacks. */
svn_wc_conflict_result_t *
svn_wc_create_conflict_result(svn_wc_conflict_choice_t choice,
                              const char *merged_file,
                              apr_pool_t *pool)
{
  svn_wc_conflict_result_t *result = apr_pcalloc(pool, sizeof(*result));
  result->choice = choice;
  result->merged_file = merged_file;
  result->save_merged = FALSE;

  /* If we add more fields to svn_wc_conflict_result_t, add them here. */

  return result;
}
