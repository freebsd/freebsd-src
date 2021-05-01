/* SPDX-License-Identifier: BSD-2-Clause-NetBSD AND BSD-3-Clause */
/*	$NetBSD: qatreg.h,v 1.1 2019/11/20 09:37:46 hikaru Exp $	*/

/*
 * Copyright (c) 2019 Internet Initiative Japan, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *   Copyright(c) 2007-2019 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $FreeBSD$ */

#ifndef _DEV_PCI_QATREG_H_
#define _DEV_PCI_QATREG_H_

#define	__BIT(__n)						\
	(((uintmax_t)(__n) >= NBBY * sizeof(uintmax_t)) ? 0 :	\
	((uintmax_t)1 << (uintmax_t)((__n) & (NBBY * sizeof(uintmax_t) - 1))))
#define	__BITS(__m, __n)		\
	((__BIT(MAX((__m), (__n)) + 1) - 1) ^ (__BIT(MIN((__m), (__n))) - 1))

#define	__LOWEST_SET_BIT(__mask)	((((__mask) - 1) & (__mask)) ^ (__mask))
#define	__SHIFTOUT(__x, __mask) (((__x) & (__mask)) / __LOWEST_SET_BIT(__mask))
#define	__SHIFTIN(__x, __mask)		((__x) * __LOWEST_SET_BIT(__mask))

/* Limits */
#define MAX_NUM_AE		0x10
#define MAX_NUM_ACCEL		6
#define MAX_AE			0x18
#define MAX_AE_CTX		8
#define MAX_ARB			4

#define MAX_USTORE_PER_SEG	0x8000	/* 16k * 2 */
#define MAX_USTORE		MAX_USTORE_PER_SEG

#define MAX_AE_PER_ACCEL	4	/* XXX */
#define MAX_BANK_PER_ACCEL	16	/* XXX */
#define MAX_RING_PER_BANK	16

#define MAX_XFER_REG		128
#define MAX_GPR_REG		128
#define MAX_NN_REG		128
#define MAX_LMEM_REG		1024
#define MAX_INP_STATE		16
#define MAX_CAM_REG		16
#define MAX_FIFO_QWADDR		160

#define MAX_EXEC_INST		100
#define UWORD_CPYBUF_SIZE	1024	/* micro-store copy buffer (bytes) */
#define INVLD_UWORD		0xffffffffffull	/* invalid micro-instruction */
#define AEV2_PACKED_UWORD_BYTES	6	/* version 2 packed uword size */
#define UWORD_MASK		0xbffffffffffull  /* micro-word mask without parity */

#define AE_ALL_CTX		0xff

/* PCIe configuration space parameter */
#define NO_PCI_REG			(-1)
#define NO_REG_OFFSET			0

#define MAX_BARS			3

/* Fuse Control */
#define FUSECTL_REG			0x40
#define FUSECTL_MASK			__BIT(31)

#define LEGFUSE_REG			0x4c
#define LEGFUSE_ACCEL_MASK_CIPHER_SLICE		__BIT(0)
#define LEGFUSE_ACCEL_MASK_AUTH_SLICE		__BIT(1)
#define LEGFUSE_ACCEL_MASK_PKE_SLICE		__BIT(2)
#define LEGFUSE_ACCEL_MASK_COMPRESS_SLICE	__BIT(3)
#define LEGFUSE_ACCEL_MASK_LZS_SLICE		__BIT(4)
#define LEGFUSE_ACCEL_MASK_EIA3_SLICE		__BIT(5)
#define LEGFUSE_ACCEL_MASK_SHA3_SLICE		__BIT(6)

/* -------------------------------------------------------------------------- */
/* PETRINGCSR region */

/* ETR parameters */
#define ETR_MAX_RINGS_PER_BANK	16

/* ETR registers */
#define ETR_RING_CONFIG		0x0000
#define ETR_RING_LBASE		0x0040
#define ETR_RING_UBASE		0x0080
#define ETR_RING_HEAD_OFFSET	0x00C0
#define ETR_RING_TAIL_OFFSET	0x0100
#define ETR_RING_STAT		0x0140
#define ETR_UO_STAT		0x0148
#define ETR_E_STAT		0x014C
#define ETR_NE_STAT		0x0150
#define ETR_NF_STAT		0x0154
#define ETR_F_STAT		0x0158
#define ETR_C_STAT		0x015C
#define ETR_INT_EN		0x016C
#define ETR_INT_REG		0x0170
#define ETR_INT_SRCSEL		0x0174
#define ETR_INT_SRCSEL_2	0x0178
#define ETR_INT_COL_EN		0x017C
#define ETR_INT_COL_CTL		0x0180
#define ETR_AP_NF_MASK		0x2000
#define ETR_AP_NF_DEST		0x2020
#define ETR_AP_NE_MASK		0x2040
#define ETR_AP_NE_DEST		0x2060
#define ETR_AP_DELAY		0x2080

/* ARB registers */
#define ARB_OFFSET			0x30000
#define ARB_REG_SIZE			0x4
#define ARB_WTR_SIZE			0x20
#define ARB_REG_SLOT			0x1000
#define ARB_WTR_OFFSET			0x010
#define ARB_RO_EN_OFFSET		0x090
#define ARB_WRK_2_SER_MAP_OFFSET	0x180
#define ARB_RINGSRVARBEN_OFFSET		0x19c

/* Ring Config */
#define ETR_RING_CONFIG_LATE_HEAD_POINTER_MODE		__BIT(31)
#define ETR_RING_CONFIG_NEAR_FULL_WM			__BITS(14, 10)
#define ETR_RING_CONFIG_NEAR_EMPTY_WM			__BITS(9, 5)
#define ETR_RING_CONFIG_RING_SIZE			__BITS(4, 0)

#define ETR_RING_CONFIG_NEAR_WM_0		0x00
#define ETR_RING_CONFIG_NEAR_WM_4		0x01
#define ETR_RING_CONFIG_NEAR_WM_8		0x02
#define ETR_RING_CONFIG_NEAR_WM_16		0x03
#define ETR_RING_CONFIG_NEAR_WM_32		0x04
#define ETR_RING_CONFIG_NEAR_WM_64		0x05
#define ETR_RING_CONFIG_NEAR_WM_128		0x06
#define ETR_RING_CONFIG_NEAR_WM_256		0x07
#define ETR_RING_CONFIG_NEAR_WM_512		0x08
#define ETR_RING_CONFIG_NEAR_WM_1K		0x09
#define ETR_RING_CONFIG_NEAR_WM_2K		0x0A
#define ETR_RING_CONFIG_NEAR_WM_4K		0x0B
#define ETR_RING_CONFIG_NEAR_WM_8K		0x0C
#define ETR_RING_CONFIG_NEAR_WM_16K		0x0D
#define ETR_RING_CONFIG_NEAR_WM_32K		0x0E
#define ETR_RING_CONFIG_NEAR_WM_64K		0x0F
#define ETR_RING_CONFIG_NEAR_WM_128K		0x10
#define ETR_RING_CONFIG_NEAR_WM_256K		0x11
#define ETR_RING_CONFIG_NEAR_WM_512K		0x12
#define ETR_RING_CONFIG_NEAR_WM_1M		0x13
#define ETR_RING_CONFIG_NEAR_WM_2M		0x14
#define ETR_RING_CONFIG_NEAR_WM_4M		0x15

#define ETR_RING_CONFIG_SIZE_64			0x00
#define ETR_RING_CONFIG_SIZE_128		0x01
#define ETR_RING_CONFIG_SIZE_256		0x02
#define ETR_RING_CONFIG_SIZE_512		0x03
#define ETR_RING_CONFIG_SIZE_1K			0x04
#define ETR_RING_CONFIG_SIZE_2K			0x05
#define ETR_RING_CONFIG_SIZE_4K			0x06
#define ETR_RING_CONFIG_SIZE_8K			0x07
#define ETR_RING_CONFIG_SIZE_16K		0x08
#define ETR_RING_CONFIG_SIZE_32K		0x09
#define ETR_RING_CONFIG_SIZE_64K		0x0A
#define ETR_RING_CONFIG_SIZE_128K		0x0B
#define ETR_RING_CONFIG_SIZE_256K		0x0C
#define ETR_RING_CONFIG_SIZE_512K		0x0D
#define ETR_RING_CONFIG_SIZE_1M			0x0E
#define ETR_RING_CONFIG_SIZE_2M			0x0F
#define ETR_RING_CONFIG_SIZE_4M			0x10

/* Default Ring Config is Nearly Full = Full and Nearly Empty = Empty */
#define ETR_RING_CONFIG_BUILD(size)					\
		(__SHIFTIN(ETR_RING_CONFIG_NEAR_WM_0,			\
		    ETR_RING_CONFIG_NEAR_FULL_WM) |			\
		__SHIFTIN(ETR_RING_CONFIG_NEAR_WM_0,			\
		    ETR_RING_CONFIG_NEAR_EMPTY_WM) |			\
		__SHIFTIN((size), ETR_RING_CONFIG_RING_SIZE))

/* Response Ring Configuration */
#define ETR_RING_CONFIG_BUILD_RESP(size, wm_nf, wm_ne)			\
		(__SHIFTIN((wm_nf), ETR_RING_CONFIG_NEAR_FULL_WM) |	\
		__SHIFTIN((wm_ne), ETR_RING_CONFIG_NEAR_EMPTY_WM) |	\
		__SHIFTIN((size), ETR_RING_CONFIG_RING_SIZE))

/* Ring Base */
#define ETR_RING_BASE_BUILD(addr, size)					\
		(((addr) >> 6) & (0xFFFFFFFFFFFFFFFFULL << (size)))

#define ETR_INT_REG_CLEAR_MASK	0xffff

/* Initial bank Interrupt Source mask */
#define ETR_INT_SRCSEL_MASK	0x44444444UL

#define ETR_INT_SRCSEL_NEXT_OFFSET	4

#define ETR_RINGS_PER_INT_SRCSEL	8

#define ETR_INT_COL_CTL_ENABLE	__BIT(31)

#define ETR_AP_NF_MASK_INIT	0xAAAAAAAA
#define ETR_AP_NE_MASK_INIT	0x55555555

