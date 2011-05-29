/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
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
 *    Implementation of osm_sm_t.
 * This object represents the SM Receiver object.
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
#include <complib/cl_thread.h>
#include <opensm/osm_sm.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_log.h>
#include <opensm/osm_node.h>
#include <opensm/osm_msgdef.h>
#include <opensm/osm_mcm_info.h>
#include <opensm/osm_perfmgr.h>
#include <opensm/osm_opensm.h>

#define  OSM_SM_INITIAL_TID_VALUE 0x1233

extern void osm_lft_rcv_process(IN void *context, IN void *data);
extern void osm_mft_rcv_process(IN void *context, IN void *data);
extern void osm_nd_rcv_process(IN void *context, IN void *data);
extern void osm_ni_rcv_process(IN void *context, IN void *data);
extern void osm_pkey_rcv_process(IN void *context, IN void *data);
extern void osm_pi_rcv_process(IN void *context, IN void *data);
extern void osm_slvl_rcv_process(IN void *context, IN void *p_data);
extern void osm_sminfo_rcv_process(IN void *context, IN void *data);
extern void osm_si_rcv_process(IN void *context, IN void *data);
extern void osm_trap_rcv_process(IN void *context, IN void *data);
extern void osm_vla_rcv_process(IN void *context, IN void *data);

extern void osm_state_mgr_process(IN osm_sm_t * sm, IN osm_signal_t signal);
extern void osm_sm_state_mgr_polling_callback(IN void *context);

/**********************************************************************
 **********************************************************************/
static void osm_sm_process(osm_sm_t * sm, osm_signal_t signal)
{
#ifdef ENABLE_OSM_PERF_MGR
	if (signal == OSM_SIGNAL_PERFMGR_SWEEP)
		osm_perfmgr_process(&sm->p_subn->p_osm->perfmgr);
	else
#endif
		osm_state_mgr_process(sm, signal);
}

static void __osm_sm_sweeper(IN void *p_ptr)
{
	ib_api_status_t status;
	osm_sm_t *const p_sm = (osm_sm_t *) p_ptr;
	unsigned signals, i;

	OSM_LOG_ENTER(p_sm->p_log);

	while (p_sm->thread_state == OSM_THREAD_STATE_RUN) {
		/*
		 * Wait on the event with a timeout.
		 * Sweeps may be initiated "off schedule" by simply
		 * signaling the event.
		 */
		status = cl_event_wait_on(&p_sm->signal_event,
					  EVENT_NO_TIMEOUT, TRUE);

		if (status == CL_SUCCESS)
			OSM_LOG(p_sm->p_log, OSM_LOG_DEBUG,
				"Off schedule sweep signalled\n");
		else if (status != CL_TIMEOUT) {
			OSM_LOG(p_sm->p_log, OSM_LOG_ERROR, "ERR 2E01: "
				"Event wait failed (%s)\n",
				CL_STATUS_MSG(status));
			continue;
		}

		if (osm_exit_flag)
			break;

		cl_spinlock_acquire(&p_sm->signal_lock);
		signals = p_sm->signal_mask;
		p_sm->signal_mask = 0;
		cl_spinlock_release(&p_sm->signal_lock);

		for (i = 0; signals; signals >>= 1, i++)
			if (signals & 1)
				osm_sm_process(p_sm, i);
	}

	OSM_LOG_EXIT(p_sm->p_log);
}

static void sm_sweep(void *arg)
{
	osm_sm_t *sm = arg;

	/*  do the sweep only if we are in MASTER state */
	if (sm->p_subn->sm_state == IB_SMINFO_STATE_MASTER ||
	    sm->p_subn->sm_state == IB_SMINFO_STATE_DISCOVERING)
		osm_sm_signal(sm, OSM_SIGNAL_SWEEP);
	cl_timer_start(&sm->sweep_timer, sm->p_subn->opt.sweep_interval * 1000);
}

static void sweep_fail_process(IN void *context, IN void *p_data)
{
	osm_sm_t *sm = context;

	OSM_LOG(sm->p_log, OSM_LOG_DEBUG, "light sweep failed\n");
	sm->p_subn->force_heavy_sweep = TRUE;
}

/**********************************************************************
 **********************************************************************/
