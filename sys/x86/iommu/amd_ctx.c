/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 The FreeBSD Foundation
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
#include <x86/iommu/amd_reg.h>
#include <x86/iommu/x86_iommu.h>
#include <x86/iommu/amd_iommu.h>

static MALLOC_DEFINE(M_AMDIOMMU_CTX, "amdiommu_ctx", "AMD IOMMU Context");
static MALLOC_DEFINE(M_AMDIOMMU_DOMAIN, "amdiommu_dom", "AMD IOMMU Domain");

static void amdiommu_unref_domain_locked(struct amdiommu_unit *unit,
    struct amdiommu_domain *domain);

static struct amdiommu_dte *
amdiommu_get_dtep(struct amdiommu_ctx *ctx)
{
	return (&CTX2AMD(ctx)->dev_tbl[ctx->context.rid]);
}

void
amdiommu_domain_unload_entry(struct iommu_map_entry *entry, bool free,
    bool cansleep)
{
	struct amdiommu_domain *domain;
	struct amdiommu_unit *unit;

	domain = IODOM2DOM(entry->domain);
	unit = DOM2AMD(domain);

	/*
	 * If "free" is false, then the IOTLB invalidation must be performed
	 * synchronously.  Otherwise, the caller might free the entry before
	 * dmar_qi_task() is finished processing it.
	 */
	if (free) {
		AMDIOMMU_LOCK(unit);
		iommu_qi_invalidate_locked(&domain->iodom, entry, true);
		AMDIOMMU_UNLOCK(unit);
	} else {
		iommu_qi_invalidate_sync(&domain->iodom, entry->start,
		    entry->end - entry->start, cansleep);
		iommu_domain_free_entry(entry, false);
	}
}

static bool
amdiommu_domain_unload_emit_wait(struct amdiommu_domain *domain,
    struct iommu_map_entry *entry)
{
	return (true); /* XXXKIB */
}

void
amdiommu_domain_unload(struct iommu_domain *iodom,
    struct iommu_map_entries_tailq *entries, bool cansleep)
{
	struct amdiommu_domain *domain;
	struct amdiommu_unit *unit;
	struct iommu_map_entry *entry, *entry1;
	int error __diagused;

	domain = IODOM2DOM(iodom);
	unit = DOM2AMD(domain);

	TAILQ_FOREACH_SAFE(entry, entries, dmamap_link, entry1) {
		KASSERT((entry->flags & IOMMU_MAP_ENTRY_MAP) != 0,
		    ("not mapped entry %p %p", domain, entry));
		error = iodom->ops->unmap(iodom, entry,
		    cansleep ? IOMMU_PGF_WAITOK : 0);
		KASSERT(error == 0, ("unmap %p error %d", domain, error));
	}
	if (TAILQ_EMPTY(entries))
		return;

	AMDIOMMU_LOCK(unit);
	while ((entry = TAILQ_FIRST(entries)) != NULL) {
		TAILQ_REMOVE(entries, entry, dmamap_link);
		iommu_qi_invalidate_locked(&domain->iodom, entry,
		    amdiommu_domain_unload_emit_wait(domain, entry));
	}
	AMDIOMMU_UNLOCK(unit);
}

static void
amdiommu_domain_destroy(struct amdiommu_domain *domain)
{
	struct iommu_domain *iodom;
	struct amdiommu_unit *unit;

	iodom = DOM2IODOM(domain);

	KASSERT(TAILQ_EMPTY(&domain->iodom.unload_entries),
	    ("unfinished unloads %p", domain));
	KASSERT(LIST_EMPTY(&iodom->contexts),
	    ("destroying dom %p with contexts", domain));
	KASSERT(domain->ctx_cnt == 0,
	    ("destroying dom %p with ctx_cnt %d", domain, domain->ctx_cnt));
	KASSERT(domain->refs == 0,
	    ("destroying dom %p with refs %d", domain, domain->refs));

	if ((domain->iodom.flags & IOMMU_DOMAIN_GAS_INITED) != 0) {
		AMDIOMMU_DOMAIN_LOCK(domain);
		iommu_gas_fini_domain(iodom);
		AMDIOMMU_DOMAIN_UNLOCK(domain);
	}
	if ((domain->iodom.flags & IOMMU_DOMAIN_PGTBL_INITED) != 0) {
		if (domain->pgtbl_obj != NULL)
			AMDIOMMU_DOMAIN_PGLOCK(domain);
		amdiommu_domain_free_pgtbl(domain);
	}
	iommu_domain_fini(iodom);
	unit = DOM2AMD(domain);
	free_unr(unit->domids, domain->domain);
	free(domain, M_AMDIOMMU_DOMAIN);
}

