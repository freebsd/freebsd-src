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
 * ATM Forum UNI Support
 * ---------------------
 *
 * SSCOP Common - Process CPCS-signals (SSCOP PDUs)
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <netatm/uni/sscop.h>
#include <netatm/uni/sscop_misc.h>
#include <netatm/uni/sscop_pdu.h>
#include <netatm/uni/sscop_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * No-op Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_noop(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	/*
	 * Just free PDU
	 */
	KB_FREEALL(m);

	return;
}


/*
 * BGN PDU / SOS_IDLE Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_bgn_idle(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct bgn_pdu	*bp = (struct bgn_pdu *)trlr;
	int		err, source;

	if (sop->so_vers == SSCOP_VERS_Q2110) {
		/*
		 * "Power-up Robustness" option
		 *
		 * Accept BGN regardless of BGN.N(SQ)
		 */
		sop->so_rcvconn = bp->bgn_nsq;

	} else {
		/*
		 * If retransmitted BGN, reject it
		 */
		if (sscop_is_rexmit(sop, bp->bgn_nsq)) {
			KB_FREEALL(m);
			(void) sscop_send_bgrej(sop);
			return;
		}
	}

	if (sop->so_vers == SSCOP_VERS_QSAAL) {
		/*
		 * Get Source value
		 */
		if (bp->bgn_type & PT_SOURCE_SSCOP)
			source = SSCOP_SOURCE_SSCOP;
		else
			source = SSCOP_SOURCE_USER;

		/*
		 * Reset receiver state variables
		 */
		qsaal1_reset_rcvr(sop);
	} else
		source = 0;

	/*
	 * Set initial transmit window
	 */
	SEQ_SET(sop->so_sendmax, ntohl(bp->bgn_nmr));

	/*
	 * Pass connection request up to user
	 */
	STACK_CALL(SSCOP_ESTABLISH_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, (int)m, source, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Wait for user's response
	 */
	sop->so_state = SOS_INCONN;

	return;
}


