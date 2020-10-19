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
#include <sys/vmem.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_map.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>
#include <x86/include/busdma_impl.h>
#include <dev/iommu/busdma_iommu.h>
#include <x86/iommu/intel_reg.h>
#include <x86/iommu/intel_dmar.h>

static MALLOC_DEFINE(M_DMAR_CTX, "dmar_ctx", "Intel DMAR Context");
static MALLOC_DEFINE(M_DMAR_DOMAIN, "dmar_dom", "Intel DMAR Domain");

static void dmar_unref_domain_locked(struct dmar_unit *dmar,
    struct dmar_domain *domain);
static void dmar_domain_destroy(struct dmar_domain *domain);

static void
dmar_ensure_ctx_page(struct dmar_unit *dmar, int bus)
{
	struct sf_buf *sf;
	dmar_root_entry_t *re;
	vm_page_t ctxm;

	/*
	 * Allocated context page must be linked.
	 */
	ctxm = dmar_pgalloc(dmar->ctx_obj, 1 + bus, IOMMU_PGF_NOALLOC);
	if (ctxm != NULL)
		return;

	/*
	 * Page not present, allocate and link.  Note that other
	 * thread might execute this sequence in parallel.  This
	 * should be safe, because the context entries written by both
	 * threads are equal.
	 */
	TD_PREP_PINNED_ASSERT;
	ctxm = dmar_pgalloc(dmar->ctx_obj, 1 + bus, IOMMU_PGF_ZERO |
	    IOMMU_PGF_WAITOK);
	re = dmar_map_pgtbl(dmar->ctx_obj, 0, IOMMU_PGF_NOALLOC, &sf);
	re += bus;
	dmar_pte_store(&re->r1, DMAR_ROOT_R1_P | (DMAR_ROOT_R1_CTP_MASK &
	    VM_PAGE_TO_PHYS(ctxm)));
	dmar_flush_root_to_ram(dmar, re);
	dmar_unmap_pgtbl(sf);
	TD_PINNED_ASSERT;
}

static dmar_ctx_entry_t *
dmar_map_ctx_entry(struct dmar_ctx *ctx, struct sf_buf **sfp)
{
	struct dmar_unit *dmar;
	dmar_ctx_entry_t *ctxp;

	dmar = CTX2DMAR(ctx);

	ctxp = dmar_map_pgtbl(dmar->ctx_obj, 1 + PCI_RID2BUS(ctx->context.rid),
	    IOMMU_PGF_NOALLOC | IOMMU_PGF_WAITOK, sfp);
	ctxp += ctx->context.rid & 0xff;
	return (ctxp);
}

static void
device_tag_init(struct dmar_ctx *ctx, device_t dev)
{
	struct dmar_domain *domain;
	bus_addr_t maxaddr;

	domain = CTX2DOM(ctx);
	maxaddr = MIN(domain->iodom.end, BUS_SPACE_MAXADDR);
	ctx->context.tag->common.ref_count = 1; /* Prevent free */
	ctx->context.tag->common.impl = &bus_dma_iommu_impl;
	ctx->context.tag->common.boundary = 0;
	ctx->context.tag->common.lowaddr = maxaddr;
	ctx->context.tag->common.highaddr = maxaddr;
	ctx->context.tag->common.maxsize = maxaddr;
	ctx->context.tag->common.nsegments = BUS_SPACE_UNRESTRICTED;
	ctx->context.tag->common.maxsegsz = maxaddr;
	ctx->context.tag->ctx = CTX2IOCTX(ctx);
	ctx->context.tag->owner = dev;
}

