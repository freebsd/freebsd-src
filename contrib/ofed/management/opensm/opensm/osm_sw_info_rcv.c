/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
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
 *    Implementation of osm_si_rcv_t.
 * This object represents the SwitchInfo Receiver object.
 * This object is part of the opensm family of objects.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <string.h>
#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_debug.h>
#include <opensm/osm_log.h>
#include <opensm/osm_switch.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_helper.h>
#include <opensm/osm_opensm.h>

/**********************************************************************
 The plock must be held before calling this function.
**********************************************************************/
static void
__osm_si_rcv_get_port_info(IN osm_sm_t * sm, IN osm_switch_t * const p_sw)
{
	osm_madw_context_t context;
	uint8_t port_num;
	osm_physp_t *p_physp;
	osm_node_t *p_node;
	uint8_t num_ports;
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_sw);

	p_node = p_sw->p_node;

	CL_ASSERT(osm_node_get_type(p_node) == IB_NODE_TYPE_SWITCH);

	/*
	   Request PortInfo attribute for each port on the switch.
	 */
	p_physp = osm_node_get_physp_ptr(p_node, 0);

	context.pi_context.node_guid = osm_node_get_node_guid(p_node);
	context.pi_context.port_guid = osm_physp_get_port_guid(p_physp);
	context.pi_context.set_method = FALSE;
	context.pi_context.light_sweep = FALSE;
	context.pi_context.active_transition = FALSE;

	num_ports = osm_node_get_num_physp(p_node);

	for (port_num = 0; port_num < num_ports; port_num++) {
		status = osm_req_get(sm, osm_physp_get_dr_path_ptr(p_physp),
				     IB_MAD_ATTR_PORT_INFO, cl_hton32(port_num),
				     CL_DISP_MSGID_NONE, &context);
		if (status != IB_SUCCESS)
			/* continue the loop despite the error */
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3602: "
				"Failure initiating PortInfo request (%s)\n",
				ib_get_err_str(status));
	}

	OSM_LOG_EXIT(sm->p_log);
}

#if 0
/**********************************************************************
 The plock must be held before calling this function.
**********************************************************************/
static void
__osm_si_rcv_get_fwd_tbl(IN osm_sm_t * sm, IN osm_switch_t * const p_sw)
{
	osm_madw_context_t context;
	osm_dr_path_t *p_dr_path;
	osm_physp_t *p_physp;
	osm_node_t *p_node;
	uint32_t block_id_ho;
	uint32_t max_block_id_ho;
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_sw);

	p_node = p_sw->p_node;

	CL_ASSERT(osm_node_get_type(p_node) == IB_NODE_TYPE_SWITCH);

	context.lft_context.node_guid = osm_node_get_node_guid(p_node);
	context.lft_context.set_method = FALSE;

	max_block_id_ho = osm_switch_get_max_block_id_in_use(p_sw);

	p_physp = osm_node_get_physp_ptr(p_node, 0);
	p_dr_path = osm_physp_get_dr_path_ptr(p_physp);

	for (block_id_ho = 0; block_id_ho <= max_block_id_ho; block_id_ho++) {
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Retrieving FT block %u\n", block_id_ho);

		status = osm_req_get(sm, p_dr_path, IB_MAD_ATTR_LIN_FWD_TBL,
				     cl_hton32(block_id_ho),
				     CL_DISP_MSGID_NONE, &context);
		if (status != IB_SUCCESS)
			/* continue the loop despite the error */
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3603: "
				"Failure initiating PortInfo request (%s)\n",
				ib_get_err_str(status));
	}

	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 The plock must be held before calling this function.
