/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013, 2014, 2024 The FreeBSD Foundation
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

#include "opt_acpi.h"
#if defined(__amd64__)
#define	DEV_APIC
#else
#include "opt_apic.h"
#endif
#include "opt_ddb.h"

#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/memdesc.h>
#include <sys/mutex.h>
#include <sys/sf_buf.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/rman.h>
#include <sys/rwlock.h>
#include <sys/taskqueue.h>
#include <sys/tree.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <x86/include/busdma_impl.h>
#include <dev/iommu/busdma_iommu.h>
#include <dev/iommu/iommu.h>
#include <x86/iommu/x86_iommu.h>
#include <x86/iommu/iommu_intrmap.h>
#ifdef DEV_APIC
#include "pcib_if.h"
#include <machine/intr_machdep.h>
#include <x86/apicreg.h>
#include <x86/apicvar.h>
#endif

vm_page_t
iommu_pgalloc(vm_object_t obj, vm_pindex_t idx, int flags)
{
	vm_page_t m;
	int zeroed, aflags;

	zeroed = (flags & IOMMU_PGF_ZERO) != 0 ? VM_ALLOC_ZERO : 0;
	aflags = zeroed | VM_ALLOC_NOBUSY | VM_ALLOC_SYSTEM | VM_ALLOC_NODUMP |
	    ((flags & IOMMU_PGF_WAITOK) != 0 ? VM_ALLOC_WAITFAIL :
	    VM_ALLOC_NOWAIT);
	for (;;) {
		if ((flags & IOMMU_PGF_OBJL) == 0)
			VM_OBJECT_WLOCK(obj);
		m = vm_page_lookup(obj, idx);
		if ((flags & IOMMU_PGF_NOALLOC) != 0 || m != NULL) {
			if ((flags & IOMMU_PGF_OBJL) == 0)
				VM_OBJECT_WUNLOCK(obj);
			break;
		}
		m = vm_page_alloc_contig(obj, idx, aflags, 1, 0,
		    iommu_high, PAGE_SIZE, 0, VM_MEMATTR_DEFAULT);
		if ((flags & IOMMU_PGF_OBJL) == 0)
			VM_OBJECT_WUNLOCK(obj);
		if (m != NULL) {
			if (zeroed && (m->flags & PG_ZERO) == 0)
				pmap_zero_page(m);
			atomic_add_int(&iommu_tbl_pagecnt, 1);
			break;
		}
		if ((flags & IOMMU_PGF_WAITOK) == 0)
			break;
	}
	return (m);
}

void
iommu_pgfree(vm_object_t obj, vm_pindex_t idx, int flags,
    struct iommu_map_entry *entry)
{
	vm_page_t m;

	if ((flags & IOMMU_PGF_OBJL) == 0)
		VM_OBJECT_WLOCK(obj);
	m = vm_page_grab(obj, idx, VM_ALLOC_NOCREAT);
	if (m != NULL) {
		if (entry == NULL) {
			vm_page_free(m);
			atomic_subtract_int(&iommu_tbl_pagecnt, 1);
		} else {
			vm_page_remove_xbusy(m);	/* keep page busy */
			SLIST_INSERT_HEAD(&entry->pgtbl_free, m, plinks.s.ss);
		}
	}
	if ((flags & IOMMU_PGF_OBJL) == 0)
		VM_OBJECT_WUNLOCK(obj);
}

void *
iommu_map_pgtbl(vm_object_t obj, vm_pindex_t idx, int flags,
    struct sf_buf **sf)
{
	vm_page_t m;
	bool allocated;

