/* temp_serializer.c: serialization functions for caching of FSFS structures
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

#include "svn_pools.h"
#include "svn_hash.h"

#include "id.h"
#include "svn_fs.h"

#include "private/svn_fs_util.h"
#include "private/svn_temp_serializer.h"
#include "private/svn_subr_private.h"

#include "temp_serializer.h"

/* Utility to encode a signed NUMBER into a variable-length sequence of
 * 8-bit chars in KEY_BUFFER and return the last writen position.
 *
 * Numbers will be stored in 7 bits / byte and using byte values above
 * 32 (' ') to make them combinable with other string by simply separating
 * individual parts with spaces.
 */
static char*
encode_number(apr_int64_t number, char *key_buffer)
{
  /* encode the sign in the first byte */
  if (number < 0)
  {
    number = -number;
    *key_buffer = (char)((number & 63) + ' ' + 65);
  }
  else
    *key_buffer = (char)((number & 63) + ' ' + 1);
  number /= 64;

  /* write 7 bits / byte until no significant bits are left */
  while (number)
  {
    *++key_buffer = (char)((number & 127) + ' ' + 1);
    number /= 128;
  }

  /* return the last written position */
  return key_buffer;
}

const char*
svn_fs_fs__combine_number_and_string(apr_int64_t number,
                                     const char *string,
                                     apr_pool_t *pool)
{
  apr_size_t len = strlen(string);

  /* number part requires max. 10x7 bits + 1 space.
   * Add another 1 for the terminal 0 */
  char *key_buffer = apr_palloc(pool, len + 12);
  const char *key = key_buffer;

  /* Prepend the number to the string and separate them by space. No other
   * number can result in the same prefix, no other string in the same
   * postfix nor can the boundary between them be ambiguous. */
  key_buffer = encode_number(number, key_buffer);
  *++key_buffer = ' ';
  memcpy(++key_buffer, string, len+1);

  /* return the start of the key */
  return key;
}

/* Utility function to serialize string S in the given serialization CONTEXT.
 */
static void
serialize_svn_string(svn_temp_serializer__context_t *context,
                     const svn_string_t * const *s)
{
  const svn_string_t *string = *s;

  /* Nothing to do for NULL string references. */
  if (string == NULL)
    return;

  svn_temp_serializer__push(context,
                            (const void * const *)s,
                            sizeof(*string));

  /* the "string" content may actually be arbitrary binary data.
   * Thus, we cannot use svn_temp_serializer__add_string. */
  svn_temp_serializer__push(context,
                            (const void * const *)&string->data,
                            string->len + 1);

  /* back to the caller's nesting level */
  svn_temp_serializer__pop(context);
  svn_temp_serializer__pop(context);
}

/* Utility function to deserialize the STRING inside the BUFFER.
 */
static void
deserialize_svn_string(void *buffer, svn_string_t **string)
{
  svn_temp_deserializer__resolve(buffer, (void **)string);
  if (*string == NULL)
    return;

  svn_temp_deserializer__resolve(*string, (void **)&(*string)->data);
}

/* Utility function to serialize checkum CS within the given serialization
 * CONTEXT.
 */
static void
serialize_checksum(svn_temp_serializer__context_t *context,
                   svn_checksum_t * const *cs)
{
  const svn_checksum_t *checksum = *cs;
  if (checksum == NULL)
    return;

  svn_temp_serializer__push(context,
                            (const void * const *)cs,
                            sizeof(*checksum));

  /* The digest is arbitrary binary data.
   * Thus, we cannot use svn_temp_serializer__add_string. */
  svn_temp_serializer__push(context,
                            (const void * const *)&checksum->digest,
                            svn_checksum_size(checksum));

  /* return to the caller's nesting level */
  svn_temp_serializer__pop(context);
  svn_temp_serializer__pop(context);
}

/* Utility function to deserialize the checksum CS inside the BUFFER.
 */
static void
deserialize_checksum(void *buffer, svn_checksum_t **cs)
{
  svn_temp_deserializer__resolve(buffer, (void **)cs);
  if (*cs == NULL)
    return;

  svn_temp_deserializer__resolve(*cs, (void **)&(*cs)->digest);
}

/* Utility function to serialize the REPRESENTATION within the given
 * serialization CONTEXT.
 */
static void
serialize_representation(svn_temp_serializer__context_t *context,
                         representation_t * const *representation)
{
  const representation_t * rep = *representation;
  if (rep == NULL)
    return;

  /* serialize the representation struct itself */
  svn_temp_serializer__push(context,
                            (const void * const *)representation,
                            sizeof(*rep));

  /* serialize sub-structures */
  serialize_checksum(context, &rep->md5_checksum);
  serialize_checksum(context, &rep->sha1_checksum);

  svn_temp_serializer__add_string(context, &rep->txn_id);
  svn_temp_serializer__add_string(context, &rep->uniquifier);

  /* return to the caller's nesting level */
  svn_temp_serializer__pop(context);
}

/* Utility function to deserialize the REPRESENTATIONS inside the BUFFER.
 */
