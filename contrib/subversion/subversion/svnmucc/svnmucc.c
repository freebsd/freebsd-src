/*
 * svnmucc.c: Subversion Multiple URL Client
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
 *
 */

/*  Multiple URL Command Client

    Combine a list of mv, cp and rm commands on URLs into a single commit.

    How it works: the command line arguments are parsed into an array of
    action structures.  The action structures are interpreted to build a
    tree of operation structures.  The tree of operation structures is
    used to drive an RA commit editor to produce a single commit.

    To build this client, type 'make svnmucc' from the root of your
    Subversion source directory.
*/

#include <stdio.h>
#include <string.h>

#include <apr_lib.h>

#include "svn_hash.h"
#include "svn_client.h"
#include "svn_cmdline.h"
#include "svn_config.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_pools.h"
#include "svn_props.h"
#include "svn_ra.h"
#include "svn_string.h"
#include "svn_subst.h"
#include "svn_utf.h"
#include "svn_version.h"

#include "private/svn_cmdline_private.h"
#include "private/svn_ra_private.h"
#include "private/svn_string_private.h"
#include "private/svn_subr_private.h"

#include "svn_private_config.h"

static void handle_error(svn_error_t *err, apr_pool_t *pool)
{
  if (err)
    svn_handle_error2(err, stderr, FALSE, "svnmucc: ");
  svn_error_clear(err);
  if (pool)
    svn_pool_destroy(pool);
  exit(EXIT_FAILURE);
}

static apr_pool_t *
init(const char *application)
{
  svn_error_t *err;
  const svn_version_checklist_t checklist[] = {
    {"svn_client", svn_client_version},
    {"svn_subr", svn_subr_version},
    {"svn_ra", svn_ra_version},
    {NULL, NULL}
  };
  SVN_VERSION_DEFINE(my_version);

  if (svn_cmdline_init(application, stderr))
    exit(EXIT_FAILURE);

  err = svn_ver_check_list2(&my_version, checklist, svn_ver_equal);
  if (err)
    handle_error(err, NULL);

  return apr_allocator_owner_get(svn_pool_create_allocator(FALSE));
}

static svn_error_t *
open_tmp_file(apr_file_t **fp,
              void *callback_baton,
              apr_pool_t *pool)
{
  /* Open a unique file;  use APR_DELONCLOSE. */
  return svn_io_open_unique_file3(fp, NULL, NULL, svn_io_file_del_on_close,
                                  pool, pool);
}

static svn_error_t *
create_ra_callbacks(svn_ra_callbacks2_t **callbacks,
                    const char *username,
                    const char *password,
                    const char *config_dir,
                    svn_config_t *cfg_config,
                    svn_boolean_t non_interactive,
                    svn_boolean_t trust_server_cert,
                    svn_boolean_t no_auth_cache,
                    apr_pool_t *pool)
{
  SVN_ERR(svn_ra_create_callbacks(callbacks, pool));

  SVN_ERR(svn_cmdline_create_auth_baton(&(*callbacks)->auth_baton,
                                        non_interactive,
                                        username, password, config_dir,
                                        no_auth_cache,
                                        trust_server_cert,
                                        cfg_config, NULL, NULL, pool));

  (*callbacks)->open_tmp_file = open_tmp_file;

  return SVN_NO_ERROR;
}



static svn_error_t *
commit_callback(const svn_commit_info_t *commit_info,
                void *baton,
                apr_pool_t *pool)
{
  SVN_ERR(svn_cmdline_printf(pool, "r%ld committed by %s at %s\n",
                             commit_info->revision,
                             (commit_info->author
                              ? commit_info->author : "(no author)"),
                             commit_info->date));
  return SVN_NO_ERROR;
}

typedef enum action_code_t {
  ACTION_MV,
  ACTION_MKDIR,
  ACTION_CP,
  ACTION_PROPSET,
  ACTION_PROPSETF,
  ACTION_PROPDEL,
  ACTION_PUT,
  ACTION_RM
} action_code_t;

struct operation {
  enum {
    OP_OPEN,
    OP_DELETE,
    OP_ADD,
    OP_REPLACE,
    OP_PROPSET           /* only for files for which no other operation is
                            occuring; directories are OP_OPEN with non-empty
                            props */
  } operation;
  svn_node_kind_t kind;  /* to copy, mkdir, put or set revprops */
  svn_revnum_t rev;      /* to copy, valid for add and replace */
  const char *url;       /* to copy, valid for add and replace */
  const char *src_file;  /* for put, the source file for contents */
  apr_hash_t *children;  /* const char *path -> struct operation * */
  apr_hash_t *prop_mods; /* const char *prop_name ->
                            const svn_string_t *prop_value */
  apr_array_header_t *prop_dels; /* const char *prop_name deletions */
  void *baton;           /* as returned by the commit editor */
};


/* An iterator (for use via apr_table_do) which sets node properties.
   REC is a pointer to a struct driver_state. */
static svn_error_t *
change_props(const svn_delta_editor_t *editor,
             void *baton,
             struct operation *child,
             apr_pool_t *pool)
{
  apr_pool_t *iterpool = svn_pool_create(pool);

  if (child->prop_dels)
    {
      int i;
      for (i = 0; i < child->prop_dels->nelts; i++)
        {
          const char *prop_name;

          svn_pool_clear(iterpool);
          prop_name = APR_ARRAY_IDX(child->prop_dels, i, const char *);
          if (child->kind == svn_node_dir)
            SVN_ERR(editor->change_dir_prop(baton, prop_name,
                                            NULL, iterpool));
          else
            SVN_ERR(editor->change_file_prop(baton, prop_name,
                                             NULL, iterpool));
        }
    }
  if (apr_hash_count(child->prop_mods))
    {
      apr_hash_index_t *hi;
      for (hi = apr_hash_first(pool, child->prop_mods);
           hi; hi = apr_hash_next(hi))
        {
          const char *propname = svn__apr_hash_index_key(hi);
          const svn_string_t *val = svn__apr_hash_index_val(hi);

          svn_pool_clear(iterpool);
          if (child->kind == svn_node_dir)
            SVN_ERR(editor->change_dir_prop(baton, propname, val, iterpool));
          else
            SVN_ERR(editor->change_file_prop(baton, propname, val, iterpool));
        }
    }

  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}