static void
ctx_id_entry_init_one(dmar_ctx_entry_t *ctxp, struct dmar_domain *domain,
    vm_page_t ctx_root)
{
	/*
	 * For update due to move, the store is not atomic.  It is
	 * possible that DMAR read upper doubleword, while low
	 * doubleword is not yet updated.  The domain id is stored in
	 * the upper doubleword, while the table pointer in the lower.
	 *
	 * There is no good solution, for the same reason it is wrong
	 * to clear P bit in the ctx entry for update.
	 */
	dmar_pte_store1(&ctxp->ctx2, DMAR_CTX2_DID(domain->domain) |
	    domain->awlvl);
	if (ctx_root == NULL) {
		dmar_pte_store1(&ctxp->ctx1, DMAR_CTX1_T_PASS | DMAR_CTX1_P);
	} else {
		dmar_pte_store1(&ctxp->ctx1, DMAR_CTX1_T_UNTR |
		    (DMAR_CTX1_ASR_MASK & VM_PAGE_TO_PHYS(ctx_root)) |
		    DMAR_CTX1_P);
	}
}

static void
ctx_id_entry_init(struct dmar_ctx *ctx, dmar_ctx_entry_t *ctxp, bool move,
    int busno)
{
	struct dmar_unit *unit;
	struct dmar_domain *domain;
	vm_page_t ctx_root;
	int i;

	domain = CTX2DOM(ctx);
	unit = DOM2DMAR(domain);
	KASSERT(move || (ctxp->ctx1 == 0 && ctxp->ctx2 == 0),
	    ("dmar%d: initialized ctx entry %d:%d:%d 0x%jx 0x%jx",
	    unit->iommu.unit, busno, pci_get_slot(ctx->context.tag->owner),
	    pci_get_function(ctx->context.tag->owner),
	    ctxp->ctx1, ctxp->ctx2));

	if ((domain->iodom.flags & IOMMU_DOMAIN_IDMAP) != 0 &&
	    (unit->hw_ecap & DMAR_ECAP_PT) != 0) {
		KASSERT(domain->pgtbl_obj == NULL,
		    ("ctx %p non-null pgtbl_obj", ctx));
		ctx_root = NULL;
	} else {
		ctx_root = dmar_pgalloc(domain->pgtbl_obj, 0,
		    IOMMU_PGF_NOALLOC);
	}

	if (iommu_is_buswide_ctx(DMAR2IOMMU(unit), busno)) {
		MPASS(!move);
		for (i = 0; i <= PCI_BUSMAX; i++) {
			ctx_id_entry_init_one(&ctxp[i], domain, ctx_root);
		}
	} else {
		ctx_id_entry_init_one(ctxp, domain, ctx_root);
	}
	dmar_flush_ctx_to_ram(unit, ctxp);
}

static int
dmar_flush_for_ctx_entry(struct dmar_unit *dmar, bool force)
{
	int error;

	/*
	 * If dmar declares Caching Mode as Set, follow 11.5 "Caching
	 * Mode Consideration" and do the (global) invalidation of the
	 * negative TLB entries.
	 */
	if ((dmar->hw_cap & DMAR_CAP_CM) == 0 && !force)
		return (0);
	if (dmar->qi_enabled) {
		dmar_qi_invalidate_ctx_glob_locked(dmar);
		if ((dmar->hw_ecap & DMAR_ECAP_DI) != 0 || force)
			dmar_qi_invalidate_iotlb_glob_locked(dmar);
		return (0);
	}
	error = dmar_inv_ctx_glob(dmar);
	if (error == 0 && ((dmar->hw_ecap & DMAR_ECAP_DI) != 0 || force))
		error = dmar_inv_iotlb_glob(dmar);
	return (error);
}

