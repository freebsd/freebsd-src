/*
 * Copyright (c) 2000
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
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

#ifndef _DEV_MII_BRGPHYREG_H_
#define	_DEV_MII_BRGPHYREG_H_

/*
 * Broadcom BCM5400 registers
 */

#define BRGPHY_MII_BMCR		0x00
#define BRGPHY_BMCR_RESET	0x8000
#define BRGPHY_BMCR_LOOP	0x4000
#define BRGPHY_BMCR_SPD0	0x2000	/* speed select, lower bit */
#define BRGPHY_BMCR_AUTOEN	0x1000	/* Autoneg enabled */
#define BRGPHY_BMCR_PDOWN	0x0800	/* Power down */
#define BRGPHY_BMCR_ISO		0x0400	/* Isolate */
#define BRGPHY_BMCR_STARTNEG	0x0200	/* Restart autoneg */
#define BRGPHY_BMCR_FDX		0x0100	/* Duplex mode */
#define BRGPHY_BMCR_CTEST	0x0080	/* Collision test enable */
#define BRGPHY_BMCR_SPD1	0x0040	/* Speed select, upper bit */

#define BRGPHY_S1000		BRGPHY_BMCR_SPD1	/* 1000mbps */
#define BRGPHY_S100		BRGPHY_BMCR_SPD0	/* 100mpbs */
#define BRGPHY_S10		0			/* 10mbps */

#define BRGPHY_MII_BMSR		0x01
#define BRGPHY_BMSR_EXTSTS	0x0100	/* Extended status present */
#define BRGPHY_BMSR_PRESUB	0x0040	/* Preamble surpression */
#define BRGPHY_BMSR_ACOMP	0x0020	/* Autoneg complete */
#define BRGPHY_BMSR_RFAULT	0x0010	/* Remote fault condition occured */
#define BRGPHY_BMSR_ANEG	0x0008	/* Autoneg capable */
#define BRGPHY_BMSR_LINK	0x0004	/* Link status */
#define BRGPHY_BMSR_JABBER	0x0002	/* Jabber detected */
#define BRGPHY_BMSR_EXT		0x0001	/* Extended capability */

#define BRGPHY_MII_ANAR		0x04
#define BRGPHY_ANAR_NP		0x8000	/* Next page */
#define BRGPHY_ANAR_RF		0x2000	/* Remote fault */
#define BRGPHY_ANAR_ASP		0x0800	/* Asymmetric Pause */
#define BRGPHY_ANAR_PC		0x0400	/* Pause capable */
#define BRGPHY_ANAR_SEL		0x001F	/* selector field, 00001=Ethernet */

#define BRGPHY_MII_ANLPAR	0x05
#define BRGPHY_ANLPAR_NP	0x8000	/* Next page */
#define BRGPHY_ANLPAR_RF	0x2000	/* Remote fault */
#define BRGPHY_ANLPAR_ASP	0x0800	/* Asymmetric Pause */
#define BRGPHY_ANLPAR_PC	0x0400	/* Pause capable */
#define BRGPHY_ANLPAR_SEL	0x001F	/* selector field, 00001=Ethernet */

#define BRGPHY_SEL_TYPE		0x0001	/* ethernet */

#define BRGPHY_MII_ANER		0x06
#define BRGPHY_ANER_PDF		0x0010	/* Parallel detection fault */
#define BRGPHY_ANER_LPNP	0x0008	/* Link partner can next page */
#define BRGPHY_ANER_NP		0x0004	/* Local PHY can next page */
#define BRGPHY_ANER_RX		0x0002	/* Next page received */
#define BRGPHY_ANER_LPAN	0x0001 	/* Link partner autoneg capable */

#define BRGPHY_MII_NEXTP	0x07	/* Next page */

#define BRGPHY_MII_NEXTP_LP	0x08	/* Next page of link partner */

#define BRGPHY_MII_1000CTL	0x09	/* 1000baseT control */
#define BRGPHY_1000CTL_TST	0xE000	/* test modes */
#define BRGPHY_1000CTL_MSE	0x1000	/* Master/Slave enable */
#define BRGPHY_1000CTL_MSC	0x0800	/* Master/Slave configuration */
#define BRGPHY_1000CTL_RD	0x0400	/* Repeater/DTE */
#define BRGPHY_1000CTL_AFD	0x0200	/* Advertise full duplex */
#define BRGPHY_1000CTL_AHD	0x0100	/* Advertise half duplex */