	if ((flags & IOMMU_PGF_OBJL) == 0)
		VM_OBJECT_WLOCK(obj);
	m = vm_page_lookup(obj, idx);
	if (m == NULL && (flags & IOMMU_PGF_ALLOC) != 0) {
		m = iommu_pgalloc(obj, idx, flags | IOMMU_PGF_OBJL);
		allocated = true;
	} else
		allocated = false;
	if (m == NULL) {
		if ((flags & IOMMU_PGF_OBJL) == 0)
			VM_OBJECT_WUNLOCK(obj);
		return (NULL);
	}
	/* Sleepable allocations cannot fail. */
	if ((flags & IOMMU_PGF_WAITOK) != 0)
		VM_OBJECT_WUNLOCK(obj);
	sched_pin();
	*sf = sf_buf_alloc(m, SFB_CPUPRIVATE | ((flags & IOMMU_PGF_WAITOK)
	    == 0 ? SFB_NOWAIT : 0));
	if (*sf == NULL) {
		sched_unpin();
		if (allocated) {
			VM_OBJECT_ASSERT_WLOCKED(obj);
			iommu_pgfree(obj, m->pindex, flags | IOMMU_PGF_OBJL,
			    NULL);
		}
		if ((flags & IOMMU_PGF_OBJL) == 0)
			VM_OBJECT_WUNLOCK(obj);
		return (NULL);
	}
	if ((flags & (IOMMU_PGF_WAITOK | IOMMU_PGF_OBJL)) ==
	    (IOMMU_PGF_WAITOK | IOMMU_PGF_OBJL))
		VM_OBJECT_WLOCK(obj);
	else if ((flags & (IOMMU_PGF_WAITOK | IOMMU_PGF_OBJL)) == 0)
		VM_OBJECT_WUNLOCK(obj);
	return ((void *)sf_buf_kva(*sf));
}

void
iommu_unmap_pgtbl(struct sf_buf *sf)
{

	sf_buf_free(sf);
	sched_unpin();
}

iommu_haddr_t iommu_high;
int iommu_tbl_pagecnt;

SYSCTL_NODE(_hw_iommu, OID_AUTO, dmar, CTLFLAG_RD | CTLFLAG_MPSAFE,
    NULL, "");
SYSCTL_INT(_hw_iommu, OID_AUTO, tbl_pagecnt, CTLFLAG_RD,
    &iommu_tbl_pagecnt, 0,
    "Count of pages used for IOMMU pagetables");

int iommu_qi_batch_coalesce = 100;
SYSCTL_INT(_hw_iommu, OID_AUTO, batch_coalesce, CTLFLAG_RWTUN,
    &iommu_qi_batch_coalesce, 0,
    "Number of qi batches between interrupt");

static struct iommu_unit *
x86_no_iommu_find(device_t dev, bool verbose)
{
	return (NULL);
}

static int
x86_no_iommu_alloc_msi_intr(device_t src, u_int *cookies, u_int count)
{
	return (EOPNOTSUPP);
}

static int
x86_no_iommu_map_msi_intr(device_t src, u_int cpu, u_int vector,
    u_int cookie, uint64_t *addr, uint32_t *data)
{
	return (EOPNOTSUPP);
}

static int
x86_no_iommu_unmap_msi_intr(device_t src, u_int cookie)
{
	return (0);
}

static int
x86_no_iommu_map_ioapic_intr(u_int ioapic_id, u_int cpu, u_int vector,
    bool edge, bool activehi, int irq, u_int *cookie, uint32_t *hi,
    uint32_t *lo)
{
	return (EOPNOTSUPP);
}

static int
x86_no_iommu_unmap_ioapic_intr(u_int ioapic_id, u_int *cookie)
{
	return (0);
}

static struct x86_iommu x86_no_iommu = {
	.find = x86_no_iommu_find,
	.alloc_msi_intr = x86_no_iommu_alloc_msi_intr,
	.map_msi_intr = x86_no_iommu_map_msi_intr,
	.unmap_msi_intr = x86_no_iommu_unmap_msi_intr,
	.map_ioapic_intr = x86_no_iommu_map_ioapic_intr,
	.unmap_ioapic_intr = x86_no_iommu_unmap_ioapic_intr,
};

static struct x86_iommu *x86_iommu = &x86_no_iommu;

void
set_x86_iommu(struct x86_iommu *x)
{
	MPASS(x86_iommu == &x86_no_iommu);
	x86_iommu = x;
}

struct x86_iommu *
get_x86_iommu(void)
{
	return (x86_iommu);
}

void
iommu_domain_unload_entry(struct iommu_map_entry *entry, bool free,
    bool cansleep)
{
	x86_iommu->domain_unload_entry(entry, free, cansleep);
}

