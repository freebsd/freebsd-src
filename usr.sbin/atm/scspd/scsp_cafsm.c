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
 *	@(#) $FreeBSD: src/usr.sbin/atm/scspd/scsp_cafsm.c,v 1.3 1999/08/28 01:15:32 peter Exp $
 *
 */


/*
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * Cache Alignment finite state machine
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

#include "scsp_msg.h"
#include "scsp_if.h"
#include "scsp_var.h"

#ifndef lint
__RCSID("@(#) $FreeBSD: src/usr.sbin/atm/scspd/scsp_cafsm.c,v 1.3 1999/08/28 01:15:32 peter Exp $");
#endif


/*
 * CA FSM actions
 */
#define	CA_ACTION_CNT	20
int	scsp_ca_act_00 __P((Scsp_dcs *, void *));
int	scsp_ca_act_01 __P((Scsp_dcs *, void *));
int	scsp_ca_act_02 __P((Scsp_dcs *, void *));
int	scsp_ca_act_03 __P((Scsp_dcs *, void *));
int	scsp_ca_act_04 __P((Scsp_dcs *, void *));
int	scsp_ca_act_05 __P((Scsp_dcs *, void *));
int	scsp_ca_act_06 __P((Scsp_dcs *, void *));
int	scsp_ca_act_07 __P((Scsp_dcs *, void *));
int	scsp_ca_act_08 __P((Scsp_dcs *, void *));
int	scsp_ca_act_09 __P((Scsp_dcs *, void *));
int	scsp_ca_act_10 __P((Scsp_dcs *, void *));
int	scsp_ca_act_11 __P((Scsp_dcs *, void *));
int	scsp_ca_act_12 __P((Scsp_dcs *, void *));
int	scsp_ca_act_13 __P((Scsp_dcs *, void *));
int	scsp_ca_act_14 __P((Scsp_dcs *, void *));
int	scsp_ca_act_15 __P((Scsp_dcs *, void *));
int	scsp_ca_act_16 __P((Scsp_dcs *, void *));
int	scsp_ca_act_17 __P((Scsp_dcs *, void *));
int	scsp_ca_act_18 __P((Scsp_dcs *, void *));
int	scsp_ca_act_19 __P((Scsp_dcs *, void *));

static int (*scsp_ca_act_vec[CA_ACTION_CNT])() = {
	scsp_ca_act_00,
	scsp_ca_act_01,
	scsp_ca_act_02,
	scsp_ca_act_03,
	scsp_ca_act_04,
	scsp_ca_act_05,
	scsp_ca_act_06,
	scsp_ca_act_07,
	scsp_ca_act_08,
	scsp_ca_act_09,
	scsp_ca_act_10,
	scsp_ca_act_11,
	scsp_ca_act_12,
	scsp_ca_act_13,
	scsp_ca_act_14,
	scsp_ca_act_15,
	scsp_ca_act_16,
	scsp_ca_act_17,
	scsp_ca_act_18,
	scsp_ca_act_19
};

/*
 * CA FSM state table
 */
static int ca_state_table[SCSP_CAFSM_EVENT_CNT][SCSP_CAFSM_STATE_CNT] = {
	/* 0   1   2   3   4   5	      */
	{  1,  1,  1,  1,  1,  1 },	/*  0 */
	{  2,  2,  2,  2,  2,  2 },	/*  1 */
	{  0,  3,  4,  5, 15, 15 },	/*  2 */
	{  0, 17, 17, 17,  7,  7 },	/*  3 */
	{  0, 17, 17, 17,  8,  8 },	/*  4 */
	{  0, 17, 17, 17, 10, 10 },	/*  5 */
	{  0,  6,  6,  0,  9,  9 },	/*  6 */
	{  0,  0,  0,  0, 12, 12 },	/*  7 */
	{  0,  0,  0,  0, 13, 13 },	/*  8 */
	{ 18, 14, 14, 14, 11, 11 },	/*  9 */
	{  0, 19,  0,  0, 16, 16 },	/* 10 */
};


/*
 * Cache Alignment finite state machine
 *
 * Arguments:
 *	dcsp	pointer to a DCS control block for the neighbor
 *	event	the event which has occurred
 *	p	pointer to further parameter, if there is one
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_cafsm(dcsp, event, p)
	Scsp_dcs	*dcsp;
	int		event;
	void		*p;
{
	int	action, rc, state;

	/*
	 * Select an action from the state table
	 */
	state = dcsp->sd_ca_state;
	action = ca_state_table[event][state];
	if (scsp_trace_mode & SCSP_TRACE_CAFSM) {
		scsp_trace("CAFSM: state=%d, event=%d, action=%d\n",
				state, event, action);
	}
	if (action >= CA_ACTION_CNT || action < 0) {
		scsp_log(LOG_ERR, "CA FSM--invalid action state=%d, event=%d, action=%d",
				state, event, action);
		abort();
	}

	/*
	 * Perform the selected action
	 */
	rc = scsp_ca_act_vec[action](dcsp, p);

	return(rc);
}