void osm_sm_construct(IN osm_sm_t * const p_sm)
{
	memset(p_sm, 0, sizeof(*p_sm));
	p_sm->thread_state = OSM_THREAD_STATE_NONE;
	p_sm->sm_trans_id = OSM_SM_INITIAL_TID_VALUE;
	cl_spinlock_construct(&p_sm->signal_lock);
	cl_spinlock_construct(&p_sm->state_lock);
	cl_timer_construct(&p_sm->polling_timer);
	cl_event_construct(&p_sm->signal_event);
	cl_event_construct(&p_sm->subnet_up_event);
	cl_event_wheel_construct(&p_sm->trap_aging_tracker);
	cl_thread_construct(&p_sm->sweeper);
	cl_spinlock_construct(&p_sm->mgrp_lock);
	osm_sm_mad_ctrl_construct(&p_sm->mad_ctrl);
	osm_lid_mgr_construct(&p_sm->lid_mgr);
	osm_ucast_mgr_construct(&p_sm->ucast_mgr);
}

/**********************************************************************
 **********************************************************************/
void osm_sm_shutdown(IN osm_sm_t * const p_sm)
{
	boolean_t signal_event = FALSE;

	OSM_LOG_ENTER(p_sm->p_log);

	/*
	 * Signal our threads that we're leaving.
	 */
	if (p_sm->thread_state != OSM_THREAD_STATE_NONE)
		signal_event = TRUE;

	p_sm->thread_state = OSM_THREAD_STATE_EXIT;

	/*
	 * Don't trigger unless event has been initialized.
	 * Destroy the thread before we tear down the other objects.
	 */
	if (signal_event)
		cl_event_signal(&p_sm->signal_event);

	cl_timer_stop(&p_sm->polling_timer);
	cl_timer_stop(&p_sm->sweep_timer);
	cl_thread_destroy(&p_sm->sweeper);

	/*
	 * Always destroy controllers before the corresponding
	 * receiver to guarantee that all callbacks from the
	 * dispatcher are complete.
	 */
	osm_sm_mad_ctrl_destroy(&p_sm->mad_ctrl);
	cl_disp_unregister(p_sm->ni_disp_h);
	cl_disp_unregister(p_sm->pi_disp_h);
	cl_disp_unregister(p_sm->si_disp_h);
	cl_disp_unregister(p_sm->nd_disp_h);
	cl_disp_unregister(p_sm->lft_disp_h);
	cl_disp_unregister(p_sm->mft_disp_h);
	cl_disp_unregister(p_sm->sm_info_disp_h);
	cl_disp_unregister(p_sm->trap_disp_h);
	cl_disp_unregister(p_sm->slvl_disp_h);
	cl_disp_unregister(p_sm->vla_disp_h);
	cl_disp_unregister(p_sm->pkey_disp_h);
	cl_disp_unregister(p_sm->sweep_fail_disp_h);

	OSM_LOG_EXIT(p_sm->p_log);
}

/**********************************************************************
 **********************************************************************/
