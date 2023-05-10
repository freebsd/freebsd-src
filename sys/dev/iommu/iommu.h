/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef _DEV_IOMMU_IOMMU_H_
#define _DEV_IOMMU_IOMMU_H_

#include <dev/iommu/iommu_types.h>

struct bus_dma_tag_common;
struct iommu_map_entry;
TAILQ_HEAD(iommu_map_entries_tailq, iommu_map_entry);

RB_HEAD(iommu_gas_entries_tree, iommu_map_entry);
RB_PROTOTYPE(iommu_gas_entries_tree, iommu_map_entry, rb_entry,
    iommu_gas_cmp_entries);

struct iommu_qi_genseq {
	u_int gen;
	uint32_t seq;
};

struct iommu_map_entry {
	iommu_gaddr_t start;
	iommu_gaddr_t end;
	iommu_gaddr_t first;		/* Least start in subtree */
	iommu_gaddr_t last;		/* Greatest end in subtree */
	iommu_gaddr_t free_down;	/* Max free space below the
					   current R/B tree node */
	u_int flags;
	union {
		TAILQ_ENTRY(iommu_map_entry) dmamap_link; /* DMA map entries */
		struct iommu_map_entry *tlb_flush_next;
	};
	RB_ENTRY(iommu_map_entry) rb_entry;	 /* Links for domain entries */
	struct iommu_domain *domain;
	struct iommu_qi_genseq gseq;
};

struct iommu_unit {
	struct mtx lock;
	device_t dev;
	int unit;

	int dma_enabled;

	/* Busdma delayed map load */
	struct task dmamap_load_task;
	TAILQ_HEAD(, bus_dmamap_iommu) delayed_maps;
	struct taskqueue *delayed_taskqueue;

	/*
	 * Bitmap of buses for which context must ignore slot:func,
	 * duplicating the page table pointer into all context table
	 * entries.  This is a client-controlled quirk to support some
	 * NTBs.
	 */
	uint32_t buswide_ctxs[(PCI_BUSMAX + 1) / NBBY / sizeof(uint32_t)];
};

struct iommu_domain_map_ops {
	int (*map)(struct iommu_domain *domain, iommu_gaddr_t base,
	    iommu_gaddr_t size, vm_page_t *ma, uint64_t pflags, int flags);
	int (*unmap)(struct iommu_domain *domain, iommu_gaddr_t base,
	    iommu_gaddr_t size, int flags);
};

/*
 * Locking annotations:
 * (u) - Protected by iommu unit lock
 * (d) - Protected by domain lock
 * (c) - Immutable after initialization
 */

struct iommu_domain {
	struct iommu_unit *iommu;	/* (c) */
	const struct iommu_domain_map_ops *ops;
	struct mtx lock;		/* (c) */
	struct task unload_task;	/* (c) */
	u_int entries_cnt;		/* (d) */
	struct iommu_map_entries_tailq unload_entries; /* (d) Entries to
							 unload */
	struct iommu_gas_entries_tree rb_root; /* (d) */
	struct iommu_map_entry *start_gap;     /* (d) */
	iommu_gaddr_t end;		/* (c) Highest address + 1 in
					   the guest AS */
	struct iommu_map_entry *first_place, *last_place; /* (d) */
	struct iommu_map_entry *msi_entry; /* (d) Arch-specific */
	iommu_gaddr_t msi_base;		/* (d) Arch-specific */
	vm_paddr_t msi_phys;		/* (d) Arch-specific */
	u_int flags;			/* (u) */
};

struct iommu_ctx {
	struct iommu_domain *domain;	/* (c) */
	struct bus_dma_tag_iommu *tag;	/* (c) Root tag */
	u_long loads;			/* atomic updates, for stat only */
	u_long unloads;			/* same */
	u_int flags;			/* (u) */
	uint16_t rid;			/* (c) pci RID */
};

/* struct iommu_ctx flags */
#define	IOMMU_CTX_FAULTED	0x0001	/* Fault was reported,
					   last_fault_rec is valid */
#define	IOMMU_CTX_DISABLED	0x0002	/* Device is disabled, the
					   ephemeral reference is kept
					   to prevent context destruction */

