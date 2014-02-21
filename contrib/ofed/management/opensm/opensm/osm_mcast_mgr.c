/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2006 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2008 Xsigo Systems Inc.  All rights reserved.
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
 *    Implementation of osm_mcast_mgr_t.
 * This file implements the Multicast Manager object.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <iba/ib_types.h>
#include <complib/cl_debug.h>
#include <opensm/osm_opensm.h>
#include <opensm/osm_sm.h>
#include <opensm/osm_multicast.h>
#include <opensm/osm_node.h>
#include <opensm/osm_switch.h>
#include <opensm/osm_helper.h>
#include <opensm/osm_msgdef.h>

/**********************************************************************
 **********************************************************************/
typedef struct osm_mcast_work_obj {
	cl_list_item_t list_item;
	osm_port_t *p_port;
} osm_mcast_work_obj_t;

/**********************************************************************
 **********************************************************************/
static osm_mcast_work_obj_t *__osm_mcast_work_obj_new(IN const osm_port_t *
						      const p_port)
{
	/*
	   TO DO - get these objects from a lockpool.
	 */
	osm_mcast_work_obj_t *p_obj;

	/*
	   clean allocated memory to avoid assertion when trying to insert to
	   qlist.
	   see cl_qlist_insert_tail(): CL_ASSERT(p_list_item->p_list != p_list)
	 */
	p_obj = malloc(sizeof(*p_obj));
	if (p_obj) {
		memset(p_obj, 0, sizeof(*p_obj));
		p_obj->p_port = (osm_port_t *) p_port;
	}

	return (p_obj);
}

/**********************************************************************
 **********************************************************************/
static void __osm_mcast_work_obj_delete(IN osm_mcast_work_obj_t * p_wobj)
{
	free(p_wobj);
}

/**********************************************************************
 Recursively remove nodes from the tree
 *********************************************************************/
static void __osm_mcast_mgr_purge_tree_node(IN osm_mtree_node_t * p_mtn)
{
	uint8_t i;

	for (i = 0; i < p_mtn->max_children; i++) {
		if (p_mtn->child_array[i] &&
		    (p_mtn->child_array[i] != OSM_MTREE_LEAF))
			__osm_mcast_mgr_purge_tree_node(p_mtn->child_array[i]);

		p_mtn->child_array[i] = NULL;

	}

	free(p_mtn);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_mcast_mgr_purge_tree(osm_sm_t * sm, IN osm_mgrp_t * const p_mgrp)
{
	OSM_LOG_ENTER(sm->p_log);

	if (p_mgrp->p_root)
		__osm_mcast_mgr_purge_tree_node(p_mgrp->p_root);

	p_mgrp->p_root = NULL;

	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 **********************************************************************/
static float
osm_mcast_mgr_compute_avg_hops(osm_sm_t * sm,
			       const osm_mgrp_t * const p_mgrp,
			       const osm_switch_t * const p_sw)
{
	float avg_hops = 0;
	uint32_t hops = 0;
	uint32_t num_ports = 0;
	const osm_port_t *p_port;
	const osm_mcm_port_t *p_mcm_port;
	const cl_qmap_t *p_mcm_tbl;

	OSM_LOG_ENTER(sm->p_log);

	p_mcm_tbl = &p_mgrp->mcm_port_tbl;

	/*
	   For each member of the multicast group, compute the
	   number of hops to its base LID.
	 */
	for (p_mcm_port = (osm_mcm_port_t *) cl_qmap_head(p_mcm_tbl);
	     p_mcm_port != (osm_mcm_port_t *) cl_qmap_end(p_mcm_tbl);
	     p_mcm_port =
	     (osm_mcm_port_t *) cl_qmap_next(&p_mcm_port->map_item)) {
		/*
		   Acquire the port object for this port guid, then create
		   the new worker object to build the list.
		 */
		p_port = osm_get_port_by_guid(sm->p_subn,
					      ib_gid_get_guid(&p_mcm_port->
							      port_gid));

		if (!p_port) {
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A18: "
				"No port object for port 0x%016" PRIx64 "\n",
				cl_ntoh64(ib_gid_get_guid
					  (&p_mcm_port->port_gid)));
			continue;
		}

		hops += osm_switch_get_port_least_hops(p_sw, p_port);
		num_ports++;
	}

	/*
	   We should be here if there aren't any ports in the group.
	 */
	CL_ASSERT(num_ports);

	if (num_ports != 0)
		avg_hops = (float)(hops / num_ports);

	OSM_LOG_EXIT(sm->p_log);
	return (avg_hops);
}

/**********************************************************************
 Calculate the maximal "min hops" from the given switch to any
 of the group HCAs
 **********************************************************************/
static float
osm_mcast_mgr_compute_max_hops(osm_sm_t * sm,
			       const osm_mgrp_t * const p_mgrp,
			       const osm_switch_t * const p_sw)
{
	uint32_t max_hops = 0;
	uint32_t hops = 0;
	const osm_port_t *p_port;
	const osm_mcm_port_t *p_mcm_port;
	const cl_qmap_t *p_mcm_tbl;

	OSM_LOG_ENTER(sm->p_log);

	p_mcm_tbl = &p_mgrp->mcm_port_tbl;

	/*
	   For each member of the multicast group, compute the
	   number of hops to its base LID.
	 */
	for (p_mcm_port = (osm_mcm_port_t *) cl_qmap_head(p_mcm_tbl);
	     p_mcm_port != (osm_mcm_port_t *) cl_qmap_end(p_mcm_tbl);
	     p_mcm_port =
	     (osm_mcm_port_t *) cl_qmap_next(&p_mcm_port->map_item)) {
		/*
		   Acquire the port object for this port guid, then create
		   the new worker object to build the list.
		 */
		p_port = osm_get_port_by_guid(sm->p_subn,
					      ib_gid_get_guid(&p_mcm_port->
							      port_gid));

		if (!p_port) {
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A1A: "
				"No port object for port 0x%016" PRIx64 "\n",
				cl_ntoh64(ib_gid_get_guid
					  (&p_mcm_port->port_gid)));
			continue;
		}

		hops = osm_switch_get_port_least_hops(p_sw, p_port);
		if (hops > max_hops)
			max_hops = hops;
	}

	if (max_hops == 0) {
		/*
		   We should be here if there aren't any ports in the group.
		 */
		max_hops = 10001;	/* see later - we use it to realize no hops */
	}

	OSM_LOG_EXIT(sm->p_log);
	return (float)(max_hops);
}

