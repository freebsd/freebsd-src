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
 * IP Over ATM Support
 * -------------------
 *
 * Virtual Channel Manager
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <machine/clock.h>
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
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <netatm/ipatm/ipatm.h>
#include <netatm/ipatm/ipatm_var.h>
#include <netatm/ipatm/ipatm_serv.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


Atm_attributes	ipatm_aal5llc = {
	NULL,			/* nif */
	CMAPI_CPCS,		/* api */
	0,			/* api_init */
	0,			/* headin */
	0,			/* headout */
	{			/* aal */
		T_ATM_PRESENT,
		ATM_AAL5
	},
	{			/* traffic */
		T_ATM_PRESENT,
		{
			{
				T_ATM_ABSENT,
				0,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_NO
			},
			{
				T_ATM_ABSENT,
				0,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_NO
			},
			T_YES
		},
	},
	{			/* bearer */
		T_ATM_PRESENT,
		{
			T_ATM_CLASS_X,
			T_ATM_NULL,
			T_ATM_NULL,
			T_NO,
			T_ATM_1_TO_1
		}
	},
	{			/* bhli */
		T_ATM_ABSENT
	},
	{			/* blli */
		T_ATM_PRESENT,
		T_ATM_ABSENT,
		{
			{
				T_ATM_SIMPLE_ID,
			},
			{
				T_ATM_ABSENT
			}
		}
	},
	{			/* llc */
		T_ATM_PRESENT,
		{
			T_ATM_LLC_SHARING,
			IPATM_LLC_LEN,
			IPATM_LLC_HDR
		}
	},
	{			/* called */
		T_ATM_PRESENT,
	},
	{			/* calling */
		T_ATM_ABSENT
	},
	{			/* qos */
		T_ATM_PRESENT,
		{
			T_ATM_NETWORK_CODING,
			{
				T_ATM_QOS_CLASS_0,
			},
			{
				T_ATM_QOS_CLASS_0
			}
		}
	},
	{			/* transit */
		T_ATM_ABSENT
	},
	{			/* cause */
		T_ATM_ABSENT
	}
};

Atm_attributes	ipatm_aal5null = {
	NULL,			/* nif */
	CMAPI_CPCS,		/* api */
	0,			/* api_init */
	sizeof(struct ifnet *),	/* headin */
	0,			/* headout */
	{			/* aal */
		T_ATM_PRESENT,
		ATM_AAL5
	},
	{			/* traffic */
		T_ATM_PRESENT,
		{
			{
				T_ATM_ABSENT,
				0,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_NO
			},
			{
				T_ATM_ABSENT,
				0,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_NO
			},
			T_YES
		},
	},
	{			/* bearer */
		T_ATM_PRESENT,
		{
			T_ATM_CLASS_X,
			T_ATM_NULL,
			T_ATM_NULL,
			T_NO,
			T_ATM_1_TO_1
		}
	},
	{			/* bhli */
		T_ATM_ABSENT
	},
	{			/* blli */
		T_ATM_ABSENT,
		T_ATM_ABSENT
	},
	{			/* llc */
		T_ATM_ABSENT
	},
	{			/* called */
		T_ATM_PRESENT,
	},
	{			/* calling */
		T_ATM_ABSENT
	},
	{			/* qos */
		T_ATM_PRESENT,
		{
			T_ATM_NETWORK_CODING,
			{
				T_ATM_QOS_CLASS_0,
			},
			{
				T_ATM_QOS_CLASS_0
			}
		}
	},
	{			/* transit */
		T_ATM_ABSENT
	},
	{			/* cause */
		T_ATM_ABSENT
	}
};

