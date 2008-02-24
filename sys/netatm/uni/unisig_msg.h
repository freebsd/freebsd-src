/*-
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
 *	@(#) $FreeBSD: src/sys/netatm/uni/unisig_msg.h,v 1.4 2005/01/07 01:45:37 imp Exp $
 *
 */

/*
 * ATM Forum UNI 3.0/3.1 Signalling Manager
 * ----------------------------------------
 *
 * Message formatting blocks
 *
 */

#ifndef _UNI_SIG_MSG_H
#define	_UNI_SIG_MSG_H

#define	UNI_MSG_DISC_Q93B	0x09
#define	UNI_MSG_MIN_LEN		9

/*
 * Values for Q.2931 message type.
 */
#define	UNI_MSG_CALP	0x02
#define	UNI_MSG_CONN	0x07
#define	UNI_MSG_CACK	0x0F
#define	UNI_MSG_SETU	0x05
#define	UNI_MSG_RLSE	0x4D
#define	UNI_MSG_RLSC	0x5A
#define	UNI_MSG_RSTR	0x46
#define	UNI_MSG_RSTA	0x4E
#define	UNI_MSG_STAT	0x7D
#define	UNI_MSG_SENQ	0x75
#define	UNI_MSG_ADDP	0x80
#define	UNI_MSG_ADPA	0x81
#define	UNI_MSG_ADPR	0x82
#define	UNI_MSG_DRPP	0x83
#define	UNI_MSG_DRPA	0x84


/*
 * Values for information element identifier.
 */
#define	UNI_IE_CAUS	0x08
#define	UNI_IE_CLST	0x14
#define	UNI_IE_EPRF	0x54
#define	UNI_IE_EPST	0x55
#define	UNI_IE_AALP	0x58
#define	UNI_IE_CLRT	0x59
#define	UNI_IE_CNID	0x5A
#define	UNI_IE_QOSP	0x5C
#define	UNI_IE_BHLI	0x5D
#define	UNI_IE_BBCP	0x5E
#define	UNI_IE_BLLI	0x5F
#define	UNI_IE_BLSH	0x60
#define	UNI_IE_BNSH	0x61
#define	UNI_IE_BSDC	0x62
#define	UNI_IE_BRPI	0x63
#define	UNI_IE_CGAD	0x6C
#define	UNI_IE_CGSA	0x6D
#define	UNI_IE_CDAD	0x70
#define	UNI_IE_CDSA	0x71
#define	UNI_IE_TRNT	0x78
#define	UNI_IE_RSTI	0x79

/*
 * Masks for information element extension in bit 8
 */
#define	UNI_IE_EXT_BIT	0x80
#define	UNI_IE_EXT_MASK	0x7F


/*
 * Signalling message in internal format.
 */
#define	UNI_MSG_IE_CNT		22

struct unisig_msg {
	u_int				msg_call_ref;
	u_char				msg_type;
	u_char				msg_type_flag;
	u_char				msg_type_action;
	int				msg_length;
	struct ie_generic		*msg_ie_vec[UNI_MSG_IE_CNT];
};

#define	UNI_MSG_CALL_REF_RMT	0x800000
#define	UNI_MSG_CALL_REF_MASK	0x7FFFFF
#define	UNI_MSG_CALL_REF_GLOBAL	0
#define	UNI_MSG_CALL_REF_DUMMY	0x7FFFFF

#define	EXTRACT_CREF(x)						\
	((x) & UNI_MSG_CALL_REF_RMT ? (x) & UNI_MSG_CALL_REF_MASK : (x) | UNI_MSG_CALL_REF_RMT)
#define GLOBAL_CREF(x)	(((x) & UNI_MSG_CALL_REF_MASK) == UNI_MSG_CALL_REF_GLOBAL)
#define DUMMY_CREF(x)	(((x) & UNI_MSG_CALL_REF_MASK) == UNI_MSG_CALL_REF_DUMMY)

#define	UNI_MSG_TYPE_FLAG_MASK	1
#define	UNI_MSG_TYPE_FLAG_SHIFT	4

#define	UNI_MSG_TYPE_ACT_CLEAR	0
#define	UNI_MSG_TYPE_ACT_DISC	1
#define	UNI_MSG_TYPE_ACT_RPRT	2
#define	UNI_MSG_TYPE_ACT_RSVD	3
#define	UNI_MSG_TYPE_ACT_MASK	3

#define	UNI_MSG_IE_AALP		0
#define	UNI_MSG_IE_CLRT		1
#define	UNI_MSG_IE_BBCP		2
#define	UNI_MSG_IE_BHLI		3
#define	UNI_MSG_IE_BLLI		4
#define	UNI_MSG_IE_CLST		5
#define	UNI_MSG_IE_CDAD		6
#define	UNI_MSG_IE_CDSA		7
#define	UNI_MSG_IE_CGAD		8
#define	UNI_MSG_IE_CGSA		9
#define	UNI_MSG_IE_CAUS		10
#define	UNI_MSG_IE_CNID		11
#define	UNI_MSG_IE_QOSP		12
#define	UNI_MSG_IE_BRPI		13
#define	UNI_MSG_IE_RSTI		14
#define	UNI_MSG_IE_BLSH		15
#define	UNI_MSG_IE_BNSH		16
#define	UNI_MSG_IE_BSDC		17
#define	UNI_MSG_IE_TRNT		18
#define	UNI_MSG_IE_EPRF		19
#define	UNI_MSG_IE_EPST		20
#define	UNI_MSG_IE_ERR		21

