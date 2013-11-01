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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/memdesc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/tree.h>
#include <sys/uio.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_map.h>
#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>
#include <x86/include/busdma_impl.h>
#include <x86/iommu/intel_reg.h>
#include <x86/iommu/busdma_dmar.h>
#include <x86/iommu/intel_dmar.h>
#include <dev/pci/pcivar.h>

static MALLOC_DEFINE(M_DMAR_CTX, "dmar_ctx", "Intel DMAR Context");

static void dmar_ctx_unload_task(void *arg, int pending);

static void
dmar_ensure_ctx_page(struct dmar_unit *dmar, int bus)
{
	struct sf_buf *sf;
	dmar_root_entry_t *re;
	vm_page_t ctxm;

	/*
	 * Allocated context page must be linked.
	 */
	ctxm = dmar_pgalloc(dmar->ctx_obj, 1 + bus, DMAR_PGF_NOALLOC);
	if (ctxm != NULL)
		return;

	/*
	 * Page not present, allocate and link.  Note that other
	 * thread might execute this sequence in parallel.  This
	 * should be safe, because the context entries written by both
	 * threads are equal.
	 */
	TD_PREP_PINNED_ASSERT;
	ctxm = dmar_pgalloc(dmar->ctx_obj, 1 + bus, DMAR_PGF_ZERO |
	    DMAR_PGF_WAITOK);
	re = dmar_map_pgtbl(dmar->ctx_obj, 0, DMAR_PGF_NOALLOC, &sf);
	re += bus;
	dmar_pte_store(&re->r1, DMAR_ROOT_R1_P | (DMAR_ROOT_R1_CTP_MASK &
	    VM_PAGE_TO_PHYS(ctxm)));
	dmar_unmap_pgtbl(sf, DMAR_IS_COHERENT(dmar));
	TD_PINNED_ASSERT;
}

static dmar_ctx_entry_t *
dmar_map_ctx_entry(struct dmar_ctx *ctx, struct sf_buf **sfp)
{
	dmar_ctx_entry_t *ctxp;

	ctxp = dmar_map_pgtbl(ctx->dmar->ctx_obj, 1 + ctx->bus,
	    DMAR_PGF_NOALLOC | DMAR_PGF_WAITOK, sfp);
	ctxp += ((ctx->slot & 0x1f) << 3) + (ctx->func & 0x7);
	return (ctxp);
}

static void
ctx_tag_init(struct dmar_ctx *ctx)
{
	bus_addr_t maxaddr;

	maxaddr = MIN(ctx->end, BUS_SPACE_MAXADDR);
	ctx->ctx_tag.common.ref_count = 1; /* Prevent free */
	ctx->ctx_tag.common.impl = &bus_dma_dmar_impl;
	ctx->ctx_tag.common.boundary = PCI_DMA_BOUNDARY;
	ctx->ctx_tag.common.lowaddr = maxaddr;
	ctx->ctx_tag.common.highaddr = maxaddr;
	ctx->ctx_tag.common.maxsize = maxaddr;
	ctx->ctx_tag.common.nsegments = BUS_SPACE_UNRESTRICTED;
	ctx->ctx_tag.common.maxsegsz = maxaddr;
	ctx->ctx_tag.ctx = ctx;
	/* XXXKIB initialize tag further */
}

static void
ctx_id_entry_init(struct dmar_ctx *ctx, dmar_ctx_entry_t *ctxp)
{
	struct dmar_unit *unit;
	vm_page_t ctx_root;

	unit = ctx->dmar;
	KASSERT(ctxp->ctx1 == 0 && ctxp->ctx2 == 0,
	    ("dmar%d: initialized ctx entry %d:%d:%d 0x%jx 0x%jx",
	    unit->unit, ctx->bus, ctx->slot, ctx->func, ctxp->ctx1,
	    ctxp->ctx2));
	ctxp->ctx2 = DMAR_CTX2_DID(ctx->domain);
	ctxp->ctx2 |= ctx->awlvl;
	if ((ctx->flags & DMAR_CTX_IDMAP) != 0 &&
	    (unit->hw_ecap & DMAR_ECAP_PT) != 0) {
		KASSERT(ctx->pgtbl_obj == NULL,
		    ("ctx %p non-null pgtbl_obj", ctx));
		dmar_pte_store(&ctxp->ctx1, DMAR_CTX1_T_PASS | DMAR_CTX1_P);
	} else {
		ctx_root = dmar_pgalloc(ctx->pgtbl_obj, 0, DMAR_PGF_NOALLOC);
		dmar_pte_store(&ctxp->ctx1, DMAR_CTX1_T_UNTR |
		    (DMAR_CTX1_ASR_MASK & VM_PAGE_TO_PHYS(ctx_root)) |
		    DMAR_CTX1_P);
	}
}

