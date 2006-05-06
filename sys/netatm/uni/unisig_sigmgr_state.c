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
 * ATM Forum UNI 3.0/3.1 Signalling Manager
 * ----------------------------------------
 *
 * Signalling manager finite state machine
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netatm/uni/unisig_sigmgr_state.c,v 1.14 2005/01/07 01:45:37 imp Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/time.h>
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
#include <netatm/atm_vc.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include <netatm/uni/uni.h>
#include <netatm/uni/unisig.h>
#include <netatm/uni/unisig_var.h>

/*
 * Local functions
 */
static int	unisig_sigmgr_invalid(struct unisig *, KBuffer *);
static int	unisig_sigmgr_act01(struct unisig *, KBuffer *);
static int	unisig_sigmgr_act02(struct unisig *, KBuffer *);
static int	unisig_sigmgr_act03(struct unisig *, KBuffer *);
static int	unisig_sigmgr_act04(struct unisig *, KBuffer *);
static int	unisig_sigmgr_act05(struct unisig *, KBuffer *);
static int	unisig_sigmgr_act06(struct unisig *, KBuffer *);
static int	unisig_sigmgr_act07(struct unisig *, KBuffer *);
static int	unisig_sigmgr_act08(struct unisig *, KBuffer *);
static int	unisig_sigmgr_act09(struct unisig *, KBuffer *);
static int	unisig_sigmgr_act10(struct unisig *, KBuffer *);
static int	unisig_sigmgr_act11(struct unisig *, KBuffer *);
static int	unisig_sigmgr_act12(struct unisig *, KBuffer *);
static int	unisig_sigmgr_act13(struct unisig *, KBuffer *);
static int	unisig_sigmgr_act14(struct unisig *, KBuffer *);


/*
 * State table.
 */
static int	sigmgr_state_table[10][7] = {
    /*     0   1   2   3   4   5				*/
	{  1,  0,  0,  0,  0 },	/* 0 - Time out		*/
	{  0,  0,  3,  5,  0 },	/* 1 - SSCF estab ind	*/
	{  0,  0,  3,  5,  0 },	/* 2 - SSCF estab cnf	*/
	{  0,  0,  4,  6,  0 },	/* 3 - SSCF release ind	*/
	{  0,  0,  0,  6,  0 },	/* 4 - SSCF release cnf	*/
	{  0,  0,  0,  7,  0 },	/* 5 - SSCF data ind	*/
	{  0,  0,  2,  2,  0 },	/* 6 - SSCF unit data ind */
	{  0,  0,  8,  8,  8 },	/* 7 - Call cleared	*/
	{ 14, 14, 14, 14,  0 },	/* 8 - Detach		*/
	{ 13, 13,  0,  0,  0 }	/* 9 - Address set	*/
};

/*
 * Action vector
 */
#define	MAX_ACTION	15
static int (*unisig_sigmgr_act_vec[MAX_ACTION])
		(struct unisig *, KBuffer *) = {
	unisig_sigmgr_invalid,
	unisig_sigmgr_act01,
	unisig_sigmgr_act02,
	unisig_sigmgr_act03,
	unisig_sigmgr_act04,
	unisig_sigmgr_act05,
	unisig_sigmgr_act06,
	unisig_sigmgr_act07,
	unisig_sigmgr_act08,
	unisig_sigmgr_act09,
	unisig_sigmgr_act10,
	unisig_sigmgr_act11,
	unisig_sigmgr_act12,
	unisig_sigmgr_act13,
	unisig_sigmgr_act14
};


/*
 * ATM endpoint for UNI signalling channel
 */
