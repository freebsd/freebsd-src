/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019-2020 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#ifndef	_ARM64_IOMMU_SMMUVAR_H_
#define	_ARM64_IOMMU_SMMUVAR_H_

#define	SMMU_DEVSTR		"ARM System Memory Management Unit"
#define	SMMU_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	SMMU_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx)

DECLARE_CLASS(smmu_driver);

struct smmu_unit {
	struct iommu_unit		iommu;
	LIST_HEAD(, smmu_domain)	domain_list;
	LIST_ENTRY(smmu_unit)		next;
	device_t			dev;
	intptr_t			xref;
};

struct smmu_domain {
	struct iommu_domain		iodom;
	LIST_HEAD(, smmu_ctx)		ctx_list;
	LIST_ENTRY(smmu_domain)	next;
	u_int entries_cnt;
	struct smmu_cd			*cd;
	struct pmap			p;
	uint16_t			asid;
};

struct smmu_ctx {
	struct iommu_ctx		ioctx;
	struct smmu_domain		*domain;
	LIST_ENTRY(smmu_ctx)		next;
	device_t			dev;
	bool				bypass;
	int				sid;
	uint16_t			vendor;
	uint16_t			device;
};

struct smmu_queue_local_copy {
	union {
		uint64_t val;
		struct {
			uint32_t prod;
			uint32_t cons;
		};
	};
};

struct smmu_cd {
	vm_paddr_t paddr;
	vm_size_t size;
	void *vaddr;
};

struct smmu_queue {
	struct smmu_queue_local_copy lc;
	vm_paddr_t paddr;
	void *vaddr;
	uint32_t prod_off;
	uint32_t cons_off;
	int size_log2;
	uint64_t base;
};

struct smmu_cmdq_entry {
	uint8_t opcode;
	union {
		struct {
			uint16_t asid;
			uint16_t vmid;
			vm_offset_t addr;
			bool leaf;
		} tlbi;
		struct {
			uint32_t sid;
			uint32_t ssid;
			bool leaf;
		} cfgi;
		struct {
			uint32_t sid;
		} prefetch;
		struct {
			uint64_t msiaddr;
		} sync;
	};
};

struct l1_desc {
	uint8_t		span;
	size_t		size;
	void		*va;
	vm_paddr_t	pa;
};

struct smmu_strtab {
	void		*vaddr;
	uint64_t	base;
	uint32_t	base_cfg;
	uint32_t	num_l1_entries;
	struct l1_desc	*l1;
};

struct smmu_softc {
	device_t		dev;
	struct resource		*res[4];
	void			*intr_cookie[3];
	uint32_t		ias; /* Intermediate Physical Address */
	uint32_t		oas; /* Physical Address */
	uint32_t		asid_bits;
	uint32_t		vmid_bits;
	uint32_t		sid_bits;
	uint32_t		ssid_bits;
	uint32_t		pgsizes;
	uint32_t		features;
#define	SMMU_FEATURE_2_LVL_STREAM_TABLE		(1 << 0)
#define	SMMU_FEATURE_2_LVL_CD			(1 << 1)
#define	SMMU_FEATURE_TT_LE			(1 << 2)
#define	SMMU_FEATURE_TT_BE			(1 << 3)
#define	SMMU_FEATURE_SEV			(1 << 4)
#define	SMMU_FEATURE_MSI			(1 << 5)
#define	SMMU_FEATURE_HYP			(1 << 6)
#define	SMMU_FEATURE_ATS			(1 << 7)
#define	SMMU_FEATURE_PRI			(1 << 8)
#define	SMMU_FEATURE_STALL_FORCE		(1 << 9)
#define	SMMU_FEATURE_STALL			(1 << 10)
#define	SMMU_FEATURE_S1P			(1 << 11)
#define	SMMU_FEATURE_S2P			(1 << 12)
#define	SMMU_FEATURE_VAX			(1 << 13)
#define	SMMU_FEATURE_COHERENCY			(1 << 14)
#define	SMMU_FEATURE_RANGE_INV			(1 << 15)
	struct smmu_queue cmdq;
	struct smmu_queue evtq;
	struct smmu_queue priq;
	struct smmu_strtab strtab;
	int				sync;
	struct mtx			sc_mtx;
	bitstr_t			*asid_set;
	int				asid_set_size;
	struct mtx			asid_set_mutex;
	struct smmu_unit		unit;
	uintptr_t			xref;
};

MALLOC_DECLARE(M_SMMU);

/* Device methods */
int smmu_attach(device_t dev);
int smmu_detach(device_t dev);

struct smmu_ctx *smmu_ctx_lookup_by_sid(device_t dev, u_int sid);
bool smmu_quirks_check(device_t dev, u_int sid, uint8_t event_id,
    uintptr_t input_addr);

#endif /* _ARM64_IOMMU_SMMUVAR_H_ */
