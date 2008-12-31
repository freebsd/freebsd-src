/*-
 * Copyright (c) 2007 The DragonFly Project.  All rights reserved.
 * 
 * This code is derived from software contributed to The DragonFly Project
 * by Sepherosa Ziehau <sepherosa@gmail.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $DragonFly: src/sys/dev/netif/et/if_etreg.h,v 1.3 2007/10/23 14:28:42 sephe Exp $
 * $FreeBSD: src/sys/dev/et/if_etreg.h,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */
/*-
 * Portions of this code is derived from NetBSD which is covered by
 * the following license:
 *
 * Copyright (c) 2004, 2005 David Young.  All rights reserved.
 *
 * Programmed for NetBSD by David Young.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of David Young may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY David Young ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL David
 * Young BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * $DragonFly: src/sys/sys/bitops.h,v 1.1 2007/10/14 04:15:17 sephe Exp $
 */

#ifndef _IF_ETREG_H
#define _IF_ETREG_H

/*
 * __BIT(n): Return a bitmask with bit n set, where the least
 *           significant bit is bit 0.
 *
 * __BITS(m, n): Return a bitmask with bits m through n, inclusive,
 *               set.  It does not matter whether m>n or m<=n.  The
 *               least significant bit is bit 0.
 *
 * A "bitfield" is a span of consecutive bits defined by a bitmask,
 * where 1s select the bits in the bitfield.  __SHIFTIN, __SHIFTOUT,
 * and __SHIFTOUT_MASK help read and write bitfields from device
 * registers.
 *
 * __SHIFTIN(v, mask): Left-shift bits `v' into the bitfield
 *                     defined by `mask', and return them.  No
 *                     side-effects.
 *
 * __SHIFTOUT(v, mask): Extract and return the bitfield selected
 *                      by `mask' from `v', right-shifting the
 *                      bits so that the rightmost selected bit
 *                      is at bit 0.  No side-effects.
 *
 * __SHIFTOUT_MASK(mask): Right-shift the bits in `mask' so that
 *                        the rightmost non-zero bit is at bit
 *                        0.  This is useful for finding the
 *                        greatest unsigned value that a bitfield
 *                        can hold.  No side-effects.  Note that
 *                        __SHIFTOUT_MASK(m) = __SHIFTOUT(m, m).
 */

/* __BIT(n): nth bit, where __BIT(0) == 0x1. */
#define	__BIT(__n) (((__n) == 32) ? 0 : ((uint32_t)1 << (__n)))

/* __BITS(m, n): bits m through n, m < n. */
#define	__BITS(__m, __n)	\
	((__BIT(MAX((__m), (__n)) + 1) - 1) ^ (__BIT(MIN((__m), (__n))) - 1))

/* Find least significant bit that is set */
#define	__LOWEST_SET_BIT(__mask) ((((__mask) - 1) & (__mask)) ^ (__mask))

#define	__SHIFTOUT(__x, __mask) (((__x) & (__mask)) / __LOWEST_SET_BIT(__mask))
#define	__SHIFTIN(__x, __mask) ((__x) * __LOWEST_SET_BIT(__mask))
#define	__SHIFTOUT_MASK(__mask) __SHIFTOUT((__mask), (__mask))

#define ET_MEM_TXSIZE_EX		182
#define ET_MEM_RXSIZE_MIN		608
#define ET_MEM_RXSIZE_DEFAULT		11216
#define ET_MEM_SIZE			16384
#define ET_MEM_UNIT			16

/*
 * PCI registers
 *
 * ET_PCIV_ACK_LATENCY_{128,256} are from
 * PCI EXPRESS BASE SPECIFICATION, REV. 1.0a, Table 3-5
 *
 * ET_PCIV_REPLAY_TIMER_{128,256} are from
 * PCI EXPRESS BASE SPECIFICATION, REV. 1.0a, Table 3-4
 */
#define ET_PCIR_BAR			PCIR_BAR(0)

