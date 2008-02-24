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
 * UNI ATMARP support (RFC1577)
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netatm/uni/uniarp.c,v 1.24 2007/06/23 00:02:20 mjacob Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
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

#include <vm/uma.h>

/*
 * Global variables
 */
struct uniarp		*uniarp_arptab[UNIARP_HASHSIZ] = {NULL};
struct uniarp		*uniarp_nomaptab = NULL;
struct uniarp		*uniarp_pvctab = NULL;
struct atm_time		uniarp_timer = {0, 0};		/* Aging timer */
struct uniarp_stat	uniarp_stat = {0};

/*
 * net.harp.uni.uniarp_print
 */
int			uniarp_print = 0;
SYSCTL_INT(_net_harp_uni, OID_AUTO, uniarp_print, CTLFLAG_RW,
    &uniarp_print, 0, "dump UNI/ARP messages");

Atm_endpoint	uniarp_endpt = {
	NULL,
	ENDPT_ATMARP,
	uniarp_ioctl,
	uniarp_getname,
	uniarp_connected,
	uniarp_cleared,
	NULL,
	NULL,
	NULL,
	NULL,
	uniarp_cpcs_data,
	NULL,
	NULL,
	NULL,
	NULL
};

uma_zone_t	uniarp_zone;


/*
 * Local variables
 */
static void	uniarp_server_mode(struct uniip *);
static void	uniarp_client_mode(struct uniip *, Atm_addr *);


/*
 * Process module loading notification
 * 
 * Called whenever the uni module is initializing.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0	initialization successful
 *	errno	initialization failed - reason indicated
 *
 */
int
uniarp_start()
{
	int	err;

	uniarp_zone = uma_zcreate("uni arp", sizeof(struct uniarp), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, 0);
	if (uniarp_zone == NULL)
		panic("uniarp_start: uma_zcreate");

	/*
	 * Register our endpoint
	 */
	err = atm_endpoint_register(&uniarp_endpt);
	return (err);
}


/*
 * Process module unloading notification
 * 
 * Called whenever the uni module is about to be unloaded.  All signalling
 * instances will have been previously detached.  All uniarp resources 
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
uniarp_stop()
{
	int	i;

	/* 
	 * Make sure the arp table is empty
	 */
	for (i = 0; i < UNIARP_HASHSIZ; i++) {
		if (uniarp_arptab[i] != NULL)
			panic("uniarp_stop: arp table not empty");
	}

	/*
	 * Cancel timers
	 */
	(void) atm_untimeout(&uniarp_timer);

	/*
	 * De-register ourselves
	 */
	(void) atm_endpoint_deregister(&uniarp_endpt);

	/*
	 * Free our storage pools
	 */
	uma_zdestroy(uniarp_zone);
}


/*
 * Process IP Network Interface Activation
 * 
 * Called whenever an IP network interface becomes active.
 *
 * Called at splnet.
 *
 * Arguments:
 *      uip     pointer to UNI IP interface
 *
 * Returns:
 *      none
 *
 */
void
uniarp_ipact(uip)
	struct uniip		*uip;
{
	struct unisig		*usp;

	ATM_DEBUG1("uniarp_ipact: uip=%p\n", uip);

	/*
	 * Set initial state
	 */
	uip->uip_arpstate = UIAS_NOTCONF;
	uip->uip_arpsvratm.address_format = T_ATM_ABSENT;
	uip->uip_arpsvratm.address_length = 0;
	uip->uip_arpsvrsub.address_format = T_ATM_ABSENT;
	uip->uip_arpsvrsub.address_length = 0;

	usp = (struct unisig *)uip->uip_ipnif->inf_nif->nif_pif->pif_siginst;
	if (usp->us_addr.address_format != T_ATM_ABSENT)
		uip->uip_flags |= UIF_IFADDR;

	/*
	 * Make sure aging timer is running
	 */
	if ((uniarp_timer.ti_flag & TIF_QUEUED) == 0)
		atm_timeout(&uniarp_timer, UNIARP_AGING, uniarp_aging);

	return;
}