static int
domain_init_rmrr(struct dmar_domain *domain, device_t dev, int bus,
    int slot, int func, int dev_domain, int dev_busno,
    const void *dev_path, int dev_path_len)
{
	struct iommu_map_entries_tailq rmrr_entries;
	struct iommu_map_entry *entry, *entry1;
	vm_page_t *ma;
	iommu_gaddr_t start, end;
	vm_pindex_t size, i;
	int error, error1;

	error = 0;
	TAILQ_INIT(&rmrr_entries);
	dmar_dev_parse_rmrr(domain, dev_domain, dev_busno, dev_path,
	    dev_path_len, &rmrr_entries);
	TAILQ_FOREACH_SAFE(entry, &rmrr_entries, unroll_link, entry1) {
		/*
		 * VT-d specification requires that the start of an
		 * RMRR entry is 4k-aligned.  Buggy BIOSes put
		 * anything into the start and end fields.  Truncate
		 * and round as neccesary.
		 *
		 * We also allow the overlapping RMRR entries, see
		 * iommu_gas_alloc_region().
		 */
		start = entry->start;
		end = entry->end;
		if (bootverbose)
			printf("dmar%d ctx pci%d:%d:%d RMRR [%#jx, %#jx]\n",
			    domain->iodom.iommu->unit, bus, slot, func,
			    (uintmax_t)start, (uintmax_t)end);
		entry->start = trunc_page(start);
		entry->end = round_page(end);
		if (entry->start == entry->end) {
			/* Workaround for some AMI (?) BIOSes */
			if (bootverbose) {
				if (dev != NULL)
					device_printf(dev, "");
				printf("pci%d:%d:%d ", bus, slot, func);
				printf("BIOS bug: dmar%d RMRR "
				    "region (%jx, %jx) corrected\n",
				    domain->iodom.iommu->unit, start, end);
			}
			entry->end += DMAR_PAGE_SIZE * 0x20;
		}
		size = OFF_TO_IDX(entry->end - entry->start);
		ma = malloc(sizeof(vm_page_t) * size, M_TEMP, M_WAITOK);
		for (i = 0; i < size; i++) {
			ma[i] = vm_page_getfake(entry->start + PAGE_SIZE * i,
			    VM_MEMATTR_DEFAULT);
		}
		error1 = iommu_gas_map_region(DOM2IODOM(domain), entry,
		    IOMMU_MAP_ENTRY_READ | IOMMU_MAP_ENTRY_WRITE,
		    IOMMU_MF_CANWAIT | IOMMU_MF_RMRR, ma);
		/*
		 * Non-failed RMRR entries are owned by context rb
		 * tree.  Get rid of the failed entry, but do not stop
		 * the loop.  Rest of the parsed RMRR entries are
		 * loaded and removed on the context destruction.
		 */
		if (error1 == 0 && entry->end != entry->start) {
			IOMMU_LOCK(domain->iodom.iommu);
			domain->refs++; /* XXXKIB prevent free */
			domain->iodom.flags |= IOMMU_DOMAIN_RMRR;
			IOMMU_UNLOCK(domain->iodom.iommu);
		} else {
			if (error1 != 0) {
				if (dev != NULL)
					device_printf(dev, "");
				printf("pci%d:%d:%d ", bus, slot, func);
				printf(
			    "dmar%d failed to map RMRR region (%jx, %jx) %d\n",
				    domain->iodom.iommu->unit, start, end,
				    error1);
				error = error1;
			}
			TAILQ_REMOVE(&rmrr_entries, entry, unroll_link);
			iommu_gas_free_entry(DOM2IODOM(domain), entry);
		}
		for (i = 0; i < size; i++)
			vm_page_putfake(ma[i]);
		free(ma, M_TEMP);
	}
	return (error);
}

static struct dmar_domain *
dmar_domain_alloc(struct dmar_unit *dmar, bool id_mapped)
{
	struct iommu_domain *iodom;
	struct iommu_unit *unit;
	struct dmar_domain *domain;
	int error, id, mgaw;

	id = alloc_unr(dmar->domids);
	if (id == -1)
		return (NULL);
	domain = malloc(sizeof(*domain), M_DMAR_DOMAIN, M_WAITOK | M_ZERO);
	iodom = DOM2IODOM(domain);
	unit = DMAR2IOMMU(dmar);
	domain->domain = id;
	LIST_INIT(&domain->contexts);
	iommu_domain_init(unit, iodom, &dmar_domain_map_ops);

	domain->dmar = dmar;

	/*
	 * For now, use the maximal usable physical address of the
	 * installed memory to calculate the mgaw on id_mapped domain.
	 * It is useful for the identity mapping, and less so for the
	 * virtualized bus address space.
	 */
	domain->iodom.end = id_mapped ? ptoa(Maxmem) : BUS_SPACE_MAXADDR;
	mgaw = dmar_maxaddr2mgaw(dmar, domain->iodom.end, !id_mapped);
	error = domain_set_agaw(domain, mgaw);
	if (error != 0)
		goto fail;
	if (!id_mapped)
		/* Use all supported address space for remapping. */
		domain->iodom.end = 1ULL << (domain->agaw - 1);

	iommu_gas_init_domain(DOM2IODOM(domain));

	if (id_mapped) {
		if ((dmar->hw_ecap & DMAR_ECAP_PT) == 0) {
			domain->pgtbl_obj = domain_get_idmap_pgtbl(domain,
			    domain->iodom.end);
		}
		domain->iodom.flags |= IOMMU_DOMAIN_IDMAP;
	} else {
		error = domain_alloc_pgtbl(domain);
		if (error != 0)
			goto fail;
		/* Disable local apic region access */
		error = iommu_gas_reserve_region(iodom, 0xfee00000,
		    0xfeefffff + 1, &iodom->msi_entry);
		if (error != 0)
			goto fail;
	}
	return (domain);

fail:
	dmar_domain_destroy(domain);
	return (NULL);
}