#define	msg_ie_aalp	msg_ie_vec[UNI_MSG_IE_AALP]
#define	msg_ie_clrt	msg_ie_vec[UNI_MSG_IE_CLRT]
#define	msg_ie_bbcp	msg_ie_vec[UNI_MSG_IE_BBCP]
#define	msg_ie_bhli	msg_ie_vec[UNI_MSG_IE_BHLI]
#define	msg_ie_blli	msg_ie_vec[UNI_MSG_IE_BLLI]
#define	msg_ie_clst	msg_ie_vec[UNI_MSG_IE_CLST]
#define	msg_ie_cdad	msg_ie_vec[UNI_MSG_IE_CDAD]
#define	msg_ie_cdsa	msg_ie_vec[UNI_MSG_IE_CDSA]
#define	msg_ie_cgad	msg_ie_vec[UNI_MSG_IE_CGAD]
#define	msg_ie_cgsa	msg_ie_vec[UNI_MSG_IE_CGSA]
#define	msg_ie_caus	msg_ie_vec[UNI_MSG_IE_CAUS]
#define	msg_ie_cnid	msg_ie_vec[UNI_MSG_IE_CNID]
#define	msg_ie_qosp	msg_ie_vec[UNI_MSG_IE_QOSP]
#define	msg_ie_brpi	msg_ie_vec[UNI_MSG_IE_BRPI]
#define	msg_ie_rsti	msg_ie_vec[UNI_MSG_IE_RSTI]
#define	msg_ie_blsh	msg_ie_vec[UNI_MSG_IE_BLSH]
#define	msg_ie_bnsh	msg_ie_vec[UNI_MSG_IE_BNSH]
#define	msg_ie_bsdc	msg_ie_vec[UNI_MSG_IE_BSDC]
#define	msg_ie_trnt	msg_ie_vec[UNI_MSG_IE_TRNT]
#define	msg_ie_eprf	msg_ie_vec[UNI_MSG_IE_EPRF]
#define	msg_ie_epst	msg_ie_vec[UNI_MSG_IE_EPST]
#define	msg_ie_err	msg_ie_vec[UNI_MSG_IE_ERR]


/*
 * Information element header.
 */
struct ie_hdr {
	u_char			ie_hdr_ident;
	u_char			ie_hdr_coding;
	u_char			ie_hdr_flag;
	u_char			ie_hdr_action;
	int			ie_hdr_length;
	int			ie_hdr_err_cause;
	struct ie_generic	*ie_hdr_next;
};

#define	UNI_IE_HDR_LEN		4

#define	UNI_IE_CODE_CCITT	0
#define	UNI_IE_CODE_STD		3
#define	UNI_IE_CODE_MASK	3
#define	UNI_IE_CODE_SHIFT	5

#define	UNI_IE_FLAG_MASK	1
#define	UNI_IE_FLAG_SHIFT	4

#define	UNI_IE_ACT_CLEAR	0
#define	UNI_IE_ACT_DIS		1
#define	UNI_IE_ACT_RPRT		2
#define	UNI_IE_ACT_DMSGIGN	5
#define	UNI_IE_ACT_DMSGRPRT	6
#define	UNI_IE_ACT_MASK		7


/*
 * ATM AAL parameters information element in internal format.
 */
struct ie_aalp {
	int8_t	ie_aal_type;
	union {
		struct aal_type_1_parm {
			u_char	subtype;
			u_char	cbr_rate;
			u_short	multiplier;
			u_char	clock_recovery;
			u_char	error_correction;
			u_char	struct_data_tran;
			u_char	partial_cells;
		} type_1;
		struct aal_type_4_parm {
			int32_t	fwd_max_sdu;
			int32_t	bkwd_max_sdu;
			int32_t	mid_range;
			u_char	mode;
			u_char	sscs_type;
		} type_4;
		struct aal_type_5_parm {
			int32_t	fwd_max_sdu;
			int32_t	bkwd_max_sdu;
			u_char	mode;
			u_char	sscs_type;
		} type_5;
		struct user_aal_type {
			u_char	aal_info[4];
		} type_user;
	} aal_u;
};

#define	UNI_IE_AALP_AT_AAL1	1
#define	UNI_IE_AALP_AT_AAL3	3
#define	UNI_IE_AALP_AT_AAL5	5
#define	UNI_IE_AALP_AT_AALU	16

#define	UNI_IE_AALP_A1_ST_NULL	0
#define	UNI_IE_AALP_A1_ST_VCE	1
#define	UNI_IE_AALP_A1_ST_SCE	2
#define	UNI_IE_AALP_A1_ST_ACE	3
#define	UNI_IE_AALP_A1_ST_HQA	4
#define	UNI_IE_AALP_A1_ST_VID	5

#define	UNI_IE_AALP_A1_CB_64	1
#define	UNI_IE_AALP_A1_CB_DS1	4
#define	UNI_IE_AALP_A1_CB_DS2	5
#define	UNI_IE_AALP_A1_CB_32064	6
#define	UNI_IE_AALP_A1_CB_DS3	7
#define	UNI_IE_AALP_A1_CB_97728	8
#define	UNI_IE_AALP_A1_CB_E1	16
#define	UNI_IE_AALP_A1_CB_E2	17
#define	UNI_IE_AALP_A1_CB_E3	18
#define	UNI_IE_AALP_A1_CB_139264	19
#define	UNI_IE_AALP_A1_CB_N64	64

