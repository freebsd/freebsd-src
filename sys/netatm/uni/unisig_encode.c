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
 * Message formatting module
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
#include <netatm/uni/unisig_mbuf.h>
#include <netatm/uni/unisig_decode.h>

/*
 * Local functions
 */
static int	usf_enc_ie(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_aalp(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_clrt(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_bbcp(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_bhli(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_blli(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_clst(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_cdad(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_cdsa(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_cgad(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_cgsa(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_caus(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_cnid(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_qosp(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_brpi(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_rsti(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_bsdc(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_trnt(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_uimp(struct usfmt *, struct ie_generic *);
static int	usf_enc_ie_ident(struct usfmt *, struct ie_generic *,
			struct ie_decode_tbl *);
static int	usf_enc_atm_addr(struct usfmt *, Atm_addr *);


/*
 * Local variables
 */
static struct {
	u_char	ident;			/* IE identifier */
	int	(*encode)(struct usfmt *, struct ie_generic *);
					/* Encoding function */
} ie_table[] = {
        { UNI_IE_AALP, usf_enc_ie_aalp },
        { UNI_IE_CLRT, usf_enc_ie_clrt },
        { UNI_IE_BBCP, usf_enc_ie_bbcp },
        { UNI_IE_BHLI, usf_enc_ie_bhli },
        { UNI_IE_BLLI, usf_enc_ie_blli },
        { UNI_IE_CLST, usf_enc_ie_clst },
        { UNI_IE_CDAD, usf_enc_ie_cdad },
        { UNI_IE_CDSA, usf_enc_ie_cdsa },
        { UNI_IE_CGAD, usf_enc_ie_cgad },
        { UNI_IE_CGSA, usf_enc_ie_cgsa },
        { UNI_IE_CAUS, usf_enc_ie_caus },
        { UNI_IE_CNID, usf_enc_ie_cnid },
        { UNI_IE_QOSP, usf_enc_ie_qosp },
        { UNI_IE_BRPI, usf_enc_ie_brpi },
        { UNI_IE_RSTI, usf_enc_ie_rsti },
        { UNI_IE_BLSH, usf_enc_ie_uimp },
        { UNI_IE_BNSH, usf_enc_ie_uimp },
        { UNI_IE_BSDC, usf_enc_ie_bsdc },
        { UNI_IE_TRNT, usf_enc_ie_trnt },
        { UNI_IE_EPRF, usf_enc_ie_uimp },
        { UNI_IE_EPST, usf_enc_ie_uimp },
        { 0,           0 }
};

extern struct ie_decode_tbl	ie_aal1_tbl[];
extern struct ie_decode_tbl	ie_aal4_tbl_30[];
extern struct ie_decode_tbl	ie_aal4_tbl_31[];
extern struct ie_decode_tbl	ie_aal5_tbl_30[];
extern struct ie_decode_tbl	ie_aal5_tbl_31[];
extern struct ie_decode_tbl	ie_clrt_tbl[];


/*
 * Encode a UNI signalling message
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	msg	pointer to a signalling message structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
int
usf_enc_msg(usf, msg)
	struct usfmt		*usf;
	struct unisig_msg	*msg;
{
	int			i, len, rc;
	u_char			c;
	u_char			*lp0, *lp1;
	struct ie_generic	*ie;

	union {
		short	s;
		u_char	sb[sizeof(short)];
	} su;

	ATM_DEBUG2("usf_enc_msg: usf=%p, msg=%p\n",
			usf, msg);

	/*
	 * Encode the protocol discriminator
	 */
	c = UNI_MSG_DISC_Q93B;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);

	/*
	 * Encode the call reference length
	 */
	c = 3;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);

	/*
	 * Encode the call reference
	 */
	rc = usf_int3(usf, &msg->msg_call_ref);
	if (rc)
		return(rc);

	/*
	 * Encode the message type
	 */
	rc = usf_byte(usf, &msg->msg_type);
	if (rc)
		return(rc);

	/*
	 * Encode the message type extension
	 */
	c = ((msg->msg_type_flag & UNI_MSG_TYPE_FLAG_MASK) <<
				UNI_MSG_TYPE_FLAG_SHIFT) +
			(msg->msg_type_action & UNI_MSG_TYPE_ACT_MASK) +
			UNI_IE_EXT_BIT;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);

	/*
	 * Save the location of the message length and encode a length
	 * of zero for now.  We'll fix the length up at the end.
	 */
	su.s = 0;
	rc = usf_byte_mark(usf, &su.sb[sizeof(short)-2], &lp0);
	if (rc)
		return(rc);
	rc = usf_byte_mark(usf, &su.sb[sizeof(short)-1], &lp1);
	if (rc)
		return(rc);

	/*
	 * Process information elements
	 */
	len = 0;
	for (i=0; i<UNI_MSG_IE_CNT; i++) {
		ie = msg->msg_ie_vec[i];
		while (ie) {
			rc = usf_enc_ie(usf, ie);
			if (rc)
				return(rc);
			len += (ie->ie_length + UNI_IE_HDR_LEN);
			ie = ie->ie_next;
		}
	}

	/*
	 * Fix the message length in the encoded message
	 */
	su.s = htons((u_short)len);
	*lp0 = su.sb[sizeof(short)-2];
	*lp1 = su.sb[sizeof(short)-1];

	return(0);
}


/*
 * Encode an information element
 *
 * Arguments:
 *	usf	pointer to a UNISIG formatting structure
 *	msg	pointer to a UNISIG message structure
 *	ie	pointer to a generic IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int			i, rc;
	u_char			c;
	u_char			*lp0, *lp1;

	union {
		short	s;
		u_char	sb[sizeof(short)];
	} su;

	ATM_DEBUG2("usf_enc_ie: usf=%p, ie=%p\n",
			usf, ie);

	/*
	 * Encode the IE identifier
	 */
	rc = usf_byte(usf, &ie->ie_ident);
	if (rc)
		return(rc);

	/*
	 * Encode the extended type
	 */
	c = ((ie->ie_coding & UNI_IE_CODE_MASK) << UNI_IE_CODE_SHIFT) +
			((ie->ie_flag & UNI_IE_FLAG_MASK) <<
			UNI_IE_FLAG_SHIFT) +
			(ie->ie_action & UNI_IE_ACT_MASK) +
			UNI_IE_EXT_BIT;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);

	/*
	 * Mark the current location in the output stream.  Encode a
	 * length of zero for now;  we'll come back and fix it up at
	 * the end.
	 */
	su.s = 0;
	rc = usf_byte_mark(usf, &su.sb[sizeof(short)-2], &lp0);
	if (rc)
		return(rc);
	rc = usf_byte_mark(usf, &su.sb[sizeof(short)-1], &lp1);
	if (rc)
		return(rc);

	/*
	 * Look up the information element in the table
	 */
	for (i=0; (ie->ie_ident != ie_table[i].ident) &&
			(ie_table[i].encode != NULL); i++) {
	}
	if (ie_table[i].encode == NULL) {
		/*
		 * Unrecognized IE
		 */
		return(EINVAL);
	}

	/*
	 * Process the IE by calling the function indicated
	 * in the IE table
	 */
	rc = ie_table[i].encode(usf, ie);
	if (rc)
		return(rc);

	/*
	 * Set the length in the output stream
	 */
	su.s = htons((u_short)ie->ie_length);
	*lp0 = su.sb[sizeof(short)-2];
	*lp1 = su.sb[sizeof(short)-1];

	return(0);
}


/*
 * Encode an AAL parameters information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to an AAL parms IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_aalp(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int		i, rc = 0;

	ATM_DEBUG2("usf_enc_ie_aalp: usf=%p, ie=%p\n",
			usf, ie);

	ie->ie_length = 0;

	/*
	 * Encode the AAL type
	 */
	if (ie->ie_aalp_aal_type == T_ATM_ABSENT)
		return(0);
	rc = usf_byte(usf, &ie->ie_aalp_aal_type);
	if (rc)
		return(rc);

	/*
	 * Process based on AAL type
	 */
	switch (ie->ie_aalp_aal_type) {
		case UNI_IE_AALP_AT_AAL1:
			rc = usf_enc_ie_ident(usf, ie, ie_aal1_tbl);
			break;
		case UNI_IE_AALP_AT_AAL3:
			if (usf->usf_sig->us_proto == ATM_SIG_UNI30)
				rc = usf_enc_ie_ident(usf, ie, ie_aal4_tbl_30);
			else
				rc = usf_enc_ie_ident(usf, ie, ie_aal4_tbl_31);
			break;
		case UNI_IE_AALP_AT_AAL5:
			if (usf->usf_sig->us_proto == ATM_SIG_UNI30)
				rc = usf_enc_ie_ident(usf, ie, ie_aal5_tbl_30);
			else
				rc = usf_enc_ie_ident(usf, ie, ie_aal5_tbl_31);
			break;
		case UNI_IE_AALP_AT_AALU:
			/*
			 * Encode the user data
			 */
			i = 0;
			while (i < sizeof(ie->ie_aalp_user_info)) {
				rc = usf_byte(usf, &ie->ie_aalp_user_info[i]);
				if (rc)
					break;
				i++;
				ie->ie_length++;
			}
			break;
		default:
			return(EINVAL);
	}

	ie->ie_length++;
	return(rc);
}


/*
 * Encode a user cell rate information element
 *
 * This routine just encodes the parameters required for best
 * effort service.
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a cell rate IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_clrt(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	rc;

	ATM_DEBUG2("usf_enc_ie_clrt: usf=%p, ie=%p\n",
			usf, ie);

#ifdef NOTDEF
	/*
	 * Encode Peak Cell Rate Forward CLP = 0 + 1
	 */
	c = UNI_IE_CLRT_FWD_PEAK_01_ID;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	rc = usf_int3(usf, &ie->ie_clrt_fwd_peak_01);
	if (rc)
		return(rc);

	/*
	 * Encode Peak Cell Rate Backward CLP = 0 + 1
	 */
	c = UNI_IE_CLRT_BKWD_PEAK_01_ID;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	rc = usf_int3(usf, &ie->ie_clrt_bkwd_peak_01);
	if (rc)
		return(rc);

	/*
	 * Encode Best Effort Flag
	 */
	c = UNI_IE_CLRT_BEST_EFFORT_ID;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);

	/*
	 * Set IE length
	 */
	ie->ie_length = 9;
#endif

	/*
	 * Encode the user cell rate IE using the table
	 */
	ie->ie_length = 0;
	rc = usf_enc_ie_ident(usf, ie, ie_clrt_tbl);

	return(rc);
}


/*
 * Encode a broadband bearer capability information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a cell rate IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_bbcp(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	rc;
	u_char	c;

	ATM_DEBUG2("usf_enc_ie_bbcp: usf=%p, ie=%p\n",
			usf, ie);

	ie->ie_length = 0;

	/*
	 * Encode the broadband bearer class
	 */
	if (ie->ie_bbcp_bearer_class == T_ATM_ABSENT)
		return(0);
	c = ie->ie_bbcp_bearer_class & UNI_IE_BBCP_BC_MASK;
	if (ie->ie_bbcp_bearer_class != UNI_IE_BBCP_BC_BCOB_X)
		c |= UNI_IE_EXT_BIT;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	ie->ie_length++;

	/*
	 * If the broadband bearer class was X, the next
	 * byte has the traffic type and timing requirements
	 */
	if (ie->ie_bbcp_bearer_class == UNI_IE_BBCP_BC_BCOB_X) {
		c = ((ie->ie_bbcp_traffic_type & UNI_IE_BBCP_TT_MASK) <<
				UNI_IE_BBCP_TT_SHIFT) +
				(ie->ie_bbcp_timing_req &
				UNI_IE_BBCP_TR_MASK) +
				UNI_IE_EXT_BIT;
		rc = usf_byte(usf, &c);
		if (rc)
			return(rc);
		ie->ie_length++;
	}

	/*
	 * Encode the clipping and user plane connection configuration
	 */
	c = ((ie->ie_bbcp_clipping & UNI_IE_BBCP_SC_MASK) <<
			UNI_IE_BBCP_SC_SHIFT) +
			(ie->ie_bbcp_conn_config &
			UNI_IE_BBCP_CC_MASK) +
			UNI_IE_EXT_BIT;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	ie->ie_length++;

	return(0);
}


/*
 * Encode a broadband high layer information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a cell rate IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_bhli(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	i, rc;
	u_int	type;

	ATM_DEBUG2("usf_enc_ie_bhli: usf=%p, ie=%p\n",
			usf, ie);

	ie->ie_length = 0;

	/*
	 * Encode the high layer information type
	 */
	if (ie->ie_bhli_type == T_ATM_ABSENT)
		return(0);
	type = ie->ie_bhli_type | UNI_IE_EXT_BIT;
	rc = usf_ext(usf, &type);
	if (rc)
		return(rc);
	ie->ie_length++;

	/*
	 * What comes next depends on the type
	 */
	switch (ie->ie_bhli_type) {
	case UNI_IE_BHLI_TYPE_ISO:
	case UNI_IE_BHLI_TYPE_USER:
		/*
		 * ISO or user-specified parameters -- take the
		 * length of information from the IE length
		 */
		for (i=0; i<ie->ie_length-1; i++) {
			rc = usf_byte(usf, &ie->ie_bhli_info[i]);
			if (rc)
				return(rc);
			ie->ie_length++;
		}
		break;
	case UNI_IE_BHLI_TYPE_HLP:
		/*
		 * Make sure the IE is long enough for the high
		 * layer profile information, then get it
		 */
		if (usf->usf_sig->us_proto != ATM_SIG_UNI30)
			return (EINVAL);
		for (i=0; i<UNI_IE_BHLI_HLP_LEN; i++) {
			rc = usf_byte(usf, &ie->ie_bhli_info[i]);
			if (rc)
				return(rc);
			ie->ie_length++;
		}
		break;
	case UNI_IE_BHLI_TYPE_VSA:
		/*
		 * Make sure the IE is long enough for the vendor-
		 * specific application information, then get it
		 */
		for (i=0; i<UNI_IE_BHLI_VSA_LEN; i++) {
			rc = usf_byte(usf, &ie->ie_bhli_info[i]);
			if (rc)
				return(rc);
			ie->ie_length++;
		}
		break;
	default:
		return(EINVAL);
	}

	return(0);
}


/*
 * Encode a broadband low layer information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a cell rate IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_blli(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	u_char		c;
	int		rc;
	u_int		ipi;

	ATM_DEBUG2("usf_enc_ie_blli: usf=%p, ie=%p\n",
			usf, ie);

	ie->ie_length = 0;

	/*
	 * Encode paramteters for whichever protocol layers the
	 * user specified
	 */

	/*
	 * Layer 1 information
	 */
	if (ie->ie_blli_l1_id && ie->ie_blli_l1_id != T_ATM_ABSENT) {
		c = (UNI_IE_BLLI_L1_ID << UNI_IE_BLLI_LID_SHIFT) +
				(ie->ie_blli_l1_id &
				UNI_IE_BLLI_LP_MASK) +
				UNI_IE_EXT_BIT;
		rc = usf_byte(usf, &c);
		if (rc)
			return(rc);
		ie->ie_length++;
	}

	/*
	 * Layer 2 information
	 */
	if (ie->ie_blli_l2_id && ie->ie_blli_l2_id != T_ATM_ABSENT) {
		c = (UNI_IE_BLLI_L2_ID << UNI_IE_BLLI_LID_SHIFT) +
				(ie->ie_blli_l2_id &
				UNI_IE_BLLI_LP_MASK);

		switch (ie->ie_blli_l2_id) {
		case UNI_IE_BLLI_L2P_X25L:
		case UNI_IE_BLLI_L2P_X25M:
		case UNI_IE_BLLI_L2P_HDLC1:
		case UNI_IE_BLLI_L2P_HDLC2:
		case UNI_IE_BLLI_L2P_HDLC3:
		case UNI_IE_BLLI_L2P_Q922:
		case UNI_IE_BLLI_L2P_ISO7776:
			/*
			 * Write the Layer 2 type
			 */
			rc = usf_byte(usf, &c);
			if (rc)
				return(rc);
			ie->ie_length++;

			/*
			 * Encode the Layer 2 mode
			 */
			if (ie->ie_blli_l2_mode) {
				c = (ie->ie_blli_l2_mode &
					UNI_IE_BLLI_L2MODE_MASK) <<
					UNI_IE_BLLI_L2MODE_SHIFT;
				if (!ie->ie_blli_l2_window)
					c |= UNI_IE_EXT_BIT;

				rc = usf_byte(usf, &c);
				if (rc)
					return(rc);
				ie->ie_length++;
			}

			/*
			 * Encode the Layer 2 window size
			 */
			if (ie->ie_blli_l2_window) {
				c = (ie->ie_blli_l2_window &
						UNI_IE_EXT_MASK) +
						UNI_IE_EXT_BIT;

				rc = usf_byte(usf, &c);
				if (rc)
					return(rc);
				ie->ie_length++;
			}
			break;
		case UNI_IE_BLLI_L2P_USER:
			/*
			 * Write the Layer 2 type
			 */
			rc = usf_byte(usf, &c);
			if (rc)
				return(rc);
			ie->ie_length++;

			/*
			 * Encode the user-specified layer 2 info
			 */
			c = (ie->ie_blli_l2_user_proto &
					UNI_IE_EXT_MASK) +
					UNI_IE_EXT_BIT;
			rc = usf_byte(usf, &c);
			if (rc)
				return(rc);
			ie->ie_length++;
			break;
		default:
			/*
			 * Write the Layer 2 type
			 */
			c |= UNI_IE_EXT_BIT;
			rc = usf_byte(usf, &c);
			if (rc)
				return(rc);
			ie->ie_length++;
			break;
		}
	}

	/*
	 * Layer 3 information
	 */
	if (ie->ie_blli_l3_id && ie->ie_blli_l3_id != T_ATM_ABSENT) {
		/*
		 * Encode the layer 3 protocol ID
		 */
		c = (UNI_IE_BLLI_L3_ID << UNI_IE_BLLI_LID_SHIFT) +
				(ie->ie_blli_l3_id &
				UNI_IE_BLLI_LP_MASK);

		/*
		 * Process other fields based on protocol ID
		 */
		switch(ie->ie_blli_l3_id) {
		case UNI_IE_BLLI_L3P_X25:
		case UNI_IE_BLLI_L3P_ISO8208:
		case UNI_IE_BLLI_L3P_ISO8878:
			/*
			 * Write the protocol ID
			 */
			rc = usf_byte(usf, &c);
			if (rc)
				return(rc);
			ie->ie_length++;

			if (ie->ie_blli_l3_mode ||
					ie->ie_blli_l3_packet_size ||
					ie->ie_blli_l3_window) {
				c = (ie->ie_blli_l3_mode &
					UNI_IE_BLLI_L3MODE_MASK) <<
					UNI_IE_BLLI_L3MODE_SHIFT;
				if (!ie->ie_blli_l3_packet_size &&
						!ie->ie_blli_l3_window)
					c |= UNI_IE_EXT_BIT;

				rc = usf_byte(usf, &c);
				if (rc)
					return(rc);
				ie->ie_length++;
			}

			if (ie->ie_blli_l3_packet_size ||
					ie->ie_blli_l3_window) {
				c = ie->ie_blli_l3_packet_size &
						UNI_IE_BLLI_L3PS_MASK;
				if (!ie->ie_blli_l3_window)
					c |= UNI_IE_EXT_BIT;

				rc = usf_byte(usf, &c);
				if (rc)
					return(rc);
				ie->ie_length++;
			}

			if (ie->ie_blli_l3_window) {
				c = (ie->ie_blli_l3_window &
						UNI_IE_EXT_MASK) +
						UNI_IE_EXT_BIT;

				rc = usf_byte(usf, &c);
				if (rc)
					return(rc);
				ie->ie_length++;
			}
			break;
		case UNI_IE_BLLI_L3P_USER:
			/*
			 * Write the protocol ID
			 */
			rc = usf_byte(usf, &c);
			if (rc)
				return(rc);
			ie->ie_length++;

			/*
			 * Encode the user-specified protocol info
			 */
			c = (ie->ie_blli_l3_user_proto &
						UNI_IE_EXT_MASK) +
						UNI_IE_EXT_BIT;

			rc = usf_byte(usf, &c);
			if (rc)
				return(rc);
			ie->ie_length++;
			break;
		case UNI_IE_BLLI_L3P_ISO9577:
			/*
			 * Write the protocol ID
			 */
			rc = usf_byte(usf, &c);
			if (rc)
				return(rc);
			ie->ie_length++;

			/*
			 * Encode the IPI
			 */
			ipi = ie->ie_blli_l3_ipi <<
					UNI_IE_BLLI_L3IPI_SHIFT;
			rc = usf_ext(usf, &ipi);
			if (rc)
				return(rc);
			ie->ie_length += 2;

			if (ie->ie_blli_l3_ipi ==
					UNI_IE_BLLI_L3IPI_SNAP) {
				c = UNI_IE_EXT_BIT;
				rc = usf_byte(usf, &c);
				if (rc)
					return(rc);

				rc = usf_byte(usf,
						&ie->ie_blli_l3_oui[0]);
				if (rc)
					return(rc);

				rc = usf_byte(usf,
						&ie->ie_blli_l3_oui[1]);
				if (rc)
					return(rc);

				rc = usf_byte(usf,
						&ie->ie_blli_l3_oui[2]);
				if (rc)
					return(rc);

				rc = usf_byte(usf,
						&ie->ie_blli_l3_pid[0]);
				if (rc)
					return(rc);

				rc = usf_byte(usf,
						&ie->ie_blli_l3_pid[1]);
				if (rc)
					return(rc);

				ie->ie_length += 6;
			}
			break;
		default:
			/*
			 * Write the layer 3 protocol ID
			 */
			c |= UNI_IE_EXT_BIT;
			rc = usf_byte(usf, &c);
			if (rc)
				return(rc);
			ie->ie_length++;
			break;
		}
	}

	return(0);
}


/*
 * Encode a call state information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a cell rate IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_clst(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	rc;
	u_char	c;

	ATM_DEBUG2("usf_enc_ie_clst: usf=%p, ie=%p\n",
			usf, ie);

	c = ie->ie_clst_state & UNI_IE_CLST_STATE_MASK;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	ie->ie_length = 1;

	return(0);
}


/*
 * Encode a called party number information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a cell rate IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_cdad(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	u_char	c;
	int	rc;

	ATM_DEBUG2("usf_enc_ie_cdad: usf=%p, ie=%p\n",
			usf, ie);

	/*
	 * Encode the numbering plan
	 */
	switch(ie->ie_cdad_addr.address_format) {
	case T_ATM_E164_ADDR:
		c = UNI_IE_CDAD_PLAN_E164 +
				(UNI_IE_CDAD_TYPE_INTL
					<< UNI_IE_CDAD_TYPE_SHIFT);
		ie->ie_length = sizeof(Atm_addr_e164) + 1;
		break;
	case T_ATM_ENDSYS_ADDR:
		c = UNI_IE_CDAD_PLAN_NSAP +
				(UNI_IE_CDAD_TYPE_UNK
					<< UNI_IE_CDAD_TYPE_SHIFT);
		ie->ie_length = sizeof(Atm_addr_nsap) + 1;
		break;
	default:
		return(EINVAL);
	}
	c |= UNI_IE_EXT_BIT;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);

	/*
	 * Encode the ATM address
	 */
	rc = usf_enc_atm_addr(usf, &ie->ie_cdad_addr);

	return(rc);
}


/*
 * Encode a called party subaddress information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a cell rate IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_cdsa(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	u_char	c;
	int	rc;

	/*
	 * Encode the subaddress type
	 */
	switch(ie->ie_cdsa_addr.address_format) {
	case T_ATM_ENDSYS_ADDR:
		c = UNI_IE_CDSA_TYPE_AESA << UNI_IE_CDSA_TYPE_SHIFT;
		ie->ie_length = sizeof(Atm_addr_nsap) + 1;
		break;
	default:
		return(EINVAL);
	}
	c |= UNI_IE_EXT_BIT;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);

	/*
	 * Encode the ATM address
	 */
	rc = usf_enc_atm_addr(usf, &ie->ie_cdsa_addr);

	return(rc);
}


/*
 * Encode a calling party number information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a cell rate IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_cgad(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	u_char	c;
	int	rc;

	ATM_DEBUG2("usf_enc_ie_cgad: usf=%p, ie=%p\n",
			usf, ie);

	/*
	 * Encode the numbering plan
	 */
	switch(ie->ie_cgad_addr.address_format) {
	case T_ATM_E164_ADDR:
		c = UNI_IE_CGAD_PLAN_E164 +
				(UNI_IE_CGAD_TYPE_INTL
					<< UNI_IE_CGAD_TYPE_SHIFT) +
				UNI_IE_EXT_BIT;
		ie->ie_length = sizeof(Atm_addr_e164) + 1;
		break;
	case T_ATM_ENDSYS_ADDR:
		c = UNI_IE_CGAD_PLAN_NSAP +
				(UNI_IE_CGAD_TYPE_UNK
					<< UNI_IE_CGAD_TYPE_SHIFT) +
				UNI_IE_EXT_BIT;
		ie->ie_length = sizeof(Atm_addr_nsap) + 1;
		break;
	default:
		return(EINVAL);
	}
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);

	/*
	 * Encode the presentation and screening indicators
	 */
#ifdef NOTDEF
	c = ((ie->ie_cgad_pres_ind & UNI_IE_CGAD_PRES_MASK)
				<< UNI_IE_CGAD_PRES_SHIFT) +
			(ie->ie_cgad_screen_ind &
				UNI_IE_CGAD_SCR_MASK) +
			UNI_IE_EXT_BIT;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
#endif
	

	/*
	 * Encode the ATM address
	 */
	rc = usf_enc_atm_addr(usf, &ie->ie_cgad_addr);

	return(rc);
}


