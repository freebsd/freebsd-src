/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>
 * Copyright (c) 2014 - 2016, The Linux Foundation. All rights reserved.
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
 * Copyright (c) 2014 - 2016, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

 */

#ifndef	__QCOM_ESS_EDMA_REG_H__
#define	__QCOM_ESS_EDMA_REG_H__

/*
 * Alignment of descriptor ring memory allocation.
 */
#define	EDMA_DESC_RING_ALIGN		PAGE_SIZE

/* Not sure if this is really valid or not */
#define	EDMA_DESC_MAX_BUFFER_SIZE	4096

/* The hardware can accept both of these, so we don't need bounce buffers! */
#define	ESS_EDMA_TX_BUFFER_ALIGN	1
#define	ESS_EDMA_RX_BUFFER_ALIGN	1

/* register definition */
#define	EDMA_REG_MAS_CTRL		0x0
#define	EDMA_REG_TIMEOUT_CTRL		0x004
#define	EDMA_REG_DBG0			0x008
#define	EDMA_REG_DBG1			0x00C
#define	EDMA_REG_SW_CTRL0		0x100
#define	EDMA_REG_SW_CTRL1		0x104

/* Interrupt Status Register */
#define	EDMA_REG_RX_ISR			0x200
#define	EDMA_REG_TX_ISR			0x208
#define	EDMA_REG_MISC_ISR		0x210
#define	EDMA_REG_WOL_ISR		0x218

#define	EDMA_MISC_ISR_RX_URG_Q(x)	(1U << (x)x)

#define	EDMA_MISC_ISR_AXIR_TIMEOUT 0x00000100
#define	EDMA_MISC_ISR_AXIR_ERR 0x00000200
#define	EDMA_MISC_ISR_TXF_DEAD 0x00000400
#define	EDMA_MISC_ISR_AXIW_ERR 0x00000800
#define	EDMA_MISC_ISR_AXIW_TIMEOUT 0x00001000

#define	EDMA_WOL_ISR 0x00000001

/* Interrupt Mask Register */
#define	EDMA_REG_MISC_IMR		0x214
#define	EDMA_REG_WOL_IMR		0x218

#define	EDMA_RX_IMR_NORMAL_MASK 0x1
#define	EDMA_TX_IMR_NORMAL_MASK 0x1
#define	EDMA_MISC_IMR_NORMAL_MASK 0x80001FFF
#define	EDMA_WOL_IMR_NORMAL_MASK 0x1

/* Edma receive consumer index */
#define	EDMA_REG_RX_SW_CONS_IDX_Q(x) (0x220 + ((x) << 2)) /* x is the queue id */
/* Edma transmit consumer index */
#define	EDMA_REG_TX_SW_CONS_IDX_Q(x) (0x240 + ((x) << 2)) /* x is the queue id */

/* IRQ Moderator Initial Timer Register */
#define	EDMA_REG_IRQ_MODRT_TIMER_INIT		0x280
#define		EDMA_IRQ_MODRT_TIMER_MASK	0xFFFF
#define		EDMA_IRQ_MODRT_RX_TIMER_SHIFT	0
#define		EDMA_IRQ_MODRT_TX_TIMER_SHIFT	16

/* Interrupt Control Register */
#define	EDMA_REG_INTR_CTRL 0x284
#define	EDMA_INTR_CLR_TYP_SHIFT 0
#define	EDMA_INTR_SW_IDX_W_TYP_SHIFT 1
#define	EDMA_INTR_CLEAR_TYPE_W1 0
#define	EDMA_INTR_CLEAR_TYPE_R 1

/* RX Interrupt Mask Register */
#define	EDMA_REG_RX_INT_MASK_Q(x)	(0x300 + ((x) << 2)) /* x = queue id */

/* TX Interrupt mask register */
#define	EDMA_REG_TX_INT_MASK_Q(x)	(0x340 + ((x) << 2)) /* x = queue id */

/* Load Ptr Register
 * Software sets this bit after the initialization of the head and tail
 */