#define	IOMMU_DOMAIN_GAS_INITED		0x0001
#define	IOMMU_DOMAIN_PGTBL_INITED	0x0002
#define	IOMMU_DOMAIN_IDMAP		0x0010	/* Domain uses identity
						   page table */
#define	IOMMU_DOMAIN_RMRR		0x0020	/* Domain contains RMRR entry,
						   cannot be turned off */

#define	IOMMU_LOCK(unit)		mtx_lock(&(unit)->lock)
#define	IOMMU_UNLOCK(unit)		mtx_unlock(&(unit)->lock)
#define	IOMMU_ASSERT_LOCKED(unit)	mtx_assert(&(unit)->lock, MA_OWNED)

#define	IOMMU_DOMAIN_LOCK(dom)		mtx_lock(&(dom)->lock)
#define	IOMMU_DOMAIN_UNLOCK(dom)	mtx_unlock(&(dom)->lock)
#define	IOMMU_DOMAIN_ASSERT_LOCKED(dom)	mtx_assert(&(dom)->lock, MA_OWNED)

void iommu_free_ctx(struct iommu_ctx *ctx);
void iommu_free_ctx_locked(struct iommu_unit *iommu, struct iommu_ctx *ctx);
struct iommu_ctx *iommu_get_ctx(struct iommu_unit *, device_t dev,
    uint16_t rid, bool id_mapped, bool rmrr_init);
struct iommu_unit *iommu_find(device_t dev, bool verbose);
void iommu_domain_unload_entry(struct iommu_map_entry *entry, bool free,
    bool cansleep);
void iommu_domain_unload(struct iommu_domain *domain,
    struct iommu_map_entries_tailq *entries, bool cansleep);

struct iommu_ctx *iommu_instantiate_ctx(struct iommu_unit *iommu,
    device_t dev, bool rmrr);
device_t iommu_get_requester(device_t dev, uint16_t *rid);
int iommu_init_busdma(struct iommu_unit *unit);
void iommu_fini_busdma(struct iommu_unit *unit);

void iommu_gas_init_domain(struct iommu_domain *domain);
void iommu_gas_fini_domain(struct iommu_domain *domain);
struct iommu_map_entry *iommu_gas_alloc_entry(struct iommu_domain *domain,
    u_int flags);
void iommu_gas_free_entry(struct iommu_map_entry *entry);
void iommu_gas_free_space(struct iommu_map_entry *entry);
void iommu_gas_remove(struct iommu_domain *domain, iommu_gaddr_t start,
    iommu_gaddr_t size);
int iommu_gas_map(struct iommu_domain *domain,
    const struct bus_dma_tag_common *common, iommu_gaddr_t size, int offset,
    u_int eflags, u_int flags, vm_page_t *ma, struct iommu_map_entry **res);
void iommu_gas_free_region(struct iommu_map_entry *entry);
int iommu_gas_map_region(struct iommu_domain *domain,
    struct iommu_map_entry *entry, u_int eflags, u_int flags, vm_page_t *ma);
int iommu_gas_reserve_region(struct iommu_domain *domain, iommu_gaddr_t start,
    iommu_gaddr_t end, struct iommu_map_entry **entry0);
int iommu_gas_reserve_region_extend(struct iommu_domain *domain,
    iommu_gaddr_t start, iommu_gaddr_t end);

void iommu_set_buswide_ctx(struct iommu_unit *unit, u_int busno);
bool iommu_is_buswide_ctx(struct iommu_unit *unit, u_int busno);
void iommu_domain_init(struct iommu_unit *unit, struct iommu_domain *domain,
    const struct iommu_domain_map_ops *ops);
void iommu_domain_fini(struct iommu_domain *domain);

bool bus_dma_iommu_set_buswide(device_t dev);
int bus_dma_iommu_load_ident(bus_dma_tag_t dmat, bus_dmamap_t map,
    vm_paddr_t start, vm_size_t length, int flags);

bus_dma_tag_t iommu_get_dma_tag(device_t dev, device_t child);
struct iommu_ctx *iommu_get_dev_ctx(device_t dev);
struct iommu_domain *iommu_get_ctx_domain(struct iommu_ctx *ctx);

SYSCTL_DECL(_hw_iommu);

#endif /* !_DEV_IOMMU_IOMMU_H_ */
