/*
 * Copyright (c) 2002-2003, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

/**********************************************************************
 * 
 * MODULE: dapl_hash.c
 *
 * PURPOSE: Hash Table
 * Description: 
 *
 * Provides a generic hash table with chaining.
 *
 * $Id:$
 **********************************************************************/

#include "dapl_hash.h"

/*********************************************************************
 *                                                                   *
 * Structures                                                        *
 *                                                                   *
 *********************************************************************/

/*
 * A hash table element
 */
typedef struct DAPL_HASH_ELEM {
	struct DAPL_HASH_ELEM *next_element;
	DAPL_HASH_KEY key;
	void *datum;
} DAPL_HASH_ELEM;

/*
 * The hash table
 */
struct dapl_hash_table {
	unsigned long num_entries;
	unsigned long tbl_size;
	DAPL_HASH_ELEM *table;
	DAPL_OS_LOCK lock;
	/*
	 * statistics - we tally on insert operations, counting
	 * the number of entries in the whole hash table, as
	 * well as the length of chains we walk to insert.  This
	 * ignores empty buckets, giving us data on overall table
	 * occupancy, as well as max/average chain length for
	 * the buckets used.  If our hash function results in
	 * hot buckets, this will show it.
	 */
	uint64_t hash_tbl_inserts;	/* total inserts ops    */
	uint64_t hash_tbl_max;	/* max in entire table  */
	uint64_t hash_tbl_total;	/* total in table       */
	uint64_t hash_chn_max;	/* longest chain        */
	uint64_t hash_chn_total;	/* total non-0 lenghts  */
};

/*********************************************************************
 *                                                                   *
 * Defines                                                           *
 *                                                                   *
 *********************************************************************/

/* datum value in empty table slots  (use 0UL or ~0UL as appropriate) */
#define NO_DATUM_VALUE          ((void *) 0UL)
#define NO_DATUM(value)         ((value) == NO_DATUM_VALUE)

/* Lookup macro (which falls back to function to rehash) */
#define DAPL_HASHLOOKUP( p_table, in_key, out_datum, bucket_head) \
    do { \
        DAPL_HASH_KEY save_key = in_key; \
        DAPL_HASH_ELEM *element = \
            &((p_table)->table)[DAPL_DOHASH(in_key,(p_table)->tbl_size)]; \
        in_key = save_key; \
        if (NO_DATUM(element->datum)) { \
            (bucket_head) = (void *)0; \
        } else if (element->key == (DAPL_HASH_KEY) (in_key)) { \
            (out_datum) = element->datum; \
            (bucket_head) = (void *)element; \
        } else if (element->next_element) { \
            dapli_hash_rehash(element, \
                            (in_key), \
                            (void **)&(out_datum), \
                            (DAPL_HASH_ELEM **)&(bucket_head)); \
        } else { \
            (bucket_head) = (void *)0; \
        }\
    } while (0)

/*********************************************************************
 *                                                                   *
 * Internal Functions                                                *
 *                                                                   *
 *********************************************************************/

/*
 * Rehash the key (used by add and lookup functions)
 * 
 * Inputs:  element	element to rehash key
 *	    key, datum	datum for key head
 *	    head	head for list
 */
static void
dapli_hash_rehash(DAPL_HASH_ELEM * element,
		  DAPL_HASH_KEY key, void **datum, DAPL_HASH_ELEM ** head)
{
	/*
	 * assume we looked at the contents of element already,
	 * and start with the next element.
	 */
	dapl_os_assert(element->next_element);
	dapl_os_assert(!NO_DATUM(element->datum));

	*head = element;
	while (1) {
		element = element->next_element;
		if (!element) {
			break;
		}
		if (element->key == key) {
			*datum = element->datum;
			return;
		}
	}
	*head = 0;
}

/*
 * Add a new key to the hash table
 * 
 * Inputs:
 *          table, key and datum to be added
 *          allow_dup   - DAT_TRUE if dups are allowed
 * Outputs: 
 *          report_dup  - should you care to know
 * Returns:
 *          DAT_TRUE on success     
 */
