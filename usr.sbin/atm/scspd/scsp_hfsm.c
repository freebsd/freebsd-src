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
 *	@(#) $FreeBSD: src/usr.sbin/atm/scspd/scsp_hfsm.c,v 1.3 1999/08/28 01:15:33 peter Exp $
 *
 */


/*
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * HELLO finite state machine
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/queue.h> 
#include <netatm/atm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>
  
#include <errno.h>
#include <libatm.h>
#include <stdio.h>
#include <syslog.h>

#include "scsp_msg.h"
#include "scsp_if.h"
#include "scsp_var.h"

#ifndef lint
__RCSID("@(#) $FreeBSD: src/usr.sbin/atm/scspd/scsp_hfsm.c,v 1.3 1999/08/28 01:15:33 peter Exp $");
#endif


/*
 * HELLO FSM actions
 */
#define	HELLO_ACTION_CNT	7
int	scsp_hello_act_00 __P((Scsp_dcs *, Scsp_msg *));
int	scsp_hello_act_01 __P((Scsp_dcs *, Scsp_msg *));
int	scsp_hello_act_02 __P((Scsp_dcs *, Scsp_msg *));
int	scsp_hello_act_03 __P((Scsp_dcs *, Scsp_msg *));
int	scsp_hello_act_04 __P((Scsp_dcs *, Scsp_msg *));
int	scsp_hello_act_05 __P((Scsp_dcs *, Scsp_msg *));
int	scsp_hello_act_06 __P((Scsp_dcs *, Scsp_msg *));

static int (*scsp_action_vector[HELLO_ACTION_CNT])() = {
	scsp_hello_act_00,
	scsp_hello_act_01,
	scsp_hello_act_02,
	scsp_hello_act_03,
	scsp_hello_act_04,
	scsp_hello_act_05,
	scsp_hello_act_06
};

/*
 * HELLO FSM state table
 */
static int hello_state_table[SCSP_HFSM_EVENT_CNT][SCSP_HFSM_STATE_CNT] = {
	/* 0  1  2  3		     */
	{  1, 1, 1, 1 },	/* 0 */
	{  0, 2, 2, 2 },	/* 1 */
	{  0, 3, 3, 3 },	/* 2 */
	{  0, 0, 4, 4 },	/* 3 */
	{  0, 5, 5, 6 },	/* 4 */
};

/*
 * HELLO finite state machine
 *
 * Arguments:
 *	dcsp	pointer to a DCS control block for the neighbor
 *	event	the event which has occurred
 *	msg	pointer to received message, if there is one
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_hfsm(dcsp, event, msg)
	Scsp_dcs	*dcsp;
	int		event;
	Scsp_msg	*msg;
{
	int	action, rc, state;

	/*
	 * Select an action from the state table
	 */
	state = dcsp->sd_hello_state;
	action = hello_state_table[event][state];
	if (scsp_trace_mode & SCSP_TRACE_HFSM) {
		scsp_trace("HFSM: state=%d, event=%d, action=%d\n",
				state, event, action);
	}
	if (action >= HELLO_ACTION_CNT || action <= 0) {
		scsp_log(LOG_ERR, "Hello FSM--invalid action %d; state=%d, event=%d",
				action, dcsp->sd_hello_state, event);
		abort();
	}

	/*
	 * Perform the selected action
	 */
	rc = scsp_action_vector[action](dcsp, msg);

	return(rc);
}


/*
 * HELLO finite state machine action 0
 * Unexpected action -- log an error message
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to received message (ignored)
 *
 * Returns:
 *	EOPNOTSUPP	always returns EOPNOTSUPP
 *
 */
int
scsp_hello_act_00(dcsp, msg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
{
	scsp_log(LOG_ERR, "Hello FSM error--unexpected action, state=%d",
			dcsp->sd_hello_state);
	return(EOPNOTSUPP);
}


/*
 * HELLO finite state machine action 1
 * VCC open -- send HELLO message, start hello timer, go to Waiting
 * state
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to received message (ignored)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_hello_act_01(dcsp, msg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
{
	int		rc;

	/*
	 * Cancel the VCC open timer if it's running
	 */
	HARP_CANCEL(&dcsp->sd_open_t);

	/*
	 * Go to Waiting state
	 */
	dcsp->sd_hello_state = SCSP_HFSM_WAITING;

	/*
	 * Send a Hello message
	 */
	rc = scsp_send_hello(dcsp);
	if (rc == 0) {
		/*
		 * Success--start the Hello timer
		 */
		HARP_TIMER(&dcsp->sd_hello_h_t, SCSP_HELLO_Interval,
				scsp_hello_timeout);
	}

	return(rc);
}


