/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * SPANS Signalling Manager
 * ---------------------------
 *
 * SPANS signalling message processing.
 *
 */

#include <netatm/kern_include.h>

#include <rpc/rpc.h>
#include "spans_xdr.h"
#include <netatm/spans/spans_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif

/*
 * External functions
 */
void		xdrmbuf_init __P((XDR *, KBuffer *, enum xdr_op));

/*
 * Local functions
 */
static void	spans_host_link __P((struct spans *, long));
static void	spans_status_ind __P((struct spans *, spans_msg *));
static void	spans_status_rsp __P((struct spans *, spans_msg *));
static void	spans_open_req __P((struct spans *, spans_msg *));
static void	spans_open_rsp __P((struct spans *, spans_msg *));
static void	spans_close_req __P((struct spans *, spans_msg *));
static void	spans_close_rsp __P((struct spans *, spans_msg *));
static void	spans_multi_req __P((struct spans *, spans_msg *));
static void	spans_add_req __P((struct spans *, spans_msg *));
static void	spans_join_req __P((struct spans *, spans_msg *));
static void	spans_leave_req __P((struct spans *, spans_msg *));
static void	spans_vcir_ind __P((struct spans *, spans_msg *));
static void	spans_query_req __P((struct spans *, spans_msg *));


/*
 * Called to set status when a status message comes in from a host
 * connected back-to-back with us.  Check the epoch and, if it has
 * changed, set the appropriate state and save updated state
 * information.
 *
 * Arguments:
 *	spp		pointer to SPANS protocol instance block
 *	host_epoch	epoch of host at far end of link
 *
 * Returns:
 *	0	message sent OK
 *	errno	error encountered
 *
 */
static void
spans_host_link(spp, host_epoch)
	struct spans	*spp;
	long	host_epoch;
{
	struct atm_pif	*pip = spp->sp_pif;

	/*
	 * There's a host at the other end of the link.  If its
	 * epoch has changed, clean up our state and save the
	 * new information.
	 */
	if (spp->sp_s_epoch != host_epoch) {
		spp->sp_s_epoch = host_epoch;
		spans_switch_reset(spp, SPANS_UNI_UP);
		spp->sp_addr.address_format = T_ATM_SPANS_ADDR;
		spp->sp_addr.address_length = sizeof(spans_addr);
		KM_COPY(&pip->pif_macaddr.ma_data[2],
				&spp->sp_addr.address[4],
				4);
		log(LOG_INFO,
		"spans: using SPANS address of %s on interface %s%d\n",
			spans_addr_print((spans_addr *)spp->sp_addr.address),
			pip->pif_name,
			pip->pif_unit);
	}
}

/*
 * Send a SPANS signalling message
 *
 * Called to send a SPANS message.  This routine gets a buffer, performs
 * XDR processing, and hands the message to the AAL for transmission.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	msg	pointer to status message
 *
 * Returns:
 *	0	message sent OK
 *	errno	error encountered
 *
 */
int
spans_send_msg(spp, msg)
	struct spans	*spp;
	spans_msg	*msg;
{
	int		err = 0;
	KBuffer		*m;
	XDR		xdrs;

#ifdef NOTDEF
	ATM_DEBUG2("spans_send_msg: msg=%p, type=%d\n", msg,
			msg->sm_type);
	if (msg->sm_type != SPANS_STAT_REQ &&
			msg->sm_type != SPANS_STAT_IND &&
			msg->sm_type != SPANS_STAT_RSP) {
		printf("spans_send_msg: sending ");
		spans_print_msg(msg);
	}
#endif

	/*
	 * If the signalling channel has been closed, don't do anything
	 */
	if (!spp->sp_conn)
		return(ECONNABORTED);

	/*
	 * Get a buffer
	 */
	KB_ALLOCPKT(m, sizeof(spans_msg), KB_F_NOWAIT, KB_T_DATA);
	if (m == NULL) {
		/* No buffer available */
		return(ENOBUFS);
	}

	/*
	 * Convert message to network order
	 */
	KB_LEN(m) = KB_BFRLEN(m);
	xdrmbuf_init(&xdrs, m, XDR_ENCODE);
	if (!xdr_spans_msg(&xdrs, msg)) {
		log(LOG_ERR, "spans_send_msg: XDR encode failed\n");
		KB_LEN(m) = XDR_GETPOS(&xdrs);
		spans_dump_buffer(m);
		KB_FREEALL(m);
		return(EIO);
	}
	KB_LEN(m) = XDR_GETPOS(&xdrs);

	/*
	 * Send the message
	 */
	err = atm_cm_cpcs_data(spp->sp_conn, m);
	if (err)
		KB_FREEALL(m);

	return(err);
}


/*
 * Send an open request
 *
 * Build and send an open request.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	svp	pointer to VCCB for which the request is being sent
 *
 * Returns:
 *	none
 *
 */
int
spans_send_open_req(spp, svp)
	struct	spans		*spp;
	struct	spans_vccb	*svp;
{
	spans_msg	*req;
	int		err = 0;

	ATM_DEBUG1("spans_send_open_req: svp=%p\n", svp);

	/*
	 * Get memory for a request message
	 */
	req = (spans_msg *)atm_allocate(&spans_msgpool);
	if (req == NULL) {
		err = ENOBUFS;
		goto done;
	}

	/*
	 * Fill in the request
	 */
	req->sm_vers = SPANS_VERS_1_0;
	req->sm_type = SPANS_OPEN_REQ;
	req->sm_open_req.opreq_conn = svp->sv_conn;
	req->sm_open_req.opreq_aal = svp->sv_spans_aal;
	req->sm_open_req.opreq_desrsrc = svp->sv_spans_qos;
	req->sm_open_req.opreq_minrsrc.rsc_peak = 0;
	req->sm_open_req.opreq_minrsrc.rsc_mean = 0;
	req->sm_open_req.opreq_minrsrc.rsc_burst = 0;
	req->sm_open_req.opreq_vpvc.vpf_valid = FALSE;

	/*
	 * Send the request
	 */
	err = spans_send_msg(spp, req);
	atm_free(req);

done:
	return(err);
}


