/*-
 * Copyright (c) 2012, Oleksandr Tymoshenko
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _IF_SMSCREG_H_
#define _IF_SMSCREG_H_

/* TX command A */
#define SMSC_TX_CMD_A_INTR			(1 << 31)
#define SMSC_TX_CMD_A_ALIGN_MASK		0x03000000
#define SMSC_TX_CMD_A_DATA_OFFSET_MASK		0x001f0000
#define SMSC_TX_CMD_A_FIRST_SEG			(1 << 13)
#define SMSC_TX_CMD_A_LAST_SEG			(1 << 12)
#define SMSC_TX_CMD_A_BUF_SIZE_MASK		0x000007ff

/* TX command B */
#define SMSC_TX_CMD_B_PACKET_TAG_MASK		0xffff0000
#define SMSC_TX_CMD_B_CSUM_ENABLE		(1 << 14)
#define SMSC_TX_CMD_B_ADD_CRC_DISABLE		(1 << 13)
#define SMSC_TX_CMD_B_DISABLE_PADDING		(1 << 12)
#define SMSC_TX_CMD_B_PKT_BYTE_LENGTH_MASK	0x000007ff

/* RX status */
#define SMSC_RX_STATUS_FF			(1 << 30)	/* Filter Fail */
#define SMSC_RX_STATUS_FL_MASK			0x3fff0000	/* Frame Length */
#define SMSC_RX_STATUS_FL_SHIFT			16
#define SMSC_RX_STATUS_ES			(1 << 15)	/* Error Summary */
#define SMSC_RX_STATUS_BF			(1 << 13)	/* Broadcast Frame */
#define SMSC_RX_STATUS_LE			(1 << 12)	/* Length Error */
#define SMSC_RX_STATUS_RF			(1 << 11)	/* Runt Frame */
#define SMSC_RX_STATUS_MF			(1 << 10)	/* Multicast Frame */
#define SMSC_RX_STATUS_TL			(1 <<  7)	/* Frame too long */
#define SMSC_RX_STATUS_CS			(1 <<  6)	/* Collision Seen */
#define SMSC_RX_STATUS_FT			(1 <<  5)	/* Frame Type */
#define SMSC_RX_STATUS_RW			(1 <<  4)	/* Receive Watchdog */
#define SMSC_RX_STATUS_ME			(1 <<  3)	/* Mii Error */
#define SMSC_RX_STATUS_DB			(1 <<  2)	/* Dribbling */
#define SMSC_RX_STATUS_CRC			(1 <<  1)	/* CRC Error */