#define BRGPHY_MII_1000STS	0x0A	/* 1000baseT status */
#define BRGPHY_1000STS_MSF	0x8000	/* Master/slave fault */
#define BRGPHY_1000STS_MSR	0x4000	/* Master/slave result */
#define BRGPHY_1000STS_LRS	0x2000	/* Local receiver status */
#define BRGPHY_1000STS_RRS	0x1000	/* Remote receiver status */
#define BRGPHY_1000STS_LPFD	0x0800	/* Link partner can FD */
#define BRGPHY_1000STS_LPHD	0x0400	/* Link partner can HD */
#define BRGPHY_1000STS_IEC	0x00FF	/* Idle error count */

#define BRGPHY_MII_EXTSTS	0x0F	/* Extended status */
#define BRGPHY_EXTSTS_X_FD_CAP	0x8000	/* 1000base-X FD capable */
#define BRGPHY_EXTSTS_X_HD_CAP	0x4000	/* 1000base-X HD capable */
#define BRGPHY_EXTSTS_T_FD_CAP	0x2000	/* 1000base-T FD capable */
#define BRGPHY_EXTSTS_T_HD_CAP	0x1000	/* 1000base-T HD capable */

#define BRGPHY_MII_PHY_EXTCTL	0x10	/* PHY extended control */
#define BRGPHY_PHY_EXTCTL_MAC_PHY	0x8000	/* 10BIT/GMI-interface */
#define BRGPHY_PHY_EXTCTL_DIS_CROSS	0x4000	/* Disable MDI crossover */
#define BRGPHY_PHY_EXTCTL_TX_DIS	0x2000	/* Tx output disable d*/
#define BRGPHY_PHY_EXTCTL_INT_DIS	0x1000	/* Interrupts disabled */
#define BRGPHY_PHY_EXTCTL_F_INT		0x0800	/* Force interrupt */
#define BRGPHY_PHY_EXTCTL_BY_45		0x0400	/* Bypass 4B5B-Decoder */
#define BRGPHY_PHY_EXTCTL_BY_SCR	0x0200	/* Bypass scrambler */
#define BRGPHY_PHY_EXTCTL_BY_MLT3	0x0100	/* Bypass MLT3 encoder */
#define BRGPHY_PHY_EXTCTL_BY_RXA	0x0080	/* Bypass RX alignment */
#define BRGPHY_PHY_EXTCTL_RES_SCR	0x0040	/* Reset scrambler */
#define BRGPHY_PHY_EXTCTL_EN_LTR	0x0020	/* Enable LED traffic mode */
#define BRGPHY_PHY_EXTCTL_LED_ON	0x0010	/* Force LEDs on */
#define BRGPHY_PHY_EXTCTL_LED_OFF	0x0008	/* Force LEDs off */
#define BRGPHY_PHY_EXTCTL_EX_IPG	0x0004	/* Extended TX IPG mode */
#define BRGPHY_PHY_EXTCTL_3_LED		0x0002	/* Three link LED mode */
#define BRGPHY_PHY_EXTCTL_HIGH_LA	0x0001	/* GMII Fifo Elasticy (?) */

#define BRGPHY_MII_PHY_EXTSTS	0x11	/* PHY extended status */
#define BRGPHY_PHY_EXTSTS_CROSS_STAT	0x2000	/* MDI crossover status */
#define BRGPHY_PHY_EXTSTS_INT_STAT	0x1000	/* Interrupt status */
#define BRGPHY_PHY_EXTSTS_RRS		0x0800	/* Remote receiver status */
#define BRGPHY_PHY_EXTSTS_LRS		0x0400	/* Local receiver status */
#define BRGPHY_PHY_EXTSTS_LOCKED	0x0200	/* Locked */
#define BRGPHY_PHY_EXTSTS_LS		0x0100	/* Link status */
#define BRGPHY_PHY_EXTSTS_RF		0x0080	/* Remove fault */
#define BRGPHY_PHY_EXTSTS_CE_ER		0x0040	/* Carrier ext error */
#define BRGPHY_PHY_EXTSTS_BAD_SSD	0x0020	/* Bad SSD */
#define BRGPHY_PHY_EXTSTS_BAD_ESD	0x0010	/* Bad ESS */
#define BRGPHY_PHY_EXTSTS_RX_ER		0x0008	/* RX error */
#define BRGPHY_PHY_EXTSTS_TX_ER		0x0004	/* TX error */
#define BRGPHY_PHY_EXTSTS_LOCK_ER	0x0002	/* Lock error */
#define BRGPHY_PHY_EXTSTS_MLT3_ER	0x0001	/* MLT3 code error */

