/*
 *  load_editor.c: The svn_delta_editor_t editor used by svnrdump to
 *  load revisions.
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

#include "svn_cmdline.h"
#include "svn_pools.h"
#include "svn_delta.h"
#include "svn_repos.h"
#include "svn_props.h"
#include "svn_path.h"
#include "svn_ra.h"
#include "svn_subst.h"
#include "svn_io.h"
#include "svn_private_config.h"
#include "private/svn_repos_private.h"
#include "private/svn_ra_private.h"
#include "private/svn_mergeinfo_private.h"
#include "private/svn_fspath.h"

#include "svnrdump.h"

#define SVNRDUMP_PROP_LOCK SVN_PROP_PREFIX "rdump-lock"

#if 0
#define LDR_DBG(x) SVN_DBG(x)
#else
#define LDR_DBG(x) while(0)
#endif



/**
 * General baton used by the parser functions.
 */
struct parse_baton
{
  /* Commit editor and baton used to transfer loaded revisions to
     the target repository. */
  const svn_delta_editor_t *commit_editor;
  void *commit_edit_baton;

  /* RA session(s) for committing to the target repository. */
  svn_ra_session_t *session;
  svn_ra_session_t *aux_session;

  /* To bleep, or not to bleep?  (What kind of question is that?) */
  svn_boolean_t quiet;

  /* UUID found in the dumpstream, if any; NULL otherwise. */
  const char *uuid;

  /* Root URL of the target repository. */
  const char *root_url;

  /* The "parent directory" of the target repository in which to load.
     (This is essentially the difference between ROOT_URL and
     SESSION's url, and roughly equivalent to the 'svnadmin load
     --parent-dir' option.) */
  const char *parent_dir;

  /* A mapping of svn_revnum_t * dump stream revisions to their
     corresponding svn_revnum_t * target repository revisions. */
  /* ### See http://subversion.tigris.org/issues/show_bug.cgi?id=3903
     ### for discussion about improving the memory costs of this mapping. */
  apr_hash_t *rev_map;

  /* The most recent (youngest) revision from the dump stream mapped in
     REV_MAP, or SVN_INVALID_REVNUM if no revisions have been mapped. */
  svn_revnum_t last_rev_mapped;

  /* The oldest revision loaded from the dump stream, or
     SVN_INVALID_REVNUM if none have been loaded. */
  svn_revnum_t oldest_dumpstream_rev;
};

/**
 * Use to wrap the dir_context_t in commit.c so we can keep track of
 * depth, relpath and parent for open_directory and close_directory.
 */
struct directory_baton
{
  void *baton;
  const char *relpath;
  int depth;
  struct directory_baton *parent;
};

/**
 * Baton used to represent a node; to be used by the parser
 * functions. Contains a link to the revision baton.
 */
struct node_baton
{
  const char *path;
  svn_node_kind_t kind;
  enum svn_node_action action;

  svn_revnum_t copyfrom_rev;
  const char *copyfrom_path;

  void *file_baton;
  const char *base_checksum;

  struct revision_baton *rb;
};

/**
 * Baton used to represet a revision; used by the parser
 * functions. Contains a link to the parser baton.
 */
struct revision_baton
{
  svn_revnum_t rev;
  apr_hash_t *revprop_table;
  apr_int32_t rev_offset;

  const svn_string_t *datestamp;
  const svn_string_t *author;

  struct parse_baton *pb;
  struct directory_baton *db;
  apr_pool_t *pool;
};



/* Record the mapping of FROM_REV to TO_REV in REV_MAP, ensuring that
   anything added to the hash is allocated in the hash's pool. */
static void
set_revision_mapping(apr_hash_t *rev_map,
                     svn_revnum_t from_rev,
                     svn_revnum_t to_rev)
{
  svn_revnum_t *mapped_revs = apr_palloc(apr_hash_pool_get(rev_map),
                                         sizeof(svn_revnum_t) * 2);
  mapped_revs[0] = from_rev;
  mapped_revs[1] = to_rev;
  apr_hash_set(rev_map, mapped_revs,
               sizeof(svn_revnum_t), mapped_revs + 1);
}

/* Return the revision to which FROM_REV maps in REV_MAP, or
   SVN_INVALID_REVNUM if no such mapping exists. */
static svn_revnum_t
get_revision_mapping(apr_hash_t *rev_map,
                     svn_revnum_t from_rev)
{
  svn_revnum_t *to_rev = apr_hash_get(rev_map, &from_rev,
                                      sizeof(from_rev));
  return to_rev ? *to_rev : SVN_INVALID_REVNUM;
}


/* Prepend the mergeinfo source paths in MERGEINFO_ORIG with
   PARENT_DIR, and return it in *MERGEINFO_VAL. */
/* ### FIXME:  Consider somehow sharing code with
   ### libsvn_repos/load-fs-vtable.c:prefix_mergeinfo_paths() */
