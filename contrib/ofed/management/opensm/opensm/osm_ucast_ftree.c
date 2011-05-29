/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2007 Mellanox Technologies LTD. All rights reserved.
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
 *    Implementation of OpenSM FatTree routing
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_debug.h>
#include <opensm/osm_opensm.h>
#include <opensm/osm_switch.h>

/*
 * FatTree rank is bounded between 2 and 8:
 *  - Tree of rank 1 has only trivial routing paths,
 *    so no need to use FatTree routing.
 *  - Why maximum rank is 8:
 *    Each node (switch) is assigned a unique tuple.
 *    Switches are stored in two cl_qmaps - one is
 *    ordered by guid, and the other by a key that is
 *    generated from tuple. Since cl_qmap supports only
 *    a 64-bit key, the maximal tuple lenght is 8 bytes.
 *    which means that maximal tree rank is 8.
 * Note that the above also implies that each switch
 * can have at max 255 up/down ports.
 */

#define FAT_TREE_MIN_RANK 2
#define FAT_TREE_MAX_RANK 8

typedef enum {
	FTREE_DIRECTION_DOWN = -1,
	FTREE_DIRECTION_SAME,
	FTREE_DIRECTION_UP
} ftree_direction_t;

/***************************************************
 **
 **  Forward references
 **
 ***************************************************/

struct ftree_sw_t_;
struct ftree_hca_t_;
struct ftree_port_t_;
struct ftree_port_group_t_;
struct ftree_fabric_t_;

/***************************************************
 **
 **  ftree_tuple_t definition
 **
 ***************************************************/

#define FTREE_TUPLE_BUFF_LEN 1024
#define FTREE_TUPLE_LEN 8

typedef uint8_t ftree_tuple_t[FTREE_TUPLE_LEN];
typedef uint64_t ftree_tuple_key_t;

struct guid_list_item {
	cl_list_item_t list;
	uint64_t guid;
};

/***************************************************
 **
 **  ftree_sw_table_element_t definition
 **
 ***************************************************/

typedef struct {
	cl_map_item_t map_item;
	struct ftree_sw_t_ *p_sw;
} ftree_sw_tbl_element_t;

/***************************************************
 **
 **  ftree_port_t definition
 **
 ***************************************************/

typedef struct ftree_port_t_ {
	cl_map_item_t map_item;
	uint8_t port_num;	/* port number on the current node */
	uint8_t remote_port_num;	/* port number on the remote node */
	uint32_t counter_up;	/* number of allocated routs upwards */
	uint32_t counter_down;	/* number of allocated routs downwards */
} ftree_port_t;

/***************************************************
 **
 **  ftree_port_group_t definition
 **
 ***************************************************/

typedef union ftree_hca_or_sw_ {
	struct ftree_hca_t_ *p_hca;
	struct ftree_sw_t_ *p_sw;
} ftree_hca_or_sw;

typedef struct ftree_port_group_t_ {
	cl_map_item_t map_item;
	ib_net16_t base_lid;	/* base lid of the current node */
	ib_net16_t remote_base_lid;	/* base lid of the remote node */
	ib_net64_t port_guid;	/* port guid of this port */
	ib_net64_t node_guid;	/* this node's guid */
	uint8_t node_type;	/* this node's type */
	ib_net64_t remote_port_guid;	/* port guid of the remote port */
	ib_net64_t remote_node_guid;	/* node guid of the remote node */
	uint8_t remote_node_type;	/* IB_NODE_TYPE_{CA,SWITCH,ROUTER,...} */
	ftree_hca_or_sw hca_or_sw;	/* pointer to this hca/switch */
	ftree_hca_or_sw remote_hca_or_sw;	/* pointer to remote hca/switch */
	cl_ptr_vector_t ports;	/* vector of ports to the same lid */
	boolean_t is_cn;	/* whether this port is a compute node */
	uint32_t counter_down;	/* number of allocated routs downwards */
} ftree_port_group_t;

/***************************************************
 **
 **  ftree_sw_t definition
 **
 ***************************************************/

typedef struct ftree_sw_t_ {
	cl_map_item_t map_item;
	osm_switch_t *p_osm_sw;
	uint32_t rank;
	ftree_tuple_t tuple;
	ib_net16_t base_lid;
	ftree_port_group_t **down_port_groups;
	uint8_t down_port_groups_num;
	ftree_port_group_t **up_port_groups;
	uint8_t up_port_groups_num;
	boolean_t is_leaf;
	int down_port_groups_idx;
} ftree_sw_t;

/***************************************************
 **
 **  ftree_hca_t definition
 **
 ***************************************************/

typedef struct ftree_hca_t_ {
	cl_map_item_t map_item;
	osm_node_t *p_osm_node;
	ftree_port_group_t **up_port_groups;
	uint16_t up_port_groups_num;
	unsigned cn_num;
} ftree_hca_t;

/***************************************************
 **
 **  ftree_fabric_t definition
 **
 ***************************************************/

typedef struct ftree_fabric_t_ {
	osm_opensm_t *p_osm;
	cl_qmap_t hca_tbl;
	cl_qmap_t sw_tbl;
	cl_qmap_t sw_by_tuple_tbl;
	cl_qlist_t root_guid_list;
	cl_qmap_t cn_guid_tbl;
	unsigned cn_num;
	uint8_t leaf_switch_rank;
	uint8_t max_switch_rank;
	ftree_sw_t **leaf_switches;
	uint32_t leaf_switches_num;
	uint16_t max_cn_per_leaf;
	uint16_t lft_max_lid_ho;
	boolean_t fabric_built;
} ftree_fabric_t;

/***************************************************
 **
 ** comparators
 **
 ***************************************************/

static int OSM_CDECL __osm_ftree_compare_switches_by_index(IN const void *p1,
							   IN const void *p2)
{
	ftree_sw_t **pp_sw1 = (ftree_sw_t **) p1;
	ftree_sw_t **pp_sw2 = (ftree_sw_t **) p2;

	uint16_t i;
	for (i = 0; i < FTREE_TUPLE_LEN; i++) {
		if ((*pp_sw1)->tuple[i] > (*pp_sw2)->tuple[i])
			return 1;
		if ((*pp_sw1)->tuple[i] < (*pp_sw2)->tuple[i])
			return -1;
	}
	return 0;
}

/***************************************************/

static int OSM_CDECL
__osm_ftree_compare_port_groups_by_remote_switch_index(IN const void *p1,
						       IN const void *p2)
{
	ftree_port_group_t **pp_g1 = (ftree_port_group_t **) p1;
	ftree_port_group_t **pp_g2 = (ftree_port_group_t **) p2;

	return
	    __osm_ftree_compare_switches_by_index(&
						  ((*pp_g1)->remote_hca_or_sw.
						   p_sw),
						  &((*pp_g2)->remote_hca_or_sw.
						    p_sw));
}

/***************************************************
 **
 ** ftree_tuple_t functions
 **
 ***************************************************/

static void __osm_ftree_tuple_init(IN ftree_tuple_t tuple)
{
	memset(tuple, 0xFF, FTREE_TUPLE_LEN);
}

/***************************************************/

static inline boolean_t __osm_ftree_tuple_assigned(IN ftree_tuple_t tuple)
{
	return (tuple[0] != 0xFF);
}

/***************************************************/

#define FTREE_TUPLE_BUFFERS_NUM 6

static char *__osm_ftree_tuple_to_str(IN ftree_tuple_t tuple)
{
	static char buffer[FTREE_TUPLE_BUFFERS_NUM][FTREE_TUPLE_BUFF_LEN];
	static uint8_t ind = 0;
	char *ret_buffer;
	uint32_t i;

	if (!__osm_ftree_tuple_assigned(tuple))
		return "INDEX.NOT.ASSIGNED";

	buffer[ind][0] = '\0';

	for (i = 0; (i < FTREE_TUPLE_LEN) && (tuple[i] != 0xFF); i++) {
		if ((strlen(buffer[ind]) + 10) > FTREE_TUPLE_BUFF_LEN)
			return "INDEX.TOO.LONG";
		if (i != 0)
			strcat(buffer[ind], ".");
		sprintf(&buffer[ind][strlen(buffer[ind])], "%u", tuple[i]);
	}

	ret_buffer = buffer[ind];
	ind = (ind + 1) % FTREE_TUPLE_BUFFERS_NUM;
	return ret_buffer;
}				/* __osm_ftree_tuple_to_str() */

/***************************************************/

static inline ftree_tuple_key_t __osm_ftree_tuple_to_key(IN ftree_tuple_t tuple)
{
	ftree_tuple_key_t key;
	memcpy(&key, tuple, FTREE_TUPLE_LEN);
	return key;
}

/***************************************************/

static inline void __osm_ftree_tuple_from_key(IN ftree_tuple_t tuple,
					      IN ftree_tuple_key_t key)
{
	memcpy(tuple, &key, FTREE_TUPLE_LEN);
}

/***************************************************
 **
 ** ftree_sw_tbl_element_t functions
 **
 ***************************************************/

static ftree_sw_tbl_element_t *__osm_ftree_sw_tbl_element_create(IN ftree_sw_t *
								 p_sw)
{
	ftree_sw_tbl_element_t *p_element =
	    (ftree_sw_tbl_element_t *) malloc(sizeof(ftree_sw_tbl_element_t));
	if (!p_element)
		return NULL;
	memset(p_element, 0, sizeof(ftree_sw_tbl_element_t));

	p_element->p_sw = p_sw;
	return p_element;
}

/***************************************************/

static void __osm_ftree_sw_tbl_element_destroy(IN ftree_sw_tbl_element_t *
					       p_element)
{
	if (!p_element)
		return;
	free(p_element);
}

/***************************************************
 **
 ** ftree_port_t functions
 **
 ***************************************************/

static ftree_port_t *__osm_ftree_port_create(IN uint8_t port_num,
					     IN uint8_t remote_port_num)
{
	ftree_port_t *p_port = (ftree_port_t *) malloc(sizeof(ftree_port_t));
	if (!p_port)
		return NULL;
	memset(p_port, 0, sizeof(ftree_port_t));

	p_port->port_num = port_num;
	p_port->remote_port_num = remote_port_num;

	return p_port;
}

/***************************************************/

static void __osm_ftree_port_destroy(IN ftree_port_t * p_port)
{
	if (p_port)
		free(p_port);
}

/***************************************************
 **
 ** ftree_port_group_t functions
 **
 ***************************************************/

static ftree_port_group_t *
__osm_ftree_port_group_create(IN ib_net16_t base_lid,
			      IN ib_net16_t remote_base_lid,
			      IN ib_net64_t port_guid,
			      IN ib_net64_t node_guid,
			      IN uint8_t node_type,
		              IN void *p_hca_or_sw,
			      IN ib_net64_t remote_port_guid,
			      IN ib_net64_t remote_node_guid,
			      IN uint8_t remote_node_type,
			      IN void *p_remote_hca_or_sw,
			      IN boolean_t is_cn)
{
	ftree_port_group_t *p_group =
	    (ftree_port_group_t *) malloc(sizeof(ftree_port_group_t));
	if (p_group == NULL)
		return NULL;
	memset(p_group, 0, sizeof(ftree_port_group_t));

	p_group->base_lid = base_lid;
	p_group->remote_base_lid = remote_base_lid;
	memcpy(&p_group->port_guid, &port_guid, sizeof(ib_net64_t));
	memcpy(&p_group->node_guid, &node_guid, sizeof(ib_net64_t));
	memcpy(&p_group->remote_port_guid, &remote_port_guid,
	       sizeof(ib_net64_t));
	memcpy(&p_group->remote_node_guid, &remote_node_guid,
	       sizeof(ib_net64_t));

	p_group->node_type = node_type;
	switch (node_type) {
	case IB_NODE_TYPE_CA:
		p_group->hca_or_sw.p_hca = (ftree_hca_t *) p_hca_or_sw;
		break;
	case IB_NODE_TYPE_SWITCH:
		p_group->hca_or_sw.p_sw = (ftree_sw_t *) p_hca_or_sw;
		break;
	default:
		/* we shouldn't get here - port is created only in hca or switch */
		CL_ASSERT(0);
	}

	p_group->remote_node_type = remote_node_type;
	switch (remote_node_type) {
	case IB_NODE_TYPE_CA:
		p_group->remote_hca_or_sw.p_hca =
		    (ftree_hca_t *) p_remote_hca_or_sw;
		break;
	case IB_NODE_TYPE_SWITCH:
		p_group->remote_hca_or_sw.p_sw =
		    (ftree_sw_t *) p_remote_hca_or_sw;
		break;
	default:
		/* we shouldn't get here - port is created only in hca or switch */
		CL_ASSERT(0);
	}

	cl_ptr_vector_init(&p_group->ports, 0,	/* min size */
			   8);	/* grow size */
	p_group->is_cn = is_cn;
	return p_group;
}				/* __osm_ftree_port_group_create() */

/***************************************************/

static void __osm_ftree_port_group_destroy(IN ftree_port_group_t * p_group)
{
	uint32_t i;
	uint32_t size;
	ftree_port_t *p_port;

	if (!p_group)
		return;

	/* remove all the elements of p_group->ports vector */
	size = cl_ptr_vector_get_size(&p_group->ports);
	for (i = 0; i < size; i++) {
		cl_ptr_vector_at(&p_group->ports, i, (void *)&p_port);
		__osm_ftree_port_destroy(p_port);
	}
	cl_ptr_vector_destroy(&p_group->ports);
	free(p_group);
}				/* __osm_ftree_port_group_destroy() */

/***************************************************/

static void
__osm_ftree_port_group_dump(IN ftree_fabric_t * p_ftree,
			    IN ftree_port_group_t * p_group,
			    IN ftree_direction_t direction)
{
	ftree_port_t *p_port;
	uint32_t size;
	uint32_t i;
	char buff[10 * 1024];

	if (!p_group)
		return;

	if (!osm_log_is_active(&p_ftree->p_osm->log, OSM_LOG_DEBUG))
		return;

	size = cl_ptr_vector_get_size(&p_group->ports);
	buff[0] = '\0';

	for (i = 0; i < size; i++) {
		cl_ptr_vector_at(&p_group->ports, i, (void *)&p_port);
		CL_ASSERT(p_port);

		if (i != 0)
			strcat(buff, ", ");
		sprintf(buff + strlen(buff), "%u", p_port->port_num);
	}

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
		"    Port Group of size %u, port(s): %s, direction: %s\n"
		"                  Local <--> Remote GUID (LID):"
		"0x%016" PRIx64 " (0x%04x) <--> 0x%016" PRIx64 " (0x%04x)\n",
		size,
		buff,
		(direction == FTREE_DIRECTION_DOWN) ? "DOWN" : "UP",
		cl_ntoh64(p_group->port_guid),
		cl_ntoh16(p_group->base_lid),
		cl_ntoh64(p_group->remote_port_guid),
		cl_ntoh16(p_group->remote_base_lid));

}				/* __osm_ftree_port_group_dump() */

/***************************************************/

static void
__osm_ftree_port_group_add_port(IN ftree_port_group_t * p_group,
				IN uint8_t port_num, IN uint8_t remote_port_num)
{
	uint16_t i;
	ftree_port_t *p_port;

	for (i = 0; i < cl_ptr_vector_get_size(&p_group->ports); i++) {
		cl_ptr_vector_at(&p_group->ports, i, (void *)&p_port);
		if (p_port->port_num == port_num)
			return;
	}

	p_port = __osm_ftree_port_create(port_num, remote_port_num);
	cl_ptr_vector_insert(&p_group->ports, p_port, NULL);
}

/***************************************************
 **
 ** ftree_sw_t functions
 **
 ***************************************************/

static ftree_sw_t *__osm_ftree_sw_create(IN ftree_fabric_t * p_ftree,
					 IN osm_switch_t * p_osm_sw)
{
	ftree_sw_t *p_sw;
	uint8_t ports_num;

	/* make sure that the switch has ports */
	if (p_osm_sw->num_ports == 1)
		return NULL;

	p_sw = (ftree_sw_t *) malloc(sizeof(ftree_sw_t));
	if (p_sw == NULL)
		return NULL;
	memset(p_sw, 0, sizeof(ftree_sw_t));

	p_sw->p_osm_sw = p_osm_sw;
	p_sw->rank = 0xFFFFFFFF;
	__osm_ftree_tuple_init(p_sw->tuple);

	p_sw->base_lid = osm_node_get_base_lid(p_sw->p_osm_sw->p_node, 0);

	ports_num = osm_node_get_num_physp(p_sw->p_osm_sw->p_node);
	p_sw->down_port_groups =
	    (ftree_port_group_t **) malloc(ports_num *
					   sizeof(ftree_port_group_t *));
	p_sw->up_port_groups =
	    (ftree_port_group_t **) malloc(ports_num *
					   sizeof(ftree_port_group_t *));
	if (!p_sw->down_port_groups || !p_sw->up_port_groups)
		return NULL;
	p_sw->down_port_groups_num = 0;
	p_sw->up_port_groups_num = 0;

	/* initialize lft buffer */
	memset(p_osm_sw->new_lft, OSM_NO_PATH, IB_LID_UCAST_END_HO + 1);

	p_sw->down_port_groups_idx = -1;

	return p_sw;
}				/* __osm_ftree_sw_create() */

