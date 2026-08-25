/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Ruslan Bukin <br@bsdpad.com>
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

#ifndef _RISCV_IOMMU_IOMMU_H_
#define _RISCV_IOMMU_IOMMU_H_

#define RISCV_IOMMU_CAPABILITIES	0x0000
#define	 CAPABILITIES_VERSION_S		0
#define	 CAPABILITIES_VERSION_M		(0xff << CAPABILITIES_VERSION_S)
#define	 CAPABILITIES_SV32		(1 << 8)
#define	 CAPABILITIES_SV39		(1 << 9)
#define	 CAPABILITIES_SV48		(1 << 10)
#define	 CAPABILITIES_SV57		(1 << 11)
#define	 CAPABILITIES_SVPBMT		(1 << 15)
#define	 CAPABILITIES_SV32X4		(1 << 16)
#define	 CAPABILITIES_SV39X4		(1 << 17)
#define	 CAPABILITIES_SV48X4		(1 << 18)
#define	 CAPABILITIES_SV57X4		(1 << 19)
#define	 CAPABILITIES_AMO_MRIF		(1 << 21)
#define	 CAPABILITIES_MSI_FLAT		(1 << 22)
#define	 CAPABILITIES_MSI_MRIF		(1 << 23)
#define	 CAPABILITIES_AMO_HWAD		(1 << 24)
#define	 CAPABILITIES_ATS		(1 << 25)
#define	 CAPABILITIES_T2GPA		(1 << 26)
#define	 CAPABILITIES_END		(1 << 27)
#define	 CAPABILITIES_IGS_S		28
#define	 CAPABILITIES_IGS_M		(0x3 << CAPABILITIES_IGS_S)
#define	 CAPABILITIES_HPM		(1 << 30)
#define	 CAPABILITIES_DBG		(1 << 31)
#define	 CAPABILITIES_PAS_S		32ULL
#define	 CAPABILITIES_PAS_M		(0x3f << CAPABILITIES_PAS_S)
#define	 CAPABILITIES_PD8		(1ULL << 38)
#define	 CAPABILITIES_PD17		(1ULL << 39)
#define	 CAPABILITIES_PD20		(1ULL << 40)
#define	RISCV_IOMMU_FCTL		0x0008
#define	 FCTL_BE			(1 << 0) /* Big-endian */
#define	 FCTL_WSI			(1 << 1) /* Wire-signalled Ints. */
#define	 FCTL_GXL			(1 << 2) /* Guest physical addresses */
#define RISCV_IOMMU_DDTP		0x0010
#define	 DDTP_IOMMU_MODE_S	0
#define	 DDTP_IOMMU_MODE_OFF	(0 << DDTP_IOMMU_MODE_S)
#define	 DDTP_IOMMU_MODE_BARE	(1 << DDTP_IOMMU_MODE_S)
#define	 DDTP_IOMMU_MODE_1LVL	(2 << DDTP_IOMMU_MODE_S)
#define	 DDTP_IOMMU_MODE_2LVL	(3 << DDTP_IOMMU_MODE_S)
#define	 DDTP_IOMMU_MODE_3LVL	(4 << DDTP_IOMMU_MODE_S)
#define	 DDTP_BUSY		(1 << 4)
#define	 DDTP_PPN_S		10
#define	 DDTP_PPN_M		(0xfffffffffffULL << DDTP_PPN_S)
#define	RISCV_IOMMU_CQB		0x18 /* Command queue base. */
#define	 CQB_LOG2SZ_1_S		0
#define	 CQB_LOG2SZ_1_M		(0x3f << CQB_LOG2SZ_1_S)
#define	 CQB_PPN_S		10
#define	 CQB_PPN_M		(0xfffffffffffULL << CQB_PPN_S)
#define	RISCV_IOMMU_CQH		0x20
#define	RISCV_IOMMU_CQT		0x24
#define	RISCV_IOMMU_FQB		0x28 /* Fault queue base. */
#define	RISCV_IOMMU_FQH		0x30
#define	RISCV_IOMMU_FQT		0x34
#define	RISCV_IOMMU_PQB		0x38 /* Page queue base. */
#define	RISCV_IOMMU_PQH		0x40
#define	RISCV_IOMMU_PQT		0x44
#define	RISCV_IOMMU_CQCSR	0x48
#define	 CQCSR_BUSY		(1 << 17) /* Write is observed */
#define	 CQCSR_CQON		(1 << 16) /* Active */
#define	 CQCSR_FENCE_W_IP	(1 << 11) /* iofence.c completed */
#define	 CQCSR_CMD_ILL		(1 << 10) /* Illegal command */
#define	 CQCSR_CMD_TO		(1 << 9) /* Timeout */
#define	 CQCSR_CQMF		(1 << 8) /* Memory Fault */
#define	 CQCSR_CIE		(1 << 1) /* Interrupt Enable */
#define	 CQCSR_CQEN		(1 << 0) /* Enable */
#define	RISCV_IOMMU_FQCSR	0x4C
#define	 FQCSR_BUSY		(1 << 17) /* Write is observed */
#define	 FQCSR_FQON		(1 << 16) /* Active */
#define	 FQCSR_FQOF		(1 << 9) /* Overflow */
#define	 FQCSR_FQMF		(1 << 8) /* Memory Fault */
#define	 FQCSR_FIE		(1 << 1) /* Interrupt Enable */
#define	 FQCSR_FQEN		(1 << 0) /* Enable */
#define	RISCV_IOMMU_PQCSR	0x50
#define	 PQCSR_BUSY		(1 << 17) /* Write is observed */
#define	 PQCSR_PQON		(1 << 16) /* Active */
#define	 PQCSR_PQOF		(1 << 9) /* Overflow */
#define	 PQCSR_PQMF		(1 << 8) /* Memory Fault */
#define	 PQCSR_PIE		(1 << 1) /* Interrupt Enable */
#define	 PQCSR_PQEN		(1 << 0) /* Enable */
#define	RISCV_IOMMU_IPSR	0x54
#define	 IPSR_CIP		(1 << 0) /* Command queue interrupt pending */
#define	 IPSR_FIP		(1 << 1) /* Fault queue interrupt pending */
#define	 IPSR_PMIP		(1 << 2) /* Performance monitoring int pend */
#define	 IPSR_PIP		(1 << 3) /* Page queue interrupt pending */