/*
 * BGN PDU / SOS_OUTDISC Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_bgn_outdisc(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct bgn_pdu	*bp = (struct bgn_pdu *)trlr;
	int		err, source;

	/*
	 * If retransmitted BGN, ACK it and send new END
	 */
	if (sscop_is_rexmit(sop, bp->bgn_nsq)) {
		KB_FREEALL(m);
		(void) sscop_send_bgak(sop);
		(void) sscop_send_end(sop, SSCOP_SOURCE_LAST);
		return;
	}

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Initialize transmit window
	 */
	SEQ_SET(sop->so_sendmax, ntohl(bp->bgn_nmr));

	/*
	 * Notify user of connection termination
	 */
	STACK_CALL(SSCOP_RELEASE_CNF, sop->so_upper, sop->so_toku,
		sop->so_connvc, 0, 0, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	if (sop->so_vers == SSCOP_VERS_QSAAL) {
		/*
		 * Get Source value
		 */
		if (bp->bgn_type & PT_SOURCE_SSCOP)
			source = SSCOP_SOURCE_SSCOP;
		else
			source = SSCOP_SOURCE_USER;

		/*
		 * Reset receiver variables
		 */
		qsaal1_reset_rcvr(sop);
	
	} else
		source = 0;

	/*
	 * Tell user about incoming connection
	 */
	STACK_CALL(SSCOP_ESTABLISH_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, (int)m, source, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Wait for user's response
	 */
	sop->so_state = SOS_INCONN;

	return;
}


/*
 * BGN PDU / SOS_OUTRESYN Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_bgn_outresyn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct bgn_pdu	*bp = (struct bgn_pdu *)trlr;
	int		err, source;

	/*
	 * If retransmitted BGN, ACK it and send new RS
	 */
	if (sscop_is_rexmit(sop, bp->bgn_nsq)) {
		KB_FREEALL(m);
		(void) sscop_send_bgak(sop);
		(void) sscop_send_rs(sop);
		return;
	}

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Initialize transmit window
	 */
	SEQ_SET(sop->so_sendmax, ntohl(bp->bgn_nmr));

	if (sop->so_vers == SSCOP_VERS_QSAAL) {
		/*
		 * Get (possible) Source value
		 */
		if (bp->bgn_type & PT_SOURCE_SSCOP)
			source = SSCOP_SOURCE_SSCOP;
		else
			source = SSCOP_SOURCE_USER;

		/*
		 * Reset receiver variables
		 */
		qsaal1_reset_rcvr(sop);
	
	} else
		source = SSCOP_SOURCE_USER;

	/*
	 * Notify user of connection termination
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, SSCOP_UU_NULL, source, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Now tell user of a "new" incoming connection
	 */
	STACK_CALL(SSCOP_ESTABLISH_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, (int)m, source, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Wait for user's response
	 */
	sop->so_state = SOS_INCONN;

	return;
}


/*
 * BGN PDU / SOS_INRESYN Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_bgn_inresyn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct bgn_pdu	*bp = (struct bgn_pdu *)trlr;
	int		err, source;

	/*
	 * If retransmitted BGN, oops
	 */
	if (sscop_is_rexmit(sop, bp->bgn_nsq)) {
		KB_FREEALL(m);
		sscop_maa_error(sop, 'B');
		return;
	}

	/*
	 * Stop data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_timer[SSCOP_T_NORESP] = 0;
	sop->so_timer[SSCOP_T_IDLE] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/*
	 * Initialize transmit window
	 */
	SEQ_SET(sop->so_sendmax, ntohl(bp->bgn_nmr));

	if (sop->so_vers == SSCOP_VERS_QSAAL) {
		/*
		 * Get (possible) Source value
		 */
		if (bp->bgn_type & PT_SOURCE_SSCOP)
			source = SSCOP_SOURCE_SSCOP;
		else
			source = SSCOP_SOURCE_USER;

		/*
		 * Reset receiver variables
		 */
		qsaal1_reset_rcvr(sop);

	} else {
		/*
		 * Stop possible retransmit timer
		 */
		sop->so_timer[SSCOP_T_CC] = 0;

		/*
		 * Drain receiver queues
		 */
		sscop_rcvr_drain(sop);

		/*
		 * Tell user current connection has been released
		 */
		STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
			sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_USER, err);
		if (err) {
			KB_FREEALL(m);
			sscop_abort(sop, "stack memory\n");
			return;
		}

		source = 0;
	}

	/*
	 * Tell user of incoming connection
	 */
	STACK_CALL(SSCOP_ESTABLISH_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, (int)m, source, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Wait for user's response
	 */
	sop->so_state = SOS_INCONN;

	return;
}


/*
 * BGAK PDU / Protocol Error
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_bgak_error(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Record error condition
	 */
	sscop_maa_error(sop, 'C');
	KB_FREEALL(m);

	return;
}


/*
 * BGAK PDU / SOS_IDLE Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_bgak_idle(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Record error condition
	 */
	sscop_bgak_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);
	return;
}


/*
 * BGAK PDU / SOS_OUTCONN Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_bgak_outconn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct bgak_pdu	*bp = (struct bgak_pdu *)trlr;
	int		err;

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Initialize transmit window
	 */
	SEQ_SET(sop->so_sendmax, ntohl(bp->bgak_nmr));

	/*
	 * Notify user of connection establishment
	 */
	if (sop->so_flags & SOF_REESTAB) {
		KB_FREEALL(m);
		STACK_CALL(SSCOP_ESTABLISH_IND, sop->so_upper, sop->so_toku,
			sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
		if (err) {
			sscop_abort(sop, "stack memory\n");
			return;
		}
		sop->so_flags &= ~SOF_REESTAB;
	} else {
		STACK_CALL(SSCOP_ESTABLISH_CNF, sop->so_upper, sop->so_toku,
			sop->so_connvc, (int)m, 0, err);
		if (err) {
			KB_FREEALL(m);
			sscop_abort(sop, "stack memory\n");
			return;
		}
	}

	if (sop->so_vers == SSCOP_VERS_QSAAL) {
		/*
		 * Reset receiver variables
		 */
		qsaal1_reset_rcvr(sop);
	
		/*
		 * Start polling timer
		 */
		sscop_set_poll(sop);

		/*
		 * Start lost poll/stat timer
		 */
		sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;

	} else {
		/*
		 * Initialize state variables
		 */
		q2110_init_state(sop);

		/*
		 * Start data transfer timers
		 */
		sop->so_timer[SSCOP_T_POLL] = sop->so_parm.sp_timepoll;
		sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;
	}

	/*
	 * OK, we're ready for data
	 */
	sop->so_state = SOS_READY;

	/*
	 * See if transmit queues need servicing
	 */
	if (sop->so_flags & SOF_XMITSRVC)
		sscop_service_xmit(sop);

	return;
}