static Atm_endpoint	unisig_endpt = {
	NULL,			/* ep_next */
	ENDPT_UNI_SIG,		/* ep_id */
	NULL,			/* ep_ioctl */
	unisig_getname,		/* ep_getname */
	unisig_connected,	/* ep_connected */
	unisig_cleared,		/* ep_cleared */
	NULL,			/* ep_incoming */
	NULL,			/* ep_addparty */
	NULL,			/* ep_dropparty */
	NULL,			/* ep_cpcs_ctl */
	NULL,			/* ep_cpcs_data */
	unisig_saal_ctl,	/* ep_saal_ctl */
	unisig_saal_data,	/* ep_saal_data */
	NULL,			/* ep_sscop_ctl */
	NULL			/* ep_sscop_data */
};


/*
 * ATM connection attributes for UNI signalling channel
 */
static Atm_attributes	unisig_attr = {
	NULL,				/* nif */
	CMAPI_SAAL,			/* api */
	UNI_VERS_3_0,			/* api_init */
	0,				/* headin */
	0,				/* headout */
	{				/* aal */
		T_ATM_PRESENT,		/* aal.tag */
		ATM_AAL5		/* aal.aal_type */
	},
	{				/* traffic */
		T_ATM_PRESENT,		/* traffic.tag */
		{			/* traffic.v */
			{		/* traffic.v.forward */
				T_ATM_ABSENT,	/* PCR_high */
				0,		/* PCR_all */
				T_ATM_ABSENT,	/* SCR_high */
				T_ATM_ABSENT,	/* SCR_all */
				T_ATM_ABSENT,	/* MBS_high */
				T_ATM_ABSENT,	/* MBS_all */
				T_NO,		/* tagging */
			},
			{		/* traffic.v.backward */
				T_ATM_ABSENT,	/* PCR_high */
				0,		/* PCR_all */
				T_ATM_ABSENT,	/* SCR_high */
				T_ATM_ABSENT,	/* SCR_all */
				T_ATM_ABSENT,	/* MBS_high */
				T_ATM_ABSENT,	/* MBS_all */
				T_NO,		/* tagging */
			},
			T_YES,			/* best_effort */
		}
	},
	{				/* bearer */
		T_ATM_PRESENT,		/* bearer.tag */
		{			/* bearer.v */
			T_ATM_CLASS_X,		/* class */
			T_ATM_NULL,		/* traffic_type */
			T_ATM_NO_END_TO_END,	/* timing_req */
			T_NO,			/* clipping */
			T_ATM_1_TO_1,		/* conn_conf */
		}
	},
	{				/* bhli */
		T_ATM_ABSENT,		/* bhli.tag */
	},
	{				/* blli */
		T_ATM_ABSENT,		/* blli.tag_l2 */
		T_ATM_ABSENT,		/* blli.tag_l3 */
	},
	{				/* llc */
		T_ATM_ABSENT,		/* llc.tag */
	},
	{				/* called */
		T_ATM_PRESENT,		/* called.tag */
	},
	{				/* calling */
		T_ATM_ABSENT,		/* calling.tag */
	},
	{				/* qos */
		T_ATM_PRESENT,		/* qos.tag */
		{			/* qos.v */
			T_ATM_NETWORK_CODING,	/* coding_standard */
			{			/* qos.v.forward */
				T_ATM_QOS_CLASS_0,	/* class */
			},
			{			/* qos.v.backward */
				T_ATM_QOS_CLASS_0,	/* class */
			}
		}
	},
	{				/* transit */
		T_ATM_ABSENT,		/* transit.tag */
	},
	{				/* cause */
		T_ATM_ABSENT,		/* cause.tag */
	}
};


/*
 * Finite state machine for the UNISIG signalling manager
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	event	indication of the event to be processed
 *	m	pointer to a buffer with a message (optional)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
unisig_sigmgr_state(usp, event, m)
	struct unisig	*usp;
	int		event;
	KBuffer		*m;
{
	int		action, err = 0;

	/*
	 * Cancel any signalling manager timer
	 */
	UNISIG_CANCEL(usp);

	/*
	 * Select an action based on the incoming event and
	 * the signalling manager's state
	 */
	action = sigmgr_state_table[event][usp->us_state];
	ATM_DEBUG4("unisig_sigmgr_state: usp=%p, state=%d, event=%d, action=%d\n",
			usp, usp->us_state, event, action);
	if (action >= MAX_ACTION || action < 0) {
		panic("unisig_sigmgr_state: invalid action\n");
	}

	/*
	 * Perform the requested action
	 */
	err = unisig_sigmgr_act_vec[action](usp, m);

	return(err);
}


