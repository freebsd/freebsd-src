/*
 * Copyright (c) 2001 Wind River Systems
 * Copyright (c) 2001
 *	Bill Paul <wpaul@bsdi.com>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _DEV_MII_NSGPHYREG_H_
#define	_DEV_MII_NSGPHYREG_H_

/*
 * NatSemi DP83891 registers
 */

#define NSGPHY_MII_BMCR		0x00
#define NSGPHY_BMCR_RESET	0x8000
#define NSGPHY_BMCR_LOOP	0x4000
#define NSGPHY_BMCR_SPD0	0x2000	/* speed select, lower bit */
#define NSGPHY_BMCR_AUTOEN	0x1000	/* Autoneg enabled */
#define NSGPHY_BMCR_PDOWN	0x0800	/* Power down */
#define NSGPHY_BMCR_ISO		0x0400	/* Isolate */
#define NSGPHY_BMCR_STARTNEG	0x0200	/* Restart autoneg */
#define NSGPHY_BMCR_FDX		0x0100	/* Duplex mode */
#define NSGPHY_BMCR_CTEST	0x0080	/* Collision test enable */
#define NSGPHY_BMCR_SPD1	0x0040	/* Speed select, upper bit */

#define NSGPHY_S1000		NSGPHY_BMCR_SPD1	/* 1000mbps */
#define NSGPHY_S100		NSGPHY_BMCR_SPD0	/* 100mpbs */
#define NSGPHY_S10		0			/* 10mbps */

#define NSGPHY_MII_BMSR		0x01
#define NSGPHY_BMSR_100BT4	0x8000	/* 100baseT4 support */
#define NSGPHY_BMSR_100FDX	0x4000	/* 100baseTX full duplex */
#define NSGPHY_BMSR_100HDX	0x2000	/* 100baseTX half duplex */
#define NSGPHY_BMSR_10FDX	0x1000	/* 10baseT full duplex */
#define NSGPHY_BMSR_10HDX	0x0800	/* 10baseT half duplex */
#define NSGPHY_BMSR_100T2FDX	0x0400	/* 100baseT2 full duplex */
#define NSGPHY_BMSR_100T2HDX	0x0200	/* 100baseT2 full duplex */
#define NSGPHY_BMSR_EXTSTS	0x0100	/* 1000baseT Extended status present */
#define NSGPHY_BMSR_PRESUB	0x0040	/* Preamble surpression */
#define NSGPHY_BMSR_ACOMP	0x0020	/* Autoneg complete */
#define NSGPHY_BMSR_RFAULT	0x0010	/* Remote fault condition occured */
#define NSGPHY_BMSR_ANEG	0x0008	/* Autoneg capable */
#define NSGPHY_BMSR_LINK	0x0004	/* Link status */
#define NSGPHY_BMSR_JABBER	0x0002	/* Jabber detected */
#define NSGPHY_BMSR_EXT		0x0001	/* Extended capability */

#define NSGPHY_MII_ANAR		0x04
#define NSGPHY_ANAR_NP		0x8000	/* Next page */
#define NSGPHY_ANAR_RF		0x2000	/* Remote fault */
#define NSGPHY_ANAR_ASP		0x0800	/* Asymetric Pause */
#define NSGPHY_ANAR_PC		0x0400	/* Pause capable */
#define NSGPHY_ANAR_100T4	0x0200	/* 100baseT4 support */
#define NSGPHY_ANAR_100FDX	0x0100	/* 100baseTX full duplex support */
#define NSGPHY_ANAR_100HDX	0x0080	/* 100baseTX half duplex support */
#define NSGPHY_ANAR_10FDX	0x0040	/* 10baseT full duplex support */
#define NSGPHY_ANAR_10HDX	0x0020	/* 10baseT half duplex support */
#define NSGPHY_ANAR_SEL		0x001F	/* selector field, 00001=Ethernet */

#define NSGPHY_MII_ANLPAR	0x05
#define NSGPHY_ANLPAR_NP	0x8000	/* Next page */
#define NSGPHY_ANLPAR_RF	0x2000	/* Remote fault */
#define NSGPHY_ANLPAR_ASP	0x0800	/* Asymetric Pause */
#define NSGPHY_ANLPAR_PC	0x0400	/* Pause capable */
#define NSGPHY_ANLPAR_100T4	0x0200	/* 100baseT4 support */
#define NSGPHY_ANLPAR_100FDX	0x0100	/* 100baseTX full duplex support */
#define NSGPHY_ANLPAR_100HDX	0x0080	/* 100baseTX half duplex support */
#define NSGPHY_ANLPAR_10FDX	0x0040	/* 10baseT full duplex support */
#define NSGPHY_ANLPAR_10HDX	0x0020	/* 10baseT half duplex support */
#define NSGPHY_ANLPAR_SEL	0x001F	/* selector field, 00001=Ethernet */

#define NSGPHY_SEL_TYPE		0x0001	/* ethernet */