/*
 * BGREJ PDU / Protocol Error
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_bgrej_error(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Record error condition
	 */
	sscop_maa_error(sop, 'D');
	KB_FREEALL(m);
	return;
}


/*
 * BGREJ PDU / SOS_OUTCONN Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_bgrej_outconn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		source, uu, err;

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	if (sop->so_vers == SSCOP_VERS_QSAAL) {
		/*
		 * Clear reestablishment flag
		 */
		sop->so_flags &= ~SOF_REESTAB;

		KB_FREEALL(m);
		m = NULL;
		uu = SSCOP_UU_NULL;
		source = SSCOP_SOURCE_SSCOP;
	} else {
		uu = (int)m;
		source = SSCOP_SOURCE_USER;
	}

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, uu, source, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * BGREJ PDU / SOS_INCONN Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_bgrej_inconn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Report protocol error
	 */
	sscop_bgrej_error(sop, m, trlr);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * BGREJ PDU / SOS_OUTRESYN Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_bgrej_outresyn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Report protocol error
	 */
	sscop_bgrej_error(sop, m, trlr);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	if (sop->so_vers == SSCOP_VERS_QSAAL) {
		/*
		 * Clear connection data
		 */
		qsaal1_clear_connection(sop);
	}

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * BGREJ PDU / SOS_READY Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_bgrej_ready(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Stop data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_timer[SSCOP_T_NORESP] = 0;
	sop->so_timer[SSCOP_T_IDLE] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/*
	 * Report protocol error
	 */
	sscop_bgrej_error(sop, m, trlr);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	if (sop->so_vers == SSCOP_VERS_QSAAL) {
		/*
		 * Clear connection data
		 */
		qsaal1_clear_connection(sop);
	} else {
		/*
		 * Clear out appropriate queues
		 */
		q2110_prep_retrieve(sop);
	}

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * END PDU / SOS_IDLE Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_end_idle(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Free buffers
	 */
	KB_FREEALL(m);

	/*
	 * Return an ENDAK to peer
	 */
	(void) sscop_send_endak(sop);

	return;
}


/*
 * END PDU / SOS_INCONN Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_end_inconn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct end_pdu	*ep = (struct end_pdu *)trlr;
	int		err, source;

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Acknowledge END
	 */
	(void) sscop_send_endak(sop);

	/*
	 * Get Source value
	 */
	if (ep->end_type & PT_SOURCE_SSCOP)
		source = SSCOP_SOURCE_SSCOP;
	else
		source = SSCOP_SOURCE_USER;

	/*
	 * Notify user of connection termination
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, (int)m, source, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * END PDU / SOS_OUTDISC Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_end_outdisc(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Release buffers
	 */
	KB_FREEALL(m);

	/*
	 * Acknowledge END
	 */
	(void) sscop_send_endak(sop);

	/*
	 * Notify user of connection termination
	 */
	STACK_CALL(SSCOP_RELEASE_CNF, sop->so_upper, sop->so_toku,
		sop->so_connvc, 0, 0, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * ENDAK PDU / Protocol Error
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_endak_error(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Record error condition
	 */
	sscop_maa_error(sop, 'F');
	KB_FREEALL(m);

	return;
}


/*
 * ENDAK PDU / SOS_INCONN Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_endak_inconn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Report protocol error
	 */
	sscop_endak_error(sop, m, trlr);

	/*
	 * Notify user of connection termination
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * ENDAK PDU / SOS_OUTDISC Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_endak_outdisc(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Release buffers
	 */
	KB_FREEALL(m);

	/*
	 * Notify user of connection termination
	 */
	STACK_CALL(SSCOP_RELEASE_CNF, sop->so_upper, sop->so_toku,
		sop->so_connvc, 0, 0, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * ENDAK PDU / SOS_READY Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_endak_ready(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Stop data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_timer[SSCOP_T_NORESP] = 0;
	sop->so_timer[SSCOP_T_IDLE] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/*
	 * Report protocol error
	 */
	sscop_endak_error(sop, m, trlr);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	if (sop->so_vers == SSCOP_VERS_QSAAL) {
		/*
		 * Clear connection data
		 */
		qsaal1_clear_connection(sop);
	} else {
		/*
		 * Clear out appropriate queues
		 */
		q2110_prep_retrieve(sop);
	}

	/*
	 * Back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * RS PDU / Protocol Error
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_rs_error(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Record error condition
	 */
	sscop_maa_error(sop, 'J');
	KB_FREEALL(m);

	return;
}


/*
 * RS PDU / SOS_IDLE Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_rs_idle(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Report error condition
	 */
	sscop_rs_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

	return;
}


/*
 * RSAK PDU / Protocol Error
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_rsak_error(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Record error condition
	 */
	sscop_maa_error(sop, 'K');
	KB_FREEALL(m);

	return;
}


/*
 * RSAK PDU / SOS_IDLE Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_rsak_idle(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Report error condition
	 */
	sscop_rsak_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);
	return;
}