static int
ctx_init_rmrr(struct dmar_ctx *ctx, device_t dev)
{
	struct dmar_map_entries_tailq rmrr_entries;
	struct dmar_map_entry *entry, *entry1;
	vm_page_t *ma;
	dmar_gaddr_t start, end;
	vm_pindex_t size, i;
	int error, error1;

	error = 0;
	TAILQ_INIT(&rmrr_entries);
	dmar_ctx_parse_rmrr(ctx, dev, &rmrr_entries);
	TAILQ_FOREACH_SAFE(entry, &rmrr_entries, unroll_link, entry1) {
		/*
		 * VT-d specification requires that the start of an
		 * RMRR entry is 4k-aligned.  Buggy BIOSes put
		 * anything into the start and end fields.  Truncate
		 * and round as neccesary.
		 *
		 * We also allow the overlapping RMRR entries, see
		 * dmar_gas_alloc_region().
		 */
		start = entry->start;
		end = entry->end;
		entry->start = trunc_page(start);
		entry->end = round_page(end);
		size = OFF_TO_IDX(entry->end - entry->start);
		ma = malloc(sizeof(vm_page_t) * size, M_TEMP, M_WAITOK);
		for (i = 0; i < size; i++) {
			ma[i] = vm_page_getfake(entry->start + PAGE_SIZE * i,
			    VM_MEMATTR_DEFAULT);
		}
		error1 = dmar_gas_map_region(ctx, entry, DMAR_MAP_ENTRY_READ |
		    DMAR_MAP_ENTRY_WRITE, DMAR_GM_CANWAIT, ma);
		/*
		 * Non-failed RMRR entries are owned by context rb
		 * tree.  Get rid of the failed entry, but do not stop
		 * the loop.  Rest of the parsed RMRR entries are
		 * loaded and removed on the context destruction.
		 */
		if (error1 == 0 && entry->end != entry->start) {
			DMAR_LOCK(ctx->dmar);
			ctx->flags |= DMAR_CTX_RMRR;
			DMAR_UNLOCK(ctx->dmar);
		} else {
			if (error1 != 0) {
				device_printf(dev,
			    "dmar%d failed to map RMRR region (%jx, %jx) %d\n",
				    ctx->dmar->unit, start, end, error1);
				error = error1;
			}
			TAILQ_REMOVE(&rmrr_entries, entry, unroll_link);
			dmar_gas_free_entry(ctx, entry);
		}
		for (i = 0; i < size; i++)
			vm_page_putfake(ma[i]);
		free(ma, M_TEMP);
	}
	return (error);
}

static struct dmar_ctx *
dmar_get_ctx_alloc(struct dmar_unit *dmar, int bus, int slot, int func)
{
	struct dmar_ctx *ctx;

	ctx = malloc(sizeof(*ctx), M_DMAR_CTX, M_WAITOK | M_ZERO);
	RB_INIT(&ctx->rb_root);
	TAILQ_INIT(&ctx->unload_entries);
	TASK_INIT(&ctx->unload_task, 0, dmar_ctx_unload_task, ctx);
	mtx_init(&ctx->lock, "dmarctx", NULL, MTX_DEF);
	ctx->dmar = dmar;
	ctx->bus = bus;
	ctx->slot = slot;
	ctx->func = func;
	return (ctx);
}

static void
dmar_ctx_dtr(struct dmar_ctx *ctx, bool gas_inited, bool pgtbl_inited)
{

	if (gas_inited) {
		DMAR_CTX_LOCK(ctx);
		dmar_gas_fini_ctx(ctx);
		DMAR_CTX_UNLOCK(ctx);
	}
	if (pgtbl_inited) {
		if (ctx->pgtbl_obj != NULL)
			DMAR_CTX_PGLOCK(ctx);
		ctx_free_pgtbl(ctx);
	}
	mtx_destroy(&ctx->lock);
	free(ctx, M_DMAR_CTX);
}