/*
 * Signalling manager state machine action 0
 *
 * Invalid action
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	m	buffer pointer (may be NULL)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_sigmgr_invalid(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{
	log(LOG_ERR, "unisig_sigmgr_state: unexpected action\n");
	if (m)
		KB_FREEALL(m);
	return(0);
}


/*
 * Signalling manager state machine action 1
 *
 * The kickoff timer has expired at attach time; go to
 * UNISIG_ADDR_WAIT state.
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	m	buffer pointer (may be NULL)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_sigmgr_act01(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{
	/*
	 * Set the new state
	 */
	usp->us_state = UNISIG_ADDR_WAIT;

	return(0);
}


/*
 * Signalling manager state machine action 2
 *
 * Ignore the event
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	m	buffer pointer (may be NULL)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_sigmgr_act02(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{
	/*
	 * Ignore event, discard message if present
	 */
	if (m)
		KB_FREEALL(m);

	return(0);
}


/*
 * Signalling manager state machine action 3
 *
 * SSCF session on signalling channel has come up
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	m	buffer pointer (may be NULL)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_sigmgr_act03(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{
	struct unisig_vccb	*uvp, *vnext;

	/*
	 * Log the activation
	 */
	log(LOG_INFO, "unisig: signalling channel active\n");

	/*
	 * Go to ACTIVE state
	 */
	usp->us_state = UNISIG_ACTIVE;

	/*
	 * Notify the VC state machine that the channel is up
	 */
	for (uvp = Q_HEAD(usp->us_vccq, struct unisig_vccb);
			uvp; uvp = vnext) {
		vnext = Q_NEXT(uvp, struct unisig_vccb, uv_sigelem);
		(void) unisig_vc_state(usp, uvp, UNI_VC_SAAL_ESTAB,
				(struct unisig_msg *) 0);
	}

	return(0);
}


/*
 * Signalling manager state machine action 4
 *
 * A SSCF release indication was received.  Try to establish an
 * SSCF session on the signalling PVC.
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	m	buffer pointer (may be NULL)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_sigmgr_act04(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{
	int		err;

	/*
	 * Try to establish an SSCF session.
	 */
	err = atm_cm_saal_ctl(SSCF_UNI_ESTABLISH_REQ,
			usp->us_conn,
			(void *)0);
	if (err)
		panic("unisig_sigmgr_act04: SSCF_UNI_ESTABLISH_REQ");

	return(0);
}


/*
 * Signalling manager state machine action 5
 *
 * SSCF session on signalling channel has been reset
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	m	buffer pointer (may be NULL)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_sigmgr_act05(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{
	struct unisig_vccb	*uvp, *vnext;

	/*
	 * Log the reset
	 */
	log(LOG_INFO, "unisig: signalling channel reset\n");

	/*
	 * Notify the VC state machine of the reset
	 */
	for (uvp = Q_HEAD(usp->us_vccq, struct unisig_vccb);
			uvp; uvp = vnext) {
		vnext = Q_NEXT(uvp, struct unisig_vccb, uv_sigelem);
		(void) unisig_vc_state(usp, uvp, UNI_VC_SAAL_ESTAB,
				(struct unisig_msg *) 0);
	}

	return(0);
}