/*
 * RSAK PDU / SOS_OUTRESYN Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_rsak_outresyn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct rsak_q2110_pdu	*rp = (struct rsak_q2110_pdu *)trlr;
	int		err;

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Notify user of resynchronization completion
	 */
	STACK_CALL(SSCOP_RESYNC_CNF, sop->so_upper, sop->so_toku,
		sop->so_connvc, 0, 0, err);
	if (err) {
		KB_FREEALL(m);
		sscop_abort(sop, "stack memory\n");
		return;
	}

	if (sop->so_vers == SSCOP_VERS_QSAAL) {
		/*
		 * Start the polling timer
		 */
		sscop_set_poll(sop);

		/*
		 * Start lost poll/stat timer
		 */
		sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;
	} else {
		/*
		 * Initialize state variables
		 */
		SEQ_SET(sop->so_sendmax, ntohl(rp->rsak_nmr));
		q2110_init_state(sop);

		/*
		 * Start data transfer timers
		 */     
		sop->so_timer[SSCOP_T_POLL] = sop->so_parm.sp_timepoll;
		sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;
	}

	/*
	 * Free buffers
	 */
	KB_FREEALL(m);

	/*
	 * Now go back to data transfer state
	 */
	sop->so_state = SOS_READY;

	/*
	 * See if transmit queues need servicing
	 */
	if (sop->so_flags & SOF_XMITSRVC)
		sscop_service_xmit(sop);

	return;
}


/*
 * SD PDU / Protocol Error
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_sd_error(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Record error condition
	 */
	sscop_maa_error(sop, 'A');
	KB_FREEALL(m);

	return;
}


/*
 * SD PDU / SOS_IDLE Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_sd_idle(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Record error condition
	 */
	sscop_sd_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);
	return;
}


/*
 * SD PDU / SOS_INCONN Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_sd_inconn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Record error condition
	 */
	sscop_sd_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Go back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * POLL PDU / Protocol Error
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_poll_error(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Record error condition
	 */
	sscop_maa_error(sop, 'G');
	KB_FREEALL(m);

	return;
}


/*
 * POLL PDU / SOS_IDLE Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_poll_idle(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Report error condition
	 */
	sscop_poll_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);
	return;
}


/*
 * POLL PDU / SOS_INCONN Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_poll_inconn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Record error condition
	 */
	sscop_poll_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Go back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * STAT PDU / Protocol Error
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_stat_error(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Record error condition
	 */
	sscop_maa_error(sop, 'H');
	KB_FREEALL(m);

	return;
}


/*
 * STAT PDU / SOS_IDLE Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_stat_idle(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Report error condition
	 */
	sscop_stat_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);
	return;
}


