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
 * VC state machine
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
 * Local functions
 */
static int	unisig_vc_invalid(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act01(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act02(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act03(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act04(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act05(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act06(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act07(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act08(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act09(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act10(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act11(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act12(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act13(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act14(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act15(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act16(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act17(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act18(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act19(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act20(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act21(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act22(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act23(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act24(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act25(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act26(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act27(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act28(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act29(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act30(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_act31(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *);
static int	unisig_vc_clear_call(struct unisig *,
			struct unisig_vccb *,
			struct unisig_msg *,
			int);


/*
 * State table
 */
static int	unisig_vc_states[21][17] = {
/* 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16 */
{  0,  2, 99,  5, 99, 99,  0, 99, 12, 99,  0, 14,  0,  3,  0,  0,  0 },
{ 29,  4, 99, 17, 99, 99, 17, 99, 17, 99, 17, 17, 17,  0,  0,  0,  0 },
{ 29,  6, 99,  6, 99, 99, 17, 99, 17, 99, 17, 17, 17,  0,  0,  0,  0 },
{ 29, 17, 99, 17, 99, 99, 17, 99, 10, 99, 17, 17, 17,  0,  0,  0,  0 },
{  8, 17, 99, 17, 99, 99, 17, 99, 17, 99, 17, 17, 17,  0,  0,  0,  0 },
{ 29,  7, 99, 15, 99, 99, 15, 99, 15, 99, 15, 16, 17,  0,  0,  0,  0 },
{ 19,  3, 99,  3, 99, 99,  3, 99,  3, 99,  3, 13,  3,  0,  0,  0,  0 },
{ 21, 21, 99, 21, 99, 99, 21, 99, 21, 99, 21, 21, 21,  0,  0,  0,  0 },
{ 22, 22, 99, 22, 99, 99, 22, 99, 22, 99, 22, 22, 22,  0,  0,  0,  0 },
{ 29, 17, 99, 17, 99, 99, 17, 99, 17, 99, 23, 17, 17,  0,  0,  0,  0 },
{ 29, 17, 99, 17, 99, 99, 17, 99, 17, 99, 17, 17, 17,  0,  0,  0,  0 },
{ 29, 17, 99, 17, 99, 99, 17, 99, 17, 99, 17, 17, 17,  0,  0,  0,  0 },
{ 29, 17, 99, 17, 99, 99, 17, 99, 17, 99, 17, 17, 17,  0,  0,  0,  0 },
{ 29, 17, 99, 17, 99, 99, 17, 99, 17, 99, 17, 17, 17,  0,  0,  0,  0 },
{  1, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99 },
{ 99, 25, 99, 25, 99, 99,  9, 99, 25, 99, 25, 25, 25, 25, 31, 25, 25 },
{ 99, 25, 99, 25, 99, 99, 11, 99, 25, 99, 25, 25, 25, 25, 19, 25, 25 },
{ 99, 12, 99, 12, 99, 99, 25, 99, 12, 99, 12, 19, 19, 30, 19, 99, 99 },
{ 99, 12, 99, 12, 99, 99, 12, 99, 12, 99, 12,  3,  3,  3, 24, 26, 26 },
{ 99,  3, 99,  3, 99, 99, 30, 99,  3, 99, 18,  3,  3,  0, 19, 27, 19 },
{ 99,  7, 99,  7, 99, 99, 30, 99,  7, 99, 19, 19, 19, 20, 19, 19, 28 }
};


/*
 * Action vector
 *
 * A given state, action pair selects an action number from the
 * state table.  This vector holds the address of the action routine
 * for each action number.
 */
#define	MAX_ACTION	32
static int (*unisig_vc_act_vec[MAX_ACTION])
		(struct unisig *, struct unisig_vccb *,
			struct unisig_msg *) = {
	unisig_vc_invalid,
	unisig_vc_act01,	
	unisig_vc_act02,
	unisig_vc_act03,
	unisig_vc_act04,
	unisig_vc_act05,
	unisig_vc_act06,
	unisig_vc_act07,
	unisig_vc_act08,
	unisig_vc_act09,
	unisig_vc_act10,
	unisig_vc_act11,
	unisig_vc_act12,
	unisig_vc_act13,
	unisig_vc_act14,
	unisig_vc_act15,
	unisig_vc_act16,
	unisig_vc_act17,
	unisig_vc_act18,
	unisig_vc_act19,
	unisig_vc_act20,
	unisig_vc_act21,
	unisig_vc_act22,
	unisig_vc_act23,
	unisig_vc_act24,
	unisig_vc_act25,
	unisig_vc_act26,
	unisig_vc_act27,
	unisig_vc_act28,
	unisig_vc_act29,
	unisig_vc_act30,
	unisig_vc_act31
};


/*
 * Process an event on a VC
 *
 * Arguments:
 *	usp	pointer to the UNISIG instance
 *	uvp	pointer to the VCCB for the affected VCC
 *	event	a numeric indication of which event has occured
 *	msg	pointer to a signalling message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
unisig_vc_state(usp, uvp, event, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	int			event;
	struct unisig_msg	*msg;
{
	int	action, rc, state;

	/*
	 * Select an action from the state table
	 */
	if (uvp)
		state = uvp->uv_sstate;
	else
		state = UNI_NULL;
	action = unisig_vc_states[event][state];
	if (action >= MAX_ACTION || action < 0)
		panic("unisig_vc_state: invalid action\n");

	/*
	 * Perform the requested action
	 */
	ATM_DEBUG4("unisig_vc_state: uvp=%p, state=%d, event=%d, action=%d\n",
			uvp, state, event, action);
	rc = unisig_vc_act_vec[action](usp, uvp, msg);

	return(rc);
}


/*
 * VC state machine action 0
 * Unexpected action - log an error message
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection (may
		be null)
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_invalid(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	log(LOG_ERR, "unisig_vc_state: unexpected action\n");
	return(EINVAL);
}


/*
 * VC state machine action 1
 * Setup handler called
 *
 * Send SETUP, start timer T303, go to UNI_CALL_INITIATED state
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act01(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int		rc;

	/*
	 * Send the setup message
	 */
	rc = unisig_send_setup(usp, uvp);
	if (rc)
		return(rc);

	/*
	 * Set timer T303
	 */
	uvp->uv_retry = 0;
	UNISIG_VC_TIMER((struct vccb *) uvp, UNI_T303);

	/*
	 * Set the new state
	 */
	uvp->uv_sstate = UNI_CALL_INITIATED;

	/*
	 * Mark the time
	 */
	uvp->uv_tstamp = time_second;

	return(0);
}


/*
 * VC state machine action 2
 * Timeout while waiting for CALL PROCEEDING or CONNECT
 *
 * If this is the second expiration, clear the call.  Otherwise,
 * retransmit the SETUP message and restart T303.
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act02(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int	rc = 0;

	if (uvp->uv_retry) {
		/*
		 * Clear the call
		 */
		rc = unisig_clear_vcc(usp, uvp,
				T_ATM_CAUSE_NO_ROUTE_TO_DESTINATION);
	} else {
		uvp->uv_retry++;
		(void) unisig_send_setup(usp, uvp);
		UNISIG_VC_TIMER((struct vccb *) uvp, UNI_T303);
	}
	
	return(rc);
}


/*
 * VC state machine action 3
 *
 * Clear the call internally
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act03(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int	rc, cause;

	/*
	 * Set cause code
	 */
	if ((msg != NULL) && (msg->msg_ie_caus != NULL)) {
		unisig_cause_attr_from_ie(&uvp->uv_connvc->cvc_attr,
			msg->msg_ie_caus);
		cause = T_ATM_ABSENT;
	} else
		cause = T_ATM_CAUSE_DESTINATION_OUT_OF_ORDER;

	/*
	 * Clear the VCCB
	 */
	rc = unisig_clear_vcc(usp, uvp, cause);

	return(rc);
}


/*
 * VC state machine action 4
 * Received CALL PROCEEDING
 *
 * Start timer T310, go to UNI_CALL_OUT_PROC
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act04(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int			cause, rc, vpi, vci;
	struct atm_pif		*pip = usp->us_pif;
	struct ie_generic	*iep;

	/*
	 * Clear any running timer
	 */
	UNISIG_VC_CANCEL((struct vccb *) uvp);

	/*
	 * Make sure a Connection ID is part of the message
	 */
	if (msg->msg_ie_cnid) {
		vpi = msg->msg_ie_cnid->ie_cnid_vpci;
		vci = msg->msg_ie_cnid->ie_cnid_vci;
	} else {
		iep = uma_zalloc(unisig_ie_zone, M_WAITOK);
		if (iep == NULL)
			return (ENOMEM);
		iep->ie_ident = UNI_IE_CNID;
		iep->ie_err_cause = UNI_IE_CAUS_MISSING;
		MSG_IE_ADD(msg, iep, UNI_MSG_IE_ERR);
		cause = UNI_IE_CAUS_MISSING;
		ATM_DEBUG0("unisig_vc_act04: no CNID in Call Proc\n");
		goto response04;
	}

	/*
	 * Make sure we can handle the specified VPI and VCI
	 */
	if (vpi > pip->pif_maxvpi  || vci > pip->pif_maxvci ||
			vci < UNI_IE_CNID_MIN_VCI) {
		cause = UNI_IE_CAUS_BAD_VCC;
		ATM_DEBUG0("unisig_vc_act04: VPI/VCI invalid\n");
		goto response04;
	}

	/*
	 * Make sure the specified VPI and VCI are not in use
	 */
	if (unisig_find_vpvc(usp, vpi, vci, VCC_OUT)) {
		cause = UNI_IE_CAUS_NA_VCC;
		ATM_DEBUG0("unisig_vc_act04: VPI/VCI in use\n");
		goto response04;
	}

	/*
	 * Start timer T310
	 */
	UNISIG_VC_TIMER((struct vccb *) uvp, UNI_T310);

	/*
	 * Save the specified VPI and VCI
	 */
	uvp->uv_vpi = vpi;
	uvp->uv_vci = vci;

	/*
	 * Set the state
	 */
	uvp->uv_sstate = UNI_CALL_OUT_PROC;

	/*
	 * Mark the time
	 */
	uvp->uv_tstamp = time_second;

	return(0);

response04:
	/*
	 * Initiate call clearing
	 */
	rc = unisig_vc_clear_call(usp, uvp, msg, cause);

	return(rc);
}


/*
 * VC state machine action 5
 * Timeout in UNI_CALL_OUT_PROC
 * 
 * Clear call towards network
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act05(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int			rc;
	struct unisig_msg	*rls_msg;
	struct ie_generic	*cause_ie;

	/*
	 * Send a RELEASE message
	 */
	rls_msg = uma_zalloc(unisig_msg_zone, M_WAITOK | M_ZERO);
	if (rls_msg == NULL)
		return(ENOMEM);
	cause_ie = uma_zalloc(unisig_ie_zone, M_WAITOK | M_ZERO);
	if (cause_ie == NULL) {
		uma_zfree(unisig_msg_zone, rls_msg);
		return(ENOMEM);
	}

	/*
	 * Fill out the RELEASE message
	 */
	rls_msg->msg_call_ref = uvp->uv_call_ref;
	rls_msg->msg_type = UNI_MSG_RLSE;
	rls_msg->msg_type_flag = 0;
	rls_msg->msg_type_action = 0;
	rls_msg->msg_ie_caus = cause_ie;

	/*
	 * Fill out the cause IE
	 */
	cause_ie->ie_caus_loc = UNI_IE_CAUS_LOC_USER;
	cause_ie->ie_caus_cause = UNI_IE_CAUS_TIMER;
	bcopy("310", cause_ie->ie_caus_diagnostic, 3);

	/*
	 * Send the RELEASE message.
	 */
	rc = unisig_send_msg(usp, rls_msg);
	unisig_free_msg(rls_msg);

	/*
	 * Start timer T308
	 */
	UNISIG_VC_TIMER((struct vccb *) uvp, UNI_T308);

	/*
	 * Set the new state
	 */
	uvp->uv_sstate = UNI_RELEASE_REQUEST;
	uvp->uv_ustate = VCCU_CLOSED;

	/*
	 * Mark the time
	 */
	uvp->uv_tstamp = time_second;

	return(rc);
}


/*
 * VC state machine action 6
 * Received CONNECT
 *
 * Send CONNECT ACK, go to UNI_ACTIVE state
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act06(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int			cause, rc, vci, vpi;
	struct atm_pif		*pip = usp->us_pif;
	struct unisig_msg	*cack_msg;
	struct ie_generic	*iep;
	Atm_attributes		*ap;

	/*
	 * Clear any running timer
	 */
	UNISIG_VC_CANCEL((struct vccb *) uvp);

	ap = &uvp->uv_connvc->cvc_attr;

	/*
	 * See if a VPI/VCI is specified
	 */
	if (msg->msg_ie_cnid) {
		/*
		 * Yes--VPI/VCI must be the first specification or must
		 * match what was specified before
		 */
		vpi = msg->msg_ie_cnid->ie_cnid_vpci;
		vci = msg->msg_ie_cnid->ie_cnid_vci;
		if ((uvp->uv_vpi || uvp->uv_vci) &&
				(vpi != uvp->uv_vpi ||
				vci != uvp->uv_vci)) {
			cause = UNI_IE_CAUS_BAD_VCC;
			ATM_DEBUG0("unisig_vc_act06: VPI/VCI invalid\n");
			goto response06;
		}

		/*
		 * Specified VPI/VCI must be within range
		 */
		if (vpi > pip->pif_maxvpi || vci > pip->pif_maxvci ||
				vci < UNI_IE_CNID_MIN_VCI) {
			cause = UNI_IE_CAUS_BAD_VCC;
			ATM_DEBUG0("unisig_vc_act06: VPI/VCI invalid\n");
			goto response06;
		}
		uvp->uv_vpi = vpi;
		uvp->uv_vci = vci;
	} else {
		/*
		 * No--VCI must have been specified earlier
		 * May be called from netisr - don't wait.
		 */
		if (!uvp->uv_vci) {
			iep = uma_zalloc(unisig_ie_zone, M_NOWAIT);
			if (iep == NULL)
				return(ENOMEM);
			iep->ie_ident = UNI_IE_CNID;
			iep->ie_err_cause = UNI_IE_CAUS_MISSING;
			MSG_IE_ADD(msg, iep, UNI_MSG_IE_ERR);
			cause = UNI_IE_CAUS_MISSING;
			ATM_DEBUG0("unisig_vc_act06: CNID missing\n");
			goto response06;
		}
	}

	/*
	 * Handle AAL parameters negotiation
	 */
	if (msg->msg_ie_aalp) {
		struct ie_generic	*aalp = msg->msg_ie_aalp;

		/*
		 * AAL parameters must have been sent in SETUP
		 */
		if ((ap->aal.tag != T_ATM_PRESENT) ||
		    (ap->aal.type != aalp->ie_aalp_aal_type)) {
			cause = UNI_IE_CAUS_IECONTENT;
			goto response06;
		}

		switch (aalp->ie_aalp_aal_type) {

		case UNI_IE_AALP_AT_AAL3:
			/*
			 * Maximum SDU size negotiation
			 */
			if (aalp->ie_aalp_4_fwd_max_sdu == T_ATM_ABSENT)
				break;
			if ((ap->aal.v.aal4.forward_max_SDU_size < 
					aalp->ie_aalp_4_fwd_max_sdu) ||
			    (ap->aal.v.aal4.backward_max_SDU_size <
					aalp->ie_aalp_4_bkwd_max_sdu)) {
				cause = UNI_IE_CAUS_IECONTENT;
				goto response06;
			} else {
				ap->aal.v.aal4.forward_max_SDU_size =
					aalp->ie_aalp_4_fwd_max_sdu;
				ap->aal.v.aal4.backward_max_SDU_size =
					aalp->ie_aalp_4_bkwd_max_sdu;
			}
			break;

		case UNI_IE_AALP_AT_AAL5:
			/*
			 * Maximum SDU size negotiation
			 */
			if (aalp->ie_aalp_5_fwd_max_sdu == T_ATM_ABSENT)
				break;
			if ((ap->aal.v.aal5.forward_max_SDU_size < 
					aalp->ie_aalp_5_fwd_max_sdu) ||
			    (ap->aal.v.aal5.backward_max_SDU_size <
					aalp->ie_aalp_5_bkwd_max_sdu)) {
				cause = UNI_IE_CAUS_IECONTENT;
				goto response06;
			} else {
				ap->aal.v.aal5.forward_max_SDU_size =
					aalp->ie_aalp_5_fwd_max_sdu;
				ap->aal.v.aal5.backward_max_SDU_size =
					aalp->ie_aalp_5_bkwd_max_sdu;
			}
			break;
		}
	}

	/*
	 * Get memory for a CONNECT ACK message
	 * May be called from netisr.
	 */
	cack_msg = uma_zalloc(unisig_msg_zone, M_NOWAIT);
	if (cack_msg == NULL)
		return(ENOMEM);

	/*
	 * Fill out the CONNECT ACK message
	 */
	cack_msg->msg_call_ref = uvp->uv_call_ref;
	cack_msg->msg_type = UNI_MSG_CACK;
	cack_msg->msg_type_flag = 0;
	cack_msg->msg_type_action = 0;

	/*
	 * Send the CONNECT ACK message
	 */
	rc = unisig_send_msg(usp, cack_msg);
	unisig_free_msg(cack_msg);

	/*
	 * Set the new state
	 */
	uvp->uv_sstate = UNI_ACTIVE;
	uvp->uv_ustate = VCCU_OPEN;

	/*
	 * Mark the time
	 */
	uvp->uv_tstamp = time_second;

	/*
	 * Notify the user that the connection is now active
	 */
	atm_cm_connected(uvp->uv_connvc);

	return(0);

response06:
	/*
	 * Initiate call clearing
	 */
	rc = unisig_vc_clear_call(usp, uvp, msg, cause);

	return(rc);
}


/*
 * VC state machine action 7
 * Abort routine called or signalling SAAL session reset while in
 * one of the call setup states
 *
 * Clear the call, send RELEASE COMPLETE, notify the user.
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act07(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int			rc;

	/*
	 * Clear any running timer
	 */
	UNISIG_VC_CANCEL((struct vccb *) uvp);

	/*
	 * Send a RELEASE COMPLETE message rejecting the connection
	 */
	rc = unisig_send_release_complete(usp, uvp, msg,
			UNI_IE_CAUS_TEMP);

	/*
	 * Clear the call VCCB
	 */
	uvp->uv_sstate = UNI_FREE;
	uvp->uv_ustate = VCCU_CLOSED;

	/*
	 * Mark the time
	 */
	uvp->uv_tstamp = time_second;

	/*
	 * Notify the user
	 */
	if ((msg != NULL) && (msg->msg_ie_caus != NULL))
		unisig_cause_attr_from_ie(&uvp->uv_connvc->cvc_attr,
			msg->msg_ie_caus);
	else
		unisig_cause_attr_from_user(&uvp->uv_connvc->cvc_attr,
			T_ATM_CAUSE_NORMAL_CALL_CLEARING);
	atm_cm_cleared(uvp->uv_connvc);

	return(rc);
}


/*
 * VC state machine action 8
 * Received SETUP
 *
 * Check call paramaters, notify user that a call has been received,
 * set UNI_CALL_PRESENT state
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act08(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int			cause = 0, rc, vpi, vci;
	struct atm_pif		*pip = usp->us_pif;
	struct atm_nif		*nip;
	Atm_addr_nsap		*nap;
	Atm_attributes		attr;

	ATM_DEBUG3("unisig_vc_act08: usp=%p, uvp=%p, msg=%p\n",
			usp, uvp, msg);

	/*
	 * Make sure that the called address is the right format
	 */
	if (msg->msg_ie_cdad->ie_cdad_plan != UNI_IE_CDAD_PLAN_NSAP) {
		cause = UNI_IE_CAUS_IECONTENT;
		ATM_DEBUG0("unisig_vc_act08: bad address format\n");
		goto response08;
	}

	/*
	 * Make sure that the called address is ours
	 */
	nap = (Atm_addr_nsap *) msg->msg_ie_cdad->ie_cdad_addr.address;
	if (bcmp(usp->us_addr.address, nap,	/* XXX */
			sizeof(Atm_addr_nsap)-1)) {
		cause = UNI_IE_CAUS_IECONTENT;
		ATM_DEBUG0("unisig_vc_act08: address not mine\n");
		goto response08;
	}

	/*
	 * Find the right NIF for the given selector byte
	 */
	nip = pip->pif_nif;
	while (nip && nip->nif_sel != nap->aan_sel) {
		nip = nip->nif_pnext;
	}
	if (!nip) {
		cause = UNI_IE_CAUS_IECONTENT;
		ATM_DEBUG0("unisig_vc_act08: bad selector byte\n");
		goto response08;
	}

	/*
	 * See if we recognize the specified AAL
	 */
	if (msg->msg_ie_aalp->ie_aalp_aal_type != UNI_IE_AALP_AT_AAL3 &&
			msg->msg_ie_aalp->ie_aalp_aal_type !=
			UNI_IE_AALP_AT_AAL5) {
		cause = UNI_IE_CAUS_UAAL;
		ATM_DEBUG0("unisig_vc_act08: bad AAL\n");
		goto response08;
	}

	/*
	 * Should verify that we can handle requested
	 * connection QOS
	 */

	/*
	 * Make sure the specified VPI/VCI is valid
	 */
	vpi = msg->msg_ie_cnid->ie_cnid_vpci;
	vci = msg->msg_ie_cnid->ie_cnid_vci;
	if (vpi > pip->pif_maxvpi ||
			vci > pip->pif_maxvci ||
			vci < UNI_IE_CNID_MIN_VCI) {
		cause = UNI_IE_CAUS_BAD_VCC;
		ATM_DEBUG0("unisig_vc_act08: VPI/VCI invalid\n");
		goto response08;
	}

	/*
	 * Make sure the specified VPI/VCI isn't in use already
	 */
	if (unisig_find_vpvc(usp, vpi, vci, VCC_IN)) {
		cause = UNI_IE_CAUS_NA_VCC;
		ATM_DEBUG0("unisig_vc_act08: VPI/VCI in use\n");
		goto response08;
	}

	/*
	 * Make sure it's a point-to-point connection
	 */
	if (msg->msg_ie_bbcp->ie_bbcp_conn_config !=
			UNI_IE_BBCP_CC_PP) {
		cause = UNI_IE_CAUS_NI_BC;
		ATM_DEBUG0("unisig_vc_act08: conn not pt-pt\n");
		goto response08;
	}

	/*
	 * Fill in the VCCB fields that we can at this point
	 */
	uvp->uv_type = VCC_SVC | VCC_IN | VCC_OUT;
	uvp->uv_proto = pip->pif_sigmgr->sm_proto;
	uvp->uv_sstate = UNI_CALL_PRESENT;
	uvp->uv_ustate = VCCU_POPEN;
	uvp->uv_pif = pip;
	uvp->uv_nif = nip;
	uvp->uv_vpi = msg->msg_ie_cnid->ie_cnid_vpci;
	uvp->uv_vci = msg->msg_ie_cnid->ie_cnid_vci;
	uvp->uv_tstamp = time_second;

	/*
	 * Copy the connection attributes from the SETUP message
	 * to an attribute block
	 */
	bzero(&attr, sizeof(attr));
	attr.nif = nip;
	attr.aal.tag = T_ATM_ABSENT;
	attr.traffic.tag = T_ATM_ABSENT;
	attr.bearer.tag = T_ATM_ABSENT;
	attr.bhli.tag = T_ATM_ABSENT;
	attr.blli.tag_l2 = T_ATM_ABSENT;
	attr.blli.tag_l3 = T_ATM_ABSENT;
	attr.llc.tag = T_ATM_ABSENT;
	attr.called.tag = T_ATM_ABSENT;
	attr.calling.tag = T_ATM_ABSENT;
	attr.qos.tag = T_ATM_ABSENT;
	attr.transit.tag = T_ATM_ABSENT;
	attr.cause.tag = T_ATM_ABSENT;
	unisig_save_attrs(usp, msg, &attr);

	/*
	 * Notify the connection manager of the new VCC
	 */
	ATM_DEBUG0("unisig_vc_act08: notifying user of connection\n");
	rc = atm_cm_incoming((struct vccb *)uvp, &attr);
	if (rc)
		goto response08;

	/*
	 * Wait for the connection recipient to issue an accept
	 * or reject
	 */
	return(0);

response08:
	ATM_DEBUG1("unisig_vc_act08: reject with cause=%d\n", cause);

	/*
	 * Clear the VCCB state
	 */
	uvp->uv_sstate = UNI_NULL;

	/*
	 * Mark the time
	 */
	uvp->uv_tstamp = time_second;

	/*
	 * Some problem was detected with the request.  Send a Q.2931
	 * message rejecting the connection.
	 */
	rc = unisig_send_release_complete(usp, uvp, msg, cause);

	return(rc);
}


/*
 * VC state machine action 9
 * Accept routine called by user
 *
 * Send CONNECT, start timer T313, go to UNI_CONNECT_REQUEST state
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act09(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int			rc;
	struct unisig_msg	*conn_msg;

	/* may be called from timeout - don't wait */
	conn_msg = uma_zalloc(unisig_msg_zone, M_NOWAIT);
	if (conn_msg == NULL)
		return(ENOMEM);

	/*
	 * Fill out the response
	 */
	conn_msg->msg_call_ref = uvp->uv_call_ref;
	conn_msg->msg_type = UNI_MSG_CONN;
	conn_msg->msg_type_flag = 0;
	conn_msg->msg_type_action = 0;

	/*
	 * Send the CONNECT message.  If the send fails, the other
	 * side will eventually time out and close the connection.
	 */
	rc = unisig_send_msg(usp, conn_msg);
	unisig_free_msg(conn_msg);
	if (rc) {
		return(rc);
	}

	/*
	 * Start timer T313
	 */
	UNISIG_VC_TIMER((struct vccb *) uvp, UNI_T313);

	/*
	 * Set the new state
	 */
	uvp->uv_sstate = UNI_CONNECT_REQUEST;

	/*
	 * Mark the time
	 */
	uvp->uv_tstamp = time_second;

	return(0);
}


/*
 * VC state machine action 10
 * Received CONNECT ACK
 *
 * Go to UNI_ACTIVE state
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act10(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	/*
	 * Clear any running timer
	 */
	UNISIG_VC_CANCEL((struct vccb *) uvp);

	/*
	 * Set the state
	 */
	uvp->uv_sstate = UNI_ACTIVE;
	uvp->uv_ustate = VCCU_OPEN;

	/*
	 * Mark the time
	 */
	uvp->uv_tstamp = time_second;

	/*
	 * Notify the user that the call is up
	 */
	atm_cm_connected(uvp->uv_connvc);

	return (0);
}


/*
 * VC state machine action 11
 * Reject handler called
 *
 * Send RELEASE COMPLETE, clear the call
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act11(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int			rc, cause;

	/*
	 * Send generic cause code if one is not already set
	 */
	if (uvp->uv_connvc->cvc_attr.cause.tag == T_ATM_PRESENT)
		cause = T_ATM_ABSENT;
	else
		cause = T_ATM_CAUSE_CALL_REJECTED;

	/*
	 * Send a RELEASE COMPLETE message
	 */
	rc = unisig_send_release_complete(usp, uvp, msg, cause);

	/*
	 * Clear the call VCCB
	 */
	uvp->uv_sstate = UNI_FREE;
	uvp->uv_ustate = VCCU_CLOSED;

	/*
	 * Mark the time
	 */
	uvp->uv_tstamp = time_second;

	return(rc);
}


/*
 * VC state machine action 12
 * Release or abort routine called
 *
 * Send RELEASE, start timer T308, go to UNI_RELEASE_REQUEST state
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act12(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int			rc;

	/*
	 * Clear any running timer
	 */
	UNISIG_VC_CANCEL((struct vccb *) uvp);

	/*
	 * Send the RELEASE message
	 */
	rc = unisig_vc_clear_call(usp, uvp, (struct unisig_msg *)NULL,
			T_ATM_ABSENT);

	return(rc);
}


/*
 * VC state machine action 13
 * RELEASE COMPLETE received
 *
 * Clear the call
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act13(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	/*
	 * Clear any running timer
	 */
	UNISIG_VC_CANCEL((struct vccb *) uvp);

	/*
	 * Set the state
	 */
	uvp->uv_sstate = UNI_FREE;
	if (uvp->uv_ustate != VCCU_ABORT)
		uvp->uv_ustate = VCCU_CLOSED;

	/*
	 * Mark the time
	 */
	uvp->uv_tstamp = time_second;

	/*
	 * Notify the user that the call is now closed
	 */
	if (msg->msg_ie_caus != NULL)
		unisig_cause_attr_from_ie(&uvp->uv_connvc->cvc_attr,
			msg->msg_ie_caus);
	atm_cm_cleared(uvp->uv_connvc);

	return(0);
}


/*
 * VC state machine action 14
 * Timer expired while waiting for RELEASE COMPLETE
 *
 * If this is the second expiration, just clear the call.  Otherwise,
 * retransmit the RELEASE message and restart timer T308.
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act14(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int			rc;

	/*
	 * Check the retry count
	 */
	if (uvp->uv_retry) {
		/*
		 * Clear the connection
		 */
		rc = unisig_clear_vcc(usp, uvp,
				T_ATM_CAUSE_NORMAL_CALL_CLEARING);
	} else {
		/*
		 * Increment the retry count
		 */
		uvp->uv_retry++;

		/*
		 * Resend the RELEASE message
		 */
		rc = unisig_send_release(usp, uvp,
				(struct unisig_msg *)0, T_ATM_ABSENT);
		if (rc)
			return(rc);

		/*
		 * Restart timer T308
		 */
		UNISIG_VC_TIMER((struct vccb *) uvp, UNI_T308);
	}

	return(0);
}


/*
 * VC state machine action 15
 * RELEASE received in UNI_ACTIVE state
 *
 * Send RELEASE COMPLETE, go to UNI_FREE, notify the user
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act15(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int			cause, rc;
	struct ie_generic	*iep;

	/*
	 * Clear any running timer
	 */
	UNISIG_VC_CANCEL((struct vccb *) uvp);

	/*
	 * If there was no Cause IE, flag an error
	 */
	if (!msg->msg_ie_caus) {
		cause = UNI_IE_CAUS_MISSING;
		for (iep=msg->msg_ie_err; iep; iep=iep->ie_next) {
			if (iep->ie_ident == UNI_IE_CAUS &&
					iep->ie_err_cause ==
					UNI_IE_CAUS_IECONTENT) {
				cause = UNI_IE_CAUS_IECONTENT;
			}
		}
		if (cause == UNI_IE_CAUS_MISSING) {
			iep = uma_zalloc(unisig_ie_zone, M_WAITOK);
			if (iep == NULL)
				return(ENOMEM);
			iep->ie_ident = UNI_IE_CNID;
			iep->ie_err_cause = UNI_IE_CAUS_MISSING;
			MSG_IE_ADD(msg, iep, UNI_MSG_IE_ERR);
		}
	} else {
		cause = UNI_IE_CAUS_NORM_UNSP;
	}

	/*
	 * Send a RELEASE COMPLETE message
	 */
	rc = unisig_send_release_complete(usp, uvp, msg, cause);

	/*
	 * Set the state
	 */
	uvp->uv_sstate = UNI_FREE;
	uvp->uv_ustate = VCCU_CLOSED;

	/*
	 * Mark the time
	 */
	uvp->uv_tstamp = time_second;

	/*
	 * Notify the user that the call is cleared
	 */
	if (msg->msg_ie_caus != NULL)
		unisig_cause_attr_from_ie(&uvp->uv_connvc->cvc_attr,
			msg->msg_ie_caus);
	else
		unisig_cause_attr_from_user(&uvp->uv_connvc->cvc_attr,
			T_ATM_CAUSE_UNSPECIFIED_NORMAL);
	atm_cm_cleared(uvp->uv_connvc);

	return(rc);
}


/*
 * VC state machine action 16
 * RELEASE received in UNI_RELEASE_REQUEST state
 *
 * Clear the call
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act16(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int	rc;

	/*
	 * Clear any running timer
	 */
	UNISIG_VC_CANCEL((struct vccb *) uvp);

	/*
	 * Clear the VCCB
	 */
	rc = unisig_clear_vcc(usp, uvp, T_ATM_ABSENT);

	return(rc);
}


/*
 * VC state machine action 17
 * Protocol error
 *
 * Send a STATUS message with cause 101, "message not compatible with
 * call state"
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act17(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int			rc;

	ATM_DEBUG3("unisig_vc_perror: usp=%p, uvp=%p, msg=%p\n",
			usp, uvp, msg);

	/*
	 * Clear any running timer
	 */
	UNISIG_VC_CANCEL((struct vccb *) uvp);

	/*
	 * Send a STATUS message
	 */
	rc = unisig_send_status(usp, uvp, msg, UNI_IE_CAUS_STATE);

	return(rc ? rc : EINVAL);
}


/*
 * VC state machine action 18
 * Signalling AAL connection has been lost
 *
 * Start timer T309.  If the timer expires before the SAAL connection
 * comes back, the VCC will be cleared.
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act18(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	/*
	 * Clear any running timer
	 */
	UNISIG_VC_CANCEL((struct vccb *) uvp);

	/*
	 * Start timer T309
	 */
	UNISIG_VC_TIMER((struct vccb *) uvp, UNI_T309);

	/*
	 * Set new state
	 */
	uvp->uv_sstate = UNI_SSCF_RECOV;

	return(0);
}


/*
 * VC state machine action 19
 * Ignore the event
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act19(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	return(0);
}


/*
 * VC state machine action 20
 * SSCF establish indication in UNI_SSCF_RECOV state -- signalling
 * AAL has come up after an outage
 *
 * Send STATUS ENQ to make sure we're in compatible state with other end
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act20(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int			rc;
	struct unisig_msg	*stat_msg;

	/*
	 * Clear any running timer
	 */
	UNISIG_VC_CANCEL((struct vccb *) uvp);

	/*
	 * Get memory for a STATUS ENQUIRY message
	 * May be called from netisr - don't wait.
	 */
	stat_msg = uma_zalloc(unisig_msg_zone, M_NOWAIT);
	if (stat_msg == NULL)
		return(ENOMEM);

	/*
	 * Fill out the message
	 */
	stat_msg->msg_call_ref = uvp->uv_call_ref;
	stat_msg->msg_type = UNI_MSG_SENQ;
	stat_msg->msg_type_flag = 0;
	stat_msg->msg_type_action = 0;

	/*
	 * Send the STATUS ENQUIRY message
	 */
	rc = unisig_send_msg(usp, stat_msg);
	unisig_free_msg(stat_msg);

	/*
	 * Return to active state
	 */
	uvp->uv_sstate = UNI_ACTIVE;

	return(rc);
}


/*
 * VC state machine action 21
 * STATUS received
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection (may
 *		be NULL)
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act21(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int	cause, rc;

	/*
	 * Ignore a STATUS message with the global call reference
	 */
	if (GLOBAL_CREF(msg->msg_call_ref)) {
		return(0);
	}

	/*
	 * If the network thinks we're in NULL state, clear the VCC
	 */
	if (msg->msg_ie_clst->ie_clst_state == UNI_NULL) {
		if (uvp) {
			(void)unisig_clear_vcc(usp, uvp,
					T_ATM_CAUSE_DESTINATION_OUT_OF_ORDER);
		}
		return(0);
	}

	/*
	 * If we are in NULL state, send a RELEASE COMPLETE
	 */
	if (!uvp || (uvp->uv_sstate == UNI_FREE) ||
			(uvp->uv_sstate == UNI_NULL)) {
		rc = unisig_send_release_complete(usp,
				uvp, msg, UNI_IE_CAUS_STATE);
		return(rc);
	}

	/*
	 * If the reported state doesn't match our state, close the VCC
	 * unless we're in UNI_RELEASE_REQUEST or UNI_RELEASE_IND
	 */
	if (msg->msg_ie_clst->ie_clst_state != uvp->uv_sstate) {
		if (uvp->uv_sstate == UNI_RELEASE_REQUEST ||
				uvp->uv_sstate == UNI_RELEASE_IND) {
			return(0);
		}
		rc = unisig_clear_vcc(usp, uvp,
			T_ATM_CAUSE_MESSAGE_INCOMPATIBLE_WITH_CALL_STATE);
	}

	/*
	 * States match, check for an error on one of our messages
	 */
	cause = msg->msg_ie_caus->ie_caus_cause;
	if (cause == UNI_IE_CAUS_MISSING ||
			cause == UNI_IE_CAUS_MTEXIST ||
			cause == UNI_IE_CAUS_IEEXIST ||
			cause == UNI_IE_CAUS_IECONTENT ||
			cause == UNI_IE_CAUS_STATE) {
		ATM_DEBUG2("unisig_vc_act21: error %d on message 0x%x\n",
				cause,
				msg->msg_ie_caus->ie_caus_diagnostic[0]);
		if (uvp) {
			(void)unisig_clear_vcc(usp, uvp, cause);
		}
	}

	return(0);
}


/*
 * VC state machine action 22
 * Received STATUS ENQ
 *
 * Send STATUS with cause 30 "response to STATUS ENQUIRY" and
 * current state
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection (may
 *		be NULL)
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act22(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int			rc;
	struct unisig_msg	*status;
	struct ie_generic	*callst_ie, *cause_ie;

	ATM_DEBUG3("unisig_vc_perror: usp=%p, uvp=%p, msg=%p\n",
			usp, uvp, msg);

	/*
	 * Get memory for a STATUS message
	 */
	status = uma_zalloc(unisig_msg_zone, M_WAITOK | M_ZERO);
	if (status == NULL)
		return(ENOMEM);
	callst_ie = uma_zalloc(unisig_ie_zone, M_WAITOK | M_ZERO);
	if (callst_ie == NULL) {
		uma_zfree(unisig_msg_zone, status);
		return(ENOMEM);
	}
	cause_ie = uma_zalloc(unisig_ie_zone, M_WAITOK | M_ZERO); 
	if (cause_ie == NULL) {
		uma_zfree(unisig_msg_zone, status);
		uma_zfree(unisig_ie_zone, callst_ie);
		return(ENOMEM);
	}

	/*
	 * Fill out the response
	 */
	if (uvp) {
		status->msg_call_ref = uvp->uv_call_ref;
	} else if (msg) {
		if (msg->msg_call_ref & UNI_MSG_CALL_REF_RMT)
			status->msg_call_ref = msg->msg_call_ref &
					UNI_MSG_CALL_REF_MASK;
		else
			status->msg_call_ref = msg->msg_call_ref |
					UNI_MSG_CALL_REF_RMT;
	} else {
		status->msg_call_ref = UNI_MSG_CALL_REF_GLOBAL;
	}
	status->msg_type = UNI_MSG_STAT;
	status->msg_type_flag = 0;
	status->msg_type_action = 0;
	status->msg_ie_clst = callst_ie;
	status->msg_ie_caus = cause_ie;

	/*
	 * Fill out the call state IE
	 */
	callst_ie->ie_ident = UNI_IE_CLST;
	callst_ie->ie_coding = 0;
	callst_ie->ie_flag = 0;
	callst_ie->ie_action = 0;
	if (uvp) {
		switch(uvp->uv_sstate) {
		case UNI_FREE:
			callst_ie->ie_clst_state = UNI_NULL;
			break;
		default:
			callst_ie->ie_clst_state = uvp->uv_sstate;
		}
	} else {
		callst_ie->ie_clst_state = UNI_NULL;
	}

	/*
	 * Fill out the cause IE
	 */
	cause_ie->ie_ident = UNI_IE_CAUS;
	cause_ie->ie_coding = 0;
	cause_ie->ie_flag = 0;
	cause_ie->ie_action = 0;
	cause_ie->ie_caus_loc = UNI_IE_CAUS_LOC_USER;
	cause_ie->ie_caus_cause = UNI_IE_CAUS_SENQ;

	/*
	 * Send the STATUS message
	 */
	rc = unisig_send_msg(usp, status);
	unisig_free_msg(status);
	return(rc);
}


/*
 * VC state machine action 23
 * Received ADD PARTY
 *
 * We don't support multipoint connections, so send an ADD PARTY REJECT
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act23(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int			rc;
	struct unisig_msg	*apr_msg;

	/*
	 * Get memory for the ADD PARTY REJECT message
	 */
	apr_msg = uma_zalloc(unisig_msg_zone, M_WAITOK | M_ZERO);
	if (apr_msg == NULL)
		return(ENOMEM);

	/*
	 * Fill out the message
	 */
	if (msg->msg_call_ref & UNI_MSG_CALL_REF_RMT)
		apr_msg->msg_call_ref = msg->msg_call_ref &
				UNI_MSG_CALL_REF_MASK;
	else
		apr_msg->msg_call_ref = msg->msg_call_ref |
				UNI_MSG_CALL_REF_RMT;
	apr_msg->msg_type = UNI_MSG_ADPR;
	apr_msg->msg_type_flag = 0;
	apr_msg->msg_type_action = 0;

	/*
	 * Use the endpoint reference IE from the received message
	 */
	apr_msg->msg_ie_eprf = msg->msg_ie_eprf;

	/*
	 * Send the ADD PARTY REJECT message
	 */
	rc = unisig_send_msg(usp, apr_msg);
	apr_msg->msg_ie_eprf = NULL;
	unisig_free_msg(apr_msg);

	return(rc);
}


/*
 * VC state machine action 24
 * User error
 *
 * Return EALREADY
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act24(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	return(EALREADY);
}


/*
 * VC state machine action 25
 * User error
 *
 * Return EINVAL
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act25(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	return(EINVAL);
}


/*
 * VC state machine action 26
 * PVC abort
 *
 * The abort handler was called to abort a PVC.  Clear the VCCB and
 * notify the user.
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act26(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int	rc;

	/*
	 * Clear any running timer
	 */
	UNISIG_VC_CANCEL((struct vccb *) uvp);

	/*
	 * Close the VCCB
	 */
	rc = unisig_close_vcc(usp, uvp);
	if (rc)
		return(rc);

	/*
	 * Notify the user
	 */
	if (uvp->uv_connvc->cvc_attr.cause.tag != T_ATM_PRESENT)
		unisig_cause_attr_from_user(&uvp->uv_connvc->cvc_attr,
			T_ATM_CAUSE_NORMAL_CALL_CLEARING);

	atm_cm_cleared(uvp->uv_connvc);

	return(0);
}


/*
 * VC state machine action 27
 * Signalling AAL failure
 *
 * Change PVC state to UNI_PVC_ACT_DOWN.
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act27(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	/*
	 * Set the state
	 */
	uvp->uv_sstate = UNI_PVC_ACT_DOWN;

	/*
	 * Mark the time
	 */
	uvp->uv_tstamp = time_second;

	return(0);
}


/*
 * VC state machine action 28
 * Signalling AAL established
 *
 * Set PVC state to UNI_PVC_ACTIVE.
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act28(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	/*
	 * Set the state
	 */
	uvp->uv_sstate = UNI_PVC_ACTIVE;

	/*
	 * Mark the time
	 */
	uvp->uv_tstamp = time_second;

	return(0);
}


/*
 * VC state machine action 29
 * Protocol error
 *
 * Send a RELEASE COMPLETE message with cause 81, "invalid call
 * reference value"
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection (may
 *		be NULL)
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act29(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	int			rc;

	/*
	 * Send a RELEASE COMPLETE message
	 */
	rc = unisig_send_release_complete(usp, uvp, msg,
			UNI_IE_CAUS_CREF);

	return(rc);
}


/*
 * VC state machine action 30
 * Release routine called while SSCF session down, or SSCF session
 * reset or lost while in UNI_CALL_PRESENT
 *
 * Go to UNI_FREE state
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act30(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	/*
	 * Clear any running timer
	 */
	UNISIG_VC_CANCEL((struct vccb *) uvp);

	/*
	 * Clear the call state
	 */
	uvp->uv_sstate = UNI_FREE;
	uvp->uv_ustate = VCCU_CLOSED;

	/*
	 * Mark the time
	 */
	uvp->uv_tstamp = time_second;

	return(0);
}


/*
 * VC state machine action 31
 * Accept handler called in UNI_FREE state.
 *
 * The call was in UNI_CALL_PRESENT state when it was closed because
 * of an SSCF failure.  Return an error indication.  The accept
 * handler will free the VCCB and return the proper code to the
 * caller.
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to a UNISIG message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_act31(usp, uvp, msg)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
{
	return(ENETDOWN);
}


/*
 * Initiate clearing a call by sending a RELEASE message.
 *
 * Arguments:
 *	usp	pointer to protocol instance block
 *	uvp	pointer to the VCCB for the affected connection
 *	msg	pointer to UNI signalling message that the RELEASE
 *		responds to (may be NULL)
 *	cause	the reason for clearing the call;  a value of
 *		T_ATM_ABSENT indicates that the cause code is
 *		in the VCC's ATM attributes block
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
unisig_vc_clear_call(usp, uvp, msg, cause)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
	int			cause;
{
	int			rc;

	/*
	 * Clear the retry count
	 */
	uvp->uv_retry = 0;

	/*
	 * Make sure the ATM attributes block has a valid cause code,
	 * if needed
	 */
	if (cause == T_ATM_ABSENT &&
			uvp->uv_connvc->cvc_attr.cause.tag !=
			T_ATM_PRESENT) {
		uvp->uv_connvc->cvc_attr.cause.tag = T_ATM_PRESENT;
		uvp->uv_connvc->cvc_attr.cause.v.coding_standard =
				T_ATM_ITU_CODING;
		uvp->uv_connvc->cvc_attr.cause.v.location =
				T_ATM_LOC_USER;
		uvp->uv_connvc->cvc_attr.cause.v.cause_value =
			usp->us_proto == ATM_SIG_UNI30 ? 
				T_ATM_CAUSE_UNSPECIFIED_NORMAL :
				T_ATM_CAUSE_NORMAL_CALL_CLEARING;
	}

	/*
	 * Send a RELEASE message
	 */
	rc = unisig_send_release(usp, uvp, msg, cause);
	if (rc)
		return(rc);

	/*
	 * Start timer T308
	 */
	UNISIG_VC_TIMER((struct vccb *) uvp, UNI_T308);

	/*
	 * Set the VCCB state
	 */
	uvp->uv_sstate = UNI_RELEASE_REQUEST;
	if (uvp->uv_ustate != VCCU_ABORT)
		uvp->uv_ustate = VCCU_CLOSED;

	/*
	 * Mark the time
	 */
	uvp->uv_tstamp = time_second;

	return(0);
}
