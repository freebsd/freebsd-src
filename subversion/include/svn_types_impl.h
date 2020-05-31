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
 *
 * @file svn_types_impl.h
 * @brief Subversion's data types (common implementation)
 *
 * @warning This is a @b private implementation-specific header file.
 *          User code should include @ref svn_types.h instead.
 */

/* NOTE:
 * This file *must not* include or depend on any other header except
 * the C standard library headers.
 */

#ifndef SVN_TYPES_IMPL_H
#define SVN_TYPES_IMPL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#ifndef DOXYGEN
/* Forward declaration of the error object. */
struct svn_error_t;
#endif


/** The various types of nodes in the Subversion filesystem. */
typedef enum svn_node_kind_t
{
  /** absent */
  svn_node_none,

  /** regular file */
  svn_node_file,

  /** directory */
  svn_node_dir,

  /** something's here, but we don't know what */
  svn_node_unknown,

  /**
   * symbolic link
   * @note This value is not currently used by the public API.
   * @since New in 1.8.
   */
  svn_node_symlink
} svn_node_kind_t;


/** Generic three-state property to represent an unknown value for values
 * that are just like booleans.  The values have been set deliberately to
 * make tristates disjoint from #svn_boolean_t.
 *
 * @note It is unsafe to use apr_pcalloc() to allocate these, since '0' is
 * not a valid value.
 *
 * @since New in 1.7. */
/* NOTE: Update svnxx/tristate.hpp when changing this enum. */
typedef enum svn_tristate_t
{
  /** state known to be false (the constant does not evaulate to false) */
  svn_tristate_false = 2,
  /** state known to be true */
  svn_tristate_true,
  /** state could be true or false */
  svn_tristate_unknown
} svn_tristate_t;


/** A revision number. */
/* NOTE: Update svnxx/revision.hpp when changing this typedef. */
typedef long int svn_revnum_t;

/** The 'official' invalid revision number. */
/* NOTE: Update svnxx/revision.hpp when changing this definition. */
#define SVN_INVALID_REVNUM ((svn_revnum_t) -1)


/** The concept of depth for directories.
 *
 * @note This is similar to, but not exactly the same as, the WebDAV
 * and LDAP concepts of depth.
 *
 * @since New in 1.5.
 */
/* NOTE: Update svnxx/depth.hpp when changing this enum. */
typedef enum svn_depth_t
{
  /* The order of these depths is important: the higher the number,
     the deeper it descends.  This allows us to compare two depths
     numerically to decide which should govern. */

  /** Depth undetermined or ignored.  In some contexts, this means the
      client should choose an appropriate default depth.  The server
      will generally treat it as #svn_depth_infinity. */
  svn_depth_unknown    = -2,

  /** Exclude (i.e., don't descend into) directory D.
      @note In Subversion 1.5, svn_depth_exclude is *not* supported
      anywhere in the client-side (libsvn_wc/libsvn_client/etc) code;
      it is only supported as an argument to set_path functions in the
      ra and repos reporters.  (This will enable future versions of
      Subversion to run updates, etc, against 1.5 servers with proper
      svn_depth_exclude behavior, once we get a chance to implement
      client-side support for svn_depth_exclude.)
  */
  svn_depth_exclude    = -1,

  /** Just the named directory D, no entries.  Updates will not pull in
      any files or subdirectories not already present. */
  svn_depth_empty      =  0,

  /** D + its file children, but not subdirs.  Updates will pull in any
      files not already present, but not subdirectories. */
  svn_depth_files      =  1,

  /** D + immediate children (D and its entries).  Updates will pull in
      any files or subdirectories not already present; those
      subdirectories' this_dir entries will have depth-empty. */
  svn_depth_immediates =  2,

  /** D + all descendants (full recursion from D).  Updates will pull
      in any files or subdirectories not already present; those
      subdirectories' this_dir entries will have depth-infinity.
      Equivalent to the pre-1.5 default update behavior. */
  svn_depth_infinity   =  3

} svn_depth_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_TYPES_IMPL_H */