#define	EDMA_REG_TX_SRAM_PART 0x400
#define	EDMA_LOAD_PTR_SHIFT 16

/* TXQ Control Register */
#define	EDMA_REG_TXQ_CTRL			0x404
#define		EDMA_TXQ_CTRL_IP_OPTION_EN	0x10
#define		EDMA_TXQ_CTRL_TXQ_EN		0x20
#define		EDMA_TXQ_CTRL_ENH_MODE		0x40
#define		EDMA_TXQ_CTRL_LS_8023_EN	0x80
#define		EDMA_TXQ_CTRL_TPD_BURST_EN	0x100
#define		EDMA_TXQ_CTRL_LSO_BREAK_EN	0x200
#define		EDMA_TXQ_NUM_TPD_BURST_MASK	0xF
#define		EDMA_TXQ_TXF_BURST_NUM_MASK	0xFFFF
#define		EDMA_TXQ_NUM_TPD_BURST_SHIFT	0
#define		EDMA_TXQ_TXF_BURST_NUM_SHIFT	16

#define	EDMA_REG_TXF_WATER_MARK			0x408 /* In 8-bytes */
#define		EDMA_TXF_WATER_MARK_MASK	0x0FFF
#define		EDMA_TXF_LOW_WATER_MARK_SHIFT	0
#define		EDMA_TXF_HIGH_WATER_MARK_SHIFT	16
#define		EDMA_TXQ_CTRL_BURST_MODE_EN	0x80000000

/* WRR Control Register */
#define	EDMA_REG_WRR_CTRL_Q0_Q3			0x40c
#define	EDMA_REG_WRR_CTRL_Q4_Q7			0x410
#define	EDMA_REG_WRR_CTRL_Q8_Q11		0x414
#define	EDMA_REG_WRR_CTRL_Q12_Q15		0x418

/* Weight round robin(WRR), it takes queue as input, and computes
 * starting bits where we need to write the weight for a particular
 * queue
 */
#define	EDMA_WRR_SHIFT(x) (((x) * 5) % 20)

/* Tx Descriptor Control Register */
#define	EDMA_REG_TPD_RING_SIZE			0x41C
#define	EDMA_TPD_RING_SIZE_SHIFT		0
#define	EDMA_TPD_RING_SIZE_MASK			0xFFFF

/* Transmit descriptor base address */
#define	EDMA_REG_TPD_BASE_ADDR_Q(x)		(0x420 + ((x) << 2)) /* x = queue id */

/* TPD Index Register */
#define	EDMA_REG_TPD_IDX_Q(x) (0x460 + ((x) << 2)) /* x = queue id */

#define	EDMA_TPD_PROD_IDX_BITS 0x0000FFFF
#define	EDMA_TPD_CONS_IDX_BITS 0xFFFF0000
#define	EDMA_TPD_PROD_IDX_MASK 0xFFFF
#define	EDMA_TPD_CONS_IDX_MASK 0xFFFF
#define	EDMA_TPD_PROD_IDX_SHIFT 0
#define	EDMA_TPD_CONS_IDX_SHIFT 16

/* TX Virtual Queue Mapping Control Register */
#define	EDMA_REG_VQ_CTRL0 0x4A0
#define	EDMA_REG_VQ_CTRL1 0x4A4

/* Virtual QID shift, it takes queue as input, and computes
 * Virtual QID position in virtual qid control register
 */
#define	EDMA_VQ_ID_SHIFT(i) (((i) * 3) % 24)

/* Virtual Queue Default Value */
#define	EDMA_VQ_REG_VALUE 0x240240

/* Tx side Port Interface Control Register */
#define	EDMA_REG_PORT_CTRL 0x4A8
#define	EDMA_PAD_EN_SHIFT 15

/* Tx side VLAN Configuration Register */
#define	EDMA_REG_VLAN_CFG 0x4AC

#define	EDMA_TX_CVLAN 16
#define	EDMA_TX_INS_CVLAN 17
#define	EDMA_TX_CVLAN_TAG_SHIFT 0