/* Drive EDITOR to affect the change represented by OPERATION.  HEAD
   is the last-known youngest revision in the repository. */
static svn_error_t *
drive(struct operation *operation,
      svn_revnum_t head,
      const svn_delta_editor_t *editor,
      apr_pool_t *pool)
{
  apr_pool_t *subpool = svn_pool_create(pool);
  apr_hash_index_t *hi;

  for (hi = apr_hash_first(pool, operation->children);
       hi; hi = apr_hash_next(hi))
    {
      const char *key = svn__apr_hash_index_key(hi);
      struct operation *child = svn__apr_hash_index_val(hi);
      void *file_baton = NULL;

      svn_pool_clear(subpool);

      /* Deletes and replacements are simple -- delete something. */
      if (child->operation == OP_DELETE || child->operation == OP_REPLACE)
        {
          SVN_ERR(editor->delete_entry(key, head, operation->baton, subpool));
        }
      /* Opens could be for directories or files. */
      if (child->operation == OP_OPEN || child->operation == OP_PROPSET)
        {
          if (child->kind == svn_node_dir)
            {
              SVN_ERR(editor->open_directory(key, operation->baton, head,
                                             subpool, &child->baton));
            }
          else
            {
              SVN_ERR(editor->open_file(key, operation->baton, head,
                                        subpool, &file_baton));
            }
        }
      /* Adds and replacements could also be for directories or files. */
      if (child->operation == OP_ADD || child->operation == OP_REPLACE)
        {
          if (child->kind == svn_node_dir)
            {
              SVN_ERR(editor->add_directory(key, operation->baton,
                                            child->url, child->rev,
                                            subpool, &child->baton));
            }
          else
            {
              SVN_ERR(editor->add_file(key, operation->baton, child->url,
                                       child->rev, subpool, &file_baton));
            }
        }
      /* If there's a source file and an open file baton, we get to
         change textual contents. */
      if ((child->src_file) && (file_baton))
        {
          svn_txdelta_window_handler_t handler;
          void *handler_baton;
          svn_stream_t *contents;

          SVN_ERR(editor->apply_textdelta(file_baton, NULL, subpool,
                                          &handler, &handler_baton));
          if (strcmp(child->src_file, "-") != 0)
            {
              SVN_ERR(svn_stream_open_readonly(&contents, child->src_file,
                                               pool, pool));
            }
          else
            {
              SVN_ERR(svn_stream_for_stdin(&contents, pool));
            }
          SVN_ERR(svn_txdelta_send_stream(contents, handler,
                                          handler_baton, NULL, pool));
        }
      /* If we opened a file, we need to apply outstanding propmods,
         then close it. */
      if (file_baton)
        {
          if (child->kind == svn_node_file)
            {
              SVN_ERR(change_props(editor, file_baton, child, subpool));
            }
          SVN_ERR(editor->close_file(file_baton, NULL, subpool));
        }
      /* If we opened, added, or replaced a directory, we need to
         recurse, apply outstanding propmods, and then close it. */
      if ((child->kind == svn_node_dir)
          && child->operation != OP_DELETE)
        {
          SVN_ERR(change_props(editor, child->baton, child, subpool));

          SVN_ERR(drive(child, head, editor, subpool));

          SVN_ERR(editor->close_directory(child->baton, subpool));
        }
    }
  svn_pool_destroy(subpool);
  return SVN_NO_ERROR;
}


/* Find the operation associated with PATH, which is a single-path
   component representing a child of the path represented by
   OPERATION.  If no such child operation exists, create a new one of
   type OP_OPEN. */
static struct operation *
get_operation(const char *path,
              struct operation *operation,
              apr_pool_t *pool)
{
  struct operation *child = svn_hash_gets(operation->children, path);
  if (! child)
    {
      child = apr_pcalloc(pool, sizeof(*child));
      child->children = apr_hash_make(pool);
      child->operation = OP_OPEN;
      child->rev = SVN_INVALID_REVNUM;
      child->kind = svn_node_dir;
      child->prop_mods = apr_hash_make(pool);
      child->prop_dels = apr_array_make(pool, 1, sizeof(const char *));
      svn_hash_sets(operation->children, path, child);
    }
  return child;
}

/* Return the portion of URL that is relative to ANCHOR (URI-decoded). */
static const char *
subtract_anchor(const char *anchor, const char *url, apr_pool_t *pool)
{
  return svn_uri_skip_ancestor(anchor, url, pool);
}

/* Add PATH to the operations tree rooted at OPERATION, creating any
   intermediate nodes that are required.  Here's what's expected for
   each action type:

      ACTION          URL    REV      SRC-FILE  PROPNAME
      ------------    -----  -------  --------  --------
      ACTION_MKDIR    NULL   invalid  NULL      NULL
      ACTION_CP       valid  valid    NULL      NULL
      ACTION_PUT      NULL   invalid  valid     NULL
      ACTION_RM       NULL   invalid  NULL      NULL
      ACTION_PROPSET  valid  invalid  NULL      valid
      ACTION_PROPDEL  valid  invalid  NULL      valid

   Node type information is obtained for any copy source (to determine
   whether to create a file or directory) and for any deleted path (to
   ensure it exists since svn_delta_editor_t->delete_entry doesn't
   return an error on non-existent nodes). */