struct dmar_ctx *
dmar_get_ctx(struct dmar_unit *dmar, device_t dev, bool id_mapped, bool rmrr_init)
{
	struct dmar_ctx *ctx, *ctx1;
	dmar_ctx_entry_t *ctxp;
	struct sf_buf *sf;
	int bus, slot, func, error, mgaw;
	bool enable;

	bus = pci_get_bus(dev);
	slot = pci_get_slot(dev);
	func = pci_get_function(dev);
	enable = false;
	TD_PREP_PINNED_ASSERT;
	DMAR_LOCK(dmar);
	ctx = dmar_find_ctx_locked(dmar, bus, slot, func);
	error = 0;
	if (ctx == NULL) {
		/*
		 * Perform the allocations which require sleep or have
		 * higher chance to succeed if the sleep is allowed.
		 */
		DMAR_UNLOCK(dmar);
		dmar_ensure_ctx_page(dmar, bus);
		ctx1 = dmar_get_ctx_alloc(dmar, bus, slot, func);

		if (id_mapped) {
			/*
			 * For now, use the maximal usable physical
			 * address of the installed memory to
			 * calculate the mgaw.  It is useful for the
			 * identity mapping, and less so for the
			 * virtualized bus address space.
			 */
			ctx1->end = ptoa(Maxmem);
			mgaw = dmar_maxaddr2mgaw(dmar, ctx1->end, false);
			error = ctx_set_agaw(ctx1, mgaw);
			if (error != 0) {
				dmar_ctx_dtr(ctx1, false, false);
				TD_PINNED_ASSERT;
				return (NULL);
			}
		} else {
			ctx1->end = BUS_SPACE_MAXADDR;
			mgaw = dmar_maxaddr2mgaw(dmar, ctx1->end, true);
			error = ctx_set_agaw(ctx1, mgaw);
			if (error != 0) {
				dmar_ctx_dtr(ctx1, false, false);
				TD_PINNED_ASSERT;
				return (NULL);
			}
			/* Use all supported address space for remapping. */
			ctx1->end = 1ULL << (ctx1->agaw - 1);
		}


		dmar_gas_init_ctx(ctx1);
		if (id_mapped) {
			if ((dmar->hw_ecap & DMAR_ECAP_PT) == 0) {
				ctx1->pgtbl_obj = ctx_get_idmap_pgtbl(ctx1,
				    ctx1->end);
			}
			ctx1->flags |= DMAR_CTX_IDMAP;
		} else {
			error = ctx_alloc_pgtbl(ctx1);
			if (error != 0) {
				dmar_ctx_dtr(ctx1, true, false);
				TD_PINNED_ASSERT;
				return (NULL);
			}
			/* Disable local apic region access */
			error = dmar_gas_reserve_region(ctx1, 0xfee00000,
			    0xfeefffff + 1);
			if (error != 0) {
				dmar_ctx_dtr(ctx1, true, true);
				TD_PINNED_ASSERT;
				return (NULL);
			}
			error = ctx_init_rmrr(ctx1, dev);
			if (error != 0) {
				dmar_ctx_dtr(ctx1, true, true);
				TD_PINNED_ASSERT;
				return (NULL);
			}
		}
		ctxp = dmar_map_ctx_entry(ctx1, &sf);
		DMAR_LOCK(dmar);

		/*
		 * Recheck the contexts, other thread might have
		 * already allocated needed one.
		 */
		ctx = dmar_find_ctx_locked(dmar, bus, slot, func);
		if (ctx == NULL) {
			ctx = ctx1;
			ctx->domain = alloc_unrl(dmar->domids);
			if (ctx->domain == -1) {
				DMAR_UNLOCK(dmar);
				dmar_unmap_pgtbl(sf, true);
				dmar_ctx_dtr(ctx, true, true);
				TD_PINNED_ASSERT;
				return (NULL);
			}
			ctx_tag_init(ctx);

			/*
			 * This is the first activated context for the
			 * DMAR unit.  Enable the translation after
			 * everything is set up.
			 */
			if (LIST_EMPTY(&dmar->contexts))
				enable = true;
			LIST_INSERT_HEAD(&dmar->contexts, ctx, link);
			ctx_id_entry_init(ctx, ctxp);
			device_printf(dev,
			    "dmar%d pci%d:%d:%d:%d domain %d mgaw %d agaw %d\n",
			    dmar->unit, dmar->segment, bus, slot,
			    func, ctx->domain, ctx->mgaw, ctx->agaw);
		} else {
			dmar_ctx_dtr(ctx1, true, true);
		}
		dmar_unmap_pgtbl(sf, DMAR_IS_COHERENT(dmar));
	}
	ctx->refs++;
	if ((ctx->flags & DMAR_CTX_RMRR) != 0)
		ctx->refs++; /* XXXKIB */

	/*
	 * If dmar declares Caching Mode as Set, follow 11.5 "Caching
	 * Mode Consideration" and do the (global) invalidation of the
	 * negative TLB entries.
	 */
	if ((dmar->hw_cap & DMAR_CAP_CM) != 0 || enable) {
		if (dmar->qi_enabled) {
			dmar_qi_invalidate_ctx_glob_locked(dmar);
			if ((dmar->hw_ecap & DMAR_ECAP_DI) != 0)
				dmar_qi_invalidate_iotlb_glob_locked(dmar);
		} else {
			error = dmar_inv_ctx_glob(dmar);
			if (error == 0 &&
			    (dmar->hw_ecap & DMAR_ECAP_DI) != 0)
				error = dmar_inv_iotlb_glob(dmar);
			if (error != 0) {
				dmar_free_ctx_locked(dmar, ctx);
				TD_PINNED_ASSERT;
				return (NULL);
			}
		}
	}

	/*
	 * The dmar lock was potentially dropped between check for the
	 * empty context list and now.  Recheck the state of GCMD_TE
	 * to avoid unneeded command.
	 */
	if (enable && !rmrr_init && (dmar->hw_gcmd & DMAR_GCMD_TE) == 0) {
		error = dmar_enable_translation(dmar);
		if (error != 0) {
			dmar_free_ctx_locked(dmar, ctx);
			TD_PINNED_ASSERT;
			return (NULL);
		}
	}
	DMAR_UNLOCK(dmar);
	TD_PINNED_ASSERT;
	return (ctx);
}