#define	EDMA_TX_SVLAN 14
#define	EDMA_TX_INS_SVLAN 15
#define	EDMA_TX_SVLAN_TAG_SHIFT 16

/* Tx Queue Packet Statistic Register */
#define	EDMA_REG_TX_STAT_PKT_Q(x) (0x700 + ((x) << 3)) /* x = queue id */

#define	EDMA_TX_STAT_PKT_MASK 0xFFFFFF

/* Tx Queue Byte Statistic Register */
#define	EDMA_REG_TX_STAT_BYTE_Q(x) (0x704 + ((x) << 3)) /* x = queue id */

/* Load Balance Based Ring Offset Register */
#define	EDMA_REG_LB_RING 0x800
#define	EDMA_LB_RING_ENTRY_MASK 0xff
#define	EDMA_LB_RING_ID_MASK 0x7
#define	EDMA_LB_RING_PROFILE_ID_MASK 0x3
#define	EDMA_LB_RING_ENTRY_BIT_OFFSET 8
#define	EDMA_LB_RING_ID_OFFSET 0
#define	EDMA_LB_RING_PROFILE_ID_OFFSET 3
#define	EDMA_LB_REG_VALUE 0x6040200

/* Load Balance Priority Mapping Register */
#define	EDMA_REG_LB_PRI_START 0x804
#define	EDMA_REG_LB_PRI_END 0x810
#define	EDMA_LB_PRI_REG_INC 4
#define	EDMA_LB_PRI_ENTRY_BIT_OFFSET 4
#define	EDMA_LB_PRI_ENTRY_MASK 0xf

/* RSS Priority Mapping Register */
#define	EDMA_REG_RSS_PRI 0x820
#define	EDMA_RSS_PRI_ENTRY_MASK 0xf
#define	EDMA_RSS_RING_ID_MASK 0x7
#define	EDMA_RSS_PRI_ENTRY_BIT_OFFSET 4

/* RSS Indirection Register */
#define	EDMA_REG_RSS_IDT(x) (0x840 + ((x) << 2)) /* x = No. of indirection table */
#define	EDMA_NUM_IDT 16
#define	EDMA_RSS_IDT_VALUE 0x64206420

/* Default RSS Ring Register */
#define	EDMA_REG_DEF_RSS 0x890
#define	EDMA_DEF_RSS_MASK 0x7

/* RSS Hash Function Type Register */
#define	EDMA_REG_RSS_TYPE 0x894
#define	EDMA_RSS_TYPE_NONE 0x01
#define	EDMA_RSS_TYPE_IPV4TCP 0x02
#define	EDMA_RSS_TYPE_IPV6_TCP 0x04
#define	EDMA_RSS_TYPE_IPV4_UDP 0x08
#define	EDMA_RSS_TYPE_IPV6UDP 0x10
#define	EDMA_RSS_TYPE_IPV4 0x20
#define	EDMA_RSS_TYPE_IPV6 0x40
#define	EDMA_RSS_HASH_MODE_MASK 0x7f

#define	EDMA_REG_RSS_HASH_VALUE 0x8C0

#define	EDMA_REG_RSS_TYPE_RESULT 0x8C4


/* rrd5 */
#define	EDMA_HASH_TYPE_SHIFT				12
#define	EDMA_HASH_TYPE_MASK				0xf
#define		EDMA_RRD_RSS_TYPE_NONE			0
#define		EDMA_RRD_RSS_TYPE_IPV4TCP		1
#define		EDMA_RRD_RSS_TYPE_IPV6_TCP		2
#define		EDMA_RRD_RSS_TYPE_IPV4_UDP		3
#define		EDMA_RRD_RSS_TYPE_IPV6UDP		4
#define		EDMA_RRD_RSS_TYPE_IPV4			5
#define		EDMA_RRD_RSS_TYPE_IPV6			6

#define	EDMA_RFS_FLOW_ENTRIES 1024
#define	EDMA_RFS_FLOW_ENTRIES_MASK (EDMA_RFS_FLOW_ENTRIES - 1)
#define	EDMA_RFS_EXPIRE_COUNT_PER_CALL 128

