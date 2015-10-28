/* temp_serializer.h : serialization functions for caching of FSFS structures
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

#ifndef SVN_LIBSVN_FS__TEMP_SERIALIZER_H
#define SVN_LIBSVN_FS__TEMP_SERIALIZER_H

#include "fs.h"

/**
 * Prepend the @a number to the @a string in a space efficient way such that
 * no other (number,string) combination can produce the same result.
 * Allocate temporaries as well as the result from @a pool.
 */
const char*
svn_fs_fs__combine_number_and_string(apr_int64_t number,
                                     const char *string,
                                     apr_pool_t *pool);

/**
 * Serialize a @a noderev_p within the serialization @a context.
 */
void
svn_fs_fs__noderev_serialize(struct svn_temp_serializer__context_t *context,
                             node_revision_t * const *noderev_p);

/**
 * Deserialize a @a noderev_p within the @a buffer.
 */
void
svn_fs_fs__noderev_deserialize(void *buffer,
                               node_revision_t **noderev_p);


/**
 * Adds position information to the the raw window data in WINDOW.
 */
typedef struct
{
  /* the (unprocessed) txdelta window byte sequence cached / to be cached */
  svn_string_t window;

  /* the offset within the representation right after reading the window */
  apr_off_t end_offset;
} svn_fs_fs__raw_cached_window_t;

/**
 * Implements #svn_cache__serialize_func_t for
 * #svn_fs_fs__raw_cached_window_t.
 */
svn_error_t *
svn_fs_fs__serialize_raw_window(void **buffer,
                                apr_size_t *buffer_size,
                                void *item,
                                apr_pool_t *pool);

/**
 * Implements #svn_cache__deserialize_func_t for
 * #svn_fs_fs__raw_cached_window_t.
 */
svn_error_t *
svn_fs_fs__deserialize_raw_window(void **item,
                                  void *buffer,
                                  apr_size_t buffer_size,
                                  apr_pool_t *pool);

/**
 * #svn_txdelta_window_t is not sufficient for caching the data it
 * represents because data read process needs auxiliary information.
 */
typedef struct
{
  /* the txdelta window information cached / to be cached */
  svn_txdelta_window_t *window;

  /* the revision file read pointer position right after reading the window */
  apr_off_t end_offset;
} svn_fs_fs__txdelta_cached_window_t;

/**
 * Implements #svn_cache__serialize_func_t for
 * #svn_fs_fs__txdelta_cached_window_t.
 */
svn_error_t *
svn_fs_fs__serialize_txdelta_window(void **buffer,
                                    apr_size_t *buffer_size,
                                    void *item,
                                    apr_pool_t *pool);

/**
 * Implements #svn_cache__deserialize_func_t for
 * #svn_fs_fs__txdelta_cached_window_t.
 */
svn_error_t *
svn_fs_fs__deserialize_txdelta_window(void **item,
                                      void *buffer,
                                      apr_size_t buffer_size,
                                      apr_pool_t *pool);

/**
 * Implements #svn_cache__serialize_func_t for a manifest
 * (@a in is an #apr_array_header_t of apr_off_t elements).
 */
svn_error_t *
svn_fs_fs__serialize_manifest(void **data,
                              apr_size_t *data_len,
                              void *in,
                              apr_pool_t *pool);

/**
 * Implements #svn_cache__deserialize_func_t for a manifest
 * (@a *out is an #apr_array_header_t of apr_off_t elements).
 */
svn_error_t *
svn_fs_fs__deserialize_manifest(void **out,
                                void *data,
                                apr_size_t data_len,
                                apr_pool_t *pool);

/**
 * Implements #svn_cache__serialize_func_t for a properties hash
 * (@a in is an #apr_hash_t of svn_string_t elements, keyed by const char*).
 */
svn_error_t *
svn_fs_fs__serialize_properties(void **data,
                                apr_size_t *data_len,
                                void *in,
                                apr_pool_t *pool);

/**
 * Implements #svn_cache__deserialize_func_t for a properties hash
 * (@a *out is an #apr_hash_t of svn_string_t elements, keyed by const char*).
 */
svn_error_t *
svn_fs_fs__deserialize_properties(void **out,
                                  void *data,
                                  apr_size_t data_len,
                                  apr_pool_t *pool);

/**
 * Implements #svn_cache__serialize_func_t for #svn_fs_id_t
 */
svn_error_t *
svn_fs_fs__serialize_id(void **data,
                        apr_size_t *data_len,
                        void *in,
                        apr_pool_t *pool);

/**
 * Implements #svn_cache__deserialize_func_t for #svn_fs_id_t
 */