/*
 * CA finite state machine action 0
 * Unexpected action -- log an error message and go to Master/Slave
 * Negotiation.  The unexpected action is probably from a protocol
 * error.
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	ignored
 *
 * Returns:
 *	EOPNOTSUPP	always returns EOPNOTSUPP
 *
 */
int
scsp_ca_act_00(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	int	rc;

	/*
	 * Log an error message
	 */
	scsp_log(LOG_ERR, "CA FSM error--unexpected action, state=%d",
			dcsp->sd_ca_state);

	/*
	 * Set the new state
	 */
	dcsp->sd_ca_state = SCSP_CAFSM_NEG;

	/*
	 * Clear out the DCS block
	 */
	scsp_dcs_cleanup(dcsp);

	/*
	 * Notify the client I/F FSM
	 */
	rc = scsp_cfsm(dcsp, SCSP_CIFSM_CA_DOWN, (Scsp_msg *)0,
			(Scsp_if_msg *)0);

	return(rc);
}


/*
 * CA finite state machine action 1
 * Hello FSM has reached Bidirectional state -- go to Master/Slave
 * Negotiation state, make a copy of the client's cache, send first CA
 * message.
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	ignored
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_01(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	int		i, rc;
	Scsp_cse	*csep, *dupp;

	/*
	 * Set the new state
	 */
	dcsp->sd_ca_state = SCSP_CAFSM_NEG;

	/*
	 * Make a copy of client's cache entries for cache alignment
	 */
	for (i = 0; i < SCSP_HASHSZ; i++) {
		for (csep = dcsp->sd_server->ss_cache[i];
				csep; csep = csep->sc_next) {
			dupp = scsp_dup_cse(csep);
			LINK2TAIL(dupp, Scsp_cse, dcsp->sd_ca_csas,
						sc_next);
		}
	}

	/*
	 * Select an initial sequence number
	 */
	dcsp->sd_ca_seq = (int)time((time_t *)0);

	/*
	 * Send a CA message
	 */
	rc = scsp_send_ca(dcsp);
	if (rc == 0) {
		HARP_TIMER(&dcsp->sd_ca_rexmt_t, dcsp->sd_ca_rexmt_int,
				scsp_ca_retran_timeout);
	}

	return(rc);
}


/*
 * CA finite state machine action 2
 * Hello FSM has gone down -- go to Down state
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	ignored
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_02(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	int		rc;

	/*
	 * Set the new state
	 */
	dcsp->sd_ca_state = SCSP_CAFSM_DOWN;

	/*
	 * Clear out the DCS block
	 */
	scsp_dcs_cleanup(dcsp);

	/*
	 * Notify the client I/F FSM
	 */
	rc = scsp_cfsm(dcsp, SCSP_CIFSM_CA_DOWN, (Scsp_msg *)0,
			(Scsp_if_msg *)0);

	return(rc);
}


