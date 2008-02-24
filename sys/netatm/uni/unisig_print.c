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
 * Print Q.2931 messages
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netatm/uni/unisig_print.c,v 1.12 2005/01/07 01:45:37 imp Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_vc.h>
#include <netatm/atm_sigmgr.h>

#include <netatm/uni/unisig_var.h>
#include <netatm/uni/unisig_msg.h>
#include <netatm/uni/unisig_print.h>

/*
 * Local declarations
 */
struct type_name {
	char	*name;
	u_char	type;
};


/*
 * Local functions
 */
static char *	find_type(struct type_name *, u_char);
static void	usp_print_atm_addr(Atm_addr *);
static void	usp_print_ie(struct ie_generic *);
static void	usp_print_ie_aalp(struct ie_generic *);
static void	usp_print_ie_clrt(struct ie_generic *);
static void	usp_print_ie_bbcp(struct ie_generic *);
static void	usp_print_ie_bhli(struct ie_generic *);
static void	usp_print_ie_blli(struct ie_generic *);
static void	usp_print_ie_clst(struct ie_generic *);
static void	usp_print_ie_cdad(struct ie_generic *);
static void	usp_print_ie_cdsa(struct ie_generic *);
static void	usp_print_ie_cgad(struct ie_generic *);
static void	usp_print_ie_cgsa(struct ie_generic *);
static void	usp_print_ie_caus(struct ie_generic *);
static void	usp_print_ie_cnid(struct ie_generic *);
static void	usp_print_ie_qosp(struct ie_generic *);
static void	usp_print_ie_brpi(struct ie_generic *);
static void	usp_print_ie_rsti(struct ie_generic *);
static void	usp_print_ie_blsh(struct ie_generic *);
static void	usp_print_ie_bnsh(struct ie_generic *);
static void	usp_print_ie_bsdc(struct ie_generic *);
static void	usp_print_ie_trnt(struct ie_generic *);
static void	usp_print_ie_eprf(struct ie_generic *);
static void	usp_print_ie_epst(struct ie_generic *);


/*
 * Values for Q.2931 message type.
 */
static struct type_name	msg_types[] = {
	{ "Call proceeding",	0x02 },
	{ "Connect",		0x07 },
	{ "Connect ACK",	0x0F },
	{ "Setup",		0x05 },
	{ "Release",		0x4D },
	{ "Release complete",	0x5A },
	{ "Restart",		0x46 },
	{ "Restart ACK",	0x4E },
	{ "Status",		0x7D },
	{ "Status enquiry",	0x75 },
	{ "Add party",		0x80 },
	{ "Add party ACK",	0x81 },
	{ "Add party reject",	0x82 },
	{ "Drop party",		0x83 },
	{ "Drop party ACK",	0x84 },
	{0,			0}
};


/*
 * Values for information element identifier.
 */
static struct type_name ie_types[] = {
	{ "Cause",				0x08 },
	{ "Call state",				0x14 },
	{ "Endpoint reference",			0x54 },
	{ "Endpoint state",			0x55 },
	{ "ATM AAL parameters",			0x58 },
	{ "ATM user cell rate",			0x59 },
	{ "Connection ID",			0x5A },
	{ "QoS parameter",			0x5C },
	{ "Broadband high layer info",		0x5D },
	{ "Broadband bearer capability",	0x5E },
	{ "Broadband low layer info",		0x5F },
	{ "Broadband locking shift",		0x60 },
	{ "Broadband non-locking shift",	0x61 },
	{ "Broadband sending complete",		0x62 },
	{ "Broadband repeat indicator",		0x63 },
	{ "Calling party number",		0x6C },
	{ "Calling party subaddress",		0x6D },
	{ "Called party number",		0x70 },
	{ "Called party subaddress",		0x71 },
	{ "Transit net selection",		0x78 },
	{ "Restart indicator",			0x79 },
	{ 0,					0 }
};


/*
 * Search a name - type translation table
 *
 * Arguments:
 *	tbl	a pointer to the table to search
 *	type	the type to look for
 *
 * Returns:
 *	name	a pointer to a character string with the name
 *
 */