static DAT_BOOLEAN
dapli_hash_add(DAPL_HASH_TABLEP p_table,
	       DAPL_HASH_KEY key,
	       void *datum, DAT_BOOLEAN allow_dup, DAT_BOOLEAN * report_dup)
{
	void *olddatum;
	DAPL_HASH_KEY hashValue, save_key = key;
	DAPL_HASH_ELEM *found;
	DAT_BOOLEAN status = DAT_FALSE;
	unsigned int chain_len = 0;

	if (report_dup) {
		(*report_dup) = DAT_FALSE;
	}

	if (NO_DATUM(datum)) {
		/*
		 * Reserved value used for datum
		 */
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "dapli_hash_add () called with magic NO_DATA value (%p) "
			     "used as datum!\n", datum);
		return DAT_FALSE;
	}

	DAPL_HASHLOOKUP(p_table, key, olddatum, found);
	if (found) {
		/*
		 * key exists already
		 */
		if (report_dup) {
			*report_dup = DAT_TRUE;
		}

		if (!allow_dup) {
			dapl_dbg_log(DAPL_DBG_TYPE_ERR,
				     "dapli_hash_add () called with duplicate key ("
				     F64x ")\n", key);
			return DAT_FALSE;
		}
	}

	hashValue = DAPL_DOHASH(key, p_table->tbl_size);
	key = save_key;
	if (NO_DATUM(p_table->table[hashValue].datum)) {
		/*
		 * Empty head - just fill it in
		 */
		p_table->table[hashValue].key = key;
		p_table->table[hashValue].datum = datum;
		p_table->table[hashValue].next_element = 0;
		p_table->num_entries++;
		status = DAT_TRUE;
	} else {
		DAPL_HASH_ELEM *newelement = (DAPL_HASH_ELEM *)
		    dapl_os_alloc(sizeof(DAPL_HASH_ELEM));
		/*
		 * Add an element to the end of the chain
		 */
		if (newelement) {
			DAPL_HASH_ELEM *lastelement;
			newelement->key = key;
			newelement->datum = datum;
			newelement->next_element = 0;
			for (lastelement = &p_table->table[hashValue];
			     lastelement->next_element;
			     lastelement = lastelement->next_element) {
				/* Walk to the end of the chain */
				chain_len++;
			}
			lastelement->next_element = newelement;
			p_table->num_entries++;
			status = DAT_TRUE;
		} else {
			/* allocation failed - should not happen */
			status = DAT_FALSE;
		}
	}

	/*
	 * Tally up our counters. chain_len is one less than current chain
	 * length.
	 */
	chain_len++;
	p_table->hash_tbl_inserts++;
	p_table->hash_tbl_total += p_table->num_entries;
	p_table->hash_chn_total += chain_len;
	if (p_table->num_entries > p_table->hash_tbl_max) {
		p_table->hash_tbl_max = p_table->num_entries;
	}
	if (chain_len > p_table->hash_chn_max) {
		p_table->hash_chn_max = chain_len;
	}

	return status;
}

/*
 * Remove element from hash bucket
 * 
 * Inputs:
 *          element, key        to be deleted
 * Returns:
 *          DAT_TRUE on success
 */
static DAT_BOOLEAN
dapl_hash_delete_element(DAPL_HASH_ELEM * element,
			 DAPL_HASH_KEY key, DAPL_HASH_DATA * p_datum)
{
	DAPL_HASH_ELEM *curelement;
	DAPL_HASH_ELEM *lastelement;

	lastelement = NULL;
	for (curelement = element;
	     curelement;
	     lastelement = curelement, curelement = curelement->next_element) {
		if (curelement->key == key) {
			if (p_datum) {
				*p_datum = curelement->datum;
			}
			if (lastelement) {
				/*
				 * curelement was malloc'd; free it
				 */
				lastelement->next_element =
				    curelement->next_element;
				dapl_os_free((void *)curelement,
					     sizeof(DAPL_HASH_ELEM));
			} else {
				/*
				 * curelement is static list head
				 */
				DAPL_HASH_ELEM *n = curelement->next_element;
				if (n) {
					/*
					 * If there is a next element, copy its contents into the
					 * head and free the original next element.
					 */
					curelement->key = n->key;
					curelement->datum = n->datum;
					curelement->next_element =
					    n->next_element;
					dapl_os_free((void *)n,
						     sizeof(DAPL_HASH_ELEM));
				} else {
					curelement->datum = NO_DATUM_VALUE;
				}
			}
			break;
		}
	}

	return (curelement != NULL ? DAT_TRUE : DAT_FALSE);
}

/*********************************************************************
 *                                                                   *
 * External Functions                                                *
 *                                                                   *
 *********************************************************************/

/*
 * Create a new hash table with at least 'table_size' hash buckets.
 */
