/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
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

#ifndef _SYS_IOMMU_H_
#define _SYS_IOMMU_H_

#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/tree.h>
#include <sys/types.h>

/* Host or physical memory address, after translation. */
typedef uint64_t iommu_haddr_t;
/* Guest or bus address, before translation. */
typedef uint64_t iommu_gaddr_t;

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
	TAILQ_ENTRY(iommu_map_entry) dmamap_link; /* Link for dmamap entries */
	RB_ENTRY(iommu_map_entry) rb_entry;	 /* Links for domain entries */
	TAILQ_ENTRY(iommu_map_entry) unroll_link; /* Link for unroll after
						    dmamap_load failure */
	struct iommu_domain *domain;
	struct iommu_qi_genseq gseq;
};

#define	IOMMU_MAP_ENTRY_PLACE	0x0001	/* Fake entry */
#define	IOMMU_MAP_ENTRY_RMRR	0x0002	/* Permanent, not linked by
					   dmamap_link */
#define	IOMMU_MAP_ENTRY_MAP	0x0004	/* Busdma created, linked by
					   dmamap_link */
#define	IOMMU_MAP_ENTRY_UNMAPPED	0x0010	/* No backing pages */
#define	IOMMU_MAP_ENTRY_QI_NF	0x0020	/* qi task, do not free entry */
#define	IOMMU_MAP_ENTRY_READ	0x1000	/* Read permitted */
#define	IOMMU_MAP_ENTRY_WRITE	0x2000	/* Write permitted */
#define	IOMMU_MAP_ENTRY_SNOOP	0x4000	/* Snoop */
#define	IOMMU_MAP_ENTRY_TM	0x8000	/* Transient */

struct iommu_unit {
	struct mtx lock;
	int unit;

	int dma_enabled;

	/* Busdma delayed map load */
	struct task dmamap_load_task;
	TAILQ_HEAD(, bus_dmamap_iommu) delayed_maps;
	struct taskqueue *delayed_taskqueue;
};

/*
 * Locking annotations:
 * (u) - Protected by iommu unit lock
 * (d) - Protected by domain lock
 * (c) - Immutable after initialization
 */

struct iommu_domain {
	struct iommu_unit *iommu;	/* (c) */
	struct mtx lock;		/* (c) */
	struct task unload_task;	/* (c) */
	u_int entries_cnt;		/* (d) */
	struct iommu_map_entries_tailq unload_entries; /* (d) Entries to
							 unload */
	struct iommu_gas_entries_tree rb_root; /* (d) */
	iommu_gaddr_t end;		/* (c) Highest address + 1 in
					   the guest AS */
	struct iommu_map_entry *first_place, *last_place; /* (d) */
	u_int flags;			/* (u) */
};

struct iommu_ctx {
	struct iommu_domain *domain;	/* (c) */
	struct bus_dma_tag_iommu *tag;	/* (c) Root tag */
	u_long loads;			/* atomic updates, for stat only */
	u_long unloads;			/* same */
	u_int flags;			/* (u) */
};

/* struct iommu_ctx flags */
#define	IOMMU_CTX_FAULTED	0x0001	/* Fault was reported,
					   last_fault_rec is valid */
#define	IOMMU_CTX_DISABLED	0x0002	/* Device is disabled, the
					   ephemeral reference is kept
					   to prevent context destruction */

#define	DMAR_DOMAIN_GAS_INITED		0x0001
#define	DMAR_DOMAIN_PGTBL_INITED	0x0002
#define	DMAR_DOMAIN_IDMAP		0x0010	/* Domain uses identity
						   page table */
#define	DMAR_DOMAIN_RMRR		0x0020	/* Domain contains RMRR entry,
						   cannot be turned off */

/* Map flags */
#define	IOMMU_MF_CANWAIT	0x0001
#define	IOMMU_MF_CANSPLIT	0x0002
#define	IOMMU_MF_RMRR		0x0004