static char *
find_type(tbl, type)
	struct type_name	*tbl;
	u_char			type;
{
	while (type != tbl->type && tbl->name)
		tbl++;

	if (tbl->name)
		return(tbl->name);
	else
		return("-");
}


/*
 * Print an ATM address
 *
 * Arguments:
 *	p	pointer to an Atm_address
 *
 * Returns:
 *	none
 *
 */
static void
usp_print_atm_addr(p)
	Atm_addr		*p;
{
	char		*cp;

	cp = unisig_addr_print(p);
	printf("%s", cp);
}


/*
 * Print a Q.2931 message structure
 *
 * Arguments:
 *	msg	pointer to the message to print
 *
 * Returns:
 *	None
 *
 */
void
usp_print_msg(msg, dir)
	struct unisig_msg	*msg;
	int			dir;
{
	char			*name;
	int			i;
	struct ie_generic	*ie, *inxt;

	name = find_type(msg_types, msg->msg_type);
	switch (dir) {
	case UNISIG_MSG_IN:
		printf("Received ");
		break;
	case UNISIG_MSG_OUT:
		printf("Sent ");
		break;
	}
	printf("message: %s (%x)\n", name, msg->msg_type);
	printf("    Call reference:      0x%x\n", msg->msg_call_ref);
#ifdef LONG_PRINT
	printf("    Message type flag:   0x%x\n", msg->msg_type_flag);
	printf("    Message type action: 0x%x\n", msg->msg_type_action);
	printf("    Message length:      %d\n", msg->msg_length);
	for (i=0; i<UNI_MSG_IE_CNT; i++) {
		ie = msg->msg_ie_vec[i];
		while (ie) {
			inxt = ie->ie_next;
			usp_print_ie(ie);
			ie = inxt;
		}
	}
#else
	for (i=0; i<UNI_MSG_IE_CNT; i++)
	{
		ie = msg->msg_ie_vec[i];
		while (ie) {
			inxt = ie->ie_next;
			name = find_type(ie_types, ie->ie_ident);
			if (ie->ie_ident == UNI_IE_CAUS ||
					ie->ie_ident == UNI_IE_RSTI ||
					ie->ie_ident == UNI_IE_CLST) {
				usp_print_ie(ie);
			} else {
				printf("    Information element: %s (0x%x)\n",
						name, ie->ie_ident);
			}
			ie = inxt;
		}
	}
#endif
}


