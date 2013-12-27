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
 *    Implementation of osm_sm_state_mgr_t.
 * This file implements the SM State Manager object.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <string.h>
#include <time.h>
#include <iba/ib_types.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_debug.h>
#include <opensm/osm_sm.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_switch.h>
#include <opensm/osm_log.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_helper.h>
#include <opensm/osm_msgdef.h>
#include <opensm/osm_node.h>
#include <opensm/osm_port.h>
#include <vendor/osm_vendor_api.h>
#include <opensm/osm_helper.h>
#include <opensm/osm_opensm.h>

/**********************************************************************
 **********************************************************************/
void osm_report_sm_state(osm_sm_t * sm)
{
	char buf[64];
	const char *state_str = osm_get_sm_mgr_state_str(sm->p_subn->sm_state);

	osm_log(sm->p_log, OSM_LOG_SYS, "Entering %s state\n", state_str);
	snprintf(buf, sizeof(buf), "ENTERING SM %s STATE", state_str);
	OSM_LOG_MSG_BOX(sm->p_log, OSM_LOG_VERBOSE, buf);
}

/**********************************************************************
 **********************************************************************/
static void __osm_sm_state_mgr_send_master_sm_info_req(osm_sm_t * sm)
{
	osm_madw_context_t context;
	const osm_port_t *p_port;
	ib_api_status_t status;

	OSM_LOG_ENTER(sm->p_log);

	memset(&context, 0, sizeof(context));
	if (sm->p_subn->sm_state == IB_SMINFO_STATE_STANDBY) {
		/*
		 * We are in STANDBY state - this means we need to poll on the master
		 * SM (according to master_guid)
		 * Send a query of SubnGet(SMInfo) to the subn master_sm_base_lid object.
		 */
		p_port = osm_get_port_by_guid(sm->p_subn, sm->master_sm_guid);
	} else {
		/*
		 * We are not in STANDBY - this means we are in MASTER state - so we need
		 * to poll on the SM that is saved in p_polling_sm under sm.
		 * Send a query of SubnGet(SMInfo) to that SM.
		 */
		p_port = sm->p_polling_sm->p_port;
	}
	if (p_port == NULL) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3203: "
			"No port object for GUID 0x%016" PRIx64 "\n",
			cl_ntoh64(sm->master_sm_guid));
		goto Exit;
	}

	context.smi_context.port_guid = p_port->guid;
	context.smi_context.set_method = FALSE;

	status = osm_req_get(sm, osm_physp_get_dr_path_ptr(p_port->p_physp),
			     IB_MAD_ATTR_SM_INFO, 0, CL_DISP_MSGID_NONE,
			     &context);

	if (status != IB_SUCCESS)
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3204: "
			"Failure requesting SMInfo (%s)\n",
			ib_get_err_str(status));

Exit:
	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 **********************************************************************/
static void __osm_sm_state_mgr_start_polling(osm_sm_t * sm)
{
	uint32_t timeout = sm->p_subn->opt.sminfo_polling_timeout;
	cl_status_t cl_status;

	OSM_LOG_ENTER(sm->p_log);

	/*
	 * Init the retry_number back to zero - need to restart counting
	 */
	sm->retry_number = 0;

	/*
	 * Send a SubnGet(SMInfo) query to the current (or new) master found.
	 */
	__osm_sm_state_mgr_send_master_sm_info_req(sm);

	/*
	 * Start a timer that will wake up every sminfo_polling_timeout milliseconds.
	 * The callback of the timer will send a SubnGet(SMInfo) to the Master SM
	 * and restart the timer
	 */
	cl_status = cl_timer_start(&sm->polling_timer, timeout);
	if (cl_status != CL_SUCCESS)
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3210: "
			"Failed to start timer\n");

	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 **********************************************************************/
