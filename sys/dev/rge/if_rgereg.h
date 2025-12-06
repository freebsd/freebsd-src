/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019, 2020, 2025 Kevin Lo <kevlo@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*	$OpenBSD: if_rgereg.h,v 1.15 2025/09/19 00:41:14 kevlo Exp $	*/

#ifndef	__IF_RGEREG_H__
#define	__IF_RGEREG_H__

#define RGE_PCI_BAR0		PCI_MAPREG_START
#define RGE_PCI_BAR1		(PCI_MAPREG_START + 4)
#define RGE_PCI_BAR2		(PCI_MAPREG_START + 8)

/* For now, a single MSI message, no multi-RX/TX ring support */
#define	RGE_MSI_MESSAGES	1

#define RGE_MAC0		0x0000
#define RGE_MAC4		0x0004
#define RGE_MAR0		0x0008
#define RGE_MAR4		0x000c
#define	RGE_DTCCR_LO		0x0010
#define		RGE_DTCCR_CMD		(1U << 3)
#define	RGE_DTCCR_HI		0x0014
#define RGE_TXDESC_ADDR_LO	0x0020
#define RGE_TXDESC_ADDR_HI	0x0024
#define RGE_INT_CFG0		0x0034
#define RGE_CMD			0x0037
#define RGE_IMR			0x0038
#define RGE_ISR			0x003c
#define RGE_TXCFG		0x0040
#define RGE_RXCFG		0x0044
#define RGE_TIMERCNT		0x0048
#define RGE_EECMD		0x0050
#define RGE_CFG0		0x0051
#define RGE_CFG1		0x0052
#define RGE_CFG2		0x0053
#define RGE_CFG3		0x0054
#define RGE_CFG4		0x0055
#define RGE_CFG5		0x0056
#define RGE_TDFNR		0x0057
#define RGE_TIMERINT0		0x0058
#define RGE_TIMERINT1		0x005c
#define RGE_CSIDR		0x0064
#define RGE_CSIAR		0x0068
#define RGE_PHYSTAT		0x006c
#define RGE_PMCH		0x006f
#define RGE_INT_CFG1		0x007a
#define RGE_EPHYAR		0x0080
#define RGE_TIMERINT2		0x008c
#define RGE_TXSTART		0x0090
#define RGE_MACOCP		0x00b0
#define RGE_PHYOCP		0x00b8
#define RGE_DLLPR		0x00d0
#define RGE_TWICMD		0x00d2
#define RGE_MCUCMD		0x00d3
#define RGE_RXMAXSIZE		0x00da
#define RGE_CPLUSCMD		0x00e0
#define RGE_IM			0x00e2
#define RGE_RXDESC_ADDR_LO	0x00e4
#define RGE_RXDESC_ADDR_HI	0x00e8
#define RGE_PPSW		0x00f2
#define RGE_TIMERINT3		0x00f4
#define RGE_RADMFIFO_PROTECT	0x0402
#define RGE_INTMITI(i)		(0x0a00 + (i) * 4)
#define RGE_PHYBASE		0x0a40
#define RGE_EPHYAR_EXT_ADDR	0x0ffe
#define RGE_ADDR0		0x19e0
#define RGE_ADDR1		0x19e4
#define RGE_RSS_CTRL		0x4500
#define RGE_RXQUEUE_CTRL	0x4800
#define RGE_EEE_TXIDLE_TIMER	0x6048

/* Flags for register RGE_INT_CFG0 */
#define RGE_INT_CFG0_EN			0x01
#define RGE_INT_CFG0_TIMEOUT_BYPASS	0x02
#define RGE_INT_CFG0_MITIGATION_BYPASS	0x04
#define RGE_INT_CFG0_RDU_BYPASS_8126	0x10
#define RGE_INT_CFG0_AVOID_MISS_INTR	0x40

/* Flags for register RGE_CMD */
#define RGE_CMD_RXBUF_EMPTY	0x01
#define RGE_CMD_TXENB		0x04
#define RGE_CMD_RXENB		0x08
#define RGE_CMD_RESET		0x10
#define RGE_CMD_STOPREQ		0x80