Atm_attributes	ipatm_aal4null = {
	NULL,			/* nif */
	CMAPI_CPCS,		/* api */
	0,			/* api_init */
	sizeof(struct ifnet *),	/* headin */
	0,			/* headout */
	{			/* aal */
		T_ATM_PRESENT,
		ATM_AAL3_4
	},
	{			/* traffic */
		T_ATM_PRESENT,
		{
			{
				T_ATM_ABSENT,
				0,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_NO
			},
			{
				T_ATM_ABSENT,
				0,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_ATM_ABSENT,
				T_NO
			},
			T_YES
		},
	},
	{			/* bearer */
		T_ATM_PRESENT,
		{
			T_ATM_CLASS_X,
			T_ATM_NULL,
			T_ATM_NULL,
			T_NO,
			T_ATM_1_TO_1
		}
	},
	{			/* bhli */
		T_ATM_ABSENT
	},
	{			/* blli */
		T_ATM_ABSENT,
		T_ATM_ABSENT
	},
	{			/* llc */
		T_ATM_ABSENT
	},
	{			/* called */
		T_ATM_PRESENT,
	},
	{			/* calling */
		T_ATM_ABSENT
	},
	{			/* qos */
		T_ATM_PRESENT,
		{
			T_ATM_NETWORK_CODING,
			{
				T_ATM_QOS_CLASS_0,
			},
			{
				T_ATM_QOS_CLASS_0
			}
		}
	},
	{			/* transit */
		T_ATM_ABSENT
	},
	{			/* cause */
		T_ATM_ABSENT
	}
};

static struct t_atm_cause	ipatm_cause = {
	T_ATM_ITU_CODING,
	T_ATM_LOC_USER,
	0,
	{0, 0, 0, 0}
};


/*
 * Open an IP PVC
 * 
 * This function will perform all actions necessary to activate a
 * PVC for IP usage.  In particular, it will allocate control blocks, 
 * open the PVC, initialize PVC stack, and initiate whatever ARP
 * procedures are required.
 *
 * Arguments:
 *	pvp	pointer to PVC parameter structure
 *	sivp	address to return pointer to IP PVC control block
 *
 * Returns:
 *	0 	PVC was successfully opened
 *	errno	open failed - reason indicated
 *
 */
int
ipatm_openpvc(pvp, sivp)
	struct ipatmpvc	*pvp;
	struct ipvcc	**sivp;
{
	struct ipvcc	*ivp;
	Atm_attributes	*ap;
	Atm_addr_pvc	*pvcp;
	struct atm_nif	*nip;
	struct ip_nif	*inp;
	int	s, err = 0;

	inp = pvp->ipp_ipnif;
	nip = inp->inf_nif;

	/*
	 * Make sure interface is ready to go
	 */
	if (inp->inf_state != IPNIF_ACTIVE) {
		err = ENETDOWN;
		goto done;
	}

