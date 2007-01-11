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
 * SPANS Signalling Manager
 * ---------------------------
 *
 * SPANS-related subroutines.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netatm/spans/spans_subr.c,v 1.13 2005/01/07 01:45:38 imp Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
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
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>

#include "spans_xdr.h"
#include <netatm/spans/spans_var.h>

/*
 * Open a SPANS VCC
 *
 * Called when a user wants to open a VC.  This function will construct
 * a VCCB, create the stack requested by the user, and, if we are
 * opening an SVC, start the SPANS signalling message exchange.  The
 * user will have to wait for a notify event to be sure the SVC is fully
 * open.
 *
 * Must be called at splnet.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance
 *	acp	pointer to PVC's connection parameters
 *
 * Returns:
 *	0	VCC creation successful
 *	errno	VCC setup failed - reason indicated
 *
 */
int
spans_open_vcc(spp, cvp)
	struct spans	*spp;
	Atm_connvc	*cvp;

{
	struct atm_pif		*pip = spp->sp_pif;
	struct spans_vccb	*svp;
	Atm_addr_pvc		*pvp;
	spans_aal		aal;
	int			err, pvc, vpi, vci;

	ATM_DEBUG2("spans_open_vcc: spp=%p, cvp=%p\n", spp, cvp);

	/*
	 * Validate user parameters. AAL and encapsulation are
	 * checked by the connection manager.
	 */

	/*
	 * Check called party address(es)
	 */
	if (cvp->cvc_attr.called.tag != T_ATM_PRESENT ||
			cvp->cvc_attr.called.addr.address_format ==
				T_ATM_ABSENT ||
			cvp->cvc_attr.called.subaddr.address_format !=
				T_ATM_ABSENT) {
		return(EINVAL);
	}
	switch (cvp->cvc_attr.called.addr.address_format) {
	case T_ATM_PVC_ADDR:
		/*
		 * Make sure VPI/VCI is valid
		 */
		pvc = 1;
		pvp = (Atm_addr_pvc *)cvp->cvc_attr.called.addr.address;
		vpi = ATM_PVC_GET_VPI(pvp);
		vci = ATM_PVC_GET_VCI(pvp);
		if ((vpi > pip->pif_maxvpi) ||
				(vci == 0) ||
				(vci > pip->pif_maxvci)) {
			return(ERANGE);
		}

		/*
		 * Make sure VPI/VCI is not already in use
		 */
		if (spans_find_vpvc(spp, vpi, vci, 0)) {
			return(EADDRINUSE);
		}
		ATM_DEBUG2("spans_open_vcc: VPI.VCI=%d.%d\n",
				vpi, vci);
		break;

	case T_ATM_SPANS_ADDR:
		pvc = 0;
		vpi = vci = 0;

		/*
		 * Check signalling state
		 */
		if (spp->sp_state != SPANS_ACTIVE) {
			return(ENETDOWN);
		}

		/*
		 *Check destination address length
		 */
		if (cvp->cvc_attr.called.addr.address_length !=
				sizeof(spans_addr)) {
			return(EINVAL);
		}
		break;

	default:
		return(EINVAL);
	}

	/*
	 * Check that this is for the same interface SPANS uses
         */
	if (!cvp->cvc_attr.nif ||
			cvp->cvc_attr.nif->nif_pif != spp->sp_pif) {
		return(EINVAL);
	}

	/*
	 * Check AAL
	 */
	if (!spans_get_spans_aal(cvp->cvc_attr.aal.type, &aal)) {
		return(EINVAL);
	}

#ifdef NOTDEF
	/*
	 * Check encapsulation
	 */
	/* XXX -- How do we check encapsulation? */
	if (cvp->ac_encaps != ATM_ENC_NULL) {
		return(EINVAL);
	}
#endif

	/*
	 * Allocate control block for VCC
	 */
	svp = uma_zalloc(spans_vc_zone, M_WAITOK);
	if (svp == NULL) {
		return(ENOMEM);
	}

	/*
	 * Fill in VCCB
	 */
	if (pvc) {
		svp->sv_type = VCC_PVC | VCC_IN | VCC_OUT;
		svp->sv_vpi = vpi;
		svp->sv_vci = vci;
		svp->sv_sstate = (spp->sp_state == SPANS_ACTIVE ?
				SPANS_VC_ACTIVE : SPANS_VC_ACT_DOWN);
		svp->sv_ustate = VCCU_OPEN;
	} else {
		svp->sv_type = VCC_SVC | VCC_OUT;
		spans_addr_copy(cvp->cvc_attr.called.addr.address,
				&svp->sv_conn.con_dst);
		spans_addr_copy(spp->sp_addr.address,
				&svp->sv_conn.con_src);
		svp->sv_conn.con_dsap = SPANS_SAP_IP;
		svp->sv_conn.con_ssap = spans_ephemeral_sap(spp);
		svp->sv_sstate = SPANS_VC_POPEN;
		svp->sv_ustate = VCCU_POPEN;
	}
	svp->sv_proto = ATM_SIG_SPANS;
	svp->sv_pif = spp->sp_pif;
	svp->sv_nif = cvp->cvc_attr.nif;
	svp->sv_connvc = cvp;
	svp->sv_spans_aal = aal;
	svp->sv_tstamp = time_second;

	/*
	 * Put VCCB on SPANS queue
	 */
	ENQUEUE(svp, struct spans_vccb, sv_sigelem, spp->sp_vccq);

	/*
	 * Link VCCB to VCC connection block
	 */
	cvp->cvc_vcc = (struct vccb *) svp;

	/*
	 * Start the SPANS message exchange if this is an SVC
	 */
	if (!pvc) {
		svp->sv_retry = 0;
		svp->sv_spans_qos.rsc_peak = 1;
		svp->sv_spans_qos.rsc_mean = 1;
		svp->sv_spans_qos.rsc_burst = 1;
		err = spans_send_open_req(spp, svp);
		if (err) {
			/*
			 * On error, delete the VCCB
			 */
			DEQUEUE(svp, struct spans_vccb, sv_sigelem,
					spp->sp_vccq);
			cvp->cvc_vcc = (struct vccb *)0;
			uma_zfree(spans_vc_zone, svp);
			return(err);
		} else {
			/*
			 * VCCB is opening--set the retransmit timer
			 */
			SPANS_VC_TIMER((struct vccb *) svp, SV_TIMEOUT);
		}
	}

	return(0);
}


