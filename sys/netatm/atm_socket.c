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
 * Core ATM Services
 * -----------------
 *
 * ATM common socket protocol processing
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Local functions
 */


/*
 * Local variables
 */
static uma_zone_t atm_pcb_zone;

static struct t_atm_cause	atm_sock_cause = {
	T_ATM_ITU_CODING,
	T_ATM_LOC_USER,
	T_ATM_CAUSE_UNSPECIFIED_NORMAL,
	{0, 0, 0, 0}
};

void
atm_sock_init(void)
{

	atm_pcb_zone = uma_zcreate("atm pcb", sizeof(Atm_pcb), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, 0);
	if (atm_pcb_zone == NULL)
		panic("atm_sock_init: unable to initialize atm_pcb_zone");
	uma_zone_set_max(atm_pcb_zone, 100);
}

/*
 * Allocate resources for a new ATM socket
 *
 * Called at splnet.
 *
 * Arguments:
 *	so	pointer to socket
 *	send	socket send buffer maximum
 *	recv	socket receive buffer maximum
 *
 * Returns:
 *	0	attach successful
 *	errno	attach failed - reason indicated
 *
 */
int
atm_sock_attach(so, send, recv)
	struct socket	*so;
	u_long		send;
	u_long		recv;
{
	Atm_pcb		*atp = sotoatmpcb(so);
	int		err;

	/*
	 * Make sure initialization has happened
	 */
	if (!atm_init)
		atm_initialize();

	/*
	 * Make sure we're not already attached
	 */
	if (atp)
		return (EISCONN);

	/*
	 * Reserve socket buffer space, if not already done
	 */
	if ((so->so_snd.sb_hiwat == 0) || (so->so_rcv.sb_hiwat == 0)) {
		err = soreserve(so, send, recv);
		if (err)
			return (err);
	}

	/*
	 * Allocate and initialize our control block
	 */
	atp = uma_zalloc(atm_pcb_zone, M_ZERO | M_NOWAIT);
	if (atp == NULL)
		return (ENOMEM);

	atp->atp_socket = so;
	so->so_pcb = (caddr_t)atp;
	return (0);
}


/*
 * Detach from socket and free resources
 *
 * Called at splnet.
 *
 * Arguments:
 *	so	pointer to socket
 *
 * Returns:
 *	0	detach successful
 *	errno	detach failed - reason indicated
 *
 */
int
atm_sock_detach(so)
	struct socket	*so;
{
	Atm_pcb		*atp = sotoatmpcb(so);

	/*
	 * Make sure we're still attached
	 */
	if (atp == NULL)
		return (ENOTCONN);

	/*
	 * Terminate any (possibly pending) connection
	 */
	if (atp->atp_conn) {
		(void) atm_sock_disconnect(so);
	}

	/*
	 * Break links and free control blocks
	 */
	so->so_pcb = NULL;
	sotryfree(so);

	uma_zfree(atm_pcb_zone, atp);

	return (0);
}