/* Autopush destination AE bit */
#define ETR_AP_DEST_ENABLE	__BIT(7)
#define ETR_AP_DEST_AE		__BITS(6, 2)
#define ETR_AP_DEST_MAILBOX	__BITS(1, 0)

/* Autopush destination enable bit */

/* Autopush CSR Offset */
#define ETR_AP_BANK_OFFSET		4

/* Autopush maximum rings per bank */
#define ETR_MAX_RINGS_PER_AP_BANK		32

/* Maximum mailbox per acclerator */
#define ETR_MAX_MAILBOX_PER_ACCELERATOR		4

/* Maximum AEs per mailbox */
#define ETR_MAX_AE_PER_MAILBOX			4

/* Macro to get the ring's autopush bank number */
#define ETR_RING_AP_BANK_NUMBER(ring)	((ring) >> 5)

/* Macro to get the ring's autopush mailbox number */
#define ETR_RING_AP_MAILBOX_NUMBER(ring)		\
    (ETR_RING_AP_BANK_NUMBER(ring) % ETR_MAX_MAILBOX_PER_ACCELERATOR)

/* Macro to get the ring number in the autopush bank */
#define ETR_RING_NUMBER_IN_AP_BANK(ring)	\
	((ring) % ETR_MAX_RINGS_PER_AP_BANK)

#define ETR_RING_EMPTY_ENTRY_SIG	(0x7F7F7F7F)

/* -------------------------------------------------------------------------- */
/* CAP_GLOBAL_CTL region */

#define FCU_CTRL			0x8c0
#define FCU_CTRL_CMD_NOOP		0
#define FCU_CTRL_CMD_AUTH		1
#define FCU_CTRL_CMD_LOAD		2
#define FCU_CTRL_CMD_START		3
#define FCU_CTRL_AE			__BITS(8, 31)

#define FCU_STATUS			0x8c4
#define FCU_STATUS_STS			__BITS(0, 2)
#define FCU_STATUS_STS_NO		0
#define FCU_STATUS_STS_VERI_DONE	1
#define FCU_STATUS_STS_LOAD_DONE	2
#define FCU_STATUS_STS_VERI_FAIL	3
#define FCU_STATUS_STS_LOAD_FAIL	4
#define FCU_STATUS_STS_BUSY		5
#define FCU_STATUS_AUTHFWLD		__BIT(8)
#define FCU_STATUS_DONE			__BIT(9)
#define FCU_STATUS_LOADED_AE		__BITS(22, 31)

#define FCU_STATUS1			0x8c8

#define FCU_DRAM_ADDR_LO	0x8cc
#define FCU_DRAM_ADDR_HI	0x8d0
#define FCU_RAMBASE_ADDR_HI	0x8d4
#define FCU_RAMBASE_ADDR_LO	0x8d8

#define FW_AUTH_WAIT_PERIOD 10
#define FW_AUTH_MAX_RETRY   300

#define CAP_GLOBAL_CTL_BASE			0xa00
#define CAP_GLOBAL_CTL_MISC			CAP_GLOBAL_CTL_BASE + 0x04
#define CAP_GLOBAL_CTL_MISC_TIMESTAMP_EN	__BIT(7)
#define CAP_GLOBAL_CTL_RESET			CAP_GLOBAL_CTL_BASE + 0x0c
#define CAP_GLOBAL_CTL_RESET_MASK		__BITS(31, 26)
#define CAP_GLOBAL_CTL_RESET_ACCEL_MASK		__BITS(25, 20)
#define CAP_GLOBAL_CTL_RESET_AE_MASK		__BITS(19, 0)
#define CAP_GLOBAL_CTL_CLK_EN			CAP_GLOBAL_CTL_BASE + 0x50
#define CAP_GLOBAL_CTL_CLK_EN_ACCEL_MASK	__BITS(25, 20)
#define CAP_GLOBAL_CTL_CLK_EN_AE_MASK		__BITS(19, 0)

/* -------------------------------------------------------------------------- */
/* AE region */
#define UPC_MASK		0x1ffff
#define USTORE_SIZE		QAT_16K

#define AE_LOCAL_AE_MASK			__BITS(31, 12)
#define AE_LOCAL_CSR_MASK			__BITS(9, 0)

/* AE_LOCAL registers */
/* Control Store Address Register */
#define USTORE_ADDRESS				0x000
#define USTORE_ADDRESS_ECS			__BIT(31)

#define USTORE_ECC_BIT_0	44
#define USTORE_ECC_BIT_1	45
#define USTORE_ECC_BIT_2	46
#define USTORE_ECC_BIT_3	47
#define USTORE_ECC_BIT_4	48
#define USTORE_ECC_BIT_5	49
#define USTORE_ECC_BIT_6	50

/* Control Store Data Lower Register */
#define USTORE_DATA_LOWER			0x004
/* Control Store Data Upper Register */
#define USTORE_DATA_UPPER			0x008
/* Control Store Error Status Register */
#define USTORE_ERROR_STATUS			0x00c
/* Arithmetic Logic Unit Output Register */
#define ALU_OUT					0x010
/* Context Arbiter Control Register */
#define CTX_ARB_CNTL				0x014
#define CTX_ARB_CNTL_INIT			0x00000000
/* Context Enables Register */
#define CTX_ENABLES				0x018
#define CTX_ENABLES_INIT			0
#define CTX_ENABLES_INUSE_CONTEXTS		__BIT(31)
#define CTX_ENABLES_CNTL_STORE_PARITY_ERROR	__BIT(29)
#define CTX_ENABLES_CNTL_STORE_PARITY_ENABLE	__BIT(28)
#define CTX_ENABLES_BREAKPOINT			__BIT(27)
#define CTX_ENABLES_PAR_ERR			__BIT(25)
#define CTX_ENABLES_NN_MODE			__BIT(20)
#define CTX_ENABLES_NN_RING_EMPTY		__BIT(18)
#define CTX_ENABLES_LMADDR_1_GLOBAL		__BIT(17)
#define CTX_ENABLES_LMADDR_0_GLOBAL		__BIT(16)
#define CTX_ENABLES_ENABLE			__BITS(15,8)

#define CTX_ENABLES_IGNORE_W1C_MASK		\
		(~(CTX_ENABLES_PAR_ERR |	\
		CTX_ENABLES_BREAKPOINT |	\
		CTX_ENABLES_CNTL_STORE_PARITY_ERROR))

/* cycles from CTX_ENABLE high to CTX entering executing state */
#define CYCLES_FROM_READY2EXE	8

/* Condition Code Enable Register */
#define CC_ENABLE				0x01c
#define CC_ENABLE_INIT				0x2000

/* CSR Context Pointer Register */
#define CSR_CTX_POINTER				0x020
#define CSR_CTX_POINTER_CONTEXT			__BITS(2,0)
/* Register Error Status Register */
#define REG_ERROR_STATUS			0x030
/* Indirect Context Status Register */
#define CTX_STS_INDIRECT			0x040
#define CTX_STS_INDIRECT_UPC_INIT		0x00000000

/* Active Context Status Register */
#define ACTIVE_CTX_STATUS			0x044
#define ACTIVE_CTX_STATUS_ABO			__BIT(31)
#define ACTIVE_CTX_STATUS_ACNO			__BITS(0, 2)
/* Indirect Context Signal Events Register */
#define CTX_SIG_EVENTS_INDIRECT			0x048
#define CTX_SIG_EVENTS_INDIRECT_INIT		0x00000001
/* Active Context Signal Events Register */
#define CTX_SIG_EVENTS_ACTIVE			0x04c
/* Indirect Context Wakeup Events Register */
#define CTX_WAKEUP_EVENTS_INDIRECT		0x050
#define CTX_WAKEUP_EVENTS_INDIRECT_VOLUNTARY	0x00000001
#define CTX_WAKEUP_EVENTS_INDIRECT_SLEEP	0x00010000

#define CTX_WAKEUP_EVENTS_INDIRECT_INIT		0x00000001

/* Active Context Wakeup Events Register */
#define CTX_WAKEUP_EVENTS_ACTIVE		0x054
/* Indirect Context Future Count Register */
#define CTX_FUTURE_COUNT_INDIRECT		0x058
/* Active Context Future Count Register */
#define CTX_FUTURE_COUNT_ACTIVE		0x05c
/* Indirect Local Memory Address 0 Register */
#define LM_ADDR_0_INDIRECT			0x060
/* Active Local Memory Address 0 Register */
#define LM_ADDR_0_ACTIVE			0x064
/* Indirect Local Memory Address 1 Register */
#define LM_ADDR_1_INDIRECT			0x068
/* Active Local Memory Address 1 Register */
#define LM_ADDR_1_ACTIVE			0x06c
/* Byte Index Register */
#define BYTE_INDEX				0x070
/* Indirect Local Memory Address 0 Byte Index Register */
#define INDIRECT_LM_ADDR_0_BYTE_INDEX		0x0e0
/* Active Local Memory Address 0 Byte Index Register */
#define ACTIVE_LM_ADDR_0_BYTE_INDEX		0x0e4
/* Indirect Local Memory Address 1 Byte Index Register */
#define INDIRECT_LM_ADDR_1_BYTE_INDEX		0x0e8
/* Active Local Memory Address 1 Byte Index Register */
#define ACTIVE_LM_ADDR_1_BYTE_INDEX		0x0ec
/* Transfer Index Concatenated with Byte Index Register */
#define T_INDEX_BYTE_INDEX			0x0f4
/* Transfer Index Register */
#define T_INDEX				0x074
/* Indirect Future Count Signal Signal Register */
#define FUTURE_COUNT_SIGNAL_INDIRECT		0x078
/* Active Context Future Count Register */
#define FUTURE_COUNT_SIGNAL_ACTIVE		0x07c
/* Next Neighbor Put Register */
#define NN_PUT					0x080
/* Next Neighbor Get Register */
#define NN_GET					0x084
/* Timestamp Low Register */
#define TIMESTAMP_LOW				0x0c0
/* Timestamp High Register */
#define TIMESTAMP_HIGH				0x0c4
/* Next Neighbor Signal Register */
#define NEXT_NEIGHBOR_SIGNAL			0x100
/* Previous Neighbor Signal Register */
#define PREV_NEIGHBOR_SIGNAL			0x104
/* Same AccelEngine Signal Register */
#define SAME_AE_SIGNAL				0x108
/* Cyclic Redundancy Check Remainder Register */
#define CRC_REMAINDER				0x140
/* Profile Count Register */
#define PROFILE_COUNT				0x144
/* Pseudorandom Number Register */
#define PSEUDO_RANDOM_NUMBER			0x148
/* Signature Enable Register */
#define SIGNATURE_ENABLE			0x150
/* Miscellaneous Control Register */
#define AE_MISC_CONTROL			0x160
#define AE_MISC_CONTROL_PARITY_ENABLE		__BIT(24)
#define AE_MISC_CONTROL_FORCE_BAD_PARITY	__BIT(23)
#define AE_MISC_CONTROL_ONE_CTX_RELOAD		__BIT(22)
#define AE_MISC_CONTROL_CS_RELOAD		__BITS(21, 20)
#define AE_MISC_CONTROL_SHARE_CS		__BIT(2)
/* Control Store Address 1 Register */
#define USTORE_ADDRESS1			0x158
/* Local CSR Status Register */
#define LOCAL_CSR_STATUS					0x180
#define LOCAL_CSR_STATUS_STATUS				0x1
/* NULL Register */
#define NULL_CSR				0x3fc

