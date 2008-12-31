/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

/* $FreeBSD: src/sys/arm/at91/if_atereg.h,v 1.2.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $ */

#ifndef ARM_AT91_IF_ATEREG_H
#define ARM_AT91_IF_ATEREG_H

#define ETH_CTL		0x00		/* EMAC Control Register */
#define ETH_CFG		0x04		/* EMAC Configuration Register */
#define ETH_SR		0x08		/* EMAC STatus Register */
#define ETH_TAR		0x0c		/* EMAC Transmit Address Register */
#define ETH_TCR		0x10		/* EMAC Transmit Control Register */
#define ETH_TSR		0x14		/* EMAC Transmit Status Register */
#define ETH_RBQP	0x18		/* EMAC Receive Buffer Queue Pointer */
		/*	0x1c		   reserved */
#define ETH_RSR		0x20		/* EMAC Receive Status Register */
#define ETH_ISR		0x24		/* EMAC Interrupt Status Register */
#define ETH_IER		0x28		/* EMAC Interrupt Enable Register */
#define ETH_IDR		0x2c		/* EMAC Interrupt Disable Register */
#define ETH_IMR		0x30		/* EMAC Interrupt Mask Register */
#define ETH_MAN		0x34		/* EMAC PHY Maintenance Register */
		/*	0x38		   reserved */
		/*	0x3c		   reserved */
#define ETH_FRA		0x40		/* Frames Transmitted OK Register */
#define ETH_SCOL	0x44		/* Single Collision Frame Register */
#define ETH_MCOL	0x48		/* Multiple Collision Frame Register */
#define ETH_OK		0x4c		/* Frames Received OK Register */
#define ETH_SEQE	0x50		/* Frame Check Sequence Error Reg */
#define ETH_ALE		0x54		/* Alignment Error Register */
#define ETH_DTE		0x58		/* Deferred Transmittion Frame Reg */
#define ETH_LCOL	0x5c		/* Late Collision Register */
#define ETH_ECOL	0x60		/* Excessive Collision Register */
#define ETH_TUE		0x64		/* Transmit Underrun Error Register */
#define ETH_CSE		0x68		/* Carrier Sense Error Register */
#define ETH_DRFC	0x6c		/* Discarded RX Frame Register */
#define ETH_ROV		0x68		/* Receive Overrun Register */
#define ETH_CDE		0x64		/* Code Error Register */
#define ETH_ELR		0x78		/* Excessive Length Error Register */
#define ETH_RJB		0x7c		/* Receive Jabber Register */
#define ETH_USF		0x80		/* Undersize Frame Register */
#define ETH_SQEE	0x84		/* SQE Test Error Register */
		/*	0x88		   reserved */
		/*	0x8c		   reserved */
#define ETH_HSL		0x90		/* EMAC Hash Address Low [31:0] */
#define ETH_HSH		0x94		/* EMAC Hash Address High [63:32] */
#define ETH_SA1L	0x98		/* EMAC Specific Address 1 Low */
#define ETH_SA1H	0x9c		/* EMAC Specific Address 1 High */
#define ETH_SA2L	0xa0		/* EMAC Specific Address 2 Low */
#define ETH_SA2H	0xa4		/* EMAC Specific Address 2 High */
#define ETH_SA3L	0xa8		/* EMAC Specific Address 3 Low */
#define ETH_SA3H	0xac		/* EMAC Specific Address 3 High */
#define ETH_SA4L	0xb0		/* EMAC Specific Address 4 Low */
#define ETH_SA4H	0xb4		/* EMAC Specific Address 4 High */


/* ETH_CTL */
#define ETH_CTL_LB	(1U << 0)	/* LB: Loopback */
#define ETH_CTL_LBL	(1U << 1)	/* LBL: Loopback Local */
#define ETH_CTL_RE	(1U << 2)	/* RE: Receive Enable */
#define ETH_CTL_TE	(1U << 3)	/* TE: Transmit Enable */
#define ETH_CTL_MPE	(1U << 4)	/* MPE: Management Port Enable */
#define ETH_CTL_CSR	(1U << 5)	/* CSR: Clear Statistics Registers */
#define ETH_CTL_ISR	(1U << 6)	/* ISR: Incremenet Statistics Regs */
#define ETH_CTL_WES	(1U << 7)	/* WES: Write Enable Statistics regs */
#define ETH_CTL_BP	(1U << 8)	/* BP: Back Pressure */

/* ETH_CFG */
#define ETH_CFG_SPD	(1U << 0)	/* SPD: Speed 1 == 100: 0 == 10 */
#define ETH_CFG_FD	(1U << 1)	/* FD: Full duplex */
#define ETH_CFG_BR	(1U << 2)	/* BR: Bit Rate (optional?) */
		/* bit 3 reserved */
#define ETH_CFG_CAF	(1U << 4)	/* CAF: Copy All Frames */
#define ETH_CFG_NBC	(1U << 5)	/* NBC: No Broadcast */
#define ETH_CFG_MTI	(1U << 6)	/* MTI: Multicast Hash Enable */
#define ETH_CFG_UNI	(1U << 7)	/* UNI: Unicast Hash Enable */
#define ETH_CFG_BIG	(1U << 8)	/* BIG: Receive 1522 Bytes */
#define ETH_CFG_EAE	(1U << 9)	/* EAE: External Address Match En */
#define ETH_CFG_CLK_8	(0U << 10)	/* CLK: Clock / 8 */
#define ETH_CFG_CLK_16	(1U << 10)	/* CLK: Clock / 16 */
#define ETH_CFG_CLK_32	(2U << 10)	/* CLK: Clock / 32 */
#define ETH_CFG_CLK_64	(3U << 10)	/* CLK: Clock / 64 */
#define ETH_CFG_RTY	(1U << 12)	/* RTY: Retry Test*/
#define ETH_CFG_RMII	(1U << 13)	/* RMII: Reduce MII */