/***************************************************/

static void __osm_ftree_sw_destroy(IN ftree_fabric_t * p_ftree,
				   IN ftree_sw_t * p_sw)
{
	uint8_t i;

	if (!p_sw)
		return;

	for (i = 0; i < p_sw->down_port_groups_num; i++)
		__osm_ftree_port_group_destroy(p_sw->down_port_groups[i]);
	for (i = 0; i < p_sw->up_port_groups_num; i++)
		__osm_ftree_port_group_destroy(p_sw->up_port_groups[i]);
	if (p_sw->down_port_groups)
		free(p_sw->down_port_groups);
	if (p_sw->up_port_groups)
		free(p_sw->up_port_groups);

	free(p_sw);
}				/* __osm_ftree_sw_destroy() */

/***************************************************/

static uint64_t __osm_ftree_sw_get_guid_no(IN ftree_sw_t * p_sw)
{
	if (!p_sw)
		return 0;
	return osm_node_get_node_guid(p_sw->p_osm_sw->p_node);
}

/***************************************************/

static uint64_t __osm_ftree_sw_get_guid_ho(IN ftree_sw_t * p_sw)
{
	return cl_ntoh64(__osm_ftree_sw_get_guid_no(p_sw));
}

/***************************************************/

static void __osm_ftree_sw_dump(IN ftree_fabric_t * p_ftree,
				IN ftree_sw_t * p_sw)
{
	uint32_t i;

	if (!p_sw)
		return;

	if (!osm_log_is_active(&p_ftree->p_osm->log, OSM_LOG_DEBUG))
		return;

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
		"Switch index: %s, GUID: 0x%016" PRIx64
		", Ports: %u DOWN, %u UP\n",
		__osm_ftree_tuple_to_str(p_sw->tuple),
		__osm_ftree_sw_get_guid_ho(p_sw), p_sw->down_port_groups_num,
		p_sw->up_port_groups_num);

	for (i = 0; i < p_sw->down_port_groups_num; i++)
		__osm_ftree_port_group_dump(p_ftree,
					    p_sw->down_port_groups[i],
					    FTREE_DIRECTION_DOWN);
	for (i = 0; i < p_sw->up_port_groups_num; i++)
		__osm_ftree_port_group_dump(p_ftree, p_sw->up_port_groups[i],
					    FTREE_DIRECTION_UP);

}				/* __osm_ftree_sw_dump() */

/***************************************************/

static boolean_t __osm_ftree_sw_ranked(IN ftree_sw_t * p_sw)
{
	return (p_sw->rank != 0xFFFFFFFF);
}

/***************************************************/

static ftree_port_group_t *
__osm_ftree_sw_get_port_group_by_remote_lid(IN ftree_sw_t * p_sw,
					    IN ib_net16_t remote_base_lid,
					    IN ftree_direction_t direction)
{
	uint32_t i;
	uint32_t size;
	ftree_port_group_t **port_groups;

	if (direction == FTREE_DIRECTION_UP) {
		port_groups = p_sw->up_port_groups;
		size = p_sw->up_port_groups_num;
	} else {
		port_groups = p_sw->down_port_groups;
		size = p_sw->down_port_groups_num;
	}

	for (i = 0; i < size; i++)
		if (remote_base_lid == port_groups[i]->remote_base_lid)
			return port_groups[i];

	return NULL;
}				/* __osm_ftree_sw_get_port_group_by_remote_lid() */

/***************************************************/

static void
__osm_ftree_sw_add_port(IN ftree_sw_t * p_sw,
			IN uint8_t port_num,
			IN uint8_t remote_port_num,
			IN ib_net16_t base_lid,
			IN ib_net16_t remote_base_lid,
			IN ib_net64_t port_guid,
			IN ib_net64_t remote_port_guid,
			IN ib_net64_t remote_node_guid,
			IN uint8_t remote_node_type,
			IN void *p_remote_hca_or_sw,
			IN ftree_direction_t direction)
{
	ftree_port_group_t *p_group =
	    __osm_ftree_sw_get_port_group_by_remote_lid(p_sw, remote_base_lid,
							direction);

	if (!p_group) {
		p_group = __osm_ftree_port_group_create(base_lid,
							remote_base_lid,
							port_guid,
							__osm_ftree_sw_get_guid_no
							(p_sw),
							IB_NODE_TYPE_SWITCH,
							p_sw, remote_port_guid,
							remote_node_guid,
							remote_node_type,
							p_remote_hca_or_sw,
							FALSE);
		CL_ASSERT(p_group);

		if (direction == FTREE_DIRECTION_UP)
			p_sw->up_port_groups[p_sw->up_port_groups_num++] =
			    p_group;
		else
			p_sw->down_port_groups[p_sw->down_port_groups_num++] =
			    p_group;
	}
	__osm_ftree_port_group_add_port(p_group, port_num, remote_port_num);

}				/* __osm_ftree_sw_add_port() */

/***************************************************/

static inline cl_status_t
__osm_ftree_sw_set_hops(IN ftree_sw_t * p_sw,
			IN uint16_t lid_ho, IN uint8_t port_num,
			IN uint8_t hops)
{
	/* set local min hop table(LID) */
	return osm_switch_set_hops(p_sw->p_osm_sw, lid_ho, port_num, hops);
}

/***************************************************
 **
 ** ftree_hca_t functions
 **
 ***************************************************/

static ftree_hca_t *__osm_ftree_hca_create(IN osm_node_t * p_osm_node)
{
	ftree_hca_t *p_hca = (ftree_hca_t *) malloc(sizeof(ftree_hca_t));
	if (p_hca == NULL)
		return NULL;
	memset(p_hca, 0, sizeof(ftree_hca_t));

	p_hca->p_osm_node = p_osm_node;
	p_hca->up_port_groups = (ftree_port_group_t **)
	    malloc(osm_node_get_num_physp(p_hca->p_osm_node) *
		   sizeof(ftree_port_group_t *));
	if (!p_hca->up_port_groups)
		return NULL;
	p_hca->up_port_groups_num = 0;
	return p_hca;
}

/***************************************************/

static void __osm_ftree_hca_destroy(IN ftree_hca_t * p_hca)
{
	uint32_t i;

	if (!p_hca)
		return;

	for (i = 0; i < p_hca->up_port_groups_num; i++)
		__osm_ftree_port_group_destroy(p_hca->up_port_groups[i]);

	if (p_hca->up_port_groups)
		free(p_hca->up_port_groups);

	free(p_hca);
}

/***************************************************/

static uint64_t __osm_ftree_hca_get_guid_no(IN ftree_hca_t * p_hca)
{
	if (!p_hca)
		return 0;
	return osm_node_get_node_guid(p_hca->p_osm_node);
}

/***************************************************/

static uint64_t __osm_ftree_hca_get_guid_ho(IN ftree_hca_t * p_hca)
{
	return cl_ntoh64(__osm_ftree_hca_get_guid_no(p_hca));
}

/***************************************************/

static void __osm_ftree_hca_dump(IN ftree_fabric_t * p_ftree,
				 IN ftree_hca_t * p_hca)
{
	uint32_t i;

	if (!p_hca)
		return;

	if (!osm_log_is_active(&p_ftree->p_osm->log, OSM_LOG_DEBUG))
		return;

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
		"CA GUID: 0x%016" PRIx64 ", Ports: %u UP\n",
		__osm_ftree_hca_get_guid_ho(p_hca), p_hca->up_port_groups_num);

	for (i = 0; i < p_hca->up_port_groups_num; i++)
		__osm_ftree_port_group_dump(p_ftree, p_hca->up_port_groups[i],
					    FTREE_DIRECTION_UP);
}

/***************************************************/

static ftree_port_group_t *
__osm_ftree_hca_get_port_group_by_remote_lid(IN ftree_hca_t * p_hca,
					     IN ib_net16_t remote_base_lid)
{
	uint32_t i;
	for (i = 0; i < p_hca->up_port_groups_num; i++)
		if (remote_base_lid ==
		    p_hca->up_port_groups[i]->remote_base_lid)
			return p_hca->up_port_groups[i];

	return NULL;
}

/***************************************************/

static void
__osm_ftree_hca_add_port(IN ftree_hca_t * p_hca,
			 IN uint8_t port_num,
			 IN uint8_t remote_port_num,
			 IN ib_net16_t base_lid,
			 IN ib_net16_t remote_base_lid,
			 IN ib_net64_t port_guid,
			 IN ib_net64_t remote_port_guid,
			 IN ib_net64_t remote_node_guid,
			 IN uint8_t remote_node_type,
			 IN void *p_remote_hca_or_sw, IN boolean_t is_cn)
{
	ftree_port_group_t *p_group;

	/* this function is supposed to be called only for adding ports
	   in hca's that lead to switches */
	CL_ASSERT(remote_node_type == IB_NODE_TYPE_SWITCH);

	p_group =
	    __osm_ftree_hca_get_port_group_by_remote_lid(p_hca,
							 remote_base_lid);

	if (!p_group) {
		p_group = __osm_ftree_port_group_create(base_lid,
							remote_base_lid,
							port_guid,
							__osm_ftree_hca_get_guid_no
							(p_hca),
							IB_NODE_TYPE_CA, p_hca,
							remote_port_guid,
							remote_node_guid,
							remote_node_type,
							p_remote_hca_or_sw,
							is_cn);
		p_hca->up_port_groups[p_hca->up_port_groups_num++] = p_group;
	}
	__osm_ftree_port_group_add_port(p_group, port_num, remote_port_num);

}				/* __osm_ftree_hca_add_port() */

/***************************************************
 **
 ** ftree_fabric_t functions
 **
 ***************************************************/

static ftree_fabric_t *__osm_ftree_fabric_create()
{
	ftree_fabric_t *p_ftree =
	    (ftree_fabric_t *) malloc(sizeof(ftree_fabric_t));
	if (p_ftree == NULL)
		return NULL;

	memset(p_ftree, 0, sizeof(ftree_fabric_t));

	cl_qmap_init(&p_ftree->hca_tbl);
	cl_qmap_init(&p_ftree->sw_tbl);
	cl_qmap_init(&p_ftree->sw_by_tuple_tbl);
	cl_qmap_init(&p_ftree->cn_guid_tbl);

	cl_qlist_init(&p_ftree->root_guid_list);

	return p_ftree;
}

/***************************************************/

static void __osm_ftree_fabric_clear(ftree_fabric_t * p_ftree)
{
	ftree_hca_t *p_hca;
	ftree_hca_t *p_next_hca;
	ftree_sw_t *p_sw;
	ftree_sw_t *p_next_sw;
	ftree_sw_tbl_element_t *p_element;
	ftree_sw_tbl_element_t *p_next_element;
	name_map_item_t *p_guid_element, *p_next_guid_element;

	if (!p_ftree)
		return;

	/* remove all the elements of hca_tbl */

	p_next_hca = (ftree_hca_t *) cl_qmap_head(&p_ftree->hca_tbl);
	while (p_next_hca != (ftree_hca_t *) cl_qmap_end(&p_ftree->hca_tbl)) {
		p_hca = p_next_hca;
		p_next_hca = (ftree_hca_t *) cl_qmap_next(&p_hca->map_item);
		__osm_ftree_hca_destroy(p_hca);
	}
	cl_qmap_remove_all(&p_ftree->hca_tbl);

	/* remove all the elements of sw_tbl */

	p_next_sw = (ftree_sw_t *) cl_qmap_head(&p_ftree->sw_tbl);
	while (p_next_sw != (ftree_sw_t *) cl_qmap_end(&p_ftree->sw_tbl)) {
		p_sw = p_next_sw;
		p_next_sw = (ftree_sw_t *) cl_qmap_next(&p_sw->map_item);
		__osm_ftree_sw_destroy(p_ftree, p_sw);
	}
	cl_qmap_remove_all(&p_ftree->sw_tbl);

	/* remove all the elements of sw_by_tuple_tbl */

	p_next_element =
	    (ftree_sw_tbl_element_t *) cl_qmap_head(&p_ftree->sw_by_tuple_tbl);
	while (p_next_element !=
	       (ftree_sw_tbl_element_t *) cl_qmap_end(&p_ftree->
						      sw_by_tuple_tbl)) {
		p_element = p_next_element;
		p_next_element =
		    (ftree_sw_tbl_element_t *) cl_qmap_next(&p_element->
							    map_item);
		__osm_ftree_sw_tbl_element_destroy(p_element);
	}
	cl_qmap_remove_all(&p_ftree->sw_by_tuple_tbl);

	/* remove all the elements of cn_guid_tbl */
	p_next_guid_element =
	    (name_map_item_t *) cl_qmap_head(&p_ftree->cn_guid_tbl);
	while (p_next_guid_element !=
	       (name_map_item_t *) cl_qmap_end(&p_ftree->cn_guid_tbl)) {
		p_guid_element = p_next_guid_element;
		p_next_guid_element =
		    (name_map_item_t *) cl_qmap_next(&p_guid_element->item);
		free(p_guid_element);
	}
	cl_qmap_remove_all(&p_ftree->cn_guid_tbl);

	/* remove all the elements of root_guid_list */
	while (!cl_is_qlist_empty(&p_ftree->root_guid_list))
		free(cl_qlist_remove_head(&p_ftree->root_guid_list));

	/* free the leaf switches array */
	if ((p_ftree->leaf_switches_num > 0) && (p_ftree->leaf_switches))
		free(p_ftree->leaf_switches);

	p_ftree->leaf_switches_num = 0;
	p_ftree->cn_num = 0;
	p_ftree->leaf_switch_rank = 0;
	p_ftree->max_switch_rank = 0;
	p_ftree->max_cn_per_leaf = 0;
	p_ftree->lft_max_lid_ho = 0;
	p_ftree->leaf_switches = NULL;
	p_ftree->fabric_built = FALSE;

}				/* __osm_ftree_fabric_destroy() */

/***************************************************/

static void __osm_ftree_fabric_destroy(ftree_fabric_t * p_ftree)
{
	if (!p_ftree)
		return;
	__osm_ftree_fabric_clear(p_ftree);
	free(p_ftree);
}

/***************************************************/

static uint8_t __osm_ftree_fabric_get_rank(ftree_fabric_t * p_ftree)
{
	return p_ftree->leaf_switch_rank + 1;
}

/***************************************************/

static void __osm_ftree_fabric_add_hca(ftree_fabric_t * p_ftree,
				       osm_node_t * p_osm_node)
{
	ftree_hca_t *p_hca = __osm_ftree_hca_create(p_osm_node);

	CL_ASSERT(osm_node_get_type(p_osm_node) == IB_NODE_TYPE_CA);

	cl_qmap_insert(&p_ftree->hca_tbl, p_osm_node->node_info.node_guid,
		       &p_hca->map_item);
}

/***************************************************/

static void __osm_ftree_fabric_add_sw(ftree_fabric_t * p_ftree,
				      osm_switch_t * p_osm_sw)
{
	ftree_sw_t *p_sw = __osm_ftree_sw_create(p_ftree, p_osm_sw);

	CL_ASSERT(osm_node_get_type(p_osm_sw->p_node) == IB_NODE_TYPE_SWITCH);

	cl_qmap_insert(&p_ftree->sw_tbl, p_osm_sw->p_node->node_info.node_guid,
		       &p_sw->map_item);

	/* track the max lid (in host order) that exists in the fabric */
	if (cl_ntoh16(p_sw->base_lid) > p_ftree->lft_max_lid_ho)
		p_ftree->lft_max_lid_ho = cl_ntoh16(p_sw->base_lid);
}

/***************************************************/