/*
 * Send an open response
 *
 * Build and send a response to an open request or open indication.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	svp	pointer to VCCB for which the response is being sent
 *	result	result code to include in the response
 *
 * Returns:
 *	none
 *
 */
int
spans_send_open_rsp(spp, svp, result)
	struct	spans		*spp;
	struct	spans_vccb	*svp;
	spans_result		result;
{
	spans_msg	*rsp;
	int		rc;

	ATM_DEBUG2("spans_send_open_rsp: svp=%p, result=%d\n", svp,
			result);

	/*
	 * Get memory for a response message
	 */
	rsp = (spans_msg *)atm_allocate(&spans_msgpool);
	if (rsp == NULL)
		return(ENOBUFS);

	/*
	 * Fill in the response
	 */
	rsp->sm_vers = SPANS_VERS_1_0;
	rsp->sm_type = SPANS_OPEN_RSP;
	rsp->sm_open_rsp.oprsp_conn = svp->sv_conn;
	rsp->sm_open_rsp.oprsp_result = result;
	rsp->sm_open_rsp.oprsp_rsrc = svp->sv_spans_qos;
	rsp->sm_open_rsp.oprsp_vpvc =
			SPANS_PACK_VPIVCI(svp->sv_vpi, svp->sv_vci);

	/*
	 * Send the response
	 */
	rc = spans_send_msg(spp, rsp);
	atm_free(rsp);

	return(rc);
}


/*
 * Send a close request
 *
 * Called to send a close request.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	svp	pointer to VCCB for which the close is being sent
 *
 * Returns:
 *	none
 *
 */
int
spans_send_close_req(spp, svp)
	struct spans		*spp;
	struct	spans_vccb	*svp;
{
	spans_msg	*req;
	int		err = 0;

	ATM_DEBUG1("spans_send_close_req: svp=%p\n", svp);

	/*
	 * Get memory for a close request
	 */
	req = (spans_msg *)atm_allocate(&spans_msgpool);
	if (req == NULL) {
		err = ENOBUFS;
		goto done;
	}

	/*
	 * Fill in the request
	 */
	req->sm_vers = SPANS_VERS_1_0;
	if (svp->sv_type & VCC_OUT) {
		req->sm_type = SPANS_CLOSE_REQ;
	} else if (svp->sv_type & VCC_IN) {
		req->sm_type = SPANS_RCLOSE_REQ;
	} else {
		err = EINVAL;
		ATM_DEBUG1(
		"spans_send_close_req: invalid VCCB type 0x%x\n",
				svp->sv_type);
		goto done;
	}
	req->sm_close_req.clreq_conn = svp->sv_conn;

	/*
	 * Send the close request
	 */
	err = spans_send_msg(spp, req);

done:
	if (req)
		atm_free(req);

	return(err);
}



/*
 * Process a status indication or status request
 *
 * Called when a status indication or status request is received.
 * Processing will be based on the current SPANS state.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	msg	pointer to the status message
 *
 * Returns:
 *	none
 *
 */
static void
spans_status_ind(spp, msg)
	struct	spans	*spp;
	spans_msg	*msg;
{
	spans_msg	*rsp_msg;
	struct atm_pif	*pip = spp->sp_pif;

	/*
	 * Reset the probe count.
	 */
	spp->sp_probe_ct = 0;

	switch (spp->sp_state) {
	case SPANS_PROBE:
		/*
		 * Interface just came up, update signalling state
		 */
		spp->sp_state = SPANS_ACTIVE;
		break;

	case SPANS_ACTIVE:
		break;

	default:
		log(LOG_ERR, "spans: received status msg in state %d\n",
				spp->sp_state);
	}

	/*
	 * Process the message
	 */
	switch (msg->sm_type) {

	case SPANS_STAT_REQ:
		/*
		 * Handle a request from a host at the other end of
		 * the link.
		 */
		spans_host_link(spp, msg->sm_stat_req.streq_es_epoch);
		break;

	case SPANS_STAT_IND:

		/*
		 * There's a switch at the other end of the link.  If
		 * its epoch has changed, reset the SPANS state and save
		 * the new information.
		 */
		if (spp->sp_s_epoch !=
				msg->sm_stat_ind.stind_sw_epoch) {
			spans_switch_reset(spp, SPANS_UNI_UP);
			spp->sp_s_epoch =
				msg->sm_stat_ind.stind_sw_epoch;
			spp->sp_addr.address_format = T_ATM_SPANS_ADDR;
			spp->sp_addr.address_length =
					sizeof(spans_addr);
			spans_addr_copy(&msg->sm_stat_ind.stind_es_addr,
				spp->sp_addr.address);
			log(LOG_INFO,
		"spans: received SPANS address %s from switch for interface %s%d\n",
					spans_addr_print((spans_addr *)spp->sp_addr.address),
					pip->pif_name,
					pip->pif_unit);
		}
		break;

	default:
		ATM_DEBUG1("spans_status_ind: Invalid message type %d\n",
				msg->sm_type);
		return;
	}

	/*
	 * Respond to the status request or indication with a
	 * status response
	 */
	rsp_msg = (spans_msg *)atm_allocate(&spans_msgpool);
	if (rsp_msg == NULL)
		return;
	rsp_msg->sm_vers = SPANS_VERS_1_0;
	rsp_msg->sm_type = SPANS_STAT_RSP;
	rsp_msg->sm_stat_rsp.strsp_es_epoch = spp->sp_h_epoch;
	spans_addr_copy(spp->sp_addr.address,
			&rsp_msg->sm_stat_rsp.strsp_es_addr);
	spans_send_msg(spp, rsp_msg);
	atm_free(rsp_msg);
}



/*
 * Process a status response
 *
 * Called when a status response is received.
 * Processing will be based on the current SPANS state.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	msg	pointer to the status response message
 *
 * Returns:
 *	none
 *
 */