static svn_error_t *
build(action_code_t action,
      const char *path,
      const char *url,
      svn_revnum_t rev,
      const char *prop_name,
      const svn_string_t *prop_value,
      const char *src_file,
      svn_revnum_t head,
      const char *anchor,
      svn_ra_session_t *session,
      struct operation *operation,
      apr_pool_t *pool)
{
  apr_array_header_t *path_bits = svn_path_decompose(path, pool);
  const char *path_so_far = "";
  const char *copy_src = NULL;
  svn_revnum_t copy_rev = SVN_INVALID_REVNUM;
  int i;

  /* Look for any previous operations we've recognized for PATH.  If
     any of PATH's ancestors have not yet been traversed, we'll be
     creating OP_OPEN operations for them as we walk down PATH's path
     components. */
  for (i = 0; i < path_bits->nelts; ++i)
    {
      const char *path_bit = APR_ARRAY_IDX(path_bits, i, const char *);
      path_so_far = svn_relpath_join(path_so_far, path_bit, pool);
      operation = get_operation(path_so_far, operation, pool);

      /* If we cross a replace- or add-with-history, remember the
      source of those things in case we need to lookup the node kind
      of one of their children.  And if this isn't such a copy,
      but we've already seen one in of our parent paths, we just need
      to extend that copy source path by our current path
      component. */
      if (operation->url
          && SVN_IS_VALID_REVNUM(operation->rev)
          && (operation->operation == OP_REPLACE
              || operation->operation == OP_ADD))
        {
          copy_src = subtract_anchor(anchor, operation->url, pool);
          copy_rev = operation->rev;
        }
      else if (copy_src)
        {
          copy_src = svn_relpath_join(copy_src, path_bit, pool);
        }
    }

  /* Handle property changes. */
  if (prop_name)
    {
      if (operation->operation == OP_DELETE)
        return svn_error_createf(SVN_ERR_BAD_URL, NULL,
                                 "cannot set properties on a location being"
                                 " deleted ('%s')", path);
      /* If we're not adding this thing ourselves, check for existence.  */
      if (! ((operation->operation == OP_ADD) ||
             (operation->operation == OP_REPLACE)))
        {
          SVN_ERR(svn_ra_check_path(session,
                                    copy_src ? copy_src : path,
                                    copy_src ? copy_rev : head,
                                    &operation->kind, pool));
          if (operation->kind == svn_node_none)
            return svn_error_createf(SVN_ERR_BAD_URL, NULL,
                                     "propset: '%s' not found", path);
          else if ((operation->kind == svn_node_file)
                   && (operation->operation == OP_OPEN))
            operation->operation = OP_PROPSET;
        }
      if (! prop_value)
        APR_ARRAY_PUSH(operation->prop_dels, const char *) = prop_name;
      else
        svn_hash_sets(operation->prop_mods, prop_name, prop_value);
      if (!operation->rev)
        operation->rev = rev;
      return SVN_NO_ERROR;
    }

  /* We won't fuss about multiple operations on the same path in the
     following cases:

       - the prior operation was, in fact, a no-op (open)
       - the prior operation was a propset placeholder
       - the prior operation was a deletion

     Note: while the operation structure certainly supports the
     ability to do a copy of a file followed by a put of new contents
     for the file, we don't let that happen (yet).
  */
  if (operation->operation != OP_OPEN
      && operation->operation != OP_PROPSET
      && operation->operation != OP_DELETE)
    return svn_error_createf(SVN_ERR_BAD_URL, NULL,
                             "unsupported multiple operations on '%s'", path);

  /* For deletions, we validate that there's actually something to
     delete.  If this is a deletion of the child of a copied
     directory, we need to remember to look in the copy source tree to
     verify that this thing actually exists. */
  if (action == ACTION_RM)
    {
      operation->operation = OP_DELETE;
      SVN_ERR(svn_ra_check_path(session,
                                copy_src ? copy_src : path,
                                copy_src ? copy_rev : head,
                                &operation->kind, pool));
      if (operation->kind == svn_node_none)
        {
          if (copy_src && strcmp(path, copy_src))
            return svn_error_createf(SVN_ERR_BAD_URL, NULL,
                                     "'%s' (from '%s:%ld') not found",
                                     path, copy_src, copy_rev);
          else
            return svn_error_createf(SVN_ERR_BAD_URL, NULL, "'%s' not found",
                                     path);
        }
    }
  /* Handle copy operations (which can be adds or replacements). */
  else if (action == ACTION_CP)
    {
      if (rev > head)
        return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                                "Copy source revision cannot be younger "
                                "than base revision");
      operation->operation =
        operation->operation == OP_DELETE ? OP_REPLACE : OP_ADD;
      if (operation->operation == OP_ADD)
        {
          /* There is a bug in the current version of mod_dav_svn
             which incorrectly replaces existing directories.
             Therefore we need to check if the target exists
             and raise an error here. */
          SVN_ERR(svn_ra_check_path(session,
                                    copy_src ? copy_src : path,
                                    copy_src ? copy_rev : head,
                                    &operation->kind, pool));
          if (operation->kind != svn_node_none)
            {
              if (copy_src && strcmp(path, copy_src))
                return svn_error_createf(SVN_ERR_BAD_URL, NULL,
                                         "'%s' (from '%s:%ld') already exists",
                                         path, copy_src, copy_rev);
              else
                return svn_error_createf(SVN_ERR_BAD_URL, NULL,
                                         "'%s' already exists", path);
            }
        }
      SVN_ERR(svn_ra_check_path(session, subtract_anchor(anchor, url, pool),
                                rev, &operation->kind, pool));
      if (operation->kind == svn_node_none)
        return svn_error_createf(SVN_ERR_BAD_URL, NULL,
                                 "'%s' not found",
                                  subtract_anchor(anchor, url, pool));
      operation->url = url;
      operation->rev = rev;
    }
  /* Handle mkdir operations (which can be adds or replacements). */
  else if (action == ACTION_MKDIR)
    {
      operation->operation =
        operation->operation == OP_DELETE ? OP_REPLACE : OP_ADD;
      operation->kind = svn_node_dir;
    }
  /* Handle put operations (which can be adds, replacements, or opens). */
  else if (action == ACTION_PUT)
    {
      if (operation->operation == OP_DELETE)
        {
          operation->operation = OP_REPLACE;
        }
      else
        {
          SVN_ERR(svn_ra_check_path(session,
                                    copy_src ? copy_src : path,
                                    copy_src ? copy_rev : head,
                                    &operation->kind, pool));
          if (operation->kind == svn_node_file)
            operation->operation = OP_OPEN;
          else if (operation->kind == svn_node_none)
            operation->operation = OP_ADD;
          else
            return svn_error_createf(SVN_ERR_BAD_URL, NULL,
                                     "'%s' is not a file", path);
        }
      operation->kind = svn_node_file;
      operation->src_file = src_file;
    }
  else
    {
      /* We shouldn't get here. */
      SVN_ERR_MALFUNCTION();
    }

  return SVN_NO_ERROR;
}

struct action {
  action_code_t action;

  /* revision (copy-from-rev of path[0] for cp; base-rev for put) */
  svn_revnum_t rev;

  /* action  path[0]  path[1]
   * ------  -------  -------
   * mv      source   target
   * mkdir   target   (null)
   * cp      source   target
   * put     target   source
   * rm      target   (null)
   * propset target   (null)
   */
  const char *path[2];

