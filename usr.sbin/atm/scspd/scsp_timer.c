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
 *	@(#) $FreeBSD: src/usr.sbin/atm/scspd/scsp_timer.c,v 1.3 1999/08/28 01:15:34 peter Exp $
 *
 */

/*
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * Timer processing
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
__RCSID("@(#) $FreeBSD: src/usr.sbin/atm/scspd/scsp_timer.c,v 1.3 1999/08/28 01:15:34 peter Exp $");
#endif


/*
 * Process an SCSP Open timeout
 *
 * The open timer is set when an attempt to open a VCC to a DCS fails.
 * This routine will be called when the timer fires and will retry
 * the open.  Retries can continue indefinitely.
 *
 * Arguments:
 *	stp	pointer to an SCSP timer block
 *
 * Returns:
 *	None
 *
 */
void
scsp_open_timeout(stp)
	Harp_timer	*stp;
{
	Scsp_dcs	*dcsp;

	/*
	 * Back off to start of DCS entry
	 */
	dcsp = (Scsp_dcs *) ((caddr_t)stp -
			(int)(&((Scsp_dcs *)0)->sd_open_t));

	/*
	 * Retry the connection
	 */
	if (scsp_dcs_connect(dcsp)) {
		/*
		 * Connect failed -- we hope the error was temporary
		 * and set the timer to try again later
		 */
		HARP_TIMER(&dcsp->sd_open_t, SCSP_Open_Interval,
				scsp_open_timeout);
	}
}


/*
 * Process an SCSP Hello timeout
 *
 * The Hello timer fires every SCSP_HELLO_Interval seconds.  This
 * routine will notify the Hello FSM when the timer fires.
 *
 * Arguments:
 *	stp	pointer to an SCSP timer block
 *
 * Returns:
 *	None
 *
 */
void
scsp_hello_timeout(stp)
	Harp_timer	*stp;
{
	Scsp_dcs	*dcsp;

	/*
	 * Back off to start of DCS entry
	 */
	dcsp = (Scsp_dcs *) ((caddr_t)stp -
			(int)(&((Scsp_dcs *)0)->sd_hello_h_t));

	/*
	 * Call the Hello FSM
	 */
	(void)scsp_hfsm(dcsp, SCSP_HFSM_HELLO_T, (Scsp_msg *)0);

	return;
}


/*
 * Process an SCSP receive timeout
 *
 * The receive timer is started whenever the Hello FSM receives a
 * Hello message from its DCS.  If the timer fires, it means that no
 * Hello messages have been received in the DCS's Hello interval.
 *
 * Arguments:
 *	stp	pointer to an SCSP timer block
 *
 * Returns:
 *	None
 *
 */
void
scsp_hello_rcv_timeout(stp)
	Harp_timer	*stp;
{
	Scsp_dcs	*dcsp;

	/*
	 * Back off to start of DCS entry
	 */
	dcsp = (Scsp_dcs *) ((caddr_t)stp -
			(int)(&((Scsp_dcs *)0)->sd_hello_rcv_t));

	/*
	 * Call the Hello FSM
	 */
	(void)scsp_hfsm(dcsp, SCSP_HFSM_RCV_T, (void *)0);

	return;
}


/*
 * Process an SCSP CA retransmit timeout
 *
 * Arguments:
 *	stp	pointer to an SCSP timer block
 *
 * Returns:
 *	None
 *
 */
void
scsp_ca_retran_timeout(stp)
	Harp_timer	*stp;
{
	Scsp_dcs	*dcsp;

	/*
	 * Back off to start of DCS entry
	 */
	dcsp = (Scsp_dcs *) ((caddr_t)stp -
			(int)(&((Scsp_dcs *)0)->sd_ca_rexmt_t));

	/*
	 * Call the CA FSM
	 */
	(void)scsp_cafsm(dcsp, SCSP_CAFSM_CA_T, (void *)0);

	return;
}


/*
 * Process an SCSP CSUS retransmit timeout
 *
 * Arguments:
 *	stp	pointer to an SCSP timer block
 *
 * Returns:
 *	None
 *
 */
void
scsp_csus_retran_timeout(stp)
	Harp_timer	*stp;
{
	Scsp_dcs	*dcsp;

	/*
	 * Back off to start of DCS entry
	 */
	dcsp = (Scsp_dcs *) ((caddr_t)stp -
			(int)(&((Scsp_dcs *)0)->sd_csus_rexmt_t));

	/*
	 * Call the CA FSM
	 */
	(void)scsp_cafsm(dcsp, SCSP_CAFSM_CSUS_T, (void *)0);

	return;
}


/*
 * Process an SCSP CSU Req retransmit timeout
 *
 * Arguments:
 *	stp	pointer to an SCSP timer block
 *
 * Returns:
 *	None
 *
 */
void
scsp_csu_req_retran_timeout(stp)
	Harp_timer	*stp;
{
	Scsp_csu_rexmt	*rxp;
	Scsp_dcs	*dcsp;

	/*
	 * Back off to start of CSU Request retransmission entry
	 */
	rxp = (Scsp_csu_rexmt *) ((caddr_t)stp -
			(int)(&((Scsp_csu_rexmt *)0)->sr_t));
	dcsp = rxp->sr_dcs;

	/*
	 * Call the CA FSM
	 */
	(void)scsp_cafsm(dcsp, SCSP_CAFSM_CSU_T, (void *)rxp);

	return;
}