static struct dmar_ctx *
dmar_ctx_alloc(struct dmar_domain *domain, uint16_t rid)
{
	struct dmar_ctx *ctx;

	ctx = malloc(sizeof(*ctx), M_DMAR_CTX, M_WAITOK | M_ZERO);
	ctx->context.domain = DOM2IODOM(domain);
	ctx->context.tag = malloc(sizeof(struct bus_dma_tag_iommu),
	    M_DMAR_CTX, M_WAITOK | M_ZERO);
	ctx->context.rid = rid;
	ctx->refs = 1;
	return (ctx);
}

static void
dmar_ctx_link(struct dmar_ctx *ctx)
{
	struct dmar_domain *domain;

	domain = CTX2DOM(ctx);
	IOMMU_ASSERT_LOCKED(domain->iodom.iommu);
	KASSERT(domain->refs >= domain->ctx_cnt,
	    ("dom %p ref underflow %d %d", domain, domain->refs,
	    domain->ctx_cnt));
	domain->refs++;
	domain->ctx_cnt++;
	LIST_INSERT_HEAD(&domain->contexts, ctx, link);
}

static void
dmar_ctx_unlink(struct dmar_ctx *ctx)
{
	struct dmar_domain *domain;

	domain = CTX2DOM(ctx);
	IOMMU_ASSERT_LOCKED(domain->iodom.iommu);
	KASSERT(domain->refs > 0,
	    ("domain %p ctx dtr refs %d", domain, domain->refs));
	KASSERT(domain->ctx_cnt >= domain->refs,
	    ("domain %p ctx dtr refs %d ctx_cnt %d", domain,
	    domain->refs, domain->ctx_cnt));
	domain->refs--;
	domain->ctx_cnt--;
	LIST_REMOVE(ctx, link);
}

static void
dmar_domain_destroy(struct dmar_domain *domain)
{
	struct iommu_domain *iodom;
	struct dmar_unit *dmar;

	iodom = DOM2IODOM(domain);

	KASSERT(TAILQ_EMPTY(&domain->iodom.unload_entries),
	    ("unfinished unloads %p", domain));
	KASSERT(LIST_EMPTY(&domain->contexts),
	    ("destroying dom %p with contexts", domain));
	KASSERT(domain->ctx_cnt == 0,
	    ("destroying dom %p with ctx_cnt %d", domain, domain->ctx_cnt));
	KASSERT(domain->refs == 0,
	    ("destroying dom %p with refs %d", domain, domain->refs));
	if ((domain->iodom.flags & IOMMU_DOMAIN_GAS_INITED) != 0) {
		DMAR_DOMAIN_LOCK(domain);
		iommu_gas_fini_domain(iodom);
		DMAR_DOMAIN_UNLOCK(domain);
	}
	if ((domain->iodom.flags & IOMMU_DOMAIN_PGTBL_INITED) != 0) {
		if (domain->pgtbl_obj != NULL)
			DMAR_DOMAIN_PGLOCK(domain);
		domain_free_pgtbl(domain);
	}
	iommu_domain_fini(iodom);
	dmar = DOM2DMAR(domain);
	free_unr(dmar->domids, domain->domain);
	free(domain, M_DMAR_DOMAIN);
}

