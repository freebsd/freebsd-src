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
 *    Implementation of osm_sminfo_rcv_t.
 * This object represents the SMInfo Receiver object.
 * This object is part of the opensm family of objects.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_debug.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_log.h>
#include <opensm/osm_node.h>
#include <opensm/osm_helper.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_sm.h>
#include <opensm/osm_opensm.h>

/**********************************************************************
 Return TRUE if the remote sm given (by ib_sm_info_t) is higher,
 return FALSE otherwise.
 By higher - we mean: SM with higher priority or with same priority
 and lower GUID.
**********************************************************************/
static inline boolean_t
__osm_sminfo_rcv_remote_sm_is_higher(IN osm_sm_t * sm,
				     IN const ib_sm_info_t * p_remote_smi)
{
	return (osm_sm_is_greater_than(ib_sminfo_get_priority(p_remote_smi),
				       p_remote_smi->guid,
				       sm->p_subn->opt.sm_priority,
				       sm->p_subn->sm_port_guid));

}

/**********************************************************************
 **********************************************************************/
static void
__osm_sminfo_rcv_process_get_request(IN osm_sm_t * sm,
				     IN const osm_madw_t * const p_madw)
{
	uint8_t payload[IB_SMP_DATA_SIZE];
	ib_smp_t *p_smp;
	ib_sm_info_t *p_smi = (ib_sm_info_t *) payload;
	ib_api_status_t status;
	ib_sm_info_t *p_remote_smi;

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_madw);

	/* No real need to grab the lock for this function. */
	memset(payload, 0, sizeof(payload));

	p_smp = osm_madw_get_smp_ptr(p_madw);

	CL_ASSERT(p_smp->method == IB_MAD_METHOD_GET);

	p_smi->guid = sm->p_subn->sm_port_guid;
	p_smi->act_count = cl_hton32(sm->p_subn->p_osm->stats.qp0_mads_sent);
	p_smi->pri_state = (uint8_t) (sm->p_subn->sm_state |
				      sm->p_subn->opt.sm_priority << 4);
	/*
	   p.840 line 20 - Return 0 for the SM key unless we authenticate the
	   requester as the master SM.
	 */
	p_remote_smi = ib_smp_get_payload_ptr(osm_madw_get_smp_ptr(p_madw));
	if (ib_sminfo_get_state(p_remote_smi) == IB_SMINFO_STATE_MASTER) {
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Responding to master SM with real sm_key\n");
		p_smi->sm_key = sm->p_subn->opt.sm_key;
	} else {
		/* The requester is not authenticated as master - set sm_key to zero. */
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Responding to SM not master with zero sm_key\n");
		p_smi->sm_key = 0;
	}

	status = osm_resp_send(sm, p_madw, 0, payload);
	if (status != IB_SUCCESS) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F02: "
			"Error sending response (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 * Check if the p_smp received is legal.
 * Current checks:
 *   MADHeader:AttributeModifier of ACKNOWLEDGE that was not sent by a
 *             Standby SM.
 *   MADHeader:AttributeModifiers of HANDOVER/DISABLE/STANDBY/DISCOVER
 *             that was not sent by a Master SM.
 * FUTURE - TO DO:
 *   Check that the SM_Key matches.
 **********************************************************************/
