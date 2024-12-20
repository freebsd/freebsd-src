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
#include <x86/iommu/x86_iommu.h>
#include <x86/iommu/intel_dmar.h>

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
dmar_qi_advance_tail(struct iommu_unit *iommu)
{
	struct dmar_unit *unit;

	unit = IOMMU2DMAR(iommu);
	DMAR_ASSERT_LOCKED(unit);
	dmar_write4(unit, DMAR_IQT_REG, unit->x86c.inv_queue_tail);
}

static void
dmar_qi_ensure(struct iommu_unit *iommu, int descr_count)
{
	struct dmar_unit *unit;
	uint32_t head;
	int bytes;

	unit = IOMMU2DMAR(iommu);
	DMAR_ASSERT_LOCKED(unit);
	bytes = descr_count << DMAR_IQ_DESCR_SZ_SHIFT;
	for (;;) {
		if (bytes <= unit->x86c.inv_queue_avail)
			break;
		/* refill */
		head = dmar_read4(unit, DMAR_IQH_REG);
		head &= DMAR_IQH_MASK;
		unit->x86c.inv_queue_avail = head - unit->x86c.inv_queue_tail -
		    DMAR_IQ_DESCR_SZ;
		if (head <= unit->x86c.inv_queue_tail)
			unit->x86c.inv_queue_avail += unit->x86c.inv_queue_size;
		if (bytes <= unit->x86c.inv_queue_avail)
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
		dmar_qi_advance_tail(DMAR2IOMMU(unit));
		unit->x86c.inv_queue_full++;
		cpu_spinwait();
	}
	unit->x86c.inv_queue_avail -= bytes;
}

static void
dmar_qi_emit(struct dmar_unit *unit, uint64_t data1, uint64_t data2)
{

	DMAR_ASSERT_LOCKED(unit);
#ifdef __LP64__
	atomic_store_64((uint64_t *)(unit->x86c.inv_queue +
	    unit->x86c.inv_queue_tail), data1);
#else
	*(volatile uint64_t *)(unit->x86c.inv_queue +
	    unit->x86c.inv_queue_tail) = data1;
#endif
	unit->x86c.inv_queue_tail += DMAR_IQ_DESCR_SZ / 2;
	KASSERT(unit->x86c.inv_queue_tail <= unit->x86c.inv_queue_size,
	    ("tail overflow 0x%x 0x%jx", unit->x86c.inv_queue_tail,
	    (uintmax_t)unit->x86c.inv_queue_size));
	unit->x86c.inv_queue_tail &= unit->x86c.inv_queue_size - 1;
#ifdef __LP64__
	atomic_store_64((uint64_t *)(unit->x86c.inv_queue +
	    unit->x86c.inv_queue_tail), data2);
#else
	*(volatile uint64_t *)(unit->x86c.inv_queue +
	    unit->x86c.inv_queue_tail) = data2;
#endif
	unit->x86c.inv_queue_tail += DMAR_IQ_DESCR_SZ / 2;
	KASSERT(unit->x86c.inv_queue_tail <= unit->x86c.inv_queue_size,
	    ("tail overflow 0x%x 0x%jx", unit->x86c.inv_queue_tail,
	    (uintmax_t)unit->x86c.inv_queue_size));
	unit->x86c.inv_queue_tail &= unit->x86c.inv_queue_size - 1;
}

static void
dmar_qi_emit_wait_descr(struct iommu_unit *iommu, uint32_t seq, bool intr,
    bool memw, bool fence)
{
	struct dmar_unit *unit;

	unit = IOMMU2DMAR(iommu);
	DMAR_ASSERT_LOCKED(unit);
	dmar_qi_emit(unit, DMAR_IQ_DESCR_WAIT_ID |
	    (intr ? DMAR_IQ_DESCR_WAIT_IF : 0) |
	    (memw ? DMAR_IQ_DESCR_WAIT_SW : 0) |
	    (fence ? DMAR_IQ_DESCR_WAIT_FN : 0) |
	    (memw ? DMAR_IQ_DESCR_WAIT_SD(seq) : 0),
	    memw ? unit->x86c.inv_waitd_seq_hw_phys : 0);
}