/* AE_XFER macros */
#define AE_XFER_AE_MASK					__BITS(31, 12)
#define AE_XFER_CSR_MASK				__BITS(9, 2)

#define AEREG_BAD_REGADDR	0xffff		/* bad register address */

/* -------------------------------------------------------------------------- */

#define SSMWDT(i)				((i) * 0x4000 + 0x54)
#define SSMWDTPKE(i)				((i) * 0x4000 + 0x58)
#define INTSTATSSM(i)				((i) * 0x4000 + 0x04)
#define INTSTATSSM_SHANGERR			__BIT(13)
#define PPERR(i)				((i) * 0x4000 + 0x08)
#define PPERRID(i)				((i) * 0x4000 + 0x0C)
#define CERRSSMSH(i)				((i) * 0x4000 + 0x10)
#define UERRSSMSH(i)				((i) * 0x4000 + 0x18)
#define UERRSSMSHAD(i)				((i) * 0x4000 + 0x1C)
#define SLICEHANGSTATUS(i)			((i) * 0x4000 + 0x4C)
#define SLICE_HANG_AUTH0_MASK			__BIT(0)
#define SLICE_HANG_AUTH1_MASK			__BIT(1)
#define SLICE_HANG_CPHR0_MASK			__BIT(4)
#define SLICE_HANG_CPHR1_MASK			__BIT(5)
#define SLICE_HANG_CMP0_MASK			__BIT(8)
#define SLICE_HANG_CMP1_MASK			__BIT(9)
#define SLICE_HANG_XLT0_MASK			__BIT(12)
#define SLICE_HANG_XLT1_MASK			__BIT(13)
#define SLICE_HANG_MMP0_MASK			__BIT(16)
#define SLICE_HANG_MMP1_MASK			__BIT(17)
#define SLICE_HANG_MMP2_MASK			__BIT(18)
#define SLICE_HANG_MMP3_MASK			__BIT(19)
#define SLICE_HANG_MMP4_MASK			__BIT(20)

#define SHINTMASKSSM(i)				((i) * 0x4000 + 0x1018)
#define ENABLE_SLICE_HANG			0x000000
#define MAX_MMP (5)
#define MMP_BASE(i)				((i) * 0x1000 % 0x3800)
#define CERRSSMMMP(i, n)			((i) * 0x4000 + MMP_BASE(n) + 0x380)
#define UERRSSMMMP(i, n)			((i) * 0x4000 + MMP_BASE(n) + 0x388)
#define UERRSSMMMPAD(i, n)			((i) * 0x4000 + MMP_BASE(n) + 0x38C)

#define CPP_CFC_ERR_STATUS			(0x30000 + 0xC04)
#define CPP_CFC_ERR_PPID			(0x30000 + 0xC08)

#define ERRSOU0					(0x3A000 + 0x00)
#define ERRSOU1					(0x3A000 + 0x04)
#define ERRSOU2					(0x3A000 + 0x08)
#define ERRSOU3					(0x3A000 + 0x0C)
#define ERRSOU4					(0x3A000 + 0xD0)
#define ERRSOU5					(0x3A000 + 0xD8)
#define ERRMSK0					(0x3A000 + 0x10)
#define ERRMSK1					(0x3A000 + 0x14)
#define ERRMSK2					(0x3A000 + 0x18)
#define ERRMSK3					(0x3A000 + 0x1C)
#define ERRMSK4					(0x3A000 + 0xD4)
#define ERRMSK5					(0x3A000 + 0xDC)
#define EMSK3_CPM0_MASK				__BIT(2)
#define EMSK3_CPM1_MASK				__BIT(3)
#define EMSK5_CPM2_MASK				__BIT(16)
#define EMSK5_CPM3_MASK				__BIT(17)
#define EMSK5_CPM4_MASK				__BIT(18)
#define RICPPINTSTS				(0x3A000 + 0x114)
#define RIERRPUSHID				(0x3A000 + 0x118)
#define RIERRPULLID				(0x3A000 + 0x11C)

#define TICPPINTSTS				(0x3A400 + 0x13C)
#define TIERRPUSHID				(0x3A400 + 0x140)
#define TIERRPULLID				(0x3A400 + 0x144)
#define SECRAMUERR				(0x3AC00 + 0x04)
#define SECRAMUERRAD				(0x3AC00 + 0x0C)
#define CPPMEMTGTERR				(0x3AC00 + 0x10)
#define ERRPPID					(0x3AC00 + 0x14)

#define ADMINMSGUR				0x3a574
#define ADMINMSGLR				0x3a578
#define MAILBOX_BASE				0x20970
#define MAILBOX_STRIDE				0x1000
#define ADMINMSG_LEN				32

/* -------------------------------------------------------------------------- */
static const uint8_t mailbox_const_tab[1024] __aligned(1024) = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x01,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x02, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x13, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13,
0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76,
0x54, 0x32, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x67, 0x45, 0x23, 0x01, 0xef, 0xcd, 0xab,
0x89, 0x98, 0xba, 0xdc, 0xfe, 0x10, 0x32, 0x54, 0x76, 0xc3, 0xd2, 0xe1, 0xf0,
0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc1, 0x05, 0x9e,
0xd8, 0x36, 0x7c, 0xd5, 0x07, 0x30, 0x70, 0xdd, 0x17, 0xf7, 0x0e, 0x59, 0x39,
0xff, 0xc0, 0x0b, 0x31, 0x68, 0x58, 0x15, 0x11, 0x64, 0xf9, 0x8f, 0xa7, 0xbe,
0xfa, 0x4f, 0xa4, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6a, 0x09, 0xe6, 0x67, 0xbb, 0x67, 0xae,
0x85, 0x3c, 0x6e, 0xf3, 0x72, 0xa5, 0x4f, 0xf5, 0x3a, 0x51, 0x0e, 0x52, 0x7f,
0x9b, 0x05, 0x68, 0x8c, 0x1f, 0x83, 0xd9, 0xab, 0x5b, 0xe0, 0xcd, 0x19, 0x05,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0xcb, 0xbb, 0x9d, 0x5d, 0xc1, 0x05, 0x9e, 0xd8, 0x62, 0x9a, 0x29,
0x2a, 0x36, 0x7c, 0xd5, 0x07, 0x91, 0x59, 0x01, 0x5a, 0x30, 0x70, 0xdd, 0x17,
0x15, 0x2f, 0xec, 0xd8, 0xf7, 0x0e, 0x59, 0x39, 0x67, 0x33, 0x26, 0x67, 0xff,
0xc0, 0x0b, 0x31, 0x8e, 0xb4, 0x4a, 0x87, 0x68, 0x58, 0x15, 0x11, 0xdb, 0x0c,
0x2e, 0x0d, 0x64, 0xf9, 0x8f, 0xa7, 0x47, 0xb5, 0x48, 0x1d, 0xbe, 0xfa, 0x4f,
0xa4, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x6a, 0x09, 0xe6, 0x67, 0xf3, 0xbc, 0xc9, 0x08, 0xbb,
0x67, 0xae, 0x85, 0x84, 0xca, 0xa7, 0x3b, 0x3c, 0x6e, 0xf3, 0x72, 0xfe, 0x94,
0xf8, 0x2b, 0xa5, 0x4f, 0xf5, 0x3a, 0x5f, 0x1d, 0x36, 0xf1, 0x51, 0x0e, 0x52,
0x7f, 0xad, 0xe6, 0x82, 0xd1, 0x9b, 0x05, 0x68, 0x8c, 0x2b, 0x3e, 0x6c, 0x1f,
0x1f, 0x83, 0xd9, 0xab, 0xfb, 0x41, 0xbd, 0x6b, 0x5b, 0xe0, 0xcd, 0x19, 0x13,
0x7e, 0x21, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/* -------------------------------------------------------------------------- */
/* Microcode */

