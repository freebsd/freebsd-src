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
 *	@(#) $FreeBSD: src/sys/netatm/uni/sscop_sigaa.c,v 1.4 2000/01/17 20:49:52 mks Exp $
 *
 */

/*
 * ATM Forum UNI Support
 * ---------------------
 *
 * SSCOP Common - Process AA-signals (SAP_SSCOP)
 *
 */

#include <netatm/kern_include.h>

#include <netatm/uni/sscop.h>
#include <netatm/uni/sscop_misc.h>
#include <netatm/uni/sscop_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD: src/sys/netatm/uni/sscop_sigaa.c,v 1.4 2000/01/17 20:49:52 mks Exp $");
#endif


/*
 * SSCOP_ESTABLISH_REQ / SOS_IDLE Command Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	pointer to buffer containing SSCOP-UU data
 *	arg2	buffer release parameter
 *
 * Returns:
 *	none
 *
 */
void
sscop_estreq_idle(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{

	/*
	 * We don't support SSCOP-UU data
	 */
	if (arg1 != SSCOP_UU_NULL)
		KB_FREEALL((KBuffer *)arg1);

	/*
	 * We currently only support BR=YES
	 */
	if (arg2 != SSCOP_BR_YES) {
		sscop_abort(sop, "sscop: BR != YES\n");
		return;
	}

	/*
	 * Initialize receiver window
	 */
	SEQ_SET(sop->so_rcvmax, sop->so_parm.sp_rcvwin);

	/*
	 * Send first BGN PDU
	 */
	sop->so_connctl = 1;
	SEQ_INCR(sop->so_sendconn, 1);
	(void) sscop_send_bgn(sop, SSCOP_SOURCE_USER);

	/*
	 * Reset transmitter state
	 */
	if (sop->so_vers == SSCOP_VERS_Q2110)
		q2110_clear_xmit(sop);
	else
		qsaal1_reset_xmit(sop);

	/*
	 * Set retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = sop->so_parm.sp_timecc;

	/*
	 * Wait for BGAK
	 */
	sop->so_state = SOS_OUTCONN;

	return;
}


/*
 * SSCOP_ESTABLISH_RSP / SOS_INCONN Command Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	pointer to buffer containing SSCOP-UU data
 *	arg2	buffer release parameter
 *
 * Returns:
 *	none
 *
 */
void
sscop_estrsp_inconn(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{

	/*
	 * We don't support SSCOP-UU data
	 */
	if (arg1 != SSCOP_UU_NULL)
		KB_FREEALL((KBuffer *)arg1);

	/*
	 * We currently only support BR=YES
	 */
	if (arg2 != SSCOP_BR_YES) {
		sscop_abort(sop, "sscop: BR != YES\n");
		return;
	}

	if (sop->so_vers == SSCOP_VERS_Q2110) {
		/*
		 * Clear transmitter buffers
		 */
		q2110_clear_xmit(sop);

		/*
		 * Initialize state variables
		 */
		SEQ_SET(sop->so_rcvmax, sop->so_parm.sp_rcvwin);
		q2110_init_state(sop);
	} else {
		/*
		 * Reset transmitter state
		 */
		qsaal1_reset_xmit(sop);
	}

	/*
	 * Send BGAK PDU
	 */
	(void) sscop_send_bgak(sop);

	/*
	 * Start polling timer
	 */
	sop->so_timer[SSCOP_T_POLL] = sop->so_parm.sp_timepoll;

	/*
	 * Start lost poll/stat timer
	 */
	sop->so_timer[SSCOP_T_NORESP] = sop->so_parm.sp_timeresp;

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
 * SSCOP_RELEASE_REQ / SOS_OUTCONN Command Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	pointer to buffer containing SSCOP-UU data
 *	arg2	unused
 *
 * Returns:
 *	none
 *
 */
void
sscop_relreq_outconn(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{

	/*
	 * We don't support SSCOP-UU data
	 */
	if (arg1 != SSCOP_UU_NULL)
		KB_FREEALL((KBuffer *)arg1);

	/*
	 * Stop retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = 0;

	/*
	 * Clear reestablishment flag
	 */
	sop->so_flags &= ~SOF_REESTAB;

	/*
	 * Send first END PDU
	 */
	sop->so_connctl = 1;
	(void) sscop_send_end(sop, SSCOP_SOURCE_USER);

	if (sop->so_vers == SSCOP_VERS_QSAAL)
		/*
		 * Clear connection data
		 */
		qsaal1_clear_connection(sop);

	/*
	 * Set retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = sop->so_parm.sp_timecc;

	/*
	 * Wait for ENDAK
	 */
	sop->so_state = SOS_OUTDISC;

	return;
}


/*
 * SSCOP_RELEASE_REQ / SOS_INCONN Command Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	pointer to buffer containing SSCOP-UU data
 *	arg2	unused
 *
 * Returns:
 *	none
 *
 */
void
sscop_relreq_inconn(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{

	/*
	 * We don't support SSCOP-UU data
	 */
	if (arg1 != SSCOP_UU_NULL)
		KB_FREEALL((KBuffer *)arg1);

	/*
	 * Return a BGREJ PDU
	 */
	(void) sscop_send_bgrej(sop);

	/*
	 * Back to IDLE state
	 */
	sop->so_state = SOS_IDLE;

	return;
}


/*
 * SSCOP_RELEASE_REQ / SOS_READY Command Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	pointer to buffer containing SSCOP-UU data
 *	arg2	unused
 *
 * Returns:
 *	none
 *
 */
void
sscop_relreq_ready(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{

	/*
	 * We don't support SSCOP-UU data
	 */
	if (arg1 != SSCOP_UU_NULL)
		KB_FREEALL((KBuffer *)arg1);

	/*
	 * Stop data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_timer[SSCOP_T_NORESP] = 0;
	sop->so_timer[SSCOP_T_IDLE] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/*
	 * Send first END PDU
	 */
	sop->so_connctl = 1;
	(void) sscop_send_end(sop, SSCOP_SOURCE_USER);

	if (sop->so_vers == SSCOP_VERS_Q2110) {
		/*
		 * Clear out appropriate queues
		 */
		if (sop->so_state == SOS_READY)
			q2110_prep_retrieve(sop);
		else
			sscop_rcvr_drain(sop);
	} else {
		/*
		 * Clear connection data
		 */
		qsaal1_clear_connection(sop);
	}

	/*
	 * Set retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = sop->so_parm.sp_timecc;

	/*
	 * Wait for ENDAK
	 */
	sop->so_state = SOS_OUTDISC;

	return;
}


/*
 * SSCOP_DATA_REQ / SOS_READY Command Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	pointer to buffer containing assured user data
 *	arg2	unused
 *
 * Returns:
 *	none
 *
 */
void
sscop_datreq_ready(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{
	KBuffer		*m = (KBuffer *)arg1;

	/*
	 * We must have a buffer (even if it contains no data)
	 */
	if (m == NULL) {
		sscop_abort(sop, "sscop_datreq_ready: no buffer\n");
		return;
	}

	/*
	 * Place data at end of transmission queue
	 */
	KB_QNEXT(m) = NULL;
	if (sop->so_xmit_hd == NULL)
		sop->so_xmit_hd = m;
	else
		KB_QNEXT(sop->so_xmit_tl) = m;
	sop->so_xmit_tl = m;

	/*
	 * Service the transmit queues
	 */
	sscop_service_xmit(sop);

	return;
}


/*
 * SSCOP_UNITDATA_REQ / SOS_* Command Processor
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *	arg1	pointer to buffer containing unassured user data
 *	arg2	unused
 *
 * Returns:
 *	none
 *
 */
void
sscop_udtreq_all(sop, arg1, arg2)
	struct sscop	*sop;
	int		arg1;
	int		arg2;
{
	KBuffer		*m = (KBuffer *)arg1;

	/*
	 * We must have a buffer (even if it contains no data)
	 */
	if (m == NULL) {
		sscop_abort(sop, "sscop_udtreq_all: no buffer\n");
		return;
	}

	/*
	 * Send the data in a UD PDU
	 */
	(void) sscop_send_ud(sop, m);
	
	return;
}

