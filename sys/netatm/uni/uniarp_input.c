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
 * UNI ATMARP support (RFC1577) - Input packet processing
 *
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
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
#include <netatm/uni/uniip_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Local functions
 */
static void	proc_arp_req __P((struct ipvcc *, KBuffer *));
static void	proc_arp_rsp __P((struct ipvcc *, KBuffer *));
static void	proc_arp_nak __P((struct ipvcc *, KBuffer *));
static void	proc_inarp_req __P((struct ipvcc *, KBuffer *));
static void	proc_inarp_rsp __P((struct ipvcc *, KBuffer *));


/*
 * Local variables
 */
static Atm_addr	satm;
static Atm_addr	satmsub;
static Atm_addr	tatm;
static Atm_addr	tatmsub;
static struct in_addr	sip;
static struct in_addr	tip;


/*
 * Process ATMARP Input Data
 * 
 * Arguments:
 *	tok	uniarp connection token (pointer to ipvcc)
 *	m	pointer to input packet buffer chain
 *
 * Returns:
 *	none
 *
 */
void
uniarp_cpcs_data(tok, m)
	void		*tok;
	KBuffer		*m;
{
	struct ipvcc	*ivp = tok;
	struct atmarp_hdr	*ahp;
	KBuffer		*n;
	int		len, plen = sizeof(struct atmarp_hdr);

#ifdef DIAGNOSTIC
	if (uniarp_print)
		uniarp_pdu_print(ivp, m, "receive");
#endif

	/*
	 * Verify IP's VCC state
	 */
	if (ivp->iv_state != IPVCC_ACTIVE) {
		goto bad;
	}

	/*
	 * Get the fixed fields together
	 */
	if (KB_LEN(m) < sizeof(struct atmarp_hdr)) {
		KB_PULLUP(m, sizeof(struct atmarp_hdr), m);
		if (m == NULL)
			goto bad;
	}

	KB_DATASTART(m, ahp, struct atmarp_hdr *);

	/*
	 * Initial packet verification
	 */
	if ((ahp->ah_hrd != htons(ARP_ATMFORUM)) ||
	    (ahp->ah_pro != htons(ETHERTYPE_IP)))
		goto bad;

	/*
	 * Verify/gather source address fields
	 */
	if ((len = (ahp->ah_shtl & ARP_TL_LMASK)) != 0) {
		if (ahp->ah_shtl & ARP_TL_E164) {
			if (len > sizeof(struct atm_addr_e164))
				goto bad;
			satm.address_format = T_ATM_E164_ADDR;
		} else {
			if (len != sizeof(struct atm_addr_nsap))
				goto bad;
			satm.address_format = T_ATM_ENDSYS_ADDR;
		}
		satm.address_length = len;
		if (KB_COPYDATA(m, plen, len, (caddr_t)satm.address))
			goto bad;
		plen += len;
	} else {
		satm.address_format = T_ATM_ABSENT;
		satm.address_length = 0;
	}

	if ((len = (ahp->ah_sstl & ARP_TL_LMASK)) != 0) {
		if (((ahp->ah_sstl & ARP_TL_TMASK) != ARP_TL_NSAPA) ||
		    (len != sizeof(struct atm_addr_nsap)))
			goto bad;
		satmsub.address_format = T_ATM_ENDSYS_ADDR;
		satmsub.address_length = len;
		if (KB_COPYDATA(m, plen, len, (caddr_t)satmsub.address))
			goto bad;
		plen += len;
	} else {
		satmsub.address_format = T_ATM_ABSENT;
		satmsub.address_length = 0;
	}

	if ((len = ahp->ah_spln) != 0) {
		if (len != sizeof(struct in_addr))
			goto bad;
		if (KB_COPYDATA(m, plen, len, (caddr_t)&sip))
			goto bad;
		plen += len;
	} else {
		sip.s_addr = 0;
	}

	/*
	 * Verify/gather target address fields
	 */
	if ((len = (ahp->ah_thtl & ARP_TL_LMASK)) != 0) {
		if (ahp->ah_thtl & ARP_TL_E164) {
			if (len > sizeof(struct atm_addr_e164))
				goto bad;
			tatm.address_format = T_ATM_E164_ADDR;
		} else {
			if (len != sizeof(struct atm_addr_nsap))
				goto bad;
			tatm.address_format = T_ATM_ENDSYS_ADDR;
		}
		tatm.address_length = len;
		if (KB_COPYDATA(m, plen, len, (caddr_t)tatm.address))
			goto bad;
		plen += len;
	} else {
		tatm.address_format = T_ATM_ABSENT;
		tatm.address_length = 0;
	}

	if ((len = (ahp->ah_tstl & ARP_TL_LMASK)) != 0) {
		if (((ahp->ah_tstl & ARP_TL_TMASK) != ARP_TL_NSAPA) ||
		    (len != sizeof(struct atm_addr_nsap)))
			goto bad;
		tatmsub.address_format = T_ATM_ENDSYS_ADDR;
		tatmsub.address_length = len;
		if (KB_COPYDATA(m, plen, len, (caddr_t)tatmsub.address))
			goto bad;
		plen += len;
	} else {
		tatmsub.address_format = T_ATM_ABSENT;
		tatmsub.address_length = 0;
	}

	if ((len = ahp->ah_tpln) != 0) {
		if (len != sizeof(struct in_addr))
			goto bad;
		if (KB_COPYDATA(m, plen, len, (caddr_t)&tip))
			goto bad;
		plen += len;
	} else {
		tip.s_addr = 0;
	}

	/*
	 * Verify packet length
	 */
	for (len = 0, n = m; n; n = KB_NEXT(n))
		len += KB_LEN(n);
	if (len != plen)
		goto bad;

	/*
	 * Now finish with packet-specific processing
	 */
	switch (ntohs(ahp->ah_op)) {
	case ARP_REQUEST:
		proc_arp_req(ivp, m);
		break;

	case ARP_REPLY:
		proc_arp_rsp(ivp, m);
		break;

	case INARP_REQUEST:
		proc_inarp_req(ivp, m);
		break;

	case INARP_REPLY:
		proc_inarp_rsp(ivp, m);
		break;

	case ARP_NAK:
		proc_arp_nak(ivp, m);
		break;

	default:
		goto bad;
	}

	return;

bad:
	uniarp_stat.uas_rcvdrop++;
	if (m)
		KB_FREEALL(m);
}