#define	RISCV_IOMMU_IOCOUNTOVF		0x0058
#define	RISCV_IOMMU_IOCOUNTINH		0x005C

#define	RISCV_IOMMU_IOHPMCYCLES		0x0060
#define	RISCV_IOMMU_IOHPMCTR_BASE	0x0068
#define	RISCV_IOMMU_IOHPMCTR(_n)	(RISCV_IOMMU_IOHPMCTR_BASE + ((_n) * 0x8))
#define	RISCV_IOMMU_IOHPMEVT_BASE	0x0160
#define	RISCV_IOMMU_IOHPMEVT(_n)	(RISCV_IOMMU_IOHPMEVT_BASE + ((_n) * 0x8))
#define	RISCV_IOMMU_TR_REQ_IOVA		0x0258
#define	RISCV_IOMMU_TR_REQ_CTL		0x0260
#define	RISCV_IOMMU_TR_RESPONSE		0x0268
#define	RISCV_IOMMU_ICVEC		0x02F8

#define	RISCV_IOMMU_LOCK(_sc)		mtx_lock(&(_sc)->mtx)
#define	RISCV_IOMMU_UNLOCK(_sc)		mtx_unlock(&(_sc)->mtx)

DECLARE_CLASS(riscv_iommu_driver);

MALLOC_DECLARE(M_IOMMU);

struct riscv_iommu_unit {
	struct iommu_unit		iommu;
	LIST_HEAD(, riscv_iommu_domain)	domain_list;
	LIST_ENTRY(riscv_iommu_unit)	next;
	device_t			dev;
	intptr_t			xref;
};

struct riscv_iommu_domain {
	struct iommu_domain		iodom;
	LIST_HEAD(, riscv_iommu_ctx)	ctx_list;
	LIST_ENTRY(riscv_iommu_domain)	next;
	u_int entries_cnt;
	struct riscv_iommu_cd		*cd;
	struct riscv_iommu_pmap		p;
	uint16_t			pscid;
};

struct riscv_iommu_ctx {
	struct iommu_ctx		ioctx;
	struct riscv_iommu_domain	*domain;
	LIST_ENTRY(riscv_iommu_ctx)	next;
	device_t			dev;
	bool				bypass;
	int				did;
	uint16_t			vendor;
	uint16_t			device;
	u_int				refcnt;
};

struct riscv_iommu_queue_local_copy {
	union {
		uint64_t val;
		struct {
			uint32_t head;
			uint32_t tail;
		};
	};
};