#define	UNI_IE_AALP_A1_CR_NULL	0
#define	UNI_IE_AALP_A1_CR_SRTS	1
#define	UNI_IE_AALP_A1_CR_ACR	2

#define	UNI_IE_AALP_A1_EC_NULL	0
#define	UNI_IE_AALP_A1_EC_FEC	1

#define	UNI_IE_AALP_A1_SD_NULL	0
#define	UNI_IE_AALP_A1_SD_SDT	1

#define	UNI_IE_AALP_A3_R_MASK	1023
#define	UNI_IE_AALP_A3_R_SHIFT	16

#define	UNI_IE_AALP_A5_M_MSG	1
#define	UNI_IE_AALP_A5_M_STR	2

#define	UNI_IE_AALP_A5_ST_NULL	0
#define	UNI_IE_AALP_A5_ST_AO	1
#define	UNI_IE_AALP_A5_ST_NAO	2
#define	UNI_IE_AALP_A5_ST_FR	4


/*
 * ATM user cell rate information element in internal format.
 */
struct ie_clrt {
	int32_t		ie_fwd_peak;
	int32_t		ie_bkwd_peak;
	int32_t		ie_fwd_peak_01;
	int32_t		ie_bkwd_peak_01;
	int32_t		ie_fwd_sust;
	int32_t		ie_bkwd_sust;
	int32_t		ie_fwd_sust_01;
	int32_t		ie_bkwd_sust_01;
	int32_t		ie_fwd_burst;
	int32_t		ie_bkwd_burst;
	int32_t		ie_fwd_burst_01;
	int32_t		ie_bkwd_burst_01;
	int8_t		ie_best_effort;
	int8_t		ie_tm_options;
};

#define	UNI_IE_CLRT_FWD_PEAK_ID		130
#define	UNI_IE_CLRT_BKWD_PEAK_ID	131
#define	UNI_IE_CLRT_FWD_PEAK_01_ID	132
#define	UNI_IE_CLRT_BKWD_PEAK_01_ID	133
#define	UNI_IE_CLRT_FWD_SUST_ID		136
#define	UNI_IE_CLRT_BKWD_SUST_ID	137
#define	UNI_IE_CLRT_FWD_SUST_01_ID	144
#define	UNI_IE_CLRT_BKWD_SUST_01_ID	145
#define	UNI_IE_CLRT_FWD_BURST_ID	160
#define	UNI_IE_CLRT_BKWD_BURST_ID	161
#define	UNI_IE_CLRT_FWD_BURST_01_ID	176
#define	UNI_IE_CLRT_BKWD_BURST_01_ID	177
#define	UNI_IE_CLRT_BEST_EFFORT_ID	190
#define	UNI_IE_CLRT_TM_OPTIONS_ID	191

#define	UNI_IE_CLRT_TM_FWD_TAG		0x01
#define	UNI_IE_CLRT_TM_BKWD_TAG		0x02


/*
 * Broadband bearer capability information element in internal format.
 */
struct ie_bbcp {
	int8_t		ie_bearer_class;
	int8_t		ie_traffic_type;
	int8_t		ie_timing_req;
	int8_t		ie_clipping;
	int8_t		ie_conn_config;
};


#define	UNI_IE_BBCP_BC_BCOB_A	1
#define	UNI_IE_BBCP_BC_BCOB_C	3
#define	UNI_IE_BBCP_BC_BCOB_X	16
#define	UNI_IE_BBCP_BC_MASK	0x1F

#define	UNI_IE_BBCP_TT_NIND	0
#define	UNI_IE_BBCP_TT_CBR	1
#define	UNI_IE_BBCP_TT_VBR	2
#define	UNI_IE_BBCP_TT_MASK	3
#define	UNI_IE_BBCP_TT_SHIFT	2

#define	UNI_IE_BBCP_TR_NIND	0
#define	UNI_IE_BBCP_TR_EER	1
#define	UNI_IE_BBCP_TR_EENR	2
#define	UNI_IE_BBCP_TR_RSVD	3
#define	UNI_IE_BBCP_TR_MASK	3

#define	UNI_IE_BBCP_SC_NSUS	0
#define	UNI_IE_BBCP_SC_SUS	1
#define	UNI_IE_BBCP_SC_MASK	3
#define	UNI_IE_BBCP_SC_SHIFT	5

#define	UNI_IE_BBCP_CC_PP		0
#define	UNI_IE_BBCP_CC_PM		1
#define	UNI_IE_BBCP_CC_MASK	3


/*
 * Broadband high layer information information element in internal
 * format.
 */
struct ie_bhli {
	int8_t		ie_type;
	u_char		ie_info[8];
};

#define	UNI_IE_BHLI_TYPE_ISO	0
#define	UNI_IE_BHLI_TYPE_USER	1
#define	UNI_IE_BHLI_TYPE_HLP	2
#define	UNI_IE_BHLI_TYPE_VSA	3

#define	UNI_IE_BHLI_HLP_LEN	4
#define	UNI_IE_BHLI_VSA_LEN	7


/*
 * Broadband low-layer information information element in internal
 * format.
 */
