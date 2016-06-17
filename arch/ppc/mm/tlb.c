/*
 * This file contains the routines for TLB flushing.
 * On machines where the MMU uses a hash table to store virtual to
 * physical translations, these routines flush entries from the
 * hash table also.
 *  -- paulus
 *
 *  Derived from arch/ppc/mm/init.c:
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Modifications by Paul Mackerras (PowerMac) (paulus@cs.anu.edu.au)
 *  and Cort Dougan (PReP) (cort@cs.nmt.edu)
 *    Copyright (C) 1996 Paul Mackerras
 *  Amiga/APUS changes by Jesper Skov (jskov@cygnus.co.uk).
 *
 *  Derived from "arch/i386/mm/init.c"
 *    Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <asm/mmu.h>
#include "mmu_decl.h"

/*
 * TLB flushing:
 *
 *  - flush_tlb_all() flushes all processes TLBs
 *  - flush_tlb_mm(mm) flushes the specified mm context TLB's
 *  - flush_tlb_page(vma, vmaddr) flushes one page
 *  - flush_tlb_range(mm, start, end) flushes a range of pages
 *
 * since the hardware hash table functions as an extension of the
 * tlb as far as the linux tables are concerned, flush it too.
 *    -- Cort
 */

/*
 * Flush all tlb/hash table entries (except perhaps for those
 * mapping RAM starting at PAGE_OFFSET, since they never change).
 */
void
local_flush_tlb_all(void)
{
	/* aargh!!! */
	/*
	 * Just flush the kernel part of the address space, that's
	 * all that the current callers of this require.
	 * Eventually I hope to persuade the powers that be that
	 * we can and should dispense with flush_tlb_all().
	 *  -- paulus.
	 */
	local_flush_tlb_range(&init_mm, TASK_SIZE, ~0UL);

#ifdef CONFIG_SMP
	smp_send_tlb_invalidate(0);
#endif /* CONFIG_SMP */
}

/*
 * Flush all the (user) entries for the address space described
 * by mm.  We can't rely on mm->mmap describing all the entries
 * that might be in the hash table.
 */
void
local_flush_tlb_mm(struct mm_struct *mm)
{
	if (Hash == 0) {
		_tlbia();
		return;
	}

	if (mm->map_count) {
		struct vm_area_struct *mp;
		for (mp = mm->mmap; mp != NULL; mp = mp->vm_next)
			local_flush_tlb_range(mm, mp->vm_start, mp->vm_end);
	} else
		local_flush_tlb_range(mm, 0, TASK_SIZE);

#ifdef CONFIG_SMP
	smp_send_tlb_invalidate(0);
#endif
}

void
local_flush_tlb_page(struct vm_area_struct *vma, unsigned long vmaddr)
{
	struct mm_struct *mm;
	pmd_t *pmd;
	pte_t *pte;

	if (Hash == 0) {
		_tlbie(vmaddr);
		return;
	}
	mm = (vmaddr < TASK_SIZE)? vma->vm_mm: &init_mm;
	pmd = pmd_offset(pgd_offset(mm, vmaddr), vmaddr);
	if (!pmd_none(*pmd)) {
		pte = pte_offset(pmd, vmaddr);
		if (pte_val(*pte) & _PAGE_HASHPTE)
			flush_hash_page(mm->context, vmaddr, pte);
	}
#ifdef CONFIG_SMP
	smp_send_tlb_invalidate(0);
#endif
}


/*
 * For each address in the range, find the pte for the address
 * and check _PAGE_HASHPTE bit; if it is set, find and destroy
 * the corresponding HPTE.
 */
void
local_flush_tlb_range(struct mm_struct *mm, unsigned long start, unsigned long end)
{
	pmd_t *pmd;
	pte_t *pte;
	unsigned long pmd_end;
	unsigned int ctx = mm->context;

	if (Hash == 0) {
		_tlbia();
		return;
	}
	start &= PAGE_MASK;
	if (start >= end)
		return;
	pmd = pmd_offset(pgd_offset(mm, start), start);
	do {
		pmd_end = (start + PGDIR_SIZE) & PGDIR_MASK;
		if (!pmd_none(*pmd)) {
			if (!pmd_end || pmd_end > end)
				pmd_end = end;
			pte = pte_offset(pmd, start);
			do {
				if ((pte_val(*pte) & _PAGE_HASHPTE) != 0)
					flush_hash_page(ctx, start, pte);
				start += PAGE_SIZE;
				++pte;
			} while (start && start < pmd_end);
		} else {
			start = pmd_end;
		}
		++pmd;
	} while (start && start < end);

#ifdef CONFIG_SMP
	smp_send_tlb_invalidate(0);
#endif
}
