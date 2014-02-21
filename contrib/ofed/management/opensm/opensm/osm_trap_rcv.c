/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2006 Mellanox Technologies LTD. All rights reserved.
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
 *    Implementation of osm_trap_rcv_t.
 * This object represents the Trap Receiver object.
 * This object is part of the opensm family of objects.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <string.h>
#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_debug.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_log.h>
#include <opensm/osm_node.h>
#include <opensm/osm_helper.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_inform.h>
#include <opensm/osm_opensm.h>

extern void osm_req_get_node_desc(IN osm_sm_t * sm, osm_physp_t *p_physp);

/**********************************************************************
 *
 * TRAP HANDLING:
 *
 * Assuming traps can be caused by bad hardware we should provide
 * a mechanism for filtering their propagation into the actual logic
 * of OpenSM such that it is not overloaded by them.
 *
 * We will provide a trap filtering mechanism with "Aging" capability.
 * This mechanism will track incoming traps, clasify them by their
 * source and content and provide back their age.
 *
 * A timer running in the background will toggle a timer counter
 * that should be referenced by the aging algorithm.
 * To provide an efficient handling of aging. We also track all traps
 * in a sorted list by their aging.
 *
 * The generic Aging Tracker mechanism is implemented in the
 * cl_aging_tracker object.
 *
 **********************************************************************/

typedef struct osm_trap_agingracker_context {
	osm_log_t *p_log;
	osm_physp_t *p_physp;
} osm_trap_aging_tracker_context_t;

/**********************************************************************
 **********************************************************************/
static osm_physp_t *get_physp_by_lid_and_num(IN osm_sm_t * sm,
					     IN uint16_t lid, IN uint8_t num)
{
	cl_ptr_vector_t *p_vec = &(sm->p_subn->port_lid_tbl);
	osm_port_t *p_port;

	if (lid > cl_ptr_vector_get_size(p_vec))
		return NULL;

	p_port = (osm_port_t *) cl_ptr_vector_get(p_vec, lid);
	if (!p_port)
		return NULL;

	if (osm_node_get_num_physp(p_port->p_node) < num)
		return NULL;

	return osm_node_get_physp_ptr(p_port->p_node, num);
}

/**********************************************************************
 **********************************************************************/
uint64_t
osm_trap_rcv_aging_tracker_callback(IN uint64_t key,
				    IN uint32_t num_regs, IN void *context)
{
	osm_sm_t *sm = context;
	uint16_t lid;
	uint8_t port_num;
	osm_physp_t *p_physp;

	OSM_LOG_ENTER(sm->p_log);

	if (osm_exit_flag)
		/* We got an exit flag - do nothing */
		return 0;

	lid = cl_ntoh16((uint16_t) ((key & 0x0000FFFF00000000ULL) >> 32));
	port_num = (uint8_t) ((key & 0x00FF000000000000ULL) >> 48);

	p_physp = get_physp_by_lid_and_num(sm, lid, port_num);
	if (!p_physp)
		OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
			"Cannot find port num:%u with lid:%u\n",
			port_num, lid);
	/* make sure the physp is still valid */
	/* If the health port was false - set it to true */
	else if (!osm_physp_is_healthy(p_physp)) {
		OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
			"Clearing health bit of port num:%u with lid:%u\n",
			port_num, lid);

		/* Clear its health bit */
		osm_physp_set_health(p_physp, TRUE);
	}

	OSM_LOG_EXIT(sm->p_log);

	/* We want to remove the event from the tracker - so
	   need to return zero. */
	return 0;
}

/**********************************************************************
 * CRC calculation for notice identification
 **********************************************************************/

#define CRC32_POLYNOMIAL   0xEDB88320L