static void
spans_status_rsp(spp, msg)
	struct	spans	*spp;
	spans_msg	*msg;
{

	/*
	 * Reset the probe count.
	 */
	spp->sp_probe_ct = 0;

	switch (spp->sp_state) {
	case SPANS_PROBE:
		/*
		 * Interface just came up, update signalling state
		 */
		spp->sp_state = SPANS_ACTIVE;
		break;

	case SPANS_ACTIVE:
		break;

	default:
		log(LOG_ERR, "spans: received status msg in state %d\n",
				spp->sp_state);
	}

	/*
	 * Process the message
	 */
	spans_host_link(spp, msg->sm_stat_req.streq_es_epoch);
}


/*
 * Process an open indication or open request
 *
 * Called when an open indication or open request is received.
 * Processing will be based on the state of the requested connection.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	msg	pointer to the open message
 *
 * Returns:
 *	none
 *
 */
static void
spans_open_req(spp, msg)
	struct spans	*spp;
	spans_msg	*msg;
{
	spans_result		result = SPANS_OK;
	spans_msg		*rsp_msg;
	struct spans_vccb	*svp = NULL;
	struct atm_pif		*pip;
	spans_vpvc		vpvc;
	int			err = 0, vpi, vci;
	Aal_t			aal;
	Atm_attributes		call_attrs;

	ATM_DEBUG2("spans_open_req: spp=%p, msg=%p\n", spp, msg);

	/*
	 * See if the connection is new
	 */
	if ((svp = spans_find_conn(spp, &msg->sm_open_req.opreq_conn)) != NULL) {
		/*
		 * We already have a VCCB that matches the connection in
		 * the request
		 */
		vpi = SPANS_EXTRACT_VPI(msg->sm_open_req.opreq_vpvc.vpf_vpvc);
		vci = SPANS_EXTRACT_VCI(msg->sm_open_req.opreq_vpvc.vpf_vpvc);
		if (msg->sm_open_req.opreq_aal == svp->sv_spans_aal &&
				(!msg->sm_open_req.opreq_vpvc.vpf_valid ||
					(vpi == svp->sv_vpi &&
					vci == svp->sv_vci))) {
			/*
			 * VCCB already exists, process depending on
			 * state
			 */
			switch (svp->sv_sstate) {
			case SPANS_VC_R_POPEN:
				/* I'm still thinking about it */
				return;
			case SPANS_VC_OPEN:
				/* Retransmit the open_rsp */
				break;
			case SPANS_VC_POPEN:
			case SPANS_VC_CLOSE:
			case SPANS_VC_ABORT:
				ATM_DEBUG0("spans_open_req: bad VCCB state\n");
				result = SPANS_FAIL;
				break;
			}
		} else {
			/*
			 * VCCB is for same connection, but other
			 * parameters don't match
			 */
			ATM_DEBUG0("spans_open_req: VCCB confusion\n");
			result = SPANS_FAIL;
		}
		svp = NULL;
		goto response;
	}

	/*
	 * Verify that the request is for our ATM addres
	 */
	if (spans_addr_cmp(spp->sp_addr.address,
			&msg->sm_open_req.opreq_conn.con_dst)) {
		ATM_DEBUG0("spans_open_req: bad destination\n");
		result = SPANS_BADDEST;
		goto response;
	}

	/*
	 * See if we recognize the specified AAL
	 */
	if (!spans_get_local_aal(msg->sm_open_req.opreq_aal, &aal)) {
		ATM_DEBUG0("spans_open_req: bad AAL\n");
		result = SPANS_FAIL;
		goto response;
	}

	/*
	 * Should verify that we can handle requested connection QOS
	 */

	/*
	 * Select a VPI/VCI for the new connection
	 */
	if (msg->sm_open_req.opreq_vpvc.vpf_valid) {
		/*
		 * Requestor asked for a certain VPI/VCI.  Make sure we
		 * aren't already using the pair that was asked for.
		 */
		vpi = SPANS_EXTRACT_VPI(msg->sm_open_req.opreq_vpvc.vpf_vpvc);
		vci = SPANS_EXTRACT_VCI(msg->sm_open_req.opreq_vpvc.vpf_vpvc);
		if (spans_find_vpvc(spp, vci, vpi, VCC_IN)) {
			ATM_DEBUG0("spans_open_req: VPI, VCI busy\n");
			result = SPANS_NOVPVC;
			goto response;
		}
		vpvc = msg->sm_open_req.opreq_vpvc.vpf_vpvc;
	} else {
		/*
		 * Allocate a VPI/VCI for this end of the VCC
		 */
		vpvc = spans_alloc_vpvc(spp);
		if (vpvc == 0) {
			ATM_DEBUG0("spans_open_req: no VPI, VCI available\n");
			result = SPANS_NOVPVC;
			goto response;
		}
	}

	/*
	 * Get a new VCCB for the connection
	 */
	svp = (struct spans_vccb *)atm_allocate(&spans_vcpool);
	if (svp == NULL) {
		ATM_DEBUG0("spans_open_req: VCCB pool empty\n");
		result = SPANS_NORSC;
		goto response;
	}

	/*
	 * Find the physical interface structure
	 */
	pip = spp->sp_pif;

	/*
	 * Fill in the VCCB fields that we can at this point
	 */
	svp->sv_type = VCC_SVC | VCC_IN;
	svp->sv_proto = ATM_SIG_SPANS;
	svp->sv_sstate = SPANS_VC_R_POPEN;
	svp->sv_ustate = VCCU_POPEN;
	svp->sv_pif = pip;
	svp->sv_nif = pip->pif_nif;
	svp->sv_conn = msg->sm_open_req.opreq_conn;
	svp->sv_spans_qos = msg->sm_open_req.opreq_desrsrc;
	svp->sv_spans_aal = msg->sm_open_req.opreq_aal;
	svp->sv_tstamp = time_second;

	svp->sv_vpi = SPANS_EXTRACT_VPI(vpvc);
	svp->sv_vci = SPANS_EXTRACT_VCI(vpvc);

	/*
	 * Put the VCCB on the SPANS queue
	 */
	ENQUEUE(svp, struct spans_vccb, sv_sigelem, spp->sp_vccq);

	/*
	 * Set up the ATM attributes block
	 */
	KM_ZERO(&call_attrs, sizeof(call_attrs));
	call_attrs.nif = svp->sv_nif;
	call_attrs.api = CMAPI_CPCS;

	call_attrs.aal.tag = T_ATM_PRESENT;
	call_attrs.aal.type = aal;
	switch(aal) {
	case ATM_AAL3_4:
		call_attrs.aal.v.aal4.forward_max_SDU_size =
				ATM_NIF_MTU;
		call_attrs.aal.v.aal4.backward_max_SDU_size =
				ATM_NIF_MTU;
		call_attrs.aal.v.aal4.SSCS_type =
				T_ATM_NULL;
		call_attrs.aal.v.aal4.mid_low = 0;
		call_attrs.aal.v.aal4.mid_high = 1023;
		break;
	case ATM_AAL5:
		call_attrs.aal.v.aal5.forward_max_SDU_size =
				ATM_NIF_MTU;
		call_attrs.aal.v.aal5.backward_max_SDU_size =
				ATM_NIF_MTU;
		call_attrs.aal.v.aal5.SSCS_type =
				T_ATM_NULL;
		break;
	}

	call_attrs.traffic.tag = T_ATM_PRESENT;
	call_attrs.traffic.v.forward.PCR_high_priority = T_ATM_ABSENT;
	call_attrs.traffic.v.forward.PCR_all_traffic = 
			msg->sm_open_req.opreq_desrsrc.rsc_peak *
			1000 / 53;
	call_attrs.traffic.v.forward.SCR_high_priority = T_ATM_ABSENT;
	call_attrs.traffic.v.forward.SCR_all_traffic = T_ATM_ABSENT;
	call_attrs.traffic.v.forward.MBS_high_priority = T_ATM_ABSENT;
	call_attrs.traffic.v.forward.MBS_all_traffic = T_ATM_ABSENT;
	call_attrs.traffic.v.forward.tagging = T_NO;
	call_attrs.traffic.v.backward.PCR_high_priority = T_ATM_ABSENT;
	call_attrs.traffic.v.backward.PCR_all_traffic = 
			call_attrs.traffic.v.forward.PCR_all_traffic;
	call_attrs.traffic.v.backward.SCR_high_priority = T_ATM_ABSENT;
	call_attrs.traffic.v.backward.SCR_all_traffic = T_ATM_ABSENT;
	call_attrs.traffic.v.backward.MBS_high_priority = T_ATM_ABSENT;
	call_attrs.traffic.v.backward.MBS_all_traffic = T_ATM_ABSENT;
	call_attrs.traffic.v.backward.tagging = T_NO;
	call_attrs.traffic.v.best_effort = T_YES;

	call_attrs.bearer.tag = T_ATM_PRESENT;
	call_attrs.bearer.v.bearer_class = T_ATM_CLASS_X;
	call_attrs.bearer.v.traffic_type = T_ATM_NULL;
	call_attrs.bearer.v.timing_requirements = T_ATM_NULL;
	call_attrs.bearer.v.clipping_susceptibility = T_NO;
	call_attrs.bearer.v.connection_configuration = T_ATM_1_TO_1;


	call_attrs.bhli.tag = T_ATM_ABSENT;
	call_attrs.blli.tag_l2 = T_ATM_ABSENT;
	call_attrs.blli.tag_l3 = T_ATM_ABSENT;
	call_attrs.llc.tag = T_ATM_ABSENT;

	call_attrs.called.tag = T_ATM_PRESENT;
	spans_addr_copy(&msg->sm_open_req.opreq_conn.con_dst,
		call_attrs.called.addr.address);
	call_attrs.called.addr.address_format = T_ATM_SPANS_ADDR;
	call_attrs.called.addr.address_length = sizeof(spans_addr);
	call_attrs.called.subaddr.address_format = T_ATM_ABSENT;
	call_attrs.called.subaddr.address_length = 0;

	call_attrs.calling.tag = T_ATM_PRESENT;
	spans_addr_copy(&msg->sm_open_req.opreq_conn.con_src,
		call_attrs.calling.addr.address);
	call_attrs.calling.addr.address_format = T_ATM_SPANS_ADDR;
	call_attrs.calling.addr.address_length = sizeof(spans_addr);
	call_attrs.calling.subaddr.address_format = T_ATM_ABSENT;
	call_attrs.calling.subaddr.address_length = 0;

	call_attrs.qos.tag = T_ATM_PRESENT;
	call_attrs.qos.v.coding_standard = T_ATM_NETWORK_CODING;
	call_attrs.qos.v.forward.qos_class = T_ATM_QOS_CLASS_0;
	call_attrs.qos.v.backward.qos_class = T_ATM_QOS_CLASS_0;

	call_attrs.transit.tag = T_ATM_ABSENT;
	call_attrs.cause.tag = T_ATM_ABSENT;

	/*
	 * Notify the connection manager that it has a new channel
	 */
	err = atm_cm_incoming((struct vccb *)svp, &call_attrs);
	if (err) {
		ATM_DEBUG0("spans_open_req: atm_cm_incoming returned error\n");
		result = SPANS_FAIL;
		goto response;
	}

	/*
	 * Wait for the connection recipient to issue an accept
	 */
	return;

response:
	/*
	 * Clean up the VCCB and the atm_conn block if we got them
	 */
	if (svp) {
		DEQUEUE(svp, struct spans_vccb, sv_sigelem,
				spp->sp_vccq);
		atm_free(svp);
	}

	/*
	 * Some problem was detected with the request.  Send a SPANS
	 * message rejecting the connection.
	 */
	rsp_msg = (spans_msg *) atm_allocate(&spans_msgpool);
	if (rsp_msg == NULL)
		return;

	/*
	 * Fill out the response
	 */
	rsp_msg->sm_vers = SPANS_VERS_1_0;
	rsp_msg->sm_type = SPANS_OPEN_RSP;
	rsp_msg->sm_open_rsp.oprsp_conn = msg->sm_open_req.opreq_conn;
	rsp_msg->sm_open_rsp.oprsp_result = result;
	rsp_msg->sm_open_rsp.oprsp_vpvc = 0;

	/*
	 * Send the Open Response
	 */
	spans_send_msg(spp, rsp_msg);
	atm_free(rsp_msg);
}