	/*
	 * Validate fixed destination IP address
	 */
	if (pvp->ipp_dst.sin_addr.s_addr != INADDR_ANY) {
#if (defined(BSD) && (BSD >= 199306))
		if (in_broadcast(pvp->ipp_dst.sin_addr, &nip->nif_if) ||
#else
		if (in_broadcast(pvp->ipp_dst.sin_addr) ||
#endif
		    IN_MULTICAST(ntohl(pvp->ipp_dst.sin_addr.s_addr)) ||
		    ipatm_chknif(pvp->ipp_dst.sin_addr, inp)) {
			err = EINVAL;
			goto done;
		}
	}

	/*
	 * Allocate IP VCC block
	 */
	ivp = (struct ipvcc *)atm_allocate(&ipatm_vcpool);
	if (ivp == NULL) {
		err = ENOMEM;
		goto done;
	}

	/*
	 * Initialize the PVC
	 */
	ivp->iv_flags = IVF_PVC;
	if (pvp->ipp_encaps == ATM_ENC_LLC)
		ivp->iv_flags |= IVF_LLC;

	/*
	 * Fill out connection attributes
	 */
	if (pvp->ipp_aal == ATM_AAL5) {
		if (pvp->ipp_encaps == ATM_ENC_LLC)
			ap = &ipatm_aal5llc;
		else
			ap = &ipatm_aal5null;
	} else {
		ap = &ipatm_aal4null;
	}

	ap->nif = nip;
	ap->traffic.v.forward.PCR_all_traffic = nip->nif_pif->pif_pcr;
	ap->traffic.v.backward.PCR_all_traffic = nip->nif_pif->pif_pcr;
	ap->called.addr.address_format = T_ATM_PVC_ADDR;
	ap->called.addr.address_length = sizeof(Atm_addr_pvc);
	pvcp = (Atm_addr_pvc *)ap->called.addr.address;
	ATM_PVC_SET_VPI(pvcp, pvp->ipp_vpi);
	ATM_PVC_SET_VCI(pvcp, pvp->ipp_vci);
	ap->called.subaddr.address_format = T_ATM_ABSENT;
	ap->called.subaddr.address_length = 0;

	/*
	 * Create PVC
	 */
	err = atm_cm_connect(&ipatm_endpt, ivp, ap, &ivp->iv_conn);
	if (err) {
		atm_free((caddr_t)ivp);
		goto done;
	}

	/*
	 * Save PVC information and link in VCC
	 */
	/* ivp->iv_ = ap->headout; */

	/*
	 * Queue VCC onto its network interface
	 */
	s = splnet();
	ipatm_vccnt++;
	ENQUEUE(ivp, struct ipvcc, iv_elem, inp->inf_vcq);
	ivp->iv_ipnif = inp;
	(void) splx(s);

	/*
	 * Set destination IP address and IPVCC state
	 */
	if (pvp->ipp_dst.sin_addr.s_addr == INADDR_ANY) {
		/*
		 * Initiate ARP processing
		 */
		switch ((*inp->inf_serv->is_arp_pvcopen)(ivp)) {

		case MAP_PROCEEDING:
			/*
			 * Wait for answer
			 */
			ivp->iv_state = IPVCC_ACTIVE;
			break;

		case MAP_VALID:
			/*
			 * We've got our answer already
			 */
			ivp->iv_state = IPVCC_ACTIVE;
			ivp->iv_flags |= IVF_MAPOK;
			ivp->iv_dst.s_addr = ivp->iv_arpent->am_dstip.s_addr;
			break;

		case MAP_FAILED:
			/*
			 * Try again later
			 */
			ivp->iv_state = IPVCC_ACTPENT;
			IPVCC_TIMER(ivp, 1 * ATM_HZ);
			break;

		default:
			panic("ipatm_openpvc: invalid arp_pvcopen return");
		}

	} else {
		/*
		 * Use configured IP destination
		 */
		ivp->iv_dst.s_addr = pvp->ipp_dst.sin_addr.s_addr;
		ivp->iv_state = IPVCC_ACTIVE;
		ivp->iv_flags |= IVF_MAPOK;
	}

done:
	if (err)
		*sivp = NULL;
	else
		*sivp = ivp;
	return (err);
}


/*
 * Create an IP SVC
 * 
 * This function will initiate the creation of an IP SVC.  The IP VCC
 * control block will be initialized and, if required, we will initiate
 * ARP processing in order to resolve the destination's ATM address.  Once
 * the destination ATM address is known, ipatm_opensvc() will be called.
 *
 * Arguments:
 *	ifp	pointer to destination ifnet structure
 *	daf	destination address family type
 *	dst	pointer to destination address
 *	sivp	address to return pointer to IP SVC control block
 *
 * Returns:
 *	0 	SVC creation was successfully initiated
 *	errno	creation failed - reason indicated
 *
 */
int
ipatm_createsvc(ifp, daf, dst, sivp)
	struct ifnet		*ifp;
	u_short			daf;
	caddr_t			dst;
	struct ipvcc		**sivp;
{
	struct atm_nif	*nip = (struct atm_nif *)ifp;
	struct ip_nif	*inp;
	struct ipvcc	*ivp;
	struct in_addr	*ip;
	Atm_addr	*atm;
	int	s, err = 0;