/**********************************************************************
   This function attempts to locate the optimal switch for the
   center of the spanning tree.  The current algorithm chooses
   a switch with the lowest average hop count to the members
   of the multicast group.
**********************************************************************/
static osm_switch_t *__osm_mcast_mgr_find_optimal_switch(osm_sm_t * sm,
							 const osm_mgrp_t *
							 const p_mgrp)
{
	cl_qmap_t *p_sw_tbl;
	const osm_switch_t *p_sw;
	const osm_switch_t *p_best_sw = NULL;
	float hops = 0;
	float best_hops = 10000;	/* any big # will do */
#ifdef OSM_VENDOR_INTF_ANAFA
	boolean_t use_avg_hops = TRUE;	/* anafa2 - bug hca on switch *//* use max hops for root */
#else
	boolean_t use_avg_hops = FALSE;	/* use max hops for root */
#endif

	OSM_LOG_ENTER(sm->p_log);

	p_sw_tbl = &sm->p_subn->sw_guid_tbl;

	CL_ASSERT(!osm_mgrp_is_empty(p_mgrp));

	for (p_sw = (osm_switch_t *) cl_qmap_head(p_sw_tbl);
	     p_sw != (osm_switch_t *) cl_qmap_end(p_sw_tbl);
	     p_sw = (osm_switch_t *) cl_qmap_next(&p_sw->map_item)) {
		if (!osm_switch_supports_mcast(p_sw))
			continue;

		if (use_avg_hops)
			hops = osm_mcast_mgr_compute_avg_hops(sm, p_mgrp, p_sw);
		else
			hops = osm_mcast_mgr_compute_max_hops(sm, p_mgrp, p_sw);

		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Switch 0x%016" PRIx64 ", hops = %f\n",
			cl_ntoh64(osm_node_get_node_guid(p_sw->p_node)), hops);

		if (hops < best_hops) {
			p_best_sw = p_sw;
			best_hops = hops;
		}
	}

	if (p_best_sw)
		OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
			"Best switch is 0x%" PRIx64 ", hops = %f\n",
			cl_ntoh64(osm_node_get_node_guid(p_best_sw->p_node)),
			best_hops);
	else
		OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
			"No multicast capable switches detected\n");

	OSM_LOG_EXIT(sm->p_log);
	return ((osm_switch_t *) p_best_sw);
}

/**********************************************************************
   This function returns the existing or optimal root swtich for the tree.
**********************************************************************/
static osm_switch_t *__osm_mcast_mgr_find_root_switch(osm_sm_t * sm,
						      const osm_mgrp_t *
						      const p_mgrp)
{
	const osm_switch_t *p_sw = NULL;

	OSM_LOG_ENTER(sm->p_log);

	/*
	   We always look for the best multicast tree root switch.
	   Otherwise since we always start with a a single join
	   the root will be always on the first switch attached to it.
	   - Very bad ...
	 */
	p_sw = __osm_mcast_mgr_find_optimal_switch(sm, p_mgrp);

	OSM_LOG_EXIT(sm->p_log);
	return ((osm_switch_t *) p_sw);
}

/**********************************************************************
 **********************************************************************/
