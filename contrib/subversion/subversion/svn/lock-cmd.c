/*
 * lock-cmd.c -- LOck a working copy path in the repository.
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

#include "svn_pools.h"
#include "svn_client.h"
#include "svn_subst.h"
#include "svn_path.h"
#include "svn_error_codes.h"
#include "svn_error.h"
#include "svn_cmdline.h"
#include "cl.h"
#include "svn_private_config.h"


/*** Code. ***/

/* Get a lock comment, allocate it in POOL and store it in *COMMENT. */
static svn_error_t *
get_comment(const char **comment, svn_client_ctx_t *ctx,
            svn_cl__opt_state_t *opt_state, apr_pool_t *pool)
{
  svn_string_t *comment_string;

  if (opt_state->filedata)
    {
      /* Get it from the -F argument. */
      if (strlen(opt_state->filedata->data) < opt_state->filedata->len)
        {
          /* A message containing a zero byte can't be represented as a C
             string. */
          return svn_error_create(SVN_ERR_CL_BAD_LOG_MESSAGE, NULL,
                                  _("Lock comment contains a zero byte"));
        }
      comment_string = svn_string_create(opt_state->filedata->data, pool);

    }
  else if (opt_state->message)
    {
      /* Get if from the -m option. */
      comment_string = svn_string_create(opt_state->message, pool);
    }
  else
    {
      *comment = NULL;
      return SVN_NO_ERROR;
    }

  /* Translate to UTF8/LF. */
  SVN_ERR(svn_subst_translate_string2(&comment_string, NULL, NULL,
                                      comment_string, opt_state->encoding,
                                      FALSE, pool, pool));
  *comment = comment_string->data;

  return SVN_NO_ERROR;
}

/* This implements the `svn_opt_subcommand_t' interface. */
svn_error_t *
svn_cl__lock(apr_getopt_t *os,
             void *baton,
             apr_pool_t *pool)
{
  svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
  svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
  apr_array_header_t *targets;
  const char *comment;

  SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
                                                      opt_state->targets,
                                                      ctx, FALSE, pool));

  /* We only support locking files, so '.' is not valid. */
  if (! targets->nelts)
    return svn_error_create(SVN_ERR_CL_INSUFFICIENT_ARGS, 0, NULL);

  SVN_ERR(svn_cl__assert_homogeneous_target_type(targets));

  /* Get comment. */
  SVN_ERR(get_comment(&comment, ctx, opt_state, pool));

  SVN_ERR(svn_cl__eat_peg_revisions(&targets, targets, pool));

  return svn_client_lock(targets, comment, opt_state->force, ctx, pool);
}