/*
 * Process an ATMARP request packet
 * 
 * Arguments:
 *	ivp	pointer to input VCC's IPVCC control block
 *	m	pointer to input packet buffer chain
 *
 * Returns:
 *	none
 *
 */
static void
proc_arp_req(ivp, m)
	struct ipvcc	*ivp;
	KBuffer		*m;
{
	struct ip_nif	*inp;
	struct atm_nif	*nip;
	struct siginst	*sgp;
	struct uniip	*uip;
	struct uniarp	*uap;
	struct in_addr	myip;
	int		s = splnet();

	/*
	 * Only an arp server should receive these
	 */
	inp = ivp->iv_ipnif;
	nip = inp->inf_nif;
	uip = (struct uniip *)inp->inf_isintf;
	if ((uip == NULL) ||
	    (uip->uip_arpstate != UIAS_SERVER_ACTIVE))
		goto drop;

	/*
	 * These should be sent only on SVCs
	 */
	if ((ivp->iv_flags & IVF_SVC) == 0)
		goto drop;

	/*
	 * Locate our addresses
	 */
	sgp = nip->nif_pif->pif_siginst;
	myip.s_addr = IA_SIN(inp->inf_addr)->sin_addr.s_addr;

	/*
	 * Target IP address must be present
	 */
	if (tip.s_addr == 0)
		goto drop;

	/*
	 * Drop packet if both Source addresses aren't present
	 */
	if ((sip.s_addr == 0) || (satm.address_format == T_ATM_ABSENT))
		goto drop;

	/*
	 * Source addresses can't be ours
	 */
	if (ATM_ADDR_SEL_EQUAL(&sgp->si_addr, nip->nif_sel, &satm) &&
	    ATM_ADDR_SEL_EQUAL(&sgp->si_subaddr, nip->nif_sel, &satmsub)) {
		struct vccb	*vcp = ivp->iv_conn->co_connvc->cvc_vcc;

		log(LOG_WARNING,
			"uniarp: vcc=(%d,%d) reports our ATM address\n",
			vcp->vc_vpi, vcp->vc_vci);
		goto drop;
	}
	if (sip.s_addr == myip.s_addr) {
		struct vccb	*vcp = ivp->iv_conn->co_connvc->cvc_vcc;

		log(LOG_WARNING,
			"uniarp: vcc=(%d,%d) reports our IP address\n",
			vcp->vc_vpi, vcp->vc_vci);
		goto drop;
	}

	/*
	 * Validate Source IP address
	 */
	if (uniarp_validate_ip(uip, &sip, UAO_REGISTER) != 0)
		goto drop;

	/*
	 * If the source and target IP addresses are the same, then this
	 * must be a client registration request (RFC-2225).  Otherwise, 
	 * try to accomodate old clients (per RFC-2225 8.4.4).
	 */
	if (sip.s_addr == tip.s_addr)
		(void) uniarp_cache_svc(uip, &sip, &satm, &satmsub,
				UAO_REGISTER);
	else {
		uap = (struct uniarp *)ivp->iv_arpent;
		if ((uap == NULL) || (uap->ua_origin < UAO_REGISTER))
			(void) uniarp_cache_svc(uip, &sip, &satm, &satmsub,
					UAO_REGISTER);
	}

	/*
	 * Lookup the target IP address in the cache (and also check if
	 * the query is for our address).
	 */
	UNIARP_LOOKUP(tip.s_addr, uap);
	if (uap && (uap->ua_flags & UAF_VALID)) {
		/*
		 * We've found a valid mapping
		 */
		(void) uniarp_arp_rsp(uip, &uap->ua_arpmap, &sip, &satm,
					&satmsub, ivp);

	} else if (tip.s_addr == myip.s_addr) {
		/*
		 * We're the target, so respond accordingly
		 */
		(void) uniarp_arp_rsp(uip, &uip->uip_arpsvrmap, &sip, &satm,
					&satmsub, ivp);

	} else {
		/*
		 * We don't know who the target is, so NAK the query
		 */
		(void) uniarp_arp_nak(uip, m, ivp);
		m = NULL;
	}

drop:
	(void) splx(s);
	if (m)
		KB_FREEALL(m);
	return;
}


