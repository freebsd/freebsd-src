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
 * UNI ATMARP support (RFC1577) - Timer processing
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_ioctl.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <netatm/ipatm/ipatm_var.h>
#include <netatm/ipatm/ipatm_serv.h>
#include <netatm/uni/uniip_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Local functions
 */
static void	uniarp_svc_oldage __P((struct uniarp *));
static void	uniarp_pvc_oldage __P((struct uniarp *));


/*
 * Process a UNI ATMARP entry timeout
 * 
 * Called when a previously scheduled uniarp control block timer expires.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tip	pointer to uniarp timer control block
 *
 * Returns:
 *	none
 *
 */
void
uniarp_timeout(tip)
	struct atm_time	*tip;
{
	struct uniip	*uip;
	struct uniarp	*uap;
	struct ipvcc	*ivp;


	/*
	 * Back-off to uniarp control block
	 */
	uap = (struct uniarp *)
			((caddr_t)tip - (int)(&((struct uniarp *)0)->ua_time));
	uip = uap->ua_intf;


	/*
	 * Do we know the IP address for this entry yet??
	 */
	if (uap->ua_dstip.s_addr == 0) {

		/*
		 * No, then send another InATMARP_REQ on each active VCC
		 * associated with this entry to solicit the peer's identity.
		 */
		for (ivp = uap->ua_ivp; ivp; ivp = ivp->iv_arpnext) {
			if (ivp->iv_state != IPVCC_ACTIVE)
				continue;
			(void) uniarp_inarp_req(uip, &uap->ua_dstatm, 
				&uap->ua_dstatmsub, ivp);
		}

		/*
		 * Restart retry timer
		 */
		UNIARP_TIMER(uap, UNIARP_ARP_RETRY);
	} else {
		/*
		 * Yes, then we're trying to find the ATM address for this
		 * IP address - so send another ATMARP_REQ to the arpserver
		 * (if it's up at the moment)
		 */
		if (uip->uip_arpstate == UIAS_CLIENT_ACTIVE)
			(void) uniarp_arp_req(uip, &uap->ua_dstip);

		/*
		 * Restart retry timer
		 */
		UNIARP_TIMER(uap, UNIARP_ARP_RETRY);
	}

	return;
}


/*
 * Process an UNI ARP SVC entry aging timer expiration
 * 
 * This function is called when an SVC arp entry's aging timer has expired.
 *
 * Called at splnet().
 *
 * Arguments:
 *	uap	pointer to atmarp table entry
 *
 * Returns:
 *	none
 *
 */
static void
uniarp_svc_oldage(uap)
	struct uniarp	*uap;
{
	struct ipvcc	*ivp, *inext;
	struct uniip	*uip = uap->ua_intf;


	/*
	 * Permanent (manually installed) entries are never aged
	 */
	if (uap->ua_origin >= UAO_PERM)
		return;

	/*
	 * If entry is valid and we're out of retrys, tell
	 * IP/ATM that the SVCs can't be used
	 */
	if ((uap->ua_flags & UAF_VALID) && (uap->ua_retry-- == 0)) {
		uap->ua_flags |= UAF_LOCKED;
		for (ivp = uap->ua_ivp; ivp; ivp = inext) {
			inext = ivp->iv_arpnext;
			(*ivp->iv_ipnif->inf_arpnotify)(ivp, MAP_INVALID);
		}
		uap->ua_flags &= ~(UAF_LOCKED | UAF_VALID);
		uap->ua_origin = 0;

		/*
		 * Delete and free an unused entry
		 */
		if (uap->ua_ivp == NULL) {
			UNIARP_CANCEL(uap);
			UNIARP_DELETE(uap);
			atm_free((caddr_t)uap);
			return;
		}
	}

	/*
	 * We want to try and refresh this entry but we don't want
	 * to keep unused entries laying around forever.
	 */
	if (uap->ua_ivp || (uap->ua_flags & UAF_USED)) {
		if (uip->uip_arpstate == UIAS_CLIENT_ACTIVE) {
			/*
			 * If we are a client (and the server VCC is active),
			 * then we'll ask the server for a refresh
			 */ 
			(void) uniarp_arp_req(uip, &uap->ua_dstip);
		} else {
			/*
			 * Otherwise, solicit the each active VCC peer with 
			 * an Inverse ATMARP
			 */
			for (ivp = uap->ua_ivp; ivp; ivp = ivp->iv_arpnext) {
				if (ivp->iv_state != IPVCC_ACTIVE)
					continue;
				(void) uniarp_inarp_req(uip, &uap->ua_dstatm,
					&uap->ua_dstatmsub, ivp);
			}
		}
	}

	/*
	 * Reset timeout
	 */
	if (uap->ua_flags & UAF_VALID)
		uap->ua_aging = UNIARP_RETRY_AGE;
	else
		uap->ua_aging = UNIARP_REVALID_AGE;

	return;
}


/*
 * Process an UNI ARP PVC entry aging timer expiration
 * 
 * This function is called when a PVC arp entry's aging timer has expired.
 *
 * Called at splnet().
 *
 * Arguments:
 *	uap	pointer to atmarp table entry
 *
 * Returns:
 *	none
 *
 */
static void
uniarp_pvc_oldage(uap)
	struct uniarp	*uap;
{
	struct ipvcc	*ivp = uap->ua_ivp;

	/*
	 * If entry is valid and we're out of retrys, tell
	 * IP/ATM that PVC can't be used
	 */
	if ((uap->ua_flags & UAF_VALID) && (uap->ua_retry-- == 0)) {
		(*ivp->iv_ipnif->inf_arpnotify)(ivp, MAP_INVALID);
		uap->ua_flags &= ~UAF_VALID;
	}

	/*
	 * Solicit peer with Inverse ATMARP
	 */
	(void) uniarp_inarp_req(uap->ua_intf, &uap->ua_dstatm,
			&uap->ua_dstatmsub, ivp);

	/*
	 * Reset timeout
	 */
	if (uap->ua_flags & UAF_VALID)
		uap->ua_aging = UNIARP_RETRY_AGE;
	else
		uap->ua_aging = UNIARP_REVALID_AGE;

	return;
}


/*
 * Process a UNI ARP aging timer tick
 * 
 * This function is called every UNIARP_AGING seconds, in order to age
 * all the arp table entries.  If an entry's timer is expired, then the
 * uniarp old-age timeout function will be called for that entry.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tip	pointer to uniarp aging timer control block
 *
 * Returns:
 *	none
 *
 */
void
uniarp_aging(tip)
	struct atm_time	*tip;
{
	struct uniarp	*uap, *unext;
	int		i;


	/*
	 * Schedule next timeout
	 */
	atm_timeout(&uniarp_timer, UNIARP_AGING, uniarp_aging);

	/*
	 * Run through arp table bumping each entry's aging timer.
	 * If an expired timer is found, process that entry.
	 */
	for (i = 0; i < UNIARP_HASHSIZ; i++) {
		for (uap = uniarp_arptab[i]; uap; uap = unext) {
			unext = uap->ua_next;

			if (uap->ua_aging && --uap->ua_aging == 0)
				uniarp_svc_oldage(uap);
		}
	}

	/*
	 * Check out PVC aging timers too
	 */
	for (uap = uniarp_pvctab; uap; uap = unext) {
		unext = uap->ua_next;

		if (uap->ua_aging && --uap->ua_aging == 0)
			uniarp_pvc_oldage(uap);
	}

	/*
	 * Only fully resolved SVC entries need aging, so there's no need
	 * to examine the 'no map' table
	 */
}

