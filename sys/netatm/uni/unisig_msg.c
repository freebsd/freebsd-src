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
 * Message handling module
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
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
#include <netatm/uni/unisig_mbuf.h>
#include <netatm/uni/unisig_print.h>

#include <vm/uma.h>

/*
 * Local functions
 */
static void	unisig_rcv_restart(struct unisig *, struct unisig_msg *);
static void	unisig_rcv_setup(struct unisig *, struct unisig_msg *);


/*
 * Local variables
 */
#ifdef DIAGNOSTIC
static int	unisig_print_msg = 0;
#endif


/*
 * Set a Cause IE based on information in an ATM attribute block
 *
 * Arguments:
 *	iep	pointer to a cause IE
 *	aap	pointer to attribute block
 *
 * Returns:
 *	0	message sent OK
 *	errno	error encountered
 *
 */
void
unisig_cause_from_attr(iep, aap)
	struct ie_generic	*iep;
	Atm_attributes		*aap;
{
	/*
	 * Copy cause info from attribute block to IE
	 */
	iep->ie_ident = UNI_IE_CAUS;
	iep->ie_coding = aap->cause.v.coding_standard;
	iep->ie_caus_loc = aap->cause.v.location;
	iep->ie_caus_cause = aap->cause.v.cause_value;
}


/*
 * Set a Cause IE based on information in a UNI signalling message
 *
 * Arguments:
 *	iep	pointer to a cause IE
 *	msg	pointer to message
 *	cause	cause code for the error
 *
 * Returns:
 *	0	message sent OK
 *	errno	error encountered
 *
 */
void
unisig_cause_from_msg(iep, msg, cause)
	struct ie_generic	*iep;
	struct unisig_msg	*msg;
	int			cause;
{
	struct ie_generic	*ie1;
	int			i;

	/*
	 * Fill out the cause IE fixed fields
	 */
	iep->ie_ident = UNI_IE_CAUS;
	iep->ie_caus_loc = UNI_IE_CAUS_LOC_USER;
	iep->ie_caus_cause = cause;

	/*
	 * Set diagnostics if indicated
	 */
	switch(cause) {
	case UNI_IE_CAUS_IECONTENT:
		iep->ie_caus_diag_len = 0;
		for (i = 0, ie1 = msg->msg_ie_err;
				ie1 && i < UNI_IE_CAUS_MAX_ID;
				ie1 = ie1->ie_next) {
			if (ie1->ie_err_cause == UNI_IE_CAUS_IECONTENT) {
				iep->ie_caus_diagnostic[i] =
						ie1->ie_ident;
				iep->ie_caus_diag_len++;
				i++;
			}
		}
		break;
	case UNI_IE_CAUS_REJECT:
		iep->ie_caus_diag_len = 2;
		iep->ie_caus_diagnostic[0] = UNI_IE_EXT_BIT +
				(UNI_IE_CAUS_RR_USER << UNI_IE_CAUS_RR_SHIFT) +
				UNI_IE_CAUS_RC_TRANS;
		iep->ie_caus_diagnostic[1] = 0;
		break;
	case UNI_IE_CAUS_MISSING:
		iep->ie_caus_diag_len = 0;
		for (i = 0, ie1 = msg->msg_ie_err;
				ie1 && i < UNI_IE_CAUS_MAX_ID;
				ie1 = ie1->ie_next) {
			if (ie1->ie_err_cause == UNI_IE_CAUS_MISSING) {
				iep->ie_caus_diagnostic[i] =
						ie1->ie_ident;
				iep->ie_caus_diag_len++;
				i++;
			}
		}
	}
}


/*
 * Send a UNISIG signalling message
 *
 * Called to send a Q.2931 message.  This routine encodes the message
 * and hands it to SSCF for transmission.
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance block
 *	msg	pointer to message
 *
 * Returns:
 *	0	message sent OK
 *	errno	error encountered
 *
 */
int
unisig_send_msg(usp, msg)
	struct unisig		*usp;
	struct unisig_msg	*msg;
{
	int		err = 0;
	struct usfmt	usf;