	/*
	 * Get IP interface and make sure its ready
	 */
	for (inp = ipatm_nif_head; inp; inp = inp->inf_next) {
		if (inp->inf_nif == nip)
			break;
	}
	if (inp == NULL) {
		err = ENXIO;
		goto done;
	}
	if (inp->inf_state != IPNIF_ACTIVE) {
		err = ENETDOWN;
		goto done;
	}

	/*
	 * Validate destination address
	 */
	if (daf == AF_INET) {
		/*
		 * Destination is IP address
		 */
		ip = (struct in_addr *)dst;
		atm = NULL;
		if (ip->s_addr == INADDR_ANY) {
			err = EADDRNOTAVAIL;
			goto done;
		}
	} else if (daf == AF_ATM) {
		/*
		 * Destination is ATM address
		 */
		atm = (Atm_addr *)dst;
		ip = NULL;
		if (atm->address_format == T_ATM_ABSENT) {
			err = EINVAL;
			goto done;
		}
	} else {
		err = EINVAL;
		goto done;
	}

	/*
	 * Make sure we have services provider and ARP support
	 */
	if ((inp->inf_serv == NULL) ||
	    (inp->inf_serv->is_arp_svcout == NULL)) {
		err = ENETDOWN;
		goto done;
	}

	/*
	 * Allocate IP VCC
	 */
	ivp = (struct ipvcc *)atm_allocate(&ipatm_vcpool);
	if (ivp == NULL) {
		err = ENOMEM;
		goto done;
	}

	/*
	 * Initialize SVC
	 */
	ivp->iv_flags = IVF_SVC;
	ivp->iv_ipnif = inp;

	/*
	 * Get destination ATM address
	 */
	if (daf == AF_INET) {
		/*
		 * ARP is the way...
		 */
		ivp->iv_dst.s_addr = ip->s_addr;

		switch ((*inp->inf_serv->is_arp_svcout)(ivp, ip)) {

		case MAP_PROCEEDING:
			/*
			 * Wait for answer
			 */
			ivp->iv_state = IPVCC_PMAP;
			IPVCC_TIMER(ivp, IPATM_ARP_TIME);
			break;

		case MAP_VALID:
			/*
			 * We've got our answer already, so open SVC
			 */
			ivp->iv_flags |= IVF_MAPOK;
			err = ipatm_opensvc(ivp);
			if (err) {
				(*inp->inf_serv->is_arp_close)(ivp);
				atm_free((caddr_t)ivp);
				goto done;
			}
			break;

		case MAP_FAILED:
			/*
			 * So sorry...come again
			 */
			atm_free((caddr_t)ivp);
			err = ENETDOWN;
			goto done;

		default:
			panic("ipatm_createsvc: invalid arp_svcout return");
		}
	} else {
		/*
		 * We were given the ATM address, so open the SVC
		 *
		 * Create temporary arp map entry so that opensvc() works.
		 * Caller must set up a permanent entry immediately! (yuk)
		 */
		struct arpmap	map;

		ATM_ADDR_COPY(atm, &map.am_dstatm);
		map.am_dstatmsub.address_format = T_ATM_ABSENT;
		map.am_dstatmsub.address_length = 0;
		ivp->iv_arpent = &map;
		err = ipatm_opensvc(ivp);
		if (err) {
			atm_free((caddr_t)ivp);
			goto done;
		}
		ivp->iv_arpent = NULL;
	}

	/*
	 * Queue VCC onto its network interface
	 */
	s = splnet();
	ipatm_vccnt++;
	ENQUEUE(ivp, struct ipvcc, iv_elem, inp->inf_vcq);
	(void) splx(s);

done:
	if (err)
		*sivp = NULL;
	else
		*sivp = ivp;
	return (err);
}


/*
 * Open an IP SVC
 * 
 * This function will continue the IP SVC creation process.  Here, we
 * will issue an SVC open to the signalling manager and then wait for
 * the final SVC setup results.
 *
 * Arguments:
 *	ivp	pointer to IP SVC to open
 *
 * Returns:
 *	0 	SVC open was successfully initiated
 *	errno	open failed - reason indicated
 *
 */
int
ipatm_opensvc(ivp)
	struct ipvcc	*ivp;
{
	struct ip_nif	*inp = ivp->iv_ipnif;
	Atm_attributes	*ap;
	int	err = 0, i;

