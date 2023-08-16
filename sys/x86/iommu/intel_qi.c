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
 */

#include <sys/cdefs.h>
#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/memdesc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/taskqueue.h>
#include <sys/time.h>
#include <sys/tree.h>
#include <sys/vmem.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/acpica/acpivar.h>
#include <dev/pci/pcireg.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <x86/include/busdma_impl.h>
#include <dev/iommu/busdma_iommu.h>
#include <x86/iommu/intel_reg.h>
#include <x86/iommu/intel_dmar.h>

static bool
dmar_qi_seq_processed(const struct dmar_unit *unit,
    const struct iommu_qi_genseq *pseq)
{
	u_int gen;

	gen = unit->inv_waitd_gen;
	return (pseq->gen < gen ||
	    (pseq->gen == gen && pseq->seq <= unit->inv_waitd_seq_hw));
}

static int
dmar_enable_qi(struct dmar_unit *unit)
{
	int error;

	DMAR_ASSERT_LOCKED(unit);
	unit->hw_gcmd |= DMAR_GCMD_QIE;
	dmar_write4(unit, DMAR_GCMD_REG, unit->hw_gcmd);
	DMAR_WAIT_UNTIL(((dmar_read4(unit, DMAR_GSTS_REG) & DMAR_GSTS_QIES)
	    != 0));
	return (error);
}

static int
dmar_disable_qi(struct dmar_unit *unit)
{
	int error;

	DMAR_ASSERT_LOCKED(unit);
	unit->hw_gcmd &= ~DMAR_GCMD_QIE;
	dmar_write4(unit, DMAR_GCMD_REG, unit->hw_gcmd);
	DMAR_WAIT_UNTIL(((dmar_read4(unit, DMAR_GSTS_REG) & DMAR_GSTS_QIES)
	    == 0));
	return (error);
}

static void
dmar_qi_advance_tail(struct dmar_unit *unit)
{

	DMAR_ASSERT_LOCKED(unit);
	dmar_write4(unit, DMAR_IQT_REG, unit->inv_queue_tail);
}

static void
dmar_qi_ensure(struct dmar_unit *unit, int descr_count)
{
	uint32_t head;
	int bytes;

	DMAR_ASSERT_LOCKED(unit);
	bytes = descr_count << DMAR_IQ_DESCR_SZ_SHIFT;
	for (;;) {
		if (bytes <= unit->inv_queue_avail)
			break;
		/* refill */
		head = dmar_read4(unit, DMAR_IQH_REG);
		head &= DMAR_IQH_MASK;
		unit->inv_queue_avail = head - unit->inv_queue_tail -
		    DMAR_IQ_DESCR_SZ;
		if (head <= unit->inv_queue_tail)
			unit->inv_queue_avail += unit->inv_queue_size;
		if (bytes <= unit->inv_queue_avail)
			break;

		/*
		 * No space in the queue, do busy wait.  Hardware must
		 * make a progress.  But first advance the tail to
		 * inform the descriptor streamer about entries we
		 * might have already filled, otherwise they could
		 * clog the whole queue..
		 *
		 * See dmar_qi_invalidate_locked() for a discussion
		 * about data race prevention.
		 */
		dmar_qi_advance_tail(unit);
		unit->inv_queue_full++;
		cpu_spinwait();
	}
	unit->inv_queue_avail -= bytes;
}

static void
dmar_qi_emit(struct dmar_unit *unit, uint64_t data1, uint64_t data2)
{

	DMAR_ASSERT_LOCKED(unit);
	*(volatile uint64_t *)(unit->inv_queue + unit->inv_queue_tail) = data1;
	unit->inv_queue_tail += DMAR_IQ_DESCR_SZ / 2;
	KASSERT(unit->inv_queue_tail <= unit->inv_queue_size,
	    ("tail overflow 0x%x 0x%jx", unit->inv_queue_tail,
	    (uintmax_t)unit->inv_queue_size));
	unit->inv_queue_tail &= unit->inv_queue_size - 1;
	*(volatile uint64_t *)(unit->inv_queue + unit->inv_queue_tail) = data2;
	unit->inv_queue_tail += DMAR_IQ_DESCR_SZ / 2;
	KASSERT(unit->inv_queue_tail <= unit->inv_queue_size,
	    ("tail overflow 0x%x 0x%jx", unit->inv_queue_tail,
	    (uintmax_t)unit->inv_queue_size));
	unit->inv_queue_tail &= unit->inv_queue_size - 1;
}