/* Clear GPR of AE */
static const uint64_t ae_clear_gprs_inst[] = {
	0x0F0000C0000ull,  /* .0 l0000!val = 0 ; immed[l0000!val, 0x0] */
	0x0F000000380ull,  /* .1 l0000!count = 128 ; immed[l0000!count, 0x80] */
	0x0D805000011ull,  /* .2 br!=ctx[0, ctx_init#] */
	0x0FC082C0300ull,  /* .3 local_csr_wr[nn_put, 0] */
	0x0F0000C0300ull,  /* .4 nop */
	0x0F0000C0300ull,  /* .5 nop */
	0x0F0000C0300ull,  /* .6 nop */
	0x0F0000C0300ull,  /* .7 nop */
	0x0A0643C0000ull,  /* .8 init_nn#:alu[*n$index++, --, b, l0000!val] */
	0x0BAC0000301ull,  /* .9 alu[l0000!count, l0000!count, -, 1] */
	0x0D802000101ull,  /* .10 bne[init_nn#] */
	0x0F0000C0001ull,  /* .11 l0000!indx = 0 ; immed[l0000!indx, 0x0] */
	0x0FC066C0001ull,  /* .12 local_csr_wr[active_lm_addr_0, l0000!indx];
			    * put indx to lm_addr */
	0x0F0000C0300ull,  /* .13 nop */
	0x0F0000C0300ull,  /* .14 nop */
	0x0F0000C0300ull,  /* .15 nop */
	0x0F000400300ull,  /* .16 l0000!count = 1024 ; immed[l0000!count, 0x400] */
	0x0A0610C0000ull,  /* .17 init_lm#:alu[*l$index0++, --, b, l0000!val] */
	0x0BAC0000301ull,  /* .18 alu[l0000!count, l0000!count, -, 1] */
	0x0D804400101ull,  /* .19 bne[init_lm#] */
	0x0A0580C0000ull,  /* .20 ctx_init#:alu[$l0000!xfers[0], --, b, l0000!val] */
	0x0A0581C0000ull,  /* .21 alu[$l0000!xfers[1], --, b, l0000!val] */
	0x0A0582C0000ull,  /* .22 alu[$l0000!xfers[2], --, b, l0000!val] */
	0x0A0583C0000ull,  /* .23 alu[$l0000!xfers[3], --, b, l0000!val] */
	0x0A0584C0000ull,  /* .24 alu[$l0000!xfers[4], --, b, l0000!val] */
	0x0A0585C0000ull,  /* .25 alu[$l0000!xfers[5], --, b, l0000!val] */
	0x0A0586C0000ull,  /* .26 alu[$l0000!xfers[6], --, b, l0000!val] */
	0x0A0587C0000ull,  /* .27 alu[$l0000!xfers[7], --, b, l0000!val] */
	0x0A0588C0000ull,  /* .28 alu[$l0000!xfers[8], --, b, l0000!val] */
	0x0A0589C0000ull,  /* .29 alu[$l0000!xfers[9], --, b, l0000!val] */
	0x0A058AC0000ull,  /* .30 alu[$l0000!xfers[10], --, b, l0000!val] */
	0x0A058BC0000ull,  /* .31 alu[$l0000!xfers[11], --, b, l0000!val] */
	0x0A058CC0000ull,  /* .32 alu[$l0000!xfers[12], --, b, l0000!val] */
	0x0A058DC0000ull,  /* .33 alu[$l0000!xfers[13], --, b, l0000!val] */
	0x0A058EC0000ull,  /* .34 alu[$l0000!xfers[14], --, b, l0000!val] */
	0x0A058FC0000ull,  /* .35 alu[$l0000!xfers[15], --, b, l0000!val] */
	0x0A05C0C0000ull,  /* .36 alu[$l0000!xfers[16], --, b, l0000!val] */
	0x0A05C1C0000ull,  /* .37 alu[$l0000!xfers[17], --, b, l0000!val] */
	0x0A05C2C0000ull,  /* .38 alu[$l0000!xfers[18], --, b, l0000!val] */
	0x0A05C3C0000ull,  /* .39 alu[$l0000!xfers[19], --, b, l0000!val] */
	0x0A05C4C0000ull,  /* .40 alu[$l0000!xfers[20], --, b, l0000!val] */
	0x0A05C5C0000ull,  /* .41 alu[$l0000!xfers[21], --, b, l0000!val] */
	0x0A05C6C0000ull,  /* .42 alu[$l0000!xfers[22], --, b, l0000!val] */
	0x0A05C7C0000ull,  /* .43 alu[$l0000!xfers[23], --, b, l0000!val] */
	0x0A05C8C0000ull,  /* .44 alu[$l0000!xfers[24], --, b, l0000!val] */
	0x0A05C9C0000ull,  /* .45 alu[$l0000!xfers[25], --, b, l0000!val] */
	0x0A05CAC0000ull,  /* .46 alu[$l0000!xfers[26], --, b, l0000!val] */
	0x0A05CBC0000ull,  /* .47 alu[$l0000!xfers[27], --, b, l0000!val] */
	0x0A05CCC0000ull,  /* .48 alu[$l0000!xfers[28], --, b, l0000!val] */
	0x0A05CDC0000ull,  /* .49 alu[$l0000!xfers[29], --, b, l0000!val] */
	0x0A05CEC0000ull,  /* .50 alu[$l0000!xfers[30], --, b, l0000!val] */
	0x0A05CFC0000ull,  /* .51 alu[$l0000!xfers[31], --, b, l0000!val] */
	0x0A0400C0000ull,  /* .52 alu[l0000!gprega[0], --, b, l0000!val] */
	0x0B0400C0000ull,  /* .53 alu[l0000!gpregb[0], --, b, l0000!val] */
	0x0A0401C0000ull,  /* .54 alu[l0000!gprega[1], --, b, l0000!val] */
	0x0B0401C0000ull,  /* .55 alu[l0000!gpregb[1], --, b, l0000!val] */
	0x0A0402C0000ull,  /* .56 alu[l0000!gprega[2], --, b, l0000!val] */
	0x0B0402C0000ull,  /* .57 alu[l0000!gpregb[2], --, b, l0000!val] */
	0x0A0403C0000ull,  /* .58 alu[l0000!gprega[3], --, b, l0000!val] */
	0x0B0403C0000ull,  /* .59 alu[l0000!gpregb[3], --, b, l0000!val] */
	0x0A0404C0000ull,  /* .60 alu[l0000!gprega[4], --, b, l0000!val] */
	0x0B0404C0000ull,  /* .61 alu[l0000!gpregb[4], --, b, l0000!val] */
	0x0A0405C0000ull,  /* .62 alu[l0000!gprega[5], --, b, l0000!val] */
	0x0B0405C0000ull,  /* .63 alu[l0000!gpregb[5], --, b, l0000!val] */
	0x0A0406C0000ull,  /* .64 alu[l0000!gprega[6], --, b, l0000!val] */
	0x0B0406C0000ull,  /* .65 alu[l0000!gpregb[6], --, b, l0000!val] */
	0x0A0407C0000ull,  /* .66 alu[l0000!gprega[7], --, b, l0000!val] */
	0x0B0407C0000ull,  /* .67 alu[l0000!gpregb[7], --, b, l0000!val] */
	0x0A0408C0000ull,  /* .68 alu[l0000!gprega[8], --, b, l0000!val] */
	0x0B0408C0000ull,  /* .69 alu[l0000!gpregb[8], --, b, l0000!val] */
	0x0A0409C0000ull,  /* .70 alu[l0000!gprega[9], --, b, l0000!val] */
	0x0B0409C0000ull,  /* .71 alu[l0000!gpregb[9], --, b, l0000!val] */
	0x0A040AC0000ull,  /* .72 alu[l0000!gprega[10], --, b, l0000!val] */
	0x0B040AC0000ull,  /* .73 alu[l0000!gpregb[10], --, b, l0000!val] */
	0x0A040BC0000ull,  /* .74 alu[l0000!gprega[11], --, b, l0000!val] */
	0x0B040BC0000ull,  /* .75 alu[l0000!gpregb[11], --, b, l0000!val] */
	0x0A040CC0000ull,  /* .76 alu[l0000!gprega[12], --, b, l0000!val] */
	0x0B040CC0000ull,  /* .77 alu[l0000!gpregb[12], --, b, l0000!val] */
	0x0A040DC0000ull,  /* .78 alu[l0000!gprega[13], --, b, l0000!val] */
	0x0B040DC0000ull,  /* .79 alu[l0000!gpregb[13], --, b, l0000!val] */
	0x0A040EC0000ull,  /* .80 alu[l0000!gprega[14], --, b, l0000!val] */
	0x0B040EC0000ull,  /* .81 alu[l0000!gpregb[14], --, b, l0000!val] */
	0x0A040FC0000ull,  /* .82 alu[l0000!gprega[15], --, b, l0000!val] */
	0x0B040FC0000ull,  /* .83 alu[l0000!gpregb[15], --, b, l0000!val] */
	0x0D81581C010ull,  /* .84 br=ctx[7, exit#] */
	0x0E000010000ull,  /* .85 ctx_arb[kill], any */
	0x0E000010000ull,  /* .86 exit#:ctx_arb[kill], any */
};

static const uint64_t ae_inst_4b[] = {
	0x0F0400C0000ull,  /* .0 immed_w0[l0000!indx, 0] */
	0x0F4400C0000ull,  /* .1 immed_w1[l0000!indx, 0] */
	0x0F040000300ull,  /* .2 immed_w0[l0000!myvalue, 0x0] */
	0x0F440000300ull,  /* .3 immed_w1[l0000!myvalue, 0x0] */
	0x0FC066C0000ull,  /* .4 local_csr_wr[active_lm_addr_0, 
	                                l0000!indx]; put indx to lm_addr */
	0x0F0000C0300ull,  /* .5 nop */
	0x0F0000C0300ull,  /* .6 nop */
	0x0F0000C0300ull,  /* .7 nop */
	0x0A021000000ull,  /* .8 alu[*l$index0++, --, b, l0000!myvalue] */
};

static const uint64_t ae_inst_1b[] = {
	0x0F0400C0000ull,  /* .0 immed_w0[l0000!indx, 0] */
	0x0F4400C0000ull,  /* .1 immed_w1[l0000!indx, 0] */
	0x0F040000300ull,  /* .2 immed_w0[l0000!myvalue, 0x0] */
	0x0F440000300ull,  /* .3 immed_w1[l0000!myvalue, 0x0] */
	0x0FC066C0000ull,  /* .4 local_csr_wr[active_lm_addr_0, 
	                               l0000!indx]; put indx to lm_addr */
	0x0F0000C0300ull,  /* .5 nop */
	0x0F0000C0300ull,  /* .6 nop */
	0x0F0000C0300ull,  /* .7 nop */
	0x0A000180000ull,  /* .8 alu[l0000!val, --, b, *l$index0] */
	0x09080000200ull,  /* .9 alu_shf[l0000!myvalue, --, b, 
	                                  l0000!myvalue,  <<24 ] */
	0x08180280201ull,  /* .10 alu_shf[l0000!val1, --, b, l0000!val,  <<8 ] */
	0x08080280102ull,  /* .11 alu_shf[l0000!val1, --, b, l0000!val1 , >>8 ] */
	0x0BA00100002ull,  /* .12 alu[l0000!val2, l0000!val1, or, l0000!myvalue] */

};