#define NSGPHY_MII_ANER		0x06
#define NSGPHY_ANER_PDF		0x0010	/* Parallel detection fault */
#define NSGPHY_ANER_LPNP	0x0008	/* Link partner can next page */
#define NSGPHY_ANER_NP		0x0004	/* Local PHY can next page */
#define NSGPHY_ANER_RX		0x0002	/* Next page received */
#define NSGPHY_ANER_LPAN	0x0001 	/* Link partner autoneg capable */

#define NSGPHY_MII_NEXTP	0x07	/* Next page */
#define NSGPHY_NEXTP_NP		0x8000	/* Next page indication */
#define NSGPHY_NEXTP_MP		0x2000	/* Message page */
#define NSGPHY_NEXTP_ACK2	0x1000	/* Acknowledge 2 */
#define NSGPHY_NEXTP_TOGGLE	0x0800	/* Toggle */
#define NSGPHY_NEXTP_CODE	0x07FF	/* Code field */

#define NSGPHY_MII_NEXTP_LP	0x08	/* Next page of link partner */
#define NSGPHY_NEXTPLP_NP	0x8000	/* Next page indication */
#define NSGPHY_NEXTPLP_MP	0x2000	/* Message page */
#define NSGPHY_NEXTPLP_ACK2	0x1000	/* Acknowledge 2 */
#define NSGPHY_NEXTPLP_TOGGLE	0x0800	/* Toggle */
#define NSGPHY_NEXTPLP_CODE	0x07FF	/* Code field */

#define NSGPHY_MII_1000CTL	0x09	/* 1000baseT control */
#define NSGPHY_1000CTL_TST	0xE000	/* test modes */
#define NSGPHY_1000CTL_MSE	0x1000	/* Master/Slave config enable */
#define NSGPHY_1000CTL_MSC	0x0800	/* Master/Slave setting */
#define NSGPHY_1000CTL_RD	0x0400	/* Port type: Repeater/DTE */
#define NSGPHY_1000CTL_AFD	0x0200	/* Advertise full duplex */
#define NSGPHY_1000CTL_AHD	0x0100	/* Advertise half duplex */

#define NSGPHY_MII_1000STS	0x0A	/* 1000baseT status */
#define NSGPHY_1000STS_MSF	0x8000	/* Master/slave fault */
#define NSGPHY_1000STS_MSR	0x4000	/* Master/slave result */
#define NSGPHY_1000STS_LRS	0x2000	/* Local receiver status */
#define NSGPHY_1000STS_RRS	0x1000	/* Remote receiver status */
#define NSGPHY_1000STS_LPFD	0x0800	/* Link partner can FD */
#define NSGPHY_1000STS_LPHD	0x0400	/* Link partner can HD */
#define NSGPHY_1000STS_ASM_DIR	0x0200	/* Asymetric pause capable */
#define NSGPHY_1000STS_IEC	0x00FF	/* Idle error count */

#define NSGPHY_MII_EXTSTS	0x0F	/* Extended status */
#define NSGPHY_EXTSTS_X_FD_CAP	0x8000	/* 1000base-X FD capable */
#define NSGPHY_EXTSTS_X_HD_CAP	0x4000	/* 1000base-X HD capable */
#define NSGPHY_EXTSTS_T_FD_CAP	0x2000	/* 1000base-T FD capable */
#define NSGPHY_EXTSTS_T_HD_CAP	0x1000	/* 1000base-T HD capable */

#define NSGPHY_MII_STRAPOPT	0x10	/* Strap options */
#define NSGPHY_STRAPOPT_PHYADDR	0xF800	/* PHY address */
#define NSGPHY_STRAPOPT_COMPAT	0x0400	/* Broadcom compat mode */
#define NSGPHY_STRAPOPT_MMSE	0x0200	/* Manual master/slave enable */
#define NSGPHY_STRAPOPT_ANEG	0x0100	/* Autoneg enable */
#define NSGPHY_STRAPOPT_MMSV	0x0080	/* Manual master/slave setting */
#define NSGPHY_STRAPOPT_1000HDX	0x0010	/* Advertise 1000 half-duplex */
#define NSGPHY_STRAPOPT_1000FDX	0x0008	/* Advertise 1000 full-duplex */
#define NSGPHY_STRAPOPT_100_ADV	0x0004	/* Advertise 100 full/half-duplex */
#define NSGPHY_STRAPOPT_SPDSEL	0x0003	/* speed selection */

#define NSGPHY_MII_PHYSUP	0x11	/* PHY support/current status */
#define NSGPHY_PHYSUP_SPDSTS	0x0018	/* speed status */
#define NSGPHY_PHYSUP_LNKSTS	0x0004	/* link status */
#define NSGPHY_PHYSUP_DUPSTS	0x0002	/* duplex status 1 == full */
#define NSGPHY_PHYSUP_10BT	0x0001	/* 10baseT resolved */

#define NSGPHY_SPDSTS_1000	0x0010
#define NSGPHY_SPDSTS_100	0x0008
#define NSGPHY_SPDSTS_10	0x0000

#endif /* _DEV_NSGPHY_MIIREG_H_ */