	ATM_DEBUG2("unisig_send_msg: msg=%p, type=%d\n", msg,
			msg->msg_type);

	/*
	 * Make sure the network is up
	 */
	if (usp->us_state != UNISIG_ACTIVE)
		return(ENETDOWN);

#ifdef DIAGNOSTIC
	/*
	 * Print the message we're sending.
	 */
	if (unisig_print_msg)
		usp_print_msg(msg, UNISIG_MSG_OUT);
#endif

	/*
	 * Convert message to network order
	 */
	err = usf_init(&usf, usp, (KBuffer *) 0, USF_ENCODE,
			usp->us_headout);
	if (err)
		return(err);

	err = usf_enc_msg(&usf, msg);
	if (err) {
		ATM_DEBUG1("unisig_send_msg: encode failed with %d\n",
				err);
		KB_FREEALL(usf.usf_m_base);
		return(EIO);
	}

#ifdef DIAGNOSTIC
	/*
	 * Print the converted message
	 */
	if (unisig_print_msg > 1)
		unisig_print_mbuf(usf.usf_m_base);
#endif

	/*
	 * Send the message
	 */
	err = atm_cm_saal_data(usp->us_conn, usf.usf_m_base);
	if (err)
		KB_FREEALL(usf.usf_m_base);

	return(err);
}


/*
 * Send a SETUP request
 *
 * Build and send a Q.2931 SETUP message.
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance block
 *	uvp	pointer to VCCB for which the request is being sent
 *
 * Returns:
 *	none
 *
 */
int
unisig_send_setup(usp, uvp)
	struct	unisig		*usp;
	struct	unisig_vccb	*uvp;
{
	int			err = 0;
	struct unisig_msg	*setup;
	Atm_attributes		*ap = &uvp->uv_connvc->cvc_attr;

	ATM_DEBUG1("unisig_send_setup: uvp=%p\n", uvp);

	/*
	 * Make sure required connection attriutes are set
	 */
	if (ap->aal.tag != T_ATM_PRESENT ||
			ap->traffic.tag != T_ATM_PRESENT ||
			ap->bearer.tag != T_ATM_PRESENT ||
			ap->called.tag != T_ATM_PRESENT ||
			ap->qos.tag != T_ATM_PRESENT) {
		err = EINVAL;
		setup = NULL;
		goto done;
	}

	/*
	 * Get memory for a SETUP message
	 */
	setup = uma_zalloc(unisig_msg_zone, M_ZERO);
	if (setup == NULL) {
		err = ENOMEM;
		goto done;
	}

	/*
	 * Fill in the SETUP message
	 */
	if (!uvp->uv_call_ref)
		uvp->uv_call_ref = unisig_alloc_call_ref(usp);
	setup->msg_call_ref = uvp->uv_call_ref;
	setup->msg_type = UNI_MSG_SETU;

	/*
	 * Set IEs from connection attributes
	 */
	err = unisig_set_attrs(usp, setup, ap);
	if (err)
		goto done;

	/*
	 * Attach a Calling Party Number IE if the user didn't
	 * specify one in the attribute block
	 */
	if (ap->calling.tag != T_ATM_PRESENT) {
		setup->msg_ie_cgad = uma_zalloc(unisig_ie_zone, 0);
		if (setup->msg_ie_cgad == NULL) {
			err = ENOMEM;
			goto done;
		}
		setup->msg_ie_cgad->ie_ident = UNI_IE_CGAD;
		ATM_ADDR_COPY(&usp->us_addr,
				&setup->msg_ie_cgad->ie_cgad_addr);
		ATM_ADDR_SEL_COPY(&usp->us_addr, 
				uvp->uv_nif ? uvp->uv_nif->nif_sel : 0, 
				&setup->msg_ie_cgad->ie_cgad_addr);
	}

	/*
	 * Send the SETUP message
	 */
	err = unisig_send_msg(usp, setup);

done:
	if (setup)
		unisig_free_msg(setup);

	return(err);
}