/*
 * CA finite state machine action 3
 * CA message received -- select Cache Summarize Master or Slave state
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	pointer to received message
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_03(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	int		rc = 0;
	Scsp_msg	*msg = (Scsp_msg *)p;

	/*
	 * Check for slave role for LS
	 */
	if (msg->sc_ca->ca_m &&
			msg->sc_ca->ca_i &&
			msg->sc_ca->ca_o &&
			msg->sc_ca->ca_mcp.rec_cnt == 0 &&
			scsp_cmp_id(&msg->sc_ca->ca_mcp.sid,
				&msg->sc_ca->ca_mcp.rid) > 0) {

		/*
		 * Stop the retransmit timer
		 */
		HARP_CANCEL(&dcsp->sd_ca_rexmt_t);

		/*
		 * Set the new state
		 */
		dcsp->sd_ca_state = SCSP_CAFSM_SLAVE;
		(void)scsp_cfsm(dcsp, SCSP_CIFSM_CA_SUMM,
				(Scsp_msg *)0, (Scsp_if_msg *)0);

		/*
		 * Save the master's sequence number
		 */
		dcsp->sd_ca_seq = msg->sc_ca->ca_seq;

		/*
		 * Send a CA message
		 */
		rc = scsp_send_ca(dcsp);
	} else
	/*
	 * Check for master role for LS
	 */
	if (!msg->sc_ca->ca_m &&
			!msg->sc_ca->ca_i &&
			scsp_cmp_id(&msg->sc_ca->ca_mcp.sid,
				&msg->sc_ca->ca_mcp.rid) < 0) {
		/*
		 * Stop the retransmit timer
		 */
		HARP_CANCEL(&dcsp->sd_ca_rexmt_t);

		/*
		 * Set the new state
		 */
		dcsp->sd_ca_state = SCSP_CAFSM_MASTER;
		rc = scsp_cfsm(dcsp, SCSP_CIFSM_CA_SUMM,
				(Scsp_msg *)0, (Scsp_if_msg *)0);

		/*
		 * Process the CA message
		 */
		scsp_process_ca(dcsp, msg->sc_ca);

		/*
		 * Increment the sequence number
		 */
		dcsp->sd_ca_seq++;

		/*
		 * Send a CA in reply
		 */
		rc = scsp_send_ca(dcsp);
		if (rc == 0) {
			HARP_TIMER(&dcsp->sd_ca_rexmt_t,
					dcsp->sd_ca_rexmt_int,
					scsp_ca_retran_timeout);
		}
	} else {
		/*
		 * Ignore the message, go to Master/Slave Negotiation
		 */
		dcsp->sd_ca_state = SCSP_CAFSM_NEG;
	}

	return(rc);
}


/*
 * CA finite state machine action 4
 * CA message received while in Cache Summarize Master state -- process
 * CA message
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	pointer to received message
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_04(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	int		rc = 0;
	Scsp_msg	*msg = (Scsp_msg *)p;

	/*
	 * If the other side thinks he's the master, or if the
	 * initialization bit is set, or if the message is out
	 * of sequence, go back to Master/Slave Negotiation state
	 */
	if (msg->sc_ca->ca_m || msg->sc_ca->ca_i ||
			msg->sc_ca->ca_seq < dcsp->sd_ca_seq - 1 ||
			msg->sc_ca->ca_seq > dcsp->sd_ca_seq) {
		HARP_CANCEL(&dcsp->sd_ca_rexmt_t);
		dcsp->sd_ca_state = SCSP_CAFSM_NEG;
		scsp_dcs_cleanup(dcsp);
		return(scsp_ca_act_01(dcsp, (Scsp_msg *)0));
	}

	/*
	 * Ignore any duplicate messages
	 */
	if (msg->sc_ca->ca_seq == dcsp->sd_ca_seq - 1) {
		return(0);
	}

	/*
	 * Stop the retransmission timer
	 */
	HARP_CANCEL(&dcsp->sd_ca_rexmt_t);

	/*
	 * Process the CA message
	 */
	scsp_process_ca(dcsp, msg->sc_ca);

	/*
	 * Increment the CA sequence number
	 */
	dcsp->sd_ca_seq++;

	/*
	 * If we have no more CSAS records to send and the slave sent
	 * a message with the 'O' bit off, we're done with Summarize
	 * state
	 */
	if (!dcsp->sd_ca_csas && !msg->sc_ca->ca_o) {
		/*
		 * Free any CA message saved for retransmission
		 */
		if (dcsp->sd_ca_rexmt_msg) {
			scsp_free_msg(dcsp->sd_ca_rexmt_msg);
			dcsp->sd_ca_rexmt_msg = (Scsp_msg *)0;
		}

		/*
		 * If the CRL is empty, we go directly to Aligned state;
		 * otherwise, we go to Update Cache and send a CSUS
		 */
		if (!dcsp->sd_crl) {
			/*
			 * Go to Aligned state
			 */
			dcsp->sd_ca_state = SCSP_CAFSM_ALIGNED;
			rc = scsp_cfsm(dcsp, SCSP_CIFSM_CA_ALIGN,
					(Scsp_msg *)0,
					(Scsp_if_msg *)0);
		} else {
			/*
			 * Go to Cache Update state
			 */
			dcsp->sd_ca_state = SCSP_CAFSM_UPDATE;
			(void)scsp_cfsm(dcsp, SCSP_CIFSM_CA_UPD,
					(Scsp_msg *)0,
					(Scsp_if_msg *)0);
			rc = scsp_send_csus(dcsp);
		}
	} else {
		/*
		 * There are more CSAS records to be exchanged--
		 * continue the cache exchange
		 */
		rc = scsp_send_ca(dcsp);
	}

	return(rc);
}