static void
dmar_qi_invalidate_emit(struct iommu_domain *idomain, iommu_gaddr_t base,
    iommu_gaddr_t size, struct iommu_qi_genseq *pseq, bool emit_wait)
{
	struct dmar_unit *unit;
	struct dmar_domain *domain;
	iommu_gaddr_t isize;
	int am;

	domain = __containerof(idomain, struct dmar_domain, iodom);
	unit = domain->dmar;
	DMAR_ASSERT_LOCKED(unit);
	for (; size > 0; base += isize, size -= isize) {
		am = calc_am(unit, base, size, &isize);
		dmar_qi_ensure(DMAR2IOMMU(unit), 1);
		dmar_qi_emit(unit, DMAR_IQ_DESCR_IOTLB_INV |
		    DMAR_IQ_DESCR_IOTLB_PAGE | DMAR_IQ_DESCR_IOTLB_DW |
		    DMAR_IQ_DESCR_IOTLB_DR |
		    DMAR_IQ_DESCR_IOTLB_DID(domain->domain),
		    base | am);
	}
	iommu_qi_emit_wait_seq(DMAR2IOMMU(unit), pseq, emit_wait);
}

static void
dmar_qi_invalidate_glob_impl(struct dmar_unit *unit, uint64_t data1)
{
	struct iommu_qi_genseq gseq;

	DMAR_ASSERT_LOCKED(unit);
	dmar_qi_ensure(DMAR2IOMMU(unit), 2);
	dmar_qi_emit(unit, data1, 0);
	iommu_qi_emit_wait_seq(DMAR2IOMMU(unit), &gseq, true);
	/* See dmar_qi_invalidate_sync(). */
	unit->x86c.inv_seq_waiters++;
	dmar_qi_advance_tail(DMAR2IOMMU(unit));
	iommu_qi_wait_for_seq(DMAR2IOMMU(unit), &gseq, false);
}

void
dmar_qi_invalidate_ctx_glob_locked(struct dmar_unit *unit)
{
	dmar_qi_invalidate_glob_impl(unit, DMAR_IQ_DESCR_CTX_INV |
	    DMAR_IQ_DESCR_CTX_GLOB);
}

void
dmar_qi_invalidate_iotlb_glob_locked(struct dmar_unit *unit)
{
	dmar_qi_invalidate_glob_impl(unit, DMAR_IQ_DESCR_IOTLB_INV |
	    DMAR_IQ_DESCR_IOTLB_GLOB | DMAR_IQ_DESCR_IOTLB_DW |
	    DMAR_IQ_DESCR_IOTLB_DR);
}

void
dmar_qi_invalidate_iec_glob(struct dmar_unit *unit)
{
	dmar_qi_invalidate_glob_impl(unit, DMAR_IQ_DESCR_IEC_INV);
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
		dmar_qi_ensure(DMAR2IOMMU(unit), 1);
		dmar_qi_emit(unit, DMAR_IQ_DESCR_IEC_INV |
		    DMAR_IQ_DESCR_IEC_IDX | DMAR_IQ_DESCR_IEC_IIDX(start) |
		    DMAR_IQ_DESCR_IEC_IM(l), 0);
	}
	dmar_qi_ensure(DMAR2IOMMU(unit), 1);
	iommu_qi_emit_wait_seq(DMAR2IOMMU(unit), &gseq, true);

	/*
	 * Since iommu_qi_wait_for_seq() will not sleep, this increment's
	 * placement relative to advancing the tail doesn't matter.
	 */
	unit->x86c.inv_seq_waiters++;

	dmar_qi_advance_tail(DMAR2IOMMU(unit));

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
	iommu_qi_wait_for_seq(DMAR2IOMMU(unit), &gseq, true);
}

int
dmar_qi_intr(void *arg)
{
	struct dmar_unit *unit;

	unit = IOMMU2DMAR((struct iommu_unit *)arg);
	KASSERT(unit->qi_enabled, ("dmar%d: QI is not enabled",
	    unit->iommu.unit));
	taskqueue_enqueue(unit->x86c.qi_taskqueue, &unit->x86c.qi_task);
	return (FILTER_HANDLED);
}