static const uint64_t ae_inst_2b[] = {
	0x0F0400C0000ull,  /* .0 immed_w0[l0000!indx, 0] */
	0x0F4400C0000ull,  /* .1 immed_w1[l0000!indx, 0] */
	0x0F040000300ull,  /* .2 immed_w0[l0000!myvalue, 0x0] */
	0x0F440000300ull,  /* .3 immed_w1[l0000!myvalue, 0x0] */
	0x0FC066C0000ull,  /* .4 local_csr_wr[active_lm_addr_0, 
	                               l0000!indx]; put indx to lm_addr */
	0x0F0000C0300ull,  /* .5 nop */
	0x0F0000C0300ull,  /* .6 nop */
	0x0F0000C0300ull,  /* .7 nop */
	0x0A000180000ull,  /* .8 alu[l0000!val, --, b, *l$index0] */
	0x09100000200ull,  /* .9 alu_shf[l0000!myvalue, --, b, 
	                                      l0000!myvalue,  <<16 ] */
	0x08100280201ull,  /* .10 alu_shf[l0000!val1, --, b, l0000!val,  <<16 ] */
	0x08100280102ull,  /* .11 alu_shf[l0000!val1, --, b, l0000!val1 , >>16 ] */
	0x0BA00100002ull,  /* .12 alu[l0000!val2, l0000!val1, or, l0000!myvalue] */
};

static const uint64_t ae_inst_3b[] = {
	0x0F0400C0000ull,  /* .0 immed_w0[l0000!indx, 0] */
	0x0F4400C0000ull,  /* .1 immed_w1[l0000!indx, 0] */
	0x0F040000300ull,  /* .2 immed_w0[l0000!myvalue, 0x0] */
	0x0F440000300ull,  /* .3 immed_w1[l0000!myvalue, 0x0] */
	0x0FC066C0000ull,  /* .4 local_csr_wr[active_lm_addr_0, 
	                               l0000!indx]; put indx to lm_addr */
	0x0F0000C0300ull,  /* .5 nop */
	0x0F0000C0300ull,  /* .6 nop */
	0x0F0000C0300ull,  /* .7 nop */
	0x0A000180000ull,  /* .8 alu[l0000!val, --, b, *l$index0] */
	0x09180000200ull,  /* .9 alu_shf[l0000!myvalue, --, 
	                                b, l0000!myvalue,  <<8 ] */
	0x08080280201ull,  /* .10 alu_shf[l0000!val1, --, b, l0000!val,  <<24 ] */
	0x08180280102ull,  /* .11 alu_shf[l0000!val1, --, b, l0000!val1 , >>24 ] */
	0x0BA00100002ull,  /* .12 alu[l0000!val2, l0000!val1, or, l0000!myvalue] */
};

/* micro-instr fixup */
#define INSERT_IMMED_GPRA_CONST(inst, const_val)		\
		inst = (inst & 0xFFFF00C03FFull) |		\
		((((const_val) << 12) & 0x0FF00000ull) |	\
		(((const_val) << 10) & 0x0003FC00ull))
#define INSERT_IMMED_GPRB_CONST(inst, const_val)		\
		inst = (inst & 0xFFFF00FFF00ull) |		\
		((((const_val) << 12) & 0x0FF00000ull) |	\
		(((const_val) << 0) & 0x000000FFull))

enum aereg_type {
	AEREG_NO_DEST,		/* no destination */
	AEREG_GPA_REL,		/* general-purpose A register under relative mode */
	AEREG_GPA_ABS,		/* general-purpose A register under absolute mode */
	AEREG_GPB_REL,		/* general-purpose B register under relative mode */
	AEREG_GPB_ABS,		/* general-purpose B register under absolute mode */
	AEREG_SR_REL,		/* sram register under relative mode */
	AEREG_SR_RD_REL,	/* sram read register under relative mode */
	AEREG_SR_WR_REL,	/* sram write register under relative mode */
	AEREG_SR_ABS,		/* sram register under absolute mode */
	AEREG_SR_RD_ABS,	/* sram read register under absolute mode */
	AEREG_SR_WR_ABS,	/* sram write register under absolute mode */
	AEREG_SR0_SPILL,	/* sram0 spill register */
	AEREG_SR1_SPILL,	/* sram1 spill register */
	AEREG_SR2_SPILL,	/* sram2 spill register */
	AEREG_SR3_SPILL,	/* sram3 spill register */
	AEREG_SR0_MEM_ADDR,	/* sram0 memory address register */
	AEREG_SR1_MEM_ADDR,	/* sram1 memory address register */
	AEREG_SR2_MEM_ADDR,	/* sram2 memory address register */
	AEREG_SR3_MEM_ADDR,	/* sram3 memory address register */
	AEREG_DR_REL,		/* dram register under relative mode */
	AEREG_DR_RD_REL,	/* dram read register under relative mode */
	AEREG_DR_WR_REL,	/* dram write register under relative mode */
	AEREG_DR_ABS,		/* dram register under absolute mode */
	AEREG_DR_RD_ABS,	/* dram read register under absolute mode */
	AEREG_DR_WR_ABS,	/* dram write register under absolute mode */
	AEREG_DR_MEM_ADDR,	/* dram memory address register */
	AEREG_LMEM,		/* local memory */
	AEREG_LMEM0,		/* local memory bank0 */
	AEREG_LMEM1,		/* local memory bank1 */
	AEREG_LMEM_SPILL,	/* local memory spill */
	AEREG_LMEM_ADDR,	/* local memory address */
	AEREG_NEIGH_REL,	/* next neighbour register under relative mode */
	AEREG_NEIGH_INDX,	/* next neighbour register under index mode */
	AEREG_SIG_REL,		/* signal register under relative mode */
	AEREG_SIG_INDX,		/* signal register under index mode */
	AEREG_SIG_DOUBLE,	/* signal register */
	AEREG_SIG_SINGLE,	/* signal register */
	AEREG_SCRATCH_MEM_ADDR,	/* scratch memory address */
	AEREG_UMEM0,		/* ustore memory bank0 */
	AEREG_UMEM1,		/* ustore memory bank1 */
	AEREG_UMEM_SPILL,	/* ustore memory spill */
	AEREG_UMEM_ADDR,	/* ustore memory address */
	AEREG_DR1_MEM_ADDR,	/* dram segment1 address */
	AEREG_SR0_IMPORTED,	/* sram segment0 imported data */
	AEREG_SR1_IMPORTED,	/* sram segment1 imported data */
	AEREG_SR2_IMPORTED,	/* sram segment2 imported data */
	AEREG_SR3_IMPORTED,	/* sram segment3 imported data */
	AEREG_DR_IMPORTED,	/* dram segment0 imported data */
	AEREG_DR1_IMPORTED,	/* dram segment1 imported data */
	AEREG_SCRATCH_IMPORTED,	/* scratch imported data */
	AEREG_XFER_RD_ABS,	/* transfer read register under absolute mode */
	AEREG_XFER_WR_ABS,	/* transfer write register under absolute mode */
	AEREG_CONST_VALUE,	/* const alue */
	AEREG_ADDR_TAKEN,	/* address taken */
	AEREG_OPTIMIZED_AWAY,	/* optimized away */
	AEREG_SHRAM_ADDR,	/* shared ram0 address */
	AEREG_SHRAM1_ADDR,	/* shared ram1 address */
	AEREG_SHRAM2_ADDR,	/* shared ram2 address */
	AEREG_SHRAM3_ADDR,	/* shared ram3 address */
	AEREG_SHRAM4_ADDR,	/* shared ram4 address */
	AEREG_SHRAM5_ADDR,	/* shared ram5 address */
	AEREG_ANY = 0xffff	/* any register */
};
#define AEREG_SR_INDX	AEREG_SR_ABS
				/* sram transfer register under index mode */
#define AEREG_DR_INDX	AEREG_DR_ABS
				/* dram transfer register under index mode */
#define AEREG_NEIGH_ABS	AEREG_NEIGH_INDX
				/* next neighbor register under absolute mode */


#define QAT_2K			0x0800
#define QAT_4K			0x1000
#define QAT_6K			0x1800
#define QAT_8K			0x2000
#define QAT_16K			0x4000

#define MOF_OBJ_ID_LEN		8
#define MOF_FID			0x00666f6d
#define MOF_MIN_VER		0x1
#define MOF_MAJ_VER		0x0
#define SYM_OBJS		"SYM_OBJS"	/* symbol object string */
#define UOF_OBJS		"UOF_OBJS"	/* uof object string */
#define SUOF_OBJS		"SUF_OBJS"	/* suof object string */
#define SUOF_IMAG		"SUF_IMAG"	/* suof chunk ID string */

#define UOF_STRT		"UOF_STRT"	/* string table section ID */
#define UOF_GTID		"UOF_GTID"	/* GTID section ID */
#define UOF_IMAG		"UOF_IMAG"	/* image section ID */
#define UOF_IMEM		"UOF_IMEM"	/* import section ID */
#define UOF_MSEG		"UOF_MSEG"	/* memory section ID */

#define CRC_POLY		0x1021
#define CRC_WIDTH		16
#define CRC_BITMASK(x)		(1L << (x))
#define CRC_WIDTHMASK(width)	((((1L<<(width-1))-1L)<<1)|1L)

struct mof_file_hdr {
	u_int mfh_fid;
	u_int mfh_csum;
	char mfh_min_ver;
	char mfh_maj_ver;
	u_short mfh_reserved;
	u_short mfh_max_chunks;
	u_short mfh_num_chunks;
};

struct mof_file_chunk_hdr {
	char mfch_id[MOF_OBJ_ID_LEN];
	uint64_t mfch_offset;
	uint64_t mfch_size;
};

struct mof_uof_hdr {
	u_short muh_max_chunks;
	u_short muh_num_chunks;
	u_int muh_reserved;
};

struct mof_uof_chunk_hdr {
	char much_id[MOF_OBJ_ID_LEN];	/* should be UOF_IMAG */
	uint64_t much_offset;		/* uof image */
	uint64_t much_size;		/* uof image size */
	u_int much_name;		/* uof name string-table offset */
	u_int much_reserved;
};