static void
deserialize_representation(void *buffer,
                           representation_t **representation)
{
  representation_t *rep;

  /* fixup the reference to the representation itself */
  svn_temp_deserializer__resolve(buffer, (void **)representation);
  rep = *representation;
  if (rep == NULL)
    return;

  /* fixup of sub-structures */
  deserialize_checksum(rep, &rep->md5_checksum);
  deserialize_checksum(rep, &rep->sha1_checksum);

  svn_temp_deserializer__resolve(rep, (void **)&rep->txn_id);
  svn_temp_deserializer__resolve(rep, (void **)&rep->uniquifier);
}

/* auxilliary structure representing the content of a directory hash */
typedef struct hash_data_t
{
  /* number of entries in the directory */
  apr_size_t count;

  /* number of unused dir entry buckets in the index */
  apr_size_t over_provision;

  /* internal modifying operations counter
   * (used to repack data once in a while) */
  apr_size_t operations;

  /* size of the serialization buffer actually used.
   * (we will allocate more than we actually need such that we may
   * append more data in situ later) */
  apr_size_t len;

  /* reference to the entries */
  svn_fs_dirent_t **entries;

  /* size of the serialized entries and don't be too wasteful
   * (needed since the entries are no longer in sequence) */
  apr_uint32_t *lengths;
} hash_data_t;

static int
compare_dirent_id_names(const void *lhs, const void *rhs)
{
  return strcmp((*(const svn_fs_dirent_t *const *)lhs)->name,
                (*(const svn_fs_dirent_t *const *)rhs)->name);
}

/* Utility function to serialize the *ENTRY_P into a the given
 * serialization CONTEXT. Return the serialized size of the
 * dir entry in *LENGTH.
 */
static void
serialize_dir_entry(svn_temp_serializer__context_t *context,
                    svn_fs_dirent_t **entry_p,
                    apr_uint32_t *length)
{
  svn_fs_dirent_t *entry = *entry_p;
  apr_size_t initial_length = svn_temp_serializer__get_length(context);

  svn_temp_serializer__push(context,
                            (const void * const *)entry_p,
                            sizeof(svn_fs_dirent_t));

  svn_fs_fs__id_serialize(context, &entry->id);
  svn_temp_serializer__add_string(context, &entry->name);

  *length = (apr_uint32_t)(  svn_temp_serializer__get_length(context)
                           - APR_ALIGN_DEFAULT(initial_length));

  svn_temp_serializer__pop(context);
}

/* Utility function to serialize the ENTRIES into a new serialization
 * context to be returned. Allocation will be made form POOL.
 */
static svn_temp_serializer__context_t *
serialize_dir(apr_hash_t *entries, apr_pool_t *pool)
{
  hash_data_t hash_data;
  apr_hash_index_t *hi;
  apr_size_t i = 0;
  svn_temp_serializer__context_t *context;

  /* calculate sizes */
  apr_size_t count = apr_hash_count(entries);
  apr_size_t over_provision = 2 + count / 4;
  apr_size_t entries_len = (count + over_provision) * sizeof(svn_fs_dirent_t*);
  apr_size_t lengths_len = (count + over_provision) * sizeof(apr_uint32_t);

  /* copy the hash entries to an auxilliary struct of known layout */
  hash_data.count = count;
  hash_data.over_provision = over_provision;
  hash_data.operations = 0;
  hash_data.entries = apr_palloc(pool, entries_len);
  hash_data.lengths = apr_palloc(pool, lengths_len);

  for (hi = apr_hash_first(pool, entries); hi; hi = apr_hash_next(hi), ++i)
    hash_data.entries[i] = svn__apr_hash_index_val(hi);

  /* sort entry index by ID name */
  qsort(hash_data.entries,
        count,
        sizeof(*hash_data.entries),
        compare_dirent_id_names);

  /* Serialize that aux. structure into a new one. Also, provide a good
   * estimate for the size of the buffer that we will need. */
  context = svn_temp_serializer__init(&hash_data,
                                      sizeof(hash_data),
                                      50 + count * 200 + entries_len,
                                      pool);

  /* serialize entries references */
  svn_temp_serializer__push(context,
                            (const void * const *)&hash_data.entries,
                            entries_len);

  /* serialize the individual entries and their sub-structures */
  for (i = 0; i < count; ++i)
    serialize_dir_entry(context,
                        &hash_data.entries[i],
                        &hash_data.lengths[i]);

  svn_temp_serializer__pop(context);

  /* serialize entries references */
  svn_temp_serializer__push(context,
                            (const void * const *)&hash_data.lengths,
                            lengths_len);

  return context;
}

/* Utility function to reconstruct a dir entries hash from serialized data
 * in BUFFER and HASH_DATA. Allocation will be made form POOL.
 */
