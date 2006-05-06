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
 *	@(#) $FreeBSD: src/sys/netatm/ipatm/ipatm.h,v 1.3 2005/01/07 01:45:37 imp Exp $
 *
 */

/*
 * IP Over ATM Support
 * -------------------
 *
 * Protocol definitions
 *
 */

#ifndef _IPATM_IPATM_H
#define _IPATM_IPATM_H

/*
 * Protocol Variables
 */
#define	IPATM_VCIDLE		15		/* VCC idle time (minutes) */
#define	IPATM_ARP_TIME		(60 * ATM_HZ)	/* Wait for ARP answer */
#define	IPATM_SVC_TIME		(60 * ATM_HZ)	/* Wait for SVC open answer */
#define	IPATM_IDLE_TIME		(60 * ATM_HZ)	/* VCC idle timer tick */

/*
 * IP/ATM LLC/SNAP header
 */
#define	IPATM_LLC_LEN		8
#define	IPATM_LLC_HDR		{0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00}

#endif	/* _IPATM_IPATM_H */