/*
 * Send a RELEASE message
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance block
 *	uvp	pointer to VCCB for which the RELEASE is being sent
 *	msg	pointer to UNI signalling message that the RELEASE
 *		responds to (may be NULL)
 *	cause	the reason for the RELEASE; a value of
 *		T_ATM_ABSENT indicates that the cause code is
 *		in the VCC's ATM attributes block
 *
 * Returns:
 *	none
 *
 */
int
unisig_send_release(usp, uvp, msg, cause)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
	int			cause;
{
	int			err = 0;
	struct unisig_msg	*rls_msg;
	struct ie_generic	*cause_ie;

	ATM_DEBUG2("unisig_send_release: usp=%p, uvp=%p\n",
			usp, uvp);

	/*
	 * Get memory for a RELEASE message
	 */
	rls_msg = uma_zalloc(unisig_msg_zone, M_ZERO);
	if (rls_msg == NULL) {
		return(ENOMEM);
	}
	cause_ie = uma_zalloc(unisig_ie_zone, M_ZERO);
	if (cause_ie == NULL) {
		uma_zfree(unisig_msg_zone, rls_msg);
		return(ENOMEM);
	}

	/*
	 * Fill in the RELEASE message
	 */
	rls_msg->msg_call_ref = uvp->uv_call_ref;
	rls_msg->msg_type = UNI_MSG_RLSE;
	rls_msg->msg_type_flag = 0;
	rls_msg->msg_type_action = 0;
	rls_msg->msg_ie_caus = cause_ie;

	/*
	 * Fill out the cause IE
	 */
	cause_ie->ie_ident = UNI_IE_CAUS;
	if (cause == T_ATM_ABSENT) {
		unisig_cause_from_attr(cause_ie,
				&uvp->uv_connvc->cvc_attr);
	} else {
		cause_ie->ie_caus_loc = UNI_IE_CAUS_LOC_USER;
		unisig_cause_from_msg(cause_ie, msg, cause);
	}

	/*
	 * Send the RELEASE
	 */
	err = unisig_send_msg(usp, rls_msg);
	unisig_free_msg(rls_msg);

	return(err);
}


/*
 * Send a RELEASE COMPLETE message
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance block
 *	uvp	pointer to VCCB for which the RELEASE is being sent.
 *		NULL indicates that a VCCB wasn't found for a call
 *		reference value.
 *	msg	pointer to the message which triggered the send
 *	cause	the cause code for the message; a value of
 *		T_ATM_ABSENT indicates that the cause code is
 *		in the VCC's ATM attributes block
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
unisig_send_release_complete(usp, uvp, msg, cause)
	struct unisig		*usp;
	struct unisig_vccb	*uvp;
	struct unisig_msg	*msg;
	int			cause;
{
	int			err = 0;
	struct unisig_msg	*rls_cmp;
	struct ie_generic	*cause_ie;

	ATM_DEBUG4("unisig_send_release_complete usp=%p, uvp=%p, msg=%p, cause=%d\n",
			usp, uvp, msg, cause);

	/*
	 * Get memory for a RELEASE COMPLETE message
	 */
	rls_cmp = uma_zalloc(unisig_msg_zone, M_ZERO);
	if (rls_cmp == NULL) {
		return(ENOMEM);
	}
	cause_ie = uma_zalloc(unisig_ie_zone, M_ZERO);
	if (cause_ie == NULL) {
		uma_zfree(unisig_msg_zone, rls_cmp);
		return(ENOMEM);
	}

	/*
	 * Fill in the RELEASE COMPLETE message
	 */
	if (uvp) {
		rls_cmp->msg_call_ref = uvp->uv_call_ref;
	} else if (msg) {
		rls_cmp->msg_call_ref = EXTRACT_CREF(msg->msg_call_ref);
	} else {
		rls_cmp->msg_call_ref = UNI_MSG_CALL_REF_GLOBAL;
	}
	rls_cmp->msg_type = UNI_MSG_RLSC;
	rls_cmp->msg_type_flag = 0;
	rls_cmp->msg_type_action = 0;
	rls_cmp->msg_ie_caus = cause_ie;

	/*
	 * Fill out the cause IE
	 */
	cause_ie->ie_ident = UNI_IE_CAUS;
	if (cause == T_ATM_ABSENT) {
		unisig_cause_from_attr(cause_ie,
				&uvp->uv_connvc->cvc_attr);
	} else {
		cause_ie->ie_caus_loc = UNI_IE_CAUS_LOC_USER;
		unisig_cause_from_msg(cause_ie, msg, cause);
	}

	/*
	 * Send the RELEASE COMPLETE
	 */
	err = unisig_send_msg(usp, rls_cmp);
	unisig_free_msg(rls_cmp);

	return(err);
}


