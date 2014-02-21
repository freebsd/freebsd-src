/*-
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

#ifndef __X86_IOMMU_INTEL_DMAR_H
#define	__X86_IOMMU_INTEL_DMAR_H

/* Host or physical memory address, after translation. */
typedef uint64_t dmar_haddr_t;
/* Guest or bus address, before translation. */
typedef uint64_t dmar_gaddr_t;

struct dmar_qi_genseq {
	u_int gen;
	uint32_t seq;
};

struct dmar_map_entry {
	dmar_gaddr_t start;
	dmar_gaddr_t end;
	dmar_gaddr_t free_after;	/* Free space after the entry */
	dmar_gaddr_t free_down;		/* Max free space below the
					   current R/B tree node */
	u_int flags;
	TAILQ_ENTRY(dmar_map_entry) dmamap_link; /* Link for dmamap entries */
	RB_ENTRY(dmar_map_entry) rb_entry;	 /* Links for ctx entries */
	TAILQ_ENTRY(dmar_map_entry) unroll_link; /* Link for unroll after
						    dmamap_load failure */
	struct dmar_ctx *ctx;
	struct dmar_qi_genseq gseq;
};

RB_HEAD(dmar_gas_entries_tree, dmar_map_entry);
RB_PROTOTYPE(dmar_gas_entries_tree, dmar_map_entry, rb_entry,
    dmar_gas_cmp_entries);

#define	DMAR_MAP_ENTRY_PLACE	0x0001	/* Fake entry */
#define	DMAR_MAP_ENTRY_RMRR	0x0002	/* Permanent, not linked by
					   dmamap_link */
#define	DMAR_MAP_ENTRY_MAP	0x0004	/* Busdma created, linked by
					   dmamap_link */
#define	DMAR_MAP_ENTRY_UNMAPPED	0x0010	/* No backing pages */
#define	DMAR_MAP_ENTRY_QI_NF	0x0020	/* qi task, do not free entry */
#define	DMAR_MAP_ENTRY_READ	0x1000	/* Read permitted */
#define	DMAR_MAP_ENTRY_WRITE	0x2000	/* Write permitted */
#define	DMAR_MAP_ENTRY_SNOOP	0x4000	/* Snoop */
#define	DMAR_MAP_ENTRY_TM	0x8000	/* Transient */

struct dmar_ctx {
	int bus;	/* pci bus/slot/func */
	int slot;
	int func;
	int domain;	/* DID */
	int mgaw;	/* Real max address width */
	int agaw;	/* Adjusted guest address width */
	int pglvl;	/* The pagelevel */
	int awlvl;	/* The pagelevel as the bitmask, to set in
			   context entry */
	dmar_gaddr_t end;/* Highest address + 1 in the guest AS */
	u_int refs;	/* References to the context, from tags */
	struct dmar_unit *dmar;
	struct bus_dma_tag_dmar ctx_tag; /* Root tag */
	struct mtx lock;
	LIST_ENTRY(dmar_ctx) link;	/* Member in the dmar list */
	vm_object_t pgtbl_obj;		/* Page table pages */
	u_int flags;			/* Protected by dmar lock */
	uint64_t last_fault_rec[2];	/* Last fault reported */
	u_int entries_cnt;
	u_long loads;
	u_long unloads;
	struct dmar_gas_entries_tree rb_root;
	struct dmar_map_entries_tailq unload_entries; /* Entries to unload */
	struct dmar_map_entry *first_place, *last_place;
	struct task unload_task;
};

/* struct dmar_ctx flags */
#define	DMAR_CTX_FAULTED	0x0001	/* Fault was reported,
					   last_fault_rec is valid */
#define	DMAR_CTX_IDMAP		0x0002	/* Context uses identity page table */
#define	DMAR_CTX_RMRR		0x0004	/* Context contains RMRR entry,
					   cannot be turned off */
#define	DMAR_CTX_DISABLED	0x0008	/* Device is disabled, the
					   ephemeral reference is kept
					   to prevent context destruction */

#define	DMAR_CTX_PGLOCK(ctx)	VM_OBJECT_WLOCK((ctx)->pgtbl_obj)
#define	DMAR_CTX_PGTRYLOCK(ctx)	VM_OBJECT_TRYWLOCK((ctx)->pgtbl_obj)
#define	DMAR_CTX_PGUNLOCK(ctx)	VM_OBJECT_WUNLOCK((ctx)->pgtbl_obj)
#define	DMAR_CTX_ASSERT_PGLOCKED(ctx) \
	VM_OBJECT_ASSERT_WLOCKED((ctx)->pgtbl_obj)