/*
 * Process an open response or open confirmation
 *
 * Called when an open response or open confirmation is received.
 * Processing will be based on the state of the requested connection and
 * the status returned.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	msg	pointer to the open response or confirmation message
 *
 * Returns:
 *	none
 *
 */
static void
spans_open_rsp(spp, msg)
	struct spans	*spp;
	spans_msg	*msg;
{
	struct spans_vccb	*svp;

	ATM_DEBUG2("spans_open_rsp: spp=%p, msg=%p\n", spp, msg);

	/*
	 * Locate the VCCB for the connection
	 */
	svp = spans_find_conn(spp, &msg->sm_open_rsp.oprsp_conn);
	if (svp == NULL)
		return;

	/*
	 * Check the connection state
	 */
	if ((svp->sv_sstate != SPANS_VC_POPEN &&
			svp->sv_sstate != SPANS_VC_R_POPEN) ||
			svp->sv_ustate != VCCU_POPEN) {
		ATM_DEBUG2(
	"spans_open_rsp: invalid VCCB state, sstate=%d, ustate=%d\n",
				svp->sv_sstate, svp->sv_ustate);
		return;
	}

	/*
	 * Cancel the retransmission timer
	 */
	SPANS_VC_CANCEL((struct vccb *) svp);

	/*
	 * Check the result
	 */
	switch (msg->sm_open_rsp.oprsp_result) {

	case SPANS_OK:
		/*
		 * Save the assigned VPI and VCI
		 */
		svp->sv_vpi = SPANS_EXTRACT_VPI(msg->sm_open_rsp.oprsp_vpvc);
		svp->sv_vci = SPANS_EXTRACT_VCI(msg->sm_open_rsp.oprsp_vpvc);

		/*
		 * Update the VCC state and notify the VCC owner
		 */
		svp->sv_sstate = SPANS_VC_OPEN;
		svp->sv_ustate = VCCU_OPEN;
		svp->sv_tstamp = time_second;
		atm_cm_connected(svp->sv_connvc);
		break;

	case SPANS_FAIL:
	case SPANS_NOVPVC:
	case SPANS_NORSC:
	case SPANS_BADDEST:
		/*
		 * Close out the VCCB and notify the user
		 */
		svp->sv_sstate = SPANS_VC_FREE;
		svp->sv_ustate = VCCU_CLOSED;
		svp->sv_connvc->cvc_attr.cause.tag = T_ATM_PRESENT;
		svp->sv_connvc->cvc_attr.cause.v.coding_standard =
				T_ATM_ITU_CODING;
		svp->sv_connvc->cvc_attr.cause.v.location =
				T_ATM_LOC_USER;
		svp->sv_connvc->cvc_attr.cause.v.cause_value =
				T_ATM_CAUSE_CALL_REJECTED;
		KM_ZERO(svp->sv_connvc->cvc_attr.cause.v.diagnostics,
				sizeof(svp->sv_connvc->cvc_attr.cause.v.diagnostics));
		atm_cm_cleared(svp->sv_connvc);
		break;

	default:
		log(LOG_ERR, "spans: unknown result %d in open rsp\n",
				msg->sm_open_rsp.oprsp_result);
		break;
	}
}


