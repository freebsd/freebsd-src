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
 *    Implementation of osm_pi_rcv_t.
 * This object represents the PortInfo Receiver object.
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
#include <vendor/osm_vendor_api.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_log.h>
#include <opensm/osm_node.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_msgdef.h>
#include <opensm/osm_helper.h>
#include <opensm/osm_pkey.h>
#include <opensm/osm_remote_sm.h>
#include <opensm/osm_opensm.h>
#include <opensm/osm_ucast_mgr.h>

/**********************************************************************
 **********************************************************************/
static void
__osm_pi_rcv_set_sm(IN osm_sm_t * sm,
		    IN osm_physp_t * const p_physp)
{
	osm_bind_handle_t h_bind;
	osm_dr_path_t *p_dr_path;

	OSM_LOG_ENTER(sm->p_log);

	OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
		"Setting IS_SM bit in port attributes\n");

	p_dr_path = osm_physp_get_dr_path_ptr(p_physp);
	h_bind = osm_dr_path_get_bind_handle(p_dr_path);

	/*
	   The 'IS_SM' bit isn't already set, so set it.
	 */
	osm_vendor_set_sm(h_bind, TRUE);

	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 **********************************************************************/
static void pi_rcv_check_and_fix_lid(osm_log_t *log, ib_port_info_t * const pi,
				     osm_physp_t * p)
{
	if (cl_ntoh16(pi->base_lid) > IB_LID_UCAST_END_HO) {
		OSM_LOG(log, OSM_LOG_ERROR, "ERR 0F04: "
			"Got invalid base LID %u from the network. "
			"Corrected to %u.\n", cl_ntoh16(pi->base_lid),
			cl_ntoh16(p->port_info.base_lid));
		pi->base_lid = p->port_info.base_lid;
	}
}

/**********************************************************************
 **********************************************************************/
static void
__osm_pi_rcv_process_endport(IN osm_sm_t * sm,
			     IN osm_physp_t * const p_physp,
			     IN const ib_port_info_t * const p_pi)
{
	osm_madw_context_t context;
	ib_api_status_t status;
	ib_net64_t port_guid;
	uint8_t rate, mtu;
	cl_qmap_t *p_sm_tbl;
	osm_remote_sm_t *p_sm;

	OSM_LOG_ENTER(sm->p_log);

	port_guid = osm_physp_get_port_guid(p_physp);

	/* HACK extended port 0 should be handled too! */
	if (osm_physp_get_port_num(p_physp) != 0) {
		/* track the minimal endport MTU and rate */
		mtu = ib_port_info_get_mtu_cap(p_pi);
		if (mtu < sm->p_subn->min_ca_mtu) {
			OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
				"Setting endport minimal MTU to:%u defined by port:0x%"
				PRIx64 "\n", mtu, cl_ntoh64(port_guid));
			sm->p_subn->min_ca_mtu = mtu;
		}

		rate = ib_port_info_compute_rate(p_pi);
		if (rate < sm->p_subn->min_ca_rate) {
			OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
				"Setting endport minimal rate to:%u defined by port:0x%"
				PRIx64 "\n", rate, cl_ntoh64(port_guid));
			sm->p_subn->min_ca_rate = rate;
		}
	}

	if (port_guid == sm->p_subn->sm_port_guid) {
		/*
		   We received the PortInfo for our own port.
		 */
		if (!(p_pi->capability_mask & IB_PORT_CAP_IS_SM))
			/*
			   Set the IS_SM bit to indicate our port hosts an SM.
			 */
			__osm_pi_rcv_set_sm(sm, p_physp);
	} else {
		p_sm_tbl = &sm->p_subn->sm_guid_tbl;
		if (p_pi->capability_mask & IB_PORT_CAP_IS_SM) {
			/*
			 * Before querying the SM - we want to make sure we
			 * clean its state, so if the querying fails we
			 * recognize that this SM is not active.
			 */
			p_sm = (osm_remote_sm_t *) cl_qmap_get(p_sm_tbl, port_guid);
			if (p_sm != (osm_remote_sm_t *) cl_qmap_end(p_sm_tbl))
				/* clean it up */
				p_sm->smi.pri_state = 0xF0 & p_sm->smi.pri_state;
			if (sm->p_subn->opt.ignore_other_sm)
				OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
					"Ignoring SM on port 0x%" PRIx64 "\n",
					cl_ntoh64(port_guid));
			else {
				OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
					"Detected another SM. Requesting SMInfo"
					"\n\t\t\t\tPort 0x%" PRIx64 "\n",
					cl_ntoh64(port_guid));

				/*
				   This port indicates it's an SM and
				   it's not our own port.
				   Acquire the SMInfo Attribute.
				 */
				memset(&context, 0, sizeof(context));
				context.smi_context.set_method = FALSE;
				context.smi_context.port_guid = port_guid;
				status = osm_req_get(sm,
						     osm_physp_get_dr_path_ptr
						     (p_physp),
						     IB_MAD_ATTR_SM_INFO, 0,
						     CL_DISP_MSGID_NONE,
						     &context);

				if (status != IB_SUCCESS)
					OSM_LOG(sm->p_log, OSM_LOG_ERROR,
						"ERR 0F05: "
						"Failure requesting SMInfo (%s)\n",
						ib_get_err_str(status));
			}
		} else {
			p_sm = (osm_remote_sm_t *) cl_qmap_remove(p_sm_tbl, port_guid);
			if (p_sm != (osm_remote_sm_t *) cl_qmap_end(p_sm_tbl))
				free(p_sm);
		}
	}

	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 The plock must be held before calling this function.