svn_error_t *
svn_fs_fs__deserialize_id(void **out,
                          void *data,
                          apr_size_t data_len,
                          apr_pool_t *pool);

/**
 * Implements #svn_cache__serialize_func_t for #node_revision_t
 */
svn_error_t *
svn_fs_fs__serialize_node_revision(void **buffer,
                                   apr_size_t *buffer_size,
                                   void *item,
                                   apr_pool_t *pool);

/**
 * Implements #svn_cache__deserialize_func_t for #node_revision_t
 */
svn_error_t *
svn_fs_fs__deserialize_node_revision(void **item,
                                     void *buffer,
                                     apr_size_t buffer_size,
                                     apr_pool_t *pool);

/**
 * Implements #svn_cache__serialize_func_t for a directory contents array
 */
svn_error_t *
svn_fs_fs__serialize_dir_entries(void **data,
                                 apr_size_t *data_len,
                                 void *in,
                                 apr_pool_t *pool);

/**
 * Implements #svn_cache__deserialize_func_t for a directory contents array
 */
svn_error_t *
svn_fs_fs__deserialize_dir_entries(void **out,
                                   void *data,
                                   apr_size_t data_len,
                                   apr_pool_t *pool);

/**
 * Implements #svn_cache__partial_getter_func_t.  Set (apr_off_t) @a *out
 * to the element indexed by (apr_int64_t) @a *baton within the
 * serialized manifest array @a data and @a data_len. */
svn_error_t *
svn_fs_fs__get_sharded_offset(void **out,
                              const void *data,
                              apr_size_t data_len,
                              void *baton,
                              apr_pool_t *pool);

/**
 * Implements #svn_cache__partial_getter_func_t for a single
 * #svn_fs_dirent_t within a serialized directory contents hash,
 * identified by its name (const char @a *baton).
 */
svn_error_t *
svn_fs_fs__extract_dir_entry(void **out,
                             const void *data,
                             apr_size_t data_len,
                             void *baton,
                             apr_pool_t *pool);

/**
 * Describes the change to be done to a directory: Set the entry
 * identify by @a name to the value @a new_entry. If the latter is
 * @c NULL, the entry shall be removed if it exists. Otherwise it
 * will be replaced or automatically added, respectively.
 */
typedef struct replace_baton_t
{
  /** name of the directory entry to modify */
  const char *name;

  /** directory entry to insert instead */
  svn_fs_dirent_t *new_entry;
} replace_baton_t;

/**
 * Implements #svn_cache__partial_setter_func_t for a single
 * #svn_fs_dirent_t within a serialized directory contents hash,
 * identified by its name in the #replace_baton_t in @a baton.
 */
svn_error_t *
svn_fs_fs__replace_dir_entry(void **data,
                             apr_size_t *data_len,
                             void *baton,
                             apr_pool_t *pool);

/**
 * Implements #svn_cache__serialize_func_t for a #svn_fs_fs__rep_header_t.
 */
svn_error_t *
svn_fs_fs__serialize_rep_header(void **data,
                                apr_size_t *data_len,
                                void *in,
                                apr_pool_t *pool);

/**
 * Implements #svn_cache__deserialize_func_t for a #svn_fs_fs__rep_header_t.
 */
svn_error_t *
svn_fs_fs__deserialize_rep_header(void **out,
                                  void *data,
                                  apr_size_t data_len,
                                  apr_pool_t *pool);

/**
 * Implements #svn_cache__serialize_func_t for an #apr_array_header_t of
 * #change_t *.
 */
svn_error_t *
svn_fs_fs__serialize_changes(void **data,
                             apr_size_t *data_len,
                             void *in,
                             apr_pool_t *pool);

/**
 * Implements #svn_cache__deserialize_func_t for an #apr_array_header_t of
 * #change_t *.
 */
svn_error_t *
svn_fs_fs__deserialize_changes(void **out,
                               void *data,
                               apr_size_t data_len,
                               apr_pool_t *pool);

/**
 * Implements #svn_cache__serialize_func_t for #svn_mergeinfo_t objects.
 */
svn_error_t *
svn_fs_fs__serialize_mergeinfo(void **data,
                               apr_size_t *data_len,
                               void *in,
                               apr_pool_t *pool);

/**
 * Implements #svn_cache__deserialize_func_t for #svn_mergeinfo_t objects.
 */
svn_error_t *
svn_fs_fs__deserialize_mergeinfo(void **out,
                                 void *data,
                                 apr_size_t data_len,
                                 apr_pool_t *pool);

#endif