struct riscv_iommu_queue {
	struct riscv_iommu_queue_local_copy lc;
	vm_paddr_t paddr;
	void *vaddr;
	uint64_t mask;
	uint32_t head_off;
	uint32_t tail_off;
	int size_log2;
	uint64_t base;
	uint64_t csr;
	int idx;
	uint8_t entry_size;
};

struct l1_desc {
	uint8_t		span;
	void		*va;
	vm_paddr_t	pa;
};

/* Base-format device-context. */
struct riscv_iommu_dc_base {
	uint64_t tc;		/* Translation control */
#define	 DC_TC_V		(1 << 0)
#define	 DC_TC_EN_ATS		(1 << 1)
#define	 DC_TC_EN_PRI		(1 << 2)
#define	 DC_TC_T2GPA		(1 << 3)
#define	 DC_TC_DTF		(1 << 4)
#define	 DC_TC_PDTV		(1 << 5)
#define	 DC_TC_PRPR		(1 << 6)
#define	 DC_TC_GADE		(1 << 7)
#define	 DC_TC_SADE		(1 << 8)
#define	 DC_TC_DPE		(1 << 9)
#define	 DC_TC_SBE		(1 << 10)
#define	 DC_TC_SXL		(1 << 11)
	uint64_t iohgatp;	/* IO Hyp guest address translation */
	uint64_t ta;		/* Translation attributes */
#define	 DC_TA_V		(1 << 0)
#define	 DC_TA_ENS		(1 << 1)
#define	 DC_TA_SUM		(1 << 2)
#define	 DC_TA_PSCID_S		12
#define	 DC_TA_PSCID_M		(0xfffff << DC_TA_PSCID_S)
	uint64_t fsc;		/* First-stage-context */
};

/* Extended-format device-context. */
struct riscv_iommu_dc {
	struct riscv_iommu_dc_base base;
	uint64_t msiptp;	/* MSI page table pointer */
	uint64_t msi_addr_mask;
	uint64_t msi_addr_pattern;
	uint64_t _reserved;
};

#define	DC_NON_LEAF_ENTRY_PPN_S	10
#define	DC_NON_LEAF_ENTRY_VALID	(1 << 0)

struct riscv_iommu_ddt {
	void		*vaddr;
	uint64_t	base;
	uint32_t	base_cfg;
	uint32_t	num_top_entries;
	struct l1_desc	*l1;
	struct riscv_iommu_dc *dc;
};

struct riscv_iommu_softc {
	device_t			dev;
	intptr_t			xref;
	struct riscv_iommu_unit		unit;
	struct resource			*res[5];
	void				*intr_cookie[4];
	struct riscv_iommu_queue	cq;
	struct riscv_iommu_queue	fq;
	struct riscv_iommu_queue	pq;
	struct riscv_iommu_ddt		ddt;
	struct mtx			mtx;
	uint32_t			l0_did_bits;
	uint32_t			dc_dwords;

	/* PSCID management. */
	bitstr_t			*pscid_set;
	int				pscid_set_size;
	struct mtx			pscid_set_mutex;
	uint32_t			pscid_bits;

	enum pmap_mode			pm_mode;
	int				iommu_mode;
};

/*
 * Command queue request.
 */
struct riscv_iommu_command {
	uint64_t dword0;
	uint64_t dword1;
};

enum riscv_iommu_fq_causes {
	FQ_CAUSE_INST_FAULT		= 1,
	FQ_CAUSE_RD_ADDR_MISALIGNED	= 4,
	FQ_CAUSE_RD_FAULT		= 5,
	FQ_CAUSE_WR_ADDR_MISALIGNED	= 6,
	FQ_CAUSE_WR_FAULT		= 7,
	FQ_CAUSE_INST_FAULT_S		= 12,
	FQ_CAUSE_RD_FAULT_S		= 13,
	FQ_CAUSE_WR_FAULT_S		= 15,
	FQ_CAUSE_INST_FAULT_VS		= 20,
	FQ_CAUSE_RD_FAULT_VS		= 21,
	FQ_CAUSE_WR_FAULT_VS		= 23,
	FQ_CAUSE_DMA_DISABLED		= 256,
	FQ_CAUSE_DDT_LOAD_FAULT		= 257,
	FQ_CAUSE_DDT_INVALID		= 258,
	FQ_CAUSE_DDT_MISCONFIGURED	= 259,
	FQ_CAUSE_TR_TYPE_DISALLOWED	= 260,
	FQ_CAUSE_MSI_LOAD_FAULT		= 261,
	FQ_CAUSE_MSI_INVALID		= 262,
	FQ_CAUSE_MSI_MISCONFIGURED	= 263,
	FQ_CAUSE_MRIF_FAULT		= 264,
	FQ_CAUSE_PDT_LOAD_FAULT		= 265,
	FQ_CAUSE_PDT_INVALID		= 266,
	FQ_CAUSE_PDT_MISCONFIGURED	= 267,
	FQ_CAUSE_DDT_CORRUPTED		= 268,
	FQ_CAUSE_PDT_CORRUPTED		= 269,
	FQ_CAUSE_MSI_PT_CORRUPTED	= 270,
	FQ_CAUSE_MRIF_CORRUPTED		= 271,
	FQ_CAUSE_INTERNAL_DP_ERROR	= 272,
	FQ_CAUSE_MSI_WR_FAULT		= 273,
	FQ_CAUSE_PT_CORRUPTED		= 274,
};