static apr_hash_t *
deserialize_dir(void *buffer, hash_data_t *hash_data, apr_pool_t *pool)
{
  apr_hash_t *result = svn_hash__make(pool);
  apr_size_t i;
  apr_size_t count;
  svn_fs_dirent_t *entry;
  svn_fs_dirent_t **entries;

  /* resolve the reference to the entries array */
  svn_temp_deserializer__resolve(buffer, (void **)&hash_data->entries);
  entries = hash_data->entries;

  /* fixup the references within each entry and add it to the hash */
  for (i = 0, count = hash_data->count; i < count; ++i)
    {
      svn_temp_deserializer__resolve(entries, (void **)&entries[i]);
      entry = hash_data->entries[i];

      /* pointer fixup */
      svn_temp_deserializer__resolve(entry, (void **)&entry->name);
      svn_fs_fs__id_deserialize(entry, (svn_fs_id_t **)&entry->id);

      /* add the entry to the hash */
      svn_hash_sets(result, entry->name, entry);
    }

  /* return the now complete hash */
  return result;
}

void
svn_fs_fs__noderev_serialize(svn_temp_serializer__context_t *context,
                             node_revision_t * const *noderev_p)
{
  const node_revision_t *noderev = *noderev_p;
  if (noderev == NULL)
    return;

  /* serialize the representation struct itself */
  svn_temp_serializer__push(context,
                            (const void * const *)noderev_p,
                            sizeof(*noderev));

  /* serialize sub-structures */
  svn_fs_fs__id_serialize(context, &noderev->id);
  svn_fs_fs__id_serialize(context, &noderev->predecessor_id);
  serialize_representation(context, &noderev->prop_rep);
  serialize_representation(context, &noderev->data_rep);

  svn_temp_serializer__add_string(context, &noderev->copyfrom_path);
  svn_temp_serializer__add_string(context, &noderev->copyroot_path);
  svn_temp_serializer__add_string(context, &noderev->created_path);

  /* return to the caller's nesting level */
  svn_temp_serializer__pop(context);
}


void
svn_fs_fs__noderev_deserialize(void *buffer,
                               node_revision_t **noderev_p)
{
  node_revision_t *noderev;

  /* fixup the reference to the representation itself,
   * if this is part of a parent structure. */
  if (buffer != *noderev_p)
    svn_temp_deserializer__resolve(buffer, (void **)noderev_p);

  noderev = *noderev_p;
  if (noderev == NULL)
    return;

  /* fixup of sub-structures */
  svn_fs_fs__id_deserialize(noderev, (svn_fs_id_t **)&noderev->id);
  svn_fs_fs__id_deserialize(noderev, (svn_fs_id_t **)&noderev->predecessor_id);
  deserialize_representation(noderev, &noderev->prop_rep);
  deserialize_representation(noderev, &noderev->data_rep);

  svn_temp_deserializer__resolve(noderev, (void **)&noderev->copyfrom_path);
  svn_temp_deserializer__resolve(noderev, (void **)&noderev->copyroot_path);
  svn_temp_deserializer__resolve(noderev, (void **)&noderev->created_path);
}


/* Utility function to serialize COUNT svn_txdelta_op_t objects
 * at OPS in the given serialization CONTEXT.
 */
static void
serialize_txdelta_ops(svn_temp_serializer__context_t *context,
                      const svn_txdelta_op_t * const * ops,
                      apr_size_t count)
{
  if (*ops == NULL)
    return;

  /* the ops form a contiguous chunk of memory with no further references */
  svn_temp_serializer__push(context,
                            (const void * const *)ops,
                            count * sizeof(svn_txdelta_op_t));
  svn_temp_serializer__pop(context);
}

/* Utility function to serialize W in the given serialization CONTEXT.
 */
static void
serialize_txdeltawindow(svn_temp_serializer__context_t *context,
                        svn_txdelta_window_t * const * w)
{
  svn_txdelta_window_t *window = *w;

  /* serialize the window struct itself */
  svn_temp_serializer__push(context,
                            (const void * const *)w,
                            sizeof(svn_txdelta_window_t));

  /* serialize its sub-structures */
  serialize_txdelta_ops(context, &window->ops, window->num_ops);
  serialize_svn_string(context, &window->new_data);

  svn_temp_serializer__pop(context);
}