/* ETH_SR */
#define ETH_SR_LINK	(1U << 0)	/* Reserved! */
#define ETH_SR_MDIO	(1U << 1)	/* MDIO pin status */
#define ETH_SR_IDLE	(1U << 2)	/* IDLE (PHY logic) */

/* ETH_TCR */
#define ETH_TCR_NCRC	(1U << 15)	/* NCRC: No CRC */

/* ETH_TSR */
#define ETH_TSR_OVR	(1U << 0)	/* OVR: Ethernet Transmit Overrun */
#define ETH_TSR_COL	(1U << 1)	/* COL: Collision Occurred */
#define ETH_TSR_RLE	(1U << 2)	/* RLE: Retry Limit Exceeded */
#define ETH_TSR_IDLE	(1U << 3)	/* IDLE: Transmitter Idle */
#define ETH_TSR_BNQ	(1U << 4)	/* BNQ: Enet Tran Buff not Queued */
#define ETH_TSR_COMP	(1U << 5)	/* COMP: Transmit Complete */
#define ETH_TSR_UND	(1U << 6)	/* UND: Transmit Underrun */
#define ETH_TSR_WR_MASK (0x67)	/* write 1 to clear bits */

/* ETH_RSR */
#define ETH_RSR_BNA	(1U << 0)	/* BNA: Buffer Not Available */
#define ETH_RSR_REC	(1U << 1)	/* REC: Frame Received */
#define ETH_RSR_OVR	(1U << 2)	/* OVR: RX Overrun */

/* ETH_ISR */
#define ETH_ISR_DONE	(1U << 0)	/* DONE: Management Done */
#define ETH_ISR_RCOM	(1U << 1)	/* RCOM: Receive Complete */
#define ETH_ISR_RBNA	(1U << 2)	/* RBNA: Receive Buffer Not Avail */
#define ETH_ISR_TOVR	(1U << 3)	/* TOVR: Transmit Buffer Overrun */
#define ETH_ISR_TUND	(1U << 4)	/* TUND: Transmit Buffer Underrun */
#define ETH_ISR_RTRY	(1U << 5)	/* RTRY: Retry Limit */
#define ETH_ISR_TBRE	(1U << 6)	/* TBRE: Trasnmit Buffer Reg empty */
#define ETH_ISR_TCOM	(1U << 7)	/* TCOM: Transmit Complete */
#define ETH_ISR_TIDLE	(1U << 8)	/* TIDLE: Transmit Idle */
#define ETH_ISR_LINK	(1U << 9)	/* LINK: Link pin delta (optional) */
#define ETH_ISR_ROVR	(1U << 10)	/* ROVR: RX Overrun */
#define ETH_ISR_ABT	(1U << 11)	/* ABT: Abort */

/* ETH_MAN */
#define ETH_MAN_BITS	0x40020000	/* HIGH and CODE bits */
#define ETH_MAN_READ	(2U << 28)
#define ETH_MAN_WRITE	(1U << 28)
#define ETH_MAN_PHYA_BIT 23
#define ETH_MAN_REGA_BIT 18
#define ETH_MAN_VALUE_MASK	0xffffU
#define ETH_MAN_REG_WR(phy, reg, val) \
		(ETH_MAN_BITS | ETH_MAN_WRITE | ((phy) << ETH_MAN_PHYA_BIT) | \
		((reg) << ETH_MAN_REGA_BIT) | ((val) & ETH_MAN_VALUE_MASK))
#define ETH_MAN_REG_RD(phy, reg) \
		(ETH_MAN_BITS | ETH_MAN_READ | ((phy) << ETH_MAN_PHYA_BIT) | \
		((reg) << ETH_MAN_REGA_BIT))

typedef struct {
	uint32_t	addr;
#define ETH_CPU_OWNER	(1U << 0)
#define ETH_WRAP_BIT	(1U << 1)
	uint32_t	status;
#define ETH_LEN_MASK	0x7ff
#define ETH_MAC_LOCAL_4	(1U << 23)	/* Packet matched addr 4 */
#define ETH_MAC_LOCAL_3	(1U << 24)	/* Packet matched addr 3 */
#define ETH_MAC_LOCAL_2	(1U << 25)	/* Packet matched addr 2 */
#define ETH_MAC_LOCAL_1	(1U << 26)	/* Packet matched addr 1 */
#define ETH_MAC_UNK	(1U << 27)	/* Unkown source address RFU */
#define ETH_MAC_EXT	(1U << 28)	/* External Address */
#define ETH_MAC_UCAST	(1U << 29)	/* Unicast hash match */
#define ETH_MAC_MCAST	(1U << 30)	/* Multicast hash match */
#define ETH_MAC_ONES	(1U << 31)	/* Global all ones bcast addr */
} eth_rx_desc_t;

#endif /* ARM_AT91_IF_ATEREG_H */