/*
 * Encode a calling party subaddress information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a cell rate IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_cgsa(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	u_char	c;
	int	rc;

	/*
	 * Encode the subaddress type
	 */
	switch(ie->ie_cgsa_addr.address_format) {
	case T_ATM_ENDSYS_ADDR:
		c = UNI_IE_CGSA_TYPE_AESA << UNI_IE_CGSA_TYPE_SHIFT;
		ie->ie_length = sizeof(Atm_addr_nsap) + 1;
		break;
	default:
		return(EINVAL);
	}
	c |= UNI_IE_EXT_BIT;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);

	/*
	 * Encode the ATM address
	 */
	rc = usf_enc_atm_addr(usf, &ie->ie_cgsa_addr);

	return(rc);
}


/*
 * Encode a cause information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a cell rate IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_caus(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int			i, rc;
	u_char			c;

	ATM_DEBUG2("usf_enc_ie_caus: usf=%p, ie=%p\n",
			usf, ie);

	ie->ie_length = 0;

	/*
	 * Encode the cause location
	 */
	c = (ie->ie_caus_loc & UNI_IE_CAUS_LOC_MASK) | UNI_IE_EXT_BIT;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	ie->ie_length++;

	/*
	 * Encode the cause value
	 */
	c = ie->ie_caus_cause | UNI_IE_EXT_BIT;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	ie->ie_length++;

	/*
	 * Encode any included diagnostics
	 */
	for (i = 0; i < ie->ie_caus_diag_len &&
			i < sizeof(ie->ie_caus_diagnostic);
			i++) {
		rc = usf_byte(usf, &ie->ie_caus_diagnostic[i]);
		if (rc)
			return(rc);
		ie->ie_length++;
	}

	return(0);
}