	/*
	 * Cancel possible arp timeout
	 */
	IPVCC_CANCEL(ivp);

	/*
	 * Fill out connection attributes
	 */
	i = ivp->iv_parmx;
	if (inp->inf_serv->is_vccparm[i].ivc_aal == ATM_AAL5) {
		if (inp->inf_serv->is_vccparm[i].ivc_encaps == ATM_ENC_LLC) {
			ap = &ipatm_aal5llc;
			ivp->iv_flags |= IVF_LLC;
		} else {
			ap = &ipatm_aal5null;
			ivp->iv_flags &= ~IVF_LLC;
		}
	} else {
		ap = &ipatm_aal4null;
		ivp->iv_flags &= ~IVF_LLC;
	}

	ap->nif = inp->inf_nif;
	ap->traffic.v.forward.PCR_all_traffic = inp->inf_nif->nif_pif->pif_pcr;
	ap->traffic.v.backward.PCR_all_traffic = inp->inf_nif->nif_pif->pif_pcr;

	ATM_ADDR_COPY(&ivp->iv_arpent->am_dstatm, &ap->called.addr);
	ATM_ADDR_COPY(&ivp->iv_arpent->am_dstatmsub, &ap->called.subaddr);

	/*
	 * Initiate SVC open
	 */
	err = atm_cm_connect(&ipatm_endpt, ivp, ap, &ivp->iv_conn);
	switch (err) {

	case EINPROGRESS:
		/*
		 * Call is progressing
		 */
		/* ivp->iv_ = ap->headout; */

		/*
		 * Now we just wait for a CALL_CONNECTED event
		 */
		ivp->iv_state = IPVCC_POPEN;
		IPVCC_TIMER(ivp, IPATM_SVC_TIME);
		err = 0;
		break;

	case 0:
		/*
		 * We've been hooked up with a shared VCC
		 */
		/* ivp->iv_ = ap->headout; */
		ipatm_activate(ivp);
		break;
	}

	return (err);
}


/*
 * Retry an IP SVC Open
 * 
 * This function will attempt to retry a failed SVC open request.  The IP
 * interface service provider specifies a list of possible VCC parameters
 * for IP to use.  We will try each set of parameters in turn until either
 * an open succeeds or we reach the end of the list.
 * 
 * Arguments:
 *	ivp	pointer to IP SVC
 *
 * Returns:
 *	0 	SVC (re)open was successfully initiated
 *	else	retry failed
 *
 */
int
ipatm_retrysvc(ivp)
	struct ipvcc	*ivp;
{
	struct ip_nif	*inp = ivp->iv_ipnif;

	/*
	 * If there isn't another set of vcc parameters to try, return
	 */
	if ((++ivp->iv_parmx >= IPATM_VCCPARMS) ||
	    (inp->inf_serv->is_vccparm[ivp->iv_parmx].ivc_aal == 0))
		return (1);

	/*
	 * Okay, now initiate open with a new set of parameters
	 */
	return (ipatm_opensvc(ivp));
}


/*
 * Finish IP SVC Activation
 * 
 * Arguments:
 *	ivp	pointer to IP SVC
 *
 * Returns:
 *	none
 *
 */
void
ipatm_activate(ivp)
	struct ipvcc	*ivp;
{

	/*
	 * Connection is now active
	 */
	ivp->iv_state = IPVCC_ACTIVE;
	IPVCC_CANCEL(ivp);

	/*
	 * Tell ARP module that connection is active
	 */
	if ((*ivp->iv_ipnif->inf_serv->is_arp_svcact)(ivp)) {
		(void) ipatm_closevc(ivp, T_ATM_CAUSE_TEMPORARY_FAILURE);
		return;
	}