/*
 * Process IP Network Interface Deactivation
 * 
 * Called whenever an IP network interface becomes inactive.  All VCCs
 * for this interface should already have been closed.
 *
 * Called at splnet.
 *
 * Arguments:
 *      uip     pointer to UNI IP interface
 *
 * Returns:
 *      none
 *
 */
void
uniarp_ipdact(uip)
	struct uniip		*uip;
{
	struct uniarp		*uap, *unext;
	int	i;

	ATM_DEBUG1("uniarp_ipdact: uip=%p\n", uip);

	/* 
	 * Delete all interface entries
	 */
	for (i = 0; i < UNIARP_HASHSIZ; i++) {
		for (uap = uniarp_arptab[i]; uap; uap = unext) {
			unext = uap->ua_next;

			if (uap->ua_intf != uip)
				continue;

			/*
			 * All VCCs should (better) be gone by now
			 */
			if (uap->ua_ivp)
				panic("uniarp_ipdact: entry not empty");

			/*
			 * Clean up any loose ends
			 */
			UNIARP_CANCEL(uap);

			/*
			 * Delete entry from arp table and free entry
			 */
			UNIARP_DELETE(uap);
			uma_zfree(uniarp_zone, uap);
		}
	}

	/*
	 * Clean up 'nomap' table
	 */
	for (uap = uniarp_nomaptab; uap; uap = unext) {
		unext = uap->ua_next;

		if (uap->ua_intf != uip)
			continue;

		/*
		 * All VCCs should (better) be gone by now
		 */
		if (uap->ua_ivp)
			panic("uniarp_ipdact: entry not empty");

		/*
		 * Clean up any loose ends
		 */
		UNIARP_CANCEL(uap);

		/*
		 * Delete entry from 'no map' table and free entry
		 */
		UNLINK(uap, struct uniarp, uniarp_nomaptab, ua_next);
		uma_zfree(uniarp_zone, uap);
	}

	/*
	 * Also clean up pvc table
	 */
	for (uap = uniarp_pvctab; uap; uap = unext) {
		unext = uap->ua_next;

		if (uap->ua_intf != uip)
			continue;

		/*
		 * All PVCs should (better) be gone by now
		 */
		panic("uniarp_ipdact: pvc table not empty");
	}

	/*
	 * Cancel arp interface timer
	 */
	UNIIP_ARP_CANCEL(uip);

	/*
	 * Stop aging timer if this is the last active interface
	 */
	if (uniip_head == uip && uip->uip_next == NULL)
		(void) atm_untimeout(&uniarp_timer);
}


/*
 * Process Interface ATM Address Change
 * 
 * This function is called whenever the ATM address for a physical
 * interface is set/changed.
 *
 * Called at splnet.
 *
 * Arguments:
 *      sip     pointer to interface's UNI signalling instance
 *
 * Returns:
 *	none
 *
 */
void
uniarp_ifaddr(sip)
	struct siginst		*sip;
{
	struct atm_nif		*nip;
	struct uniip		*uip;

	ATM_DEBUG1("uniarp_ifaddr: sip=%p\n", sip);

	/*
	 * We've got to handle this for every network interface
	 */
	for (nip = sip->si_pif->pif_nif; nip; nip = nip->nif_pnext) {

		/*
		 * Find our control blocks
		 */
		for (uip = uniip_head; uip; uip = uip->uip_next) {
			if (uip->uip_ipnif->inf_nif == nip)
				break;
		}
		if (uip == NULL)
			continue;

		/*
		 * We don't support changing prefix (yet)
		 */
		if (uip->uip_flags & UIF_IFADDR) {
			log(LOG_ERR, "uniarp_ifaddr: change not supported\n");
			continue;
		}

		/*
		 * Note that address has been set and figure out what
		 * to do next
		 */
		uip->uip_flags |= UIF_IFADDR;

		if (uip->uip_arpstate == UIAS_CLIENT_PADDR) {
			/*
			 * This is what we're waiting for
			 */
			uniarp_client_mode(uip, NULL);
		} else if (uip->uip_arpstate == UIAS_SERVER_ACTIVE) {
			/*
			 * Set new local arpserver atm address
			 */
			ATM_ADDR_SEL_COPY(&sip->si_addr, nip->nif_sel,
						&uip->uip_arpsvratm);
		}
	}