/*
 * Encode a conection identifier information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a cell rate IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_cnid(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	rc;
	u_char	c;

	ATM_DEBUG2("usf_enc_ie_cnid: usf=%p, ie=%p\n",
			usf, ie);

	c = ((ie->ie_cnid_vp_sig & UNI_IE_CNID_VPSIG_MASK)
				<< UNI_IE_CNID_VPSIG_SHIFT) +
			(ie->ie_cnid_pref_excl & UNI_IE_CNID_PREX_MASK) +
			UNI_IE_EXT_BIT;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);

	rc = usf_short(usf, &ie->ie_cnid_vpci);
	if (rc)
		return(rc);
	rc = usf_short(usf, &ie->ie_cnid_vci);
	if (rc)
		return(rc);

	ie->ie_length = 5;
	return(0);
}


/*
 * Encode a quality of service parameters information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a cell rate IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_qosp(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
        int             rc;

	ATM_DEBUG2("usf_enc_ie_qosp: usf=%p, ie=%p\n",
			usf, ie);

        /*
         * Encode forward QoS class
         */
	if (ie->ie_qosp_fwd_class == T_ATM_ABSENT ||
			ie->ie_qosp_bkwd_class == T_ATM_ABSENT)
		return(0);
        rc = usf_byte(usf, &ie->ie_qosp_fwd_class);
        if (rc)
                return(rc);

        /*
         * Encode backward QoS class
         */
        rc = usf_byte(usf, &ie->ie_qosp_bkwd_class);

	ie->ie_length = 2;
        return(rc);
}