#define BRGPHY_MII_RXERRCNT	0x12	/* RX error counter */

#define BRGPHY_MII_FCERRCNT	0x13	/* false carrier sense counter */
#define BGRPHY_FCERRCNT		0x00FF	/* False carrier counter */

#define BRGPHY_MII_RXNOCNT	0x14	/* RX not OK counter */
#define BRGPHY_RXNOCNT_LOCAL	0xFF00	/* Local RX not OK counter */
#define BRGPHY_RXNOCNT_REMOTE	0x00FF	/* Local RX not OK counter */

#define BRGPHY_MII_DSP_RW_PORT	0x15	/* DSP coefficient r/w port */

#define BRGPHY_MII_DSP_ADDR_REG	0x17	/* DSP coefficient addr register */

#define BRGPHY_DSP_TAP_NUMBER_MASK		0x00
#define BRGPHY_DSP_AGC_A			0x00
#define BRGPHY_DSP_AGC_B			0x01
#define BRGPHY_DSP_MSE_PAIR_STATUS		0x02
#define BRGPHY_DSP_SOFT_DECISION		0x03
#define BRGPHY_DSP_PHASE_REG			0x04
#define BRGPHY_DSP_SKEW				0x05
#define BRGPHY_DSP_POWER_SAVER_UPPER_BOUND	0x06
#define BRGPHY_DSP_POWER_SAVER_LOWER_BOUND	0x07
#define BRGPHY_DSP_LAST_ECHO			0x08
#define BRGPHY_DSP_FREQUENCY			0x09
#define BRGPHY_DSP_PLL_BANDWIDTH		0x0A
#define BRGPHY_DSP_PLL_PHASE_OFFSET		0x0B

#define BRGPHYDSP_FILTER_DCOFFSET		0x0C00
#define BRGPHY_DSP_FILTER_FEXT3			0x0B00
#define BRGPHY_DSP_FILTER_FEXT2			0x0A00
#define BRGPHY_DSP_FILTER_FEXT1			0x0900
#define BRGPHY_DSP_FILTER_FEXT0			0x0800
#define BRGPHY_DSP_FILTER_NEXT3			0x0700
#define BRGPHY_DSP_FILTER_NEXT2			0x0600
#define BRGPHY_DSP_FILTER_NEXT1			0x0500
#define BRGPHY_DSP_FILTER_NEXT0			0x0400
#define BRGPHY_DSP_FILTER_ECHO			0x0300
#define BRGPHY_DSP_FILTER_DFE			0x0200
#define BRGPHY_DSP_FILTER_FFE			0x0100

#define BRGPHY_DSP_CONTROL_ALL_FILTERS		0x1000

#define BRGPHY_DSP_SEL_CH_0			0x0000
#define BRGPHY_DSP_SEL_CH_1			0x2000
#define BRGPHY_DSP_SEL_CH_2			0x4000
#define BRGPHY_DSP_SEL_CH_3			0x6000

#define BRGPHY_MII_AUXCTL	0x18	/* AUX control */
#define BRGPHY_AUXCTL_LOW_SQ	0x8000	/* Low squelch */
#define BRGPHY_AUXCTL_LONG_PKT	0x4000	/* RX long packets */
#define BRGPHY_AUXCTL_ER_CTL	0x3000	/* Edgerate control */
#define BRGPHY_AUXCTL_TX_TST	0x0400	/* TX test, always 1 */
#define BRGPHY_AUXCTL_DIS_PRF	0x0080	/* dis part resp filter */
#define BRGPHY_AUXCTL_DIAG_MODE	0x0004	/* Diagnostic mode */