#define	DMAR_CTX_LOCK(ctx)	mtx_lock(&(ctx)->lock)
#define	DMAR_CTX_UNLOCK(ctx)	mtx_unlock(&(ctx)->lock)
#define	DMAR_CTX_ASSERT_LOCKED(ctx) mtx_assert(&(ctx)->lock, MA_OWNED)

struct dmar_msi_data {
	int irq;
	int irq_rid;
	struct resource *irq_res;
	void *intr_handle;
	int (*handler)(void *);
	int msi_data_reg;
	int msi_addr_reg;
	int msi_uaddr_reg;
	void (*enable_intr)(struct dmar_unit *);
	void (*disable_intr)(struct dmar_unit *);
	const char *name;
};

#define	DMAR_INTR_FAULT		0
#define	DMAR_INTR_QI		1
#define	DMAR_INTR_TOTAL		2

struct dmar_unit {
	device_t dev;
	int unit;
	uint16_t segment;
	uint64_t base;

	/* Resources */
	int reg_rid;
	struct resource *regs;

	struct dmar_msi_data intrs[DMAR_INTR_TOTAL];

	/* Hardware registers cache */
	uint32_t hw_ver;
	uint64_t hw_cap;
	uint64_t hw_ecap;
	uint32_t hw_gcmd;

	/* Data for being a dmar */
	struct mtx lock;
	LIST_HEAD(, dmar_ctx) contexts;
	struct unrhdr *domids;
	vm_object_t ctx_obj;
	u_int barrier_flags;

	/* Fault handler data */
	struct mtx fault_lock;
	uint64_t *fault_log;
	int fault_log_head;
	int fault_log_tail;
	int fault_log_size;
	struct task fault_task;
	struct taskqueue *fault_taskqueue;

	/* QI */
	int qi_enabled;
	vm_offset_t inv_queue;
	vm_size_t inv_queue_size;
	uint32_t inv_queue_avail;
	uint32_t inv_queue_tail;
	volatile uint32_t inv_waitd_seq_hw; /* hw writes there on wait
					       descr completion */
	uint64_t inv_waitd_seq_hw_phys;
	uint32_t inv_waitd_seq; /* next sequence number to use for wait descr */
	u_int inv_waitd_gen;	/* seq number generation AKA seq overflows */
	u_int inv_seq_waiters;	/* count of waiters for seq */
	u_int inv_queue_full;	/* informational counter */

	/* Delayed freeing of map entries queue processing */
	struct dmar_map_entries_tailq tlb_flush_entries;
	struct task qi_task;
	struct taskqueue *qi_taskqueue;

	/* Busdma delayed map load */
	struct task dmamap_load_task;
	TAILQ_HEAD(, bus_dmamap_dmar) delayed_maps;
	struct taskqueue *delayed_taskqueue;
};

#define	DMAR_LOCK(dmar)		mtx_lock(&(dmar)->lock)
#define	DMAR_UNLOCK(dmar)	mtx_unlock(&(dmar)->lock)
#define	DMAR_ASSERT_LOCKED(dmar) mtx_assert(&(dmar)->lock, MA_OWNED)

#define	DMAR_FAULT_LOCK(dmar)	mtx_lock_spin(&(dmar)->fault_lock)
#define	DMAR_FAULT_UNLOCK(dmar)	mtx_unlock_spin(&(dmar)->fault_lock)
#define	DMAR_FAULT_ASSERT_LOCKED(dmar) mtx_assert(&(dmar)->fault_lock, MA_OWNED)

#define	DMAR_IS_COHERENT(dmar)	(((dmar)->hw_ecap & DMAR_ECAP_C) != 0)
#define	DMAR_HAS_QI(dmar)	(((dmar)->hw_ecap & DMAR_ECAP_QI) != 0)

/* Barrier ids */
#define	DMAR_BARRIER_RMRR	0
#define	DMAR_BARRIER_USEQ	1

struct dmar_unit *dmar_find(device_t dev);

u_int dmar_nd2mask(u_int nd);
bool dmar_pglvl_supported(struct dmar_unit *unit, int pglvl);
int ctx_set_agaw(struct dmar_ctx *ctx, int mgaw);
int dmar_maxaddr2mgaw(struct dmar_unit* unit, dmar_gaddr_t maxaddr,
    bool allow_less);
vm_pindex_t pglvl_max_pages(int pglvl);
int ctx_is_sp_lvl(struct dmar_ctx *ctx, int lvl);
dmar_gaddr_t pglvl_page_size(int total_pglvl, int lvl);
dmar_gaddr_t ctx_page_size(struct dmar_ctx *ctx, int lvl);
int calc_am(struct dmar_unit *unit, dmar_gaddr_t base, dmar_gaddr_t size,
    dmar_gaddr_t *isizep);