static svn_error_t *
prefix_mergeinfo_paths(svn_string_t **mergeinfo_val,
                       const svn_string_t *mergeinfo_orig,
                       const char *parent_dir,
                       apr_pool_t *pool)
{
  apr_hash_t *prefixed_mergeinfo, *mergeinfo;
  apr_hash_index_t *hi;
  void *rangelist;

  SVN_ERR(svn_mergeinfo_parse(&mergeinfo, mergeinfo_orig->data, pool));
  prefixed_mergeinfo = apr_hash_make(pool);
  for (hi = apr_hash_first(pool, mergeinfo); hi; hi = apr_hash_next(hi))
    {
      const void *key;
      const char *path, *merge_source;

      apr_hash_this(hi, &key, NULL, &rangelist);
      merge_source = svn_relpath_canonicalize(key, pool);

      /* The svn:mergeinfo property syntax demands a repos abspath */
      path = svn_fspath__canonicalize(svn_relpath_join(parent_dir,
                                                       merge_source, pool),
                                      pool);
      svn_hash_sets(prefixed_mergeinfo, path, rangelist);
    }
  return svn_mergeinfo_to_string(mergeinfo_val, prefixed_mergeinfo, pool);
}


/* Examine the mergeinfo in INITIAL_VAL, renumber revisions in rangelists
   as appropriate, and return the (possibly new) mergeinfo in *FINAL_VAL
   (allocated from POOL). */
/* ### FIXME:  Consider somehow sharing code with
   ### libsvn_repos/load-fs-vtable.c:renumber_mergeinfo_revs() */
static svn_error_t *
renumber_mergeinfo_revs(svn_string_t **final_val,
                        const svn_string_t *initial_val,
                        struct revision_baton *rb,
                        apr_pool_t *pool)
{
  apr_pool_t *subpool = svn_pool_create(pool);
  svn_mergeinfo_t mergeinfo, predates_stream_mergeinfo;
  svn_mergeinfo_t final_mergeinfo = apr_hash_make(subpool);
  apr_hash_index_t *hi;

  SVN_ERR(svn_mergeinfo_parse(&mergeinfo, initial_val->data, subpool));

  /* Issue #3020
     http://subversion.tigris.org/issues/show_bug.cgi?id=3020#desc16
     Remove mergeinfo older than the oldest revision in the dump stream
     and adjust its revisions by the difference between the head rev of
     the target repository and the current dump stream rev. */
  if (rb->pb->oldest_dumpstream_rev > 1)
    {
      SVN_ERR(svn_mergeinfo__filter_mergeinfo_by_ranges(
                  &predates_stream_mergeinfo, mergeinfo,
                  rb->pb->oldest_dumpstream_rev - 1, 0,
                  TRUE, subpool, subpool));
      SVN_ERR(svn_mergeinfo__filter_mergeinfo_by_ranges(
                  &mergeinfo, mergeinfo,
                  rb->pb->oldest_dumpstream_rev - 1, 0,
                  FALSE, subpool, subpool));
      SVN_ERR(svn_mergeinfo__adjust_mergeinfo_rangelists(
                  &predates_stream_mergeinfo,
                  predates_stream_mergeinfo,
                  -rb->rev_offset, subpool, subpool));
    }
  else
    {
      predates_stream_mergeinfo = NULL;
    }

  for (hi = apr_hash_first(subpool, mergeinfo); hi; hi = apr_hash_next(hi))
    {
      svn_rangelist_t *rangelist;
      struct parse_baton *pb = rb->pb;
      int i;
      const void *path;
      apr_ssize_t pathlen;
      void *val;

      apr_hash_this(hi, &path, &pathlen, &val);
      rangelist = val;

      /* Possibly renumber revisions in merge source's rangelist. */
      for (i = 0; i < rangelist->nelts; i++)
        {
          svn_revnum_t rev_from_map;
          svn_merge_range_t *range = APR_ARRAY_IDX(rangelist, i,
                                                   svn_merge_range_t *);
          rev_from_map = get_revision_mapping(pb->rev_map, range->start);
          if (SVN_IS_VALID_REVNUM(rev_from_map))
            {
              range->start = rev_from_map;
            }
          else if (range->start == pb->oldest_dumpstream_rev - 1)
            {
              /* Since the start revision of svn_merge_range_t are not
                 inclusive there is one possible valid start revision that
                 won't be found in the PB->REV_MAP mapping of load stream
                 revsions to loaded revisions: The revision immediately
                 preceeding the oldest revision from the load stream.
                 This is a valid revision for mergeinfo, but not a valid
                 copy from revision (which PB->REV_MAP also maps for) so it
                 will never be in the mapping.

                 If that is what we have here, then find the mapping for the
                 oldest rev from the load stream and subtract 1 to get the
                 renumbered, non-inclusive, start revision. */
              rev_from_map = get_revision_mapping(pb->rev_map,
                                                  pb->oldest_dumpstream_rev);
              if (SVN_IS_VALID_REVNUM(rev_from_map))
                range->start = rev_from_map - 1;
            }
          else
            {
              /* If we can't remap the start revision then don't even bother
                 trying to remap the end revision.  It's possible we might
                 actually succeed at the latter, which can result in invalid
                 mergeinfo with a start rev > end rev.  If that gets into the
                 repository then a world of bustage breaks loose anytime that
                 bogus mergeinfo is parsed.  See
                 http://subversion.tigris.org/issues/show_bug.cgi?id=3020#desc16.
                 */
              continue;
            }

          rev_from_map = get_revision_mapping(pb->rev_map, range->end);
          if (SVN_IS_VALID_REVNUM(rev_from_map))
            range->end = rev_from_map;
        }
      apr_hash_set(final_mergeinfo, path, pathlen, rangelist);
    }

  if (predates_stream_mergeinfo)
    {
      SVN_ERR(svn_mergeinfo_merge2(final_mergeinfo, predates_stream_mergeinfo,
                                   subpool, subpool));
    }

  SVN_ERR(svn_mergeinfo_sort(final_mergeinfo, subpool));

  /* Mergeinfo revision sources for r0 and r1 are invalid; you can't merge r0
     or r1.  However, svndumpfilter can be abused to produce r1 merge source
     revs.  So if we encounter any, then strip them out, no need to put them
     into the load target. */
  SVN_ERR(svn_mergeinfo__filter_mergeinfo_by_ranges(&final_mergeinfo,
                                                    final_mergeinfo,
                                                    1, 0, FALSE,
                                                    subpool, subpool));

  SVN_ERR(svn_mergeinfo_to_string(final_val, final_mergeinfo, pool));
  svn_pool_destroy(subpool);

  return SVN_NO_ERROR;
}