/* Registers */
#define SMSC_REG_ID_REV			0x00
#define 	ID_REV_CHIP_ID_MASK		0xffff0000
#define 	ID_REV_CHIP_ID_SHIFT		16
#define 	ID_REV_CHIP_REV_MASK		0x0000ffff
#define SMSC_REG_INT_STS		0x08
#define SMSC_REG_TX_CFG			0x10
#define 	TX_CFG_ON			(1 << 2)
#define SMSC_REG_HW_CFG			0x14
#define 	HW_CFG_BIR			(1 << 12)
#define 	HW_CFG_RXDOFF			(3 <<  9)
#define 	HW_CFG_LRST			(1 <<  3)
#define SMSC_REG_PM_CTRL		0x20
#define 	PM_CTRL_PHY_RST			(1 << 4)
#define SMSC_REG_LED_GPIO_CFG		0x24
#define 	LED_GPIO_CFG_SPD_LED		(1 << 24)
#define 	LED_GPIO_CFG_LNK_LED		(1 << 20)
#define 	LED_GPIO_CFG_FDX_LED		(1 << 16)
#define SMSC_REG_AFC_CFG		0x2C
/* Hi watermark = 15.5Kb (~10 mtu pkts) */
/* low watermark = 3k (~2 mtu pkts) */
/* backpressure duration = ~ 350us */
/* Apply FC on any frame. */
#define AFC_CFG_DEFAULT			0x00F830A1
#define SMSC_REG_E2P_CMD		0x30
#define 	E2P_CMD_BUSY				(1 << 31)
#define 	E2P_CMD_READ				(0 << 28)
#define 	E2P_CMD_EWDS				(1 << 28)
#define 	E2P_CMD_EWEN				(2 << 28)
#define 	E2P_CMD_WRITE				(3 << 28)
#define 	E2P_CMD_WRAL				(4 << 28)
#define 	E2P_CMD_ERASE				(5 << 28)
#define 	E2P_CMD_ERAL				(6 << 28)
#define 	E2P_CMD_RELOAD				(7 << 28)
#define 	E2P_CMD_TIMEOUT				(1 << 10)
#define 	E2P_CMD_LOADED				(1 << 9)
#define 	E2P_CMD_ADDR				(0x000001ff)
#define SMSC_REG_E2P_DATA		0x34
#define 	E2P_DATA_MASK			0x000000ff
#define SMSC_REG_BURST_CAP		0x38
#define SMSC_REG_INT_EP_CTL		0x68
#define 	INT_EP_CTL_PHY_INT		(1 << 15)
#define SMSC_REG_BULK_IN_DLY		0x6C
#define SMSC_REG_MAC_CR			0x100
#define 	MAC_CR_RXALL			(1 << 31)
#define 	MAC_CR_RCVOWN			(1 << 23)
#define 	MAC_CR_LOOPBK			(1 << 21)
#define 	MAC_CR_FDPX			(1 << 20)
#define 	MAC_CR_MCPAS			(1 << 19)
#define 	MAC_CR_PRMS			(1 << 18)
#define 	MAC_CR_INVFILT			(1 << 17)
#define 	MAC_CR_PASSBAD			(1 << 16)
#define 	MAC_CR_HFILT			(1 << 15)
#define 	MAC_CR_HPFILT			(1 << 13)
#define 	MAC_CR_LCOLL			(1 << 12)
#define 	MAC_CR_BCAST			(1 << 11)
#define 	MAC_CR_DISRTY			(1 << 10)
#define 	MAC_CR_PADSTR			(1 << 8)
#define 	MAC_CR_DFCHK			(1 << 5)
#define 	MAC_CR_TXEN			(1 << 3)
#define 	MAC_CR_RXEN			(1 << 2)
#define SMSC_REG_ADDRH			0x104
#define SMSC_REG_ADDRL			0x108
#define SMSC_REG_HASHH			0x10C
#define SMSC_REG_HASHL			0x110
#define SMSC_REG_MII_ADDR		0x114
#define 	MII_WRITE			(1 << 1)
#define 	MII_BUSY			(1 << 0)
#define 	MII_READ			(0 << 0)
#define SMS_REG_MII_DATA		0x118
#define SMSC_REG_FLOW			0x11C
#define SMSC_REG_VLAN1			0x120
#define SMSC_REG_VLAN2			0x124

/* MII registers */
#define MII_PHY_INT_SRC			29
#define 	PHY_INT_SRC_ENERGY_ON		(1 << 7)
#define 	PHY_INT_SRC_ANEG_COMP		(1 << 6)
#define 	PHY_INT_SRC_REMOTE_FAULT	(1 << 5)
#define 	PHY_INT_SRC_LINK_DOWN		(1 << 4)

#define MII_PHY_INT_MASK		30
#define 	PHY_INT_MASK_ENERGY_ON		(1 << 7)
#define 	PHY_INT_MASK_ANEG_COMP		(1 << 6)
#define 	PHY_INT_MASK_REMOTE_FAULT	(1 << 5)
#define 	PHY_INT_MASK_LINK_DOWN		(1 << 4)
#define 	PHY_INT_MASK_DEFAULT		(PHY_INT_MASK_ANEG_COMP | \
					 PHY_INT_MASK_LINK_DOWN)

/* USB requests */
#define SMSC_UR_WRITE				0xA0
#define SMSC_UR_READ				0xA1

#define	SMSC_CONFIG_INDEX	0	/* config number 1 */
#define	SMSC_IFACE_IDX		0

#endif  /* _IF_SMSCREG_H_ */