static void
dmar_qi_emit_wait_descr(struct dmar_unit *unit, uint32_t seq, bool intr,
    bool memw, bool fence)
{

	DMAR_ASSERT_LOCKED(unit);
	dmar_qi_emit(unit, DMAR_IQ_DESCR_WAIT_ID |
	    (intr ? DMAR_IQ_DESCR_WAIT_IF : 0) |
	    (memw ? DMAR_IQ_DESCR_WAIT_SW : 0) |
	    (fence ? DMAR_IQ_DESCR_WAIT_FN : 0) |
	    (memw ? DMAR_IQ_DESCR_WAIT_SD(seq) : 0),
	    memw ? unit->inv_waitd_seq_hw_phys : 0);
}

static void
dmar_qi_emit_wait_seq(struct dmar_unit *unit, struct iommu_qi_genseq *pseq,
    bool emit_wait)
{
	struct iommu_qi_genseq gsec;
	uint32_t seq;

	KASSERT(pseq != NULL, ("wait descriptor with no place for seq"));
	DMAR_ASSERT_LOCKED(unit);
	if (unit->inv_waitd_seq == 0xffffffff) {
		gsec.gen = unit->inv_waitd_gen;
		gsec.seq = unit->inv_waitd_seq;
		dmar_qi_ensure(unit, 1);
		dmar_qi_emit_wait_descr(unit, gsec.seq, false, true, false);
		dmar_qi_advance_tail(unit);
		while (!dmar_qi_seq_processed(unit, &gsec))
			cpu_spinwait();
		unit->inv_waitd_gen++;
		unit->inv_waitd_seq = 1;
	}
	seq = unit->inv_waitd_seq++;
	pseq->gen = unit->inv_waitd_gen;
	pseq->seq = seq;
	if (emit_wait) {
		dmar_qi_ensure(unit, 1);
		dmar_qi_emit_wait_descr(unit, seq, true, true, false);
	}
}

/*
 * To avoid missed wakeups, callers must increment the unit's waiters count
 * before advancing the tail past the wait descriptor.
 */
static void
dmar_qi_wait_for_seq(struct dmar_unit *unit, const struct iommu_qi_genseq *gseq,
    bool nowait)
{

	DMAR_ASSERT_LOCKED(unit);
	KASSERT(unit->inv_seq_waiters > 0, ("%s: no waiters", __func__));
	while (!dmar_qi_seq_processed(unit, gseq)) {
		if (cold || nowait) {
			cpu_spinwait();
		} else {
			msleep(&unit->inv_seq_waiters, &unit->iommu.lock, 0,
			    "dmarse", hz);
		}
	}
	unit->inv_seq_waiters--;
}

static void
dmar_qi_invalidate_emit(struct dmar_domain *domain, iommu_gaddr_t base,
    iommu_gaddr_t size, struct iommu_qi_genseq *pseq, bool emit_wait)
{
	struct dmar_unit *unit;
	iommu_gaddr_t isize;
	int am;

	unit = domain->dmar;
	DMAR_ASSERT_LOCKED(unit);
	for (; size > 0; base += isize, size -= isize) {
		am = calc_am(unit, base, size, &isize);
		dmar_qi_ensure(unit, 1);
		dmar_qi_emit(unit, DMAR_IQ_DESCR_IOTLB_INV |
		    DMAR_IQ_DESCR_IOTLB_PAGE | DMAR_IQ_DESCR_IOTLB_DW |
		    DMAR_IQ_DESCR_IOTLB_DR |
		    DMAR_IQ_DESCR_IOTLB_DID(domain->domain),
		    base | am);
	}
	dmar_qi_emit_wait_seq(unit, pseq, emit_wait);
}

/*
 * The caller must not be using the entry's dmamap_link field.
 */
void
dmar_qi_invalidate_locked(struct dmar_domain *domain,
    struct iommu_map_entry *entry, bool emit_wait)
{
	struct dmar_unit *unit;

