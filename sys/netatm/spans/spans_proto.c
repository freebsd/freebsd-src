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
 * SPANS protocol processing module.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
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
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include "spans_xdr.h"
#include <netatm/spans/spans_var.h>

/*
 * Internal functions
 */
caddr_t	spans_getname(void *);
void	spans_connected(void *);
void	spans_cleared(void *, struct t_atm_cause *);
void	spans_cpcs_data(void *, KBuffer *);


/*
 * ATM endpoint for SPANS signalling channel
 */
static Atm_endpoint	spans_endpt = {
	NULL,			/* ep_next */
	ENDPT_SPANS_SIG,	/* ep_id */
	NULL,			/* ep_ioctl */
	spans_getname,		/* ep_getname */
	spans_connected,	/* ep_connected */
	spans_cleared,		/* ep_cleared */
	NULL,			/* ep_incoming */
	NULL,			/* ep_addparty */
	NULL,			/* ep_dropparty */
	NULL,			/* ep_cpcs_ctl */
	spans_cpcs_data,	/* ep_cpcs_data */
	NULL,			/* ep_saal_ctl */
	NULL,			/* ep_saal_data */
	NULL,			/* ep_sscop_ctl */
	NULL			/* ep_sscop_data */
};


/*
 * ATM connection attributes for UNI signalling channel
 */
