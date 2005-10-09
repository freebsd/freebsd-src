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
 *	@(#) $FreeBSD$
 *
 */

/*
 * ATM Forum UNI Support
 * ---------------------
 *
 * SSCOP Protocol Data Unit (PDU) definitions
 *
 */

#ifndef _UNI_SSCOP_PDU_H
#define _UNI_SSCOP_PDU_H

/*
 * SSCOP PDU Constants
 */
#define	PDU_MIN_LEN	4		/* Minimum PDU length */
#define	PDU_LEN_MASK	3		/* PDU length must be 32-bit aligned */
#define	PDU_ADDR_MASK	3		/* PDUs must be 32-bit aligned */
#define	PDU_SEQ_MASK	0x00ffffff	/* Mask for 24-bit sequence values */
#define	PDU_MAX_INFO	65528		/* Maximum length of PDU info field */
#define	PDU_MAX_UU	65524		/* Maximum length of SSCOP-UU field */
#define	PDU_MAX_STAT	65520		/* Maximum length of STAT list */
#define	PDU_MAX_ELEM	67		/* Maximum elements sent in STAT */
#define	PDU_PAD_ALIGN	4		/* I-field padding alignment */


/*
 * PDU Queueing Header
 *
 * There will be a queueing header tacked on to the front of each
 * buffer chain that is placed on any of the sscop SD PDU queues (not
 * including the SD transmission queue).  Note that this header will
 * not be included in the buffer data length/offset fields.
 */
struct pdu_hdr {
	union {
		struct pdu_hdr	*phu_pack_lk;	/* Pending ack queue link */
		struct pdu_hdr	*phu_recv_lk;	/* Receive queue link */
	} ph_u;
	struct pdu_hdr	*ph_rexmit_lk;	/* Retranmit queue link */
	sscop_seq	ph_ns;		/* SD.N(S) - SD's sequence number */
	sscop_seq	ph_nps;		/* SD.N(PS) - SD's poll sequence */
	KBuffer		*ph_buf;	/* Pointer to containing buffer */
};
#define	ph_pack_lk	ph_u.phu_pack_lk
#define	ph_recv_lk	ph_u.phu_recv_lk


/*
 * SSCOP PDU formats
 *
 * N.B. - all SSCOP PDUs are trailer oriented (don't ask me...)
 */

/*
 * PDU Type Fields
 */
#define	PT_PAD_MASK	0xc0		/* Pad length mask */
#define	PT_PAD_SHIFT	6		/* Pad byte shift count */
#define	PT_SOURCE_SSCOP	0x10		/* Source = SSCOP */
#define	PT_TYPE_MASK	0x0f		/* Type mask */
#define	PT_TYPE_MAX	0x0f		/* Maximum pdu type */
#define	PT_TYPE_SHIFT	24		/* Type word shift count */

#define	PT_BGN		0x01		/* Begin */
#define	PT_BGAK		0x02		/* Begin Acknowledge */
#define	PT_BGREJ	0x07		/* Begin Reject */
#define	PT_END		0x03		/* End */
#define	PT_ENDAK	0x04		/* End Acknowledge */
#define	PT_RS		0x05		/* Resynchronization */
#define	PT_RSAK		0x06		/* Resynchronization Acknowledge */
#define	PT_ER		0x09		/* Error Recovery */
#define	PT_ERAK		0x0f		/* Error Recovery Acknowledge */
#define	PT_SD		0x08		/* Sequenced Data */
#define	PT_SDP		0x09		/* Sequenced Data with Poll */
#define	PT_POLL		0x0a		/* Status Request */
#define	PT_STAT		0x0b		/* Solicited Status Response */
#define	PT_USTAT	0x0c		/* Unsolicited Status Response */
#define	PT_UD		0x0d		/* Unnumbered Data */
#define	PT_MD		0x0e		/* Management Data */

/*
 * Begin PDU
 */
struct bgn_pdu {
	u_char		bgn_rsvd[3];	/* Reserved */
	u_char		bgn_nsq;	/* N(SQ) */
	union {
		u_char		bgnu_type;	/* PDU type, etc */
		sscop_seq	bgnu_nmr;	/* N(MR) */
	} bgn_u;
};
#define	bgn_type	bgn_u.bgnu_type
#define	bgn_nmr		bgn_u.bgnu_nmr

/*
 * Begin Acknowledge PDU
 */
struct bgak_pdu {
	int		bgak_rsvd;	/* Reserved */
	union {
		u_char		bgaku_type;	/* PDU type, etc */
		sscop_seq	bgaku_nmr;	/* N(MR) */
	} bgak_u;
};
#define	bgak_type	bgak_u.bgaku_type
#define	bgak_nmr	bgak_u.bgaku_nmr

/*
 * Begin Reject PDU
 */
struct bgrej_pdu {
	int		bgrej_rsvd2;	/* Reserved */
	u_char		bgrej_type;	/* PDU type, etc */
	u_char		bgrej_rsvd1[3];	/* Reserved */
};

/*
 * End PDU
 */
struct end_pdu {
	int		end_rsvd2;	/* Reserved */
	u_char		end_type;	/* PDU type, etc */
	u_char		end_rsvd1[3];	/* Reserved */
};

/*
 * End Acknowledge PDU (Q.2110)
 */
struct endak_q2110_pdu {
	int		endak_rsvd2;	/* Reserved */
	u_char		endak_type;	/* PDU type, etc */
	u_char		endak_rsvd1[3];	/* Reserved */
};

/*
 * End Acknowledge PDU (Q.SAAL)
 */
struct endak_qsaal_pdu {
	u_char		endak_type;	/* PDU type, etc */
	u_char		endak_rsvd[3];	/* Reserved */
};

/*
 * Resynchronization PDU
 */
struct rs_pdu {
	char		rs_rsvd[3];	/* Reserved */
	u_char		rs_nsq;		/* N(SQ) */
	union {
		u_char		rsu_type;	/* PDU type, etc */
		sscop_seq	rsu_nmr;	/* N(MR) */
	} rs_u;
};
#define	rs_type		rs_u.rsu_type
#define	rs_nmr		rs_u.rsu_nmr

/*
 * Resynchronization Acknowledge PDU (Q.2110)
 */
struct rsak_q2110_pdu {
	int		rsak_rsvd;	/* Reserved */
	union {
		u_char		rsaku_type;	/* PDU type, etc */
		sscop_seq	rsaku_nmr;	/* N(MR) */
	} rsak_u;
};
#define	rsak2_type	rsak_u.rsaku_type
#define	rsak_nmr	rsak_u.rsaku_nmr

/*
 * Resynchronization Acknowledge PDU (Q.SAAL)
 */
struct rsak_qsaal_pdu {
	u_char		rsaks_type;	/* PDU type, etc */
	u_char		rsak_rsvd[3];	/* Reserved */
};

/*
 * Error Recovery PDU
 */
struct er_pdu {
	char		er_rsvd[3];	/* Reserved */
	u_char		er_nsq;		/* N(SQ) */
	union {
		u_char		eru_type;	/* PDU type, etc */
		sscop_seq	eru_nmr;	/* N(MR) */
	} er_u;
};
#define	er_type		er_u.eru_type
#define	er_nmr		er_u.eru_nmr

/*
 * Error Recovery Acknowledge PDU
 */
struct erak_pdu {
	int		erak_rsvd;	/* Reserved */
	union {
		u_char		eraku_type;	/* PDU type, etc */
		sscop_seq	eraku_nmr;	/* N(MR) */
	} erak_u;
};
#define	erak_type	erak_u.eraku_type
#define	erak_nmr	erak_u.eraku_nmr

/*
 * Sequenced Data PDU
 */
struct sd_pdu {
	union {
		u_char		sdu_type;	/* PDU type, etc */
		sscop_seq	sdu_ns;		/* N(S) */
	} sd_u;
};
#define	sd_type		sd_u.sdu_type
#define	sd_ns		sd_u.sdu_ns

/*
 * Sequenced Data with Poll PDU
 */
struct sdp_pdu {
	sscop_seq	sdp_nps;	/* N(PS) */
	union {
		u_char		sdpu_type;	/* PDU type, etc */
		sscop_seq	sdpu_ns;	/* N(S) */
	} sdp_u;
};
#define	sdp_type	sdp_u.sdpu_type
#define	sdp_ns		sdp_u.sdpu_ns

/*
 * Poll PDU
 */
struct poll_pdu {
	sscop_seq	poll_nps;	/* N(PS) */
	union {
		u_char		pollu_type;	/* PDU type, etc */
		sscop_seq	pollu_ns;	/* N(S) */
	} poll_u;
};
#define	poll_type	poll_u.pollu_type
#define	poll_ns		poll_u.pollu_ns

/*
 * Solicited Status PDU
 */
struct stat_pdu {
	sscop_seq	stat_nps;	/* N(PS) */
	sscop_seq	stat_nmr;	/* N(MR) */
	union {
		u_char		statu_type;	/* PDU type, etc */
		sscop_seq	statu_nr;	/* N(R) */
	} stat_u;
};
#define	stat_type	stat_u.statu_type
#define	stat_nr		stat_u.statu_nr

/*
 * Unsolicited Status PDU
 */
struct ustat_pdu {
	sscop_seq	ustat_le1;	/* List element 1 */
	sscop_seq	ustat_le2;	/* List element 2 */
	sscop_seq	ustat_nmr;	/* N(MR) */
	union {
		u_char		ustatu_type;	/* PDU type, etc */
		sscop_seq	ustatu_nr;	/* N(R) */
	} ustat_u;
};
#define	ustat_type	ustat_u.ustatu_type
#define	ustat_nr	ustat_u.ustatu_nr

/*
 * Unit Data PDU
 */
struct ud_pdu {
	u_char		ud_type;	/* PDU type, etc */
	u_char		ud_rsvd[3];	/* Reserved */
};

/*
 * Management Data PDU
 */
struct md_pdu {
	u_char		md_type;	/* PDU type, etc */
	u_char		md_rsvd[3];	/* Reserved */
};

#endif	/* _UNI_SSCOP_PDU_H */
