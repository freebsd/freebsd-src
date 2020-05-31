/*
 * path_driver.c -- drive an editor across a set of paths
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


#include <apr_pools.h>
#include <apr_strings.h>

#include "svn_types.h"
#include "svn_delta.h"
#include "svn_pools.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_sorts.h"
#include "private/svn_sorts_private.h"


/*** Helper functions. ***/

typedef struct dir_stack_t
{
  void *dir_baton;   /* the dir baton. */
  apr_pool_t *pool;  /* the pool associated with the dir baton. */

} dir_stack_t;


/* Push onto dir_stack a new item allocated in POOL and containing
 * DIR_BATON and POOL.
 */
static void
push_dir_stack_item(apr_array_header_t *db_stack,
                    void *dir_baton,
                    apr_pool_t *pool)
{
  dir_stack_t *item = apr_pcalloc(pool, sizeof(*item));

  item->dir_baton = dir_baton;
  item->pool = pool;
  APR_ARRAY_PUSH(db_stack, dir_stack_t *) = item;
}


/* Call EDITOR's open_directory() function with the PATH argument, then
 * add the resulting dir baton to the dir baton stack.
 */
static svn_error_t *
open_dir(apr_array_header_t *db_stack,
         const svn_delta_editor_t *editor,
         const char *path,
         apr_pool_t *pool)
{
  void *parent_db, *db;
  dir_stack_t *item;
  apr_pool_t *subpool;

  /* Assert that we are in a stable state. */
  SVN_ERR_ASSERT(db_stack && db_stack->nelts);

  /* Get the parent dir baton. */
  item = APR_ARRAY_IDX(db_stack, db_stack->nelts - 1, void *);
  parent_db = item->dir_baton;

  /* Call the EDITOR's open_directory function to get a new directory
     baton. */
  subpool = svn_pool_create(pool);
  SVN_ERR(editor->open_directory(path, parent_db, SVN_INVALID_REVNUM, subpool,
                                 &db));

  /* Now add the dir baton to the stack. */
  push_dir_stack_item(db_stack, db, subpool);

  return SVN_NO_ERROR;
}


/* Pop a directory from the dir baton stack and update the stack
 * pointer.
 *
 * This function calls the EDITOR's close_directory() function.
 */
static svn_error_t *
pop_stack(apr_array_header_t *db_stack,
          const svn_delta_editor_t *editor)
{
  dir_stack_t *item;

  /* Assert that we are in a stable state. */
  SVN_ERR_ASSERT(db_stack && db_stack->nelts);

  /* Close the most recent directory pushed to the stack. */
  item = APR_ARRAY_IDX(db_stack, db_stack->nelts - 1, dir_stack_t *);
  (void) apr_array_pop(db_stack);
  SVN_ERR(editor->close_directory(item->dir_baton, item->pool));
  svn_pool_destroy(item->pool);

  return SVN_NO_ERROR;
}


/* Count the number of path components in PATH. */
static int
count_components(const char *path)
{
  int count = 1;
  const char *instance = path;

  if ((strlen(path) == 1) && (path[0] == '/'))
    return 0;

  do
    {
      instance++;
      instance = strchr(instance, '/');
      if (instance)
        count++;
    }
  while (instance);

  return count;
}



/*** Public interfaces ***/
svn_error_t *
svn_delta_path_driver3(const svn_delta_editor_t *editor,
                       void *edit_baton,
                       const apr_array_header_t *relpaths,
                       svn_boolean_t sort_paths,
                       svn_delta_path_driver_cb_func2_t callback_func,
                       void *callback_baton,
                       apr_pool_t *pool)
{
  svn_delta_path_driver_state_t *state;
  int i;
  apr_pool_t *subpool, *iterpool;

  /* Do nothing if there are no paths. */
  if (! relpaths->nelts)
    return SVN_NO_ERROR;

  subpool = svn_pool_create(pool);
  iterpool = svn_pool_create(pool);

  /* sort paths if necessary */
  if (sort_paths && relpaths->nelts > 1)
    {
      apr_array_header_t *sorted = apr_array_copy(subpool, relpaths);
      svn_sort__array(sorted, svn_sort_compare_paths);
      relpaths = sorted;
    }

  SVN_ERR(svn_delta_path_driver_start(&state,
                                      editor, edit_baton,
                                      callback_func, callback_baton,
                                      pool));

  /* Now, loop over the commit items, traversing the URL tree and
     driving the editor. */
  for (i = 0; i < relpaths->nelts; i++)
    {
      const char *relpath;

      /* Clear the iteration pool. */
      svn_pool_clear(iterpool);

      /* Get the next path. */
      relpath = APR_ARRAY_IDX(relpaths, i, const char *);

      SVN_ERR(svn_delta_path_driver_step(state, relpath, iterpool));
    }

  /* Destroy the iteration subpool. */
  svn_pool_destroy(iterpool);

  SVN_ERR(svn_delta_path_driver_finish(state, pool));

  return SVN_NO_ERROR;
}