struct vm_page *dmar_pgalloc(vm_object_t obj, vm_pindex_t idx, int flags);
void dmar_pgfree(vm_object_t obj, vm_pindex_t idx, int flags);
void *dmar_map_pgtbl(vm_object_t obj, vm_pindex_t idx, int flags,
    struct sf_buf **sf);
void dmar_unmap_pgtbl(struct sf_buf *sf, bool coherent);
int dmar_load_root_entry_ptr(struct dmar_unit *unit);
int dmar_inv_ctx_glob(struct dmar_unit *unit);
int dmar_inv_iotlb_glob(struct dmar_unit *unit);
int dmar_flush_write_bufs(struct dmar_unit *unit);
int dmar_enable_translation(struct dmar_unit *unit);
int dmar_disable_translation(struct dmar_unit *unit);
bool dmar_barrier_enter(struct dmar_unit *dmar, u_int barrier_id);
void dmar_barrier_exit(struct dmar_unit *dmar, u_int barrier_id);

int dmar_fault_intr(void *arg);
void dmar_enable_fault_intr(struct dmar_unit *unit);
void dmar_disable_fault_intr(struct dmar_unit *unit);
int dmar_init_fault_log(struct dmar_unit *unit);
void dmar_fini_fault_log(struct dmar_unit *unit);

int dmar_qi_intr(void *arg);
void dmar_enable_qi_intr(struct dmar_unit *unit);
void dmar_disable_qi_intr(struct dmar_unit *unit);
int dmar_init_qi(struct dmar_unit *unit);
void dmar_fini_qi(struct dmar_unit *unit);
void dmar_qi_invalidate_locked(struct dmar_ctx *ctx, dmar_gaddr_t start,
    dmar_gaddr_t size, struct dmar_qi_genseq *pseq);
void dmar_qi_invalidate_ctx_glob_locked(struct dmar_unit *unit);
void dmar_qi_invalidate_iotlb_glob_locked(struct dmar_unit *unit);

vm_object_t ctx_get_idmap_pgtbl(struct dmar_ctx *ctx, dmar_gaddr_t maxaddr);
void put_idmap_pgtbl(vm_object_t obj);
int ctx_map_buf(struct dmar_ctx *ctx, dmar_gaddr_t base, dmar_gaddr_t size,
    vm_page_t *ma, uint64_t pflags, int flags);
int ctx_unmap_buf(struct dmar_ctx *ctx, dmar_gaddr_t base, dmar_gaddr_t size,
    int flags);
void ctx_flush_iotlb_sync(struct dmar_ctx *ctx, dmar_gaddr_t base,
    dmar_gaddr_t size);
int ctx_alloc_pgtbl(struct dmar_ctx *ctx);
void ctx_free_pgtbl(struct dmar_ctx *ctx);

struct dmar_ctx *dmar_instantiate_ctx(struct dmar_unit *dmar, device_t dev,
    bool rmrr);
struct dmar_ctx *dmar_get_ctx(struct dmar_unit *dmar, device_t dev,
    bool id_mapped, bool rmrr_init);
void dmar_free_ctx_locked(struct dmar_unit *dmar, struct dmar_ctx *ctx);
void dmar_free_ctx(struct dmar_ctx *ctx);
struct dmar_ctx *dmar_find_ctx_locked(struct dmar_unit *dmar, int bus,
    int slot, int func);
void dmar_ctx_unload_entry(struct dmar_map_entry *entry, bool free);
void dmar_ctx_unload(struct dmar_ctx *ctx,
    struct dmar_map_entries_tailq *entries, bool cansleep);
void dmar_ctx_free_entry(struct dmar_map_entry *entry, bool free);

int dmar_init_busdma(struct dmar_unit *unit);
void dmar_fini_busdma(struct dmar_unit *unit);

void dmar_gas_init_ctx(struct dmar_ctx *ctx);
void dmar_gas_fini_ctx(struct dmar_ctx *ctx);
struct dmar_map_entry *dmar_gas_alloc_entry(struct dmar_ctx *ctx, u_int flags);
void dmar_gas_free_entry(struct dmar_ctx *ctx, struct dmar_map_entry *entry);
void dmar_gas_free_space(struct dmar_ctx *ctx, struct dmar_map_entry *entry);
int dmar_gas_map(struct dmar_ctx *ctx, const struct bus_dma_tag_common *common,
    dmar_gaddr_t size, u_int eflags, u_int flags, vm_page_t *ma,
    struct dmar_map_entry **res);
