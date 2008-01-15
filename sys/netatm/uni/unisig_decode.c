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
__FBSDID("$FreeBSD: src/sys/netatm/uni/unisig_decode.c,v 1.17 2005/01/07 01:45:37 imp Exp $");

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
#include <netatm/uni/unisig_mbuf.h>
#include <netatm/uni/unisig_decode.h>

#define	ALLOC_IE(ie) do {						\
	(ie) = uma_zalloc(unisig_ie_zone, M_NOWAIT | M_ZERO);		\
	if ((ie) == NULL)						\
		return (ENOMEM);					\
} while (0)

/*
 * Local functions
 */
static int	usf_dec_ie(struct usfmt *, struct unisig_msg *, struct ie_generic *);
static int	usf_dec_ie_hdr(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_aalp(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_clrt(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_bbcp(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_bhli(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_blli(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_clst(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_cdad(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_cdsa(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_cgad(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_cgsa(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_caus(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_cnid(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_qosp(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_brpi(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_rsti(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_bsdc(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_trnt(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_uimp(struct usfmt *, struct ie_generic *);
static int	usf_dec_ie_ident(struct usfmt *, struct ie_generic *,
			struct ie_decode_tbl *);
static int	usf_dec_atm_addr(struct usfmt *, Atm_addr *, int);


/*
 * Table associating IE type with IE vector index
 */
u_char unisig_ie_ident_vec[] = {
	UNI_IE_AALP,
	UNI_IE_CLRT,
	UNI_IE_BBCP,
	UNI_IE_BHLI,
	UNI_IE_BLLI,
	UNI_IE_CLST,
	UNI_IE_CDAD,
	UNI_IE_CDSA,
	UNI_IE_CGAD,
	UNI_IE_CGSA,
	UNI_IE_CAUS,
	UNI_IE_CNID,
	UNI_IE_QOSP,
	UNI_IE_BRPI,
	UNI_IE_RSTI,
	UNI_IE_BLSH,
	UNI_IE_BNSH,
	UNI_IE_BSDC,
	UNI_IE_TRNT,
	UNI_IE_EPRF,
	UNI_IE_EPST
};


/*
 * Tables specifying which IEs are mandatory, optional, and
 * not allowed for each Q.2931 message type
 */
static u_char uni_calp_ie_tbl[] = {
	IE_NA,		/* ATM AAL Parameters */
	IE_NA,		/* ATM User Cell Rate */
	IE_NA,		/* Broadband Bearer Capability */
	IE_NA,		/* Broadband High Layer Information */
	IE_NA,		/* Broadband Low Layer Information */
	IE_NA,		/* Call State */
	IE_NA,		/* Called Party Number */
	IE_NA,		/* Called Party Subaddress */
	IE_NA,		/* Calling Party Number */
	IE_NA,		/* Calling Party Subaddress */
	IE_NA,		/* Cause */
	IE_OPT,		/* Connection Identifier */
	IE_NA,		/* Quality of Service Parameters */
	IE_NA,		/* Broadband Repeat Indicator */
	IE_NA,		/* Restart Indicator */
	IE_NA,		/* Broadband Locking Shift */
	IE_NA,		/* Broadband Non-locking Shift */
	IE_NA,		/* Broadband Sending Complete */
	IE_NA,		/* Transit Net */
	IE_OPT,		/* Endpoint Reference */
	IE_NA		/* Endpoint State */
};

static u_char uni_conn_ie_tbl[] = {
	IE_OPT,		/* ATM AAL Parameters */
	IE_NA,		/* ATM User Cell Rate */
	IE_NA,		/* Broadband Bearer Capability */
	IE_NA,		/* Broadband High Layer Information */
	IE_OPT,		/* Broadband Low Layer Information */
	IE_NA,		/* Call State */
	IE_NA,		/* Called Party Number */
	IE_NA,		/* Called Party Subaddress */
	IE_NA,		/* Calling Party Number */
	IE_NA,		/* Calling Party Subaddress */
	IE_NA,		/* Cause */
	IE_OPT,		/* Connection Identifier */
	IE_NA,		/* Quality of Service Parameters */
	IE_NA,		/* Broadband Repeat Indicator */
	IE_NA,		/* Restart Indicator */
	IE_NA,		/* Broadband Locking Shift */
	IE_NA,		/* Broadband Non-locking Shift */
	IE_NA,		/* Broadband Sending Complete */
	IE_NA,		/* Transit Net */
	IE_OPT,		/* Endpoint Reference */
	IE_NA		/* Endpoint State */
};

static u_char uni_cack_ie_tbl[] = {
	IE_NA,		/* ATM AAL Parameters */
	IE_NA,		/* ATM User Cell Rate */
	IE_NA,		/* Broadband Bearer Capability */
	IE_NA,		/* Broadband High Layer Information */
	IE_NA,		/* Broadband Low Layer Information */
	IE_NA,		/* Call State */
	IE_NA,		/* Called Party Number */
	IE_NA,		/* Called Party Subaddress */
	IE_NA,		/* Calling Party Number */
	IE_NA,		/* Calling Party Subaddress */
	IE_NA,		/* Cause */
	IE_NA,		/* Connection Identifier */
	IE_NA,		/* Quality of Service Parameters */
	IE_NA,		/* Broadband Repeat Indicator */
	IE_NA,		/* Restart Indicator */
	IE_NA,		/* Broadband Locking Shift */
	IE_NA,		/* Broadband Non-locking Shift */
	IE_NA,		/* Broadband Sending Complete */
	IE_NA,		/* Transit Net */
	IE_NA,		/* Endpoint Reference */
	IE_NA		/* Endpoint State */
};

static u_char uni_setu_ie_tbl[] = {
	IE_MAND,	/* ATM AAL Parameters (not required by
			   UNI 3.0) */
	IE_MAND,	/* ATM User Cell Rate */
	IE_MAND,	/* Broadband Bearer Capability */
	IE_OPT,		/* Broadband High Layer Information */
	IE_MAND,	/* Broadband Low Layer Information (not required				   by UNI 3.0 */
	IE_NA,		/* Call State */
	IE_MAND,	/* Called Party Number */
	IE_OPT,		/* Called Party Subaddress */
	IE_OPT,		/* Calling Party Number */
	IE_OPT,		/* Calling Party Subaddress */
	IE_NA,		/* Cause */
	IE_MAND,	/* Connection Identifier */
	IE_MAND,	/* Quality of Service Parameters */
	IE_OPT,		/* Broadband Repeat Indicator */
	IE_NA,		/* Restart Indicator */
	IE_NA,		/* Broadband Locking Shift */
	IE_NA,		/* Broadband Non-locking Shift */
	IE_OPT,		/* Broadband Sending Complete */
	IE_NA,		/* Transit Net */
	IE_OPT,		/* Endpoint Reference */
	IE_NA		/* Endpoint State */
};

static u_char uni_rlse_ie_tbl[] = {
	IE_NA,		/* ATM AAL Parameters */
	IE_NA,		/* ATM User Cell Rate */
	IE_NA,		/* Broadband Bearer Capability */
	IE_NA,		/* Broadband High Layer Information */
	IE_NA,		/* Broadband Low Layer Information */
	IE_NA,		/* Call State */
	IE_NA,		/* Called Party Number */
	IE_NA,		/* Called Party Subaddress */
	IE_NA,		/* Calling Party Number */
	IE_NA,		/* Calling Party Subaddress */
	IE_MAND,	/* Cause */
	IE_NA,		/* Connection Identifier */
	IE_NA,		/* Quality of Service Parameters */
	IE_NA,		/* Broadband Repeat Indicator */
	IE_NA,		/* Restart Indicator */
	IE_NA,		/* Broadband Locking Shift */
	IE_NA,		/* Broadband Non-locking Shift */
	IE_NA,		/* Broadband Sending Complete */
	IE_NA,		/* Transit Net */
	IE_NA,		/* Endpoint Reference */
	IE_NA		/* Endpoint State */
};

static u_char uni_rlsc_ie_tbl[] = {
	IE_NA,		/* ATM AAL Parameters */
	IE_NA,		/* ATM User Cell Rate */
	IE_NA,		/* Broadband Bearer Capability */
	IE_NA,		/* Broadband High Layer Information */
	IE_NA,		/* Broadband Low Layer Information */
	IE_NA,		/* Call State */
	IE_NA,		/* Called Party Number */
	IE_NA,		/* Called Party Subaddress */
	IE_NA,		/* Calling Party Number */
	IE_NA,		/* Calling Party Subaddress */
	IE_MAND,	/* Cause */
	IE_NA,		/* Connection Identifier */
	IE_NA,		/* Quality of Service Parameters */
	IE_NA,		/* Broadband Repeat Indicator */
	IE_NA,		/* Restart Indicator */
	IE_NA,		/* Broadband Locking Shift */
	IE_NA,		/* Broadband Non-locking Shift */
	IE_NA,		/* Broadband Sending Complete */
	IE_NA,		/* Transit Net */
	IE_NA,		/* Endpoint Reference */
	IE_NA		/* Endpoint State */
};

static u_char uni_rstr_ie_tbl[] = {
	IE_NA,		/* ATM AAL Parameters */
	IE_NA,		/* ATM User Cell Rate */
	IE_NA,		/* Broadband Bearer Capability */
	IE_NA,		/* Broadband High Layer Information */
	IE_NA,		/* Broadband Low Layer Information */
	IE_NA,		/* Call State */
	IE_NA,		/* Called Party Number */
	IE_NA,		/* Called Party Subaddress */
	IE_NA,		/* Calling Party Number */
	IE_NA,		/* Calling Party Subaddress */
	IE_NA,		/* Cause */
	IE_OPT,		/* Connection Identifier */
	IE_NA,		/* Quality of Service Parameters */
	IE_NA,		/* Broadband Repeat Indicator */
	IE_MAND,	/* Restart Indicator */
	IE_NA,		/* Broadband Locking Shift */
	IE_NA,		/* Broadband Non-locking Shift */
	IE_NA,		/* Broadband Sending Complete */
	IE_NA,		/* Transit Net */
	IE_NA,		/* Endpoint Reference */
	IE_NA		/* Endpoint State */
};

static u_char uni_rsta_ie_tbl[] = {
	IE_NA,		/* ATM AAL Parameters */
	IE_NA,		/* ATM User Cell Rate */
	IE_NA,		/* Broadband Bearer Capability */
	IE_NA,		/* Broadband High Layer Information */
	IE_NA,		/* Broadband Low Layer Information */
	IE_NA,		/* Call State */
	IE_NA,		/* Called Party Number */
	IE_NA,		/* Called Party Subaddress */
	IE_NA,		/* Calling Party Number */
	IE_NA,		/* Calling Party Subaddress */
	IE_NA,		/* Cause */
	IE_OPT,		/* Connection Identifier */
	IE_NA,		/* Quality of Service Parameters */
	IE_NA,		/* Broadband Repeat Indicator */
	IE_MAND,	/* Restart Indicator */
	IE_NA,		/* Broadband Locking Shift */
	IE_NA,		/* Broadband Non-locking Shift */
	IE_NA,		/* Broadband Sending Complete */
	IE_NA,		/* Transit Net */
	IE_NA,		/* Endpoint Reference */
	IE_NA		/* Endpoint State */
};

static u_char uni_stat_ie_tbl[] = {
	IE_NA,		/* ATM AAL Parameters */
	IE_NA,		/* ATM User Cell Rate */
	IE_NA,		/* Broadband Bearer Capability */
	IE_NA,		/* Broadband High Layer Information */
	IE_NA,		/* Broadband Low Layer Information */
	IE_MAND,	/* Call State */
	IE_NA,		/* Called Party Number */
	IE_NA,		/* Called Party Subaddress */
	IE_NA,		/* Calling Party Number */
	IE_NA,		/* Calling Party Subaddress */
	IE_MAND,	/* Cause */
	IE_NA,		/* Connection Identifier */
	IE_NA,		/* Quality of Service Parameters */
	IE_NA,		/* Broadband Repeat Indicator */
	IE_NA,		/* Restart Indicator */
	IE_NA,		/* Broadband Locking Shift */
	IE_NA,		/* Broadband Non-locking Shift */
	IE_NA,		/* Broadband Sending Complete */
	IE_NA,		/* Transit Net */
	IE_OPT,		/* Endpoint Reference */
	IE_OPT		/* Endpoint State */
};

static u_char uni_senq_ie_tbl[] = {
	IE_NA,		/* ATM AAL Parameters */
	IE_NA,		/* ATM User Cell Rate */
	IE_NA,		/* Broadband Bearer Capability */
	IE_NA,		/* Broadband High Layer Information */
	IE_NA,		/* Broadband Low Layer Information */
	IE_NA,		/* Call State */
	IE_NA,		/* Called Party Number */
	IE_NA,		/* Called Party Subaddress */
	IE_NA,		/* Calling Party Number */
	IE_NA,		/* Calling Party Subaddress */
	IE_NA,		/* Cause */
	IE_NA,		/* Connection Identifier */
	IE_NA,		/* Quality of Service Parameters */
	IE_NA,		/* Broadband Repeat Indicator */
	IE_NA,		/* Restart Indicator */
	IE_NA,		/* Broadband Locking Shift */
	IE_NA,		/* Broadband Non-locking Shift */
	IE_NA,		/* Broadband Sending Complete */
	IE_NA,		/* Transit Net */
	IE_OPT,		/* Endpoint Reference */
	IE_NA		/* Endpoint State */
};

static u_char uni_addp_ie_tbl[] = {
	IE_OPT,		/* ATM AAL Parameters */
	IE_NA,		/* ATM User Cell Rate */
	IE_NA,		/* Broadband Bearer Capability */
	IE_OPT,		/* Broadband High Layer Information */
	IE_OPT,		/* Broadband Low Layer Information */
	IE_NA,		/* Call State */
	IE_MAND,	/* Called Party Number */
	IE_OPT,		/* Called Party Subaddress */
	IE_OPT,		/* Calling Party Number */
	IE_OPT,		/* Calling Party Subaddress */
	IE_NA,		/* Cause */
	IE_NA,		/* Connection Identifier */
	IE_NA,		/* Quality of Service Parameters */
	IE_NA,		/* Broadband Repeat Indicator */
	IE_NA,		/* Restart Indicator */
	IE_NA,		/* Broadband Locking Shift */
	IE_NA,		/* Broadband Non-locking Shift */
	IE_OPT,		/* Broadband Sending Complete */
	IE_NA,		/* Transit Net */
	IE_MAND,	/* Endpoint Reference */
	IE_NA		/* Endpoint State */
};

static u_char uni_adpa_ie_tbl[] = {
	IE_NA,		/* ATM AAL Parameters */
	IE_NA,		/* ATM User Cell Rate */
	IE_NA,		/* Broadband Bearer Capability */
	IE_NA,		/* Broadband High Layer Information */
	IE_NA,		/* Broadband Low Layer Information */
	IE_NA,		/* Call State */
	IE_NA,		/* Called Party Number */
	IE_NA,		/* Called Party Subaddress */
	IE_NA,		/* Calling Party Number */
	IE_NA,		/* Calling Party Subaddress */
	IE_NA,		/* Cause */
	IE_NA,		/* Connection Identifier */
	IE_NA,		/* Quality of Service Parameters */
	IE_NA,		/* Broadband Repeat Indicator */
	IE_NA,		/* Restart Indicator */
	IE_NA,		/* Broadband Locking Shift */
	IE_NA,		/* Broadband Non-locking Shift */
	IE_NA,		/* Broadband Sending Complete */
	IE_NA,		/* Transit Net */
	IE_MAND,	/* Endpoint Reference */
	IE_NA		/* Endpoint State */
};

static u_char uni_adpr_ie_tbl[] = {
	IE_NA,		/* ATM AAL Parameters */
	IE_NA,		/* ATM User Cell Rate */
	IE_NA,		/* Broadband Bearer Capability */
	IE_NA,		/* Broadband High Layer Information */
	IE_NA,		/* Broadband Low Layer Information */
	IE_NA,		/* Call State */
	IE_NA,		/* Called Party Number */
	IE_NA,		/* Called Party Subaddress */
	IE_NA,		/* Calling Party Number */
	IE_NA,		/* Calling Party Subaddress */
	IE_MAND,	/* Cause */
	IE_NA,		/* Connection Identifier */
	IE_NA,		/* Quality of Service Parameters */
	IE_NA,		/* Broadband Repeat Indicator */
	IE_NA,		/* Restart Indicator */
	IE_NA,		/* Broadband Locking Shift */
	IE_NA,		/* Broadband Non-locking Shift */
	IE_NA,		/* Broadband Sending Complete */
	IE_NA,		/* Transit Net */
	IE_MAND,	/* Endpoint Reference */
	IE_NA		/* Endpoint State */
};

static u_char uni_drpp_ie_tbl[] = {
	IE_NA,		/* ATM AAL Parameters */
	IE_NA,		/* ATM User Cell Rate */
	IE_NA,		/* Broadband Bearer Capability */
	IE_NA,		/* Broadband High Layer Information */
	IE_NA,		/* Broadband Low Layer Information */
	IE_NA,		/* Call State */
	IE_NA,		/* Called Party Number */
	IE_NA,		/* Called Party Subaddress */
	IE_NA,		/* Calling Party Number */
	IE_NA,		/* Calling Party Subaddress */
	IE_MAND,	/* Cause */
	IE_NA,		/* Connection Identifier */
	IE_NA,		/* Quality of Service Parameters */
	IE_NA,		/* Broadband Repeat Indicator */
	IE_NA,		/* Restart Indicator */
	IE_NA,		/* Broadband Locking Shift */
	IE_NA,		/* Broadband Non-locking Shift */
	IE_NA,		/* Broadband Sending Complete */
	IE_NA,		/* Transit Net */
	IE_MAND,	/* Endpoint Reference */
	IE_NA		/* Endpoint State */
};

static u_char uni_drpa_ie_tbl[] = {
	IE_NA,		/* ATM AAL Parameters */
	IE_NA,		/* ATM User Cell Rate */
	IE_NA,		/* Broadband Bearer Capability */
	IE_NA,		/* Broadband High Layer Information */
	IE_NA,		/* Broadband Low Layer Information */
	IE_NA,		/* Call State */
	IE_NA,		/* Called Party Number */
	IE_NA,		/* Called Party Subaddress */
	IE_NA,		/* Calling Party Number */
	IE_NA,		/* Calling Party Subaddress */
	IE_OPT,		/* Cause */
	IE_NA,		/* Connection Identifier */
	IE_NA,		/* Quality of Service Parameters */
	IE_NA,		/* Broadband Repeat Indicator */
	IE_NA,		/* Restart Indicator */
	IE_NA,		/* Broadband Locking Shift */
	IE_NA,		/* Broadband Non-locking Shift */
	IE_NA,		/* Broadband Sending Complete */
	IE_NA,		/* Transit Net */
	IE_MAND,	/* Endpoint Reference */
	IE_NA		/* Endpoint State */
};

/*
 * Table of Q.2931 message types
 */
static struct {
	u_char	msg_type;
	u_char	*msg_ie_tbl;
} uni_msg_types[] = {
	{ UNI_MSG_CALP, uni_calp_ie_tbl },
	{ UNI_MSG_CONN, uni_conn_ie_tbl },
	{ UNI_MSG_CACK, uni_cack_ie_tbl },
	{ UNI_MSG_SETU, uni_setu_ie_tbl },
	{ UNI_MSG_RLSE, uni_rlse_ie_tbl },
	{ UNI_MSG_RLSC, uni_rlsc_ie_tbl },
	{ UNI_MSG_RSTR, uni_rstr_ie_tbl },
	{ UNI_MSG_RSTA, uni_rsta_ie_tbl },
	{ UNI_MSG_STAT, uni_stat_ie_tbl },
	{ UNI_MSG_SENQ, uni_senq_ie_tbl },
	{ UNI_MSG_ADDP, uni_addp_ie_tbl },
	{ UNI_MSG_ADPA, uni_adpa_ie_tbl },
	{ UNI_MSG_ADPR, uni_adpr_ie_tbl },
	{ UNI_MSG_DRPP, uni_drpp_ie_tbl },
	{ UNI_MSG_DRPA, uni_drpa_ie_tbl },
};


/*
 * Table of information elements
 */
static struct ie_ent	ie_table[] = {
	{ UNI_IE_AALP, 5, 16, UNI_MSG_IE_AALP, usf_dec_ie_aalp },
	{ UNI_IE_CLRT, 0, 26, UNI_MSG_IE_CLRT, usf_dec_ie_clrt },
	{ UNI_IE_BBCP, 2,  3, UNI_MSG_IE_BBCP, usf_dec_ie_bbcp },
	{ UNI_IE_BHLI, 1,  9, UNI_MSG_IE_BHLI, usf_dec_ie_bhli },
	{ UNI_IE_BLLI, 0, 13, UNI_MSG_IE_BLLI, usf_dec_ie_blli },
	{ UNI_IE_CLST, 1,  1, UNI_MSG_IE_CLST, usf_dec_ie_clst },
	{ UNI_IE_CDAD, 1, 21, UNI_MSG_IE_CDAD, usf_dec_ie_cdad },
	{ UNI_IE_CDSA, 1, 21, UNI_MSG_IE_CDSA, usf_dec_ie_cdsa },
	{ UNI_IE_CGAD, 1, 22, UNI_MSG_IE_CGAD, usf_dec_ie_cgad },
	{ UNI_IE_CGSA, 1, 21, UNI_MSG_IE_CGSA, usf_dec_ie_cgsa },
	{ UNI_IE_CAUS, 2, 30, UNI_MSG_IE_CAUS, usf_dec_ie_caus },
	{ UNI_IE_CNID, 5,  5, UNI_MSG_IE_CNID, usf_dec_ie_cnid },
	{ UNI_IE_QOSP, 2,  2, UNI_MSG_IE_QOSP, usf_dec_ie_qosp },
	{ UNI_IE_BRPI, 1,  1, UNI_MSG_IE_BRPI, usf_dec_ie_brpi },
	{ UNI_IE_RSTI, 1,  1, UNI_MSG_IE_RSTI, usf_dec_ie_rsti },
	{ UNI_IE_BLSH, 1,  1, UNI_MSG_IE_ERR,  usf_dec_ie_uimp },
	{ UNI_IE_BNSH, 1,  1, UNI_MSG_IE_ERR,  usf_dec_ie_uimp },
	{ UNI_IE_BSDC, 1,  1, UNI_MSG_IE_BSDC, usf_dec_ie_bsdc },
	{ UNI_IE_TRNT, 1,  5, UNI_MSG_IE_TRNT, usf_dec_ie_trnt },
	{ UNI_IE_EPRF, 3,  3, UNI_MSG_IE_ERR,  usf_dec_ie_uimp },
	{ UNI_IE_EPST, 1,  1, UNI_MSG_IE_ERR,  usf_dec_ie_uimp },
	{ 0,           0,  0, 0,               0 }
};

/*
 * Decoding table for AAL 1
 */
struct ie_decode_tbl	ie_aal1_tbl[] = {
	{ 133, 1, IE_OFF_SIZE(ie_aalp_1_subtype) },
	{ 134, 1, IE_OFF_SIZE(ie_aalp_1_cbr_rate) },
	{ 135, 2, IE_OFF_SIZE(ie_aalp_1_multiplier) },
	{ 136, 1, IE_OFF_SIZE(ie_aalp_1_clock_recovery) },
	{ 137, 1, IE_OFF_SIZE(ie_aalp_1_error_correction) },
	{ 138, 1, IE_OFF_SIZE(ie_aalp_1_struct_data_tran) },
	{ 139, 1, IE_OFF_SIZE(ie_aalp_1_partial_cells) },
	{   0, 0, 0, 0 }
};

/*
 * Decoding table for AAL 3/4
 */
struct ie_decode_tbl	ie_aal4_tbl_30[] = {
	{ 140, 2, IE_OFF_SIZE(ie_aalp_4_fwd_max_sdu) },
	{ 129, 2, IE_OFF_SIZE(ie_aalp_4_bkwd_max_sdu) },
	{ 130, 2, IE_OFF_SIZE(ie_aalp_4_mid_range) },
	{ 131, 1, IE_OFF_SIZE(ie_aalp_4_mode) },
	{ 132, 1, IE_OFF_SIZE(ie_aalp_4_sscs_type) },
	{   0, 0, 0, 0 }
};
struct ie_decode_tbl	ie_aal4_tbl_31[] = {
	{ 140, 2, IE_OFF_SIZE(ie_aalp_4_fwd_max_sdu) },
	{ 129, 2, IE_OFF_SIZE(ie_aalp_4_bkwd_max_sdu) },
	{ 130, 4, IE_OFF_SIZE(ie_aalp_4_mid_range) },
	{ 132, 1, IE_OFF_SIZE(ie_aalp_4_sscs_type) },
	{   0, 0, 0, 0 }
};

/*
 * Decoding table for AAL 5
 */
struct ie_decode_tbl	ie_aal5_tbl_30[] = {
	{ 140, 2, IE_OFF_SIZE(ie_aalp_5_fwd_max_sdu) },
	{ 129, 2, IE_OFF_SIZE(ie_aalp_5_bkwd_max_sdu) },
	{ 131, 1, IE_OFF_SIZE(ie_aalp_5_mode) },
	{ 132, 1, IE_OFF_SIZE(ie_aalp_5_sscs_type) },
	{   0, 0, 0, 0 }
};
struct ie_decode_tbl	ie_aal5_tbl_31[] = {
	{ 140, 2, IE_OFF_SIZE(ie_aalp_5_fwd_max_sdu) },
	{ 129, 2, IE_OFF_SIZE(ie_aalp_5_bkwd_max_sdu) },
	{ 132, 1, IE_OFF_SIZE(ie_aalp_5_sscs_type) },
	{   0, 0, 0, 0 }
};

/*
 * Decoding table for ATM user cell rate
 */
struct ie_decode_tbl	ie_clrt_tbl[] = {
    {UNI_IE_CLRT_FWD_PEAK_ID,      3, IE_OFF_SIZE(ie_clrt_fwd_peak)},
    {UNI_IE_CLRT_BKWD_PEAK_ID,     3, IE_OFF_SIZE(ie_clrt_bkwd_peak)},
    {UNI_IE_CLRT_FWD_PEAK_01_ID,   3, IE_OFF_SIZE(ie_clrt_fwd_peak_01)},
    {UNI_IE_CLRT_BKWD_PEAK_01_ID,  3, IE_OFF_SIZE(ie_clrt_bkwd_peak_01)},
    {UNI_IE_CLRT_FWD_SUST_ID,      3, IE_OFF_SIZE(ie_clrt_fwd_sust)},
    {UNI_IE_CLRT_BKWD_SUST_ID,     3, IE_OFF_SIZE(ie_clrt_bkwd_sust)},
    {UNI_IE_CLRT_FWD_SUST_01_ID,   3, IE_OFF_SIZE(ie_clrt_fwd_sust_01)},
    {UNI_IE_CLRT_BKWD_SUST_01_ID,  3, IE_OFF_SIZE(ie_clrt_bkwd_sust_01)},
    {UNI_IE_CLRT_FWD_BURST_ID,     3, IE_OFF_SIZE(ie_clrt_fwd_burst)},
    {UNI_IE_CLRT_BKWD_BURST_ID,    3, IE_OFF_SIZE(ie_clrt_bkwd_burst)},
    {UNI_IE_CLRT_FWD_BURST_01_ID,  3, IE_OFF_SIZE(ie_clrt_fwd_burst_01)},
    {UNI_IE_CLRT_BKWD_BURST_01_ID, 3, IE_OFF_SIZE(ie_clrt_bkwd_burst_01)},
    {UNI_IE_CLRT_BEST_EFFORT_ID,   0, IE_OFF_SIZE(ie_clrt_best_effort)},
    {UNI_IE_CLRT_TM_OPTIONS_ID,    1, IE_OFF_SIZE(ie_clrt_tm_options)},
    {0,                            0, 0, 0 }
};

/*
 * IEs initialized to empty values
 */
struct ie_aalp	ie_aalp_absent = {
	T_ATM_ABSENT
};

struct ie_clrt	ie_clrt_absent = {
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT
};

struct ie_bbcp	ie_bbcp_absent = {
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT
};

struct ie_bhli	ie_bhli_absent = {
	T_ATM_ABSENT,
	{ 0, 0, 0, 0, 0, 0, 0, 0 }
};

struct ie_blli	ie_blli_absent = {
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	{ 0, 0, 0 },
	{ 0, 0 }
};

struct ie_clst	ie_clst_absent = {
	T_ATM_ABSENT
};

struct ie_cdad	ie_cdad_absent = {
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	{ T_ATM_ABSENT, 0 }
};

struct ie_cdsa	ie_cdsa_absent = {
	{ T_ATM_ABSENT, 0 }
};

struct ie_cgad	ie_cgad_absent = {
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	{ T_ATM_ABSENT, 0 }
};

struct ie_cgsa	ie_cgsa_absent = {
	{ T_ATM_ABSENT, 0 }
};

struct ie_caus	ie_caus_absent = {
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	0
};

struct ie_cnid	ie_cnid_absent = {
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	T_ATM_ABSENT
};

struct ie_qosp	ie_qosp_absent = {
	T_ATM_ABSENT,
	T_ATM_ABSENT
};

struct ie_brpi	ie_brpi_absent = {
	T_ATM_ABSENT
};

struct ie_rsti	ie_rsti_absent = {
	T_ATM_ABSENT
};

struct ie_blsh	ie_blsh_absent = {
	T_ATM_ABSENT
};

struct ie_bnsh	ie_bnsh_absent = {
	T_ATM_ABSENT
};

struct ie_bsdc	ie_bsdc_absent = {
	T_ATM_ABSENT
};

struct ie_trnt	ie_trnt_absent = {
	T_ATM_ABSENT,
	T_ATM_ABSENT,
	0
};

struct ie_eprf	ie_eprf_absent = {
	T_ATM_ABSENT,
	T_ATM_ABSENT
};

struct ie_epst	ie_epst_absent = {
	T_ATM_ABSENT
};


/*
 * Decode a UNI signalling message
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
usf_dec_msg(usf, msg)
	struct usfmt		*usf;
	struct unisig_msg	*msg;
{
	int			i, len, rc;
	short			s;
	u_char			c, *ie_tbl;
	struct ie_generic	*ie;

	ATM_DEBUG2("usf_dec_msg: usf=%p, msg=%p\n", usf, msg);

	/*
	 * Check the total message length
	 */
	if (usf_count(usf) < UNI_MSG_MIN_LEN) {
		return(EIO);
	}

	/*
	 * Get and check the protocol discriminator
	 */
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	if (c != UNI_MSG_DISC_Q93B)
		return(EIO);

	/*
	 * Get and check the call reference length
	 */
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	if (c != 3)
		return(EIO);

	/*
	 * Get the call reference
	 */
	rc = usf_int3(usf, &msg->msg_call_ref);
	if (rc)
		return(rc);

	/*
	 * Get the message type
	 */
	rc = usf_byte(usf, &msg->msg_type);
	if (rc)
		return(rc);

	/*
	 * Get the message type extension
	 */
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	msg->msg_type_flag = (c >> UNI_MSG_TYPE_FLAG_SHIFT) &
			UNI_MSG_TYPE_FLAG_MASK;
	msg->msg_type_action = c & UNI_MSG_TYPE_ACT_MASK;

	/*
	 * Get the message length and make sure we actually have
	 * enough data for the whole message
	 */
	rc = usf_short(usf, &s);
	if (rc)
		return(rc);
	msg->msg_length = s;
	if (usf_count(usf) != msg->msg_length) {
		return(EMSGSIZE);
	}

	/*
	 * Process information elements
	 */
	len = msg->msg_length;
	while (len) {
		ALLOC_IE(ie);
		rc = usf_dec_ie(usf, msg, ie);
		if (rc) {
			uma_zfree(unisig_ie_zone, ie);
			return(rc);
		}
		len -= (ie->ie_length + UNI_IE_HDR_LEN);
	}

	/*
	 * Make sure that mandatory IEs are included and
	 * unwanted ones aren't
	 */
	for (i=0; msg->msg_type!=uni_msg_types[i].msg_type &&
			uni_msg_types[i].msg_type!=0; i++) {
	}
	if (!uni_msg_types[i].msg_ie_tbl)
		goto done;

	/*
	 * If the message type is in the table, check the IEs.
	 * If it isn't, the receive routine will catch the error.
	 */
	ie_tbl = uni_msg_types[i].msg_ie_tbl;
	for (i=0; i<UNI_MSG_IE_CNT-1; i++) {
		switch(ie_tbl[i]) {
		case IE_MAND:
			if (!msg->msg_ie_vec[i]) {
				/*
				 * Mandatory IE missing
				 */
				ALLOC_IE(ie);
				ie->ie_ident = unisig_ie_ident_vec[i];
				ie->ie_err_cause = UNI_IE_CAUS_MISSING;
				MSG_IE_ADD(msg, ie, UNI_MSG_IE_ERR);
			}
			break;
		case IE_NA:
			if (msg->msg_ie_vec[i]) {
				/*
				 * Disallowed IE present
				 */
				ie = msg->msg_ie_vec[i];
				msg->msg_ie_vec[i] =
						(struct ie_generic *) 0;
				MSG_IE_ADD(msg, ie, UNI_MSG_IE_ERR);
				while (ie) {
					ie->ie_err_cause =
						UNI_IE_CAUS_IEEXIST;
					ie = ie->ie_next;
				}
			}
			break;
		case IE_OPT:
			break;
		}
	}

done:
	return(0);
}


/*
 * Decode an information element
 *
 * This routine will be called repeatedly as long as there are
 * information elements left to be decoded.  It will decode the
 * first part of the IE, look its type up in a table, and call
 * the appropriate routine to decode the rest.  After an IE is
 * successfully decoded, it is linked into the UNI signalling
 * message structure.  If an error is discovered, the IE is linked
 * into the IE error chain and an error cause is set in the header.
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
usf_dec_ie(usf, msg, ie)
	struct usfmt			*usf;
	struct unisig_msg		*msg;
	struct ie_generic		*ie;
{
	int			i, ie_index, rc;

	/*
	 * Decode the IE header (identifier, instruction field,
	 * and length)
	 */
	rc = usf_dec_ie_hdr(usf, ie);
	if (rc)
		return(rc);
	/*
	 * Ignore the IE if it is of zero length.
	 */
	if (!ie->ie_length) {
		uma_zfree(unisig_ie_zone, ie);
		return(0);
	}

	/*
	 * Look up the information element in the table
	 */
	for (i=0; (ie->ie_ident != ie_table[i].ident) &&
			(ie_table[i].decode != NULL); i++) {
	}
	if (ie_table[i].decode == NULL) {
		/*
		 * Unrecognized IE
		 */
		ie_index = UNI_MSG_IE_ERR;
	} else {
		ie_index = ie_table[i].p_idx;
	}

	/*
	 * Check for unimplemented or unrecognized IEs
	 */
	if (ie_index == UNI_MSG_IE_ERR) {
		ie->ie_err_cause = UNI_IE_CAUS_IEEXIST;

		/*
		 * Skip over the invalid IE
		 */
		rc = usf_dec_ie_uimp(usf, ie);
		if (rc)
			return(rc);
		goto done;
	}

	/*
	 * Check the length against the IE table
	 */
	if (ie->ie_length < ie_table[i].min_len ||
			ie->ie_length > ie_table[i].max_len) {
		ie_index = UNI_MSG_IE_ERR;
		ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;

		/*
		 * Skip over the invalid IE
		 */
		rc = usf_dec_ie_uimp(usf, ie);
		if (rc)
			return(rc);
		goto done;
	}

	/*
	 * Process the IE by calling the function indicated
	 * in the IE table
	 */
	rc = ie_table[i].decode(usf, ie);
	if (rc)
		return(rc);

	/*
	 * Link the IE into the signalling message
	 */
done:
	if (ie->ie_err_cause) {
		ie_index = UNI_MSG_IE_ERR;
	}
	MSG_IE_ADD(msg, ie, ie_index);

	return(0);
}


/*
 * Decode an information element header
 *
 * Arguments:
 *	usf	pointer to a UNISIG formatting structure
 *	ie	pointer to a generic IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_dec_ie_hdr(usf, ie)
	struct usfmt			*usf;
	struct ie_generic		*ie;
{
	u_char			c;
	short			s;
	int			rc;

	/*
	 * Get the IE identifier
	 */
	rc = usf_byte(usf, &ie->ie_ident);
	if (rc)
		return(rc);

	/*
	 * Get the extended type
	 */
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	ie->ie_coding = (c >> UNI_IE_CODE_SHIFT) & UNI_IE_CODE_MASK;
	ie->ie_flag = (c >> UNI_IE_FLAG_SHIFT) & UNI_IE_FLAG_MASK;
	ie->ie_action = c & UNI_IE_ACT_MASK;

	/*
	 * Get the length.
	 */
	rc = usf_short(usf, &s);
	if (rc)
		return(rc);
	ie->ie_length = s;

	return(0);
}


/*
 * Decode an AAL parameters information element
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
usf_dec_ie_aalp(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int		i, rc = 0;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_aalp_absent, &ie->ie_u.ie_aalp,
			sizeof(ie_aalp_absent));

	/*
	 * Get the AAL type
	 */
	rc = usf_byte(usf, &ie->ie_aalp_aal_type);
	if (rc)
		return(rc);

	/*
	 * Subtract the length of the AAL type from the total.
	 * It will be readjusted after usf_dec_ie_ident is finished.
	 */
	ie->ie_length--;

	/*
	 * Process based on AAL type
	 */
	switch (ie->ie_aalp_aal_type) {
	case UNI_IE_AALP_AT_AAL1:
		/*
		 * Clear the AAL 1 subparameters
		 */
		ie->ie_aalp_1_subtype = T_ATM_ABSENT;
		ie->ie_aalp_1_cbr_rate = T_ATM_ABSENT;
		ie->ie_aalp_1_multiplier = T_ATM_ABSENT;
		ie->ie_aalp_1_clock_recovery = T_ATM_ABSENT;
		ie->ie_aalp_1_error_correction = T_ATM_ABSENT;
		ie->ie_aalp_1_struct_data_tran = T_ATM_ABSENT;
		ie->ie_aalp_1_partial_cells = T_ATM_ABSENT;

		/*
		 * Parse the AAL fields based on their IDs
		 */
		rc = usf_dec_ie_ident(usf, ie, ie_aal1_tbl);
		break;
	case UNI_IE_AALP_AT_AAL3:
		/*
		 * Clear the AAL 3/4 subparameters
		 */
		ie->ie_aalp_4_fwd_max_sdu = T_ATM_ABSENT;
		ie->ie_aalp_4_bkwd_max_sdu = T_ATM_ABSENT;
		ie->ie_aalp_4_mid_range = T_ATM_ABSENT;
		ie->ie_aalp_4_mode = T_ATM_ABSENT;
		ie->ie_aalp_4_sscs_type = T_ATM_ABSENT;

		/*
		 * Parse the AAL fields based on their IDs
		 */
		if (usf->usf_sig->us_proto == ATM_SIG_UNI30)
			rc = usf_dec_ie_ident(usf, ie, ie_aal4_tbl_30);
		else
			rc = usf_dec_ie_ident(usf, ie, ie_aal4_tbl_31);

		/*
		 * If either forward or backward maximum SDU
		 * size is specified, the other must also be
		 * specified.
		 */
		if ((ie->ie_aalp_4_fwd_max_sdu != T_ATM_ABSENT &&
				ie->ie_aalp_4_bkwd_max_sdu == T_ATM_ABSENT) ||
		    (ie->ie_aalp_4_fwd_max_sdu == T_ATM_ABSENT &&
				ie->ie_aalp_4_bkwd_max_sdu != T_ATM_ABSENT)) {
			ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
		}
		break;
	case UNI_IE_AALP_AT_AAL5:
		/*
		 * Clear the AAL 5 subparameters
		 */
		ie->ie_aalp_5_fwd_max_sdu = T_ATM_ABSENT;
		ie->ie_aalp_5_bkwd_max_sdu = T_ATM_ABSENT;
		ie->ie_aalp_5_mode = T_ATM_ABSENT;
		ie->ie_aalp_5_sscs_type = T_ATM_ABSENT;

		/*
		 * Parse the AAL fields based on their IDs
		 */
		if (usf->usf_sig->us_proto == ATM_SIG_UNI30)
			rc = usf_dec_ie_ident(usf, ie, ie_aal5_tbl_30);
		else
			rc = usf_dec_ie_ident(usf, ie, ie_aal5_tbl_31);

		/*
		 * If either forward or backward maximum SDU
		 * size is specified, the other must also be
		 * specified.
		 */
		if ((ie->ie_aalp_5_fwd_max_sdu != T_ATM_ABSENT &&
				ie->ie_aalp_5_bkwd_max_sdu == T_ATM_ABSENT) ||
		    (ie->ie_aalp_5_fwd_max_sdu == T_ATM_ABSENT &&
				ie->ie_aalp_5_bkwd_max_sdu != T_ATM_ABSENT)) {
			ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
		}
		break;
	case UNI_IE_AALP_AT_AALU:
		/*
		 * Check user parameter length
		 */
		if (ie->ie_length >
				sizeof(ie->ie_aalp_user_info) +
				1) {
			ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
		}

		/*
		 * Get the user data
		 */
		i = 0;
		while (i < ie->ie_length - 2) {
			rc = usf_byte(usf, &ie->ie_aalp_user_info[i]);
			if (rc)
				break;
			i++;
		}
		break;
	default:
		ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
	}
	ie->ie_length++;

	return(rc);
}


/*
 * Decode a user cell rate information element
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
usf_dec_ie_clrt(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	rc;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_clrt_absent, &ie->ie_u.ie_clrt,
			sizeof(ie_clrt_absent));

	/*
	 * Parse the IE using field identifiers
	 */
	rc = usf_dec_ie_ident(usf, ie, ie_clrt_tbl);
	return(rc);
}


/*
 * Decode a broadband bearer capability information element
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
usf_dec_ie_bbcp(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	rc;
	u_char	c;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_bbcp_absent, &ie->ie_u.ie_bbcp,
			sizeof(ie_bbcp_absent));

	/*
	 * Get the broadband bearer class
	 */
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	ie->ie_bbcp_bearer_class = c & UNI_IE_BBCP_BC_MASK;

	/*
	 * If the broadband bearer class was X, the next
	 * byte has the traffic type and timing requirements
	 */
	if (ie->ie_bbcp_bearer_class == UNI_IE_BBCP_BC_BCOB_X &&
			!(c & UNI_IE_EXT_BIT)) {
		rc = usf_byte(usf, &c);
		if (rc)
			return(rc);
		ie->ie_bbcp_traffic_type = (c >> UNI_IE_BBCP_TT_SHIFT) &
				UNI_IE_BBCP_TT_MASK;
		ie->ie_bbcp_timing_req = c & UNI_IE_BBCP_TR_MASK;
	}

	/*
	 * Get the clipping and user plane connection configuration
	 */
	if (c & UNI_IE_EXT_BIT) {
		rc = usf_byte(usf, &c);
		if (rc)
			return(rc);
		ie->ie_bbcp_clipping = (c >> UNI_IE_BBCP_SC_SHIFT) &
				UNI_IE_BBCP_SC_MASK;
		ie->ie_bbcp_conn_config = c & UNI_IE_BBCP_CC_MASK;
	}

	return(0);
}


/*
 * Decode a broadband high layer information element
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
usf_dec_ie_bhli(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	i, rc;
	u_char	c;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_bhli_absent, &ie->ie_u.ie_bhli,
			sizeof(ie_bhli_absent));

	/*
	 * Get the high layer information type
	 */
	rc = usf_ext(usf, &i);
	ie->ie_bhli_type = i & UNI_IE_EXT_MASK;
	if (rc)
		return(rc);

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
		}
		break;
	case UNI_IE_BHLI_TYPE_HLP:
		/*
		 * Make sure the IE is long enough for the high
		 * layer profile information, then get it
		 */
		if (usf->usf_sig->us_proto != ATM_SIG_UNI30)
			ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
		if (ie->ie_length < UNI_IE_BHLI_HLP_LEN+1)
			ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
		for (i=0; i<ie->ie_length &&
				i<UNI_IE_BHLI_HLP_LEN; i++) {
			rc = usf_byte(usf, &ie->ie_bhli_info[i]);
			if (rc)
				return(rc);
		}
		break;
	case UNI_IE_BHLI_TYPE_VSA:
		/*
		 * Make sure the IE is long enough for the vendor-
		 * specific application information, then get it
		 */
		if (ie->ie_length < UNI_IE_BHLI_VSA_LEN+1)
			ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
		for (i=0; i<ie->ie_length &&
				i<UNI_IE_BHLI_VSA_LEN; i++) {
			rc = usf_byte(usf, &ie->ie_bhli_info[i]);
			if (rc)
				return(rc);
		}
		break;
	default:
		ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
		for (i=0; i<ie->ie_length; i++) {
			rc = usf_byte(usf, &c);
			if (rc)
				return(rc);
		}
	}

	return(0);
}


/*
 * Decode a broadband low layer information element
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
usf_dec_ie_blli(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	u_char		c, id;
	int		bc, i, rc;
	u_int		ipi;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_blli_absent, &ie->ie_u.ie_blli,
			sizeof(ie_blli_absent));

	/*
	 * Get paramteters for the protocol layers as long as
	 * there is still information left in the IE
	 */
	bc = ie->ie_length;
	while (bc) {
		/*
		 * Get the type and process based on what it is
		 */
		rc = usf_byte(usf, &id);
		if (rc)
			return(rc);
		switch (((id & UNI_IE_EXT_MASK) >>
				UNI_IE_BLLI_LID_SHIFT) &
				UNI_IE_BLLI_LID_MASK) {
		case UNI_IE_BLLI_L1_ID:
			/*
			 * Layer 1 info
			 */
			ie->ie_blli_l1_id = id & UNI_IE_BLLI_LP_MASK;
			bc--;
			break;
		case UNI_IE_BLLI_L2_ID:
			/*
			 * Layer 2 info--contents vary based on type
			 */
			ie->ie_blli_l2_id = id & UNI_IE_BLLI_LP_MASK;
			bc--;
			if (id & UNI_IE_EXT_BIT)
				break;
			switch (ie->ie_blli_l2_id) {
			case UNI_IE_BLLI_L2P_X25L:
			case UNI_IE_BLLI_L2P_X25M:
			case UNI_IE_BLLI_L2P_HDLC1:
			case UNI_IE_BLLI_L2P_HDLC2:
			case UNI_IE_BLLI_L2P_HDLC3:
			case UNI_IE_BLLI_L2P_Q922:
			case UNI_IE_BLLI_L2P_ISO7776:
				rc = usf_byte(usf, &c);
				if (rc)
					return(rc);
				bc--;
				ie->ie_blli_l2_mode = (c >>
					UNI_IE_BLLI_L2MODE_SHIFT) &
					UNI_IE_BLLI_L2MODE_MASK;
				if (!(c & UNI_IE_EXT_BIT))
					break;
				rc = usf_byte(usf, &c);
				if (rc)
					return(rc);
				bc--;
				ie->ie_blli_l2_window =
						c & UNI_IE_EXT_MASK;
				break;
			case UNI_IE_BLLI_L2P_USER:
				rc = usf_byte(usf, &c);
				if (rc)
					return(rc);
				bc--;
				ie->ie_blli_l2_user_proto =
						c & UNI_IE_EXT_MASK;
				break;
			}
			break;
		case UNI_IE_BLLI_L3_ID:
			/*
			 * Layer 3 info--contents vary based on type
			 */
			ie->ie_blli_l3_id = id & UNI_IE_BLLI_LP_MASK;
			bc--;
			switch (ie->ie_blli_l3_id) {
			case UNI_IE_BLLI_L3P_X25:
			case UNI_IE_BLLI_L3P_ISO8208:
			case UNI_IE_BLLI_L3P_ISO8878:
				rc = usf_byte(usf, &c);
				if (rc)
					return(rc);
				bc--;
				ie->ie_blli_l3_mode = (c >>
					UNI_IE_BLLI_L3MODE_SHIFT) &
					UNI_IE_BLLI_L3MODE_MASK;
				if (!(c & UNI_IE_EXT_BIT))
					break;
				rc = usf_byte(usf, &c);
				if (rc)
					return(rc);
				bc--;
				ie->ie_blli_l3_packet_size =
					c & UNI_IE_BLLI_L3PS_MASK;
				if (!(c & UNI_IE_EXT_BIT))
					break;
				rc = usf_byte(usf, &c);
				if (rc)
					return(rc);
				bc--;
				ie->ie_blli_l3_window =
						c & UNI_IE_EXT_MASK;
				break;
			case UNI_IE_BLLI_L3P_USER:
				rc = usf_byte(usf, &c);
				if (rc)
					return(rc);
				bc--;
				ie->ie_blli_l3_mode =
						c & UNI_IE_EXT_MASK;
				break;
			case UNI_IE_BLLI_L3P_ISO9577:
				rc = usf_ext(usf, &ipi);
				if (rc)
					return(rc);
				bc -= 2;
				ie->ie_blli_l3_ipi = ipi >>
						UNI_IE_BLLI_L3IPI_SHIFT;
				if (ie->ie_blli_l3_ipi !=
						UNI_IE_BLLI_L3IPI_SNAP)
					break;

				rc = usf_byte(usf, &c);
				ie->ie_blli_l3_snap_id = c & UNI_IE_EXT_MASK;
				if (rc)
					return(rc);
				bc --;

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
				bc -= 5;
				break;
			}
			break;
		default:
			ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
			for (i=0; i<ie->ie_length; i++) {
				rc = usf_byte(usf, &c);
				if (rc)
					return(rc);
			}
		}
	}

	return(0);
}


/*
 * Decode a call state information element
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
usf_dec_ie_clst(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	rc;
	u_char	c;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_clst_absent, &ie->ie_u.ie_clst,
			sizeof(ie_clst_absent));

	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	ie->ie_clst_state = c & UNI_IE_CLST_STATE_MASK;

	return(0);
}


/*
 * Decode a called party number information element
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
usf_dec_ie_cdad(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int		len, rc;
	u_char		c;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_cdad_absent, &ie->ie_u.ie_cdad,
			sizeof(ie_cdad_absent));

	/*
	 * Get and check the numbering plan
	 */
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	ie->ie_cdad_plan = c & UNI_IE_CDAD_PLAN_MASK;
	len = ie->ie_length - 1;
	switch (ie->ie_cdad_plan) {
	case UNI_IE_CDAD_PLAN_E164:
		ie->ie_cdad_addr.address_format = T_ATM_E164_ADDR;
		break;
	case UNI_IE_CDAD_PLAN_NSAP:
		ie->ie_cdad_addr.address_format = T_ATM_ENDSYS_ADDR;
		break;
	default:
		/*
		 * Invalid numbering plan
		 */
		ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
		while (len) {
			rc = usf_byte(usf, &c);
			if (rc)
				return(rc);
			len--;
		}

		return(0);
	}

	/*
	 * Get the ATM address
	 */
	rc = usf_dec_atm_addr(usf, &ie->ie_cdad_addr, len);
	if (rc == EINVAL) {
		ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
		rc = 0;
	}

	return(rc);
}


/*
 * Decode a called party subaddress information element
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
usf_dec_ie_cdsa(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int		len, rc;
	u_char		c;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_cdsa_absent, &ie->ie_u.ie_cdsa,
			sizeof(ie_cdsa_absent));

	/*
	 * Get and check the subaddress type
	 */
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	len = ie->ie_length - 1;
	if (((c >> UNI_IE_CDSA_TYPE_SHIFT) & UNI_IE_CDSA_TYPE_MASK) != 
			UNI_IE_CDSA_TYPE_AESA) {
		/*
		 * Invalid subaddress type
		 */
		ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
		while (len) {
			rc = usf_byte(usf, &c);
			if (rc)
				return(rc);
			len--;
		}

		return(0);
	}

	/*
	 * Get the ATM address
	 */
	ie->ie_cdsa_addr.address_format = T_ATM_ENDSYS_ADDR;
	rc = usf_dec_atm_addr(usf, &ie->ie_cdsa_addr, len);
	if (rc == EINVAL) {
		ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
		rc = 0;
	}

	return(rc);
}


/*
 * Decode a calling party number information element
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
usf_dec_ie_cgad(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int		len, rc;
	u_char		c;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_cgad_absent, &ie->ie_u.ie_cgad,
			sizeof(ie_cgad_absent));

	/*
	 * Get and check the numbering plan
	 */
	len = ie->ie_length;
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	ie->ie_cgad_type = (c >> UNI_IE_CGAD_TYPE_SHIFT) &
			UNI_IE_CGAD_TYPE_MASK;
	ie->ie_cgad_plan = c & UNI_IE_CGAD_PLAN_MASK;
	len--;
	switch (ie->ie_cgad_plan) {
	case UNI_IE_CGAD_PLAN_E164:
		ie->ie_cgad_addr.address_format = T_ATM_E164_ADDR;
		break;
	case UNI_IE_CGAD_PLAN_NSAP:
		ie->ie_cgad_addr.address_format = T_ATM_ENDSYS_ADDR;
		break;
	default:
		ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
		while (len) {
			rc = usf_byte(usf, &c);
			if (rc)
				return(rc);
			len--;
		}

		return(0);
	}

	/*
	 * Get the presentation and screening indicators, if present
	 */
	if (!(c & UNI_IE_EXT_BIT)) {
		rc = usf_byte(usf, &c);
		if (rc)
			return(rc);
		len--;
		ie->ie_cgad_pres_ind = (c >> UNI_IE_CGAD_PRES_SHIFT) &
				UNI_IE_CGAD_PRES_MASK;
		ie->ie_cgad_screen_ind = c & UNI_IE_CGAD_SCR_MASK;
	} else {
		ie->ie_cgad_pres_ind = 0;
		ie->ie_cgad_screen_ind =0;
	}

	/*
	 * Get the ATM address
	 */
	rc = usf_dec_atm_addr(usf, &ie->ie_cgad_addr, len);
	if (rc == EINVAL) {
		ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
		rc = 0;
	}

	return(rc);
}


/*
 * Decode a calling party subaddress information element
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
usf_dec_ie_cgsa(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int		len, rc;
	u_char		c;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_cgsa_absent, &ie->ie_u.ie_cgsa,
			sizeof(ie_cgsa_absent));

	/*
	 * Get and check the subaddress type
	 */
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	len = ie->ie_length - 1;
	if (((c >> UNI_IE_CGSA_TYPE_SHIFT) & UNI_IE_CGSA_TYPE_MASK) != 
			UNI_IE_CGSA_TYPE_AESA) {
		/*
		 * Invalid subaddress type
		 */
		ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
		while (len) {
			rc = usf_byte(usf, &c);
			if (rc)
				return(rc);
			len--;
		}

		return(0);
	}

	/*
	 * Get the ATM address
	 */
	ie->ie_cgsa_addr.address_format = T_ATM_ENDSYS_ADDR;
	rc = usf_dec_atm_addr(usf, &ie->ie_cgsa_addr, len);
	if (rc == EINVAL) {
		ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
		rc = 0;
	}

	return(rc);
}


/*
 * Decode a cause information element
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
usf_dec_ie_caus(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	i, len, rc;
	u_char	c;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_caus_absent, &ie->ie_u.ie_caus,
			sizeof(ie_caus_absent));

	/*
	 * Get the cause location
	 */
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	ie->ie_caus_loc = c & UNI_IE_CAUS_LOC_MASK;

	/*
	 * Get the cause value
	 */
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	ie->ie_caus_cause = c & UNI_IE_EXT_MASK;

	/*
	 * Get any included diagnostics
	 */
	len = ie->ie_length - 2;
	for (i = 0, ie->ie_caus_diag_len = 0;
			len && i < sizeof(ie->ie_caus_diagnostic);
			len--, i++, ie->ie_caus_diag_len++) {
		rc = usf_byte(usf, &ie->ie_caus_diagnostic[i]);
		if (rc)
			return(rc);
	}

	return(0);
}


/*
 * Decode a conection identifier information element
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
usf_dec_ie_cnid(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	i, rc;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_cnid_absent, &ie->ie_u.ie_cnid,
			sizeof(ie_cnid_absent));

	rc = usf_ext(usf, &i);
	if (rc)
		return(rc);
	ie->ie_cnid_vp_sig = (i >> UNI_IE_CNID_VPSIG_SHIFT) &
			UNI_IE_CNID_VPSIG_MASK;
	ie->ie_cnid_pref_excl = i & UNI_IE_CNID_PREX_MASK;

	rc = usf_short(usf, &ie->ie_cnid_vpci);
	if (rc)
		return(rc);
	rc = usf_short(usf, &ie->ie_cnid_vci);
	return(rc);
}


/*
 * Decode a quality of service parameters information element
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
usf_dec_ie_qosp(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int		rc;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_qosp_absent, &ie->ie_u.ie_qosp,
			sizeof(ie_qosp_absent));

	/*
	 * Get forward QoS class
	 */
	rc = usf_byte(usf, &ie->ie_qosp_fwd_class);
	if (rc)
		return(rc);

	/*
	 * Get backward QoS class
	 */
	rc = usf_byte(usf, &ie->ie_qosp_bkwd_class);

	return(rc);
}


/*
 * Decode a broadband repeat indicator information element
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
usf_dec_ie_brpi(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	rc;
	u_char	c;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_brpi_absent, &ie->ie_u.ie_brpi,
			sizeof(ie_brpi_absent));

	/*
	 * Get the repeat indicator
	 */
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);

	ie->ie_brpi_ind = c & UNI_IE_BRPI_IND_MASK;

	return(0);
}


