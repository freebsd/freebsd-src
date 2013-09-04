/* id.h : interface to node ID functions, private to libsvn_fs_fs
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

#ifndef SVN_LIBSVN_FS_FS_ID_H
#define SVN_LIBSVN_FS_FS_ID_H

#include "svn_fs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*** ID accessor functions. ***/

/* Get the "node id" portion of ID. */
const char *svn_fs_fs__id_node_id(const svn_fs_id_t *id);

/* Get the "copy id" portion of ID. */
const char *svn_fs_fs__id_copy_id(const svn_fs_id_t *id);

/* Get the "txn id" portion of ID, or NULL if it is a permanent ID. */
const char *svn_fs_fs__id_txn_id(const svn_fs_id_t *id);

/* Get the "rev" portion of ID, or SVN_INVALID_REVNUM if it is a
   transaction ID. */
svn_revnum_t svn_fs_fs__id_rev(const svn_fs_id_t *id);

/* Access the "offset" portion of the ID, or -1 if it is a transaction
   ID. */
apr_off_t svn_fs_fs__id_offset(const svn_fs_id_t *id);

/* Convert ID into string form, allocated in POOL. */
svn_string_t *svn_fs_fs__id_unparse(const svn_fs_id_t *id,
                                    apr_pool_t *pool);

/* Return true if A and B are equal. */
svn_boolean_t svn_fs_fs__id_eq(const svn_fs_id_t *a,
                               const svn_fs_id_t *b);

/* Return true if A and B are related. */
svn_boolean_t svn_fs_fs__id_check_related(const svn_fs_id_t *a,
                                          const svn_fs_id_t *b);

/* Return 0 if A and B are equal, 1 if they are related, -1 otherwise. */
int svn_fs_fs__id_compare(const svn_fs_id_t *a,
                          const svn_fs_id_t *b);

/* Create an ID within a transaction based on NODE_ID, COPY_ID, and
   TXN_ID, allocated in POOL. */
svn_fs_id_t *svn_fs_fs__id_txn_create(const char *node_id,
                                      const char *copy_id,
                                      const char *txn_id,
                                      apr_pool_t *pool);

/* Create a permanent ID based on NODE_ID, COPY_ID, REV, and OFFSET,
   allocated in POOL. */
svn_fs_id_t *svn_fs_fs__id_rev_create(const char *node_id,
                                      const char *copy_id,
                                      svn_revnum_t rev,
                                      apr_off_t offset,
                                      apr_pool_t *pool);

/* Return a copy of ID, allocated from POOL. */
svn_fs_id_t *svn_fs_fs__id_copy(const svn_fs_id_t *id,
                                apr_pool_t *pool);

/* Return an ID resulting from parsing the string DATA (with length
   LEN), or NULL if DATA is an invalid ID string. */
svn_fs_id_t *svn_fs_fs__id_parse(const char *data,
                                 apr_size_t len,
                                 apr_pool_t *pool);


/* (de-)serialization support*/

struct svn_temp_serializer__context_t;

/**
 * Serialize an @a id within the serialization @a context.
 */
void
svn_fs_fs__id_serialize(struct svn_temp_serializer__context_t *context,
                        const svn_fs_id_t * const *id);

/**
 * Deserialize an @a id within the @a buffer.
 */
void
svn_fs_fs__id_deserialize(void *buffer,
                          svn_fs_id_t **id);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SVN_LIBSVN_FS_FS_ID_H */