  /* property name/value */
  const char *prop_name;
  const svn_string_t *prop_value;
};

struct fetch_baton
{
  svn_ra_session_t *session;
  svn_revnum_t head;
};

static svn_error_t *
fetch_base_func(const char **filename,
                void *baton,
                const char *path,
                svn_revnum_t base_revision,
                apr_pool_t *result_pool,
                apr_pool_t *scratch_pool)
{
  struct fetch_baton *fb = baton;
  svn_stream_t *fstream;
  svn_error_t *err;

  if (! SVN_IS_VALID_REVNUM(base_revision))
    base_revision = fb->head;

  SVN_ERR(svn_stream_open_unique(&fstream, filename, NULL,
                                 svn_io_file_del_on_pool_cleanup,
                                 result_pool, scratch_pool));

  err = svn_ra_get_file(fb->session, path, base_revision, fstream, NULL, NULL,
                         scratch_pool);
  if (err && err->apr_err == SVN_ERR_FS_NOT_FOUND)
    {
      svn_error_clear(err);
      SVN_ERR(svn_stream_close(fstream));

      *filename = NULL;
      return SVN_NO_ERROR;
    }
  else if (err)
    return svn_error_trace(err);

  SVN_ERR(svn_stream_close(fstream));

  return SVN_NO_ERROR;
}

static svn_error_t *
fetch_props_func(apr_hash_t **props,
                 void *baton,
                 const char *path,
                 svn_revnum_t base_revision,
                 apr_pool_t *result_pool,
                 apr_pool_t *scratch_pool)
{
  struct fetch_baton *fb = baton;
  svn_node_kind_t node_kind;

  if (! SVN_IS_VALID_REVNUM(base_revision))
    base_revision = fb->head;

  SVN_ERR(svn_ra_check_path(fb->session, path, base_revision, &node_kind,
                            scratch_pool));

  if (node_kind == svn_node_file)
    {
      SVN_ERR(svn_ra_get_file(fb->session, path, base_revision, NULL, NULL,
                              props, result_pool));
    }
  else if (node_kind == svn_node_dir)
    {
      apr_array_header_t *tmp_props;

      SVN_ERR(svn_ra_get_dir2(fb->session, NULL, NULL, props, path,
                              base_revision, 0 /* Dirent fields */,
                              result_pool));
      tmp_props = svn_prop_hash_to_array(*props, result_pool);
      SVN_ERR(svn_categorize_props(tmp_props, NULL, NULL, &tmp_props,
                                   result_pool));
      *props = svn_prop_array_to_hash(tmp_props, result_pool);
    }
  else
    {
      *props = apr_hash_make(result_pool);
    }

  return SVN_NO_ERROR;
}

static svn_error_t *
fetch_kind_func(svn_node_kind_t *kind,
                void *baton,
                const char *path,
                svn_revnum_t base_revision,
                apr_pool_t *scratch_pool)
{
  struct fetch_baton *fb = baton;

  if (! SVN_IS_VALID_REVNUM(base_revision))
    base_revision = fb->head;

  SVN_ERR(svn_ra_check_path(fb->session, path, base_revision, kind,
                             scratch_pool));

  return SVN_NO_ERROR;
}

static svn_delta_shim_callbacks_t *
get_shim_callbacks(svn_ra_session_t *session,
                   svn_revnum_t head,
                   apr_pool_t *result_pool)
{
  svn_delta_shim_callbacks_t *callbacks =
                            svn_delta_shim_callbacks_default(result_pool);
  struct fetch_baton *fb = apr_pcalloc(result_pool, sizeof(*fb));

  fb->session = session;
  fb->head = head;

  callbacks->fetch_props_func = fetch_props_func;
  callbacks->fetch_kind_func = fetch_kind_func;
  callbacks->fetch_base_func = fetch_base_func;
  callbacks->fetch_baton = fb;

  return callbacks;
}