#define UOF_MAX_NUM_OF_AE	16	/* maximum number of AE */

#define UOF_OBJ_ID_LEN		8	/* length of object ID */
#define UOF_FIELD_POS_SIZE	12	/* field postion size */
#define MIN_UOF_SIZE		24	/* minimum .uof file size */
#define UOF_FID			0xc6c2	/* uof magic number */
#define UOF_MIN_VER		0x11
#define UOF_MAJ_VER		0x4

struct uof_file_hdr {
	u_short ufh_id;			/* file id and endian indicator */
	u_short ufh_reserved1;		/* reserved for future use */
	char ufh_min_ver;		/* file format minor version */
	char ufh_maj_ver;		/* file format major version */
	u_short ufh_reserved2;		/* reserved for future use */
	u_short ufh_max_chunks;		/* max chunks in file */
	u_short ufh_num_chunks;		/* num of actual chunks */
};

struct uof_file_chunk_hdr {
	char ufch_id[UOF_OBJ_ID_LEN];	/* chunk identifier */
	u_int ufch_csum;		/* chunk checksum */
	u_int ufch_offset;		/* offset of the chunk in the file */
	u_int ufch_size;		/* size of the chunk */
};

struct uof_obj_hdr {
	u_int uoh_cpu_type;		/* CPU type */
	u_short uoh_min_cpu_ver;	/* starting CPU version */
	u_short uoh_max_cpu_ver;	/* ending CPU version */
	short uoh_max_chunks;		/* max chunks in chunk obj */
	short uoh_num_chunks;		/* num of actual chunks */
	u_int uoh_reserved1;
	u_int uoh_reserved2;
};

struct uof_chunk_hdr {
	char uch_id[UOF_OBJ_ID_LEN];
	u_int uch_offset;
	u_int uch_size;
};

struct uof_str_tab {
	u_int ust_table_len;		/* length of table */
	u_int ust_reserved;		/* reserved for future use */
	uint64_t ust_strings;		/* pointer to string table.
					 * NULL terminated strings */
};

#define AE_MODE_RELOAD_CTX_SHARED	__BIT(12)
#define AE_MODE_SHARED_USTORE		__BIT(11)
#define AE_MODE_LMEM1			__BIT(9)
#define AE_MODE_LMEM0			__BIT(8)
#define AE_MODE_NN_MODE			__BITS(7, 4)
#define AE_MODE_CTX_MODE		__BITS(3, 0)

#define AE_MODE_NN_MODE_NEIGH		0
#define AE_MODE_NN_MODE_SELF		1
#define AE_MODE_NN_MODE_DONTCARE	0xff

struct uof_image {
	u_int ui_name;			/* image name */
	u_int ui_ae_assigned;		/* AccelEngines assigned */
	u_int ui_ctx_assigned;		/* AccelEngine contexts assigned */
	u_int ui_cpu_type;		/* cpu type */
	u_int ui_entry_address;		/* entry uaddress */
	u_int ui_fill_pattern[2];	/* uword fill value */
	u_int ui_reloadable_size;	/* size of reloadable ustore section */

	u_char ui_sensitivity;		/*
					 * case sensitivity: 0 = insensitive,
					 * 1 = sensitive
					 */
	u_char ui_reserved;		/* reserved for future use */
	u_short ui_ae_mode;		/*
					 * unused<15:14>, legacyMode<13>,
					 * reloadCtxShared<12>, sharedUstore<11>,
					 * ecc<10>, locMem1<9>, locMem0<8>,
					 * nnMode<7:4>, ctx<3:0>
					 */

	u_short ui_max_ver;		/* max cpu ver on which the image can run */
	u_short ui_min_ver;		/* min cpu ver on which the image can run */

	u_short ui_image_attrib;	/* image attributes */
	u_short ui_reserved2;		/* reserved for future use */

	u_short ui_num_page_regions;	/* number of page regions */
	u_short ui_num_pages;		/* number of pages */

	u_int ui_reg_tab;		/* offset to register table */
	u_int ui_init_reg_sym_tab;	/* reg/sym init table */
	u_int ui_sbreak_tab;		/* offset to sbreak table */

	u_int ui_app_metadata;		/* application meta-data */
	/* ui_npages of code page follows this header */
};

struct uof_obj_table {
	u_int uot_nentries;		/* number of table entries */
	/* uot_nentries of object follows */
};

struct uof_ae_reg {
	u_int uar_name;			/* reg name string-table offset */
	u_int uar_vis_name;		/* reg visible name string-table offset */
	u_short uar_type;		/* reg type */
	u_short uar_addr;		/* reg address */
	u_short uar_access_mode;	/* uof_RegAccessMode_T: read/write/both/undef */
	u_char uar_visible;		/* register visibility */
	u_char uar_reserved1;		/* reserved for future use */
	u_short uar_ref_count;		/* number of contiguous registers allocated */
	u_short uar_reserved2;		/* reserved for future use */
	u_int uar_xoid;			/* xfer order ID */
};

enum uof_value_kind {
	UNDEF_VAL,	/* undefined value */
	CHAR_VAL,	/* character value */
	SHORT_VAL,	/* short value */
	INT_VAL,	/* integer value */
	STR_VAL,	/* string value */
	STRTAB_VAL,	/* string table value */
	NUM_VAL,	/* number value */
	EXPR_VAL	/* expression value */
};

enum uof_init_type {
	INIT_EXPR,
	INIT_REG,
	INIT_REG_CTX,
	INIT_EXPR_ENDIAN_SWAP
};

struct uof_init_reg_sym {
	u_int uirs_name;		/* symbol name */
	char uirs_init_type;		/* 0=expr, 1=register, 2=ctxReg,
					 * 3=expr_endian_swap */
	char uirs_value_type;		/* EXPR_VAL, STRTAB_VAL */
	char uirs_reg_type;		/* register type: ae_reg_type */
	u_char uirs_ctx;		/* AE context when initType=2 */
	u_int uirs_addr_offset;		/* reg address, or sym-value offset */
	u_int uirs_value;		/* integer value, or expression */
};

struct uof_sbreak {
	u_int us_page_num;		/* page number */
	u_int us_virt_uaddr;		/* virt uaddress */
	u_char us_sbreak_type;		/* sbreak type */
	u_char us_reg_type;		/* register type: ae_reg_type */
	u_short us_reserved1;		/* reserved for future use */
	u_int us_addr_offset;		/* branch target address or offset
					 * to be used with the reg value to
					 * calculate the target address */
	u_int us_reg_rddr;		/* register address */
};
struct uof_code_page {
	u_int ucp_page_region;		/* page associated region */
	u_int ucp_page_num;		/* code-page number */
	u_char ucp_def_page;		/* default page indicator */
	u_char ucp_reserved2;		/* reserved for future use */
	u_short ucp_reserved1;		/* reserved for future use */
	u_int ucp_beg_vaddr;		/* starting virtual uaddr */
	u_int ucp_beg_paddr;		/* starting physical uaddr */
	u_int ucp_neigh_reg_tab;	/* offset to neighbour-reg table */
	u_int ucp_uc_var_tab;		/* offset to uC var table */
	u_int ucp_imp_var_tab;		/* offset to import var table */
	u_int ucp_imp_expr_tab;		/* offset to import expression table */
	u_int ucp_code_area;		/* offset to code area */
};

struct uof_code_area {
	u_int uca_num_micro_words;	/* number of micro words */
	u_int uca_uword_block_tab;	/* offset to ublock table */
};

struct uof_uword_block {
	u_int uub_start_addr;		/* start address */
	u_int uub_num_words;		/* number of microwords */
	u_int uub_uword_offset;		/* offset to the uwords */
	u_int uub_reserved;		/* reserved for future use */
};

struct uof_uword_fixup {
	u_int uuf_name;			/* offset to string table */
	u_int uuf_uword_address;	/* micro word address */
	u_int uuf_expr_value;		/* string table offset of expr string, or value */
	u_char uuf_val_type;		/* VALUE_UNDEF, VALUE_NUM, VALUE_EXPR */
	u_char uuf_value_attrs;		/* bit<0> (Scope: 0=global, 1=local),
					 * bit<1> (init: 0=no, 1=yes) */
	u_short uuf_reserved1;		/* reserved for future use */
	char uuf_field_attrs[UOF_FIELD_POS_SIZE];
					/* field pos, size, and right shift value */
};

struct uof_import_var {
	u_int uiv_name;			/* import var name string-table offset */
	u_char uiv_value_attrs;		/* bit<0> (Scope: 0=global),
					 * bit<1> (init: 0=no, 1=yes) */
	u_char uiv_reserved1;		/* reserved for future use */
	u_short uiv_reserved2;		/* reserved for future use */
	uint64_t uiv_value;		/* 64-bit imported value */
};

struct uof_mem_val_attr {
	u_int umva_byte_offset;		/* byte-offset from the allocated memory */
	u_int umva_value;		/* memory value */
};

enum uof_mem_region {
	SRAM_REGION,	/* SRAM region */
	DRAM_REGION,	/* DRAM0 region */
	DRAM1_REGION,	/* DRAM1 region */
	LMEM_REGION,	/* local memory region */
	SCRATCH_REGION,	/* SCRATCH region */
	UMEM_REGION,	/* micro-store region */
	RAM_REGION,	/* RAM region */
	SHRAM_REGION,	/* shared memory-0 region */
	SHRAM1_REGION,	/* shared memory-1 region */
	SHRAM2_REGION,	/* shared memory-2 region */
	SHRAM3_REGION,	/* shared memory-3 region */
	SHRAM4_REGION,	/* shared memory-4 region */
	SHRAM5_REGION	/* shared memory-5 region */
};

#define UOF_SCOPE_GLOBAL	0
#define UOF_SCOPE_LOCAL		1

struct uof_init_mem {
	u_int uim_sym_name;		/* symbol name */
	char uim_region;		/* memory region -- uof_mem_region */
	char uim_scope;			/* visibility scope */
	u_short uim_reserved1;		/* reserved for future use */
	u_int uim_addr;			/* memory address */
	u_int uim_num_bytes;		/* number of bytes */
	u_int uim_num_val_attr;		/* number of values attributes */

