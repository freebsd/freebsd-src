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
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * Interface to client server protocol
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h> 
#include <netatm/queue.h> 
#include <netatm/atm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>
  
#include <errno.h>
#include <libatm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "scsp_msg.h"
#include "scsp_if.h"
#include "scsp_var.h"

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * SCSP client server interface FSM actions
 */
#define	SCSP_CIFSM_ACTION_CNT	11
int	scsp_client_act_00
			__P((Scsp_dcs *, Scsp_msg *, Scsp_if_msg *));
int	scsp_client_act_01
			__P((Scsp_dcs *, Scsp_msg *, Scsp_if_msg *));
int	scsp_client_act_02
			__P((Scsp_dcs *, Scsp_msg *, Scsp_if_msg *));
int	scsp_client_act_03
			__P((Scsp_dcs *, Scsp_msg *, Scsp_if_msg *));
int	scsp_client_act_04
			__P((Scsp_dcs *, Scsp_msg *, Scsp_if_msg *));
int	scsp_client_act_05
			__P((Scsp_dcs *, Scsp_msg *, Scsp_if_msg *));
int	scsp_client_act_06
			__P((Scsp_dcs *, Scsp_msg *, Scsp_if_msg *));
int	scsp_client_act_07
			__P((Scsp_dcs *, Scsp_msg *, Scsp_if_msg *));
int	scsp_client_act_08
			__P((Scsp_dcs *, Scsp_msg *, Scsp_if_msg *));
int	scsp_client_act_09
			__P((Scsp_dcs *, Scsp_msg *, Scsp_if_msg *));
int	scsp_client_act_10
			__P((Scsp_dcs *, Scsp_msg *, Scsp_if_msg *));

static int (*scsp_action_vector[SCSP_CIFSM_ACTION_CNT])() = {
	scsp_client_act_00,
	scsp_client_act_01,
	scsp_client_act_02,
	scsp_client_act_03,
	scsp_client_act_04,
	scsp_client_act_05,
	scsp_client_act_06,
	scsp_client_act_07,
	scsp_client_act_08,
	scsp_client_act_09,
	scsp_client_act_10
};


/*
 * Client server interface FSM state table
 */
static int client_state_table[SCSP_CIFSM_EVENT_CNT][SCSP_CIFSM_STATE_CNT] = {
	/* 0   1   2   3  */
	{  1,  3,  3,  3 },	/*  0 */
	{  2,  5,  5,  5 },	/*  1 */
	{  0,  4,  0,  0 },	/*  2 */
	{  0,  6,  6,  1 },	/*  3 */
	{  1,  0,  7,  7 },	/*  4 */
	{  7,  7,  7,  7 },	/*  5 */
	{  1,  1,  8,  8 },	/*  6 */
	{  0,  0, 10, 10 },	/*  7 */
	{  0,  0,  1,  1 },	/*  8 */
	{  0,  0,  9,  9 }	/*  9 */
};


/*
 * SCSP client server interface finite state machine
 *
 * Arguments:
 *	ssp	pointer to server control block
 *	event	the event which has occurred
 *	msg	pointer to message from DCS, if there is one
 *	cmsg	pointer to message from server, if there is one
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_cfsm(dcsp, event, msg, cmsg)
	Scsp_dcs	*dcsp;
	int		event;
	Scsp_msg	*msg;
	Scsp_if_msg	*cmsg;
{
	int	action, rc, state;

	/*
	 * Select an action from the state table
	 */
	state = dcsp->sd_client_state;
	action = client_state_table[event][state];
	if (scsp_trace_mode & SCSP_TRACE_CFSM) {
		scsp_trace("Server I/F FSM: state=%d, event=%d, action=%d\n",
				state, event, action);
	}
	if (action >= SCSP_CIFSM_ACTION_CNT || action <= 0) {
		scsp_log(LOG_ERR, "Server I/F FSM--invalid action %d; state=%d, event=%d",
				action, dcsp->sd_client_state, event);
		exit(1);
	}

	/*
	 * Perform the selected action
	 */
	rc = scsp_action_vector[action](dcsp, msg, cmsg);

	return(rc);
}