struct svn_delta_path_driver_state_t
{
  const svn_delta_editor_t *editor;
  void *edit_baton;
  svn_delta_path_driver_cb_func2_t callback_func;
  void *callback_baton;
  apr_array_header_t *db_stack;
  const char *last_path;
  apr_pool_t *pool;  /* at least the lifetime of the entire drive */
};

svn_error_t *
svn_delta_path_driver_start(svn_delta_path_driver_state_t **state_p,
                            const svn_delta_editor_t *editor,
                            void *edit_baton,
                            svn_delta_path_driver_cb_func2_t callback_func,
                            void *callback_baton,
                            apr_pool_t *pool)
{
  svn_delta_path_driver_state_t *state = apr_pcalloc(pool, sizeof(*state));

  state->editor = editor;
  state->edit_baton = edit_baton;
  state->callback_func = callback_func;
  state->callback_baton = callback_baton;
  state->db_stack = apr_array_make(pool, 4, sizeof(void *));
  state->last_path = NULL;
  state->pool = pool;

  *state_p = state;
  return SVN_NO_ERROR;
}

svn_error_t *
svn_delta_path_driver_step(svn_delta_path_driver_state_t *state,
                           const char *relpath,
                           apr_pool_t *scratch_pool)
{
  const char *pdir;
  const char *common = "";
  size_t common_len;
  apr_pool_t *subpool;
  dir_stack_t *item;
  void *parent_db, *db;

  SVN_ERR_ASSERT(svn_relpath_is_canonical(relpath));

  /* If the first target path is not the root of the edit, we must first
     call open_root() ourselves. (If the first target path is the root of
     the edit, then we expect the user's callback to do so.) */
  if (!state->last_path && !svn_path_is_empty(relpath))
    {
      subpool = svn_pool_create(state->pool);
      SVN_ERR(state->editor->open_root(state->edit_baton, SVN_INVALID_REVNUM,
                                       subpool, &db));
      push_dir_stack_item(state->db_stack, db, subpool);
    }

  /*** Step A - Find the common ancestor of the last path and the
       current one.  For the first iteration, this is just the
       empty string. ***/
  if (state->last_path)
    common = svn_relpath_get_longest_ancestor(state->last_path, relpath,
                                              scratch_pool);
  common_len = strlen(common);

  /*** Step B - Close any directories between the last path and
       the new common ancestor, if any need to be closed.
       Sometimes there is nothing to do here (like, for the first
       iteration, or when the last path was an ancestor of the
       current one). ***/
  if ((state->last_path) && (strlen(state->last_path) > common_len))
    {
      const char *rel = state->last_path + (common_len ? (common_len + 1) : 0);
      int count = count_components(rel);
      while (count--)
        {
          SVN_ERR(pop_stack(state->db_stack, state->editor));
        }
    }

  /*** Step C - Open any directories between the common ancestor
       and the parent of the current path. ***/
  pdir = svn_relpath_dirname(relpath, scratch_pool);

  if (strlen(pdir) > common_len)
    {
      const char *piece = pdir + common_len + 1;

      while (1)
        {
          const char *rel = pdir;

          /* Find the first separator. */
          piece = strchr(piece, '/');

          /* Calculate REL as the portion of PDIR up to (but not
             including) the location to which PIECE is pointing. */
          if (piece)
            rel = apr_pstrmemdup(scratch_pool, pdir, piece - pdir);

          /* Open the subdirectory. */
          SVN_ERR(open_dir(state->db_stack, state->editor, rel, state->pool));

          /* If we found a '/', advance our PIECE pointer to
             character just after that '/'.  Otherwise, we're
             done.  */
          if (piece)
            piece++;
          else
            break;
        }
    }

  /*** Step D - Tell our caller to handle the current path. ***/
  if (state->db_stack->nelts)
    {
      item = APR_ARRAY_IDX(state->db_stack, state->db_stack->nelts - 1, void *);
      parent_db = item->dir_baton;
    }
  else
    parent_db = NULL;
  db = NULL;  /* predictable behaviour for callbacks that don't set it */
  subpool = svn_pool_create(state->pool);
  SVN_ERR(state->callback_func(&db,
                               state->editor, state->edit_baton, parent_db,
                               state->callback_baton,
                               relpath, subpool));
  if (db)
    {
      push_dir_stack_item(state->db_stack, db, subpool);
    }
  else
    {
      svn_pool_destroy(subpool);
    }

  /*** Step E - Save our state for the next iteration.  If our
       caller opened or added PATH as a directory, that becomes
       our LAST_PATH.  Otherwise, we use PATH's parent
       directory. ***/
  state->last_path = apr_pstrdup(state->pool, db ? relpath : pdir);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_delta_path_driver_finish(svn_delta_path_driver_state_t *state,
                             apr_pool_t *scratch_pool)
{
  /* Close down any remaining open directory batons. */
  while (state->db_stack->nelts)
    {
      SVN_ERR(pop_stack(state->db_stack, state->editor));
    }

  return SVN_NO_ERROR;
}