static ib_api_status_t
__osm_sminfo_rcv_check_set_req_legality(IN const ib_smp_t * const p_smp)
{
	ib_sm_info_t *p_smi;

	p_smi = ib_smp_get_payload_ptr(p_smp);

	if (p_smp->attr_mod == IB_SMINFO_ATTR_MOD_ACKNOWLEDGE) {
		if (ib_sminfo_get_state(p_smi) == IB_SMINFO_STATE_STANDBY)
			return (IB_SUCCESS);
	} else if (p_smp->attr_mod == IB_SMINFO_ATTR_MOD_HANDOVER ||
		   p_smp->attr_mod == IB_SMINFO_ATTR_MOD_DISABLE ||
		   p_smp->attr_mod == IB_SMINFO_ATTR_MOD_STANDBY ||
		   p_smp->attr_mod == IB_SMINFO_ATTR_MOD_DISCOVER) {
		if (ib_sminfo_get_state(p_smi) == IB_SMINFO_STATE_MASTER)
			return (IB_SUCCESS);
	}

	return (IB_INVALID_PARAMETER);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_sminfo_rcv_process_set_request(IN osm_sm_t * sm,
				     IN const osm_madw_t * const p_madw)
{
	uint8_t payload[IB_SMP_DATA_SIZE];
	ib_smp_t *p_smp;
	ib_sm_info_t *p_smi = (ib_sm_info_t *) payload;
	ib_sm_info_t *sm_smi;
	ib_api_status_t status;
	osm_sm_signal_t sm_signal;
	ib_sm_info_t *p_remote_smi;

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_madw);

	memset(payload, 0, sizeof(payload));

	p_smp = osm_madw_get_smp_ptr(p_madw);
	sm_smi = ib_smp_get_payload_ptr(p_smp);

	if (p_smp->method != IB_MAD_METHOD_SET) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F03: "
			"Unsupported method 0x%X\n", p_smp->method);
		goto Exit;
	}

	CL_PLOCK_EXCL_ACQUIRE(sm->p_lock);

	p_smi->guid = sm->p_subn->sm_port_guid;
	p_smi->act_count = cl_hton32(sm->p_subn->p_osm->stats.qp0_mads_sent);
	p_smi->pri_state = (uint8_t) (sm->p_subn->sm_state |
				      sm->p_subn->opt.sm_priority << 4);
	/*
	   p.840 line 20 - Return 0 for the SM key unless we authenticate the
	   requester as the master SM.
	 */
	p_remote_smi = ib_smp_get_payload_ptr(osm_madw_get_smp_ptr(p_madw));
	if (ib_sminfo_get_state(p_remote_smi) == IB_SMINFO_STATE_MASTER) {
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Responding to master SM with real sm_key\n");
		p_smi->sm_key = sm->p_subn->opt.sm_key;
	} else {
		/* The requester is not authenticated as master - set sm_key to zero. */
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Responding to SM not master with zero sm_key\n");
		p_smi->sm_key = 0;
	}

	/* Check the legality of the packet */
	status = __osm_sminfo_rcv_check_set_req_legality(p_smp);
	if (status != IB_SUCCESS) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F04: "
			"Check legality failed. AttributeModifier:0x%X RemoteState:%s\n",
			p_smp->attr_mod,
			osm_get_sm_mgr_state_str(ib_sminfo_get_state(sm_smi)));
		status = osm_resp_send(sm, p_madw, 7, payload);
		if (status != IB_SUCCESS)
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F05: "
				"Error sending response (%s)\n",
				ib_get_err_str(status));
		CL_PLOCK_RELEASE(sm->p_lock);
		goto Exit;
	}

	/* translate from IB_SMINFO_ATTR to OSM_SM_SIGNAL */
	switch (p_smp->attr_mod) {
	case IB_SMINFO_ATTR_MOD_HANDOVER:
		sm_signal = OSM_SM_SIGNAL_HANDOVER;
		break;
	case IB_SMINFO_ATTR_MOD_ACKNOWLEDGE:
		sm_signal = OSM_SM_SIGNAL_ACKNOWLEDGE;
		break;
	case IB_SMINFO_ATTR_MOD_DISABLE:
		sm_signal = OSM_SM_SIGNAL_DISABLE;
		break;
	case IB_SMINFO_ATTR_MOD_STANDBY:
		sm_signal = OSM_SM_SIGNAL_STANDBY;
		break;
	case IB_SMINFO_ATTR_MOD_DISCOVER:
		sm_signal = OSM_SM_SIGNAL_DISCOVER;
		break;
	default:
		/*
		   This code shouldn't be reached - checked in the
		   check legality
		 */
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F06: "
			"THIS CODE SHOULD NOT BE REACHED!!\n");
		CL_PLOCK_RELEASE(sm->p_lock);
		goto Exit;
	}

	/* check legality of the needed transition in the SM state machine */
	status = osm_sm_state_mgr_check_legality(sm, sm_signal);
	if (status != IB_SUCCESS) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F07: "
			"Failed check of legality of needed SM transition. AttributeModifier:0x%X RemoteState:%s\n",
			p_smp->attr_mod,
			osm_get_sm_mgr_state_str(ib_sminfo_get_state(sm_smi)));
		status = osm_resp_send(sm, p_madw, 7, payload);
		if (status != IB_SUCCESS)
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F08: "
				"Error sending response (%s)\n",
				ib_get_err_str(status));
		CL_PLOCK_RELEASE(sm->p_lock);
		goto Exit;
	}

	/* the SubnSet(SMInfo) command is ok. Send a response. */
	status = osm_resp_send(sm, p_madw, 0, payload);
	if (status != IB_SUCCESS)
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F09: "
			"Error sending response (%s)\n",
			ib_get_err_str(status));

	/* it is a legal packet - act according to it */

	/* if the AttributeModifier is STANDBY - need to save on the sm in */
	/* the master_sm_guid variable - the guid of the current master. */
	if (p_smp->attr_mod == IB_SMINFO_ATTR_MOD_STANDBY) {
		OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
			"Received a STANDBY signal. Updating "
			"sm_state_mgr master_guid: 0x%016" PRIx64 "\n",
			cl_ntoh64(sm_smi->guid));
		sm->master_sm_guid = sm_smi->guid;
	}

	CL_PLOCK_RELEASE(sm->p_lock);
	status = osm_sm_state_mgr_process(sm, sm_signal);

	if (status != IB_SUCCESS)
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F10: "
			"Error in SM state transition (%s)\n",
			ib_get_err_str(status));

