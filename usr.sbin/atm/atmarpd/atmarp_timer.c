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
 * SCSP-ATMARP server interface: timer routines
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

#include "../scspd/scsp_msg.h"
#include "../scspd/scsp_if.h"
#include "../scspd/scsp_var.h"
#include "atmarp_var.h"

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Cache update timeout processing
 *
 * When the cache update timer fires, we read the cache from the
 * kernel, update the internal cache, and restart the timer.
 *
 * Arguments:
 *	tp	pointer to a HARP timer block
 *
 * Returns:
 *	None
 *
 */
void
atmarp_cache_timeout(tp)
	Harp_timer	*tp;
{
	Atmarp_intf	*aip;

	/*
	 * Verify the status of all configured interfaces
	 */
	for (aip = atmarp_intf_head; aip; aip = aip->ai_next) {
		if (atmarp_if_ready(aip)) {
			/*
			 * The interface is up but we don't have
			 * a connection to SCSP--make a connection
			 */
			if (aip->ai_state == AI_STATE_NULL)
				(void)atmarp_scsp_connect(aip);
		} else {
			/*
			 * The interface is down--disconnect from SCSP
			 */
			if (aip->ai_state != AI_STATE_NULL)
				(void)atmarp_scsp_disconnect(aip);
		}
	}

	/*
	 * Read the cache from the kernel
	 */
	atmarp_get_updated_cache();

	/*
	 * Restart the cache update timer
	 */
	HARP_TIMER(tp, ATMARP_CACHE_INTERVAL, atmarp_cache_timeout);
}


/*
 * Permanent cache entry timer processing
 *
 * Permanent cache entries (entries that are administratively added
 * and the entry for the server itself) don't ever get refreshed, so
 * we broadcast updates for them every 10 minutes so they won't get
 * deleted from the remote servers' caches
 *
 * Arguments:
 *	tp	pointer to a HARP timer block
 *
 * Returns:
 *	None
 *
 */
void
atmarp_perm_timeout(tp)
	Harp_timer	*tp;
{
	int		i, rc;
	Atmarp_intf	*aip;
	Atmarp		*aap;

	/*
	 * Loop through all interfaces
	 */
	for (aip = atmarp_intf_head; aip; aip = aip->ai_next) {
		/*
		 * Loop through the interface's cache
		 */
		for (i = 0; i < ATMARP_HASHSIZ; i++) {
			for (aap = aip->ai_arptbl[i]; aap;
					aap = aap->aa_next) {
				/*
				 * Find and update permanent entries
				 */
				if ((aap->aa_flags & (AAF_PERM |
						AAF_SERVER)) != 0) {
					aap->aa_seq++;
					rc = atmarp_scsp_update(aap,
						SCSP_ASTATE_UPD);
				}
			}
		}
	}

	/*
	 * Restart the permanent cache entry timer
	 */
	HARP_TIMER(tp, ATMARP_PERM_INTERVAL, atmarp_perm_timeout);
}


/*
 * Keepalive timeout processing
 *
 * When the keepalive timer fires, we send a NOP to SCSP.  This
 * will help us detect a broken connection.
 *
 * Arguments:
 *	tp	pointer to a HARP timer block
 *
 * Returns:
 *	None
 *
 */
void
atmarp_keepalive_timeout(tp)
	Harp_timer	*tp;
{
	Atmarp_intf	*aip;
	Scsp_if_msg	*msg;

	/*
	 * Back off to start of DCS entry
	 */
	aip = (Atmarp_intf *) ((caddr_t)tp -
			(int)(&((Atmarp_intf *)0)->ai_keepalive_t));

	/*
	 * Get a message buffer
	 */
	msg = (Scsp_if_msg *)UM_ALLOC(sizeof(Scsp_if_msg));
	if (!msg) {
	}
	UM_ZERO(msg, sizeof(Scsp_if_msg));

	/*
	 * Build a NOP message
	 */
	msg->si_type = SCSP_NOP_REQ;
	msg->si_proto = SCSP_PROTO_ATMARP;
	msg->si_len = sizeof(Scsp_if_msg_hdr);

	/*
	 * Send the message to SCSP
	 */
	(void)atmarp_scsp_out(aip, (char *)msg, msg->si_len);
	UM_FREE(msg);

	/*
	 * Restart the keepalive timer
	 */
	HARP_TIMER(&aip->ai_keepalive_t, ATMARP_KEEPALIVE_INTERVAL,
			atmarp_keepalive_timeout);
}