struct ie_blli {
	int8_t		ie_l1_id;
	int8_t		ie_l2_id;
	int8_t		ie_l2_mode;
	int8_t		ie_l2_q933_use;
	int8_t		ie_l2_window;
	int8_t		ie_l2_user_proto;
	int8_t		ie_l3_id;
	int8_t		ie_l3_mode;
	int8_t		ie_l3_packet_size;
	int8_t		ie_l3_window;
	int8_t		ie_l3_user_proto;
	int16_t		ie_l3_ipi;
	int8_t		ie_l3_snap_id;
	u_char		ie_l3_oui[3];
	u_char		ie_l3_pid[2];
};

#define	UNI_IE_BLLI_L1_ID	1
#define	UNI_IE_BLLI_L2_ID	2
#define	UNI_IE_BLLI_L3_ID	3
#define	UNI_IE_BLLI_LID_MASK	3
#define	UNI_IE_BLLI_LID_SHIFT	5
#define	UNI_IE_BLLI_LP_MASK	31

#define	UNI_IE_BLLI_L2P_ISO1745	1
#define	UNI_IE_BLLI_L2P_Q921	2
#define	UNI_IE_BLLI_L2P_X25L	6
#define	UNI_IE_BLLI_L2P_X25M	7
#define	UNI_IE_BLLI_L2P_LAPB	8
#define	UNI_IE_BLLI_L2P_HDLC1	9
#define	UNI_IE_BLLI_L2P_HDLC2	10
#define	UNI_IE_BLLI_L2P_HDLC3	11
#define	UNI_IE_BLLI_L2P_LLC	12
#define	UNI_IE_BLLI_L2P_X75	13
#define	UNI_IE_BLLI_L2P_Q922	14
#define	UNI_IE_BLLI_L2P_USER	16
#define	UNI_IE_BLLI_L2P_ISO7776	17

#define	UNI_IE_BLLI_L2MODE_NORM		1
#define	UNI_IE_BLLI_L2MODE_EXT		2
#define	UNI_IE_BLLI_L2MODE_SHIFT	5
#define	UNI_IE_BLLI_L2MODE_MASK		3

#define	UNI_IE_BLLI_Q933_ALT	0

#define	UNI_IE_BLLI_L3P_X25	6
#define	UNI_IE_BLLI_L3P_ISO8208	7
#define	UNI_IE_BLLI_L3P_ISO8878	8
#define	UNI_IE_BLLI_L3P_ISO8473	9
#define	UNI_IE_BLLI_L3P_T70	10
#define	UNI_IE_BLLI_L3P_ISO9577	11
#define	UNI_IE_BLLI_L3P_USER	16

#define	UNI_IE_BLLI_L3MODE_NORM		1
#define	UNI_IE_BLLI_L3MODE_EXT		2
#define	UNI_IE_BLLI_L3MODE_SHIFT	5
#define	UNI_IE_BLLI_L3MODE_MASK		3

#define	UNI_IE_BLLI_L3PS_16	4
#define	UNI_IE_BLLI_L3PS_32	5
#define	UNI_IE_BLLI_L3PS_64	6
#define	UNI_IE_BLLI_L3PS_128	7
#define	UNI_IE_BLLI_L3PS_256	8
#define	UNI_IE_BLLI_L3PS_512	9
#define	UNI_IE_BLLI_L3PS_1024	10
#define	UNI_IE_BLLI_L3PS_2048	11
#define	UNI_IE_BLLI_L3PS_4096	12
#define	UNI_IE_BLLI_L3PS_MASK	15

#define	UNI_IE_BLLI_L3IPI_SHIFT	6
#define	UNI_IE_BLLI_L3IPI_SNAP	0x80


/*
 * Call state information element in internal format.
 */
struct ie_clst {
	int8_t		ie_state;
};

#define	UNI_IE_CLST_STATE_U0	0
#define	UNI_IE_CLST_STATE_U1	1
#define	UNI_IE_CLST_STATE_U3	3
#define	UNI_IE_CLST_STATE_U6	6
#define	UNI_IE_CLST_STATE_U8	8
#define	UNI_IE_CLST_STATE_U9	9
#define	UNI_IE_CLST_STATE_U10	10
#define	UNI_IE_CLST_STATE_U11	11
#define	UNI_IE_CLST_STATE_U12	12

#define	UNI_IE_CLST_STATE_N0	0
#define	UNI_IE_CLST_STATE_N1	1
#define	UNI_IE_CLST_STATE_N3	3
#define	UNI_IE_CLST_STATE_N6	6
#define	UNI_IE_CLST_STATE_N8	8
#define	UNI_IE_CLST_STATE_N9	9
#define	UNI_IE_CLST_STATE_N10	10
#define	UNI_IE_CLST_STATE_N11	11
#define	UNI_IE_CLST_STATE_N12	12

#define	UNI_IE_CLST_GLBL_REST0	0x00
#define	UNI_IE_CLST_GLBL_REST1	0x3d
#define	UNI_IE_CLST_GLBL_REST2	0x3e

#define	UNI_IE_CLST_STATE_MASK	0x3f


/*
 * Called party number information element in internal format.
 */
struct ie_cdad {
	int8_t		ie_type;
	int8_t		ie_plan;
	Atm_addr	ie_addr;
};

#define	UNI_IE_CDAD_TYPE_UNK	0
#define	UNI_IE_CDAD_TYPE_INTL	1
#define	UNI_IE_CDAD_TYPE_MASK	7
#define	UNI_IE_CDAD_TYPE_SHIFT	4