	return;
}


/*
 * Set ATMARP Server Mode
 * 
 * This function is called to configure the local node to become the 
 * ATMARP server for the specified LIS.
 *
 * Called at splnet.
 *
 * Arguments:
 *      uip     pointer to UNI IP interface
 *
 * Returns:
 *	none
 *
 */
static void
uniarp_server_mode(uip)
	struct uniip		*uip;
{
	struct ip_nif	*inp;
	struct atm_nif	*nip;
	struct siginst	*sgp;
	struct ipvcc	*ivp, *inext;
	struct uniarp	*uap, *unext;
	int		i;

	ATM_DEBUG1("uniarp_server_mode: uip=%p\n", uip);

	/*
	 * Handle client/server mode changes first
	 */
	switch (uip->uip_arpstate) {

	case UIAS_NOTCONF:
	case UIAS_SERVER_ACTIVE:
	case UIAS_CLIENT_PADDR:
		/*
		 * Nothing to undo
		 */
		break;

	case UIAS_CLIENT_POPEN:
		/*
		 * We're becoming the server, so kill the pending connection
		 */
		UNIIP_ARP_CANCEL(uip);
		if ((ivp = uip->uip_arpsvrvcc) != NULL) {
			ivp->iv_flags &= ~IVF_NOIDLE;
			uip->uip_arpsvrvcc = NULL;
			(*ivp->iv_ipnif->inf_arpnotify)(ivp, MAP_FAILED);
		}
		break;

	case UIAS_CLIENT_REGISTER:
	case UIAS_CLIENT_ACTIVE:
		/*
		 * We're becoming the server, but leave existing VCC as a
		 * "normal" IP VCC
		 */
		UNIIP_ARP_CANCEL(uip);
		ivp = uip->uip_arpsvrvcc;
		ivp->iv_flags &= ~IVF_NOIDLE;
		uip->uip_arpsvrvcc = NULL;
		break;
	}

	/*
	 * Revalidate status for all arp entries on this interface
	 */
	for (i = 0; i < UNIARP_HASHSIZ; i++) {
		for (uap = uniarp_arptab[i]; uap; uap = unext) {
			unext = uap->ua_next;

			if (uap->ua_intf != uip)
				continue;

			if (uap->ua_origin >= UAO_PERM)
				continue;

			if (uap->ua_origin >= UAO_SCSP) {
				if (uniarp_validate_ip(uip, &uap->ua_dstip,
							uap->ua_origin) == 0)
					continue;
			}

			if (uap->ua_ivp == NULL) {
				UNIARP_CANCEL(uap);
				UNIARP_DELETE(uap);
				uma_zfree(uniarp_zone, uap);
				continue;
			}

			if (uap->ua_flags & UAF_VALID) {
				uap->ua_flags |= UAF_LOCKED;
				for (ivp = uap->ua_ivp; ivp; ivp = inext) {
					inext = ivp->iv_arpnext;
					(*ivp->iv_ipnif->inf_arpnotify)
							(ivp, MAP_INVALID);
				}
				uap->ua_flags &= ~(UAF_LOCKED | UAF_VALID);
			}
			uap->ua_aging = 1;
			uap->ua_origin = 0;
		}
	}

	/*
	 * OK, now let's make ourselves the server
	 */
	inp = uip->uip_ipnif;
	nip = inp->inf_nif;
	sgp = nip->nif_pif->pif_siginst;
	ATM_ADDR_SEL_COPY(&sgp->si_addr, nip->nif_sel, &uip->uip_arpsvratm);
	uip->uip_arpsvrip = IA_SIN(inp->inf_addr)->sin_addr;
	uip->uip_arpstate = UIAS_SERVER_ACTIVE;
	return;
}


/*
 * Set ATMARP Client Mode
 * 
 * This function is called to configure the local node to be an ATMARP 
 * client on the specified LIS using the specified ATMARP server.
 *
 * Called at splnet.
 *
 * Arguments:
 *      uip     pointer to UNI IP interface
 *      aap     pointer to the ATMARP server's ATM address
 *
 * Returns:
 *	none
 *
 */