/*
 * Send a STATUS message
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance block
 *	uvp	pointer to VCCB for which the STATUS is being sent.
 *		NULL indicates that a VCCB wasn't found for a call
 *		reference value.
 *	msg	pointer to the message which triggered the send
 *	cause	the cause code to include in the message
 *
 * Returns:
 *	none
 *
 */
int
unisig_send_status(usp, uvp, msg, cause)
	struct unisig		*usp;
	struct	unisig_vccb	*uvp;
	struct unisig_msg	*msg;
	int			cause;
{
	int			err = 0, i;
	struct unisig_msg	*stat_msg;
	struct ie_generic	*cause_ie, *clst_ie, *iep;

	ATM_DEBUG4("unisig_send_status: usp=%p, uvp=%p, msg=%p, cause=%d\n",
			usp, uvp, msg, cause);

	/*
	 * Get memory for a STATUS message
	 */
	stat_msg = uma_zalloc(unisig_msg_zone, M_ZERO);
	if (stat_msg == NULL) {
		return(ENOMEM);
	}
	cause_ie = uma_zalloc(unisig_ie_zone, M_ZERO);
	if (cause_ie == NULL) {
		uma_zfree(unisig_msg_zone, stat_msg);
		return(ENOMEM);
	}
	clst_ie = uma_zalloc(unisig_ie_zone, M_ZERO);
	if (clst_ie == NULL) {
		uma_zfree(unisig_msg_zone, stat_msg);
		uma_zfree(unisig_ie_zone, cause_ie);
		return(ENOMEM);
	}

	/*
	 * Fill in the STATUS message
	 */
	if (uvp) {
		stat_msg->msg_call_ref = uvp->uv_call_ref;
	} else if (msg) {
		stat_msg->msg_call_ref =
				EXTRACT_CREF(msg->msg_call_ref);
	} else {
		stat_msg->msg_call_ref = UNI_MSG_CALL_REF_GLOBAL;
	}
	stat_msg->msg_type = UNI_MSG_STAT;
	stat_msg->msg_type_flag = 0;
	stat_msg->msg_type_action = 0;
	stat_msg->msg_ie_clst = clst_ie;
	stat_msg->msg_ie_caus = cause_ie;

	/*
	 * Fill out the call state IE
	 */
	clst_ie->ie_ident = UNI_IE_CLST;
	clst_ie->ie_coding = 0;
	clst_ie->ie_flag = 0;
	clst_ie->ie_action = 0;
	if (uvp) {
		clst_ie->ie_clst_state = uvp->uv_sstate;
	} else {
		clst_ie->ie_clst_state = UNI_NULL;
	}

	/*
	 * Fill out the cause IE
	 */
	cause_ie->ie_ident = UNI_IE_CAUS;
	cause_ie->ie_coding = 0;
	cause_ie->ie_flag = 0;
	cause_ie->ie_action = 0;
	cause_ie->ie_caus_loc = UNI_IE_CAUS_LOC_USER;
	cause_ie->ie_caus_cause = cause;
	switch (cause) {
	case UNI_IE_CAUS_MTEXIST:
	case UNI_IE_CAUS_STATE:
		if (msg) {
			cause_ie->ie_caus_diagnostic[0] = msg->msg_type;
		}
		break;
	case UNI_IE_CAUS_MISSING:
	case UNI_IE_CAUS_IECONTENT:
	case UNI_IE_CAUS_IEEXIST:
		for (i=0, iep=msg->msg_ie_err;
				iep && i<UNI_MSG_IE_CNT;
				i++, iep = iep->ie_next) {
			if (iep->ie_err_cause == cause) {
				cause_ie->ie_caus_diagnostic[i] =
						iep->ie_ident;
			}
		}
	}

	/*
	 * Send the STATUS message
	 */
	err = unisig_send_msg(usp, stat_msg);
	unisig_free_msg(stat_msg);

	return(err);
}


