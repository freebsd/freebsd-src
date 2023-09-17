/*-
 * Copyright (c) 2016 Alexander Motin <mav@FreeBSD.org>
 * Copyright (C) 2013 Intel Corporation
 * Copyright (C) 2015 EMC Corporation
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
 */

#ifndef _NTB_REGS_H_
#define _NTB_REGS_H_
#include <sys/types.h>
#include <sys/stdint.h>

/*---------------------------------------------------------------------------
 *   Macro: M*_M : Create a mask to isolate a bit field of a data word.
 *          M*_F : Extract value from a bit field of a data word.
 *          M*_I : Insert value into a bit field of a data word.
 *
 * Purpose: Bit field manipulation macros for mask, insert and extract for
 *          8-bit, 16-bit, 32-bit and 64-bit data words.
 *
 *  Params: [in] P = Bit position of start of the bit field (lsb is 0).
 *          [in] N = Size of the bit field in bits.
 *          [in] X = Value to insert or remove from the bit field.
 *---------------------------------------------------------------------------
 */
#define M8_M(P, N)      ((UINT8_MAX >> (8 - (N))) << (P))
#define M8_F(X, P, N)   (((uint8_t)(X) & M8_M(P, N)) >> (P))
#define M8_I(X, P, N)   (((uint8_t)(X) << (P)) & M8_M(P, N))

#define NTB_LINK_STATUS_ACTIVE	0x2000
#define NTB_LINK_SPEED_MASK	0x000f
#define NTB_LINK_WIDTH_MASK	0x03f0
#define NTB_LNK_STA_WIDTH(sta)	(((sta) & NTB_LINK_WIDTH_MASK) >> 4)

#define XEON_SNB_MW_COUNT	2
#define XEON_HSX_SPLIT_MW_COUNT	3
/* Reserve the uppermost bit for link interrupt */
#define XEON_DB_COUNT		15
#define XEON_DB_TOTAL_SHIFT	16
#define XEON_DB_LINK		15
#define XEON_DB_MSIX_VECTOR_COUNT	4
#define XEON_DB_MSIX_VECTOR_SHIFT	5
#define XEON_DB_LINK_BIT	(1 << XEON_DB_LINK)
#define XEON_NONLINK_DB_MSIX_BITS	3

#define XEON_SPCICMD_OFFSET	0x0504
#define XEON_DEVCTRL_OFFSET	0x0598
#define XEON_DEVSTS_OFFSET	0x059a
#define XEON_LINK_STATUS_OFFSET	0x01a2
#define XEON_SLINK_STATUS_OFFSET	0x05a2

#define XEON_PBAR2LMT_OFFSET	0x0000
#define XEON_PBAR4LMT_OFFSET	0x0008
#define XEON_PBAR5LMT_OFFSET	0x000c
#define XEON_PBAR2XLAT_OFFSET	0x0010
#define XEON_PBAR4XLAT_OFFSET	0x0018
#define XEON_PBAR5XLAT_OFFSET	0x001c
#define XEON_SBAR2LMT_OFFSET	0x0020
#define XEON_SBAR4LMT_OFFSET	0x0028
#define XEON_SBAR5LMT_OFFSET	0x002c
#define XEON_SBAR2XLAT_OFFSET	0x0030
#define XEON_SBAR4XLAT_OFFSET	0x0038
#define XEON_SBAR5XLAT_OFFSET	0x003c
#define XEON_SBAR0BASE_OFFSET	0x0040
#define XEON_SBAR2BASE_OFFSET	0x0048
#define XEON_SBAR4BASE_OFFSET	0x0050
#define XEON_SBAR5BASE_OFFSET	0x0054
#define XEON_NTBCNTL_OFFSET	0x0058
#define XEON_SBDF_OFFSET	0x005c
#define XEON_PDOORBELL_OFFSET	0x0060
#define XEON_PDBMSK_OFFSET	0x0062
#define XEON_SDOORBELL_OFFSET	0x0064
#define XEON_SDBMSK_OFFSET	0x0066
#define XEON_USMEMMISS_OFFSET	0x0070
#define XEON_SPAD_OFFSET	0x0080
#define XEON_SPAD_COUNT		16
#define XEON_SPADSEMA4_OFFSET	0x00c0
#define XEON_WCCNTRL_OFFSET	0x00e0
#define XEON_UNCERRSTS_OFFSET	0x014c
#define XEON_CORERRSTS_OFFSET	0x0158
#define XEON_B2B_SPAD_OFFSET	0x0100
#define XEON_B2B_DOORBELL_OFFSET	0x0140
#define XEON_B2B_XLAT_OFFSETL	0x0144
#define XEON_B2B_XLAT_OFFSETU	0x0148