Exit:
	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 **********************************************************************/
static osm_signal_t
__osm_sminfo_rcv_process_get_sm(IN osm_sm_t * sm,
				IN const osm_remote_sm_t * const p_sm,
				boolean_t light_sweep)
{
	const ib_sm_info_t *p_smi;

	OSM_LOG_ENTER(sm->p_log);

	p_smi = &p_sm->smi;

	OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
		"Detected SM 0x%016" PRIx64 " in state %u\n",
		cl_ntoh64(p_smi->guid), ib_sminfo_get_state(p_smi));

	/* Check the state of this SM vs. our own. */
	switch (sm->p_subn->sm_state) {
	case IB_SMINFO_STATE_NOTACTIVE:
		break;

	case IB_SMINFO_STATE_DISCOVERING:
		switch (ib_sminfo_get_state(p_smi)) {
		case IB_SMINFO_STATE_NOTACTIVE:
			break;
		case IB_SMINFO_STATE_MASTER:
			sm->master_sm_found = 1;
			/* save on the sm the guid of the current master. */
			OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
				"Found master SM. Updating sm_state_mgr master_guid: 0x%016"
				PRIx64 "\n", cl_ntoh64(p_sm->p_port->guid));
			sm->master_sm_guid = p_sm->p_port->guid;
			break;
		case IB_SMINFO_STATE_DISCOVERING:
		case IB_SMINFO_STATE_STANDBY:
			if (__osm_sminfo_rcv_remote_sm_is_higher(sm, p_smi)
			    == TRUE) {
				/* the remote is a higher sm - need to stop sweeping */
				sm->master_sm_found = 1;
				/* save on the sm the guid of the higher SM we found - */
				/* we will poll it - as long as it lives - we should be in Standby. */
				OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
					"Found higher SM. Updating sm_state_mgr master_guid:"
					" 0x%016" PRIx64 "\n",
					cl_ntoh64(p_sm->p_port->guid));
				sm->master_sm_guid = p_sm->p_port->guid;
			}
			break;
		default:
			break;
		}
		break;

	case IB_SMINFO_STATE_STANDBY:
		/* if the guid of the SM that sent us this response is equal to the */
		/* p_sm_mgr->master_guid - then this is a signal that the polling */
		switch (ib_sminfo_get_state(p_smi)) {
		case IB_SMINFO_STATE_MASTER:
			/* This means the master is alive */
			/* Signal that to the SM state mgr */
			osm_sm_state_mgr_signal_master_is_alive(sm);
			break;
		case IB_SMINFO_STATE_STANDBY:
			/* This should be the response from the sm we are polling. */
			/* If it is - then signal master is alive */
			if (sm->master_sm_guid == p_sm->p_port->guid) {
				/* Make sure that it is an SM with higher priority than us.
				   If we started polling it when it was master, and it moved
				   to standby - then it might be with a lower priority than
				   us - and then we don't want to continue polling it. */
				if (__osm_sminfo_rcv_remote_sm_is_higher
				    (sm, p_smi) == TRUE)
					osm_sm_state_mgr_signal_master_is_alive(sm);
			}
			break;
		default:
			/* any other state - do nothing */
			break;
		}
		break;

	case IB_SMINFO_STATE_MASTER:
		switch (ib_sminfo_get_state(p_smi)) {
		case IB_SMINFO_STATE_MASTER:
			/* If this is a response due to our polling, this means that we are
			   waiting for a handover from this SM, and it is still alive -
			   signal that. */
			if (sm->p_polling_sm)
				osm_sm_state_mgr_signal_master_is_alive(sm);
			else {
				/* This is a response we got while sweeping the subnet.
				   We will handle a case of handover needed later on, when the sweep
				   is done and all SMs are recongnized. */
			}
			break;
		case IB_SMINFO_STATE_STANDBY:
			if (light_sweep &&
			    __osm_sminfo_rcv_remote_sm_is_higher(sm, p_smi))
				sm->p_subn->force_heavy_sweep = TRUE;
			break;
		default:
			/* any other state - do nothing */
			break;
		}
		break;

	default:
		break;
	}

	OSM_LOG_EXIT(sm->p_log);
	return 0;
}