/*
 * STAT PDU / SOS_INCONN Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_stat_inconn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Record error condition
	 */
	sscop_stat_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Go back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * STAT PDU / SOS_READY Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_stat_ready(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct stat_pdu	*sp = (struct stat_pdu *)trlr;
	struct pdu_hdr	*php;
	KBuffer		*m0 = m;
	sscop_seq	seq1, seq2, opa;
	int		cnt = 0;

	NTOHL(sp->stat_nps);
	NTOHL(sp->stat_nmr);
	NTOHL(sp->stat_nr);

	/*
	 * Validate peer's received poll sequence number
	 */
	if (SEQ_GT(sop->so_pollack, sp->stat_nps, sop->so_pollack) ||
	    SEQ_GT(sp->stat_nps, sop->so_pollsend, sop->so_pollack)) {
		/*
		 * Bad poll sequence number
		 */
		sscop_maa_error(sop, 'R');
		goto goterr;
	}

	/*
	 * Validate peer's current receive data sequence number
	 */
	if (SEQ_GT(sop->so_ack, sp->stat_nr, sop->so_ack) ||
	    SEQ_GT(sp->stat_nr, sop->so_send, sop->so_ack)) {
		/*
		 * Bad data sequence number
		 */
		sscop_maa_error(sop, 'S');
		goto goterr;
	}

	/*
	 * Free acknowledged PDUs
	 */
	for (seq1 = sop->so_ack, SEQ_SET(seq2, sp->stat_nr);
			SEQ_LT(seq1, seq2, sop->so_ack);
			SEQ_INCR(seq1, 1)) {
		sscop_pack_free(sop, seq1);
	}

	/*
	 * Update transmit state variables
	 */
	opa = sop->so_pollack;
	sop->so_ack = seq2;
	SEQ_SET(sop->so_pollack, sp->stat_nps);
	SEQ_SET(sop->so_sendmax, sp->stat_nmr);

	/*
	 * Get first element in STAT list
	 */
	while (m && (KB_LEN(m) == 0))
		m = KB_NEXT(m);
	if (m == NULL)
		goto done;
	m = sscop_stat_getelem(m, &seq1);

	/*
	 * Make sure there's a second element too
	 */
	if (m == NULL)
		goto done;

	/*
	 * Validate first element (start of missing pdus)
	 */
	if (SEQ_GT(sop->so_ack, seq1, sop->so_ack) ||
	    SEQ_GEQ(seq1, sop->so_send, sop->so_ack)) {
		/*
		 * Bad element sequence number
		 */
		sscop_maa_error(sop, 'S');
		goto goterr;
	}

	/*
	 * Loop thru all STAT elements in list
	 */
	while (m) {
		/*
		 * Get next even element (start of received pdus)
		 */
		m = sscop_stat_getelem(m, &seq2);

		/*
		 * Validate seqence number
		 */
		if (SEQ_GEQ(seq1, seq2, sop->so_ack) ||
		    SEQ_GT(seq2, sop->so_send, sop->so_ack)) {
			/*
			 * Bad element sequence number
			 */
			sscop_maa_error(sop, 'S');
			goto goterr;
		}

		/*
		 * Process each missing sequence number in this gap
		 */
		while (SEQ_LT(seq1, seq2, sop->so_ack)) {
			/*
			 * Find corresponding SD PDU on pending ack queue
			 */
			php = sscop_pack_locate(sop, seq1);
			if (php == NULL) {
				sscop_maa_error(sop, 'S');
				goto goterr;
			}

			/*
			 * Retransmit this SD PDU only if it was last sent
			 * during an earlier poll sequence and it's not
			 * already scheduled for retranmission.
			 */
			if (SEQ_LT(php->ph_nps, sp->stat_nps, opa) &&
			    (php->ph_rexmit_lk == NULL) &&
			    (sop->so_rexmit_tl != php)) {
				/*
				 * Put PDU on retransmit queue and schedule
				 * transmit servicing
				 */
				sscop_rexmit_insert(sop, php);
				sop->so_flags |= SOF_XMITSRVC;
				cnt++;
			}

			/*
			 * Bump to next sequence number
			 */
			SEQ_INCR(seq1, 1);
		}

		/*
		 * Now process series of acknowledged PDUs
		 *
		 * Get next odd element (start of missing pdus),
		 * but make sure there is one and that it's valid
		 */
		if (m == NULL)
			goto done;
		m = sscop_stat_getelem(m, &seq2);
		if (SEQ_GEQ(seq1, seq2, sop->so_ack) ||
		    SEQ_GT(seq2, sop->so_send, sop->so_ack)) {
			/*
			 * Bad element sequence number
			 */
			sscop_maa_error(sop, 'S');
			goto goterr;
		}

		/*
		 * Process each acked sequence number
		 */
		while (SEQ_LT(seq1, seq2, sop->so_ack)) {
			/*
			 * Can we clear transmit buffers ??
			 */
			if ((sop->so_flags & SOF_NOCLRBUF) == 0) {
				/*
				 * Yes, free acked buffers
				 */
				sscop_pack_free(sop, seq1);
			}

			/*
			 * Bump to next sequence number
			 */
			SEQ_INCR(seq1, 1);
		}
	}

done:
	/*
	 * Free PDU buffer chain
	 */
	KB_FREEALL(m0);

	/*
	 * Report retransmitted PDUs
	 */
	if (cnt)
		sscop_maa_error(sop, 'V');

	/*
	 * Record transmit window closed transitions
	 */
	if (SEQ_LT(sop->so_send, sop->so_sendmax, sop->so_ack)) {
		if (sop->so_flags & SOF_NOCREDIT) {
			sop->so_flags &= ~SOF_NOCREDIT;
			sscop_maa_error(sop, 'X');
		}
	} else {
		if ((sop->so_flags & SOF_NOCREDIT) == 0) {
			sop->so_flags |= SOF_NOCREDIT;
			sscop_maa_error(sop, 'W');
		}
	}

	if (sop->so_vers == SSCOP_VERS_QSAAL)
		/*
		 * Restart lost poll/stat timer
		 */
		sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;
	else {
		/*
		 * Determine new polling phase
		 */
		if ((sop->so_timer[SSCOP_T_POLL] != 0) &&
		    ((sop->so_flags & SOF_KEEPALIVE) == 0)) {
			/*
			 * Remain in active phase - reset NO-RESPONSE timer
			 */
			sop->so_timer[SSCOP_T_NORESP] =
					sop->so_parm.sp_timeresp;

		} else if (sop->so_timer[SSCOP_T_IDLE] == 0) {
			/*
			 * Go from transient to idle phase
			 */
			sop->so_timer[SSCOP_T_POLL] = 0;
			sop->so_flags &= ~SOF_KEEPALIVE;
			sop->so_timer[SSCOP_T_NORESP] = 0;
			sop->so_timer[SSCOP_T_IDLE] = sop->so_parm.sp_timeidle;
		}
	}

	/*
	 * See if transmit queues need servicing
	 */
	if (sop->so_flags & SOF_XMITSRVC)
		sscop_service_xmit(sop);

	return;

goterr:
	/*
	 * Protocol/parameter error encountered
	 */

	/*
	 * Free PDU buffer chain
	 */
	KB_FREEALL(m0);

	if (sop->so_vers == SSCOP_VERS_QSAAL)
		/*
		 * Reestablish a new connection
		 */
		qsaal1_reestablish(sop);
	else
		/*
		 * Initiate error recovery
		 */
		q2110_error_recovery(sop);

	return;
}