/*
 * Process a RESTART message
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance block
 *	msg	pointer to the RESTART message
 *
 * Returns:
 *	none
 *
 */
static void
unisig_rcv_restart(usp, msg)
	struct unisig		*usp;
	struct unisig_msg	*msg;
{
	struct unisig_vccb	*uvp, *uvnext;
	struct unisig_msg	*rsta_msg;
	int			s;

	ATM_DEBUG2("unisig_rcv_restart: usp=%p, msg=%p\n",
			usp, msg);

	/*
	 * Check what class of VCCs we're supposed to restart
	 */
	if (msg->msg_ie_rsti->ie_rsti_class == UNI_IE_RSTI_IND_VC) {
		/*
		 * Just restart the indicated VCC
		 */
		if (msg->msg_ie_cnid) {
			uvp = unisig_find_vpvc(usp,
					msg->msg_ie_cnid->ie_cnid_vpci,
					msg->msg_ie_cnid->ie_cnid_vci,
					0);
			if (uvp && uvp->uv_type & VCC_SVC) {
				(void) unisig_clear_vcc(usp, uvp,
						T_ATM_CAUSE_NORMAL_CALL_CLEARING);
			}
		}
	} else {
		/*
		 * Restart all VCCs
		 */
		s = splnet();
		for (uvp=Q_HEAD(usp->us_vccq, struct unisig_vccb); uvp;
				uvp=uvnext) {
			uvnext = Q_NEXT(uvp, struct unisig_vccb,
					uv_sigelem);
			if (uvp->uv_type & VCC_SVC) {
				(void) unisig_clear_vcc(usp, uvp,
						T_ATM_CAUSE_NORMAL_CALL_CLEARING);
			}
		}
		(void) splx(s);
	}

	/*
	 * Get memory for a RESTART ACKNOWLEDGE message
	 */
	rsta_msg = uma_zalloc(unisig_msg_zone, 0);
	if (rsta_msg == NULL) {
		return;
	}

	/*
	 * Fill out the message
	 */
	rsta_msg->msg_call_ref = EXTRACT_CREF(msg->msg_call_ref);
	rsta_msg->msg_type = UNI_MSG_RSTA;
	rsta_msg->msg_type_flag = 0;
	rsta_msg->msg_type_action = 0;
	rsta_msg->msg_ie_rsti = msg->msg_ie_rsti;
	if (msg->msg_ie_cnid) {
		rsta_msg->msg_ie_cnid = msg->msg_ie_cnid;
	}

	/*
	 * Send the message
	 */
	(void) unisig_send_msg(usp, rsta_msg);
	rsta_msg->msg_ie_rsti = NULL;
	rsta_msg->msg_ie_cnid = NULL;
	unisig_free_msg(rsta_msg);

	return;
}


/*
 * Process a SETUP message
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance block
 *	msg	pointer to the SETUP message
 *
 * Returns:
 *	none
 *
 */
static void
unisig_rcv_setup(usp, msg)
	struct unisig		*usp;
	struct unisig_msg	*msg;
{
	struct unisig_vccb	*uvp = NULL;
	struct ie_generic	*iep;

	ATM_DEBUG2("unisig_rcv_setup: usp=%p, msg=%p\n", usp, msg);

	/*
	 * If we already have a VCC with the call reference,
	 * ignore the SETUP message
	 */
	uvp = unisig_find_conn(usp, EXTRACT_CREF(msg->msg_call_ref));
	if (uvp)
		return;

	/*
	 * If the call reference flag is incorrectly set, 
	 * ignore the SETUP message
	 */
	if (msg->msg_call_ref & UNI_MSG_CALL_REF_RMT)
		return;