#define ATOM_MW_COUNT		2
#define ATOM_DB_COUNT		34
#define ATOM_DB_MSIX_VECTOR_COUNT	34
#define ATOM_DB_MSIX_VECTOR_SHIFT	1

#define ATOM_SPCICMD_OFFSET	0xb004
#define ATOM_MBAR23_OFFSET	0xb018
#define ATOM_MBAR45_OFFSET	0xb020
#define ATOM_DEVCTRL_OFFSET	0xb048
#define ATOM_LINK_STATUS_OFFSET	0xb052
#define ATOM_ERRCORSTS_OFFSET	0xb110

#define ATOM_SBAR2XLAT_OFFSET	0x0008
#define ATOM_SBAR4XLAT_OFFSET	0x0010
#define ATOM_PDOORBELL_OFFSET	0x0020
#define ATOM_PDBMSK_OFFSET	0x0028
#define ATOM_NTBCNTL_OFFSET	0x0060
#define ATOM_EBDF_OFFSET		0x0064
#define ATOM_SPAD_OFFSET		0x0080
#define ATOM_SPAD_COUNT		16
#define ATOM_SPADSEMA_OFFSET	0x00c0
#define ATOM_STKYSPAD_OFFSET	0x00c4
#define ATOM_PBAR2XLAT_OFFSET	0x8008
#define ATOM_PBAR4XLAT_OFFSET	0x8010
#define ATOM_B2B_DOORBELL_OFFSET	0x8020
#define ATOM_B2B_SPAD_OFFSET	0x8080
#define ATOM_B2B_SPADSEMA_OFFSET	0x80c0
#define ATOM_B2B_STKYSPAD_OFFSET	0x80c4

#define ATOM_MODPHY_PCSREG4	0x1c004
#define ATOM_MODPHY_PCSREG6	0x1c006

#define ATOM_IP_BASE		0xc000
#define ATOM_DESKEWSTS_OFFSET	(ATOM_IP_BASE + 0x3024)
#define	ATOM_LTSSMERRSTS0_OFFSET (ATOM_IP_BASE + 0x3180)
#define ATOM_LTSSMSTATEJMP_OFFSET	(ATOM_IP_BASE + 0x3040)
#define ATOM_IBSTERRRCRVSTS0_OFFSET	(ATOM_IP_BASE + 0x3324)

#define ATOM_DESKEWSTS_DBERR		(1 << 15)
#define ATOM_LTSSMERRSTS0_UNEXPECTEDEI	(1 << 20)
#define ATOM_LTSSMSTATEJMP_FORCEDETECT	(1 << 2)
#define ATOM_IBIST_ERR_OFLOW		0x7fff7fff

#define NTB_CNTL_CFG_LOCK		(1 << 0)
#define NTB_CNTL_LINK_DISABLE		(1 << 1)
#define NTB_CNTL_S2P_BAR23_SNOOP	(1 << 2)
#define NTB_CNTL_P2S_BAR23_SNOOP	(1 << 4)
#define NTB_CNTL_S2P_BAR4_SNOOP		(1 << 6)
#define NTB_CNTL_P2S_BAR4_SNOOP		(1 << 8)
#define NTB_CNTL_S2P_BAR5_SNOOP		(1 << 12)
#define NTB_CNTL_P2S_BAR5_SNOOP		(1 << 14)
#define ATOM_CNTL_LINK_DOWN		(1 << 16)

#define XEON_PBAR23SZ_OFFSET	0x00d0
#define XEON_PBAR45SZ_OFFSET	0x00d1
#define XEON_PBAR4SZ_OFFSET	0x00d1
#define XEON_PBAR5SZ_OFFSET	0x00d5
#define XEON_SBAR23SZ_OFFSET	0x00d2
#define XEON_SBAR4SZ_OFFSET	0x00d3
#define XEON_SBAR5SZ_OFFSET	0x00d6
#define NTB_PPD_OFFSET		0x00d4
#define XEON_PPD_CONN_TYPE	0x0003
#define XEON_PPD_DEV_TYPE	0x0010
#define XEON_PPD_SPLIT_BAR	0x0040
#define ATOM_PPD_INIT_LINK	0x0008
#define ATOM_PPD_CONN_TYPE	0x0300
#define ATOM_PPD_DEV_TYPE	0x1000