static osm_signal_t
__osm_mcast_mgr_set_tbl(osm_sm_t * sm, IN osm_switch_t * const p_sw)
{
	osm_node_t *p_node;
	osm_dr_path_t *p_path;
	osm_madw_context_t mad_context;
	ib_api_status_t status;
	uint32_t block_id_ho = 0;
	int16_t block_num = 0;
	uint32_t position = 0;
	uint32_t max_position;
	osm_mcast_tbl_t *p_tbl;
	ib_net16_t block[IB_MCAST_BLOCK_SIZE];
	osm_signal_t signal = OSM_SIGNAL_DONE;

	CL_ASSERT(sm);

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_sw);

	p_node = p_sw->p_node;

	CL_ASSERT(p_node);

	p_path = osm_physp_get_dr_path_ptr(osm_node_get_physp_ptr(p_node, 0));

	/*
	   Send multicast forwarding table blocks to the switch
	   as long as the switch indicates it has blocks needing
	   configuration.
	 */

	mad_context.mft_context.node_guid = osm_node_get_node_guid(p_node);
	mad_context.mft_context.set_method = TRUE;

	p_tbl = osm_switch_get_mcast_tbl_ptr(p_sw);
	max_position = p_tbl->max_position;

	while (osm_mcast_tbl_get_block(p_tbl, block_num,
				       (uint8_t) position, block)) {
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Writing MFT block 0x%X\n", block_id_ho);

		block_id_ho = block_num + (position << 28);

		status = osm_req_set(sm, p_path, (void *)block, sizeof(block),
				     IB_MAD_ATTR_MCAST_FWD_TBL,
				     cl_hton32(block_id_ho),
				     CL_DISP_MSGID_NONE, &mad_context);

		if (status != IB_SUCCESS) {
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A02: "
				"Sending multicast fwd. tbl. block failed (%s)\n",
				ib_get_err_str(status));
		}

		signal = OSM_SIGNAL_DONE_PENDING;

		if (++position > max_position) {
			position = 0;
			block_num++;
		}
	}

	OSM_LOG_EXIT(sm->p_log);
	return (signal);
}

/**********************************************************************
  This is part of the recursive function to compute the paths in the
  spanning tree that eminate from this switch.  On input, the p_list
  contains the group members that must be routed from this switch.
**********************************************************************/
static void
__osm_mcast_mgr_subdivide(osm_sm_t * sm,
			  osm_mgrp_t * const p_mgrp,
			  osm_switch_t * const p_sw,
			  cl_qlist_t * const p_list,
			  cl_qlist_t * const list_array,
			  uint8_t const array_size)
{
	uint8_t port_num;
	uint16_t mlid_ho;
	boolean_t ignore_existing;
	osm_mcast_work_obj_t *p_wobj;

	OSM_LOG_ENTER(sm->p_log);

	mlid_ho = cl_ntoh16(osm_mgrp_get_mlid(p_mgrp));

	/*
	   For Multicast Groups, we want not to count on previous
	   configurations - since we can easily generate a storm
	   by loops.
	 */
	ignore_existing = TRUE;

	/*
	   Subdivide the set of ports into non-overlapping subsets
	   that will be routed to other switches.
	 */
	while ((p_wobj =
		(osm_mcast_work_obj_t *) cl_qlist_remove_head(p_list)) !=
	       (osm_mcast_work_obj_t *) cl_qlist_end(p_list)) {
		port_num =
		    osm_switch_recommend_mcast_path(p_sw, p_wobj->p_port,
						    mlid_ho, ignore_existing);

		if (port_num == OSM_NO_PATH) {
			/*
			   This typically occurs if the switch does not support
			   multicast and the multicast tree must branch at this
			   switch.
			 */
			uint64_t node_guid_ho =
			    cl_ntoh64(osm_node_get_node_guid(p_sw->p_node));
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A03: "
				"Error routing MLID 0x%X through switch 0x%"
				PRIx64 "\n"
				"\t\t\t\tNo multicast paths from this switch for port "
				"with LID %u\n", mlid_ho, node_guid_ho,
				cl_ntoh16(osm_port_get_base_lid
					  (p_wobj->p_port)));

			__osm_mcast_work_obj_delete(p_wobj);
			continue;
		}

		if (port_num > array_size) {
			uint64_t node_guid_ho =
			    cl_ntoh64(osm_node_get_node_guid(p_sw->p_node));
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A04: "
				"Error routing MLID 0x%X through switch 0x%"
				PRIx64 "\n"
				"\t\t\t\tNo multicast paths from this switch to port "
				"with LID %u\n", mlid_ho, node_guid_ho,
				cl_ntoh16(osm_port_get_base_lid
					  (p_wobj->p_port)));

			__osm_mcast_work_obj_delete(p_wobj);

			/* This is means OpenSM has a bug. */
			CL_ASSERT(FALSE);
			continue;
		}

		cl_qlist_insert_tail(&list_array[port_num], &p_wobj->list_item);
	}

	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 **********************************************************************/
static void __osm_mcast_mgr_purge_list(osm_sm_t * sm, cl_qlist_t * const p_list)
{
	osm_mcast_work_obj_t *p_wobj;

	OSM_LOG_ENTER(sm->p_log);

	while ((p_wobj = (osm_mcast_work_obj_t *) cl_qlist_remove_head(p_list))
	       != (osm_mcast_work_obj_t *) cl_qlist_end(p_list)) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A06: "
			"Unable to route for port 0x%" PRIx64 "\n",
			osm_port_get_guid(p_wobj->p_port));
		__osm_mcast_work_obj_delete(p_wobj);
	}

	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
  This is the recursive function to compute the paths in the spanning
  tree that emanate from this switch.  On input, the p_list contains
  the group members that must be routed from this switch.

  The function returns the newly created mtree node element.
