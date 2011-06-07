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
 * 	Declaration of osm_sa_t.
 *	This object represents an IBA subnet.
 *	This object is part of the OpenSM family of objects.
 */

#ifndef _OSM_SA_H_
#define _OSM_SA_H_

#include <iba/ib_types.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_event.h>
#include <complib/cl_thread.h>
#include <complib/cl_timer.h>
#include <complib/cl_dispatcher.h>
#include <opensm/osm_stats.h>
#include <opensm/osm_subnet.h>
#include <vendor/osm_vendor_api.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_log.h>
#include <opensm/osm_sa_mad_ctrl.h>
#include <opensm/osm_sm.h>
#include <opensm/osm_multicast.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS
/****h* OpenSM/SA
* NAME
*	SA
*
* DESCRIPTION
*	The SA object encapsulates the information needed by the
*	OpenSM to instantiate a subnet administrator.  The OpenSM allocates
*	one SA object per subnet manager.
*
*	The SA object is thread safe.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* AUTHOR
*	Ranjit Pandit, Intel
*	Anil Keshavamurthy, Intel
*
*********/
/****d* OpenSM: SA/osm_sa_state_t
* NAME
*	osm_sa_state_t
*
* DESCRIPTION
*	Enumerates the possible states of SA object.
*
* SYNOPSIS
*/
typedef enum _osm_sa_state {
	OSM_SA_STATE_INIT = 0,
	OSM_SA_STATE_READY
} osm_sa_state_t;
/***********/

/****s* OpenSM: SM/osm_sa_t
* NAME
*	osm_sa_t
*
* DESCRIPTION
*	Subnet Administration structure.
*
*	This object should be treated as opaque and should
*	be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct osm_sa {
	osm_sa_state_t state;
	osm_sm_t *sm;
	osm_subn_t *p_subn;
	osm_vendor_t *p_vendor;
	osm_log_t *p_log;
	osm_mad_pool_t *p_mad_pool;
	cl_dispatcher_t *p_disp;
	cl_plock_t *p_lock;
	atomic32_t sa_trans_id;
	osm_sa_mad_ctrl_t mad_ctrl;
	cl_timer_t sr_timer;
	cl_disp_reg_handle_t cpi_disp_h;
	cl_disp_reg_handle_t nr_disp_h;
	cl_disp_reg_handle_t pir_disp_h;
	cl_disp_reg_handle_t gir_disp_h;
	cl_disp_reg_handle_t lr_disp_h;
	cl_disp_reg_handle_t pr_disp_h;
	cl_disp_reg_handle_t smir_disp_h;
	cl_disp_reg_handle_t mcmr_disp_h;
	cl_disp_reg_handle_t sr_disp_h;
#if defined (VENDOR_RMPP_SUPPORT) && defined (DUAL_SIDED_RMPP)
	cl_disp_reg_handle_t mpr_disp_h;
#endif
	cl_disp_reg_handle_t infr_disp_h;
	cl_disp_reg_handle_t infir_disp_h;
	cl_disp_reg_handle_t vlarb_disp_h;
	cl_disp_reg_handle_t slvl_disp_h;
	cl_disp_reg_handle_t pkey_disp_h;
	cl_disp_reg_handle_t lft_disp_h;
	cl_disp_reg_handle_t sir_disp_h;
	cl_disp_reg_handle_t mft_disp_h;
} osm_sa_t;
/*
* FIELDS
*	state
*		State of this SA object
*
*	sm
*		Pointer to the Subnet Manager object.
*
*	p_subn
*		Pointer to the Subnet object for this subnet.
*
*	p_vendor
*		Pointer to the vendor specific interfaces object.
*
*	p_log
*		Pointer to the log object.
*
*	p_mad_pool
*		Pointer to the MAD pool.
*
*	p_disp
*		Pointer to dispatcher
*
*	p_lock
*		Pointer to Lock for serialization
*
*	sa_trans_id
*		Transaction ID
*
*	mad_ctrl
*		Mad Controller
*
* SEE ALSO
*	SM object
*********/