#define ET_PCIR_DEVICE_CAPS		0x4c
#define ET_PCIM_DEVICE_CAPS_MAX_PLSZ	0x7	/* Max playload size */
#define ET_PCIV_DEVICE_CAPS_PLSZ_128	0x0
#define ET_PCIV_DEVICE_CAPS_PLSZ_256	0x1

#define ET_PCIR_DEVICE_CTRL		0x50
#define ET_PCIM_DEVICE_CTRL_MAX_RRSZ	0x7000	/* Max read request size */
#define ET_PCIV_DEVICE_CTRL_RRSZ_2K	0x4000

#define ET_PCIR_MAC_ADDR0		0xa4
#define ET_PCIR_MAC_ADDR1		0xa8

#define ET_PCIR_EEPROM_STATUS		0xb2	/* XXX undocumented */
#define ET_PCIM_EEPROM_STATUS_ERROR	0x4c

#define ET_PCIR_ACK_LATENCY		0xc0
#define ET_PCIV_ACK_LATENCY_128		237
#define ET_PCIV_ACK_LATENCY_256		416

#define ET_PCIR_REPLAY_TIMER		0xc2
#define ET_REPLAY_TIMER_RX_L0S_ADJ	250	/* XXX infered from default */
#define ET_PCIV_REPLAY_TIMER_128	(711 + ET_REPLAY_TIMER_RX_L0S_ADJ)
#define ET_PCIV_REPLAY_TIMER_256	(1248 + ET_REPLAY_TIMER_RX_L0S_ADJ)

#define ET_PCIR_L0S_L1_LATENCY		0xcf
#define ET_PCIM_L0S_LATENCY		__BITS(2, 0)
#define ET_PCIM_L1_LATENCY		__BITS(5, 3)
#define ET_PCIV_L0S_LATENCY(l)		__SHIFTIN((l) - 1, ET_PCIM_L0S_LATENCY)
#define ET_PCIV_L1_LATENCY(l)		__SHIFTIN((l) - 1, ET_PCIM_L1_LATENCY)

/*
 * CSR
 */
#define ET_TXQUEUE_START		0x0000
#define ET_TXQUEUE_END			0x0004
#define ET_RXQUEUE_START		0x0008
#define ET_RXQUEUE_END			0x000c
#define ET_QUEUE_ADDR(addr)		(((addr) / ET_MEM_UNIT) - 1)
#define ET_QUEUE_ADDR_START		0
#define ET_QUEUE_ADDR_END		ET_QUEUE_ADDR(ET_MEM_SIZE)

#define ET_PM				0x0010
#define ET_PM_SYSCLK_GATE		__BIT(3)
#define ET_PM_TXCLK_GATE		__BIT(4)
#define ET_PM_RXCLK_GATE		__BIT(5)

#define ET_INTR_STATUS			0x0018
#define ET_INTR_MASK			0x001c

#define ET_SWRST			0x0028
#define ET_SWRST_TXDMA			__BIT(0)
#define ET_SWRST_RXDMA			__BIT(1)
#define ET_SWRST_TXMAC			__BIT(2)
#define ET_SWRST_RXMAC			__BIT(3)
#define ET_SWRST_MAC			__BIT(4)
#define ET_SWRST_MAC_STAT		__BIT(5)
#define ET_SWRST_MMC			__BIT(6)
#define ET_SWRST_SELFCLR_DISABLE	__BIT(31)

#define ET_MSI_CFG			0x0030

#define ET_LOOPBACK			0x0034

#define ET_TIMER			0x0038

#define ET_TXDMA_CTRL			0x1000
#define ET_TXDMA_CTRL_HALT		__BIT(0)
#define ET_TXDMA_CTRL_CACHE_THR		__BITS(7, 4)
#define ET_TXDMA_CTRL_SINGLE_EPKT	__BIT(8)	/* ??? */

#define ET_TX_RING_HI			0x1004
#define ET_TX_RING_LO			0x1008
#define ET_TX_RING_CNT			0x100c

