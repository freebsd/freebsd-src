/*-
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
 * UNI ATMARP support (RFC1577) - Virtual Channel Management
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
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

#include <vm/uma.h>

extern uma_zone_t	unisig_vc_zone;

/*
 * Local variables
 */
static struct attr_llc	uniarp_llc = {
	T_ATM_PRESENT,
	{
		T_ATM_LLC_SHARING,
		8,
		{0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x08, 0x06}
	}
};

static struct t_atm_cause       uniarp_cause = {
	T_ATM_ITU_CODING,
	T_ATM_LOC_USER,
	T_ATM_CAUSE_TEMPORARY_FAILURE,
	{0, 0, 0, 0}
};


/*
 * Process a new PVC requiring ATMARP support
 * 
 * This function is called after IP/ATM has successfully opened a PVC which
 * requires ATMARP support.  We will send an InATMARP request over the PVC
 * to elicit a response from the PVC's ATMARP peer informing us of its
 * network address.  This information will also be used by IP/ATM in order 
 * to complete its address-to-VC mapping table.
 *
 * Arguments:
 *	ivp	pointer to PVC's IPVCC control block
 *
 * Returns:
 *	MAP_PROCEEDING	- OK so far, querying for peer's mapping
 *	MAP_FAILED	- error, unable to allocate resources
 *
 */
int
uniarp_pvcopen(ivp)
	struct ipvcc	*ivp;
{
	struct uniip	*uip;
	struct uniarp	*uap;
	int		s, err;

	ATM_DEBUG1("uniarp_pvcopen: ivp=%p\n", ivp);

	ivp->iv_arpent = NULL;

	/*
	 * Check things out
	 */
	if ((ivp->iv_flags & IVF_LLC) == 0)
		return (MAP_FAILED);

	/*
	 * Get uni interface
	 */
	uip = (struct uniip *)ivp->iv_ipnif->inf_isintf;
	if (uip == NULL)
		return (MAP_FAILED);

	/*
	 * Get an arp map entry
	 */
	uap = uma_zalloc(uniarp_zone, M_WAITOK | M_ZERO);
	if (uap == NULL)
		return (MAP_FAILED);

	/*
	 * Create our CM connection
	 */
	err = atm_cm_addllc(&uniarp_endpt, ivp, &uniarp_llc,
			ivp->iv_conn, &ivp->iv_arpconn);
	if (err) {
		/*
		 * We don't take no (or maybe) for an answer
		 */
		if (ivp->iv_arpconn) {
			(void) atm_cm_release(ivp->iv_arpconn, &uniarp_cause);
			ivp->iv_arpconn = NULL;
		}
		uma_zfree(uniarp_zone, uap);
		return (MAP_FAILED);
	}

	/*
	 * Get map entry set up
	 */
	s = splnet();
	uap->ua_dstatm.address_format = T_ATM_ABSENT;
	uap->ua_dstatmsub.address_format = T_ATM_ABSENT;
	uap->ua_intf = uip;

	/*
	 * Put ivp on arp entry chain
	 */
	LINK2TAIL(ivp, struct ipvcc, uap->ua_ivp, iv_arpnext);
	ivp->iv_arpent = (struct arpmap *)uap;

	/*
	 * Put arp entry on pvc chain
	 */
	LINK2TAIL(uap, struct uniarp, uniarp_pvctab, ua_next);

	/*
	 * Send Inverse ATMARP request
	 */
	(void) uniarp_inarp_req(uip, &uap->ua_dstatm, &uap->ua_dstatmsub, ivp);

	/*
	 * Start resend timer 
	 */
	uap->ua_aging = UNIARP_REVALID_AGE;

	(void) splx(s);
	return (MAP_PROCEEDING);
}


/*
 * Process a new outgoing SVC requiring ATMARP support
 * 
 * This function is called by the IP/ATM module to resolve a destination 
 * IP address to an ATM address in order to open an SVC to that destination.
 * If a valid mapping is already in our cache, then we just tell the caller
 * about it and that's that.  Otherwise, we have to allocate a new arp entry
 * and issue a query for the mapping.
 *
 * Arguments:
 *	ivp	pointer to SVC's IPVCC control block
 *	dst	pointer to destination IP address
 *
 * Returns:
 *	MAP_VALID	- Got the answer, returned via iv_arpent field.
 *	MAP_PROCEEDING	- OK so far, querying for peer's mapping
 *	MAP_FAILED	- error, unable to allocate resources
 *
 */
int
uniarp_svcout(ivp, dst)
	struct ipvcc	*ivp;
	struct in_addr	*dst;
{
	struct uniip	*uip;
	struct uniarp	*uap;
	int	s = splnet();

	ATM_DEBUG2("uniarp_svcout: ivp=%p,dst=0x%x\n", ivp, dst->s_addr);

