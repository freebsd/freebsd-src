/*
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
 */

/*
 * ATM Forum UNI Support
 * ---------------------
 *
 * ITU-T Q.SAAL1 - Subroutines
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_cm.h>

#include <netatm/uni/sscop.h>
#include <netatm/uni/sscop_misc.h>
#include <netatm/uni/sscop_var.h>

/*
 * Re-establish a new SSCOP Connection
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *
 * Returns:
 *	none
 *
 */
void
qsaal1_reestablish(sop)
	struct sscop	*sop;
{

	/*
	 * Stop polling timers
	 */
	sop->so_timer[SSCOP_T_POLL] = 0;
	sop->so_timer[SSCOP_T_NORESP] = 0;
	sop->so_flags &= ~SOF_KEEPALIVE;

	/*
	 * Note that we're reestablishing a connection
	 */
	sop->so_flags |= SOF_REESTAB;

	/*
	 * Send first BGN PDU
	 */
	sop->so_connctl = 1;
	(void) sscop_send_bgn(sop, SSCOP_SOURCE_SSCOP);

	/*
	 * Reset transmit variables
	 */
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
 * Reset connection's transmitter state
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *
 * Returns:
 *	none
 *
 */
void
qsaal1_reset_xmit(sop)
	struct sscop	*sop;
{

	/*
	 * Drain the transmission queues
	 */
	sscop_xmit_drain(sop);

	/*
	 * Reset transmit variables
	 */
	SEQ_SET(sop->so_send, 0);
	SEQ_SET(sop->so_pollsend, 0);
	SEQ_SET(sop->so_ack, 0);
	SEQ_SET(sop->so_pollack, 0);
	if (sop->so_state != SOS_INCONN)
		SEQ_SET(sop->so_sendmax, 0);
	sop->so_polldata = 0;

	return;
}


/*
 * Reset connection's receiver state
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *
 * Returns:
 *	none
 *
 */
void
qsaal1_reset_rcvr(sop)
	struct sscop	*sop;
{

	/*
	 * Drain the receiver queues
	 */
	sscop_rcvr_drain(sop);

	/*
	 * Reset transmit variables
	 */
	SEQ_SET(sop->so_rcvnext, 0);
	SEQ_SET(sop->so_rcvhigh, 0);
	SEQ_SET(sop->so_rcvmax, sop->so_parm.sp_rcvwin);

	return;
}


/*
 * Clear connection's connection data
 * 
 * Arguments:
 *	sop	pointer to sscop connection block
 *
 * Returns:
 *	none
 *
 */
void
qsaal1_clear_connection(sop)
	struct sscop	*sop;
{

	/*
	 * Can we clear transmit buffers ??
	 */
	if ((sop->so_flags & SOF_NOCLRBUF) == 0) {
		/*
		 * Yes, drain the transmission queues
		 */
		sscop_xmit_drain(sop);
	}

	/*
	 * Clear service required flag
	 */
	sop->so_flags &= ~SOF_XMITSRVC;

	/*
	 * Drain receive queue buffers
	 */
	sscop_rcvr_drain(sop);

	return;
}

