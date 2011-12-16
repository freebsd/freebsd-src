/*
 * Copyright (c) 2004, 2005 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/****h* OpenSM/DB-Pack
* NAME
*	Database Types
*
* DESCRIPTION
*	This module provides packing and unpacking of the database
*  storage into specific types.
*
*  The following domains/conversions are supported:
*  guid2lid - key is a guid and data is a lid.
*
* AUTHOR
*	Eitan Zahavi, Mellanox Technologies LTD
*
*********/

#ifndef _OSM_DB_PACK_H_
#define _OSM_DB_PACK_H_

#include <opensm/osm_db.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****f* OpenSM: DB-Pack/osm_db_guid2lid_init
* NAME
*	osm_db_guid2lid_init
*
* DESCRIPTION
*	Initialize a domain for the guid2lid table
*
* SYNOPSIS
*/
static inline osm_db_domain_t *osm_db_guid2lid_init(IN osm_db_t * const p_db)
{
	return (osm_db_domain_init(p_db, "guid2lid"));
}

/*
* PARAMETERS
*	p_db
*		[in] Pointer to the database object to construct
*
* RETURN VALUES
*	The pointer to the new allocated domain object or NULL.
*
* NOTE: DB domains are destroyed by the osm_db_destroy
*
* SEE ALSO
*	Database, osm_db_init, osm_db_destroy
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2lid_init
* NAME
*	osm_db_guid2lid_init
*
* DESCRIPTION
*	Initialize a domain for the guid2lid table
*
* SYNOPSIS
*/
typedef struct osm_db_guid_elem {
	cl_list_item_t item;
	uint64_t guid;
} osm_db_guid_elem_t;
/*
* FIELDS
*	item
*		required for list manipulations
*
*  guid
*
************/

/****f* OpenSM: DB-Pack/osm_db_guid2lid_guids
* NAME
*	osm_db_guid2lid_guids
*
* DESCRIPTION
*	Provides back a list of guid elements.
*
* SYNOPSIS
*/
int
osm_db_guid2lid_guids(IN osm_db_domain_t * const p_g2l,
		      OUT cl_qlist_t * p_guid_list);
/*
* PARAMETERS
*	p_g2l
*		[in] Pointer to the guid2lid domain
*
*  p_guid_list
*     [out] A quick list of guid elements of type osm_db_guid_elem_t
*
* RETURN VALUES
*	0 if successful
*
* NOTE: the output qlist should be initialized and each item freed
*       by the caller, then destroyed.
*
* SEE ALSO
* osm_db_guid2lid_init, osm_db_guid2lid_guids, osm_db_guid2lid_get
* osm_db_guid2lid_set, osm_db_guid2lid_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2lid_get
* NAME
*	osm_db_guid2lid_get
*
* DESCRIPTION
*	Get a lid range by given guid.
*
* SYNOPSIS
*/
int
osm_db_guid2lid_get(IN osm_db_domain_t * const p_g2l,
		    IN uint64_t guid,
		    OUT uint16_t * p_min_lid, OUT uint16_t * p_max_lid);
/*
* PARAMETERS
*	p_g2l
*		[in] Pointer to the guid2lid domain
*
*  guid
*     [in] The guid to look for
*
*  p_min_lid
*     [out] Pointer to the resulting min lid in host order.
*
*  p_max_lid
*     [out] Pointer to the resulting max lid in host order.
*
* RETURN VALUES
*	0 if successful. The lid will be set to 0 if not found.
*
* SEE ALSO
* osm_db_guid2lid_init, osm_db_guid2lid_guids
* osm_db_guid2lid_set, osm_db_guid2lid_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2lid_set
* NAME
*	osm_db_guid2lid_set
*
* DESCRIPTION
*	Set a lid range for the given guid.
*
* SYNOPSIS
*/
int
osm_db_guid2lid_set(IN osm_db_domain_t * const p_g2l,
		    IN uint64_t guid, IN uint16_t min_lid, IN uint16_t max_lid);
/*
* PARAMETERS
*	p_g2l
*		[in] Pointer to the guid2lid domain
*
*  guid
*     [in] The guid to look for
*
*  min_lid
*     [in] The min lid value to set
*
*  max_lid
*     [in] The max lid value to set
*
* RETURN VALUES
*	0 if successful
*
* SEE ALSO
* osm_db_guid2lid_init, osm_db_guid2lid_guids
* osm_db_guid2lid_get, osm_db_guid2lid_delete
*********/

/****f* OpenSM: DB-Pack/osm_db_guid2lid_delete
* NAME
*	osm_db_guid2lid_delete
*
* DESCRIPTION
*	Delete the entry by the given guid
*
* SYNOPSIS
*/
int osm_db_guid2lid_delete(IN osm_db_domain_t * const p_g2l, IN uint64_t guid);
/*
* PARAMETERS
*	p_g2l
*		[in] Pointer to the guid2lid domain
*
*  guid
*     [in] The guid to look for
*
* RETURN VALUES
*	0 if successful otherwise 1
*
* SEE ALSO
* osm_db_guid2lid_init, osm_db_guid2lid_guids
* osm_db_guid2lid_get, osm_db_guid2lid_set
*********/

END_C_DECLS
#endif				/* _OSM_DB_PACK_H_ */