static void
uniarp_client_mode(uip, aap)
	struct uniip		*uip;
	Atm_addr		*aap;
{
	struct ip_nif		*inp = uip->uip_ipnif;
	struct uniarp		*uap, *unext;
	struct ipvcc		*ivp, *inext;
	int			i;

	ATM_DEBUG2("uniarp_client_mode: uip=%p, atm=(%s,-)\n",
		uip, aap ? unisig_addr_print(aap): "-");

	/*
	 * Handle client/server mode changes first
	 */
	switch (uip->uip_arpstate) {

	case UIAS_NOTCONF:
	case UIAS_CLIENT_PADDR:
		/*
		 * Nothing to undo
		 */
		break;

	case UIAS_CLIENT_POPEN:
		/*
		 * If this is this a timeout retry, just go do it
		 */
		if (aap == NULL)
			break;

		/*
		 * If this isn't really a different arpserver, we're done
		 */
		if (ATM_ADDR_EQUAL(aap, &uip->uip_arpsvratm))
			return;

		/*
		 * We're changing servers, so kill the pending connection
		 */
		UNIIP_ARP_CANCEL(uip);
		if ((ivp = uip->uip_arpsvrvcc) != NULL) {
			ivp->iv_flags &= ~IVF_NOIDLE;
			uip->uip_arpsvrvcc = NULL;
			(*ivp->iv_ipnif->inf_arpnotify)(ivp, MAP_FAILED);
		}
		break;

	case UIAS_CLIENT_REGISTER:
	case UIAS_CLIENT_ACTIVE:
		/*
		 * If this isn't really a different arpserver, we're done
		 */
		if (ATM_ADDR_EQUAL(aap, &uip->uip_arpsvratm))
			return;

		/*
		 * We're changing servers, but leave existing VCC as a
		 * "normal" IP VCC
		 */
		UNIIP_ARP_CANCEL(uip);
		ivp = uip->uip_arpsvrvcc;
		ivp->iv_flags &= ~IVF_NOIDLE;
		uip->uip_arpsvrvcc = NULL;
		break;

	case UIAS_SERVER_ACTIVE:
		/*
		 * We're changing from server mode, so...
		 *
		 * Reset valid/authoritative status for all arp entries
		 * on this interface
		 */
		for (i = 0; i < UNIARP_HASHSIZ; i++) {
			for (uap = uniarp_arptab[i]; uap; uap = unext) {
				unext = uap->ua_next;

				if (uap->ua_intf != uip)
					continue;

				if (uap->ua_origin >= UAO_PERM)
					continue;

				if (uap->ua_ivp == NULL) {
					UNIARP_CANCEL(uap);
					UNIARP_DELETE(uap);
					uma_zfree(uniarp_zone, uap);
					continue;
				}

				if (uap->ua_flags & UAF_VALID) {
					uap->ua_flags |= UAF_LOCKED;
					for (ivp = uap->ua_ivp; ivp;
								ivp = inext) {
						inext = ivp->iv_arpnext;
						(*ivp->iv_ipnif->inf_arpnotify)
							(ivp, MAP_INVALID);
					}
					uap->ua_flags &=
						~(UAF_LOCKED | UAF_VALID);
				}
				uap->ua_aging = 1;
				uap->ua_origin = 0;
			}
		}
		uip->uip_arpsvratm.address_format = T_ATM_ABSENT;
		uip->uip_arpsvratm.address_length = 0;
		uip->uip_arpsvrsub.address_format = T_ATM_ABSENT;
		uip->uip_arpsvrsub.address_length = 0;
		uip->uip_arpsvrip.s_addr = 0;
		break;
	}

	/*
	 * Save the arp server address, if supplied now
	 */
	if (aap)
		ATM_ADDR_COPY(aap, &uip->uip_arpsvratm);

	/*
	 * If the interface's ATM address isn't set yet, then we
	 * can't do much until it is
	 */
	if ((uip->uip_flags & UIF_IFADDR) == 0) {
		uip->uip_arpstate = UIAS_CLIENT_PADDR;
		return;
	}

	/*
	 * Just to keep things simple, if we already have (or are trying to
	 * setup) any SVCs to our new server, kill the connections so we can
	 * open a "fresh" SVC for the arpserver connection.
	 */
	for (i = 0; i < UNIARP_HASHSIZ; i++) {
		for (uap = uniarp_arptab[i]; uap; uap = unext) {
			unext = uap->ua_next;

			if (ATM_ADDR_EQUAL(&uip->uip_arpsvratm,
							&uap->ua_dstatm) &&
			    ATM_ADDR_EQUAL(&uip->uip_arpsvrsub,
							&uap->ua_dstatmsub)) {
				uap->ua_flags &= ~UAF_VALID;
				for (ivp = uap->ua_ivp; ivp; ivp = inext) {
					inext = ivp->iv_arpnext;
					(*inp->inf_arpnotify)(ivp, MAP_FAILED);
				}
			}
		}
	}
	for (uap = uniarp_nomaptab; uap; uap = unext) {
		unext = uap->ua_next;

		if (ATM_ADDR_EQUAL(&uip->uip_arpsvratm, &uap->ua_dstatm) &&
		    ATM_ADDR_EQUAL(&uip->uip_arpsvrsub, &uap->ua_dstatmsub)) {
			uap->ua_flags &= ~UAF_VALID;
			for (ivp = uap->ua_ivp; ivp; ivp = inext) {
				inext = ivp->iv_arpnext;
				(*inp->inf_arpnotify)(ivp, MAP_FAILED);
			}
		}
	}

	/*
	 * Now, get an arp entry for the server connection
	 * May be called from timeout - don't wait.
	 */
	uip->uip_arpstate = UIAS_CLIENT_POPEN;
	uap = uma_zalloc(uniarp_zone, M_NOWAIT | M_ZERO);
	if (uap == NULL) {
		UNIIP_ARP_TIMER(uip, 1 * ATM_HZ);
		return;
	}

	/*
	 * Next, initiate an SVC to the server
	 */
	if ((*inp->inf_createsvc)(ANIF2IFP(inp->inf_nif), AF_ATM,
			(caddr_t)&uip->uip_arpsvratm, &ivp)) {
		uma_zfree(uniarp_zone, uap);
		UNIIP_ARP_TIMER(uip, 1 * ATM_HZ);
		return;
	}

	/*
	 * Finally, get everything set up and wait for the SVC
	 * connection to complete
	 */
	uip->uip_arpsvrvcc = ivp;
	ivp->iv_flags |= IVF_NOIDLE;

	ATM_ADDR_COPY(&uip->uip_arpsvratm, &uap->ua_dstatm);
	ATM_ADDR_COPY(&uip->uip_arpsvrsub, &uap->ua_dstatmsub);
	uap->ua_intf = uip;

	LINK2TAIL(ivp, struct ipvcc, uap->ua_ivp, iv_arpnext);
	ivp->iv_arpent = (struct arpmap *)uap;

	LINK2TAIL(uap, struct uniarp, uniarp_nomaptab, ua_next);

	return;
}