void
dmar_free_ctx_locked(struct dmar_unit *dmar, struct dmar_ctx *ctx)
{
	struct sf_buf *sf;
	dmar_ctx_entry_t *ctxp;

	DMAR_ASSERT_LOCKED(dmar);
	KASSERT(ctx->refs >= 1,
	    ("dmar %p ctx %p refs %u", dmar, ctx, ctx->refs));

	/*
	 * If our reference is not last, only the dereference should
	 * be performed.
	 */
	if (ctx->refs > 1) {
		ctx->refs--;
		DMAR_UNLOCK(dmar);
		return;
	}

	KASSERT((ctx->flags & DMAR_CTX_RMRR) == 0,
	    ("lost ref on RMRR ctx %p", ctx));
	KASSERT((ctx->flags & DMAR_CTX_DISABLED) == 0,
	    ("lost ref on disabled ctx %p", ctx));

	/*
	 * Otherwise, the context entry must be cleared before the
	 * page table is destroyed.  The mapping of the context
	 * entries page could require sleep, unlock the dmar.
	 */
	DMAR_UNLOCK(dmar);
	TD_PREP_PINNED_ASSERT;
	ctxp = dmar_map_ctx_entry(ctx, &sf);
	DMAR_LOCK(dmar);
	KASSERT(ctx->refs >= 1,
	    ("dmar %p ctx %p refs %u", dmar, ctx, ctx->refs));

	/*
	 * Other thread might have referenced the context, in which
	 * case again only the dereference should be performed.
	 */
	if (ctx->refs > 1) {
		ctx->refs--;
		DMAR_UNLOCK(dmar);
		dmar_unmap_pgtbl(sf, DMAR_IS_COHERENT(dmar));
		TD_PINNED_ASSERT;
		return;
	}

	KASSERT((ctx->flags & DMAR_CTX_RMRR) == 0,
	    ("lost ref on RMRR ctx %p", ctx));
	KASSERT((ctx->flags & DMAR_CTX_DISABLED) == 0,
	    ("lost ref on disabled ctx %p", ctx));

	/*
	 * Clear the context pointer and flush the caches.
	 * XXXKIB: cannot do this if any RMRR entries are still present.
	 */
	dmar_pte_clear(&ctxp->ctx1);
	ctxp->ctx2 = 0;
	dmar_inv_ctx_glob(dmar);
	if ((dmar->hw_ecap & DMAR_ECAP_DI) != 0) {
		if (dmar->qi_enabled)
			dmar_qi_invalidate_iotlb_glob_locked(dmar);
		else
			dmar_inv_iotlb_glob(dmar);
	}
	LIST_REMOVE(ctx, link);
	DMAR_UNLOCK(dmar);

	/*
	 * The rest of the destruction is invisible for other users of
	 * the dmar unit.
	 */
	taskqueue_drain(dmar->delayed_taskqueue, &ctx->unload_task);
	KASSERT(TAILQ_EMPTY(&ctx->unload_entries),
	    ("unfinished unloads %p", ctx));
	dmar_unmap_pgtbl(sf, DMAR_IS_COHERENT(dmar));
	free_unr(dmar->domids, ctx->domain);
	dmar_ctx_dtr(ctx, true, true);
	TD_PINNED_ASSERT;
}