#define	UNI_IE_CDAD_PLAN_E164	1
#define	UNI_IE_CDAD_PLAN_NSAP	2
#define	UNI_IE_CDAD_PLAN_MASK	15


/*
 * Called party subaddress information element in internal format.
 */
struct ie_cdsa {
	Atm_addr	ie_addr;
};

#define	UNI_IE_CDSA_TYPE_NSAP	0
#define	UNI_IE_CDSA_TYPE_AESA	1
#define	UNI_IE_CDSA_TYPE_MASK	7
#define	UNI_IE_CDSA_TYPE_SHIFT	4


/*
 * Calling party number information element in internal format.
 */
struct ie_cgad {
	int8_t		ie_type;
	int8_t		ie_plan;
	int8_t		ie_pres_ind;
	int8_t		ie_screen_ind;
	Atm_addr	ie_addr;
};

#define	UNI_IE_CGAD_TYPE_UNK	0
#define	UNI_IE_CGAD_TYPE_INTL	1
#define	UNI_IE_CGAD_TYPE_MASK	7
#define	UNI_IE_CGAD_TYPE_SHIFT	4

#define	UNI_IE_CGAD_PLAN_E164	1
#define	UNI_IE_CGAD_PLAN_NSAP	2
#define	UNI_IE_CGAD_PLAN_MASK	15

#define	UNI_IE_CGAD_PRES_ALLOW	0
#define	UNI_IE_CGAD_PRES_RSTR	1
#define	UNI_IE_CGAD_PRES_NNA	2
#define	UNI_IE_CGAD_PRES_RSVD	3
#define	UNI_IE_CGAD_PRES_MASK	3
#define	UNI_IE_CGAD_PRES_SHIFT	5

#define	UNI_IE_CGAD_SCR_UNS	0
#define	UNI_IE_CGAD_SCR_UVP	1
#define	UNI_IE_CGAD_SCR_UVF	2
#define	UNI_IE_CGAD_SCR_NET	3
#define	UNI_IE_CGAD_SCR_MASK	3


/*
 * Calling party subaddress information element in internal format.
 */
struct ie_cgsa {
	Atm_addr	ie_addr;
};

#define	UNI_IE_CGSA_TYPE_NSAP	0
#define	UNI_IE_CGSA_TYPE_AESA	1
#define	UNI_IE_CGSA_TYPE_MASK	7
#define	UNI_IE_CGSA_TYPE_SHIFT	4


/*
 * Cause information element in internal format.
 */
#define	UNI_IE_CAUS_MAX_ID	24
#define	UNI_IE_CAUS_MAX_QOS_SUB	24
struct ie_caus {
	int8_t		ie_loc;
	int8_t		ie_cause;
	int8_t		ie_diag_len;
	u_int8_t	ie_diagnostic[24];
};

#define	UNI_IE_CAUS_LOC_USER	0
#define	UNI_IE_CAUS_LOC_PRI_LCL	1
#define	UNI_IE_CAUS_LOC_PUB_LCL	2
#define	UNI_IE_CAUS_LOC_TRANSIT	3
#define	UNI_IE_CAUS_LOC_PUB_RMT	4
#define	UNI_IE_CAUS_LOC_PRI_RMT	5
#define	UNI_IE_CAUS_LOC_INTL	7
#define	UNI_IE_CAUS_LOC_BEYOND	10
#define	UNI_IE_CAUS_LOC_MASK	15

#define	UNI_IE_CAUS_UN_NS_SHIFT	3
#define	UNI_IE_CAUS_UN_NS_MASK	1

#define	UNI_IE_CAUS_UN_NA_SHIFT	2
#define	UNI_IE_CAUS_UN_NA_MASK	1

#define	UNI_IE_CAUS_UN_CAU_MASK	3

#define	UNI_IE_CAUS_RR_USER	0
#define	UNI_IE_CAUS_RR_IE	1
#define	UNI_IE_CAUS_RR_INSUFF	2
#define UNI_IE_CAUS_RR_SHIFT	2
#define UNI_IE_CAUS_RR_MASK	31

#define	UNI_IE_CAUS_RC_UNK	0
#define	UNI_IE_CAUS_RC_PERM	1
#define	UNI_IE_CAUS_RC_TRANS	2
#define	UNI_IE_CAUS_RC_MASK	3

/*
 * Cause codes from UNI 3.0, section 5.4.5.15
 */
