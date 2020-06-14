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
 * @file svn_opt_impl.h
 * @brief Option and argument parsing for Subversion command lines
 *        (common implementation)
 *
 * @warning This is a @b private implementation-specific header file.
 *          User code should include @ref svn_opt.h instead.
 */

/* NOTE:
 * This file *must not* include or depend on any other header except
 * the C standard library headers.
 */

#ifndef SVN_OPT_IMPL_H
#define SVN_OPT_IMPL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/**
 * Various ways of specifying revisions.
 *
 * @note
 * In contexts where local mods are relevant, the `working' kind
 * refers to the uncommitted "working" revision, which may be modified
 * with respect to its base revision.  In other contexts, `working'
 * should behave the same as `committed' or `current'.
 */
/* NOTE: Update svnxx/revision.hpp when changing this enum. */
enum svn_opt_revision_kind {
  /** No revision information given. */
  svn_opt_revision_unspecified,

  /** revision given as number */
  svn_opt_revision_number,

  /** revision given as date */
  svn_opt_revision_date,

  /** rev of most recent change */
  svn_opt_revision_committed,

  /** (rev of most recent change) - 1 */
  svn_opt_revision_previous,

  /** .svn/entries current revision */
  svn_opt_revision_base,

  /** current, plus local mods */
  svn_opt_revision_working,

  /** repository youngest */
  svn_opt_revision_head

  /* please update svn_opt__revision_to_string() when extending this enum */
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_OPT_IMPL_H */