/*
 * CA finite state machine action 5
 * CA message received while in Cache Summarize Slave state -- process
 * CA message
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	pointer to received message
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_05(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	int		rc = 0;
	Scsp_msg	*msg = (Scsp_msg *)p;

	/*
	 * If the other side thinks we're the master, or if the
	 * initialization bit is set, or if the message is out
	 * of sequence, go back to Master/Slave Negotiation state
	 */
	if (!msg->sc_ca->ca_m || msg->sc_ca->ca_i ||
			msg->sc_ca->ca_seq < dcsp->sd_ca_seq ||
			msg->sc_ca->ca_seq > dcsp->sd_ca_seq + 1) {
		HARP_CANCEL(&dcsp->sd_ca_rexmt_t);
		dcsp->sd_ca_state = SCSP_CAFSM_NEG;
		scsp_dcs_cleanup(dcsp);
		return(scsp_ca_act_01(dcsp, (Scsp_msg *)0));
	}

	/*
	 * If this is a duplicate, retransmit the last message
	 */
	if (msg->sc_ca->ca_seq == dcsp->sd_ca_seq) {
		if (dcsp->sd_ca_rexmt_msg) {
			rc = scsp_send_msg(dcsp, dcsp->sd_ca_rexmt_msg);
			if (rc == 0) {
				HARP_TIMER(&dcsp->sd_ca_rexmt_t,
						dcsp->sd_ca_rexmt_int,
						scsp_ca_retran_timeout);
			}
		}
		return(rc);
	}

	/*
	 * Free the last CA message
	 */
	if (dcsp->sd_ca_rexmt_msg) {
		scsp_free_msg(dcsp->sd_ca_rexmt_msg);
		dcsp->sd_ca_rexmt_msg = (Scsp_msg *)0;
	}

	/*
	 * Process the CA message
	 */
	scsp_process_ca(dcsp, msg->sc_ca);

	/*
	 * Increment the CA sequence number
	 */
	dcsp->sd_ca_seq++;

	/*
	 * Answer the CA message
	 */
	rc = scsp_send_ca(dcsp);
	if (rc)
		return(rc);

	/*
	 * If we're done sending CSAS records and the other side is,
	 * too, we're done with Summarize state
	 */
	if (!dcsp->sd_ca_csas && !msg->sc_ca->ca_o) {
		/*
		 * If the CRL is empty, we go directly to Aligned state;
		 * otherwise, we go to Update Cache and send a CSUS
		 */
		if (!dcsp->sd_crl) {
			/*
			 * Go to Aligned state
			 */
			dcsp->sd_ca_state = SCSP_CAFSM_ALIGNED;
			rc = scsp_cfsm(dcsp, SCSP_CIFSM_CA_ALIGN,
					(Scsp_msg *)0,
					(Scsp_if_msg *)0);
		} else {
			/*
			 * Go to Cache Update state
			 */
			dcsp->sd_ca_state = SCSP_CAFSM_UPDATE;
			HARP_CANCEL(&dcsp->sd_ca_rexmt_t);
			HARP_TIMER(&dcsp->sd_ca_rexmt_t,
					dcsp->sd_ca_rexmt_int,
					scsp_ca_retran_timeout);
			(void)scsp_cfsm(dcsp, SCSP_CIFSM_CA_UPD,
					(Scsp_msg *)0,
					(Scsp_if_msg *)0);
			rc = scsp_send_csus(dcsp);
		}
	}

	return(rc);
}


/*
 * CA finite state machine action 6
 * Retransmit timer expired -- retransmit last CA message
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	ignored
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_06(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	int	rc;

	/*
	 * Resend the CA message
	 */
	rc = scsp_send_msg(dcsp, dcsp->sd_ca_rexmt_msg);

	/*
	 * Restart the retransmit timer
	 */
	if (rc == 0) {
		HARP_TIMER(&dcsp->sd_ca_rexmt_t, dcsp->sd_ca_rexmt_int,
				scsp_ca_retran_timeout);
	}

	return(rc);
}