/*
 * Bind local address to socket
 *
 * Called at splnet.
 *
 * Arguments:
 *	so	pointer to socket
 *	addr	pointer to protocol address
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
int
atm_sock_bind(so, addr)
	struct socket	*so;
	struct sockaddr	*addr;
{
	Atm_pcb			*atp = sotoatmpcb(so);
	Atm_attributes		attr;
	struct sockaddr_atm	*satm;
	struct t_atm_sap_addr	*sapadr;
	struct t_atm_sap_layer2	*sapl2;
	struct t_atm_sap_layer3	*sapl3;
	struct t_atm_sap_appl	*sapapl;

	/*
	 * Make sure we're still attached
	 */
	if (atp == NULL)
		return (ENOTCONN);

	/*
	 * Can't change local address once we've started connection process
	 */
	if (atp->atp_conn != NULL)
		return (EADDRNOTAVAIL);

	/*
	 * Validate requested local address
	 */
	satm = (struct sockaddr_atm *)addr;
	if (satm->satm_family != AF_ATM)
		return (EAFNOSUPPORT);

	sapadr = &satm->satm_addr.t_atm_sap_addr;
	if (sapadr->SVE_tag_addr == T_ATM_PRESENT) {
		if (sapadr->address_format == T_ATM_ENDSYS_ADDR) {
			if (sapadr->SVE_tag_selector != T_ATM_PRESENT)
				return (EINVAL);
		} else if (sapadr->address_format == T_ATM_E164_ADDR) {
			if (sapadr->SVE_tag_selector != T_ATM_ABSENT)
				return (EINVAL);
		} else
			return (EINVAL);
	} else if ((sapadr->SVE_tag_addr != T_ATM_ABSENT) &&
		   (sapadr->SVE_tag_addr != T_ATM_ANY))
		return (EINVAL);
	if (sapadr->address_length > ATM_ADDR_LEN)
		return (EINVAL);

	sapl2 = &satm->satm_addr.t_atm_sap_layer2;
	if (sapl2->SVE_tag == T_ATM_PRESENT) {
		if ((sapl2->ID_type != T_ATM_SIMPLE_ID) &&
		    (sapl2->ID_type != T_ATM_USER_ID))
			return (EINVAL);
	} else if ((sapl2->SVE_tag != T_ATM_ABSENT) &&
		   (sapl2->SVE_tag != T_ATM_ANY))
		return (EINVAL);

	sapl3 = &satm->satm_addr.t_atm_sap_layer3;
	if (sapl3->SVE_tag == T_ATM_PRESENT) {
		if ((sapl3->ID_type != T_ATM_SIMPLE_ID) &&
		    (sapl3->ID_type != T_ATM_IPI_ID) &&
		    (sapl3->ID_type != T_ATM_SNAP_ID) &&
		    (sapl3->ID_type != T_ATM_USER_ID))
			return (EINVAL);
	} else if ((sapl3->SVE_tag != T_ATM_ABSENT) &&
		   (sapl3->SVE_tag != T_ATM_ANY))
		return (EINVAL);

	sapapl = &satm->satm_addr.t_atm_sap_appl;
	if (sapapl->SVE_tag == T_ATM_PRESENT) {
		if ((sapapl->ID_type != T_ATM_ISO_APP_ID) &&
		    (sapapl->ID_type != T_ATM_USER_APP_ID) &&
		    (sapapl->ID_type != T_ATM_VENDOR_APP_ID))
			return (EINVAL);
	} else if ((sapapl->SVE_tag != T_ATM_ABSENT) &&
		   (sapapl->SVE_tag != T_ATM_ANY))
		return (EINVAL);

	/*
	 * Create temporary attributes list so that we can check out the
	 * new bind parameters before we modify the socket's values;
	 */
	attr = atp->atp_attr;
	attr.called.tag = sapadr->SVE_tag_addr;
	bcopy(&sapadr->address_format, &attr.called.addr, sizeof(Atm_addr));

	attr.blli.tag_l2 = sapl2->SVE_tag;
	if (sapl2->SVE_tag == T_ATM_PRESENT) {
		attr.blli.v.layer_2_protocol.ID_type = sapl2->ID_type;
		bcopy(&sapl2->ID, &attr.blli.v.layer_2_protocol.ID,
			sizeof(attr.blli.v.layer_2_protocol.ID));
	}

	attr.blli.tag_l3 = sapl3->SVE_tag;
	if (sapl3->SVE_tag == T_ATM_PRESENT) {
		attr.blli.v.layer_3_protocol.ID_type = sapl3->ID_type;
		bcopy(&sapl3->ID, &attr.blli.v.layer_3_protocol.ID,
			sizeof(attr.blli.v.layer_3_protocol.ID));
	}

	attr.bhli.tag = sapapl->SVE_tag;
	if (sapapl->SVE_tag == T_ATM_PRESENT) {
		attr.bhli.v.ID_type = sapapl->ID_type;
		bcopy(&sapapl->ID, &attr.bhli.v.ID,
			sizeof(attr.bhli.v.ID));
	}

	/*
	 * Make sure we have unique listening attributes
	 */
	if (atm_cm_match(&attr, NULL) != NULL)
		return (EADDRINUSE);

	/*
	 * Looks good, save new attributes
	 */
	atp->atp_attr = attr;

	return (0);
}


/*
 * Listen for incoming connections
 *
 * Called at splnet.
 *
 * Arguments:
 *	so	pointer to socket
 *	epp	pointer to endpoint definition structure
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
int
atm_sock_listen(so, epp)
	struct socket	*so;
	Atm_endpoint	*epp;
{
	Atm_pcb		*atp = sotoatmpcb(so);

	/*
	 * Make sure we're still attached
	 */
	if (atp == NULL)
		return (ENOTCONN);

	/*
	 * Start listening for incoming calls
	 */
	return (atm_cm_listen(epp, atp, &atp->atp_attr, &atp->atp_conn));
}


