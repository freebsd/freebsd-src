/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2008 Mellanox Technologies LTD. All rights reserved.
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
 *    Implementation of osm_switch_t.
 * This object represents an Infiniband switch.
 * This object is part of the opensm family of objects.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <complib/cl_math.h>
#include <iba/ib_types.h>
#include <opensm/osm_switch.h>

/**********************************************************************
 **********************************************************************/
cl_status_t
osm_switch_set_hops(IN osm_switch_t * const p_sw,
		    IN const uint16_t lid_ho,
		    IN const uint8_t port_num, IN const uint8_t num_hops)
{
	if (lid_ho > p_sw->max_lid_ho)
		return -1;
	if (!p_sw->hops[lid_ho]) {
		p_sw->hops[lid_ho] = malloc(p_sw->num_ports);
		if (!p_sw->hops[lid_ho])
			return -1;
		memset(p_sw->hops[lid_ho], OSM_NO_PATH, p_sw->num_ports);
	}

	p_sw->hops[lid_ho][port_num] = num_hops;
	if (p_sw->hops[lid_ho][0] > num_hops)
		p_sw->hops[lid_ho][0] = num_hops;

	return 0;
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
osm_switch_init(IN osm_switch_t * const p_sw,
		IN osm_node_t * const p_node,
		IN const osm_madw_t * const p_madw)
{
	ib_api_status_t status = IB_SUCCESS;
	ib_switch_info_t *p_si;
	ib_smp_t *p_smp;
	uint8_t num_ports;
	uint32_t port_num;

	p_smp = osm_madw_get_smp_ptr(p_madw);
	p_si = (ib_switch_info_t *) ib_smp_get_payload_ptr(p_smp);
	num_ports = osm_node_get_num_physp(p_node);

	CL_ASSERT(p_smp->attr_id == IB_MAD_ATTR_SWITCH_INFO);

	p_sw->p_node = p_node;
	p_sw->switch_info = *p_si;
	p_sw->num_ports = num_ports;
	p_sw->need_update = 2;

	/* Initiate the linear forwarding table */

	if (!p_si->lin_cap) {
		/* This switch does not support linear forwarding tables */
		status = IB_UNSUPPORTED;
		goto Exit;
	}

	p_sw->lft = malloc(IB_LID_UCAST_END_HO + 1);
	if (!p_sw->lft) {
		status = IB_INSUFFICIENT_MEMORY;
		goto Exit;
	}

	/* Initialize the table to OSM_NO_PATH, which is "invalid port" */
	memset(p_sw->lft, OSM_NO_PATH, IB_LID_UCAST_END_HO + 1);

	p_sw->p_prof = malloc(sizeof(*p_sw->p_prof) * num_ports);
	if (p_sw->p_prof == NULL) {
		status = IB_INSUFFICIENT_MEMORY;
		goto Exit;
	}

	memset(p_sw->p_prof, 0, sizeof(*p_sw->p_prof) * num_ports);

	status = osm_mcast_tbl_init(&p_sw->mcast_tbl,
				    osm_node_get_num_physp(p_node),
				    cl_ntoh16(p_si->mcast_cap));
	if (status != IB_SUCCESS)
		goto Exit;

	for (port_num = 0; port_num < num_ports; port_num++)
		osm_port_prof_construct(&p_sw->p_prof[port_num]);

Exit:
	return (status);
}

/**********************************************************************
 **********************************************************************/
void osm_switch_delete(IN OUT osm_switch_t ** const pp_sw)
{
	osm_switch_t *p_sw = *pp_sw;
	unsigned i;

	osm_mcast_tbl_destroy(&p_sw->mcast_tbl);
	free(p_sw->p_prof);
	if (p_sw->lft)
		free(p_sw->lft);
	if (p_sw->new_lft)
		free(p_sw->new_lft);
	if (p_sw->hops) {
		for (i = 0; i < p_sw->num_hops; i++)
			if (p_sw->hops[i])
				free(p_sw->hops[i]);
		free(p_sw->hops);
	}
	free(*pp_sw);
	*pp_sw = NULL;
}

/**********************************************************************
 **********************************************************************/
osm_switch_t *osm_switch_new(IN osm_node_t * const p_node,
			     IN const osm_madw_t * const p_madw)
{
	ib_api_status_t status;
	osm_switch_t *p_sw;

	CL_ASSERT(p_madw);
	CL_ASSERT(p_node);

	p_sw = (osm_switch_t *) malloc(sizeof(*p_sw));
	if (p_sw) {
		memset(p_sw, 0, sizeof(*p_sw));
		status = osm_switch_init(p_sw, p_node, p_madw);
		if (status != IB_SUCCESS)
			osm_switch_delete(&p_sw);
	}

	return (p_sw);
}

/**********************************************************************
 **********************************************************************/
boolean_t
osm_switch_get_lft_block(IN const osm_switch_t * const p_sw,
			 IN const uint16_t block_id,
			 OUT uint8_t * const p_block)
{
	uint16_t base_lid_ho = block_id * IB_SMP_DATA_SIZE;

	CL_ASSERT(p_sw);
	CL_ASSERT(p_block);

	if (base_lid_ho > p_sw->max_lid_ho)
		return FALSE;

	CL_ASSERT(base_lid_ho + IB_SMP_DATA_SIZE <= IB_LID_UCAST_END_HO);
	memcpy(p_block, &(p_sw->lft[base_lid_ho]), IB_SMP_DATA_SIZE);
	return TRUE;
}

/**********************************************************************
 **********************************************************************/
static struct osm_remote_node *
osm_switch_find_guid_common(IN const osm_switch_t * const p_sw,
			    IN struct osm_remote_guids_count *r,
			    IN uint8_t port_num,
			    IN int find_sys_guid,
			    IN int find_node_guid)
{
	struct osm_remote_node *p_remote_guid = NULL;
	osm_physp_t *p_physp;
	osm_physp_t *p_rem_physp;
	osm_node_t *p_rem_node;
	uint64_t sys_guid;
	uint64_t node_guid;
	int i;

	CL_ASSERT(p_sw);

	p_physp = osm_node_get_physp_ptr(p_sw->p_node, port_num);
	p_rem_physp = osm_physp_get_remote(p_physp);
	p_rem_node = osm_physp_get_node_ptr(p_rem_physp);
	sys_guid = p_rem_node->node_info.sys_guid;
	node_guid = p_rem_node->node_info.node_guid;

	for (i = 0; i < r->count; i++) {
		if ((!find_sys_guid
		     || r->guids[i].node->node_info.sys_guid == sys_guid)
		    && (!find_node_guid
			|| r->guids[i].node->node_info.node_guid == node_guid)) {
			p_remote_guid = &r->guids[i];
			break;
		}
	}

	return p_remote_guid;
}

static struct osm_remote_node *
osm_switch_find_sys_guid_count(IN const osm_switch_t * const p_sw,
			       IN struct osm_remote_guids_count *r,
			       IN uint8_t port_num)
{
	return osm_switch_find_guid_common(p_sw, r, port_num, 1, 0);
}

static struct osm_remote_node *
osm_switch_find_node_guid_count(IN const osm_switch_t * const p_sw,
				IN struct osm_remote_guids_count *r,
				IN uint8_t port_num)
{
	return osm_switch_find_guid_common(p_sw, r, port_num, 0, 1);
}

/**********************************************************************
 **********************************************************************/
uint8_t
osm_switch_recommend_path(IN const osm_switch_t * const p_sw,
			  IN osm_port_t * p_port,
			  IN const uint16_t lid_ho,
			  IN unsigned start_from,
			  IN const boolean_t ignore_existing,
			  IN const boolean_t dor)
{
	/*
	   We support an enhanced LMC aware routing mode:
	   In the case of LMC > 0, we can track the remote side
	   system and node for all of the lids of the target
	   and try and avoid routing again through the same
	   system / node.

	   If this procedure is provided with the tracking array
	   and counter we can conduct this algorithm.
	 */
	boolean_t routing_for_lmc = (p_port->priv != NULL);
	uint16_t base_lid;
	uint8_t hops;
	uint8_t least_hops;
	uint8_t port_num;
	uint8_t num_ports;
	uint32_t least_paths = 0xFFFFFFFF;
	unsigned i;
	/*
	   The follwing will track the least paths if the
	   route should go through a new system/node
	 */
	uint32_t least_paths_other_sys = 0xFFFFFFFF;
	uint32_t least_paths_other_nodes = 0xFFFFFFFF;
	uint32_t least_forwarded_to = 0xFFFFFFFF;
	uint32_t check_count;
	uint8_t best_port = 0;
	/*
	   These vars track the best port if it connects to
	   not used system/node.
	 */
	uint8_t best_port_other_sys = 0;
	uint8_t best_port_other_node = 0;
	boolean_t port_found = FALSE;
	osm_physp_t *p_physp;
	osm_physp_t *p_rem_physp;
	osm_node_t *p_rem_node;
	osm_node_t *p_rem_node_first = NULL;
	struct osm_remote_node *p_remote_guid = NULL;

	CL_ASSERT(lid_ho > 0);

	if (p_port->p_node->sw) {
		if (p_port->p_node->sw == p_sw)
			return 0;
		base_lid = osm_port_get_base_lid(p_port);
	} else {
		p_physp = p_port->p_physp;
		if (!p_physp || !p_physp->p_remote_physp ||
		    !p_physp->p_remote_physp->p_node->sw)
			return OSM_NO_PATH;

		if (p_physp->p_remote_physp->p_node->sw == p_sw)
			return p_physp->p_remote_physp->port_num;
		base_lid =
		    osm_node_get_base_lid(p_physp->p_remote_physp->p_node, 0);
	}
	base_lid = cl_ntoh16(base_lid);

	num_ports = p_sw->num_ports;

	least_hops = osm_switch_get_least_hops(p_sw, base_lid);
	if (least_hops == OSM_NO_PATH)
		return (OSM_NO_PATH);

	/*
	   First, inquire with the forwarding table for an existing
	   route.  If one is found, honor it unless:
	   1. the ignore existing flag is set.
	   2. the physical port is not a valid one or not healthy
	   3. the physical port has a remote port (the link is up)
	   4. the port has min-hops to the target (avoid loops)
	 */
	if (!ignore_existing) {
		port_num = osm_switch_get_port_by_lid(p_sw, lid_ho);

		if (port_num != OSM_NO_PATH) {
			CL_ASSERT(port_num < num_ports);

			p_physp =
			    osm_node_get_physp_ptr(p_sw->p_node, port_num);
			/*
			   Don't be too trusting of the current forwarding table!
			   Verify that the port number is legal and that the
			   LID is reachable through this port.
			 */
			if (p_physp && osm_physp_is_healthy(p_physp) &&
			    osm_physp_get_remote(p_physp)) {
				hops =
				    osm_switch_get_hop_count(p_sw, base_lid,
							     port_num);
				/*
				   If we aren't using pre-defined user routes
				   function, then we need to make sure that the
				   current path is the minimum one. In case of
				   having such a user function - this check will
				   not be done, and the old routing will be used.
				   Note: This means that it is the user's job to
				   clean all data in the forwarding tables that
				   he wants to be overridden by the minimum
				   hop function.
				 */
				if (hops == least_hops)
					return (port_num);
			}
		}
	}

	/*
	   This algorithm selects a port based on a static load balanced
	   selection across equal hop-count ports.
	   There is lots of room for improved sophistication here,
	   possibly guided by user configuration info.
	 */

	/*
	   OpenSM routing is "local" - not considering a full lid to lid
	   path. As such we can not guarantee a path will not loop if we
	   do not always follow least hops.
	   So we must abort if not least hops.
	 */

	/* port number starts with one and num_ports is 1 + num phys ports */
	for (i = start_from; i < start_from + num_ports; i++) {
		port_num = i%num_ports;
		if (!port_num ||
		    osm_switch_get_hop_count(p_sw, base_lid, port_num) !=
		    least_hops)
			continue;

		/* let us make sure it is not down or unhealthy */
		p_physp = osm_node_get_physp_ptr(p_sw->p_node, port_num);
		if (!p_physp || !osm_physp_is_healthy(p_physp) ||
		    /*
		       we require all - non sma ports to be linked
		       to be routed through
		     */
		    !osm_physp_get_remote(p_physp))
			continue;

		/*
		   We located a least-hop port, possibly one of many.
		   For this port, check the running total count of
		   the number of paths through this port.  Select
		   the port routing the least number of paths.
		 */
		check_count =
		    osm_port_prof_path_count_get(&p_sw->p_prof[port_num]);

		/*
		   Advanced LMC routing requires tracking of the
		   best port by the node connected to the other side of
		   it.
		 */
		if (routing_for_lmc) {
			/* Is the sys guid already used ? */
			p_remote_guid = osm_switch_find_sys_guid_count(p_sw,
								       p_port->priv,
								       port_num);

			/* If not update the least hops for this case */
			if (!p_remote_guid) {
				if (check_count < least_paths_other_sys) {
					least_paths_other_sys = check_count;
					best_port_other_sys = port_num;
					least_forwarded_to = 0;
				}
			} else {	/* same sys found - try node */
				/* Else is the node guid already used ? */
				p_remote_guid = osm_switch_find_node_guid_count(p_sw,
										p_port->priv,
										port_num);

				/* If not update the least hops for this case */
				if (!p_remote_guid
				    && check_count < least_paths_other_nodes) {
					least_paths_other_nodes = check_count;
					best_port_other_node = port_num;
					least_forwarded_to = 0;
				}
				/* else prior sys and node guid already used */

			}	/* same sys found */
		}

		/* routing for LMC mode */
		/*
		   the count is min but also lower then the max subscribed
		 */
		if (check_count < least_paths) {
			if (dor) {
				/* Get the Remote Node */
				p_rem_physp = osm_physp_get_remote(p_physp);
				p_rem_node =
				    osm_physp_get_node_ptr(p_rem_physp);
				/* use the first dimension, but spread
				 * traffic out among the group of ports
				 * representing that dimension */
				if (port_found) {
					if (p_rem_node != p_rem_node_first)
						continue;
				} else
					p_rem_node_first = p_rem_node;
			}
			port_found = TRUE;
			best_port = port_num;
			least_paths = check_count;
			if (routing_for_lmc
			    && p_remote_guid
			    && p_remote_guid->forwarded_to < least_forwarded_to)
				least_forwarded_to = p_remote_guid->forwarded_to;
		} else if (routing_for_lmc
			   && p_remote_guid
			   && check_count == least_paths
			   && p_remote_guid->forwarded_to < least_forwarded_to) {
			least_forwarded_to = p_remote_guid->forwarded_to;
			best_port = port_num;
		}
	}

	if (port_found == FALSE)
		return (OSM_NO_PATH);

	/*
	   if we are in enhanced routing mode and the best port is not
	   the local port 0
	 */
	if (routing_for_lmc && best_port) {
		/* Select the least hop port of the non used sys first */
		if (best_port_other_sys)
			best_port = best_port_other_sys;
		else if (best_port_other_node)
			best_port = best_port_other_node;
	}

	return (best_port);
}

/**********************************************************************
 **********************************************************************/
void osm_switch_clear_hops(IN osm_switch_t * p_sw)
{
	unsigned i;

	for (i = 0; i < p_sw->num_hops; i++)
		if (p_sw->hops[i])
			memset(p_sw->hops[i], OSM_NO_PATH, p_sw->num_ports);
}

/**********************************************************************
 **********************************************************************/
int
osm_switch_prepare_path_rebuild(IN osm_switch_t * p_sw, IN uint16_t max_lids)
{
	uint8_t **hops;
	unsigned i;

	for (i = 0; i < p_sw->num_ports; i++)
		osm_port_prof_construct(&p_sw->p_prof[i]);

	osm_switch_clear_hops(p_sw);

	if (!p_sw->new_lft &&
	    !(p_sw->new_lft = malloc(IB_LID_UCAST_END_HO + 1)))
		return IB_INSUFFICIENT_MEMORY;

	memset(p_sw->new_lft, OSM_NO_PATH, IB_LID_UCAST_END_HO + 1);

	if (!p_sw->hops) {
		hops = malloc((max_lids + 1) * sizeof(hops[0]));
		if (!hops)
			return -1;
		memset(hops, 0, (max_lids + 1) * sizeof(hops[0]));
		p_sw->hops = hops;
		p_sw->num_hops = max_lids + 1;
	} else if (max_lids + 1 > p_sw->num_hops) {
		uint8_t **old_hops;

		hops = malloc((max_lids + 1) * sizeof(hops[0]));
		if (!hops)
			return -1;
		memcpy(hops, p_sw->hops, p_sw->num_hops * sizeof(hops[0]));
		memset(hops + p_sw->num_hops, 0,
		       (max_lids + 1 - p_sw->num_hops) * sizeof(hops[0]));
		old_hops = p_sw->hops;
		p_sw->hops = hops;
		p_sw->num_hops = max_lids + 1;
		free(old_hops);
	}
	p_sw->max_lid_ho = max_lids;

	return 0;
}

/**********************************************************************
 **********************************************************************/
uint8_t
osm_switch_get_port_least_hops(IN const osm_switch_t * const p_sw,
			       IN const osm_port_t * p_port)
{
	uint16_t lid;

	if (p_port->p_node->sw) {
		if (p_port->p_node->sw == p_sw)
			return 0;
		lid = osm_node_get_base_lid(p_port->p_node, 0);
		return osm_switch_get_least_hops(p_sw, cl_ntoh16(lid));
	} else {
		osm_physp_t *p = p_port->p_physp;
		uint8_t hops;

		if (!p || !p->p_remote_physp || !p->p_remote_physp->p_node->sw)
			return OSM_NO_PATH;
		if (p->p_remote_physp->p_node->sw == p_sw)
			return 1;
		lid = osm_node_get_base_lid(p->p_remote_physp->p_node, 0);
		hops = osm_switch_get_least_hops(p_sw, cl_ntoh16(lid));
		return hops != OSM_NO_PATH ? hops + 1 : OSM_NO_PATH;
	}
}

/**********************************************************************
 **********************************************************************/
uint8_t
osm_switch_recommend_mcast_path(IN osm_switch_t * const p_sw,
				IN osm_port_t * p_port,
				IN uint16_t const mlid_ho,
				IN boolean_t const ignore_existing)
{
	uint16_t base_lid;
	uint8_t hops;
	uint8_t port_num;
	uint8_t num_ports;
	uint8_t least_hops;

	CL_ASSERT(mlid_ho >= IB_LID_MCAST_START_HO);

	if (p_port->p_node->sw) {
		if (p_port->p_node->sw == p_sw)
			return 0;
		base_lid = osm_port_get_base_lid(p_port);
	} else {
		osm_physp_t *p_physp = p_port->p_physp;
		if (!p_physp || !p_physp->p_remote_physp ||
		    !p_physp->p_remote_physp->p_node->sw)
			return OSM_NO_PATH;
		if (p_physp->p_remote_physp->p_node->sw == p_sw)
			return p_physp->p_remote_physp->port_num;
		base_lid =
		    osm_node_get_base_lid(p_physp->p_remote_physp->p_node, 0);
	}
	base_lid = cl_ntoh16(base_lid);
	num_ports = p_sw->num_ports;

	/*
	   If the user wants us to ignore existing multicast routes,
	   then simply return the shortest hop count path to the
	   target port.

	   Otherwise, return the first port that has a path to the target,
	   picking from the ports that are already in the multicast group.
	 */
	if (!ignore_existing) {
		for (port_num = 1; port_num < num_ports; port_num++) {
			if (!osm_mcast_tbl_is_port
			    (&p_sw->mcast_tbl, mlid_ho, port_num))
				continue;
			/*
			   Don't be too trusting of the current forwarding table!
			   Verify that the LID is reachable through this port.
			 */
			hops =
			    osm_switch_get_hop_count(p_sw, base_lid, port_num);
			if (hops != OSM_NO_PATH)
				return (port_num);
		}
	}

	/*
	   Either no existing mcast paths reach this port or we are
	   ignoring existing paths.

	   Determine the best multicast path to the target.  Note that this
	   algorithm is slightly different from the one used for unicast route
	   recommendation.  In this case (multicast), we must NOT
	   perform any sort of load balancing.  We MUST take the FIRST
	   port found that has <= the lowest hop count path.  This prevents
	   more than one multicast path to the same remote switch which
	   prevents a multicast loop.  Multicast loops are bad since the same
	   multicast packet will go around and around, inevitably creating
	   a black hole that will destroy the Earth in a firey conflagration.
	 */
	least_hops = osm_switch_get_least_hops(p_sw, base_lid);
	for (port_num = 1; port_num < num_ports; port_num++)
		if (osm_switch_get_hop_count(p_sw, base_lid, port_num) ==
		    least_hops)
			break;

	CL_ASSERT(port_num < num_ports);
	return (port_num);
}