/*
 * Signalling manager state machine action 6
 *
 * SSCF session on signalling channel has been lost
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	m	buffer pointer (may be NULL)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_sigmgr_act06(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{
	struct unisig_vccb	*uvp, *vnext;

	/*
	 * Log the fact that the session has been lost
	 */
	log(LOG_INFO, "unisig: signalling channel SSCF session lost\n");

	/*
	 * Notify the VC state machine of the loss
	 */
	for (uvp = Q_HEAD(usp->us_vccq, struct unisig_vccb);
			uvp; uvp = vnext) {
		vnext = Q_NEXT(uvp, struct unisig_vccb, uv_sigelem);
		(void) unisig_vc_state(usp, uvp, UNI_VC_SAAL_FAIL,
				(struct unisig_msg *) 0);
	}

	/*
	 * Try to restart the SSCF session
	 */
	(void) unisig_sigmgr_act04(usp, (KBuffer *) 0);

	/*
	 * Go to INIT state
	 */
	usp->us_state = UNISIG_INIT;

	return(0);
}


/*
 * Signalling manager state machine action 7
 *
 * A Q.2931 signalling message has been received
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	m	buffer pointer (may be NULL)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_sigmgr_act07(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{
	int	err;

	/*
	 * Pass the Q.2931 signalling message on
	 * to the VC state machine
	 */
	err = unisig_rcv_msg(usp, m);

	return(err);
}


/*
 * Signalling manager state machine action 8
 *
 * Process a CALL_CLOSED event for the signalling PVC
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	m	buffer pointer (may be NULL)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_sigmgr_act08(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{

	/*
	 * Signalling manager is now incommunicado
	 */
	if (usp->us_state != UNISIG_DETACH) {
		/*
		 * Log an error and set the state to NULL if
		 * we're not detaching
		 */
		log(LOG_ERR, "unisig: signalling channel closed\n");
		usp->us_state = UNISIG_NULL;
	}
	usp->us_conn = 0;

	return(0);
}


/*
 * Signalling manager state machine action 9
 *
 * Not used
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	m	buffer pointer (may be NULL)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_sigmgr_act09(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{
	log(LOG_ERR, "unisig_sigmgr_act09: unexpected action\n");
	if (m)
		KB_FREEALL(m);
	return (0);
}


/*
 * Signalling manager state machine action 10
 *
 * Not used
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	m	buffer pointer (may be NULL)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_sigmgr_act10(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{
	return(0);
}


/*
 * Signalling manager state machine action 11
 *
 * Not used
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	m	buffer pointer (may be NULL)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_sigmgr_act11(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{
	log(LOG_ERR, "unisig_sigmgr_act11: unexpected action\n");
	if (m)
		KB_FREEALL(m);
	return(0);
}


/*
 * Signalling manager state machine action 12
 *
 * Not used
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	m	buffer pointer (may be NULL)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_sigmgr_act12(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{
	log(LOG_ERR, "unisig_sigmgr_act11: unexpected action\n");
	if (m)
		KB_FREEALL(m);
	return(0);
}


/*
 * Signalling manager state machine action 13
 *
 * NSAP prefix has been set
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	m	buffer pointer (may be NULL)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_sigmgr_act13(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{
	int		err;
	Atm_addr_pvc	*pvcp;

	/*
	 * Set UNI signalling channel connection attributes
	 */
	if (usp->us_proto == ATM_SIG_UNI30)
		unisig_attr.api_init = UNI_VERS_3_0;
	else
		unisig_attr.api_init = UNI_VERS_3_1;

	unisig_attr.nif = usp->us_pif->pif_nif;

	unisig_attr.aal.v.aal5.forward_max_SDU_size = ATM_NIF_MTU;
	unisig_attr.aal.v.aal5.backward_max_SDU_size = ATM_NIF_MTU;
	unisig_attr.aal.v.aal5.SSCS_type = T_ATM_SSCS_SSCOP_REL;

	unisig_attr.called.tag = T_ATM_PRESENT;
	unisig_attr.called.addr.address_format = T_ATM_PVC_ADDR;
	unisig_attr.called.addr.address_length = sizeof(Atm_addr_pvc);
	pvcp = (Atm_addr_pvc *)unisig_attr.called.addr.address;
	ATM_PVC_SET_VPI(pvcp, UNISIG_SIG_VPI);
	ATM_PVC_SET_VCI(pvcp, UNISIG_SIG_VCI);
	unisig_attr.called.subaddr.address_format = T_ATM_ABSENT;
	unisig_attr.called.subaddr.address_length = 0;

	unisig_attr.traffic.v.forward.PCR_all_traffic =
			usp->us_pif->pif_pcr;
	unisig_attr.traffic.v.backward.PCR_all_traffic =
			usp->us_pif->pif_pcr;

	/*
	 * Create UNISIG signalling channel
	 */
	err = atm_cm_connect(&unisig_endpt, usp, &unisig_attr,
			&usp->us_conn);
	if (err) {
		return(err);
	}

	/*
	 * Establish the SSCF session
	 */
	err = atm_cm_saal_ctl(SSCF_UNI_ESTABLISH_REQ,
			usp->us_conn,
			(void *)0);
	if (err)
		panic("unisig_sigmgr_act13: SSCF_UNI_ESTABLISH_REQ");

	/*
	 * Set the new state
	 */
	usp->us_state = UNISIG_INIT;

	return(0);
}