static void __osm_ftree_fabric_add_sw_by_tuple(IN ftree_fabric_t * p_ftree,
					       IN ftree_sw_t * p_sw)
{
	CL_ASSERT(__osm_ftree_tuple_assigned(p_sw->tuple));

	cl_qmap_insert(&p_ftree->sw_by_tuple_tbl,
		       __osm_ftree_tuple_to_key(p_sw->tuple),
		       &__osm_ftree_sw_tbl_element_create(p_sw)->map_item);
}

/***************************************************/

static ftree_sw_t *__osm_ftree_fabric_get_sw_by_tuple(IN ftree_fabric_t *
						      p_ftree,
						      IN ftree_tuple_t tuple)
{
	ftree_sw_tbl_element_t *p_element;

	CL_ASSERT(__osm_ftree_tuple_assigned(tuple));

	__osm_ftree_tuple_to_key(tuple);

	p_element =
	    (ftree_sw_tbl_element_t *) cl_qmap_get(&p_ftree->sw_by_tuple_tbl,
						   __osm_ftree_tuple_to_key
						   (tuple));
	if (p_element ==
	    (ftree_sw_tbl_element_t *) cl_qmap_end(&p_ftree->sw_by_tuple_tbl))
		return NULL;

	return p_element->p_sw;
}

/***************************************************/

static ftree_sw_t *__osm_ftree_fabric_get_sw_by_guid(IN ftree_fabric_t *
						     p_ftree, IN uint64_t guid)
{
	ftree_sw_t *p_sw;
	p_sw = (ftree_sw_t *) cl_qmap_get(&p_ftree->sw_tbl, guid);
	if (p_sw == (ftree_sw_t *) cl_qmap_end(&p_ftree->sw_tbl))
		return NULL;
	return p_sw;
}

/***************************************************/

static ftree_hca_t *__osm_ftree_fabric_get_hca_by_guid(IN ftree_fabric_t *
						       p_ftree,
						       IN uint64_t guid)
{
	ftree_hca_t *p_hca;
	p_hca = (ftree_hca_t *) cl_qmap_get(&p_ftree->hca_tbl, guid);
	if (p_hca == (ftree_hca_t *) cl_qmap_end(&p_ftree->hca_tbl))
		return NULL;
	return p_hca;
}

/***************************************************/

static void __osm_ftree_fabric_dump(ftree_fabric_t * p_ftree)
{
	uint32_t i;
	ftree_hca_t *p_hca;
	ftree_sw_t *p_sw;

	if (!osm_log_is_active(&p_ftree->p_osm->log, OSM_LOG_DEBUG))
		return;

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG, "\n"
		"                       |-------------------------------|\n"
		"                       |-  Full fabric topology dump  -|\n"
		"                       |-------------------------------|\n\n");

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG, "-- CAs:\n");

	for (p_hca = (ftree_hca_t *) cl_qmap_head(&p_ftree->hca_tbl);
	     p_hca != (ftree_hca_t *) cl_qmap_end(&p_ftree->hca_tbl);
	     p_hca = (ftree_hca_t *) cl_qmap_next(&p_hca->map_item)) {
		__osm_ftree_hca_dump(p_ftree, p_hca);
	}

	for (i = 0; i < p_ftree->max_switch_rank; i++) {
		OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
			"-- Rank %u switches\n", i);
		for (p_sw = (ftree_sw_t *) cl_qmap_head(&p_ftree->sw_tbl);
		     p_sw != (ftree_sw_t *) cl_qmap_end(&p_ftree->sw_tbl);
		     p_sw = (ftree_sw_t *) cl_qmap_next(&p_sw->map_item)) {
			if (p_sw->rank == i)
				__osm_ftree_sw_dump(p_ftree, p_sw);
		}
	}

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG, "\n"
		"                       |---------------------------------------|\n"
		"                       |- Full fabric topology dump completed -|\n"
		"                       |---------------------------------------|\n\n");
}				/* __osm_ftree_fabric_dump() */

/***************************************************/

static void __osm_ftree_fabric_dump_general_info(IN ftree_fabric_t * p_ftree)
{
	uint32_t i, j;
	ftree_sw_t *p_sw;

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_INFO,
		"General fabric topology info\n");
	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_INFO,
		"============================\n");

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_INFO,
		"  - FatTree rank (roots to leaf switches): %u\n",
		p_ftree->leaf_switch_rank + 1);
	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_INFO,
		"  - FatTree max switch rank: %u\n", p_ftree->max_switch_rank);
	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_INFO,
		"  - Fabric has %u CAs (%u of them CNs), %u switches\n",
		cl_qmap_count(&p_ftree->hca_tbl), p_ftree->cn_num,
		cl_qmap_count(&p_ftree->sw_tbl));

	CL_ASSERT(cl_qmap_count(&p_ftree->hca_tbl) >= p_ftree->cn_num);

	for (i = 0; i <= p_ftree->max_switch_rank; i++) {
		j = 0;
		for (p_sw = (ftree_sw_t *) cl_qmap_head(&p_ftree->sw_tbl);
		     p_sw != (ftree_sw_t *) cl_qmap_end(&p_ftree->sw_tbl);
		     p_sw = (ftree_sw_t *) cl_qmap_next(&p_sw->map_item)) {
			if (p_sw->rank == i)
				j++;
		}
		if (i == 0)
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_INFO,
				"  - Fabric has %u switches at rank %u (roots)\n",
				j, i);
		else if (i == p_ftree->leaf_switch_rank)
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_INFO,
				"  - Fabric has %u switches at rank %u (%u of them leafs)\n",
				j, i, p_ftree->leaf_switches_num);
		else
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_INFO,
				"  - Fabric has %u switches at rank %u\n", j,
				i);
	}

	if (osm_log_is_active(&p_ftree->p_osm->log, OSM_LOG_VERBOSE)) {
		OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
			"  - Root switches:\n");
		for (p_sw = (ftree_sw_t *) cl_qmap_head(&p_ftree->sw_tbl);
		     p_sw != (ftree_sw_t *) cl_qmap_end(&p_ftree->sw_tbl);
		     p_sw = (ftree_sw_t *) cl_qmap_next(&p_sw->map_item)) {
			if (p_sw->rank == 0)
				OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
					"      GUID: 0x%016" PRIx64
					", LID: %u, Index %s\n",
					__osm_ftree_sw_get_guid_ho(p_sw),
					cl_ntoh16(p_sw->base_lid),
					__osm_ftree_tuple_to_str(p_sw->tuple));
		}

		OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
			"  - Leaf switches (sorted by index):\n");
		for (i = 0; i < p_ftree->leaf_switches_num; i++) {
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
				"      GUID: 0x%016" PRIx64
				", LID: %u, Index %s\n",
				__osm_ftree_sw_get_guid_ho(p_ftree->
							   leaf_switches[i]),
				cl_ntoh16(p_ftree->leaf_switches[i]->base_lid),
				__osm_ftree_tuple_to_str(p_ftree->
							 leaf_switches[i]->
							 tuple));
		}
	}
}				/* __osm_ftree_fabric_dump_general_info() */

/***************************************************/

static void __osm_ftree_fabric_dump_hca_ordering(IN ftree_fabric_t * p_ftree)
{
	ftree_hca_t *p_hca;
	ftree_sw_t *p_sw;
	ftree_port_group_t *p_group_on_sw;
	ftree_port_group_t *p_group_on_hca;
	uint32_t i;
	uint32_t j;
	unsigned printed_hcas_on_leaf;

	char path[1024];
	FILE *p_hca_ordering_file;
	char *filename = "opensm-ftree-ca-order.dump";

	snprintf(path, sizeof(path), "%s/%s",
		 p_ftree->p_osm->subn.opt.dump_files_dir, filename);
	p_hca_ordering_file = fopen(path, "w");
	if (!p_hca_ordering_file) {
		OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR, "ERR AB01: "
			"cannot open file \'%s\': %s\n", filename,
			strerror(errno));
		return;
	}

	/* for each leaf switch (in indexing order) */
	for (i = 0; i < p_ftree->leaf_switches_num; i++) {
		p_sw = p_ftree->leaf_switches[i];
		printed_hcas_on_leaf = 0;

		/* for each real CA (CNs and not) connected to this switch */
		for (j = 0; j < p_sw->down_port_groups_num; j++) {
			p_group_on_sw = p_sw->down_port_groups[j];

			if (p_group_on_sw->remote_node_type != IB_NODE_TYPE_CA)
				continue;

			p_hca = p_group_on_sw->remote_hca_or_sw.p_hca;
			p_group_on_hca =
			    __osm_ftree_hca_get_port_group_by_remote_lid(p_hca,
									 p_group_on_sw->
									 base_lid);

			/* treat non-compute nodes as dummies */
			if (!p_group_on_hca->is_cn)
				continue;

			fprintf(p_hca_ordering_file, "0x%04x\t%s\n",
				cl_ntoh16(p_group_on_hca->base_lid),
				p_hca->p_osm_node->print_desc);

			printed_hcas_on_leaf++;
		}

		/* now print missing HCAs */
		for (j = 0;
		     j < (p_ftree->max_cn_per_leaf - printed_hcas_on_leaf); j++)
			fprintf(p_hca_ordering_file, "0xFFFF\tDUMMY\n");

	}
	/* done going through all the leaf switches */

	fclose(p_hca_ordering_file);
}				/* __osm_ftree_fabric_dump_hca_ordering() */

/***************************************************/

static void
__osm_ftree_fabric_assign_tuple(IN ftree_fabric_t * p_ftree,
				IN ftree_sw_t * p_sw,
				IN ftree_tuple_t new_tuple)
{
	memcpy(p_sw->tuple, new_tuple, FTREE_TUPLE_LEN);
	__osm_ftree_fabric_add_sw_by_tuple(p_ftree, p_sw);
}

/***************************************************/

static void __osm_ftree_fabric_assign_first_tuple(IN ftree_fabric_t * p_ftree,
						  IN ftree_sw_t * p_sw)
{
	uint8_t i;
	ftree_tuple_t new_tuple;

	__osm_ftree_tuple_init(new_tuple);
	new_tuple[0] = (uint8_t) p_sw->rank;
	for (i = 1; i <= p_sw->rank; i++)
		new_tuple[i] = 0;

	__osm_ftree_fabric_assign_tuple(p_ftree, p_sw, new_tuple);
}

/***************************************************/

static void
__osm_ftree_fabric_get_new_tuple(IN ftree_fabric_t * p_ftree,
				 OUT ftree_tuple_t new_tuple,
				 IN ftree_tuple_t from_tuple,
				 IN ftree_direction_t direction)
{
	ftree_sw_t *p_sw;
	ftree_tuple_t temp_tuple;
	uint8_t var_index;
	uint8_t i;

	__osm_ftree_tuple_init(new_tuple);
	memcpy(temp_tuple, from_tuple, FTREE_TUPLE_LEN);

	if (direction == FTREE_DIRECTION_DOWN) {
		temp_tuple[0]++;
		var_index = from_tuple[0] + 1;
	} else {
		temp_tuple[0]--;
		var_index = from_tuple[0];
	}

	for (i = 0; i < 0xFF; i++) {
		temp_tuple[var_index] = i;
		p_sw = __osm_ftree_fabric_get_sw_by_tuple(p_ftree, temp_tuple);
		if (p_sw == NULL)	/* found free tuple */
			break;
	}

	if (i == 0xFF) {
		/* new tuple not found - there are more than 255 ports in one direction */
		return;
	}
	memcpy(new_tuple, temp_tuple, FTREE_TUPLE_LEN);

}				/* __osm_ftree_fabric_get_new_tuple() */

/***************************************************/

static inline boolean_t __osm_ftree_fabric_roots_provided(IN ftree_fabric_t *
							  p_ftree)
{
	return (p_ftree->p_osm->subn.opt.root_guid_file != NULL);
}

/***************************************************/

static inline boolean_t __osm_ftree_fabric_cns_provided(IN ftree_fabric_t *
							p_ftree)
{
	return (p_ftree->p_osm->subn.opt.cn_guid_file != NULL);
}

/***************************************************/

static int __osm_ftree_fabric_mark_leaf_switches(IN ftree_fabric_t * p_ftree)
{
	ftree_sw_t *p_sw;
	ftree_hca_t *p_hca;
	ftree_hca_t *p_next_hca;
	unsigned i;
	int res = 0;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
		"Marking leaf switches in fabric\n");

	/* Scan all the CAs, if they have CNs - find CN port and mark switch
	   that is connected to this port as leaf switch.
	   Also, ensure that this marked leaf has rank of p_ftree->leaf_switch_rank. */
	p_next_hca = (ftree_hca_t *) cl_qmap_head(&p_ftree->hca_tbl);
	while (p_next_hca != (ftree_hca_t *) cl_qmap_end(&p_ftree->hca_tbl)) {
		p_hca = p_next_hca;
		p_next_hca = (ftree_hca_t *) cl_qmap_next(&p_hca->map_item);
		if (!p_hca->cn_num)
			continue;

		for (i = 0; i < p_hca->up_port_groups_num; i++) {
			if (!p_hca->up_port_groups[i]->is_cn)
				continue;

			/* In CAs, port group alway has one port, and since this
			   port group is CN, we know that this port is compute node */
			CL_ASSERT(p_hca->up_port_groups[i]->remote_node_type ==
				  IB_NODE_TYPE_SWITCH);
			p_sw = p_hca->up_port_groups[i]->remote_hca_or_sw.p_sw;

			/* check if this switch was already processed */
			if (p_sw->is_leaf)
				continue;
			p_sw->is_leaf = TRUE;

			/* ensure that this leaf switch is at the correct tree level */
			if (p_sw->rank != p_ftree->leaf_switch_rank) {
				OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR,
					"ERR AB26: CN port 0x%" PRIx64
					" is connected to switch 0x%" PRIx64
					" with rank %u, "
					"while FatTree leaf rank is %u\n",
					cl_ntoh64(p_hca->up_port_groups[i]->
						  port_guid),
					__osm_ftree_sw_get_guid_ho(p_sw),
					p_sw->rank, p_ftree->leaf_switch_rank);
				res = -1;
				goto Exit;

			}
		}
	}

Exit:
	OSM_LOG_EXIT(&p_ftree->p_osm->log);
	return res;
}				/* __osm_ftree_fabric_mark_leaf_switches() */

/***************************************************/