/*
 * CA finite state machine action 7
 * CSU Solicit received -- send it to the client interface FSM
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	pointer to received message
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_07(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	int		rc;
	Scsp_msg	*msg = (Scsp_msg *)p;

	/*
	 * Cancel the CA retransmit timer and free any CA message
	 * saved for retransmission
	 */
	if (dcsp->sd_ca_rexmt_msg) {
		HARP_CANCEL(&dcsp->sd_ca_rexmt_t);
		scsp_free_msg(dcsp->sd_ca_rexmt_msg);
		dcsp->sd_ca_rexmt_msg = (Scsp_msg *)0;
	}

	/*
	 * Pass the CSUS to the client interface FSM
	 */
	rc = scsp_cfsm(dcsp, SCSP_CIFSM_CSU_SOL, msg,
			(Scsp_if_msg *)0);

	return(rc);
}


/*
 * CA finite state machine action 8
 * CSU Request received -- pass it to the client interface FSM
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	pointer to received message
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_08(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	int		rc;
	Scsp_msg	*msg = (Scsp_msg *)p;
	Scsp_csa	*csap;

	/*
	 * Check whether this messages answers a CSUS
	 */
	scsp_csus_ack(dcsp, msg);

	/*
	 * If all CSAs requestd in CSUS messages have been
	 * received, the cache is aligned, so go to Aligned State
	 */
	if (!dcsp->sd_csus_rexmt_msg && !dcsp->sd_crl &&
			dcsp->sd_ca_state != SCSP_CAFSM_ALIGNED) {
		dcsp->sd_ca_state = SCSP_CAFSM_ALIGNED;
		rc = scsp_cfsm(dcsp, SCSP_CIFSM_CA_ALIGN,
				(Scsp_msg *)0, (Scsp_if_msg *)0);
	}

	/*
	 * Pass the CSU Req to the client interface FSM
	 */
	rc = scsp_cfsm(dcsp, SCSP_CIFSM_CSU_REQ, msg,
			(Scsp_if_msg *)0);

	/*
	 * Move the CSA chain from the message to the list of
	 * requests that need acknowledgements
	 */
	for (csap = msg->sc_csu_msg->csu_csa_rec; csap;
			csap = csap->next) {
		LINK2TAIL(csap, Scsp_csa, dcsp->sd_csu_ack_pend, next);
	}
	msg->sc_csu_msg->csu_csa_rec = (Scsp_csa *)0;

	return(rc);
}


/*
 * CA finite state machine action 9
 * CA Retransmit timer expired in Update Cache or Aligned state--free
 * the saved CA message
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	ignored
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_09(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	/*
	 * Free any CA message saved for retransmission
	 */
	if (dcsp->sd_ca_rexmt_msg) {
		scsp_free_msg(dcsp->sd_ca_rexmt_msg);
		dcsp->sd_ca_rexmt_msg = (Scsp_msg *)0;
	}

	return(0);
}


/*
 * CA finite state machine action 10
 * CSU Reply received -- Process the message
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	pointer to received message
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_10(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	int		rc = 0;
	Scsp_msg	*msg = (Scsp_msg *)p;
	Scsp_csu_rexmt	*rxp, *next_rxp;
	Scsp_csa	*csap, *next_csap, *mcp;

	/*
	 * Dequeue acknowledged CSAs.  For each CSAS in the received
	 * message, find the corresponding CSA on the CSU Request
	 * retransmit queue.  Remove the CSA from the queue;  if this
	 * results in the retransmit queue entry being empty, delete
	 * the entry.  If the DCS has a newer CSA, send a CSUS to
	 * request it.
	 *
	 * Caution--potentially confusing lack of indentation ahead.
	 */
	for (mcp = msg->sc_csu_msg->csu_csa_rec; mcp;
			mcp = mcp->next) {
	for (rxp = dcsp->sd_csu_rexmt; rxp; rxp = next_rxp) {
	next_rxp = rxp->sr_next;
	for (csap = rxp->sr_csa; csap; csap = next_csap) {
		next_csap = csap->next;
		if (scsp_cmp_key(&csap->key, &mcp->key) ||
				scsp_cmp_id(&csap->oid, &mcp->oid))
			continue;
		/*
		 * Found a CSA whose key and ID are equal to
		 * those in the CSU Reply
		 */
		if (csap->seq == mcp->seq) {
			/*
			 * The queued seq no is equal to the
			 * received seq no--the CSA is acknowledged
			 */
			UNLINK(csap, Scsp_csa, rxp->sr_csa, next);
			SCSP_FREE_CSA(csap);
		} else if (csap->seq < mcp->seq) {
			/*
			 * Queued seq no is less than received.
			 * We must dequeue the CSA and send a
			 * CSUS to request the more-up-to-date
			 * cache entry.
			 */
			UNLINK(mcp, Scsp_csa,
					msg->sc_csu_msg->csu_csa_rec,
					next);
			LINK2TAIL(mcp, Scsp_csa, dcsp->sd_crl, next);
			UNLINK(csap, Scsp_csa, rxp->sr_csa, next);
			SCSP_FREE_CSA(csap);
			if (!dcsp->sd_csus_rexmt_msg) {
				rc = scsp_send_csus(dcsp);
				if (rc) {
					return(rc);
				}
			}
		}
			/*
			 * Queued seq no is greater than
			 * received.  Ignore the received CSAS.
			 */

		/*
		 * If the retransmission block is empty, stop the
		 * timer and free it
		 */
		if (!rxp->sr_csa) {
			HARP_CANCEL(&rxp->sr_t);
			UNLINK(rxp, Scsp_csu_rexmt,
					dcsp->sd_csu_rexmt, sr_next);
			UM_FREE(rxp);
		}

		break;
	} /* for (csap = ... */
	} /* for (rxp = ... */
	} /* for (mcp = ... */

	return(rc);
}