void osm_sm_destroy(IN osm_sm_t * const p_sm)
{
	OSM_LOG_ENTER(p_sm->p_log);
	osm_lid_mgr_destroy(&p_sm->lid_mgr);
	osm_ucast_mgr_destroy(&p_sm->ucast_mgr);
	cl_event_wheel_destroy(&p_sm->trap_aging_tracker);
	cl_timer_destroy(&p_sm->sweep_timer);
	cl_timer_destroy(&p_sm->polling_timer);
	cl_event_destroy(&p_sm->signal_event);
	cl_event_destroy(&p_sm->subnet_up_event);
	cl_spinlock_destroy(&p_sm->signal_lock);
	cl_spinlock_destroy(&p_sm->mgrp_lock);
	cl_spinlock_destroy(&p_sm->state_lock);

	osm_log(p_sm->p_log, OSM_LOG_SYS, "Exiting SM\n");	/* Format Waived */
	OSM_LOG_EXIT(p_sm->p_log);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
osm_sm_init(IN osm_sm_t * const p_sm,
	    IN osm_subn_t * const p_subn,
	    IN osm_db_t * const p_db,
	    IN osm_vendor_t * const p_vendor,
	    IN osm_mad_pool_t * const p_mad_pool,
	    IN osm_vl15_t * const p_vl15,
	    IN osm_log_t * const p_log,
	    IN osm_stats_t * const p_stats,
	    IN cl_dispatcher_t * const p_disp, IN cl_plock_t * const p_lock)
{
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(p_log);

	p_sm->p_subn = p_subn;
	p_sm->p_db = p_db;
	p_sm->p_vendor = p_vendor;
	p_sm->p_mad_pool = p_mad_pool;
	p_sm->p_vl15 = p_vl15;
	p_sm->p_log = p_log;
	p_sm->p_disp = p_disp;
	p_sm->p_lock = p_lock;

	status = cl_spinlock_init(&p_sm->signal_lock);
	if (status != CL_SUCCESS)
		goto Exit;

	status = cl_spinlock_init(&p_sm->state_lock);
	if (status != CL_SUCCESS)
		goto Exit;

	status = cl_event_init(&p_sm->signal_event, FALSE);
	if (status != CL_SUCCESS)
		goto Exit;

	status = cl_event_init(&p_sm->subnet_up_event, FALSE);
	if (status != CL_SUCCESS)
		goto Exit;

	status = cl_timer_init(&p_sm->sweep_timer, sm_sweep, p_sm);
	if (status != CL_SUCCESS)
		goto Exit;

	status = cl_timer_init(&p_sm->polling_timer,
			       osm_sm_state_mgr_polling_callback, p_sm);
	if (status != CL_SUCCESS)
		goto Exit;

	cl_qlist_init(&p_sm->mgrp_list);

	status = cl_spinlock_init(&p_sm->mgrp_lock);
	if (status != CL_SUCCESS)
		goto Exit;

	status = osm_sm_mad_ctrl_init(&p_sm->mad_ctrl,
				      p_sm->p_subn,
				      p_sm->p_mad_pool,
				      p_sm->p_vl15,
				      p_sm->p_vendor,
				      p_log, p_stats, p_lock, p_disp);
	if (status != IB_SUCCESS)
		goto Exit;

	status = cl_event_wheel_init(&p_sm->trap_aging_tracker);
	if (status != IB_SUCCESS)
		goto Exit;

	status = osm_lid_mgr_init(&p_sm->lid_mgr, p_sm);
	if (status != IB_SUCCESS)
		goto Exit;

	status = osm_ucast_mgr_init(&p_sm->ucast_mgr, p_sm);
	if (status != IB_SUCCESS)
		goto Exit;

	p_sm->sweep_fail_disp_h = cl_disp_register(p_disp,
						   OSM_MSG_LIGHT_SWEEP_FAIL,
						   sweep_fail_process, p_sm);
	if (p_sm->sweep_fail_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sm->ni_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_NODE_INFO,
					   osm_ni_rcv_process, p_sm);
	if (p_sm->ni_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sm->pi_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_PORT_INFO,
					   osm_pi_rcv_process, p_sm);
	if (p_sm->pi_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sm->si_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_SWITCH_INFO,
					   osm_si_rcv_process, p_sm);
	if (p_sm->si_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sm->nd_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_NODE_DESC,
					   osm_nd_rcv_process, p_sm);
	if (p_sm->nd_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sm->lft_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_LFT,
					    osm_lft_rcv_process, p_sm);
	if (p_sm->lft_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sm->mft_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_MFT,
					    osm_mft_rcv_process, p_sm);
	if (p_sm->mft_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sm->sm_info_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_SM_INFO,
						osm_sminfo_rcv_process, p_sm);
	if (p_sm->sm_info_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sm->trap_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_NOTICE,
					     osm_trap_rcv_process, p_sm);
	if (p_sm->trap_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sm->slvl_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_SLVL,
					     osm_slvl_rcv_process, p_sm);
	if (p_sm->slvl_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sm->vla_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_VL_ARB,
					    osm_vla_rcv_process, p_sm);
	if (p_sm->vla_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sm->pkey_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_PKEY,
					     osm_pkey_rcv_process, p_sm);
	if (p_sm->pkey_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_subn->sm_state = p_subn->opt.sm_inactive ?
	    IB_SMINFO_STATE_NOTACTIVE : IB_SMINFO_STATE_DISCOVERING;
	osm_report_sm_state(p_sm);

	/*
	 * Now that the component objects are initialized, start
	 * the sweeper thread if the user wants sweeping.
	 */
	p_sm->thread_state = OSM_THREAD_STATE_RUN;
	status = cl_thread_init(&p_sm->sweeper, __osm_sm_sweeper, p_sm,
				"opensm sweeper");
	if (status != IB_SUCCESS)
		goto Exit;

	if (p_sm->p_subn->opt.sweep_interval)
		cl_timer_start(&p_sm->sweep_timer,
			       p_sm->p_subn->opt.sweep_interval * 1000);

Exit:
	OSM_LOG_EXIT(p_log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
void osm_sm_signal(osm_sm_t * p_sm, osm_signal_t signal)
{
	cl_spinlock_acquire(&p_sm->signal_lock);
	p_sm->signal_mask |= 1 << signal;
	cl_event_signal(&p_sm->signal_event);
	cl_spinlock_release(&p_sm->signal_lock);
}

/**********************************************************************
 **********************************************************************/
void osm_sm_sweep(IN osm_sm_t * const p_sm)
{
	OSM_LOG_ENTER(p_sm->p_log);
	osm_sm_signal(p_sm, OSM_SIGNAL_SWEEP);
	OSM_LOG_EXIT(p_sm->p_log);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
osm_sm_bind(IN osm_sm_t * const p_sm, IN const ib_net64_t port_guid)
{
	ib_api_status_t status;

	OSM_LOG_ENTER(p_sm->p_log);

	status = osm_sm_mad_ctrl_bind(&p_sm->mad_ctrl, port_guid);

	if (status != IB_SUCCESS) {
		OSM_LOG(p_sm->p_log, OSM_LOG_ERROR, "ERR 2E10: "
			"SM MAD Controller bind failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(p_sm->p_log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
__osm_sm_mgrp_process(IN osm_sm_t * const p_sm,
		      IN osm_mgrp_t * const p_mgrp,
		      IN const ib_net64_t port_guid,
		      IN osm_mcast_req_type_t req_type)
{
	osm_mcast_mgr_ctxt_t *ctx;

	/*
	 * 'Schedule' all the QP0 traffic for when the state manager
	 * isn't busy trying to do something else.
	 */
	ctx = malloc(sizeof(*ctx));
	if (!ctx)
		return IB_ERROR;
	memset(ctx, 0, sizeof(*ctx));
	ctx->mlid = p_mgrp->mlid;
	ctx->req_type = req_type;
	ctx->port_guid = port_guid;

	cl_spinlock_acquire(&p_sm->mgrp_lock);
	cl_qlist_insert_tail(&p_sm->mgrp_list, &ctx->list_item);
	cl_spinlock_release(&p_sm->mgrp_lock);

	osm_sm_signal(p_sm, OSM_SIGNAL_IDLE_TIME_PROCESS_REQUEST);

	return IB_SUCCESS;
}

/**********************************************************************
 **********************************************************************/
static ib_api_status_t
__osm_sm_mgrp_connect(IN osm_sm_t * const p_sm,
		      IN osm_mgrp_t * const p_mgrp,
		      IN const ib_net64_t port_guid,
		      IN osm_mcast_req_type_t req_type)
{
	return __osm_sm_mgrp_process(p_sm, p_mgrp, port_guid, req_type);
}

/**********************************************************************
 **********************************************************************/
static void
__osm_sm_mgrp_disconnect(IN osm_sm_t * const p_sm,
			 IN osm_mgrp_t * const p_mgrp,
			 IN const ib_net64_t port_guid)
{
	__osm_sm_mgrp_process(p_sm, p_mgrp, port_guid,
			      OSM_MCAST_REQ_TYPE_LEAVE);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
osm_sm_mcgrp_join(IN osm_sm_t * const p_sm,
		  IN const ib_net16_t mlid,
		  IN const ib_net64_t port_guid,
		  IN osm_mcast_req_type_t req_type)
{
	osm_mgrp_t *p_mgrp;
	osm_port_t *p_port;
	ib_api_status_t status = IB_SUCCESS;
	osm_mcm_info_t *p_mcm;

	OSM_LOG_ENTER(p_sm->p_log);

	OSM_LOG(p_sm->p_log, OSM_LOG_VERBOSE,
		"Port 0x%016" PRIx64 " joining MLID 0x%X\n",
		cl_ntoh64(port_guid), cl_ntoh16(mlid));

	/*
	 * Acquire the port object for the port joining this group.
	 */
	CL_PLOCK_EXCL_ACQUIRE(p_sm->p_lock);
	p_port = osm_get_port_by_guid(p_sm->p_subn, port_guid);
	if (!p_port) {
		CL_PLOCK_RELEASE(p_sm->p_lock);
		OSM_LOG(p_sm->p_log, OSM_LOG_ERROR, "ERR 2E05: "
			"No port object for port 0x%016" PRIx64 "\n",
			cl_ntoh64(port_guid));
		status = IB_INVALID_PARAMETER;
		goto Exit;
	}

	/*
	 * If this multicast group does not already exist, create it.
	 */
	p_mgrp = osm_get_mgrp_by_mlid(p_sm->p_subn, mlid);
	if (!p_mgrp || !osm_mgrp_is_guid(p_mgrp, port_guid)) {
		/*
		 * The group removed or the port is not a
		 * member of the group, then fail immediately.
		 * This can happen since the spinlock is released briefly
		 * before the SA calls this function.
		 */
		CL_PLOCK_RELEASE(p_sm->p_lock);
		OSM_LOG(p_sm->p_log, OSM_LOG_ERROR, "ERR 2E12: "
			"MC group with mlid 0x%x doesn't exist or "
			"port 0x%016" PRIx64 " is not in the group.\n",
			cl_ntoh16(mlid), cl_ntoh64(port_guid));
		status = IB_NOT_FOUND;
		goto Exit;
	}

	/*
	 * Check if the object (according to mlid) already exists on this port.
	 * If it does - then no need to update it again, and no need to
	 * create the mc tree again. Just goto Exit.
	 */
	p_mcm = (osm_mcm_info_t *) cl_qlist_head(&p_port->mcm_list);
	while (p_mcm != (osm_mcm_info_t *) cl_qlist_end(&p_port->mcm_list)) {
		if (p_mcm->mlid == mlid) {
			CL_PLOCK_RELEASE(p_sm->p_lock);
			OSM_LOG(p_sm->p_log, OSM_LOG_DEBUG,
				"Found mlid object for Port:"
				"0x%016" PRIx64 " lid:0x%X\n",
				cl_ntoh64(port_guid), cl_ntoh16(mlid));
			goto Exit;
		}
		p_mcm = (osm_mcm_info_t *) cl_qlist_next(&p_mcm->list_item);
	}

	status = osm_port_add_mgrp(p_port, mlid);
	if (status != IB_SUCCESS) {
		CL_PLOCK_RELEASE(p_sm->p_lock);
		OSM_LOG(p_sm->p_log, OSM_LOG_ERROR, "ERR 2E03: "
			"Unable to associate port 0x%" PRIx64 " to mlid 0x%X\n",
			cl_ntoh64(osm_port_get_guid(p_port)),
			cl_ntoh16(osm_mgrp_get_mlid(p_mgrp)));
		goto Exit;
	}

	status = __osm_sm_mgrp_connect(p_sm, p_mgrp, port_guid, req_type);
	CL_PLOCK_RELEASE(p_sm->p_lock);

Exit:
	OSM_LOG_EXIT(p_sm->p_log);
	return (status);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
osm_sm_mcgrp_leave(IN osm_sm_t * const p_sm,
		   IN const ib_net16_t mlid, IN const ib_net64_t port_guid)
{
	osm_mgrp_t *p_mgrp;
	osm_port_t *p_port;
	ib_api_status_t status = IB_SUCCESS;

	OSM_LOG_ENTER(p_sm->p_log);

	OSM_LOG(p_sm->p_log, OSM_LOG_VERBOSE,
		"Port 0x%" PRIx64 " leaving MLID 0x%X\n",
		cl_ntoh64(port_guid), cl_ntoh16(mlid));

	/*
	 * Acquire the port object for the port leaving this group.
	 */
	CL_PLOCK_EXCL_ACQUIRE(p_sm->p_lock);

	p_port = osm_get_port_by_guid(p_sm->p_subn, port_guid);
	if (!p_port) {
		CL_PLOCK_RELEASE(p_sm->p_lock);
		OSM_LOG(p_sm->p_log, OSM_LOG_ERROR, "ERR 2E04: "
			"No port object for port 0x%" PRIx64 "\n",
			cl_ntoh64(port_guid));
		status = IB_INVALID_PARAMETER;
		goto Exit;
	}

	/*
	 * Get the multicast group object for this group.
	 */
	p_mgrp = osm_get_mgrp_by_mlid(p_sm->p_subn, mlid);
	if (!p_mgrp) {
		CL_PLOCK_RELEASE(p_sm->p_lock);
		OSM_LOG(p_sm->p_log, OSM_LOG_ERROR, "ERR 2E08: "
			"No multicast group for MLID 0x%X\n", cl_ntoh16(mlid));
		status = IB_INVALID_PARAMETER;
		goto Exit;
	}

	/*
	 * Walk the list of ports in the group, and remove the appropriate one.
	 */
	osm_port_remove_mgrp(p_port, mlid);

	__osm_sm_mgrp_disconnect(p_sm, p_mgrp, port_guid);
	CL_PLOCK_RELEASE(p_sm->p_lock);

Exit:
	OSM_LOG_EXIT(p_sm->p_log);
	return (status);
}

void osm_set_sm_priority(osm_sm_t *sm, uint8_t priority)
{
	uint8_t old_pri = sm->p_subn->opt.sm_priority;

	sm->p_subn->opt.sm_priority = priority;

	if (old_pri < priority &&
	    sm->p_subn->sm_state == IB_SMINFO_STATE_STANDBY)
		osm_send_trap144(sm, TRAP_144_MASK_SM_PRIORITY_CHANGE);
}