/*
 * Connect socket to peer
 *
 * Called at splnet.
 *
 * Arguments:
 *	so	pointer to socket
 *	addr	pointer to protocol address
 *	epp	pointer to endpoint definition structure
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
int
atm_sock_connect(so, addr, epp)
	struct socket	*so;
	struct sockaddr	*addr;
	Atm_endpoint	*epp;
{
	Atm_pcb		*atp = sotoatmpcb(so);
	struct sockaddr_atm	*satm;
	struct t_atm_sap_addr	*sapadr;
	struct t_atm_sap_layer2	*sapl2;
	struct t_atm_sap_layer3	*sapl3;
	struct t_atm_sap_appl	*sapapl;
	int		err;

	/*
	 * Make sure we're still attached
	 */
	if (atp == NULL)
		return (ENOTCONN);

	/*
	 * Validate requested peer address
	 */
	satm = (struct sockaddr_atm *)addr;
	if (satm->satm_family != AF_ATM)
		return (EAFNOSUPPORT);

	sapadr = &satm->satm_addr.t_atm_sap_addr;
	if (sapadr->SVE_tag_addr != T_ATM_PRESENT)
		return (EINVAL);
	if (sapadr->address_format == T_ATM_ENDSYS_ADDR) {
		if (sapadr->SVE_tag_selector != T_ATM_PRESENT)
			return (EINVAL);
	} else if (sapadr->address_format == T_ATM_E164_ADDR) {
		if (sapadr->SVE_tag_selector != T_ATM_ABSENT)
			return (EINVAL);
	} else if (sapadr->address_format == T_ATM_PVC_ADDR) {
		if (sapadr->SVE_tag_selector != T_ATM_ABSENT)
			return (EINVAL);
	} else
		return (EINVAL);
	if (sapadr->address_length > ATM_ADDR_LEN)
		return (EINVAL);

	sapl2 = &satm->satm_addr.t_atm_sap_layer2;
	if (sapl2->SVE_tag == T_ATM_PRESENT) {
		if ((sapl2->ID_type != T_ATM_SIMPLE_ID) &&
		    (sapl2->ID_type != T_ATM_USER_ID))
			return (EINVAL);
	} else if (sapl2->SVE_tag != T_ATM_ABSENT)
		return (EINVAL);

	sapl3 = &satm->satm_addr.t_atm_sap_layer3;
	if (sapl3->SVE_tag == T_ATM_PRESENT) {
		if ((sapl3->ID_type != T_ATM_SIMPLE_ID) &&
		    (sapl3->ID_type != T_ATM_IPI_ID) &&
		    (sapl3->ID_type != T_ATM_SNAP_ID) &&
		    (sapl3->ID_type != T_ATM_USER_ID))
			return (EINVAL);
	} else if (sapl3->SVE_tag != T_ATM_ABSENT)
		return (EINVAL);

	sapapl = &satm->satm_addr.t_atm_sap_appl;
	if (sapapl->SVE_tag == T_ATM_PRESENT) {
		if ((sapapl->ID_type != T_ATM_ISO_APP_ID) &&
		    (sapapl->ID_type != T_ATM_USER_APP_ID) &&
		    (sapapl->ID_type != T_ATM_VENDOR_APP_ID))
			return (EINVAL);
	} else if (sapapl->SVE_tag != T_ATM_ABSENT)
		return (EINVAL);

	/*
	 * Select an outgoing network interface
	 */
	if (atp->atp_attr.nif == NULL) {
		struct atm_pif	*pip;

		for (pip = atm_interface_head; pip != NULL;
						pip = pip->pif_next) {
			if (pip->pif_nif != NULL) {
				atp->atp_attr.nif = pip->pif_nif;
				break;
			}
		}
		if (atp->atp_attr.nif == NULL)
			return (ENXIO);
	}

	/*
	 * Set supplied connection attributes
	 */
	atp->atp_attr.called.tag = T_ATM_PRESENT;
	bcopy(&sapadr->address_format, &atp->atp_attr.called.addr,
			sizeof(Atm_addr));

	atp->atp_attr.blli.tag_l2 = sapl2->SVE_tag;
	if (sapl2->SVE_tag == T_ATM_PRESENT) {
		atp->atp_attr.blli.v.layer_2_protocol.ID_type = sapl2->ID_type;
		bcopy(&sapl2->ID, &atp->atp_attr.blli.v.layer_2_protocol.ID,
			sizeof(atp->atp_attr.blli.v.layer_2_protocol.ID));
	}

	atp->atp_attr.blli.tag_l3 = sapl3->SVE_tag;
	if (sapl3->SVE_tag == T_ATM_PRESENT) {
		atp->atp_attr.blli.v.layer_3_protocol.ID_type = sapl3->ID_type;
		bcopy(&sapl3->ID, &atp->atp_attr.blli.v.layer_3_protocol.ID,
			sizeof(atp->atp_attr.blli.v.layer_3_protocol.ID));
	}

	atp->atp_attr.bhli.tag = sapapl->SVE_tag;
	if (sapapl->SVE_tag == T_ATM_PRESENT) {
		atp->atp_attr.bhli.v.ID_type = sapapl->ID_type;
		bcopy(&sapapl->ID, &atp->atp_attr.bhli.v.ID,
			sizeof(atp->atp_attr.bhli.v.ID));
	}

	/*
	 * We're finally ready to initiate the ATM connection
	 */
	soisconnecting(so);
	atm_sock_stat.as_connreq[atp->atp_type]++;
	err = atm_cm_connect(epp, atp, &atp->atp_attr, &atp->atp_conn);
	if (err == 0) {
		/*
		 * Connection is setup
		 */
		atm_sock_stat.as_conncomp[atp->atp_type]++;
		soisconnected(so);

	} else if (err == EINPROGRESS) {
		/*
		 * We've got to wait for a connected event
		 */
		err = 0;

	} else {
		/*
		 * Call failed...
		 */
		atm_sock_stat.as_connfail[atp->atp_type]++;
		soisdisconnected(so);
	}

	return (err);
}


/*
 * Disconnect connected socket
 *
 * Called at splnet.
 *
 * Arguments:
 *	so	pointer to socket
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
int
atm_sock_disconnect(so)
	struct socket	*so;
{
	Atm_pcb		*atp = sotoatmpcb(so);
	struct t_atm_cause	*cause;
	int		err;

	/*
	 * Make sure we're still attached
	 */
	if (atp == NULL)
		return (ENOTCONN);

	/*
	 * Release the ATM connection
	 */
	if (atp->atp_conn) {
		if (atp->atp_attr.cause.tag == T_ATM_PRESENT)
			cause = &atp->atp_attr.cause.v;
		else
			cause = &atm_sock_cause;
		err = atm_cm_release(atp->atp_conn, cause);
		if (err)
			log(LOG_ERR, "atm_sock_disconnect: release fail (%d)\n",
				err);
		atm_sock_stat.as_connrel[atp->atp_type]++;
		atp->atp_conn = NULL;
	}

	soisdisconnected(so);

	return (0);
}


