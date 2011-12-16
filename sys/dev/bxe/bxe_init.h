/*-
 * Copyright (c) 2007-2011 Broadcom Corporation. All rights reserved.
 *
 *    Gary Zambrano <zambrano@broadcom.com>
 *    David Christensen <davidch@broadcom.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Broadcom Corporation nor the name of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written consent.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

 /*$FreeBSD$*/

#ifndef BXE_INIT_H
#define	BXE_INIT_H
/*
 * bxe_init.h: Broadcom Everest network driver.
 * Structures and macros needed during the initialization.
 */

/* RAM0 size in bytes */
#define	STORM_INTMEM_SIZE_E1		0x5800
#define	STORM_INTMEM_SIZE_E1H		0x10000
#define	STORM_INTMEM_SIZE(sc)		\
	((CHIP_IS_E1(sc) ? STORM_INTMEM_SIZE_E1 : STORM_INTMEM_SIZE_E1H) / 4)


/* Init operation types and structures */
/* Common for both E1 and E1H */
#define	OP_RD			0x1 /* read single register */
#define	OP_WR			0x2 /* write single register */
#define	OP_IW			0x3 /* write single register using mailbox */
#define	OP_SW			0x4 /* copy a string to the device */
#define	OP_SI			0x5 /* copy a string using mailbox */
#define	OP_ZR			0x6 /* clear memory */
#define	OP_ZP			0x7 /* unzip then copy with DMAE */
#define	OP_WR_64		0x8 /* write 64 bit pattern */
#define	OP_WB			0x9 /* copy a string using DMAE */

/* FPGA and EMUL specific operations */
#define	OP_WR_EMUL		0xa /* write single register on Emulation */
#define	OP_WR_FPGA		0xb /* write single register on FPGA */
#define	OP_WR_ASIC		0xc /* write single register on ASIC */

/* Init stages */
/* Never reorder stages !!! */
#define	COMMON_STAGE		0
#define	PORT0_STAGE		1
#define	PORT1_STAGE		2
#define	FUNC0_STAGE		3
#define	FUNC1_STAGE		4
#define	FUNC2_STAGE		5
#define	FUNC3_STAGE		6
#define	FUNC4_STAGE		7
#define	FUNC5_STAGE		8
#define	FUNC6_STAGE		9
#define	FUNC7_STAGE		10
#define	STAGE_IDX_MAX		11

#define	STAGE_START		0
#define	STAGE_END		1


/* Indices of blocks */
#define	PRS_BLOCK		0
#define	SRCH_BLOCK		1
#define	TSDM_BLOCK		2
#define	TCM_BLOCK		3
#define	BRB1_BLOCK		4
#define	TSEM_BLOCK		5
#define	PXPCS_BLOCK		6
#define	EMAC0_BLOCK		7
#define	EMAC1_BLOCK		8
#define	DBU_BLOCK		9
#define	MISC_BLOCK		10
#define	DBG_BLOCK		11
#define	NIG_BLOCK		12
#define	MCP_BLOCK		13
#define	UPB_BLOCK		14
#define	CSDM_BLOCK		15
#define	USDM_BLOCK		16
#define	CCM_BLOCK		17
#define	UCM_BLOCK		18
#define	USEM_BLOCK		19
#define	CSEM_BLOCK		20
#define	XPB_BLOCK		21
#define	DQ_BLOCK		22
#define	TIMERS_BLOCK		23
#define	XSDM_BLOCK		24
#define	QM_BLOCK		25
#define	PBF_BLOCK		26
#define	XCM_BLOCK		27
#define	XSEM_BLOCK		28
#define	CDU_BLOCK		29
#define	DMAE_BLOCK		30
#define	PXP_BLOCK		31
#define	CFC_BLOCK		32
#define	HC_BLOCK		33
#define	PXP2_BLOCK		34
#define	MISC_AEU_BLOCK		35
#define	PGLUE_B_BLOCK		36
#define	IGU_BLOCK		37

/* Returns the index of start or end of a specific block stage in ops array. */
#define	BLOCK_OPS_IDX(block, stage, end)	\
	(2 * (((block) * STAGE_IDX_MAX) + (stage)) + (end))

struct raw_op {
	uint32_t	op:8;
	uint32_t	offset:24;
	uint32_t	raw_data;
};

struct op_read {
	uint32_t	op:8;
	uint32_t	offset:24;
	uint32_t	pad;
};

struct op_write {
	uint32_t	op:8;
	uint32_t	offset:24;
	uint32_t	val;
};

struct op_string_write {
	uint32_t	op:8;
	uint32_t	offset:24;
#ifdef __LITTLE_ENDIAN
	uint16_t	data_off;
	uint16_t	data_len;
#else /* __BIG_ENDIAN */
	uint16_t	data_len;
	uint16_t	data_off;
#endif
};

struct op_zero {
	uint32_t	op:8;
	uint32_t	offset:24;
	uint32_t	len;
};

union init_op {
	struct op_read		read;
	struct op_write		write;
	struct op_string_write	str_wr;
	struct op_zero		zero;
	struct raw_op		raw;
};

#include "bxe_init_values_e1.h"
#include "bxe_init_values_e1h.h"

#endif /* BXE_INIT_H */