/****f* OpenSM: SA/osm_sa_construct
* NAME
*	osm_sa_construct
*
* DESCRIPTION
*	This function constructs an SA object.
*
* SYNOPSIS
*/
void osm_sa_construct(IN osm_sa_t * const p_sa);
/*
* PARAMETERS
*	p_sa
*		[in] Pointer to a SA object to construct.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Allows calling osm_sa_destroy.
*
*	Calling osm_sa_construct is a prerequisite to calling any other
*	method except osm_sa_init.
*
* SEE ALSO
*	SA object, osm_sa_init, osm_sa_destroy
*********/

/****f* OpenSM: SA/osm_sa_shutdown
* NAME
*	osm_sa_shutdown
*
* DESCRIPTION
*	The osm_sa_shutdown function shutdowns an SA, unregistering from all
*  dispatcher messages and unbinding the QP1 mad service
*
* SYNOPSIS
*/
void osm_sa_shutdown(IN osm_sa_t * const p_sa);
/*
* PARAMETERS
*	p_sa
*		[in] Pointer to a SA object to shutdown.
*
* RETURN VALUE
*	This function does not return a value.
*
* SEE ALSO
*	SA object, osm_sa_construct, osm_sa_init
*********/

/****f* OpenSM: SA/osm_sa_destroy
* NAME
*	osm_sa_destroy
*
* DESCRIPTION
*	The osm_sa_destroy function destroys an SA, releasing
*	all resources.
*
* SYNOPSIS
*/
void osm_sa_destroy(IN osm_sa_t * const p_sa);
/*
* PARAMETERS
*	p_sa
*		[in] Pointer to a SA object to destroy.
*
* RETURN VALUE
*	This function does not return a value.
*
* NOTES
*	Performs any necessary cleanup of the specified SA object.
*	Further operations should not be attempted on the destroyed object.
*	This function should only be called after a call to osm_sa_construct or
*	osm_sa_init.
*
* SEE ALSO
*	SA object, osm_sa_construct, osm_sa_init
*********/

/****f* OpenSM: SA/osm_sa_init
* NAME
*	osm_sa_init
*
* DESCRIPTION
*	The osm_sa_init function initializes a SA object for use.
*
* SYNOPSIS
*/
ib_api_status_t osm_sa_init(IN osm_sm_t * const p_sm,
			    IN osm_sa_t * const p_sa,
			    IN osm_subn_t * const p_subn,
			    IN osm_vendor_t * const p_vendor,
			    IN osm_mad_pool_t * const p_mad_pool,
			    IN osm_log_t * const p_log,
			    IN osm_stats_t * const p_stats,
			    IN cl_dispatcher_t * const p_disp,
			    IN cl_plock_t * const p_lock);
/*
* PARAMETERS
*	p_sa
*		[in] Pointer to an osm_sa_t object to initialize.
*
*	p_subn
*		[in] Pointer to the Subnet object for this subnet.
*
*	p_vendor
*		[in] Pointer to the vendor specific interfaces object.
*
*	p_mad_pool
*		[in] Pointer to the MAD pool.
*
*	p_log
*		[in] Pointer to the log object.
*
*	p_stats
*		[in] Pointer to the statistics object.
*
*	p_disp
*		[in] Pointer to the OpenSM central Dispatcher.
*
*	p_lock
*		[in] Pointer to the OpenSM serializing lock.
*
* RETURN VALUES
*	CL_SUCCESS if the SA object was initialized successfully.
*
* NOTES
*	Allows calling other SA methods.
*
* SEE ALSO
*	SA object, osm_sa_construct, osm_sa_destroy
*********/

/****f* OpenSM: SA/osm_sa_bind
* NAME
*	osm_sa_bind
*
* DESCRIPTION
*	Binds the SA object to a port guid.
*
* SYNOPSIS
*/
ib_api_status_t
osm_sa_bind(IN osm_sa_t * const p_sa, IN const ib_net64_t port_guid);
/*
* PARAMETERS
*	p_sa
*		[in] Pointer to an osm_sa_t object to bind.
*
*	port_guid
*		[in] Local port GUID with which to bind.
*
*
* RETURN VALUES
*	None
*
* NOTES
*	A given SA object can only be bound to one port at a time.
*
* SEE ALSO
*********/