**********************************************************************/
static osm_mtree_node_t *__osm_mcast_mgr_branch(osm_sm_t * sm,
						osm_mgrp_t * const p_mgrp,
						osm_switch_t * const p_sw,
						cl_qlist_t * const p_list,
						uint8_t depth,
						uint8_t const upstream_port,
						uint8_t * const p_max_depth)
{
	uint8_t max_children;
	osm_mtree_node_t *p_mtn = NULL;
	cl_qlist_t *list_array = NULL;
	uint8_t i;
	ib_net64_t node_guid;
	uint64_t node_guid_ho;
	osm_mcast_work_obj_t *p_wobj;
	cl_qlist_t *p_port_list;
	size_t count;
	uint16_t mlid_ho;
	osm_mcast_tbl_t *p_tbl;

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_sw);
	CL_ASSERT(p_list);
	CL_ASSERT(p_max_depth);

	node_guid = osm_node_get_node_guid(p_sw->p_node);
	node_guid_ho = cl_ntoh64(node_guid);
	mlid_ho = cl_ntoh16(osm_mgrp_get_mlid(p_mgrp));

	OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
		"Routing MLID 0x%X through switch 0x%" PRIx64
		", %u nodes at depth %u\n",
		mlid_ho, node_guid_ho, cl_qlist_count(p_list), depth);

	CL_ASSERT(cl_qlist_count(p_list) > 0);

	depth++;

	if (depth >= 64) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR,
			"Maximal hops number is reached for MLID 0x%x."
			" Break processing.", mlid_ho);
		__osm_mcast_mgr_purge_list(sm, p_list);
		goto Exit;
	}

	if (depth > *p_max_depth) {
		CL_ASSERT(depth == *p_max_depth + 1);
		*p_max_depth = depth;
	}

	if (osm_switch_supports_mcast(p_sw) == FALSE) {
		/*
		   This switch doesn't do multicast.  Clean-up.
		 */
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A14: "
			"Switch 0x%" PRIx64 " does not support multicast\n",
			node_guid_ho);

		/*
		   Deallocate all the work objects on this branch of the tree.
		 */
		__osm_mcast_mgr_purge_list(sm, p_list);
		goto Exit;
	}

	p_mtn = osm_mtree_node_new(p_sw);
	if (p_mtn == NULL) {
		/*
		   We are unable to continue routing down this
		   leg of the tree.  Clean-up.
		 */
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A15: "
			"Insufficient memory to build multicast tree\n");

		/*
		   Deallocate all the work objects on this branch of the tree.
		 */
		__osm_mcast_mgr_purge_list(sm, p_list);
		goto Exit;
	}

	max_children = osm_mtree_node_get_max_children(p_mtn);

	CL_ASSERT(max_children > 1);

	/*
	   Prepare an empty list for each port in the switch.
	   TO DO - this list array could probably be moved
	   inside the switch element to save on malloc thrashing.
	 */
	list_array = malloc(sizeof(cl_qlist_t) * max_children);
	if (list_array == NULL) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A16: "
			"Unable to allocate list array\n");
		__osm_mcast_mgr_purge_list(sm, p_list);
		goto Exit;
	}

	memset(list_array, 0, sizeof(cl_qlist_t) * max_children);

	for (i = 0; i < max_children; i++)
		cl_qlist_init(&list_array[i]);

	__osm_mcast_mgr_subdivide(sm, p_mgrp, p_sw, p_list, list_array,
				  max_children);

	p_tbl = osm_switch_get_mcast_tbl_ptr(p_sw);

	/*
	   Add the upstream port to the forwarding table unless
	   we're at the root of the spanning tree.
	 */
	if (depth > 1) {
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Adding upstream port %u\n", upstream_port);

		CL_ASSERT(upstream_port);
		osm_mcast_tbl_set(p_tbl, mlid_ho, upstream_port);
	}

	/*
	   For each port that was allocated some routes,
	   recurse into this function to continue building the tree
	   if the node on the other end of that port is another switch.
	   Otherwise, the node is an endpoint, and we've found a leaf
	   of the tree.  Mark leaves with our special pointer value.
	 */

	for (i = 0; i < max_children; i++) {
		const osm_physp_t *p_physp;
		const osm_physp_t *p_remote_physp;
		osm_node_t *p_node;
		const osm_node_t *p_remote_node;

		p_port_list = &list_array[i];

		count = cl_qlist_count(p_port_list);

		/*
		   There should be no children routed through the upstream port!
		 */
		CL_ASSERT((upstream_port == 0) || (i != upstream_port) ||
			  ((i == upstream_port) && (count == 0)));

		if (count == 0)
			continue;	/* No routes down this port. */

		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Routing %zu destinations via switch port %u\n",
			count, i);

		/*
		   This port routes frames for this mcast group.  Therefore,
		   set the appropriate bit in the multicast forwarding
		   table for this switch.
		 */
		osm_mcast_tbl_set(p_tbl, mlid_ho, i);
		if (i == 0) {
			/* This means we are adding the switch to the MC group.
			   We do not need to continue looking at the remote port, just
			   needed to add the port to the table */
			CL_ASSERT(count == 1);

			p_wobj = (osm_mcast_work_obj_t *)
			    cl_qlist_remove_head(p_port_list);
			__osm_mcast_work_obj_delete(p_wobj);
			continue;
		}

		p_node = p_sw->p_node;
		p_remote_node = osm_node_get_remote_node(p_node, i, NULL);
		if (!p_remote_node)
			continue;

		if (osm_node_get_type(p_remote_node) == IB_NODE_TYPE_SWITCH) {
			/*
			   Acquire a pointer to the remote switch then recurse.
			 */
			CL_ASSERT(p_remote_node->sw);

			p_physp = osm_node_get_physp_ptr(p_node, i);
			CL_ASSERT(p_physp);

			p_remote_physp = osm_physp_get_remote(p_physp);
			CL_ASSERT(p_remote_physp);

			p_mtn->child_array[i] =
			    __osm_mcast_mgr_branch(sm, p_mgrp,
						   p_remote_node->sw,
						   p_port_list, depth,
						   osm_physp_get_port_num
						   (p_remote_physp),
						   p_max_depth);
		} else {
			/*
			   The neighbor node is not a switch, so this
			   must be a leaf.
			 */
			CL_ASSERT(count == 1);

			p_mtn->child_array[i] = OSM_MTREE_LEAF;
			p_wobj = (osm_mcast_work_obj_t *)
			    cl_qlist_remove_head(p_port_list);

			CL_ASSERT(cl_is_qlist_empty(p_port_list));

			OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
				"Found leaf for port 0x%016" PRIx64
				" on switch port %u\n",
				cl_ntoh64(osm_port_get_guid(p_wobj->p_port)),
				i);

			__osm_mcast_work_obj_delete(p_wobj);
		}
	}

	free(list_array);
