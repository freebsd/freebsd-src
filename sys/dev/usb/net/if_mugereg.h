/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (C) 2018 The FreeBSD Foundation.
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

/*
 * Definitions for the Microchip LAN78xx USB-to-Ethernet controllers.
 *
 * This information was mostly taken from the LAN7800 manual, but some
 * undocumented registers are based on the Linux driver.
 *
 */

#ifndef _IF_MUGEREG_H_
#define _IF_MUGEREG_H_

/* USB Vendor Requests */
#define UVR_WRITE_REG		0xA0
#define UVR_READ_REG		0xA1
#define UVR_GET_STATS		0xA2

/* Device ID and revision register */
#define ID_REV			0x000
#define ID_REV_CHIP_ID_MASK_	0xFFFF0000UL
#define ID_REV_CHIP_REV_MASK_	0x0000FFFFUL

/* Device interrupt status register. */
#define INT_STS			0x00C
#define INT_STS_CLEAR_ALL_	0xFFFFFFFFUL

/* Hardware Configuration Register. */
#define HW_CFG			0x010
#define HW_CFG_LED3_EN_		(0x1UL << 23)
#define HW_CFG_LED2_EN_		(0x1UL << 22)
#define HW_CFG_LED1_EN_		(0x1UL << 21)
#define HW_CFG_LEDO_EN_		(0x1UL << 20)
#define HW_CFG_MEF_		(0x1UL << 4)
#define HW_CFG_ETC_		(0x1UL << 3)
#define HW_CFG_LRST_		(0x1UL << 1)	/* Lite reset */
#define HW_CFG_SRST_		(0x1UL << 0)	/* Soft reset */

/* Power Management Control Register. */
#define PMT_CTL			0x014
#define PMT_CTL_PHY_RST_	(0x1UL << 4)	/* PHY reset */
#define PMT_CTL_WOL_EN_		(0x1UL << 3)	/* PHY wake-on-lan enable */
#define PMT_CTL_PHY_WAKE_EN_	(0x1UL << 2)	/* PHY interrupt as a wake up event*/

/* GPIO Configuration 0 Register. */
#define GPIO_CFG0		0x018

/* GPIO Configuration 1 Register. */
#define GPIO_CFG1		0x01C

/* GPIO wake enable and polarity register. */
#define GPIO_WAKE		0x020

/* RX Command A */
#define RX_CMD_A_RED_		(0x1UL << 22)	/* Receive Error Detected */
#define RX_CMD_A_ICSM_		(0x1UL << 14)
#define RX_CMD_A_LEN_MASK_	0x00003FFFUL

/* TX Command A */
#define TX_CMD_A_LEN_MASK_	0x000FFFFFUL
#define TX_CMD_A_FCS_		(0x1UL << 22)

/* Data Port Select Register */
#define DP_SEL			0x024
#define DP_SEL_DPRDY_		(0x1UL << 31)
#define DP_SEL_RSEL_VLAN_DA_	(0x1UL << 0)	/* RFE VLAN and DA Hash Table */
#define DP_SEL_RSEL_MASK_	0x0000000F
#define DP_SEL_VHF_HASH_LEN	16
#define DP_SEL_VHF_VLAN_LEN	128

/* Data Port Command Register */
#define DP_CMD			0x028
#define DP_CMD_WRITE_		(0x1UL << 0)		/* 1 for write */
#define DP_CMD_READ_		(0x0UL << 0)		/* 0 for read */

/* Data Port Address Register */
#define DP_ADDR			0x02C

/* Data Port Data Register */
#define DP_DATA			0x030