	ivp->iv_arpent = NULL;

	/*
	 * Get uni interface
	 */
	uip = (struct uniip *)ivp->iv_ipnif->inf_isintf;
	if (uip == NULL) {
		(void) splx(s);
		return (MAP_FAILED);
	}

	/*
	 * Lookup IP destination address
	 */
	UNIARP_LOOKUP(dst->s_addr, uap);

	if (uap) {
		/*
		 * We've got an entry, verify interface
		 */
		if (uap->ua_intf != uip) {
			(void) splx(s);
			return (MAP_FAILED);
		}

		/*
		 * Chain this vcc onto entry
		 */
		LINK2TAIL(ivp, struct ipvcc, uap->ua_ivp, iv_arpnext);
		ivp->iv_arpent = (struct arpmap *)uap;
		uap->ua_flags |= UAF_USED;

		if (uap->ua_flags & UAF_VALID) {
			/*
			 * Entry is valid, we're done
			 */
			(void) splx(s);
			return (MAP_VALID);
		} else {
			/*
			 * We're already looking for this address
			 */
			(void) splx(s);
			return (MAP_PROCEEDING);
		}
	}

	/*
	 * No info in the cache.  If we're the server, then
	 * we're already authoritative, so just deny request.
	 * If we're a client but the server VCC isn't open we
	 * also deny the request.
	 */
	if (uip->uip_arpstate != UIAS_CLIENT_ACTIVE) {
		(void) splx(s);
		return (MAP_FAILED);
	}

	/*
	 * We're a client with an open VCC to the server, get a new arp entry
	 * May be called from timeout - don't wait.
	 */
	uap = uma_zalloc(uniarp_zone, M_NOWAIT);
	if (uap == NULL) {
		(void) splx(s);
		return (MAP_FAILED);
	}

	/*
	 * Get entry set up
	 */
	uap->ua_dstip.s_addr = dst->s_addr;
	uap->ua_dstatm.address_format = T_ATM_ABSENT;
	uap->ua_dstatm.address_length = 0;
	uap->ua_dstatmsub.address_format = T_ATM_ABSENT;
	uap->ua_dstatmsub.address_length = 0;
	uap->ua_intf = uip;

	/*
	 * Link ipvcc to arp entry for later notification
	 */
	LINK2TAIL(ivp, struct ipvcc, uap->ua_ivp, iv_arpnext);
	ivp->iv_arpent = (struct arpmap *)uap;
	uap->ua_flags |= UAF_USED;

	/*
	 * Add arp entry to table
	 */
	UNIARP_ADD(uap);

	/*
	 * Issue arp request for this address
	 */
	(void) uniarp_arp_req(uip, dst);

	/*
	 * Start retry timer
	 */
	UNIARP_TIMER(uap, UNIARP_ARP_RETRY);

	(void) splx(s);
	return (MAP_PROCEEDING);
}


/*
 * Process a new incoming SVC requiring ATMARP support
 * 
 * This function is called by the IP/ATM module to resolve a caller's ATM
 * address to its IP address for an incoming call in order to allow a
 * bi-directional flow of IP packets on the SVC.  If a valid mapping is
 * already in our cache, then we will use it.  Otherwise, we have to allocate
 * a new arp entry and wait for the SVC to become active so that we can issue
 * an InATMARP to the peer.
 *
 * Arguments:
 *	ivp	pointer to SVC's IPVCC control block
 *	dst	pointer to caller's ATM address
 *	dstsub	pointer to caller's ATM subaddress
 *
 * Returns:
 *	MAP_VALID	- Got the answer, returned via iv_arpent field.
 *	MAP_PROCEEDING	- OK so far, querying for peer's mapping
 *	MAP_FAILED	- error, unable to allocate resources
 *
 */