/*
 * Retrieve local socket address
 *
 * Called at splnet.
 *
 * Arguments:
 *	so	pointer to socket
 *	addr	pointer to pointer to contain protocol address
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
int
atm_sock_sockaddr(so, addr)
	struct socket	*so;
	struct sockaddr	**addr;
{
	struct sockaddr_atm	*satm;
	struct t_atm_sap_addr	*saddr;
	Atm_pcb		*atp = sotoatmpcb(so);

	/*
	 * Return local interface address, if known
	 */
	satm = malloc(sizeof(*satm), M_SONAME, M_ZERO);
	if (satm == NULL)
		return (ENOMEM);

	satm->satm_family = AF_ATM;
	satm->satm_len = sizeof(*satm);

	saddr = &satm->satm_addr.t_atm_sap_addr;
	if (atp->atp_attr.nif && atp->atp_attr.nif->nif_pif->pif_siginst) {
		saddr->SVE_tag_addr = T_ATM_PRESENT;
		ATM_ADDR_SEL_COPY(
			&atp->atp_attr.nif->nif_pif->pif_siginst->si_addr,
			atp->atp_attr.nif->nif_sel, saddr);
		if (saddr->address_format == T_ATM_ENDSYS_ADDR)
			saddr->SVE_tag_selector = T_ATM_PRESENT;
		else
			saddr->SVE_tag_selector = T_ATM_ABSENT;
	} else {
		saddr->SVE_tag_addr = T_ATM_ABSENT;
		saddr->SVE_tag_selector = T_ATM_ABSENT;
		saddr->address_format = T_ATM_ABSENT;
	}
	satm->satm_addr.t_atm_sap_layer2.SVE_tag = T_ATM_ABSENT;
	satm->satm_addr.t_atm_sap_layer3.SVE_tag = T_ATM_ABSENT;
	satm->satm_addr.t_atm_sap_appl.SVE_tag = T_ATM_ABSENT;

	*addr = (struct sockaddr *)satm;
	return (0);
}


/*
 * Retrieve peer socket address
 *
 * Called at splnet.
 *
 * Arguments:
 *	so	pointer to socket
 *	addr	pointer to pointer to contain protocol address
 *
 * Returns:
 *	0	request processed
 *	errno	error processing request - reason indicated
 *
 */
int
atm_sock_peeraddr(so, addr)
	struct socket	*so;
	struct sockaddr	**addr;
{
	struct sockaddr_atm	*satm;
	struct t_atm_sap_addr	*saddr;
	Atm_pcb		*atp = sotoatmpcb(so);
	Atm_connvc	*cvp;

	/*
	 * Return remote address, if known
	 */
	satm = malloc(sizeof(*satm), M_SONAME, M_ZERO);
	if (satm == NULL)
		return (ENOMEM);

	satm->satm_family = AF_ATM;
	satm->satm_len = sizeof(*satm);
	saddr = &satm->satm_addr.t_atm_sap_addr;
	if (so->so_state & SS_ISCONNECTED) {
		cvp = atp->atp_conn->co_connvc;
		saddr->SVE_tag_addr = T_ATM_PRESENT;
		if (cvp->cvc_flags & CVCF_CALLER) {
			ATM_ADDR_COPY(&cvp->cvc_attr.called.addr, saddr);
		} else {
			if (cvp->cvc_attr.calling.tag == T_ATM_PRESENT) {
				ATM_ADDR_COPY(&cvp->cvc_attr.calling.addr,
							saddr);
			} else {
				saddr->SVE_tag_addr = T_ATM_ABSENT;
				saddr->address_format = T_ATM_ABSENT;
			}
		}
		if (saddr->address_format == T_ATM_ENDSYS_ADDR)
			saddr->SVE_tag_selector = T_ATM_PRESENT;
		else
			saddr->SVE_tag_selector = T_ATM_ABSENT;
	} else {
		saddr->SVE_tag_addr = T_ATM_ABSENT;
		saddr->SVE_tag_selector = T_ATM_ABSENT;
		saddr->address_format = T_ATM_ABSENT;
	}
	satm->satm_addr.t_atm_sap_layer2.SVE_tag = T_ATM_ABSENT;
	satm->satm_addr.t_atm_sap_layer3.SVE_tag = T_ATM_ABSENT;
	satm->satm_addr.t_atm_sap_appl.SVE_tag = T_ATM_ABSENT;

	*addr = (struct sockaddr *)satm;
	return (0);
}


/*
 * Common setsockopt processing
 *
 * Called at splnet.
 *
 * Arguments:
 *	so	pointer to socket
 *	sopt	pointer to socket option info
 *	atp	pointer to ATM PCB
 *
 * Returns:
 *	0 	request processed
 *	errno	error processing request - reason indicated
 *
 */