/*
 * HELLO finite state machine action 2
 * VCC closed -- notify CA FSM, go to Down state, try to re-open VCC
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to received message (ignored)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_hello_act_02(dcsp, msg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
{
	int		rc;

	/*
	 * Cancel any current timers
	 */
	HARP_CANCEL(&dcsp->sd_hello_h_t);
	HARP_CANCEL(&dcsp->sd_hello_rcv_t);

	/*
	 * Log the loss of the VCC
	 */
	if (dcsp->sd_hello_state > SCSP_HFSM_WAITING) {
		scsp_log(LOG_ERR, "VC to %s closed",
				format_atm_addr(&dcsp->sd_addr));
	}

	/*
	 * Tell the CA FSM that the conection to the DCS is lost
	 */
	rc = scsp_cafsm(dcsp, SCSP_CAFSM_HELLO_DOWN, (void *)0);

	/*
	 * Go to Down state
	 */
	dcsp->sd_hello_state = SCSP_HFSM_DOWN;

	/*
	 * If our ID is lower than the DCS's, wait a second before
	 * trying to connect.  This should keep both of us from
	 * trying to connect at the same time, resulting in two
	 * VCCs being open.
	 */
	if (scsp_cmp_id(&dcsp->sd_server->ss_lsid,
			&dcsp->sd_dcsid) < 0) {
		/*
		 * Our ID is lower--start the VCC open timer for one
		 * second so we'll try to open the VCC if the DCS
		 * doesn't do it by then
		 */
		HARP_TIMER(&dcsp->sd_open_t, 1, scsp_open_timeout);
	} else {
		/*
		 * Our ID is higher--try to reopen the VCC immediately
		 */
		if (scsp_dcs_connect(dcsp)) {
			/*
			 * Conncect failed -- set a timer and try
			 * again later
			 */
			HARP_TIMER(&dcsp->sd_open_t, SCSP_Open_Interval,
					scsp_open_timeout);
		}
	}

	return(0);
}


/*
 * HELLO finite state machine action 3
 * Hello timer expired -- send HELLO message, restart hello timer
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to received message (ignored)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_hello_act_03(dcsp, msg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
{
	int		rc;

	/*
	 * Send a Hello message
	 */
	rc = scsp_send_hello(dcsp);
	if (rc == 0) {
		/*
		 * Success--restart the Hello timer
		 */
		HARP_TIMER(&dcsp->sd_hello_h_t, SCSP_HELLO_Interval,
				scsp_hello_timeout);
	}

	return(rc);
}


/*
 * HELLO finite state machine action 4
 * Receive timer expired -- if we haven't received any Hellos, notify
 * CA FSM and go to Waiting state;  if we've received Hellos, but we
 * weren't in the receiver ID list, go to Unidirectional state
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to received message (ignored)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_hello_act_04(dcsp, msg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
{
	int	rc = 0;

	/*
	 * Check whether we'ver received any Hellos lately
	 */
	if (dcsp->sd_hello_rcvd) {
		/*
		 * We've had Hellos since the receive timer was
		 * started--go to Unidirectional state
		 */
		dcsp->sd_hello_rcvd = 0;
		dcsp->sd_hello_state = SCSP_HFSM_UNI_DIR;
	} else {
		/*
		 * We haven't seen any Hellos at all from the DCS in
		 * hello_interval * dead_factor seconds--go to Waiting
		 * state
		 */
		dcsp->sd_hello_state = SCSP_HFSM_WAITING;
	}

	/*
	 * Notify the CA FSM
	 */
	rc = scsp_cafsm(dcsp, SCSP_CAFSM_HELLO_DOWN, (void *)0);

	return(rc);
}


/*
 * HELLO finite state machine action 5
 * Message received -- Ignore all but HELLO messages;  if local server
 * is in receiver list, notify CA FSM and go to Bidirectional state;
 * otherwise, go to Unidirectional state
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to received message
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_hello_act_05(dcsp, msg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
{
	int	rc;
	Scsp_id	*ridp;

	/*
	 * Null message pointer means message decode failed, so
	 * message must have been invalid.  Go to Waiting state.
	 */
	if (msg == (Scsp_msg *)0) {
		dcsp->sd_hello_state = SCSP_HFSM_WAITING;
		HARP_CANCEL(&dcsp->sd_hello_rcv_t);
		return(0);
	}

	/*
	 * Ignore the message if it isn't a Hello
	 */
	if (msg->sc_msg_type != SCSP_HELLO_MSG) {
		return(0);
	}

	/*
	 * Save relevant information about DCS, but don't let him give
	 * us zero for timeout values
	 */
	if (msg->sc_hello->hello_int) {
		dcsp->sd_hello_int = msg->sc_hello->hello_int;
	} else {
		dcsp->sd_hello_int = 1;
	}
	if (msg->sc_hello->dead_factor) {
		dcsp->sd_hello_df = msg->sc_hello->dead_factor;
	} else {
		dcsp->sd_hello_df = 1;
	}
	dcsp->sd_dcsid = msg->sc_hello->hello_mcp.sid;

	/*
	 * Check the message for the local server's ID
	 */
	for (ridp = &msg->sc_hello->hello_mcp.rid;
			ridp;
			ridp = ridp->next) {
		if (scsp_cmp_id(&dcsp->sd_server->ss_lsid, ridp) == 0) {
			/*
			 * Cancel and restart the receive timer
			 */
			HARP_CANCEL(&dcsp->sd_hello_rcv_t);
			HARP_TIMER(&dcsp->sd_hello_rcv_t,
					dcsp->sd_hello_int * dcsp->sd_hello_df,
					scsp_hello_rcv_timeout);

			/*
			 * Go to Bidirectional state and notify the
			 * CA FSM that the connection is up
			 */
			dcsp->sd_hello_state = SCSP_HFSM_BI_DIR;
			rc = scsp_cafsm(dcsp,
					SCSP_CAFSM_HELLO_UP,
					(void *)0);
			return(rc);
		}
	}

	/*
	 * We weren't in the receiver ID list, so go to
	 * Unidirectional state
	 */
	dcsp->sd_hello_state = SCSP_HFSM_UNI_DIR;

	return(0);
}