static iommu_gaddr_t
lvl2addr(int lvl)
{
	int x;

	x = IOMMU_PAGE_SHIFT + IOMMU_NPTEPGSHIFT * lvl;
	/* Level 6 has only 8 bits for page table index */
	if (x >= NBBY * sizeof(uint64_t))
		return (-1ull);
	return (1ull < (1ull << x));
}

static void
amdiommu_domain_init_pglvl(struct amdiommu_unit *unit,
    struct amdiommu_domain *domain)
{
	iommu_gaddr_t end;
	int hats, i;
	uint64_t efr_hats;

	end = DOM2IODOM(domain)->end;
	for (i = AMDIOMMU_PGTBL_MAXLVL; i > 1; i--) {
		if (lvl2addr(i) >= end && lvl2addr(i - 1) < end)
			break;
	}
	domain->pglvl = i;

	efr_hats = unit->efr & AMDIOMMU_EFR_HATS_MASK;
	switch (efr_hats) {
	case AMDIOMMU_EFR_HATS_6LVL:
		hats = 6;
		break;
	case AMDIOMMU_EFR_HATS_5LVL:
		hats = 5;
		break;
	case AMDIOMMU_EFR_HATS_4LVL:
		hats = 4;
		break;
	default:
		printf("amdiommu%d: HATS %#jx (reserved) ignoring\n",
		    unit->iommu.unit, (uintmax_t)efr_hats);
		return;
	}
	if (hats >= domain->pglvl)
		return;

	printf("amdiommu%d: domain %d HATS %d pglvl %d reducing to HATS\n",
	    unit->iommu.unit, domain->domain, hats, domain->pglvl);
	domain->pglvl = hats;
	domain->iodom.end = lvl2addr(hats);
}