int
atm_sock_setopt(so, sopt, atp)
	struct socket	*so;
	struct sockopt	*sopt;
	Atm_pcb		*atp;
{
	int	err = 0;
	union {
		struct t_atm_aal5	aal5;
		struct t_atm_traffic	trf;
		struct t_atm_bearer	brr;
		struct t_atm_bhli	bhl;
		struct t_atm_blli	bll;
		Atm_addr		addr;
		struct t_atm_cause	cau;
		struct t_atm_qos	qos;
		struct t_atm_transit	trn;
		struct t_atm_net_intf	nif;
		struct t_atm_llc	llc;
		struct t_atm_app_name	appn;
	} p;

#define	MAXVAL(bits)	((1 << bits) - 1)
#define	MAXMASK(bits)	(~MAXVAL(bits))

	switch (sopt->sopt_name) {

	case T_ATM_AAL5:
		err = sooptcopyin(sopt, &p.aal5, sizeof p.aal5, sizeof p.aal5);
		if (err)
			break;
		if ((p.aal5.forward_max_SDU_size != T_ATM_ABSENT) &&
		    (p.aal5.forward_max_SDU_size & MAXMASK(16)))
			return (EINVAL);
		if ((p.aal5.backward_max_SDU_size != T_ATM_ABSENT) &&
		    (p.aal5.backward_max_SDU_size & MAXMASK(16)))
			return (EINVAL);
		if ((p.aal5.SSCS_type != T_ATM_ABSENT) &&
		    (p.aal5.SSCS_type != T_ATM_NULL) &&
		    (p.aal5.SSCS_type != T_ATM_SSCS_SSCOP_REL) &&
		    (p.aal5.SSCS_type != T_ATM_SSCS_SSCOP_UNREL) &&
		    (p.aal5.SSCS_type != T_ATM_SSCS_FR))
			return (EINVAL);

		if ((p.aal5.forward_max_SDU_size == T_ATM_ABSENT) &&
		    (p.aal5.backward_max_SDU_size == T_ATM_ABSENT) &&
		    (p.aal5.SSCS_type == T_ATM_ABSENT))
			atp->atp_attr.aal.tag = T_ATM_ABSENT;
		else {
			atp->atp_attr.aal.tag = T_ATM_PRESENT;
			atp->atp_attr.aal.type = ATM_AAL5;
			atp->atp_attr.aal.v.aal5 = p.aal5;
		}
		break;

	case T_ATM_TRAFFIC:
		err = sooptcopyin(sopt, &p.trf, sizeof p.trf, sizeof p.trf);
		if (err)
			break;
		if ((p.trf.forward.PCR_high_priority != T_ATM_ABSENT) &&
		    (p.trf.forward.PCR_high_priority & MAXMASK(24)))
			return (EINVAL);
		if (p.trf.forward.PCR_all_traffic & MAXMASK(24))
			return (EINVAL);
		if ((p.trf.forward.SCR_high_priority != T_ATM_ABSENT) &&
		    (p.trf.forward.SCR_high_priority & MAXMASK(24)))
			return (EINVAL);
		if ((p.trf.forward.SCR_all_traffic != T_ATM_ABSENT) &&
		    (p.trf.forward.SCR_all_traffic & MAXMASK(24)))
			return (EINVAL);
		if ((p.trf.forward.MBS_high_priority != T_ATM_ABSENT) &&
		    (p.trf.forward.MBS_high_priority & MAXMASK(24)))
			return (EINVAL);
		if ((p.trf.forward.MBS_all_traffic != T_ATM_ABSENT) &&
		    (p.trf.forward.MBS_all_traffic & MAXMASK(24)))
			return (EINVAL);
		if ((p.trf.forward.tagging != T_YES) &&
		    (p.trf.forward.tagging != T_NO))
			return (EINVAL);

		if ((p.trf.backward.PCR_high_priority != T_ATM_ABSENT) &&
		    (p.trf.backward.PCR_high_priority & MAXMASK(24)))
			return (EINVAL);
		if (p.trf.backward.PCR_all_traffic & MAXMASK(24))
			return (EINVAL);
		if ((p.trf.backward.SCR_high_priority != T_ATM_ABSENT) &&
		    (p.trf.backward.SCR_high_priority & MAXMASK(24)))
			return (EINVAL);
		if ((p.trf.backward.SCR_all_traffic != T_ATM_ABSENT) &&
		    (p.trf.backward.SCR_all_traffic & MAXMASK(24)))
			return (EINVAL);
		if ((p.trf.backward.MBS_high_priority != T_ATM_ABSENT) &&
		    (p.trf.backward.MBS_high_priority & MAXMASK(24)))
			return (EINVAL);
		if ((p.trf.backward.MBS_all_traffic != T_ATM_ABSENT) &&
		    (p.trf.backward.MBS_all_traffic & MAXMASK(24)))
			return (EINVAL);
		if ((p.trf.backward.tagging != T_YES) &&
		    (p.trf.backward.tagging != T_NO))
			return (EINVAL);
		if ((p.trf.best_effort != T_YES) &&
		    (p.trf.best_effort != T_NO))
			return (EINVAL);

		atp->atp_attr.traffic.tag = T_ATM_PRESENT;
		atp->atp_attr.traffic.v = p.trf;
		break;

	case T_ATM_BEARER_CAP:
		err = sooptcopyin(sopt, &p.brr, sizeof p.brr, sizeof p.brr);
		if (err)
			break;
		if ((p.brr.bearer_class != T_ATM_CLASS_A) &&
		    (p.brr.bearer_class != T_ATM_CLASS_C) &&
		    (p.brr.bearer_class != T_ATM_CLASS_X))
			return (EINVAL);
		if ((p.brr.traffic_type != T_ATM_NULL) &&
		    (p.brr.traffic_type != T_ATM_CBR) &&
		    (p.brr.traffic_type != T_ATM_VBR))
			return (EINVAL);
		if ((p.brr.timing_requirements != T_ATM_NULL) &&
		    (p.brr.timing_requirements != T_ATM_END_TO_END) &&
		    (p.brr.timing_requirements != T_ATM_NO_END_TO_END))
			return (EINVAL);
		if ((p.brr.clipping_susceptibility != T_NO) &&
		    (p.brr.clipping_susceptibility != T_YES))
			return (EINVAL);
		if ((p.brr.connection_configuration != T_ATM_1_TO_1) &&
		    (p.brr.connection_configuration != T_ATM_1_TO_MANY))
			return (EINVAL);

		atp->atp_attr.bearer.tag = T_ATM_PRESENT;
		atp->atp_attr.bearer.v = p.brr;
		break;

	case T_ATM_BHLI:
		err = sooptcopyin(sopt, &p.bhl, sizeof p.bhl, sizeof p.bhl);
		if (err)
			break;
		if ((p.bhl.ID_type != T_ATM_ABSENT) &&
		    (p.bhl.ID_type != T_ATM_ISO_APP_ID) &&
		    (p.bhl.ID_type != T_ATM_USER_APP_ID) &&
		    (p.bhl.ID_type != T_ATM_VENDOR_APP_ID))
			return (EINVAL);

		if (p.bhl.ID_type == T_ATM_ABSENT)
			atp->atp_attr.bhli.tag = T_ATM_ABSENT;
		else {
			atp->atp_attr.bhli.tag = T_ATM_PRESENT;
			atp->atp_attr.bhli.v = p.bhl;
		}
		break;

	case T_ATM_BLLI:
		err = sooptcopyin(sopt, &p.bll, sizeof p.bll, sizeof p.bll);
		if (err)
			break;
		if ((p.bll.layer_2_protocol.ID_type != T_ATM_ABSENT) &&
		    (p.bll.layer_2_protocol.ID_type != T_ATM_SIMPLE_ID) &&
		    (p.bll.layer_2_protocol.ID_type != T_ATM_USER_ID))
			return (EINVAL);
		if ((p.bll.layer_2_protocol.mode != T_ATM_ABSENT) &&
		    (p.bll.layer_2_protocol.mode != T_ATM_BLLI_NORMAL_MODE) &&
		    (p.bll.layer_2_protocol.mode != T_ATM_BLLI_EXTENDED_MODE))
			return (EINVAL);
		if ((p.bll.layer_2_protocol.window_size != T_ATM_ABSENT) &&
		    (p.bll.layer_2_protocol.window_size < 1))
			return (EINVAL);

		if ((p.bll.layer_3_protocol.ID_type != T_ATM_ABSENT) &&
		    (p.bll.layer_3_protocol.ID_type != T_ATM_SIMPLE_ID) &&
		    (p.bll.layer_3_protocol.ID_type != T_ATM_IPI_ID) &&
		    (p.bll.layer_3_protocol.ID_type != T_ATM_SNAP_ID) &&
		    (p.bll.layer_3_protocol.ID_type != T_ATM_USER_ID))
			return (EINVAL);
		if ((p.bll.layer_3_protocol.mode != T_ATM_ABSENT) &&
		    (p.bll.layer_3_protocol.mode != T_ATM_BLLI_NORMAL_MODE) &&
		    (p.bll.layer_3_protocol.mode != T_ATM_BLLI_EXTENDED_MODE))
			return (EINVAL);
		if ((p.bll.layer_3_protocol.packet_size != T_ATM_ABSENT) &&
		    (p.bll.layer_3_protocol.packet_size & MAXMASK(4)))
			return (EINVAL);
		if ((p.bll.layer_3_protocol.window_size != T_ATM_ABSENT) &&
		    (p.bll.layer_3_protocol.window_size < 1))
			return (EINVAL);

		if (p.bll.layer_2_protocol.ID_type == T_ATM_ABSENT) 
			atp->atp_attr.blli.tag_l2 = T_ATM_ABSENT;
		else
			atp->atp_attr.blli.tag_l2 = T_ATM_PRESENT;

		if (p.bll.layer_3_protocol.ID_type == T_ATM_ABSENT) 
			atp->atp_attr.blli.tag_l3 = T_ATM_ABSENT;
		else
			atp->atp_attr.blli.tag_l3 = T_ATM_PRESENT;

		if ((atp->atp_attr.blli.tag_l2 == T_ATM_PRESENT) ||
		    (atp->atp_attr.blli.tag_l3 == T_ATM_PRESENT))
			atp->atp_attr.blli.v = p.bll;
		break;

	case T_ATM_DEST_ADDR:
		err = sooptcopyin(sopt, &p.addr, sizeof p.addr, sizeof p.addr);
		if (err)
			break;
		if ((p.addr.address_format != T_ATM_ENDSYS_ADDR) &&
		    (p.addr.address_format != T_ATM_E164_ADDR))
			return (EINVAL);
		if (p.addr.address_length > ATM_ADDR_LEN)
			return (EINVAL);

		atp->atp_attr.called.tag = T_ATM_PRESENT;
		atp->atp_attr.called.addr = p.addr;
		break;

	case T_ATM_DEST_SUB:
		err = sooptcopyin(sopt, &p.addr, sizeof p.addr, sizeof p.addr);
		if (err)
			break;
		if ((p.addr.address_format != T_ATM_ABSENT) &&
		    (p.addr.address_format != T_ATM_NSAP_ADDR))
			return (EINVAL);
		if (p.addr.address_length > ATM_ADDR_LEN)
			return (EINVAL);

		/* T_ATM_DEST_ADDR controls tag */
		atp->atp_attr.called.subaddr = p.addr;
		break;

	case T_ATM_ORIG_ADDR:
		return (EACCES);

	case T_ATM_ORIG_SUB:
		return (EACCES);

	case T_ATM_CALLER_ID:
		return (EACCES);

	case T_ATM_CAUSE:
		err = sooptcopyin(sopt, &p.cau, sizeof p.cau, sizeof p.cau);
		if (err)
			break;
		if ((p.cau.coding_standard != T_ATM_ABSENT) &&
		    (p.cau.coding_standard != T_ATM_ITU_CODING) &&
		    (p.cau.coding_standard != T_ATM_NETWORK_CODING))
			return (EINVAL);
		if ((p.cau.location != T_ATM_LOC_USER) &&
		    (p.cau.location != T_ATM_LOC_LOCAL_PRIVATE_NET) &&
		    (p.cau.location != T_ATM_LOC_LOCAL_PUBLIC_NET) &&
		    (p.cau.location != T_ATM_LOC_TRANSIT_NET) &&
		    (p.cau.location != T_ATM_LOC_REMOTE_PUBLIC_NET) &&
		    (p.cau.location != T_ATM_LOC_REMOTE_PRIVATE_NET) &&
		    (p.cau.location != T_ATM_LOC_INTERNATIONAL_NET) &&
		    (p.cau.location != T_ATM_LOC_BEYOND_INTERWORKING))
			return (EINVAL);

		if (p.cau.coding_standard == T_ATM_ABSENT)
			atp->atp_attr.cause.tag = T_ATM_ABSENT;
		else {
			atp->atp_attr.cause.tag = T_ATM_PRESENT;
			atp->atp_attr.cause.v = p.cau;
		}
		break;

	case T_ATM_QOS:
		err = sooptcopyin(sopt, &p.qos, sizeof p.qos, sizeof p.qos);
		if (err)
			break;
		if ((p.qos.coding_standard != T_ATM_ABSENT) &&
		    (p.qos.coding_standard != T_ATM_ITU_CODING) &&
		    (p.qos.coding_standard != T_ATM_NETWORK_CODING))
			return (EINVAL);
		if ((p.qos.forward.qos_class != T_ATM_QOS_CLASS_0) &&
		    (p.qos.forward.qos_class != T_ATM_QOS_CLASS_1) &&
		    (p.qos.forward.qos_class != T_ATM_QOS_CLASS_2) &&
		    (p.qos.forward.qos_class != T_ATM_QOS_CLASS_3) &&
		    (p.qos.forward.qos_class != T_ATM_QOS_CLASS_4))
			return (EINVAL);
		if ((p.qos.backward.qos_class != T_ATM_QOS_CLASS_0) &&
		    (p.qos.backward.qos_class != T_ATM_QOS_CLASS_1) &&
		    (p.qos.backward.qos_class != T_ATM_QOS_CLASS_2) &&
		    (p.qos.backward.qos_class != T_ATM_QOS_CLASS_3) &&
		    (p.qos.backward.qos_class != T_ATM_QOS_CLASS_4))
			return (EINVAL);

		if (p.qos.coding_standard == T_ATM_ABSENT)
			atp->atp_attr.qos.tag = T_ATM_ABSENT;
		else {
			atp->atp_attr.qos.tag = T_ATM_PRESENT;
			atp->atp_attr.qos.v = p.qos;
		}
		break;

	case T_ATM_TRANSIT:
		err = sooptcopyin(sopt, &p.trn, sizeof p.trn, sizeof p.trn);
		if (err)
			break;
		if (p.trn.length > T_ATM_MAX_NET_ID)
			return (EINVAL);

		if (p.trn.length == 0)
			atp->atp_attr.transit.tag = T_ATM_ABSENT;
		else {
			atp->atp_attr.transit.tag = T_ATM_PRESENT;
			atp->atp_attr.transit.v = p.trn;
		}
		break;

	case T_ATM_ADD_LEAF:
		return (EPROTONOSUPPORT);	/* XXX */

	case T_ATM_DROP_LEAF:
		return (EPROTONOSUPPORT);	/* XXX */

	case T_ATM_NET_INTF:
		err = sooptcopyin(sopt, &p.nif, sizeof p.nif, sizeof p.nif);
		if (err)
			break;

		atp->atp_attr.nif = atm_nifname(p.nif.net_intf);
		if (atp->atp_attr.nif == NULL)
			return (ENXIO);
		break;

	case T_ATM_LLC:
		err = sooptcopyin(sopt, &p.llc, sizeof p.llc, sizeof p.llc);
		if (err)
			break;
		if ((p.llc.llc_len < T_ATM_LLC_MIN_LEN) ||
		    (p.llc.llc_len > T_ATM_LLC_MAX_LEN))
			return (EINVAL);

		atp->atp_attr.llc.tag = T_ATM_PRESENT;
		atp->atp_attr.llc.v = p.llc;
		break;

	case T_ATM_APP_NAME:
		err = sooptcopyin(sopt, &p.appn, sizeof p.appn, sizeof p.appn);
		if (err)
			break;

		strncpy(atp->atp_name, p.appn.app_name, T_ATM_APP_NAME_LEN);
		break;

	default:
		return (ENOPROTOOPT);
	}

	return (err);
}