/*
 * Encode a broadband repeat indicator information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a cell rate IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_brpi(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	rc;
	u_char	c;

	ATM_DEBUG2("usf_enc_ie_brpi: usf=%p, ie=%p\n",
			usf, ie);

	/*
	 * Encode the repeat indicator
	 */
	c = ie->ie_brpi_ind + UNI_IE_EXT_BIT;
	rc = usf_byte(usf, &c);

	return(rc);
}


/*
 * Encode a restart indicator information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a cell rate IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_rsti(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	rc;
	u_char	c;

	ATM_DEBUG2("usf_enc_ie_rsti: usf=%p, ie=%p\n",
			usf, ie);

	/*
	 * Encode the restart class
	 */
	c = (ie->ie_rsti_class & UNI_IE_RSTI_CLASS_MASK) |
			UNI_IE_EXT_BIT;
	rc = usf_byte(usf, &c);
	ie->ie_length = 1;

	return(rc);
}


/*
 * Encode a broadband sending complete information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a broadband sending complete IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_bsdc(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	rc;
	u_char	c;

	ATM_DEBUG2("usf_enc_ie_bsdc: usf=%p, ie=%p\n",
			usf, ie);

	/*
	 * Encode the sending complete indicator
	 */
	c = UNI_IE_BSDC_IND | UNI_IE_EXT_BIT;
	rc = usf_byte(usf, &c);
	ie->ie_length = 1;

	return(rc);
}