static svn_error_t *
commit_callback(const svn_commit_info_t *commit_info,
                void *baton,
                apr_pool_t *pool)
{
  struct revision_baton *rb = baton;
  struct parse_baton *pb = rb->pb;

  /* ### Don't print directly; generate a notification. */
  if (! pb->quiet)
    SVN_ERR(svn_cmdline_printf(pool, "* Loaded revision %ld.\n",
                               commit_info->revision));

  /* Add the mapping of the dumpstream revision to the committed revision. */
  set_revision_mapping(pb->rev_map, rb->rev, commit_info->revision);

  /* If the incoming dump stream has non-contiguous revisions (e.g. from
     using svndumpfilter --drop-empty-revs without --renumber-revs) then
     we must account for the missing gaps in PB->REV_MAP.  Otherwise we
     might not be able to map all mergeinfo source revisions to the correct
     revisions in the target repos. */
  if ((pb->last_rev_mapped != SVN_INVALID_REVNUM)
      && (rb->rev != pb->last_rev_mapped + 1))
    {
      svn_revnum_t i;

      for (i = pb->last_rev_mapped + 1; i < rb->rev; i++)
        {
          set_revision_mapping(pb->rev_map, i, pb->last_rev_mapped);
        }
    }

  /* Update our "last revision mapped". */
  pb->last_rev_mapped = rb->rev;

  return SVN_NO_ERROR;
}

/* Implements `svn_ra__lock_retry_func_t'. */
static svn_error_t *
lock_retry_func(void *baton,
                const svn_string_t *reposlocktoken,
                apr_pool_t *pool)
{
  return svn_cmdline_printf(pool,
                            _("Failed to get lock on destination "
                              "repos, currently held by '%s'\n"),
                            reposlocktoken->data);
}