#define	DMAR_PGF_WAITOK		0x0001
#define	DMAR_PGF_ZERO		0x0002
#define	DMAR_PGF_ALLOC		0x0004
#define	DMAR_PGF_NOALLOC	0x0008
#define	DMAR_PGF_OBJL		0x0010

#define	IOMMU_LOCK(unit)		mtx_lock(&(unit)->lock)
#define	IOMMU_UNLOCK(unit)		mtx_unlock(&(unit)->lock)
#define	IOMMU_ASSERT_LOCKED(unit)	mtx_assert(&(unit)->lock, MA_OWNED)

#define	IOMMU_DOMAIN_LOCK(dom)		mtx_lock(&(dom)->lock)
#define	IOMMU_DOMAIN_UNLOCK(dom)	mtx_unlock(&(dom)->lock)
#define	IOMMU_DOMAIN_ASSERT_LOCKED(dom)	mtx_assert(&(dom)->lock, MA_OWNED)

static inline bool
iommu_test_boundary(iommu_gaddr_t start, iommu_gaddr_t size,
    iommu_gaddr_t boundary)
{

	if (boundary == 0)
		return (true);
	return (start + size <= ((start + boundary) & ~(boundary - 1)));
}

void iommu_free_ctx(struct iommu_ctx *ctx);
void iommu_free_ctx_locked(struct iommu_unit *iommu, struct iommu_ctx *ctx);
struct iommu_ctx *iommu_get_ctx(struct iommu_unit *, device_t dev,
    uint16_t rid, bool id_mapped, bool rmrr_init);
struct iommu_unit *iommu_find(device_t dev, bool verbose);
void iommu_domain_unload_entry(struct iommu_map_entry *entry, bool free);
void iommu_domain_unload(struct iommu_domain *domain,
    struct iommu_map_entries_tailq *entries, bool cansleep);

struct iommu_ctx *iommu_instantiate_ctx(struct iommu_unit *iommu,
    device_t dev, bool rmrr);
device_t iommu_get_requester(device_t dev, uint16_t *rid);
int iommu_init_busdma(struct iommu_unit *unit);
void iommu_fini_busdma(struct iommu_unit *unit);
struct iommu_map_entry *iommu_map_alloc_entry(struct iommu_domain *iodom,
    u_int flags);
void iommu_map_free_entry(struct iommu_domain *, struct iommu_map_entry *);
int iommu_map(struct iommu_domain *iodom,
    const struct bus_dma_tag_common *common, iommu_gaddr_t size, int offset,
    u_int eflags, u_int flags, vm_page_t *ma, struct iommu_map_entry **res);
int iommu_map_region(struct iommu_domain *domain,
    struct iommu_map_entry *entry, u_int eflags, u_int flags, vm_page_t *ma);

void iommu_gas_init_domain(struct iommu_domain *domain);
void iommu_gas_fini_domain(struct iommu_domain *domain);
struct iommu_map_entry *iommu_gas_alloc_entry(struct iommu_domain *domain,
    u_int flags);
void iommu_gas_free_entry(struct iommu_domain *domain,
    struct iommu_map_entry *entry);
void iommu_gas_free_space(struct iommu_domain *domain,
    struct iommu_map_entry *entry);
int iommu_gas_map(struct iommu_domain *domain,
    const struct bus_dma_tag_common *common, iommu_gaddr_t size, int offset,
    u_int eflags, u_int flags, vm_page_t *ma, struct iommu_map_entry **res);
void iommu_gas_free_region(struct iommu_domain *domain,
    struct iommu_map_entry *entry);
int iommu_gas_map_region(struct iommu_domain *domain,
    struct iommu_map_entry *entry, u_int eflags, u_int flags, vm_page_t *ma);
int iommu_gas_reserve_region(struct iommu_domain *domain, iommu_gaddr_t start,
    iommu_gaddr_t end);

SYSCTL_DECL(_hw_iommu);

#endif /* !_SYS_IOMMU_H_ */
