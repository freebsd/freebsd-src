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
 * SPANS Signalling Manager
 * ---------------------------
 *
 * SPANS CLS - ARP support
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
#include "spans_xdr.h"
#include <netatm/spans/spans_var.h>
#include <netatm/spans/spans_cls.h>

#include <vm/uma.h>

/*
 * Global variables
 */
struct spansarp		*spansarp_arptab[SPANSARP_HASHSIZ] = {NULL};


/*
 * Local functions
 */
static int		spansarp_request(struct spansarp *);
static void		spansarp_aging(struct atm_time *);
static void		spansarp_retry(struct atm_time *);

/*
 * Local variables
 */
static struct atm_time	spansarp_timer = {0, 0};	/* Aging timer */
static struct atm_time	spansarp_rtimer = {0, 0};	/* Retry timer */

static struct spansarp	*spansarp_retry_head = NULL;	/* Retry chain */

static uma_zone_t	spansarp_zone;


/*
 * Process a new outgoing SVC requiring SPANS ARP support
 * 
 * This function is called by an endpoint wishing to resolve a destination 
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
spansarp_svcout(ivp, dst)
	struct ipvcc	*ivp;
	struct in_addr	*dst;
{
	struct spanscls	*clp;
	struct spansarp	*sap;
	int	s;

	ivp->iv_arpent = NULL;

	/*
	 * Lookup destination address
	 */
	s = splnet();
	SPANSARP_LOOKUP(dst->s_addr, sap);

	if (sap) {
		/*
		 * Link this vcc to entry queue
		 */
		LINK2TAIL(ivp, struct ipvcc, sap->sa_ivp, iv_arpnext);

		/*
		 * If entry is valid, we're done
		 */
		if (sap->sa_flags & SAF_VALID) {
			ivp->iv_arpent = (struct arpmap *)sap;
			(void) splx(s);
			return (MAP_VALID);
		}

		/*
		 * We're already looking for this address
		 */
		(void) splx(s);
		return (MAP_PROCEEDING);
	}

	/*
	 * Need a new arp entry - first, find the cls instance
	 * corresponding to the requestor's IP interface.
	 */
	for (clp = spanscls_head; clp; clp = clp->cls_next) {
		if (clp->cls_ipnif == ivp->iv_ipnif)
			break;
	}
	if (clp == NULL) {
		(void) splx(s);
		return (MAP_FAILED);
	}

	/*
	 * Now get the new arp entry
	 */
	sap = uma_zalloc(spansarp_zone, M_WAITOK);
	if (sap == NULL) {
		(void) splx(s);
		return (MAP_FAILED);
	}

	/*
	 * Get entry set up
	 */
	sap->sa_dstip.s_addr = dst->s_addr;
	sap->sa_dstatm.address_format = T_ATM_ABSENT;
	sap->sa_dstatm.address_length = 0;
	sap->sa_dstatmsub.address_format = T_ATM_ABSENT;
	sap->sa_dstatmsub.address_length = 0;
	sap->sa_cls = clp;
	sap->sa_origin = SAO_LOOKUP;

	/*
	 * Link ipvcc to arp entry for later notification
	 */
	LINK2TAIL(ivp, struct ipvcc, sap->sa_ivp, iv_arpnext);

	/*
	 * Add arp entry to table
	 */
	SPANSARP_ADD(sap);

	/*
	 * Add arp entry to retry list and start retry timer if needed
	 */
	LINK2TAIL(sap, struct spansarp, spansarp_retry_head, sa_rnext);
	if ((spansarp_rtimer.ti_flag & TIF_QUEUED) == 0)
		atm_timeout(&spansarp_rtimer, SPANSARP_RETRY, spansarp_retry);

	/*
	 * Issue arp request for this address
	 */
	(void) spansarp_request(sap);

	(void) splx(s);
	return (MAP_PROCEEDING);
}


/*
 * Process a new incoming SVC requiring SPANS ARP support
 * 
 * This function is called by an endpoint wishing to resolve a destination 
 * ATM address to its IP address for an incoming call in order to allow a
 * bi-directional flow of IP packets on the SVC.
 *
 * SPANS ARP does not provide reverse mapping facilities and only supports
 * uni-directional SVCs.  Thus, we lie a little to IP and always return a
 * MAP_PROCEEDING indication, but we will never later notify IP of a 
 * MAP_VALID condition.
 *
 * Arguments:
 *	ivp	pointer to SVC's IPVCC control block
 *	dst	pointer to destination ATM address
 *	dstsub	pointer to destination ATM subaddress
 *
 * Returns:
 *	MAP_VALID	- Got the answer, returned via iv_arpent field.
 *	MAP_PROCEEDING	- OK so far, querying for peer's mapping
 *	MAP_FAILED	- error, unable to allocate resources
 *
 */