#define	UNI_IE_CAUS_UNO		1	/* Unallocated number */
#define	UNI_IE_CAUS_NOTROUTE	2	/* No route to transit net */
#define	UNI_IE_CAUS_NODROUTE	3	/* No route to destination */
#define	UNI_IE_CAUS_BAD_VCC	10	/* VPI/VCI unacceptable */
#define	UNI_IE_CAUS_NORM	16	/* Normal call clearing */
#define	UNI_IE_CAUS_BUSY	17	/* User busy */
#define	UNI_IE_CAUS_NORSP	18	/* No user responding */
#define	UNI_IE_CAUS_REJECT	21	/* Call rejected */
#define	UNI_IE_CAUS_CHANGED	22	/* Number changed */
#define	UNI_IE_CAUS_CLIR	23	/* User rejects CLIR */
#define	UNI_IE_CAUS_DORDER	27	/* Dest out of order */
#define	UNI_IE_CAUS_INVNO	28	/* Invalid number format */
#define	UNI_IE_CAUS_SENQ	30	/* Rsp to Status Enquiry */
#define	UNI_IE_CAUS_NORM_UNSP	31	/* Normal, unspecified */
#define	UNI_IE_CAUS_NA_VCC	35	/* VCC not available */
#define	UNI_IE_CAUS_ASSIGN_VCC	36	/* VPCI/VCI assignment failure */
#define	UNI_IE_CAUS_NORDER	38	/* Network out of order */
#define	UNI_IE_CAUS_TEMP	41	/* Temporary failure */
#define	UNI_IE_CAUS_DISCARD	43	/* Access info discarded */
#define	UNI_IE_CAUS_NO_VCC	45	/* No VPI/VCI available */
#define	UNI_IE_CAUS_UNAVAIL	47	/* Resource unavailable */
#define	UNI_IE_CAUS_NO_QOS	49	/* QoS unavailable */
#define	UNI_IE_CAUS_NO_CR	51	/* User cell rate not avail */
#define	UNI_IE_CAUS_NO_BC	57	/* Bearer capability not auth */
#define	UNI_IE_CAUS_NA_BC	58	/* Bearer capability n/a */
#define	UNI_IE_CAUS_SERVICE	63	/* Service or opt not avail */
#define	UNI_IE_CAUS_NI_BC	65	/* Bearer cap not implemented */
#define	UNI_IE_CAUS_COMB	73	/* Unsupported combination */
#define	UNI_IE_CAUS_CREF	81	/* Invalid call reference */
#define	UNI_IE_CAUS_CEXIST	82	/* Channel does not exist */
#define	UNI_IE_CAUS_IDEST	88	/* Incompatible destination */
#define	UNI_IE_CAUS_ENDPT	89	/* Invalid endpoint reference */
#define	UNI_IE_CAUS_TRNET	91	/* Invalid transit net */
#define	UNI_IE_CAUS_APPEND	92	/* Too many pending add party */
#define	UNI_IE_CAUS_UAAL	93	/* AAL parms can't be supp */
#define	UNI_IE_CAUS_MISSING	96	/* Mandatory IE missing */
#define	UNI_IE_CAUS_MTEXIST	97	/* Message type nonexistent */
#define	UNI_IE_CAUS_IEEXIST	99	/* IE type nonexistent */
#define	UNI_IE_CAUS_IECONTENT	100	/* IE content invalid */
#define	UNI_IE_CAUS_STATE	101	/* Message incomp with state */
#define	UNI_IE_CAUS_TIMER	102	/* Recovery on timer expire */
#define	UNI_IE_CAUS_LEN		104	/* Incorrect message length */
#define	UNI_IE_CAUS_PROTO	111	/* Protocol error */


/*
 * Connection identifier information element in internal format.
 */
struct ie_cnid {
	int8_t		ie_vp_sig;
	int8_t		ie_pref_excl;
	u_short		ie_vpci;
	u_short		ie_vci;
};

#define	UNI_IE_CNID_VPSIG_MASK	3
#define	UNI_IE_CNID_VPSIG_SHIFT	3
#define	UNI_IE_CNID_PREX_MASK	7

#define	UNI_IE_CNID_MIN_VCI	32


/*
 * Quality of service parameter information element in internal format.
 */
struct ie_qosp {
	int8_t		ie_fwd_class;
	int8_t		ie_bkwd_class;
};

#define	UNI_IE_QOSP_FWD_CLASS_0	0
#define	UNI_IE_QOSP_FWD_CLASS_1	1
#define	UNI_IE_QOSP_FWD_CLASS_2	2
#define	UNI_IE_QOSP_FWD_CLASS_3	3
#define	UNI_IE_QOSP_FWD_CLASS_4	4

#define	UNI_IE_QOSP_BKWD_CLASS_0	0
#define	UNI_IE_QOSP_BKWD_CLASS_1	1
#define	UNI_IE_QOSP_BKWD_CLASS_2	2
#define	UNI_IE_QOSP_BKWD_CLASS_3	3
#define	UNI_IE_QOSP_BKWD_CLASS_4	4


/*
 * Broadband repeat indicator information element in internal format.
 */
struct ie_brpi {
	int8_t		ie_ind;
};

#define	UNI_IE_BRPI_PRI_LIST	2
#define	UNI_IE_BRPI_IND_MASK	15


/*
 * Restart indicator information element in internal format.
 */
struct ie_rsti {
	int8_t		ie_class;
};

#define UNI_IE_RSTI_IND_VC	0
#define UNI_IE_RSTI_ALL_VC	2
#define UNI_IE_RSTI_CLASS_MASK	3


/*
 * Broadband locking shift information element in internal format.
 */
struct ie_blsh {
	int8_t		ie_dummy;
};


/*
 * Broadband non-locking shift information element in internal format.
 */
struct ie_bnsh {
	int8_t		ie_dummy;
};


/*
 * Broadband sending complete information element in internal format.
 */
struct ie_bsdc {
	int8_t		ie_ind;
};

#define	UNI_IE_BSDC_IND		0x21


/*
 * Transit net selection information element in internal format.
 */
struct ie_trnt {
	int8_t		ie_id_type;
	int8_t		ie_id_plan;
	u_char		ie_id_len;
	u_char		ie_id[4];
};

#define	UNI_IE_TRNT_IDT_MASK	7
#define	UNI_IE_TRNT_IDT_SHIFT	4
#define	UNI_IE_TRNT_IDP_MASK	15