/*
 * SCSP client server interface finite state machine action 0
 * Unexpected action -- log an error message
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to message from DCS (ignored)
 *	cmsg	pointer to message from server (ignored)
 *
 * Returns:
 *	EOPNOTSUPP	always returns EOPNOTSUPP
 *
 */
int
scsp_client_act_00(dcsp, msg, cmsg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
	Scsp_if_msg	*cmsg;
{
	scsp_log(LOG_ERR, "Server I/F FSM error--unexpected action, state=%d",
			dcsp->sd_client_state);
	return(EOPNOTSUPP);
}


/*
 * SCSP client server interface finite state machine action 1
 *
 * Ignore an event
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to message from DCS
 *	cmsg	pointer to message from server
 *
 * Returns:
 *	0	always returns 0
 *
 */
int
scsp_client_act_01(dcsp, msg, cmsg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
	Scsp_if_msg	*cmsg;
{
	return(0);
}


/*
 * SCSP client server interface finite state machine action 2
 *
 * CA FSM went to Cache Summarize state--go to Summarize
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to message from DCS
 *	cmsg	pointer to message from server
 *
 * Returns:
 *	0	success
 *	else	errno describing error
 *
 */
int
scsp_client_act_02(dcsp, msg, cmsg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
	Scsp_if_msg	*cmsg;
{
	/*
	 * Set the new state
	 */
	dcsp->sd_client_state = SCSP_CIFSM_SUM;

	return(0);
}


/*
 * SCSP client server interface finite state machine action 3
 *
 * CA FSM went down--clean up and go to Null
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to message from DCS
 *	cmsg	pointer to message from server
 *
 * Returns:
 *	0	success
 *	else	errno describing error
 *
 */
int
scsp_client_act_03(dcsp, msg, cmsg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
	Scsp_if_msg	*cmsg;
{
	/*
	 * Set the new state
	 */
	dcsp->sd_client_state = SCSP_CIFSM_NULL;

	return(0);
}


/*
 * SCSP client server interface finite state machine action 4
 *
 * CA FSM went to Update Cache state--go to Update state
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to message from DCS
 *	cmsg	pointer to message from server
 *
 * Returns:
 *	0	success
 *	else	errno describing error
 *
 */
int
scsp_client_act_04(dcsp, msg, cmsg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
	Scsp_if_msg	*cmsg;
{
	/*
	 * Set the new state
	 */
	dcsp->sd_client_state = SCSP_CIFSM_UPD;

	return(0);
}


/*
 * SCSP client server interface finite state machine action 5
 *
 * The CA FSM went to Cache Summarize state from Summarize,
 * Update, or Aligned, implying that the CA FSM went down and came
 * back up--copy the server's cache to the DCSs CSAS list and go to
 * Summarize state
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to message from DCS
 *	cmsg	pointer to message from server
 *
 * Returns:
 *	0	success
 *	else	errno describing error
 *
 */
int
scsp_client_act_05(dcsp, msg, cmsg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
	Scsp_if_msg	*cmsg;
{
	int		i;
	Scsp_cse	*csep, *ncsep;

	/*
	 * Copy the cache summmary to the CSAS list
	 */
	for (i = 0; i < SCSP_HASHSZ; i++) {
		for (csep = dcsp->sd_server->ss_cache[i]; csep;
				csep = csep->sc_next) {
			ncsep = scsp_dup_cse(csep);
			LINK2TAIL(ncsep, Scsp_cse, dcsp->sd_ca_csas,
					sc_next);
		}
	}

	/*
	 * Set the new state
	 */
	dcsp->sd_client_state = SCSP_CIFSM_SUM;

	return(0);
}


/*
 * SCSP client server interface finite state machine action 6
 *
 * CA FSM went to Aligned state--go to Aligned
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to message from DCS
 *	cmsg	pointer to message from server
 *
 * Returns:
 *	0	success
 *	else	errno describing error
 *
 */
int
scsp_client_act_06(dcsp, msg, cmsg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
	Scsp_if_msg	*cmsg;
{
	/*
	 * Set the new state
	 */
	dcsp->sd_client_state = SCSP_CIFSM_ALIGN;

	return(0);
}


/*
 * SCSP client server interface finite state machine action 7
 *
 * We received a Solicit Rsp or Update Req from the server--pass it
 * to the CA FSM
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to message from DCS
 *	cmsg	pointer to message from server
 *
 * Returns:
 *	0	success
 *	else	errno describing error
 *
 */
int
scsp_client_act_07(dcsp, msg, cmsg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
	Scsp_if_msg	*cmsg;
{
	int		rc;
	Scsp_csa	*csap;
	Scsp_atmarp_csa	*acp;

	/*
	 * Allocate memory for a CSA record
	 */
	csap = (Scsp_csa *)UM_ALLOC(sizeof(Scsp_csa));
	if (!csap) {
		scsp_mem_err("scsp_client_act_07: sizeof(Scsp_csa)");
	}
	acp = (Scsp_atmarp_csa *)UM_ALLOC(sizeof(Scsp_atmarp_csa));
	if (!acp) {
		scsp_mem_err("scsp_client_act_07: sizeof(Scsp_atmarp_csa)");
	}
	UM_ZERO(csap, sizeof(Scsp_csa));
	UM_ZERO(acp, sizeof(Scsp_atmarp_csa));

	/*
	 * Build a CSA record from the server's message
	 */
	csap->hops = dcsp->sd_hops;
	csap->null = (cmsg->si_atmarp.sa_state == SCSP_ASTATE_DEL) ||
			(cmsg->si_type == SCSP_SOLICIT_RSP &&
			cmsg->si_rc != SCSP_RSP_OK);
	csap->seq = cmsg->si_atmarp.sa_seq;
	csap->key = cmsg->si_atmarp.sa_key;
	csap->oid = cmsg->si_atmarp.sa_oid;
	csap->atmarp_data = acp;
	acp->sa_state = cmsg->si_atmarp.sa_state;
	acp->sa_sha = cmsg->si_atmarp.sa_cha;
	acp->sa_ssa = cmsg->si_atmarp.sa_csa;
	acp->sa_spa = cmsg->si_atmarp.sa_cpa;
	acp->sa_tpa = cmsg->si_atmarp.sa_cpa;

	/*
	 * Call the CA FSM
	 */
	rc = scsp_cafsm(dcsp, SCSP_CAFSM_CACHE_UPD, (void *)csap);

	return(rc);
}


/*
 * SCSP client server interface finite state machine action 8
 *
 * Update Rsp from server--pass the update to the CA FSM.
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to message from DCS
 *	cmsg	pointer to message from server
 *
 * Returns:
 *	0	success
 *	else	errno describing error
 *
 */
int
scsp_client_act_08(dcsp, msg, cmsg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
	Scsp_if_msg	*cmsg;
{
	int		rc;

	/* 
	 * Pass the response to the CA FSM
	 */
	switch (dcsp->sd_server->ss_pid) {
	case SCSP_PROTO_ATMARP:
		rc = scsp_cafsm(dcsp, SCSP_CAFSM_CACHE_RSP, cmsg);
		break;
	default:
		rc = EPROTONOSUPPORT;
	}

	return(rc);
}


/*
 * SCSP client server interface finite state machine action 9
 *
 * CSU Solicit from DCS--pass Solicit Ind to server
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to message from DCS
 *	cmsg	pointer to message from server
 *
 * Returns:
 *	0	success
 *	else	errno describing error
 *
 */
int
scsp_client_act_09(dcsp, msg, cmsg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
	Scsp_if_msg	*cmsg;
{
	int		rc, rrc = 0;
	Scsp_csa	*csap;
	Scsp_if_msg	*csip;

	/*
	 * Get memory for a Solicit Ind
	 */
	csip = (Scsp_if_msg *)UM_ALLOC(sizeof(Scsp_if_msg));
	if (!csip) {
		scsp_mem_err("scsp_client_act_09: sizeof(Scsp_if_msg)");
	}

	/*
	 * Loop through list of CSAs
	 */
	for (csap = msg->sc_csu_msg->csu_csa_rec; csap;
			csap = csap->next) {
		/*
		 * Fill out the Solicit Indication
		 */
		UM_ZERO(csip, sizeof(Scsp_if_msg));
		csip->si_type = SCSP_SOLICIT_IND;
		csip->si_proto = dcsp->sd_server->ss_pid;
		csip->si_tok = (u_long)dcsp;
		csip->si_len = sizeof(Scsp_if_msg_hdr) +
				sizeof(Scsp_sum_msg);
		csip->si_sum.ss_hops = csap->hops;
		csip->si_sum.ss_null = csap->null;
		csip->si_sum.ss_seq = csap->seq;
		csip->si_sum.ss_key = csap->key;
		csip->si_sum.ss_oid = csap->oid;

		/*
		 * Send the Solicit Ind to the server
		 */
		rc = scsp_if_sock_write(dcsp->sd_server->ss_sock, csip);
		if (rc) {
			rrc = rc;
		}
	}

	UM_FREE(csip);
	return(rrc);
}


/*
 * SCSP client server interface finite state machine action 10
 *
 * CSU Request from DCS--pass it to the server as a Cache Update
 * Indication
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to message from DCS
 *	cmsg	pointer to message from server
 *
 * Returns:
 *	0	success
 *	else	errno describing error
 *
 */
int
scsp_client_act_10(dcsp, msg, cmsg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
	Scsp_if_msg	*cmsg;
{
	int		rc, rrc = 0;
	Scsp_csa	*csap;
	Scsp_atmarp_csa	*acp;
	Scsp_if_msg	*cuip;

	/*
	 * Get memory for a Cache Update Ind
	 */
	cuip = (Scsp_if_msg *)UM_ALLOC(sizeof(Scsp_if_msg));
	if (!cuip) {
		scsp_mem_err("scsp_client_act_10: sizeof(Scsp_if_msg)");
	}

	/*
	 * Loop through CSAs in message
	 */
	for (csap = msg->sc_csu_msg->csu_csa_rec; csap;
			csap = csap->next) {
		acp = csap->atmarp_data;
		if (!acp)
			continue;

		/*
		 * Fill out the Cache Update Ind
		 */
		UM_ZERO(cuip, sizeof(Scsp_if_msg));
		cuip->si_type = SCSP_UPDATE_IND;
		cuip->si_proto = dcsp->sd_server->ss_pid;
		cuip->si_tok = (u_long)dcsp;
		switch(dcsp->sd_server->ss_pid) {
		case SCSP_PROTO_ATMARP:
			cuip->si_len = sizeof(Scsp_if_msg_hdr) +
					sizeof(Scsp_atmarp_msg);
			cuip->si_atmarp.sa_state = acp->sa_state;
			cuip->si_atmarp.sa_cpa = acp->sa_spa;
			cuip->si_atmarp.sa_cha = acp->sa_sha;
			cuip->si_atmarp.sa_csa = acp->sa_ssa;
			cuip->si_atmarp.sa_key = csap->key;
			cuip->si_atmarp.sa_oid = csap->oid;
			cuip->si_atmarp.sa_seq = csap->seq;
			break;
		case SCSP_PROTO_NHRP:
			/*
			 * Not implemented yet
			 */
			break;
		}

		/*
		 * Send the Cache Update Ind to the server
		 */
		rc = scsp_if_sock_write(dcsp->sd_server->ss_sock, cuip);
		if (rc) {
			rrc = rc;
		}
	}

	UM_FREE(cuip);
	return(rrc);
}