/*
 * Common getsockopt processing
 *
 * Called at splnet.
 *
 * Arguments:
 *	so	pointer to socket
 *	sopt	pointer to socket option info
 *	atp	pointer to ATM PCB
 *
 * Returns:
 *	0 	request processed
 *	errno	error processing request - reason indicated
 *
 */
int
atm_sock_getopt(so, sopt, atp)
	struct socket	*so;
	struct sockopt	*sopt;
	Atm_pcb		*atp;
{
	Atm_attributes	*ap;

	/*
	 * If socket is connected, return attributes for the VCC in use,
	 * otherwise just return what the user has setup so far.
	 */
	if (so->so_state & SS_ISCONNECTED)
		ap = &atp->atp_conn->co_connvc->cvc_attr;
	else
		ap = &atp->atp_attr;

	switch (sopt->sopt_name) {

	case T_ATM_AAL5:
		if ((ap->aal.tag == T_ATM_PRESENT) &&
		    (ap->aal.type == ATM_AAL5)) {
			return (sooptcopyout(sopt, &ap->aal.v.aal5,
					sizeof ap->aal.v.aal5));
		} else {
			return (ENOENT);
		}
		break;

	case T_ATM_TRAFFIC:
		if (ap->traffic.tag == T_ATM_PRESENT) {
			return (sooptcopyout(sopt, &ap->traffic.v,
					sizeof ap->traffic.v));
		} else {
			return (ENOENT);
		}
		break;

	case T_ATM_BEARER_CAP:
		if (ap->bearer.tag == T_ATM_PRESENT) {
			return (sooptcopyout(sopt, &ap->bearer.v,
					sizeof ap->bearer.v));
		} else {
			return (ENOENT);
		}
		break;

	case T_ATM_BHLI:
		if (ap->bhli.tag == T_ATM_PRESENT) {
			return (sooptcopyout(sopt, &ap->bhli.v,
					sizeof ap->bhli.v));
		} else {
			return (ENOENT);
		}
		break;

	case T_ATM_BLLI:
		if ((ap->blli.tag_l2 == T_ATM_PRESENT) ||
		    (ap->blli.tag_l3 == T_ATM_PRESENT)) {
			return (sooptcopyout(sopt, &ap->blli.v,
					sizeof ap->blli.v));
		} else {
			return (ENOENT);
		}
		break;

	case T_ATM_DEST_ADDR:
		if (ap->called.tag == T_ATM_PRESENT) {
			return (sooptcopyout(sopt, &ap->called.addr,
					sizeof ap->called.addr));
		} else {
			return (ENOENT);
		}
		break;

	case T_ATM_DEST_SUB:
		if (ap->called.tag == T_ATM_PRESENT) {
			return (sooptcopyout(sopt, &ap->called.subaddr,
					sizeof ap->called.subaddr));
		} else {
			return (ENOENT);
		}
		break;

	case T_ATM_ORIG_ADDR:
		if (ap->calling.tag == T_ATM_PRESENT) {
			return (sooptcopyout(sopt, &ap->calling.addr,
					sizeof ap->calling.addr));
		} else {
			return (ENOENT);
		}
		break;

	case T_ATM_ORIG_SUB:
		if (ap->calling.tag == T_ATM_PRESENT) {
			return (sooptcopyout(sopt, &ap->calling.subaddr,
					sizeof ap->calling.subaddr));
		} else {
			return (ENOENT);
		}
		break;

	case T_ATM_CALLER_ID:
		if (ap->calling.tag == T_ATM_PRESENT) {
			return (sooptcopyout(sopt, &ap->calling.cid,
					sizeof ap->calling.cid));
		} else {
			return (ENOENT);
		}
		break;

	case T_ATM_CAUSE:
		if (ap->cause.tag == T_ATM_PRESENT) {
			return (sooptcopyout(sopt, &ap->cause.v,
					sizeof ap->cause.v));
		} else {
			return (ENOENT);
		}
		break;

	case T_ATM_QOS:
		if (ap->qos.tag == T_ATM_PRESENT) {
			return (sooptcopyout(sopt, &ap->qos.v,
					sizeof ap->qos.v));
		} else {
			return (ENOENT);
		}
		break;

	case T_ATM_TRANSIT:
		if (ap->transit.tag == T_ATM_PRESENT) {
			return (sooptcopyout(sopt, &ap->transit.v,
					sizeof ap->transit.v));
		} else {
			return (ENOENT);
		}
		break;

	case T_ATM_LEAF_IND:
		return (EPROTONOSUPPORT);	/* XXX */

	case T_ATM_NET_INTF:
		if (ap->nif) {
			struct t_atm_net_intf	netif;
			struct ifnet		*ifp;

			ifp = &ap->nif->nif_if;
			(void) snprintf(netif.net_intf, sizeof(netif.net_intf),
			    "%s%d", ifp->if_name, ifp->if_unit);
			return (sooptcopyout(sopt, &netif,
					sizeof netif));
		} else {
			return (ENOENT);
		}
		break;

	case T_ATM_LLC:
		if (ap->llc.tag == T_ATM_PRESENT) {
			return (sooptcopyout(sopt, &ap->llc.v,
					sizeof ap->llc.v));
		} else {
			return (ENOENT);
		}
		break;

	default:
		return (ENOPROTOOPT);
	}

	return (0);
}