#define	UNI_IE_TRNT_IDT_NATL	2
#define	UNI_IE_TRNT_IDP_CIC	1


/*
 * Endpoint reference information element in internal format.
 */
struct ie_eprf {
	int8_t		ie_type;
	int16_t		ie_id;
};

#define	UNI_IE_EPRF_LDI		0


/*
 * Endpoint state information element in internal format.
 */
struct ie_epst {
	int8_t		ie_state;
};

#define	UNI_IE_EPST_NULL	0
#define	UNI_IE_EPST_API		1
#define	UNI_IE_EPST_APR		6
#define	UNI_IE_EPST_DPI		11
#define	UNI_IE_EPST_DPR		12
#define	UNI_IE_EPST_ACTIVE	10
#define	UNI_IE_EPST_STATE_MASK	0x3F


/*
 * Generic information element
 */
struct ie_generic {
	struct ie_hdr		ie_hdr;
	union {
		struct ie_aalp		ie_aalp;
		struct ie_clrt		ie_clrt;
		struct ie_bbcp		ie_bbcp;
		struct ie_bhli		ie_bhli;
		struct ie_blli		ie_blli;
		struct ie_clst		ie_clst;
		struct ie_cdad		ie_cdad;
		struct ie_cdsa		ie_cdsa;
		struct ie_cgad		ie_cgad;
		struct ie_cgsa		ie_cgsa;
		struct ie_caus		ie_caus;
		struct ie_cnid		ie_cnid;
		struct ie_qosp		ie_qosp;
		struct ie_brpi		ie_brpi;
		struct ie_rsti		ie_rsti;
		struct ie_blsh		ie_blsh;
		struct ie_bnsh		ie_bnsh;
		struct ie_bsdc		ie_bsdc;
		struct ie_trnt		ie_trnt;
		struct ie_eprf		ie_eprf;
		struct ie_epst		ie_epst;
	} ie_u;
};

#define	ie_ident	ie_hdr.ie_hdr_ident
#define	ie_coding	ie_hdr.ie_hdr_coding
#define	ie_flag		ie_hdr.ie_hdr_flag
#define	ie_action	ie_hdr.ie_hdr_action
#define	ie_length	ie_hdr.ie_hdr_length
#define	ie_err_cause	ie_hdr.ie_hdr_err_cause
#define	ie_next		ie_hdr.ie_hdr_next

#define	ie_aalp_aal_type		ie_u.ie_aalp.ie_aal_type
#define	ie_aalp_1_subtype		ie_u.ie_aalp.aal_u.type_1.subtype
#define	ie_aalp_1_cbr_rate		ie_u.ie_aalp.aal_u.type_1.cbr_rate
#define	ie_aalp_1_multiplier		ie_u.ie_aalp.aal_u.type_1.multiplier
#define	ie_aalp_1_clock_recovery	ie_u.ie_aalp.aal_u.type_1.clock_recovery
#define	ie_aalp_1_error_correction	ie_u.ie_aalp.aal_u.type_1.error_correction
#define	ie_aalp_1_struct_data_tran	ie_u.ie_aalp.aal_u.type_1.struct_data_tran
#define	ie_aalp_1_partial_cells		ie_u.ie_aalp.aal_u.type_1.partial_cells

#define	ie_aalp_4_fwd_max_sdu		ie_u.ie_aalp.aal_u.type_4.fwd_max_sdu
#define	ie_aalp_4_bkwd_max_sdu		ie_u.ie_aalp.aal_u.type_4.bkwd_max_sdu
#define	ie_aalp_4_mid_range		ie_u.ie_aalp.aal_u.type_4.mid_range
#define	ie_aalp_4_mode			ie_u.ie_aalp.aal_u.type_4.mode
#define	ie_aalp_4_sscs_type		ie_u.ie_aalp.aal_u.type_4.sscs_type

#define	ie_aalp_5_fwd_max_sdu		ie_u.ie_aalp.aal_u.type_5.fwd_max_sdu
#define	ie_aalp_5_bkwd_max_sdu		ie_u.ie_aalp.aal_u.type_5.bkwd_max_sdu
#define	ie_aalp_5_mode			ie_u.ie_aalp.aal_u.type_5.mode
#define	ie_aalp_5_sscs_type		ie_u.ie_aalp.aal_u.type_5.sscs_type
#define	ie_aalp_user_info		ie_u.ie_aalp.aal_u.type_user.aal_info

#define	ie_clrt_fwd_peak	ie_u.ie_clrt.ie_fwd_peak
#define	ie_clrt_bkwd_peak	ie_u.ie_clrt.ie_bkwd_peak
#define	ie_clrt_fwd_peak_01	ie_u.ie_clrt.ie_fwd_peak_01
#define	ie_clrt_bkwd_peak_01	ie_u.ie_clrt.ie_bkwd_peak_01
#define	ie_clrt_fwd_sust	ie_u.ie_clrt.ie_fwd_sust
#define	ie_clrt_bkwd_sust	ie_u.ie_clrt.ie_bkwd_sust
#define	ie_clrt_fwd_sust_01	ie_u.ie_clrt.ie_fwd_sust_01
#define	ie_clrt_bkwd_sust_01	ie_u.ie_clrt.ie_bkwd_sust_01
#define	ie_clrt_fwd_burst	ie_u.ie_clrt.ie_fwd_burst
#define	ie_clrt_bkwd_burst	ie_u.ie_clrt.ie_bkwd_burst
#define	ie_clrt_fwd_burst_01	ie_u.ie_clrt.ie_fwd_burst_01
#define	ie_clrt_bkwd_burst_01	ie_u.ie_clrt.ie_bkwd_burst_01
#define	ie_clrt_best_effort	ie_u.ie_clrt.ie_best_effort
#define	ie_clrt_tm_options	ie_u.ie_clrt.ie_tm_options