**********************************************************************/
static void
__osm_si_rcv_get_mcast_fwd_tbl(IN osm_sm_t * sm, IN osm_switch_t * const p_sw)
{
	osm_madw_context_t context;
	osm_dr_path_t *p_dr_path;
	osm_physp_t *p_physp;
	osm_node_t *p_node;
	osm_mcast_tbl_t *p_tbl;
	uint32_t block_id_ho;
	uint32_t max_block_id_ho;
	uint32_t position;
	uint32_t max_position;
	uint32_t attr_mod_ho;
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_sw);

	p_node = p_sw->p_node;

	CL_ASSERT(osm_node_get_type(p_node) == IB_NODE_TYPE_SWITCH);

	if (osm_switch_get_mcast_fwd_tbl_size(p_sw) == 0) {
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Multicast not supported by switch 0x%016" PRIx64 "\n",
			cl_ntoh64(osm_node_get_node_guid(p_node)));
		goto Exit;
	}

	context.mft_context.node_guid = osm_node_get_node_guid(p_node);
	context.mft_context.set_method = FALSE;

	p_tbl = osm_switch_get_mcast_tbl_ptr(p_sw);
	max_block_id_ho = osm_mcast_tbl_get_max_block(p_tbl);

	if (max_block_id_ho > IB_MCAST_MAX_BLOCK_ID) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3609: "
			"Out-of-range mcast block size = %u on switch 0x%016"
			PRIx64 "\n", max_block_id_ho,
			cl_ntoh64(osm_node_get_node_guid(p_node)));
		goto Exit;
	}

	max_position = osm_mcast_tbl_get_max_position(p_tbl);

	CL_ASSERT(max_position <= IB_MCAST_POSITION_MAX);

	OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
		"Max MFT block = %u, Max position = %u\n", max_block_id_ho,
		max_position);

	p_physp = osm_node_get_physp_ptr(p_node, 0);
	p_dr_path = osm_physp_get_dr_path_ptr(p_physp);

	for (block_id_ho = 0; block_id_ho <= max_block_id_ho; block_id_ho++) {
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Retrieving MFT block %u\n", block_id_ho);

		for (position = 0; position <= max_position; position++) {
			OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
				"Retrieving MFT position %u\n", position);

			attr_mod_ho =
			    block_id_ho | position << IB_MCAST_POSITION_SHIFT;
			status =
			    osm_req_get(sm, p_dr_path,
					IB_MAD_ATTR_MCAST_FWD_TBL,
					cl_hton32(attr_mod_ho),
					CL_DISP_MSGID_NONE, &context);
			if (status != IB_SUCCESS)
				/* continue the loop despite the error */
				OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3607: "
					"Failure initiating PortInfo request (%s)\n",
					ib_get_err_str(status));
		}
	}

Exit:
	OSM_LOG_EXIT(sm->p_log);
}
#endif

/**********************************************************************
   Lock must be held on entry to this function.
**********************************************************************/
static void
__osm_si_rcv_process_new(IN osm_sm_t * sm,
			 IN osm_node_t * const p_node,
			 IN const osm_madw_t * const p_madw)
{
	osm_switch_t *p_sw;
	osm_switch_t *p_check;
	ib_switch_info_t *p_si;
	ib_smp_t *p_smp;
	cl_qmap_t *p_sw_guid_tbl;

	CL_ASSERT(sm);

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_madw);

	p_sw_guid_tbl = &sm->p_subn->sw_guid_tbl;

	p_smp = osm_madw_get_smp_ptr(p_madw);
	p_si = (ib_switch_info_t *) ib_smp_get_payload_ptr(p_smp);

	osm_dump_switch_info(sm->p_log, p_si, OSM_LOG_DEBUG);

	/*
	   Allocate a new switch object for this switch,
	   and place it in the switch table.
	 */
	p_sw = osm_switch_new(p_node, p_madw);
	if (p_sw == NULL) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3608: "
			"Unable to allocate new switch object\n");
		goto Exit;
	}

	/* set subnet max mlid to the minimum MulticastFDBCap of all switches */
	if (p_sw->mcast_tbl.max_mlid_ho < sm->p_subn->max_mcast_lid_ho) {
		sm->p_subn->max_mcast_lid_ho = p_sw->mcast_tbl.max_mlid_ho;
		OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
			"Subnet max multicast lid is 0x%X\n",
			sm->p_subn->max_mcast_lid_ho);
	}

	/* set subnet max unicast lid to the minimum LinearFDBCap of all switches */
	if (cl_ntoh16(p_si->lin_cap) < sm->p_subn->max_ucast_lid_ho) {
		sm->p_subn->max_ucast_lid_ho = cl_ntoh16(p_si->lin_cap);
		OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
			"Subnet max unicast lid is 0x%X\n",
			sm->p_subn->max_ucast_lid_ho);
	}

	p_check = (osm_switch_t *) cl_qmap_insert(p_sw_guid_tbl,
						  osm_node_get_node_guid
						  (p_node), &p_sw->map_item);

	if (p_check != p_sw) {
		/*
		   This shouldn't happen since we hold the lock!
		 */
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3605: "
			"Unable to add new switch object to database\n");
		osm_switch_delete(&p_sw);
		goto Exit;
	}

	p_node->sw = p_sw;

	/*
	   Update the switch info according to the
	   info we just received.
	 */
	osm_switch_set_switch_info(p_sw, p_si);
	p_sw->discovery_count++;

	/*
	   Get the PortInfo attribute for every port.
	 */
	__osm_si_rcv_get_port_info(sm, p_sw);

	/*
	   Don't bother retrieving the current unicast and multicast tables
	   from the switches.  The current version of SM does
	   not support silent take-over of an existing multicast
	   configuration.

	   Gathering the multicast tables can also generate large amounts
	   of extra subnet-init traffic.

	   The code to retrieve the tables was fully debugged.
	 */
#if 0
	__osm_si_rcv_get_fwd_tbl(sm, p_sw);
	if (!sm->p_subn->opt.disable_multicast)
		__osm_si_rcv_get_mcast_fwd_tbl(sm, p_sw);
#endif

Exit:
	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
   Lock must be held on entry to this function.
   Return 1 if the caller is expected to send a change_detected event.
   this can not be done internally as the event needs the lock...