void osm_sm_state_mgr_polling_callback(IN void *context)
{
	osm_sm_t *sm = context;
	uint32_t timeout = sm->p_subn->opt.sminfo_polling_timeout;
	cl_status_t cl_status;

	OSM_LOG_ENTER(sm->p_log);

	/*
	 * We can be here in one of two cases:
	 * 1. We are a STANDBY sm polling on the master SM.
	 * 2. We are a MASTER sm, waiting for a handover from a remote master sm.
	 * If we are not in one of these cases - don't need to restart the poller.
	 */
	if (!((sm->p_subn->sm_state == IB_SMINFO_STATE_MASTER &&
	       sm->p_polling_sm != NULL) ||
	      (sm->p_subn->sm_state == IB_SMINFO_STATE_STANDBY)))
		goto Exit;

	/*
	 * If we are a STANDBY sm and the osm_exit_flag is set, then let's
	 * signal the subnet_up. This is relevant for the case of running only
	 * once. In that case - the program is stuck until this signal is
	 * received. In other cases - it is not relevant whether or not the
	 * signal is on - since we are currently in exit flow
	 */
	if (sm->p_subn->sm_state == IB_SMINFO_STATE_STANDBY && osm_exit_flag) {
		OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
			"Signalling subnet_up_event\n");
		cl_event_signal(&sm->subnet_up_event);
		goto Exit;
	}

	/*
	 * Incr the retry number.
	 * If it reached the max_retry_number in the subnet opt - call
	 * osm_sm_state_mgr_process with signal OSM_SM_SIGNAL_POLLING_TIMEOUT
	 */
	sm->retry_number++;
	OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
		"Retry number:%d\n", sm->retry_number);

	if (sm->retry_number >= sm->p_subn->opt.polling_retry_number) {
		OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
			"Reached polling_retry_number value in retry_number. "
			"Go to DISCOVERY state\n");
		osm_sm_state_mgr_process(sm, OSM_SM_SIGNAL_POLLING_TIMEOUT);
		goto Exit;
	}

	/* Send a SubnGet(SMInfo) request to the remote sm (depends on our state) */
	__osm_sm_state_mgr_send_master_sm_info_req(sm);

	/* restart the timer */
	cl_status = cl_timer_start(&sm->polling_timer, timeout);
	if (cl_status != CL_SUCCESS)
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3211: "
			"Failed to restart timer\n");

Exit:
	OSM_LOG_EXIT(sm->p_log);
	return;
}

/**********************************************************************
 **********************************************************************/
static void __osm_sm_state_mgr_signal_error(osm_sm_t * sm,
					    IN const osm_sm_signal_t signal)
{
	OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3207: "
		"Invalid signal %s in state %s\n",
		osm_get_sm_mgr_signal_str(signal),
		osm_get_sm_mgr_state_str(sm->p_subn->sm_state));
}

/**********************************************************************
 **********************************************************************/