/*
 * Print a Q.2931 information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie(ie)
	struct ie_generic	*ie;
{
	char	*name;

	while (ie) {
		name = find_type(ie_types, ie->ie_ident);
		printf("    Information element: %s (0x%x)\n",
				name, ie->ie_ident);
#ifdef LONG_PRINT
		printf("        Coding:        0x%x\n",
				ie->ie_coding);
		printf("        Flag:          0x%x\n", ie->ie_flag);
		printf("        Action:	       0x%x\n",
				ie->ie_action);
		printf("        Length:	       %d\n", ie->ie_length);
#endif

		switch (ie->ie_ident) {
		case UNI_IE_AALP:
			usp_print_ie_aalp(ie);
			break;
		case UNI_IE_CLRT:
			usp_print_ie_clrt(ie);
			break;
		case UNI_IE_BBCP:
			usp_print_ie_bbcp(ie);
			break;
		case UNI_IE_BHLI:
			usp_print_ie_bhli(ie);
			break;
		case UNI_IE_BLLI:
			usp_print_ie_blli(ie);
			break;
		case UNI_IE_CLST:
			usp_print_ie_clst(ie);
			break;
		case UNI_IE_CDAD:
			usp_print_ie_cdad(ie);
			break;
		case UNI_IE_CDSA:
			usp_print_ie_cdsa(ie);
			break;
		case UNI_IE_CGAD:
			usp_print_ie_cgad(ie);
			break;
		case UNI_IE_CGSA:
			usp_print_ie_cgsa(ie);
			break;
		case UNI_IE_CAUS:
			usp_print_ie_caus(ie);
			break;
		case UNI_IE_CNID:
			usp_print_ie_cnid(ie);
			break;
		case UNI_IE_QOSP:
			usp_print_ie_qosp(ie);
			break;
		case UNI_IE_BRPI:
			usp_print_ie_brpi(ie);
			break;
		case UNI_IE_RSTI:
			usp_print_ie_rsti(ie);
			break;
		case UNI_IE_BLSH:
			usp_print_ie_blsh(ie);
			break;
		case UNI_IE_BNSH:
			usp_print_ie_bnsh(ie);
			break;
		case UNI_IE_BSDC:
			usp_print_ie_bsdc(ie);
			break;
		case UNI_IE_TRNT:
			usp_print_ie_trnt(ie);
			break;
		case UNI_IE_EPRF:
			usp_print_ie_eprf(ie);
			break;
		case UNI_IE_EPST:
			usp_print_ie_epst(ie);
			break;
		}
		ie = ie->ie_next;
	}
}


/*
 * Print an AAL parameters information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_aalp(ie)
	struct ie_generic	*ie;
{
	printf("        AAL type:      %d\n", ie->ie_aalp_aal_type);
	switch(ie->ie_aalp_aal_type) {
	case UNI_IE_AALP_AT_AAL1:
		printf("        Subtype:       0x%x\n",
				ie->ie_aalp_1_subtype);
		printf("        CBR rate:      0x%x\n",
				ie->ie_aalp_1_cbr_rate);
		printf("        Multiplier:    0x%x\n",
				ie->ie_aalp_1_multiplier);
		printf("        Clock rcvry:   0x%x\n",
				ie->ie_aalp_1_clock_recovery);
		printf("        Err corr:      0x%x\n",
				ie->ie_aalp_1_error_correction);
		printf("        Struct data:   0x%x\n",
				ie->ie_aalp_1_struct_data_tran);
		printf("        Partial cells: 0x%x\n",
				ie->ie_aalp_1_partial_cells);
		break;
	case UNI_IE_AALP_AT_AAL3:
		printf("        Fwd max SDU:   %d\n",
				ie->ie_aalp_4_fwd_max_sdu);
		printf("        Bkwd max SDU:  %d\n",
				ie->ie_aalp_4_bkwd_max_sdu);
		printf("        MID range:     %d\n",
				ie->ie_aalp_4_mid_range);
		printf("        Mode:          0x%x\n",
				ie->ie_aalp_4_mode);
		printf("        SSCS type:     0x%x\n",
				ie->ie_aalp_4_sscs_type);
		break;
	case UNI_IE_AALP_AT_AAL5:
		printf("        Fwd max SDU:   %d\n",
				ie->ie_aalp_5_fwd_max_sdu);
		printf("        Bkwd max SDU:  %d\n",
				ie->ie_aalp_5_bkwd_max_sdu);
		printf("        Mode:          0x%x\n",
				ie->ie_aalp_5_mode);
		printf("        SSCS type:     0x%x\n",
				ie->ie_aalp_5_sscs_type);
		break;
	case UNI_IE_AALP_AT_AALU:
		printf("        User info:     0x%x %x %x %x\n",
				ie->ie_aalp_user_info[0],
				ie->ie_aalp_user_info[1],
				ie->ie_aalp_user_info[2],
				ie->ie_aalp_user_info[3]);
		break;
	}
}


/*
 * Print a user cell rate information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_clrt(ie)
	struct ie_generic	*ie;
{
	printf("        Forward peak:  %d\n", ie->ie_clrt_fwd_peak);
	printf("        Backward peak: %d\n", ie->ie_clrt_bkwd_peak);
	printf("        Fwd peak 01:   %d\n", ie->ie_clrt_fwd_peak_01);
	printf("        Bkwd peak 01:  %d\n", ie->ie_clrt_bkwd_peak_01);
	printf("        Fwd sust:      %d\n", ie->ie_clrt_fwd_sust);
	printf("        Bkwd sust:     %d\n", ie->ie_clrt_bkwd_sust);
	printf("        Fwd sust 01:   %d\n", ie->ie_clrt_fwd_sust_01);
	printf("        Bkwd sust 01:  %d\n", ie->ie_clrt_bkwd_sust_01);
	printf("        Fwd burst:     %d\n", ie->ie_clrt_fwd_burst);
	printf("        Bkwd burst:    %d\n", ie->ie_clrt_bkwd_burst);
	printf("        Fwd burst 01:  %d\n", ie->ie_clrt_fwd_burst_01);
	printf("        Bkwd burst 01: %d\n",
			ie->ie_clrt_bkwd_burst_01);
	printf("        Best effort:   %d\n", ie->ie_clrt_best_effort);
	printf("        TM optons:     0x%x\n",
			ie->ie_clrt_tm_options);
}


/*
 * Print a broadband bearer capability information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_bbcp(ie)
	struct ie_generic	*ie;
{
	printf("        Bearer class:  0x%x\n",
			ie->ie_bbcp_bearer_class);
	printf("        Traffic type:  0x%x\n",
			ie->ie_bbcp_traffic_type);
	printf("        Timing req:    0x%x\n",
			ie->ie_bbcp_timing_req);
	printf("        Clipping:      0x%x\n", ie->ie_bbcp_clipping);
	printf("        Conn config:   0x%x\n",
			ie->ie_bbcp_conn_config);
}


/*
 * Print a broadband high layer information information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_bhli(ie)
	struct ie_generic	*ie;
{
	int	i;

	printf("        Type:          0x%x\n", ie->ie_bhli_type);
	printf("        HL info:       0x");
	for (i=0; i<ie->ie_length-1; i++) {
		printf("%x ", ie->ie_bhli_info[i]);
	}
	printf("\n");
}


/*
 * Print a broadband low-layer information information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_blli(ie)
	struct ie_generic	*ie;
{
	printf("        Layer 1 ID:    0x%x\n", ie->ie_blli_l1_id);
	printf("        Layer 2 ID:    0x%x\n", ie->ie_blli_l2_id);
	printf("        Layer 2 mode:  0x%x\n", ie->ie_blli_l2_mode);
	printf("        Layer 2 Q.933: 0x%x\n",
			ie->ie_blli_l2_q933_use);
	printf("        Layer 2 win:   0x%x\n",
			ie->ie_blli_l2_window);
	printf("        Layer 2 user:  0x%x\n",
			ie->ie_blli_l2_user_proto);
	printf("        Layer 3 ID:    0x%x\n", ie->ie_blli_l3_id);
	printf("        Layer 3 mode:  0x%x\n", ie->ie_blli_l3_mode);
	printf("        Layer 3 pkt:   0x%x\n",
			ie->ie_blli_l3_packet_size);
	printf("        Layer 3 win:   0x%x\n",
			ie->ie_blli_l3_window);
	printf("        Layer 3 user:  0x%x\n",
			ie->ie_blli_l3_user_proto);
	printf("        Layer 3 IPI:   0x%x\n", ie->ie_blli_l3_ipi);
	printf("        Layer 3 SNAP:  0x%x\n",
			ie->ie_blli_l3_snap_id);
	printf("        Layer 3 OUI:   0x%x %x %x\n",
			ie->ie_blli_l3_oui[0],
			ie->ie_blli_l3_oui[1],
			ie->ie_blli_l3_oui[2]);
	printf("        Layer 3 PID:   0x%x %x\n",
			ie->ie_blli_l3_pid[0],
			ie->ie_blli_l3_pid[1]);
}


/*
 * Print a call state information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_clst(ie)
	struct ie_generic	*ie;
{
	printf("        Call state:    %d\n",
			ie->ie_clst_state);
}


/*
 * Print a called party number information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_cdad(ie)
	struct ie_generic	*ie;
{
	printf("        ATM addr:      ");
	usp_print_atm_addr(&ie->ie_cdad_addr);
	printf("\n");
}


/*
 * Print a called party subaddress information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_cdsa(ie)
	struct ie_generic	*ie;
{
	printf("        ATM subaddr:   ");
	usp_print_atm_addr(&ie->ie_cdsa_addr);
	printf("\n");
}


/*
 * Print a calling party number information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_cgad(ie)
	struct ie_generic	*ie;
{
	printf("        ATM addr:      ");
	usp_print_atm_addr(&ie->ie_cgad_addr);
	printf("\n");
}


/*
 * Print a calling party subaddress information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_cgsa(ie)
	struct ie_generic	*ie;
{
	printf("        ATM subaddr:   ");
	usp_print_atm_addr(&ie->ie_cgsa_addr);
	printf("\n");
}


/*
 * Print a cause information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_caus(ie)
	struct ie_generic	*ie;
{
	int	i;

	printf("        Location:      %d\n", ie->ie_caus_loc);
	printf("        Cause:         %d\n", ie->ie_caus_cause);
	switch(ie->ie_caus_cause) {
	case UNI_IE_CAUS_IECONTENT:
		printf("        Flagged IEs:   ");
		for (i=0; ie->ie_caus_diagnostic[i]; i++) {
			printf("0x%x ", ie->ie_caus_diagnostic[i]);
		}
		printf("\n");
		break;
	case UNI_IE_CAUS_TIMER:
		printf("        Timer ID:      %c%c%c\n",
				ie->ie_caus_diagnostic[0],
				ie->ie_caus_diagnostic[1],
				ie->ie_caus_diagnostic[2]);
		break;
	default:
		printf("        Diag length:   %d\n",
				ie->ie_caus_diag_len);
		printf("        Diagnostic:    ");
		for (i=0; i<ie->ie_caus_diag_len; i++) {
			printf("0x%x ", ie->ie_caus_diagnostic[i]);
		}
		printf("\n");
	}
}


/*
 * Print a connection identifier information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_cnid(ie)
	struct ie_generic	*ie;
{
	printf("        VP assoc sig:  0x%x\n", ie->ie_cnid_vp_sig);
	printf("        Pref/excl:     0x%x\n",
			ie->ie_cnid_pref_excl);
	printf("        VPCI:          %d\n", ie->ie_cnid_vpci);
	printf("        VCI:           %d\n", ie->ie_cnid_vci);
}


/*
 * Print a quality of service parameter information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_qosp(ie)
	struct ie_generic	*ie;
{
	printf("        QoS fwd:       0x%x\n",
			ie->ie_qosp_fwd_class);
	printf("        QoS bkwd:      0x%x\n",
			ie->ie_qosp_bkwd_class);
}


/*
 * Print a broadband repeat indicator information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_brpi(ie)
	struct ie_generic	*ie;
{
	printf("        Indicator:     0x%x\n", ie->ie_brpi_ind);
}


/*
 * Print a restart indicator information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_rsti(ie)
	struct ie_generic	*ie;
{
	printf("        Class:         0x%x\n", ie->ie_rsti_class);
}


/*
 * Print a broadband locking shift information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_blsh(ie)
	struct ie_generic	*ie;
{
}


/*
 * Print a broadband non-locking shift information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_bnsh(ie)
	struct ie_generic	*ie;
{
}


/*
 * Print a broadband sending complete information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_bsdc(ie)
	struct ie_generic	*ie;
{
	printf("        Indication:    0x%x\n", ie->ie_bsdc_ind);
}


/*
 * Print a transit net selection information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_trnt(ie)
	struct ie_generic	*ie;
{
#ifdef NOTDEF
	struct ie_generic	ie_trnt_hdr;
	u_char		ie_trnt_id_type;
	u_char		ie_trnt_id_plan;
	Atm_addr	ie_trnt_id;
#endif
}


/*
 * Print an endpoint reference information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_eprf(ie)
	struct ie_generic	*ie;
{
	printf("        Ref type:      0x%x\n",
			ie->ie_eprf_type);
	printf("        Endpt ref:     0x%x\n",
			ie->ie_eprf_id);
}


/*
 * Print an endpoint state information element
 *
 * Arguments:
 *	ie	pointer to the IE to print
 *
 * Returns:
 *	None
 *
 */
static void
usp_print_ie_epst(ie)
	struct ie_generic	*ie;
{
	printf("        Endpt state:   %d\n",
			ie->ie_epst_state);
}
