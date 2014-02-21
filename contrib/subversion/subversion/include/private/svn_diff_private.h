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
 */


#ifndef SVN_DIFF_PRIVATE_H
#define SVN_DIFF_PRIVATE_H

#include <apr_pools.h>
#include <apr_tables.h>

#include "svn_types.h"
#include "svn_io.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



/* The separator string used below the "Index:" or similar line of
 * Subversion's Unidiff-like diff format.  */
#define SVN_DIFF__EQUAL_STRING \
  "==================================================================="

/* The separator string used below the "Properties on ..." line of
 * Subversion's Unidiff-like diff format.  */
#define SVN_DIFF__UNDER_STRING \
  "___________________________________________________________________"

/* The string used to mark a line in a hunk that doesn't end with a newline,
 * when diffing a file.  Intentionally not marked for translation, for wider
 * interoperability with patch(1) programs. */
#define SVN_DIFF__NO_NEWLINE_AT_END_OF_FILE \
          "\\ No newline at end of file"

/* The string used to mark a line in a hunk that doesn't end with a newline,
 * when diffing a Subversion property. */
#define SVN_DIFF__NO_NEWLINE_AT_END_OF_PROPERTY \
          "\\ No newline at end of property"

/* Write a unidiff "---" and "+++" header to OUTPUT_STREAM.
 *
 * Write "---" followed by a space and OLD_HEADER and a newline,
 * then "+++" followed by a space and NEW_HEADER and a newline.
 *
 * The text will be encoded into HEADER_ENCODING.
 */
svn_error_t *
svn_diff__unidiff_write_header(svn_stream_t *output_stream,
                               const char *header_encoding,
                               const char *old_header,
                               const char *new_header,
                               apr_pool_t *scratch_pool);

/* Display property changes in pseudo-Unidiff format.
 *
 * Write to @a outstream the changes described by @a propchanges based on
 * original properties @a original_props.
 *
 * Write all mark-up text (headers and so on) using the character encoding
 * @a encoding.
 *
 *   ### I think the idea is: we want the output to use @a encoding, and
 *       we will assume the text of the user's files and the values of any
 *       user-defined properties are already using @a encoding, so we don't
 *       want to re-code the *whole* output.
 *       So, shouldn't we also convert all prop names and all 'svn:*' prop
 *       values to @a encoding, since we know those are stored in UTF-8?
 *
 * @a original_props is a hash mapping (const char *) property names to
 * (svn_string_t *) values.  @a propchanges is an array of svn_prop_t
 * representing the new values for any of the properties that changed, with
 * a NULL value to represent deletion.
 *
 * If @a pretty_print_mergeinfo is true, then describe 'svn:mergeinfo'
 * property changes in a human-readable form that says what changes were
 * merged or reverse merged; otherwise (or if the mergeinfo property values
 * don't parse correctly) display them just like any other property.
 *
 * Use @a pool for temporary allocations.
 */
svn_error_t *
svn_diff__display_prop_diffs(svn_stream_t *outstream,
                             const char *encoding,
                             const apr_array_header_t *propchanges,
                             apr_hash_t *original_props,
                             svn_boolean_t pretty_print_mergeinfo,
                             apr_pool_t *pool);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_DIFF_PRIVATE_H */