#define BRGPHY_MII_AUXSTS	0x19	/* AUX status */
#define BRGPHY_AUXSTS_ACOMP	0x8000	/* autoneg complete */
#define BRGPHY_AUXSTS_AN_ACK	0x4000	/* autoneg complete ack */
#define BRGPHY_AUXSTS_AN_ACK_D	0x2000	/* autoneg complete ack detect */
#define BRGPHY_AUXSTS_AN_NPW	0x1000	/* autoneg next page wait */
#define BRGPHY_AUXSTS_AN_RES	0x0700	/* AN HDC */
#define BRGPHY_AUXSTS_PDF	0x0080	/* Parallel detect. fault */
#define BRGPHY_AUXSTS_RF	0x0040	/* remote fault */
#define BRGPHY_AUXSTS_ANP_R	0x0020	/* AN page received */
#define BRGPHY_AUXSTS_LP_ANAB	0x0010	/* LP AN ability */
#define BRGPHY_AUXSTS_LP_NPAB	0x0008	/* LP Next page ability */
#define BRGPHY_AUXSTS_LINK	0x0004	/* Link status */
#define BRGPHY_AUXSTS_PRR	0x0002	/* Pause resolution-RX */
#define BRGPHY_AUXSTS_PRT	0x0001	/* Pause resolution-TX */

#define BRGPHY_RES_1000FD	0x0700	/* 1000baseT full duplex */
#define BRGPHY_RES_1000HD	0x0600	/* 1000baseT half duplex */
#define BRGPHY_RES_100FD	0x0500	/* 100baseT full duplex */
#define BRGPHY_RES_100T4	0x0400	/* 100baseT4 */
#define BRGPHY_RES_100HD	0x0300	/* 100baseT half duplex */
#define BRGPHY_RES_10HD		0x0200	/* 10baseT full duplex */
#define BRGPHY_RES_10FD		0x0100	/* 10baseT half duplex */

#define BRGPHY_MII_ISR		0x1A	/* interrupt status */
#define BRGPHY_ISR_PSERR	0x4000	/* Pair swap error */
#define BRGPHY_ISR_MDXI_SC	0x2000	/* MDIX Status Change */
#define BRGPHY_ISR_HCT		0x1000	/* counter above 32K */
#define BRGPHY_ISR_LCT		0x0800	/* all counter below 128 */
#define BRGPHY_ISR_AN_PR	0x0400	/* Autoneg page received */
#define BRGPHY_ISR_NO_HDCL	0x0200	/* No HCD Link */
#define BRGPHY_ISR_NO_HDC	0x0100	/* No HCD */
#define BRGPHY_ISR_USHDC	0x0080	/* Negotiated Unsupported HCD */
#define BRGPHY_ISR_SCR_S_ERR	0x0040	/* Scrambler sync error */
#define BRGPHY_ISR_RRS_CHG	0x0020	/* Remote RX status change */
#define BRGPHY_ISR_LRS_CHG	0x0010	/* Local RX status change */
#define BRGPHY_ISR_DUP_CHG	0x0008	/* Duplex mode change */
#define BRGPHY_ISR_LSP_CHG	0x0004	/* Link speed changed */
#define BRGPHY_ISR_LNK_CHG	0x0002	/* Link status change */
#define BRGPHY_ISR_CRCERR	0x0001	/* CEC error */

#define BRGPHY_MII_IMR		0x1B	/* interrupt mask */
#define BRGPHY_IMR_PSERR	0x4000	/* Pair swap error */
#define BRGPHY_IMR_MDXI_SC	0x2000	/* MDIX Status Change */
#define BRGPHY_IMR_HCT		0x1000	/* counter above 32K */
#define BRGPHY_IMR_LCT		0x0800	/* all counter below 128 */
#define BRGPHY_IMR_AN_PR	0x0400	/* Autoneg page received */
#define BRGPHY_IMR_NO_HDCL	0x0200	/* No HCD Link */
#define BRGPHY_IMR_NO_HDC	0x0100	/* No HCD */
#define BRGPHY_IMR_USHDC	0x0080	/* Negotiated Unsupported HCD */
#define BRGPHY_IMR_SCR_S_ERR	0x0040	/* Scrambler sync error */
#define BRGPHY_IMR_RRS_CHG	0x0020	/* Remote RX status change */
#define BRGPHY_IMR_LRS_CHG	0x0010	/* Local RX status change */
#define BRGPHY_IMR_DUP_CHG	0x0008	/* Duplex mode change */
#define BRGPHY_IMR_LSP_CHG	0x0004	/* Link speed changed */
#define BRGPHY_IMR_LNK_CHG	0x0002	/* Link status change */
#define BRGPHY_IMR_CRCERR	0x0001	/* CEC error */

#define BRGPHY_INTRS	\
	~(BRGPHY_IMR_LNK_CHG|BRGPHY_IMR_LSP_CHG|BRGPHY_IMR_DUP_CHG)

#endif /* _DEV_BRGPHY_MIIREG_H_ */