/* calculate the crc for a given buffer */
static uint32_t __osm_trap_calc_crc32(void *buffer, uint32_t count)
{
	uint32_t temp1, temp2;
	uint32_t crc = -1L;
	unsigned char *p = (unsigned char *)buffer;
	/* pre - calculated table for faster crc calculation */
	static uint32_t crc_table[256];
	static boolean_t first = TRUE;
	int i, j;

	/* if we need to initialize the lookup table */
	if (first) {
		/* calc the CRC table */
		for (i = 0; i <= 255; i++) {
			crc = i;
			for (j = 8; j > 0; j--)
				if (crc & 1)
					crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
				else
					crc >>= 1;
			crc_table[i] = crc;
		}
		first = FALSE;
	}

	crc = -1L;
	/* do the calculation */
	while (count-- != 0) {
		temp1 = (crc >> 8) & 0x00FFFFFFL;
		temp2 = crc_table[((int)crc ^ *p++) & 0xFF];
		crc = temp1 ^ temp2;
	}
	return crc;
}

/********************************************************************
 ********************************************************************/

/* The key is created in the following manner:
   port_num  lid   crc
   \______/ \___/ \___/
     16b     16b   32b
*/
static void
__osm_trap_get_key(IN uint16_t lid,
		   IN uint8_t port_num,
		   IN ib_mad_notice_attr_t * p_ntci, OUT uint64_t * trap_key)
{
	uint32_t crc = 0;

	CL_ASSERT(trap_key);

	crc = __osm_trap_calc_crc32(p_ntci, sizeof(ib_mad_notice_attr_t));
	*trap_key = ((uint64_t) port_num << 48) | ((uint64_t) lid << 32) | crc;
}

/**********************************************************************
 **********************************************************************/
static int __print_num_received(IN uint32_t num_received)
{
	uint32_t i;

	/* Series is 10, 20, 50, 100, 200, 500, ... */
	i = num_received;
	while (i >= 10) {
		if (i % 10)
			break;
		i = i / 10;
	}

	if (i == 1 || i == 2 || i == 5)
		return 1;
	else
		return 0;
}

static int disable_port(osm_sm_t *sm, osm_physp_t *p)
{
	uint8_t payload[IB_SMP_DATA_SIZE];
	osm_madw_context_t context;
	ib_port_info_t *pi = (ib_port_info_t *)payload;
	int ret;

	/* select the nearest port to master opensm */
	if (p->p_remote_physp &&
	    p->dr_path.hop_count > p->p_remote_physp->dr_path.hop_count)
		p = p->p_remote_physp;

	/* If trap 131, might want to disable peer port if available */
	/* but peer port has been observed not to respond to SM requests */

	OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3810: "
		"Disabling physical port 0x%016" PRIx64 " num:%u\n",
		cl_ntoh64(osm_physp_get_port_guid(p)), p->port_num);

	memcpy(payload, &p->port_info, sizeof(ib_port_info_t));

	/* Set port to disabled/down */
	ib_port_info_set_port_state(pi, IB_LINK_DOWN);
	ib_port_info_set_port_phys_state(IB_PORT_PHYS_STATE_DISABLED, pi);

	/* Issue set of PortInfo */
	context.pi_context.node_guid = osm_node_get_node_guid(p->p_node);
	context.pi_context.port_guid = osm_physp_get_port_guid(p);
	context.pi_context.set_method = TRUE;
	context.pi_context.light_sweep = FALSE;
	context.pi_context.active_transition = FALSE;

	ret = osm_req_set(sm, osm_physp_get_dr_path_ptr(p),
			  payload, sizeof(payload), IB_MAD_ATTR_PORT_INFO,
			  cl_hton32(osm_physp_get_port_num(p)),
			  CL_DISP_MSGID_NONE, &context);
	if (ret)
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3811: "
			"Request to set PortInfo failed\n");

	return ret;
}

/**********************************************************************
 **********************************************************************/