void
iommu_domain_unload(struct iommu_domain *iodom,
    struct iommu_map_entries_tailq *entries, bool cansleep)
{
	x86_iommu->domain_unload(iodom, entries, cansleep);
}

struct iommu_ctx *
iommu_get_ctx(struct iommu_unit *iommu, device_t dev, uint16_t rid,
    bool id_mapped, bool rmrr_init)
{
	return (x86_iommu->get_ctx(iommu, dev, rid, id_mapped, rmrr_init));
}

void
iommu_free_ctx_locked(struct iommu_unit *iommu, struct iommu_ctx *context)
{
	x86_iommu->free_ctx_locked(iommu, context);
}

struct iommu_unit *
iommu_find(device_t dev, bool verbose)
{
	return (x86_iommu->find(dev, verbose));
}

int
iommu_alloc_msi_intr(device_t src, u_int *cookies, u_int count)
{
	return (x86_iommu->alloc_msi_intr(src, cookies, count));
}

int
iommu_map_msi_intr(device_t src, u_int cpu, u_int vector, u_int cookie,
    uint64_t *addr, uint32_t *data)
{
	return (x86_iommu->map_msi_intr(src, cpu, vector, cookie,
	    addr, data));
}

int
iommu_unmap_msi_intr(device_t src, u_int cookie)
{
	return (x86_iommu->unmap_msi_intr(src, cookie));
}

int
iommu_map_ioapic_intr(u_int ioapic_id, u_int cpu, u_int vector, bool edge,
    bool activehi, int irq, u_int *cookie, uint32_t *hi, uint32_t *lo)
{
	return (x86_iommu->map_ioapic_intr(ioapic_id, cpu, vector, edge,
	    activehi, irq, cookie, hi, lo));
}

int
iommu_unmap_ioapic_intr(u_int ioapic_id, u_int *cookie)
{
	return (x86_iommu->unmap_ioapic_intr(ioapic_id, cookie));
}

void
iommu_unit_pre_instantiate_ctx(struct iommu_unit *unit)
{
	x86_iommu->unit_pre_instantiate_ctx(unit);
}

#define	IOMMU2X86C(iommu)	(x86_iommu->get_x86_common(iommu))

static bool
iommu_qi_seq_processed(struct iommu_unit *unit,
    const struct iommu_qi_genseq *pseq)
{
	struct x86_unit_common *x86c;
	u_int gen;

	x86c = IOMMU2X86C(unit);
	gen = x86c->inv_waitd_gen;
	return (pseq->gen < gen || (pseq->gen == gen && pseq->seq <=
	    atomic_load_64(&x86c->inv_waitd_seq_hw)));
}

void
iommu_qi_emit_wait_seq(struct iommu_unit *unit, struct iommu_qi_genseq *pseq,
    bool emit_wait)
{
	struct x86_unit_common *x86c;
	struct iommu_qi_genseq gsec;
	uint32_t seq;

	KASSERT(pseq != NULL, ("wait descriptor with no place for seq"));
	IOMMU_ASSERT_LOCKED(unit);
	x86c = IOMMU2X86C(unit);

	if (x86c->inv_waitd_seq == 0xffffffff) {
		gsec.gen = x86c->inv_waitd_gen;
		gsec.seq = x86c->inv_waitd_seq;
		x86_iommu->qi_ensure(unit, 1);
		x86_iommu->qi_emit_wait_descr(unit, gsec.seq, false,
		    true, false);
		x86_iommu->qi_advance_tail(unit);
		while (!iommu_qi_seq_processed(unit, &gsec))
			cpu_spinwait();
		x86c->inv_waitd_gen++;
		x86c->inv_waitd_seq = 1;
	}
	seq = x86c->inv_waitd_seq++;
	pseq->gen = x86c->inv_waitd_gen;
	pseq->seq = seq;
	if (emit_wait) {
		x86_iommu->qi_ensure(unit, 1);
		x86_iommu->qi_emit_wait_descr(unit, seq, true, true, false);
	}
}

/*
 * To avoid missed wakeups, callers must increment the unit's waiters count
 * before advancing the tail past the wait descriptor.
 */
