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
 *	@(#) $FreeBSD: src/sys/dev/hfa/fore_stats.h,v 1.2 1999/08/28 00:41:52 peter Exp $
 *
 */

/*
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * Driver statistics definitions
 *
 */

#ifndef _FORE_STATS_H
#define _FORE_STATS_H


/*
 * Fore Driver Statistics
 */
struct Stats_driver {
	u_long		drv_xm_notact;	/* PDU drops out - VCC not active */
	u_long		drv_xm_full;	/* Xmit queue full */
	u_long		drv_xm_maxpdu;	/* PDU drops out - max segment/size */
	u_long		drv_xm_segnoal;	/* Non-aligned segments */
	u_long		drv_xm_seglen;	/* Padded length segments */
	u_long		drv_xm_segdma;	/* PDU drops out - no dma address */
	u_long		drv_rv_novcc;	/* PDU drops in - no VCC */
	u_long		drv_rv_nosbf;	/* No small buffers */
	u_long		drv_rv_nomb;	/* PDU drops in - no buffer */
	u_long		drv_rv_ifull;	/* PDU drops in - intr queue full */
	u_long		drv_bf_segdma;	/* Buffer supply - no dma address */
	u_long		drv_cm_full;	/* Command queue full */
	u_long		drv_cm_nodma;	/* Command failed - no dma address */
};
typedef struct Stats_driver	Stats_driver;


/*
 * Fore Device Statistics
 *
 * This structure is used by pass all statistics (including CP maintained 
 * and driver maintained) data to user space (atm command).
 */
struct fore_stats {
	Fore_cp_stats	st_cpstat;	/* CP stats */
	Stats_driver	st_drv;		/* Driver maintained stats */
};
typedef struct fore_stats	Fore_stats;

#define	st_taxi		st_cpstat.st_cp_taxi
#define	st_oc3		st_cpstat.st_cp_oc3
#define	st_atm		st_cpstat.st_cp_atm
#define	st_aal0		st_cpstat.st_cp_aal0
#define	st_aal4		st_cpstat.st_cp_aal4
#define	st_aal5		st_cpstat.st_cp_aal5
#define	st_misc		st_cpstat.st_cp_misc

#endif	/* _FORE_STATS_H */