/*
 * USTAT PDU / Protocol Error
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_ustat_error(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Record error condition
	 */
	sscop_maa_error(sop, 'I');
	KB_FREEALL(m);

	return;
}


/*
 * USTAT PDU / SOS_IDLE Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_ustat_idle(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * Report error condition
	 */
	sscop_ustat_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);
	return;
}


/*
 * USTAT PDU / SOS_INCONN Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_ustat_inconn(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Record error condition
	 */
	sscop_ustat_error(sop, m, trlr);

	/*
	 * Return an END to peer
	 */
	(void) sscop_send_end(sop, SSCOP_SOURCE_SSCOP);

	/*
	 * Notify user of connection failure
	 */
	STACK_CALL(SSCOP_RELEASE_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, SSCOP_UU_NULL, SSCOP_SOURCE_SSCOP, err);
	if (err) {
		sscop_abort(sop, "stack memory\n");
		return;
	}

	/*
	 * Go back to idle state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * USTAT PDU / SOS_READY Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_ustat_ready(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	struct ustat_pdu	*up = (struct ustat_pdu *)trlr;
	struct pdu_hdr	*php;
	sscop_seq	seq1, seq2;

	NTOHL(up->ustat_nmr);
	NTOHL(up->ustat_nr);

	/*
	 * Validate peer's current receive data sequence number
	 */
	if (SEQ_GT(sop->so_ack, up->ustat_nr, sop->so_ack) ||
	    SEQ_GEQ(up->ustat_nr, sop->so_send, sop->so_ack)) {
		/*
		 * Bad data sequence number
		 */
		goto goterr;
	}

	/*
	 * Free acknowledged PDUs
	 */
	for (seq1 = sop->so_ack, SEQ_SET(seq2, up->ustat_nr);
			SEQ_LT(seq1, seq2, sop->so_ack);
			SEQ_INCR(seq1, 1)) {
		sscop_pack_free(sop, seq1);
	}

	/*
	 * Update transmit state variables
	 */
	sop->so_ack = seq2;
	SEQ_SET(sop->so_sendmax, up->ustat_nmr);

	/*
	 * Get USTAT list elements
	 */
	SEQ_SET(seq1, ntohl(up->ustat_le1));
	SEQ_SET(seq2, ntohl(up->ustat_le2));

	/*
	 * Validate elements
	 */
	if (SEQ_GT(sop->so_ack, seq1, sop->so_ack) ||
	    SEQ_GEQ(seq1, seq2, sop->so_ack) ||
	    SEQ_GEQ(seq2, sop->so_send, sop->so_ack)) {
		/*
		 * Bad element sequence number
		 */
		goto goterr;
	}

	/*
	 * Process each missing sequence number in this gap
	 */
	while (SEQ_LT(seq1, seq2, sop->so_ack)) {
		/*
		 * Find corresponding SD PDU on pending ack queue
		 */
		php = sscop_pack_locate(sop, seq1);
		if (php == NULL) {
			goto goterr;
		}

		/*
		 * Retransmit this SD PDU if it's not
		 * already scheduled for retranmission.
		 */
		if ((php->ph_rexmit_lk == NULL) &&
		    (sop->so_rexmit_tl != php)) {
			/*
			 * Put PDU on retransmit queue and schedule
			 * transmit servicing
			 */
			sscop_rexmit_insert(sop, php);
			sop->so_flags |= SOF_XMITSRVC;
		}

		/*
		 * Bump to next sequence number
		 */
		SEQ_INCR(seq1, 1);
	}

	/*
	 * Report retransmitted PDUs
	 */
	sscop_maa_error(sop, 'V');

	/*
	 * Free PDU buffer chain
	 */
	KB_FREEALL(m);

	/*
	 * See if transmit queues need servicing
	 */
	if (sop->so_flags & SOF_XMITSRVC)
		sscop_service_xmit(sop);

	return;

