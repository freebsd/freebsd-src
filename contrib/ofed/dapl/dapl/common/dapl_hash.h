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
 * HEADER: dapl_hash.h
 *
 * PURPOSE: Utility defs & routines for the hash data structure
 *
 * $Id:$
 **********************************************************************/

#ifndef _DAPL_HASH_H_
#define _DAPL_HASH_H_

#include "dapl.h"


/*********************************************************************
 *                                                                   *
 * Defines                                                           *
 *                                                                   *
 *********************************************************************/

/*
 * Hash table size.
 *
 * Default is small; use the larger sample values for hash tables
 * known to be heavily used.  The sample values chosen are the
 * largest primes below 2^8, 2^9, and 2^10.
 */
#define DAPL_DEF_HASHSIZE               251
#define DAPL_MED_HASHSIZE               509
#define DAPL_LRG_HASHSIZE               1021

#define DAPL_HASH_TABLE_DEFAULT_CAPACITY DAPL_DEF_HASHSIZE

/* The hash function */
#if defined(__KDAPL__)
#define DAPL_DOHASH(key,hashsize) dapl_os_mod64(key,hashsize)
#else
#define DAPL_DOHASH(key,hashsize)   ((uint64_t)((key) % (hashsize)))
#endif	/* defined(__KDAPL__) */


/*********************************************************************
 *                                                                   *
 * Function Prototypes                                               *
 *                                                                   *
 *********************************************************************/

extern DAT_RETURN
dapls_hash_create(
    IN DAT_COUNT capacity,
    OUT DAPL_HASH_TABLE **pp_table);

extern DAT_RETURN
dapls_hash_free(
    IN DAPL_HASH_TABLE *p_table);

extern DAT_RETURN
dapls_hash_size(
    IN DAPL_HASH_TABLE *p_table,
    OUT DAT_COUNT *p_size);

extern DAT_RETURN
dapls_hash_insert(
    IN DAPL_HASH_TABLE *p_table, 
    IN DAPL_HASH_KEY key,
    IN DAPL_HASH_DATA data);

extern DAT_RETURN
dapls_hash_search(
    IN DAPL_HASH_TABLE *p_table,
    IN DAPL_HASH_KEY key,
    OUT DAPL_HASH_DATA *p_data);

extern DAT_RETURN
dapls_hash_remove(
    IN DAPL_HASH_TABLE *p_table,
    IN DAPL_HASH_KEY key,
    OUT DAPL_HASH_DATA *p_data);


#endif /* _DAPL_HASH_H_ */