/*
 * Decode a restart indicator information element
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
usf_dec_ie_rsti(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int		rc;
	u_char		c;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_rsti_absent, &ie->ie_u.ie_rsti,
			sizeof(ie_rsti_absent));

	/*
	 * Get the restart class
	 */
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);

	ie->ie_rsti_class = c & UNI_IE_RSTI_CLASS_MASK;

	return(0);
}


/*
 * Decode a broadband sending complete information element
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
usf_dec_ie_bsdc(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	rc;
	u_char	c;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_bsdc_absent, &ie->ie_u.ie_bsdc,
			sizeof(ie_bsdc_absent));

	/*
	 * Get the sending complete indicator
	 */
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);

	/*
	 * Validate the indicator
	 */
	c &= UNI_IE_EXT_MASK;
	if (c != UNI_IE_BSDC_IND)
		ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
	ie->ie_bsdc_ind = c;

	return(0);
}


/*
 * Decode a transit network selection information element
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	ie	pointer to a transit network selection IE structure
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_dec_ie_trnt(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	i, len, rc;
	u_char	c;

	/*
	 * Clear the IE
	 */
	bcopy(&ie_trnt_absent, &ie->ie_u.ie_trnt,
			sizeof(ie_trnt_absent));

	/*
	 * Get the network ID type and plan
	 */
	rc = usf_byte(usf, &c);
	if (rc)
		return(rc);
	ie->ie_trnt_id_type = (c >> UNI_IE_TRNT_IDT_SHIFT) &
			UNI_IE_TRNT_IDT_MASK;
	ie->ie_trnt_id_plan = c & UNI_IE_TRNT_IDP_MASK;

	/*
	 * Get the length of the network ID
	 */
	len = ie->ie_length - 1;
	ie->ie_trnt_id_len = MIN(len, sizeof(ie->ie_trnt_id));

	/*
	 * Get the network ID
	 */
	for (i=0; i<len; i++) {
		if (i<sizeof(ie->ie_trnt_id))
			rc = usf_byte(usf, &ie->ie_trnt_id[i]);
		else
			rc = usf_byte(usf, &c);
		if (rc)
			return(rc);
	}

	return(0);
}


