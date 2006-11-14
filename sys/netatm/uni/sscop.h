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
 * SSCOP protocol definitions
 *
 */

#ifndef _UNI_SSCOP_H
#define _UNI_SSCOP_H

/*
 * SSCOP Version
 */
enum sscop_vers {
	SSCOP_VERS_QSAAL,			/* Version = Q.SAAL1 */
	SSCOP_VERS_Q2110			/* Version = Q.2110 */
};


/*
 * SSCOP API definitions
 */
#define	SSCOP_UU_NULL		0		/* User-to-User Info = null */
#define	SSCOP_RN_TOTAL		-1		/* Retrieval Number = Total */
#define	SSCOP_RN_UNKNOWN	-2		/* Retrieval Number = Unknown */
#define	SSCOP_BR_YES		1		/* Buffer Release = Yes */
#define	SSCOP_BR_NO		2		/* Buffer Release = No */
#define	SSCOP_SOURCE_SSCOP	1		/* Source = SSCOP */
#define	SSCOP_SOURCE_USER	2		/* Source = User */
#define	SSCOP_SOURCE_LAST	3		/* Source = from last END */


/*
 * Connection parameters for an SSCOP entity.
 * Passed via an SSCOP_INIT stack call argument.
 */
struct sscop_parms {
	u_short		sp_maxinfo;	/* k - max information field size */
	u_short		sp_maxuu;	/* j - max SSCOP-UU field size */
	short		sp_maxcc;	/* MaxCC - max value of VT(CC) */
	short		sp_maxpd;	/* MaxPD - max value of VT(PD) */
	u_short		sp_timecc;	/* Timer_CC value (ticks) */
	u_short		sp_timekeep;	/* Timer_KEEPALIVE value (ticks) */
	u_short		sp_timeresp;	/* Timer_NO-RESPONSE value (ticks) */
	u_short		sp_timepoll;	/* Timer_POLL value (ticks) */
	u_short		sp_timeidle;	/* Timer_IDLE value (ticks) */
	short		sp_rcvwin;	/* Receiver window size */
};

#endif	/* _UNI_SSCOP_H */