/* All addresses are in low 32-bit space so 32-bit BARs can function */
#define XEON_B2B_BAR0_ADDR	0x1000000000000000ull
#define XEON_B2B_BAR2_ADDR64	0x2000000000000000ull
#define XEON_B2B_BAR4_ADDR64	0x4000000000000000ull
#define XEON_B2B_BAR4_ADDR32	0x20000000ull
#define XEON_B2B_BAR5_ADDR32	0x40000000ull

/* The peer ntb secondary config space is 32KB fixed size */
#define XEON_B2B_MIN_SIZE		0x8000
#define XEON_GEN3_MW_COUNT		2
#define XEON_GEN3_SPLIT_MW_COUNT	3
#define XEON_GEN3_SPAD_COUNT		16
#define XEON_GEN3_DB_COUNT		32
#define XEON_GEN3_DB_LINK		32
#define XEON_GEN3_DB_LINK_BIT		(1ULL << XEON_GEN3_DB_LINK)
#define XEON_GEN3_DB_MSIX_VECTOR_COUNT	33
#define XEON_GEN3_DB_MSIX_VECTOR_SHIFT	1

#define XEON_GEN3_LINK_VECTOR_INDEX	31

/* Xeon Skylake NTB register definitions */

/*
 * Internal EndPoint Configuration Registers
 */
#define XEON_GEN3_INT_REG_BAR0BASE	0x10
#define XEON_GEN3_INT_REG_BAR1BASE	0x18
#define XEON_GEN3_INT_REG_BAR2BASE	0x20
#define XEON_GEN3_INT_REG_IMBAR1SZ	0xd0
#define XEON_GEN3_INT_REG_IMBAR2SZ	0xd1
#define XEON_GEN3_INT_REG_EMBAR1SZ	0xd2
#define XEON_GEN3_INT_REG_EMBAR2SZ	0xd3
#define XEON_GEN3_INT_REG_PPD		0xd4
#define XEON_GEN3_INT_LNK_STS_OFFSET	0x01a2

/*
 * External EndPoint Configuration Registers
 * These are located within BAR0 of the internal endpoint.
 */
#define XEON_GEN3_EXT_REG_PCI_CMD	0x4504
#define XEON_GEN3_EXT_REG_BAR0BASE	0x4510
#define XEON_GEN3_EXT_REG_BAR1BASE	0x4518
#define XEON_GEN3_EXT_REG_BAR2BASE	0x4520

/*
 * Internal Endpoint Memory Mapped Registers
 */
#define XEON_GEN3_REG_IMNTB_CTRL	0x0000
#define XEON_GEN3_REG_IMBAR1XBASE	0x0010
#define XEON_GEN3_REG_IMBAR1XLIMIT	0x0018
#define XEON_GEN3_REG_IMBAR2XBASE	0x0020
#define XEON_GEN3_REG_IMBAR2XLIMIT	0x0028
#define XEON_GEN3_REG_IMINT_STATUS	0x0040
#define XEON_GEN3_REG_IMINT_DISABLE	0x0048
#define XEON_GEN3_REG_IMSPAD		0x0080
#define XEON_GEN3_REG_IMINTVEC00	0x00d0
#define XEON_GEN3_REG_IMDOORBELL	0x0100
#define XEON_GEN3_REG_IMB2B_SSPAD	0x0180	/* Pseudo SP registers */

/*
 * External Endpoint Memory Mapped Registers
 */
#define XEON_GEN3_REG_EMBAR0XBASE	0x4008
#define XEON_GEN3_REG_EMBAR1XBASE	0x4010
#define XEON_GEN3_REG_EMBAR1XLIMIT	0x4018
#define XEON_GEN3_REG_EMBAR2XBASE	0x4020
#define XEON_GEN3_REG_EMBAR2XLIMIT	0x4028
#define XEON_GEN3_REG_EMINT_STATUS	0x4040
#define XEON_GEN3_REG_EMINT_DISABLE	0x4048
#define XEON_GEN3_REG_EMSPAD		0x4080
#define XEON_GEN3_REG_EMDOORBELL	0x4100

/* XEON_GEN3_INT_REG_PPD: PPD register */
#define XEON_GEN3_REG_PPD_PORT_DEF_F(X)		M8_F(X, 0, 2)
#define XEON_GEN3_REG_PPD_CONF_STS_F(X)		M8_F(X, 4, 1)
#define XEON_GEN3_REG_PPD_ONE_MSIX_F(X)		M8_F(X, 5, 1)
#define XEON_GEN3_REG_PPD_BAR45_SPL_F(X)	M8_F(X, 6, 1)

#endif /* _NTB_REGS_H_ */
