/*	$NetBSD: mii.h,v 1.1 1998/08/10 23:55:17 thorpej Exp $	*/
 
/*
 * Copyright (c) 1997 Manuel Bouyer.  All rights reserved.
 *
 * Modification to match BSD/OS 3.0 MII interface by Jason R. Thorpe,
 * Numerical Aerospace Simulation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Manuel Bouyer.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/mii/mii.h,v 1.2 1999/08/28 00:42:14 peter Exp $
 */

#ifndef _DEV_MII_MII_H_
#define	_DEV_MII_MII_H_

/*
 * Registers common to all PHYs.
 */

#define	MII_NPHY	32	/* max # of PHYs per MII */

/*
 * MII commands, used if a device must drive the MII lines
 * manually.
 */
#define	MII_COMMAND_START	0x01
#define	MII_COMMAND_READ	0x02
#define	MII_COMMAND_WRITE	0x01
#define	MII_COMMAND_ACK		0x02

#define	MII_BMCR	0x00 	/* Basic mode control register (rw) */
#define	BMCR_RESET	0x8000	/* reset */
#define	BMCR_LOOP	0x4000	/* loopback */
#define	BMCR_S100	0x2000	/* speed (10/100) select */
#define	BMCR_AUTOEN	0x1000	/* autonegotiation enable */
#define	BMCR_PDOWN	0x0800	/* power down */
#define	BMCR_ISO	0x0400	/* isolate */
#define	BMCR_STARTNEG	0x0200	/* restart autonegotiation */
#define	BMCR_FDX	0x0100	/* Set duplex mode */
#define	BMCR_CTEST	0x0080	/* collision test */

#define	MII_BMSR	0x01	/* Basic mode status register (ro) */
#define	BMSR_100T4	0x8000	/* 100 base T4 capable */
#define	BMSR_100TXFDX	0x4000	/* 100 base Tx full duplex capable */
#define	BMSR_100TXHDX	0x2000	/* 100 base Tx half duplex capable */
#define	BMSR_10TFDX	0x1000	/* 10 base T full duplex capable */
#define	BMSR_10THDX	0x0800	/* 10 base T half duplex capable */
#define	BMSR_ACOMP	0x0020	/* Autonegotiation complete */
#define	BMSR_RFAULT	0x0010	/* Link partner fault */
#define	BMSR_ANEG	0x0008	/* Autonegotiation capable */
#define	BMSR_LINK	0x0004	/* Link status */
#define	BMSR_JABBER	0x0002	/* Jabber detected */
#define	BMSR_EXT	0x0001	/* Extended capability */

#define	BMSR_MEDIAMASK	(BMSR_100T4|BMSR_100TXFDX|BMSR_100TXHDX|BMSR_10TFDX| \
			 BMSR_10THDX|BMSR_ANEG)

/*
 * Convert BMSR media capabilities to ANAR bits for autonegotiation.
 * Note the shift chopps off the BMSR_ANEG bit.
 */
#define	BMSR_MEDIA_TO_ANAR(x)	(((x) & BMSR_MEDIAMASK) >> 6)

#define	MII_PHYIDR1	0x02	/* ID register 1 (ro) */

#define	MII_PHYIDR2	0x03	/* ID register 2 (ro) */
#define	IDR2_OUILSB	0xfc00	/* OUI LSB */
#define	IDR2_MODEL	0x03f0	/* vendor model */
#define	IDR2_REV	0x000f	/* vendor revision */

#define	MII_OUI(id1, id2)	(((id1) << 6) | ((id2) >> 10))
#define	MII_MODEL(id2)		(((id2) & IDR2_MODEL) >> 4)
#define	MII_REV(id2)		((id2) & IDR2_REV)

#define	MII_ANAR	0x04	/* Autonegotiation advertisement (rw) */
#define ANAR_NP		0x8000	/* Next page (ro) */
#define	ANAR_ACK	0x4000	/* link partner abilities acknowledged (ro) */
#define ANAR_RF		0x2000	/* remote fault (ro) */
#define ANAR_T4		0x0200	/* local device supports 100bT4 */
#define ANAR_TX_FD	0x0100	/* local device supports 100bTx FD */
#define ANAR_TX		0x0080	/* local device supports 100bTx */
#define ANAR_10_FD	0x0040	/* local device supports 10bT FD */
#define ANAR_10		0x0020	/* local device supports 10bT */
#define	ANAR_CSMA	0x0001	/* protocol selector CSMA/CD */

#define	MII_ANLPAR	0x05	/* Autonegotiation lnk partner abilities (rw) */
#define ANLPAR_NP	0x8000	/* Next page (ro) */
#define	ANLPAR_ACK	0x4000	/* link partner accepted ACK (ro) */
#define ANLPAR_RF	0x2000	/* remote fault (ro) */
#define ANLPAR_T4	0x0200	/* link partner supports 100bT4 */
#define ANLPAR_TX_FD	0x0100	/* link partner supports 100bTx FD */
#define ANLPAR_TX	0x0080	/* link partner supports 100bTx */
#define ANLPAR_10_FD	0x0040	/* link partner supports 10bT FD */
#define ANLPAR_10	0x0020	/* link partner supports 10bT */
#define	ANLPAR_CSMA	0x0001	/* protocol selector CSMA/CD */

#define	MII_ANER	0x06	/* Autonegotiation expansion (ro) */
#define ANER_MLF	0x0010	/* multiple link detection fault */
#define ANER_LPNP	0x0008	/* link parter next page-able */
#define ANER_NP		0x0004	/* next page-able */
#define ANER_PAGE_RX	0x0002	/* Page received */
#define ANER_LPAN	0x0001	/* link parter autoneg-able */

#endif /* _DEV_MII_MII_H_ */