static void __osm_ftree_fabric_make_indexing(IN ftree_fabric_t * p_ftree)
{
	ftree_sw_t *p_remote_sw;
	ftree_sw_t *p_sw = NULL;
	ftree_sw_t *p_next_sw;
	ftree_tuple_t new_tuple;
	uint32_t i;
	cl_list_t bfs_list;
	ftree_sw_tbl_element_t *p_sw_tbl_element;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
		"Starting FatTree indexing\n");

	/* using the first leaf switch as a starting point for indexing algorithm. */
	p_next_sw = (ftree_sw_t *) cl_qmap_head(&p_ftree->sw_tbl);
	while (p_next_sw != (ftree_sw_t *) cl_qmap_end(&p_ftree->sw_tbl)) {
		p_sw = p_next_sw;
		if (p_sw->is_leaf)
			break;
		p_next_sw = (ftree_sw_t *) cl_qmap_next(&p_sw->map_item);
	}

	CL_ASSERT(p_next_sw != (ftree_sw_t *) cl_qmap_end(&p_ftree->sw_tbl));

	/* Assign the first tuple to the switch that is used as BFS starting point.
	   The tuple will be as follows: [rank].0.0.0...
	   This fuction also adds the switch it into the switch_by_tuple table. */
	__osm_ftree_fabric_assign_first_tuple(p_ftree, p_sw);

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
		"Indexing starting point:\n"
		"                                            - Switch rank  : %u\n"
		"                                            - Switch index : %s\n"
		"                                            - Node LID     : %u\n"
		"                                            - Node GUID    : 0x%016"
		PRIx64 "\n", p_sw->rank, __osm_ftree_tuple_to_str(p_sw->tuple),
		cl_ntoh16(p_sw->base_lid), __osm_ftree_sw_get_guid_ho(p_sw));

	/*
	 * Now run BFS and assign indexes to all switches
	 * Pseudo code of the algorithm is as follows:
	 *
	 *  * Add first switch to BFS queue
	 *  * While (BFS queue not empty)
	 *      - Pop the switch from the head of the queue
	 *      - Scan all the downward and upward ports
	 *      - For each port
	 *          + Get the remote switch
	 *          + Assign index to the remote switch
	 *          + Add remote switch to the BFS queue
	 */

	cl_list_init(&bfs_list, cl_qmap_count(&p_ftree->sw_tbl));
	cl_list_insert_tail(&bfs_list,
			    &__osm_ftree_sw_tbl_element_create(p_sw)->map_item);

	while (!cl_is_list_empty(&bfs_list)) {
		p_sw_tbl_element =
		    (ftree_sw_tbl_element_t *) cl_list_remove_head(&bfs_list);
		p_sw = p_sw_tbl_element->p_sw;
		__osm_ftree_sw_tbl_element_destroy(p_sw_tbl_element);

		/* Discover all the nodes from ports that are pointing down */

		if (p_sw->rank >= p_ftree->leaf_switch_rank) {
			/* whether downward ports are pointing to CAs or switches,
			   we don't assign indexes to switches that are located
			   lower than leaf switches */
		} else {
			/* This is not the leaf switch */
			for (i = 0; i < p_sw->down_port_groups_num; i++) {
				/* Work with port groups that are pointing to switches only.
				   No need to assign indexing to HCAs */
				if (p_sw->down_port_groups[i]->
				    remote_node_type != IB_NODE_TYPE_SWITCH)
					continue;

				p_remote_sw =
				    p_sw->down_port_groups[i]->remote_hca_or_sw.
				    p_sw;
				if (__osm_ftree_tuple_assigned
				    (p_remote_sw->tuple)) {
					/* this switch has been already indexed */
					continue;
				}
				/* allocate new tuple */
				__osm_ftree_fabric_get_new_tuple(p_ftree,
								 new_tuple,
								 p_sw->tuple,
								 FTREE_DIRECTION_DOWN);
				/* Assign the new tuple to the remote switch.
				   This fuction also adds the switch into the switch_by_tuple table. */
				__osm_ftree_fabric_assign_tuple(p_ftree,
								p_remote_sw,
								new_tuple);

				/* add the newly discovered switch to the BFS queue */
				cl_list_insert_tail(&bfs_list,
						    &__osm_ftree_sw_tbl_element_create
						    (p_remote_sw)->map_item);
			}
			/* Done assigning indexes to all the remote switches
			   that are pointed by the downgoing ports.
			   Now sort port groups according to remote index. */
			qsort(p_sw->down_port_groups,	/* array */
			      p_sw->down_port_groups_num,	/* number of elements */
			      sizeof(ftree_port_group_t *),	/* size of each element */
			      __osm_ftree_compare_port_groups_by_remote_switch_index);	/* comparator */
		}

		/* Done indexing switches from ports that go down.
		   Now do the same with ports that are pointing up. */

		if (p_sw->rank != 0) {
			/* This is not the root switch, which means that all the ports
			   that are pointing up are taking us to another switches. */
			for (i = 0; i < p_sw->up_port_groups_num; i++) {
				p_remote_sw =
				    p_sw->up_port_groups[i]->remote_hca_or_sw.
				    p_sw;
				if (__osm_ftree_tuple_assigned
				    (p_remote_sw->tuple))
					continue;
				/* allocate new tuple */
				__osm_ftree_fabric_get_new_tuple(p_ftree,
								 new_tuple,
								 p_sw->tuple,
								 FTREE_DIRECTION_UP);
				/* Assign the new tuple to the remote switch.
				   This fuction also adds the switch to the
				   switch_by_tuple table. */
				__osm_ftree_fabric_assign_tuple(p_ftree,
								p_remote_sw,
								new_tuple);
				/* add the newly discovered switch to the BFS queue */
				cl_list_insert_tail(&bfs_list,
						    &__osm_ftree_sw_tbl_element_create
						    (p_remote_sw)->map_item);
			}
			/* Done assigning indexes to all the remote switches
			   that are pointed by the upgoing ports.
			   Now sort port groups according to remote index. */
			qsort(p_sw->up_port_groups,	/* array */
			      p_sw->up_port_groups_num,	/* number of elements */
			      sizeof(ftree_port_group_t *),	/* size of each element */
			      __osm_ftree_compare_port_groups_by_remote_switch_index);	/* comparator */
		}
		/* Done assigning indexes to all the switches that are directly connected
		   to the current switch - go to the next switch in the BFS queue */
	}
	cl_list_destroy(&bfs_list);

	OSM_LOG_EXIT(&p_ftree->p_osm->log);
}				/* __osm_ftree_fabric_make_indexing() */

/***************************************************/

static int __osm_ftree_fabric_create_leaf_switch_array(IN ftree_fabric_t *
						       p_ftree)
{
	ftree_sw_t *p_sw;
	ftree_sw_t *p_next_sw;
	ftree_sw_t **all_switches_at_leaf_level;
	unsigned i;
	unsigned all_leaf_idx = 0;
	unsigned first_leaf_idx;
	unsigned last_leaf_idx;
	int res = 0;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	/* create array of ALL the switches that have leaf rank */
	all_switches_at_leaf_level = (ftree_sw_t **)
	    malloc(cl_qmap_count(&p_ftree->sw_tbl) * sizeof(ftree_sw_t *));
	if (!all_switches_at_leaf_level) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"Fat-tree routing: Memory allocation failed\n");
		res = -1;
		goto Exit;
	}
	memset(all_switches_at_leaf_level, 0,
	       cl_qmap_count(&p_ftree->sw_tbl) * sizeof(ftree_sw_t *));

	p_next_sw = (ftree_sw_t *) cl_qmap_head(&p_ftree->sw_tbl);
	while (p_next_sw != (ftree_sw_t *) cl_qmap_end(&p_ftree->sw_tbl)) {
		p_sw = p_next_sw;
		p_next_sw = (ftree_sw_t *) cl_qmap_next(&p_sw->map_item);
		if (p_sw->rank == p_ftree->leaf_switch_rank) {
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
				"Adding switch 0x%" PRIx64
				" to full leaf switch array\n",
				__osm_ftree_sw_get_guid_ho(p_sw));
			all_switches_at_leaf_level[all_leaf_idx++] = p_sw;

		}
	}

	/* quick-sort array of leaf switches by index */
	qsort(all_switches_at_leaf_level,	/* array */
	      all_leaf_idx,	/* number of elements */
	      sizeof(ftree_sw_t *),	/* size of each element */
	      __osm_ftree_compare_switches_by_index);	/* comparator */

	/* check the first and the last REAL leaf (the one
	   that has CNs) in the array of all the leafs */

	first_leaf_idx = all_leaf_idx;
	last_leaf_idx = 0;
	for (i = 0; i < all_leaf_idx; i++) {
		if (all_switches_at_leaf_level[i]->is_leaf) {
			if (i < first_leaf_idx)
				first_leaf_idx = i;
			last_leaf_idx = i;
		}
	}
	CL_ASSERT(first_leaf_idx < last_leaf_idx);

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
		"Full leaf array info: first_leaf_idx = %u, last_leaf_idx = %u\n",
		first_leaf_idx, last_leaf_idx);

	/* Create array of REAL leaf switches, sorted by index.
	   This array may contain switches at the same rank w/o CNs,
	   in case this is the order of indexing. */
	p_ftree->leaf_switches_num = last_leaf_idx - first_leaf_idx + 1;
	p_ftree->leaf_switches = (ftree_sw_t **)
	    malloc(p_ftree->leaf_switches_num * sizeof(ftree_sw_t *));
	if (!p_ftree->leaf_switches) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"Fat-tree routing: Memory allocation failed\n");
		res = -1;
		goto Exit;
	}

	memcpy(p_ftree->leaf_switches,
	       &(all_switches_at_leaf_level[first_leaf_idx]),
	       p_ftree->leaf_switches_num * sizeof(ftree_sw_t *));

	free(all_switches_at_leaf_level);

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
		"Created array of %u leaf switches\n",
		p_ftree->leaf_switches_num);

Exit:
	OSM_LOG_EXIT(&p_ftree->p_osm->log);
	return res;
}				/* __osm_ftree_fabric_create_leaf_switch_array() */

/***************************************************/

static void __osm_ftree_fabric_set_max_cn_per_leaf(IN ftree_fabric_t * p_ftree)
{
	unsigned i;
	unsigned j;
	unsigned cns_on_this_leaf;
	ftree_sw_t *p_sw;
	ftree_port_group_t *p_group;

	for (i = 0; i < p_ftree->leaf_switches_num; i++) {
		p_sw = p_ftree->leaf_switches[i];
		cns_on_this_leaf = 0;
		for (j = 0; j < p_sw->down_port_groups_num; j++) {
			p_group = p_sw->down_port_groups[j];
			if (p_group->remote_node_type != IB_NODE_TYPE_CA)
				continue;
			cns_on_this_leaf +=
			    p_group->remote_hca_or_sw.p_hca->cn_num;
		}
		if (cns_on_this_leaf > p_ftree->max_cn_per_leaf)
			p_ftree->max_cn_per_leaf = cns_on_this_leaf;
	}
}				/* __osm_ftree_fabric_set_max_cn_per_leaf() */

/***************************************************/

static boolean_t __osm_ftree_fabric_validate_topology(IN ftree_fabric_t *
						      p_ftree)
{
	ftree_port_group_t *p_group;
	ftree_port_group_t *p_ref_group;
	ftree_sw_t *p_sw;
	ftree_sw_t *p_next_sw;
	ftree_sw_t **reference_sw_arr;
	uint16_t tree_rank = __osm_ftree_fabric_get_rank(p_ftree);
	boolean_t res = TRUE;
	uint8_t i;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
		"Validating fabric topology\n");

	reference_sw_arr =
	    (ftree_sw_t **) malloc(tree_rank * sizeof(ftree_sw_t *));
	if (reference_sw_arr == NULL) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"Fat-tree routing: Memory allocation failed\n");
		return FALSE;
	}
	memset(reference_sw_arr, 0, tree_rank * sizeof(ftree_sw_t *));

	p_next_sw = (ftree_sw_t *) cl_qmap_head(&p_ftree->sw_tbl);
	while (res && p_next_sw != (ftree_sw_t *) cl_qmap_end(&p_ftree->sw_tbl)) {
		p_sw = p_next_sw;
		p_next_sw = (ftree_sw_t *) cl_qmap_next(&p_sw->map_item);

		if (!reference_sw_arr[p_sw->rank]) {
			/* This is the first switch in the current level that
			   we're checking - use it as a reference */
			reference_sw_arr[p_sw->rank] = p_sw;
		} else {
			/* compare this switch properties to the reference switch */

			if (reference_sw_arr[p_sw->rank]->up_port_groups_num !=
			    p_sw->up_port_groups_num) {
				OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR,
					"ERR AB09: Different number of upward port groups on switches:\n"
					"       GUID 0x%016" PRIx64
					", LID %u, Index %s - %u groups\n"
					"       GUID 0x%016" PRIx64
					", LID %u, Index %s - %u groups\n",
					__osm_ftree_sw_get_guid_ho
					(reference_sw_arr[p_sw->rank]),
					cl_ntoh16(reference_sw_arr[p_sw->rank]->
						  base_lid),
					__osm_ftree_tuple_to_str
					(reference_sw_arr[p_sw->rank]->tuple),
					reference_sw_arr[p_sw->rank]->
					up_port_groups_num,
					__osm_ftree_sw_get_guid_ho(p_sw),
					cl_ntoh16(p_sw->base_lid),
					__osm_ftree_tuple_to_str(p_sw->tuple),
					p_sw->up_port_groups_num);
				res = FALSE;
				break;
			}

			if (p_sw->rank != (tree_rank - 1) &&
			    reference_sw_arr[p_sw->rank]->
			    down_port_groups_num !=
			    p_sw->down_port_groups_num) {
				/* we're allowing some hca's to be missing */
				OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR,
					"ERR AB0A: Different number of downward port groups on switches:\n"
					"       GUID 0x%016" PRIx64
					", LID %u, Index %s - %u port groups\n"
					"       GUID 0x%016" PRIx64
					", LID %u, Index %s - %u port groups\n",
					__osm_ftree_sw_get_guid_ho
					(reference_sw_arr[p_sw->rank]),
					cl_ntoh16(reference_sw_arr[p_sw->rank]->
						  base_lid),
					__osm_ftree_tuple_to_str
					(reference_sw_arr[p_sw->rank]->tuple),
					reference_sw_arr[p_sw->rank]->
					down_port_groups_num,
					__osm_ftree_sw_get_guid_ho(p_sw),
					cl_ntoh16(p_sw->base_lid),
					__osm_ftree_tuple_to_str(p_sw->tuple),
					p_sw->down_port_groups_num);
				res = FALSE;
				break;
			}

			if (reference_sw_arr[p_sw->rank]->up_port_groups_num !=
			    0) {
				p_ref_group =
				    reference_sw_arr[p_sw->rank]->
				    up_port_groups[0];
				for (i = 0; i < p_sw->up_port_groups_num; i++) {
					p_group = p_sw->up_port_groups[i];
					if (cl_ptr_vector_get_size
					    (&p_ref_group->ports) !=
					    cl_ptr_vector_get_size(&p_group->
								   ports)) {
						OSM_LOG(&p_ftree->p_osm->log,
							OSM_LOG_ERROR,
							"ERR AB0B: Different number of ports in an upward port group on switches:\n"
							"       GUID 0x%016"
							PRIx64
							", LID %u, Index %s - %u ports\n"
							"       GUID 0x%016"
							PRIx64
							", LID %u, Index %s - %u ports\n",
							__osm_ftree_sw_get_guid_ho
							(reference_sw_arr
							 [p_sw->rank]),
							cl_ntoh16
							(reference_sw_arr
							 [p_sw->rank]->
							 base_lid),
							__osm_ftree_tuple_to_str
							(reference_sw_arr
							 [p_sw->rank]->tuple),
							cl_ptr_vector_get_size
							(&p_ref_group->ports),
							__osm_ftree_sw_get_guid_ho
							(p_sw),
							cl_ntoh16(p_sw->
								  base_lid),
							__osm_ftree_tuple_to_str
							(p_sw->tuple),
							cl_ptr_vector_get_size
							(&p_group->ports));
						res = FALSE;
						break;
					}
				}
			}
			if (reference_sw_arr[p_sw->rank]->
			    down_port_groups_num != 0
			    && p_sw->rank != (tree_rank - 1)) {
				/* we're allowing some hca's to be missing */
				p_ref_group =
				    reference_sw_arr[p_sw->rank]->
				    down_port_groups[0];
				for (i = 0; i < p_sw->down_port_groups_num; i++) {
					p_group = p_sw->down_port_groups[0];
					if (cl_ptr_vector_get_size
					    (&p_ref_group->ports) !=
					    cl_ptr_vector_get_size(&p_group->
								   ports)) {
						OSM_LOG(&p_ftree->p_osm->log,
							OSM_LOG_ERROR,
							"ERR AB0C: Different number of ports in an downward port group on switches:\n"
							"       GUID 0x%016"
							PRIx64
							", LID %u, Index %s - %u ports\n"
							"       GUID 0x%016"
							PRIx64
							", LID %u, Index %s - %u ports\n",
							__osm_ftree_sw_get_guid_ho
							(reference_sw_arr
							 [p_sw->rank]),
							cl_ntoh16
							(reference_sw_arr
							 [p_sw->rank]->
							 base_lid),
							__osm_ftree_tuple_to_str
							(reference_sw_arr
							 [p_sw->rank]->tuple),
							cl_ptr_vector_get_size
							(&p_ref_group->ports),
							__osm_ftree_sw_get_guid_ho
							(p_sw),
							cl_ntoh16(p_sw->
								  base_lid),
							__osm_ftree_tuple_to_str
							(p_sw->tuple),
							cl_ptr_vector_get_size
							(&p_group->ports));
						res = FALSE;
						break;
					}
				}
			}
		}		/* end of else */
	}			/* end of while */

	if (res == TRUE)
		OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
			"Fabric topology has been identified as FatTree\n");
	else
		OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR,
			"ERR AB0D: Fabric topology hasn't been identified as FatTree\n");

	free(reference_sw_arr);
	OSM_LOG_EXIT(&p_ftree->p_osm->log);
	return res;
}				/* __osm_ftree_fabric_validate_topology() */

/***************************************************
 ***************************************************/

static void __osm_ftree_set_sw_fwd_table(IN cl_map_item_t * const p_map_item,
					 IN void *context)
{
	ftree_sw_t *p_sw = (ftree_sw_t * const)p_map_item;
	ftree_fabric_t *p_ftree = (ftree_fabric_t *) context;

	p_sw->p_osm_sw->max_lid_ho = p_ftree->lft_max_lid_ho;
	osm_ucast_mgr_set_fwd_table(&p_ftree->p_osm->sm.ucast_mgr,
				    p_sw->p_osm_sw);
}