/*
 * Signalling manager state machine action 14
 *
 * Process a detach event
 *
 * Arguments:
 *	usp	pointer to the UNISIG protocol control block
 *	m	buffer pointer (may be NULL)
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_sigmgr_act14(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{
	int			err;
	struct unisig_vccb	*sig_vccb, *uvp, *vnext;
	struct atm_pif		*pip;
	struct t_atm_cause	cause;

	/*
	 * Locate the signalling channel's VCCB
	 */
	sig_vccb = (struct unisig_vccb *)0;
	if (usp->us_conn && usp->us_conn->co_connvc)
		sig_vccb = (struct unisig_vccb *)
				usp->us_conn->co_connvc->cvc_vcc;

	/*
	 * Terminate all of our VCCs
	 */
	for (uvp = Q_HEAD(usp->us_vccq, struct unisig_vccb);
			uvp; uvp = vnext) {
		vnext = Q_NEXT(uvp, struct unisig_vccb, uv_sigelem);

		/*
		 * Don't close the signalling VCC yet
		 */
		if (uvp == sig_vccb)
			continue;

		/*
		 * Close VCC and notify owner
		 */
		err = unisig_clear_vcc(usp, uvp,
				T_ATM_CAUSE_NORMAL_CALL_CLEARING);
	}

	/*
	 * Close the signalling channel
	 */
	if (usp->us_conn) {
		cause.coding_standard = T_ATM_ITU_CODING;
		cause.coding_standard = T_ATM_LOC_USER;
		cause.coding_standard = T_ATM_CAUSE_UNSPECIFIED_NORMAL;
		err = atm_cm_release(usp->us_conn, &cause);
		if (err)
			panic("unisig_sigmgr_act14: close failed\n");
	}

	/*
	 * Get rid of protocol instance if there are no VCCs queued
	 */
	pip = usp->us_pif;
	if (Q_HEAD(usp->us_vccq, struct vccb) == NULL &&
			pip->pif_sigmgr) {
		struct sigmgr	*smp = pip->pif_sigmgr;
		int		s = splimp();

		pip->pif_sigmgr = NULL;
		pip->pif_siginst = NULL;
		(void) splx(s);

		UNLINK((struct siginst *)usp, struct siginst,
				smp->sm_prinst, si_next);
		free(usp, M_DEVBUF);
	} else {
		/*
		 * Otherwise, set new signalling manager state and
		 * wait for protocol instance to be freed during 
		 * unisig_free processing for the last queued VCC
		 */
		usp->us_state = UNISIG_DETACH;
	}

	return (0);
}
