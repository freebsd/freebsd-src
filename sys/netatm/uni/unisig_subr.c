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
 * ATM Forum UNI 3.0/3.1 Signalling Manager
 * ----------------------------------------
 *
 * Subroutines
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

#include <netatm/uni/unisig_var.h>
#include <netatm/uni/unisig_msg.h>

/*
 * External variables
 */
extern struct ie_aalp  ie_aalp_absent;
extern struct ie_clrt  ie_clrt_absent;
extern struct ie_bbcp  ie_bbcp_absent;
extern struct ie_bhli  ie_bhli_absent;
extern struct ie_blli  ie_blli_absent;
extern struct ie_clst  ie_clst_absent;
extern struct ie_cdad  ie_cdad_absent;
extern struct ie_cdsa  ie_cdsa_absent;
extern struct ie_cgad  ie_cgad_absent;
extern struct ie_cgsa  ie_cgsa_absent;
extern struct ie_caus  ie_caus_absent;
extern struct ie_cnid  ie_cnid_absent;
extern struct ie_qosp  ie_qosp_absent;
extern struct ie_brpi  ie_brpi_absent;
extern struct ie_rsti  ie_rsti_absent;
extern struct ie_blsh  ie_blsh_absent;
extern struct ie_bnsh  ie_bnsh_absent;
extern struct ie_bsdc  ie_bsdc_absent;
extern struct ie_trnt  ie_trnt_absent;
extern struct ie_eprf  ie_eprf_absent;
extern struct ie_epst  ie_epst_absent;


/*
 * Set a User Location cause code in an ATM attribute block
 *
 * Arguments:
 *	aap	pointer to attribute block
 *	cause	cause code
 *
 * Returns:
 *	none
 *
 */
void
unisig_cause_attr_from_user(aap, cause)
	Atm_attributes	*aap;
	int		cause;
{
	if (cause == T_ATM_ABSENT)
		return;

	/*
	 * Set the fields in the attribute block
	 */
	aap->cause.tag = T_ATM_PRESENT;
	aap->cause.v.coding_standard = T_ATM_ITU_CODING;
	aap->cause.v.location = T_ATM_LOC_USER;
	aap->cause.v.cause_value = cause;
	bzero(aap->cause.v.diagnostics,
			sizeof(aap->cause.v.diagnostics));
}


/*
 * Set a cause code in an ATM attribute block from a Cause IE
 *
 * Arguments:
 *	aap	pointer to attribute block
 *	iep	pointer to Cause IE
 *
 * Returns:
 *	none
 *
 */
void
unisig_cause_attr_from_ie(aap, iep)
	Atm_attributes		*aap;
	struct ie_generic	*iep;
{
	/*
	 * Set the fields in the attribute block
	 */
	aap->cause.tag = T_ATM_PRESENT;
	aap->cause.v.coding_standard = iep->ie_coding;
	aap->cause.v.location = iep->ie_caus_loc;
	aap->cause.v.cause_value = iep->ie_caus_cause;
	bzero(aap->cause.v.diagnostics, sizeof(aap->cause.v.diagnostics));
	bcopy(iep->ie_caus_diagnostic, aap->cause.v.diagnostics,
		MIN(sizeof(aap->cause.v.diagnostics), iep->ie_caus_diag_len));
}


/*
 * Open a UNI VCC
 *
 * Called when a user wants to open a VC.  This function will construct
 * a VCCB and, if we are opening an SVC, call the Q.2931 VC state
 * machine.  The user will have to wait for a notify event to be sure
 * the SVC is fully open.
 *
 * Must be called at splnet.
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance
 *	cvp	pointer to connection parameters for the VCC
 *
 * Returns:
 *	0	VCC creation successful
 *	errno	VCC setup failed - reason indicated
 *
 */
int
unisig_open_vcc(usp, cvp)
	struct unisig	*usp;
	Atm_connvc	*cvp;
{
	struct atm_pif		*pip = usp->us_pif;
	struct unisig_vccb	*uvp;
	Atm_addr_pvc		*pvp;
	int			err, pvc;

	ATM_DEBUG2("unisig_open_vcc: usp=%p, cvp=%p\n", usp, cvp);

	/*
	 * Validate user parameters.  AAL and encapsulation are
	 * checked by the connection manager
	 */

	/*
	 * Check called party address(es)
	 */
	if(cvp->cvc_attr.called.tag != T_ATM_PRESENT ||
			cvp->cvc_attr.called.addr.address_format ==
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
		if ((ATM_PVC_GET_VPI(pvp) > pip->pif_maxvpi) ||
				(ATM_PVC_GET_VCI(pvp) == 0) ||
				(ATM_PVC_GET_VCI(pvp) > pip->pif_maxvci)) {
			return(ERANGE);
		}

		/*
		 * Make sure VPI/VCI is not already in use
		 */
		if (unisig_find_vpvc(usp,
				ATM_PVC_GET_VPI(pvp),
				ATM_PVC_GET_VCI(pvp), 0)) {
			return(EEXIST);
		}
		ATM_DEBUG2("unisig_open_vcc: VPI.VCI=%d.%d\n",
				ATM_PVC_GET_VPI(pvp),
				ATM_PVC_GET_VCI(pvp));
		break;

	case T_ATM_ENDSYS_ADDR:
		/*
		 * Check signalling state
		 */
		pvc = 0;
		pvp = NULL;
		if (usp->us_state != UNISIG_ACTIVE) {
			return(ENETDOWN);
		}

		/*
		 * Make sure there's no subaddress
		 */
		if (cvp->cvc_attr.called.subaddr.address_format !=
				T_ATM_ABSENT) {
			return(EINVAL);
		}
		break;

	case T_ATM_E164_ADDR:
		/*
		 * Check signalling state
		 */
		pvc = 0;
		pvp = NULL;
		if (usp->us_state != UNISIG_ACTIVE) {
			return(ENETDOWN);
		}

		/*
		 * Check destination address format
		 */
		if (cvp->cvc_attr.called.subaddr.address_format !=
					T_ATM_ENDSYS_ADDR &&
				cvp->cvc_attr.called.subaddr.address_format !=
					T_ATM_ABSENT) {
			return(EINVAL);
		}
		break;

	default:
		return(EPROTONOSUPPORT);
	}

	/*
	 * Check that this is for the same interface UNISIG uses
	 */
	if (!cvp->cvc_attr.nif ||
			cvp->cvc_attr.nif->nif_pif != usp->us_pif) {
		return(EINVAL);
	}

	/*
	 * Allocate control block for VCC
	 * May be called from timeout - don't wait.
	 */
	uvp = uma_zalloc(unisig_vc_zone, M_NOWAIT | M_ZERO);
	if (uvp == NULL) {
		return(ENOMEM);
	}

	/*
	 * Fill in VCCB
	 */
	if (pvc) {
		uvp->uv_type = VCC_PVC | VCC_IN | VCC_OUT;
		uvp->uv_vpi = ATM_PVC_GET_VPI(pvp);
		uvp->uv_vci = ATM_PVC_GET_VCI(pvp);
		uvp->uv_sstate = (usp->us_state == UNISIG_ACTIVE ?
				UNI_PVC_ACTIVE : UNI_PVC_ACT_DOWN);
		uvp->uv_ustate = VCCU_OPEN;
	} else {
		uvp->uv_type = VCC_SVC | VCC_IN | VCC_OUT;
		uvp->uv_sstate = UNI_NULL;
		uvp->uv_ustate = VCCU_POPEN;
	}
	uvp->uv_proto = usp->us_pif->pif_sigmgr->sm_proto;
	uvp->uv_pif = usp->us_pif;
	uvp->uv_nif = cvp->cvc_attr.nif;
	uvp->uv_connvc = cvp;
	uvp->uv_tstamp = time_second;

	/*
	 * Put VCCB on UNISIG queue
	 */
	ENQUEUE(uvp, struct unisig_vccb, uv_sigelem, usp->us_vccq);

	/*
	 * Call the VC state machine if this is an SVC
	 */
	if (!pvc) {
		err = unisig_vc_state(usp, uvp, UNI_VC_SETUP_CALL,
				(struct unisig_msg *) 0);
		if (err) {
			/*
			 * On error, delete the VCCB
			 */
			DEQUEUE(uvp, struct unisig_vccb, uv_sigelem,
					usp->us_vccq);
			uma_zfree(unisig_vc_zone, uvp);
			return(err);
		}
	}

	/*
	 * Link VCCB to VCC connection block
	 */
	cvp->cvc_vcc = (struct vccb *) uvp;

	return(0);
}