/***************************************************
 ***************************************************/

/*
 * Function: assign-up-going-port-by-descending-down
 * Given   : a switch and a LID
 * Pseudo code:
 *    foreach down-going-port-group (in indexing order)
 *        skip this group if the LFT(LID) port is part of this group
 *        find the least loaded port of the group (scan in indexing order)
 *        r-port is the remote port connected to it
 *        assign the remote switch node LFT(LID) to r-port
 *        increase r-port usage counter
 *        assign-up-going-port-by-descending-down to r-port node (recursion)
 */

static void
__osm_ftree_fabric_route_upgoing_by_going_down(IN ftree_fabric_t * p_ftree,
					       IN ftree_sw_t * p_sw,
					       IN ftree_sw_t * p_prev_sw,
					       IN ib_net16_t target_lid,
					       IN uint8_t target_rank,
					       IN boolean_t is_real_lid,
					       IN boolean_t is_main_path,
					       IN uint8_t highest_rank_in_route)
{
	ftree_sw_t *p_remote_sw;
	uint16_t ports_num;
	ftree_port_group_t *p_group;
	ftree_port_t *p_port;
	ftree_port_t *p_min_port;
	uint16_t i;
	uint16_t j;
	uint16_t k;

	/* we shouldn't enter here if both real_lid and main_path are false */
	CL_ASSERT(is_real_lid || is_main_path);

	/* if there is no down-going ports */
	if (p_sw->down_port_groups_num == 0)
		return;

	/* promote the index that indicates which group should we
	   start with when going through all the downgoing groups */
	p_sw->down_port_groups_idx =
		(p_sw->down_port_groups_idx + 1) % p_sw->down_port_groups_num;

	/* foreach down-going port group (in indexing order) */
	i = p_sw->down_port_groups_idx;
	for (k = 0; k < p_sw->down_port_groups_num; k++) {

		p_group = p_sw->down_port_groups[i];
		i = (i + 1) % p_sw->down_port_groups_num;

		/* Skip this port group unless it points to a switch */
		if (p_group->remote_node_type != IB_NODE_TYPE_SWITCH)
			continue;

		if (p_prev_sw
		    && (p_group->remote_base_lid == p_prev_sw->base_lid)) {
			/* This port group has a port that was used when we entered this switch,
			   which means that the current group points to the switch where we were
			   at the previous step of the algorithm (before going up).
			   Skipping this group. */
			continue;
		}

		/* find the least loaded port of the group (in indexing order) */
		p_min_port = NULL;
		ports_num = (uint16_t) cl_ptr_vector_get_size(&p_group->ports);
		/* ToDo: no need to select a least loaded port for non-main path.
		   Think about optimization. */
		for (j = 0; j < ports_num; j++) {
			cl_ptr_vector_at(&p_group->ports, j, (void *)&p_port);
			if (!p_min_port) {
				/* first port that we're checking - set as port with the lowest load */
				p_min_port = p_port;
			} else if (p_port->counter_up < p_min_port->counter_up) {
				/* this port is less loaded - use it as min */
				p_min_port = p_port;
			}
		}
		/* At this point we have selected a port in this group with the
		   lowest load of upgoing routes.
		   Set on the remote switch how to get to the target_lid -
		   set LFT(target_lid) on the remote switch to the remote port */
		p_remote_sw = p_group->remote_hca_or_sw.p_sw;

		if (osm_switch_get_least_hops(p_remote_sw->p_osm_sw,
					      cl_ntoh16(target_lid)) !=
		    OSM_NO_PATH) {
			/* Loop in the fabric - we already routed the remote switch
			   on our way UP, and now we see it again on our way DOWN */
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
				"Loop of lenght %d in the fabric:\n                             "
				"Switch %s (LID %u) closes loop through switch %s (LID %u)\n",
				(p_remote_sw->rank - highest_rank_in_route) * 2,
				__osm_ftree_tuple_to_str(p_remote_sw->tuple),
				cl_ntoh16(p_group->base_lid),
				__osm_ftree_tuple_to_str(p_sw->tuple),
				cl_ntoh16(p_group->remote_base_lid));
			continue;
		}

		/* Four possible cases:
		 *
		 *  1. is_real_lid == TRUE && is_main_path == TRUE:
		 *      - going DOWN(TRUE,TRUE) through ALL the groups
		 *         + promoting port counter
		 *         + setting path in remote switch fwd tbl
		 *         + setting hops in remote switch on all the ports of each group
		 *
		 *  2. is_real_lid == TRUE && is_main_path == FALSE:
		 *      - going DOWN(TRUE,FALSE) through ALL the groups but only if
		 *        the remote (lower) switch hasn't been already configured
		 *        for this target LID
		 *         + NOT promoting port counter
		 *         + setting path in remote switch fwd tbl if it hasn't been set yet
		 *         + setting hops in remote switch on all the ports of each group
		 *           if it hasn't been set yet
		 *
		 *  3. is_real_lid == FALSE && is_main_path == TRUE:
		 *      - going DOWN(FALSE,TRUE) through ALL the groups
		 *         + promoting port counter
		 *         + NOT setting path in remote switch fwd tbl
		 *         + NOT setting hops in remote switch
		 *
		 *  4. is_real_lid == FALSE && is_main_path == FALSE:
		 *      - illegal state - we shouldn't get here
		 */

		/* second case: skip the port group if the remote (lower)
		   switch has been already configured for this target LID */
		if (is_real_lid && !is_main_path &&
		    p_remote_sw->p_osm_sw->new_lft[cl_ntoh16(target_lid)] != OSM_NO_PATH)
			continue;

		/* setting fwd tbl port only if this is real LID */
		if (is_real_lid) {
			p_remote_sw->p_osm_sw->new_lft[cl_ntoh16(target_lid)] =
				p_min_port->remote_port_num;
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
				"Switch %s: set path to CA LID %u through port %u\n",
				__osm_ftree_tuple_to_str(p_remote_sw->tuple),
				cl_ntoh16(target_lid),
				p_min_port->remote_port_num);

			/* On the remote switch that is pointed by the p_group,
			   set hops for ALL the ports in the remote group. */

			for (j = 0; j < ports_num; j++) {
				cl_ptr_vector_at(&p_group->ports, j,
						 (void *)&p_port);

				__osm_ftree_sw_set_hops(p_remote_sw,
							cl_ntoh16(target_lid),
							p_port->remote_port_num,
							((target_rank -
							  highest_rank_in_route)
							 + (p_remote_sw->rank -
							    highest_rank_in_route)));
			}

		}

		/* The number of upgoing routes is tracked in the
		   p_port->counter_up counter of the port that belongs to
		   the upper side of the link (on switch with lower rank).
		   Counter is promoted only if we're routing LID on the main
		   path (whether it's a real LID or a dummy one). */
		if (is_main_path)
			p_min_port->counter_up++;

		/* Recursion step:
		   Assign upgoing ports by stepping down, starting on REMOTE switch */
		__osm_ftree_fabric_route_upgoing_by_going_down(p_ftree, p_remote_sw,	/* remote switch - used as a route-upgoing alg. start point */
							       NULL,	/* prev. position - NULL to mark that we went down and not up */
							       target_lid,	/* LID that we're routing to */
							       target_rank,	/* rank of the LID that we're routing to */
							       is_real_lid,	/* whether the target LID is real or dummy */
							       is_main_path,	/* whether this is path to HCA that should by tracked by counters */
							       highest_rank_in_route);	/* highest visited point in the tree before going down */
	}
	/* done scanning all the down-going port groups */

}				/* __osm_ftree_fabric_route_upgoing_by_going_down() */

/***************************************************/

/*
 * Function: assign-down-going-port-by-ascending-up
 * Given   : a switch and a LID
 * Pseudo code:
 *    find the least loaded port of all the upgoing groups (scan in indexing order)
 *    assign the LFT(LID) of remote switch to that port
 *    track that port usage
 *    assign-up-going-port-by-descending-down on CURRENT switch
 *    assign-down-going-port-by-ascending-up on REMOTE switch (recursion)
 */

static void
__osm_ftree_fabric_route_downgoing_by_going_up(IN ftree_fabric_t * p_ftree,
					       IN ftree_sw_t * p_sw,
					       IN ftree_sw_t * p_prev_sw,
					       IN ib_net16_t target_lid,
					       IN uint8_t target_rank,
					       IN boolean_t is_real_lid,
					       IN boolean_t is_main_path)
{
	ftree_sw_t *p_remote_sw;
	uint16_t ports_num;
	ftree_port_group_t *p_group;
	ftree_port_t *p_port;
	ftree_port_group_t *p_min_group;
	ftree_port_t *p_min_port;
	uint16_t i;
	uint16_t j;

	/* we shouldn't enter here if both real_lid and main_path are false */
	CL_ASSERT(is_real_lid || is_main_path);

	/* Assign upgoing ports by stepping down, starting on THIS switch */
	__osm_ftree_fabric_route_upgoing_by_going_down(p_ftree, p_sw,	/* local switch - used as a route-upgoing alg. start point */
						       p_prev_sw,	/* switch that we went up from (NULL means that we went down) */
						       target_lid,	/* LID that we're routing to */
						       target_rank,	/* rank of the LID that we're routing to */
						       is_real_lid,	/* whether this target LID is real or dummy */
						       is_main_path,	/* whether this path to HCA should by tracked by counters */
						       p_sw->rank);	/* the highest visited point in the tree before going down */

	/* recursion stop condition - if it's a root switch, */
	if (p_sw->rank == 0)
		return;

	/* Find the least loaded upgoing port group */
	p_min_group = NULL;
	for (i = 0; i < p_sw->up_port_groups_num; i++) {
		p_group = p_sw->up_port_groups[i];
		if (!p_min_group) {
			/* first group that we're checking - use
			   it as a group with the lowest load */
			p_min_group = p_group;
		} else if (p_group->counter_down < p_min_group->counter_down) {
			/* this group is less loaded - use it as min */
			p_min_group = p_group;
		}
	}

	/* Find the least loaded upgoing port in the selected group */
	p_min_port = NULL;
	ports_num = (uint16_t) cl_ptr_vector_get_size(&p_min_group->ports);
	for (j = 0; j < ports_num; j++) {
		cl_ptr_vector_at(&p_min_group->ports, j, (void *)&p_port);
		if (!p_min_port) {
			/* first port that we're checking - use
			   it as a port with the lowest load */
			p_min_port = p_port;
		} else if (p_port->counter_down < p_min_port->counter_down) {
			/* this port is less loaded - use it as min */
			p_min_port = p_port;
		}
	}

	/* At this point we have selected a group and port with the
	   lowest load of downgoing routes.
	   Set on the remote switch how to get to the target_lid -
	   set LFT(target_lid) on the remote switch to the remote port */
	p_remote_sw = p_min_group->remote_hca_or_sw.p_sw;

	/* Four possible cases:
	 *
	 *  1. is_real_lid == TRUE && is_main_path == TRUE:
	 *      - going UP(TRUE,TRUE) on selected min_group and min_port
	 *         + promoting port counter
	 *         + setting path in remote switch fwd tbl
	 *         + setting hops in remote switch on all the ports of selected group
	 *      - going UP(TRUE,FALSE) on rest of the groups, each time on port 0
	 *         + NOT promoting port counter
	 *         + setting path in remote switch fwd tbl if it hasn't been set yet
	 *         + setting hops in remote switch on all the ports of each group
	 *           if it hasn't been set yet
	 *
	 *  2. is_real_lid == TRUE && is_main_path == FALSE:
	 *      - going UP(TRUE,FALSE) on ALL the groups, each time on port 0,
	 *        but only if the remote (upper) switch hasn't been already
	 *        configured for this target LID
	 *         + NOT promoting port counter
	 *         + setting path in remote switch fwd tbl if it hasn't been set yet
	 *         + setting hops in remote switch on all the ports of each group
	 *           if it hasn't been set yet
	 *
	 *  3. is_real_lid == FALSE && is_main_path == TRUE:
	 *      - going UP(FALSE,TRUE) ONLY on selected min_group and min_port
	 *         + promoting port counter
	 *         + NOT setting path in remote switch fwd tbl
	 *         + NOT setting hops in remote switch
	 *
	 *  4. is_real_lid == FALSE && is_main_path == FALSE:
	 *      - illegal state - we shouldn't get here
	 */

	/* covering first half of case 1, and case 3 */
	if (is_main_path) {
		if (p_sw->is_leaf) {
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
				" - Routing MAIN path for %s CA LID %u: %s --> %s\n",
				(is_real_lid) ? "real" : "DUMMY",
				cl_ntoh16(target_lid),
				__osm_ftree_tuple_to_str(p_sw->tuple),
				__osm_ftree_tuple_to_str(p_remote_sw->tuple));
		}
		/* The number of downgoing routes is tracked in the
		   p_group->counter_down p_port->counter_down counters of the
		   group and port that belong to the lower side of the link
		   (on switch with higher rank) */
		p_min_group->counter_down++;
		p_min_port->counter_down++;
		if (is_real_lid) {
			p_remote_sw->p_osm_sw->new_lft[cl_ntoh16(target_lid)] =
				p_min_port->remote_port_num;
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
				"Switch %s: set path to CA LID %u through port %u\n",
				__osm_ftree_tuple_to_str(p_remote_sw->tuple),
				cl_ntoh16(target_lid),
				p_min_port->remote_port_num);

			/* On the remote switch that is pointed by the min_group,
			   set hops for ALL the ports in the remote group. */

			ports_num =
			    (uint16_t) cl_ptr_vector_get_size(&p_min_group->
							      ports);
			for (j = 0; j < ports_num; j++) {
				cl_ptr_vector_at(&p_min_group->ports, j,
						 (void *)&p_port);
				__osm_ftree_sw_set_hops(p_remote_sw,
							cl_ntoh16(target_lid),
							p_port->remote_port_num,
							target_rank -
							p_remote_sw->rank);
			}
		}

		/* Recursion step:
		   Assign downgoing ports by stepping up, starting on REMOTE switch. */
		__osm_ftree_fabric_route_downgoing_by_going_up(p_ftree, p_remote_sw,	/* remote switch - used as a route-downgoing alg. next step point */
							       p_sw,	/* this switch - prev. position switch for the function */
							       target_lid,	/* LID that we're routing to */
							       target_rank,	/* rank of the LID that we're routing to */
							       is_real_lid,	/* whether this target LID is real or dummy */
							       is_main_path);	/* whether this is path to HCA that should by tracked by counters */
	}

	/* we're done for the third case */
	if (!is_real_lid)
		return;

	/* What's left to do at this point:
	 *
	 *  1. is_real_lid == TRUE && is_main_path == TRUE:
	 *      - going UP(TRUE,FALSE) on rest of the groups, each time on port 0,
	 *        but only if the remote (upper) switch hasn't been already
	 *        configured for this target LID
	 *         + NOT promoting port counter
	 *         + setting path in remote switch fwd tbl if it hasn't been set yet
	 *         + setting hops in remote switch on all the ports of each group
	 *           if it hasn't been set yet
	 *
	 *  2. is_real_lid == TRUE && is_main_path == FALSE:
	 *      - going UP(TRUE,FALSE) on ALL the groups, each time on port 0,
	 *        but only if the remote (upper) switch hasn't been already
	 *        configured for this target LID
	 *         + NOT promoting port counter
	 *         + setting path in remote switch fwd tbl if it hasn't been set yet
	 *         + setting hops in remote switch on all the ports of each group
	 *           if it hasn't been set yet
	 *
	 *  These two rules can be rephrased this way:
	 *   - foreach UP port group
	 *      + if remote switch has been set with the target LID
	 *         - skip this port group
	 *      + else
	 *         - select port 0
	 *         - do NOT promote port counter
	 *         - set path in remote switch fwd tbl
	 *         - set hops in remote switch on all the ports of this group
	 *         - go UP(TRUE,FALSE) to the remote switch
	 */

	for (i = 0; i < p_sw->up_port_groups_num; i++) {
		p_group = p_sw->up_port_groups[i];
		p_remote_sw = p_group->remote_hca_or_sw.p_sw;

		/* skip if target lid has been already set on remote switch fwd tbl */
		if (p_remote_sw->p_osm_sw->new_lft[cl_ntoh16(target_lid)] != OSM_NO_PATH)
			continue;

		if (p_sw->is_leaf) {
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
				" - Routing SECONDARY path for LID %u: %s --> %s\n",
				cl_ntoh16(target_lid),
				__osm_ftree_tuple_to_str(p_sw->tuple),
				__osm_ftree_tuple_to_str(p_remote_sw->tuple));
		}

		/* Routing REAL lids on SECONDARY path means routing
		   switch-to-switch or switch-to-CA paths.
		   We can safely assume that switch will initiate very
		   few traffic, so there's no point waisting runtime on
		   trying to balance these routes - always pick port 0. */

		cl_ptr_vector_at(&p_group->ports, 0, (void *)&p_port);
		p_remote_sw->p_osm_sw->new_lft[cl_ntoh16(target_lid)] =
			p_port->remote_port_num;

		/* On the remote switch that is pointed by the p_group,
		   set hops for ALL the ports in the remote group. */

		ports_num = (uint16_t) cl_ptr_vector_get_size(&p_group->ports);
		for (j = 0; j < ports_num; j++) {
			cl_ptr_vector_at(&p_group->ports, j, (void *)&p_port);

			__osm_ftree_sw_set_hops(p_remote_sw,
						cl_ntoh16(target_lid),
						p_port->remote_port_num,
						target_rank -
						p_remote_sw->rank);
		}

		/* Recursion step:
		   Assign downgoing ports by stepping up, starting on REMOTE switch. */
		__osm_ftree_fabric_route_downgoing_by_going_up(p_ftree, p_remote_sw,	/* remote switch - used as a route-downgoing alg. next step point */
							       p_sw,	/* this switch - prev. position switch for the function */
							       target_lid,	/* LID that we're routing to */
							       target_rank,	/* rank of the LID that we're routing to */
							       TRUE,	/* whether the target LID is real or dummy */
							       FALSE);	/* whether this is path to HCA that should by tracked by counters */
	}

}				/* ftree_fabric_route_downgoing_by_going_up() */