	unit = domain->dmar;
	DMAR_ASSERT_LOCKED(unit);
	dmar_qi_invalidate_emit(domain, entry->start, entry->end -
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
	atomic_store_rel_ptr((uintptr_t *)&unit->tlb_flush_tail->tlb_flush_next,
	    (uintptr_t)entry);
	unit->tlb_flush_tail = entry;

	dmar_qi_advance_tail(unit);
}

void
dmar_qi_invalidate_sync(struct dmar_domain *domain, iommu_gaddr_t base,
    iommu_gaddr_t size, bool cansleep)
{
	struct dmar_unit *unit;
	struct iommu_qi_genseq gseq;

	unit = domain->dmar;
	DMAR_LOCK(unit);
	dmar_qi_invalidate_emit(domain, base, size, &gseq, true);

	/*
	 * To avoid a missed wakeup in dmar_qi_task(), the unit's waiters count
	 * must be incremented before the tail is advanced.
	 */
	unit->inv_seq_waiters++;

	dmar_qi_advance_tail(unit);
	dmar_qi_wait_for_seq(unit, &gseq, !cansleep);
	DMAR_UNLOCK(unit);
}

void
dmar_qi_invalidate_ctx_glob_locked(struct dmar_unit *unit)
{
	struct iommu_qi_genseq gseq;

	DMAR_ASSERT_LOCKED(unit);
	dmar_qi_ensure(unit, 2);
	dmar_qi_emit(unit, DMAR_IQ_DESCR_CTX_INV | DMAR_IQ_DESCR_CTX_GLOB, 0);
	dmar_qi_emit_wait_seq(unit, &gseq, true);
	/* See dmar_qi_invalidate_sync(). */
	unit->inv_seq_waiters++;
	dmar_qi_advance_tail(unit);
	dmar_qi_wait_for_seq(unit, &gseq, false);
}

void
dmar_qi_invalidate_iotlb_glob_locked(struct dmar_unit *unit)
{
	struct iommu_qi_genseq gseq;

	DMAR_ASSERT_LOCKED(unit);
	dmar_qi_ensure(unit, 2);
	dmar_qi_emit(unit, DMAR_IQ_DESCR_IOTLB_INV | DMAR_IQ_DESCR_IOTLB_GLOB |
	    DMAR_IQ_DESCR_IOTLB_DW | DMAR_IQ_DESCR_IOTLB_DR, 0);
	dmar_qi_emit_wait_seq(unit, &gseq, true);
	/* See dmar_qi_invalidate_sync(). */
	unit->inv_seq_waiters++;
	dmar_qi_advance_tail(unit);
	dmar_qi_wait_for_seq(unit, &gseq, false);
}

void
dmar_qi_invalidate_iec_glob(struct dmar_unit *unit)
{
	struct iommu_qi_genseq gseq;

	DMAR_ASSERT_LOCKED(unit);
	dmar_qi_ensure(unit, 2);
	dmar_qi_emit(unit, DMAR_IQ_DESCR_IEC_INV, 0);
	dmar_qi_emit_wait_seq(unit, &gseq, true);
	/* See dmar_qi_invalidate_sync(). */
	unit->inv_seq_waiters++;
	dmar_qi_advance_tail(unit);
	dmar_qi_wait_for_seq(unit, &gseq, false);
}

void
dmar_qi_invalidate_iec(struct dmar_unit *unit, u_int start, u_int cnt)
{
	struct iommu_qi_genseq gseq;
	u_int c, l;

	DMAR_ASSERT_LOCKED(unit);
	KASSERT(start < unit->irte_cnt && start < start + cnt &&
	    start + cnt <= unit->irte_cnt,
	    ("inv iec overflow %d %d %d", unit->irte_cnt, start, cnt));
	for (; cnt > 0; cnt -= c, start += c) {
		l = ffs(start | cnt) - 1;
		c = 1 << l;
		dmar_qi_ensure(unit, 1);
		dmar_qi_emit(unit, DMAR_IQ_DESCR_IEC_INV |
		    DMAR_IQ_DESCR_IEC_IDX | DMAR_IQ_DESCR_IEC_IIDX(start) |
		    DMAR_IQ_DESCR_IEC_IM(l), 0);
	}
	dmar_qi_ensure(unit, 1);
	dmar_qi_emit_wait_seq(unit, &gseq, true);

	/*
	 * Since dmar_qi_wait_for_seq() will not sleep, this increment's
	 * placement relative to advancing the tail doesn't matter.
	 */
	unit->inv_seq_waiters++;

	dmar_qi_advance_tail(unit);

	/*
	 * The caller of the function, in particular,
	 * dmar_ir_program_irte(), may be called from the context
	 * where the sleeping is forbidden (in fact, the
	 * intr_table_lock mutex may be held, locked from
	 * intr_shuffle_irqs()).  Wait for the invalidation completion
	 * using the busy wait.
	 *
	 * The impact on the interrupt input setup code is small, the
	 * expected overhead is comparable with the chipset register
	 * read.  It is more harmful for the parallel DMA operations,
	 * since we own the dmar unit lock until whole invalidation
	 * queue is processed, which includes requests possibly issued
	 * before our request.
	 */
	dmar_qi_wait_for_seq(unit, &gseq, true);
}

int
dmar_qi_intr(void *arg)
{
	struct dmar_unit *unit;

	unit = arg;
	KASSERT(unit->qi_enabled, ("dmar%d: QI is not enabled",
	    unit->iommu.unit));
	taskqueue_enqueue(unit->qi_taskqueue, &unit->qi_task);
	return (FILTER_HANDLED);
}

static void
dmar_qi_drain_tlb_flush(struct dmar_unit *unit)
{
	struct iommu_map_entry *entry, *head;

	for (head = unit->tlb_flush_head;; head = entry) {
		entry = (struct iommu_map_entry *)
		    atomic_load_acq_ptr((uintptr_t *)&head->tlb_flush_next);
		if (entry == NULL ||
		    !dmar_qi_seq_processed(unit, &entry->gseq))
			break;
		unit->tlb_flush_head = entry;
		iommu_gas_free_entry(head);
		if ((entry->flags & IOMMU_MAP_ENTRY_RMRR) != 0)
			iommu_gas_free_region(entry);
		else
			iommu_gas_free_space(entry);
	}
}

static void
dmar_qi_task(void *arg, int pending __unused)
{
	struct dmar_unit *unit;
	uint32_t ics;

	unit = arg;
	dmar_qi_drain_tlb_flush(unit);

	/*
	 * Request an interrupt on the completion of the next invalidation
	 * wait descriptor with the IF field set.
	 */
	ics = dmar_read4(unit, DMAR_ICS_REG);
	if ((ics & DMAR_ICS_IWC) != 0) {
		ics = DMAR_ICS_IWC;
		dmar_write4(unit, DMAR_ICS_REG, ics);

		/*
		 * Drain a second time in case the DMAR processes an entry
		 * after the first call and before clearing DMAR_ICS_IWC.
		 * Otherwise, such entries will linger until a later entry
		 * that requests an interrupt is processed.
		 */
		dmar_qi_drain_tlb_flush(unit);
	}

	if (unit->inv_seq_waiters > 0) {
		/*
		 * Acquire the DMAR lock so that wakeup() is called only after
		 * the waiter is sleeping.
		 */
		DMAR_LOCK(unit);
		wakeup(&unit->inv_seq_waiters);
		DMAR_UNLOCK(unit);
	}
}

int
dmar_init_qi(struct dmar_unit *unit)
{
	uint64_t iqa;
	uint32_t ics;
	int qi_sz;

	if (!DMAR_HAS_QI(unit) || (unit->hw_cap & DMAR_CAP_CM) != 0)
		return (0);
	unit->qi_enabled = 1;
	TUNABLE_INT_FETCH("hw.dmar.qi", &unit->qi_enabled);
	if (!unit->qi_enabled)
		return (0);

	unit->tlb_flush_head = unit->tlb_flush_tail =
            iommu_gas_alloc_entry(NULL, 0);
	TASK_INIT(&unit->qi_task, 0, dmar_qi_task, unit);
	unit->qi_taskqueue = taskqueue_create_fast("dmarqf", M_WAITOK,
	    taskqueue_thread_enqueue, &unit->qi_taskqueue);
	taskqueue_start_threads(&unit->qi_taskqueue, 1, PI_AV,
	    "dmar%d qi taskq", unit->iommu.unit);

	unit->inv_waitd_gen = 0;
	unit->inv_waitd_seq = 1;

	qi_sz = DMAR_IQA_QS_DEF;
	TUNABLE_INT_FETCH("hw.dmar.qi_size", &qi_sz);
	if (qi_sz > DMAR_IQA_QS_MAX)
		qi_sz = DMAR_IQA_QS_MAX;
	unit->inv_queue_size = (1ULL << qi_sz) * PAGE_SIZE;
	/* Reserve one descriptor to prevent wraparound. */
	unit->inv_queue_avail = unit->inv_queue_size - DMAR_IQ_DESCR_SZ;

	/* The invalidation queue reads by DMARs are always coherent. */
	unit->inv_queue = kmem_alloc_contig(unit->inv_queue_size, M_WAITOK |
	    M_ZERO, 0, dmar_high, PAGE_SIZE, 0, VM_MEMATTR_DEFAULT);
	unit->inv_waitd_seq_hw_phys = pmap_kextract(
	    (vm_offset_t)&unit->inv_waitd_seq_hw);

	DMAR_LOCK(unit);
	dmar_write8(unit, DMAR_IQT_REG, 0);
	iqa = pmap_kextract((uintptr_t)unit->inv_queue);
	iqa |= qi_sz;
	dmar_write8(unit, DMAR_IQA_REG, iqa);
	dmar_enable_qi(unit);
	ics = dmar_read4(unit, DMAR_ICS_REG);
	if ((ics & DMAR_ICS_IWC) != 0) {
		ics = DMAR_ICS_IWC;
		dmar_write4(unit, DMAR_ICS_REG, ics);
	}
	dmar_enable_qi_intr(unit);
	DMAR_UNLOCK(unit);

	return (0);
}

void
dmar_fini_qi(struct dmar_unit *unit)
{
	struct iommu_qi_genseq gseq;

	if (!unit->qi_enabled)
		return;
	taskqueue_drain(unit->qi_taskqueue, &unit->qi_task);
	taskqueue_free(unit->qi_taskqueue);
	unit->qi_taskqueue = NULL;

	DMAR_LOCK(unit);
	/* quisce */
	dmar_qi_ensure(unit, 1);
	dmar_qi_emit_wait_seq(unit, &gseq, true);
	/* See dmar_qi_invalidate_sync_locked(). */
	unit->inv_seq_waiters++;
	dmar_qi_advance_tail(unit);
	dmar_qi_wait_for_seq(unit, &gseq, false);
	/* only after the quisce, disable queue */
	dmar_disable_qi_intr(unit);
	dmar_disable_qi(unit);
	KASSERT(unit->inv_seq_waiters == 0,
	    ("dmar%d: waiters on disabled queue", unit->iommu.unit));
	DMAR_UNLOCK(unit);

	kmem_free(unit->inv_queue, unit->inv_queue_size);
	unit->inv_queue = NULL;
	unit->inv_queue_size = 0;
	unit->qi_enabled = 0;
}

void
dmar_enable_qi_intr(struct dmar_unit *unit)
{
	uint32_t iectl;

	DMAR_ASSERT_LOCKED(unit);
	KASSERT(DMAR_HAS_QI(unit), ("dmar%d: QI is not supported",
	    unit->iommu.unit));
	iectl = dmar_read4(unit, DMAR_IECTL_REG);
	iectl &= ~DMAR_IECTL_IM;
	dmar_write4(unit, DMAR_IECTL_REG, iectl);
}

void
dmar_disable_qi_intr(struct dmar_unit *unit)
{
	uint32_t iectl;

	DMAR_ASSERT_LOCKED(unit);
	KASSERT(DMAR_HAS_QI(unit), ("dmar%d: QI is not supported",
	    unit->iommu.unit));
	iectl = dmar_read4(unit, DMAR_IECTL_REG);
	dmar_write4(unit, DMAR_IECTL_REG, iectl | DMAR_IECTL_IM);
}