static struct dmar_ctx *
dmar_get_ctx_for_dev1(struct dmar_unit *dmar, device_t dev, uint16_t rid,
    int dev_domain, int dev_busno, const void *dev_path, int dev_path_len,
    bool id_mapped, bool rmrr_init)
{
	struct dmar_domain *domain, *domain1;
	struct dmar_ctx *ctx, *ctx1;
	struct iommu_unit *unit;
	dmar_ctx_entry_t *ctxp;
	struct sf_buf *sf;
	int bus, slot, func, error;
	bool enable;

	if (dev != NULL) {
		bus = pci_get_bus(dev);
		slot = pci_get_slot(dev);
		func = pci_get_function(dev);
	} else {
		bus = PCI_RID2BUS(rid);
		slot = PCI_RID2SLOT(rid);
		func = PCI_RID2FUNC(rid);
	}
	enable = false;
	TD_PREP_PINNED_ASSERT;
	unit = DMAR2IOMMU(dmar);
	DMAR_LOCK(dmar);
	KASSERT(!iommu_is_buswide_ctx(unit, bus) || (slot == 0 && func == 0),
	    ("iommu%d pci%d:%d:%d get_ctx for buswide", dmar->iommu.unit, bus,
	    slot, func));
	ctx = dmar_find_ctx_locked(dmar, rid);
	error = 0;
	if (ctx == NULL) {
		/*
		 * Perform the allocations which require sleep or have
		 * higher chance to succeed if the sleep is allowed.
		 */
		DMAR_UNLOCK(dmar);
		dmar_ensure_ctx_page(dmar, PCI_RID2BUS(rid));
		domain1 = dmar_domain_alloc(dmar, id_mapped);
		if (domain1 == NULL) {
			TD_PINNED_ASSERT;
			return (NULL);
		}
		if (!id_mapped) {
			error = domain_init_rmrr(domain1, dev, bus,
			    slot, func, dev_domain, dev_busno, dev_path,
			    dev_path_len);
			if (error != 0) {
				dmar_domain_destroy(domain1);
				TD_PINNED_ASSERT;
				return (NULL);
			}
		}
		ctx1 = dmar_ctx_alloc(domain1, rid);
		ctxp = dmar_map_ctx_entry(ctx1, &sf);
		DMAR_LOCK(dmar);

		/*
		 * Recheck the contexts, other thread might have
		 * already allocated needed one.
		 */
		ctx = dmar_find_ctx_locked(dmar, rid);
		if (ctx == NULL) {
			domain = domain1;
			ctx = ctx1;
			dmar_ctx_link(ctx);
			ctx->context.tag->owner = dev;
			device_tag_init(ctx, dev);

			/*
			 * This is the first activated context for the
			 * DMAR unit.  Enable the translation after
			 * everything is set up.
			 */
			if (LIST_EMPTY(&dmar->domains))
				enable = true;
			LIST_INSERT_HEAD(&dmar->domains, domain, link);
			ctx_id_entry_init(ctx, ctxp, false, bus);
			if (dev != NULL) {
				device_printf(dev,
			    "dmar%d pci%d:%d:%d:%d rid %x domain %d mgaw %d "
				    "agaw %d %s-mapped\n",
				    dmar->iommu.unit, dmar->segment, bus, slot,
				    func, rid, domain->domain, domain->mgaw,
				    domain->agaw, id_mapped ? "id" : "re");
			}
			dmar_unmap_pgtbl(sf);
		} else {
			dmar_unmap_pgtbl(sf);
			dmar_domain_destroy(domain1);
			/* Nothing needs to be done to destroy ctx1. */
			free(ctx1, M_DMAR_CTX);
			domain = CTX2DOM(ctx);
			ctx->refs++; /* tag referenced us */
		}
	} else {
		domain = CTX2DOM(ctx);
		if (ctx->context.tag->owner == NULL)
			ctx->context.tag->owner = dev;
		ctx->refs++; /* tag referenced us */
	}

	error = dmar_flush_for_ctx_entry(dmar, enable);
	if (error != 0) {
		dmar_free_ctx_locked(dmar, ctx);
		TD_PINNED_ASSERT;
		return (NULL);
	}

	/*
	 * The dmar lock was potentially dropped between check for the
	 * empty context list and now.  Recheck the state of GCMD_TE
	 * to avoid unneeded command.
	 */
	if (enable && !rmrr_init && (dmar->hw_gcmd & DMAR_GCMD_TE) == 0) {
		error = dmar_enable_translation(dmar);
		if (error == 0) {
			if (bootverbose) {
				printf("dmar%d: enabled translation\n",
				    dmar->iommu.unit);
			}
		} else {
			printf("dmar%d: enabling translation failed, "
			    "error %d\n", dmar->iommu.unit, error);
			dmar_free_ctx_locked(dmar, ctx);
			TD_PINNED_ASSERT;
			return (NULL);
		}
	}
	DMAR_UNLOCK(dmar);
	TD_PINNED_ASSERT;
	return (ctx);
}