	/*
	 * If there are missing mandatory IEs, send a
	 * RELEASE COMPLETE message and ignore the SETUP
	 */
	for (iep = msg->msg_ie_err; iep; iep = iep->ie_next) {
		if (iep->ie_err_cause == UNI_IE_CAUS_MISSING) {
			(void) unisig_send_release_complete(usp,
					uvp, msg, UNI_IE_CAUS_MISSING);
			return;
		}
	}

	/*
	 * If there are mandatory IEs with invalid content, send a
	 * RELEASE COMPLETE message and ignore the SETUP
	 */
	for (iep = msg->msg_ie_err; iep; iep = iep->ie_next) {
		if (iep->ie_err_cause == UNI_IE_CAUS_IECONTENT) {
			(void) unisig_send_release_complete(usp,
					uvp, msg,
					UNI_IE_CAUS_IECONTENT);
			return;
		}
	}

	/*
	 * Get a new VCCB for the connection
	 */
	uvp = uma_zalloc(unisig_vc_zone, M_ZERO);
	if (uvp == NULL) {
		return;
	}

	/*
	 * Put the VCCB on the UNISIG queue
	 */
	ENQUEUE(uvp, struct unisig_vccb, uv_sigelem, usp->us_vccq);

	/*
	 * Set the state and call reference value
	 */
	uvp->uv_sstate = UNI_NULL;
	uvp->uv_call_ref = EXTRACT_CREF(msg->msg_call_ref);

	/*
	 * Pass the VCCB and message to the VC state machine
	 */
	(void) unisig_vc_state(usp, uvp, UNI_VC_SETUP_MSG, msg);

	/*
	 * If the VCCB state is NULL, the open failed and the
	 * VCCB should be released
	 */
	if (uvp->uv_sstate == UNI_NULL) {
		DEQUEUE(uvp, struct unisig_vccb, uv_sigelem,
				usp->us_vccq);
		uma_zfree(unisig_vc_zone, uvp);
	}
	return;
}


/*
 * Process a UNISIG signalling message
 *
 * Called when a UNISIG message is received.  The message is decoded
 * and passed to the UNISIG state machine.  Unrecognized and
 * unexpected messages are logged.
 *
 * Arguments:
 *	usp	pointer to UNISIG protocol instance block
 *	m	pointer to a buffer chain containing the UNISIG message
 *
 * Returns:
 *	none
 *
 */