void
iommu_qi_wait_for_seq(struct iommu_unit *unit, const struct iommu_qi_genseq *
    gseq, bool nowait)
{
	struct x86_unit_common *x86c;

	IOMMU_ASSERT_LOCKED(unit);
	x86c = IOMMU2X86C(unit);

	KASSERT(x86c->inv_seq_waiters > 0, ("%s: no waiters", __func__));
	while (!iommu_qi_seq_processed(unit, gseq)) {
		if (cold || nowait) {
			cpu_spinwait();
		} else {
			msleep(&x86c->inv_seq_waiters, &unit->lock, 0,
			    "dmarse", hz);
		}
	}
	x86c->inv_seq_waiters--;
}

/*
 * The caller must not be using the entry's dmamap_link field.
 */
void
iommu_qi_invalidate_locked(struct iommu_domain *domain,
    struct iommu_map_entry *entry, bool emit_wait)
{
	struct iommu_unit *unit;
	struct x86_unit_common *x86c;

	unit = domain->iommu;
	x86c = IOMMU2X86C(unit);
	IOMMU_ASSERT_LOCKED(unit);

	x86_iommu->qi_invalidate_emit(domain, entry->start, entry->end -
	    entry->start, &entry->gseq, emit_wait);

	/*
	 * To avoid a data race in dmar_qi_task(), the entry's gseq must be
	 * initialized before the entry is added to the TLB flush list, and the
	 * entry must be added to that list before the tail is advanced.  More
	 * precisely, the tail must not be advanced past the wait descriptor
	 * that will generate the interrupt that schedules dmar_qi_task() for
	 * execution before the entry is added to the list.  While an earlier
	 * call to dmar_qi_ensure() might have advanced the tail, it will not
	 * advance it past the wait descriptor.
	 *
	 * See the definition of struct dmar_unit for more information on
	 * synchronization.
	 */
	entry->tlb_flush_next = NULL;
	atomic_store_rel_ptr((uintptr_t *)&x86c->tlb_flush_tail->
	    tlb_flush_next, (uintptr_t)entry);
	x86c->tlb_flush_tail = entry;

	x86_iommu->qi_advance_tail(unit);
}

void
iommu_qi_invalidate_sync(struct iommu_domain *domain, iommu_gaddr_t base,
    iommu_gaddr_t size, bool cansleep)
{
	struct iommu_unit *unit;
	struct iommu_qi_genseq gseq;

	unit = domain->iommu;
	IOMMU_LOCK(unit);
	x86_iommu->qi_invalidate_emit(domain, base, size, &gseq, true);

	/*
	 * To avoid a missed wakeup in iommu_qi_task(), the unit's
	 * waiters count must be incremented before the tail is
	 * advanced.
	 */
	IOMMU2X86C(unit)->inv_seq_waiters++;

	x86_iommu->qi_advance_tail(unit);
	iommu_qi_wait_for_seq(unit, &gseq, !cansleep);
	IOMMU_UNLOCK(unit);
}

void
iommu_qi_drain_tlb_flush(struct iommu_unit *unit)
{
	struct x86_unit_common *x86c;
	struct iommu_map_entry *entry, *head;

	x86c = IOMMU2X86C(unit);
	for (head = x86c->tlb_flush_head;; head = entry) {
		entry = (struct iommu_map_entry *)
		    atomic_load_acq_ptr((uintptr_t *)&head->tlb_flush_next);
		if (entry == NULL ||
		    !iommu_qi_seq_processed(unit, &entry->gseq))
			break;
		x86c->tlb_flush_head = entry;
		iommu_gas_free_entry(head);
		if ((entry->flags & IOMMU_MAP_ENTRY_RMRR) != 0)
			iommu_gas_free_region(entry);
		else
			iommu_gas_free_space(entry);
	}
}