	/*
	 * Send any queued packet
	 */
	if ((ivp->iv_flags & IVF_MAPOK) && ivp->iv_queue) {
		struct sockaddr_in	sin;
		struct ifnet		*ifp;

		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = ivp->iv_dst.s_addr;
		ifp = (struct ifnet *)ivp->iv_ipnif->inf_nif;
		(void) ipatm_ifoutput(ifp, ivp->iv_queue, 
			(struct sockaddr *)&sin);
		ivp->iv_queue = NULL;
	}
}


/*
 * Process Incoming Calls
 * 
 * This function will receive control when an incoming call has been matched
 * to one of our registered listen parameter blocks.  Assuming the call passes
 * acceptance criteria and all required resources are available, we will
 * create an IP SVC and notify the connection manager of our decision.  We
 * will then await notification of the final SVC setup results.  If any
 * problems are encountered, we will just tell the connection manager to
 * reject the call.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tok	owner's matched listening token
 *	cop	pointer to incoming call's connection block
 *	ap	pointer to incoming call's attributes
 *	tokp	pointer to location to store our connection token
 *
 * Returns:
 *	0	call is accepted
 *	errno	call rejected - reason indicated
 *
 */
int
ipatm_incoming(tok, cop, ap, tokp)
	void		*tok;
	Atm_connection	*cop;
	Atm_attributes	*ap;
	void		**tokp;
{
	struct atm_nif	*nip = ap->nif;
	struct ip_nif	*inp;
	struct ipvcc	*ivp = NULL;
	int	err, cause;
	int	usellc = 0, mtu = ATM_NIF_MTU;

	/*
	 * Get IP interface and make sure its ready
	 */
	for (inp = ipatm_nif_head; inp; inp = inp->inf_next) {
		if (inp->inf_nif == nip)
			break;
	}
	if ((inp == NULL) || (inp->inf_state != IPNIF_ACTIVE)) {
		err = ENETUNREACH;
		cause = T_ATM_CAUSE_SERVICE_OR_OPTION_UNAVAILABLE;
		goto reject;
	}

	/*
	 * Make sure we have services provider and ARP support
	 */
	if ((inp->inf_serv == NULL) ||
	    (inp->inf_serv->is_arp_svcin == NULL)) {
		err = ENETUNREACH;
		cause = T_ATM_CAUSE_SERVICE_OR_OPTION_UNAVAILABLE;
		goto reject;
	}

	/*
	 * Check for LLC encapsulation
	 */
	if ((ap->blli.tag_l2 == T_ATM_PRESENT) &&
	    (ap->blli.v.layer_2_protocol.ID_type == T_ATM_SIMPLE_ID) &&
	    (ap->blli.v.layer_2_protocol.ID.simple_ID == T_ATM_BLLI2_I8802)) {
		usellc = 1;
		mtu += IPATM_LLC_LEN;
	}

	/*
	 * Verify requested MTU
	 */
	if (ap->aal.type == ATM_AAL5) {
		if ((ap->aal.v.aal5.forward_max_SDU_size > mtu) ||
		    (ap->aal.v.aal5.backward_max_SDU_size < mtu)) {
			err = ENETUNREACH;
			cause = T_ATM_CAUSE_AAL_PARAMETERS_NOT_SUPPORTED;
			goto reject;
		}
	} else {
		if ((ap->aal.v.aal4.forward_max_SDU_size > mtu) ||
		    (ap->aal.v.aal4.backward_max_SDU_size < mtu)) {
			err = ENETUNREACH;
			cause = T_ATM_CAUSE_AAL_PARAMETERS_NOT_SUPPORTED;
			goto reject;
		}
	}

	/*
	 * Allocate IP VCC
	 */
	ivp = (struct ipvcc *)atm_allocate(&ipatm_vcpool);
	if (ivp == NULL) {
		err = ENOMEM;
		cause = T_ATM_CAUSE_UNSPECIFIED_RESOURCE_UNAVAILABLE;
		goto reject;
	}

	/*
	 * Initialize SVC
	 */
	ivp->iv_flags = IVF_SVC;
	ivp->iv_ipnif = inp;
	if (usellc)
		ivp->iv_flags |= IVF_LLC;

	/*
	 * Lookup ARP entry for destination
	 */
	switch ((*inp->inf_serv->is_arp_svcin)
			(ivp, &ap->calling.addr, &ap->calling.subaddr)) {

	case MAP_PROCEEDING:
		/*
		 * We'll be (hopefully) notified later
		 */
		break;

	case MAP_VALID:
		/*
		 * We've got our answer already
		 */
		ivp->iv_flags |= IVF_MAPOK;
		ivp->iv_dst.s_addr = ivp->iv_arpent->am_dstip.s_addr;
		break;

	case MAP_FAILED:
		/*
		 * So sorry...come again
		 */
		err = ENETUNREACH;
		cause = T_ATM_CAUSE_SERVICE_OR_OPTION_UNAVAILABLE;
		goto reject;

	default:
		panic("ipatm_incoming: invalid arp_svcin return");
	}

	/*
	 * Accept SVC connection
	 */
	ivp->iv_state = IPVCC_PACCEPT;

	/*
	 * Save VCC information
	 */
	ivp->iv_conn = cop;
	*tokp = ivp;
	/* ivp->iv_ = ap->headout; */

	/*
	 * Queue VCC onto its network interface
	 */
	ipatm_vccnt++;
	ENQUEUE(ivp, struct ipvcc, iv_elem, inp->inf_vcq);

	/*
	 * Wait for a CALL_CONNECTED event
	 */
	IPVCC_TIMER(ivp, IPATM_SVC_TIME);

	return (0);

reject:
	/*
	 * Clean up after call failure
	 */
	if (ivp) {
		(*inp->inf_serv->is_arp_close)(ivp);
		atm_free((caddr_t)ivp);
	}
	ap->cause.tag = T_ATM_PRESENT;
	ap->cause.v = ipatm_cause;
	ap->cause.v.cause_value = cause;
	return (err);
}


/*
 * Close an IP VCC
 * 
 * This function will close an IP VCC (PVC or SVC), including notifying 
 * the signalling and ARP subsystems of the VCC's demise and cleaning 
 * up memory after ourselves.
 *
 * Arguments:
 *	ivp	pointer to VCC
 *	code	cause code
 *
 * Returns:
 *	0 	VCC successfully closed
 *	errno	close failed - reason indicated
 *
 */
int
ipatm_closevc(ivp, code)
	struct ipvcc	*ivp;
	int		code;
{
	struct ip_nif	*inp = ivp->iv_ipnif;
	int	s, err;

	/*
	 * Make sure VCC hasn't been through here already
	 */
	switch (ivp->iv_state) {

	case IPVCC_FREE:
		return (EALREADY);
	}

	/*
	 * Reset lookup cache
	 */
	if (last_map_ipvcc == ivp) {
		last_map_ipvcc = NULL;
		last_map_ipdst = 0;
	}

	/*
	 * Tell ARP about SVCs and dynamic PVCs
	 */
	if (inp->inf_serv && 
	    ((ivp->iv_flags & IVF_SVC) || inp->inf_serv->is_arp_pvcopen)) {
		(*inp->inf_serv->is_arp_close)(ivp);
	}

	/*
	 * Free queued packets
	 */
	if (ivp->iv_queue)
		KB_FREEALL(ivp->iv_queue);

	/*
	 * Cancel any timers
	 */
	IPVCC_CANCEL(ivp);

	/*
	 * Close VCC
	 */
	switch (ivp->iv_state) {

	case IPVCC_PMAP:
		break;

	case IPVCC_POPEN:
	case IPVCC_PACCEPT:
	case IPVCC_ACTPENT:
	case IPVCC_ACTIVE:
		ipatm_cause.cause_value = code;
		err = atm_cm_release(ivp->iv_conn, &ipatm_cause);
		if (err) {
			log(LOG_ERR, 
				"ipatm_closevc: release fail: err=%d\n", err);
		}
		break;

	case IPVCC_CLOSED:
		break;

	default:
		log(LOG_ERR, 
			"ipatm_closevc: unknown state: ivp=%p, state=%d\n",
                       	ivp, ivp->iv_state);
	}

	/*
	 * Remove VCC from network i/f
	 */
	s = splnet();
	DEQUEUE(ivp, struct ipvcc, iv_elem, inp->inf_vcq);

	/*
	 * Reset state just to be sure
	 */
	ivp->iv_state = IPVCC_FREE;

	/*
	 * If ARP module is done with VCC too, then free it
	 */
	if (ivp->iv_arpconn == NULL)
		atm_free((caddr_t)ivp);
	ipatm_vccnt--;
	(void) splx(s);

	return (0);
}


/*
 * Check if IP address is valid on a Network Interface
 * 
 * Checks whether the supplied IP address is allowed to be assigned to
 * the supplied IP network interface.
 *
 * Arguments:
 *	in	IP address
 *	inp	pointer to IP network interface
 *
 * Returns:
 *	0 - OK to assign
 *	1 - not valid to assign
 *
 */
int
ipatm_chknif(in, inp)
	struct in_addr	in;
	struct ip_nif	*inp;
{
	struct in_ifaddr	*ia;
	u_long	i;