Exit:
	OSM_LOG_EXIT(sm->p_log);
	return (p_mtn);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
__osm_mcast_mgr_build_spanning_tree(osm_sm_t * sm, osm_mgrp_t * const p_mgrp)
{
	const cl_qmap_t *p_mcm_tbl;
	const osm_port_t *p_port;
	const osm_mcm_port_t *p_mcm_port;
	uint32_t num_ports;
	cl_qlist_t port_list;
	osm_switch_t *p_sw;
	osm_mcast_work_obj_t *p_wobj;
	ib_api_status_t status = IB_SUCCESS;
	uint8_t max_depth = 0;
	uint32_t count;

	OSM_LOG_ENTER(sm->p_log);

	cl_qlist_init(&port_list);

	/*
	   TO DO - for now, just blow away the old tree.
	   In the future we'll need to construct the tree based
	   on multicast forwarding table information if the user wants to
	   preserve existing multicast routes.
	 */
	__osm_mcast_mgr_purge_tree(sm, p_mgrp);

	p_mcm_tbl = &p_mgrp->mcm_port_tbl;
	num_ports = cl_qmap_count(p_mcm_tbl);
	if (num_ports == 0) {
		OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
			"MLID 0x%X has no members - nothing to do\n",
			cl_ntoh16(osm_mgrp_get_mlid(p_mgrp)));
		goto Exit;
	}

	/*
	   This function builds the single spanning tree recursively.
	   At each stage, the ports to be reached are divided into
	   non-overlapping subsets of member ports that can be reached through
	   a given switch port.  Construction then moves down each
	   branch, and the process starts again with each branch computing
	   for its own subset of the member ports.

	   The maximum recursion depth is at worst the maximum hop count in the
	   subnet, which is spec limited to 64.
	 */

	/*
	   Locate the switch around which to create the spanning
	   tree for this multicast group.
	 */
	p_sw = __osm_mcast_mgr_find_root_switch(sm, p_mgrp);
	if (p_sw == NULL) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A08: "
			"Unable to locate a suitable switch for group 0x%X\n",
			cl_ntoh16(osm_mgrp_get_mlid(p_mgrp)));
		status = IB_ERROR;
		goto Exit;
	}

	/*
	   Build the first "subset" containing all member ports.
	 */
	for (p_mcm_port = (osm_mcm_port_t *) cl_qmap_head(p_mcm_tbl);
	     p_mcm_port != (osm_mcm_port_t *) cl_qmap_end(p_mcm_tbl);
	     p_mcm_port =
	     (osm_mcm_port_t *) cl_qmap_next(&p_mcm_port->map_item)) {
		/*
		   Acquire the port object for this port guid, then create
		   the new worker object to build the list.
		 */
		p_port = osm_get_port_by_guid(sm->p_subn,
					      ib_gid_get_guid(&p_mcm_port->
							      port_gid));
		if (!p_port) {
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A09: "
				"No port object for port 0x%016" PRIx64 "\n",
				cl_ntoh64(ib_gid_get_guid
					  (&p_mcm_port->port_gid)));
			continue;
		}

		p_wobj = __osm_mcast_work_obj_new(p_port);
		if (p_wobj == NULL) {
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A10: "
				"Insufficient memory to route port 0x%016"
				PRIx64 "\n",
				cl_ntoh64(osm_port_get_guid(p_port)));
			continue;
		}

		cl_qlist_insert_tail(&port_list, &p_wobj->list_item);
	}

	count = cl_qlist_count(&port_list);
	p_mgrp->p_root = __osm_mcast_mgr_branch(sm, p_mgrp, p_sw,
						&port_list, 0, 0, &max_depth);

	OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
		"Configured MLID 0x%X for %u ports, max tree depth = %u\n",
		cl_ntoh16(osm_mgrp_get_mlid(p_mgrp)), count, max_depth);