static svn_error_t *
execute(const apr_array_header_t *actions,
        const char *anchor,
        apr_hash_t *revprops,
        const char *username,
        const char *password,
        const char *config_dir,
        const apr_array_header_t *config_options,
        svn_boolean_t non_interactive,
        svn_boolean_t trust_server_cert,
        svn_boolean_t no_auth_cache,
        svn_revnum_t base_revision,
        apr_pool_t *pool)
{
  svn_ra_session_t *session;
  svn_ra_session_t *aux_session;
  const char *repos_root;
  svn_revnum_t head;
  const svn_delta_editor_t *editor;
  svn_ra_callbacks2_t *ra_callbacks;
  void *editor_baton;
  struct operation root;
  svn_error_t *err;
  apr_hash_t *config;
  svn_config_t *cfg_config;
  int i;

  SVN_ERR(svn_config_get_config(&config, config_dir, pool));
  SVN_ERR(svn_cmdline__apply_config_options(config, config_options,
                                            "svnmucc: ", "--config-option"));
  cfg_config = svn_hash_gets(config, SVN_CONFIG_CATEGORY_CONFIG);

  if (! svn_hash_gets(revprops, SVN_PROP_REVISION_LOG))
    {
      svn_string_t *msg = svn_string_create("", pool);

      /* If we can do so, try to pop up $EDITOR to fetch a log message. */
      if (non_interactive)
        {
          return svn_error_create
            (SVN_ERR_CL_INSUFFICIENT_ARGS, NULL,
             _("Cannot invoke editor to get log message "
               "when non-interactive"));
        }
      else
        {
          SVN_ERR(svn_cmdline__edit_string_externally(
                      &msg, NULL, NULL, "", msg, "svnmucc-commit", config,
                      TRUE, NULL, apr_hash_pool_get(revprops)));
        }

      svn_hash_sets(revprops, SVN_PROP_REVISION_LOG, msg);
    }

  SVN_ERR(create_ra_callbacks(&ra_callbacks, username, password, config_dir,
                              cfg_config, non_interactive, trust_server_cert,
                              no_auth_cache, pool));
  SVN_ERR(svn_ra_open4(&session, NULL, anchor, NULL, ra_callbacks,
                       NULL, config, pool));
  /* Open, then reparent to avoid AUTHZ errors when opening the reposroot */
  SVN_ERR(svn_ra_open4(&aux_session, NULL, anchor, NULL, ra_callbacks,
                       NULL, config, pool));
  SVN_ERR(svn_ra_get_repos_root2(aux_session, &repos_root, pool));
  SVN_ERR(svn_ra_reparent(aux_session, repos_root, pool));
  SVN_ERR(svn_ra_get_latest_revnum(session, &head, pool));

  /* Reparent to ANCHOR's dir, if ANCHOR is not a directory. */
  {
    svn_node_kind_t kind;

    SVN_ERR(svn_ra_check_path(aux_session,
                              svn_uri_skip_ancestor(repos_root, anchor, pool),
                              head, &kind, pool));
    if (kind != svn_node_dir)
      {
        anchor = svn_uri_dirname(anchor, pool);
        SVN_ERR(svn_ra_reparent(session, anchor, pool));
      }
  }

  if (SVN_IS_VALID_REVNUM(base_revision))
    {
      if (base_revision > head)
        return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
                                 "No such revision %ld (youngest is %ld)",
                                 base_revision, head);
      head = base_revision;
    }

  memset(&root, 0, sizeof(root));
  root.children = apr_hash_make(pool);
  root.operation = OP_OPEN;
  root.kind = svn_node_dir; /* For setting properties */
  root.prop_mods = apr_hash_make(pool);
  root.prop_dels = apr_array_make(pool, 1, sizeof(const char *));

  for (i = 0; i < actions->nelts; ++i)
    {
      struct action *action = APR_ARRAY_IDX(actions, i, struct action *);
      const char *path1, *path2;
      switch (action->action)
        {
        case ACTION_MV:
          path1 = subtract_anchor(anchor, action->path[0], pool);
          path2 = subtract_anchor(anchor, action->path[1], pool);
          SVN_ERR(build(ACTION_RM, path1, NULL,
                        SVN_INVALID_REVNUM, NULL, NULL, NULL, head, anchor,
                        session, &root, pool));
          SVN_ERR(build(ACTION_CP, path2, action->path[0],
                        head, NULL, NULL, NULL, head, anchor,
                        session, &root, pool));
          break;
        case ACTION_CP:
          path2 = subtract_anchor(anchor, action->path[1], pool);
          if (action->rev == SVN_INVALID_REVNUM)
            action->rev = head;
          SVN_ERR(build(ACTION_CP, path2, action->path[0],
                        action->rev, NULL, NULL, NULL, head, anchor,
                        session, &root, pool));
          break;
        case ACTION_RM:
          path1 = subtract_anchor(anchor, action->path[0], pool);
          SVN_ERR(build(ACTION_RM, path1, NULL,
                        SVN_INVALID_REVNUM, NULL, NULL, NULL, head, anchor,
                        session, &root, pool));
          break;
        case ACTION_MKDIR:
          path1 = subtract_anchor(anchor, action->path[0], pool);
          SVN_ERR(build(ACTION_MKDIR, path1, action->path[0],
                        SVN_INVALID_REVNUM, NULL, NULL, NULL, head, anchor,
                        session, &root, pool));
          break;
        case ACTION_PUT:
          path1 = subtract_anchor(anchor, action->path[0], pool);
          SVN_ERR(build(ACTION_PUT, path1, action->path[0],
                        SVN_INVALID_REVNUM, NULL, NULL, action->path[1],
                        head, anchor, session, &root, pool));
          break;
        case ACTION_PROPSET:
        case ACTION_PROPDEL:
          path1 = subtract_anchor(anchor, action->path[0], pool);
          SVN_ERR(build(action->action, path1, action->path[0],
                        SVN_INVALID_REVNUM,
                        action->prop_name, action->prop_value,
                        NULL, head, anchor, session, &root, pool));
          break;
        case ACTION_PROPSETF:
        default:
          SVN_ERR_MALFUNCTION_NO_RETURN();
        }
    }

  SVN_ERR(svn_ra__register_editor_shim_callbacks(session,
                            get_shim_callbacks(aux_session, head, pool)));
  SVN_ERR(svn_ra_get_commit_editor3(session, &editor, &editor_baton, revprops,
                                    commit_callback, NULL, NULL, FALSE, pool));

  SVN_ERR(editor->open_root(editor_baton, head, pool, &root.baton));
  err = change_props(editor, root.baton, &root, pool);
  if (!err)
    err = drive(&root, head, editor, pool);
  if (!err)
    err = editor->close_directory(root.baton, pool);
  if (!err)
    err = editor->close_edit(editor_baton, pool);

  if (err)
    err = svn_error_compose_create(err,
                                   editor->abort_edit(editor_baton, pool));

  return err;
}

static svn_error_t *
read_propvalue_file(const svn_string_t **value_p,
                    const char *filename,
                    apr_pool_t *pool)
{
  svn_stringbuf_t *value;
  apr_pool_t *scratch_pool = svn_pool_create(pool);

  SVN_ERR(svn_stringbuf_from_file2(&value, filename, scratch_pool));
  *value_p = svn_string_create_from_buf(value, pool);
  svn_pool_destroy(scratch_pool);
  return SVN_NO_ERROR;
}

/* Perform the typical suite of manipulations for user-provided URLs
   on URL, returning the result (allocated from POOL): IRI-to-URI
   conversion, auto-escaping, and canonicalization. */
static const char *
sanitize_url(const char *url,
             apr_pool_t *pool)
{
  url = svn_path_uri_from_iri(url, pool);
  url = svn_path_uri_autoescape(url, pool);
  return svn_uri_canonicalize(url, pool);
}

