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
 * UNI ATMARP support (RFC1577) - ARP cache processing
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_vc.h>
#include <netatm/atm_ioctl.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <netatm/ipatm/ipatm_var.h>
#include <netatm/ipatm/ipatm_serv.h>
#include <netatm/uni/unisig_var.h>
#include <netatm/uni/uniip_var.h>

/*
 * Add data to the arp table cache
 * 
 * Called at splnet.
 *
 * Arguments:
 *	uip	pointer to UNI IP interface
 *	ip	pointer to IP address structure
 *	atm	pointer to ATM address structure
 *	atmsub	pointer to ATM subaddress structure
 *	origin	source of arp information
 *
 * Returns:
 *	0	cache successfully updated
 *	else	updated failed - reason indicated
 *
 */
int
uniarp_cache_svc(uip, ip, atm, atmsub, origin)
	struct uniip		*uip;
	struct in_addr		*ip;
	Atm_addr		*atm;
	Atm_addr		*atmsub;
	u_int			origin;
{
	struct ip_nif		*inp;
	struct ipvcc		*ivp, *inext, *itail;
	struct uniarp		*nouap, *ipuap;
	char			abuf[64];

#ifdef DIAGNOSTIC
	strncpy(abuf, unisig_addr_print(atmsub), sizeof(abuf));
	abuf[sizeof(abuf) - 1] = 0;
	ATM_DEBUG4("cache_svc: ip=%s, atm=(%s,%s), origin=%d\n",
		inet_ntoa(*ip), unisig_addr_print(atm), abuf, origin);
#endif

	/*
	 * Get interface info
	 */
	inp = uip->uip_ipnif;

	/*
	 * Find both cached entry and 'nomap' entries for this data.
	 */
	UNIARP_LOOKUP(ip->s_addr, ipuap);
	for (nouap = uniarp_nomaptab; nouap; nouap = nouap->ua_next) {
		if (ATM_ADDR_EQUAL(atm, &nouap->ua_dstatm) &&
		    ATM_ADDR_EQUAL(atmsub, &nouap->ua_dstatmsub) &&
		    (nouap->ua_intf == uip))
			break;
	}

	/*
	 * If there aren't any entries yet, create one
	 * May be called from netisr - don't wait.
	 */
	if ((ipuap == NULL) && (nouap == NULL)) {
		ipuap = uma_zalloc(uniarp_zone, M_NOWAIT);
		if (ipuap == NULL)
			return (ENOMEM);
		ipuap->ua_dstip.s_addr = ip->s_addr;
		ipuap->ua_dstatm.address_format = T_ATM_ABSENT;
		ipuap->ua_dstatmsub.address_format = T_ATM_ABSENT;
		ipuap->ua_intf = uip;
		UNIARP_ADD(ipuap);
	}

	/*
	 * If there's no cached mapping, then make the 'nomap' entry
	 * the new cached entry.
	 */
	if (ipuap == NULL) {
		UNLINK(nouap, struct uniarp, uniarp_nomaptab, ua_next);
		nouap->ua_dstip.s_addr = ip->s_addr;
		ipuap = nouap;
		nouap = NULL;
		UNIARP_ADD(ipuap);
	}

	/*
	 * We need to check the consistency of the new data with any 
	 * cached data.  So taking the easy case first, if there isn't
	 * an ATM address in the cache then we can skip all these checks.
	 */
	if (ipuap->ua_dstatm.address_format != T_ATM_ABSENT) {
		/*
		 * See if the new data conflicts with what's in the cache
		 */
		if (ATM_ADDR_EQUAL(atm, &ipuap->ua_dstatm) &&
		    ATM_ADDR_EQUAL(atmsub, &ipuap->ua_dstatmsub) &&
		    (uip == ipuap->ua_intf)) {
			/*
			 * No conflicts here
			 */
			goto dataok;
		}

		/*
		 * Data conflict...how we deal with this depends on
		 * the origins of the conflicting data
		 */
		if (origin == ipuap->ua_origin) {
			/*
			 * The new data has equal precedence - if there are
			 * any VCCs using this entry, then we reject this
			 * "duplicate IP address" update.
			 */
			if (ipuap->ua_ivp != NULL) {
				strncpy(abuf, unisig_addr_print(atmsub),
					sizeof(abuf));
				abuf[sizeof(abuf) - 1] = 0;
				log(LOG_WARNING, 
					"uniarp: duplicate IP address %s from %s,%s\n",
					inet_ntoa(*ip), unisig_addr_print(atm),
					abuf);
				return (EACCES);
			}

		} else if (origin > ipuap->ua_origin) {
			/*
			 * New data's origin has higher precedence,
			 * so accept the new mapping and notify IP/ATM
			 * that a mapping change has occurred.  IP/ATM will
			 * close any VCC's which aren't waiting for this map.
			 */
			ipuap->ua_flags |= UAF_LOCKED;
			for (ivp = ipuap->ua_ivp; ivp; ivp = inext) {
				inext = ivp->iv_arpnext;
				(*inp->inf_arpnotify)(ivp, MAP_CHANGED);
			}
			ipuap->ua_flags &= ~UAF_LOCKED;
		} else {
			/*
			 * New data is of lesser origin precedence,
			 * so we just reject the update attempt.
			 */
			return (EACCES);
		}

		strncpy(abuf, unisig_addr_print(atmsub), sizeof(abuf));
		abuf[sizeof(abuf) - 1] = 0;
		log(LOG_WARNING, 
			"uniarp: ATM address for %s changed to %s,%s\n",
			inet_ntoa(*ip), unisig_addr_print(atm), abuf);
	}

	/*
	 * Update the cache entry with the new data
	 */
	ATM_ADDR_COPY(atm, &ipuap->ua_dstatm);
	ATM_ADDR_COPY(atmsub, &ipuap->ua_dstatmsub);
	ipuap->ua_intf = uip;

dataok:
	/*
	 * Update cache data origin
	 */
	ipuap->ua_origin = MAX(ipuap->ua_origin, origin);

	/*
	 * Ok, now act on this new/updated cache data
	 */
	ipuap->ua_flags |= UAF_LOCKED;

	/*
	 * Save pointer to last VCC currently on cached entry chain that
	 * will need to be notified of the map becoming valid
	 */
	itail = NULL;
	if ((ipuap->ua_flags & UAF_VALID) == 0) {

		for (itail = ipuap->ua_ivp; itail && itail->iv_arpnext; 
				itail = itail->iv_arpnext) {
		}
	}

	/*
	 * If there was a 'nomap' entry for this mapping, then we need to
	 * announce the new mapping to them first.
	 */
	if (nouap) {
		
		/*
		 * Move the VCCs from this entry to the cache entry and
		 * let them know there's a valid mapping now
		 */
		for (ivp = nouap->ua_ivp; ivp; ivp = inext) {
			inext = ivp->iv_arpnext;

			UNLINK(ivp, struct ipvcc, nouap->ua_ivp, iv_arpnext);

			LINK2TAIL(ivp, struct ipvcc, ipuap->ua_ivp, iv_arpnext);
			ivp->iv_arpent = (struct arpmap *)ipuap;

			(*inp->inf_arpnotify)(ivp, MAP_VALID);
		}

		/*
		 * Unlink and free the 'nomap' entry
		 */
		UNLINK(nouap, struct uniarp, uniarp_nomaptab, ua_next);
		UNIARP_CANCEL(nouap);
		uma_zfree(uniarp_zone, nouap);
	}

	/*
	 * Now, if this entry wasn't valid, notify the remaining VCCs
	 */
	if (itail) {

		for (ivp = ipuap->ua_ivp; ivp; ivp = inext) {
			inext = ivp->iv_arpnext;
			(*inp->inf_arpnotify)(ivp, MAP_VALID);
			if (ivp == itail)
				break;
		}
	}
	ipuap->ua_flags &= ~UAF_LOCKED;

	/*
	 * We now have a valid cache entry, so cancel any retry timer
	 * and reset the aging timeout
	 */
	UNIARP_CANCEL(ipuap);
	if ((ipuap->ua_origin == UAO_REGISTER) && (origin != UAO_REGISTER)) {
		if (((ipuap->ua_flags & UAF_VALID) == 0) ||
		    (ipuap->ua_aging <= 
				UNIARP_SERVER_AGE - UNIARP_MIN_REFRESH)) {
			ipuap->ua_flags |= UAF_REFRESH;
			ipuap->ua_aging = UNIARP_SERVER_AGE;
			ipuap->ua_retry = UNIARP_SERVER_RETRY;
		}
	} else {
		if (uip->uip_arpstate == UIAS_SERVER_ACTIVE) {
			ipuap->ua_aging = UNIARP_SERVER_AGE;
			ipuap->ua_retry = UNIARP_SERVER_RETRY;
		} else {
			ipuap->ua_aging = UNIARP_CLIENT_AGE;
			ipuap->ua_retry = UNIARP_CLIENT_RETRY;
		}
		ipuap->ua_flags |= UAF_REFRESH;
	}
	ipuap->ua_flags |= UAF_VALID;
	ipuap->ua_flags &= ~UAF_USED;
	return (0);
}