/*
 * Process an ATMARP reply packet
 * 
 * Arguments:
 *	ivp	pointer to input VCC's IPVCC control block
 *	m	pointer to input packet buffer chain
 *
 * Returns:
 *	none
 *
 */
static void
proc_arp_rsp(ivp, m)
	struct ipvcc	*ivp;
	KBuffer		*m;
{
	struct ip_nif	*inp;
	struct atm_nif	*nip;
	struct siginst	*sgp;
	struct uniip	*uip;
	struct uniarp	*uap;
	struct in_addr	myip;
	int		s = splnet();

	/*
	 * Only the arp server should send these
	 */
	inp = ivp->iv_ipnif;
	nip = inp->inf_nif;
	uip = (struct uniip *)inp->inf_isintf;
	if ((uip == NULL) ||
	    (uip->uip_arpsvrvcc != ivp))
		goto drop;

	/*
	 * Locate our addresses
	 */
	sgp = nip->nif_pif->pif_siginst;
	myip.s_addr = IA_SIN(inp->inf_addr)->sin_addr.s_addr;

	/*
	 * Target addresses must be ours
	 */
	if ((tip.s_addr != myip.s_addr) ||
	    !ATM_ADDR_SEL_EQUAL(&sgp->si_addr, nip->nif_sel, &tatm) ||
	    !ATM_ADDR_SEL_EQUAL(&sgp->si_subaddr, nip->nif_sel, &tatmsub))
		goto drop;

	/*
	 * Drop packet if both Source addresses aren't present
	 */
	if ((sip.s_addr == 0) || (satm.address_format == T_ATM_ABSENT))
		goto drop;

	/*
	 * If the Source addresses are ours, this is an arp server
	 * registration response
	 */
	if (ATM_ADDR_SEL_EQUAL(&sgp->si_addr, nip->nif_sel, &satm) &&
	    ATM_ADDR_SEL_EQUAL(&sgp->si_subaddr, nip->nif_sel, &satmsub)) {
		if (sip.s_addr == myip.s_addr) {
			/*
			 * Registration response - update our state and
			 * set a registration refresh timer
			 */
			if (uip->uip_arpstate == UIAS_CLIENT_REGISTER)
				uip->uip_arpstate = UIAS_CLIENT_ACTIVE;

			if (uip->uip_arpstate == UIAS_CLIENT_ACTIVE) {
				UNIIP_ARP_CANCEL(uip);
				UNIIP_ARP_TIMER(uip, UNIARP_REGIS_REFRESH);
			}

			/*
			 * If the cache entry for the server VCC isn't valid
			 * yet, then send an Inverse ATMARP request to solicit
			 * the server's IP address
			 */
			uap = (struct uniarp *)ivp->iv_arpent;
			if ((uap->ua_flags & UAF_VALID) == 0) {
				(void) uniarp_inarp_req(uip, &uap->ua_dstatm,
					&uap->ua_dstatmsub, ivp);
			}
			goto drop;
		} else {
			log(LOG_WARNING,
				"uniarp: arpserver has our IP address wrong\n");
			goto drop;
		}
	} else if (sip.s_addr == myip.s_addr) {
		log(LOG_WARNING,
			"uniarp: arpserver has our ATM address wrong\n");
		goto drop;
	}

	/*
	 * Validate the Source IP address
	 */
	if (uniarp_validate_ip(uip, &sip, UAO_LOOKUP) != 0)
		goto drop;

	/*
	 * Now we believe this packet contains an authoritative mapping,
	 * which we probably need to setup an outgoing SVC connection
	 */
	(void) uniarp_cache_svc(uip, &sip, &satm, &satmsub, UAO_LOOKUP);

drop:
	(void) splx(s);
	KB_FREEALL(m);
	return;
}