/*
 * CA finite state machine action 11
 * Updated cache entry -- update the summary cache and send a
 * CSU Request
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	pointer to CSA describing new cache entry
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_11(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	int		rc, state;
	Scsp_csa	*csap =  (Scsp_csa *)p;
	Scsp_cse	*csep;

	/*
	 * Get the state of the CSA
	 */
	switch(dcsp->sd_server->ss_pid) {
	case SCSP_PROTO_ATMARP:
		state = csap->atmarp_data->sa_state;
		break;
	default:
		SCSP_FREE_CSA(csap);
		return(EINVAL);
	}

	if (state < SCSP_ASTATE_NEW || state > SCSP_ASTATE_DEL) {
		SCSP_FREE_CSA(csap);
		return(EINVAL);
	}

	/*
	 * Look up the cache summary entry for the CSA
	 */
	SCSP_LOOKUP(dcsp->sd_server, &csap->key, csep);

	/*
	 * Process ATMARP entries
	 */
	if (dcsp->sd_server->ss_pid == SCSP_PROTO_ATMARP) {
		switch(state) {
		case SCSP_ASTATE_NEW:
		case SCSP_ASTATE_UPD:
			/*
			 * Add the entry if we don't have it already
			 */
			if (!csep) {
				csep = (Scsp_cse *)UM_ALLOC(
						sizeof(Scsp_cse));
				if (!csep)
					scsp_mem_err("scsp_ca_act_11: sizeof(Scsp_cse)");
				UM_ZERO(csep, sizeof(Scsp_cse));

				csep->sc_key = csap->key;
				SCSP_ADD(dcsp->sd_server, csep);
			}

			/*
			 * Update the cache summary entry
			 */
			csep->sc_seq = csap->seq;
			csep->sc_oid = csap->oid;
			break;
		case SCSP_ASTATE_DEL:
			/*
			 * Delete any entry, but don't send the
			 * delete to the DCS
			 */
			if (csep) {
				SCSP_DELETE(dcsp->sd_server, csep);
				UM_FREE(csep);
			}

			SCSP_FREE_CSA(csap);
			return(0);
		}
	}

	/*
	 * Send the CSA in a CSU Request
	 */
	csap->trans_ct = 0;
	rc = scsp_send_csu_req(dcsp, csap);

	return(rc);
}


/*
 * CA finite state machine action 12
 * CSUS retransmit timer expired--send a CSUS with any pending CSA
 * records
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	ignored
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_12(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	int	rc;

	rc = scsp_send_csus(dcsp);

	return(rc);
}


/*
 * CA finite state machine action 13
 * CSU retransmit timer fired in Update or Aligned state--
 * retransmit CSU Req
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	pointer to retransmission block whose timer fired
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_13(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	int		rc = 0;
	Scsp_csu_rexmt	*rxp = (Scsp_csu_rexmt *)p;
	Scsp_csa	*csap, *csap1, *next_csap;

	/*
	 * Unlink and free the retransmit request block
	 */
	csap = rxp->sr_csa;
	UNLINK(rxp, Scsp_csu_rexmt, dcsp->sd_csu_rexmt, sr_next);
	UM_FREE(rxp);

	/*
	 * Increment the transmission count for the CSAs in the request
	 */
	for (csap1 = csap; csap1; csap1 = next_csap) {
		next_csap = csap1->next;
		csap1->trans_ct++;
		if (csap1->trans_ct >= dcsp->sd_csu_rexmt_max) {
			/*
			 * We've already sent this as many times as
			 * the limit allows.  Drop this CSA.
			 */
			UNLINK(csap1, Scsp_csa, csap, next);
			SCSP_FREE_CSA(csap1);
		}
	}

	/*
	 * Send another CSU Request with the CSA list, if it isn't
	 * empty now
	 */
	if (csap) {
		rc = scsp_send_csu_req(dcsp, csap);
	}

	return(rc);
}


