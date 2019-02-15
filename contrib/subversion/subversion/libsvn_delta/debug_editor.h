/*
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


#ifndef SVN_DEBUG_EDITOR_H
#define SVN_DEBUG_EDITOR_H

#include "svn_delta.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Return a debug editor that wraps @a wrapped_editor.
 *
 * The debug editor simply prints an indication of what callbacks are being
 * called to @c stderr, and is only intended for use in debugging subversion
 * editors.
 */
svn_error_t *
svn_delta__get_debug_editor(const svn_delta_editor_t **editor,
                            void **edit_baton,
                            const svn_delta_editor_t *wrapped_editor,
                            void *wrapped_baton,
                            apr_pool_t *pool);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_DEBUG_EDITOR_H */