static Atm_attributes	spans_attr = {
	NULL,				/* nif */
	CMAPI_CPCS,			/* api */
	0,				/* api_init */
	0,				/* headin */
	0,				/* headout */
	{				/* aal */
		T_ATM_PRESENT,		/* aal.tag */
		ATM_AAL3_4		/* aal.aal_type */
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
			T_YES,		/* best_effort */
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
 * SPANS cause structre
 */
struct t_atm_cause spans_cause = {
	T_ATM_ITU_CODING,		/* coding_standard */
	T_ATM_LOC_USER,			/* location */
	T_ATM_CAUSE_UNSPECIFIED_NORMAL,	/* cause_value */
	{ 0, 0, 0, 0 }			/* diagnostics */
};

SYSCTL_NODE(_net_harp, OID_AUTO, spans, CTLFLAG_RW, 0, "spans");

/*
 * Process a SPANS timeout
 *
 * Called when a previously scheduled spans control block timer expires.
 * Processing will based on the current SPANS state.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tip	pointer to spans timer control block
 *
 * Returns:
 *	none
 *
 */
void
spans_timer(tip)
	struct atm_time	*tip;
{
	struct spans	*spp;
	spans_msg	*msg;
	Atm_addr_pvc	*pvcp;
	int		err;

	/*
	 * Back-off to SPANS control block
	 */
	spp = (struct spans *)
		((caddr_t)tip - (int)(&((struct spans *)0)->sp_time));

	ATM_DEBUG2("spans_timer: spp=%p,state=%d\n",
			spp, spp->sp_state);

	/*
	 * Process timeout based on protocol state
	 */
	switch (spp->sp_state) {

	case SPANS_INIT:

		/*
		 * Open signalling channel
		 */
		spans_attr.nif = spp->sp_pif->pif_nif;

		spans_attr.aal.v.aal4.forward_max_SDU_size =
				ATM_NIF_MTU;
		spans_attr.aal.v.aal4.backward_max_SDU_size =
				ATM_NIF_MTU;
		spans_attr.aal.v.aal4.SSCS_type =
				T_ATM_SSCS_SSCOP_UNREL;
		spans_attr.aal.v.aal4.mid_low = 0;
		spans_attr.aal.v.aal4.mid_high = 0;

		spans_attr.called.tag = T_ATM_PRESENT;
		spans_attr.called.addr.address_format = T_ATM_PVC_ADDR;
		spans_attr.called.addr.address_length =
			sizeof(Atm_addr_pvc);
		pvcp = (Atm_addr_pvc *)spans_attr.called.addr.address;
		ATM_PVC_SET_VPI(pvcp, SPANS_SIG_VPI);
		ATM_PVC_SET_VCI(pvcp, SPANS_SIG_VCI);
		spans_attr.called.subaddr.address_format = T_ATM_ABSENT;
		spans_attr.called.subaddr.address_length = 0;

		spans_attr.traffic.v.forward.PCR_all_traffic =
				spp->sp_pif->pif_pcr;
		spans_attr.traffic.v.backward.PCR_all_traffic =
				spp->sp_pif->pif_pcr;

		err = atm_cm_connect(&spans_endpt, spp, &spans_attr,
				&spp->sp_conn);
		if (err) {
			log(LOG_CRIT, "spans: signalling channel setup failed\n");
			return;
		}

		/*
		 * Signalling channel open, start probing
		 */
		spp->sp_state = SPANS_PROBE;

		/* FALLTHRU */

	case SPANS_PROBE:
	case SPANS_ACTIVE:

		/*
		 * Send out SPANS_STAT_REQ message
		 */
		msg = uma_zalloc(spans_msg_zone, M_WAITOK);
		if (msg == NULL) {
			/* XXX arr: This is bogus and will go away RSN */
			/* Retry later if no memory */
			SPANS_TIMER(spp, SPANS_PROBE_ERR_WAIT);
			break;
		}
		msg->sm_vers = SPANS_VERS_1_0;
		msg->sm_type = SPANS_STAT_REQ;
		msg->sm_stat_req.streq_es_epoch = spp->sp_h_epoch;
		if (spans_send_msg(spp, msg)) {
			/* Retry later if send fails */
			SPANS_TIMER(spp, SPANS_PROBE_ERR_WAIT);
			uma_zfree(spans_msg_zone, msg);
			break;
		}
		uma_zfree(spans_msg_zone, msg);
		spp->sp_probe_ct++;

		/*
		 * Check whether we're getting an answer to our probes
		 */
		if (spp->sp_state == SPANS_ACTIVE &&
				spp->sp_probe_ct > SPANS_PROBE_THRESH) {
			/*
			 * Interface is down, notify VCC owners
			 */
			spans_switch_reset(spp, SPANS_UNI_DOWN);

			/*
			 * Set new state and increment host epoch so
			 * switch knows we reset everyting.
			 */
			spp->sp_state = SPANS_PROBE;
			spp->sp_h_epoch++;
			spp->sp_s_epoch = 0;
		}

		/*
		 * Keep sending status requests
		 */
		SPANS_TIMER(spp, SPANS_PROBE_INTERVAL);

		break;

	case SPANS_DETACH:
		/*
		 * Try to terminate the SPANS signalling PVC
		 */
		err = atm_cm_release(spp->sp_conn, &spans_cause);
		if (err) {
			log(LOG_ERR, "spans: can't close signalling channel\n");
		}
		break;

	default:
		log(LOG_ERR, "spans: timer state: spp=%p, state=%d\n",
			spp, spp->sp_state);
	}
}


/*
 * Process a SPANS VCC timeout
 *
 * Called when a previously scheduled SPANS VCCB timer expires.
 * Processing will based on the current VCC state.
 *
 * Called at splnet.
 *
 * Arguments:
 *	tip	pointer to vccb timer control block
 *
 * Returns:
 *	none
 *
 */
void
spans_vctimer(tip)
	struct atm_time	*tip;
{
	int			err;
	struct spans		*spp;
	struct spans_vccb	*svp;

	/*
	 * Get VCCB and SPANS control block addresses
	 */
	svp = (struct spans_vccb *) ((caddr_t)tip -
			(int)(&((struct vccb *)0)->vc_time));
	spp = (struct spans *)svp->sv_pif->pif_siginst;

	ATM_DEBUG3("spans_vctimer: svp=%p, sstate=%d, ustate=%d\n",
			svp, svp->sv_sstate, svp->sv_ustate);

	/*
	 * Process timeout based on protocol state
	 */
	switch (svp->sv_sstate) {

	case SPANS_VC_ABORT:
		/*
		 * Kill the VCCB and notify the owner
		 */
		err = spans_clear_vcc(spp, svp);
		break;

	case SPANS_VC_FREE:
		/*
		 * Free VCCB storage
		 */
		svp->sv_ustate = VCCU_CLOSED;
		svp->sv_sstate = SPANS_VC_FREE;
		spans_free((struct vccb *)svp);
		break;

	case SPANS_VC_POPEN:
		/*
		 * Issued open request, but didn't get response.
		 */
		if (svp->sv_retry < SV_MAX_RETRY) {
			/*
			 * Retransmit the open request
			 */
			err = spans_send_open_req(spp, svp);
			svp->sv_retry++;
			SPANS_VC_TIMER((struct vccb *) svp, SV_TIMEOUT);
		} else {
			/*
			 * Retry limit exceeded--report the open failed
			 */
			svp->sv_ustate = VCCU_CLOSED;
			svp->sv_sstate = SPANS_VC_FREE;
			svp->sv_connvc->cvc_attr.cause.tag =
					T_ATM_PRESENT;
			svp->sv_connvc->cvc_attr.cause.v.coding_standard =
					T_ATM_ITU_CODING;
			svp->sv_connvc->cvc_attr.cause.v.location =
					T_ATM_LOC_USER;
			svp->sv_connvc->cvc_attr.cause.v.cause_value =
					T_ATM_CAUSE_NO_USER_RESPONDING;
			bzero(svp->sv_connvc->cvc_attr.cause.v.diagnostics,
					sizeof(svp->sv_connvc->cvc_attr.cause.v.diagnostics));
			atm_cm_cleared(svp->sv_connvc);
		}
		break;

	case SPANS_VC_CLOSE:
		/*
		 * Issued close request, but didn't get response.
		 */
		if (svp->sv_retry < SV_MAX_RETRY) {
			/*
			 * Retransmit the close request
			 */
			err = spans_send_close_req(spp, svp);
			svp->sv_retry++;
			SPANS_VC_TIMER((struct vccb *) svp, SV_TIMEOUT);
		} else {
			/*
			 * Retry limit exceeded--just finish the close
			 */
			svp->sv_sstate = SPANS_VC_FREE;
			svp->sv_connvc->cvc_attr.cause.tag = T_ATM_PRESENT;
			svp->sv_connvc->cvc_attr.cause.v.coding_standard =
					T_ATM_ITU_CODING;
			svp->sv_connvc->cvc_attr.cause.v.location =
					T_ATM_LOC_USER;
			svp->sv_connvc->cvc_attr.cause.v.cause_value =
					T_ATM_CAUSE_NO_USER_RESPONDING;
			bzero(svp->sv_connvc->cvc_attr.cause.v.diagnostics,
					sizeof(svp->sv_connvc->cvc_attr.cause.v.diagnostics));
			atm_cm_cleared(svp->sv_connvc);
		}
		break;

	case SPANS_VC_ACTIVE:
	case SPANS_VC_ACT_DOWN:
		/*
		 * Shouldn't happen
		 */
		log(LOG_ERR, "spans_vctimer: unexpected state %d\n",
				svp->sv_sstate);
		break;

	default:
		log(LOG_ERR, "spans: vctimer state: svp=%p, sstate=%d\n",
				svp, svp->sv_sstate);
	}
}


/*
 * SPANS name routine
 *
 * Arguments:
 *	tok	SPANS signalling channel token (ignored)
 *
 * Returns:
 *	pointer to a string identifying the SPANS signalling manager
 *
 */
caddr_t
spans_getname(tok)
	void		*tok;
{
	return("SPANS");
}


/*
 * Process a VCC connection notification
 *
 * Should never be called
 *
 * Arguments:
 *	tok	user's connection token (SPANS protocol block)
 *
 * Returns:
 *	none
 *
 */
void
spans_connected(tok)
	void		*tok;
{
	struct spans		*spp = (struct spans *)tok;

	ATM_DEBUG2("spans_connected: spp=%p,state=%d\n",
			spp, spp->sp_state);

	/*
	 * Connected routine shouldn't ever get called for a PVC
	 */
	log(LOG_ERR, "spans: connected function called, tok=%p\n", spp);
}


/*
 * Process a VCC close notification
 *
 * Called when the SPANS signalling channel is closed
 *
 * Arguments:
 *	tok	user's connection token (spans protocol block)
 *	cp	pointer to cause structure
 *
 * Returns:
 *	none
 *
 */
void
spans_cleared(tok, cp)
	void			*tok;
	struct t_atm_cause	*cp;
{
	struct spans	*spp = (struct spans *)tok;

	/*
	 * VCC has been closed.
	 */
	log(LOG_ERR, "spans: signalling channel closed\n");
	SPANS_CANCEL(spp);
	spp->sp_conn = 0;
}


/*
 * SPANS CPCS data handler
 *
 * This is the module which receives data on the SPANS signalling
 * channel.  Processing is based on the indication received from the
 * AAL and the protocol state.
 *
 * Arguments:
 *	tok	session token (pointer to spans protocol control block)
 *	m	pointer to buffer with data
 *
 * Returns:
 *	none
 *
 */
void
spans_cpcs_data(tok, m)
	void	*tok;
	KBuffer	*m;
{
	struct spans	*spp = tok;

	ATM_DEBUG3("spans_cpcs_data: spp=%p,state=%d,m=%p,\n",
			spp, spp->sp_state, m);

	/*
	 * Process data
	 */
	spans_rcv_msg(spp, m);
}