/*
 * CA finite state machine action 14
 * Updated cache entry in Master/Slave Negotiation, Master, or
 * Slave state--add entry to cache and CSA list
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	pointer to new cache summary entry
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_14(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	Scsp_csa        *csap =  (Scsp_csa *)p;
	Scsp_cse	*csep, *csep1;

	/*
	 * Check to see whether we already have this
	 */
	SCSP_LOOKUP(dcsp->sd_server, &csap->key, csep);

	/*
	 * If we don't already have it and it's not being deleted,
	 * build a new cache summary entry
	 */
	if (!csep && !csap->null) {
		/*
		 * Get memory for a new entry
		 */
		csep = (Scsp_cse *)UM_ALLOC(sizeof(Scsp_cse));
		if (!csep) {
			scsp_mem_err("scsp_ca_act_14: sizeof(Scsp_cse)");
		}
		UM_ZERO(csep, sizeof(Scsp_cse));

		/*
		 * Fill out the new cache entry
		 */
		csep->sc_seq = csap->seq;
		csep->sc_key = csap->key;
		csep->sc_oid = csap->oid;

		/*
		 * Duplicate the new cache entry
		 */
		csep1 = scsp_dup_cse(csep);

		/*
		 * Add entry to the summary cache and the CSAS list
		 */
		SCSP_ADD(dcsp->sd_server, csep);
		LINK2TAIL(csep1, Scsp_cse, dcsp->sd_ca_csas, sc_next);
	} else {
		/*
		 * We already have the entry.  Find it on the CSAS
		 * list.
		 */
		for (csep1 = dcsp->sd_ca_csas; csep1;
				csep1 = csep1->sc_next) {
			if (scsp_cmp_key(&csep->sc_key,
					&csep1->sc_key) == 0)
				break;
		}

		/*
		 * Update or delete the entry
		 */
		if (csap->null) {
			/*
			 * The null flag is set--delete the entry
			 */
			SCSP_DELETE(dcsp->sd_server, csep);
			UM_FREE(csep);
			if (csep1) {
				UNLINK(csep1, Scsp_cse,
						dcsp->sd_ca_csas,
						sc_next);
				UM_FREE(csep1);
			}
		} else {
			/*
			 * Update the entry
			 */
			csep->sc_seq = csap->seq;
			csep->sc_oid = csap->oid;
			if (!csep1) {
				csep1 = scsp_dup_cse(csep);
				LINK2TAIL(csep1, Scsp_cse,
					dcsp->sd_ca_csas, sc_next);
			} else {
				csep1->sc_seq = csap->seq;
				csep1->sc_oid = csap->oid;
			}
		}
	}

	return(0);
}


/*
 * CA finite state machine action 15
 * CA message received in Update Cache state--if we have a saved CA
 * message, retransmit it;  otherwise, go to Master/Slave Negotiation
 * state
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	ignored
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_15(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	int		rc;
	Scsp_msg	*msg = (Scsp_msg *)p;

	/*
	 * If we don't have a saved CA message, or the sequence no. in
	 * the received message isn't right, fall back to Master/Slave
	 * Negotiation state
	 */
	if (!dcsp->sd_ca_rexmt_msg ||
			msg->sc_ca->ca_seq != dcsp->sd_ca_seq) {
		dcsp->sd_ca_state = SCSP_CAFSM_NEG;
		scsp_dcs_cleanup(dcsp);
		rc = scsp_ca_act_01(dcsp, (Scsp_msg *)0);
	} else {
		/*
		 * Retransmit the saved CA message and reset the
		 * CA timer
		 */
		rc = scsp_send_msg(dcsp, dcsp->sd_ca_rexmt_msg);
		if (rc == 0) {
			HARP_CANCEL(&dcsp->sd_ca_rexmt_t);
			HARP_TIMER(&dcsp->sd_ca_rexmt_t,
					dcsp->sd_ca_rexmt_int,
					scsp_ca_retran_timeout);
		}
	}

	return(rc);
}