void
iommu_qi_common_init(struct iommu_unit *unit, task_fn_t qi_task)
{
	struct x86_unit_common *x86c;
	u_int qi_sz;

	x86c = IOMMU2X86C(unit);

	x86c->tlb_flush_head = x86c->tlb_flush_tail =
            iommu_gas_alloc_entry(NULL, 0);
	TASK_INIT(&x86c->qi_task, 0, qi_task, unit);
	x86c->qi_taskqueue = taskqueue_create_fast("iommuqf", M_WAITOK,
	    taskqueue_thread_enqueue, &x86c->qi_taskqueue);
	taskqueue_start_threads(&x86c->qi_taskqueue, 1, PI_AV,
	    "iommu%d qi taskq", unit->unit);

	x86c->inv_waitd_gen = 0;
	x86c->inv_waitd_seq = 1;

	qi_sz = 3;
	TUNABLE_INT_FETCH("hw.iommu.qi_size", &qi_sz);
	if (qi_sz > x86c->qi_buf_maxsz)
		qi_sz = x86c->qi_buf_maxsz;
	x86c->inv_queue_size = (1ULL << qi_sz) * PAGE_SIZE;
	/* Reserve one descriptor to prevent wraparound. */
	x86c->inv_queue_avail = x86c->inv_queue_size -
	    x86c->qi_cmd_sz;

	/*
	 * The invalidation queue reads by DMARs/AMDIOMMUs are always
	 * coherent.
	 */
	x86c->inv_queue = kmem_alloc_contig(x86c->inv_queue_size,
	    M_WAITOK | M_ZERO, 0, iommu_high, PAGE_SIZE, 0,
	    VM_MEMATTR_DEFAULT);
	x86c->inv_waitd_seq_hw_phys = pmap_kextract(
	    (vm_offset_t)&x86c->inv_waitd_seq_hw);
}

void
iommu_qi_common_fini(struct iommu_unit *unit, void (*disable_qi)(
    struct iommu_unit *))
{
	struct x86_unit_common *x86c;
	struct iommu_qi_genseq gseq;

	x86c = IOMMU2X86C(unit);

	taskqueue_drain(x86c->qi_taskqueue, &x86c->qi_task);
	taskqueue_free(x86c->qi_taskqueue);
	x86c->qi_taskqueue = NULL;

	IOMMU_LOCK(unit);
	/* quisce */
	x86_iommu->qi_ensure(unit, 1);
	iommu_qi_emit_wait_seq(unit, &gseq, true);
	/* See iommu_qi_invalidate_locked(). */
	x86c->inv_seq_waiters++;
	x86_iommu->qi_advance_tail(unit);
	iommu_qi_wait_for_seq(unit, &gseq, false);
	/* only after the quisce, disable queue */
	disable_qi(unit);
	KASSERT(x86c->inv_seq_waiters == 0,
	    ("iommu%d: waiters on disabled queue", unit->unit));
	IOMMU_UNLOCK(unit);

	kmem_free(x86c->inv_queue, x86c->inv_queue_size);
	x86c->inv_queue = NULL;
	x86c->inv_queue_size = 0;
}

int
iommu_alloc_irq(struct iommu_unit *unit, int idx)
{
	device_t dev, pcib;
	struct iommu_msi_data *dmd;
	uint64_t msi_addr;
	uint32_t msi_data;
	int error;

	MPASS(idx >= 0 || idx < IOMMU_MAX_MSI);

	dev = unit->dev;
	dmd = &IOMMU2X86C(unit)->intrs[idx];
	pcib = device_get_parent(device_get_parent(dev)); /* Really not pcib */
	error = PCIB_ALLOC_MSIX(pcib, dev, &dmd->irq);
	if (error != 0) {
		device_printf(dev, "cannot allocate %s interrupt, %d\n",
		    dmd->name, error);
		goto err1;
	}
	error = bus_set_resource(dev, SYS_RES_IRQ, dmd->irq_rid,
	    dmd->irq, 1);
	if (error != 0) {
		device_printf(dev, "cannot set %s interrupt resource, %d\n",
		    dmd->name, error);
		goto err2;
	}
	dmd->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &dmd->irq_rid, RF_ACTIVE);
	if (dmd->irq_res == NULL) {
		device_printf(dev,
		    "cannot allocate resource for %s interrupt\n", dmd->name);
		error = ENXIO;
		goto err3;
	}
	error = bus_setup_intr(dev, dmd->irq_res, INTR_TYPE_MISC,
	    dmd->handler, NULL, unit, &dmd->intr_handle);
	if (error != 0) {
		device_printf(dev, "cannot setup %s interrupt, %d\n",
		    dmd->name, error);
		goto err4;
	}
	bus_describe_intr(dev, dmd->irq_res, dmd->intr_handle, "%s", dmd->name);
	error = PCIB_MAP_MSI(pcib, dev, dmd->irq, &msi_addr, &msi_data);
	if (error != 0) {
		device_printf(dev, "cannot map %s interrupt, %d\n",
		    dmd->name, error);
		goto err5;
	}

	dmd->msi_data = msi_data;
	dmd->msi_addr = msi_addr;

	return (0);

