/* fs-util.c : internal utility functions used by both FSFS and BDB back
 * ends.
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
#include <apr_strings.h>

#include "svn_hash.h"
#include "svn_fs.h"
#include "svn_dirent_uri.h"
#include "svn_path.h"
#include "svn_private_config.h"

#include "private/svn_fs_util.h"
#include "private/svn_fspath.h"
#include "../libsvn_fs/fs-loader.h"

/* Return TRUE, if PATH of PATH_LEN > 0 chars starts with a '/' and does
 * not end with a '/' and does not contain duplicate '/'.
 */
static svn_boolean_t
is_canonical_abspath(const char *path, size_t path_len)
{
  const char *end;

  /* check for leading '/' */
  if (path[0] != '/')
    return FALSE;

  /* check for trailing '/' */
  if (path_len == 1)
    return TRUE;
  if (path[path_len - 1] == '/')
    return FALSE;

  /* check for "//" */
  end = path + path_len - 1;
  for (; path != end; ++path)
    if ((path[0] == '/') && (path[1] == '/'))
      return FALSE;

  return TRUE;
}

svn_boolean_t
svn_fs__is_canonical_abspath(const char *path)
{
  /* No PATH?  No problem. */
  if (! path)
    return TRUE;

  /* Empty PATH?  That's just "/". */
  if (! *path)
    return FALSE;

  /* detailed checks */
  return is_canonical_abspath(path, strlen(path));
}

const char *
svn_fs__canonicalize_abspath(const char *path, apr_pool_t *pool)
{
  char *newpath;
  size_t path_len;
  size_t path_i = 0, newpath_i = 0;
  svn_boolean_t eating_slashes = FALSE;

  /* No PATH?  No problem. */
  if (! path)
    return NULL;

  /* Empty PATH?  That's just "/". */
  if (! *path)
    return "/";

  /* Non-trivial cases.  Maybe, the path already is canonical after all? */
  path_len = strlen(path);
  if (is_canonical_abspath(path, path_len))
    return apr_pstrmemdup(pool, path, path_len);

  /* Now, the fun begins.  Alloc enough room to hold PATH with an
     added leading '/'. */
  newpath = apr_palloc(pool, path_len + 2);

  /* No leading slash?  Fix that. */
  if (*path != '/')
    {
      newpath[newpath_i++] = '/';
    }

  for (path_i = 0; path_i < path_len; path_i++)
    {
      if (path[path_i] == '/')
        {
          /* The current character is a '/'.  If we are eating up
             extra '/' characters, skip this character.  Else, note
             that we are now eating slashes. */
          if (eating_slashes)
            continue;
          eating_slashes = TRUE;
        }
      else
        {
          /* The current character is NOT a '/'.  If we were eating
             slashes, we need not do that any more. */
          if (eating_slashes)
            eating_slashes = FALSE;
        }

      /* Copy the current character into our new buffer. */
      newpath[newpath_i++] = path[path_i];
    }

  /* Did we leave a '/' attached to the end of NEWPATH (other than in
     the root directory case)? */
  if ((newpath[newpath_i - 1] == '/') && (newpath_i > 1))
    newpath[newpath_i - 1] = '\0';
  else
    newpath[newpath_i] = '\0';

  return newpath;
}

svn_error_t *
svn_fs__check_fs(svn_fs_t *fs,
                 svn_boolean_t expect_open)
{
  if ((expect_open && fs->fsap_data)
      || ((! expect_open) && (! fs->fsap_data)))
    return SVN_NO_ERROR;
  if (expect_open)
    return svn_error_create(SVN_ERR_FS_NOT_OPEN, 0,
                            _("Filesystem object has not been opened yet"));
  else
    return svn_error_create(SVN_ERR_FS_ALREADY_OPEN, 0,
                            _("Filesystem object already open"));
}

char *
svn_fs__next_entry_name(const char **next_p,
                        const char *path,
                        apr_pool_t *pool)
{
  const char *end;

  /* Find the end of the current component.  */
  end = strchr(path, '/');

  if (! end)
    {
      /* The path contains only one component, with no trailing
         slashes. */
      *next_p = 0;
      return apr_pstrdup(pool, path);
    }
  else
    {
      /* There's a slash after the first component.  Skip over an arbitrary
         number of slashes to find the next one. */
      const char *next = end;
      while (*next == '/')
        next++;
      *next_p = next;
      return apr_pstrndup(pool, path, end - path);
    }
}

svn_fs_path_change2_t *
svn_fs__path_change_create_internal(const svn_fs_id_t *node_rev_id,
                                    svn_fs_path_change_kind_t change_kind,
                                    apr_pool_t *pool)
{
  svn_fs_path_change2_t *change;

  change = apr_pcalloc(pool, sizeof(*change));
  change->node_rev_id = node_rev_id;
  change->change_kind = change_kind;

  return change;
}

svn_error_t *
svn_fs__append_to_merged_froms(svn_mergeinfo_t *output,
                               svn_mergeinfo_t input,
                               const char *rel_path,
                               apr_pool_t *pool)
{
  apr_hash_index_t *hi;

  *output = apr_hash_make(pool);
  for (hi = apr_hash_first(pool, input); hi; hi = apr_hash_next(hi))
    {
      const char *path = svn__apr_hash_index_key(hi);
      svn_rangelist_t *rangelist = svn__apr_hash_index_val(hi);

      svn_hash_sets(*output,
                    svn_fspath__join(path, rel_path, pool),
                    svn_rangelist_dup(rangelist, pool));
    }

  return SVN_NO_ERROR;
}