goterr:
	/*
	 * Protocol/parameter error encountered
	 */
	sscop_maa_error(sop, 'T');

	/*
	 * Free PDU buffer chain
	 */
	KB_FREEALL(m);

	if (sop->so_vers == SSCOP_VERS_QSAAL)
		/*
		 * Reestablish a new connection
		 */
		qsaal1_reestablish(sop);
	else
		/*
		 * Initiate error recovery
		 */
		q2110_error_recovery(sop);

	return;
}


/*
 * UD PDU / SOS_* Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_ud_all(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{
	int		err;

	/*
	 * Pass data up to user
	 */
	STACK_CALL(SSCOP_UNITDATA_IND, sop->so_upper, sop->so_toku,
		sop->so_connvc, (int)m, 0, err);
	if (err)
		KB_FREEALL(m);
	return;
}


/*
 * MD PDU / SOS_* Processor
 *
 * Arguments:
 *	sop	pointer to sscop connection block
 *	m	pointer to PDU buffer (without trailer)
 *	trlr	pointer to PDU trailer
 *
 * Returns:
 *	none
 *
 */
void
sscop_md_all(sop, m, trlr)
	struct sscop	*sop;
	KBuffer		*m;
	caddr_t		trlr;
{

	/*
	 * We don't support MD PDUs
	 */
	KB_FREEALL(m);
	return;
}