/* RFD Base Address Register */
#define	EDMA_REG_RFD_BASE_ADDR_Q(x) (0x950 + ((x) << 2)) /* x = queue id */

/* RFD Index Register */
#define	EDMA_REG_RFD_IDX_Q(x) (0x9B0 + ((x) << 2))

#define	EDMA_RFD_PROD_IDX_BITS 0x00000FFF
#define	EDMA_RFD_CONS_IDX_BITS 0x0FFF0000
#define	EDMA_RFD_PROD_IDX_MASK 0xFFF
#define	EDMA_RFD_CONS_IDX_MASK 0xFFF
#define	EDMA_RFD_PROD_IDX_SHIFT 0
#define	EDMA_RFD_CONS_IDX_SHIFT 16

/* Rx Descriptor Control Register */
#define	EDMA_REG_RX_DESC0 0xA10
#define	EDMA_RFD_RING_SIZE_MASK 0xFFF
#define	EDMA_RX_BUF_SIZE_MASK 0xFFFF
#define	EDMA_RFD_RING_SIZE_SHIFT 0
#define	EDMA_RX_BUF_SIZE_SHIFT 16

#define	EDMA_REG_RX_DESC1 0xA14
#define	EDMA_RXQ_RFD_BURST_NUM_MASK 0x3F
#define	EDMA_RXQ_RFD_PF_THRESH_MASK 0x1F
#define	EDMA_RXQ_RFD_LOW_THRESH_MASK 0xFFF
#define	EDMA_RXQ_RFD_BURST_NUM_SHIFT 0
#define	EDMA_RXQ_RFD_PF_THRESH_SHIFT 8
#define	EDMA_RXQ_RFD_LOW_THRESH_SHIFT 16

/* RXQ Control Register */
#define	EDMA_REG_RXQ_CTRL 0xA18
#define	EDMA_FIFO_THRESH_TYPE_SHIF 0
#define	EDMA_FIFO_THRESH_128_BYTE 0x0
#define	EDMA_FIFO_THRESH_64_BYTE 0x1
#define	EDMA_RXQ_CTRL_RMV_VLAN 0x00000002
#define	EDMA_RXQ_CTRL_EN 0x0000FF00

/* AXI Burst Size Config */
#define	EDMA_REG_AXIW_CTRL_MAXWRSIZE 0xA1C
#define	EDMA_AXIW_MAXWRSIZE_VALUE 0x0

/* Rx Statistics Register */
#define	EDMA_REG_RX_STAT_BYTE_Q(x) (0xA30 + ((x) << 2)) /* x = queue id */
#define	EDMA_REG_RX_STAT_PKT_Q(x) (0xA50 + ((x) << 2)) /* x = queue id */

/* WoL Pattern Length Register */
#define	EDMA_REG_WOL_PATTERN_LEN0 0xC00
#define	EDMA_WOL_PT_LEN_MASK 0xFF
#define	EDMA_WOL_PT0_LEN_SHIFT 0
#define	EDMA_WOL_PT1_LEN_SHIFT 8
#define	EDMA_WOL_PT2_LEN_SHIFT 16
#define	EDMA_WOL_PT3_LEN_SHIFT 24

#define	EDMA_REG_WOL_PATTERN_LEN1 0xC04
#define	EDMA_WOL_PT4_LEN_SHIFT 0
#define	EDMA_WOL_PT5_LEN_SHIFT 8
#define	EDMA_WOL_PT6_LEN_SHIFT 16

/* WoL Control Register */
#define	EDMA_REG_WOL_CTRL 0xC08
#define	EDMA_WOL_WK_EN 0x00000001
#define	EDMA_WOL_MG_EN 0x00000002
#define	EDMA_WOL_PT0_EN 0x00000004
#define	EDMA_WOL_PT1_EN 0x00000008
#define	EDMA_WOL_PT2_EN 0x00000010
#define	EDMA_WOL_PT3_EN 0x00000020
#define	EDMA_WOL_PT4_EN 0x00000040
#define	EDMA_WOL_PT5_EN 0x00000080
#define	EDMA_WOL_PT6_EN 0x00000100