/*
 * Close a SPANS VCC
 *
 * Called when a user wants to close a VCC.  This function will clean
 * up the VCCB and, for an SVC, send a close request.
 *
 * Must be called at splnet.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance
 *	svp	pointer to VCCB for the VCC to be closed
 *
 * Returns:
 *	0	VCC is now closed
 *	errno	error encountered
 */
int
spans_close_vcc(spp, svp, force)
	struct spans		*spp;
	struct spans_vccb	*svp;
	int			force;

{
	int		err = 0;

	ATM_DEBUG2("spans_close_vcc: svp=%p, state=%d\n", svp,
			svp->sv_sstate);

	/*
	 * Check that this is for the same interface SPANS uses
         */
	if (svp->sv_pif != spp->sp_pif) {
		return (EINVAL);
	}

	/*
	 * Kill any possible timer
	 */
	SPANS_VC_CANCEL((struct vccb *) svp);

	/*
	 * Mark the close time.
	 */
	svp->sv_tstamp = time_second;

	/*
	 * Process based on the connection type
	 */
	if (svp->sv_type & VCC_PVC) {
		svp->sv_sstate = SPANS_VC_FREE;
		svp->sv_ustate = VCCU_CLOSED;
	} else if (svp->sv_type & VCC_SVC) {
		/*
		 * Update VCCB states
		 */
		svp->sv_ustate = VCCU_CLOSED;

		/*
		 * Send the appropriate SPANS close message
		 */
		switch (svp->sv_sstate) {
		case SPANS_VC_R_POPEN:
			err = spans_send_open_rsp(spp, svp, SPANS_FAIL);
			svp->sv_sstate = SPANS_VC_FREE;
			break;
		case SPANS_VC_OPEN:
		case SPANS_VC_POPEN:
		case SPANS_VC_ABORT:
			svp->sv_retry = 0;
			err = spans_send_close_req(spp, svp);
			if (force) {
				svp->sv_sstate = SPANS_VC_FREE;
			} else {
				svp->sv_sstate = SPANS_VC_CLOSE;
				SPANS_VC_TIMER((struct vccb *) svp,
						SV_TIMEOUT);
			}
			break;
		case SPANS_VC_CLOSE:
			if (force) {
				svp->sv_sstate = SPANS_VC_FREE;
			}
			break;
		}
	}

	/*
	 * Wait for user to free resources
	 */
	return(err);
}