/**********************************************************************
 **********************************************************************/
static void
__osm_sminfo_rcv_process_get_response(IN osm_sm_t * sm,
				      IN const osm_madw_t * const p_madw)
{
	const ib_smp_t *p_smp;
	const ib_sm_info_t *p_smi;
	cl_qmap_t *p_sm_tbl;
	osm_port_t *p_port;
	ib_net64_t port_guid;
	osm_remote_sm_t *p_sm;

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_madw);

	p_smp = osm_madw_get_smp_ptr(p_madw);

	if (p_smp->method != IB_MAD_METHOD_GET_RESP) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F11: "
			"Unsupported method 0x%X\n", p_smp->method);
		goto Exit;
	}

	p_smi = ib_smp_get_payload_ptr(p_smp);
	p_sm_tbl = &sm->p_subn->sm_guid_tbl;
	port_guid = p_smi->guid;

	osm_dump_sm_info(sm->p_log, p_smi, OSM_LOG_DEBUG);

	/* Check that the sm_key of the found SM is the same as ours,
	   or is zero. If not - OpenSM cannot continue with configuration!. */
	if (p_smi->sm_key != 0 && p_smi->sm_key != sm->p_subn->opt.sm_key) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F18: "
			"Got SM with sm_key that doesn't match our "
			"local key. Exiting\n");
		osm_log(sm->p_log, OSM_LOG_SYS,
			"Found remote SM with non-matching sm_key. Exiting\n");
		osm_exit_flag = TRUE;
		goto Exit;
	}

	/* Determine if we already have another SM object for this SM. */
	CL_PLOCK_EXCL_ACQUIRE(sm->p_lock);

	p_port = osm_get_port_by_guid(sm->p_subn, port_guid);
	if (!p_port) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F12: "
			"No port object for this SM\n");
		goto _unlock_and_exit;
	}

	if (osm_port_get_guid(p_port) != p_smi->guid) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F13: "
			"Bogus SM port GUID\n\t\t\t\tExpected 0x%016" PRIx64
			", Received 0x%016" PRIx64 "\n",
			cl_ntoh64(osm_port_get_guid(p_port)),
			cl_ntoh64(p_smi->guid));
		goto _unlock_and_exit;
	}

	if (port_guid == sm->p_subn->sm_port_guid) {
		OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
			"Self query response received - SM port 0x%016" PRIx64
			"\n", cl_ntoh64(port_guid));
		goto _unlock_and_exit;
	}

	p_sm = (osm_remote_sm_t *) cl_qmap_get(p_sm_tbl, port_guid);
	if (p_sm == (osm_remote_sm_t *) cl_qmap_end(p_sm_tbl)) {
		p_sm = malloc(sizeof(*p_sm));
		if (p_sm == NULL) {
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F14: "
				"Unable to allocate SM object\n");
			goto _unlock_and_exit;
		}

		osm_remote_sm_init(p_sm, p_port, p_smi);

		cl_qmap_insert(p_sm_tbl, port_guid, &p_sm->map_item);
	} else
		/* We already know this SM. Update the SMInfo attribute. */
		p_sm->smi = *p_smi;

	__osm_sminfo_rcv_process_get_sm(sm, p_sm,
					osm_madw_get_smi_context_ptr(p_madw)->light_sweep);