/*
 * Process a close request from the network
 *
 * Called when a close request, close indication, rclose request, or
 * rclose indication is received.  Processing will be based on the
 * state of the connection.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	msg	pointer to the close request message
 *
 * Returns:
 *	none
 *
 */
static void
spans_close_req(spp, msg)
	struct spans	*spp;
	spans_msg	*msg;
{
	struct spans_vccb	*svp;
	spans_result		result;
	spans_msg		*rsp_msg;
	u_char			outstate;
	Atm_connvc		*cvp;

	ATM_DEBUG2("spans_close_req: spp=%p, msg=%p\n", spp, msg);

	/*
	 * Locate the VCCB for the connection
	 */
	svp = spans_find_conn(spp, &msg->sm_close_req.clreq_conn);
	if (svp == NULL) {
		result = SPANS_BADDEST;
		goto response;
	}

	/*
	 * Check the connection type
	 */
	if (!(svp->sv_type & VCC_SVC)) {
		result = SPANS_FAIL;
		goto response;
	}

	/*
	 * Check the connection state
	 */
	switch (svp->sv_sstate) {
	case SPANS_VC_OPEN:
	case SPANS_VC_R_POPEN:
	case SPANS_VC_POPEN:
		/*
		 * VCC is open or opening--continue
		 */
		break;
	case SPANS_VC_CLOSE:
	case SPANS_VC_FREE:
	case SPANS_VC_ABORT:
		/*
		 * We're already closing--give a response, since this
		 * is probably a retransmission
		 */
		result = SPANS_OK;
		goto response;
	case SPANS_VC_NULL:
		result = SPANS_FAIL;
		goto response;
	}

	/*
	 * Cancel the retransmission timer
	 */
	SPANS_VC_CANCEL((struct vccb *) svp);

	/*
	 * Close out the VCCB and notify the user
	 */
	outstate = svp->sv_sstate;
	svp->sv_ustate = VCCU_CLOSED;
	svp->sv_sstate = SPANS_VC_FREE;
	cvp = svp->sv_connvc;
	switch (outstate) {
	case SPANS_VC_R_POPEN:
		spans_free((struct vccb *)svp);
		/* FALLTHRU */

	case SPANS_VC_POPEN:
	case SPANS_VC_OPEN:
		cvp->cvc_attr.cause.tag = T_ATM_PRESENT;
		cvp->cvc_attr.cause.v.coding_standard =
				T_ATM_ITU_CODING;
		cvp->cvc_attr.cause.v.location = T_ATM_LOC_USER;
		cvp->cvc_attr.cause.v.cause_value =
				T_ATM_CAUSE_NORMAL_CALL_CLEARING;
		KM_ZERO(cvp->cvc_attr.cause.v.diagnostics,
				sizeof(cvp->cvc_attr.cause.v.diagnostics));
		atm_cm_cleared(svp->sv_connvc);
		break;
	}

	result = SPANS_OK;

response:
	/*
	 * Respond to the SPANS_CLOSE_IND with a SPANS_CLOSE_RSP
	 */
	rsp_msg = (spans_msg *)atm_allocate(&spans_msgpool);
	if (rsp_msg == NULL)
		return;
	rsp_msg->sm_vers = SPANS_VERS_1_0;
	if (msg->sm_type == SPANS_RCLOSE_REQ ||
			msg->sm_type == SPANS_RCLOSE_IND) {
		rsp_msg->sm_type = SPANS_RCLOSE_RSP;
	} else {
		rsp_msg->sm_type = SPANS_CLOSE_RSP;
	}
	rsp_msg->sm_close_rsp.clrsp_conn = msg->sm_close_req.clreq_conn;
	rsp_msg->sm_close_rsp.clrsp_result = result;
	spans_send_msg(spp, rsp_msg);
	atm_free(rsp_msg);
}