Exit:
	OSM_LOG_EXIT(sm->p_log);
	return (status);
}

#if 0
/* unused */
/**********************************************************************
 **********************************************************************/
void
osm_mcast_mgr_set_table(osm_sm_t * sm,
			IN const osm_mgrp_t * const p_mgrp,
			IN const osm_mtree_node_t * const p_mtn)
{
	uint8_t i;
	uint8_t max_children;
	osm_mtree_node_t *p_child_mtn;
	uint16_t mlid_ho;
	osm_mcast_tbl_t *p_tbl;
	osm_switch_t *p_sw;

	OSM_LOG_ENTER(sm->p_log);

	mlid_ho = cl_ntoh16(osm_mgrp_get_mlid(p_mgrp));
	p_sw = osm_mtree_node_get_switch_ptr(p_mtn);

	CL_ASSERT(p_sw);

	OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
		"Configuring MLID 0x%X on switch 0x%" PRIx64 "\n",
		mlid_ho, osm_node_get_node_guid(p_sw->p_node));

	/*
	   For every child of this tree node, set the corresponding
	   bit in the switch's mcast table.
	 */
	p_tbl = osm_switch_get_mcast_tbl_ptr(p_sw);
	max_children = osm_mtree_node_get_max_children(p_mtn);

	CL_ASSERT(max_children <= osm_switch_get_num_ports(p_sw));

	osm_mcast_tbl_clear_mlid(p_tbl, mlid_ho);

	for (i = 0; i < max_children; i++) {
		p_child_mtn = osm_mtree_node_get_child(p_mtn, i);
		if (p_child_mtn == NULL)
			continue;

		osm_mcast_tbl_set(p_tbl, mlid_ho, i);
	}

	OSM_LOG_EXIT(sm->p_log);
}
#endif

/**********************************************************************
 **********************************************************************/
static void __osm_mcast_mgr_clear(osm_sm_t * sm, IN osm_mgrp_t * const p_mgrp)
{
	osm_switch_t *p_sw;
	cl_qmap_t *p_sw_tbl;
	osm_mcast_tbl_t *p_mcast_tbl;

	OSM_LOG_ENTER(sm->p_log);

	/*
	   Walk the switches and clear the routing entries for
	   this MLID.
	 */
	p_sw_tbl = &sm->p_subn->sw_guid_tbl;
	p_sw = (osm_switch_t *) cl_qmap_head(p_sw_tbl);
	while (p_sw != (osm_switch_t *) cl_qmap_end(p_sw_tbl)) {
		p_mcast_tbl = osm_switch_get_mcast_tbl_ptr(p_sw);
		osm_mcast_tbl_clear_mlid(p_mcast_tbl, cl_ntoh16(p_mgrp->mlid));
		p_sw = (osm_switch_t *) cl_qmap_next(&p_sw->map_item);
	}

	OSM_LOG_EXIT(sm->p_log);
}