#define ET_TX_STATUS_HI			0x101c
#define ET_TX_STATUS_LO			0x1020

#define ET_TX_READY_POS			0x1024
#define ET_TX_READY_POS_INDEX		__BITS(9, 0)
#define ET_TX_READY_POS_WRAP		__BIT(10)

#define ET_TX_DONE_POS			0x1060
#define ET_TX_DONE_POS_INDEX		__BITS(9, 0)
#define ET_TX_DONE_POS_WRAP		__BIT(10)

#define ET_RXDMA_CTRL			0x2000
#define ET_RXDMA_CTRL_HALT		__BIT(0)
#define ET_RXDMA_CTRL_RING0_SIZE	__BITS(9, 8)
#define ET_RXDMA_CTRL_RING0_128		0		/* 127 */
#define ET_RXDMA_CTRL_RING0_256		1		/* 255 */
#define ET_RXDMA_CTRL_RING0_512		2		/* 511 */
#define ET_RXDMA_CTRL_RING0_1024	3		/* 1023 */
#define ET_RXDMA_CTRL_RING0_ENABLE	__BIT(10)
#define ET_RXDMA_CTRL_RING1_SIZE	__BITS(12, 11)
#define ET_RXDMA_CTRL_RING1_2048	0		/* 2047 */
#define ET_RXDMA_CTRL_RING1_4096	1		/* 4095 */
#define ET_RXDMA_CTRL_RING1_8192	2		/* 8191 */
#define ET_RXDMA_CTRL_RING1_16384	3		/* 16383 (9022?) */
#define ET_RXDMA_CTRL_RING1_ENABLE	__BIT(13)
#define ET_RXDMA_CTRL_HALTED		__BIT(17)

#define ET_RX_STATUS_LO			0x2004
#define ET_RX_STATUS_HI			0x2008

#define ET_RX_INTR_NPKTS		0x200c
#define ET_RX_INTR_DELAY		0x2010

#define ET_RXSTAT_LO			0x2020
#define ET_RXSTAT_HI			0x2024
#define ET_RXSTAT_CNT			0x2028

#define ET_RXSTAT_POS			0x2030
#define ET_RXSTAT_POS_INDEX		__BITS(11, 0)
#define ET_RXSTAT_POS_WRAP		__BIT(12)

#define ET_RXSTAT_MINCNT		0x2038

#define ET_RX_RING0_LO			0x203c
#define ET_RX_RING0_HI			0x2040
#define ET_RX_RING0_CNT			0x2044

#define ET_RX_RING0_POS			0x204c
#define ET_RX_RING0_POS_INDEX		__BITS(9, 0)
#define ET_RX_RING0_POS_WRAP		__BIT(10)

#define ET_RX_RING0_MINCNT		0x2054

#define ET_RX_RING1_LO			0x2058
#define ET_RX_RING1_HI			0x205c
#define ET_RX_RING1_CNT			0x2060

#define ET_RX_RING1_POS			0x2068
#define ET_RX_RING1_POS_INDEX		__BITS(9, 0)
#define ET_RX_RING1_POS_WRAP		__BIT(10)

#define ET_RX_RING1_MINCNT		0x2070

#define ET_TXMAC_CTRL			0x3000
#define ET_TXMAC_CTRL_ENABLE		__BIT(0)
#define ET_TXMAC_CTRL_FC_DISABLE	__BIT(3)

#define ET_TXMAC_FLOWCTRL		0x3010

#define ET_RXMAC_CTRL			0x4000
#define ET_RXMAC_CTRL_ENABLE		__BIT(0)
#define ET_RXMAC_CTRL_NO_PKTFILT	__BIT(2)
#define ET_RXMAC_CTRL_WOL_DISABLE	__BIT(3)

#define ET_WOL_CRC			0x4004
#define ET_WOL_SA_LO			0x4010
#define ET_WOL_SA_HI			0x4014
#define ET_WOL_MASK			0x4018