int
unisig_rcv_msg(usp, m)
	struct unisig	*usp;
	KBuffer		*m;
{
	int			err;
	u_int			cref;
	struct usfmt		usf;
	struct unisig_msg	*msg = 0;
	struct unisig_vccb	*uvp = 0;
	struct ie_generic	*iep;

	ATM_DEBUG2("unisig_rcv_msg: bfr=%p, len=%d\n", m, KB_LEN(m));

#ifdef NOTDEF
	unisig_print_mbuf(m);
#endif

	/*
	 * Get storage for the message
	 */
	msg = uma_zalloc(unisig_msg_zone, M_ZERO);
	if (msg == NULL) {
		err = ENOMEM;
		goto done;
	}

	/*
	 * Convert the message from network order to internal format
	 */
	err = usf_init(&usf, usp, m, USF_DECODE, 0);
	if (err) {
		if (err == EINVAL)
			panic("unisig_rcv_msg: invalid parameter\n");
		ATM_DEBUG1("unisig_rcv_msg: decode init failed with %d\n",
				err);
		goto done;
	}

	err = usf_dec_msg(&usf, msg);
	if (err) {
		ATM_DEBUG1("unisig_rcv_msg: decode failed with %d\n",
				err);
		goto done;
	}

#ifdef DIAGNOSTIC
	/*
	 * Debug--print some information about the message
	 */
	if (unisig_print_msg)
		usp_print_msg(msg, UNISIG_MSG_IN);
#endif

	/*
	 * Get the call reference value
	 */
	cref = EXTRACT_CREF(msg->msg_call_ref);

	/*
	 * Any message with the global call reference value except
	 * RESTART, RESTART ACK, or STATUS is in error
	 */
	if (GLOBAL_CREF(cref) &&
			msg->msg_type != UNI_MSG_RSTR &&
			msg->msg_type != UNI_MSG_RSTA &&
			msg->msg_type != UNI_MSG_STAT) {
		/*
		 * Send STATUS message indicating the error
		 */
		err = unisig_send_status(usp, (struct unisig_vccb *) 0,
				msg, UNI_IE_CAUS_CREF);
		goto done;
	}

	/*
	 * Check for missing mandatory IEs.  Checks for SETUP,
	 * RELEASE, and RELEASE COMPLETE are handled elsewhere.
	 */
	if (msg->msg_type != UNI_MSG_SETU &&
			msg->msg_type != UNI_MSG_RLSE &&
			msg->msg_type != UNI_MSG_RLSC) {
		for (iep = msg->msg_ie_err; iep; iep = iep->ie_next) {
			if (iep->ie_err_cause == UNI_IE_CAUS_MISSING) {
				err = unisig_send_status(usp,
						uvp, msg,
						UNI_IE_CAUS_MISSING);
				goto done;
			}
		}
	}

	/*
	 * Find the VCCB associated with the message
	 */
	uvp = unisig_find_conn(usp, cref);

	/*
	 * Process the message based on its type
	 */
	switch(msg->msg_type) {
	case UNI_MSG_CALP:
		(void) unisig_vc_state(usp, uvp,
				UNI_VC_CALLP_MSG, msg);
		break;
	case UNI_MSG_CONN:
		(void) unisig_vc_state(usp, uvp,
				UNI_VC_CONNECT_MSG, msg);
		break;
	case UNI_MSG_CACK:
		(void) unisig_vc_state(usp, uvp,
				UNI_VC_CNCTACK_MSG, msg);
		break;
	case UNI_MSG_SETU:
		unisig_rcv_setup(usp, msg);
		break;
	case UNI_MSG_RLSE:
		(void) unisig_vc_state(usp, uvp,
				UNI_VC_RELEASE_MSG, msg);
		break;
	case UNI_MSG_RLSC:
		/*
		 * Ignore a RELEASE COMPLETE with an unrecognized
		 * call reference value
		 */
		if (uvp) {
			(void) unisig_vc_state(usp, uvp,
					UNI_VC_RLSCMP_MSG, msg);
		}
		break;
	case UNI_MSG_RSTR:
		unisig_rcv_restart(usp, msg);
		break;
	case UNI_MSG_RSTA:
		break;
	case UNI_MSG_STAT:
		(void) unisig_vc_state(usp, uvp,
				UNI_VC_STATUS_MSG, msg);
		break;
	case UNI_MSG_SENQ:
		(void) unisig_vc_state(usp, uvp,
				UNI_VC_STATUSENQ_MSG, msg);
		break;
	case UNI_MSG_ADDP:
		(void) unisig_vc_state(usp, uvp,
				UNI_VC_ADDP_MSG, msg);
		break;
	case UNI_MSG_ADPA:
		(void) unisig_vc_state(usp, uvp,
				UNI_VC_ADDPACK_MSG, msg);
		break;
	case UNI_MSG_ADPR:
		(void) unisig_vc_state(usp, uvp,
				UNI_VC_ADDPREJ_MSG, msg);
		break;
	case UNI_MSG_DRPP:
		(void) unisig_vc_state(usp, uvp,
				UNI_VC_DROP_MSG, msg);
		break;
	case UNI_MSG_DRPA:
		(void) unisig_vc_state(usp, uvp,
				UNI_VC_DROPACK_MSG, msg);
		break;
	default:
		/*
		 * Message size didn't match size received
		 */
		err = unisig_send_status(usp, uvp, msg,
				UNI_IE_CAUS_MTEXIST);
	}

done:
	/*
	 * Handle message errors that require a response
	 */
	switch(err) {
	case EMSGSIZE:
		/*
		 * Message size didn't match size received
		 */
		err = unisig_send_status(usp, uvp, msg,
				UNI_IE_CAUS_LEN);
		break;
	}

	/*
	 * Free the incoming message (both buffer and internal format)
	 * if necessary.
	 */
	if (msg)
		unisig_free_msg(msg);
	if (m)
		KB_FREEALL(m);

	return (err);
}