/*
 * Process a close response or close confirmation
 *
 * Called when an close response or close confirmation is received.
 * Processing will be based on the state of the requested connection and
 * the returned status.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	msg	pointer to the close response or confirmation message
 *
 * Returns:
 *	none
 *
 */
static void
spans_close_rsp(spp, msg)
	struct spans	*spp;
	spans_msg	*msg;
{
	struct spans_vccb	*svp;

	ATM_DEBUG2("spans_close_rsp: spp=%p, msg=%p\n", spp, msg);

	/*
	 * Locate the VCCB for the connection
	 */
	svp = spans_find_conn(spp, &msg->sm_close_rsp.clrsp_conn);
	if (svp == NULL) {
		return;
	}

	/*
	 * Check the VCCB state
	 */
	if (svp->sv_sstate != SPANS_VC_CLOSE) {
		return;
	}

	/*
	 * Cancel the retransmission timer
	 */
	SPANS_VC_CANCEL((struct vccb *) svp);

	/*
	 * Check the response from the remote end
	 */
	switch (msg->sm_close_rsp.clrsp_result) {

	case SPANS_OK:
		/*
		 * Mark the VCCB as closed and notify the owner
		 */
		svp->sv_sstate = SPANS_VC_FREE;
		svp->sv_connvc->cvc_attr.cause.tag = T_ATM_PRESENT;
		svp->sv_connvc->cvc_attr.cause.v.coding_standard =
				T_ATM_ITU_CODING;
		svp->sv_connvc->cvc_attr.cause.v.location =
				T_ATM_LOC_USER;
		svp->sv_connvc->cvc_attr.cause.v.cause_value =
				T_ATM_CAUSE_NORMAL_CALL_CLEARING;
		KM_ZERO(svp->sv_connvc->cvc_attr.cause.v.diagnostics,
				sizeof(svp->sv_connvc->cvc_attr.cause.v.diagnostics));
		atm_cm_cleared(svp->sv_connvc);
		break;

	case SPANS_NOVPVC:
	case SPANS_BADDEST:
	case SPANS_FAIL:
	case SPANS_NORSC:
		/*
		 * Mark the VCCB as closed and notify the owner
		 */
		svp->sv_sstate = SPANS_VC_FREE;
		svp->sv_connvc->cvc_attr.cause.tag = T_ATM_PRESENT;
		svp->sv_connvc->cvc_attr.cause.v.coding_standard =
				T_ATM_ITU_CODING;
		svp->sv_connvc->cvc_attr.cause.v.location =
				T_ATM_LOC_USER;
		svp->sv_connvc->cvc_attr.cause.v.cause_value =
				T_ATM_CAUSE_UNSPECIFIED_NORMAL;
		KM_ZERO(svp->sv_connvc->cvc_attr.cause.v.diagnostics,
				sizeof(svp->sv_connvc->cvc_attr.cause.v.diagnostics));
		atm_cm_cleared(svp->sv_connvc);
		break;

	default:
		log(LOG_ERR, "spans: unknown result %d in close rsp\n",
				msg->sm_close_rsp.clrsp_result);
		break;
	}
}


/*
 * Process a multi request or multi indication
 *
 * Called when a multi response or multi confirmation is received.  We
 * don't support multicast channels, so we just reject the request.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	msg	pointer to the multi request or indication message
 *
 * Returns:
 *	none
 *
 */
static void
spans_multi_req(spp, msg)
	struct spans	*spp;
	spans_msg	*msg;
{
	spans_msg	*rsp_msg;

	/*
	 * Get memory for a SPANS_MULTI_RSP message.
	 */
	rsp_msg = (spans_msg *) atm_allocate(&spans_msgpool);
	if (rsp_msg == NULL)
		return;

	/*
	 * Fill out the response.
	 */
	rsp_msg->sm_vers = SPANS_VERS_1_0;
	rsp_msg->sm_type = SPANS_MULTI_RSP;
	rsp_msg->sm_multi_rsp.mursp_conn = msg->sm_multi_req.mureq_conn;
	rsp_msg->sm_multi_rsp.mursp_result = SPANS_FAIL;
	rsp_msg->sm_multi_rsp.mursp_rsrc = msg->sm_multi_req.mureq_desrsrc;
	rsp_msg->sm_multi_rsp.mursp_vpvc = 0;

	/*
	 * Send the response and free the message.
	 */
	(void) spans_send_msg(spp, rsp_msg);
	atm_free(rsp_msg);
}


/*
 * Process an add request or add indication
 *
 * Called when an add response or add confirmation is received.  We
 * don't support multicast channels, so we just reject the request.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	msg	pointer to the add request or indication message
 *
 * Returns:
 *	none
 *
 */
static void
spans_add_req(spp, msg)
	struct spans	*spp;
	spans_msg	*msg;
{
	spans_msg	*rsp_msg;

	/*
	 * Get memory for a SPANS_ADD_RSP message.
	 */
	rsp_msg = (spans_msg *) atm_allocate(&spans_msgpool);
	if (rsp_msg == NULL)
		return;

	/*
	 * Fill out the response.
	 */
	rsp_msg->sm_vers = SPANS_VERS_1_0;
	rsp_msg->sm_type = SPANS_ADD_RSP;
	rsp_msg->sm_add_rsp.adrsp_conn = msg->sm_add_req.adreq_desconn;
	rsp_msg->sm_add_rsp.adrsp_result = SPANS_FAIL;
	rsp_msg->sm_add_rsp.adrsp_rsrc.rsc_peak = 0;
	rsp_msg->sm_add_rsp.adrsp_rsrc.rsc_mean = 0;
	rsp_msg->sm_add_rsp.adrsp_rsrc.rsc_burst = 0;

	/*
	 * Send the response and free the message.
	 */
	(void) spans_send_msg(spp, rsp_msg);
	atm_free(rsp_msg);
}