#if 0
/* TO DO - make this real -- at least update spanning tree */
/**********************************************************************
   Lock must be held on entry.
**********************************************************************/
ib_api_status_t
osm_mcast_mgr_process_single(osm_sm_t * sm,
			     IN ib_net16_t const mlid,
			     IN ib_net64_t const port_guid,
			     IN uint8_t const join_state)
{
	uint8_t port_num;
	uint16_t mlid_ho;
	ib_net64_t sw_guid;
	osm_port_t *p_port;
	osm_physp_t *p_physp;
	osm_physp_t *p_remote_physp;
	osm_node_t *p_remote_node;
	osm_mcast_tbl_t *p_mcast_tbl;
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(mlid);
	CL_ASSERT(port_guid);

	mlid_ho = cl_ntoh16(mlid);

	OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
		"Attempting to add port 0x%" PRIx64 " to MLID 0x%X, "
		"\n\t\t\t\tjoin state = 0x%X\n",
		cl_ntoh64(port_guid), mlid_ho, join_state);

	/*
	   Acquire the Port object.
	 */
	p_port = osm_get_port_by_guid(sm->p_subn, port_guid);
	if (!p_port) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A01: "
			"Unable to acquire port object for 0x%" PRIx64 "\n",
			cl_ntoh64(port_guid));
		status = IB_ERROR;
		goto Exit;
	}

	p_physp = p_port->p_physp;
	if (p_physp == NULL) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A05: "
			"Unable to acquire phsyical port object for 0x%" PRIx64
			"\n", cl_ntoh64(port_guid));
		status = IB_ERROR;
		goto Exit;
	}

	p_remote_physp = osm_physp_get_remote(p_physp);
	if (p_remote_physp == NULL) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A11: "
			"Unable to acquire remote phsyical port object "
			"for 0x%" PRIx64 "\n", cl_ntoh64(port_guid));
		status = IB_ERROR;
		goto Exit;
	}

	p_remote_node = osm_physp_get_node_ptr(p_remote_physp);

	CL_ASSERT(p_remote_node);

	sw_guid = osm_node_get_node_guid(p_remote_node);

	if (osm_node_get_type(p_remote_node) != IB_NODE_TYPE_SWITCH) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A22: "
			"Remote node not a switch node 0x%" PRIx64 "\n",
			cl_ntoh64(sw_guid));
		status = IB_ERROR;
		goto Exit;
	}

	if (!p_remote_node->sw) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A12: "
			"No switch object 0x%" PRIx64 "\n", cl_ntoh64(sw_guid));
		status = IB_ERROR;
		goto Exit;
	}

	if (osm_switch_is_in_mcast_tree(p_remote_node->sw, mlid_ho)) {
		/*
		   We're in luck. The switch attached to this port
		   is already in the multicast group, so we can just
		   add the specified port as a new leaf of the tree.
		 */
		if (join_state & (IB_JOIN_STATE_FULL | IB_JOIN_STATE_NON)) {
			/*
			   This node wants to receive multicast frames.
			   Get the switch port number to which the new member port
			   is attached, then configure this single mcast table.
			 */
			port_num = osm_physp_get_port_num(p_remote_physp);
			CL_ASSERT(port_num);

			p_mcast_tbl =
			    osm_switch_get_mcast_tbl_ptr(p_remote_node->sw);
			osm_mcast_tbl_set(p_mcast_tbl, mlid_ho, port_num);
		} else {
			if (join_state & IB_JOIN_STATE_SEND_ONLY)
				OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
					"Success.  Nothing to do for send"
					"only member\n");
			else {
				OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A13: "
					"Unknown join state 0x%X\n",
					join_state);
				status = IB_ERROR;
				goto Exit;
			}
		}
	} else
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG, "Unable to add port\n");

Exit:
	OSM_LOG_EXIT(sm->p_log);
	return (status);
}
#endif

/**********************************************************************
   lock must already be held on entry
**********************************************************************/
static ib_api_status_t
osm_mcast_mgr_process_tree(osm_sm_t * sm,
			   IN osm_mgrp_t * const p_mgrp,
			   IN osm_mcast_req_type_t req_type,
			   ib_net64_t port_guid)
{
	ib_api_status_t status = IB_SUCCESS;
	ib_net16_t mlid;

	OSM_LOG_ENTER(sm->p_log);

	mlid = osm_mgrp_get_mlid(p_mgrp);

	OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
		"Processing multicast group 0x%X\n", cl_ntoh16(mlid));

	/*
	   If there are no switches in the subnet, then we have nothing to do.
	 */
	if (cl_qmap_count(&sm->p_subn->sw_guid_tbl) == 0) {
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"No switches in subnet. Nothing to do\n");
		goto Exit;
	}

	/*
	   Clear the multicast tables to start clean, then build
	   the spanning tree which sets the mcast table bits for each
	   port in the group.
	 */
	__osm_mcast_mgr_clear(sm, p_mgrp);

	if (!p_mgrp->full_members)
		goto Exit;

	status = __osm_mcast_mgr_build_spanning_tree(sm, p_mgrp);
	if (status != IB_SUCCESS) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A17: "
			"Unable to create spanning tree (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(sm->p_log);
	return (status);
}

/**********************************************************************
 Process the entire group.
 NOTE : The lock should be held externally!
 **********************************************************************/
static ib_api_status_t
mcast_mgr_process_mgrp(osm_sm_t * sm,
		       IN osm_mgrp_t * const p_mgrp,
		       IN osm_mcast_req_type_t req_type,
		       IN ib_net64_t port_guid)
{
	ib_api_status_t status;

	OSM_LOG_ENTER(sm->p_log);

	status = osm_mcast_mgr_process_tree(sm, p_mgrp, req_type, port_guid);
	if (status != IB_SUCCESS) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0A19: "
			"Unable to create spanning tree (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}
	p_mgrp->last_tree_id = p_mgrp->last_change_id;

	/* remove MCGRP if it is marked for deletion */
	if (p_mgrp->to_be_deleted) {
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Destroying mgrp with lid:0x%x\n",
			cl_ntoh16(p_mgrp->mlid));
		sm->p_subn->mgroups[cl_ntoh16(p_mgrp->mlid) - IB_LID_MCAST_START_HO] = NULL;
		osm_mgrp_delete(p_mgrp);
	}

Exit:
	OSM_LOG_EXIT(sm->p_log);
	return status;
}

/**********************************************************************
 **********************************************************************/