struct dmar_ctx *
dmar_get_ctx_for_dev(struct dmar_unit *dmar, device_t dev, uint16_t rid,
    bool id_mapped, bool rmrr_init)
{
	int dev_domain, dev_path_len, dev_busno;

	dev_domain = pci_get_domain(dev);
	dev_path_len = dmar_dev_depth(dev);
	ACPI_DMAR_PCI_PATH dev_path[dev_path_len];
	dmar_dev_path(dev, &dev_busno, dev_path, dev_path_len);
	return (dmar_get_ctx_for_dev1(dmar, dev, rid, dev_domain, dev_busno,
	    dev_path, dev_path_len, id_mapped, rmrr_init));
}

struct dmar_ctx *
dmar_get_ctx_for_devpath(struct dmar_unit *dmar, uint16_t rid,
    int dev_domain, int dev_busno,
    const void *dev_path, int dev_path_len,
    bool id_mapped, bool rmrr_init)
{

	return (dmar_get_ctx_for_dev1(dmar, NULL, rid, dev_domain, dev_busno,
	    dev_path, dev_path_len, id_mapped, rmrr_init));
}

int
dmar_move_ctx_to_domain(struct dmar_domain *domain, struct dmar_ctx *ctx)
{
	struct dmar_unit *dmar;
	struct dmar_domain *old_domain;
	dmar_ctx_entry_t *ctxp;
	struct sf_buf *sf;
	int error;

	dmar = domain->dmar;
	old_domain = CTX2DOM(ctx);
	if (domain == old_domain)
		return (0);
	KASSERT(old_domain->iodom.iommu == domain->iodom.iommu,
	    ("domain %p %u moving between dmars %u %u", domain,
	    domain->domain, old_domain->iodom.iommu->unit,
	    domain->iodom.iommu->unit));
	TD_PREP_PINNED_ASSERT;

	ctxp = dmar_map_ctx_entry(ctx, &sf);
	DMAR_LOCK(dmar);
	dmar_ctx_unlink(ctx);
	ctx->context.domain = &domain->iodom;
	dmar_ctx_link(ctx);
	ctx_id_entry_init(ctx, ctxp, true, PCI_BUSMAX + 100);
	dmar_unmap_pgtbl(sf);
	error = dmar_flush_for_ctx_entry(dmar, true);
	/* If flush failed, rolling back would not work as well. */
	printf("dmar%d rid %x domain %d->%d %s-mapped\n",
	    dmar->iommu.unit, ctx->context.rid, old_domain->domain,
	    domain->domain, (domain->iodom.flags & IOMMU_DOMAIN_IDMAP) != 0 ?
	    "id" : "re");
	dmar_unref_domain_locked(dmar, old_domain);
	TD_PINNED_ASSERT;
	return (error);
}