/*
 * Process a join request
 *
 * Called when an join request is received.  We don't support group
 * addresses, so we just reject the request.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	msg	pointer to the join request message
 *
 * Returns:
 *	none
 *
 */
static void
spans_join_req(spp, msg)
	struct spans	*spp;
	spans_msg	*msg;
{
	spans_msg	*rsp_msg;

	/*
	 * Get memory for a SPANS_JOIN_CNF message.
	 */
	rsp_msg = (spans_msg *) atm_allocate(&spans_msgpool);
	if (rsp_msg == NULL)
		return;

	/*
	 * Fill out the response.
	 */
	rsp_msg->sm_vers = SPANS_VERS_1_0;
	rsp_msg->sm_type = SPANS_JOIN_CNF;
	spans_addr_copy(&msg->sm_join_req.jnreq_addr,
			&rsp_msg->sm_join_cnf.jncnf_addr);
	rsp_msg->sm_join_cnf.jncnf_result = SPANS_FAIL;

	/*
	 * Send the response and free the message.
	 */
	(void) spans_send_msg(spp, rsp_msg);
	atm_free(rsp_msg);
}


/*
 * Process a leave request
 *
 * Called when an leave request is received.  We don't support group
 * addresses, so we just reject the request.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	msg	pointer to the leave request message
 *
 * Returns:
 *	none
 *
 */
static void
spans_leave_req(spp, msg)
	struct spans	*spp;
	spans_msg	*msg;
{
	spans_msg	*rsp_msg;

	/*
	 * Get memory for a SPANS_LEAVE_CNF message.
	 */
	rsp_msg = (spans_msg *) atm_allocate(&spans_msgpool);
	if (rsp_msg == NULL)
		return;

	/*
	 * Fill out the response.
	 */
	rsp_msg->sm_vers = SPANS_VERS_1_0;
	rsp_msg->sm_type = SPANS_LEAVE_CNF;
	spans_addr_copy(&msg->sm_leave_req.lvreq_addr,
			&rsp_msg->sm_leave_cnf.lvcnf_addr);
	rsp_msg->sm_leave_cnf.lvcnf_result = SPANS_FAIL;

	/*
	 * Send the response and free the message.
	 */
	(void) spans_send_msg(spp, rsp_msg);
	atm_free(rsp_msg);
}


/*
 * Process a VCI range indication
 *
 * Called when a VCI range indication is received.  Adjust the VCI
 * bounds if they have changed.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	msg	pointer to the VCI range indication message
 *
 * Returns:
 *	none
 *
 */
static void
spans_vcir_ind(spp, msg)
	struct spans	*spp;
	spans_msg	*msg;
{
	/*
	 * Adjust the limits if they have changed
	 */
	if (msg->sm_vcir_ind.vrind_min != spp->sp_min_vci) {
		spp->sp_min_vci =
				(msg->sm_vcir_ind.vrind_min <
				SPANS_MIN_VCI ?
				SPANS_MIN_VCI :
				msg->sm_vcir_ind.vrind_min);
	}
	if (msg->sm_vcir_ind.vrind_max != spp->sp_max_vci) {
		spp->sp_max_vci =
				(msg->sm_vcir_ind.vrind_max >
				SPANS_MAX_VCI ?
				SPANS_MAX_VCI :
				msg->sm_vcir_ind.vrind_max);
	}
}


/*
 * Process a query request
 *
 * Called when a query request is received.  Respond with the
 * appropriate query response.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	msg	pointer to the VCI range indication message
 *
 * Returns:
 *	none
 *
 */
static void
spans_query_req(spp, msg)
	struct spans	*spp;
	spans_msg	*msg;
{
	struct spans_vccb	*svp = NULL;
	spans_msg		*rsp_msg;

	ATM_DEBUG1("spans_query_req: msg=%p\n", msg);

	/*
	 * Ignore an end-to-end query
	 */
	if (msg->sm_query_req.qyreq_type == SPANS_QUERY_END_TO_END) {
		return;
	}

	/*
	 * Get memory for a SPANS_QUERY_RSP message.
	 */
	rsp_msg = (spans_msg *) atm_allocate(&spans_msgpool);
	if (rsp_msg == NULL)
		return;

	/*
	 * Fill out the response.
	 */
	rsp_msg->sm_vers = SPANS_VERS_1_0;
	rsp_msg->sm_type = SPANS_QUERY_RSP;
	rsp_msg->sm_query_rsp.qyrsp_conn = msg->sm_query_req.qyreq_conn;
	rsp_msg->sm_query_rsp.qyrsp_type = msg->sm_query_req.qyreq_type;
	rsp_msg->sm_query_rsp.qyrsp_data = 0;

	/*
	 * Get the state of the requested connection
	 */
	svp = spans_find_conn(spp, &msg->sm_query_req.qyreq_conn);
	if (svp) {
		switch(svp->sv_sstate) {
		case SPANS_VC_NULL:
		case SPANS_VC_FREE:
			rsp_msg->sm_query_rsp.qyrsp_state =
				SPANS_CONN_CLOSED;
			break;
		case SPANS_VC_OPEN:
			rsp_msg->sm_query_rsp.qyrsp_state =
				SPANS_CONN_OPEN;
			break;
		case SPANS_VC_POPEN:
		case SPANS_VC_R_POPEN:
			rsp_msg->sm_query_rsp.qyrsp_state =
				SPANS_CONN_OPEN_PEND;
			break;
		case SPANS_VC_CLOSE:
		case SPANS_VC_ABORT:
			rsp_msg->sm_query_rsp.qyrsp_state =
				SPANS_CONN_CLOSE_PEND;
			break;
		case SPANS_VC_ACTIVE:
		case SPANS_VC_ACT_DOWN:
			/*
			 * VCCB is for a PVC (shouldn't happen)
			 */
			atm_free(rsp_msg);
			return;
		}
	} else {
		/*
		 * No VCCB found--connection doesn't exist
		 */
		rsp_msg->sm_query_rsp.qyrsp_state = SPANS_CONN_CLOSED;
	}

	/*
	 * Send the response and free the message.
	 */
	(void) spans_send_msg(spp, rsp_msg);
	atm_free(rsp_msg);
}