_unlock_and_exit:
	CL_PLOCK_RELEASE(sm->p_lock);

Exit:
	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_sminfo_rcv_process_set_response(IN osm_sm_t * sm,
				      IN const osm_madw_t * const p_madw)
{
	const ib_smp_t *p_smp;
	const ib_sm_info_t *p_smi;

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_madw);

	p_smp = osm_madw_get_smp_ptr(p_madw);

	if (p_smp->method != IB_MAD_METHOD_GET_RESP) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F16: "
			"Unsupported method 0x%X\n", p_smp->method);
		goto Exit;
	}

	p_smi = ib_smp_get_payload_ptr(p_smp);
	osm_dump_sm_info(sm->p_log, p_smi, OSM_LOG_DEBUG);

	/* Check the AttributeModifier */
	if (p_smp->attr_mod != IB_SMINFO_ATTR_MOD_HANDOVER) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F17: "
			"Unsupported attribute modifier 0x%X\n",
			p_smp->attr_mod);
		goto Exit;
	}

	/* This is a response on a HANDOVER request - Nothing to do. */

Exit:
	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 **********************************************************************/
void osm_sminfo_rcv_process(IN void *context, IN void *data)
{
	osm_sm_t *sm = context;
	osm_madw_t *p_madw = data;
	ib_smp_t *p_smp;
	osm_smi_context_t *p_smi_context;

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_madw);

	p_smp = osm_madw_get_smp_ptr(p_madw);

	/* Determine if this is a request for our own SMInfo or if
	   this is a response to our request for another SM's SMInfo. */
	if (ib_smp_is_response(p_smp)) {
		const ib_sm_info_t *p_smi = ib_smp_get_payload_ptr(p_smp);

		/* Get the context - to see if this is a response to a Get or Set method */
		p_smi_context = osm_madw_get_smi_context_ptr(p_madw);

		/* Verify that response is from expected port and there is
		   no port moving issue. */
		if (p_smi_context->port_guid != p_smi->guid) {
			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 2F19: "
				"Unexpected SM port GUID in response"
				"\n\t\t\t\tExpected 0x%016" PRIx64
				", Received 0x%016" PRIx64 "\n",
				cl_ntoh64(p_smi_context->port_guid),
				cl_ntoh64(p_smi->guid));
			goto Exit;
		}

		if (p_smi_context->set_method == FALSE)
			/* this is a response to a Get method */
			__osm_sminfo_rcv_process_get_response(sm, p_madw);
		else
			/* this is a response to a Set method */
			__osm_sminfo_rcv_process_set_response(sm, p_madw);
	} else if (p_smp->method == IB_MAD_METHOD_GET)
		/* This is a SubnGet request */
		__osm_sminfo_rcv_process_get_request(sm, p_madw);
	else
		/* This should be a SubnSet request */
		__osm_sminfo_rcv_process_set_request(sm, p_madw);

Exit:
	OSM_LOG_EXIT(sm->p_log);
}