	/*
	 * Make sure there's an interface requested
	 */
	if (inp == NULL)
		return (1);

	/*
	 * Make sure we have an IP address
	 */
	i = ntohl(in.s_addr);
	if (i == 0)
		return (1);

	/*
	 * Make sure an interface address is set 
	 */
	ia = inp->inf_addr;
	if (ia == NULL)
		return (1);

	/*
	 * Make sure we're on the right subnet
	 */
	if ((i & ia->ia_subnetmask) != ia->ia_subnet)
		return (1);

	return (0);
}


/*
 * Map an IP Address to an IP VCC
 * 
 * Given a destination IP address, this function will return a pointer
 * to the appropriate output IP VCC to which to send the packet.
 * This is currently implemented using a one-behind cache containing the 
 * last successful mapping result.  If the cache lookup fails, then a
 * simple linear search of all IP VCCs on the destination network interface 
 * is performed.  This is obviously an area to look at for performance 
 * improvements.
 *
 * Arguments:
 *	dst	pointer to destination IP address
 *	nip	pointer to destination network interface
 *
 * Returns:
 *	addr 	pointer to located IP VCC
 *	0	no such mapping exists
 *
 */
struct ipvcc *
ipatm_iptovc(dst, nip)
	struct sockaddr_in	*dst;
	struct atm_nif		*nip;
{
	struct ip_nif	*inp;
	struct ipvcc	*ivp;
	u_long		dstip = dst->sin_addr.s_addr;
	int	s;

	/*
	 * Look in cache first
	 */
	if (last_map_ipdst == dstip)
		return (last_map_ipvcc);

	/*
	 * Oh well, we've got to search for it...first find the interface
	 */
	s = splnet();
	for (inp = ipatm_nif_head; inp; inp = inp->inf_next) {
		if (inp->inf_nif == nip)
			break;
	}
	if (inp == NULL) {
		(void) splx(s);
		return (NULL);
	}

	/*
	 * Now home in on the VCC
	 */
	for (ivp = Q_HEAD(inp->inf_vcq, struct ipvcc); ivp;
				ivp = Q_NEXT(ivp, struct ipvcc, iv_elem)) {
		if (ivp->iv_dst.s_addr == dstip)
			break;
	}

	/*
	 * Update lookup cache
	 */
	if (ivp) {
		last_map_ipdst = dstip;
		last_map_ipvcc = ivp;
	}
	(void) splx(s);

	return (ivp);
}