/* EEPROM Command Register */
#define E2P_CMD			0x040
#define E2P_CMD_MASK_		0x70000000UL
#define E2P_CMD_ADDR_MASK_	0x000001FFUL
#define E2P_CMD_BUSY_		(0x1UL << 31)
#define E2P_CMD_READ_		(0x0UL << 28)
#define E2P_CMD_WRITE_		(0x3UL << 28)
#define E2P_CMD_ERASE_		(0x5UL << 28)
#define E2P_CMD_RELOAD_		(0x7UL << 28)
#define E2P_CMD_TIMEOUT_	(0x1UL << 10)
#define E2P_MAC_OFFSET		0x01
#define E2P_INDICATOR_OFFSET	0x00

/* EEPROM Data Register */
#define E2P_DATA		0x044
#define E2P_INDICATOR		0xA5	/* Indicates an EEPROM is present */

/* Packet sizes. */
#define MUGE_SS_USB_PKT_SIZE		1024
#define MUGE_HS_USB_PKT_SIZE		512
#define MUGE_FS_USB_PKT_SIZE		64

/* Receive Filtering Engine Control Register */
#define RFE_CTL			0x0B0
#define RFE_CTL_IGMP_COE_	(0x1U << 14)
#define RFE_CTL_ICMP_COE_	(0x1U << 13)
#define RFE_CTL_TCPUDP_COE_	(0x1U << 12)
#define RFE_CTL_IP_COE_		(0x1U << 11)
#define RFE_CTL_BCAST_EN_	(0x1U << 10)
#define RFE_CTL_MCAST_EN_	(0x1U << 9)
#define RFE_CTL_UCAST_EN_	(0x1U << 8)
#define RFE_CTL_VLAN_FILTER_	(0x1U << 5)
#define RFE_CTL_MCAST_HASH_	(0x1U << 3)
#define RFE_CTL_DA_PERFECT_	(0x1U << 1)

/* End address of the RX FIFO */
#define FCT_RX_FIFO_END		0x0C8
#define FCT_RX_FIFO_END_MASK_	0x0000007FUL
#define MUGE_MAX_RX_FIFO_SIZE	(12 * 1024)

/* End address of the TX FIFO */
#define FCT_TX_FIFO_END		0x0CC
#define FCT_TX_FIFO_END_MASK_	0x0000003FUL
#define MUGE_MAX_TX_FIFO_SIZE	(12 * 1024)

/* USB Configuration Register 0 */
#define USB_CFG0	0x080
#define USB_CFG_BIR_	(0x1U << 6)	/* Bulk-In Empty response */
#define USB_CFG_BCE_	(0x1U << 5)	/* Burst Cap Enable */

/* USB Configuration Register 1 */
#define USB_CFG1	0x084

/* USB Configuration Register 2 */
#define USB_CFG2			0x088

/* USB bConfigIndex: it only has one configuration. */
#define MUGE_CONFIG_INDEX			0

/* Burst Cap Register */
#define BURST_CAP			0x090
#define MUGE_DEFAULT_BURST_CAP_SIZE	MUGE_MAX_TX_FIFO_SIZE

/* Bulk-In Delay Register */
#define BULK_IN_DLY			0x094
#define MUGE_DEFAULT_BULK_IN_DELAY		0x0800

/* Interrupt Endpoint Control Register */
#define INT_EP_CTL			0x098
#define INT_ENP_PHY_INT			(0x1U << 17)	/* PHY Enable */

/* Registers on the phy, accessed via MII/MDIO */
#define MUGE_PHY_INTR_STAT			25
#define MUGE_PHY_INTR_MASK			26
#define MUGE_PHY_INTR_LINK_CHANGE		(0x1U << 13)
#define MUGE_PHY_INTR_ANEG_COMP		(0x1U << 10)
#define MUGE_EXT_PAGE_ACCESS			0x1F
#define MUGE_EXT_PAGE_SPACE_0		0x0000
#define MUGE_EXT_PAGE_SPACE_1		0x0001
#define MUGE_EXT_PAGE_SPACE_2		0x0002

/* Extended Register Page 1 Space */
#define MUGE_EXT_MODE_CTRL			0x0013
#define MUGE_EXT_MODE_CTRL_MDIX_MASK_	0x000C
#define MUGE_EXT_MODE_CTRL_AUTO_MDIX_	0x0000