/*
 * Process ARP data from a PVC
 * 
 * The arp table cache is never updated with PVC information.
 * 
 * Called at splnet.
 *
 * Arguments:
 *	ivp	pointer to input PVC's IPVCC control block
 *	ip	pointer to IP address structure
 *	atm	pointer to ATM address structure
 *	atmsub	pointer to ATM subaddress structure
 *
 * Returns:
 *	none
 *
 */
void
uniarp_cache_pvc(ivp, ip, atm, atmsub)
	struct ipvcc		*ivp;
	struct in_addr		*ip;
	Atm_addr		*atm;
	Atm_addr		*atmsub;
{
	struct ip_nif		*inp;
	struct uniarp		*uap;

#ifdef DIAGNOSTIC
	char	buf[64];
	int	vpi = 0, vci = 0;

	if ((ivp->iv_conn) && (ivp->iv_conn->co_connvc)) {
		vpi = ivp->iv_conn->co_connvc->cvc_vcc->vc_vpi;
		vci = ivp->iv_conn->co_connvc->cvc_vcc->vc_vci;
	}
	strncpy(buf, unisig_addr_print(atmsub), sizeof(buf));
	buf[sizeof(buf) - 1] = 0;
	ATM_DEBUG5("cache_pvc: vcc=(%d,%d), ip=%s, atm=(%s,%s)\n",
		vpi, vci, inet_ntoa(*ip), unisig_addr_print(atm), buf);
#endif

	/*
	 * Get PVC info
	 */
	inp = ivp->iv_ipnif;
	uap = (struct uniarp *)ivp->iv_arpent;

	/*
	 * See if IP address for PVC has changed
	 */
	if (uap->ua_dstip.s_addr != ip->s_addr) {
		if (uap->ua_dstip.s_addr != 0)
			(*inp->inf_arpnotify)(ivp, MAP_CHANGED);
		uap->ua_dstip.s_addr = ip->s_addr;
	}

	/*
	 * Let IP/ATM know if address has become valid
	 */
	if ((uap->ua_flags & UAF_VALID) == 0)
		(*inp->inf_arpnotify)(ivp, MAP_VALID);
	uap->ua_flags |= UAF_VALID;
	uap->ua_aging = UNIARP_CLIENT_AGE;
	uap->ua_retry = UNIARP_CLIENT_RETRY;

	/*
	 * Save ATM addresses just for debugging
	 */
	ATM_ADDR_COPY(atm, &uap->ua_dstatm);
	ATM_ADDR_COPY(atmsub, &uap->ua_dstatmsub);

	return;
}


/*
 * Validate IP address
 * 
 * Arguments:
 *	uip	pointer to UNI IP interface
 *	ip	pointer to IP address structure
 *	origin	source of arp information
 *
 * Returns:
 *	0	IP address is acceptable
 *	else	invalid IP address
 *
 */
int
uniarp_validate_ip(uip, ip, origin)
	struct uniip		*uip;
	struct in_addr		*ip;
	u_int			origin;
{
	struct uniarp_prf	*upp;
	u_int i;


	/*
	 * Can't be multicast or broadcast address
	 */
	if (IN_MULTICAST(ntohl(ip->s_addr)) ||
	    in_broadcast(*ip, &uip->uip_ipnif->inf_nif->nif_if))
		return (1);

	/*
	 * For ATMARP registration information (including SCSP data),
	 * the address must be allowed by the interface's prefix list.
	 */
	if ((origin == UAO_REGISTER) || (origin == UAO_SCSP)) {
		for (i = uip->uip_nprefix, upp = uip->uip_prefix;
		     i; i--, upp++) {
			if ((ip->s_addr & upp->upf_mask.s_addr) == 
					upp->upf_addr.s_addr)
				break;
		}
		if (i == 0)
			return (1);
	}

	return (0);
}

