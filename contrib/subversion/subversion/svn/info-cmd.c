/*
 * info-cmd.c -- Display information about a resource
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

#include "svn_string.h"
#include "svn_cmdline.h"
#include "svn_wc.h"
#include "svn_pools.h"
#include "svn_error_codes.h"
#include "svn_error.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_time.h"
#include "svn_xml.h"
#include "cl.h"

#include "svn_private_config.h"
#include "cl-conflicts.h"


/*** Code. ***/

static svn_error_t *
svn_cl__info_print_time(apr_time_t atime,
                        const char *desc,
                        apr_pool_t *pool)
{
  const char *time_utf8;

  time_utf8 = svn_time_to_human_cstring(atime, pool);
  return svn_cmdline_printf(pool, "%s: %s\n", desc, time_utf8);
}


/* Return string representation of SCHEDULE */
static const char *
schedule_str(svn_wc_schedule_t schedule)
{
  switch (schedule)
    {
    case svn_wc_schedule_normal:
      return "normal";
    case svn_wc_schedule_add:
      return "add";
    case svn_wc_schedule_delete:
      return "delete";
    case svn_wc_schedule_replace:
      return "replace";
    default:
      return "none";
    }
}


/* A callback of type svn_client_info_receiver2_t.
   Prints svn info in xml mode to standard out */
static svn_error_t *
print_info_xml(void *baton,
               const char *target,
               const svn_client_info2_t *info,
               apr_pool_t *pool)
{
  svn_stringbuf_t *sb = svn_stringbuf_create_empty(pool);
  const char *rev_str;
  const char *path_prefix = baton;

  if (SVN_IS_VALID_REVNUM(info->rev))
    rev_str = apr_psprintf(pool, "%ld", info->rev);
  else
    rev_str = apr_pstrdup(pool, _("Resource is not under version control."));

  /* "<entry ...>" */
  svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "entry",
                        "path", svn_cl__local_style_skip_ancestor(
                                  path_prefix, target, pool),
                        "kind", svn_cl__node_kind_str_xml(info->kind),
                        "revision", rev_str,
                        NULL);

  /* "<url> xx </url>" */
  svn_cl__xml_tagged_cdata(&sb, pool, "url", info->URL);

  if (info->repos_root_URL && info->URL)
    {
      /* "<relative-url> xx </relative-url>" */
      svn_cl__xml_tagged_cdata(&sb, pool, "relative-url",
                               apr_pstrcat(pool, "^/",
                                           svn_path_uri_encode(
                                               svn_uri_skip_ancestor(
                                                   info->repos_root_URL,
                                                   info->URL, pool),
                                               pool),
                                           NULL));
    }

  if (info->repos_root_URL || info->repos_UUID)
    {
      /* "<repository>" */
      svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "repository", NULL);

      /* "<root> xx </root>" */
      svn_cl__xml_tagged_cdata(&sb, pool, "root", info->repos_root_URL);

      /* "<uuid> xx </uuid>" */
      svn_cl__xml_tagged_cdata(&sb, pool, "uuid", info->repos_UUID);

      /* "</repository>" */
      svn_xml_make_close_tag(&sb, pool, "repository");
    }

  if (info->wc_info)
    {
      /* "<wc-info>" */
      svn_xml_make_open_tag(&sb, pool, svn_xml_normal, "wc-info", NULL);

      /* "<wcroot-abspath> xx </wcroot-abspath>" */
      if (info->wc_info->wcroot_abspath)
        svn_cl__xml_tagged_cdata(&sb, pool, "wcroot-abspath",
                                 info->wc_info->wcroot_abspath);

      /* "<schedule> xx </schedule>" */
      svn_cl__xml_tagged_cdata(&sb, pool, "schedule",
                               schedule_str(info->wc_info->schedule));

      /* "<depth> xx </depth>" */
      {
        svn_depth_t depth = info->wc_info->depth;

        /* In the entries world info just passed depth infinity for files */
        if (depth == svn_depth_unknown && info->kind == svn_node_file)
          depth = svn_depth_infinity;

        svn_cl__xml_tagged_cdata(&sb, pool, "depth", svn_depth_to_word(depth));
      }

      /* "<copy-from-url> xx </copy-from-url>" */
      svn_cl__xml_tagged_cdata(&sb, pool, "copy-from-url",
                               info->wc_info->copyfrom_url);

      /* "<copy-from-rev> xx </copy-from-rev>" */
      if (SVN_IS_VALID_REVNUM(info->wc_info->copyfrom_rev))
        svn_cl__xml_tagged_cdata(&sb, pool, "copy-from-rev",
                                 apr_psprintf(pool, "%ld",
                                              info->wc_info->copyfrom_rev));

      /* "<text-updated> xx </text-updated>" */
      if (info->wc_info->recorded_time)
        svn_cl__xml_tagged_cdata(&sb, pool, "text-updated",
                                 svn_time_to_cstring(
                                          info->wc_info->recorded_time,
                                          pool));

      /* "<checksum> xx </checksum>" */
      /* ### Print the checksum kind. */
      svn_cl__xml_tagged_cdata(&sb, pool, "checksum",
                               svn_checksum_to_cstring(info->wc_info->checksum,
                                                       pool));

      if (info->wc_info->changelist)
        /* "<changelist> xx </changelist>" */
        svn_cl__xml_tagged_cdata(&sb, pool, "changelist",
                                 info->wc_info->changelist);

      if (info->wc_info->moved_from_abspath)
        {
          const char *relpath;

          relpath = svn_dirent_skip_ancestor(info->wc_info->wcroot_abspath,
                                             info->wc_info->moved_from_abspath);

          /* <moved-from> xx </moved-from> */
          if (relpath && relpath[0] != '\0')
            svn_cl__xml_tagged_cdata(&sb, pool, "moved-from", relpath);
          else
            svn_cl__xml_tagged_cdata(&sb, pool, "moved-from",
                                     info->wc_info->moved_from_abspath);
        }

      if (info->wc_info->moved_to_abspath)
        {
          const char *relpath;

          relpath = svn_dirent_skip_ancestor(info->wc_info->wcroot_abspath,
                                             info->wc_info->moved_to_abspath);
          /* <moved-to> xx </moved-to> */
          if (relpath && relpath[0] != '\0')
            svn_cl__xml_tagged_cdata(&sb, pool, "moved-to", relpath);
          else
            svn_cl__xml_tagged_cdata(&sb, pool, "moved-to",
                                     info->wc_info->moved_to_abspath);
        }

      /* "</wc-info>" */
      svn_xml_make_close_tag(&sb, pool, "wc-info");
    }

  if (info->last_changed_author
      || SVN_IS_VALID_REVNUM(info->last_changed_rev)
      || info->last_changed_date)
    {
      svn_cl__print_xml_commit(&sb, info->last_changed_rev,
                               info->last_changed_author,
                               svn_time_to_cstring(info->last_changed_date,
                                                   pool),
                               pool);
    }

  if (info->wc_info && info->wc_info->conflicts)
    {
      int i;

      for (i = 0; i < info->wc_info->conflicts->nelts; i++)
        {
          const svn_wc_conflict_description2_t *conflict =
                      APR_ARRAY_IDX(info->wc_info->conflicts, i,
                                    const svn_wc_conflict_description2_t *);

          SVN_ERR(svn_cl__append_conflict_info_xml(sb, conflict, pool));
        }
    }

  if (info->lock)
    svn_cl__print_xml_lock(&sb, info->lock, pool);

  /* "</entry>" */
  svn_xml_make_close_tag(&sb, pool, "entry");

  return svn_cl__error_checked_fputs(sb->data, stdout);
}