static svn_error_t *
fetch_base_func(const char **filename,
                void *baton,
                const char *path,
                svn_revnum_t base_revision,
                apr_pool_t *result_pool,
                apr_pool_t *scratch_pool)
{
  struct revision_baton *rb = baton;
  svn_stream_t *fstream;
  svn_error_t *err;

  if (! SVN_IS_VALID_REVNUM(base_revision))
    base_revision = rb->rev - 1;

  SVN_ERR(svn_stream_open_unique(&fstream, filename, NULL,
                                 svn_io_file_del_on_pool_cleanup,
                                 result_pool, scratch_pool));

  err = svn_ra_get_file(rb->pb->aux_session, path, base_revision,
                        fstream, NULL, NULL, scratch_pool);
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
  struct revision_baton *rb = baton;
  svn_node_kind_t node_kind;

  if (! SVN_IS_VALID_REVNUM(base_revision))
    base_revision = rb->rev - 1;

  SVN_ERR(svn_ra_check_path(rb->pb->aux_session, path, base_revision,
                            &node_kind, scratch_pool));

  if (node_kind == svn_node_file)
    {
      SVN_ERR(svn_ra_get_file(rb->pb->aux_session, path, base_revision,
                              NULL, NULL, props, result_pool));
    }
  else if (node_kind == svn_node_dir)
    {
      apr_array_header_t *tmp_props;

      SVN_ERR(svn_ra_get_dir2(rb->pb->aux_session, NULL, NULL, props, path,
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
  struct revision_baton *rb = baton;

  if (! SVN_IS_VALID_REVNUM(base_revision))
    base_revision = rb->rev - 1;

  SVN_ERR(svn_ra_check_path(rb->pb->aux_session, path, base_revision,
                            kind, scratch_pool));

  return SVN_NO_ERROR;
}

static svn_delta_shim_callbacks_t *
get_shim_callbacks(struct revision_baton *rb,
                   apr_pool_t *pool)
{
  svn_delta_shim_callbacks_t *callbacks =
                        svn_delta_shim_callbacks_default(pool);

  callbacks->fetch_props_func = fetch_props_func;
  callbacks->fetch_kind_func = fetch_kind_func;
  callbacks->fetch_base_func = fetch_base_func;
  callbacks->fetch_baton = rb;

  return callbacks;
}

/* Acquire a lock (of sorts) on the repository associated with the
 * given RA SESSION. This lock is just a revprop change attempt in a
 * time-delay loop. This function is duplicated by svnsync in
 * svnsync/svnsync.c
 *
 * ### TODO: Make this function more generic and
 * expose it through a header for use by other Subversion
 * applications to avoid duplication.
 */
static svn_error_t *
get_lock(const svn_string_t **lock_string_p,
         svn_ra_session_t *session,
         svn_cancel_func_t cancel_func,
         void *cancel_baton,
         apr_pool_t *pool)
{
  svn_boolean_t be_atomic;

  SVN_ERR(svn_ra_has_capability(session, &be_atomic,
                                SVN_RA_CAPABILITY_ATOMIC_REVPROPS,
                                pool));
  if (! be_atomic)
    {
      /* Pre-1.7 servers can't lock without a race condition.  (Issue #3546) */
      svn_error_t *err =
        svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                         _("Target server does not support atomic revision "
                           "property edits; consider upgrading it to 1.7."));
      svn_handle_warning2(stderr, err, "svnrdump: ");
      svn_error_clear(err);
    }

  return svn_ra__get_operational_lock(lock_string_p, NULL, session,
                                      SVNRDUMP_PROP_LOCK, FALSE,
                                      10 /* retries */, lock_retry_func, NULL,
                                      cancel_func, cancel_baton, pool);
}

static svn_error_t *
new_revision_record(void **revision_baton,
                    apr_hash_t *headers,
                    void *parse_baton,
                    apr_pool_t *pool)
{
  struct revision_baton *rb;
  struct parse_baton *pb;
  apr_hash_index_t *hi;
  svn_revnum_t head_rev;

  rb = apr_pcalloc(pool, sizeof(*rb));
  pb = parse_baton;
  rb->pool = svn_pool_create(pool);
  rb->pb = pb;

  for (hi = apr_hash_first(pool, headers); hi; hi = apr_hash_next(hi))
    {
      const char *hname = svn__apr_hash_index_key(hi);
      const char *hval = svn__apr_hash_index_val(hi);

      if (strcmp(hname, SVN_REPOS_DUMPFILE_REVISION_NUMBER) == 0)
        rb->rev = atoi(hval);
    }

  SVN_ERR(svn_ra_get_latest_revnum(pb->session, &head_rev, pool));

  /* FIXME: This is a lame fallback loading multiple segments of dump in
     several separate operations. It is highly susceptible to race conditions.
     Calculate the revision 'offset' for finding copyfrom sources.
     It might be positive or negative. */
  rb->rev_offset = (apr_int32_t) ((rb->rev) - (head_rev + 1));

  /* Stash the oldest (non-zero) dumpstream revision seen. */
  if ((rb->rev > 0) && (!SVN_IS_VALID_REVNUM(pb->oldest_dumpstream_rev)))
    pb->oldest_dumpstream_rev = rb->rev;

  /* Set the commit_editor/ commit_edit_baton to NULL and wait for
     them to be created in new_node_record */
  rb->pb->commit_editor = NULL;
  rb->pb->commit_edit_baton = NULL;
  rb->revprop_table = apr_hash_make(rb->pool);

  *revision_baton = rb;
  return SVN_NO_ERROR;
}

static svn_error_t *
magic_header_record(int version,
            void *parse_baton,
            apr_pool_t *pool)
{
  return SVN_NO_ERROR;
}

static svn_error_t *
uuid_record(const char *uuid,
            void *parse_baton,
            apr_pool_t *pool)
{
  struct parse_baton *pb;
  pb = parse_baton;
  pb->uuid = apr_pstrdup(pool, uuid);
  return SVN_NO_ERROR;
}

static svn_error_t *
new_node_record(void **node_baton,
                apr_hash_t *headers,
                void *revision_baton,
                apr_pool_t *pool)
{
  struct revision_baton *rb = revision_baton;
  const struct svn_delta_editor_t *commit_editor = rb->pb->commit_editor;
  void *commit_edit_baton = rb->pb->commit_edit_baton;
  struct node_baton *nb;
  struct directory_baton *child_db;
  apr_hash_index_t *hi;
  void *child_baton;
  char *relpath_compose;
  const char *nb_dirname;

  nb = apr_pcalloc(rb->pool, sizeof(*nb));
  nb->rb = rb;

  nb->copyfrom_path = NULL;
  nb->copyfrom_rev = SVN_INVALID_REVNUM;

  /* If the creation of commit_editor is pending, create it now and
     open_root on it; also create a top-level directory baton. */

  if (!commit_editor)
    {
      /* The revprop_table should have been filled in with important
         information like svn:log in set_revision_property. We can now
         use it all this information to create our commit_editor. But
         first, clear revprops that we aren't allowed to set with the
         commit_editor. We'll set them separately using the RA API
         after closing the editor (see close_revision). */

      svn_hash_sets(rb->revprop_table, SVN_PROP_REVISION_AUTHOR, NULL);
      svn_hash_sets(rb->revprop_table, SVN_PROP_REVISION_DATE, NULL);

      SVN_ERR(svn_ra__register_editor_shim_callbacks(rb->pb->session,
                                    get_shim_callbacks(rb, rb->pool)));
      SVN_ERR(svn_ra_get_commit_editor3(rb->pb->session, &commit_editor,
                                        &commit_edit_baton, rb->revprop_table,
                                        commit_callback, revision_baton,
                                        NULL, FALSE, rb->pool));

      rb->pb->commit_editor = commit_editor;
      rb->pb->commit_edit_baton = commit_edit_baton;

      SVN_ERR(commit_editor->open_root(commit_edit_baton,
                                       rb->rev - rb->rev_offset - 1,
                                       rb->pool, &child_baton));

      LDR_DBG(("Opened root %p\n", child_baton));

      /* child_db corresponds to the root directory baton here */
      child_db = apr_pcalloc(rb->pool, sizeof(*child_db));
      child_db->baton = child_baton;
      child_db->depth = 0;
      child_db->relpath = "";
      child_db->parent = NULL;
      rb->db = child_db;
    }

  for (hi = apr_hash_first(rb->pool, headers); hi; hi = apr_hash_next(hi))
    {
      const char *hname = svn__apr_hash_index_key(hi);
      const char *hval = svn__apr_hash_index_val(hi);

      /* Parse the different kinds of headers we can encounter and
         stuff them into the node_baton for writing later */
      if (strcmp(hname, SVN_REPOS_DUMPFILE_NODE_PATH) == 0)
        nb->path = apr_pstrdup(rb->pool, hval);
      if (strcmp(hname, SVN_REPOS_DUMPFILE_NODE_KIND) == 0)
        nb->kind = strcmp(hval, "file") == 0 ? svn_node_file : svn_node_dir;
      if (strcmp(hname, SVN_REPOS_DUMPFILE_NODE_ACTION) == 0)
        {
          if (strcmp(hval, "add") == 0)
            nb->action = svn_node_action_add;
          if (strcmp(hval, "change") == 0)
            nb->action = svn_node_action_change;
          if (strcmp(hval, "delete") == 0)
            nb->action = svn_node_action_delete;
          if (strcmp(hval, "replace") == 0)
            nb->action = svn_node_action_replace;
        }
      if (strcmp(hname, SVN_REPOS_DUMPFILE_TEXT_DELTA_BASE_MD5) == 0)
        nb->base_checksum = apr_pstrdup(rb->pool, hval);
      if (strcmp(hname, SVN_REPOS_DUMPFILE_NODE_COPYFROM_REV) == 0)
        nb->copyfrom_rev = atoi(hval);
      if (strcmp(hname, SVN_REPOS_DUMPFILE_NODE_COPYFROM_PATH) == 0)
        nb->copyfrom_path = apr_pstrdup(rb->pool, hval);
    }

  nb_dirname = svn_relpath_dirname(nb->path, pool);
  if (svn_path_compare_paths(nb_dirname,
                             rb->db->relpath) != 0)
    {
      char *ancestor_path;
      apr_size_t residual_close_count;
      apr_array_header_t *residual_open_path;
      int i;
      apr_size_t n;

      /* Before attempting to handle the action, call open_directory
         for all the path components and set the directory baton
         accordingly */
      ancestor_path =
        svn_relpath_get_longest_ancestor(nb_dirname,
                                         rb->db->relpath, pool);
      residual_close_count =
        svn_path_component_count(svn_relpath_skip_ancestor(ancestor_path,
                                                           rb->db->relpath));
      residual_open_path =
        svn_path_decompose(svn_relpath_skip_ancestor(ancestor_path,
                                                     nb_dirname), pool);

      /* First close all as many directories as there are after
         skip_ancestor, and then open fresh directories */
      for (n = 0; n < residual_close_count; n ++)
        {
          /* Don't worry about destroying the actual rb->db object,
             since the pool we're using has the lifetime of one
             revision anyway */
          LDR_DBG(("Closing dir %p\n", rb->db->baton));
          SVN_ERR(commit_editor->close_directory(rb->db->baton, rb->pool));
          rb->db = rb->db->parent;
        }

      for (i = 0; i < residual_open_path->nelts; i ++)
        {
          relpath_compose =
            svn_relpath_join(rb->db->relpath,
                             APR_ARRAY_IDX(residual_open_path, i, const char *),
                             rb->pool);
          SVN_ERR(commit_editor->open_directory(relpath_compose,
                                                rb->db->baton,
                                                rb->rev - rb->rev_offset - 1,
                                                rb->pool, &child_baton));
          LDR_DBG(("Opened dir %p\n", child_baton));
          child_db = apr_pcalloc(rb->pool, sizeof(*child_db));
          child_db->baton = child_baton;
          child_db->depth = rb->db->depth + 1;
          child_db->relpath = relpath_compose;
          child_db->parent = rb->db;
          rb->db = child_db;
        }
    }

  /* Fix up the copyfrom information in light of mapped revisions and
     non-root load targets, and convert copyfrom path into a full
     URL. */
  if (nb->copyfrom_path && SVN_IS_VALID_REVNUM(nb->copyfrom_rev))
    {
      svn_revnum_t copyfrom_rev;

      /* Try to find the copyfrom revision in the revision map;
         failing that, fall back to the revision offset approach. */
      copyfrom_rev = get_revision_mapping(rb->pb->rev_map, nb->copyfrom_rev);
      if (! SVN_IS_VALID_REVNUM(copyfrom_rev))
        copyfrom_rev = nb->copyfrom_rev - rb->rev_offset;

      if (! SVN_IS_VALID_REVNUM(copyfrom_rev))
        return svn_error_createf(SVN_ERR_FS_NO_SUCH_REVISION, NULL,
                                 _("Relative source revision %ld is not"
                                   " available in current repository"),
                                 copyfrom_rev);

      nb->copyfrom_rev = copyfrom_rev;

      if (rb->pb->parent_dir)
        nb->copyfrom_path = svn_relpath_join(rb->pb->parent_dir,
                                             nb->copyfrom_path, rb->pool);
      nb->copyfrom_path = svn_path_url_add_component2(rb->pb->root_url,
                                                      nb->copyfrom_path,
                                                      rb->pool);
    }


  switch (nb->action)
    {
    case svn_node_action_delete:
    case svn_node_action_replace:
      LDR_DBG(("Deleting entry %s in %p\n", nb->path, rb->db->baton));
      SVN_ERR(commit_editor->delete_entry(nb->path, rb->rev - rb->rev_offset,
                                          rb->db->baton, rb->pool));
      if (nb->action == svn_node_action_delete)
        break;
      else
        /* FALL THROUGH */;
    case svn_node_action_add:
      switch (nb->kind)
        {
        case svn_node_file:
          SVN_ERR(commit_editor->add_file(nb->path, rb->db->baton,
                                          nb->copyfrom_path,
                                          nb->copyfrom_rev,
                                          rb->pool, &(nb->file_baton)));
          LDR_DBG(("Added file %s to dir %p as %p\n",
                   nb->path, rb->db->baton, nb->file_baton));
          break;
        case svn_node_dir:
          SVN_ERR(commit_editor->add_directory(nb->path, rb->db->baton,
                                               nb->copyfrom_path,
                                               nb->copyfrom_rev,
                                               rb->pool, &child_baton));
          LDR_DBG(("Added dir %s to dir %p as %p\n",
                   nb->path, rb->db->baton, child_baton));
          child_db = apr_pcalloc(rb->pool, sizeof(*child_db));
          child_db->baton = child_baton;
          child_db->depth = rb->db->depth + 1;
          child_db->relpath = apr_pstrdup(rb->pool, nb->path);
          child_db->parent = rb->db;
          rb->db = child_db;
          break;
        default:
          break;
        }
      break;
    case svn_node_action_change:
      switch (nb->kind)
        {
        case svn_node_file:
          SVN_ERR(commit_editor->open_file(nb->path, rb->db->baton,
                                           SVN_INVALID_REVNUM, rb->pool,
                                           &(nb->file_baton)));
          break;
        default:
          SVN_ERR(commit_editor->open_directory(nb->path, rb->db->baton,
                                                rb->rev - rb->rev_offset - 1,
                                                rb->pool, &child_baton));
          child_db = apr_pcalloc(rb->pool, sizeof(*child_db));
          child_db->baton = child_baton;
          child_db->depth = rb->db->depth + 1;
          child_db->relpath = apr_pstrdup(rb->pool, nb->path);
          child_db->parent = rb->db;
          rb->db = child_db;
          break;
        }
      break;
    }

  *node_baton = nb;
  return SVN_NO_ERROR;
}

static svn_error_t *
set_revision_property(void *baton,
                      const char *name,
                      const svn_string_t *value)
{
  struct revision_baton *rb = baton;

  SVN_ERR(svn_rdump__normalize_prop(name, &value, rb->pool));

  SVN_ERR(svn_repos__validate_prop(name, value, rb->pool));

  if (rb->rev > 0)
    {
      svn_hash_sets(rb->revprop_table,
                    apr_pstrdup(rb->pool, name),
                    svn_string_dup(value, rb->pool));
    }
  else if (rb->rev_offset == -1)
    {
      /* Special case: set revision 0 properties directly (which is
         safe because the commit_editor hasn't been created yet), but
         only when loading into an 'empty' filesystem. */
      SVN_ERR(svn_ra_change_rev_prop2(rb->pb->session, 0,
                                      name, NULL, value, rb->pool));
    }

  /* Remember any datestamp/ author that passes through (see comment
     in close_revision). */
  if (!strcmp(name, SVN_PROP_REVISION_DATE))
    rb->datestamp = svn_string_dup(value, rb->pool);
  if (!strcmp(name, SVN_PROP_REVISION_AUTHOR))
    rb->author = svn_string_dup(value, rb->pool);

  return SVN_NO_ERROR;
}

static svn_error_t *
set_node_property(void *baton,
                  const char *name,
                  const svn_string_t *value)
{
  struct node_baton *nb = baton;
  const struct svn_delta_editor_t *commit_editor = nb->rb->pb->commit_editor;
  apr_pool_t *pool = nb->rb->pool;

  if (value && strcmp(name, SVN_PROP_MERGEINFO) == 0)
    {
      svn_string_t *renumbered_mergeinfo;
      svn_string_t prop_val;

      /* Tolerate mergeinfo with "\r\n" line endings because some
         dumpstream sources might contain as much.  If so normalize
         the line endings to '\n' and make a notification to
         PARSE_BATON->FEEDBACK_STREAM that we have made this
         correction. */
      if (strstr(value->data, "\r"))
        {
          const char *prop_eol_normalized;

          SVN_ERR(svn_subst_translate_cstring2(value->data,
                                               &prop_eol_normalized,
                                               "\n",  /* translate to LF */
                                               FALSE, /* no repair */
                                               NULL,  /* no keywords */
                                               FALSE, /* no expansion */
                                               pool));
          prop_val.data = prop_eol_normalized;
          prop_val.len = strlen(prop_eol_normalized);
          value = &prop_val;

          /* ### TODO: notify? */
        }

      /* Renumber mergeinfo as appropriate. */
      SVN_ERR(renumber_mergeinfo_revs(&renumbered_mergeinfo, value,
                                      nb->rb, pool));
      value = renumbered_mergeinfo;

      if (nb->rb->pb->parent_dir)
        {
          /* Prefix the merge source paths with PB->parent_dir. */
          /* ASSUMPTION: All source paths are included in the dump stream. */
          svn_string_t *mergeinfo_val;
          SVN_ERR(prefix_mergeinfo_paths(&mergeinfo_val, value,
                                         nb->rb->pb->parent_dir, pool));
          value = mergeinfo_val;
        }
    }

  SVN_ERR(svn_rdump__normalize_prop(name, &value, pool));

  SVN_ERR(svn_repos__validate_prop(name, value, pool));

  switch (nb->kind)
    {
    case svn_node_file:
      LDR_DBG(("Applying properties on %p\n", nb->file_baton));
      SVN_ERR(commit_editor->change_file_prop(nb->file_baton, name,
                                              value, pool));
      break;
    case svn_node_dir:
      LDR_DBG(("Applying properties on %p\n", nb->rb->db->baton));
      SVN_ERR(commit_editor->change_dir_prop(nb->rb->db->baton, name,
                                             value, pool));
      break;
    default:
      break;
    }
  return SVN_NO_ERROR;
}

static svn_error_t *
delete_node_property(void *baton,
                     const char *name)
{
  struct node_baton *nb = baton;
  const struct svn_delta_editor_t *commit_editor = nb->rb->pb->commit_editor;
  apr_pool_t *pool = nb->rb->pool;

  SVN_ERR(svn_repos__validate_prop(name, NULL, pool));

  if (nb->kind == svn_node_file)
    SVN_ERR(commit_editor->change_file_prop(nb->file_baton, name,
                                            NULL, pool));
  else
    SVN_ERR(commit_editor->change_dir_prop(nb->rb->db->baton, name,
                                           NULL, pool));

  return SVN_NO_ERROR;
}

static svn_error_t *
remove_node_props(void *baton)
{
  struct node_baton *nb = baton;
  apr_pool_t *pool = nb->rb->pool;
  apr_hash_index_t *hi;
  apr_hash_t *props;

  if ((nb->action == svn_node_action_add
            || nb->action == svn_node_action_replace)
      && ! SVN_IS_VALID_REVNUM(nb->copyfrom_rev))
    /* Add-without-history; no "old" properties to worry about. */
    return SVN_NO_ERROR;

  if (nb->kind == svn_node_file)
    {
      SVN_ERR(svn_ra_get_file(nb->rb->pb->aux_session, nb->path,
                              SVN_INVALID_REVNUM, NULL, NULL, &props, pool));
    }
  else  /* nb->kind == svn_node_dir */
    {
      SVN_ERR(svn_ra_get_dir2(nb->rb->pb->aux_session, NULL, NULL, &props,
                              nb->path, SVN_INVALID_REVNUM, 0, pool));
    }

  for (hi = apr_hash_first(pool, props); hi; hi = apr_hash_next(hi))
    {
      const char *name = svn__apr_hash_index_key(hi);
      svn_prop_kind_t kind = svn_property_kind2(name);

      if (kind == svn_prop_regular_kind)
        SVN_ERR(set_node_property(nb, name, NULL));
    }

  return SVN_NO_ERROR;
}

static svn_error_t *
set_fulltext(svn_stream_t **stream,
             void *node_baton)
{
  struct node_baton *nb = node_baton;
  const struct svn_delta_editor_t *commit_editor = nb->rb->pb->commit_editor;
  svn_txdelta_window_handler_t handler;
  void *handler_baton;
  apr_pool_t *pool = nb->rb->pool;

  LDR_DBG(("Setting fulltext for %p\n", nb->file_baton));
  SVN_ERR(commit_editor->apply_textdelta(nb->file_baton, nb->base_checksum,
                                         pool, &handler, &handler_baton));
  *stream = svn_txdelta_target_push(handler, handler_baton,
                                    svn_stream_empty(pool), pool);
  return SVN_NO_ERROR;
}

static svn_error_t *
apply_textdelta(svn_txdelta_window_handler_t *handler,
                void **handler_baton,
                void *node_baton)
{
  struct node_baton *nb = node_baton;
  const struct svn_delta_editor_t *commit_editor = nb->rb->pb->commit_editor;
  apr_pool_t *pool = nb->rb->pool;

  LDR_DBG(("Applying textdelta to %p\n", nb->file_baton));
  SVN_ERR(commit_editor->apply_textdelta(nb->file_baton, nb->base_checksum,
                                         pool, handler, handler_baton));

  return SVN_NO_ERROR;
}

static svn_error_t *
close_node(void *baton)
{
  struct node_baton *nb = baton;
  const struct svn_delta_editor_t *commit_editor = nb->rb->pb->commit_editor;

  /* Pass a file node closure through to the editor *unless* we
     deleted the file (which doesn't require us to open it). */
  if ((nb->kind == svn_node_file) && (nb->file_baton))
    {
      LDR_DBG(("Closing file %p\n", nb->file_baton));
      SVN_ERR(commit_editor->close_file(nb->file_baton, NULL, nb->rb->pool));
    }

  /* The svn_node_dir case is handled in close_revision */

  return SVN_NO_ERROR;
}

static svn_error_t *
close_revision(void *baton)
{
  struct revision_baton *rb = baton;
  const svn_delta_editor_t *commit_editor = rb->pb->commit_editor;
  void *commit_edit_baton = rb->pb->commit_edit_baton;
  svn_revnum_t committed_rev = SVN_INVALID_REVNUM;

  /* Fake revision 0 */
  if (rb->rev == 0)
    {
      /* ### Don't print directly; generate a notification. */
      if (! rb->pb->quiet)
        SVN_ERR(svn_cmdline_printf(rb->pool, "* Loaded revision 0.\n"));
    }
  else if (commit_editor)
    {
      /* Close all pending open directories, and then close the edit
         session itself */
      while (rb->db && rb->db->parent)
        {
          LDR_DBG(("Closing dir %p\n", rb->db->baton));
          SVN_ERR(commit_editor->close_directory(rb->db->baton, rb->pool));
          rb->db = rb->db->parent;
        }
      /* root dir's baton */
      LDR_DBG(("Closing edit on %p\n", commit_edit_baton));
      SVN_ERR(commit_editor->close_directory(rb->db->baton, rb->pool));
      SVN_ERR(commit_editor->close_edit(commit_edit_baton, rb->pool));
    }
  else
    {
      void *child_baton;

      /* Legitimate revision with no node information */
      SVN_ERR(svn_ra_get_commit_editor3(rb->pb->session, &commit_editor,
                                        &commit_edit_baton, rb->revprop_table,
                                        commit_callback, baton,
                                        NULL, FALSE, rb->pool));

      SVN_ERR(commit_editor->open_root(commit_edit_baton,
                                       rb->rev - rb->rev_offset - 1,
                                       rb->pool, &child_baton));

      LDR_DBG(("Opened root %p\n", child_baton));
      LDR_DBG(("Closing edit on %p\n", commit_edit_baton));
      SVN_ERR(commit_editor->close_directory(child_baton, rb->pool));
      SVN_ERR(commit_editor->close_edit(commit_edit_baton, rb->pool));
    }

  /* svn_fs_commit_txn() rewrites the datestamp and author properties;
     we'll rewrite them again by hand after closing the commit_editor.
     The only time we don't do this is for revision 0 when loaded into
     a non-empty repository.  */
  if (rb->rev > 0)
    {
      committed_rev = get_revision_mapping(rb->pb->rev_map, rb->rev);
    }
  else if (rb->rev_offset == -1)
    {
      committed_rev = 0;
    }

  if (SVN_IS_VALID_REVNUM(committed_rev))
    {
      SVN_ERR(svn_repos__validate_prop(SVN_PROP_REVISION_DATE,
                                       rb->datestamp, rb->pool));
      SVN_ERR(svn_ra_change_rev_prop2(rb->pb->session, committed_rev,
                                      SVN_PROP_REVISION_DATE,
                                      NULL, rb->datestamp, rb->pool));
      SVN_ERR(svn_repos__validate_prop(SVN_PROP_REVISION_AUTHOR,
                                       rb->author, rb->pool));
      SVN_ERR(svn_ra_change_rev_prop2(rb->pb->session, committed_rev,
                                      SVN_PROP_REVISION_AUTHOR,
                                      NULL, rb->author, rb->pool));
    }

  svn_pool_destroy(rb->pool);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_rdump__load_dumpstream(svn_stream_t *stream,
                           svn_ra_session_t *session,
                           svn_ra_session_t *aux_session,
                           svn_boolean_t quiet,
                           svn_cancel_func_t cancel_func,
                           void *cancel_baton,
                           apr_pool_t *pool)
{
  svn_repos_parse_fns3_t *parser;
  struct parse_baton *parse_baton;
  const svn_string_t *lock_string;
  svn_boolean_t be_atomic;
  svn_error_t *err;
  const char *session_url, *root_url, *parent_dir;

  SVN_ERR(svn_ra_has_capability(session, &be_atomic,
                                SVN_RA_CAPABILITY_ATOMIC_REVPROPS,
                                pool));
  SVN_ERR(get_lock(&lock_string, session, cancel_func, cancel_baton, pool));
  SVN_ERR(svn_ra_get_repos_root2(session, &root_url, pool));
  SVN_ERR(svn_ra_get_session_url(session, &session_url, pool));
  SVN_ERR(svn_ra_get_path_relative_to_root(session, &parent_dir,
                                           session_url, pool));

  parser = apr_pcalloc(pool, sizeof(*parser));
  parser->magic_header_record = magic_header_record;
  parser->uuid_record = uuid_record;
  parser->new_revision_record = new_revision_record;
  parser->new_node_record = new_node_record;
  parser->set_revision_property = set_revision_property;
  parser->set_node_property = set_node_property;
  parser->delete_node_property = delete_node_property;
  parser->remove_node_props = remove_node_props;
  parser->set_fulltext = set_fulltext;
  parser->apply_textdelta = apply_textdelta;
  parser->close_node = close_node;
  parser->close_revision = close_revision;

  parse_baton = apr_pcalloc(pool, sizeof(*parse_baton));
  parse_baton->session = session;
  parse_baton->aux_session = aux_session;
  parse_baton->quiet = quiet;
  parse_baton->root_url = root_url;
  parse_baton->parent_dir = parent_dir;
  parse_baton->rev_map = apr_hash_make(pool);
  parse_baton->last_rev_mapped = SVN_INVALID_REVNUM;
  parse_baton->oldest_dumpstream_rev = SVN_INVALID_REVNUM;

  err = svn_repos_parse_dumpstream3(stream, parser, parse_baton, FALSE,
                                    cancel_func, cancel_baton, pool);

  /* If all goes well, or if we're cancelled cleanly, don't leave a
     stray lock behind. */
  if ((! err) || (err && (err->apr_err == SVN_ERR_CANCELLED)))
    err = svn_error_compose_create(
              svn_ra__release_operational_lock(session, SVNRDUMP_PROP_LOCK,
                                               lock_string, pool),
              err);
  return err;
}