svn_error_t *
svn_fs_fs__serialize_txdelta_window(void **buffer,
                                    apr_size_t *buffer_size,
                                    void *item,
                                    apr_pool_t *pool)
{
  svn_fs_fs__txdelta_cached_window_t *window_info = item;
  svn_stringbuf_t *serialized;

  /* initialize the serialization process and allocate a buffer large
   * enough to do without the need of re-allocations in most cases. */
  apr_size_t text_len = window_info->window->new_data
                      ? window_info->window->new_data->len
                      : 0;
  svn_temp_serializer__context_t *context =
      svn_temp_serializer__init(window_info,
                                sizeof(*window_info),
                                500 + text_len,
                                pool);

  /* serialize the sub-structure(s) */
  serialize_txdeltawindow(context, &window_info->window);

  /* return the serialized result */
  serialized = svn_temp_serializer__get(context);

  *buffer = serialized->data;
  *buffer_size = serialized->len;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__deserialize_txdelta_window(void **item,
                                      void *buffer,
                                      apr_size_t buffer_size,
                                      apr_pool_t *pool)
{
  svn_txdelta_window_t *window;

  /* Copy the _full_ buffer as it also contains the sub-structures. */
  svn_fs_fs__txdelta_cached_window_t *window_info =
      (svn_fs_fs__txdelta_cached_window_t *)buffer;

  /* pointer reference fixup */
  svn_temp_deserializer__resolve(window_info,
                                 (void **)&window_info->window);
  window = window_info->window;

  svn_temp_deserializer__resolve(window, (void **)&window->ops);

  deserialize_svn_string(window, (svn_string_t**)&window->new_data);

  /* done */
  *item = window_info;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__serialize_manifest(void **data,
                              apr_size_t *data_len,
                              void *in,
                              apr_pool_t *pool)
{
  apr_array_header_t *manifest = in;

  *data_len = sizeof(apr_off_t) *manifest->nelts;
  *data = apr_palloc(pool, *data_len);
  memcpy(*data, manifest->elts, *data_len);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__deserialize_manifest(void **out,
                                void *data,
                                apr_size_t data_len,
                                apr_pool_t *pool)
{
  apr_array_header_t *manifest = apr_array_make(pool, 1, sizeof(apr_off_t));

  manifest->nelts = (int) (data_len / sizeof(apr_off_t));
  manifest->nalloc = (int) (data_len / sizeof(apr_off_t));
  manifest->elts = (char*)data;

  *out = manifest;

  return SVN_NO_ERROR;
}

/* Auxilliary structure representing the content of a properties hash.
   This structure is much easier to (de-)serialize than an apr_hash.
 */
typedef struct properties_data_t
{
  /* number of entries in the hash */
  apr_size_t count;

  /* reference to the keys */
  const char **keys;

  /* reference to the values */
  const svn_string_t **values;
} properties_data_t;

/* Serialize COUNT C-style strings from *STRINGS into CONTEXT. */
static void
serialize_cstring_array(svn_temp_serializer__context_t *context,
                        const char ***strings,
                        apr_size_t count)
{
  apr_size_t i;
  const char **entries = *strings;

  /* serialize COUNT entries pointers (the array) */
  svn_temp_serializer__push(context,
                            (const void * const *)strings,
                            count * sizeof(const char*));

  /* serialize array elements */
  for (i = 0; i < count; ++i)
    svn_temp_serializer__add_string(context, &entries[i]);

  svn_temp_serializer__pop(context);
}

/* Serialize COUNT svn_string_t* items from *STRINGS into CONTEXT. */
static void
serialize_svn_string_array(svn_temp_serializer__context_t *context,
                           const svn_string_t ***strings,
                           apr_size_t count)
{
  apr_size_t i;
  const svn_string_t **entries = *strings;

  /* serialize COUNT entries pointers (the array) */
  svn_temp_serializer__push(context,
                            (const void * const *)strings,
                            count * sizeof(const char*));

  /* serialize array elements */
  for (i = 0; i < count; ++i)
    serialize_svn_string(context, &entries[i]);

  svn_temp_serializer__pop(context);
}

svn_error_t *
svn_fs_fs__serialize_properties(void **data,
                                apr_size_t *data_len,
                                void *in,
                                apr_pool_t *pool)
{
  apr_hash_t *hash = in;
  properties_data_t properties;
  svn_temp_serializer__context_t *context;
  apr_hash_index_t *hi;
  svn_stringbuf_t *serialized;
  apr_size_t i;

  /* create our auxilliary data structure */
  properties.count = apr_hash_count(hash);
  properties.keys = apr_palloc(pool, sizeof(const char*) * (properties.count + 1));
  properties.values = apr_palloc(pool, sizeof(const char*) * properties.count);

  /* populate it with the hash entries */
  for (hi = apr_hash_first(pool, hash), i=0; hi; hi = apr_hash_next(hi), ++i)
    {
      properties.keys[i] = svn__apr_hash_index_key(hi);
      properties.values[i] = svn__apr_hash_index_val(hi);
    }

  /* serialize it */
  context = svn_temp_serializer__init(&properties,
                                      sizeof(properties),
                                      properties.count * 100,
                                      pool);

  properties.keys[i] = "";
  serialize_cstring_array(context, &properties.keys, properties.count + 1);
  serialize_svn_string_array(context, &properties.values, properties.count);

  /* return the serialized result */
  serialized = svn_temp_serializer__get(context);

  *data = serialized->data;
  *data_len = serialized->len;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__deserialize_properties(void **out,
                                  void *data,
                                  apr_size_t data_len,
                                  apr_pool_t *pool)
{
  apr_hash_t *hash = svn_hash__make(pool);
  properties_data_t *properties = (properties_data_t *)data;
  size_t i;

  /* de-serialize our auxilliary data structure */
  svn_temp_deserializer__resolve(properties, (void**)&properties->keys);
  svn_temp_deserializer__resolve(properties, (void**)&properties->values);

  /* de-serialize each entry and put it into the hash */
  for (i = 0; i < properties->count; ++i)
    {
      apr_size_t len = properties->keys[i+1] - properties->keys[i] - 1;
      svn_temp_deserializer__resolve((void*)properties->keys,
                                     (void**)&properties->keys[i]);

      deserialize_svn_string((void*)properties->values,
                             (svn_string_t **)&properties->values[i]);

      apr_hash_set(hash,
                   properties->keys[i], len,
                   properties->values[i]);
    }

  /* done */
  *out = hash;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__serialize_id(void **data,
                        apr_size_t *data_len,
                        void *in,
                        apr_pool_t *pool)
{
  const svn_fs_id_t *id = in;
  svn_stringbuf_t *serialized;

  /* create an (empty) serialization context with plenty of buffer space */
  svn_temp_serializer__context_t *context =
      svn_temp_serializer__init(NULL, 0, 250, pool);

  /* serialize the id */
  svn_fs_fs__id_serialize(context, &id);

  /* return serialized data */
  serialized = svn_temp_serializer__get(context);
  *data = serialized->data;
  *data_len = serialized->len;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__deserialize_id(void **out,
                          void *data,
                          apr_size_t data_len,
                          apr_pool_t *pool)
{
  /* Copy the _full_ buffer as it also contains the sub-structures. */
  svn_fs_id_t *id = (svn_fs_id_t *)data;

  /* fixup of all pointers etc. */
  svn_fs_fs__id_deserialize(id, &id);

  /* done */
  *out = id;
  return SVN_NO_ERROR;
}

/** Caching node_revision_t objects. **/

svn_error_t *
svn_fs_fs__serialize_node_revision(void **buffer,
                                   apr_size_t *buffer_size,
                                   void *item,
                                   apr_pool_t *pool)
{
  svn_stringbuf_t *serialized;
  node_revision_t *noderev = item;

  /* create an (empty) serialization context with plenty of (initial)
   * buffer space. */
  svn_temp_serializer__context_t *context =
      svn_temp_serializer__init(NULL, 0,
                                1024 - SVN_TEMP_SERIALIZER__OVERHEAD,
                                pool);

  /* serialize the noderev */
  svn_fs_fs__noderev_serialize(context, &noderev);

  /* return serialized data */
  serialized = svn_temp_serializer__get(context);
  *buffer = serialized->data;
  *buffer_size = serialized->len;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__deserialize_node_revision(void **item,
                                     void *buffer,
                                     apr_size_t buffer_size,
                                     apr_pool_t *pool)
{
  /* Copy the _full_ buffer as it also contains the sub-structures. */
  node_revision_t *noderev = (node_revision_t *)buffer;

  /* fixup of all pointers etc. */
  svn_fs_fs__noderev_deserialize(noderev, &noderev);

  /* done */
  *item = noderev;
  return SVN_NO_ERROR;
}

/* Utility function that returns the directory serialized inside CONTEXT
 * to DATA and DATA_LEN. */
static svn_error_t *
return_serialized_dir_context(svn_temp_serializer__context_t *context,
                              void **data,
                              apr_size_t *data_len)
{
  svn_stringbuf_t *serialized = svn_temp_serializer__get(context);

  *data = serialized->data;
  *data_len = serialized->blocksize;
  ((hash_data_t *)serialized->data)->len = serialized->len;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__serialize_dir_entries(void **data,
                                 apr_size_t *data_len,
                                 void *in,
                                 apr_pool_t *pool)
{
  apr_hash_t *dir = in;

  /* serialize the dir content into a new serialization context
   * and return the serialized data */
  return return_serialized_dir_context(serialize_dir(dir, pool),
                                       data,
                                       data_len);
}

svn_error_t *
svn_fs_fs__deserialize_dir_entries(void **out,
                                   void *data,
                                   apr_size_t data_len,
                                   apr_pool_t *pool)
{
  /* Copy the _full_ buffer as it also contains the sub-structures. */
  hash_data_t *hash_data = (hash_data_t *)data;

  /* reconstruct the hash from the serialized data */
  *out = deserialize_dir(hash_data, hash_data, pool);

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__get_sharded_offset(void **out,
                              const void *data,
                              apr_size_t data_len,
                              void *baton,
                              apr_pool_t *pool)
{
  const apr_off_t *manifest = data;
  apr_int64_t shard_pos = *(apr_int64_t *)baton;

  *(apr_off_t *)out = manifest[shard_pos];

  return SVN_NO_ERROR;
}

/* Utility function that returns the lowest index of the first entry in
 * *ENTRIES that points to a dir entry with a name equal or larger than NAME.
 * If an exact match has been found, *FOUND will be set to TRUE. COUNT is
 * the number of valid entries in ENTRIES.
 */
static apr_size_t
find_entry(svn_fs_dirent_t **entries,
           const char *name,
           apr_size_t count,
           svn_boolean_t *found)
{
  /* binary search for the desired entry by name */
  apr_size_t lower = 0;
  apr_size_t upper = count;
  apr_size_t middle;

  for (middle = upper / 2; lower < upper; middle = (upper + lower) / 2)
    {
      const svn_fs_dirent_t *entry =
          svn_temp_deserializer__ptr(entries, (const void *const *)&entries[middle]);
      const char* entry_name =
          svn_temp_deserializer__ptr(entry, (const void *const *)&entry->name);

      int diff = strcmp(entry_name, name);
      if (diff < 0)
        lower = middle + 1;
      else
        upper = middle;
    }

  /* check whether we actually found a match */
  *found = FALSE;
  if (lower < count)
    {
      const svn_fs_dirent_t *entry =
          svn_temp_deserializer__ptr(entries, (const void *const *)&entries[lower]);
      const char* entry_name =
          svn_temp_deserializer__ptr(entry, (const void *const *)&entry->name);

      if (strcmp(entry_name, name) == 0)
        *found = TRUE;
    }

  return lower;
}

svn_error_t *
svn_fs_fs__extract_dir_entry(void **out,
                             const void *data,
                             apr_size_t data_len,
                             void *baton,
                             apr_pool_t *pool)
{
  const hash_data_t *hash_data = data;
  const char* name = baton;
  svn_boolean_t found;

  /* resolve the reference to the entries array */
  const svn_fs_dirent_t * const *entries =
    svn_temp_deserializer__ptr(data, (const void *const *)&hash_data->entries);

  /* resolve the reference to the lengths array */
  const apr_uint32_t *lengths =
    svn_temp_deserializer__ptr(data, (const void *const *)&hash_data->lengths);

  /* binary search for the desired entry by name */
  apr_size_t pos = find_entry((svn_fs_dirent_t **)entries,
                              name,
                              hash_data->count,
                              &found);

  /* de-serialize that entry or return NULL, if no match has been found */
  *out = NULL;
  if (found)
    {
      const svn_fs_dirent_t *source =
          svn_temp_deserializer__ptr(entries, (const void *const *)&entries[pos]);

      /* Entries have been serialized one-by-one, each time including all
       * nested structures and strings. Therefore, they occupy a single
       * block of memory whose end-offset is either the beginning of the
       * next entry or the end of the buffer
       */
      apr_size_t size = lengths[pos];

      /* copy & deserialize the entry */
      svn_fs_dirent_t *new_entry = apr_palloc(pool, size);
      memcpy(new_entry, source, size);

      svn_temp_deserializer__resolve(new_entry, (void **)&new_entry->name);
      svn_fs_fs__id_deserialize(new_entry, (svn_fs_id_t **)&new_entry->id);
      *(svn_fs_dirent_t **)out = new_entry;
    }

  return SVN_NO_ERROR;
}

/* Utility function for svn_fs_fs__replace_dir_entry that implements the
 * modification as a simply deserialize / modify / serialize sequence.
 */
static svn_error_t *
slowly_replace_dir_entry(void **data,
                         apr_size_t *data_len,
                         void *baton,
                         apr_pool_t *pool)
{
  replace_baton_t *replace_baton = (replace_baton_t *)baton;
  hash_data_t *hash_data = (hash_data_t *)*data;
  apr_hash_t *dir;

  SVN_ERR(svn_fs_fs__deserialize_dir_entries((void **)&dir,
                                             *data,
                                             hash_data->len,
                                             pool));
  svn_hash_sets(dir, replace_baton->name, replace_baton->new_entry);

  return svn_fs_fs__serialize_dir_entries(data, data_len, dir, pool);
}

svn_error_t *
svn_fs_fs__replace_dir_entry(void **data,
                             apr_size_t *data_len,
                             void *baton,
                             apr_pool_t *pool)
{
  replace_baton_t *replace_baton = (replace_baton_t *)baton;
  hash_data_t *hash_data = (hash_data_t *)*data;
  svn_boolean_t found;
  svn_fs_dirent_t **entries;
  apr_uint32_t *lengths;
  apr_uint32_t length;
  apr_size_t pos;

  svn_temp_serializer__context_t *context;

  /* after quite a number of operations, let's re-pack everything.
   * This is to limit the number of vasted space as we cannot overwrite
   * existing data but must always append. */
  if (hash_data->operations > 2 + hash_data->count / 4)
    return slowly_replace_dir_entry(data, data_len, baton, pool);

  /* resolve the reference to the entries array */
  entries = (svn_fs_dirent_t **)
    svn_temp_deserializer__ptr((const char *)hash_data,
                               (const void *const *)&hash_data->entries);

  /* resolve the reference to the lengths array */
  lengths = (apr_uint32_t *)
    svn_temp_deserializer__ptr((const char *)hash_data,
                               (const void *const *)&hash_data->lengths);

  /* binary search for the desired entry by name */
  pos = find_entry(entries, replace_baton->name, hash_data->count, &found);

  /* handle entry removal (if found at all) */
  if (replace_baton->new_entry == NULL)
    {
      if (found)
        {
          /* remove reference to the entry from the index */
          memmove(&entries[pos],
                  &entries[pos + 1],
                  sizeof(entries[pos]) * (hash_data->count - pos));
          memmove(&lengths[pos],
                  &lengths[pos + 1],
                  sizeof(lengths[pos]) * (hash_data->count - pos));

          hash_data->count--;
          hash_data->over_provision++;
          hash_data->operations++;
        }

      return SVN_NO_ERROR;
    }

  /* if not found, prepare to insert the new entry */
  if (!found)
    {
      /* fallback to slow operation if there is no place left to insert an
       * new entry to index. That will automatically give add some spare
       * entries ("overprovision"). */
      if (hash_data->over_provision == 0)
        return slowly_replace_dir_entry(data, data_len, baton, pool);

      /* make entries[index] available for pointing to the new entry */
      memmove(&entries[pos + 1],
              &entries[pos],
              sizeof(entries[pos]) * (hash_data->count - pos));
      memmove(&lengths[pos + 1],
              &lengths[pos],
              sizeof(lengths[pos]) * (hash_data->count - pos));

      hash_data->count++;
      hash_data->over_provision--;
      hash_data->operations++;
    }

  /* de-serialize the new entry */
  entries[pos] = replace_baton->new_entry;
  context = svn_temp_serializer__init_append(hash_data,
                                             entries,
                                             hash_data->len,
                                             *data_len,
                                             pool);
  serialize_dir_entry(context, &entries[pos], &length);

  /* return the updated serialized data */
  SVN_ERR (return_serialized_dir_context(context,
                                         data,
                                         data_len));

  /* since the previous call may have re-allocated the buffer, the lengths
   * pointer may no longer point to the entry in that buffer. Therefore,
   * re-map it again and store the length value after that. */

  hash_data = (hash_data_t *)*data;
  lengths = (apr_uint32_t *)
    svn_temp_deserializer__ptr((const char *)hash_data,
                               (const void *const *)&hash_data->lengths);
  lengths[pos] = length;

  return SVN_NO_ERROR;
}

/* Utility function to serialize change CHANGE_P in the given serialization
 * CONTEXT.
 */
static void
serialize_change(svn_temp_serializer__context_t *context,
                 change_t * const *change_p)
{
  const change_t * change = *change_p;
  if (change == NULL)
    return;

  /* serialize the change struct itself */
  svn_temp_serializer__push(context,
                            (const void * const *)change_p,
                            sizeof(*change));

  /* serialize sub-structures */
  svn_fs_fs__id_serialize(context, &change->noderev_id);

  svn_temp_serializer__add_string(context, &change->path);
  svn_temp_serializer__add_string(context, &change->copyfrom_path);

  /* return to the caller's nesting level */
  svn_temp_serializer__pop(context);
}

/* Utility function to serialize the CHANGE_P within the given
 * serialization CONTEXT.
 */
static void
deserialize_change(void *buffer, change_t **change_p)
{
  change_t * change;

  /* fix-up of the pointer to the struct in question */
  svn_temp_deserializer__resolve(buffer, (void **)change_p);

  change = *change_p;
  if (change == NULL)
    return;

  /* fix-up of sub-structures */
  svn_fs_fs__id_deserialize(change, (svn_fs_id_t **)&change->noderev_id);

  svn_temp_deserializer__resolve(change, (void **)&change->path);
  svn_temp_deserializer__resolve(change, (void **)&change->copyfrom_path);
}

/* Auxiliary structure representing the content of a change_t array.
   This structure is much easier to (de-)serialize than an APR array.
 */
typedef struct changes_data_t
{
  /* number of entries in the array */
  int count;

  /* reference to the changes */
  change_t **changes;
} changes_data_t;

svn_error_t *
svn_fs_fs__serialize_changes(void **data,
                             apr_size_t *data_len,
                             void *in,
                             apr_pool_t *pool)
{
  apr_array_header_t *array = in;
  changes_data_t changes;
  svn_temp_serializer__context_t *context;
  svn_stringbuf_t *serialized;
  int i;

  /* initialize our auxiliary data structure */
  changes.count = array->nelts;
  changes.changes = apr_palloc(pool, sizeof(change_t*) * changes.count);

  /* populate it with the array elements */
  for (i = 0; i < changes.count; ++i)
    changes.changes[i] = APR_ARRAY_IDX(array, i, change_t*);

  /* serialize it and all its elements */
  context = svn_temp_serializer__init(&changes,
                                      sizeof(changes),
                                      changes.count * 100,
                                      pool);

  svn_temp_serializer__push(context,
                            (const void * const *)&changes.changes,
                            changes.count * sizeof(change_t*));

  for (i = 0; i < changes.count; ++i)
    serialize_change(context, &changes.changes[i]);

  svn_temp_serializer__pop(context);

  /* return the serialized result */
  serialized = svn_temp_serializer__get(context);

  *data = serialized->data;
  *data_len = serialized->len;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__deserialize_changes(void **out,
                               void *data,
                               apr_size_t data_len,
                               apr_pool_t *pool)
{
  int i;
  changes_data_t *changes = (changes_data_t *)data;
  apr_array_header_t *array = apr_array_make(pool, changes->count,
                                             sizeof(change_t *));

  /* de-serialize our auxiliary data structure */
  svn_temp_deserializer__resolve(changes, (void**)&changes->changes);

  /* de-serialize each entry and add it to the array */
  for (i = 0; i < changes->count; ++i)
    {
      deserialize_change((void*)changes->changes,
                         (change_t **)&changes->changes[i]);
      APR_ARRAY_PUSH(array, change_t *) = changes->changes[i];
    }

  /* done */
  *out = array;

  return SVN_NO_ERROR;
}

/* Auxiliary structure representing the content of a svn_mergeinfo_t hash.
   This structure is much easier to (de-)serialize than an APR array.
 */
typedef struct mergeinfo_data_t
{
  /* number of paths in the hash */
  unsigned count;

  /* COUNT keys (paths) */
  const char **keys;

  /* COUNT keys lengths (strlen of path) */
  apr_ssize_t *key_lengths;

  /* COUNT entries, each giving the number of ranges for the key */
  int *range_counts;

  /* all ranges in a single, concatenated buffer */
  svn_merge_range_t *ranges;
} mergeinfo_data_t;

svn_error_t *
svn_fs_fs__serialize_mergeinfo(void **data,
                               apr_size_t *data_len,
                               void *in,
                               apr_pool_t *pool)
{
  svn_mergeinfo_t mergeinfo = in;
  mergeinfo_data_t merges;
  svn_temp_serializer__context_t *context;
  svn_stringbuf_t *serialized;
  apr_hash_index_t *hi;
  unsigned i;
  int k;
  apr_size_t range_count;

  /* initialize our auxiliary data structure */
  merges.count = apr_hash_count(mergeinfo);
  merges.keys = apr_palloc(pool, sizeof(*merges.keys) * merges.count);
  merges.key_lengths = apr_palloc(pool, sizeof(*merges.key_lengths) *
                                        merges.count);
  merges.range_counts = apr_palloc(pool, sizeof(*merges.range_counts) *
                                         merges.count);

  i = 0;
  range_count = 0;
  for (hi = apr_hash_first(pool, mergeinfo); hi; hi = apr_hash_next(hi), ++i)
    {
      svn_rangelist_t *ranges;
      apr_hash_this(hi, (const void**)&merges.keys[i],
                        &merges.key_lengths[i],
                        (void **)&ranges);
      merges.range_counts[i] = ranges->nelts;
      range_count += ranges->nelts;
    }

  merges.ranges = apr_palloc(pool, sizeof(*merges.ranges) * range_count);

  i = 0;
  for (hi = apr_hash_first(pool, mergeinfo); hi; hi = apr_hash_next(hi))
    {
      svn_rangelist_t *ranges = svn__apr_hash_index_val(hi);
      for (k = 0; k < ranges->nelts; ++k, ++i)
        merges.ranges[i] = *APR_ARRAY_IDX(ranges, k, svn_merge_range_t*);
    }

  /* serialize it and all its elements */
  context = svn_temp_serializer__init(&merges,
                                      sizeof(merges),
                                      range_count * 30,
                                      pool);

  /* keys array */
  svn_temp_serializer__push(context,
                            (const void * const *)&merges.keys,
                            merges.count * sizeof(*merges.keys));

  for (i = 0; i < merges.count; ++i)
    svn_temp_serializer__add_string(context, &merges.keys[i]);

  svn_temp_serializer__pop(context);

  /* key lengths array */
  svn_temp_serializer__push(context,
                            (const void * const *)&merges.key_lengths,
                            merges.count * sizeof(*merges.key_lengths));
  svn_temp_serializer__pop(context);

  /* range counts array */
  svn_temp_serializer__push(context,
                            (const void * const *)&merges.range_counts,
                            merges.count * sizeof(*merges.range_counts));
  svn_temp_serializer__pop(context);

  /* ranges */
  svn_temp_serializer__push(context,
                            (const void * const *)&merges.ranges,
                            range_count * sizeof(*merges.ranges));
  svn_temp_serializer__pop(context);

  /* return the serialized result */
  serialized = svn_temp_serializer__get(context);

  *data = serialized->data;
  *data_len = serialized->len;

  return SVN_NO_ERROR;
}

svn_error_t *
svn_fs_fs__deserialize_mergeinfo(void **out,
                                 void *data,
                                 apr_size_t data_len,
                                 apr_pool_t *pool)
{
  unsigned i;
  int k, n;
  mergeinfo_data_t *merges = (mergeinfo_data_t *)data;
  svn_mergeinfo_t mergeinfo;

  /* de-serialize our auxiliary data structure */
  svn_temp_deserializer__resolve(merges, (void**)&merges->keys);
  svn_temp_deserializer__resolve(merges, (void**)&merges->key_lengths);
  svn_temp_deserializer__resolve(merges, (void**)&merges->range_counts);
  svn_temp_deserializer__resolve(merges, (void**)&merges->ranges);

  /* de-serialize keys and add entries to the result */
  n = 0;
  mergeinfo = svn_hash__make(pool);
  for (i = 0; i < merges->count; ++i)
    {
      svn_rangelist_t *ranges = apr_array_make(pool,
                                               merges->range_counts[i],
                                               sizeof(svn_merge_range_t*));
      for (k = 0; k < merges->range_counts[i]; ++k, ++n)
        APR_ARRAY_PUSH(ranges, svn_merge_range_t*) = &merges->ranges[n];

      svn_temp_deserializer__resolve((void*)merges->keys,
                                     (void**)&merges->keys[i]);
      apr_hash_set(mergeinfo, merges->keys[i], merges->key_lengths[i], ranges);
    }

  /* done */
  *out = mergeinfo;

  return SVN_NO_ERROR;
}

