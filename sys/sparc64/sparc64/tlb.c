/*-
 * Copyright (c) 2001 Jake Burkholder.
 * All rights reserved.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/pcpu.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/pmap.h>
#include <machine/smp.h>
#include <machine/tlb.h>

int tlb_slot_count;

/*
 * Some tlb operations must be atomic, so no interrupt or trap can be allowed
 * while they are in progress. Traps should not happen, but interrupts need to
 * be explicitely disabled. critical_enter() cannot be used here, since it only
 * disables soft interrupts.
 */

void
tlb_context_demap(struct pmap *pm)
{
	void *cookie;
	u_long s;

	/*
	 * It is important that we are not interrupted or preempted while
	 * doing the IPIs. The interrupted CPU may hold locks, and since
	 * it will wait for the CPU that sent the IPI, this can lead
	 * to a deadlock when an interrupt comes in on that CPU and it's
	 * handler tries to grab one of that locks. This will only happen for
	 * spin locks, but these IPI types are delivered even if normal
	 * interrupts are disabled, so the lock critical section will not
	 * protect the target processor from entering the IPI handler with
	 * the lock held.
	 */
	critical_enter();
	cookie = ipi_tlb_context_demap(pm);
	if (pm->pm_active & PCPU_GET(cpumask)) {
		KASSERT(pm->pm_context[PCPU_GET(cpuid)] != -1,
		    ("tlb_context_demap: inactive pmap?"));
		s = intr_disable();
		stxa(TLB_DEMAP_PRIMARY | TLB_DEMAP_CONTEXT, ASI_DMMU_DEMAP, 0);
		stxa(TLB_DEMAP_PRIMARY | TLB_DEMAP_CONTEXT, ASI_IMMU_DEMAP, 0);
		membar(Sync);
		intr_restore(s);
	}
	ipi_wait(cookie);
	critical_exit();
}

void
tlb_page_demap(struct pmap *pm, vm_offset_t va)
{
	u_long flags;
	void *cookie;
	u_long s;

	critical_enter();
	cookie = ipi_tlb_page_demap(pm, va);
	if (pm->pm_active & PCPU_GET(cpumask)) {
		KASSERT(pm->pm_context[PCPU_GET(cpuid)] != -1,
		    ("tlb_page_demap: inactive pmap?"));
		if (pm == kernel_pmap)
			flags = TLB_DEMAP_NUCLEUS | TLB_DEMAP_PAGE;
		else
			flags = TLB_DEMAP_PRIMARY | TLB_DEMAP_PAGE;
	
		s = intr_disable();
		stxa(TLB_DEMAP_VA(va) | flags, ASI_DMMU_DEMAP, 0);
		stxa(TLB_DEMAP_VA(va) | flags, ASI_IMMU_DEMAP, 0);
		membar(Sync);
		intr_restore(s);
	}
	ipi_wait(cookie);
	critical_exit();
}

void
tlb_range_demap(struct pmap *pm, vm_offset_t start, vm_offset_t end)
{
	vm_offset_t va;
	void *cookie;
	u_long flags;
	u_long s;

	critical_enter();
	cookie = ipi_tlb_range_demap(pm, start, end);
	if (pm->pm_active & PCPU_GET(cpumask)) {
		KASSERT(pm->pm_context[PCPU_GET(cpuid)] != -1,
		    ("tlb_range_demap: inactive pmap?"));
		if (pm == kernel_pmap)
			flags = TLB_DEMAP_NUCLEUS | TLB_DEMAP_PAGE;
		else
			flags = TLB_DEMAP_PRIMARY | TLB_DEMAP_PAGE;
	
		s = intr_disable();
		for (va = start; va < end; va += PAGE_SIZE) {
			stxa(TLB_DEMAP_VA(va) | flags, ASI_DMMU_DEMAP, 0);
			stxa(TLB_DEMAP_VA(va) | flags, ASI_IMMU_DEMAP, 0);
			membar(Sync);
		}
		intr_restore(s);
	}
	ipi_wait(cookie);
	critical_exit();
}

void
tlb_dump(void)
{
	u_long data;
	u_long tag;
	int slot;

	for (slot = 0; slot < tlb_slot_count; slot++) {
		data = ldxa(TLB_DAR_SLOT(slot), ASI_DTLB_DATA_ACCESS_REG);
		if ((data & TD_V) != 0) {
			tag = ldxa(TLB_DAR_SLOT(slot), ASI_DTLB_TAG_READ_REG);
			TR3("pmap_dump_tlb: dltb slot=%d data=%#lx tag=%#lx",
			    slot, data, tag);
		}
		data = ldxa(TLB_DAR_SLOT(slot), ASI_ITLB_DATA_ACCESS_REG);
		if ((data & TD_V) != 0) {
			tag = ldxa(TLB_DAR_SLOT(slot), ASI_ITLB_TAG_READ_REG);
			TR3("pmap_dump_tlb: iltb slot=%d data=%#lx tag=%#lx",
			    slot, data, tag);
		}
	}
}