/*
 * HELLO finite state machine action 6
 * Message received -- if message is not a HELLO, pass it to the CA
 * FSM;  otherwise, if local server is not in receiver list, notify
 * CA FSM and go to Unidirectional state
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	msg	pointer to received message
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_hello_act_06(dcsp, msg)
	Scsp_dcs	*dcsp;
	Scsp_msg	*msg;
{
	int	rc = 0, rcv_found;
	Scsp_id	*ridp;

	/*
	 * Null message pointer means message decode failed, so
	 * message must have been invalid.  Go to Waiting state.
	 */
	if (msg == (Scsp_msg *)0) {
		HARP_CANCEL(&dcsp->sd_hello_rcv_t);
		dcsp->sd_hello_state = SCSP_HFSM_WAITING;
		rc = scsp_cafsm(dcsp, SCSP_CAFSM_HELLO_DOWN, (void *)0);
		return(rc);
	}

	/*
	 * Process the message depending on its type
	 */
	switch(msg->sc_msg_type) {
	case SCSP_CA_MSG:
		rc = scsp_cafsm(dcsp, SCSP_CAFSM_CA_MSG, (void *)msg);
		break;
	case SCSP_CSU_REQ_MSG:
		rc = scsp_cafsm(dcsp, SCSP_CAFSM_CSU_REQ, (void *)msg);
		break;
	case SCSP_CSU_REPLY_MSG:
		rc = scsp_cafsm(dcsp, SCSP_CAFSM_CSU_REPLY,
				(void *)msg);
		break;
	case SCSP_CSUS_MSG:
		rc = scsp_cafsm(dcsp, SCSP_CAFSM_CSUS_MSG, (void *)msg);
		break;
	case SCSP_HELLO_MSG:
		/*
		 * Make sure DCS info is consistent.  The sender ID,
		 * family ID, protocol ID, and server group ID are
		 * checked.
		 */
		if (scsp_cmp_id(&msg->sc_hello->hello_mcp.sid,
					&dcsp->sd_dcsid) ||
				(msg->sc_hello->family_id !=
					dcsp->sd_server->ss_fid) ||
				(msg->sc_hello->hello_mcp.pid !=
					dcsp->sd_server->ss_pid) ||
				(msg->sc_hello->hello_mcp.sgid !=
					dcsp->sd_server->ss_sgid)) {
			/*
			 * Bad info--revert to waiting state
			 */
			HARP_CANCEL(&dcsp->sd_hello_rcv_t);
			dcsp->sd_hello_state = SCSP_HFSM_WAITING;
			rc = scsp_cafsm(dcsp,
					SCSP_CAFSM_HELLO_DOWN,
					(void *)0);
			return(rc);
		}

		/*
		 * Mark the arrival of the Hello message
		 */
		dcsp->sd_hello_rcvd = 1;

		/*
		 * Check the message for the local server's ID
		 */
		for (ridp = &msg->sc_hello->hello_mcp.rid,
					rcv_found = 0;
				ridp;
				ridp = ridp->next) {
			rcv_found = (scsp_cmp_id(ridp,
				&dcsp->sd_server->ss_lsid) == 0);
		}

		if (rcv_found) {
			/*
			 * The LS ID was in the list of receiver IDs--
			 * Reset the Hello receive timer
			 */
			dcsp->sd_hello_rcvd = 0;
			HARP_CANCEL(&dcsp->sd_hello_rcv_t);
			HARP_TIMER(&dcsp->sd_hello_rcv_t,
					dcsp->sd_hello_int *
						dcsp->sd_hello_df,
					scsp_hello_rcv_timeout);
		}
		break;
	}

	return(rc);
}