#define	ie_bbcp_bearer_class	ie_u.ie_bbcp.ie_bearer_class
#define	ie_bbcp_traffic_type	ie_u.ie_bbcp.ie_traffic_type
#define	ie_bbcp_timing_req	ie_u.ie_bbcp.ie_timing_req
#define	ie_bbcp_clipping	ie_u.ie_bbcp.ie_clipping
#define	ie_bbcp_conn_config	ie_u.ie_bbcp.ie_conn_config

#define	ie_bhli_type		ie_u.ie_bhli.ie_type
#define	ie_bhli_info		ie_u.ie_bhli.ie_info

#define	ie_blli_l1_id		ie_u.ie_blli.ie_l1_id
#define	ie_blli_l2_id		ie_u.ie_blli.ie_l2_id
#define	ie_blli_l2_mode		ie_u.ie_blli.ie_l2_mode
#define	ie_blli_l2_q933_use	ie_u.ie_blli.ie_l2_q933_use
#define	ie_blli_l2_window	ie_u.ie_blli.ie_l2_window
#define	ie_blli_l2_user_proto	ie_u.ie_blli.ie_l2_user_proto
#define	ie_blli_l3_id		ie_u.ie_blli.ie_l3_id
#define	ie_blli_l3_mode		ie_u.ie_blli.ie_l3_mode
#define	ie_blli_l3_packet_size	ie_u.ie_blli.ie_l3_packet_size
#define	ie_blli_l3_window	ie_u.ie_blli.ie_l3_window
#define	ie_blli_l3_user_proto	ie_u.ie_blli.ie_l3_user_proto
#define	ie_blli_l3_ipi		ie_u.ie_blli.ie_l3_ipi
#define	ie_blli_l3_snap_id	ie_u.ie_blli.ie_l3_snap_id
#define	ie_blli_l3_oui		ie_u.ie_blli.ie_l3_oui
#define	ie_blli_l3_pid		ie_u.ie_blli.ie_l3_pid

#define	ie_clst_state		ie_u.ie_clst.ie_state

#define	ie_cdad_type		ie_u.ie_cdad.ie_type
#define	ie_cdad_plan		ie_u.ie_cdad.ie_plan
#define	ie_cdad_addr		ie_u.ie_cdad.ie_addr

#define	ie_cdsa_addr		ie_u.ie_cdsa.ie_addr

#define	ie_cgad_type		ie_u.ie_cgad.ie_type
#define	ie_cgad_plan		ie_u.ie_cgad.ie_plan
#define	ie_cgad_pres_ind	ie_u.ie_cgad.ie_pres_ind
#define	ie_cgad_screen_ind	ie_u.ie_cgad.ie_screen_ind
#define	ie_cgad_addr		ie_u.ie_cgad.ie_addr

#define	ie_cgsa_addr		ie_u.ie_cgsa.ie_addr

#define	ie_caus_loc		ie_u.ie_caus.ie_loc
#define	ie_caus_cause		ie_u.ie_caus.ie_cause
#define	ie_caus_diag_len	ie_u.ie_caus.ie_diag_len
#define	ie_caus_diagnostic	ie_u.ie_caus.ie_diagnostic

#define	ie_cnid_vp_sig		ie_u.ie_cnid.ie_vp_sig
#define	ie_cnid_pref_excl	ie_u.ie_cnid.ie_pref_excl
#define	ie_cnid_vpci		ie_u.ie_cnid.ie_vpci
#define	ie_cnid_vci		ie_u.ie_cnid.ie_vci

#define	ie_qosp_fwd_class	ie_u.ie_qosp.ie_fwd_class
#define	ie_qosp_bkwd_class	ie_u.ie_qosp.ie_bkwd_class

#define	ie_brpi_ind		ie_u.ie_brpi.ie_ind

#define	ie_rsti_class		ie_u.ie_rsti.ie_class

#define	ie_bsdc_ind		ie_u.ie_bsdc.ie_ind

#define	ie_trnt_id_type		ie_u.ie_trnt.ie_id_type
#define	ie_trnt_id_plan		ie_u.ie_trnt.ie_id_plan
#define	ie_trnt_id_len		ie_u.ie_trnt.ie_id_len
#define	ie_trnt_id		ie_u.ie_trnt.ie_id

#define	ie_eprf_type		ie_u.ie_eprf.ie_type
#define	ie_eprf_id		ie_u.ie_eprf.ie_id

#define	ie_epst_state		ie_u.ie_epst.ie_state

/*
 * Macro to add an IE to the end of a list of IEs
 */
#define	MSG_IE_ADD(m, i, ind) 					\
	if (m->msg_ie_vec[ind]) {				\
		struct ie_generic *_iep = msg->msg_ie_vec[ind];	\
		while (_iep->ie_next) {				\
			_iep = _iep->ie_next;			\
		}						\
		_iep->ie_next = i;				\
	} else {						\
		m->msg_ie_vec[ind] = i;				\
	}

#endif	/* _UNI_SIG_MSG_H */