static void
dmar_unref_domain_locked(struct dmar_unit *dmar, struct dmar_domain *domain)
{

	DMAR_ASSERT_LOCKED(dmar);
	KASSERT(domain->refs >= 1,
	    ("dmar %d domain %p refs %u", dmar->iommu.unit, domain,
	    domain->refs));
	KASSERT(domain->refs > domain->ctx_cnt,
	    ("dmar %d domain %p refs %d ctx_cnt %d", dmar->iommu.unit, domain,
	    domain->refs, domain->ctx_cnt));

	if (domain->refs > 1) {
		domain->refs--;
		DMAR_UNLOCK(dmar);
		return;
	}

	KASSERT((domain->iodom.flags & IOMMU_DOMAIN_RMRR) == 0,
	    ("lost ref on RMRR domain %p", domain));

	LIST_REMOVE(domain, link);
	DMAR_UNLOCK(dmar);

	taskqueue_drain(dmar->iommu.delayed_taskqueue,
	    &domain->iodom.unload_task);
	dmar_domain_destroy(domain);
}

void
dmar_free_ctx_locked(struct dmar_unit *dmar, struct dmar_ctx *ctx)
{
	struct sf_buf *sf;
	dmar_ctx_entry_t *ctxp;
	struct dmar_domain *domain;

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

	KASSERT((ctx->context.flags & IOMMU_CTX_DISABLED) == 0,
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
		dmar_unmap_pgtbl(sf);
		TD_PINNED_ASSERT;
		return;
	}

	KASSERT((ctx->context.flags & IOMMU_CTX_DISABLED) == 0,
	    ("lost ref on disabled ctx %p", ctx));

	/*
	 * Clear the context pointer and flush the caches.
	 * XXXKIB: cannot do this if any RMRR entries are still present.
	 */
	dmar_pte_clear(&ctxp->ctx1);
	ctxp->ctx2 = 0;
	dmar_flush_ctx_to_ram(dmar, ctxp);
	dmar_inv_ctx_glob(dmar);
	if ((dmar->hw_ecap & DMAR_ECAP_DI) != 0) {
		if (dmar->qi_enabled)
			dmar_qi_invalidate_iotlb_glob_locked(dmar);
		else
			dmar_inv_iotlb_glob(dmar);
	}
	dmar_unmap_pgtbl(sf);
	domain = CTX2DOM(ctx);
	dmar_ctx_unlink(ctx);
	free(ctx->context.tag, M_DMAR_CTX);
	free(ctx, M_DMAR_CTX);
	dmar_unref_domain_locked(dmar, domain);
	TD_PINNED_ASSERT;
}

void
dmar_free_ctx(struct dmar_ctx *ctx)
{
	struct dmar_unit *dmar;

	dmar = CTX2DMAR(ctx);
	DMAR_LOCK(dmar);
	dmar_free_ctx_locked(dmar, ctx);
}

/*
 * Returns with the domain locked.
 */
struct dmar_ctx *
dmar_find_ctx_locked(struct dmar_unit *dmar, uint16_t rid)
{
	struct dmar_domain *domain;
	struct dmar_ctx *ctx;

	DMAR_ASSERT_LOCKED(dmar);

	LIST_FOREACH(domain, &dmar->domains, link) {
		LIST_FOREACH(ctx, &domain->contexts, link) {
			if (ctx->context.rid == rid)
				return (ctx);
		}
	}
	return (NULL);
}

void
dmar_domain_free_entry(struct iommu_map_entry *entry, bool free)
{
	struct iommu_domain *domain;

	domain = entry->domain;
	IOMMU_DOMAIN_LOCK(domain);
	if ((entry->flags & IOMMU_MAP_ENTRY_RMRR) != 0)
		iommu_gas_free_region(domain, entry);
	else
		iommu_gas_free_space(domain, entry);
	IOMMU_DOMAIN_UNLOCK(domain);
	if (free)
		iommu_gas_free_entry(domain, entry);
	else
		entry->flags = 0;
}