**********************************************************************/
static void
__osm_pi_rcv_process_switch_port(IN osm_sm_t * sm,
				 IN osm_node_t * const p_node,
				 IN osm_physp_t * const p_physp,
				 IN ib_port_info_t * const p_pi)
{
	ib_api_status_t status = IB_SUCCESS;
	osm_madw_context_t context;
	osm_physp_t *p_remote_physp;
	osm_node_t *p_remote_node;
	uint8_t port_num;
	uint8_t remote_port_num;
	osm_dr_path_t path;

	OSM_LOG_ENTER(sm->p_log);

	/*
	   Check the state of the physical port.
	   If there appears to be something on the other end of the wire,
	   then ask for NodeInfo.  Ignore the switch management port.
	 */
	port_num = osm_physp_get_port_num(p_physp);
	/* if in_sweep_hop_0 is TRUE, then this means the SM is on the switch,
	   and we got switchInfo of our local switch. Do not continue
	   probing through the switch. */
	if (port_num != 0 && sm->p_subn->in_sweep_hop_0 == FALSE) {
		switch (ib_port_info_get_port_state(p_pi)) {
		case IB_LINK_DOWN:
			p_remote_physp = osm_physp_get_remote(p_physp);
			if (p_remote_physp) {
				p_remote_node =
				    osm_physp_get_node_ptr(p_remote_physp);
				remote_port_num =
				    osm_physp_get_port_num(p_remote_physp);

				OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
					"Unlinking local node 0x%" PRIx64
					", port %u"
					"\n\t\t\t\tand remote node 0x%" PRIx64
					", port %u\n",
					cl_ntoh64(osm_node_get_node_guid
						  (p_node)), port_num,
					cl_ntoh64(osm_node_get_node_guid
						  (p_remote_node)),
					remote_port_num);

				if (sm->ucast_mgr.cache_valid)
					osm_ucast_cache_add_link(&sm->ucast_mgr,
								 p_physp,
								 p_remote_physp);

				osm_node_unlink(p_node, (uint8_t) port_num,
						p_remote_node,
						(uint8_t) remote_port_num);

			}
			break;

		case IB_LINK_INIT:
		case IB_LINK_ARMED:
		case IB_LINK_ACTIVE:
			/*
			   To avoid looping forever, only probe the port if it
			   is NOT the port that responded to the SMP.

			   Request node info from the other end of this link:
			   1) Copy the current path from the parent node.
			   2) Extend the path to the next hop thru this port.
			   3) Request node info with the new path

			 */
			if (p_pi->local_port_num !=
			    osm_physp_get_port_num(p_physp)) {
				path = *osm_physp_get_dr_path_ptr(p_physp);

				osm_dr_path_extend(&path,
						   osm_physp_get_port_num
						   (p_physp));

				memset(&context, 0, sizeof(context));
				context.ni_context.node_guid =
				    osm_node_get_node_guid(p_node);
				context.ni_context.port_num =
				    osm_physp_get_port_num(p_physp);

				status = osm_req_get(sm,
						     &path,
						     IB_MAD_ATTR_NODE_INFO,
						     0,
						     CL_DISP_MSGID_NONE,
						     &context);

				if (status != IB_SUCCESS)
					OSM_LOG(sm->p_log, OSM_LOG_ERROR,
						"ERR 0F02: "
						"Failure initiating NodeInfo request (%s)\n",
						ib_get_err_str(status));
			} else
				OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
					"Skipping SMP responder port %u\n",
					p_pi->local_port_num);
			break;

		default:
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0F03: "
				"Unknown link state = %u, port = %u\n",
				ib_port_info_get_port_state(p_pi),
				p_pi->local_port_num);
			break;
		}
	}

	if (ib_port_info_get_port_state(p_pi) > IB_LINK_INIT && p_node->sw &&
	    p_node->sw->need_update == 1)
		p_node->sw->need_update = 0;

	if (p_physp->need_update)
		sm->p_subn->ignore_existing_lfts = TRUE;

	if (port_num == 0)
		pi_rcv_check_and_fix_lid(sm->p_log, p_pi, p_physp);

	/*
	   Update the PortInfo attribute.
	 */
	osm_physp_set_port_info(p_physp, p_pi);

	if (port_num == 0) {
		/* Determine if base switch port 0 */
		if (p_node->sw &&
		    !ib_switch_info_is_enhanced_port0(&p_node->sw->switch_info))
			/* PortState is not used on BSP0 but just in case it is DOWN */
			p_physp->port_info = *p_pi;
		__osm_pi_rcv_process_endport(sm, p_physp, p_pi);
	}

	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_pi_rcv_process_ca_or_router_port(IN osm_sm_t * sm,
				       IN osm_node_t * const p_node,
				       IN osm_physp_t * const p_physp,
				       IN ib_port_info_t * const p_pi)
{
	OSM_LOG_ENTER(sm->p_log);

	UNUSED_PARAM(p_node);

	pi_rcv_check_and_fix_lid(sm->p_log, p_pi, p_physp);

	osm_physp_set_port_info(p_physp, p_pi);

	__osm_pi_rcv_process_endport(sm, p_physp, p_pi);

	OSM_LOG_EXIT(sm->p_log);
}