/*
 * Process a UNI ARP interface timeout
 * 
 * Called when a previously scheduled uniip arp interface timer expires.
 * Processing will be based on the current uniip arp state.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tip	pointer to uniip arp timer control block
 *
 * Returns:
 *	none
 *
 */
void
uniarp_iftimeout(tip)
	struct atm_time	*tip;
{
	struct ip_nif	*inp;
	struct uniip	*uip;


	/*
	 * Back-off to uniip control block
	 */
	uip = (struct uniip *)
		((caddr_t)tip - offsetof(struct uniip, uip_arptime));

	ATM_DEBUG2("uniarp_iftimeout: uip=%p, state=%d\n", uip, 
		uip->uip_arpstate);

	/*
	 * Process timeout based on protocol state
	 */
	switch (uip->uip_arpstate) {

	case UIAS_CLIENT_POPEN:
		/*
		 * Retry opening arp server connection
		 */
		uniarp_client_mode(uip, NULL);
		break;

	case UIAS_CLIENT_REGISTER:
		/*
		 * Resend registration request
		 */
		inp = uip->uip_ipnif;
		(void) uniarp_arp_req(uip, &(IA_SIN(inp->inf_addr)->sin_addr));

		/*
		 * Restart timer
		 */
		UNIIP_ARP_TIMER(uip, 2 * ATM_HZ);

		break;

	case UIAS_CLIENT_ACTIVE:
		/*
		 * Refresh our registration
		 */
		inp = uip->uip_ipnif;
		(void) uniarp_arp_req(uip, &(IA_SIN(inp->inf_addr)->sin_addr));

		/*
		 * Restart timer
		 */
		UNIIP_ARP_TIMER(uip, UNIARP_REGIS_RETRY);

		break;

	default:
		log(LOG_ERR, "uniarp_iftimeout: invalid state %d\n",
			uip->uip_arpstate);
	}
}