static void
usage(apr_pool_t *pool, int exit_val)
{
  FILE *stream = exit_val == EXIT_SUCCESS ? stdout : stderr;
  svn_error_clear(svn_cmdline_fputs(
    _("Subversion multiple URL command client\n"
      "usage: svnmucc ACTION...\n"
      "\n"
      "  Perform one or more Subversion repository URL-based ACTIONs, committing\n"
      "  the result as a (single) new revision.\n"
      "\n"
      "Actions:\n"
      "  cp REV SRC-URL DST-URL : copy SRC-URL@REV to DST-URL\n"
      "  mkdir URL              : create new directory URL\n"
      "  mv SRC-URL DST-URL     : move SRC-URL to DST-URL\n"
      "  rm URL                 : delete URL\n"
      "  put SRC-FILE URL       : add or modify file URL with contents copied from\n"
      "                           SRC-FILE (use \"-\" to read from standard input)\n"
      "  propset NAME VALUE URL : set property NAME on URL to VALUE\n"
      "  propsetf NAME FILE URL : set property NAME on URL to value read from FILE\n"
      "  propdel NAME URL       : delete property NAME from URL\n"
      "\n"
      "Valid options:\n"
      "  -h, -? [--help]        : display this text\n"
      "  -m [--message] ARG     : use ARG as a log message\n"
      "  -F [--file] ARG        : read log message from file ARG\n"
      "  -u [--username] ARG    : commit the changes as username ARG\n"
      "  -p [--password] ARG    : use ARG as the password\n"
      "  -U [--root-url] ARG    : interpret all action URLs relative to ARG\n"
      "  -r [--revision] ARG    : use revision ARG as baseline for changes\n"
      "  --with-revprop ARG     : set revision property in the following format:\n"
      "                               NAME[=VALUE]\n"
      "  --non-interactive      : do no interactive prompting (default is to\n"
      "                           prompt only if standard input is a terminal)\n"
      "  --force-interactive    : do interactive prompting even if standard\n"
      "                           input is not a terminal\n"
      "  --trust-server-cert    : accept SSL server certificates from unknown\n"
      "                           certificate authorities without prompting (but\n"
      "                           only with '--non-interactive')\n"
      "  -X [--extra-args] ARG  : append arguments from file ARG (one per line;\n"
      "                           use \"-\" to read from standard input)\n"
      "  --config-dir ARG       : use ARG to override the config directory\n"
      "  --config-option ARG    : use ARG to override a configuration option\n"
      "  --no-auth-cache        : do not cache authentication tokens\n"
      "  --version              : print version information\n"),
                  stream, pool));
  svn_pool_destroy(pool);
  exit(exit_val);
}

static void
insufficient(apr_pool_t *pool)
{
  handle_error(svn_error_create(SVN_ERR_INCORRECT_PARAMS, NULL,
                                "insufficient arguments"),
               pool);
}

static svn_error_t *
display_version(apr_getopt_t *os, apr_pool_t *pool)
{
  const char *ra_desc_start
    = "The following repository access (RA) modules are available:\n\n";
  svn_stringbuf_t *version_footer;

  version_footer = svn_stringbuf_create(ra_desc_start, pool);
  SVN_ERR(svn_ra_print_modules(version_footer, pool));

  SVN_ERR(svn_opt_print_help4(os, "svnmucc", TRUE, FALSE, FALSE,
                              version_footer->data,
                              NULL, NULL, NULL, NULL, NULL, pool));

  return SVN_NO_ERROR;
}

/* Return an error about the mutual exclusivity of the -m, -F, and
   --with-revprop=svn:log command-line options. */
static svn_error_t *
mutually_exclusive_logs_error(void)
{
  return svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                          _("--message (-m), --file (-F), and "
                            "--with-revprop=svn:log are mutually "
                            "exclusive"));
}

/* Ensure that the REVPROPS hash contains a command-line-provided log
   message, if any, and that there was but one source of such a thing
   provided on that command-line.  */
static svn_error_t *
sanitize_log_sources(apr_hash_t *revprops,
                     const char *message,
                     svn_stringbuf_t *filedata)
{
  apr_pool_t *hash_pool = apr_hash_pool_get(revprops);

  /* If we already have a log message in the revprop hash, then just
     make sure the user didn't try to also use -m or -F.  Otherwise,
     we need to consult -m or -F to find a log message, if any. */
  if (svn_hash_gets(revprops, SVN_PROP_REVISION_LOG))
    {
      if (filedata || message)
        return mutually_exclusive_logs_error();
    }
  else if (filedata)
    {
      if (message)
        return mutually_exclusive_logs_error();

      SVN_ERR(svn_utf_cstring_to_utf8(&message, filedata->data, hash_pool));
      svn_hash_sets(revprops, SVN_PROP_REVISION_LOG,
                    svn_stringbuf__morph_into_string(filedata));
    }
  else if (message)
    {
      svn_hash_sets(revprops, SVN_PROP_REVISION_LOG,
                    svn_string_create(message, hash_pool));
    }

  return SVN_NO_ERROR;
}