int
spansarp_svcin(ivp, dst, dstsub)
	struct ipvcc	*ivp;
	Atm_addr	*dst;
	Atm_addr	*dstsub;
{
	/*
	 * Clear ARP entry field
	 */
	ivp->iv_arpent = NULL;

	return (MAP_PROCEEDING);
}


/*
 * SPANS ARP SVC activation notification
 * 
 * This function is called when a previously opened SVC has successfully
 * been connected.
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
spansarp_svcactive(ivp)
	struct ipvcc	*ivp;
{
	struct spansarp	*sap;
	int	s = splnet();

	/* 
	 * Find an entry for the destination address
	 */
	SPANSARP_LOOKUP(ivp->iv_dst.s_addr, sap);
	if (sap) {
		/*
		 * IP is finished with entry, so remove IP VCC from chain
		 */
		UNLINK(ivp, struct ipvcc, sap->sa_ivp, iv_arpnext);
		ivp->iv_arpent = NULL;

		/*
		 * This seems like a reasonable reason to refresh the entry
		 */
		sap->sa_reftime = 0;
	}

	(void) splx(s);
	return (0);
}


/*
 * SPANS ARP supported VCC is closing
 * 
 * This function is called just prior to a user closing a VCC which 
 * supports SPANS ARP.  We'll sever our links to the VCC and then
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
spansarp_vcclose(ivp)
	struct ipvcc	*ivp;
{
	struct spansarp	*sap;
	int	s = splnet();

	/*
	 * Get spansarp entry
	 */
	SPANSARP_LOOKUP(ivp->iv_dst.s_addr, sap);
	if (sap == NULL) {
		(void) splx(s);
		return;
	}

	/*
	 * Remove IP VCC from chain
	 */
	UNLINK(ivp, struct ipvcc, sap->sa_ivp, iv_arpnext);
	ivp->iv_arpent = NULL;

	/*
	 * If entry is currently valid or in use, not much else for us to do
	 */
	if ((sap->sa_flags & (SAF_VALID | SAF_LOCKED)) ||
	    (sap->sa_origin >= SAO_PERM)) {
		(void) splx(s);
		return;
	}

	/*
	 * If there are still other VCCs waiting, exit
	 */
	if (sap->sa_ivp) {
		(void) splx(s);
		return;
	}

	/*
	 * Noone else waiting, so remove entry from the retry chain
	 */
	UNLINK(sap, struct spansarp, spansarp_retry_head, sa_rnext);

	/*
	 * Free entry
	 */
	SPANSARP_DELETE(sap);
	uma_zfree(spansarp_zone, sap);
	(void) splx(s);
}

/*
 * Called when the spans module is loaded.
 */