/*
 * Encode a transit network selection information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a transit network selection rate IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_trnt(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	i, rc;
	u_char	c;

	ATM_DEBUG2("usf_enc_ie_trnt: usf=%p, ie=%p\n",
			usf, ie);

	/*
	 * Encode the sending complete indicator
	 */
	c = ((ie->ie_trnt_id_type & UNI_IE_TRNT_IDT_MASK) <<
				UNI_IE_TRNT_IDT_SHIFT) +
			(ie->ie_trnt_id_plan & UNI_IE_TRNT_IDP_MASK) +
			UNI_IE_EXT_BIT;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	ie->ie_length = 1;

	/*
	 * Encode the network identification
	 */
	for (i=0; i<ie->ie_trnt_id_len; i++) {
		rc = usf_byte(usf, &ie->ie_trnt_id[i]);
		if (rc)
			return(rc);
		ie->ie_length++;
	}

	return(rc);
}


/*
 * Encode an unsupported IE type
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to an IE structure
 *
 * Returns:
 *	0	success
 *
 */
static int
usf_enc_ie_uimp(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	return(0);
}


/*
 * Encode an information element using field identifiers
 *
 * The AAL parameters and ATM user cell rate IEs are formatted
 * with a one-byte identifier preceding each field.  The routine
 * encodes these IEs by using a table which relates the field
 * identifiers with the fields in the appropriate IE structure.
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a cell rate IE structure
 *	tbl	pointer to an IE decoding table
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_ie_ident(usf, ie, tbl)
	struct usfmt		*usf;
	struct ie_generic	*ie;
	struct ie_decode_tbl	*tbl;
{
	int	i, len, rc;
	char	*cp;
	u_int8_t	cv;
	u_int16_t	sv;
	u_int32_t	iv;

	ATM_DEBUG3("usf_enc_ie_ident: usf=%p, ie=%p, tbl=%p\n",
			usf, ie, tbl);

	/*
	 * Scan through the IE table
	 */
	len = 0;
	for (i=0; tbl[i].ident; i++) {
		/*
		 * Check whether to send the field
		 */
		cp = (char *) ((intptr_t)ie + tbl[i].f_offs);
		if (tbl[i].len == 0) {
			if ((*cp == T_NO || *cp == T_ATM_ABSENT))
				continue;
		} else {
			switch (tbl[i].f_size) {
			case 1:
				if (*(int8_t *)cp == T_ATM_ABSENT)
					continue;
				break;
			case 2:
				if (*(int16_t *)cp == T_ATM_ABSENT)
					continue;
				break;
			case 4:
				if (*(int32_t *)cp == T_ATM_ABSENT)
					continue;
				break;
			default:
badtbl:
				log(LOG_ERR,
				    "uni encode: id=%d,len=%d,off=%d,size=%d\n",
					tbl[i].ident, tbl[i].len,
					tbl[i].f_offs, tbl[i].f_size);
				return (EFAULT);
			}
		}

		/*
		 * Encode the field identifier
		 */
		rc = usf_byte(usf, &tbl[i].ident);
		if (rc)
			return(rc);
		len++;

		/*
		 * Encode the field value
		 */
		switch (tbl[i].len) {
			case 0:
				break;
			case 1:
				switch (tbl[i].f_size) {
				case 1:
					cv = *(u_int8_t *)cp;
					break;
				case 2:
					cv = *(u_int16_t *)cp;
					break;
				case 4:
					cv = *(u_int32_t *)cp;
					break;
				default:
					goto badtbl;
				}
				rc = usf_byte(usf, &cv);
				break;

			case 2:
				switch (tbl[i].f_size) {
				case 2:
					sv = *(u_int16_t *)cp;
					break;
				case 4:
					sv = *(u_int32_t *)cp;
					break;
				default:
					goto badtbl;
				}
				rc = usf_short(usf, &sv);
				break;

			case 3:
				switch (tbl[i].f_size) {
				case 4:
					iv = *(u_int32_t *)cp;
					break;
				default:
					goto badtbl;
				}
				rc = usf_int3(usf, &iv);
				break;

			case 4:
				switch (tbl[i].f_size) {
				case 4:
					iv = *(u_int32_t *)cp;
					break;
				default:
					goto badtbl;
				}
				rc = usf_int(usf, &iv);
				break;

			default:
				goto badtbl;
		}

		len += tbl[i].len;

		if (rc)
			return(rc);
	}

	ie->ie_length = len;
	return(0);
}


/*
 * Encode an ATM address
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	addr	pointer to an ATM address structure.  The address
 *		type must already be set correctly.
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_enc_atm_addr(usf, addr)
	struct usfmt	*usf;
	Atm_addr	*addr;
{
	int	len, rc;
	u_char	*cp;

	/*
	 * Check the address type
	 */
	switch (addr->address_format) {
	case T_ATM_E164_ADDR:
		cp = (u_char *) addr->address;
		len = sizeof(Atm_addr_e164);
		break;
	case T_ATM_ENDSYS_ADDR:
		cp = (u_char *) addr->address;
		len = sizeof(Atm_addr_nsap);
		break;
	default:
		return(EINVAL);
	}

	/*
	 * Get the address bytes
	 */
	while (len) {
		rc = usf_byte(usf, cp);
		if (rc)
			return(rc);
		len--;
		cp++;
	}

	return(0);
}