#define ET_UCAST_FILTADDR1		0x4068
#define ET_UCAST_FILTADDR2		0x406c
#define ET_UCAST_FILTADDR3		0x4070

#define ET_MULTI_HASH			0x4074

#define ET_PKTFILT			0x4084
#define ET_PKTFILT_BCAST		__BIT(0)
#define ET_PKTFILT_MCAST		__BIT(1)
#define ET_PKTFILT_UCAST		__BIT(2)
#define ET_PKTFILT_FRAG			__BIT(3)
#define ET_PKTFILT_MINLEN		__BITS(22, 16)

#define ET_RXMAC_MC_SEGSZ		0x4088
#define ET_RXMAC_MC_SEGSZ_ENABLE	__BIT(0)
#define ET_RXMAC_MC_SEGSZ_FC		__BIT(1)
#define ET_RXMAC_MC_SEGSZ_MAX		__BITS(9, 2)
#define ET_RXMAC_SEGSZ(segsz)		((segsz) / ET_MEM_UNIT)
#define ET_RXMAC_CUT_THRU_FRMLEN	8074

#define ET_RXMAC_MC_WATERMARK		0x408c
#define ET_RXMAC_SPACE_AVL		0x4094

#define ET_RXMAC_MGT			0x4098
#define ET_RXMAC_MGT_PASS_ECRC		__BIT(4)
#define ET_RXMAC_MGT_PASS_ELEN		__BIT(5)
#define ET_RXMAC_MGT_PASS_ETRUNC	__BIT(16)
#define ET_RXMAC_MGT_CHECK_PKT		__BIT(17)

#define ET_MAC_CFG1			0x5000
#define ET_MAC_CFG1_TXEN		__BIT(0)
#define ET_MAC_CFG1_SYNC_TXEN		__BIT(1)
#define ET_MAC_CFG1_RXEN		__BIT(2)
#define ET_MAC_CFG1_SYNC_RXEN		__BIT(3)
#define ET_MAC_CFG1_TXFLOW		__BIT(4)
#define ET_MAC_CFG1_RXFLOW		__BIT(5)
#define ET_MAC_CFG1_LOOPBACK		__BIT(8)
#define ET_MAC_CFG1_RST_TXFUNC		__BIT(16)
#define ET_MAC_CFG1_RST_RXFUNC		__BIT(17)
#define ET_MAC_CFG1_RST_TXMC		__BIT(18)
#define ET_MAC_CFG1_RST_RXMC		__BIT(19)
#define ET_MAC_CFG1_SIM_RST		__BIT(30)
#define ET_MAC_CFG1_SOFT_RST		__BIT(31)

#define ET_MAC_CFG2			0x5004
#define ET_MAC_CFG2_FDX			__BIT(0)
#define ET_MAC_CFG2_CRC			__BIT(1)
#define ET_MAC_CFG2_PADCRC		__BIT(2)
#define ET_MAC_CFG2_LENCHK		__BIT(4)
#define ET_MAC_CFG2_BIGFRM		__BIT(5)
#define ET_MAC_CFG2_MODE_MII		__BIT(8)
#define ET_MAC_CFG2_MODE_GMII		__BIT(9)
#define ET_MAC_CFG2_PREAMBLE_LEN	__BITS(15, 12)

#define ET_IPG				0x5008
#define ET_IPG_B2B			__BITS(6, 0)
#define ET_IPG_MINIFG			__BITS(15, 8)
#define ET_IPG_NONB2B_2			__BITS(22, 16)
#define ET_IPG_NONB2B_1			__BITS(30, 24)

#define ET_MAC_HDX			0x500c
#define ET_MAC_HDX_COLLWIN		__BITS(9, 0)
#define ET_MAC_HDX_REXMIT_MAX		__BITS(15, 12)
#define ET_MAC_HDX_EXC_DEFER		__BIT(16)
#define ET_MAC_HDX_NOBACKOFF		__BIT(17)
#define ET_MAC_HDX_BP_NOBACKOFF		__BIT(18)
#define ET_MAC_HDX_ALT_BEB		__BIT(19)
#define ET_MAC_HDX_ALT_BEB_TRUNC	__BITS(23, 20)