/*
 * Clear a SPANS VCC
 *
 * Called when the signalling manager wants to close a VCC immediately.
 * This function will clean up the VCCB and notify the owner.
 *
 * Must be called at splnet.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance
 *	svp	pointer to VCCB for the VCC to be closed
 *
 * Returns:
 *	0	VCC is now closed
 *	errno	error encountered
 */
int
spans_clear_vcc(spp, svp)
	struct spans		*spp;
	struct spans_vccb	*svp;

{
	u_char	outstate;

	ATM_DEBUG2("spans_clear_vcc: svp=%p, state=%d\n", svp,
			svp->sv_sstate);

	/*
	 * Check that this is for the same interface SPANS uses
         */
	if (svp->sv_pif != spp->sp_pif) {
		return (EINVAL);
	}

	/*
	 * Kill any possible timer
	 */
	SPANS_VC_CANCEL((struct vccb *) svp);

	/*
	 * Mark the close time
	 */
	svp->sv_tstamp = time_second;

	/*
	 * Mark the VCCB closed
	 */
	outstate = svp->sv_sstate;
	svp->sv_sstate = SPANS_VC_FREE;
	svp->sv_ustate = VCCU_CLOSED;

	/*
	 * Notify the user if old state indicates.
	 */
	switch (outstate) {
	case SPANS_VC_ACTIVE:
	case SPANS_VC_ACT_DOWN:
	case SPANS_VC_POPEN:
	case SPANS_VC_OPEN:
	case SPANS_VC_CLOSE:
	case SPANS_VC_ABORT:
		/* XXX -- set cause */
		atm_cm_cleared(svp->sv_connvc);
		break;
	case SPANS_VC_NULL:
	case SPANS_VC_R_POPEN:
	case SPANS_VC_FREE:
		break;
	}

	/*
	 * Wait for user to free resources
	 */
	return(0);
}


/*
 * Reset the switch state
 *
 * Called when the switch or host at the far end of the ATM link has
 * gone away.  This can be deteched either by a number of SPANS_STAT_REQ
 * messages going unanswered or by the host epoch changing in a SPANS
 * SPANS_STAT_IND or SPANS_STAT_REQ message.
 *
 * Arguments:
 *	spp	pointer to SPANS protocol instance
 *
 * Returns:
 *	none
 *
 */
void
spans_switch_reset(spp, cause)
	struct spans	*spp;
	int		cause;

{
	int		s;
	struct vccb	*vcp, *vnext;

	ATM_DEBUG2("spans_switch_reset: spp=%p, cause=%d\n",
			spp, cause);

	/*
	 * Log the event
	 */
	log(LOG_INFO, "spans: signalling %s on interface %s%d\n",
			(cause == SPANS_UNI_DOWN ? "down" : "up"),
			spp->sp_pif->pif_name,
			spp->sp_pif->pif_unit);

	/*
	 * Terminate all of our VCCs
	 */
	s = splnet();
	for (vcp = Q_HEAD(spp->sp_vccq, struct vccb); vcp;
			vcp = vnext) {

		u_char  outstate;

		vnext = Q_NEXT(vcp, struct vccb, vc_sigelem);

		if (vcp->vc_type & VCC_SVC) {
			/*
			 * Close the SVC and notify the owner
			 */
			outstate = vcp->vc_sstate;
			SPANS_VC_CANCEL((struct vccb *) vcp);
			vcp->vc_ustate = VCCU_CLOSED;
			vcp->vc_sstate = SPANS_VC_FREE;
			if (outstate == SPANS_VC_OPEN ||
					outstate == SPANS_VC_POPEN) {
				/* XXX -- set cause */
				atm_cm_cleared(vcp->vc_connvc);
			}
		} else if (vcp->vc_type & VCC_PVC) {
			/*
			 * Note new state
			 */
			switch(cause) {
			case SPANS_UNI_DOWN:
				vcp->vc_sstate = SPANS_VC_ACT_DOWN;
				break;
			case SPANS_UNI_UP:
				vcp->vc_sstate = SPANS_VC_ACTIVE;
				break;
			}
		} else {
			log(LOG_ERR, "spans: invalid VCC type: vccb=%p, type=%d\n",
					vcp, vcp->vc_type);
		}
	}
	(void) splx(s);
}