	/* uim_num_val_attr of uof_mem_val_attr follows this header */
};

struct uof_var_mem_seg {
	u_int uvms_sram_base;		/* SRAM memory segment base addr */
	u_int uvms_sram_size;		/* SRAM segment size bytes */
	u_int uvms_sram_alignment;	/* SRAM segment alignment bytes */
	u_int uvms_sdram_base;		/* DRAM0 memory segment base addr */
	u_int uvms_sdram_size;		/* DRAM0 segment size bytes */
	u_int uvms_sdram_alignment;	/* DRAM0 segment alignment bytes */
	u_int uvms_sdram1_base;		/* DRAM1 memory segment base addr */
	u_int uvms_sdram1_size;		/* DRAM1 segment size bytes */
	u_int uvms_sdram1_alignment;	/* DRAM1 segment alignment bytes */
	u_int uvms_scratch_base;	/* SCRATCH memory segment base addr */
	u_int uvms_scratch_size;	/* SCRATCH segment size bytes */
	u_int uvms_scratch_alignment;	/* SCRATCH segment alignment bytes */
};

#define SUOF_OBJ_ID_LEN		8
#define SUOF_FID		0x53554f46
#define SUOF_MAJ_VER		0x0
#define SUOF_MIN_VER		0x1
#define SIMG_AE_INIT_SEQ_LEN	(50 * sizeof(unsigned long long))
#define SIMG_AE_INSTS_LEN	(0x4000 * sizeof(unsigned long long))
#define CSS_FWSK_MODULUS_LEN	256
#define CSS_FWSK_EXPONENT_LEN	4
#define CSS_FWSK_PAD_LEN	252
#define CSS_FWSK_PUB_LEN	(CSS_FWSK_MODULUS_LEN + \
				    CSS_FWSK_EXPONENT_LEN + \
				    CSS_FWSK_PAD_LEN)
#define CSS_SIGNATURE_LEN	256
#define CSS_AE_IMG_LEN		(sizeof(struct simg_ae_mode) + \
				    SIMG_AE_INIT_SEQ_LEN +         \
				    SIMG_AE_INSTS_LEN)
#define CSS_AE_SIMG_LEN		(sizeof(struct css_hdr) + \
				    CSS_FWSK_PUB_LEN + \
				    CSS_SIGNATURE_LEN + \
				    CSS_AE_IMG_LEN)
#define AE_IMG_OFFSET		(sizeof(struct css_hdr) + \
				    CSS_FWSK_MODULUS_LEN + \
				    CSS_FWSK_EXPONENT_LEN + \
				    CSS_SIGNATURE_LEN)
#define CSS_MAX_IMAGE_LEN	0x40000

struct fw_auth_desc {
	u_int fad_img_len;
	u_int fad_reserved;
	u_int fad_css_hdr_high;
	u_int fad_css_hdr_low;
	u_int fad_img_high;
	u_int fad_img_low;
	u_int fad_signature_high;
	u_int fad_signature_low;
	u_int fad_fwsk_pub_high;
	u_int fad_fwsk_pub_low;
	u_int fad_img_ae_mode_data_high;
	u_int fad_img_ae_mode_data_low;
	u_int fad_img_ae_init_data_high;
	u_int fad_img_ae_init_data_low;
	u_int fad_img_ae_insts_high;
	u_int fad_img_ae_insts_low;
};

struct auth_chunk {
	struct fw_auth_desc ac_fw_auth_desc;
	uint64_t ac_chunk_size;
	uint64_t ac_chunk_bus_addr;
};

enum css_fwtype {
	CSS_AE_FIRMWARE = 0,
	CSS_MMP_FIRMWARE = 1
};

struct css_hdr {
	u_int css_module_type;
	u_int css_header_len;
	u_int css_header_ver;
	u_int css_module_id;
	u_int css_module_vendor;
	u_int css_date;
	u_int css_size;
	u_int css_key_size;
	u_int css_module_size;
	u_int css_exponent_size;
	u_int css_fw_type;
	u_int css_reserved[21];
};

struct simg_ae_mode {
	u_int sam_file_id;
	u_short sam_maj_ver;
	u_short sam_min_ver;
	u_int sam_dev_type;
	u_short sam_devmax_ver;
	u_short sam_devmin_ver;
	u_int sam_ae_mask;
	u_int sam_ctx_enables;
	char sam_fw_type;
	char sam_ctx_mode;
	char sam_nn_mode;
	char sam_lm0_mode;
	char sam_lm1_mode;
	char sam_scs_mode;
	char sam_lm2_mode;
	char sam_lm3_mode;
	char sam_tindex_mode;
	u_char sam_reserved[7];
	char sam_simg_name[256];
	char sam_appmeta_data[256];
};

struct suof_file_hdr {
	u_int sfh_file_id;
	u_int sfh_check_sum;
	char sfh_min_ver;
	char sfh_maj_ver;
	char sfh_fw_type;
	char sfh_reserved;
	u_short sfh_max_chunks;
	u_short sfh_num_chunks;
};

struct suof_chunk_hdr {
	char sch_chunk_id[SUOF_OBJ_ID_LEN];
	uint64_t sch_offset;
	uint64_t sch_size;
};

struct suof_str_tab {
	u_int sst_tab_length;
	u_int sst_strings;
};

struct suof_obj_hdr {
	u_int soh_img_length;
	u_int soh_reserved;
};

/* -------------------------------------------------------------------------- */
/* accel */

enum fw_slice {
	FW_SLICE_NULL = 0,	/* NULL slice type */
	FW_SLICE_CIPHER = 1,	/* CIPHER slice type */
	FW_SLICE_AUTH = 2,	/* AUTH slice type */
	FW_SLICE_DRAM_RD = 3,	/* DRAM_RD Logical slice type */
	FW_SLICE_DRAM_WR = 4,	/* DRAM_WR Logical slice type */
	FW_SLICE_COMP = 5,	/* Compression slice type */
	FW_SLICE_XLAT = 6,	/* Translator slice type */
	FW_SLICE_DELIMITER	/* End delimiter */
};
#define MAX_FW_SLICE	FW_SLICE_DELIMITER

#define QAT_OPTIMAL_ALIGN_SHIFT			6
#define QAT_OPTIMAL_ALIGN			(1 << QAT_OPTIMAL_ALIGN_SHIFT)

enum hw_auth_algo {
	HW_AUTH_ALGO_NULL = 0,		/* Null hashing */
	HW_AUTH_ALGO_SHA1 = 1,		/* SHA1 hashing */
	HW_AUTH_ALGO_MD5 = 2,		/* MD5 hashing */
	HW_AUTH_ALGO_SHA224 = 3,	/* SHA-224 hashing */
	HW_AUTH_ALGO_SHA256 = 4,	/* SHA-256 hashing */
	HW_AUTH_ALGO_SHA384 = 5,	/* SHA-384 hashing */
	HW_AUTH_ALGO_SHA512 = 6,	/* SHA-512 hashing */
	HW_AUTH_ALGO_AES_XCBC_MAC = 7,	/* AES-XCBC-MAC hashing */
	HW_AUTH_ALGO_AES_CBC_MAC = 8,	/* AES-CBC-MAC hashing */
	HW_AUTH_ALGO_AES_F9 = 9,	/* AES F9 hashing */
	HW_AUTH_ALGO_GALOIS_128 = 10,	/* Galois 128 bit hashing */
	HW_AUTH_ALGO_GALOIS_64 = 11,	/* Galois 64 hashing */
	HW_AUTH_ALGO_KASUMI_F9 = 12,	/* Kasumi F9 hashing */
	HW_AUTH_ALGO_SNOW_3G_UIA2 = 13,	/* UIA2/SNOW_3H F9 hashing */
	HW_AUTH_ALGO_ZUC_3G_128_EIA3 = 14,
	HW_AUTH_RESERVED_1 = 15,
	HW_AUTH_RESERVED_2 = 16,
	HW_AUTH_ALGO_SHA3_256 = 17,
	HW_AUTH_RESERVED_3 = 18,
	HW_AUTH_ALGO_SHA3_512 = 19,
	HW_AUTH_ALGO_DELIMITER = 20
};

enum hw_auth_mode {
	HW_AUTH_MODE0,
	HW_AUTH_MODE1,
	HW_AUTH_MODE2,
	HW_AUTH_MODE_DELIMITER
};

struct hw_auth_config {
	uint32_t config;
	/* Configuration used for setting up the slice */
	uint32_t reserved;
	/* Reserved */
};

#define HW_AUTH_CONFIG_SHA3_ALGO	__BITS(22, 23)
#define HW_AUTH_CONFIG_SHA3_PADDING	__BIT(16)
#define HW_AUTH_CONFIG_CMPLEN		__BITS(14, 8)
	/* The length of the digest if the QAT is to the check*/
#define HW_AUTH_CONFIG_MODE		__BITS(7, 4)
#define HW_AUTH_CONFIG_ALGO		__BITS(3, 0)

#define HW_AUTH_CONFIG_BUILD(mode, algo, cmp_len)	\
	    __SHIFTIN(mode, HW_AUTH_CONFIG_MODE) |	\
	    __SHIFTIN(algo, HW_AUTH_CONFIG_ALGO) |	\
	    __SHIFTIN(cmp_len, HW_AUTH_CONFIG_CMPLEN)

struct hw_auth_counter {
	uint32_t counter;		/* Counter value */
	uint32_t reserved;		/* Reserved */
};

struct hw_auth_setup {
	struct hw_auth_config auth_config;
	/* Configuration word for the auth slice */
	struct hw_auth_counter auth_counter;
	/* Auth counter value for this request */
};