static void
__osm_trap_rcv_process_request(IN osm_sm_t * sm,
			       IN const osm_madw_t * const p_madw)
{
	uint8_t payload[sizeof(ib_mad_notice_attr_t)];
	ib_smp_t *p_smp;
	ib_mad_notice_attr_t *p_ntci = (ib_mad_notice_attr_t *) payload;
	ib_api_status_t status;
	osm_madw_t tmp_madw;	/* we need a copy to last after repress */
	uint64_t trap_key;
	uint32_t num_received;
	osm_physp_t *p_physp;
	cl_ptr_vector_t *p_tbl;
	osm_port_t *p_port;
	ib_net16_t source_lid = 0;
	boolean_t is_gsi = TRUE;
	uint8_t port_num = 0;
	boolean_t physp_change_trap = FALSE;
	uint64_t event_wheel_timeout = OSM_DEFAULT_TRAP_SUPRESSION_TIMEOUT;
	boolean_t run_heavy_sweep = FALSE;

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_madw);

	if (osm_exit_flag)
		/*
		   We got an exit flag - do nothing
		   Otherwise we start a sweep on the trap 144 caused by
		   cleaning up SM Cap bit...
		 */
		goto Exit;

	/* update the is_gsi flag according to the mgmt_class field */
	if (p_madw->p_mad->mgmt_class == IB_MCLASS_SUBN_LID ||
	    p_madw->p_mad->mgmt_class == IB_MCLASS_SUBN_DIR)
		is_gsi = FALSE;

	/* No real need to grab the lock for this function. */
	memset(payload, 0, sizeof(payload));
	memset(&tmp_madw, 0, sizeof(tmp_madw));

	p_smp = osm_madw_get_smp_ptr(p_madw);

	if (p_smp->method != IB_MAD_METHOD_TRAP) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3801: "
			"Unsupported method 0x%X\n", p_smp->method);
		goto Exit;
	}

	/*
	 * The NOTICE Attribute is part of the SMP CLASS attributes
	 * As such the actual attribute data resides inside the SMP
	 * payload.
	 */

	memcpy(payload, &(p_smp->data), IB_SMP_DATA_SIZE);
	memcpy(&tmp_madw, p_madw, sizeof(tmp_madw));

	if (is_gsi == FALSE) {
		/* We are in smi flow */
		/*
		 * When we received a TRAP with dlid = 0 - it means it
		 * came from our own node. So we need to fix it.
		 */

		if (p_madw->mad_addr.addr_type.smi.source_lid == 0) {
			/* Check if the sm_base_lid is 0. If yes - this means
			   that the local lid wasn't configured yet. Don't send
			   a response to the trap. */
			if (sm->p_subn->sm_base_lid == 0) {
				OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
					"Received SLID=0 Trap with local LID=0. Ignoring MAD\n");
				goto Exit;
			}
			OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
				"Received SLID=0 Trap. Using local LID:%u instead\n",
				cl_ntoh16(sm->p_subn->sm_base_lid));
			tmp_madw.mad_addr.addr_type.smi.source_lid =
			    sm->p_subn->sm_base_lid;
		}

		source_lid = tmp_madw.mad_addr.addr_type.smi.source_lid;

		/* Print some info about the incoming Trap */
		if (ib_notice_is_generic(p_ntci)) {
			if ((p_ntci->g_or_v.generic.trap_num == CL_HTON16(129))
			    || (p_ntci->g_or_v.generic.trap_num ==
				CL_HTON16(130))
			    || (p_ntci->g_or_v.generic.trap_num ==
				CL_HTON16(131)))
				OSM_LOG(sm->p_log, OSM_LOG_ERROR,
					"Received Generic Notice type:%u "
					"num:%u (%s) Producer:%u (%s) "
					"from LID:%u Port %d TID:0x%016"
					PRIx64 "\n", ib_notice_get_type(p_ntci),
					cl_ntoh16(p_ntci->g_or_v.generic.
						  trap_num),
					ib_get_trap_str(p_ntci->g_or_v.generic.
							trap_num),
					cl_ntoh32(ib_notice_get_prod_type
						  (p_ntci)),
					ib_get_producer_type_str
					(ib_notice_get_prod_type(p_ntci)),
					cl_hton16(source_lid),
					p_ntci->data_details.ntc_129_131.
					port_num, cl_ntoh64(p_smp->trans_id));
			else
				OSM_LOG(sm->p_log, OSM_LOG_ERROR,
					"Received Generic Notice type:%u "
					"num:%u (%s) Producer:%u (%s) "
					"from LID:%u TID:0x%016" PRIx64
					"\n", ib_notice_get_type(p_ntci),
					cl_ntoh16(p_ntci->g_or_v.generic.
						  trap_num),
					ib_get_trap_str(p_ntci->g_or_v.generic.
							trap_num),
					cl_ntoh32(ib_notice_get_prod_type
						  (p_ntci)),
					ib_get_producer_type_str
					(ib_notice_get_prod_type(p_ntci)),
					cl_hton16(source_lid),
					cl_ntoh64(p_smp->trans_id));
		} else
			OSM_LOG(sm->p_log, OSM_LOG_ERROR,
				"Received Vendor Notice type:%u vend:0x%06X "
				"dev:%u from LID:%u TID:0x%016" PRIx64 "\n",
				ib_notice_get_type(p_ntci),
				cl_ntoh32(ib_notice_get_vend_id(p_ntci)),
				cl_ntoh16(p_ntci->g_or_v.vend.dev_id),
				cl_ntoh16(source_lid),
				cl_ntoh64(p_smp->trans_id));
	}

	osm_dump_notice(sm->p_log, p_ntci, OSM_LOG_VERBOSE);

	p_physp = osm_get_physp_by_mad_addr(sm->p_log,
					    sm->p_subn, &tmp_madw.mad_addr);
	if (p_physp)
		p_smp->m_key = p_physp->port_info.m_key;
	else
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3809: "
			"Failed to find source physical port for trap\n");

	status = osm_resp_send(sm, &tmp_madw, 0, payload);
	if (status != IB_SUCCESS) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3802: "
			"Error sending response (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

	/*
	 * We would like to filter out recurring Traps so we track them by
	 * their source lid and content. If the same trap was already
	 * received within the aging time window more than 10 times,
	 * we simply ignore it. This is done only if we are in smi mode
	 */

	if (is_gsi == FALSE) {
		if (ib_notice_is_generic(p_ntci) &&
		    ((p_ntci->g_or_v.generic.trap_num == CL_HTON16(129)) ||
		     (p_ntci->g_or_v.generic.trap_num == CL_HTON16(130)) ||
		     (p_ntci->g_or_v.generic.trap_num == CL_HTON16(131)))) {
			/* If this is a trap 129, 130, or 131 - then this is a
			 * trap signaling a change on a physical port.
			 * Mark the physp_change_trap flag as TRUE.
			 */
			physp_change_trap = TRUE;
			/* The source_lid should be based on the source_lid from the trap */
			source_lid = p_ntci->data_details.ntc_129_131.lid;
		}

		/* If physp_change_trap is TRUE - the key will include the port number.
		   If not - the port_number in the key will be zero. */
		if (physp_change_trap == TRUE) {
			port_num = p_ntci->data_details.ntc_129_131.port_num;
			__osm_trap_get_key(source_lid, port_num, p_ntci,
					   &trap_key);
		} else
			__osm_trap_get_key(source_lid, 0, p_ntci, &trap_key);

		/* try to find it in the aging tracker */
		num_received =
		    cl_event_wheel_num_regs(&sm->trap_aging_tracker,
					    trap_key);

		/* Now we know how many times it provided this trap */
		if (num_received > 10) {
			if (__print_num_received(num_received))
				OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3804: "
					"Received trap %u times consecutively\n",
					num_received);
			/*
			 * If the trap provides info about a bad port
			 * we mark it as unhealthy.
			 */
			if (physp_change_trap == TRUE) {
				/* get the port */
				p_physp = get_physp_by_lid_and_num(sm,
								   cl_ntoh16
								   (p_ntci->
								    data_details.
								    ntc_129_131.
								    lid),
								   port_num);

				if (!p_physp)
					OSM_LOG(sm->p_log, OSM_LOG_ERROR,
						"ERR 3805: "
						"Failed to find physical port by lid:%u num:%u\n",
						cl_ntoh16(p_ntci->data_details.
							  ntc_129_131.lid),
						p_ntci->data_details.
						ntc_129_131.port_num);
				else {
					/* When babbling port policy option is enabled and
					   Threshold for disabling a "babbling" port is exceeded */
					if (sm->p_subn->opt.
					    babbling_port_policy
					    && num_received >= 250
					    && disable_port(sm, p_physp) == 0)
						goto Exit;

					OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
						"Marking unhealthy physical port by lid:%u num:%u\n",
						cl_ntoh16(p_ntci->data_details.
							  ntc_129_131.lid),
						p_ntci->data_details.
						ntc_129_131.port_num);
					/* check if the current state of the p_physp is healthy. If
					   it is - then this is a first change of state. Run a heavy sweep.
					   if it is not - no need to mark it again - just restart the timer. */
					if (osm_physp_is_healthy(p_physp)) {
						osm_physp_set_health(p_physp,
								     FALSE);
						/* Make sure we sweep again - force a heavy sweep. */
						/* The sweep should be done only after the re-registration, or
						   else we'll be losing track of the timer. */
						run_heavy_sweep = TRUE;
					}
					/* If we are marking the port as unhealthy - we want to
					   keep this for a longer period of time than the
					   OSM_DEFAULT_TRAP_SUPRESSION_TIMEOUT. Use the
					   OSM_DEFAULT_UNHEALTHY_TIMEOUT */
					event_wheel_timeout =
					    OSM_DEFAULT_UNHEALTHY_TIMEOUT;
				}
			}
		}

		/* restart the aging anyway */
		/* If physp_change_trap is TRUE - then use a callback to unset the
		   healthy bit. If not - no need to use a callback. */
		if (physp_change_trap == TRUE)
			cl_event_wheel_reg(&sm->trap_aging_tracker, trap_key, cl_get_time_stamp() + event_wheel_timeout, osm_trap_rcv_aging_tracker_callback,	/* no callback */
					   sm	/* no context */ );
		else
			cl_event_wheel_reg(&sm->trap_aging_tracker, trap_key, cl_get_time_stamp() + event_wheel_timeout, NULL,	/* no callback */
					   NULL	/* no context */ );

		/* If was already registered do nothing more */
		if (num_received > 10 && run_heavy_sweep == FALSE) {
			if (__print_num_received(num_received))
				OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
					"Continuously received this trap %u times. Ignoring\n",
					num_received);
			goto Exit;
		}
	}

	/* Check for node description update. IB Spec v1.2.1 pg 823 */
	if ((p_ntci->data_details.ntc_144.local_changes & TRAP_144_MASK_OTHER_LOCAL_CHANGES) &&
		(p_ntci->data_details.ntc_144.change_flgs & TRAP_144_MASK_NODE_DESCRIPTION_CHANGE)
		) {
		OSM_LOG(sm->p_log, OSM_LOG_INFO, "Trap 144 Node description update\n");

		if (p_physp) {
			CL_PLOCK_ACQUIRE(sm->p_lock);
			osm_req_get_node_desc(sm, p_physp);
			CL_PLOCK_RELEASE(sm->p_lock);
		} else {
			OSM_LOG(sm->p_log, OSM_LOG_ERROR,
				"ERR 3812: No physical port found for "
				"trap 144: \"node description update\"\n");
		}
	}

	/* do a sweep if we received a trap */
	if (sm->p_subn->opt.sweep_on_trap) {
		/* if this is trap number 128 or run_heavy_sweep is TRUE - update the
		   force_single_heavy_sweep flag of the subnet.
		   Sweep also on traps 144/145 - these traps signal a change of a certain
		   port capability/system image guid.
		   TODO: In the future we can change this to just getting PortInfo on
		   this port instead of sweeping the entire subnet. */
		if (ib_notice_is_generic(p_ntci) &&
		    ((cl_ntoh16(p_ntci->g_or_v.generic.trap_num) == 128) ||
		     (cl_ntoh16(p_ntci->g_or_v.generic.trap_num) == 144) ||
		     (cl_ntoh16(p_ntci->g_or_v.generic.trap_num) == 145) ||
		     run_heavy_sweep)) {
			OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
				"Forcing heavy sweep. Received trap:%u\n",
				cl_ntoh16(p_ntci->g_or_v.generic.trap_num));

			sm->p_subn->force_heavy_sweep = TRUE;
		}
		osm_sm_signal(sm, OSM_SIGNAL_SWEEP);
	}

	/* If we reached here due to trap 129/130/131 - do not need to do
	   the notice report. Just goto exit. We know this is the case
	   if physp_change_trap is TRUE. */
	if (physp_change_trap == TRUE)
		goto Exit;

	/* Add a call to osm_report_notice */
	/* We are going to report the notice - so need to fix the IssuerGID
	   accordingly. See IBA 1.2 p.739 or IBA 1.1 p.653 for details. */
	if (is_gsi) {
		if (!tmp_madw.mad_addr.addr_type.gsi.global_route) {
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3806: "
				"Received gsi trap with global_route FALSE. "
				"Cannot update issuer_gid!\n");
			goto Exit;
		}
		memcpy(&(p_ntci->issuer_gid),
		       &(tmp_madw.mad_addr.addr_type.gsi.grh_info.src_gid),
		       sizeof(ib_gid_t));
	} else {
		/* Need to use the IssuerLID */
		p_tbl = &sm->p_subn->port_lid_tbl;

		CL_ASSERT(cl_ptr_vector_get_size(p_tbl) < 0x10000);

		if ((uint16_t) cl_ptr_vector_get_size(p_tbl) <=
		    cl_ntoh16(source_lid)) {
			/*  the source lid is out of range */
			OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
				"source lid is out of range:%u\n",
				cl_ntoh16(source_lid));

			goto Exit;
		}
		p_port = cl_ptr_vector_get(p_tbl, cl_ntoh16(source_lid));
		if (p_port == 0) {
			/* We have the lid - but no corresponding port */
			OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
				"Cannot find port corresponding to lid:%u\n",
				cl_ntoh16(source_lid));

			goto Exit;
		}

		p_ntci->issuer_gid.unicast.prefix =
		    sm->p_subn->opt.subnet_prefix;
		p_ntci->issuer_gid.unicast.interface_id = p_port->guid;
	}

	/* we need a lock here as the InformInfo DB must be stable */
	CL_PLOCK_ACQUIRE(sm->p_lock);
	status = osm_report_notice(sm->p_log, sm->p_subn, p_ntci);
	CL_PLOCK_RELEASE(sm->p_lock);
	if (status != IB_SUCCESS) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3803: "
			"Error sending trap reports (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(sm->p_log);
}