/* A callback of type svn_client_info_receiver2_t. */
static svn_error_t *
print_info(void *baton,
           const char *target,
           const svn_client_info2_t *info,
           apr_pool_t *pool)
{
  const char *path_prefix = baton;

  SVN_ERR(svn_cmdline_printf(pool, _("Path: %s\n"),
                             svn_cl__local_style_skip_ancestor(
                               path_prefix, target, pool)));

  /* ### remove this someday:  it's only here for cmdline output
     compatibility with svn 1.1 and older.  */
  if (info->kind != svn_node_dir)
    SVN_ERR(svn_cmdline_printf(pool, _("Name: %s\n"),
                               svn_dirent_basename(target, pool)));

  if (info->wc_info && info->wc_info->wcroot_abspath)
    SVN_ERR(svn_cmdline_printf(pool, _("Working Copy Root Path: %s\n"),
                               svn_dirent_local_style(
                                            info->wc_info->wcroot_abspath,
                                            pool)));

  if (info->URL)
    SVN_ERR(svn_cmdline_printf(pool, _("URL: %s\n"), info->URL));

  if (info->URL && info->repos_root_URL)
    SVN_ERR(svn_cmdline_printf(pool, _("Relative URL: ^/%s\n"),
                               svn_path_uri_encode(
                                   svn_uri_skip_ancestor(info->repos_root_URL,
                                                         info->URL, pool),
                                   pool)));

  if (info->repos_root_URL)
    SVN_ERR(svn_cmdline_printf(pool, _("Repository Root: %s\n"),
                               info->repos_root_URL));

  if (info->repos_UUID)
    SVN_ERR(svn_cmdline_printf(pool, _("Repository UUID: %s\n"),
                               info->repos_UUID));

  if (SVN_IS_VALID_REVNUM(info->rev))
    SVN_ERR(svn_cmdline_printf(pool, _("Revision: %ld\n"), info->rev));

  switch (info->kind)
    {
    case svn_node_file:
      SVN_ERR(svn_cmdline_printf(pool, _("Node Kind: file\n")));
      break;

    case svn_node_dir:
      SVN_ERR(svn_cmdline_printf(pool, _("Node Kind: directory\n")));
      break;

    case svn_node_none:
      SVN_ERR(svn_cmdline_printf(pool, _("Node Kind: none\n")));
      break;

    case svn_node_unknown:
    default:
      SVN_ERR(svn_cmdline_printf(pool, _("Node Kind: unknown\n")));
      break;
    }

  if (info->wc_info)
    {
      switch (info->wc_info->schedule)
        {
        case svn_wc_schedule_normal:
          SVN_ERR(svn_cmdline_printf(pool, _("Schedule: normal\n")));
          break;

        case svn_wc_schedule_add:
          SVN_ERR(svn_cmdline_printf(pool, _("Schedule: add\n")));
          break;

        case svn_wc_schedule_delete:
          SVN_ERR(svn_cmdline_printf(pool, _("Schedule: delete\n")));
          break;

        case svn_wc_schedule_replace:
          SVN_ERR(svn_cmdline_printf(pool, _("Schedule: replace\n")));
          break;

        default:
          break;
        }

      switch (info->wc_info->depth)
        {
        case svn_depth_unknown:
          /* Unknown depth is the norm for remote directories anyway
             (although infinity would be equally appropriate).  Let's
             not bother to print it. */
          break;

        case svn_depth_empty:
          SVN_ERR(svn_cmdline_printf(pool, _("Depth: empty\n")));
          break;

        case svn_depth_files:
          SVN_ERR(svn_cmdline_printf(pool, _("Depth: files\n")));
          break;

        case svn_depth_immediates:
          SVN_ERR(svn_cmdline_printf(pool, _("Depth: immediates\n")));
          break;

        case svn_depth_exclude:
          SVN_ERR(svn_cmdline_printf(pool, _("Depth: exclude\n")));
          break;

        case svn_depth_infinity:
          /* Infinity is the default depth for working copy
             directories.  Let's not print it, it's not special enough
             to be worth mentioning.  */
          break;

        default:
          /* Other depths should never happen here. */
          SVN_ERR(svn_cmdline_printf(pool, _("Depth: INVALID\n")));
        }

      if (info->wc_info->copyfrom_url)
        SVN_ERR(svn_cmdline_printf(pool, _("Copied From URL: %s\n"),
                                   info->wc_info->copyfrom_url));

      if (SVN_IS_VALID_REVNUM(info->wc_info->copyfrom_rev))
        SVN_ERR(svn_cmdline_printf(pool, _("Copied From Rev: %ld\n"),
                                   info->wc_info->copyfrom_rev));
      if (info->wc_info->moved_from_abspath)
        {
          const char *relpath;

          relpath = svn_dirent_skip_ancestor(info->wc_info->wcroot_abspath,
                                             info->wc_info->moved_from_abspath);
          if (relpath && relpath[0] != '\0')
            SVN_ERR(svn_cmdline_printf(pool, _("Moved From: %s\n"), relpath));
          else
            SVN_ERR(svn_cmdline_printf(pool, _("Moved From: %s\n"),
                                       info->wc_info->moved_from_abspath));
        }

      if (info->wc_info->moved_to_abspath)
        {
          const char *relpath;

          relpath = svn_dirent_skip_ancestor(info->wc_info->wcroot_abspath,
                                             info->wc_info->moved_to_abspath);
          if (relpath && relpath[0] != '\0')
            SVN_ERR(svn_cmdline_printf(pool, _("Moved To: %s\n"), relpath));
          else
            SVN_ERR(svn_cmdline_printf(pool, _("Moved To: %s\n"),
                                       info->wc_info->moved_to_abspath));
        }
    }

  if (info->last_changed_author)
    SVN_ERR(svn_cmdline_printf(pool, _("Last Changed Author: %s\n"),
                               info->last_changed_author));

  if (SVN_IS_VALID_REVNUM(info->last_changed_rev))
    SVN_ERR(svn_cmdline_printf(pool, _("Last Changed Rev: %ld\n"),
                               info->last_changed_rev));

  if (info->last_changed_date)
    SVN_ERR(svn_cl__info_print_time(info->last_changed_date,
                                    _("Last Changed Date"), pool));

  if (info->wc_info)
    {
      if (info->wc_info->recorded_time)
        SVN_ERR(svn_cl__info_print_time(info->wc_info->recorded_time,
                                        _("Text Last Updated"), pool));

      if (info->wc_info->checksum)
        SVN_ERR(svn_cmdline_printf(pool, _("Checksum: %s\n"),
                                   svn_checksum_to_cstring(
                                              info->wc_info->checksum, pool)));

      if (info->wc_info->conflicts)
        {
          svn_boolean_t printed_prop_conflict_file = FALSE;
          int i;

          for (i = 0; i < info->wc_info->conflicts->nelts; i++)
            {
              const svn_wc_conflict_description2_t *conflict =
                    APR_ARRAY_IDX(info->wc_info->conflicts, i,
                                  const svn_wc_conflict_description2_t *);
              const char *desc;

              switch (conflict->kind)
                {
                  case svn_wc_conflict_kind_text:
                    if (conflict->base_abspath)
                      SVN_ERR(svn_cmdline_printf(pool,
                                _("Conflict Previous Base File: %s\n"),
                                svn_cl__local_style_skip_ancestor(
                                        path_prefix, conflict->base_abspath,
                                        pool)));

                    if (conflict->my_abspath)
                      SVN_ERR(svn_cmdline_printf(pool,
                                _("Conflict Previous Working File: %s\n"),
                                svn_cl__local_style_skip_ancestor(
                                        path_prefix, conflict->my_abspath,
                                        pool)));

                    if (conflict->their_abspath)
                      SVN_ERR(svn_cmdline_printf(pool,
                                _("Conflict Current Base File: %s\n"),
                                svn_cl__local_style_skip_ancestor(
                                        path_prefix, conflict->their_abspath,
                                        pool)));
                  break;

                  case svn_wc_conflict_kind_property:
                    if (! printed_prop_conflict_file)
                      SVN_ERR(svn_cmdline_printf(pool,
                                _("Conflict Properties File: %s\n"),
                                svn_dirent_local_style(conflict->their_abspath,
                                                       pool)));
                    printed_prop_conflict_file = TRUE;
                  break;

                  case svn_wc_conflict_kind_tree:
                    SVN_ERR(
                        svn_cl__get_human_readable_tree_conflict_description(
                                                    &desc, conflict, pool));

                    SVN_ERR(svn_cmdline_printf(pool, "%s: %s\n",
                                               _("Tree conflict"), desc));
                  break;
                }
            }

          /* We only store one left and right version for all conflicts, which is
             referenced from all conflicts.
             Print it after the conflicts to match the 1.6/1.7 output where it is
             only available for tree conflicts */
          {
            const char *src_left_version;
            const char *src_right_version;
            const svn_wc_conflict_description2_t *conflict =
                  APR_ARRAY_IDX(info->wc_info->conflicts, 0,
                                const svn_wc_conflict_description2_t *);

            src_left_version =
                        svn_cl__node_description(conflict->src_left_version,
                                                 info->repos_root_URL, pool);

            src_right_version =
                        svn_cl__node_description(conflict->src_right_version,
                                                 info->repos_root_URL, pool);

            if (src_left_version)
              SVN_ERR(svn_cmdline_printf(pool, "  %s: %s\n",
                                         _("Source  left"), /* (1) */
                                         src_left_version));
            /* (1): Sneaking in a space in "Source  left" so that
             * it is the same length as "Source right" while it still
             * starts in the same column. That's just a tiny tweak in
             * the English `svn'. */

            if (src_right_version)
              SVN_ERR(svn_cmdline_printf(pool, "  %s: %s\n",
                                         _("Source right"),
                                         src_right_version));
          }
        }
    }

  if (info->lock)
    {
      if (info->lock->token)
        SVN_ERR(svn_cmdline_printf(pool, _("Lock Token: %s\n"),
                                   info->lock->token));

      if (info->lock->owner)
        SVN_ERR(svn_cmdline_printf(pool, _("Lock Owner: %s\n"),
                                   info->lock->owner));

      if (info->lock->creation_date)
        SVN_ERR(svn_cl__info_print_time(info->lock->creation_date,
                                        _("Lock Created"), pool));

      if (info->lock->expiration_date)
        SVN_ERR(svn_cl__info_print_time(info->lock->expiration_date,
                                        _("Lock Expires"), pool));

      if (info->lock->comment)
        {
          int comment_lines;
          /* NOTE: The stdio will handle newline translation. */
          comment_lines = svn_cstring_count_newlines(info->lock->comment) + 1;
          SVN_ERR(svn_cmdline_printf(pool,
                                     Q_("Lock Comment (%i line):\n%s\n",
                                        "Lock Comment (%i lines):\n%s\n",
                                        comment_lines),
                                     comment_lines,
                                     info->lock->comment));
        }
    }

  if (info->wc_info && info->wc_info->changelist)
    SVN_ERR(svn_cmdline_printf(pool, _("Changelist: %s\n"),
                               info->wc_info->changelist));

  /* Print extra newline separator. */
  return svn_cmdline_printf(pool, "\n");
}