void
spansarp_start()
{

	spansarp_zone = uma_zcreate("spansarp", sizeof(struct spansarp),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	uma_zone_set_max(spansarp_zone, 100);
}

/*
 * Process module unloading notification
 * 
 * Called whenever the spans module is about to be unloaded.  All signalling
 * instances will have been previously detached.  All spansarp resources 
 * must be freed now.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	none
 *
 */
void
spansarp_stop()
{
	int	i;

	/* 
	 * Make sure the arp table is empty
	 */
	for (i = 0; i < SPANSARP_HASHSIZ; i++) {
		if (spansarp_arptab[i] != NULL)
			panic("spansarp_stop: arp table not empty");
	}

	/*
	 * Cancel timers
	 */
	(void) atm_untimeout(&spansarp_timer);
	(void) atm_untimeout(&spansarp_rtimer);

	/*
	 * Free our storage pools
	 */
	uma_zdestroy(spansarp_zone);
}


/*
 * Process IP Network Interface Activation
 * 
 * Called whenever an IP network interface becomes active.
 *
 * Called at splnet.
 *
 * Arguments:
 *      clp     pointer to CLS interface
 *
 * Returns:
 *      none
 *
 */
void
spansarp_ipact(clp)
	struct spanscls		*clp;
{
	/*
	 * Make sure aging timer is running
	 */
	if ((spansarp_timer.ti_flag & TIF_QUEUED) == 0)
		atm_timeout(&spansarp_timer, SPANSARP_AGING, spansarp_aging);
}


/*
 * Process IP Network Interface Deactivation
 * 
 * Called whenever an IP network interface becomes inactive.
 *
 * Called at splnet.
 *
 * Arguments:
 *      clp     pointer to CLS interface
 *
 * Returns:
 *      none
 *
 */
void
spansarp_ipdact(clp)
	struct spanscls		*clp;
{
	struct spanscls		*clp2;
	struct spansarp		*sap, *snext;
	int		i;

	/* 
	 * Delete all interface entries
	 */
	for (i = 0; i < SPANSARP_HASHSIZ; i++) {
		for (sap = spansarp_arptab[i]; sap; sap = snext) {
			snext = sap->sa_next;

			/*
			 * Clean up entries for this interface
			 */
			if (sap->sa_cls != clp)
				continue;

			/*
			 * All VCCs better be gone by now
			 */
			if (sap->sa_ivp)
				panic("spansarp_ipdact: entry not empty");

			/*
			 * Remove entry from the retry chain
			 */
			UNLINK(sap, struct spansarp, 
				spansarp_retry_head, sa_rnext);

			/*
			 * Delete entry from arp table
			 */
			SPANSARP_DELETE(sap);
			uma_zfree(spansarp_zone, sap);
		}
	}

	/*
	 * Stop aging timer if this is the last active interface
	 */
	for (clp2 = spanscls_head; clp2; clp2 = clp2->cls_next) {
		if ((clp != clp2) && (clp2->cls_ipnif))
			break;
	}
	if (clp2 == NULL)
		(void) atm_untimeout(&spansarp_timer);
}


/*
 * Issue a SPANS ARP request packet
 * 
 * Arguments:
 *	sap	pointer to arp table entry
 *
 * Returns:
 *	0	packet was successfully sent
 *	else	unable to send packet
 *
 */
static int
spansarp_request(sap)
	struct spansarp	*sap;
{
	struct spanscls		*clp;
	struct spans		*spp;
	struct spanscls_hdr	*chp;
	struct spansarp_hdr	*ahp;
	KBuffer			*m;
	struct ip_nif		*inp;
	int			err;

	clp = sap->sa_cls;
	spp = clp->cls_spans;
	inp = clp->cls_ipnif;

	/*
	 * Make sure CLS VCC is open and that we know our addresses
	 */
	if (clp->cls_state != CLS_OPEN)
		return (1);
	if (spp->sp_addr.address_format != T_ATM_SPANS_ADDR)
		return (1);
	if (inp == NULL)
		return (1);

	/*
	 * Get a buffer for pdu
	 */
	KB_ALLOCPKT(m, ARP_PACKET_LEN, KB_F_NOWAIT, KB_T_DATA);
	if (m == NULL)
		return (1);

	/*
	 * Place pdu at end of buffer
	 */
	KB_PLENSET(m, ARP_PACKET_LEN);
	KB_TAILALIGN(m, ARP_PACKET_LEN);
	KB_DATASTART(m, chp, struct spanscls_hdr *);
	ahp = (struct spansarp_hdr *)(chp + 1);

	/*
	 * Build headers
	 */
	spans_addr_copy(&spans_bcastaddr, &chp->ch_dst);
	spans_addr_copy(spp->sp_addr.address, &chp->ch_src);
	*(u_int *)&chp->ch_proto = *(u_int *)&spanscls_hdr.ch_proto;
	*(u_int *)&chp->ch_dsap = *(u_int *)&spanscls_hdr.ch_dsap;
	*(u_short *)&chp->ch_oui[1] = *(u_short *)&spanscls_hdr.ch_oui[1];
	chp->ch_pid = htons(ETHERTYPE_ARP);


	/*
	 * Build ARP packet
	 */
	ahp->ah_hrd = htons(ARP_SPANS);
	ahp->ah_pro = htons(ETHERTYPE_IP);
	ahp->ah_hln = sizeof(spans_addr);
	ahp->ah_pln = sizeof(struct in_addr);
	ahp->ah_op = htons(ARP_REQUEST);
	spans_addr_copy(spp->sp_addr.address, &ahp->ah_sha);
	bcopy(&(IA_SIN(inp->inf_addr)->sin_addr), ahp->ah_spa,
		sizeof(struct in_addr));
	bcopy(&sap->sa_dstip, ahp->ah_tpa, sizeof(struct in_addr));

	/*
	 * Now, send the pdu via the CLS service
	 */
	err = atm_cm_cpcs_data(clp->cls_conn, m);
	if (err) {
		KB_FREEALL(m);
		return (1);
	}

	return (0);
}


/*
 * Process a SPANS ARP input packet
 * 
 * Arguments:
 *	clp	pointer to interface CLS control block
 *	m	pointer to input packet buffer chain
 *
 * Returns:
 *	none
 *
 */
void
spansarp_input(clp, m)
	struct spanscls	*clp;
	KBuffer		*m;
{
	struct spans		*spp = clp->cls_spans;
	struct spanscls_hdr	*chp;
	struct spansarp_hdr	*ahp;
	struct spansarp		*sap;
	struct ip_nif		*inp = clp->cls_ipnif;
	struct in_addr	in_me, in_src, in_targ;
	int		s, err;

	/*
	 * Make sure IP interface has been activated
	 */
	if (inp == NULL)
		goto free;

	/*
	 * Get the packet together
	 */
	if (KB_LEN(m) < ARP_PACKET_LEN) {
		KB_PULLUP(m, ARP_PACKET_LEN, m);
		if (m == 0)
			return;
	}
	KB_DATASTART(m, chp, struct spanscls_hdr *);
	ahp = (struct spansarp_hdr *)(chp + 1);

	bcopy(ahp->ah_spa, &in_src, sizeof(struct in_addr));
	bcopy(ahp->ah_tpa, &in_targ, sizeof(struct in_addr));
	bcopy(&(IA_SIN(inp->inf_addr)->sin_addr), &in_me,
		sizeof(struct in_addr));

	/*
	 * Initial packet verification
	 */
	if ((ahp->ah_hrd != htons(ARP_SPANS)) ||
	    (ahp->ah_pro != htons(ETHERTYPE_IP)))
		goto free;

	/*
	 * Validate source addresses
	 * 	can't be from hardware broadcast
	 *	can't be from me
	 */
	if (!spans_addr_cmp(&ahp->ah_sha, &spans_bcastaddr))
		goto free;
	if (!spans_addr_cmp(&ahp->ah_sha, spp->sp_addr.address))
		goto free;
	if (in_src.s_addr == in_me.s_addr) {
		log(LOG_ERR, 
			"duplicate IP address sent from spans address %s\n",
			spans_addr_print(&ahp->ah_sha));
		in_targ = in_me;
		goto chkop;
	}

	/*
	 * If source IP address is from unspecified or broadcast addresses,
	 * don't bother updating arp table, but answer possible requests
	 */
	if (in_broadcast(in_src, &inp->inf_nif->nif_if))
		goto chkop;

	/*
	 * Update arp table with source address info
	 */
	s = splnet();
	SPANSARP_LOOKUP(in_src.s_addr, sap);
	if (sap) {
		/*
		 * Found an entry for the source, but don't
		 * update permanent entries
		 */
		if (sap->sa_origin != SAO_PERM) {

			/*
			 * Update the entry
			 */
			sap->sa_dstatm.address_format = T_ATM_SPANS_ADDR;
			sap->sa_dstatm.address_length = sizeof(spans_addr);
			spans_addr_copy(&ahp->ah_sha, sap->sa_dstatm.address);
			sap->sa_cls = clp;
			sap->sa_reftime = 0;
			if ((sap->sa_flags & SAF_VALID) == 0) {
				/*
				 * Newly valid entry, notify waiting users
				 */
				struct ipvcc	*ivp, *inext;

				sap->sa_flags |= SAF_VALID;
				for (ivp = sap->sa_ivp; ivp; ivp = inext) {
					inext = ivp->iv_arpnext;

					ivp->iv_arpent = (struct arpmap *)sap;
					(*inp->inf_arpnotify)(ivp, MAP_VALID);
				}

				/*
				 * Remove ourselves from the retry chain
				 */
				UNLINK(sap, struct spansarp,
					spansarp_retry_head, sa_rnext);
			}
		}

	} else if (in_targ.s_addr == in_me.s_addr) {
		/*
		 * Source unknown and we're the target - add new entry
		 */
		sap = uma_zalloc(spansarp_zone, M_WAITOK);
		if (sap) {
			sap->sa_dstip.s_addr = in_src.s_addr;
			sap->sa_dstatm.address_format = T_ATM_SPANS_ADDR;
			sap->sa_dstatm.address_length = sizeof(spans_addr);
			spans_addr_copy(&ahp->ah_sha, sap->sa_dstatm.address);
			sap->sa_dstatmsub.address_format = T_ATM_ABSENT;
			sap->sa_dstatmsub.address_length = 0;
			sap->sa_cls = clp;
			sap->sa_flags = SAF_VALID;
			sap->sa_origin = SAO_LOOKUP;
			SPANSARP_ADD(sap);
		}
	}
	(void) splx(s);

chkop:
	/*
	 * If this is a request for our address, send a reply 
	 */
	if (ntohs(ahp->ah_op) != ARP_REQUEST)
		goto free;
	if (in_targ.s_addr != in_me.s_addr)
		goto free;

	spans_addr_copy(&chp->ch_src, &chp->ch_dst);
	spans_addr_copy(spp->sp_addr.address, &chp->ch_src);
	ahp->ah_op = htons(ARP_REPLY);
	spans_addr_copy(&ahp->ah_sha, &ahp->ah_tha);
	spans_addr_copy(spp->sp_addr.address, &ahp->ah_sha);
	bcopy(ahp->ah_spa, ahp->ah_tpa, sizeof(struct in_addr));
	bcopy(&in_me, ahp->ah_spa, sizeof(struct in_addr));

	err = atm_cm_cpcs_data(clp->cls_conn, m);
	if (err)
		goto free;
	return;

free:
	KB_FREEALL(m);
}


/*
 * Process a SPANS ARP aging timer tick
 * 
 * This function is called every SPANSARP_AGING seconds, in order to age
 * all the arp table entries.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tip	pointer to spansarp aging timer control block
 *
 * Returns:
 *	none
 *
 */
static void
spansarp_aging(tip)
	struct atm_time	*tip;
{
	struct spansarp	*sap, *snext;
	struct ipvcc	*ivp, *inext;
	int		i;


	/*
	 * Schedule next timeout
	 */
	atm_timeout(&spansarp_timer, SPANSARP_AGING, spansarp_aging);

	/*
	 * Run through arp table bumping each entry's aging timer.
	 */
	for (i = 0; i < SPANSARP_HASHSIZ; i++) {
		for (sap = spansarp_arptab[i]; sap; sap = snext) {
			snext = sap->sa_next;

			/*
			 * Permanent (manually installed) entries aren't aged
			 */
			if (sap->sa_origin == SAO_PERM)
				continue;

			/*
			 * See if entry is valid and over-aged
			 */
			if ((sap->sa_flags & SAF_VALID) == 0)
				continue;
			if (++sap->sa_reftime < SPANSARP_MAXAGE)
				continue;

			/*
			 * Entry is now invalid, tell IP/ATM about it
			 */
			sap->sa_flags |= SAF_LOCKED;
			for (ivp = sap->sa_ivp; ivp; ivp = inext) {
				inext = ivp->iv_arpnext;
				(*ivp->iv_ipnif->inf_arpnotify)
						(ivp, MAP_INVALID);
			}
			sap->sa_flags &= ~(SAF_LOCKED | SAF_VALID);

			if (sap->sa_ivp != NULL) {
				/*
				 * Somebody still cares, so add the arp
				 * entry to the retry list.
				 */
				LINK2TAIL(sap, struct spansarp,
						spansarp_retry_head, sa_rnext);
				if ((spansarp_rtimer.ti_flag & TIF_QUEUED) == 0)
					atm_timeout(&spansarp_rtimer,
						SPANSARP_RETRY, spansarp_retry);

				/*
				 * Issue arp request for this address
				 */
				(void) spansarp_request(sap);

			} else {
				/*
				 * Delete unused entry
				 */
				SPANSARP_DELETE(sap);
				uma_zfree(spansarp_zone, sap);
			}
		}
	}
}


/*
 * Process a SPANS ARP retry timer tick
 * 
 * This function is called every SPANSARP_RETRY seconds, in order to retry
 * awaiting arp resolution requests.  We will retry requests indefinitely,
 * assuming that IP will set a timeout to close the VCC(s) requesting the
 * failing address resolution.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tip	pointer to spansarp retry timer control block
 *
 * Returns:
 *	none
 *
 */
static void
spansarp_retry(tip)
	struct atm_time	*tip;
{
	struct spansarp	*sap;


	/*
	 * See if there's work to do
	 */
	if (spansarp_retry_head == NULL) {
		return;
	}

	/*
	 * Schedule next timeout
	 */
	atm_timeout(&spansarp_rtimer, SPANSARP_RETRY, spansarp_retry);

	/*
	 * Run through retry chain, (re)issuing arp requests.
	 */
	for (sap = spansarp_retry_head; sap; sap = sap->sa_next) {

		/*
		 * Send another arp request
		 */
		(void) spansarp_request(sap);
	}
}


/*
 * SPANS ARP IOCTL support
 *
 * Function will be called at splnet.
 *
 * Arguments:
 *	code	PF_ATM sub-operation code
 *      data    pointer to code specific parameter data area
 *      arg1    pointer to code specific argument
 *
 * Returns:
 *	0	request procesed
 *	errno	error processing request - reason indicated
 *
 */
int
spansarp_ioctl(code, data, arg1)
        int		code;
        caddr_t		data;
        caddr_t		arg1;
{
	struct atmaddreq	*aap;
	struct atmdelreq	*adp;
	struct atminfreq	*aip;
	struct spans		*spp;
	struct spanscls		*clp;
	struct spansarp		*sap;
	struct air_arp_rsp	aar;
	struct ip_nif		*inp;
	struct ipvcc		*ivp, *inext;
	struct in_addr		ip;
	u_long			dst;
	int			err = 0, i, buf_len;
	caddr_t			buf_addr;


	switch (code) {

	case AIOCS_ADD_ARP:
		/*
		 * Add a permanent ARP mapping
		 */
		aap = (struct atmaddreq *)data;
		clp = (struct spanscls *)arg1;
		inp = clp->cls_ipnif;
		if ((aap->aar_arp_addr.address_format != T_ATM_SPANS_ADDR) ||
		    (aap->aar_arp_origin != ARP_ORIG_PERM)) {
			err = EINVAL;
			break;
		}
		ip = SATOSIN(&aap->aar_arp_dst)->sin_addr;

		/*
		 * See if we already have an entry for this IP address
		 */
		SPANSARP_LOOKUP(ip.s_addr, sap);
		if (sap == NULL) {
			/*
			 * No, get a new arp entry
			 */
			sap = uma_zalloc(spansarp_zone, M_WAITOK);
			if (sap == NULL) {
				err = ENOMEM;
				break;
			}

			/*
			 * Get entry set up
			 */
			sap->sa_dstip = ip;
			ATM_ADDR_COPY(&aap->aar_arp_addr, &sap->sa_dstatm);
			sap->sa_dstatmsub.address_format = T_ATM_ABSENT;
			sap->sa_dstatmsub.address_length = 0;
			sap->sa_cls = clp;
			sap->sa_flags |= SAF_VALID;
			sap->sa_origin = SAO_PERM;

			/*
			 * Add entry to table
			 */
			SPANSARP_ADD(sap);
			break;

		}

		/*
		 * See if we're attempting to change the ATM address for
		 * this cached entry
		 */
		if ((sap->sa_dstatm.address_format != T_ATM_ABSENT) &&
		    (!ATM_ADDR_EQUAL(&aap->aar_arp_addr, &sap->sa_dstatm) ||
		     (clp != sap->sa_cls))) {

			/*
			 * Yes, notify IP/ATM that a mapping change has
			 * occurred.  IP/ATM will close any VCC's which
			 * aren't waiting for this map.
			 */
			sap->sa_flags |= SAF_LOCKED;
			for (ivp = sap->sa_ivp; ivp; ivp = inext) {
				inext = ivp->iv_arpnext;
				(*inp->inf_arpnotify)(ivp, MAP_CHANGED);
			}
			sap->sa_flags &= ~SAF_LOCKED;
		}

		/*
		 * Update the cached entry with the new data
		 */
		ATM_ADDR_COPY(&aap->aar_arp_addr, &sap->sa_dstatm);
		sap->sa_cls = clp;

		/*
		 * If this entry isn't valid, notify anyone who might
		 * be interested
		 */
		if ((sap->sa_flags & SAF_VALID) == 0) {

			sap->sa_flags |= SAF_LOCKED;
			for (ivp = sap->sa_ivp; ivp; ivp = inext) {
				inext = ivp->iv_arpnext;
				(*inp->inf_arpnotify)(ivp, MAP_VALID);
			}
			sap->sa_flags &= ~SAF_LOCKED;
		}

		/*
		 * Remove this entry from the retry chain
		 */
		UNLINK(sap, struct spansarp, spansarp_retry_head, sa_rnext);

		/*
		 * Mark the entry as permanent
		 */
		sap->sa_flags |= SAF_VALID;
		sap->sa_origin = SAO_PERM;
		break;

	case AIOCS_DEL_ARP:
		/*
		 * Delete an ARP mapping
		 */
		adp = (struct atmdelreq *)data;
		clp = (struct spanscls *)arg1;
		ip = SATOSIN(&adp->adr_arp_dst)->sin_addr;

		/*
		 * Now find the entry to be deleted
		 */
		SPANSARP_LOOKUP(ip.s_addr, sap);
		if (sap == NULL) {
			err = ENOENT;
			break;
		}

		/*
		 * Notify all VCCs using this entry that they must finish
		 * up now.  
		 */
		sap->sa_flags |= SAF_LOCKED;
		for (ivp = sap->sa_ivp; ivp; ivp = inext) {
			inext = ivp->iv_arpnext;
			(*ivp->iv_ipnif->inf_arpnotify)(ivp, MAP_FAILED);
		}

		/*
		 * Now free up the entry
		 */
		UNLINK(sap, struct spansarp, spansarp_retry_head, sa_rnext);
		SPANSARP_DELETE(sap);
		uma_zfree(spansarp_zone, sap);
		break;

	case AIOCS_INF_ARP:
		/*
		 * Get ARP table information
		 */
		aip = (struct atminfreq *)data;
		spp = (struct spans *)arg1;

		if (aip->air_arp_addr.sa_family != AF_INET)
			break;
		dst = SATOSIN(&aip->air_arp_addr)->sin_addr.s_addr;

		buf_addr = aip->air_buf_addr;
		buf_len = aip->air_buf_len;

		if ((clp = spp->sp_cls) == NULL)
			break;

		/*
		 * Run through entire arp table
		 */
		for (i = 0; i < SPANSARP_HASHSIZ; i++) {
			for (sap = spansarp_arptab[i]; sap;
						sap = sap->sa_next) {
				/*
				 * We only want entries learned
				 * from the supplied interface.
				 */
				if (sap->sa_cls != clp)
					continue;
				if ((dst != INADDR_ANY) &&
				    (dst != sap->sa_dstip.s_addr))
					continue;

				/*
				 * Make sure there's room in the user's buffer
				 */
				if (buf_len < sizeof(aar)) {
					err = ENOSPC;
					break;
				}

				/*
				 * Fill in info to be returned
				 */
				SATOSIN(&aar.aap_arp_addr)->sin_family =
					AF_INET;
				SATOSIN(&aar.aap_arp_addr)->sin_addr.s_addr =
					sap->sa_dstip.s_addr;
				(void) snprintf(aar.aap_intf,
				    sizeof(aar.aap_intf), "%s%d",
					clp->cls_ipnif->inf_nif->nif_if.if_name,
					clp->cls_ipnif->inf_nif->nif_if.if_unit
					);
				aar.aap_flags = sap->sa_flags;
				aar.aap_origin = sap->sa_origin;
				if (sap->sa_flags & SAF_VALID)
					aar.aap_age = SPANSARP_MAXAGE - 
							sap->sa_reftime;
				else
					aar.aap_age = 0;
				ATM_ADDR_COPY(&sap->sa_dstatm, &aar.aap_addr);
				ATM_ADDR_COPY(&sap->sa_dstatmsub,
					&aar.aap_subaddr);

				/*
				 * Copy the response into the user's buffer
				 */
				if ((err = copyout((caddr_t)&aar, buf_addr, 
							sizeof(aar))) != 0)
					break;
				buf_addr += sizeof(aar);
				buf_len -= sizeof(aar);
			}
			if (err)
				break;
		}

		/*
		 * Update the buffer pointer and length
		 */
		aip->air_buf_addr = buf_addr;
		aip->air_buf_len = buf_len;
		break;

	case AIOCS_INF_ASV:
		/*
		 * Get ARP server information
		 */
		/* SPANS doesn't have an ARP server */
		break;

	default:
		err = EOPNOTSUPP;
	}

	return (err);
}