err5:
	bus_teardown_intr(dev, dmd->irq_res, dmd->intr_handle);
err4:
	bus_release_resource(dev, SYS_RES_IRQ, dmd->irq_rid, dmd->irq_res);
err3:
	bus_delete_resource(dev, SYS_RES_IRQ, dmd->irq_rid);
err2:
	PCIB_RELEASE_MSIX(pcib, dev, dmd->irq);
	dmd->irq = -1;
err1:
	return (error);
}

void
iommu_release_intr(struct iommu_unit *unit, int idx)
{
	device_t dev;
	struct iommu_msi_data *dmd;

	MPASS(idx >= 0 || idx < IOMMU_MAX_MSI);

	dmd = &IOMMU2X86C(unit)->intrs[idx];
	if (dmd->handler == NULL || dmd->irq == -1)
		return;
	dev = unit->dev;

	bus_teardown_intr(dev, dmd->irq_res, dmd->intr_handle);
	bus_release_resource(dev, SYS_RES_IRQ, dmd->irq_rid, dmd->irq_res);
	bus_delete_resource(dev, SYS_RES_IRQ, dmd->irq_rid);
	PCIB_RELEASE_MSIX(device_get_parent(device_get_parent(dev)),
	    dev, dmd->irq);
	dmd->irq = -1;
}

void
iommu_device_tag_init(struct iommu_ctx *ctx, device_t dev)
{
	bus_addr_t maxaddr;

	maxaddr = MIN(ctx->domain->end, BUS_SPACE_MAXADDR);
	ctx->tag->common.impl = &bus_dma_iommu_impl;
	ctx->tag->common.boundary = 0;
	ctx->tag->common.lowaddr = maxaddr;
	ctx->tag->common.highaddr = maxaddr;
	ctx->tag->common.maxsize = maxaddr;
	ctx->tag->common.nsegments = BUS_SPACE_UNRESTRICTED;
	ctx->tag->common.maxsegsz = maxaddr;
	ctx->tag->ctx = ctx;
	ctx->tag->owner = dev;
}

void
iommu_domain_free_entry(struct iommu_map_entry *entry, bool free)
{
	if ((entry->flags & IOMMU_MAP_ENTRY_RMRR) != 0)
		iommu_gas_free_region(entry);
	else
		iommu_gas_free_space(entry);
	if (free)
		iommu_gas_free_entry(entry);
	else
		entry->flags = 0;
}

/*
 * Index of the pte for the guest address base in the page table at
 * the level lvl.
 */
int
pglvl_pgtbl_pte_off(int pglvl, iommu_gaddr_t base, int lvl)
{

	base >>= IOMMU_PAGE_SHIFT + (pglvl - lvl - 1) *
	    IOMMU_NPTEPGSHIFT;
	return (base & IOMMU_PTEMASK);
}

/*
 * Returns the page index of the page table page in the page table
 * object, which maps the given address base at the page table level
 * lvl.
 */
vm_pindex_t
pglvl_pgtbl_get_pindex(int pglvl, iommu_gaddr_t base, int lvl)
{
	vm_pindex_t idx, pidx;
	int i;

	KASSERT(lvl >= 0 && lvl < pglvl,
	    ("wrong lvl %d %d", pglvl, lvl));

	for (pidx = idx = 0, i = 0; i < lvl; i++, pidx = idx) {
		idx = pglvl_pgtbl_pte_off(pglvl, base, i) +
		    pidx * IOMMU_NPTEPG + 1;
	}
	return (idx);
}

/*
 * Calculate the total amount of page table pages needed to map the
 * whole bus address space on the context with the selected agaw.
 */
