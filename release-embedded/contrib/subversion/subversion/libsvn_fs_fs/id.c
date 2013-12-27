/* id.c : operations on node-revision IDs
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
#include <stdlib.h>

#include "id.h"
#include "../libsvn_fs/fs-loader.h"
#include "private/svn_temp_serializer.h"
#include "private/svn_string_private.h"


typedef struct id_private_t {
  const char *node_id;
  const char *copy_id;
  const char *txn_id;
  svn_revnum_t rev;
  apr_off_t offset;
} id_private_t;


/* Accessing ID Pieces.  */

const char *
svn_fs_fs__id_node_id(const svn_fs_id_t *id)
{
  id_private_t *pvt = id->fsap_data;

  return pvt->node_id;
}


const char *
svn_fs_fs__id_copy_id(const svn_fs_id_t *id)
{
  id_private_t *pvt = id->fsap_data;

  return pvt->copy_id;
}


const char *
svn_fs_fs__id_txn_id(const svn_fs_id_t *id)
{
  id_private_t *pvt = id->fsap_data;

  return pvt->txn_id;
}


svn_revnum_t
svn_fs_fs__id_rev(const svn_fs_id_t *id)
{
  id_private_t *pvt = id->fsap_data;

  return pvt->rev;
}


apr_off_t
svn_fs_fs__id_offset(const svn_fs_id_t *id)
{
  id_private_t *pvt = id->fsap_data;

  return pvt->offset;
}


svn_string_t *
svn_fs_fs__id_unparse(const svn_fs_id_t *id,
                      apr_pool_t *pool)
{
  id_private_t *pvt = id->fsap_data;

  if ((! pvt->txn_id))
    {
      char rev_string[SVN_INT64_BUFFER_SIZE];
      char offset_string[SVN_INT64_BUFFER_SIZE];

      svn__i64toa(rev_string, pvt->rev);
      svn__i64toa(offset_string, pvt->offset);
      return svn_string_createf(pool, "%s.%s.r%s/%s",
                                pvt->node_id, pvt->copy_id,
                                rev_string, offset_string);
    }
  else
    {
      return svn_string_createf(pool, "%s.%s.t%s",
                                pvt->node_id, pvt->copy_id,
                                pvt->txn_id);
    }
}


/*** Comparing node IDs ***/

svn_boolean_t
svn_fs_fs__id_eq(const svn_fs_id_t *a,
                 const svn_fs_id_t *b)
{
  id_private_t *pvta = a->fsap_data, *pvtb = b->fsap_data;

  if (a == b)
    return TRUE;
  if (strcmp(pvta->node_id, pvtb->node_id) != 0)
     return FALSE;
  if (strcmp(pvta->copy_id, pvtb->copy_id) != 0)
    return FALSE;
  if ((pvta->txn_id == NULL) != (pvtb->txn_id == NULL))
    return FALSE;
  if (pvta->txn_id && pvtb->txn_id && strcmp(pvta->txn_id, pvtb->txn_id) != 0)
    return FALSE;
  if (pvta->rev != pvtb->rev)
    return FALSE;
  if (pvta->offset != pvtb->offset)
    return FALSE;
  return TRUE;
}


svn_boolean_t
svn_fs_fs__id_check_related(const svn_fs_id_t *a,
                            const svn_fs_id_t *b)
{
  id_private_t *pvta = a->fsap_data, *pvtb = b->fsap_data;

  if (a == b)
    return TRUE;
  /* If both node_ids start with _ and they have differing transaction
     IDs, then it is impossible for them to be related. */
  if (pvta->node_id[0] == '_')
    {
      if (pvta->txn_id && pvtb->txn_id &&
          (strcmp(pvta->txn_id, pvtb->txn_id) != 0))
        return FALSE;
    }

  return (strcmp(pvta->node_id, pvtb->node_id) == 0);
}


int
svn_fs_fs__id_compare(const svn_fs_id_t *a,
                      const svn_fs_id_t *b)
{
  if (svn_fs_fs__id_eq(a, b))
    return 0;
  return (svn_fs_fs__id_check_related(a, b) ? 1 : -1);
}



/* Creating ID's.  */

static id_vtable_t id_vtable = {
  svn_fs_fs__id_unparse,
  svn_fs_fs__id_compare
};


svn_fs_id_t *
svn_fs_fs__id_txn_create(const char *node_id,
                         const char *copy_id,
                         const char *txn_id,
                         apr_pool_t *pool)
{
  svn_fs_id_t *id = apr_palloc(pool, sizeof(*id));
  id_private_t *pvt = apr_palloc(pool, sizeof(*pvt));

  pvt->node_id = apr_pstrdup(pool, node_id);
  pvt->copy_id = apr_pstrdup(pool, copy_id);
  pvt->txn_id = apr_pstrdup(pool, txn_id);
  pvt->rev = SVN_INVALID_REVNUM;
  pvt->offset = -1;

  id->vtable = &id_vtable;
  id->fsap_data = pvt;
  return id;
}


svn_fs_id_t *
svn_fs_fs__id_rev_create(const char *node_id,
                         const char *copy_id,
                         svn_revnum_t rev,
                         apr_off_t offset,
                         apr_pool_t *pool)
{
  svn_fs_id_t *id = apr_palloc(pool, sizeof(*id));
  id_private_t *pvt = apr_palloc(pool, sizeof(*pvt));

  pvt->node_id = apr_pstrdup(pool, node_id);
  pvt->copy_id = apr_pstrdup(pool, copy_id);
  pvt->txn_id = NULL;
  pvt->rev = rev;
  pvt->offset = offset;

  id->vtable = &id_vtable;
  id->fsap_data = pvt;
  return id;
}


