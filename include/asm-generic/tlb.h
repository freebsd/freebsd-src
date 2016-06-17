/* asm-generic/tlb.h
 *
 *	Generic TLB shootdown code
 *
 * Copyright 2001 Red Hat, Inc.
 * Based on code from mm/memory.c Copyright Linus Torvalds and others.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef _ASM_GENERIC__TLB_H
#define _ASM_GENERIC__TLB_H

#include <linux/config.h>

#ifdef CONFIG_SMP
/* aim for something that fits in the L1 cache */
#define FREE_PTE_NR	508

/* mmu_gather_t is an opaque type used by the mm code for passing around any
 * data needed by arch specific code for tlb_remove_page.  This structure can
 * be per-CPU or per-MM as the page table lock is held for the duration of TLB
 * shootdown.
 */
typedef struct free_pte_ctx {
	struct mm_struct	*mm;
	unsigned long		nr;	/* set to ~0UL means fast mode */
	unsigned long	start_addr, end_addr;
	pte_t	ptes[FREE_PTE_NR];
} mmu_gather_t;

/* Users of the generic TLB shootdown code must declare this storage space. */
extern mmu_gather_t	mmu_gathers[NR_CPUS];

/* tlb_gather_mmu
 *	Return a pointer to an initialized mmu_gather_t.
 */
static inline mmu_gather_t *tlb_gather_mmu(struct mm_struct *mm)
{
	mmu_gather_t *tlb = &mmu_gathers[smp_processor_id()];

	tlb->mm = mm;
	/* Use fast mode if there is only one user of this mm (this process) */
	tlb->nr = (atomic_read(&(mm)->mm_users) == 1) ? ~0UL : 0UL;
	return tlb;
}

/* void tlb_remove_page(mmu_gather_t *tlb, pte_t *ptep, unsigned long addr)
 *	Must perform the equivalent to __free_pte(pte_get_and_clear(ptep)), while
 *	handling the additional races in SMP caused by other CPUs caching valid
 *	mappings in their TLBs.
 */
#define tlb_remove_page(ctxp, pte, addr) do {\
		/* Handle the common case fast, first. */\
		if ((ctxp)->nr == ~0UL) {\
			pte_t __pte = *(pte);\
			pte_clear(pte);\
			__free_pte(__pte);\
			break;\
		}\
		if (!(ctxp)->nr) \
			(ctxp)->start_addr = (addr);\
		(ctxp)->ptes[(ctxp)->nr++] = ptep_get_and_clear(pte);\
		(ctxp)->end_addr = (addr) + PAGE_SIZE;\
		if ((ctxp)->nr >= FREE_PTE_NR)\
			tlb_finish_mmu((ctxp), 0, 0);\
	} while (0)

/* tlb_finish_mmu
 *	Called at the end of the shootdown operation to free up any resources
 *	that were required.  The page talbe lock is still held at this point.
 */
static inline void tlb_finish_mmu(struct free_pte_ctx *ctx, unsigned long start, unsigned long end)
{
	unsigned long i, nr;

	/* Handle the fast case first. */
	if (ctx->nr == ~0UL) {
		flush_tlb_range(ctx->mm, start, end);
		return;
	}
	nr = ctx->nr;
	ctx->nr = 0;
	if (nr)
		flush_tlb_range(ctx->mm, ctx->start_addr, ctx->end_addr);
	for (i=0; i < nr; i++) {
		pte_t pte = ctx->ptes[i];
		__free_pte(pte);
	}
}

#else

/* The uniprocessor functions are quite simple and are inline macros in an
 * attempt to get gcc to generate optimal code since this code is run on each
 * page in a process at exit.
 */
typedef struct mm_struct mmu_gather_t;

#define tlb_gather_mmu(mm)	(mm)
#define tlb_finish_mmu(tlb, start, end)	flush_tlb_range(tlb, start, end)
#define tlb_remove_page(tlb, ptep, addr)	do {\
		pte_t __pte = *(ptep);\
		pte_clear(ptep);\
		__free_pte(__pte);\
	} while (0)

#endif


#endif /* _ASM_GENERIC__TLB_H */