/*
 * Decode an unimplemented information element
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
usf_dec_ie_uimp(usf, ie)
	struct usfmt		*usf;
	struct ie_generic	*ie;
{
	int	i, rc;
	u_char	c;

	/*
	 * Skip over the IE contents
	 */
	for (i=0; i<ie->ie_length; i++) {
		rc = usf_byte(usf, &c);
		if (rc)
			return(rc);
	}

	return(0);
}


/*
 * Decode an information element using field identifiers
 *
 * The AAL parameters and ATM user cell rate IEs are formatted
 * with a one-byte identifier preceding each field.  The routine
 * parses these IEs by using a table which relates the field
 * identifiers with the fields in the appropriate IE structure.
 * Field order in the received message is immaterial.
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
usf_dec_ie_ident(usf, ie, tbl)
	struct usfmt		*usf;
	struct ie_generic	*ie;
	struct ie_decode_tbl	*tbl;
{
	int	i, len, rc;
	u_char	c;
	u_int8_t	cv;
	u_int16_t	sv;
	u_int32_t	iv;
	void		*dest;

	/*
	 * Scan through the IE
	 */
	len = ie->ie_length;
	while (len) {
		/*
		 * Get the field identifier
		 */
		rc = usf_byte(usf, &c);
		if (rc)
			return(rc);
		len--;

		/*
		 * Look up the field in the table
		 */
		for (i=0; (tbl[i].ident != c) && tbl[i].len; i++) {
		}
		if (tbl[i].ident == 0) {
			/*
			 * Bad subfield identifier -- flag an
			 * error and skip over the rest of the IE
			 */
			ie->ie_err_cause = UNI_IE_CAUS_IECONTENT;
			while (len) {
				rc = usf_byte(usf, &c);
				if (rc)
					return(rc);
			}
			return(0);
		}

		/*
		 * Save final destination address
		 */
		dest = (void *)((intptr_t)ie + tbl[i].f_offs);

		/*
		 * Get the field value
		 */
		switch (tbl[i].len) {
			case 0:
				cv = 1;
				goto savec;

			case 1:
				rc = usf_byte(usf, &cv);
				if (rc)
					break;
savec:
				/*
				 * Save field value
				 */
				switch (tbl[i].f_size) {
				case 1:
					*(u_int8_t *)dest = cv;
					break;
				case 2:
					*(u_int16_t *)dest = cv;
					break;
				case 4:
					*(u_int32_t *)dest = cv;
					break;
				default:
					goto badtbl;
				}
				break;

			case 2:
				rc = usf_short(usf, &sv);
				if (rc)
					break;

				/*
				 * Save field value
				 */
				switch (tbl[i].f_size) {
				case 2:
					*(u_int16_t *)dest = sv;
					break;
				case 4:
					*(u_int32_t *)dest = sv;
					break;
				default:
					goto badtbl;
				}
				break;

			case 3:
				rc = usf_int3(usf, &iv);
				goto savei;

			case 4:
				rc = usf_int(usf, &iv);
savei:
				/*
				 * Save field value
				 */
				if (rc)
					break;
				switch (tbl[i].f_size) {
				case 4:
					*(u_int32_t *)dest = iv;
					break;
				default:
					goto badtbl;
				}
				break;

			default:
badtbl:
				log(LOG_ERR,
				    "uni decode: id=%d,len=%d,off=%d,size=%d\n",
					tbl[i].ident, tbl[i].len,
					tbl[i].f_offs, tbl[i].f_size);
				rc = EFAULT;
				break;
		}

		if (rc)
			return(rc);

		len -= tbl[i].len;

	}

	return(0);
}