DAT_RETURN
dapls_hash_create(IN DAT_COUNT table_size, OUT DAPL_HASH_TABLE ** pp_table)
{
	DAPL_HASH_TABLE *p_table;
	DAT_COUNT table_length = table_size * sizeof(DAPL_HASH_ELEM);
	DAT_RETURN dat_status;
	DAT_COUNT i;

	dapl_os_assert(pp_table);
	dat_status = DAT_SUCCESS;

	/* Allocate hash table */
	p_table = dapl_os_alloc(sizeof(DAPL_HASH_TABLE));
	if (NULL == p_table) {
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	/* Init hash table, allocate and init and buckets */
	dapl_os_memzero(p_table, sizeof(DAPL_HASH_TABLE));
	p_table->tbl_size = table_size;
	p_table->table = (DAPL_HASH_ELEM *) dapl_os_alloc(table_length);
	if (NULL == p_table->table) {
		dapl_os_free(p_table, sizeof(DAPL_HASH_TABLE));
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	dapl_os_lock_init(&p_table->lock);
	for (i = 0; i < table_size; i++) {
		p_table->table[i].datum = NO_DATUM_VALUE;
		p_table->table[i].key = 0;
		p_table->table[i].next_element = 0;
	}

	*pp_table = p_table;

      bail:
	return DAT_SUCCESS;
}

/*
 * Destroy a hash table
 */
DAT_RETURN dapls_hash_free(IN DAPL_HASH_TABLE * p_table)
{
	dapl_os_assert(p_table && p_table->table);

	dapl_os_lock_destroy(&p_table->lock);
	dapl_os_free(p_table->table,
		     sizeof(DAPL_HASH_ELEM) * p_table->tbl_size);
	dapl_os_free(p_table, sizeof(DAPL_HASH_TABLE));

	return DAT_SUCCESS;
}

/*
 * Returns the number of elements stored in the table
 */

DAT_RETURN dapls_hash_size(IN DAPL_HASH_TABLE * p_table, OUT DAT_COUNT * p_size)
{
	dapl_os_assert(p_table && p_size);

	*p_size = p_table->num_entries;

	return DAT_SUCCESS;
}

/*
 * Inserts the specified data into the table with the given key.
 * Duplicates are not expected, and return in error, having done nothing.
 */

DAT_RETURN
dapls_hash_insert(IN DAPL_HASH_TABLE * p_table,
		  IN DAPL_HASH_KEY key, IN DAPL_HASH_DATA data)
{
	DAT_RETURN dat_status;

	dapl_os_assert(p_table);
	dat_status = DAT_SUCCESS;

	dapl_os_lock(&p_table->lock);
	if (!dapli_hash_add(p_table, key, data, DAT_FALSE, NULL)) {
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
	}
	dapl_os_unlock(&p_table->lock);

	return dat_status;
}

/*
 * Searches for the given key.  If found, 
 * DAT_SUCCESS is returned and the associated 
 * data is returned in the DAPL_HASH_DATA 
 * pointer if that pointer is not NULL.
 */
DAT_RETURN
dapls_hash_search(IN DAPL_HASH_TABLE * p_table,
		  IN DAPL_HASH_KEY key, OUT DAPL_HASH_DATA * p_data)
{
	DAT_RETURN dat_status;
	void *olddatum;
	DAPL_HASH_ELEM *found;

	dapl_os_assert(p_table);
	dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, 0);

	dapl_os_lock(&p_table->lock);
	DAPL_HASHLOOKUP(p_table, key, olddatum, found);
	dapl_os_unlock(&p_table->lock);

	if (found) {
		if (p_data) {
			*p_data = olddatum;
		}
		dat_status = DAT_SUCCESS;
	}

	return dat_status;
}

DAT_RETURN
dapls_hash_remove(IN DAPL_HASH_TABLE * p_table,
		  IN DAPL_HASH_KEY key, OUT DAPL_HASH_DATA * p_data)
{
	DAT_RETURN dat_status;
	DAPL_HASH_KEY hashValue, save_key = key;

	dapl_os_assert(p_table);
	dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, 0);

	if (p_table->num_entries == 0) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "dapls_hash_remove () called on empty hash table!\n");
		return dat_status;
	}

	hashValue = DAPL_DOHASH(key, p_table->tbl_size);
	key = save_key;
	dapl_os_lock(&p_table->lock);
	if (dapl_hash_delete_element(&p_table->table[hashValue], key, p_data)) {
		p_table->num_entries--;
		dat_status = DAT_SUCCESS;
	}
	dapl_os_unlock(&p_table->lock);

	return dat_status;
}