/*
 * Close a UNISIG VCC
 *
 * Called when a user wants to close a VCC.  This function will clean
 * up the VCCB and, for an SVC, send a close request.
 *
 * Must be called at splnet.
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance
 *	uvp	pointer to VCCB for the VCC to be closed
 *
 * Returns:
 *	0	VCC is now closed
 *	errno	error encountered
 */
int
unisig_close_vcc(usp, uvp)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
{
	int		err = 0;

	ATM_DEBUG2("unisig_close_vcc: uvp=%p, state=%d\n", uvp,
			uvp->uv_sstate);

	/*
	 * Check that this is for the same interface UNISIG uses
	 */
	if (uvp->uv_pif != usp->us_pif) {
		return (EINVAL);
	}

	/*
	 * Mark the close time.
	 */
	uvp->uv_tstamp = time_second;

	/*
	 * Process based on the connection type
	 */
	if (uvp->uv_type & VCC_PVC) {
		uvp->uv_sstate = UNI_FREE;
		uvp->uv_ustate = VCCU_CLOSED;
	} else if (uvp->uv_type & VCC_SVC) {
		/*
		 * Call the VC state machine
		 */
		uvp->uv_ustate = VCCU_CLOSED;
		err = unisig_vc_state(usp, uvp, UNI_VC_RELEASE_CALL,
				(struct unisig_msg *) 0);
	}

	/*
	 * Wait for user to free resources
	 */
	return(err);
}


/*
 * Clear a UNISIG VCC
 *
 * Called to internally clear a VCC.  No external protocol is
 * initiated, the VCC is just closed and the owner is notified.
 *
 * Must be called at splnet.
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance
 *	uvp	pointer to VCCB for the VCC to be closed
 *	cause	cause code giving the reason for the close
 *
 * Returns:
 *	0	VCC is closed
 *	errno	error encountered
 */
int
unisig_clear_vcc(usp, uvp, cause)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	int			cause;
{
	u_char	outstate;

	ATM_DEBUG3("unisig_clear_vcc: uvp=%p, state=%d, cause=%d\n",
			uvp, uvp->uv_sstate, cause);

	/*
	 * Check that this is for the same interface UNISIG uses
	 */
	if (uvp->uv_pif != usp->us_pif) {
		return (EINVAL);
	}

	/*
	 * Kill any possible timer
	 */
	UNISIG_VC_CANCEL((struct vccb *) uvp);

	/*
	 * Mark the close time.
	 */
	uvp->uv_tstamp = time_second;

	/*
	 * Close the VCC and notify the user
	 */
	outstate = uvp->uv_sstate;
	uvp->uv_sstate = UNI_FREE;
	uvp->uv_ustate = VCCU_CLOSED;
	if (outstate == UNI_ACTIVE ||
			outstate == UNI_CALL_INITIATED ||
			outstate == UNI_CALL_OUT_PROC ||
			outstate == UNI_CONNECT_REQUEST ||
			outstate == UNI_RELEASE_REQUEST ||
			outstate == UNI_RELEASE_IND ||
			outstate == UNI_SSCF_RECOV ||
			outstate == UNI_PVC_ACT_DOWN ||
			outstate == UNI_PVC_ACTIVE) {
		unisig_cause_attr_from_user(&uvp->uv_connvc->cvc_attr, cause);
		atm_cm_cleared(uvp->uv_connvc);
	}

	/*
	 * Wait for user to free resources
	 */
	return(0);
}


#ifdef NOTDEF
/*
 * Reset the switch state
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance
 *
 * Returns:
 *	none
 *
 */