osm_signal_t osm_mcast_mgr_process(osm_sm_t * sm)
{
	osm_signal_t signal;
	osm_switch_t *p_sw;
	cl_qmap_t *p_sw_tbl;
	cl_qlist_t *p_list = &sm->mgrp_list;
	osm_mgrp_t *p_mgrp;
	boolean_t pending_transactions = FALSE;
	int i;

	OSM_LOG_ENTER(sm->p_log);

	p_sw_tbl = &sm->p_subn->sw_guid_tbl;
	/*
	   While holding the lock, iterate over all the established
	   multicast groups, servicing each in turn.

	   Then, download the multicast tables to the switches.
	 */
	CL_PLOCK_EXCL_ACQUIRE(sm->p_lock);

	for (i = 0; i <= sm->p_subn->max_mcast_lid_ho - IB_LID_MCAST_START_HO;
	     i++) {
		/*
		   We reached here due to some change that caused a heavy sweep
		   of the subnet. Not due to a specific multicast request.
		   So the request type is subnet_change and the port guid is 0.
		 */
		p_mgrp = sm->p_subn->mgroups[i];
		if (p_mgrp)
			mcast_mgr_process_mgrp(sm, p_mgrp,
					       OSM_MCAST_REQ_TYPE_SUBNET_CHANGE,
					       0);
	}

	/*
	   Walk the switches and download the tables for each.
	 */
	p_sw = (osm_switch_t *) cl_qmap_head(p_sw_tbl);
	while (p_sw != (osm_switch_t *) cl_qmap_end(p_sw_tbl)) {
		signal = __osm_mcast_mgr_set_tbl(sm, p_sw);
		if (signal == OSM_SIGNAL_DONE_PENDING)
			pending_transactions = TRUE;
		p_sw = (osm_switch_t *) cl_qmap_next(&p_sw->map_item);
	}

	while (!cl_is_qlist_empty(p_list)) {
		cl_list_item_t *p = cl_qlist_remove_head(p_list);
		free(p);
	}

	CL_PLOCK_RELEASE(sm->p_lock);

	OSM_LOG_EXIT(sm->p_log);

	if (pending_transactions == TRUE)
		return (OSM_SIGNAL_DONE_PENDING);
	else
		return (OSM_SIGNAL_DONE);
}

/**********************************************************************
  This is the function that is invoked during idle time to handle the
  process request for mcast groups where join/leave/delete was required.
 **********************************************************************/
osm_signal_t osm_mcast_mgr_process_mgroups(osm_sm_t * sm)
{
	cl_qlist_t *p_list = &sm->mgrp_list;
	osm_switch_t *p_sw;
	cl_qmap_t *p_sw_tbl;
	osm_mgrp_t *p_mgrp;
	ib_net16_t mlid;
	osm_signal_t ret, signal = OSM_SIGNAL_DONE;
	osm_mcast_mgr_ctxt_t *ctx;
	osm_mcast_req_type_t req_type;
	ib_net64_t port_guid;

	OSM_LOG_ENTER(sm->p_log);

	/* we need a lock to make sure the p_mgrp is not change other ways */
	CL_PLOCK_EXCL_ACQUIRE(sm->p_lock);

	while (!cl_is_qlist_empty(p_list)) {
		ctx = (osm_mcast_mgr_ctxt_t *) cl_qlist_remove_head(p_list);
		req_type = ctx->req_type;
		port_guid = ctx->port_guid;

		/* nice copy no warning on size diff */
		memcpy(&mlid, &ctx->mlid, sizeof(mlid));

		/* we can destroy the context now */
		free(ctx);

		/* since we delayed the execution we prefer to pass the
		   mlid as the mgrp identifier and then find it or abort */
		p_mgrp = osm_get_mgrp_by_mlid(sm->p_subn, mlid);
		if (!p_mgrp)
			continue;

		/* if there was no change from the last time
		 * we processed the group we can skip doing anything
		 */
		if (p_mgrp->last_change_id == p_mgrp->last_tree_id) {
			OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
				"Skip processing mgrp with lid:0x%X change id:%u\n",
				cl_ntoh16(mlid), p_mgrp->last_change_id);
			continue;
		}

		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Processing mgrp with lid:0x%X change id:%u\n",
			cl_ntoh16(mlid), p_mgrp->last_change_id);
		mcast_mgr_process_mgrp(sm, p_mgrp, req_type, port_guid);
	}

	/*
	   Walk the switches and download the tables for each.
	 */
	p_sw_tbl = &sm->p_subn->sw_guid_tbl;
	p_sw = (osm_switch_t *) cl_qmap_head(p_sw_tbl);
	while (p_sw != (osm_switch_t *) cl_qmap_end(p_sw_tbl)) {
		ret = __osm_mcast_mgr_set_tbl(sm, p_sw);
		if (ret == OSM_SIGNAL_DONE_PENDING)
			signal = ret;
		p_sw = (osm_switch_t *) cl_qmap_next(&p_sw->map_item);
	}

	osm_dump_mcast_routes(sm->p_subn->p_osm);

	CL_PLOCK_RELEASE(sm->p_lock);
	OSM_LOG_EXIT(sm->p_log);
	return signal;
}