int
uniarp_svcin(ivp, dst, dstsub)
	struct ipvcc	*ivp;
	Atm_addr	*dst;
	Atm_addr	*dstsub;
{
	struct uniip	*uip;
	struct uniarp	*uap;
	int	found = 0, i, s = splnet();

	ATM_DEBUG1("uniarp_svcin: ivp=%p\n", ivp);

	/*
	 * Clear ARP entry field
	 */
	ivp->iv_arpent = NULL;

	/*
	 * Check things out
	 */
	if ((ivp->iv_flags & IVF_LLC) == 0)
		return (MAP_FAILED);

	/*
         * Get uni interface
         */
	uip = (struct uniip *)ivp->iv_ipnif->inf_isintf;
	if (uip == NULL) {
		(void) splx(s);
		return (MAP_FAILED);
	}

	/*
	 * Make sure we're configured as a client or server
	 */
	if (uip->uip_arpstate == UIAS_NOTCONF) {
		(void) splx(s);
		return (MAP_FAILED);
	}

	/*
	 * If we know the caller's ATM address, look it up
	 */
	uap = NULL;
	if (dst->address_format != T_ATM_ABSENT) {
		for (i = 0; (i < UNIARP_HASHSIZ) && (found == 0); i++) {
			for (uap = uniarp_arptab[i]; uap; uap = uap->ua_next) {
				if (ATM_ADDR_EQUAL(dst, &uap->ua_dstatm) &&
				    ATM_ADDR_EQUAL(dstsub, &uap->ua_dstatmsub)){
					found = 1;
					break;
				}
			}
		}
		if (uap == NULL) {
			for (uap = uniarp_nomaptab; uap; uap = uap->ua_next) {
				if (ATM_ADDR_EQUAL(dst, &uap->ua_dstatm) &&
				    ATM_ADDR_EQUAL(dstsub, &uap->ua_dstatmsub))
					break;
			}
		}
	}

	if (uap) {
		/*
		 * We've got an entry, verify interface
		 */
		if (uap->ua_intf != uip) {
			(void) splx(s);
			return (MAP_FAILED);
		}

		/*
		 * Chain the vcc onto this entry
		 */
		LINK2TAIL(ivp, struct ipvcc, uap->ua_ivp, iv_arpnext);
		ivp->iv_arpent = (struct arpmap *)uap;
		uap->ua_flags |= UAF_USED;

		if (uap->ua_flags & UAF_VALID) {
			/*
			 * Entry is valid, we're done
			 */
			(void) splx(s);
			return (MAP_VALID);
		} else {
			/*
			 * We're already looking for this address
			 */
			(void) splx(s);
			return (MAP_PROCEEDING);
		}
	}

	/*
	 * No info in the cache - get a new arp entry
	 * May be called from timeout - don't wait.
	 */
	uap = uma_zalloc(uniarp_zone, M_NOWAIT | M_ZERO);
	if (uap == NULL) {
		(void) splx(s);
		return (MAP_FAILED);
	}

	/*
	 * Get entry set up
	 */
	ATM_ADDR_COPY(dst, &uap->ua_dstatm);
	ATM_ADDR_COPY(dstsub, &uap->ua_dstatmsub);
	uap->ua_intf = uip;

	/*
	 * Link ipvcc to arp entry for later notification
	 */
	LINK2TAIL(ivp, struct ipvcc, uap->ua_ivp, iv_arpnext);
	ivp->iv_arpent = (struct arpmap *)uap;
	uap->ua_flags |= UAF_USED;

	/*
	 * Add arp entry to 'nomap' table
	 */
	LINK2TAIL(uap, struct uniarp, uniarp_nomaptab, ua_next);

	(void) splx(s);

	/*
	 * Now we just wait for SVC to become active
	 */
	return (MAP_PROCEEDING);
}


/*
 * Process ARP SVC activation notification
 *
 * This function is called by the IP/ATM module whenever a previously
 * opened SVC has successfully been connected.
 *
 * Arguments:
 *	ivp	pointer to SVC's IPVCC control block
 *
 * Returns:
 *	0	activation processing successful
 *	errno	activation failed - reason indicated
 *
 */
int
uniarp_svcactive(ivp)
	struct ipvcc	*ivp;
{
	struct ip_nif	*inp;
	struct uniip	*uip;
	struct uniarp	*uap;
	int	err, s = splnet();

	ATM_DEBUG1("uniarp_svcactive: ivp=%p\n", ivp);

	inp = ivp->iv_ipnif;
	uip = (struct uniip *)inp->inf_isintf;
	uap = (struct uniarp *)ivp->iv_arpent;

	/*
	 * First, we need to create our CM connection
	 */
	err = atm_cm_addllc(&uniarp_endpt, ivp, &uniarp_llc,
			ivp->iv_conn, &ivp->iv_arpconn);
	if (err) {
		/*
		 * We don't take no (or maybe) for an answer
		 */
		if (ivp->iv_arpconn) {
			(void) atm_cm_release(ivp->iv_arpconn, &uniarp_cause);
			ivp->iv_arpconn = NULL;
		}
		return (err);
	}

	/*
	 * Is this the client->server vcc??
	 */
	if (uip->uip_arpsvrvcc == ivp) {

		/*
		 * Yep, go into the client registration phase
		 */
		uip->uip_arpstate = UIAS_CLIENT_REGISTER;

		/*
		 * To register ourselves, RFC1577 says we should wait
		 * around for the server to send us an InATMARP_Request.
		 * However, draft-1577+ just has us send an ATMARP_Request
		 * for our own address.  To keep everyone happy, we'll go 
		 * with both and see what works!
		 */
		(void) uniarp_arp_req(uip, &(IA_SIN(inp->inf_addr)->sin_addr));

		/*
		 * Start retry timer
		 */
		UNIIP_ARP_TIMER(uip, 1 * ATM_HZ);

		(void) splx(s);
		return (0);
	}