void
unisig_switch_reset(usp, cause)
	struct unisig	*usp;
	int		cause;
{
	int			s;
	struct unisig_vccb	*uvp, *vnext;

	ATM_DEBUG2("unisig_switch_reset: usp=%p, cause=%d\n",
			usp, cause);

	/*
	 * Terminate all of our VCCs
	 */
	s = splnet();
	for (uvp = Q_HEAD(usp->us_vccq, struct unisig_vccb); uvp;
			uvp = vnext) {
		vnext = Q_NEXT(uvp, struct unisig_vccb, uv_sigelem);

		if (uvp->uv_type & VCC_SVC) {
			/*
			 * Close the SVC and notify the owner
			 */
			(void)unisig_clear_vcc(usp, uvp,
					T_ATM_CAUSE_NORMAL_CALL_CLEARING);
		} else if (uvp->uv_type & VCC_PVC) {
			/*
			 * Notify PVC owner of the state change
			 */
			switch(cause) {
			case UNI_DOWN:
				uvp->uv_sstate = UNI_PVC_ACT_DOWN;
				break;
			case UNI_UP:
				uvp->uv_sstate = UNI_PVC_ACTIVE;
				break;
			}
			atm_cm_cleared(uvp->uv_connvc, cause);
		} else {
			log(LOG_ERR, "unisig: invalid VCC type: vccb=%p, type=%d\n",
					uvp, uvp->uv_type);
		}
	}
	(void) splx(s);
}
#endif


/*
 * Copy connection parameters from UNI 3.0 message IEs into
 * an attribute block
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance
 *	msg	pointer to the SETUP message
 *	ap	pointer to the attribute block
 *
 * Returns:
 *	none
 *
 */