svn_fs_id_t *
svn_fs_fs__id_copy(const svn_fs_id_t *id, apr_pool_t *pool)
{
  svn_fs_id_t *new_id = apr_palloc(pool, sizeof(*new_id));
  id_private_t *new_pvt = apr_palloc(pool, sizeof(*new_pvt));
  id_private_t *pvt = id->fsap_data;

  new_pvt->node_id = apr_pstrdup(pool, pvt->node_id);
  new_pvt->copy_id = apr_pstrdup(pool, pvt->copy_id);
  new_pvt->txn_id = pvt->txn_id ? apr_pstrdup(pool, pvt->txn_id) : NULL;
  new_pvt->rev = pvt->rev;
  new_pvt->offset = pvt->offset;

  new_id->vtable = &id_vtable;
  new_id->fsap_data = new_pvt;
  return new_id;
}


svn_fs_id_t *
svn_fs_fs__id_parse(const char *data,
                    apr_size_t len,
                    apr_pool_t *pool)
{
  svn_fs_id_t *id;
  id_private_t *pvt;
  char *data_copy, *str;

  /* Dup the ID data into POOL.  Our returned ID will have references
     into this memory. */
  data_copy = apr_pstrmemdup(pool, data, len);

  /* Alloc a new svn_fs_id_t structure. */
  id = apr_palloc(pool, sizeof(*id));
  pvt = apr_palloc(pool, sizeof(*pvt));
  id->vtable = &id_vtable;
  id->fsap_data = pvt;

  /* Now, we basically just need to "split" this data on `.'
     characters.  We will use svn_cstring_tokenize, which will put
     terminators where each of the '.'s used to be.  Then our new
     id field will reference string locations inside our duplicate
     string.*/

  /* Node Id */
  str = svn_cstring_tokenize(".", &data_copy);
  if (str == NULL)
    return NULL;
  pvt->node_id = str;

  /* Copy Id */
  str = svn_cstring_tokenize(".", &data_copy);
  if (str == NULL)
    return NULL;
  pvt->copy_id = str;

  /* Txn/Rev Id */
  str = svn_cstring_tokenize(".", &data_copy);
  if (str == NULL)
    return NULL;

  if (str[0] == 'r')
    {
      apr_int64_t val;
      svn_error_t *err;

      /* This is a revision type ID */
      pvt->txn_id = NULL;

      data_copy = str + 1;
      str = svn_cstring_tokenize("/", &data_copy);
      if (str == NULL)
        return NULL;
      pvt->rev = SVN_STR_TO_REV(str);

      str = svn_cstring_tokenize("/", &data_copy);
      if (str == NULL)
        return NULL;
      err = svn_cstring_atoi64(&val, str);
      if (err)
        {
          svn_error_clear(err);
          return NULL;
        }
      pvt->offset = (apr_off_t)val;
    }
  else if (str[0] == 't')
    {
      /* This is a transaction type ID */
      pvt->txn_id = str + 1;
      pvt->rev = SVN_INVALID_REVNUM;
      pvt->offset = -1;
    }
  else
    return NULL;

  return id;
}

/* (de-)serialization support */

/* Serialization of the PVT sub-structure within the CONTEXT.
 */
static void
serialize_id_private(svn_temp_serializer__context_t *context,
                     const id_private_t * const *pvt)
{
  const id_private_t *private = *pvt;

  /* serialize the pvt data struct itself */
  svn_temp_serializer__push(context,
                            (const void * const *)pvt,
                            sizeof(*private));

  /* append the referenced strings */
  svn_temp_serializer__add_string(context, &private->node_id);
  svn_temp_serializer__add_string(context, &private->copy_id);
  svn_temp_serializer__add_string(context, &private->txn_id);

  /* return to caller's nesting level */
  svn_temp_serializer__pop(context);
}

/* Serialize an ID within the serialization CONTEXT.
 */
void
svn_fs_fs__id_serialize(svn_temp_serializer__context_t *context,
                        const struct svn_fs_id_t * const *id)
{
  /* nothing to do for NULL ids */
  if (*id == NULL)
    return;

  /* serialize the id data struct itself */
  svn_temp_serializer__push(context,
                            (const void * const *)id,
                            sizeof(**id));

  /* serialize the id_private_t data sub-struct */
  serialize_id_private(context,
                       (const id_private_t * const *)&(*id)->fsap_data);

  /* return to caller's nesting level */
  svn_temp_serializer__pop(context);
}

/* Deserialization of the PVT sub-structure in BUFFER.
 */
static void
deserialize_id_private(void *buffer, id_private_t **pvt)
{
  /* fixup the reference to the only sub-structure */
  id_private_t *private;
  svn_temp_deserializer__resolve(buffer, (void**)pvt);

  /* fixup the sub-structure itself */
  private = *pvt;
  svn_temp_deserializer__resolve(private, (void**)&private->node_id);
  svn_temp_deserializer__resolve(private, (void**)&private->copy_id);
  svn_temp_deserializer__resolve(private, (void**)&private->txn_id);
}

/* Deserialize an ID inside the BUFFER.
 */
void
svn_fs_fs__id_deserialize(void *buffer, svn_fs_id_t **id)
{
  /* The id maybe all what is in the whole buffer.
   * Don't try to fixup the pointer in that case*/
  if (*id != buffer)
    svn_temp_deserializer__resolve(buffer, (void**)id);

  /* no id, no sub-structure fixup necessary */
  if (*id == NULL)
    return;

  /* the stored vtable is bogus at best -> set the right one */
  (*id)->vtable = &id_vtable;

  /* handle sub-structures */
  deserialize_id_private(*id, (id_private_t **)&(*id)->fsap_data);
}