/*
 * Process an ATMARP negative ack packet
 * 
 * Arguments:
 *	ivp	pointer to input VCC's IPVCC control block
 *	m	pointer to input packet buffer chain
 *
 * Returns:
 *	none
 *
 */
static void
proc_arp_nak(ivp, m)
	struct ipvcc	*ivp;
	KBuffer		*m;
{
	struct ip_nif	*inp;
	struct atm_nif	*nip;
	struct siginst	*sgp;
	struct uniip	*uip;
	struct uniarp	*uap;
	struct in_addr	myip;
	struct ipvcc	*inext;
	int		s = splnet();

	/*
	 * Only the arp server should send these
	 */
	inp = ivp->iv_ipnif;
	nip = inp->inf_nif;
	uip = (struct uniip *)inp->inf_isintf;
	if ((uip == NULL) ||
	    (uip->uip_arpsvrvcc != ivp))
		goto drop;

	/*
	 * Locate our addresses
	 */
	sgp = nip->nif_pif->pif_siginst;
	myip.s_addr = IA_SIN(inp->inf_addr)->sin_addr.s_addr;

	/*
	 * Source addresses must be ours
	 */
	if ((sip.s_addr != myip.s_addr) ||
	    !ATM_ADDR_SEL_EQUAL(&sgp->si_addr, nip->nif_sel, &satm) ||
	    !ATM_ADDR_SEL_EQUAL(&sgp->si_subaddr, nip->nif_sel, &satmsub))
		goto drop;

	/*
	 * Drop packet if the Target IP address isn't there or if this
	 * is a registration response, indicating an old or flakey server
	 */
	if ((tip.s_addr == 0) || (tip.s_addr == myip.s_addr))
		goto drop;

	/*
	 * Otherwise, see who we were looking for
	 */
	UNIARP_LOOKUP(tip.s_addr, uap);
	if (uap == NULL)
		goto drop;

	/*
	 * This entry isn't valid any longer, so notify all VCCs using this
	 * entry that they must finish up.  The last notify should cause
	 * this entry to be freed by the vcclose() function.
	 */
	uap->ua_flags &= ~UAF_VALID;
	for (ivp = uap->ua_ivp; ivp; ivp = inext) {
		inext = ivp->iv_arpnext;
		(*inp->inf_arpnotify)(ivp, MAP_FAILED);
	}

drop:
	(void) splx(s);
	KB_FREEALL(m);
	return;
}