#define HW_NULL_STATE1_SZ		32
#define HW_MD5_STATE1_SZ		16
#define HW_SHA1_STATE1_SZ		20
#define HW_SHA224_STATE1_SZ		32
#define HW_SHA256_STATE1_SZ		32
#define HW_SHA3_256_STATE1_SZ		32
#define HW_SHA384_STATE1_SZ		64
#define HW_SHA512_STATE1_SZ		64
#define HW_SHA3_512_STATE1_SZ		64
#define HW_SHA3_224_STATE1_SZ		28
#define HW_SHA3_384_STATE1_SZ		48
#define HW_AES_XCBC_MAC_STATE1_SZ	16
#define HW_AES_CBC_MAC_STATE1_SZ	16
#define HW_AES_F9_STATE1_SZ		32
#define HW_KASUMI_F9_STATE1_SZ		16
#define HW_GALOIS_128_STATE1_SZ		16
#define HW_SNOW_3G_UIA2_STATE1_SZ	8
#define HW_ZUC_3G_EIA3_STATE1_SZ	8
#define HW_NULL_STATE2_SZ		32
#define HW_MD5_STATE2_SZ		16
#define HW_SHA1_STATE2_SZ		20
#define HW_SHA224_STATE2_SZ		32
#define HW_SHA256_STATE2_SZ		32
#define HW_SHA3_256_STATE2_SZ		0
#define HW_SHA384_STATE2_SZ		64
#define HW_SHA512_STATE2_SZ		64
#define HW_SHA3_512_STATE2_SZ		0
#define HW_SHA3_224_STATE2_SZ		0
#define HW_SHA3_384_STATE2_SZ		0
#define HW_AES_XCBC_MAC_KEY_SZ		16
#define HW_AES_CBC_MAC_KEY_SZ		16
#define HW_AES_CCM_CBC_E_CTR0_SZ	16
#define HW_F9_IK_SZ			16
#define HW_F9_FK_SZ			16
#define HW_KASUMI_F9_STATE2_SZ		(HW_F9_IK_SZ + HW_F9_FK_SZ)
#define HW_AES_F9_STATE2_SZ		HW_KASUMI_F9_STATE2_SZ
#define HW_SNOW_3G_UIA2_STATE2_SZ	24
#define HW_ZUC_3G_EIA3_STATE2_SZ	32
#define HW_GALOIS_H_SZ			16
#define HW_GALOIS_LEN_A_SZ		8
#define HW_GALOIS_E_CTR0_SZ		16

struct hw_auth_sha512 {
	struct hw_auth_setup inner_setup;
	/* Inner loop configuration word for the slice */
	uint8_t state1[HW_SHA512_STATE1_SZ];
	/* Slice state1 variable */
	struct hw_auth_setup outer_setup;
	/* Outer configuration word for the slice */
	uint8_t state2[HW_SHA512_STATE2_SZ];
	/* Slice state2 variable */
};

union hw_auth_algo_blk {
	struct hw_auth_sha512 max;
	/* This is the largest possible auth setup block size */
};

enum hw_cipher_algo {
	HW_CIPHER_ALGO_NULL = 0,		/* Null ciphering */
	HW_CIPHER_ALGO_DES = 1,			/* DES ciphering */
	HW_CIPHER_ALGO_3DES = 2,		/* 3DES ciphering */
	HW_CIPHER_ALGO_AES128 = 3,		/* AES-128 ciphering */
	HW_CIPHER_ALGO_AES192 = 4,		/* AES-192 ciphering */
	HW_CIPHER_ALGO_AES256 = 5,		/* AES-256 ciphering */
	HW_CIPHER_ALGO_ARC4 = 6,		/* ARC4 ciphering */
	HW_CIPHER_ALGO_KASUMI = 7,		/* Kasumi */
	HW_CIPHER_ALGO_SNOW_3G_UEA2 = 8,	/* Snow_3G */
	HW_CIPHER_ALGO_ZUC_3G_128_EEA3 = 9,
	HW_CIPHER_DELIMITER = 10		/* Delimiter type */
};

enum hw_cipher_mode {
	HW_CIPHER_ECB_MODE = 0,		/* ECB mode */
	HW_CIPHER_CBC_MODE = 1,		/* CBC mode */
	HW_CIPHER_CTR_MODE = 2,		/* CTR mode */
	HW_CIPHER_F8_MODE = 3,		/* F8 mode */
	HW_CIPHER_XTS_MODE = 6,
	HW_CIPHER_MODE_DELIMITER = 7	/* Delimiter type */
};

struct hw_cipher_config {
	uint32_t val;			/* Cipher slice configuration */
	uint32_t reserved;		/* Reserved */
};

#define CIPHER_CONFIG_CONVERT		__BIT(9)
#define CIPHER_CONFIG_DIR		__BIT(8)
#define CIPHER_CONFIG_MODE		__BITS(7, 4)
#define CIPHER_CONFIG_ALGO		__BITS(3, 0)
#define HW_CIPHER_CONFIG_BUILD(mode, algo, convert, dir)	\
	    __SHIFTIN(mode, CIPHER_CONFIG_MODE) |		\
	    __SHIFTIN(algo, CIPHER_CONFIG_ALGO) |		\
	    __SHIFTIN(convert, CIPHER_CONFIG_CONVERT) |		\
	    __SHIFTIN(dir, CIPHER_CONFIG_DIR)

enum hw_cipher_dir {
	HW_CIPHER_ENCRYPT = 0,		/* encryption is required */
	HW_CIPHER_DECRYPT = 1,		/* decryption is required */
};

enum hw_cipher_convert {
	HW_CIPHER_NO_CONVERT = 0,	/* no key convert is required*/
	HW_CIPHER_KEY_CONVERT = 1,	/* key conversion is required*/
};

#define CIPHER_MODE_F8_KEY_SZ_MULT	2
#define CIPHER_MODE_XTS_KEY_SZ_MULT	2

#define HW_DES_BLK_SZ			8
#define HW_3DES_BLK_SZ			8
#define HW_NULL_BLK_SZ			8
#define HW_AES_BLK_SZ			16
#define HW_KASUMI_BLK_SZ		8
#define HW_SNOW_3G_BLK_SZ		8
#define HW_ZUC_3G_BLK_SZ		8
#define HW_NULL_KEY_SZ			256
#define HW_DES_KEY_SZ			8
#define HW_3DES_KEY_SZ			24
#define HW_AES_128_KEY_SZ		16
#define HW_AES_192_KEY_SZ		24
#define HW_AES_256_KEY_SZ		32
#define HW_AES_128_F8_KEY_SZ		(HW_AES_128_KEY_SZ *	\
					    CIPHER_MODE_F8_KEY_SZ_MULT)
#define HW_AES_192_F8_KEY_SZ		(HW_AES_192_KEY_SZ *	\
					    CIPHER_MODE_F8_KEY_SZ_MULT)
#define HW_AES_256_F8_KEY_SZ		(HW_AES_256_KEY_SZ *	\
					    CIPHER_MODE_F8_KEY_SZ_MULT)
#define HW_AES_128_XTS_KEY_SZ		(HW_AES_128_KEY_SZ *	\
					    CIPHER_MODE_XTS_KEY_SZ_MULT)
#define HW_AES_256_XTS_KEY_SZ		(HW_AES_256_KEY_SZ *	\
					    CIPHER_MODE_XTS_KEY_SZ_MULT)
#define HW_KASUMI_KEY_SZ		16
#define HW_KASUMI_F8_KEY_SZ		(HW_KASUMI_KEY_SZ *	\
					    CIPHER_MODE_F8_KEY_SZ_MULT)
#define HW_AES_128_XTS_KEY_SZ		(HW_AES_128_KEY_SZ *	\
					    CIPHER_MODE_XTS_KEY_SZ_MULT)
#define HW_AES_256_XTS_KEY_SZ		(HW_AES_256_KEY_SZ *	\
					    CIPHER_MODE_XTS_KEY_SZ_MULT)
#define HW_ARC4_KEY_SZ			256
#define HW_SNOW_3G_UEA2_KEY_SZ		16
#define HW_SNOW_3G_UEA2_IV_SZ		16
#define HW_ZUC_3G_EEA3_KEY_SZ		16
#define HW_ZUC_3G_EEA3_IV_SZ		16
#define HW_MODE_F8_NUM_REG_TO_CLEAR	2

struct hw_cipher_aes256_f8 {
	struct hw_cipher_config cipher_config;
	/* Cipher configuration word for the slice set to
	 * AES-256 and the F8 mode */
	uint8_t key[HW_AES_256_F8_KEY_SZ];
	/* Cipher key */
};

union hw_cipher_algo_blk {
	struct hw_cipher_aes256_f8 max;		/* AES-256 F8 Cipher */
	/* This is the largest possible cipher setup block size */
};

struct flat_buffer_desc {
	uint32_t data_len_in_bytes;
	uint32_t reserved;
	uint64_t phy_buffer;
};

#define	HW_MAXSEG	32

struct buffer_list_desc {
	uint64_t resrvd;
	uint32_t num_buffers;
	uint32_t reserved;
	struct flat_buffer_desc flat_bufs[HW_MAXSEG];
};

/* -------------------------------------------------------------------------- */
/* look aside */

enum fw_la_cmd_id {
	FW_LA_CMD_CIPHER,			/* Cipher Request */
	FW_LA_CMD_AUTH,			/* Auth Request */
	FW_LA_CMD_CIPHER_HASH,		/* Cipher-Hash Request */
	FW_LA_CMD_HASH_CIPHER,		/* Hash-Cipher Request */
	FW_LA_CMD_TRNG_GET_RANDOM,		/* TRNG Get Random Request */
	FW_LA_CMD_TRNG_TEST,		/* TRNG Test Request */
	FW_LA_CMD_SSL3_KEY_DERIVE,		/* SSL3 Key Derivation Request */
	FW_LA_CMD_TLS_V1_1_KEY_DERIVE,	/* TLS Key Derivation Request */
	FW_LA_CMD_TLS_V1_2_KEY_DERIVE,	/* TLS Key Derivation Request */
	FW_LA_CMD_MGF1,			/* MGF1 Request */
	FW_LA_CMD_AUTH_PRE_COMP,		/* Auth Pre-Compute Request */
#if 0 /* incompatible between qat 1.5 and 1.7 */
	FW_LA_CMD_CIPHER_CIPHER,		/* Cipher-Cipher Request */
	FW_LA_CMD_HASH_HASH,		/* Hash-Hash Request */
	FW_LA_CMD_CIPHER_PRE_COMP,		/* Auth Pre-Compute Request */
#endif
	FW_LA_CMD_DELIMITER,		/* Delimiter type */
};

#endif