void
dmar_free_ctx(struct dmar_ctx *ctx)
{
	struct dmar_unit *dmar;

	dmar = ctx->dmar;
	DMAR_LOCK(dmar);
	dmar_free_ctx_locked(dmar, ctx);
}

struct dmar_ctx *
dmar_find_ctx_locked(struct dmar_unit *dmar, int bus, int slot, int func)
{
	struct dmar_ctx *ctx;

	DMAR_ASSERT_LOCKED(dmar);

	LIST_FOREACH(ctx, &dmar->contexts, link) {
		if (ctx->bus == bus && ctx->slot == slot && ctx->func == func)
			return (ctx);
	}
	return (NULL);
}

void
dmar_ctx_free_entry(struct dmar_map_entry *entry, bool free)
{
	struct dmar_ctx *ctx;

	ctx = entry->ctx;
	DMAR_CTX_LOCK(ctx);
	if ((entry->flags & DMAR_MAP_ENTRY_RMRR) != 0)
		dmar_gas_free_region(ctx, entry);
	else
		dmar_gas_free_space(ctx, entry);
	DMAR_CTX_UNLOCK(ctx);
	if (free)
		dmar_gas_free_entry(ctx, entry);
	else
		entry->flags = 0;
}

void
dmar_ctx_unload_entry(struct dmar_map_entry *entry, bool free)
{
	struct dmar_unit *unit;

	unit = entry->ctx->dmar;
	if (unit->qi_enabled) {
		DMAR_LOCK(unit);
		dmar_qi_invalidate_locked(entry->ctx, entry->start,
		    entry->end - entry->start, &entry->gseq);
		if (!free)
			entry->flags |= DMAR_MAP_ENTRY_QI_NF;
		TAILQ_INSERT_TAIL(&unit->tlb_flush_entries, entry, dmamap_link);
		DMAR_UNLOCK(unit);
	} else {
		ctx_flush_iotlb_sync(entry->ctx, entry->start, entry->end -
		    entry->start);
		dmar_ctx_free_entry(entry, free);
	}
}

void
dmar_ctx_unload(struct dmar_ctx *ctx, struct dmar_map_entries_tailq *entries,
    bool cansleep)
{
	struct dmar_unit *unit;
	struct dmar_map_entry *entry, *entry1;
	struct dmar_qi_genseq gseq;
	int error;

	unit = ctx->dmar;

	TAILQ_FOREACH_SAFE(entry, entries, dmamap_link, entry1) {
		KASSERT((entry->flags & DMAR_MAP_ENTRY_MAP) != 0,
		    ("not mapped entry %p %p", ctx, entry));
		error = ctx_unmap_buf(ctx, entry->start, entry->end -
		    entry->start, cansleep ? DMAR_PGF_WAITOK : 0);
		KASSERT(error == 0, ("unmap %p error %d", ctx, error));
		if (!unit->qi_enabled) {
			ctx_flush_iotlb_sync(ctx, entry->start,
			    entry->end - entry->start);
			TAILQ_REMOVE(entries, entry, dmamap_link);
			dmar_ctx_free_entry(entry, true);
		}
	}
	if (TAILQ_EMPTY(entries))
		return;

	KASSERT(unit->qi_enabled, ("loaded entry left"));
	DMAR_LOCK(unit);
	TAILQ_FOREACH(entry, entries, dmamap_link) {
		entry->gseq.gen = 0;
		entry->gseq.seq = 0;
		dmar_qi_invalidate_locked(ctx, entry->start, entry->end -
		    entry->start, TAILQ_NEXT(entry, dmamap_link) == NULL ?
		    &gseq : NULL);
	}
	TAILQ_FOREACH_SAFE(entry, entries, dmamap_link, entry1) {
		entry->gseq = gseq;
		TAILQ_REMOVE(entries, entry, dmamap_link);
		TAILQ_INSERT_TAIL(&unit->tlb_flush_entries, entry, dmamap_link);
	}
	DMAR_UNLOCK(unit);
}	

static void
dmar_ctx_unload_task(void *arg, int pending)
{
	struct dmar_ctx *ctx;
	struct dmar_map_entries_tailq entries;

	ctx = arg;
	TAILQ_INIT(&entries);

	for (;;) {
		DMAR_CTX_LOCK(ctx);
		TAILQ_SWAP(&ctx->unload_entries, &entries, dmar_map_entry,
		    dmamap_link);
		DMAR_CTX_UNLOCK(ctx);
		if (TAILQ_EMPTY(&entries))
			break;
		dmar_ctx_unload(ctx, &entries, true);
	}
}
