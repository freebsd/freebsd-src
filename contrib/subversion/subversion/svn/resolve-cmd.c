/*
 * resolve-cmd.c -- Subversion resolve subcommand
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
#include "svn_path.h"
#include "svn_client.h"
#include "svn_error.h"
#include "svn_pools.h"
#include "cl.h"

#include "svn_private_config.h"



/*** Code. ***/

/* This implements the `svn_opt_subcommand_t' interface. */
svn_error_t *
svn_cl__resolve(apr_getopt_t *os,
                void *baton,
                apr_pool_t *scratch_pool)
{
  svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
  svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
  svn_wc_conflict_choice_t conflict_choice;
  svn_error_t *err;
  apr_array_header_t *targets;
  int i;
  apr_pool_t *iterpool;
  svn_boolean_t had_error = FALSE;

  switch (opt_state->accept_which)
    {
    case svn_cl__accept_working:
      conflict_choice = svn_wc_conflict_choose_merged;
      break;
    case svn_cl__accept_base:
      conflict_choice = svn_wc_conflict_choose_base;
      break;
    case svn_cl__accept_theirs_conflict:
      conflict_choice = svn_wc_conflict_choose_theirs_conflict;
      break;
    case svn_cl__accept_mine_conflict:
      conflict_choice = svn_wc_conflict_choose_mine_conflict;
      break;
    case svn_cl__accept_theirs_full:
      conflict_choice = svn_wc_conflict_choose_theirs_full;
      break;
    case svn_cl__accept_mine_full:
      conflict_choice = svn_wc_conflict_choose_mine_full;
      break;
    case svn_cl__accept_unspecified:
      if (opt_state->non_interactive)
        return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                                _("missing --accept option"));
      conflict_choice = svn_wc_conflict_choose_unspecified;
      break;
    default:
      return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                              _("invalid 'accept' ARG"));
    }

  SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
                                                      opt_state->targets,
                                                      ctx, FALSE,
                                                      scratch_pool));
  if (! targets->nelts)
    svn_opt_push_implicit_dot_target(targets, scratch_pool);

  if (opt_state->depth == svn_depth_unknown)
    {
      if (opt_state->accept_which == svn_cl__accept_unspecified)
        opt_state->depth = svn_depth_infinity;
      else
        opt_state->depth = svn_depth_empty;
    }

  SVN_ERR(svn_cl__eat_peg_revisions(&targets, targets, scratch_pool));

  SVN_ERR(svn_cl__check_targets_are_local_paths(targets));

  iterpool = svn_pool_create(scratch_pool);
  for (i = 0; i < targets->nelts; i++)
    {
      const char *target = APR_ARRAY_IDX(targets, i, const char *);
      svn_pool_clear(iterpool);
      SVN_ERR(svn_cl__check_cancel(ctx->cancel_baton));
      err = svn_client_resolve(target,
                               opt_state->depth, conflict_choice,
                               ctx,
                               iterpool);
      if (err)
        {
          svn_handle_warning2(stderr, err, "svn: ");
          svn_error_clear(err);
          had_error = TRUE;
        }
    }
  svn_pool_destroy(iterpool);

  if (had_error)
    return svn_error_create(SVN_ERR_CL_ERROR_PROCESSING_EXTERNALS, NULL,
                            _("Failure occurred resolving one or more "
                              "conflicts"));

  return SVN_NO_ERROR;
}