vm_pindex_t
pglvl_max_pages(int pglvl)
{
	vm_pindex_t res;
	int i;

	for (res = 0, i = pglvl; i > 0; i--) {
		res *= IOMMU_NPTEPG;
		res++;
	}
	return (res);
}

iommu_gaddr_t
pglvl_page_size(int total_pglvl, int lvl)
{
	int rlvl;
	static const iommu_gaddr_t pg_sz[] = {
		(iommu_gaddr_t)IOMMU_PAGE_SIZE,
		(iommu_gaddr_t)IOMMU_PAGE_SIZE << IOMMU_NPTEPGSHIFT,
		(iommu_gaddr_t)IOMMU_PAGE_SIZE << (2 * IOMMU_NPTEPGSHIFT),
		(iommu_gaddr_t)IOMMU_PAGE_SIZE << (3 * IOMMU_NPTEPGSHIFT),
		(iommu_gaddr_t)IOMMU_PAGE_SIZE << (4 * IOMMU_NPTEPGSHIFT),
		(iommu_gaddr_t)IOMMU_PAGE_SIZE << (5 * IOMMU_NPTEPGSHIFT),
		(iommu_gaddr_t)IOMMU_PAGE_SIZE << (6 * IOMMU_NPTEPGSHIFT),
	};

	KASSERT(lvl >= 0 && lvl < total_pglvl,
	    ("total %d lvl %d", total_pglvl, lvl));
	rlvl = total_pglvl - lvl - 1;
	KASSERT(rlvl < nitems(pg_sz), ("sizeof pg_sz lvl %d", lvl));
	return (pg_sz[rlvl]);
}

void
iommu_device_set_iommu_prop(device_t dev, device_t iommu)
{
	device_t iommu_dev;
	int error;

	bus_topo_lock();
	error = device_get_prop(dev, DEV_PROP_NAME_IOMMU, (void **)&iommu_dev);
	if (error == ENOENT)
		device_set_prop(dev, DEV_PROP_NAME_IOMMU, iommu, NULL, NULL);
	bus_topo_unlock();
}

#ifdef DDB
#include <ddb/ddb.h>
#include <ddb/db_lex.h>

void
iommu_db_print_domain_entry(const struct iommu_map_entry *entry)
{
	struct iommu_map_entry *l, *r;

	db_printf(
	    "    start %jx end %jx first %jx last %jx free_down %jx flags %x ",
	    entry->start, entry->end, entry->first, entry->last,
	    entry->free_down, entry->flags);
	db_printf("left ");
	l = RB_LEFT(entry, rb_entry);
	if (l == NULL)
		db_printf("NULL ");
	else
		db_printf("%jx ", l->start);
	db_printf("right ");
	r = RB_RIGHT(entry, rb_entry);
	if (r == NULL)
		db_printf("NULL");
	else
		db_printf("%jx", r->start);
	db_printf("\n");
}

void
iommu_db_print_ctx(struct iommu_ctx *ctx)
{
	db_printf(
	    "    @%p pci%d:%d:%d refs %d flags %#x loads %lu unloads %lu\n",
	    ctx, pci_get_bus(ctx->tag->owner),
	    pci_get_slot(ctx->tag->owner),
	    pci_get_function(ctx->tag->owner), ctx->refs,
	    ctx->flags, ctx->loads, ctx->unloads);
}

void
iommu_db_domain_print_contexts(struct iommu_domain *iodom)
{
	struct iommu_ctx *ctx;

	if (LIST_EMPTY(&iodom->contexts))
		return;

	db_printf("  Contexts:\n");
	LIST_FOREACH(ctx, &iodom->contexts, link)
		iommu_db_print_ctx(ctx);
}

void
iommu_db_domain_print_mappings(struct iommu_domain *iodom)
{
	struct iommu_map_entry *entry;

	db_printf("    mapped:\n");
	RB_FOREACH(entry, iommu_gas_entries_tree, &iodom->rb_root) {
		iommu_db_print_domain_entry(entry);
		if (db_pager_quit)
			break;
	}
	if (db_pager_quit)
		return;
	db_printf("    unloading:\n");
	TAILQ_FOREACH(entry, &iodom->unload_entries, dmamap_link) {
		iommu_db_print_domain_entry(entry);
		if (db_pager_quit)
			break;
	}
}

#endif