/****f* OpenSM: SA/osm_sa_send
* NAME
*	osm_sa_send
*
* DESCRIPTION
*	Sends SA MAD via osm_vendor_send and maintains the QP1 sent statistic
*
* SYNOPSIS
*/
ib_api_status_t osm_sa_send(osm_sa_t *sa, IN osm_madw_t * const p_madw,
			    IN boolean_t const resp_expected);

/****f* IBA Base: Types/osm_sa_send_error
* NAME
*	osm_sa_send_error
*
* DESCRIPTION
*	Sends a generic SA response with the specified error status.
*	The payload is simply replicated from the request MAD.
*
* SYNOPSIS
*/
void osm_sa_send_error(IN osm_sa_t * sa, IN const osm_madw_t * const p_madw,
		       IN const ib_net16_t sa_status);
/*
* PARAMETERS
*	sa
*		[in] Pointer to an osm_sa_t object.
*
*	p_madw
*		[in] Original MAD to which the response must be sent.
*
*	sa_status
*		[in] Status to send in the response.
*
* RETURN VALUES
*	None.
*
* SEE ALSO
*	SA object
*********/

/****f* OpenSM: SA/osm_sa_respond
* NAME
*	osm_sa_respond
*
* DESCRIPTION
*	Sends SA MAD response
*/
void osm_sa_respond(osm_sa_t *sa, osm_madw_t *madw, size_t attr_size,
		    cl_qlist_t *list);
/*
* PARAMETERS
*	sa
*		[in] Pointer to an osm_sa_t object.
*
*	p_madw
*		[in] Original MAD to which the response must be sent.
*
*	attr_size
*		[in] Size of this SA attribute.
*
*	list
*		[in] List of attribute to respond - it will be freed after
*		sending.
*
* RETURN VALUES
*	None.
*
* SEE ALSO
*	SA object
*********/

struct osm_opensm;
/****f* OpenSM: SA/osm_sa_db_file_dump
* NAME
*	osm_sa_db_file_dump
*
* DESCRIPTION
*	Dumps the SA DB to the dump file.
*
* SYNOPSIS
*/
int osm_sa_db_file_dump(struct osm_opensm *p_osm);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to an osm_opensm_t object.
*
* RETURN VALUES
*	None
*
*********/

/****f* OpenSM: SA/osm_sa_db_file_load
* NAME
*	osm_sa_db_file_load
*
* DESCRIPTION
*	Loads SA DB from the file.
*
* SYNOPSIS
*/
int osm_sa_db_file_load(struct osm_opensm *p_osm);
/*
* PARAMETERS
*	p_osm
*		[in] Pointer to an osm_opensm_t object.
*
* RETURN VALUES
*	0 on success, other value on failure.
*
*********/

/****f* OpenSM: MC Member Record Receiver/osm_mcmr_rcv_find_or_create_new_mgrp
* NAME
*	osm_mcmr_rcv_find_or_create_new_mgrp
*
* DESCRIPTION
*	Create new Multicast group
*
* SYNOPSIS
*/

ib_api_status_t
osm_mcmr_rcv_find_or_create_new_mgrp(IN osm_sa_t * sa,
				     IN uint64_t comp_mask,
				     IN ib_member_rec_t *
				     const p_recvd_mcmember_rec,
				     OUT osm_mgrp_t ** pp_mgrp);
/*
* PARAMETERS
*	p_sa
*		[in] Pointer to an osm_sa_t object.
*	p_recvd_mcmember_rec
*		[in] Received Multicast member record
*
*	pp_mgrp
*		[out] pointer the osm_mgrp_t object
*
* RETURN VALUES
*	IB_SUCCESS, IB_ERROR
*
*********/

osm_mgrp_t *osm_get_mgrp_by_mgid(IN osm_sa_t * sa, IN ib_gid_t * p_mgid);

END_C_DECLS
#endif				/* _OSM_SA_H_ */