**********************************************************************/
static boolean_t
__osm_si_rcv_process_existing(IN osm_sm_t * sm,
			      IN osm_node_t * const p_node,
			      IN const osm_madw_t * const p_madw)
{
	osm_switch_t *p_sw = p_node->sw;
	ib_switch_info_t *p_si;
	osm_si_context_t *p_si_context;
	ib_smp_t *p_smp;
	boolean_t is_change_detected = FALSE;

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_madw);

	p_smp = osm_madw_get_smp_ptr(p_madw);
	p_si = (ib_switch_info_t *) ib_smp_get_payload_ptr(p_smp);
	p_si_context = osm_madw_get_si_context_ptr(p_madw);

	if (p_si_context->set_method) {
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Received logical SetResp()\n");

		osm_switch_set_switch_info(p_sw, p_si);
	} else {
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Received logical GetResp()\n");

		osm_switch_set_switch_info(p_sw, p_si);

		/*
		   Check the port state change bit.  If true, then this switch
		   has seen a port state transition, so continue probing.
		 */
		if (p_si_context->light_sweep == TRUE) {
			/* This is a light sweep */
			/* If the mad was returned with an error -
			   signal a change to the state manager. */
			if (ib_smp_get_status(p_smp) != 0) {
				OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
					"GetResp() received with error in light sweep. "
					"Commencing heavy sweep\n");
				is_change_detected = TRUE;
			} else {
				/*
				   If something changed, then just signal the
				   state manager.  Don't attempt to probe
				   further during a light sweep.
				 */
				if (ib_switch_info_get_state_change(p_si)) {
					osm_dump_switch_info(sm->p_log, p_si,
							     OSM_LOG_DEBUG);
					is_change_detected = TRUE;
				}
			}
		} else {
			/*
			   This is a heavy sweep.  Get information regardless
			   of the state change bit.
			 */
			p_sw->discovery_count++;
			OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
				"discovery_count is:%u\n",
				p_sw->discovery_count);

			/* If this is the first discovery - then get the port_info */
			if (p_sw->discovery_count == 1)
				__osm_si_rcv_get_port_info(sm, p_sw);
			else
				OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
					"Not discovering again through switch:0x%"
					PRIx64 "\n",
					osm_node_get_node_guid(p_sw->p_node));
		}
	}

	OSM_LOG_EXIT(sm->p_log);
	return is_change_detected;
}

/**********************************************************************
 **********************************************************************/
void osm_si_rcv_process(IN void *context, IN void *data)
{
	osm_sm_t *sm = context;
	osm_madw_t *p_madw = data;
	ib_switch_info_t *p_si;
	ib_smp_t *p_smp;
	osm_node_t *p_node;
	ib_net64_t node_guid;
	osm_si_context_t *p_context;

	CL_ASSERT(sm);

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_madw);

	p_smp = osm_madw_get_smp_ptr(p_madw);
	p_si = (ib_switch_info_t *) ib_smp_get_payload_ptr(p_smp);

	/*
	   Acquire the switch object and add the switch info.
	 */
	p_context = osm_madw_get_si_context_ptr(p_madw);

	node_guid = p_context->node_guid;

	OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
		"Switch GUID 0x%016" PRIx64 ", TID 0x%" PRIx64 "\n",
		cl_ntoh64(node_guid), cl_ntoh64(p_smp->trans_id));

	CL_PLOCK_EXCL_ACQUIRE(sm->p_lock);

	p_node = osm_get_node_by_guid(sm->p_subn, node_guid);
	if (!p_node)
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3606: "
			"SwitchInfo received for nonexistent node "
			"with GUID 0x%" PRIx64 "\n", cl_ntoh64(node_guid));
	else {

		/*
		   Hack for bad value in Mellanox switch
		 */
		if (cl_ntoh16(p_si->lin_top) > IB_LID_UCAST_END_HO) {
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3610: "
				"\n\t\t\t\tBad LinearFDBTop value = 0x%X "
				"on switch 0x%" PRIx64
				"\n\t\t\t\tForcing internal correction to 0x%X\n",
				cl_ntoh16(p_si->lin_top),
				cl_ntoh64(osm_node_get_node_guid(p_node)), 0);

			p_si->lin_top = 0;
		}

		/*
		   Acquire the switch object for this switch.
		 */
		if (!p_node->sw) {
			__osm_si_rcv_process_new(sm, p_node, p_madw);
			/*
			   A new switch was found during the sweep so we need
			   to ignore the current LFT settings.
			 */
			sm->p_subn->ignore_existing_lfts = TRUE;
		} else {
			/* we might get back a request for signaling change was detected */
			if (__osm_si_rcv_process_existing(sm, p_node, p_madw)) {
				CL_PLOCK_RELEASE(sm->p_lock);
				sm->p_subn->force_heavy_sweep = TRUE;
				goto Exit;
			}
		}
	}

	CL_PLOCK_RELEASE(sm->p_lock);
Exit:
	OSM_LOG_EXIT(sm->p_log);
}