static void
dmar_qi_task(void *arg, int pending __unused)
{
	struct dmar_unit *unit;
	uint32_t ics;

	unit = IOMMU2DMAR(arg);
	iommu_qi_drain_tlb_flush(DMAR2IOMMU(unit));

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
		iommu_qi_drain_tlb_flush(DMAR2IOMMU(unit));
	}

	if (unit->x86c.inv_seq_waiters > 0) {
		/*
		 * Acquire the DMAR lock so that wakeup() is called only after
		 * the waiter is sleeping.
		 */
		DMAR_LOCK(unit);
		wakeup(&unit->x86c.inv_seq_waiters);
		DMAR_UNLOCK(unit);
	}
}

int
dmar_init_qi(struct dmar_unit *unit)
{
	uint64_t iqa;
	uint32_t ics;
	u_int qi_sz;

	if (!DMAR_HAS_QI(unit) || (unit->hw_cap & DMAR_CAP_CM) != 0)
		return (0);
	unit->qi_enabled = 1;
	TUNABLE_INT_FETCH("hw.dmar.qi", &unit->qi_enabled);
	if (!unit->qi_enabled)
		return (0);

	unit->x86c.qi_buf_maxsz = DMAR_IQA_QS_MAX;
	unit->x86c.qi_cmd_sz = DMAR_IQ_DESCR_SZ;
	iommu_qi_common_init(DMAR2IOMMU(unit), dmar_qi_task);
	get_x86_iommu()->qi_ensure = dmar_qi_ensure;
	get_x86_iommu()->qi_emit_wait_descr = dmar_qi_emit_wait_descr;
	get_x86_iommu()->qi_advance_tail = dmar_qi_advance_tail;
	get_x86_iommu()->qi_invalidate_emit = dmar_qi_invalidate_emit;

	qi_sz = ilog2(unit->x86c.inv_queue_size / PAGE_SIZE);

	DMAR_LOCK(unit);
	dmar_write8(unit, DMAR_IQT_REG, 0);
	iqa = pmap_kextract((uintptr_t)unit->x86c.inv_queue);
	iqa |= qi_sz;
	dmar_write8(unit, DMAR_IQA_REG, iqa);
	dmar_enable_qi(unit);
	ics = dmar_read4(unit, DMAR_ICS_REG);
	if ((ics & DMAR_ICS_IWC) != 0) {
		ics = DMAR_ICS_IWC;
		dmar_write4(unit, DMAR_ICS_REG, ics);
	}
	dmar_enable_qi_intr(DMAR2IOMMU(unit));
	DMAR_UNLOCK(unit);

	return (0);
}

static void
dmar_fini_qi_helper(struct iommu_unit *iommu)
{
	dmar_disable_qi_intr(iommu);
	dmar_disable_qi(IOMMU2DMAR(iommu));
}

void
dmar_fini_qi(struct dmar_unit *unit)
{
	if (!unit->qi_enabled)
		return;
	iommu_qi_common_fini(DMAR2IOMMU(unit), dmar_fini_qi_helper);
	unit->qi_enabled = 0;
}

void
dmar_enable_qi_intr(struct iommu_unit *iommu)
{
	struct dmar_unit *unit;
	uint32_t iectl;

	unit = IOMMU2DMAR(iommu);
	DMAR_ASSERT_LOCKED(unit);
	KASSERT(DMAR_HAS_QI(unit), ("dmar%d: QI is not supported",
	    unit->iommu.unit));
	iectl = dmar_read4(unit, DMAR_IECTL_REG);
	iectl &= ~DMAR_IECTL_IM;
	dmar_write4(unit, DMAR_IECTL_REG, iectl);
}

void
dmar_disable_qi_intr(struct iommu_unit *iommu)
{
	struct dmar_unit *unit;
	uint32_t iectl;

	unit = IOMMU2DMAR(iommu);
	DMAR_ASSERT_LOCKED(unit);
	KASSERT(DMAR_HAS_QI(unit), ("dmar%d: QI is not supported",
	    unit->iommu.unit));
	iectl = dmar_read4(unit, DMAR_IECTL_REG);
	dmar_write4(unit, DMAR_IECTL_REG, iectl | DMAR_IECTL_IM);
}