#if 0
/**********************************************************************
 CURRENTLY WE ARE NOT CREATING TRAPS - SO THIS CALL IS AN ERROR
**********************************************************************/
static void
__osm_trap_rcv_process_sm(IN osm_sm_t * sm,
			  IN const osm_remote_sm_t * const p_sm)
{
	/* const ib_sm_info_t*        p_smi; */

	OSM_LOG_ENTER(sm->p_log);

	OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3807: "
		"This function is not supported yet\n");

	OSM_LOG_EXIT(sm->p_log);
}
#endif

/**********************************************************************
 CURRENTLY WE ARE NOT CREATING TRAPS - SO THIS CALL IN AN ERROR
**********************************************************************/
static void
__osm_trap_rcv_process_response(IN osm_sm_t * sm,
				IN const osm_madw_t * const p_madw)
{

	OSM_LOG_ENTER(sm->p_log);

	OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3808: "
		"This function is not supported yet\n");

	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 **********************************************************************/
void osm_trap_rcv_process(IN void *context, IN void *data)
{
	osm_sm_t *sm = context;
	osm_madw_t *p_madw = data;
	ib_smp_t *p_smp;

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_madw);

	p_smp = osm_madw_get_smp_ptr(p_madw);

	/*
	   Determine if this is a request for our own Trap
	   or if this is a response to our request for another
	   SM's Trap.
	 */
	if (ib_smp_is_response(p_smp))
		__osm_trap_rcv_process_response(sm, p_madw);
	else
		__osm_trap_rcv_process_request(sm, p_madw);

	OSM_LOG_EXIT(sm->p_log);
}