/*
 * Decode an ATM address
 *
 * Arguments:
 *	usf	pointer to a unisig formatting structure
 *	addr	pointer to an ATM address structure
 *	len	length of data remainig in the IE
 *
 * Returns:
 *	0	success
 *	errno	error encountered
 *
 */
static int
usf_dec_atm_addr(usf, addr, len)
	struct usfmt	*usf;
	Atm_addr	*addr;
	int		len;
{
	int		rc;
	u_char		c, *cp;

	/*
	 * Check the address type
	 */
	addr->address_length = len;
	switch (addr->address_format) {
	case T_ATM_E164_ADDR:
		if (len > sizeof(Atm_addr_e164)) {
			goto flush;
		}
		cp = (u_char *) addr->address;
		break;
	case T_ATM_ENDSYS_ADDR:
		if (len != sizeof(Atm_addr_nsap)) {
			goto flush;
		}
		cp = (u_char *) addr->address;
		break;
	default:
		/* Silence the compiler */
		cp = NULL;
	}

	/*
	 * Get the ATM address
	 */
	while (len) {
		rc = usf_byte(usf, cp);
		if (rc)
			return(rc);
		len--;
		cp++;
	}

	return(0);

flush:
	while (len) {
		rc = usf_byte(usf, &c);
		if (rc)
			return(rc);
		len--;
	}

	return(EINVAL);
}
