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

#ifndef	_MACHINE_TLB_H_
#define	_MACHINE_TLB_H_

#define	TLB_SLOT_COUNT			64	/* XXX */

#define	TLB_SLOT_TSB_KERNEL_MIN		62	/* XXX */
#define	TLB_SLOT_KERNEL			63	/* XXX */

#define	TLB_DAR_SLOT_SHIFT		(3)
#define	TLB_DAR_SLOT(slot)		((slot) << TLB_DAR_SLOT_SHIFT)

#define	TAR_VPN_SHIFT			(13)
#define	TAR_CTX_MASK			((1 << TAR_VPN_SHIFT) - 1)

#define	TLB_TAR_VA(va)			((va) & ~TAR_CTX_MASK)
#define	TLB_TAR_CTX(ctx)		((ctx) & TAR_CTX_MASK)

#define	TLB_DEMAP_ID_SHIFT		(4)
#define	TLB_DEMAP_ID_PRIMARY		(0)
#define	TLB_DEMAP_ID_SECONDARY		(1)
#define	TLB_DEMAP_ID_NUCLEUS		(2)

#define	TLB_DEMAP_TYPE_SHIFT		(6)
#define	TLB_DEMAP_TYPE_PAGE		(0)
#define	TLB_DEMAP_TYPE_CONTEXT		(1)

#define	TLB_DEMAP_VA(va)		((va) & ~PAGE_MASK)
#define	TLB_DEMAP_ID(id)		((id) << TLB_DEMAP_ID_SHIFT)
#define	TLB_DEMAP_TYPE(type)		((type) << TLB_DEMAP_TYPE_SHIFT)

#define	TLB_DEMAP_PAGE			(TLB_DEMAP_TYPE(TLB_DEMAP_TYPE_PAGE))
#define	TLB_DEMAP_CONTEXT		(TLB_DEMAP_TYPE(TLB_DEMAP_TYPE_CONTEXT))

#define	TLB_DEMAP_PRIMARY		(TLB_DEMAP_ID(TLB_DEMAP_ID_PRIMARY))
#define	TLB_DEMAP_SECONDARY		(TLB_DEMAP_ID(TLB_DEMAP_ID_SECONDARY))
#define	TLB_DEMAP_NUCLEUS		(TLB_DEMAP_ID(TLB_DEMAP_ID_NUCLEUS))

#define	TLB_CTX_KERNEL			(0)
#define	TLB_CTX_USER_MIN		(1)
#define	TLB_CTX_USER_MAX		(8192)

#define	TLB_DTLB			(1 << 0)
#define	TLB_ITLB			(1 << 1)

#define	MMU_SFSR_ASI_SHIFT		(16)
#define	MMU_SFSR_FT_SHIFT		(7)
#define	MMU_SFSR_E_SHIFT		(6)
#define	MMU_SFSR_CT_SHIFT		(4)
#define	MMU_SFSR_PR_SHIFT		(3)
#define	MMU_SFSR_W_SHIFT		(2)
#define	MMU_SFSR_OW_SHIFT		(1)
#define	MMU_SFSR_FV_SHIFT		(0)

#define	MMU_SFSR_ASI_SIZE		(8)
#define	MMU_SFSR_FT_SIZE		(6)
#define	MMU_SFSR_CT_SIZE		(2)

#define	MMU_SFSR_W			(1L << MMU_SFSR_W_SHIFT)

extern int kernel_tlb_slots;
extern struct tte *kernel_ttes;

/*
 * Some tlb operations must be atomic, so no interrupt or trap can be allowed
 * while they are in progress. Traps should not happen, but interrupts need to
 * be explicitely disabled. critical_enter() cannot be used here, since it only
 * disables soft interrupts.
 */

static __inline void
tlb_context_demap(struct pmap *pm)
{
	void *cookie;
	u_long s;

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
}

static __inline void
tlb_page_demap(u_int tlb, struct pmap *pm, vm_offset_t va)
{
	u_long flags;
	void *cookie;
	u_long s;

	cookie = ipi_tlb_page_demap(tlb, pm, va);
	if (pm->pm_active & PCPU_GET(cpumask)) {
		KASSERT(pm->pm_context[PCPU_GET(cpuid)] != -1,
		    ("tlb_page_demap: inactive pmap?"));
		if (pm == kernel_pmap)
			flags = TLB_DEMAP_NUCLEUS | TLB_DEMAP_PAGE;
		else
			flags = TLB_DEMAP_PRIMARY | TLB_DEMAP_PAGE;
	
		s = intr_disable();
		if (tlb & TLB_DTLB) {
			stxa(TLB_DEMAP_VA(va) | flags, ASI_DMMU_DEMAP, 0);
			membar(Sync);
		}
		if (tlb & TLB_ITLB) {
			stxa(TLB_DEMAP_VA(va) | flags, ASI_IMMU_DEMAP, 0);
			membar(Sync);
		}
		intr_restore(s);
	}
	ipi_wait(cookie);
}

static __inline void
tlb_range_demap(struct pmap *pm, vm_offset_t start, vm_offset_t end)
{
	vm_offset_t va;
	void *cookie;
	u_long flags;
	u_long s;

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
}

#define	tlb_tte_demap(tte, pm) \
	tlb_page_demap(TD_GET_TLB((tte).tte_data), pm, \
	    TV_GET_VA((tte).tte_vpn));

#endif /* !_MACHINE_TLB_H_ */