/*
 * Process an InATMARP request packet
 * 
 * Arguments:
 *	ivp	pointer to input VCC's IPVCC control block
 *	m	pointer to input packet buffer chain
 *
 * Returns:
 *	none
 *
 */
static void
proc_inarp_req(ivp, m)
	struct ipvcc	*ivp;
	KBuffer		*m;
{
	struct ip_nif	*inp;
	struct atm_nif	*nip;
	struct siginst	*sgp;
	struct uniip	*uip;
	struct in_addr	myip;
	int		s = splnet();

	/*
	 * Get interface pointers
	 */
	inp = ivp->iv_ipnif;
	nip = inp->inf_nif;
	uip = (struct uniip *)inp->inf_isintf;
	if (uip == NULL)
		goto drop;

	/*
	 * Locate our addresses
	 */
	sgp = nip->nif_pif->pif_siginst;
	myip.s_addr = IA_SIN(inp->inf_addr)->sin_addr.s_addr;

	/*
	 * Packet must have a Source IP address and, if it was received
	 * over an SVC, a Source ATM address too.
	 */
	if ((sip.s_addr == 0) ||
	    ((ivp->iv_flags & IVF_SVC) && (satm.address_format == T_ATM_ABSENT)))
		goto drop;

	/*
	 * Validate Source ATM address
	 *      - can't be me
	 */
	if (satm.address_format != T_ATM_ABSENT) {
		if (ATM_ADDR_SEL_EQUAL(&sgp->si_addr, nip->nif_sel, &satm) &&
		    ATM_ADDR_SEL_EQUAL(&sgp->si_subaddr, nip->nif_sel,
						&satmsub))
			goto drop;
	}

	/*
	 * Validate Source IP address
	 */
	if ((sip.s_addr == myip.s_addr) ||
	    (uniarp_validate_ip(uip, &sip, UAO_PEER_REQ) != 0))
		goto drop;

	/*
	 * The Target ATM address is required for a packet received over
	 * an SVC, optional for a PVC.  If one is present, it must be our
	 * address.
	 */
	if ((ivp->iv_flags & IVF_SVC) && (tatm.address_format == T_ATM_ABSENT))
		goto drop;
	if ((tatm.address_format != T_ATM_ABSENT) &&
	    (!ATM_ADDR_SEL_EQUAL(&sgp->si_addr, nip->nif_sel, &tatm) ||
	     !ATM_ADDR_SEL_EQUAL(&sgp->si_subaddr, nip->nif_sel, &tatmsub)))
		goto drop;

	/*
	 * See where this packet is from
	 */
	if (ivp->iv_flags & IVF_PVC) {
		/*
		 * Process the PVC arp data, although we don't really 
		 * update the arp cache with this information
		 */
		uniarp_cache_pvc(ivp, &sip, &satm, &satmsub);

	} else if (uip->uip_arpsvrvcc == ivp) {
		/*
		 * Packet is from the arp server, so we've received a
		 * registration/refresh request (1577 version).
		 *
		 * Therefore, update cache with authoritative data.
		 */
		(void) uniarp_cache_svc(uip, &sip, &satm, &satmsub, UAO_LOOKUP);

		/*
		 * Make sure the cache update didn't kill the server VCC
		 */
		if (uip->uip_arpsvrvcc != ivp)
			goto drop;

		/*
		 * Update the server state and set the
		 * registration refresh timer
		 */
		uip->uip_arpstate = UIAS_CLIENT_ACTIVE;
		UNIIP_ARP_CANCEL(uip);
		UNIIP_ARP_TIMER(uip, UNIARP_REGIS_REFRESH);
	} else {
		/*
		 * Otherwise, we consider this source mapping data as
		 * non-authoritative and update the cache appropriately
		 */
		if (uniarp_cache_svc(uip, &sip, &satm, &satmsub, UAO_PEER_REQ))
			goto drop;
	}

	/*
	 * Send an InATMARP response back to originator
	 */
	(void) uniarp_inarp_rsp(uip, &sip, &satm, &satmsub, ivp);

drop:
	(void) splx(s);
	KB_FREEALL(m);
	return;
}