void
dmar_domain_unload_entry(struct iommu_map_entry *entry, bool free)
{
	struct dmar_domain *domain;
	struct dmar_unit *unit;

	domain = IODOM2DOM(entry->domain);
	unit = DOM2DMAR(domain);
	if (unit->qi_enabled) {
		DMAR_LOCK(unit);
		dmar_qi_invalidate_locked(IODOM2DOM(entry->domain),
		    entry->start, entry->end - entry->start, &entry->gseq,
		    true);
		if (!free)
			entry->flags |= IOMMU_MAP_ENTRY_QI_NF;
		TAILQ_INSERT_TAIL(&unit->tlb_flush_entries, entry, dmamap_link);
		DMAR_UNLOCK(unit);
	} else {
		domain_flush_iotlb_sync(IODOM2DOM(entry->domain),
		    entry->start, entry->end - entry->start);
		dmar_domain_free_entry(entry, free);
	}
}

static bool
dmar_domain_unload_emit_wait(struct dmar_domain *domain,
    struct iommu_map_entry *entry)
{

	if (TAILQ_NEXT(entry, dmamap_link) == NULL)
		return (true);
	return (domain->batch_no++ % dmar_batch_coalesce == 0);
}

void
dmar_domain_unload(struct dmar_domain *domain,
    struct iommu_map_entries_tailq *entries, bool cansleep)
{
	struct dmar_unit *unit;
	struct iommu_domain *iodom;
	struct iommu_map_entry *entry, *entry1;
	int error;

	iodom = DOM2IODOM(domain);
	unit = DOM2DMAR(domain);

	TAILQ_FOREACH_SAFE(entry, entries, dmamap_link, entry1) {
		KASSERT((entry->flags & IOMMU_MAP_ENTRY_MAP) != 0,
		    ("not mapped entry %p %p", domain, entry));
		error = iodom->ops->unmap(iodom, entry->start, entry->end -
		    entry->start, cansleep ? IOMMU_PGF_WAITOK : 0);
		KASSERT(error == 0, ("unmap %p error %d", domain, error));
		if (!unit->qi_enabled) {
			domain_flush_iotlb_sync(domain, entry->start,
			    entry->end - entry->start);
			TAILQ_REMOVE(entries, entry, dmamap_link);
			dmar_domain_free_entry(entry, true);
		}
	}
	if (TAILQ_EMPTY(entries))
		return;

	KASSERT(unit->qi_enabled, ("loaded entry left"));
	DMAR_LOCK(unit);
	TAILQ_FOREACH(entry, entries, dmamap_link) {
		dmar_qi_invalidate_locked(domain, entry->start, entry->end -
		    entry->start, &entry->gseq,
		    dmar_domain_unload_emit_wait(domain, entry));
	}
	TAILQ_CONCAT(&unit->tlb_flush_entries, entries, dmamap_link);
	DMAR_UNLOCK(unit);
}

struct iommu_ctx *
iommu_get_ctx(struct iommu_unit *iommu, device_t dev, uint16_t rid,
    bool id_mapped, bool rmrr_init)
{
	struct dmar_unit *dmar;
	struct dmar_ctx *ret;

	dmar = IOMMU2DMAR(iommu);

	ret = dmar_get_ctx_for_dev(dmar, dev, rid, id_mapped, rmrr_init);

	return (CTX2IOCTX(ret));
}

void
iommu_free_ctx_locked(struct iommu_unit *iommu, struct iommu_ctx *context)
{
	struct dmar_unit *dmar;
	struct dmar_ctx *ctx;

	dmar = IOMMU2DMAR(iommu);
	ctx = IOCTX2CTX(context);

	dmar_free_ctx_locked(dmar, ctx);
}

void
iommu_free_ctx(struct iommu_ctx *context)
{
	struct dmar_ctx *ctx;

	ctx = IOCTX2CTX(context);

	dmar_free_ctx(ctx);
}

void
iommu_domain_unload_entry(struct iommu_map_entry *entry, bool free)
{

	dmar_domain_unload_entry(entry, free);
}

void
iommu_domain_unload(struct iommu_domain *iodom,
    struct iommu_map_entries_tailq *entries, bool cansleep)
{
	struct dmar_domain *domain;

	domain = IODOM2DOM(iodom);

	dmar_domain_unload(domain, entries, cansleep);
}