/***************************************************/

/*
 * Pseudo code:
 *    foreach leaf switch (in indexing order)
 *       for each compute node (in indexing order)
 *          obtain the LID of the compute node
 *          set local LFT(LID) of the port connecting to compute node
 *          call assign-down-going-port-by-ascending-up(TRUE,TRUE) on CURRENT switch
 *       for each MISSING compute node
 *          call assign-down-going-port-by-ascending-up(FALSE,TRUE) on CURRENT switch
 */

static void __osm_ftree_fabric_route_to_cns(IN ftree_fabric_t * p_ftree)
{
	ftree_sw_t *p_sw;
	ftree_hca_t *p_hca;
	ftree_port_group_t *p_leaf_port_group;
	ftree_port_group_t *p_hca_port_group;
	ftree_port_t *p_port;
	uint32_t i;
	uint32_t j;
	ib_net16_t hca_lid;
	unsigned routed_targets_on_leaf;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	/* for each leaf switch (in indexing order) */
	for (i = 0; i < p_ftree->leaf_switches_num; i++) {
		p_sw = p_ftree->leaf_switches[i];
		routed_targets_on_leaf = 0;

		/* for each HCA connected to this switch */
		for (j = 0; j < p_sw->down_port_groups_num; j++) {
			p_leaf_port_group = p_sw->down_port_groups[j];

			/* work with this port group only if the remote node is CA */
			if (p_leaf_port_group->remote_node_type !=
			    IB_NODE_TYPE_CA)
				continue;

			p_hca = p_leaf_port_group->remote_hca_or_sw.p_hca;

			/* work with this port group only if remote HCA has CNs */
			if (!p_hca->cn_num)
				continue;

			p_hca_port_group =
			    __osm_ftree_hca_get_port_group_by_remote_lid(p_hca,
									 p_leaf_port_group->
									 base_lid);
			CL_ASSERT(p_hca_port_group);

			/* work with this port group only if remote port is CN */
			if (!p_hca_port_group->is_cn)
				continue;

			/* obtain the LID of HCA port */
			hca_lid = p_leaf_port_group->remote_base_lid;

			/* set local LFT(LID) to the port that is connected to HCA */
			cl_ptr_vector_at(&p_leaf_port_group->ports, 0,
					 (void *)&p_port);
			p_sw->p_osm_sw->new_lft[cl_ntoh16(hca_lid)] = p_port->port_num;

			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
				"Switch %s: set path to CN LID %u through port %u\n",
				__osm_ftree_tuple_to_str(p_sw->tuple),
				cl_ntoh16(hca_lid), p_port->port_num);

			/* set local min hop table(LID) to route to the CA */
			__osm_ftree_sw_set_hops(p_sw,
						cl_ntoh16(hca_lid),
						p_port->port_num, 1);

			/* Assign downgoing ports by stepping up.
			   Since we're routing here only CNs, we're routing it as REAL
			   LID and updating fat-tree balancing counters. */
			__osm_ftree_fabric_route_downgoing_by_going_up(p_ftree, p_sw,	/* local switch - used as a route-downgoing alg. start point */
								       NULL,	/* prev. position switch */
								       hca_lid,	/* LID that we're routing to */
								       p_sw->rank + 1,	/* rank of the LID that we're routing to */
								       TRUE,	/* whether this HCA LID is real or dummy */
								       TRUE);	/* whether this path to HCA should by tracked by counters */

			/* count how many real targets have been routed from this leaf switch */
			routed_targets_on_leaf++;
		}

		/* We're done with the real targets (all CNs) of this leaf switch.
		   Now route the dummy HCAs that are missing or that are non-CNs.
		   When routing to dummy HCAs we don't fill lid matrices. */

		if (p_ftree->max_cn_per_leaf > routed_targets_on_leaf) {
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
				"Routing %u dummy CAs\n",
				p_ftree->max_cn_per_leaf -
				p_sw->down_port_groups_num);
			for (j = 0;
			     ((int)j) <
			     (p_ftree->max_cn_per_leaf -
			      routed_targets_on_leaf); j++) {
				/* assign downgoing ports by stepping up */
				__osm_ftree_fabric_route_downgoing_by_going_up(p_ftree, p_sw,	/* local switch - used as a route-downgoing alg. start point */
									       NULL,	/* prev. position switch */
									       0,	/* LID that we're routing to - ignored for dummy HCA */
									       0,	/* rank of the LID that we're routing to - ignored for dummy HCA */
									       FALSE,	/* whether this HCA LID is real or dummy */
									       TRUE);	/* whether this path to HCA should by tracked by counters */
			}
		}
	}
	/* done going through all the leaf switches */
	OSM_LOG_EXIT(&p_ftree->p_osm->log);
}				/* __osm_ftree_fabric_route_to_cns() */

/***************************************************/

/*
 * Pseudo code:
 *    foreach HCA non-CN port in fabric
 *       obtain the LID of the HCA port
 *       get switch that is connected to this HCA port
 *       set switch LFT(LID) to the port connecting to compute node
 *       call assign-down-going-port-by-ascending-up(TRUE,FALSE) on CURRENT switch
 *
 * Routing to these HCAs is routing a REAL hca lid on SECONDARY path.
 * However, we do want to allow load-leveling of the traffic to the non-CNs,
 * because such nodes may include IO nodes with heavy usage
 *   - we should set fwd tables
 *   - we should update port counters
 * Routing to non-CNs is done after routing to CNs, so updated port
 * counters will not affect CN-to-CN routing.
 */

static void __osm_ftree_fabric_route_to_non_cns(IN ftree_fabric_t * p_ftree)
{
	ftree_sw_t *p_sw;
	ftree_hca_t *p_hca;
	ftree_hca_t *p_next_hca;
	ftree_port_t *p_hca_port;
	ftree_port_group_t *p_hca_port_group;
	ib_net16_t hca_lid;
	unsigned port_num_on_switch;
	unsigned i;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	p_next_hca = (ftree_hca_t *) cl_qmap_head(&p_ftree->hca_tbl);
	while (p_next_hca != (ftree_hca_t *) cl_qmap_end(&p_ftree->hca_tbl)) {
		p_hca = p_next_hca;
		p_next_hca = (ftree_hca_t *) cl_qmap_next(&p_hca->map_item);

		for (i = 0; i < p_hca->up_port_groups_num; i++) {
			p_hca_port_group = p_hca->up_port_groups[i];

			/* skip this port if it's CN, in which case it has been already routed */
			if (p_hca_port_group->is_cn)
				continue;

			/* skip this port if it is not connected to switch */
			if (p_hca_port_group->remote_node_type !=
			    IB_NODE_TYPE_SWITCH)
				continue;

			p_sw = p_hca_port_group->remote_hca_or_sw.p_sw;
			hca_lid = p_hca_port_group->base_lid;

			/* set switches  LFT(LID) to the port that is connected to HCA */
			cl_ptr_vector_at(&p_hca_port_group->ports, 0,
					 (void *)&p_hca_port);
			port_num_on_switch = p_hca_port->remote_port_num;
			p_sw->p_osm_sw->new_lft[cl_ntoh16(hca_lid)] = port_num_on_switch;

			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
				"Switch %s: set path to non-CN HCA LID %u through port %u\n",
				__osm_ftree_tuple_to_str(p_sw->tuple),
				cl_ntoh16(hca_lid), port_num_on_switch);

			/* set local min hop table(LID) to route to the CA */
			__osm_ftree_sw_set_hops(p_sw, cl_ntoh16(hca_lid),
						port_num_on_switch,	/* port num */
						1);	/* hops */

			/* Assign downgoing ports by stepping up.
			   We're routing REAL targets. They are not CNs and not included
			   in the leafs array, but we treat them as MAIN path to allow load
			   leveling, which means that the counters will be updated. */
			__osm_ftree_fabric_route_downgoing_by_going_up(p_ftree, p_sw,	/* local switch - used as a route-downgoing alg. start point */
								       NULL,	/* prev. position switch */
								       hca_lid,	/* LID that we're routing to */
								       p_sw->rank + 1,	/* rank of the LID that we're routing to */
								       TRUE,	/* whether this HCA LID is real or dummy */
								       TRUE);	/* whether this path to HCA should by tracked by counters */
		}
		/* done with all the port groups of this HCA - go to next HCA */
	}

	OSM_LOG_EXIT(&p_ftree->p_osm->log);
}				/* __osm_ftree_fabric_route_to_non_cns() */

/***************************************************/

/*
 * Pseudo code:
 *    foreach switch in fabric
 *       obtain its LID
 *       set local LFT(LID) to port 0
 *       call assign-down-going-port-by-ascending-up(TRUE,FALSE) on CURRENT switch
 *
 * Routing to switch is similar to routing a REAL hca lid on SECONDARY path:
 *   - we should set fwd tables
 *   - we should NOT update port counters
 */

static void __osm_ftree_fabric_route_to_switches(IN ftree_fabric_t * p_ftree)
{
	ftree_sw_t *p_sw;
	ftree_sw_t *p_next_sw;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	p_next_sw = (ftree_sw_t *) cl_qmap_head(&p_ftree->sw_tbl);
	while (p_next_sw != (ftree_sw_t *) cl_qmap_end(&p_ftree->sw_tbl)) {
		p_sw = p_next_sw;
		p_next_sw = (ftree_sw_t *) cl_qmap_next(&p_sw->map_item);

		/* set local LFT(LID) to 0 (route to itself) */
		p_sw->p_osm_sw->new_lft[cl_ntoh16(p_sw->base_lid)] = 0;

		OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
			"Switch %s (LID %u): routing switch-to-switch paths\n",
			__osm_ftree_tuple_to_str(p_sw->tuple),
			cl_ntoh16(p_sw->base_lid));

		/* set min hop table of the switch to itself */
		__osm_ftree_sw_set_hops(p_sw, cl_ntoh16(p_sw->base_lid),
					0,	/* port_num */
					0);	/* hops     */

		__osm_ftree_fabric_route_downgoing_by_going_up(p_ftree, p_sw,	/* local switch - used as a route-downgoing alg. start point */
							       NULL,	/* prev. position switch */
							       p_sw->base_lid,	/* LID that we're routing to */
							       p_sw->rank,	/* rank of the LID that we're routing to */
							       TRUE,	/* whether the target LID is a real or dummy */
							       FALSE);	/* whether this path should by tracked by counters */
	}

	OSM_LOG_EXIT(&p_ftree->p_osm->log);
}				/* __osm_ftree_fabric_route_to_switches() */

/***************************************************
 ***************************************************/

static int __osm_ftree_fabric_populate_nodes(IN ftree_fabric_t * p_ftree)
{
	osm_node_t *p_osm_node;
	osm_node_t *p_next_osm_node;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	p_next_osm_node =
	    (osm_node_t *) cl_qmap_head(&p_ftree->p_osm->subn.node_guid_tbl);
	while (p_next_osm_node !=
	       (osm_node_t *) cl_qmap_end(&p_ftree->p_osm->subn.
					  node_guid_tbl)) {
		p_osm_node = p_next_osm_node;
		p_next_osm_node =
		    (osm_node_t *) cl_qmap_next(&p_osm_node->map_item);
		switch (osm_node_get_type(p_osm_node)) {
		case IB_NODE_TYPE_CA:
			__osm_ftree_fabric_add_hca(p_ftree, p_osm_node);
			break;
		case IB_NODE_TYPE_ROUTER:
			break;
		case IB_NODE_TYPE_SWITCH:
			__osm_ftree_fabric_add_sw(p_ftree, p_osm_node->sw);
			break;
		default:
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR, "ERR AB0E: "
				"Node GUID 0x%016" PRIx64
				" - Unknown node type: %s\n",
				cl_ntoh64(osm_node_get_node_guid(p_osm_node)),
				ib_get_node_type_str(osm_node_get_type
						     (p_osm_node)));
			OSM_LOG_EXIT(&p_ftree->p_osm->log);
			return -1;
		}
	}

	OSM_LOG_EXIT(&p_ftree->p_osm->log);
	return 0;
}				/* __osm_ftree_fabric_populate_nodes() */

/***************************************************
 ***************************************************/

static boolean_t __osm_ftree_sw_update_rank(IN ftree_sw_t * p_sw,
					    IN uint32_t new_rank)
{
	if (__osm_ftree_sw_ranked(p_sw) && p_sw->rank <= new_rank)
		return FALSE;
	p_sw->rank = new_rank;
	return TRUE;

}

/***************************************************/

static void
__osm_ftree_rank_switches_from_leafs(IN ftree_fabric_t * p_ftree,
				     IN cl_list_t * p_ranking_bfs_list)
{
	ftree_sw_t *p_sw;
	ftree_sw_t *p_remote_sw;
	osm_node_t *p_node;
	osm_node_t *p_remote_node;
	osm_physp_t *p_osm_port;
	uint8_t i;
	unsigned max_rank = 0;

	while (!cl_is_list_empty(p_ranking_bfs_list)) {
		p_sw = (ftree_sw_t *) cl_list_remove_head(p_ranking_bfs_list);
		p_node = p_sw->p_osm_sw->p_node;

		/* note: skipping port 0 on switches */
		for (i = 1; i < osm_node_get_num_physp(p_node); i++) {
			p_osm_port = osm_node_get_physp_ptr(p_node, i);
			if (!p_osm_port || !osm_link_is_healthy(p_osm_port))
				continue;

			p_remote_node =
			    osm_node_get_remote_node(p_node, i, NULL);
			if (!p_remote_node)
				continue;
			if (osm_node_get_type(p_remote_node) !=
			    IB_NODE_TYPE_SWITCH)
				continue;

			p_remote_sw = __osm_ftree_fabric_get_sw_by_guid(p_ftree,
									osm_node_get_node_guid
									(p_remote_node));
			if (!p_remote_sw) {
				/* remote node is not a switch */
				continue;
			}

			/* if needed, rank the remote switch and add it to the BFS list */
			if (__osm_ftree_sw_update_rank
			    (p_remote_sw, p_sw->rank + 1)) {
				max_rank = p_remote_sw->rank;
				cl_list_insert_tail(p_ranking_bfs_list,
						    p_remote_sw);
			}
		}
	}

	/* set FatTree maximal switch rank */
	p_ftree->max_switch_rank = max_rank;

}				/* __osm_ftree_rank_switches_from_leafs() */

/***************************************************/

