/*
 * cleanup-cmd.c -- Subversion cleanup command
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

#include "svn_client.h"
#include "svn_path.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "cl.h"

#include "svn_private_config.h"


/*** Code. ***/

/* This implements the `svn_opt_subcommand_t' interface. */
svn_error_t *
svn_cl__cleanup(apr_getopt_t *os,
                void *baton,
                apr_pool_t *pool)
{
  svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
  svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
  apr_array_header_t *targets;
  apr_pool_t *subpool;
  int i;

  SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
                                                      opt_state->targets,
                                                      ctx, FALSE, pool));

  /* Add "." if user passed 0 arguments */
  svn_opt_push_implicit_dot_target(targets, pool);

  SVN_ERR(svn_cl__check_targets_are_local_paths(targets));

  SVN_ERR(svn_cl__eat_peg_revisions(&targets, targets, pool));

  subpool = svn_pool_create(pool);
  for (i = 0; i < targets->nelts; i++)
    {
      const char *target = APR_ARRAY_IDX(targets, i, const char *);
      svn_error_t *err;

      svn_pool_clear(subpool);
      SVN_ERR(svn_cl__check_cancel(ctx->cancel_baton));
      err = svn_client_cleanup(target, ctx, subpool);
      if (err && err->apr_err == SVN_ERR_WC_LOCKED)
        {
          const char *target_abspath;
          svn_error_t *err2 = svn_dirent_get_absolute(&target_abspath,
                                                      target, subpool);
          if (err2)
            {
              err =  svn_error_compose_create(err, err2);
            }
          else
            {
              const char *wcroot_abspath;

              err2 = svn_client_get_wc_root(&wcroot_abspath, target_abspath,
                                            ctx, subpool, subpool);
              if (err2)
                err =  svn_error_compose_create(err, err2);
              else
                err = svn_error_createf(SVN_ERR_WC_LOCKED, err,
                                        _("Working copy locked; try running "
                                          "'svn cleanup' on the root of the "
                                          "working copy ('%s') instead."),
                                          svn_dirent_local_style(wcroot_abspath,
                                                                 subpool));
            }
        }
      SVN_ERR(err);
    }

  svn_pool_destroy(subpool);
  return SVN_NO_ERROR;
}