/* FCT Flow Control Threshold Register */
#define FCT_FLOW			0x0D0

/* FCT RX FIFO Control Register */
#define FCT_RX_CTL			0x0C0

/* FCT TX FIFO Control Register */
#define FCT_TX_CTL			0x0C4
#define FCT_TX_CTL_EN_			(0x1U << 31)

/* MAC Control Register */
#define MAC_CR				0x100
#define MAC_CR_AUTO_DUPLEX_		(0x1U << 12)
#define MAC_CR_AUTO_SPEED_		(0x1U << 11)

/* MAC Receive Register */
#define MAC_RX				0x104
#define MAC_RX_MAX_FR_SIZE_MASK_	0x3FFF0000
#define MAC_RX_MAX_FR_SIZE_SHIFT_	16
#define MAC_RX_EN_			(0x1U << 0)	/* Enable Receiver */

/* MAC Transmit Register */
#define MAC_TX				0x108
#define MAC_TX_TXEN_			(0x1U << 0)	/* Enable Transmitter */

/* Flow Control Register */
#define FLOW				0x10C
#define FLOW_CR_TX_FCEN_		(0x1U << 30)	/* TX FC Enable */
#define FLOW_CR_RX_FCEN_		(0x1U << 29)	/* RX FC Enable */

/* MAC Receive Address Registers */
#define RX_ADDRH			0x118	/* High */
#define RX_ADDRL			0x11C	/* Low */

/* MII Access Register */
#define MII_ACCESS			0x120
#define MII_BUSY_			(0x1UL << 0)
#define MII_READ_			(0x0UL << 1)
#define MII_WRITE_			(0x1UL << 1)

/* MII Data Register */
#define MII_DATA			0x124

 /* MAC address perfect filter registers (ADDR_FILTx) */
#define PFILTER_BASE			0x400
#define PFILTER_HIX			0x00
#define PFILTER_LOX			0x04
#define MUGE_NUM_PFILTER_ADDRS_		33
#define PFILTER_ADDR_VALID_		(0x1UL << 31)
#define PFILTER_ADDR_TYPE_SRC_		(0x1UL << 30)
#define PFILTER_ADDR_TYPE_DST_		(0x0UL << 30)
#define PFILTER_HI(index)		(PFILTER_BASE + (8 * (index)) + (PFILTER_HIX))
#define PFILTER_LO(index)		(PFILTER_BASE + (8 * (index)) + (PFILTER_LOX))

/*
 * These registers are not documented in the datasheet, and are based on
 * the Linux driver.
 */
#define OTP_BASE_ADDR			0x01000
#define OTP_PWR_DN			(OTP_BASE_ADDR + 4 * 0x00)
#define OTP_PWR_DN_PWRDN_N		0x01
#define OTP_ADDR1			(OTP_BASE_ADDR + 4 * 0x01)
#define OTP_ADDR1_15_11			0x1F
#define OTP_ADDR2			(OTP_BASE_ADDR + 4 * 0x02)
#define OTP_ADDR2_10_3			0xFF
#define OTP_ADDR3			(OTP_BASE_ADDR + 4 * 0x03)
#define OTP_ADDR3_2_0			0x03
#define OTP_RD_DATA			(OTP_BASE_ADDR + 4 * 0x06)
#define OTP_FUNC_CMD			(OTP_BASE_ADDR + 4 * 0x08)
#define OTP_FUNC_CMD_RESET		0x04
#define OTP_FUNC_CMD_PROGRAM_		0x02
#define OTP_FUNC_CMD_READ_		0x01
#define OTP_MAC_OFFSET			0x01
#define OTP_INDICATOR_OFFSET		0x00
#define OTP_INDICATOR_1			0xF3
#define OTP_INDICATOR_2			0xF7
#define OTP_CMD_GO			(OTP_BASE_ADDR + 4 * 0x0A)
#define OTP_CMD_GO_GO_			0x01
#define OTP_STATUS			(OTP_BASE_ADDR + 4 * 0x0A)
#define OTP_STATUS_OTP_LOCK_		0x10
#define OTP_STATUS_BUSY_		0x01