/* MAC Control Register */
#define	EDMA_REG_MAC_CTRL0 0xC20
#define	EDMA_REG_MAC_CTRL1 0xC24

/* WoL Pattern Register */
#define	EDMA_REG_WOL_PATTERN_START 0x5000
#define	EDMA_PATTERN_PART_REG_OFFSET 0x40

/* TX descriptor fields */
#define	EDMA_TPD_HDR_SHIFT 0
#define	EDMA_TPD_PPPOE_EN 0x00000100
#define	EDMA_TPD_IP_CSUM_EN 0x00000200
#define	EDMA_TPD_TCP_CSUM_EN 0x0000400
#define	EDMA_TPD_UDP_CSUM_EN 0x00000800
#define	EDMA_TPD_CUSTOM_CSUM_EN 0x00000C00
#define	EDMA_TPD_LSO_EN 0x00001000
#define	EDMA_TPD_LSO_V2_EN 0x00002000
#define	EDMA_TPD_IPV4_EN 0x00010000
#define	EDMA_TPD_MSS_MASK 0x1FFF
#define	EDMA_TPD_MSS_SHIFT 18
#define	EDMA_TPD_CUSTOM_CSUM_SHIFT 18
#define	EDMA_TPD_EOP	0x80000000

/* word3 */
#define	EDMA_TPD_PORT_BITMAP_SHIFT 18
#define	EDMA_TPD_FROM_CPU_SHIFT 25
#define	EDMA_FROM_CPU_MASK 0x80

/* TX descriptor - little endian */
struct qcom_ess_edma_tx_desc {
	uint16_t len; /* full packet including CRC */
	uint16_t svlan_tag; /* vlan tag */
	uint32_t word1; /* byte 4-7 */
	uint32_t addr; /* address of buffer */
	uint32_t word3; /* byte 12 */
} __packed;

/* RRD descriptor fields */
#define	EDMA_RRD_NUM_RFD_MASK 0x000F
#define	EDMA_RRD_SVLAN 0x8000
#define	EDMA_RRD_FLOW_COOKIE_MASK 0x07FF

#define	EDMA_RRD_PKT_SIZE_MASK 0x3FFF
#define	EDMA_RRD_CSUM_FAIL_MASK 0xC000
#define	EDMA_RRD_CVLAN 0x0001
#define	EDMA_RRD_DESC_VALID 0x8000

#define	EDMA_RRD_PRIORITY_SHIFT 4
#define	EDMA_RRD_PRIORITY_MASK 0x7
#define	EDMA_RRD_PORT_TYPE_SHIFT 7
#define	EDMA_RRD_PORT_TYPE_MASK 0x1F

#define	EDMA_PORT_ID_SHIFT 12
#define	EDMA_PORT_ID_MASK 0x7

/* RX RRD descriptor - 16 bytes */
struct qcom_edma_rx_return_desc {
	uint16_t rrd0;
	uint16_t rrd1;
	uint16_t rrd2;
	uint16_t rrd3;
	uint16_t rrd4;
	uint16_t rrd5;
	uint16_t rrd6;
	uint16_t rrd7;
} __packed;


/* RX RFD descriptor - little endian */
struct qcom_ess_edma_rx_free_desc {
	uint32_t	addr; /* buffer addr */
} __packed;

#define	ESS_RGMII_CTRL		0x0004

/* Configurations */
#define EDMA_INTR_CLEAR_TYPE 0
#define EDMA_INTR_SW_IDX_W_TYPE 0
#define EDMA_FIFO_THRESH_TYPE 0
#define EDMA_RSS_TYPE 0
#define EDMA_RX_IMT 0x0020
#define EDMA_TX_IMT 0x0050
#define EDMA_TPD_BURST 5
#define EDMA_TXF_BURST 0x100
#define EDMA_RFD_BURST 8
#define EDMA_RFD_THR 16
#define EDMA_RFD_LTHR 0

#endif	/* __QCOM_ESS_EDMA_REG_H__ */