void
unisig_save_attrs(usp, msg, ap)
	struct unisig		*usp;
	struct unisig_msg	*msg;
	Atm_attributes		*ap;
{
	/*
	 * Sanity check
	 */
	if (!msg || !ap)
		return;

	/*
	 * Save the AAL parameters (AAL 3/4 and AAL 5 only)
	 */
	if (msg->msg_ie_aalp) {
		struct ie_generic	*aalp = msg->msg_ie_aalp;

		switch(msg->msg_ie_aalp->ie_aalp_aal_type) {
		case UNI_IE_AALP_AT_AAL3:
			ap->aal.tag = T_ATM_PRESENT;
			ap->aal.type =
				msg->msg_ie_aalp->ie_aalp_aal_type;
			ap->aal.v.aal4.forward_max_SDU_size =
				msg->msg_ie_aalp->ie_aalp_4_fwd_max_sdu;
			ap->aal.v.aal4.backward_max_SDU_size =
				msg->msg_ie_aalp->ie_aalp_4_bkwd_max_sdu;
			ap->aal.v.aal4.SSCS_type =
				msg->msg_ie_aalp->ie_aalp_4_sscs_type;
			if (aalp->ie_aalp_4_mid_range == T_ATM_ABSENT) {
				ap->aal.v.aal4.mid_low = T_ATM_ABSENT;
				ap->aal.v.aal4.mid_high = T_ATM_ABSENT;
			} else {
				if (usp->us_proto == ATM_SIG_UNI30) {
					ap->aal.v.aal4.mid_low = 0;
					ap->aal.v.aal4.mid_high =
						aalp->ie_aalp_4_mid_range
							& UNI_IE_AALP_A3_R_MASK;
				} else {
					ap->aal.v.aal4.mid_low =
						(aalp->ie_aalp_4_mid_range >>
							UNI_IE_AALP_A3_R_SHIFT)
							& UNI_IE_AALP_A3_R_MASK;
					ap->aal.v.aal4.mid_high =
						aalp->ie_aalp_4_mid_range
							& UNI_IE_AALP_A3_R_MASK;
				}
			}
			break;
		case UNI_IE_AALP_AT_AAL5:
			ap->aal.tag = T_ATM_PRESENT;
			ap->aal.type =
				msg->msg_ie_aalp->ie_aalp_aal_type;
			ap->aal.v.aal5.forward_max_SDU_size =
				msg->msg_ie_aalp->ie_aalp_5_fwd_max_sdu;
			ap->aal.v.aal5.backward_max_SDU_size =
				msg->msg_ie_aalp->ie_aalp_5_bkwd_max_sdu;
			ap->aal.v.aal5.SSCS_type =
				msg->msg_ie_aalp->ie_aalp_5_sscs_type;
			break;
		}
	}

	/*
	 * Save traffic descriptor attributes
	 */
	if (msg->msg_ie_clrt) {
		ap->traffic.tag = T_ATM_PRESENT;
		ap->traffic.v.forward.PCR_high_priority =
				msg->msg_ie_clrt->ie_clrt_fwd_peak;
		ap->traffic.v.forward.PCR_all_traffic =
				msg->msg_ie_clrt->ie_clrt_fwd_peak_01;
		ap->traffic.v.forward.SCR_high_priority =
				msg->msg_ie_clrt->ie_clrt_fwd_sust;
		ap->traffic.v.forward.SCR_all_traffic =
				msg->msg_ie_clrt->ie_clrt_fwd_sust_01;
		ap->traffic.v.forward.MBS_high_priority =
				msg->msg_ie_clrt->ie_clrt_fwd_burst;
		ap->traffic.v.forward.MBS_all_traffic =
				msg->msg_ie_clrt->ie_clrt_fwd_burst_01;
		ap->traffic.v.backward.PCR_high_priority =
				msg->msg_ie_clrt->ie_clrt_bkwd_peak;
		ap->traffic.v.backward.PCR_all_traffic =
				msg->msg_ie_clrt->ie_clrt_bkwd_peak_01;
		ap->traffic.v.backward.SCR_high_priority =
				msg->msg_ie_clrt->ie_clrt_bkwd_sust;
		ap->traffic.v.backward.SCR_all_traffic =
				msg->msg_ie_clrt->ie_clrt_bkwd_sust_01;
		ap->traffic.v.backward.MBS_high_priority =
				msg->msg_ie_clrt->ie_clrt_bkwd_burst;
		ap->traffic.v.backward.MBS_all_traffic =
				msg->msg_ie_clrt->ie_clrt_bkwd_burst_01;
		ap->traffic.v.best_effort =
				msg->msg_ie_clrt->ie_clrt_best_effort;
		if (msg->msg_ie_clrt->ie_clrt_tm_options ==
				T_ATM_ABSENT) {
			ap->traffic.v.forward.tagging = T_NO;
			ap->traffic.v.backward.tagging = T_NO;
		} else {
			ap->traffic.v.forward.tagging =
					(msg->msg_ie_clrt->ie_clrt_tm_options &
					UNI_IE_CLRT_TM_FWD_TAG) != 0;
			ap->traffic.v.backward.tagging =
					(msg->msg_ie_clrt->ie_clrt_tm_options &
					UNI_IE_CLRT_TM_BKWD_TAG) != 0;
		}
	}

	/*
	 * Save broadband bearer attributes
	 */
	if (msg->msg_ie_bbcp) {
		ap->bearer.tag = T_ATM_PRESENT;
		ap->bearer.v.bearer_class =
				msg->msg_ie_bbcp->ie_bbcp_bearer_class;
		ap->bearer.v.traffic_type =
				msg->msg_ie_bbcp->ie_bbcp_traffic_type;
		ap->bearer.v.timing_requirements =
				msg->msg_ie_bbcp->ie_bbcp_timing_req;
		ap->bearer.v.clipping_susceptibility =
				msg->msg_ie_bbcp->ie_bbcp_clipping;
		ap->bearer.v.connection_configuration =
				msg->msg_ie_bbcp->ie_bbcp_conn_config;
	}

	/*
	 * Save broadband high layer attributes
	 */
	if (msg->msg_ie_bhli) {
		ap->bhli.tag = T_ATM_PRESENT;
		ap->bhli.v.ID_type = msg->msg_ie_bhli->ie_bhli_type;
		switch(ap->bhli.v.ID_type) {
		case T_ATM_ISO_APP_ID:
			bcopy(msg->msg_ie_bhli->ie_bhli_info,
					ap->bhli.v.ID.ISO_ID,
					sizeof(ap->bhli.v.ID.ISO_ID));
			break;
		case T_ATM_USER_APP_ID:
			bcopy(msg->msg_ie_bhli->ie_bhli_info,
					ap->bhli.v.ID.user_defined_ID,
					sizeof(ap->bhli.v.ID.user_defined_ID));
			break;
		case T_ATM_VENDOR_APP_ID:
			bcopy(msg->msg_ie_bhli->ie_bhli_info,
					ap->bhli.v.ID.vendor_ID.OUI,
					sizeof(ap->bhli.v.ID.vendor_ID.OUI));
			bcopy(&msg->msg_ie_bhli->ie_bhli_info[sizeof(ap->bhli.v.ID.vendor_ID.OUI)-1],
					ap->bhli.v.ID.vendor_ID.app_ID,
					sizeof(ap->bhli.v.ID.vendor_ID.app_ID));
			break;
		}
	}

	/*
	 * Save Broadband low layer, user layer 2 and 3 attributes
	 */
	if (msg->msg_ie_blli) {
		/*
		 * Layer 2 parameters
		 */
		switch(msg->msg_ie_blli->ie_blli_l2_id) {
		case UNI_IE_BLLI_L2P_ISO1745:
		case UNI_IE_BLLI_L2P_Q921:
		case UNI_IE_BLLI_L2P_X25L:
		case UNI_IE_BLLI_L2P_X25M:
		case UNI_IE_BLLI_L2P_LAPB:
		case UNI_IE_BLLI_L2P_HDLC1:
		case UNI_IE_BLLI_L2P_HDLC2:
		case UNI_IE_BLLI_L2P_HDLC3:
		case UNI_IE_BLLI_L2P_LLC:
		case UNI_IE_BLLI_L2P_X75:
		case UNI_IE_BLLI_L2P_Q922:
		case UNI_IE_BLLI_L2P_ISO7776:
			ap->blli.tag_l2 = T_ATM_PRESENT;
			ap->blli.v.layer_2_protocol.ID_type =
					T_ATM_SIMPLE_ID;
			ap->blli.v.layer_2_protocol.ID.simple_ID =
					msg->msg_ie_blli->ie_blli_l2_id;
			break;
		case UNI_IE_BLLI_L2P_USER:
			ap->blli.tag_l2 = T_ATM_PRESENT;
			ap->blli.v.layer_2_protocol.ID_type =
					T_ATM_USER_ID;
			ap->blli.v.layer_2_protocol.ID.user_defined_ID =
					msg->msg_ie_blli->ie_blli_l2_user_proto;
			break;
		default:
			ap->blli.tag_l2 = T_ATM_ABSENT;
		}
		if (ap->blli.tag_l2 == T_ATM_PRESENT) {
			ap->blli.v.layer_2_protocol.mode =
					msg->msg_ie_blli->ie_blli_l2_mode;
			ap->blli.v.layer_2_protocol.window_size =
					msg->msg_ie_blli->ie_blli_l2_window;
		}

		/*
		 * Layer 3 parameters
		 */
		switch(msg->msg_ie_blli->ie_blli_l3_id) {
		case UNI_IE_BLLI_L3P_X25:
		case UNI_IE_BLLI_L3P_ISO8208:
		case UNI_IE_BLLI_L3P_ISO8878:
		case UNI_IE_BLLI_L3P_ISO8473:
		case UNI_IE_BLLI_L3P_T70:
			ap->blli.tag_l3 = T_ATM_PRESENT;
			ap->blli.v.layer_3_protocol.ID_type =
					T_ATM_SIMPLE_ID;
			ap->blli.v.layer_3_protocol.ID.simple_ID =
					msg->msg_ie_blli->ie_blli_l3_id;
			break;
		case UNI_IE_BLLI_L3P_ISO9577:
			ap->blli.tag_l3 = T_ATM_PRESENT;
			ap->blli.v.layer_3_protocol.ID_type =
					T_ATM_SIMPLE_ID;
			ap->blli.v.layer_3_protocol.ID.simple_ID =
					msg->msg_ie_blli->ie_blli_l3_id;
			if (msg->msg_ie_blli->ie_blli_l3_ipi ==
					UNI_IE_BLLI_L3IPI_SNAP) {
				bcopy(msg->msg_ie_blli->ie_blli_l3_oui,
						ap->blli.v.layer_3_protocol.ID.SNAP_ID.OUI,
						sizeof(ap->blli.v.layer_3_protocol.ID.SNAP_ID.OUI));
				bcopy(msg->msg_ie_blli->ie_blli_l3_pid,
						ap->blli.v.layer_3_protocol.ID.SNAP_ID.PID,
						sizeof(ap->blli.v.layer_3_protocol.ID.SNAP_ID.PID));
			} else {
				ap->blli.v.layer_3_protocol.ID.IPI_ID =
						msg->msg_ie_blli->ie_blli_l3_ipi;
			}
			break;
		case UNI_IE_BLLI_L3P_USER:
			ap->blli.tag_l3 = T_ATM_PRESENT;
			ap->blli.v.layer_3_protocol.ID_type =
					T_ATM_USER_ID;
			ap->blli.v.layer_3_protocol.ID.user_defined_ID =
					msg->msg_ie_blli->ie_blli_l3_user_proto;
			break;
		default:
			ap->blli.tag_l3 = T_ATM_ABSENT;
		}
		if (ap->blli.tag_l3 == T_ATM_PRESENT) {
			ap->blli.v.layer_3_protocol.mode =
					msg->msg_ie_blli->ie_blli_l3_mode;
			ap->blli.v.layer_3_protocol.packet_size =
					msg->msg_ie_blli->ie_blli_l3_packet_size;
			ap->blli.v.layer_3_protocol.window_size =
					msg->msg_ie_blli->ie_blli_l3_window;
		}
	}

	/*
	 * Save the called party address and subaddress
	 */
	if (msg->msg_ie_cdad) {
		ap->called.tag = T_ATM_PRESENT;
		ATM_ADDR_COPY(&msg->msg_ie_cdad->ie_cdad_addr,
				&ap->called.addr);
		ap->called.subaddr.address_format = T_ATM_ABSENT;
		ap->called.subaddr.address_length = 0;
	}
	if (msg->msg_ie_cdsa) {
		ATM_ADDR_COPY(&msg->msg_ie_cdsa->ie_cdsa_addr,
				&ap->called.subaddr);
	}

	/*
	 * Save the calling party address and subaddress
	 */
	if (msg->msg_ie_cgad) {
		ap->calling.tag = T_ATM_PRESENT;
		ATM_ADDR_COPY(&msg->msg_ie_cgad->ie_cgad_addr,
				&ap->calling.addr);
		ap->calling.subaddr.address_format = T_ATM_ABSENT;
		ap->calling.subaddr.address_length = 0;
	}

	if (msg->msg_ie_cgsa) {
		ATM_ADDR_COPY(&msg->msg_ie_cgsa->ie_cgsa_addr,
				&ap->calling.subaddr);
	}

	/*
	 * Save quality of service attributes
	 */
	if (msg->msg_ie_qosp) {
		ap->qos.tag = T_ATM_PRESENT;
		ap->qos.v.coding_standard = msg->msg_ie_qosp->ie_coding;
		ap->qos.v.forward.qos_class = msg->msg_ie_qosp->ie_qosp_fwd_class;
		ap->qos.v.forward.qos_class =
				msg->msg_ie_qosp->ie_qosp_bkwd_class;
	}

	/*
	 * Save transit network attributes
	 */
	if (msg->msg_ie_trnt) {
		ap->transit.tag = T_ATM_PRESENT;
		ap->transit.v.length = 
				MIN(msg->msg_ie_trnt->ie_trnt_id_len,
				sizeof(ap->transit.v.network_id));
		bcopy(msg->msg_ie_trnt->ie_trnt_id,
				ap->transit.v.network_id,
				ap->transit.v.length);
	}

	/*
	 * Save cause code
	 */
	if (msg->msg_ie_caus) {
		ap->cause.tag = T_ATM_PRESENT;
		ap->cause.v.coding_standard =
				msg->msg_ie_caus->ie_coding;
		ap->cause.v.location = 
				msg->msg_ie_caus->ie_caus_loc;
		ap->cause.v.cause_value = 
				msg->msg_ie_caus->ie_caus_cause;
		bzero(ap->cause.v.diagnostics,
				sizeof(ap->cause.v.diagnostics));
#ifdef NOTDEF
		bcopy(msg->msg_ie_caus->ie_caus_diagnostic,
				ap->transit.v.diagnostics,
				MIN(sizeof(ap->transit.v.diagnostics),
				msg->msg_ie_caus->ie_caus_diag_len));
#endif
	}
}