/*
 * UNI ARP IOCTL support
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
uniarp_ioctl(code, data, arg1)
        int		code;
        caddr_t		data;
        caddr_t		arg1;
{
	struct atmaddreq	*aap;
	struct atmdelreq	*adp;
	struct atmsetreq	*asp;
	struct atminfreq	*aip;
	struct air_arp_rsp	aar;
	struct air_asrv_rsp	asr;
	struct atm_pif		*pip;
	struct atm_nif		*nip;
	struct ipvcc		*ivp, *inext;
	struct uniip		*uip;
	struct uniarp		*uap;
	struct unisig		*usp;
	struct in_addr		ip;
	Atm_addr		atmsub;
	u_long			dst;
	int			err = 0;
	size_t			buf_len, tlen;
	u_int			i;
	caddr_t			buf_addr;

	switch (code) {

	case AIOCS_ADD_ARP:
		/*
		 * Add a permanent ARP mapping
		 */
		aap = (struct atmaddreq *)data;
		uip = (struct uniip *)arg1;
		if (aap->aar_arp_addr.address_format != T_ATM_ENDSYS_ADDR) {
			err = EINVAL;
			break;
		}
		atmsub.address_format = T_ATM_ABSENT;
		atmsub.address_length = 0;
		ip = SATOSIN(&aap->aar_arp_dst)->sin_addr;

		/*
		 * Validate IP address
		 */
		if (uniarp_validate_ip(uip, &ip, aap->aar_arp_origin) != 0) {
			err = EADDRNOTAVAIL;
			break;
		}

		/*
		 * Add an entry to the cache
		 */
		err = uniarp_cache_svc(uip, &ip, &aap->aar_arp_addr,
				&atmsub, aap->aar_arp_origin);
		break;

	case AIOCS_DEL_ARP:
		/*
		 * Delete an ARP mapping
		 */
		adp = (struct atmdelreq *)data;
		uip = (struct uniip *)arg1;
		ip = SATOSIN(&adp->adr_arp_dst)->sin_addr;

		/*
		 * Now find the entry to be deleted
		 */
		UNIARP_LOOKUP(ip.s_addr, uap);
		if (uap == NULL) {
			err = ENOENT;
			break;
		}

		/*
		 * Notify all VCCs using this entry that they must finish
		 * up now.  
		 */
		uap->ua_flags |= UAF_LOCKED;
		for (ivp = uap->ua_ivp; ivp; ivp = inext) {
			inext = ivp->iv_arpnext;
			(*ivp->iv_ipnif->inf_arpnotify)(ivp, MAP_FAILED);
		}

		/*
		 * Now free up the entry
		 */
		UNIARP_CANCEL(uap);
		UNIARP_DELETE(uap);
		uma_zfree(uniarp_zone, uap);
		break;

	case AIOCS_SET_ASV:
		/*
		 * Set interface ARP server address
		 */
		asp = (struct atmsetreq *)data;
		for (uip = uniip_head; uip; uip = uip->uip_next) {
			if (uip->uip_ipnif->inf_nif == (struct atm_nif *)arg1)
				break;
		}
		if (uip == NULL) {
			err = ENOPROTOOPT;
			break;
		}

		/*
		 * Check for our own address
		 */
		usp = (struct unisig *)
				uip->uip_ipnif->inf_nif->nif_pif->pif_siginst;
		if (ATM_ADDR_EQUAL(&asp->asr_arp_addr, &usp->us_addr)) {
			asp->asr_arp_addr.address_format = T_ATM_ABSENT;
		}

		/*
		 * If we're going into server mode, make sure we can get 
		 * the memory for the prefix list before continuing
		 */
		if (asp->asr_arp_addr.address_format == T_ATM_ABSENT) {
			i = asp->asr_arp_plen / sizeof(struct uniarp_prf);
			if (i == 0) {
				err = EINVAL;
				break;
			}
			buf_len = i * sizeof(struct uniarp_prf);
			buf_addr = malloc(buf_len, M_DEVBUF, M_NOWAIT);
			if (buf_addr == NULL) {
				err = ENOMEM;
				break;
			}
			err = copyin(asp->asr_arp_pbuf, buf_addr, buf_len);
			if (err) {
				free(buf_addr, M_DEVBUF);
				break;
			}
		} else {
			/* Silence the compiler */
			i = 0;
			buf_addr = NULL;
		}

		/*
		 * Free any existing prefix address list
		 */
		if (uip->uip_prefix != NULL) {
			free(uip->uip_prefix, M_DEVBUF);
			uip->uip_prefix = NULL;
			uip->uip_nprefix = 0;
		}

		if (asp->asr_arp_addr.address_format == T_ATM_ABSENT) {
			/*
			 * Set ATMARP server mode
			 */
			uip->uip_prefix = (struct uniarp_prf *)buf_addr;
			uip->uip_nprefix = i;
			uniarp_server_mode(uip);
		} else
			/*
			 * Set ATMARP client mode
			 */
			uniarp_client_mode(uip, &asp->asr_arp_addr);
		break;

	case AIOCS_INF_ARP:
		/*
		 * Get ARP table information
		 */
		aip = (struct atminfreq *)data;

		if (aip->air_arp_addr.sa_family != AF_INET)
			break;
		dst = SATOSIN(&aip->air_arp_addr)->sin_addr.s_addr;

		buf_addr = aip->air_buf_addr;
		buf_len = aip->air_buf_len;

		pip = ((struct siginst *)arg1)->si_pif;

		/*
		 * Run through entire arp table
		 */
		for (i = 0; i < UNIARP_HASHSIZ; i++) {
			for (uap = uniarp_arptab[i]; uap; uap = uap->ua_next) {
				/*
				 * We only want valid entries learned
				 * from the supplied interface.
				 */
				nip = uap->ua_intf->uip_ipnif->inf_nif;
				if (nip->nif_pif != pip)
					continue;
				if ((dst != INADDR_ANY) &&
				    (dst != uap->ua_dstip.s_addr))
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
					uap->ua_dstip.s_addr;
				strlcpy(aar.aap_intf, ANIF2IFP(nip)->if_xname,
				    sizeof(aar.aap_intf));
				aar.aap_flags = uap->ua_flags;
				aar.aap_origin = uap->ua_origin;
				if (uap->ua_flags & UAF_VALID)
					aar.aap_age = uap->ua_aging + 
					    uap->ua_retry * UNIARP_RETRY_AGE;
				else
					aar.aap_age = 0;
				ATM_ADDR_COPY(&uap->ua_dstatm, &aar.aap_addr);
				ATM_ADDR_COPY(&uap->ua_dstatmsub,
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
		 * Now go through the 'nomap' table
		 */
		if (err || (dst != INADDR_ANY))
			goto updbuf;
		for (uap = uniarp_nomaptab; uap; uap = uap->ua_next) {
			/*
			 * We only want valid entries learned
			 * from the supplied interface.
			 */
			nip = uap->ua_intf->uip_ipnif->inf_nif;
			if (nip->nif_pif != pip)
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
			SATOSIN(&aar.aap_arp_addr)->sin_family = AF_INET;
			SATOSIN(&aar.aap_arp_addr)->sin_addr.s_addr = 0;
			strlcpy(aar.aap_intf, ANIF2IFP(nip)->if_xname,
				sizeof(aar.aap_intf));
			aar.aap_flags = 0;
			aar.aap_origin = uap->ua_origin;
			aar.aap_age = 0;
			ATM_ADDR_COPY(&uap->ua_dstatm, &aar.aap_addr);
			ATM_ADDR_COPY(&uap->ua_dstatmsub,
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

updbuf:
		/*
		 * Update the buffer pointer and length
		 */
		aip->air_buf_addr = buf_addr;
		aip->air_buf_len = buf_len;

		/*
		 * If the user wants the refresh status reset and no 
		 * errors have been encountered, then do the reset
		 */
		if ((err == 0) && (aip->air_arp_flags & ARP_RESET_REF)) {
			for (i = 0; i < UNIARP_HASHSIZ; i++) {
				for (uap = uniarp_arptab[i]; uap; 
							uap = uap->ua_next) {
					/*
					 * We only want valid entries learned
					 * from the supplied interface.
					 */
					nip = uap->ua_intf->uip_ipnif->inf_nif;
					if (nip->nif_pif != pip)
						continue;
					if ((dst != INADDR_ANY) &&
					    (dst != uap->ua_dstip.s_addr))
						continue;

					/*
					 * Reset refresh flag
					 */
					uap->ua_flags &= ~UAF_REFRESH;
				}
			}
		}
		break;

	case AIOCS_INF_ASV:
		/*
		 * Get ARP server information
		 */
		aip = (struct atminfreq *)data;
		nip = (struct atm_nif *)arg1;

		buf_addr = aip->air_buf_addr;
		buf_len = aip->air_buf_len;

		for (uip = uniip_head; uip; uip = uip->uip_next) {

			if (uip->uip_ipnif->inf_nif != nip)
				continue;

			/*
			 * Make sure there's room in the user's buffer
			 */
			if (buf_len < sizeof(asr)) {
				err = ENOSPC;
				break;
			}

			/*
			 * Fill in info to be returned
			 */
			strlcpy(asr.asp_intf, ANIF2IFP(nip)->if_xname,
				sizeof(asr.asp_intf));
			asr.asp_state = uip->uip_arpstate;
			if (uip->uip_arpstate == UIAS_SERVER_ACTIVE) {
				asr.asp_addr.address_format = T_ATM_ABSENT;
				asr.asp_addr.address_length = 0;
			} else {
				ATM_ADDR_COPY(&uip->uip_arpsvratm,
						&asr.asp_addr);
			}
			asr.asp_subaddr.address_format = T_ATM_ABSENT;
			asr.asp_subaddr.address_length = 0;
			asr.asp_nprefix = uip->uip_nprefix;

			/*
			 * Copy the response into the user's buffer
			 */
			if ((err = copyout((caddr_t)&asr, buf_addr, sizeof(asr))) != 0)
				break;
			buf_addr += sizeof(asr);
			buf_len -= sizeof(asr);

			/*
			 * Copy the prefix list into the user's buffer
			 */
			if (uip->uip_nprefix) {
				tlen = uip->uip_nprefix *
				    sizeof(struct uniarp_prf);
				if (buf_len < tlen) {
					err = ENOSPC;
					break;
				}
				err = copyout(uip->uip_prefix, buf_addr, tlen);
				if (err != 0)
					break;
				buf_addr += tlen;
				buf_len -= tlen;
			}
		}

		/*
		 * Update the buffer pointer and length
		 */
		aip->air_buf_addr = buf_addr;
		aip->air_buf_len = buf_len;
		break;

	default:
		err = EOPNOTSUPP;
	}

	return (err);
}


/*
 * Get Connection's Application/Owner Name
 * 
 * Arguments:
 *	tok	uniarp connection token (pointer to ipvcc)
 *
 * Returns:
 *	addr	pointer to string containing our name
 *
 */
caddr_t
uniarp_getname(tok)
	void		*tok;
{
	return ("ATMARP");
}