/*
 * Process a SPANS signalling message
 *
 * Called when a SPANS message is received.  The message is converted
 * into internal format with XDR and decoded by calling the appropriate
 * mesage handling routine.  Unrecognized and unexpected messages are
 * logged.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance block
 *	m	pointer to a buffer chain containing the SPANS message
 *
 * Returns:
 *	none
 *
 */
void
spans_rcv_msg(spp, m)
	struct spans	*spp;
	KBuffer		*m;
{
	XDR		xdrs;
	spans_msg	*msg;

	/*
	 * Get storage for the message
	 */
	msg = (spans_msg *)atm_allocate(&spans_msgpool);
	if (msg == NULL) {
		return;
	}

	/*
	 * Convert the message from network order to internal format
	 */
	xdrmbuf_init(&xdrs, m, XDR_DECODE);
	if (!xdr_spans_msg(&xdrs, msg)) {
		log(LOG_ERR, "spans_rcv_msg: XDR decode failed\n");
		spans_dump_buffer(m);
		goto done;
	}

#ifdef NOTDEF
	/*
	 * Debug--print some information about the message
	 */
	if (msg->sm_type != SPANS_STAT_REQ &&
			msg->sm_type != SPANS_STAT_IND &&
			msg->sm_type != SPANS_STAT_RSP) {
		printf("spans_rcv_msg: got ");
		spans_print_msg(msg);
	}
#endif

	/*
	 * Verify the message sm_vers
	 */
	if (msg->sm_vers != SPANS_VERS_1_0) {
		log(LOG_ERR, "spans: invalid message version 0x%x\n",
				msg->sm_vers);
	}

	/*
	 * Ignore the message if SPANS isn't up yet
	 */
	if (spp->sp_state != SPANS_ACTIVE &&
			(spp->sp_state != SPANS_PROBE ||
			(msg->sm_type != SPANS_STAT_REQ &&
			msg->sm_type != SPANS_STAT_RSP &&
			msg->sm_type != SPANS_STAT_IND))) {
		goto done;
	}

	/*
	 * Process the message based on its type
	 */
	switch(msg->sm_type) {
	case SPANS_STAT_REQ:
		spans_status_ind(spp, msg);
		break;
	case SPANS_STAT_IND:
		spans_status_ind(spp, msg);
		break;
	case SPANS_STAT_RSP:
		spans_status_rsp(spp, msg);
		break;
	case SPANS_OPEN_REQ:
		spans_open_req(spp, msg);
		break;
	case SPANS_OPEN_IND:
		spans_open_req(spp, msg);
		break;
	case SPANS_OPEN_RSP:
		spans_open_rsp(spp, msg);
		break;
	case SPANS_OPEN_CNF:
		spans_open_rsp(spp, msg);
		break;
	case SPANS_CLOSE_REQ:
		spans_close_req(spp, msg);
		break;
	case SPANS_CLOSE_IND:
		spans_close_req(spp, msg);
		break;
	case SPANS_CLOSE_RSP:
		spans_close_rsp(spp, msg);
		break;
	case SPANS_CLOSE_CNF:
		spans_close_rsp(spp, msg);
		break;
	case SPANS_RCLOSE_REQ:
		spans_close_req(spp, msg);
		break;
	case SPANS_RCLOSE_IND:
		spans_close_req(spp, msg);
		break;
	case SPANS_RCLOSE_RSP:
		spans_close_rsp(spp, msg);
		break;
	case SPANS_RCLOSE_CNF:
		spans_close_rsp(spp, msg);
		break;
	case SPANS_MULTI_REQ:
		spans_multi_req(spp, msg);
		break;
	case SPANS_MULTI_IND:
		spans_multi_req(spp, msg);
		break;
	case SPANS_MULTI_RSP:
		log(LOG_ERR,
			"spans: unexpected message (multi_rsp)\n");
		break;
	case SPANS_MULTI_CNF:
		log(LOG_ERR,
			"spans: unexpected message (multi_conf)\n");
		break;
	case SPANS_ADD_REQ:
		spans_add_req(spp, msg);
		break;
	case SPANS_ADD_IND:
		spans_add_req(spp, msg);
		break;
	case SPANS_ADD_RSP:
		log(LOG_ERR,
			"spans: unexpected message (add_rsp)\n");
		break;
	case SPANS_ADD_CNF:
		log(LOG_ERR, "spans: unexpected message (add_conf)\n");
		break;
	case SPANS_JOIN_REQ:
		spans_join_req(spp, msg);
		break;
	case SPANS_JOIN_CNF:
		log(LOG_ERR, "spans: unexpected message (join_conf)\n");
		break;
	case SPANS_LEAVE_REQ:
		spans_leave_req(spp, msg);
		break;
	case SPANS_LEAVE_CNF:
		log(LOG_ERR,
			"spans: unexpected message (leave_conf)\n");
		break;
	case SPANS_VCIR_IND:
		spans_vcir_ind(spp, msg);
		break;
	case SPANS_QUERY_REQ:
		spans_query_req(spp, msg);
		break;
	case SPANS_QUERY_RSP:
		log(LOG_ERR,
			"spans: unexpected message (query_rsp)\n");
		break;
	default:
		log(LOG_ERR, "spans: unknown SPANS message type %d\n",
				msg->sm_type);
	}

done:
	/*
	 * Free the incoming message (both buffer and internal format) if
	 * necessary.
	 */
	if (msg)
		atm_free(msg);
	if (m)
		KB_FREEALL(m);
}