/* Flags for register RGE_ISR */
#define RGE_ISR_RX_OK		0x00000001
#define RGE_ISR_RX_ERR		0x00000002
#define RGE_ISR_TX_OK		0x00000004
#define RGE_ISR_TX_ERR		0x00000008
#define RGE_ISR_RX_DESC_UNAVAIL	0x00000010
#define RGE_ISR_LINKCHG		0x00000020
#define RGE_ISR_RX_FIFO_OFLOW	0x00000040
#define RGE_ISR_TX_DESC_UNAVAIL	0x00000080
#define RGE_ISR_SWI		0x00000100
#define RGE_ISR_PCS_TIMEOUT	0x00004000
#define RGE_ISR_SYSTEM_ERR	0x00008000

#define RGE_INTRS		\
	(RGE_ISR_RX_OK | RGE_ISR_RX_ERR | RGE_ISR_TX_OK |		\
	RGE_ISR_TX_ERR | RGE_ISR_LINKCHG | RGE_ISR_TX_DESC_UNAVAIL |	\
	RGE_ISR_PCS_TIMEOUT | RGE_ISR_SYSTEM_ERR)

#define RGE_INTRS_TIMER		\
	(RGE_ISR_RX_ERR | RGE_ISR_TX_ERR | RGE_ISR_PCS_TIMEOUT |	\
	RGE_ISR_SYSTEM_ERR)

/* Flags for register RGE_TXCFG */
#define RGE_TXCFG_HWREV		0x7cf00000

/* Flags for register RGE_RXCFG */
#define RGE_RXCFG_ALLPHYS	0x00000001
#define RGE_RXCFG_INDIV		0x00000002
#define RGE_RXCFG_MULTI		0x00000004
#define RGE_RXCFG_BROAD		0x00000008
#define RGE_RXCFG_RUNT		0x00000010
#define RGE_RXCFG_ERRPKT	0x00000020
#define RGE_RXCFG_VLANSTRIP	0x00c00000

/* Flags for register RGE_EECMD */
#define RGE_EECMD_WRITECFG	0xc0

/* Flags for register RGE_CFG1 */
#define RGE_CFG1_PM_EN		0x01
#define RGE_CFG1_SPEED_DOWN	0x10

/* Flags for register RGE_CFG2 */
#define RGE_CFG2_PMSTS_EN	0x20
#define RGE_CFG2_CLKREQ_EN	0x80

/* Flags for register RGE_CFG3 */
#define RGE_CFG3_RDY_TO_L23	0x02
#define RGE_CFG3_WOL_LINK	0x10
#define RGE_CFG3_WOL_MAGIC	0x20

/* Flags for register RGE_CFG5 */
#define RGE_CFG5_PME_STS	0x01
#define RGE_CFG5_WOL_LANWAKE	0x02
#define RGE_CFG5_WOL_UCAST	0x10
#define RGE_CFG5_WOL_MCAST	0x20
#define RGE_CFG5_WOL_BCAST	0x40

/* Flags for register RGE_CSIAR */
#define RGE_CSIAR_BYTE_EN	0x0000000f
#define RGE_CSIAR_BYTE_EN_SHIFT	12
#define RGE_CSIAR_ADDR_MASK	0x00000fff
#define RGE_CSIAR_BUSY		0x80000000

/* Flags for register RGE_PHYSTAT */
#define RGE_PHYSTAT_FDX		0x0001
#define RGE_PHYSTAT_LINK	0x0002
#define RGE_PHYSTAT_10MBPS	0x0004
#define RGE_PHYSTAT_100MBPS	0x0008
#define RGE_PHYSTAT_1000MBPS	0x0010
#define RGE_PHYSTAT_RXFLOW	0x0020
#define RGE_PHYSTAT_TXFLOW	0x0040
#define RGE_PHYSTAT_2500MBPS	0x0400
#define RGE_PHYSTAT_5000MBPS	0x1000
#define RGE_PHYSTAT_10000MBPS	0x4000

/* Flags for register RGE_EPHYAR */
#define RGE_EPHYAR_DATA_MASK	0x0000ffff
#define RGE_EPHYAR_BUSY		0x80000000
#define RGE_EPHYAR_ADDR_MASK	0x0000007f
#define RGE_EPHYAR_ADDR_SHIFT	16

/* Flags for register RGE_TXSTART */
#define RGE_TXSTART_START	0x0001