/*
 * Copy connection parameters from an attribute block into
 * UNI 3.0 message IEs
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance
 *	msg	pointer to the SETUP message
 *	ap	pointer to the attribute block
 *
 * Returns:
 *	0	everything OK
 *	else	error encountered
 *
 * May be called from timeout so make allocations non-waiting
 */
int
unisig_set_attrs(usp, msg, ap)
	struct unisig		*usp;
	struct unisig_msg	*msg;
	Atm_attributes		*ap;
{
	int			err = 0;

	/*
	 * Sanity check
	 */
	if (!msg || !ap)
		return(EINVAL);

	/*
	 * Set the AAL parameters (AAL 3/4 and AAL 5 only)
	 */
	if (ap->aal.tag == T_ATM_PRESENT) {
		if (!msg->msg_ie_aalp) {
			msg->msg_ie_aalp = uma_zalloc(unisig_ie_zone,
			    M_NOWAIT | M_ZERO);
			if (msg->msg_ie_aalp == NULL) {
				err = ENOMEM;
				goto done;
			}
		}
		bcopy(&ie_aalp_absent,
				&msg->msg_ie_aalp->ie_u.ie_aalp,
				sizeof(ie_aalp_absent));
		msg->msg_ie_aalp->ie_ident = UNI_IE_AALP;
		msg->msg_ie_aalp->ie_aalp_aal_type = ap->aal.type;
		switch(ap->aal.type) {
		case ATM_AAL3_4:
			msg->msg_ie_aalp->ie_aalp_4_fwd_max_sdu =
					ap->aal.v.aal4.forward_max_SDU_size;
			msg->msg_ie_aalp->ie_aalp_4_bkwd_max_sdu =
					ap->aal.v.aal4.backward_max_SDU_size;
			msg->msg_ie_aalp->ie_aalp_4_mode = UNI_IE_AALP_A5_M_MSG;
			msg->msg_ie_aalp->ie_aalp_4_sscs_type =
					ap->aal.v.aal4.SSCS_type;
			if (ap->aal.v.aal4.mid_low == T_ATM_ABSENT) {
				msg->msg_ie_aalp->ie_aalp_4_mid_range =
					T_ATM_ABSENT;
			} else {
				if (usp->us_proto == ATM_SIG_UNI30) {
					msg->msg_ie_aalp->ie_aalp_4_mid_range =
						ap->aal.v.aal4.mid_high &
							UNI_IE_AALP_A3_R_MASK;
				} else {
					msg->msg_ie_aalp->ie_aalp_4_mid_range =
						((ap->aal.v.aal4.mid_low &
							UNI_IE_AALP_A3_R_MASK)
						    << UNI_IE_AALP_A3_R_SHIFT)
						  |
						 (ap->aal.v.aal4.mid_high &
							UNI_IE_AALP_A3_R_MASK);
				}
			}
			break;
		case ATM_AAL5:
			msg->msg_ie_aalp->ie_aalp_5_fwd_max_sdu =
					ap->aal.v.aal5.forward_max_SDU_size;
			msg->msg_ie_aalp->ie_aalp_5_bkwd_max_sdu =
					ap->aal.v.aal5.backward_max_SDU_size;
			msg->msg_ie_aalp->ie_aalp_5_mode =
					UNI_IE_AALP_A5_M_MSG;
			msg->msg_ie_aalp->ie_aalp_5_sscs_type =
					ap->aal.v.aal5.SSCS_type;
			break;
		}
	}

	/*
	 * Set traffic descriptor attributes
	 */
	if (ap->traffic.tag == T_ATM_PRESENT) {
		if (!msg->msg_ie_clrt) {
			msg->msg_ie_clrt = uma_zalloc(unisig_ie_zone,
			    M_NOWAIT | M_ZERO);
			if (msg->msg_ie_clrt == NULL) {
				err = ENOMEM;
				goto done;
			}
		}
		bcopy(&ie_clrt_absent,
				&msg->msg_ie_clrt->ie_u.ie_clrt,
				sizeof(ie_clrt_absent));
		msg->msg_ie_clrt->ie_ident = UNI_IE_CLRT;
		msg->msg_ie_clrt->ie_clrt_fwd_peak =
				ap->traffic.v.forward.PCR_high_priority;
		msg->msg_ie_clrt->ie_clrt_fwd_peak_01 =
				ap->traffic.v.forward.PCR_all_traffic;
		msg->msg_ie_clrt->ie_clrt_fwd_sust =
				ap->traffic.v.forward.SCR_high_priority;
		msg->msg_ie_clrt->ie_clrt_fwd_sust_01 =
				ap->traffic.v.forward.SCR_all_traffic;
		msg->msg_ie_clrt->ie_clrt_fwd_burst =
				ap->traffic.v.forward.MBS_high_priority;
		msg->msg_ie_clrt->ie_clrt_fwd_burst_01 =
				ap->traffic.v.forward.MBS_all_traffic;
		msg->msg_ie_clrt->ie_clrt_bkwd_peak =
				ap->traffic.v.backward.PCR_high_priority;
		msg->msg_ie_clrt->ie_clrt_bkwd_peak_01 =
				ap->traffic.v.backward.PCR_all_traffic;
		msg->msg_ie_clrt->ie_clrt_bkwd_sust =
				ap->traffic.v.backward.SCR_high_priority;
		msg->msg_ie_clrt->ie_clrt_bkwd_sust_01 =
				ap->traffic.v.backward.SCR_all_traffic;
		msg->msg_ie_clrt->ie_clrt_bkwd_burst =
				ap->traffic.v.backward.MBS_high_priority;
		msg->msg_ie_clrt->ie_clrt_bkwd_burst_01 =
				ap->traffic.v.backward.MBS_all_traffic;
		msg->msg_ie_clrt->ie_clrt_best_effort =
				ap->traffic.v.best_effort;
		msg->msg_ie_clrt->ie_clrt_tm_options = 0;
		if (ap->traffic.v.forward.tagging) {
			msg->msg_ie_clrt->ie_clrt_tm_options |=
					UNI_IE_CLRT_TM_FWD_TAG;
		}
		if (ap->traffic.v.backward.tagging) {
			msg->msg_ie_clrt->ie_clrt_tm_options |=
					UNI_IE_CLRT_TM_BKWD_TAG;
		}
		if (msg->msg_ie_clrt->ie_clrt_tm_options == 0) {
			msg->msg_ie_clrt->ie_clrt_tm_options =
					T_ATM_ABSENT;
		}
	}

	/*
	 * Set broadband bearer attributes
	 */
	if (ap->bearer.tag == T_ATM_PRESENT) {
		if (!msg->msg_ie_bbcp) {
			msg->msg_ie_bbcp = uma_zalloc(unisig_ie_zone,
			    M_NOWAIT | M_ZERO);
			if (msg->msg_ie_bbcp == NULL) {
				err = ENOMEM;
				goto done;
			}
		}
		bcopy(&ie_bbcp_absent,
				&msg->msg_ie_bbcp->ie_u.ie_bbcp,
				sizeof(ie_bbcp_absent));
		msg->msg_ie_bbcp->ie_ident = UNI_IE_BBCP;
		msg->msg_ie_bbcp->ie_bbcp_bearer_class =
				ap->bearer.v.bearer_class;
		msg->msg_ie_bbcp->ie_bbcp_traffic_type =
				ap->bearer.v.traffic_type;
		msg->msg_ie_bbcp->ie_bbcp_timing_req =
				ap->bearer.v.timing_requirements;
		msg->msg_ie_bbcp->ie_bbcp_clipping =
				ap->bearer.v.clipping_susceptibility;
		msg->msg_ie_bbcp->ie_bbcp_conn_config =
				ap->bearer.v.connection_configuration;
	}

	/*
	 * Set broadband high layer attributes
	 */
	if (ap->bhli.tag == T_ATM_PRESENT) {
		if (!msg->msg_ie_bhli) {
			msg->msg_ie_bhli = uma_zalloc(unisig_ie_zone,
			    M_NOWAIT | M_ZERO);
			if (msg->msg_ie_bhli == NULL) {
				err = ENOMEM;
				goto done;
			}
		}
		bcopy(&ie_bhli_absent,
				&msg->msg_ie_bhli->ie_u.ie_bhli,
				sizeof(ie_bhli_absent));
		msg->msg_ie_bhli->ie_ident = UNI_IE_BHLI;
		msg->msg_ie_bhli->ie_bhli_type = ap->bhli.v.ID_type;
		switch (ap->bhli.v.ID_type) {
		case T_ATM_ISO_APP_ID:
			bcopy(ap->bhli.v.ID.ISO_ID,
					msg->msg_ie_bhli->ie_bhli_info,
					sizeof(ap->bhli.v.ID.ISO_ID));
			break;
		case T_ATM_USER_APP_ID:
			bcopy(ap->bhli.v.ID.user_defined_ID,
					msg->msg_ie_bhli->ie_bhli_info,
					sizeof(ap->bhli.v.ID.user_defined_ID));
			break;
		case T_ATM_VENDOR_APP_ID:
			bcopy(ap->bhli.v.ID.vendor_ID.OUI,
					msg->msg_ie_bhli->ie_bhli_info,
					sizeof(ap->bhli.v.ID.vendor_ID.OUI));
			bcopy(ap->bhli.v.ID.vendor_ID.app_ID,
					&msg->msg_ie_bhli->ie_bhli_info[sizeof(ap->bhli.v.ID.vendor_ID.OUI)-1],
					sizeof(ap->bhli.v.ID.vendor_ID.app_ID));
			break;
		}
	}

	/*
	 * Set Broadband low layer, user layer 2 and 3 attributes
	 */
	if (ap->blli.tag_l2 == T_ATM_PRESENT ||
			ap->blli.tag_l3 == T_ATM_PRESENT) {
		if (!msg->msg_ie_blli) {
			msg->msg_ie_blli = uma_zalloc(unisig_ie_zone,
			    M_NOWAIT | M_ZERO);
			if (msg->msg_ie_blli == NULL) {
				err = ENOMEM;
				goto done;
			}
		}
		bcopy(&ie_blli_absent,
				&msg->msg_ie_blli->ie_u.ie_blli,
				sizeof(ie_blli_absent));
		msg->msg_ie_blli->ie_ident = UNI_IE_BLLI;

		if (ap->blli.tag_l2 == T_ATM_PRESENT) {
			switch(ap->blli.v.layer_2_protocol.ID_type) {
			case T_ATM_SIMPLE_ID:
				msg->msg_ie_blli->ie_blli_l2_id =
						ap->blli.v.layer_2_protocol.ID.simple_ID;
				break;
			case T_ATM_USER_ID:
				msg->msg_ie_blli->ie_blli_l2_id =
						UNI_IE_BLLI_L2P_USER;
				msg->msg_ie_blli->ie_blli_l2_user_proto =
						ap->blli.v.layer_2_protocol.ID.user_defined_ID;
				break;
			}
			if (ap->blli.v.layer_2_protocol.ID_type !=
					T_ATM_ABSENT) {
				msg->msg_ie_blli->ie_blli_l2_mode =
						ap->blli.v.layer_2_protocol.mode;
				msg->msg_ie_blli->ie_blli_l2_window =
						ap->blli.v.layer_2_protocol.window_size;
			}
		}

		if (ap->blli.tag_l3 == T_ATM_PRESENT) {
			switch (ap->blli.v.layer_3_protocol.ID_type) {
			case T_ATM_SIMPLE_ID:
				msg->msg_ie_blli->ie_blli_l3_id =
						ap->blli.v.layer_3_protocol.ID.simple_ID;
				break;

			case T_ATM_IPI_ID:
				msg->msg_ie_blli->ie_blli_l3_id =
						UNI_IE_BLLI_L3P_ISO9577;
				msg->msg_ie_blli->ie_blli_l3_ipi =
						ap->blli.v.layer_3_protocol.ID.IPI_ID;
				break;

			case T_ATM_SNAP_ID:
				msg->msg_ie_blli->ie_blli_l3_id =
						UNI_IE_BLLI_L3P_ISO9577;
				msg->msg_ie_blli->ie_blli_l3_ipi =
						UNI_IE_BLLI_L3IPI_SNAP;
				bcopy(ap->blli.v.layer_3_protocol.ID.SNAP_ID.OUI,
						msg->msg_ie_blli->ie_blli_l3_oui,
						sizeof(msg->msg_ie_blli->ie_blli_l3_oui));
				bcopy(ap->blli.v.layer_3_protocol.ID.SNAP_ID.PID,
						msg->msg_ie_blli->ie_blli_l3_pid,
						sizeof(msg->msg_ie_blli->ie_blli_l3_pid));
				break;

			case T_ATM_USER_ID:
				msg->msg_ie_blli->ie_blli_l3_id =
						UNI_IE_BLLI_L3P_USER;
				msg->msg_ie_blli->ie_blli_l3_user_proto =
						ap->blli.v.layer_3_protocol.ID.user_defined_ID;
				break;
			}
			if (ap->blli.v.layer_3_protocol.ID_type
					!= T_ATM_ABSENT) {
				msg->msg_ie_blli->ie_blli_l3_mode =
						ap->blli.v.layer_3_protocol.mode;
				msg->msg_ie_blli->ie_blli_l3_packet_size =
						ap->blli.v.layer_3_protocol.packet_size;
				msg->msg_ie_blli->ie_blli_l3_window =
						ap->blli.v.layer_3_protocol.window_size;
			}
		}
	}

	/*
	 * Set the called party address and subaddress
	 */
	if (ap->called.tag == T_ATM_PRESENT) {
		if (!msg->msg_ie_cdad) {
			msg->msg_ie_cdad = uma_zalloc(unisig_ie_zone,
			    M_NOWAIT | M_ZERO);
			if (msg->msg_ie_cdad == NULL) {
				err = ENOMEM;
				goto done;
			}
		}
		bcopy(&ie_cdad_absent,
				&msg->msg_ie_cdad->ie_u.ie_cdad,
				sizeof(ie_cdad_absent));
		msg->msg_ie_cdad->ie_ident = UNI_IE_CDAD;
		ATM_ADDR_COPY(&ap->called.addr,
				&msg->msg_ie_cdad->ie_cdad_addr);

		if (ap->called.subaddr.address_format != T_ATM_ABSENT) {
			if (!msg->msg_ie_cdsa) {
				msg->msg_ie_cdsa = uma_zalloc(unisig_ie_zone,
				    M_NOWAIT | M_ZERO);
				if (msg->msg_ie_cdsa == NULL) {
					err = ENOMEM;
					goto done;
				}
			}
			bcopy(&ie_cdsa_absent,
					&msg->msg_ie_cdsa->ie_u.ie_cdsa,
					sizeof(ie_cdsa_absent));
			msg->msg_ie_cdsa->ie_ident = UNI_IE_CDSA;
			ATM_ADDR_COPY(&ap->called.subaddr,
					&msg->msg_ie_cdsa->ie_cdsa_addr);
		}
	}

	/*
	 * Set the calling party address and subaddress
	 */

	if (ap->calling.tag == T_ATM_PRESENT) {
		if (!msg->msg_ie_cgad) {
			msg->msg_ie_cgad = uma_zalloc(unisig_ie_zone,
			    M_NOWAIT | M_ZERO);
			if (msg->msg_ie_cgad == NULL) {
				err = ENOMEM;
				goto done;
			}
		}
		bcopy(&ie_cgad_absent,
				&msg->msg_ie_cgad->ie_u.ie_cgad,
				sizeof(ie_cgad_absent));
		msg->msg_ie_cgsa->ie_ident = UNI_IE_CGSA;
		ATM_ADDR_COPY(&ap->calling.addr,
				&msg->msg_ie_cgad->ie_cgad_addr);

		if (ap->calling.subaddr.address_format !=
				T_ATM_ABSENT) {
			if (!msg->msg_ie_cgsa) {
				msg->msg_ie_cgsa = uma_zalloc(unisig_ie_zone,
				    M_NOWAIT | M_ZERO);
				if (msg->msg_ie_cgsa == NULL) {
					err = ENOMEM;
					goto done;
				}
			}
			bcopy(&ie_cgsa_absent,
					&msg->msg_ie_cgsa->ie_u.ie_cgsa,
					sizeof(ie_cgsa_absent));
			msg->msg_ie_cgsa->ie_ident = UNI_IE_CGSA;
			ATM_ADDR_COPY(&ap->calling.subaddr,
					&msg->msg_ie_cgsa->ie_cgsa_addr);
		}
	}

	/*
	 * Set quality of service attributes
	 */
	if (ap->qos.tag == T_ATM_PRESENT) {
		if (!msg->msg_ie_qosp) {
			msg->msg_ie_qosp = uma_zalloc(unisig_ie_zone,
			    M_NOWAIT | M_ZERO);
			if (msg->msg_ie_qosp == NULL) {
				err = ENOMEM;
				goto done;
			}
		}
		bcopy(&ie_qosp_absent,
				&msg->msg_ie_qosp->ie_u.ie_qosp,
				sizeof(ie_qosp_absent));
		msg->msg_ie_qosp->ie_ident = UNI_IE_QOSP;
		if (usp->us_proto == ATM_SIG_UNI30)
			msg->msg_ie_qosp->ie_coding = UNI_IE_CODE_STD;
		else if ((ap->qos.v.forward.qos_class == 
					T_ATM_QOS_CLASS_0) || 
			   (ap->qos.v.backward.qos_class == 
					T_ATM_QOS_CLASS_0))
			msg->msg_ie_qosp->ie_coding = UNI_IE_CODE_CCITT;
		else
			msg->msg_ie_qosp->ie_coding = ap->qos.v.coding_standard;
		msg->msg_ie_qosp->ie_qosp_fwd_class =
				ap->qos.v.forward.qos_class;
		msg->msg_ie_qosp->ie_qosp_bkwd_class =
				ap->qos.v.backward.qos_class;
	}

	/*
	 * Set transit network attributes
	 */
	if (ap->transit.tag == T_ATM_PRESENT &&
				ap->transit.v.length != 0) {
		if (!msg->msg_ie_trnt) {
			msg->msg_ie_trnt = uma_zalloc(unisig_ie_zone,
			    M_NOWAIT | M_ZERO);
			if (msg->msg_ie_trnt == NULL) {
				err = ENOMEM;
				goto done;
			}
		}
		bcopy(&ie_trnt_absent,
				&msg->msg_ie_trnt->ie_u.ie_trnt,
				sizeof(ie_trnt_absent));
		msg->msg_ie_trnt->ie_ident = UNI_IE_TRNT;
		msg->msg_ie_trnt->ie_trnt_id_type =
				UNI_IE_TRNT_IDT_NATL;
		msg->msg_ie_trnt->ie_trnt_id_plan =
				UNI_IE_TRNT_IDP_CIC;
		bcopy(ap->transit.v.network_id,
				msg->msg_ie_trnt->ie_trnt_id,
				ap->transit.v.length);
	}

	/*
	 * Set cause code
	 */
	if (ap->cause.tag == T_ATM_PRESENT) {
		if (!msg->msg_ie_caus) {
			msg->msg_ie_caus = uma_zalloc(unisig_ie_zone,
			    M_NOWAIT | M_ZERO);
			if (msg->msg_ie_caus == NULL) {
				err = ENOMEM;
				goto done;
			}
		}
		bcopy(&ie_caus_absent,
				&msg->msg_ie_caus->ie_u.ie_caus,
				sizeof(ie_caus_absent));
		msg->msg_ie_caus->ie_ident = UNI_IE_CAUS;
		msg->msg_ie_caus->ie_coding =
				ap->cause.v.coding_standard;
		msg->msg_ie_caus->ie_caus_loc =
				ap->cause.v.location;
		msg->msg_ie_caus->ie_caus_cause =
				ap->cause.v.cause_value;

		/*
		 * Don't copy the diagnostics from the attribute
		 * block, as there's no way to tell how much of
		 * the diagnostic field is relevant
		 */
		msg->msg_ie_caus->ie_caus_diag_len = 0;
	}

done:
	return(err);
}
