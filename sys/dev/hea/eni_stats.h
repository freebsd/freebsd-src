/*
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
 * Efficient ENI Adapter Support
 * -----------------------------
 *
 * Defines for statistics
 *
 */

#ifndef	_ENI_ENI_STATS_H
#define	_ENI_ENI_STATS_H

struct eni_stats_oc3 {
	u_long		oc3_sect_bip8;	/* Section 8-bit intrlv parity errors */
	u_long		oc3_path_bip8;	/* Path 8-bit intrlv parity errors */
	u_long		oc3_line_bip24;	/* Line 24-bit intrlv parity errors */
	u_long		oc3_line_febe;	/* Line far-end block errors */
	u_long		oc3_path_febe;	/* Path far-end block errors */
	u_long		oc3_hec_corr;	/* Correctable HEC errors */
	u_long		oc3_hec_uncorr;	/* Uncorrectable HEC errors */
	u_long		oc3_pad;	/* Pad to quad-word boundary */
};
typedef	struct eni_stats_oc3	Eni_Stats_oc3;

struct eni_stats_atm {
	u_long		atm_xmit;	/* Cells transmitted */
	u_long		atm_rcvd;	/* Cells received */
	u_long		atm_pad[2];	/* Pad to quad-word boundary */
};
typedef	struct eni_stats_atm	Eni_Stats_atm;

struct eni_stats_aal0 {
	u_long		aal0_xmit;	/* Cells transmitted */
	u_long		aal0_rcvd;	/* Cells received */
	u_long		aal0_drops;	/* Cells dropped */
	u_long		aal0_pad;	/* Pad to quad-word boundary */
};
typedef	struct eni_stats_aal0	Eni_Stats_aal0;

struct eni_stats_aal5 {
	u_long		aal5_xmit;	/* Cells transmitted */
	u_long		aal5_rcvd;	/* Cells received */
	u_long		aal5_crc_len;	/* Cells with CRC/length errors */
	u_long		aal5_drops;	/* Cell drops */
	u_long		aal5_pdu_xmit;	/* CS PDUs transmitted */
	u_long		aal5_pdu_rcvd;	/* CS PDUs received */
	u_long		aal5_pdu_crc;	/* CS PDUs with CRC errors */
	u_long		aal5_pdu_errs;	/* CS layer protocol errors */
	u_long		aal5_pdu_drops;	/* CS PDUs dropped */
	u_long		aal5_pad[3];	/* Pad to quad-word boundary */
};
typedef	struct eni_stats_aal5	Eni_Stats_aal5;

struct eni_stats_driver {
	/*
	 * Adapter memory allocator stats
	 */
	u_long		drv_mm_toobig;	/* Size larger then adapter supports */
	u_long		drv_mm_nodesc;	/* No memory area descriptor avail */
	u_long		drv_mm_nobuf;	/* No memory buffer available */
	u_long		drv_mm_notuse;	/* Calling free() on free buffer */
	u_long		drv_mm_notfnd;	/* Couldn't find descr for free() */

	/*
	 * VCM sats
	 */
	u_long		drv_vc_maxpdu;	/* Requested PDU size too large */
	u_long		drv_vc_badrng;	/* VPI and/or VCI too large */

	/*
	 * Receive stats
	 */
	u_long		drv_rv_norsc;	/* No buffer for resource pointers */
	u_long		drv_rv_nobufs;	/* No buffers for PDU */
	u_long		drv_rv_nodma;	/* No room in RXDMA list */
	u_long		drv_rv_rxq;	/* No room in local rxqueue */
	u_long		drv_rv_novcc;	/* Draining PDU on closed VCC */
	u_long		drv_rv_intrq;	/* No room in atm_intrq */
	u_long		drv_rv_null;	/* Trying to pass null PDU up stack */
	u_long		drv_rv_segdma;	/* No DMA address */

	/*
	 * Transmit stats
	 */
	u_long		drv_xm_segdma;	/* No DMA address */
	u_long		drv_xm_segnoal;	/* Non-aligned segment */
	u_long		drv_xm_seglen;	/* Padded length segment */
	u_long		drv_xm_maxpdu;	/* Too many segments - dropped */
	u_long		drv_xm_nobuf;	/* No space in TX buffer - dropped */
	u_long		drv_xm_norsc;	/* No buffers for resource pointers */
	u_long		drv_xm_nodma;	/* No space in TXDMA list */
	u_long		drv_xm_dmaovfl;	/* DMA overflow */

};
typedef struct eni_stats_driver Eni_Stats_drv;

struct	eni_stats {
	Eni_Stats_oc3	eni_st_oc3;	/* OC3 layer stats */
	Eni_Stats_atm	eni_st_atm;	/* ATM layer stats */
	Eni_Stats_aal0	eni_st_aal0;	/* AAL0 layer stats */
	Eni_Stats_aal5	eni_st_aal5;	/* AAL5 layer stats */
	Eni_Stats_drv	eni_st_drv;	/* Driver stats */
};
typedef	struct eni_stats	Eni_stats;

#endif	/* _ENI_ENI_STATS_H */