/*
 * CA finite state machine action 16
 * Update Response received from client in Update Cache or Aligned
 * state.  Move the acknowledged CSA to the acknowledged queue.  If
 * the list of CSAs pending acknowledgement is empty, send a CSU
 * Reply.
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	pointer to message from client
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_16(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	int		found, rc = 0;
	Scsp_if_msg	*cmsg = (Scsp_if_msg *)p;
	Scsp_csa	*csap;

	/*
	 * Find the acknowledged CSA
	 */      
	for (csap = dcsp->sd_csu_ack_pend, found = 0; csap && !found;
			csap = csap->next) {
		switch (dcsp->sd_server->ss_pid) {
		case SCSP_PROTO_ATMARP:
			found = ((scsp_cmp_key(&csap->key,
				&cmsg->si_atmarp.sa_key) == 0) &&
				(scsp_cmp_id(&csap->oid,
				&cmsg->si_atmarp.sa_oid) == 0));
			break;
		default: 
			/*
			 * Protocol not implemented
			 */
			return(EPROTONOSUPPORT);
		}
		if (found)
			break;
	}

	if (!found) {
		if (scsp_trace_mode & SCSP_TRACE_CAFSM) {
			scsp_trace("scsp_ca_act_16: can't find CSA entry for Update Response\n");
		}
		return(0);
	}

	if (cmsg->si_rc == SCSP_RSP_OK) {
		/*
		 * The server accepted the cache entry
		 */

		/*
		 * Update SCSP's cache
		 */
		scsp_update_cache(dcsp, csap);

		/*
		 * Send this CSA to any other DCSs in the server group
		 */
		rc = scsp_propagate_csa(dcsp, csap);
	}

	/*
	 * Move the CSA from the ACK pending queue to the
	 * acknowledged queue
	 */
	UNLINK(csap, Scsp_csa, dcsp->sd_csu_ack_pend, next);
	LINK2TAIL(csap, Scsp_csa, dcsp->sd_csu_ack, next);
	if (!dcsp->sd_csu_ack_pend) {
		/*
		 * ACK pending list is empty--send a CSU Reply
		 */
		csap = dcsp->sd_csu_ack;
		dcsp->sd_csu_ack = (Scsp_csa *)0;
		rc = scsp_send_csu_reply(dcsp, csap);
	}

	return(rc);
}


/*
 * CA finite state machine action 17
 * Ignore an event.
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	ignored
 *
 * Returns:
 *	always returns 0
 *
 */
int
scsp_ca_act_17(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	return(0);
}


/*
 * CA finite state machine action 18
 * Updated cache entry in Down state--add entry to summary cache
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	pointer to new cache summary entry
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_18(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	Scsp_csa        *csap =  (Scsp_csa *)p;

	/*
	 * Update the cache as appropriate
	 */
	scsp_update_cache(dcsp, csap);

	return(0);
}


/*
 * CA finite state machine action 19
 * Update Response received from client in Master/Slave Negotiation
 * state.  Update the cache as appropriate.
 *
 * Arguments:
 *	dcsp	pointer to DCS control block
 *	p	pointer to message from client
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
scsp_ca_act_19(dcsp, p)
	Scsp_dcs	*dcsp;
	void		*p;
{
	Scsp_if_msg	*cmsg = (Scsp_if_msg *)p;
	Scsp_csa	*csap;

	/*
	 * Ignore the message if the client rejected the update
	 */
	if (cmsg->si_rc != SCSP_RSP_OK) {
		return(0);
	}

	/*
	 * Create a CSAS from the client's update
	 */      
	csap = (Scsp_csa *)UM_ALLOC(sizeof(Scsp_csa));
	if (!csap) {
		scsp_mem_err("scsp_ca_act_19: sizeof(Scsp_csa)");
	}
	UM_ZERO(csap, sizeof(Scsp_csa));

	csap->hops = 1;
	switch (dcsp->sd_server->ss_pid) {
	case SCSP_PROTO_ATMARP:
		csap->null = cmsg->si_atmarp.sa_state ==
				SCSP_ASTATE_DEL;
		csap->seq = cmsg->si_atmarp.sa_seq;
		csap->key = cmsg->si_atmarp.sa_key;
		csap->oid = cmsg->si_atmarp.sa_oid;
		break;
	default:
		return(EINVAL);
	}

	/*
	 * Update SCSP's cache
	 */
	scsp_update_cache(dcsp, csap);

	return(0);
}
