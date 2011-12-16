/*      $FreeBSD$       */

/*-
 * Copyright (C) 2011 Gabor Kovesdan <gabor@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "glue.h"

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"


/*
 * Return a 32-bit hash of the given buffer.  The init
 * value should be 0, or the previous hash value to extend
 * the previous hash.
 */
static uint32_t
hash32_buf(const void *buf, size_t len, uint32_t hash)
{
  const unsigned char *p = buf;

  while (len--)
    hash = HASHSTEP(hash, *p++);

  return hash;
}

/*
 * Initializes a hash table that can hold table_size number of entries,
 * each of which has a key of key_size bytes and a value of value_size
 * bytes. On successful allocation returns a pointer to the hash table.
 * Otherwise, returns NULL and sets errno to indicate the error.
 */
hashtable
*hashtable_init(size_t table_size, size_t key_size, size_t value_size)
{
  hashtable *tbl;

  DPRINT(("hashtable_init: table_size %zu, key_size %zu, value_size %zu\n",
	  table_size, key_size, value_size));

  tbl = malloc(sizeof(hashtable));
  if (tbl == NULL)
    goto mem1;

  tbl->entries = calloc(sizeof(hashtable_entry *), table_size);
  if (tbl->entries == NULL)
    goto mem2;

  tbl->table_size = table_size;
  tbl->usage = 0;
  tbl->key_size = key_size;
  tbl->value_size = value_size;

  return (tbl);

mem2:
  free(tbl);
mem1:
  DPRINT(("hashtable_init: allocation failed\n"));
  errno = ENOMEM;
  return (NULL);
}

/*
 * Places the key-value pair to the hashtable tbl.
 * Returns:
 *   HASH_OK:		if the key was not present in the hash table yet
 *			but the kay-value pair has been successfully added.
 *   HASH_UPDATED:	if the value for the key has been updated with the
 *			new value.
 *   HASH_FULL:		if the hash table is full and the entry could not
 *			be added.
 *   HASH_FAIL:		if an error has occurred and errno has been set to
 *			indicate the error.
 */
int
hashtable_put(hashtable *tbl, const void *key, const void *value)
{
  uint32_t hash = 0;

  if (tbl->table_size == tbl->usage)
    {
      DPRINT(("hashtable_put: hashtable is full\n"));
      return (HASH_FULL);
    }

  hash = hash32_buf(key, tbl->key_size, hash) % tbl->table_size;
  DPRINT(("hashtable_put: calculated hash %" PRIu32 "\n", hash));

  /*
   * On hash collision entries are inserted at the next free space,
   * so we have to increase the index until we either find an entry
   * with the same key (and update it) or we find a free space.
   */
  for(;;)
    {
      if (tbl->entries[hash] == NULL)
	break;
      else if (memcmp(tbl->entries[hash]->key, key, tbl->key_size) == 0)
	{
	  memcpy(tbl->entries[hash]->value, value, tbl->value_size);
	  DPRINT(("hashtable_put: effective location is %" PRIu32
		  ", entry updated\n", hash));
	  return (HASH_UPDATED);
	}
      if (++hash == tbl->table_size)
	hash = 0;
    }

  DPRINT(("hashtable_put: effective location is %" PRIu32 "\n", hash));

  tbl->entries[hash] = malloc(sizeof(hashtable_entry));
  if (tbl->entries[hash] == NULL)
    {
      errno = ENOMEM;
      goto mem1;
    }

  tbl->entries[hash]->key = malloc(tbl->key_size);
  if (tbl->entries[hash]->key == NULL)
    {
      errno = ENOMEM;
      goto mem2;
    }

  tbl->entries[hash]->value = malloc(tbl->value_size);
  if (tbl->entries[hash]->value == NULL)
    {
      errno = ENOMEM;
      goto mem3;
    }

  memcpy(tbl->entries[hash]->key, key, tbl->key_size);
  memcpy(tbl->entries[hash]->value, value, tbl->value_size);
  tbl->usage++;

  DPRINT(("hashtable_put: entry successfully inserted\n"));

  return (HASH_OK);

mem3:
  free(tbl->entries[hash]->key);
mem2:
  free(tbl->entries[hash]);
mem1:
  DPRINT(("hashtable_put: insertion failed\n"));
  return (HASH_FAIL);
}

static hashtable_entry
**hashtable_lookup(const hashtable *tbl, const void *key)
{
  uint32_t hash = 0;

  hash = hash32_buf(key, tbl->key_size, hash) % tbl->table_size;

  for (;;)
    {
      if (tbl->entries[hash] == NULL)
	return (NULL);
      else if (memcmp(key, tbl->entries[hash]->key, tbl->key_size) == 0)
	{
	  DPRINT(("hashtable_lookup: entry found at location %" PRIu32 "\n", hash));
	  return (&tbl->entries[hash]);
	}

      if (++hash == tbl->table_size)
	hash = 0;
    }
}

/*
 * Retrieves the value for key from the hash table tbl and places
 * it to the space indicated by the value argument.
 * Returns HASH_OK if the value has been found and retrieved or
 * HASH_NOTFOUND otherwise.
 */
int
hashtable_get(hashtable *tbl, const void *key, void *value)
{
  hashtable_entry **entry;

  entry = hashtable_lookup(tbl, key);
  if (entry == NULL)
    {
      DPRINT(("hashtable_get: entry is not available in the hashtable\n"));
      return (HASH_NOTFOUND);
    }

  memcpy(value, (*entry)->value, tbl->value_size);
  DPRINT(("hashtable_get: entry successfully copied into output buffer\n"));
  return (HASH_OK);
}

/*
 * Removes the entry with the specifified key from the hash table
 * tbl. Returns HASH_OK if the entry has been found and removed
 * or HASH_NOTFOUND otherwise.
 */
int
hashtable_remove(hashtable *tbl, const void *key)
{
  hashtable_entry **entry;

  entry = hashtable_lookup(tbl, key);
  if (entry == NULL)
    {
      DPRINT(("hashtable_remove: entry is not available in the hashtable\n"));
      return (HASH_NOTFOUND);
    }

  free((*entry)->key);
  free((*entry)->value);
  free(*entry);
  *entry = NULL;

  tbl->usage--;
  DPRINT(("hashtable_remove: entry successfully removed\n"));
  return (HASH_OK);
}

/*
 * Frees the resources associated with the hash table tbl.
 */
void
hashtable_free(hashtable *tbl)
{
  if (tbl == NULL)
    return;

  for (unsigned int i = 0; i < tbl->table_size; i++)
    if ((tbl->entries[i] != NULL))
      {
	free(tbl->entries[i]->key);
	free(tbl->entries[i]->value);
      }

  free(tbl->entries);
  DPRINT(("hashtable_free: resources are successfully freed\n"));
}