void osm_sm_state_mgr_signal_master_is_alive(osm_sm_t * sm)
{
	OSM_LOG_ENTER(sm->p_log);
	sm->retry_number = 0;
	OSM_LOG_EXIT(sm->p_log);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t osm_sm_state_mgr_process(osm_sm_t * sm,
					 IN osm_sm_signal_t signal)
{
	ib_api_status_t status = IB_SUCCESS;

	CL_ASSERT(sm);

	OSM_LOG_ENTER(sm->p_log);

	/*
	 * The state lock prevents many race conditions from screwing
	 * up the state transition process.
	 */
	cl_spinlock_acquire(&sm->state_lock);

	OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
		"Received signal %s in state %s\n",
		osm_get_sm_mgr_signal_str(signal),
		osm_get_sm_mgr_state_str(sm->p_subn->sm_state));

	switch (sm->p_subn->sm_state) {
	case IB_SMINFO_STATE_DISCOVERING:
		switch (signal) {
		case OSM_SM_SIGNAL_DISCOVERY_COMPLETED:
			/*
			 * Update the state of the SM to MASTER
			 */
			/* Turn on the first_time_master_sweep flag */
			sm->p_subn->first_time_master_sweep = TRUE;
			sm->p_subn->sm_state = IB_SMINFO_STATE_MASTER;
			osm_report_sm_state(sm);
			/*
			 * Make sure to set the subnet master_sm_base_lid
			 * to the sm_base_lid value
			 */
			sm->p_subn->master_sm_base_lid =
			    sm->p_subn->sm_base_lid;
			break;
		case OSM_SM_SIGNAL_MASTER_OR_HIGHER_SM_DETECTED:
			/*
			 * Finished all discovery actions - move to STANDBY
			 * start the polling
			 */
			sm->p_subn->sm_state = IB_SMINFO_STATE_STANDBY;
			osm_report_sm_state(sm);
			/*
			 * Since another SM is doing the LFT config - we should not
			 * ignore the results of it
			 */
			sm->p_subn->ignore_existing_lfts = FALSE;

			__osm_sm_state_mgr_start_polling(sm);
			break;
		case OSM_SM_SIGNAL_HANDOVER:
			/*
			 * Do nothing. We will discover it later on. If we already discovered
			 * this SM, and got the HANDOVER - this means the remote SM is of
			 * lower priority. In this case we will stop polling it (since it is
			 * a lower priority SM in STANDBY state).
			 */
			break;
		default:
			__osm_sm_state_mgr_signal_error(sm, signal);
			status = IB_INVALID_PARAMETER;
			break;
		}
		break;

	case IB_SMINFO_STATE_STANDBY:
		switch (signal) {
		case OSM_SM_SIGNAL_POLLING_TIMEOUT:
		case OSM_SM_SIGNAL_DISCOVER:
			/*
			 * case 1: Polling timeout occured - this means that the Master SM
			 * is no longer alive.
			 * case 2: Got a signal to move to DISCOVERING
			 * Move to DISCOVERING state and start sweeping
			 */
			sm->p_subn->sm_state = IB_SMINFO_STATE_DISCOVERING;
			osm_report_sm_state(sm);
			sm->p_subn->coming_out_of_standby = TRUE;
			osm_sm_signal(sm, OSM_SIGNAL_SWEEP);
			break;
		case OSM_SM_SIGNAL_DISABLE:
			/*
			 * Update the state to NOT_ACTIVE
			 */
			sm->p_subn->sm_state = IB_SMINFO_STATE_NOTACTIVE;
			osm_report_sm_state(sm);
			osm_vendor_set_sm(sm->mad_ctrl.h_bind, FALSE);
			break;
		case OSM_SM_SIGNAL_HANDOVER:
			/*
			 * Update the state to MASTER, and start sweeping
			 * OPTIONAL: send ACKNOWLEDGE
			 */
			/* Turn on the first_time_master_sweep flag */
			sm->p_subn->first_time_master_sweep = TRUE;
			/* Turn on the force_heavy_sweep - we want a
			 * heavy sweep to occur on the first sweep of this SM. */
			sm->p_subn->force_heavy_sweep = TRUE;

			sm->p_subn->sm_state = IB_SMINFO_STATE_MASTER;
			osm_report_sm_state(sm);
			/*
			 * Make sure to set the subnet master_sm_base_lid
			 * to the sm_base_lid value
			 */
			sm->p_subn->master_sm_base_lid =
			    sm->p_subn->sm_base_lid;
			sm->p_subn->coming_out_of_standby = TRUE;
			osm_sm_signal(sm, OSM_SIGNAL_SWEEP);
			break;
		case OSM_SM_SIGNAL_ACKNOWLEDGE:
			/*
			 * Do nothing - already moved to STANDBY
			 */
			break;
		default:
			__osm_sm_state_mgr_signal_error(sm, signal);
			status = IB_INVALID_PARAMETER;
			break;
		}
		break;

	case IB_SMINFO_STATE_NOTACTIVE:
		switch (signal) {
		case OSM_SM_SIGNAL_STANDBY:
			/*
			 * Update the state to STANDBY
			 * start the polling
			 */
			sm->p_subn->sm_state = IB_SMINFO_STATE_STANDBY;
			osm_report_sm_state(sm);
			__osm_sm_state_mgr_start_polling(sm);
			break;
		default:
			__osm_sm_state_mgr_signal_error(sm, signal);
			status = IB_INVALID_PARAMETER;
			break;
		}
		break;

	case IB_SMINFO_STATE_MASTER:
		switch (signal) {
		case OSM_SM_SIGNAL_POLLING_TIMEOUT:
			/*
			 * we received a polling timeout - this means that we waited for
			 * a remote master sm to send us a handover, but didn't get it, and
			 * didn't get a response from that remote sm.
			 * We want to force a heavy sweep - hopefully this occurred because
			 * the remote sm died, and we'll find this out and configure the
			 * subnet after a heavy sweep.
			 * We also want to clear the p_polling_sm object - since we are
			 * done polling on that remote sm - we are sweeping again.
			 */
		case OSM_SM_SIGNAL_HANDOVER:
			/*
			 * If we received a handover in a master state - then we want to
			 * force a heavy sweep. This means that either we are in a sweep
			 * currently - in this case - no change, or we are in idle state -
			 * since we recognized a master SM before - so we want to make a
			 * heavy sweep and reconfigure the new subnet.
			 * We also want to clear the p_polling_sm object - since we are
			 * done polling on that remote sm - we got a handover from it.
			 */
			OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
				"Forcing heavy sweep. "
				"Received OSM_SM_SIGNAL_HANDOVER or OSM_SM_SIGNAL_POLLING_TIMEOUT\n");
			sm->p_polling_sm = NULL;
			sm->p_subn->force_heavy_sweep = TRUE;
			osm_sm_signal(sm, OSM_SIGNAL_SWEEP);
			break;
		case OSM_SM_SIGNAL_HANDOVER_SENT:
			/*
			 * Just sent a HANDOVER signal - move to STANDBY
			 * start the polling
			 */
			sm->p_subn->sm_state = IB_SMINFO_STATE_STANDBY;
			osm_report_sm_state(sm);
			__osm_sm_state_mgr_start_polling(sm);
			break;
		case OSM_SM_SIGNAL_WAIT_FOR_HANDOVER:
			/*
			 * We found a remote master SM, and we are waiting for it
			 * to handover the mastership to us. Need to start polling
			 * on that SM, to make sure it is alive, if it isn't - then
			 * we should move back to discovering, since something must
			 * have happened to it.
			 */
			__osm_sm_state_mgr_start_polling(sm);
			break;
		case OSM_SM_SIGNAL_DISCOVER:
			sm->p_subn->sm_state = IB_SMINFO_STATE_DISCOVERING;
			osm_report_sm_state(sm);
			break;
		default:
			__osm_sm_state_mgr_signal_error(sm, signal);
			status = IB_INVALID_PARAMETER;
			break;
		}
		break;

	default:
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3208: "
			"Invalid state %s\n",
			osm_get_sm_mgr_state_str(sm->p_subn->sm_state));

	}

	cl_spinlock_release(&sm->state_lock);

	OSM_LOG_EXIT(sm->p_log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t osm_sm_state_mgr_check_legality(osm_sm_t * sm,
						IN osm_sm_signal_t signal)
{
	ib_api_status_t status = IB_SUCCESS;

	CL_ASSERT(sm);

	OSM_LOG_ENTER(sm->p_log);

	/*
	 * The state lock prevents many race conditions from screwing
	 * up the state transition process.
	 */
	cl_spinlock_acquire(&sm->state_lock);

	OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
		"Received signal %s in state %s\n",
		osm_get_sm_mgr_signal_str(signal),
		osm_get_sm_mgr_state_str(sm->p_subn->sm_state));

	switch (sm->p_subn->sm_state) {
	case IB_SMINFO_STATE_DISCOVERING:
		switch (signal) {
		case OSM_SM_SIGNAL_DISCOVERY_COMPLETED:
		case OSM_SM_SIGNAL_MASTER_OR_HIGHER_SM_DETECTED:
		case OSM_SM_SIGNAL_HANDOVER:
			status = IB_SUCCESS;
			break;
		default:
			__osm_sm_state_mgr_signal_error(sm, signal);
			status = IB_INVALID_PARAMETER;
			break;
		}
		break;

	case IB_SMINFO_STATE_STANDBY:
		switch (signal) {
		case OSM_SM_SIGNAL_POLLING_TIMEOUT:
		case OSM_SM_SIGNAL_DISCOVER:
		case OSM_SM_SIGNAL_DISABLE:
		case OSM_SM_SIGNAL_HANDOVER:
		case OSM_SM_SIGNAL_ACKNOWLEDGE:
			status = IB_SUCCESS;
			break;
		default:
			__osm_sm_state_mgr_signal_error(sm, signal);
			status = IB_INVALID_PARAMETER;
			break;
		}
		break;

	case IB_SMINFO_STATE_NOTACTIVE:
		switch (signal) {
		case OSM_SM_SIGNAL_STANDBY:
			status = IB_SUCCESS;
			break;
		default:
			__osm_sm_state_mgr_signal_error(sm, signal);
			status = IB_INVALID_PARAMETER;
			break;
		}
		break;

	case IB_SMINFO_STATE_MASTER:
		switch (signal) {
		case OSM_SM_SIGNAL_HANDOVER:
		case OSM_SM_SIGNAL_HANDOVER_SENT:
			status = IB_SUCCESS;
			break;
		default:
			__osm_sm_state_mgr_signal_error(sm, signal);
			status = IB_INVALID_PARAMETER;
			break;
		}
		break;

	default:
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3209: "
			"Invalid state %s\n",
			osm_get_sm_mgr_state_str(sm->p_subn->sm_state));
		status = IB_INVALID_PARAMETER;

	}

	cl_spinlock_release(&sm->state_lock);

	OSM_LOG_EXIT(sm->p_log);
	return (status);
}
