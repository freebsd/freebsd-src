/*
 * Copyright (c) 2006 Voltaire, Inc. All rights reserved.
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

/*
 * Abstract:
 * 	Declaration of osmtest_t.
 *	This object represents the OSMTest Test object.
 *
 */

#ifndef _OSMTEST_SUBNET_H_
#define _OSMTEST_SUBNET_H_

#include <stdlib.h>
#include <complib/cl_qmap.h>
#include <opensm/osm_log.h>
#include <vendor/osm_vendor_api.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_helper.h>

/****s* Subnet Database/generic_t
* NAME
*	generic_t
*
* DESCRIPTION
*	Subnet database object for fields common to all record types.
*	All other database types must be castable to this type.
*
* SYNOPSIS
*/
typedef struct _generic {
	cl_map_item_t map_item;	/* must be first element! */
	uint32_t count;		/* must be second element! */
} generic_t;

/*
* FIELDS
*
* SEE ALSO
*********/

/****s* Subnet Database/node_t
* NAME
*	node_t
*
* DESCRIPTION
*	Subnet database object for nodes.
*	Must be castable to generic_t.
*
* SYNOPSIS
*/
typedef struct _node {
	cl_map_item_t map_item;	/* must be first element! */
	uint32_t count;		/* must be second element! */
	ib_node_record_t rec;
	ib_node_record_t comp;
} node_t;

/*
* FIELDS
* map_item
*	Provides linkage for the qmap container.
*
* rec
*	NodeRecord for this node as read from the database file.
*
* comp
*	NodeRecord indicating which fields should be compared against rec.
*	Bits set in the comp NodeRecord indicate that bit in the rec structure
*	should be compared against real-time data from the SA.
*
* count
*	Utility counter used by the validation logic.  Typically used to
*	to indicate the number of times a matching node was received from
*	the SA.
*
* SEE ALSO
*********/

static inline node_t *node_new(void)
{
	node_t *p_obj;

	p_obj = malloc(sizeof(*p_obj));
	if (p_obj)
		memset(p_obj, 0, sizeof(*p_obj));
	return (p_obj);
}

static inline void node_delete(IN node_t * p_obj)
{
	free(p_obj);
}

/****s* Subnet Database/port_t
* NAME
*	port_t
*
* DESCRIPTION
*	Subnet database object for ports.
*	Must be castable to generic_t.
*
* SYNOPSIS
*/
typedef struct _port {
	cl_map_item_t map_item;	/* must be first element! */
	uint32_t count;		/* must be second element! */
	/* Since there is no unique identifier for all ports we
	   must be able to have such a key by the lid and port num */
	uint64_t port_id;
	ib_portinfo_record_t rec;
	ib_portinfo_record_t comp;
} port_t;

/*
* FIELDS
*
* map_item
*	Provides linkage for the qmap container.
*
* rec
*	PortInfoRecord for this port as read from the database file.
*
* comp
*	PortInfoRecord indicating which fields should be compared against rec.
*	Bits set in the comp NodeRecord indicate that bit in the rec structure
*	should be compared against real-time data from the SA.
*
* count
*	Utility counter used by the validation logic.  Typically used to
*	to indicate the number of times a matching node was received from
*	the SA.
*
* SEE ALSO
*********/

static inline port_t *port_new(void)
{
	port_t *p_obj;

	p_obj = malloc(sizeof(*p_obj));
	if (p_obj)
		memset(p_obj, 0, sizeof(*p_obj));
	return (p_obj);
}

static inline void port_delete(IN port_t * p_obj)
{
	free(p_obj);
}

static inline uint64_t
port_gen_id(IN ib_net16_t const lid, IN uint8_t const port_num)
{
	return (lid << 8 | port_num);
}

static inline void
port_ext_id(IN uint64_t id, IN ib_net16_t * p_lid, IN uint8_t * p_port_num)
{
	CL_ASSERT((id & 0xFF) < 0x100);
	*p_port_num = (uint8_t) (id & 0xFF);
	CL_ASSERT(((id >> 8) & 0xFFFF) < 0x10000);
	*p_lid = (uint16_t) ((id >> 8) & 0xFFFF);
}

static inline void
port_set_id(IN port_t * p_obj,
	    IN ib_net16_t const lid, IN uint8_t const port_num)
{
	p_obj->port_id = port_gen_id(lid, port_num);
}

static inline void
port_get_id(IN port_t * p_obj, IN ib_net16_t * p_lid, IN uint8_t * p_port_num)
{
	port_ext_id(p_obj->port_id, p_lid, p_port_num);
}

/****s* Subnet Database/path_t
* NAME
*	node_t
*
* DESCRIPTION
*	Subnet database object for paths.
*	Must be castable to generic_t.
*
* SYNOPSIS
*/
typedef struct _path {
	cl_map_item_t map_item;	/* must be first element! */
	uint32_t count;		/* must be second element! */
	ib_path_rec_t rec;
	ib_path_rec_t comp;
} path_t;

/*
* FIELDS
* map_item
*	Provides linkage for the qmap container.
*
* rec
*	PathRecord for this path as read from the database file.
*
* comp
*	PathRecord indicating which fields should be compared against rec.
*	Bits set in the comp PathRecord indicate that bit in the rec structure
*	should be compared against real-time data from the SA.
*
* count
*	Utility counter used by the validation logic.  Typically used to
*	to indicate the number of times a matching node was received from
*	the SA.
*
* SEE ALSO
*********/

static inline path_t *path_new(void)
{
	path_t *p_obj;

	p_obj = malloc(sizeof(*p_obj));
	if (p_obj)
		memset(p_obj, 0, sizeof(*p_obj));
	return (p_obj);
}

static inline void path_delete(IN path_t * p_obj)
{
	free(p_obj);
}

/****s* Subnet Database/subnet_t
* NAME
*	subnet_t
*
* DESCRIPTION
*	Subnet database object.
*
* SYNOPSIS
*/
typedef struct _subnet {
	cl_qmap_t node_lid_tbl;
	cl_qmap_t node_guid_tbl;
	cl_qmap_t mgrp_mlid_tbl;
	/* cl_qmap_t port_lid_tbl; */
	/* cl_qmap_t port_guid_tbl; */
	cl_qmap_t port_key_tbl;
	cl_qmap_t link_tbl;
	cl_qmap_t path_tbl;
} subnet_t;

/*
* FIELDS
*
* SEE ALSO
*********/

/****f* Subnet Database/subnet_construct
* NAME
*	subnet_construct
*
* DESCRIPTION
*	This function constructs an subnet database object.
*	This function cannot fail.
*
* SYNOPSIS
*/
void subnet_construct(IN subnet_t * const p_subn);

/*
* FIELDS
*
* SEE ALSO
*********/

/****f* Subnet Database/subnet_init
* NAME
*	subnet_init
*
* DESCRIPTION
*	This function initializes an subnet database object.
*
* SYNOPSIS
*/
cl_status_t subnet_init(IN subnet_t * const p_subn);

/*
* FIELDS
*
* SEE ALSO
*********/

#endif