#define IBM_VENDOR_ID  (0x5076)
/**********************************************************************
 **********************************************************************/
static void get_pkey_table(IN osm_log_t * p_log,
			   IN osm_sm_t * sm,
			   IN osm_node_t * const p_node,
			   IN osm_physp_t * const p_physp)
{

	osm_madw_context_t context;
	ib_api_status_t status;
	osm_dr_path_t path;
	uint8_t port_num;
	uint16_t block_num, max_blocks;
	uint32_t attr_mod_ho;

	OSM_LOG_ENTER(p_log);

	path = *osm_physp_get_dr_path_ptr(p_physp);

	context.pkey_context.node_guid = osm_node_get_node_guid(p_node);
	context.pkey_context.port_guid = osm_physp_get_port_guid(p_physp);
	context.pkey_context.set_method = FALSE;

	port_num = p_physp->port_num;

	if (!p_node->sw || port_num == 0)
		/* The maximum blocks is defined by the node info partition cap for CA,
		   router, and switch management ports. */
		max_blocks =
		    (cl_ntoh16(p_node->node_info.partition_cap) +
		     IB_NUM_PKEY_ELEMENTS_IN_BLOCK - 1)
		    / IB_NUM_PKEY_ELEMENTS_IN_BLOCK;
	else {
		/* This is a switch, and not a management port. The maximum blocks
		   is defined in the switch info partition enforcement cap. */

		/* Check for IBM eHCA firmware defect in reporting partition enforcement cap */
		if (cl_ntoh32(ib_node_info_get_vendor_id(&p_node->node_info)) ==
		    IBM_VENDOR_ID)
			p_node->sw->switch_info.enforce_cap = 0;

		/* Bail out if this is a switch with no partition enforcement capability */
		if (cl_ntoh16(p_node->sw->switch_info.enforce_cap) == 0)
			goto Exit;

		max_blocks = (cl_ntoh16(p_node->sw->switch_info.enforce_cap) +
			      IB_NUM_PKEY_ELEMENTS_IN_BLOCK -
			      1) / IB_NUM_PKEY_ELEMENTS_IN_BLOCK;
	}

	for (block_num = 0; block_num < max_blocks; block_num++) {
		if (osm_node_get_type(p_node) != IB_NODE_TYPE_SWITCH)
			attr_mod_ho = block_num;
		else
			attr_mod_ho = block_num | (port_num << 16);
		status = osm_req_get(sm, &path, IB_MAD_ATTR_P_KEY_TABLE,
				     cl_hton32(attr_mod_ho),
				     CL_DISP_MSGID_NONE, &context);

		if (status != IB_SUCCESS) {
			OSM_LOG(p_log, OSM_LOG_ERROR, "ERR 0F12: "
				"Failure initiating PKeyTable request (%s)\n",
				ib_get_err_str(status));
			goto Exit;
		}
	}

Exit:
	OSM_LOG_EXIT(p_log);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_pi_rcv_get_pkey_slvl_vla_tables(IN osm_sm_t * sm,
				      IN osm_node_t * const p_node,
				      IN osm_physp_t * const p_physp)
{
	OSM_LOG_ENTER(sm->p_log);

	get_pkey_table(sm->p_log, sm, p_node, p_physp);

	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 **********************************************************************/
static void
osm_pi_rcv_process_set(IN osm_sm_t * sm, IN osm_node_t * const p_node,
		       IN const uint8_t port_num, IN osm_madw_t * const p_madw)
{
	osm_physp_t *p_physp;
	ib_net64_t port_guid;
	ib_smp_t *p_smp;
	ib_port_info_t *p_pi;
	osm_pi_context_t *p_context;
	osm_log_level_t level;

	OSM_LOG_ENTER(sm->p_log);

	p_context = osm_madw_get_pi_context_ptr(p_madw);

	CL_ASSERT(p_node);

	p_physp = osm_node_get_physp_ptr(p_node, port_num);
	CL_ASSERT(p_physp);

	port_guid = osm_physp_get_port_guid(p_physp);

	p_smp = osm_madw_get_smp_ptr(p_madw);
	p_pi = (ib_port_info_t *) ib_smp_get_payload_ptr(p_smp);

	/* check for error */
	if (cl_ntoh16(p_smp->status) & 0x7fff) {
		/* If port already ACTIVE, don't treat status 7 as error */
		if (p_context->active_transition &&
		    (cl_ntoh16(p_smp->status) & 0x7fff) == 0x1c) {
			level = OSM_LOG_INFO;
			OSM_LOG(sm->p_log, OSM_LOG_INFO,
				"Received error status 0x%x for SetResp() during ACTIVE transition\n",
				cl_ntoh16(p_smp->status) & 0x7fff);
			/* Should there be a subsequent Get to validate that port is ACTIVE ? */
		} else {
			level = OSM_LOG_ERROR;
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0F10: "
				"Received error status for SetResp()\n");
		}
		osm_dump_port_info(sm->p_log,
				   osm_node_get_node_guid(p_node),
				   port_guid, port_num, p_pi, level);
	}

	OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
		"Received logical SetResp() for GUID 0x%" PRIx64
		", port num %u"
		"\n\t\t\t\tfor parent node GUID 0x%" PRIx64
		" TID 0x%" PRIx64 "\n",
		cl_ntoh64(port_guid), port_num,
		cl_ntoh64(osm_node_get_node_guid(p_node)),
		cl_ntoh64(p_smp->trans_id));

	osm_physp_set_port_info(p_physp, p_pi);

	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 **********************************************************************/
void osm_pi_rcv_process(IN void *context, IN void *data)
{
	osm_sm_t *sm = context;
	osm_madw_t *p_madw = data;
	ib_port_info_t *p_pi;
	ib_smp_t *p_smp;
	osm_port_t *p_port;
	osm_physp_t *p_physp;
	osm_dr_path_t *p_dr_path;
	osm_node_t *p_node;
	osm_pi_context_t *p_context;
	ib_net64_t port_guid;
	ib_net64_t node_guid;
	uint8_t port_num;

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(sm);
	CL_ASSERT(p_madw);

	p_smp = osm_madw_get_smp_ptr(p_madw);
	p_context = osm_madw_get_pi_context_ptr(p_madw);
	p_pi = (ib_port_info_t *) ib_smp_get_payload_ptr(p_smp);

	CL_ASSERT(p_smp->attr_id == IB_MAD_ATTR_PORT_INFO);

	port_num = (uint8_t) cl_ntoh32(p_smp->attr_mod);

	port_guid = p_context->port_guid;
	node_guid = p_context->node_guid;

	osm_dump_port_info(sm->p_log,
			   node_guid, port_guid, port_num, p_pi, OSM_LOG_DEBUG);

	/* On receipt of client reregister, clear the reregister bit so
	   reregistering won't be sent again and again */
	if (ib_port_info_get_client_rereg(p_pi)) {
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Client reregister received on response\n");
		ib_port_info_set_client_rereg(p_pi, 0);
	}

	/*
	   we might get a response during a light sweep looking for a change in
	   the status of a remote port that did not respond in earlier sweeps.
	   So if the context of the Get was light_sweep - we do not need to
	   do anything with the response - just flag that we need a heavy sweep
	 */
	if (p_context->light_sweep == TRUE) {
		OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
			"Got light sweep response from remote port of parent node "
			"GUID 0x%" PRIx64 " port 0x%016" PRIx64
			", Commencing heavy sweep\n",
			cl_ntoh64(node_guid), cl_ntoh64(port_guid));
		sm->p_subn->force_heavy_sweep = TRUE;
		sm->p_subn->ignore_existing_lfts = TRUE;
		goto Exit;
	}

	CL_PLOCK_EXCL_ACQUIRE(sm->p_lock);
	p_port = osm_get_port_by_guid(sm->p_subn, port_guid);
	if (!p_port) {
		CL_PLOCK_RELEASE(sm->p_lock);
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0F06: "
			"No port object for port with GUID 0x%" PRIx64
			"\n\t\t\t\tfor parent node GUID 0x%" PRIx64
			", TID 0x%" PRIx64 "\n",
			cl_ntoh64(port_guid),
			cl_ntoh64(node_guid), cl_ntoh64(p_smp->trans_id));
		goto Exit;
	}

	p_node = p_port->p_node;
	CL_ASSERT(p_node);

	/*
	   If we were setting the PortInfo, then receiving
	   this attribute was not part of sweeping the subnet.
	   In this case, just update the PortInfo attribute.

	   In an unfortunate blunder, the IB spec defines the
	   return method for Set() as a GetResp().  Thus, we can't
	   use the method (what would have been SetResp()) to determine
	   our course of action.  So, we have to carry this extra
	   boolean around to determine if we were doing Get() or Set().
	 */
	if (p_context->set_method)
		osm_pi_rcv_process_set(sm, p_node, port_num, p_madw);
	else {
		p_port->discovery_count++;

		/*
		   This PortInfo arrived because we did a Get() method,
		   most likely due to a subnet sweep in progress.
		 */
		OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
			"Discovered port num %u with GUID 0x%" PRIx64
			" for parent node GUID 0x%" PRIx64
			", TID 0x%" PRIx64 "\n",
			port_num, cl_ntoh64(port_guid),
			cl_ntoh64(node_guid), cl_ntoh64(p_smp->trans_id));

		p_physp = osm_node_get_physp_ptr(p_node, port_num);

		/*
		   Determine if we encountered a new Physical Port.
		   If so, initialize the new Physical Port then
		   continue processing as normal.
		 */
		if (!p_physp) {
			OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
				"Initializing port number %u\n", port_num);
			p_physp = &p_node->physp_table[port_num];
			osm_physp_init(p_physp,
				       port_guid,
				       port_num,
				       p_node,
				       osm_madw_get_bind_handle(p_madw),
				       p_smp->hop_count, p_smp->initial_path);
		} else {
			/*
			   Update the directed route path to this port
			   in case the old path is no longer usable.
			 */
			p_dr_path = osm_physp_get_dr_path_ptr(p_physp);
			osm_dr_path_init(p_dr_path,
					 osm_madw_get_bind_handle(p_madw),
					 p_smp->hop_count, p_smp->initial_path);
		}

		/* if port just inited or reached INIT state (external reset)
		   request update for port related tables */
		p_physp->need_update =
		    (ib_port_info_get_port_state(p_pi) == IB_LINK_INIT ||
		     p_physp->need_update > 1) ? 1 : 0;

		switch (osm_node_get_type(p_node)) {
		case IB_NODE_TYPE_CA:
		case IB_NODE_TYPE_ROUTER:
			__osm_pi_rcv_process_ca_or_router_port(sm,
							       p_node, p_physp,
							       p_pi);
			break;
		case IB_NODE_TYPE_SWITCH:
			__osm_pi_rcv_process_switch_port(sm,
							 p_node, p_physp, p_pi);
			break;
		default:
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 0F07: "
				"Unknown node type %u with GUID 0x%" PRIx64
				"\n", osm_node_get_type(p_node),
				cl_ntoh64(node_guid));
			break;
		}

		/*
		   Get the tables on the physp.
		 */
		if (p_physp->need_update || sm->p_subn->need_update)
			__osm_pi_rcv_get_pkey_slvl_vla_tables(sm, p_node,
							      p_physp);

	}

	CL_PLOCK_RELEASE(sm->p_lock);

Exit:
	/*
	   Release the lock before jumping here!!
	 */
	OSM_LOG_EXIT(sm->p_log);
}