static int
__osm_ftree_rank_leaf_switches(IN ftree_fabric_t * p_ftree,
			       IN ftree_hca_t * p_hca,
			       IN cl_list_t * p_ranking_bfs_list)
{
	ftree_sw_t *p_sw;
	osm_node_t *p_osm_node = p_hca->p_osm_node;
	osm_node_t *p_remote_osm_node;
	osm_physp_t *p_osm_port;
	static uint8_t i = 0;
	int res = 0;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	for (i = 0; i < osm_node_get_num_physp(p_osm_node); i++) {
		p_osm_port = osm_node_get_physp_ptr(p_osm_node, i);
		if (!p_osm_port || !osm_link_is_healthy(p_osm_port))
			continue;

		p_remote_osm_node =
		    osm_node_get_remote_node(p_osm_node, i, NULL);
		if (!p_remote_osm_node)
			continue;

		switch (osm_node_get_type(p_remote_osm_node)) {
		case IB_NODE_TYPE_CA:
			/* HCA connected directly to another HCA - not FatTree */
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR, "ERR AB0F: "
				"CA conected directly to another CA: "
				"0x%016" PRIx64 " <---> 0x%016" PRIx64 "\n",
				__osm_ftree_hca_get_guid_ho(p_hca),
				cl_ntoh64(osm_node_get_node_guid
					  (p_remote_osm_node)));
			res = -1;
			goto Exit;

		case IB_NODE_TYPE_ROUTER:
			/* leaving this port - proceeding to the next one */
			continue;

		case IB_NODE_TYPE_SWITCH:
			/* continue with this port */
			break;

		default:
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR,
				"ERR AB10: Node GUID 0x%016" PRIx64
				" - Unknown node type: %s\n",
				cl_ntoh64(osm_node_get_node_guid
					  (p_remote_osm_node)),
				ib_get_node_type_str(osm_node_get_type
						     (p_remote_osm_node)));
			res = -1;
			goto Exit;
		}

		/* remote node is switch */

		p_sw = __osm_ftree_fabric_get_sw_by_guid(p_ftree,
							 osm_node_get_node_guid
							 (p_osm_port->
							  p_remote_physp->
							  p_node));
		CL_ASSERT(p_sw);

		/* if needed, rank the remote switch and add it to the BFS list */

		if (!__osm_ftree_sw_update_rank(p_sw, 0))
			continue;
		OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
			"Marking rank of switch that is directly connected to CA:\n"
			"                                            - CA guid    : 0x%016"
			PRIx64 "\n"
			"                                            - Switch guid: 0x%016"
			PRIx64 "\n"
			"                                            - Switch LID : %u\n",
			__osm_ftree_hca_get_guid_ho(p_hca),
			__osm_ftree_sw_get_guid_ho(p_sw),
			cl_ntoh16(p_sw->base_lid));
		cl_list_insert_tail(p_ranking_bfs_list, p_sw);
	}

Exit:
	OSM_LOG_EXIT(&p_ftree->p_osm->log);
	return res;
}				/* __osm_ftree_rank_leaf_switches() */

/***************************************************/

static void __osm_ftree_sw_reverse_rank(IN cl_map_item_t * const p_map_item,
					IN void *context)
{
	ftree_fabric_t *p_ftree = (ftree_fabric_t *) context;
	ftree_sw_t *p_sw = (ftree_sw_t * const)p_map_item;
	p_sw->rank = p_ftree->max_switch_rank - p_sw->rank;
}

/***************************************************
 ***************************************************/

static int
__osm_ftree_fabric_construct_hca_ports(IN ftree_fabric_t * p_ftree,
				       IN ftree_hca_t * p_hca)
{
	ftree_sw_t *p_remote_sw;
	osm_node_t *p_node = p_hca->p_osm_node;
	osm_node_t *p_remote_node;
	uint8_t remote_node_type;
	ib_net64_t remote_node_guid;
	osm_physp_t *p_remote_osm_port;
	uint8_t i;
	uint8_t remote_port_num;
	boolean_t is_cn = FALSE;
	int res = 0;

	for (i = 0; i < osm_node_get_num_physp(p_node); i++) {
		osm_physp_t *p_osm_port = osm_node_get_physp_ptr(p_node, i);
		if (!p_osm_port || !osm_link_is_healthy(p_osm_port))
			continue;

		p_remote_osm_port = osm_physp_get_remote(p_osm_port);
		p_remote_node =
		    osm_node_get_remote_node(p_node, i, &remote_port_num);

		if (!p_remote_osm_port)
			continue;

		remote_node_type = osm_node_get_type(p_remote_node);
		remote_node_guid = osm_node_get_node_guid(p_remote_node);

		switch (remote_node_type) {
		case IB_NODE_TYPE_ROUTER:
			/* leaving this port - proceeding to the next one */
			continue;

		case IB_NODE_TYPE_CA:
			/* HCA connected directly to another HCA - not FatTree */
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR, "ERR AB11: "
				"CA conected directly to another CA: "
				"0x%016" PRIx64 " <---> 0x%016" PRIx64 "\n",
				cl_ntoh64(osm_node_get_node_guid(p_node)),
				cl_ntoh64(remote_node_guid));
			res = -1;
			goto Exit;

		case IB_NODE_TYPE_SWITCH:
			/* continue with this port */
			break;

		default:
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR,
				"ERR AB12: Node GUID 0x%016" PRIx64
				" - Unknown node type: %s\n",
				cl_ntoh64(remote_node_guid),
				ib_get_node_type_str(remote_node_type));
			res = -1;
			goto Exit;
		}

		/* remote node is switch */

		p_remote_sw =
		    __osm_ftree_fabric_get_sw_by_guid(p_ftree,
						      remote_node_guid);
		CL_ASSERT(p_remote_sw);

		/* If CN file is not supplied, then all the CAs considered as Compute Nodes.
		   Otherwise all the CAs are not CNs, and only guids that are present in the
		   CN file will be marked as compute nodes. */
		if (!__osm_ftree_fabric_cns_provided(p_ftree)) {
			is_cn = TRUE;
		} else {
			name_map_item_t *p_elem =
			    (name_map_item_t *) cl_qmap_get(&p_ftree->
							    cn_guid_tbl,
							    cl_ntoh64(osm_physp_get_port_guid
							    (p_osm_port)));
			if (p_elem !=
			    (name_map_item_t *) cl_qmap_end(&p_ftree->
							    cn_guid_tbl))
				is_cn = TRUE;
		}

		if (is_cn) {
			p_ftree->cn_num++;
			p_hca->cn_num++;
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
				"Marking CN port GUID 0x%016" PRIx64 "\n",
				cl_ntoh64(osm_physp_get_port_guid(p_osm_port)));
		} else {
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
				"Marking non-CN port GUID 0x%016" PRIx64 "\n",
				cl_ntoh64(osm_physp_get_port_guid(p_osm_port)));
		}

		__osm_ftree_hca_add_port(p_hca,	/* local ftree_hca object */
					 i,	/* local port number */
					 remote_port_num,	/* remote port number */
					 osm_node_get_base_lid(p_node, i),	/* local lid */
					 osm_node_get_base_lid(p_remote_node, 0),	/* remote lid */
					 osm_physp_get_port_guid(p_osm_port),	/* local port guid */
					 osm_physp_get_port_guid(p_remote_osm_port),	/* remote port guid */
					 remote_node_guid,	/* remote node guid */
					 remote_node_type,	/* remote node type */
					 (void *)p_remote_sw,	/* remote ftree_hca/sw object */
					 is_cn);	/* whether this port is compute node */
	}

Exit:
	return res;
}				/* __osm_ftree_fabric_construct_hca_ports() */

/***************************************************
 ***************************************************/

static int __osm_ftree_fabric_construct_sw_ports(IN ftree_fabric_t * p_ftree,
						 IN ftree_sw_t * p_sw)
{
	ftree_hca_t *p_remote_hca;
	ftree_sw_t *p_remote_sw;
	osm_node_t *p_node = p_sw->p_osm_sw->p_node;
	osm_node_t *p_remote_node;
	ib_net16_t remote_base_lid;
	uint8_t remote_node_type;
	ib_net64_t remote_node_guid;
	osm_physp_t *p_remote_osm_port;
	ftree_direction_t direction;
	void *p_remote_hca_or_sw;
	uint8_t i;
	uint8_t remote_port_num;
	int res = 0;

	CL_ASSERT(osm_node_get_type(p_node) == IB_NODE_TYPE_SWITCH);

	for (i = 1; i < osm_node_get_num_physp(p_node); i++) {
		osm_physp_t *p_osm_port = osm_node_get_physp_ptr(p_node, i);
		if (!p_osm_port || !osm_link_is_healthy(p_osm_port))
			continue;

		p_remote_osm_port = osm_physp_get_remote(p_osm_port);
		if (!p_remote_osm_port)
			continue;

		p_remote_node =
		    osm_node_get_remote_node(p_node, i, &remote_port_num);

		/* ignore any loopback connection on switch */
		if (p_node == p_remote_node) {
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
				"Ignoring loopback on switch GUID 0x%016" PRIx64
				", LID %u, rank %u\n",
				__osm_ftree_sw_get_guid_ho(p_sw),
				cl_ntoh16(p_sw->base_lid),
				p_sw->rank);
			continue;
		}

		remote_node_type = osm_node_get_type(p_remote_node);
		remote_node_guid = osm_node_get_node_guid(p_remote_node);

		switch (remote_node_type) {
		case IB_NODE_TYPE_ROUTER:
			/* leaving this port - proceeding to the next one */
			continue;

		case IB_NODE_TYPE_CA:
			/* switch connected to hca */

			p_remote_hca =
			    __osm_ftree_fabric_get_hca_by_guid(p_ftree,
							       remote_node_guid);
			CL_ASSERT(p_remote_hca);

			p_remote_hca_or_sw = (void *)p_remote_hca;
			direction = FTREE_DIRECTION_DOWN;

			remote_base_lid =
			    osm_physp_get_base_lid(p_remote_osm_port);
			break;

		case IB_NODE_TYPE_SWITCH:
			/* switch connected to another switch */

			p_remote_sw =
			    __osm_ftree_fabric_get_sw_by_guid(p_ftree,
							      remote_node_guid);
			CL_ASSERT(p_remote_sw);

			p_remote_hca_or_sw = (void *)p_remote_sw;

			if (abs(p_sw->rank - p_remote_sw->rank) != 1) {
				OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR,
					"ERR AB16: "
					"Illegal link between switches with ranks %u and %u:\n"
					"       GUID 0x%016" PRIx64
					", LID %u, rank %u\n"
					"       GUID 0x%016" PRIx64
					", LID %u, rank %u\n", p_sw->rank,
					p_remote_sw->rank,
					__osm_ftree_sw_get_guid_ho(p_sw),
					cl_ntoh16(p_sw->base_lid), p_sw->rank,
					__osm_ftree_sw_get_guid_ho(p_remote_sw),
					cl_ntoh16(p_remote_sw->base_lid),
					p_remote_sw->rank);
				res = -1;
				goto Exit;
			}

			if (p_sw->rank > p_remote_sw->rank)
				direction = FTREE_DIRECTION_UP;
			else
				direction = FTREE_DIRECTION_DOWN;

			/* switch LID is only in port 0 port_info structure */
			remote_base_lid =
			    osm_node_get_base_lid(p_remote_node, 0);

			break;

		default:
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR,
				"ERR AB13: Node GUID 0x%016" PRIx64
				" - Unknown node type: %s\n",
				cl_ntoh64(remote_node_guid),
				ib_get_node_type_str(remote_node_type));
			res = -1;
			goto Exit;
		}
		__osm_ftree_sw_add_port(p_sw,	/* local ftree_sw object */
					i,	/* local port number */
					remote_port_num,	/* remote port number */
					p_sw->base_lid,	/* local lid */
					remote_base_lid,	/* remote lid */
					osm_physp_get_port_guid(p_osm_port),	/* local port guid */
					osm_physp_get_port_guid(p_remote_osm_port),	/* remote port guid */
					remote_node_guid,	/* remote node guid */
					remote_node_type,	/* remote node type */
					p_remote_hca_or_sw,	/* remote ftree_hca/sw object */
					direction);	/* port direction (up or down) */

		/* Track the max lid (in host order) that exists in the fabric */
		if (cl_ntoh16(remote_base_lid) > p_ftree->lft_max_lid_ho)
			p_ftree->lft_max_lid_ho = cl_ntoh16(remote_base_lid);
	}

Exit:
	return res;
}				/* __osm_ftree_fabric_construct_sw_ports() */

/***************************************************
 ***************************************************/

static int __osm_ftree_fabric_rank_from_roots(IN ftree_fabric_t * p_ftree)
{
	osm_node_t *p_osm_node;
	osm_node_t *p_remote_osm_node;
	osm_physp_t *p_osm_physp;
	ftree_sw_t *p_sw;
	ftree_sw_t *p_remote_sw;
	cl_list_t ranking_bfs_list;
	struct guid_list_item *item;
	int res = 0;
	unsigned num_roots;
	unsigned max_rank = 0;
	unsigned i;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);
	cl_list_init(&ranking_bfs_list, 10);

	/* Rank all the roots and add them to list */
	for (item = (void *)cl_qlist_head(&p_ftree->root_guid_list);
	     item != (void *)cl_qlist_end(&p_ftree->root_guid_list);
	     item = (void *)cl_qlist_next(&item->list)) {
		p_sw =
		    __osm_ftree_fabric_get_sw_by_guid(p_ftree,
						      cl_hton64(item->guid));
		if (!p_sw) {
			/* the specified root guid wasn't found in the fabric */
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR, "ERR AB24: "
				"Root switch GUID 0x%" PRIx64 " not found\n",
				item->guid);
			continue;
		}

		OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
			"Ranking root switch with GUID 0x%" PRIx64 "\n",
			item->guid);
		p_sw->rank = 0;
		cl_list_insert_tail(&ranking_bfs_list, p_sw);
	}

	num_roots = cl_list_count(&ranking_bfs_list);
	if (!num_roots) {
		OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR, "ERR AB25: "
			"No valid roots supplied\n");
		res = -1;
		goto Exit;
	}

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
		"Ranked %u valid root switches\n", num_roots);

	/* Now the list has all the roots.
	   BFS the subnet and update rank on all the switches. */

	while (!cl_is_list_empty(&ranking_bfs_list)) {
		p_sw = (ftree_sw_t *) cl_list_remove_head(&ranking_bfs_list);
		p_osm_node = p_sw->p_osm_sw->p_node;

		/* note: skipping port 0 on switches */
		for (i = 1; i < osm_node_get_num_physp(p_osm_node); i++) {
			p_osm_physp = osm_node_get_physp_ptr(p_osm_node, i);
			if (!p_osm_physp  || !osm_link_is_healthy(p_osm_physp))
				continue;

			p_remote_osm_node =
			    osm_node_get_remote_node(p_osm_node, i, NULL);
			if (!p_remote_osm_node)
				continue;

			if (osm_node_get_type(p_remote_osm_node) !=
			    IB_NODE_TYPE_SWITCH)
				continue;

			p_remote_sw = __osm_ftree_fabric_get_sw_by_guid(p_ftree,
									osm_node_get_node_guid
									(p_remote_osm_node));
			CL_ASSERT(p_remote_sw);

			/* if needed, rank the remote switch and add it to the BFS list */
			if (__osm_ftree_sw_update_rank
			    (p_remote_sw, p_sw->rank + 1)) {
				OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
					"Ranking switch 0x%" PRIx64
					" with rank %u\n",
					__osm_ftree_sw_get_guid_ho(p_remote_sw),
					p_remote_sw->rank);
				max_rank = p_remote_sw->rank;
				cl_list_insert_tail(&ranking_bfs_list,
						    p_remote_sw);
			}
		}
		/* done with ports of this switch - go to the next switch in the list */
	}

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
		"Subnet ranking completed. Max Node Rank = %u\n", max_rank);

	/* set FatTree maximal switch rank */
	p_ftree->max_switch_rank = max_rank;

Exit:
	cl_list_destroy(&ranking_bfs_list);
	OSM_LOG_EXIT(&p_ftree->p_osm->log);
	return res;
}				/* __osm_ftree_fabric_rank_from_roots() */

/***************************************************
 ***************************************************/