static struct amdiommu_domain *
amdiommu_domain_alloc(struct amdiommu_unit *unit, bool id_mapped)
{
	struct amdiommu_domain *domain;
	struct iommu_domain *iodom;
	int error, id;

	id = alloc_unr(unit->domids);
	if (id == -1)
		return (NULL);
	domain = malloc(sizeof(*domain), M_AMDIOMMU_DOMAIN, M_WAITOK | M_ZERO);
	iodom = DOM2IODOM(domain);
	domain->domain = id;
	LIST_INIT(&iodom->contexts);
	iommu_domain_init(AMD2IOMMU(unit), iodom, &amdiommu_domain_map_ops);

	domain->unit = unit;

	domain->iodom.end = id_mapped ? ptoa(Maxmem) : BUS_SPACE_MAXADDR;
	amdiommu_domain_init_pglvl(unit, domain);
	iommu_gas_init_domain(DOM2IODOM(domain));

	if (id_mapped) {
		domain->iodom.flags |= IOMMU_DOMAIN_IDMAP;
	} else {
		error = amdiommu_domain_alloc_pgtbl(domain);
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
	amdiommu_domain_destroy(domain);
	return (NULL);
}

static struct amdiommu_ctx *
amdiommu_ctx_alloc(struct amdiommu_domain *domain, uint16_t rid)
{
	struct amdiommu_ctx *ctx;

	ctx = malloc(sizeof(*ctx), M_AMDIOMMU_CTX, M_WAITOK | M_ZERO);
	ctx->context.domain = DOM2IODOM(domain);
	ctx->context.tag = malloc(sizeof(struct bus_dma_tag_iommu),
	    M_AMDIOMMU_CTX, M_WAITOK | M_ZERO);
	ctx->context.rid = rid;
	ctx->context.refs = 1;
	return (ctx);
}

static void
amdiommu_ctx_link(struct amdiommu_ctx *ctx)
{
	struct amdiommu_domain *domain;

	domain = CTX2DOM(ctx);
	IOMMU_ASSERT_LOCKED(domain->iodom.iommu);
	KASSERT(domain->refs >= domain->ctx_cnt,
	    ("dom %p ref underflow %d %d", domain, domain->refs,
	    domain->ctx_cnt));
	domain->refs++;
	domain->ctx_cnt++;
	LIST_INSERT_HEAD(&domain->iodom.contexts, &ctx->context, link);
}

static void
amdiommu_ctx_unlink(struct amdiommu_ctx *ctx)
{
	struct amdiommu_domain *domain;

	domain = CTX2DOM(ctx);
	IOMMU_ASSERT_LOCKED(domain->iodom.iommu);
	KASSERT(domain->refs > 0,
	    ("domain %p ctx dtr refs %d", domain, domain->refs));
	KASSERT(domain->ctx_cnt >= domain->refs,
	    ("domain %p ctx dtr refs %d ctx_cnt %d", domain,
	    domain->refs, domain->ctx_cnt));
	domain->refs--;
	domain->ctx_cnt--;
	LIST_REMOVE(&ctx->context, link);
}

struct amdiommu_ctx *
amdiommu_find_ctx_locked(struct amdiommu_unit *unit, uint16_t rid)
{
	struct amdiommu_domain *domain;
	struct iommu_ctx *ctx;

	AMDIOMMU_ASSERT_LOCKED(unit);

	LIST_FOREACH(domain, &unit->domains, link) {
		LIST_FOREACH(ctx, &domain->iodom.contexts, link) {
			if (ctx->rid == rid)
				return (IOCTX2CTX(ctx));
		}
	}
	return (NULL);
}

struct amdiommu_domain *
amdiommu_find_domain(struct amdiommu_unit *unit, uint16_t rid)
{
	struct amdiommu_domain *domain;
	struct iommu_ctx *ctx;

	AMDIOMMU_LOCK(unit);
	LIST_FOREACH(domain, &unit->domains, link) {
		LIST_FOREACH(ctx, &domain->iodom.contexts, link) {
			if (ctx->rid == rid)
				break;
		}
	}
	AMDIOMMU_UNLOCK(unit);
	return (domain);
}

static void
amdiommu_free_ctx_locked(struct amdiommu_unit *unit, struct amdiommu_ctx *ctx)
{
	struct amdiommu_dte *dtep;
	struct amdiommu_domain *domain;

	AMDIOMMU_ASSERT_LOCKED(unit);
	KASSERT(ctx->context.refs >= 1,
	    ("amdiommu %p ctx %p refs %u", unit, ctx, ctx->context.refs));

	/*
	 * If our reference is not last, only the dereference should
	 * be performed.
	 */
	if (ctx->context.refs > 1) {
		ctx->context.refs--;
		AMDIOMMU_UNLOCK(unit);
		return;
	}

	KASSERT((ctx->context.flags & IOMMU_CTX_DISABLED) == 0,
	    ("lost ref on disabled ctx %p", ctx));

	/*
	 * Otherwise, the device table entry must be cleared before
	 * the page table is destroyed.
	 */
	dtep = amdiommu_get_dtep(ctx);
	dtep->v = 0;
	atomic_thread_fence_rel();
	memset(dtep, 0, sizeof(*dtep));

	domain = CTX2DOM(ctx);
	amdiommu_qi_invalidate_ctx_locked_nowait(ctx);
	amdiommu_qi_invalidate_ir_locked_nowait(unit, ctx->context.rid);
	amdiommu_qi_invalidate_all_pages_locked_nowait(domain);
	amdiommu_qi_invalidate_wait_sync(AMD2IOMMU(CTX2AMD(ctx)));

	if (unit->irte_enabled)
		amdiommu_ctx_fini_irte(ctx);

	amdiommu_ctx_unlink(ctx);
	free(ctx->context.tag, M_AMDIOMMU_CTX);
	free(ctx, M_AMDIOMMU_CTX);
	amdiommu_unref_domain_locked(unit, domain);
}

static void
amdiommu_unref_domain_locked(struct amdiommu_unit *unit,
    struct amdiommu_domain *domain)
{
	AMDIOMMU_ASSERT_LOCKED(unit);
	KASSERT(domain->refs >= 1,
	    ("amdiommu%d domain %p refs %u", unit->iommu.unit, domain,
	    domain->refs));
	KASSERT(domain->refs > domain->ctx_cnt,
	    ("amdiommu%d domain %p refs %d ctx_cnt %d", unit->iommu.unit,
	    domain, domain->refs, domain->ctx_cnt));

	if (domain->refs > 1) {
		domain->refs--;
		AMDIOMMU_UNLOCK(unit);
		return;
	}

	LIST_REMOVE(domain, link);
	AMDIOMMU_UNLOCK(unit);

	taskqueue_drain(unit->iommu.delayed_taskqueue,
	    &domain->iodom.unload_task);
	amdiommu_domain_destroy(domain);
}

static void
dte_entry_init_one(struct amdiommu_dte *dtep, struct amdiommu_ctx *ctx,
    vm_page_t pgtblr, uint8_t dte, uint32_t edte)
{
	struct amdiommu_domain *domain;
	struct amdiommu_unit *unit;

	domain = CTX2DOM(ctx);
	unit = DOM2AMD(domain);

	dtep->tv = 1;
	/* dtep->had not used for now */
	dtep->ir = 1;
	dtep->iw = 1;
	dtep->domainid = domain->domain;
	dtep->pioctl = AMDIOMMU_DTE_PIOCTL_DIS;

	/* fill device interrupt passing hints from IVHD. */
	dtep->initpass = (dte & ACPI_IVHD_INIT_PASS) != 0;
	dtep->eintpass = (dte & ACPI_IVHD_EINT_PASS) != 0;
	dtep->nmipass = (dte & ACPI_IVHD_NMI_PASS) != 0;
	dtep->sysmgt = (dte & ACPI_IVHD_SYSTEM_MGMT) >> 4;
	dtep->lint0pass = (dte & ACPI_IVHD_LINT0_PASS) != 0;
	dtep->lint1pass = (dte & ACPI_IVHD_LINT1_PASS) != 0;

	if (unit->irte_enabled) {
		dtep->iv = 1;
		dtep->i = 0;
		dtep->inttablen = ilog2(unit->irte_nentries);
		dtep->intrroot = pmap_kextract(unit->irte_x2apic ?
		    (vm_offset_t)ctx->irtx2 :
		    (vm_offset_t)ctx->irtb) >> 6;

		dtep->intctl = AMDIOMMU_DTE_INTCTL_MAP;
	}

	if ((DOM2IODOM(domain)->flags & IOMMU_DOMAIN_IDMAP) != 0) {
		dtep->pgmode = AMDIOMMU_DTE_PGMODE_1T1;
	} else {
		MPASS(domain->pglvl > 0 && domain->pglvl <=
		    AMDIOMMU_PGTBL_MAXLVL);
		dtep->pgmode = domain->pglvl;
		dtep->ptroot = VM_PAGE_TO_PHYS(pgtblr) >> 12;
	}

	atomic_thread_fence_rel();
	dtep->v = 1;
}

static void
dte_entry_init(struct amdiommu_ctx *ctx, bool move, uint8_t dte, uint32_t edte)
{
	struct amdiommu_dte *dtep;
	struct amdiommu_unit *unit;
	struct amdiommu_domain *domain;
	int i;

	domain = CTX2DOM(ctx);
	unit = DOM2AMD(domain);

	dtep = amdiommu_get_dtep(ctx);
	KASSERT(dtep->v == 0,
	    ("amdiommu%d initializing valid dte @%p %#jx",
	    CTX2AMD(ctx)->iommu.unit, dtep, (uintmax_t)(*(uint64_t *)dtep)));

	if (iommu_is_buswide_ctx(AMD2IOMMU(unit),
	    PCI_RID2BUS(ctx->context.rid))) {
		MPASS(!move);
		for (i = 0; i <= PCI_BUSMAX; i++) {
			dte_entry_init_one(&dtep[i], ctx, domain->pgtblr,
			    dte, edte);
		}
	} else {
		dte_entry_init_one(dtep, ctx, domain->pgtblr, dte, edte);
	}
}

struct amdiommu_ctx *
amdiommu_get_ctx_for_dev(struct amdiommu_unit *unit, device_t dev, uint16_t rid,
    int dev_domain, bool id_mapped, bool rmrr_init, uint8_t dte, uint32_t edte)
{
	struct amdiommu_domain *domain, *domain1;
	struct amdiommu_ctx *ctx, *ctx1;
	int bus, slot, func;

	if (dev != NULL) {
		bus = pci_get_bus(dev);
		slot = pci_get_slot(dev);
		func = pci_get_function(dev);
	} else {
		bus = PCI_RID2BUS(rid);
		slot = PCI_RID2SLOT(rid);
		func = PCI_RID2FUNC(rid);
	}
	AMDIOMMU_LOCK(unit);
	KASSERT(!iommu_is_buswide_ctx(AMD2IOMMU(unit), bus) ||
	    (slot == 0 && func == 0),
	    ("iommu%d pci%d:%d:%d get_ctx for buswide", AMD2IOMMU(unit)->unit,
	    bus, slot, func));
	ctx = amdiommu_find_ctx_locked(unit, rid);
	if (ctx == NULL) {
		/*
		 * Perform the allocations which require sleep or have
		 * higher chance to succeed if the sleep is allowed.
		 */
		AMDIOMMU_UNLOCK(unit);
		domain1 = amdiommu_domain_alloc(unit, id_mapped);
		if (domain1 == NULL)
			return (NULL);
		if (!id_mapped) {
			/*
			 * XXXKIB IVMD seems to be less significant
			 * and less used on AMD than RMRR on Intel.
			 * Not implemented for now.
			 */
		}
		ctx1 = amdiommu_ctx_alloc(domain1, rid);
		amdiommu_ctx_init_irte(ctx1);
		AMDIOMMU_LOCK(unit);

		/*
		 * Recheck the contexts, other thread might have
		 * already allocated needed one.
		 */
		ctx = amdiommu_find_ctx_locked(unit, rid);
		if (ctx == NULL) {
			domain = domain1;
			ctx = ctx1;
			amdiommu_ctx_link(ctx);
			ctx->context.tag->owner = dev;
			iommu_device_tag_init(CTX2IOCTX(ctx), dev);

			LIST_INSERT_HEAD(&unit->domains, domain, link);
			dte_entry_init(ctx, false, dte, edte);
			amdiommu_qi_invalidate_ctx_locked(ctx);
			if (dev != NULL) {
				device_printf(dev,
			    "amdiommu%d pci%d:%d:%d:%d rid %x domain %d "
				    "%s-mapped\n",
				    AMD2IOMMU(unit)->unit, unit->unit_dom,
				    bus, slot, func, rid, domain->domain,
				    id_mapped ? "id" : "re");
			}
		} else {
			amdiommu_domain_destroy(domain1);
			/* Nothing needs to be done to destroy ctx1. */
			free(ctx1, M_AMDIOMMU_CTX);
			domain = CTX2DOM(ctx);
			ctx->context.refs++; /* tag referenced us */
		}
	} else {
		domain = CTX2DOM(ctx);
		if (ctx->context.tag->owner == NULL)
			ctx->context.tag->owner = dev;
		ctx->context.refs++; /* tag referenced us */
	}
	AMDIOMMU_UNLOCK(unit);

	return (ctx);
}

struct iommu_ctx *
amdiommu_get_ctx(struct iommu_unit *iommu, device_t dev, uint16_t rid,
    bool id_mapped, bool rmrr_init)
{
	struct amdiommu_unit *unit;
	struct amdiommu_ctx *ret;
	int error;
	uint32_t edte;
	uint16_t rid1;
	uint8_t dte;

	error = amdiommu_find_unit(dev, &unit,  &rid1,  &dte, &edte,
	    bootverbose);
	if (error != 0)
		return (NULL);
	if (AMD2IOMMU(unit) != iommu)	/* XXX complain loudly */
		return (NULL);
	ret = amdiommu_get_ctx_for_dev(unit, dev, rid1, pci_get_domain(dev),
	    id_mapped, rmrr_init, dte, edte);
	return (CTX2IOCTX(ret));
}

void
amdiommu_free_ctx_locked_method(struct iommu_unit *iommu,
    struct iommu_ctx *context)
{
	struct amdiommu_unit *unit;
	struct amdiommu_ctx *ctx;

	unit = IOMMU2AMD(iommu);
	ctx = IOCTX2CTX(context);
	amdiommu_free_ctx_locked(unit, ctx);
}
