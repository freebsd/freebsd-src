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
 * ITU-T Q.2110 - Subroutines
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <machine/clock.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_cm.h>

#include <netatm/uni/sscop.h>
#include <netatm/uni/sscop_misc.h>
#include <netatm/uni/sscop_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Conditionally Clear Transmission Queues
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *
 * Returns:
 *	none
 *
 */
void
q2110_clear_xmit(sop)
	struct sscop	*sop;
{
	/*
	 * Only clear queues if 'Clear Buffers' == No
	 */
	if (sop->so_flags & SOF_NOCLRBUF)
		sscop_xmit_drain(sop);
}


/*
 * Initialize Data Transfer State Variables
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *
 * Returns:
 *	none
 *
 */
void
q2110_init_state(sop)
	struct sscop	*sop;
{
	/*
	 * Initialize for entry into Data Transfer Ready state
	 */
	sop->so_send = 0;
	sop->so_pollsend = 0;
	sop->so_ack = 0;
	sop->so_pollack = 1;
	sop->so_polldata = 0;
	sop->so_rcvhigh = 0;
	sop->so_rcvnext = 0;
}


/*
 * Prepare Queues for Data Retrieval
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *
 * Returns:
 *	none
 *
 */
void
q2110_prep_retrieve(sop)
	struct sscop	*sop;
{
	/*
	 * If 'Clear Buffers' == No, just clear retransmit queue,
	 * else clear all transmission queues
	 */
	if (sop->so_flags & SOF_NOCLRBUF) {
		sop->so_rexmit_hd = NULL;
		sop->so_rexmit_tl = NULL;
	} else
		sscop_xmit_drain(sop);

	/*
	 * Clear receiver queue
	 */
	sscop_rcvr_drain(sop);
}


/*
 * Prepare Queues for Error Recovery
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *
 * Returns:
 *	none
 *
 */
void
q2110_prep_recovery(sop)
	struct sscop	*sop;
{
	/*
	 * If 'Clear Buffers' == No, just clear retransmit queue,
	 * else clear all transmission queues
	 */
	if (sop->so_flags & SOF_NOCLRBUF) {
		sop->so_rexmit_hd = NULL;
		sop->so_rexmit_tl = NULL;
	} else
		sscop_xmit_drain(sop);
}


/*
 * Conditionally Deliver Received Data to User
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *
 * Returns:
 *	none
 *
 */
void
q2110_deliver_data(sop)
	struct sscop	*sop;
{
	/*
	 * If 'Clear Buffers' == No, give data to user
	 */
	if (sop->so_flags & SOF_NOCLRBUF) {
		/*
		 * We don't support 'Clear Buffers' == No, so don't bother
		 */
	}

	/*
	 * Clear receiver queue
	 */
	sscop_rcvr_drain(sop);
}


/*
 * Enter Connection Recovery Mode
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *
 * Returns:
 *	none
 *
 */
void
q2110_error_recovery(sop)
	struct sscop	*sop;
{

	/*
	 * Stop data transfer timers
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_timer[SSCOP_T_NORESP] = 0;
	sop->so_timer[SSCOP_T_IDLE] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/* 
	 * Initialize receiver window
	 */
	SEQ_SET(sop->so_rcvmax, sop->so_parm.sp_rcvwin);

	/*
	 * Send first ER PDU
	 */
	sop->so_connctl = 1;
	SEQ_INCR(sop->so_sendconn, 1);
	(void) sscop_send_er(sop);

	/*
	 * Set retransmit timer
	 */
	sop->so_timer[SSCOP_T_CC] = sop->so_parm.sp_timecc;

	/*
	 * Clear out appropriate queues
	 */
	q2110_prep_recovery(sop);

	/*
	 * Wait for ERAK
	 */
	sop->so_state = SOS_OUTRECOV;

	return;
}