/*
 * Fault queue record.
 */
struct riscv_iommu_fq_record {
	uint64_t hdr;
#define	FQR_HDR_CAUSE_S	0
#define	FQR_HDR_CAUSE_M	(0xfff << FQR_HDR_CAUSE_S)
#define	FQR_HDR_PID_S	12
#define	FQR_HDR_PID_M	(0xfffffULL << FQR_HDR_PID_S)
#define	FQR_HDR_PV	(1ULL << 32)
#define	FQR_HDR_PRIV	(1ULL << 33)
#define	FQR_HDR_TTYP_S	34ULL
#define	FQR_HDR_TTYP_M	(0x3fULL << FQR_HDR_TTYP_S)
#define	FQR_HDR_DID_S	40ULL
#define	FQR_HDR_DID_M	(0xffffffULL << FQR_HDR_DID_S)
	uint32_t custom;
	uint32_t reserved;
	uint64_t iotval;
	uint64_t iotval2;
};

#define	COMMAND_OPCODE_S	0
#define	COMMAND_OPCODE_IOTINVAL	(1 << COMMAND_OPCODE_S)
#define	COMMAND_OPCODE_IOFENCE	(2 << COMMAND_OPCODE_S)
#define	COMMAND_OPCODE_IODIR	(3 << COMMAND_OPCODE_S)
#define	COMMAND_OPCODE_ATS	(4 << COMMAND_OPCODE_S)
#define	COMMAND_OPCODE_FUNC_S	7
#define	COMMAND_OPCODE_FUNC_M	(0x3 << COMMAND_OPCODE_FUNC_S)
#define	 FUNC_IODIR_INVAL_DDT	(0 << COMMAND_OPCODE_FUNC_S)
#define	 FUNC_IODIR_INVAL_PDT	(1 << COMMAND_OPCODE_FUNC_S)
#define	 FUNC_IODIR_PID_S	12
#define	 FUNC_IODIR_DV		(1ULL << 33)	/* DID Valid */
#define	 FUNC_IODIR_DID_S	40ULL
/* dword0 */
#define	 FUNC_IOTINVAL_VMA	(0 << COMMAND_OPCODE_FUNC_S)
#define	 FUNC_IOTINVAL_GVMA	(1 << COMMAND_OPCODE_FUNC_S)
#define	 FUNC_IOTINVAL_AV	(1 << 10)	/* Address Valid */
#define	 FUNC_IOTINVAL_PSCID_S	12		/* Process-Soft-Context ID */
#define	 FUNC_IOTINVAL_PSCV	(1ULL << 32)	/* PSCID Valid */
#define	 FUNC_IOTINVAL_GV	(1ULL << 33)	/* GSCID Valid */
#define	 FUNC_IOTINVAL_GSCID_S	44		/* Guest-Soft-Context ID */
/* dword1 */
#define	 FUNC_IOTINVAL_ADDR_S	10
#define	 FUNC_IOFENCE_FUNC_C	(0 << 7)
#define	 FUNC_IOFENCE_AV	(1 << 10)
#define	 FUNC_IOFENCE_WSI	(1 << 11)
#define	 FUNC_IOFENCE_PR	(1 << 12)
#define	 FUNC_IOFENCE_PW	(1 << 13)
#define	 FUNC_IOFENCE_DATA_S	32ULL

int riscv_iommu_attach(device_t dev);
struct riscv_iommu_ctx *riscv_iommu_ctx_lookup_by_did(device_t dev, u_int did);

#endif /* _RISCV_IOMMU_IOMMU_H_ */