/* Some unused registers, from the data sheet. */
#if 0
#define BOS_ATTR			0x050
#define SS_ATTR				0x054
#define HS_ATTR				0x058
#define FS_ATTR				0x05C
#define STRNG_ATTR0			0x060
#define STRNG_ATTR1			0x064
#define FLAG_ATTR			0x068
#define SW_GP_1				0x06C
#define SW_GP_2				0x070
#define SW_GP_3				0x074
#define VLAN_TYPE			0x0B4
#define RX_DP_STOR			0x0D4
#define TX_DP_STOR			0x0D8
#define LTM_BELT_IDLE0			0x0E0
#define LTM_BELT_IDLE1			0x0E4
#define LTM_BELT_ACT0			0x0E8
#define LTM_BELT_ACT1			0x0EC
#define LTM_INACTIVE0			0x0F0
#define LTM_INACTIVE1			0x0F4

#define RAND_SEED			0x110
#define ERR_STS				0x114

#define EEE_TX_LPI_REQUEST_DELAY_CNT	0x130
#define EEE_TW_TX_SYS			0x134
#define EEE_TX_LPI_AUTO_REMOVAL_DELAY	0x138

#define WUCSR1				0x140
#define WK_SRC				0x144
#define WUF_CFG_BASE			0x150
#define WUF_MASK_BASE			0x200
#define WUCSR2				0x600

#define NSx_IPV6_ADDR_DEST_0		0x610
#define NSx_IPV6_ADDR_DEST_1		0x614
#define NSx_IPV6_ADDR_DEST_2		0x618
#define NSx_IPV6_ADDR_DEST_3		0x61C

#define NSx_IPV6_ADDR_SRC_0		0x620
#define NSx_IPV6_ADDR_SRC_1		0x624
#define NSx_IPV6_ADDR_SRC_2		0x628
#define NSx_IPV6_ADDR_SRC_3		0x62C

#define NSx_ICMPV6_ADDR0_0		0x630
#define NSx_ICMPV6_ADDR0_1		0x634
#define NSx_ICMPV6_ADDR0_2		0x638
#define NSx_ICMPV6_ADDR0_3		0x63C

#define NSx_ICMPV6_ADDR1_0		0x640
#define NSx_ICMPV6_ADDR1_1		0x644
#define NSx_ICMPV6_ADDR1_2		0x648
#define NSx_ICMPV6_ADDR1_3		0x64C

#define NSx_IPV6_ADDR_DEST		0x650
#define NSx_IPV6_ADDR_SRC		0x660
#define NSx_ICMPV6_ADDR0		0x670
#define NSx_ICMPV6_ADDR1		0x680

#define SYN_IPV4_ADDR_SRC		0x690
#define SYN_IPV4_ADDR_DEST		0x694
#define SYN_IPV4_TCP_PORTS		0x698

#define SYN_IPV6_ADDR_SRC_0		0x69C
#define SYN_IPV6_ADDR_SRC_1		0x6A0
#define SYN_IPV6_ADDR_SRC_2		0x6A4
#define SYN_IPV6_ADDR_SRC_3		0x6A8

#define SYN_IPV6_ADDR_DEST_0		0x6AC
#define SYN_IPV6_ADDR_DEST_1		0x6B0
#define SYN_IPV6_ADDR_DEST_2		0x6B4
#define SYN_IPV6_ADDR_DEST_3		0x6B8

#define SYN_IPV6_TCP_PORTS		0x6BC
#define ARP_SPA				0x6C0
#define ARP_TPA				0x6C4
#define PHY_DEV_ID			0x700
#endif

#endif /* _IF_MUGEREG_H_ */