/*
 * Process Socket VCC Connected Notification
 *
 * Arguments:
 *	toku	owner's connection token (atm_pcb protocol block)
 *
 * Returns:
 *	none
 *
 */
void
atm_sock_connected(toku)
	void		*toku;
{
	Atm_pcb		*atp = (Atm_pcb *)toku;

	/*
	 * Connection is setup
	 */
	atm_sock_stat.as_conncomp[atp->atp_type]++;
	soisconnected(atp->atp_socket);
}


/*
 * Process Socket VCC Cleared Notification
 *
 * Arguments:
 *	toku	owner's connection token (atm_pcb protocol block)
 *	cause	pointer to cause code
 *
 * Returns:
 *	none
 *
 */
void
atm_sock_cleared(toku, cause)
	void		*toku;
	struct t_atm_cause	*cause;
{
	Atm_pcb		*atp = (Atm_pcb *)toku;
	struct socket	*so;

	so = atp->atp_socket;

	/*
	 * Save call clearing cause
	 */
	atp->atp_attr.cause.tag = T_ATM_PRESENT;
	atp->atp_attr.cause.v = *cause;

	/*
	 * Set user error code
	 */
	if (so->so_state & SS_ISCONNECTED) {
		so->so_error = ECONNRESET;
		atm_sock_stat.as_connclr[atp->atp_type]++;
	} else {
		so->so_error = ECONNREFUSED;
		atm_sock_stat.as_connfail[atp->atp_type]++;
	}

	/*
	 * Connection is gone
	 */
	atp->atp_conn = NULL;
	soisdisconnected(so);

	/*
	 * Cleanup failed incoming connection setup
	 */
	if (so->so_state & SS_NOFDREF) {
		(void) atm_sock_detach(so);
	}
}