int
main(int argc, const char **argv)
{
  apr_pool_t *pool = init("svnmucc");
  apr_array_header_t *actions = apr_array_make(pool, 1,
                                               sizeof(struct action *));
  const char *anchor = NULL;
  svn_error_t *err = SVN_NO_ERROR;
  apr_getopt_t *opts;
  enum {
    config_dir_opt = SVN_OPT_FIRST_LONGOPT_ID,
    config_inline_opt,
    no_auth_cache_opt,
    version_opt,
    with_revprop_opt,
    non_interactive_opt,
    force_interactive_opt,
    trust_server_cert_opt
  };
  static const apr_getopt_option_t options[] = {
    {"message", 'm', 1, ""},
    {"file", 'F', 1, ""},
    {"username", 'u', 1, ""},
    {"password", 'p', 1, ""},
    {"root-url", 'U', 1, ""},
    {"revision", 'r', 1, ""},
    {"with-revprop",  with_revprop_opt, 1, ""},
    {"extra-args", 'X', 1, ""},
    {"help", 'h', 0, ""},
    {NULL, '?', 0, ""},
    {"non-interactive", non_interactive_opt, 0, ""},
    {"force-interactive", force_interactive_opt, 0, ""},
    {"trust-server-cert", trust_server_cert_opt, 0, ""},
    {"config-dir", config_dir_opt, 1, ""},
    {"config-option",  config_inline_opt, 1, ""},
    {"no-auth-cache",  no_auth_cache_opt, 0, ""},
    {"version", version_opt, 0, ""},
    {NULL, 0, 0, NULL}
  };
  const char *message = NULL;
  svn_stringbuf_t *filedata = NULL;
  const char *username = NULL, *password = NULL;
  const char *root_url = NULL, *extra_args_file = NULL;
  const char *config_dir = NULL;
  apr_array_header_t *config_options;
  svn_boolean_t non_interactive = FALSE;
  svn_boolean_t force_interactive = FALSE;
  svn_boolean_t trust_server_cert = FALSE;
  svn_boolean_t no_auth_cache = FALSE;
  svn_revnum_t base_revision = SVN_INVALID_REVNUM;
  apr_array_header_t *action_args;
  apr_hash_t *revprops = apr_hash_make(pool);
  int i;

  config_options = apr_array_make(pool, 0,
                                  sizeof(svn_cmdline__config_argument_t*));

  apr_getopt_init(&opts, pool, argc, argv);
  opts->interleave = 1;
  while (1)
    {
      int opt;
      const char *arg;
      const char *opt_arg;

      apr_status_t status = apr_getopt_long(opts, options, &opt, &arg);
      if (APR_STATUS_IS_EOF(status))
        break;
      if (status != APR_SUCCESS)
        handle_error(svn_error_wrap_apr(status, "getopt failure"), pool);
      switch(opt)
        {
        case 'm':
          err = svn_utf_cstring_to_utf8(&message, arg, pool);
          if (err)
            handle_error(err, pool);
          break;
        case 'F':
          {
            const char *arg_utf8;
            err = svn_utf_cstring_to_utf8(&arg_utf8, arg, pool);
            if (! err)
              err = svn_stringbuf_from_file2(&filedata, arg, pool);
            if (err)
              handle_error(err, pool);
          }
          break;
        case 'u':
          username = apr_pstrdup(pool, arg);
          break;
        case 'p':
          password = apr_pstrdup(pool, arg);
          break;
        case 'U':
          err = svn_utf_cstring_to_utf8(&root_url, arg, pool);
          if (err)
            handle_error(err, pool);
          if (! svn_path_is_url(root_url))
            handle_error(svn_error_createf(SVN_ERR_INCORRECT_PARAMS, NULL,
                                           "'%s' is not a URL\n", root_url),
                         pool);
          root_url = sanitize_url(root_url, pool);
          break;
        case 'r':
          {
            char *digits_end = NULL;
            base_revision = strtol(arg, &digits_end, 10);
            if ((! SVN_IS_VALID_REVNUM(base_revision))
                || (! digits_end)
                || *digits_end)
              handle_error(svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR,
                                            NULL, "Invalid revision number"),
                           pool);
          }
          break;
        case with_revprop_opt:
          err = svn_opt_parse_revprop(&revprops, arg, pool);
          if (err != SVN_NO_ERROR)
            handle_error(err, pool);
          break;
        case 'X':
          extra_args_file = apr_pstrdup(pool, arg);
          break;
        case non_interactive_opt:
          non_interactive = TRUE;
          break;
        case force_interactive_opt:
          force_interactive = TRUE;
          break;
        case trust_server_cert_opt:
          trust_server_cert = TRUE;
          break;
        case config_dir_opt:
          err = svn_utf_cstring_to_utf8(&config_dir, arg, pool);
          if (err)
            handle_error(err, pool);
          break;
        case config_inline_opt:
          err = svn_utf_cstring_to_utf8(&opt_arg, arg, pool);
          if (err)
            handle_error(err, pool);

          err = svn_cmdline__parse_config_option(config_options, opt_arg,
                                                 pool);
          if (err)
            handle_error(err, pool);
          break;
        case no_auth_cache_opt:
          no_auth_cache = TRUE;
          break;
        case version_opt:
          SVN_INT_ERR(display_version(opts, pool));
          exit(EXIT_SUCCESS);
          break;
        case 'h':
        case '?':
          usage(pool, EXIT_SUCCESS);
          break;
        }
    }

  if (non_interactive && force_interactive)
    {
      err = svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                             _("--non-interactive and --force-interactive "
                               "are mutually exclusive"));
      return svn_cmdline_handle_exit_error(err, pool, "svnmucc: ");
    }
  else
    non_interactive = !svn_cmdline__be_interactive(non_interactive,
                                                   force_interactive);

  if (trust_server_cert && !non_interactive)
    {
      err = svn_error_create(SVN_ERR_CL_ARG_PARSING_ERROR, NULL,
                             _("--trust-server-cert requires "
                               "--non-interactive"));
      return svn_cmdline_handle_exit_error(err, pool, "svnmucc: ");
    }

  /* Make sure we have a log message to use. */
  err = sanitize_log_sources(revprops, message, filedata);
  if (err)
    handle_error(err, pool);

  /* Copy the rest of our command-line arguments to an array,
     UTF-8-ing them along the way. */
  action_args = apr_array_make(pool, opts->argc, sizeof(const char *));
  while (opts->ind < opts->argc)
    {
      const char *arg = opts->argv[opts->ind++];
      if ((err = svn_utf_cstring_to_utf8(&(APR_ARRAY_PUSH(action_args,
                                                          const char *)),
                                         arg, pool)))
        handle_error(err, pool);
    }

  /* If there are extra arguments in a supplementary file, tack those
     on, too (again, in UTF8 form). */
  if (extra_args_file)
    {
      const char *extra_args_file_utf8;
      svn_stringbuf_t *contents, *contents_utf8;

      err = svn_utf_cstring_to_utf8(&extra_args_file_utf8,
                                    extra_args_file, pool);
      if (! err)
        err = svn_stringbuf_from_file2(&contents, extra_args_file_utf8, pool);
      if (! err)
        err = svn_utf_stringbuf_to_utf8(&contents_utf8, contents, pool);
      if (err)
        handle_error(err, pool);
      svn_cstring_split_append(action_args, contents_utf8->data, "\n\r",
                               FALSE, pool);
    }

  /* Now, we iterate over the combined set of arguments -- our actions. */
  for (i = 0; i < action_args->nelts; )
    {
      int j, num_url_args;
      const char *action_string = APR_ARRAY_IDX(action_args, i, const char *);
      struct action *action = apr_pcalloc(pool, sizeof(*action));

      /* First, parse the action. */
      if (! strcmp(action_string, "mv"))
        action->action = ACTION_MV;
      else if (! strcmp(action_string, "cp"))
        action->action = ACTION_CP;
      else if (! strcmp(action_string, "mkdir"))
        action->action = ACTION_MKDIR;
      else if (! strcmp(action_string, "rm"))
        action->action = ACTION_RM;
      else if (! strcmp(action_string, "put"))
        action->action = ACTION_PUT;
      else if (! strcmp(action_string, "propset"))
        action->action = ACTION_PROPSET;
      else if (! strcmp(action_string, "propsetf"))
        action->action = ACTION_PROPSETF;
      else if (! strcmp(action_string, "propdel"))
        action->action = ACTION_PROPDEL;
      else if (! strcmp(action_string, "?") || ! strcmp(action_string, "h")
               || ! strcmp(action_string, "help"))
        usage(pool, EXIT_SUCCESS);
      else
        handle_error(svn_error_createf(SVN_ERR_INCORRECT_PARAMS, NULL,
                                       "'%s' is not an action\n",
                                       action_string), pool);
      if (++i == action_args->nelts)
        insufficient(pool);

      /* For copies, there should be a revision number next. */
      if (action->action == ACTION_CP)
        {
          const char *rev_str = APR_ARRAY_IDX(action_args, i, const char *);
          if (strcmp(rev_str, "head") == 0)
            action->rev = SVN_INVALID_REVNUM;
          else if (strcmp(rev_str, "HEAD") == 0)
            action->rev = SVN_INVALID_REVNUM;
          else
            {
              char *end;

              while (*rev_str == 'r')
                ++rev_str;

              action->rev = strtol(rev_str, &end, 0);
              if (*end)
                handle_error(svn_error_createf(SVN_ERR_INCORRECT_PARAMS, NULL,
                                               "'%s' is not a revision\n",
                                               rev_str), pool);
            }
          if (++i == action_args->nelts)
            insufficient(pool);
        }
      else
        {
          action->rev = SVN_INVALID_REVNUM;
        }

      /* For puts, there should be a local file next. */
      if (action->action == ACTION_PUT)
        {
          action->path[1] =
            svn_dirent_internal_style(APR_ARRAY_IDX(action_args, i,
                                                    const char *), pool);
          if (++i == action_args->nelts)
            insufficient(pool);
        }

      /* For propset, propsetf, and propdel, a property name (and
         maybe a property value or file which contains one) comes next. */
      if ((action->action == ACTION_PROPSET)
          || (action->action == ACTION_PROPSETF)
          || (action->action == ACTION_PROPDEL))
        {
          action->prop_name = APR_ARRAY_IDX(action_args, i, const char *);
          if (++i == action_args->nelts)
            insufficient(pool);

          if (action->action == ACTION_PROPDEL)
            {
              action->prop_value = NULL;
            }
          else if (action->action == ACTION_PROPSET)
            {
              action->prop_value =
                svn_string_create(APR_ARRAY_IDX(action_args, i,
                                                const char *), pool);
              if (++i == action_args->nelts)
                insufficient(pool);
            }
          else
            {
              const char *propval_file =
                svn_dirent_internal_style(APR_ARRAY_IDX(action_args, i,
                                                        const char *), pool);

              if (++i == action_args->nelts)
                insufficient(pool);

              err = read_propvalue_file(&(action->prop_value),
                                        propval_file, pool);
              if (err)
                handle_error(err, pool);

              action->action = ACTION_PROPSET;
            }

          if (action->prop_value
              && svn_prop_needs_translation(action->prop_name))
            {
              svn_string_t *translated_value;
              err = svn_subst_translate_string2(&translated_value, NULL,
                                                NULL, action->prop_value, NULL,
                                                FALSE, pool, pool);
              if (err)
                handle_error(
                    svn_error_quick_wrap(err,
                                         "Error normalizing property value"),
                    pool);
              action->prop_value = translated_value;
            }
        }

      /* How many URLs does this action expect? */
      if (action->action == ACTION_RM
          || action->action == ACTION_MKDIR
          || action->action == ACTION_PUT
          || action->action == ACTION_PROPSET
          || action->action == ACTION_PROPSETF /* shouldn't see this one */
          || action->action == ACTION_PROPDEL)
        num_url_args = 1;
      else
        num_url_args = 2;

      /* Parse the required number of URLs. */
      for (j = 0; j < num_url_args; ++j)
        {
          const char *url = APR_ARRAY_IDX(action_args, i, const char *);

          /* If there's a ROOT_URL, we expect URL to be a path
             relative to ROOT_URL (and we build a full url from the
             combination of the two).  Otherwise, it should be a full
             url. */
          if (! svn_path_is_url(url))
            {
              if (! root_url)
                handle_error(svn_error_createf(SVN_ERR_INCORRECT_PARAMS, NULL,
                                               "'%s' is not a URL, and "
                                               "--root-url (-U) not provided\n",
                                               url), pool);
              /* ### These relpaths are already URI-encoded. */
              url = apr_pstrcat(pool, root_url, "/",
                                svn_relpath_canonicalize(url, pool),
                                (char *)NULL);
            }
          url = sanitize_url(url, pool);
          action->path[j] = url;

          /* The first URL arguments to 'cp', 'pd', 'ps' could be the anchor,
             but the other URLs should be children of the anchor. */
          if (! (action->action == ACTION_CP && j == 0)
              && action->action != ACTION_PROPDEL
              && action->action != ACTION_PROPSET
              && action->action != ACTION_PROPSETF)
            url = svn_uri_dirname(url, pool);
          if (! anchor)
            anchor = url;
          else
            {
              anchor = svn_uri_get_longest_ancestor(anchor, url, pool);
              if (!anchor || !anchor[0])
                handle_error(svn_error_createf(SVN_ERR_INCORRECT_PARAMS, NULL,
                                               "URLs in the action list do not "
                                               "share a common ancestor"),
                             pool);
            }

          if ((++i == action_args->nelts) && (j + 1 < num_url_args))
            insufficient(pool);
        }
      APR_ARRAY_PUSH(actions, struct action *) = action;
    }

  if (! actions->nelts)
    usage(pool, EXIT_FAILURE);

  if ((err = execute(actions, anchor, revprops, username, password,
                     config_dir, config_options, non_interactive,
                     trust_server_cert, no_auth_cache, base_revision, pool)))
    {
      if (err->apr_err == SVN_ERR_AUTHN_FAILED && non_interactive)
        err = svn_error_quick_wrap(err,
                                   _("Authentication failed and interactive"
                                     " prompting is disabled; see the"
                                     " --force-interactive option"));
      handle_error(err, pool);
    }

  /* Ensure that stdout is flushed, so the user will see all results. */
  svn_error_clear(svn_cmdline_fflush(stdout));

  svn_pool_destroy(pool);
  return EXIT_SUCCESS;
}