static int __osm_ftree_fabric_rank_from_hcas(IN ftree_fabric_t * p_ftree)
{
	ftree_hca_t *p_hca;
	ftree_hca_t *p_next_hca;
	cl_list_t ranking_bfs_list;
	int res = 0;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	cl_list_init(&ranking_bfs_list, 10);

	/* Mark REVERSED rank of all the switches in the subnet.
	   Start from switches that are connected to hca's, and
	   scan all the switches in the subnet. */
	p_next_hca = (ftree_hca_t *) cl_qmap_head(&p_ftree->hca_tbl);
	while (p_next_hca != (ftree_hca_t *) cl_qmap_end(&p_ftree->hca_tbl)) {
		p_hca = p_next_hca;
		p_next_hca = (ftree_hca_t *) cl_qmap_next(&p_hca->map_item);
		if (__osm_ftree_rank_leaf_switches
		    (p_ftree, p_hca, &ranking_bfs_list) != 0) {
			res = -1;
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR, "ERR AB14: "
				"Subnet ranking failed - subnet is not FatTree");
			goto Exit;
		}
	}

	/* Now rank rest of the switches in the fabric, while the
	   list already contains all the ranked leaf switches */
	__osm_ftree_rank_switches_from_leafs(p_ftree, &ranking_bfs_list);

	/* fix ranking of the switches by reversing the ranking direction */
	cl_qmap_apply_func(&p_ftree->sw_tbl, __osm_ftree_sw_reverse_rank,
			   (void *)p_ftree);

Exit:
	cl_list_destroy(&ranking_bfs_list);
	OSM_LOG_EXIT(&p_ftree->p_osm->log);
	return res;
}				/* __osm_ftree_fabric_rank_from_hcas() */

/***************************************************
 ***************************************************/

static int __osm_ftree_fabric_rank(IN ftree_fabric_t * p_ftree)
{
	int res = 0;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	if (__osm_ftree_fabric_roots_provided(p_ftree))
		res = __osm_ftree_fabric_rank_from_roots(p_ftree);
	else
		res = __osm_ftree_fabric_rank_from_hcas(p_ftree);

	if (res)
		goto Exit;

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_INFO,
		"FatTree max switch rank is %u\n", p_ftree->max_switch_rank);

Exit:
	OSM_LOG_EXIT(&p_ftree->p_osm->log);
	return res;
}				/* __osm_ftree_fabric_rank() */

/***************************************************
 ***************************************************/

static void __osm_ftree_fabric_set_leaf_rank(IN ftree_fabric_t * p_ftree)
{
	unsigned i;
	ftree_sw_t *p_sw;
	ftree_hca_t *p_hca = NULL;
	ftree_hca_t *p_next_hca;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	if (!__osm_ftree_fabric_roots_provided(p_ftree)) {
		/* If root file is not provided, the fabric has to be pure fat-tree
		   in terms of ranking. Thus, leaf switches rank is the max rank. */
		p_ftree->leaf_switch_rank = p_ftree->max_switch_rank;
	} else {
		/* Find the first CN and set the leaf_switch_rank to the rank
		   of the switch that is connected to this CN. Later we will
		   ensure that all the leaf switches have the same rank. */
		p_next_hca = (ftree_hca_t *) cl_qmap_head(&p_ftree->hca_tbl);
		while (p_next_hca !=
		       (ftree_hca_t *) cl_qmap_end(&p_ftree->hca_tbl)) {
			p_hca = p_next_hca;
			if (p_hca->cn_num)
				break;
			p_next_hca =
			    (ftree_hca_t *) cl_qmap_next(&p_hca->map_item);
		}
		/* we know that there are CNs in the fabric, so just to be sure... */
		CL_ASSERT(p_next_hca !=
			  (ftree_hca_t *) cl_qmap_end(&p_ftree->hca_tbl));

		OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
			"Selected CN port GUID 0x%" PRIx64 "\n",
			__osm_ftree_hca_get_guid_ho(p_hca));

		for (i = 0; (i < p_hca->up_port_groups_num)
		     && (!p_hca->up_port_groups[i]->is_cn); i++) ;
		CL_ASSERT(i < p_hca->up_port_groups_num);
		CL_ASSERT(p_hca->up_port_groups[i]->remote_node_type ==
			  IB_NODE_TYPE_SWITCH);

		p_sw = p_hca->up_port_groups[i]->remote_hca_or_sw.p_sw;
		OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
			"Selected leaf switch GUID 0x%" PRIx64 ", rank %u\n",
			__osm_ftree_sw_get_guid_ho(p_sw), p_sw->rank);
		p_ftree->leaf_switch_rank = p_sw->rank;
	}

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_INFO,
		"FatTree leaf switch rank is %u\n", p_ftree->leaf_switch_rank);
	OSM_LOG_EXIT(&p_ftree->p_osm->log);
}				/* __osm_ftree_fabric_set_leaf_rank() */

/***************************************************
 ***************************************************/

static int __osm_ftree_fabric_populate_ports(IN ftree_fabric_t * p_ftree)
{
	ftree_hca_t *p_hca;
	ftree_hca_t *p_next_hca;
	ftree_sw_t *p_sw;
	ftree_sw_t *p_next_sw;
	int res = 0;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	p_next_hca = (ftree_hca_t *) cl_qmap_head(&p_ftree->hca_tbl);
	while (p_next_hca != (ftree_hca_t *) cl_qmap_end(&p_ftree->hca_tbl)) {
		p_hca = p_next_hca;
		p_next_hca = (ftree_hca_t *) cl_qmap_next(&p_hca->map_item);
		if (__osm_ftree_fabric_construct_hca_ports(p_ftree, p_hca) != 0) {
			res = -1;
			goto Exit;
		}
	}

	p_next_sw = (ftree_sw_t *) cl_qmap_head(&p_ftree->sw_tbl);
	while (p_next_sw != (ftree_sw_t *) cl_qmap_end(&p_ftree->sw_tbl)) {
		p_sw = p_next_sw;
		p_next_sw = (ftree_sw_t *) cl_qmap_next(&p_sw->map_item);
		if (__osm_ftree_fabric_construct_sw_ports(p_ftree, p_sw) != 0) {
			res = -1;
			goto Exit;
		}
	}
Exit:
	OSM_LOG_EXIT(&p_ftree->p_osm->log);
	return res;
}				/* __osm_ftree_fabric_populate_ports() */

/***************************************************
 ***************************************************/
static int add_guid_item_to_list(void *cxt, uint64_t guid, char *p)
{
	cl_qlist_t *list = cxt;
	struct guid_list_item *item;

	item = malloc(sizeof(*item));
	if (!item)
		return -1;

	item->guid = guid;
	cl_qlist_insert_tail(list, &item->list);

	return 0;
}

static int add_guid_item_to_map(void *cxt, uint64_t guid, char *p)
{
	cl_qmap_t *map = cxt;
	name_map_item_t *item;

	item = malloc(sizeof(*item));
	if (!item)
		return -1;

	item->guid = guid;
	cl_qmap_insert(map, guid, &item->item);

	return 0;
}

static int __osm_ftree_fabric_read_guid_files(IN ftree_fabric_t * p_ftree)
{
	int status = 0;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	if (__osm_ftree_fabric_roots_provided(p_ftree)) {
		OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
			"Fetching root nodes from file %s\n",
			p_ftree->p_osm->subn.opt.root_guid_file);

		if (parse_node_map(p_ftree->p_osm->subn.opt.root_guid_file,
				   add_guid_item_to_list,
				   &p_ftree->root_guid_list)) {
			status = -1;
			goto Exit;
		}

		if (!cl_qlist_count(&p_ftree->root_guid_list)) {
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR, "ERR AB22: "
				"Root guids file has no valid guids\n");
			status = -1;
			goto Exit;
		}
	}

	if (__osm_ftree_fabric_cns_provided(p_ftree)) {
		OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_DEBUG,
			"Fetching compute nodes from file %s\n",
			p_ftree->p_osm->subn.opt.cn_guid_file);

		if (parse_node_map(p_ftree->p_osm->subn.opt.cn_guid_file,
				   add_guid_item_to_map,
				   &p_ftree->cn_guid_tbl)) {
			status = -1;
			goto Exit;
		}

		if (!cl_qmap_count(&p_ftree->cn_guid_tbl)) {
			OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_ERROR, "ERR AB23: "
				"Compute node guids file has no valid guids\n");
			status = -1;
			goto Exit;
		}
	}

Exit:
	OSM_LOG_EXIT(&p_ftree->p_osm->log);
	return status;
} /*__osm_ftree_fabric_read_guid_files() */

/***************************************************
 ***************************************************/

static int __osm_ftree_construct_fabric(IN void *context)
{
	ftree_fabric_t *p_ftree = context;
	int status = 0;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	__osm_ftree_fabric_clear(p_ftree);

	if (p_ftree->p_osm->subn.opt.lmc > 0) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"LMC > 0 is not supported by fat-tree routing.\n"
			"Falling back to default routing\n");
		status = -1;
		goto Exit;
	}

	if (cl_qmap_count(&p_ftree->p_osm->subn.sw_guid_tbl) < 2) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"Fabric has %u switches - topology is not fat-tree.\n"
			"Falling back to default routing\n",
			cl_qmap_count(&p_ftree->p_osm->subn.sw_guid_tbl));
		status = -1;
		goto Exit;
	}

	if ((cl_qmap_count(&p_ftree->p_osm->subn.node_guid_tbl) -
	     cl_qmap_count(&p_ftree->p_osm->subn.sw_guid_tbl)) < 2) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"Fabric has %u nodes (%u switches) - topology is not fat-tree.\n"
			"Falling back to default routing\n",
			cl_qmap_count(&p_ftree->p_osm->subn.node_guid_tbl),
			cl_qmap_count(&p_ftree->p_osm->subn.sw_guid_tbl));
		status = -1;
		goto Exit;
	}

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE, "\n"
		"                       |----------------------------------------|\n"
		"                       |- Starting FatTree fabric construction -|\n"
		"                       |----------------------------------------|\n\n");

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
		"Populating FatTree Switch and CA tables\n");
	if (__osm_ftree_fabric_populate_nodes(p_ftree) != 0) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"Fabric topology is not fat-tree - "
			"falling back to default routing\n");
		status = -1;
		goto Exit;
	}

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
		"Reading guid files provided by user\n");
	if (__osm_ftree_fabric_read_guid_files(p_ftree) != 0) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"Failed reading guid files - "
			"falling back to default routing\n");
		status = -1;
		goto Exit;
	}

	if (cl_qmap_count(&p_ftree->hca_tbl) < 2) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"Fabric has %u CAa - topology is not fat-tree.\n"
			"Falling back to default routing\n",
			cl_qmap_count(&p_ftree->hca_tbl));
		status = -1;
		goto Exit;
	}

	/* Rank all the switches in the fabric.
	   After that we will know only fabric max switch rank.
	   We will be able to check leaf switches rank and the
	   whole tree rank after filling ports and marking CNs. */
	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE, "Ranking FatTree\n");
	if (__osm_ftree_fabric_rank(p_ftree) != 0) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"Failed ranking the tree\n");
		status = -1;
		goto Exit;
	}

	/* For each hca and switch, construct array of ports.
	   This is done after the whole FatTree data structure is ready,
	   because we want the ports to have pointers to ftree_{sw,hca}_t
	   objects, and we need the switches to be already ranked because
	   that's how the port direction is determined. */
	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
		"Populating CA & switch ports\n");
	if (__osm_ftree_fabric_populate_ports(p_ftree) != 0) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"Fabric topology is not a fat-tree\n");
		status = -1;
		goto Exit;
	} else if (p_ftree->cn_num == 0) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"Fabric has no valid compute nodes\n");
		status = -1;
		goto Exit;
	}

	/* Now that the CA ports have been created and CNs were marked,
	   we can complete the fabric ranking - set leaf switches rank. */
	__osm_ftree_fabric_set_leaf_rank(p_ftree);

	if (__osm_ftree_fabric_get_rank(p_ftree) > FAT_TREE_MAX_RANK ||
	    __osm_ftree_fabric_get_rank(p_ftree) < FAT_TREE_MIN_RANK) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"Fabric rank is %u (should be between %u and %u)\n",
			__osm_ftree_fabric_get_rank(p_ftree), FAT_TREE_MIN_RANK,
			FAT_TREE_MAX_RANK);
		status = -1;
		goto Exit;
	}

	/* Mark all the switches in the fabric with rank equal to
	   p_ftree->leaf_switch_rank and that are also connected to CNs.
	   As a by-product, this function also runs basic topology
	   validation - it checks that all the CNs are at the same rank. */
	if (__osm_ftree_fabric_mark_leaf_switches(p_ftree)) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"Fabric topology is not a fat-tree\n");
		status = -1;
		goto Exit;
	}

	/* Assign index to all the switches in the fabric.
	   This function also sorts leaf switch array by the switch index,
	   sorts all the port arrays of the indexed switches by remote
	   switch index, and creates switch-by-tuple table (sw_by_tuple_tbl) */
	__osm_ftree_fabric_make_indexing(p_ftree);

	/* Create leaf switch array sorted by index.
	   This array contains switches with rank equal to p_ftree->leaf_switch_rank
	   and that are also connected to CNs (REAL leafs), and it may contain
	   switches at the same leaf rank w/o CNs, if this is the order of indexing.
	   In any case, the first and the last switches in the array are REAL leafs. */
	if (__osm_ftree_fabric_create_leaf_switch_array(p_ftree)) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"Fabric topology is not a fat-tree\n");
		status = -1;
		goto Exit;
	}

	/* calculate and set ftree.max_cn_per_leaf field */
	__osm_ftree_fabric_set_max_cn_per_leaf(p_ftree);

	/* print general info about fabric topology */
	__osm_ftree_fabric_dump_general_info(p_ftree);

	/* dump full tree topology */
	if (osm_log_is_active(&p_ftree->p_osm->log, OSM_LOG_DEBUG))
		__osm_ftree_fabric_dump(p_ftree);

	/* the fabric is required to be PURE fat-tree only if the root
	   guid file hasn't been provided by user */
	if (!__osm_ftree_fabric_roots_provided(p_ftree) &&
	    !__osm_ftree_fabric_validate_topology(p_ftree)) {
		osm_log(&p_ftree->p_osm->log, OSM_LOG_SYS,
			"Fabric topology is not a fat-tree\n");
		status = -1;
		goto Exit;
	}

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
		"Max LID in switch LFTs: %u\n",
		p_ftree->lft_max_lid_ho);

Exit:
	if (status != 0) {
		OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
			"Clearing FatTree Fabric data structures\n");
		__osm_ftree_fabric_clear(p_ftree);
	} else
		p_ftree->fabric_built = TRUE;

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE, "\n"
		"                       |--------------------------------------------------|\n"
		"                       |- Done constructing FatTree fabric (status = %d) -|\n"
		"                       |--------------------------------------------------|\n\n",
		status);

	OSM_LOG_EXIT(&p_ftree->p_osm->log);
	return status;
}				/* __osm_ftree_construct_fabric() */

/***************************************************
 ***************************************************/

static int __osm_ftree_do_routing(IN void *context)
{
	ftree_fabric_t *p_ftree = context;
	int status = 0;

	OSM_LOG_ENTER(&p_ftree->p_osm->log);

	if (!p_ftree->fabric_built) {
		status = -1;
		goto Exit;
	}

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
		"Starting FatTree routing\n");

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
		"Filling switch forwarding tables for Compute Nodes\n");
	__osm_ftree_fabric_route_to_cns(p_ftree);

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
		"Filling switch forwarding tables for non-CN targets\n");
	__osm_ftree_fabric_route_to_non_cns(p_ftree);

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
		"Filling switch forwarding tables for switch-to-switch paths\n");
	__osm_ftree_fabric_route_to_switches(p_ftree);

	/* for each switch, set its fwd table */
	cl_qmap_apply_func(&p_ftree->sw_tbl, __osm_ftree_set_sw_fwd_table,
			   (void *)p_ftree);

	/* write out hca ordering file */
	__osm_ftree_fabric_dump_hca_ordering(p_ftree);

	OSM_LOG(&p_ftree->p_osm->log, OSM_LOG_VERBOSE,
		"FatTree routing is done\n");

Exit:
	OSM_LOG_EXIT(&p_ftree->p_osm->log);
	return status;
}

/***************************************************
 ***************************************************/

static void __osm_ftree_delete(IN void *context)
{
	if (!context)
		return;
	__osm_ftree_fabric_destroy((ftree_fabric_t *) context);
}

/***************************************************
 ***************************************************/

int osm_ucast_ftree_setup(struct osm_routing_engine *r, osm_opensm_t * p_osm)
{
	ftree_fabric_t *p_ftree = __osm_ftree_fabric_create();
	if (!p_ftree)
		return -1;

	p_ftree->p_osm = p_osm;

	r->context = (void *)p_ftree;
	r->build_lid_matrices = __osm_ftree_construct_fabric;
	r->ucast_build_fwd_tables = __osm_ftree_do_routing;
	r->delete = __osm_ftree_delete;

	return 0;
}