/* Flags for register RGE_MACOCP */
#define RGE_MACOCP_DATA_MASK	0x0000ffff
#define RGE_MACOCP_BUSY		0x80000000
#define RGE_MACOCP_ADDR_SHIFT	16

/* Flags for register RGE_PHYOCP */
#define RGE_PHYOCP_DATA_MASK	0x0000ffff
#define RGE_PHYOCP_BUSY		0x80000000
#define RGE_PHYOCP_ADDR_SHIFT	16

/* Flags for register RGE_DLLPR. */
#define RGE_DLLPR_PFM_EN	0x40
#define RGE_DLLPR_TX_10M_PS_EN	0x80

/* Flags for register RGE_MCUCMD */
#define RGE_MCUCMD_RXFIFO_EMPTY	0x10
#define RGE_MCUCMD_TXFIFO_EMPTY	0x20
#define RGE_MCUCMD_IS_OOB	0x80

/* Flags for register RGE_CPLUSCMD */
#define RGE_CPLUSCMD_RXCSUM	0x0020

#define RGE_TX_NSEGS		32

#define RGE_TX_LIST_CNT		1024
#define RGE_RX_LIST_CNT		1024

#define RGE_ALIGN		256
#define RGE_TX_LIST_SZ		(sizeof(struct rge_tx_desc) * RGE_TX_LIST_CNT)
#define RGE_RX_LIST_SZ		(sizeof(struct rge_rx_desc) * RGE_RX_LIST_CNT)
#define RGE_NEXT_TX_DESC(x)	(((x) + 1) % RGE_TX_LIST_CNT)
#define RGE_NEXT_RX_DESC(x)	(((x) + 1) % RGE_RX_LIST_CNT)
#define RGE_ADDR_LO(y)		((uint64_t) (y) & 0xffffffff)
#define RGE_ADDR_HI(y)		((uint64_t) (y) >> 32)

#define RGE_ADV_2500TFDX	0x0080
#define RGE_ADV_5000TFDX	0x0100
#define RGE_ADV_10000TFDX	0x1000

/* Tx descriptor */
struct rge_tx_desc {
	uint32_t		rge_cmdsts;
	uint32_t		rge_extsts;
	uint64_t		rge_addr;
	uint32_t		reserved[4];
} __packed __aligned(16);

#define RGE_TDCMDSTS_COLL	0x000f0000
#define RGE_TDCMDSTS_EXCESSCOLL	0x00100000
#define RGE_TDCMDSTS_TXERR	0x00800000
#define RGE_TDCMDSTS_EOF	0x10000000
#define RGE_TDCMDSTS_SOF	0x20000000
#define RGE_TDCMDSTS_EOR	0x40000000
#define RGE_TDCMDSTS_OWN	0x80000000

#define RGE_TDEXTSTS_VTAG	0x00020000
#define RGE_TDEXTSTS_IPCSUM	0x20000000
#define RGE_TDEXTSTS_TCPCSUM	0x40000000
#define RGE_TDEXTSTS_UDPCSUM	0x80000000

/* Rx descriptor */
struct rge_rx_desc {
	union {
		struct {
			uint32_t	rsvd0;
			uint32_t	rsvd1;
		} rx_qword0;
	} lo_qword0;

	union {
		struct {
			uint32_t	rss;
			uint16_t	length;
			uint16_t	hdr_info;
		} rx_qword1;

		struct {
			uint32_t	rsvd2;
			uint32_t	rsvd3;
		} rx_qword2;
	} lo_qword1;

	union {
		uint64_t		rge_addr;

		struct {
			uint64_t	timestamp;
		} rx_timestamp;

		struct {
			uint32_t	rsvd4;
			uint32_t	rsvd5;
		} rx_qword3;
	} hi_qword0;

	union {
		struct {
			uint32_t	rge_extsts;
			uint32_t	rge_cmdsts;
		} rx_qword4;

		struct {
			uint16_t	rsvd6;
			uint16_t	rsvd7;
			uint32_t	rsvd8;
		} rx_ptp;
	} hi_qword1;
} __packed __aligned(16);