/* This implements the `svn_opt_subcommand_t' interface. */
svn_error_t *
svn_cl__info(apr_getopt_t *os,
             void *baton,
             apr_pool_t *pool)
{
  svn_cl__opt_state_t *opt_state = ((svn_cl__cmd_baton_t *) baton)->opt_state;
  svn_client_ctx_t *ctx = ((svn_cl__cmd_baton_t *) baton)->ctx;
  apr_array_header_t *targets = NULL;
  apr_pool_t *subpool = svn_pool_create(pool);
  int i;
  svn_error_t *err;
  svn_boolean_t seen_nonexistent_target = FALSE;
  svn_opt_revision_t peg_revision;
  svn_client_info_receiver2_t receiver;
  const char *path_prefix;

  SVN_ERR(svn_cl__args_to_target_array_print_reserved(&targets, os,
                                                      opt_state->targets,
                                                      ctx, FALSE, pool));

  /* Add "." if user passed 0 arguments. */
  svn_opt_push_implicit_dot_target(targets, pool);

  if (opt_state->xml)
    {
      receiver = print_info_xml;

      /* If output is not incremental, output the XML header and wrap
         everything in a top-level element. This makes the output in
         its entirety a well-formed XML document. */
      if (! opt_state->incremental)
        SVN_ERR(svn_cl__xml_print_header("info", pool));
    }
  else
    {
      receiver = print_info;

      if (opt_state->incremental)
        return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                                _("'incremental' option only valid in XML "
                                  "mode"));
    }

  if (opt_state->depth == svn_depth_unknown)
    opt_state->depth = svn_depth_empty;

  SVN_ERR(svn_dirent_get_absolute(&path_prefix, "", pool));

  for (i = 0; i < targets->nelts; i++)
    {
      const char *truepath;
      const char *target = APR_ARRAY_IDX(targets, i, const char *);

      svn_pool_clear(subpool);
      SVN_ERR(svn_cl__check_cancel(ctx->cancel_baton));

      /* Get peg revisions. */
      SVN_ERR(svn_opt_parse_path(&peg_revision, &truepath, target, subpool));

      /* If no peg-rev was attached to a URL target, then assume HEAD. */
      if (svn_path_is_url(truepath))
        {
          if (peg_revision.kind == svn_opt_revision_unspecified)
            peg_revision.kind = svn_opt_revision_head;
        }
      else
        {
          SVN_ERR(svn_dirent_get_absolute(&truepath, truepath, subpool));
        }

      err = svn_client_info3(truepath,
                             &peg_revision, &(opt_state->start_revision),
                             opt_state->depth, TRUE, TRUE,
                             opt_state->changelists,
                             receiver, (void *) path_prefix,
                             ctx, subpool);

      if (err)
        {
          /* If one of the targets is a non-existent URL or wc-entry,
             don't bail out.  Just warn and move on to the next target. */
          if (err->apr_err == SVN_ERR_WC_PATH_NOT_FOUND ||
              err->apr_err == SVN_ERR_RA_ILLEGAL_URL)
            {
              svn_handle_warning2(stderr, err, "svn: ");
              svn_error_clear(svn_cmdline_fprintf(stderr, subpool, "\n"));
            }
          else
            {
              return svn_error_trace(err);
            }

          svn_error_clear(err);
          err = NULL;
          seen_nonexistent_target = TRUE;
        }
    }
  svn_pool_destroy(subpool);

  if (opt_state->xml && (! opt_state->incremental))
    SVN_ERR(svn_cl__xml_print_footer("info", pool));

  if (seen_nonexistent_target)
    return svn_error_create(
      SVN_ERR_ILLEGAL_TARGET, NULL,
      _("Could not display info for all targets because some "
        "targets don't exist"));
  else
    return SVN_NO_ERROR;
}