/*
 * Process an InATMARP response packet
 * 
 * Arguments:
 *	ivp	pointer to input VCC's IPVCC control block
 *	m	pointer to input packet buffer chain
 *
 * Returns:
 *	none
 *
 */
static void
proc_inarp_rsp(ivp, m)
	struct ipvcc	*ivp;
	KBuffer		*m;
{
	struct ip_nif	*inp;
	struct atm_nif	*nip;
	struct siginst	*sgp;
	struct uniip	*uip;
	struct in_addr	myip;
	int		s = splnet();

	/*
	 * Get interface pointers
	 */
	inp = ivp->iv_ipnif;
	nip = inp->inf_nif;
	uip = (struct uniip *)inp->inf_isintf;
	if (uip == NULL)
		goto drop;

	/*
	 * Locate our addresses
	 */
	sgp = nip->nif_pif->pif_siginst;
	myip.s_addr = IA_SIN(inp->inf_addr)->sin_addr.s_addr;

	/*
	 * Packet must have a Source IP address and, if it was received
	 * over an SVC, a Source ATM address too.
	 */
	if ((sip.s_addr == 0) ||
	    ((ivp->iv_flags & IVF_SVC) && (satm.address_format == T_ATM_ABSENT)))
		goto drop;

	/*
	 * Validate Source ATM address
	 *      - can't be me
	 */
	if (satm.address_format != T_ATM_ABSENT) {
		if (ATM_ADDR_SEL_EQUAL(&sgp->si_addr, nip->nif_sel, &satm) &&
		    ATM_ADDR_SEL_EQUAL(&sgp->si_subaddr, nip->nif_sel,
						&satmsub))
			goto drop;
	}

	/*
	 * Validate Source IP address
	 *      - must be in our LIS
	 *      - can't be me
	 *      - can't be broadcast
	 *      - can't be multicast
	 */
	if ((sip.s_addr == myip.s_addr) ||
	    (uniarp_validate_ip(uip, &sip, UAO_PEER_RSP) != 0))
		goto drop;

	/*
	 * The Target ATM address is required for a packet received over
	 * an SVC, optional for a PVC.  If one is present, it must be our
	 * address.
	 */
	if ((ivp->iv_flags & IVF_SVC) && (tatm.address_format == T_ATM_ABSENT))
		goto drop;
	if ((tatm.address_format != T_ATM_ABSENT) &&
	    (!ATM_ADDR_SEL_EQUAL(&sgp->si_addr, nip->nif_sel, &tatm) ||
	     !ATM_ADDR_SEL_EQUAL(&sgp->si_subaddr, nip->nif_sel, &tatmsub)))
		goto drop;

	/*
	 * See where this packet is from
	 */
	if (ivp->iv_flags & IVF_PVC) {
		/*
		 * Process the PVC arp data, although we don't really 
		 * update the arp cache with this information
		 */
		uniarp_cache_pvc(ivp, &sip, &satm, &satmsub);

	} else {
		/*
		 * Can't tell the difference between an RFC-1577 registration
		 * and a data connection from a client of another arpserver 
		 * on our LIS (using SCSP) - so we'll update the cache now
		 * with what we've got.  Our clients will get "registered"
		 * when (if) they query us with an arp request.
		 */
		(void) uniarp_cache_svc(uip, &sip, &satm, &satmsub,
				UAO_PEER_RSP);
	}

drop:
	(void) splx(s);
	KB_FREEALL(m);
	return;
}


/*
 * Print an ATMARP PDU
 * 
 * Arguments:
 *	ivp	pointer to input VCC control block
 *	m	pointer to pdu buffer chain
 *	msg	pointer to message string
 *
 * Returns:
 *	none
 *
 */
void
uniarp_pdu_print(ivp, m, msg)
	struct ipvcc	*ivp;
	KBuffer		*m;
	char		*msg;
{
	char		buf[128];
	struct vccb	*vcp;

	vcp = ivp->iv_conn->co_connvc->cvc_vcc;
	snprintf(buf, sizeof(buf),
	    "uniarp %s: vcc=(%d,%d)\n", msg, vcp->vc_vpi, vcp->vc_vci);
	atm_pdu_print(m, buf);
}