	/*
	 * Send an InATMARP_Request on this VCC to find out/notify who's at 
	 * the other end.  If we're the server, this will start off the
	 * RFC1577 registration procedure.  If we're a client, then this
	 * SVC is for user data and it's pretty likely that both ends are
	 * going to be sending packets.  So, if we're the caller, we'll be
	 * nice and let the callee know right away who we are.  If we're the
	 * callee, let's find out asap the caller's IP address.
	 */
	(void) uniarp_inarp_req(uip, &uap->ua_dstatm, &uap->ua_dstatmsub, ivp);

	/*
	 * Start retry timer if entry isn't valid yet
	 */
	if (((uap->ua_flags & UAF_VALID) == 0) &&
	    ((uap->ua_time.ti_flag & TIF_QUEUED) == 0))
		UNIARP_TIMER(uap, UNIARP_ARP_RETRY);

	(void) splx(s);
	return (0);
}


/*
 * Process VCC close
 * 
 * This function is called just prior to IP/ATM closing a VCC which 
 * supports ATMARP.  We'll sever our links to the VCC and then
 * figure out how much more cleanup we need to do for now.
 *
 * Arguments:
 *	ivp	pointer to VCC's IPVCC control block
 *
 * Returns:
 *	none
 *
 */
void
uniarp_vcclose(ivp)
	struct ipvcc	*ivp;
{
	struct uniip	*uip;
	struct uniarp	*uap;
	int	s;

	ATM_DEBUG1("uniarp_vcclose: ivp=%p\n", ivp);

	/*
	 * Close our CM connection
	 */
	if (ivp->iv_arpconn) {
		(void) atm_cm_release(ivp->iv_arpconn, &uniarp_cause);
		ivp->iv_arpconn = NULL;
	}

	/*
	 * Get atmarp entry
	 */
	if ((uap = (struct uniarp *)ivp->iv_arpent) == NULL)
		return;
	uip = uap->ua_intf;

	s = splnet();

	/*
	 * If this is the arpserver VCC, then schedule ourselves to
	 * reopen the connection soon
	 */
	if (uip->uip_arpsvrvcc == ivp) {
		uip->uip_arpsvrvcc = NULL;
		uip->uip_arpstate = UIAS_CLIENT_POPEN;
		UNIIP_ARP_CANCEL(uip);
		UNIIP_ARP_TIMER(uip, 5 * ATM_HZ);
	}

	/*
	 * Remove IP VCC from chain
	 */
	UNLINK(ivp, struct ipvcc, uap->ua_ivp, iv_arpnext);

	/*
	 * SVCs and PVCs are handled separately
	 */
	if (ivp->iv_flags & IVF_SVC) {
		/*
		 * If the mapping is currently valid or in use, or if there 
		 * are other VCCs still using this mapping, we're done for now
		 */
		if ((uap->ua_flags & (UAF_VALID | UAF_LOCKED)) ||
		    (uap->ua_origin >= UAO_PERM) ||
		    (uap->ua_ivp != NULL)) {
			(void) splx(s);
			return;
		}

		/*
		 * Unlink the entry
		 */
		if (uap->ua_dstip.s_addr == 0) {
			UNLINK(uap, struct uniarp, uniarp_nomaptab, ua_next);
		} else {
			UNIARP_DELETE(uap);
		}
	} else {
		/*
		 * Remove entry from pvc table
		 */
		UNLINK(uap, struct uniarp, uniarp_pvctab, ua_next);
	}

	UNIARP_CANCEL(uap);

	/*
	 * Finally, free the entry
	 */
	uma_zfree(uniarp_zone, uap);
	(void) splx(s);
	return;
}


/*
 * Process ATMARP VCC Connected Notification
 * 
 * Arguments:
 *	toku	owner's connection token (ipvcc protocol block)
 *
 * Returns:
 *	none
 *
 */
void
uniarp_connected(toku)
	void		*toku;
{

	/*
	 * Since we only do atm_cm_addllc()'s on active connections,
	 * we should never get called here...
	 */
	panic("uniarp_connected");
}


/*
 * Process ATMARP VCC Cleared Notification
 * 
 * Arguments:
 *	toku	owner's connection token (ipvcc protocol block)
 *	cause	pointer to cause code
 *
 * Returns:
 *	none
 *
 */
void
uniarp_cleared(toku, cause)
	void		*toku;
	struct t_atm_cause	*cause;
{
	struct ipvcc	*ivp = toku;
	int		s;

	s = splnet();

	/*
	 * We're done with VCC
	 */
	ivp->iv_arpconn = NULL;

	/*
	 * If IP is finished with VCC, then we'll free it
	 */
	if (ivp->iv_state == IPVCC_FREE)
		uma_zfree(unisig_vc_zone, ivp);
	(void) splx(s);
}