void dmar_gas_free_region(struct dmar_ctx *ctx, struct dmar_map_entry *entry);
int dmar_gas_map_region(struct dmar_ctx *ctx, struct dmar_map_entry *entry,
    u_int eflags, u_int flags, vm_page_t *ma);
int dmar_gas_reserve_region(struct dmar_ctx *ctx, dmar_gaddr_t start,
    dmar_gaddr_t end);

void dmar_ctx_parse_rmrr(struct dmar_ctx *ctx, device_t dev,
    struct dmar_map_entries_tailq *rmrr_entries);
int dmar_instantiate_rmrr_ctxs(struct dmar_unit *dmar);

void dmar_quirks_post_ident(struct dmar_unit *dmar);
void dmar_quirks_pre_use(struct dmar_unit *dmar);

#define	DMAR_GM_CANWAIT	0x0001
#define	DMAR_GM_CANSPLIT 0x0002

#define	DMAR_PGF_WAITOK	0x0001
#define	DMAR_PGF_ZERO	0x0002
#define	DMAR_PGF_ALLOC	0x0004
#define	DMAR_PGF_NOALLOC 0x0008
#define	DMAR_PGF_OBJL	0x0010

extern dmar_haddr_t dmar_high;
extern int haw;
extern int dmar_tbl_pagecnt;
extern int dmar_match_verbose;
extern int dmar_check_free;

static inline uint32_t
dmar_read4(const struct dmar_unit *unit, int reg)
{

	return (bus_read_4(unit->regs, reg));
}

static inline uint64_t
dmar_read8(const struct dmar_unit *unit, int reg)
{
#ifdef __i386__
	uint32_t high, low;

	low = bus_read_4(unit->regs, reg);
	high = bus_read_4(unit->regs, reg + 4);
	return (low | ((uint64_t)high << 32));
#else
	return (bus_read_8(unit->regs, reg));
#endif
}

static inline void
dmar_write4(const struct dmar_unit *unit, int reg, uint32_t val)
{

	KASSERT(reg != DMAR_GCMD_REG || (val & DMAR_GCMD_TE) ==
	    (unit->hw_gcmd & DMAR_GCMD_TE),
	    ("dmar%d clearing TE 0x%08x 0x%08x", unit->unit,
	    unit->hw_gcmd, val));
	bus_write_4(unit->regs, reg, val);
}

static inline void
dmar_write8(const struct dmar_unit *unit, int reg, uint64_t val)
{

	KASSERT(reg != DMAR_GCMD_REG, ("8byte GCMD write"));
#ifdef __i386__
	uint32_t high, low;

	low = val;
	high = val >> 32;
	bus_write_4(unit->regs, reg, low);
	bus_write_4(unit->regs, reg + 4, high);
#else
	bus_write_8(unit->regs, reg, val);
#endif
}

/*
 * dmar_pte_store and dmar_pte_clear ensure that on i386, 32bit writes
 * are issued in the correct order.  For store, the lower word,
 * containing the P or R and W bits, is set only after the high word
 * is written.  For clear, the P bit is cleared first, then the high
 * word is cleared.
 */
static inline void
dmar_pte_store(volatile uint64_t *dst, uint64_t val)
{

	KASSERT(*dst == 0, ("used pte %p oldval %jx newval %jx",
	    dst, (uintmax_t)*dst, (uintmax_t)val));
#ifdef __i386__
	volatile uint32_t *p;
	uint32_t hi, lo;

	hi = val >> 32;
	lo = val;
	p = (volatile uint32_t *)dst;
	*(p + 1) = hi;
	*p = lo;
#else
	*dst = val;
#endif
}

static inline void
dmar_pte_clear(volatile uint64_t *dst)
{
#ifdef __i386__
	volatile uint32_t *p;

	p = (volatile uint32_t *)dst;
	*p = 0;
	*(p + 1) = 0;
#else
	*dst = 0;
#endif
}

static inline bool
dmar_test_boundary(dmar_gaddr_t start, dmar_gaddr_t size,
    dmar_gaddr_t boundary)
{

	if (boundary == 0)
		return (true);
	return (start + size <= ((start + boundary) & ~(boundary - 1)));
}

#ifdef INVARIANTS
#define	TD_PREP_PINNED_ASSERT						\
	int old_td_pinned;						\
	old_td_pinned = curthread->td_pinned
#define	TD_PINNED_ASSERT						\
	KASSERT(curthread->td_pinned == old_td_pinned,			\
	    ("pin count leak: %d %d %s:%d", curthread->td_pinned,	\
	    old_td_pinned, __FILE__, __LINE__))
#else
#define	TD_PREP_PINNED_ASSERT
#define	TD_PINNED_ASSERT
#endif

#endif