#define RGE_RDCMDSTS_RXERRSUM	0x00100000
#define RGE_RDCMDSTS_EOF	0x01000000
#define RGE_RDCMDSTS_SOF	0x02000000
#define RGE_RDCMDSTS_EOR	0x40000000
#define RGE_RDCMDSTS_OWN	0x80000000
#define RGE_RDCMDSTS_FRAGLEN	0x00003fff

#define RGE_RDEXTSTS_VTAG	0x00010000
#define RGE_RDEXTSTS_VLAN_MASK	0x0000ffff
#define RGE_RDEXTSTS_TCPCSUMERR	0x01000000
#define RGE_RDEXTSTS_UDPCSUMERR	0x02000000
#define RGE_RDEXTSTS_IPCSUMERR	0x04000000
#define RGE_RDEXTSTS_TCPPKT	0x10000000
#define RGE_RDEXTSTS_UDPPKT	0x20000000
#define RGE_RDEXTSTS_IPV4	0x40000000
#define RGE_RDEXTSTS_IPV6	0x80000000

/*
 * @brief Statistics counter structure
 *
 * This is the layout of the hardware structure that
 * is populated by the hardware when RGE_DTCCR_* is
 * appropriately poked.
 */
struct rge_hw_mac_stats {
	uint64_t		rge_tx_ok;
	uint64_t		rge_rx_ok;
	uint64_t		rge_tx_er;
	uint32_t		rge_rx_er;
	uint16_t		rge_miss_pkt;
	uint16_t		rge_fae; /* frame align errors */
	uint32_t		rge_tx_1col; /* one collision */
	uint32_t		rge_tx_mcol; /* multple collisions */
	uint64_t		rge_rx_ok_phy; /* unicast */
	uint64_t		rge_rx_ok_brd; /* broadcasts */
	uint32_t		rge_rx_ok_mul; /* multicasts */
	uint16_t		rge_tx_abt;
	uint16_t		rge_tx_undrn;

	/* extended */
	uint64_t		re_tx_octets;
	uint64_t		re_rx_octets;
	uint64_t		re_rx_multicast64;
	uint64_t		re_tx_unicast64;
	uint64_t		re_tx_broadcast64;
	uint64_t		re_tx_multicast64;
	uint32_t		re_tx_pause_on;
	uint32_t		re_tx_pause_off;
	uint32_t		re_tx_pause_all;
	uint32_t		re_tx_deferred;
	uint32_t		re_tx_late_collision;
	uint32_t		re_tx_all_collision;
	uint32_t		re_tx_aborted32;
	uint32_t		re_align_errors32;
	uint32_t		re_rx_frame_too_long;
	uint32_t		re_rx_runt;
	uint32_t		re_rx_pause_on;
	uint32_t		re_rx_pause_off;
	uint32_t		re_rx_pause_all;
	uint32_t		re_rx_unknown_opcode;
	uint32_t		re_rx_mac_error;
	uint32_t		re_tx_underrun32;
	uint32_t		re_rx_mac_missed;
	uint32_t		re_rx_tcam_dropped;
	uint32_t		re_tdu;
	uint32_t		re_rdu;

} __packed __aligned(sizeof(uint64_t));

#define RGE_STATS_BUF_SIZE	sizeof(struct rge_hw_mac_stats)

#define RGE_STATS_ALIGNMENT	64

/* Ram version */
#define RGE_MAC_R25D_RCODE_VER		0x0027
#define RGE_MAC_R26_RCODE_VER		0x0033
#define RGE_MAC_R27_RCODE_VER		0x0036
#define RGE_MAC_R25_RCODE_VER		0x0b33
#define RGE_MAC_R25B_RCODE_VER		0x0b99

#define RGE_TIMEOUT		100

#define RGE_JUMBO_FRAMELEN	9216
#define RGE_JUMBO_MTU							\
	(RGE_JUMBO_FRAMELEN - ETHER_HDR_LEN - ETHER_CRC_LEN - 		\
	ETHER_VLAN_ENCAP_LEN)

#define RGE_TXCFG_CONFIG	0x03000700
#define RGE_RXCFG_CONFIG	0x41000700
#define RGE_RXCFG_CONFIG_8125B	0x41000c00
#define RGE_RXCFG_CONFIG_8125D	0x41200c00
#define RGE_RXCFG_CONFIG_8126	0x41200d00

#endif	/* __IF_RGEREG_H__ */