#define ET_MAX_FRMLEN			0x5010

#define ET_MII_CFG			0x5020
#define ET_MII_CFG_CLKRST		__BITS(2, 0)
#define ET_MII_CFG_PREAMBLE_SUP		__BIT(4)
#define ET_MII_CFG_SCAN_AUTOINC		__BIT(5)
#define ET_MII_CFG_RST			__BIT(31)

#define ET_MII_CMD			0x5024
#define ET_MII_CMD_READ			__BIT(0)

#define ET_MII_ADDR			0x5028
#define ET_MII_ADDR_REG			__BITS(4, 0)
#define ET_MII_ADDR_PHY			__BITS(12, 8)

#define ET_MII_CTRL			0x502c
#define ET_MII_CTRL_VALUE		__BITS(15, 0) 

#define ET_MII_STAT			0x5030
#define ET_MII_STAT_VALUE		__BITS(15, 0)

#define ET_MII_IND			0x5034
#define ET_MII_IND_BUSY			__BIT(0)
#define ET_MII_IND_INVALID		__BIT(2)

#define ET_MAC_CTRL			0x5038
#define ET_MAC_CTRL_MODE_MII		__BIT(24)
#define ET_MAC_CTRL_LHDX		__BIT(25)
#define ET_MAC_CTRL_GHDX		__BIT(26)

#define ET_MAC_ADDR1			0x5040
#define ET_MAC_ADDR2			0x5044

#define ET_MMC_CTRL			0x7000
#define ET_MMC_CTRL_ENABLE		__BIT(0)
#define ET_MMC_CTRL_ARB_DISABLE		__BIT(1)
#define ET_MMC_CTRL_RXMAC_DISABLE	__BIT(2)
#define ET_MMC_CTRL_TXMAC_DISABLE	__BIT(3)
#define ET_MMC_CTRL_TXDMA_DISABLE	__BIT(4)
#define ET_MMC_CTRL_RXDMA_DISABLE	__BIT(5)
#define ET_MMC_CTRL_FORCE_CE		__BIT(6)

/*
 * Interrupts
 */
#define ET_INTR_TXEOF			__BIT(3)
#define ET_INTR_TXDMA_ERROR		__BIT(4)
#define ET_INTR_RXEOF			__BIT(5)
#define ET_INTR_RXRING0_LOW		__BIT(6)
#define ET_INTR_RXRING1_LOW		__BIT(7)
#define ET_INTR_RXSTAT_LOW		__BIT(8)
#define ET_INTR_RXDMA_ERROR		__BIT(9)
#define ET_INTR_TIMER			__BIT(14)
#define ET_INTR_WOL			__BIT(15)
#define ET_INTR_PHY			__BIT(16)
#define ET_INTR_TXMAC			__BIT(17)
#define ET_INTR_RXMAC			__BIT(18)
#define ET_INTR_MAC_STATS		__BIT(19)
#define ET_INTR_SLAVE_TO		__BIT(20)

#define ET_INTRS			(ET_INTR_TXEOF | \
					 ET_INTR_RXEOF | \
					 ET_INTR_TIMER)

/*
 * RX ring position uses same layout
 */
#define ET_RX_RING_POS_INDEX		__BITS(9, 0)
#define ET_RX_RING_POS_WRAP		__BIT(10)

/*
 * PCI IDs
 */
#define PCI_VENDOR_LUCENT		0x11c1
#define PCI_PRODUCT_LUCENT_ET1310	0xed00		/* ET1310 10/100/1000M Ethernet */
#define PCI_PRODUCT_LUCENT_ET1310_FAST	0xed01		/* ET1310 10/100M Ethernet */

#endif	/* !_IF_ETREG_H */
