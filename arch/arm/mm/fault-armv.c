/*
 *  linux/arch/arm/mm/fault-armv.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Modifications for ARM processor (c) 1995-2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/config.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/bitops.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>

extern void show_pte(struct mm_struct *mm, unsigned long addr);
extern int do_page_fault(unsigned long addr, int error_code,
			 struct pt_regs *regs);
extern int do_translation_fault(unsigned long addr, int error_code,
				struct pt_regs *regs);
extern void do_bad_area(struct task_struct *tsk, struct mm_struct *mm,
			unsigned long addr, int error_code,
			struct pt_regs *regs);

#ifdef CONFIG_ALIGNMENT_TRAP
extern int do_alignment(unsigned long addr, int error_code, struct pt_regs *regs);
#else
#define do_alignment do_bad
#endif


/*
 * Some section permission faults need to be handled gracefully.
 * They can happen due to a __{get,put}_user during an oops.
 */
static int
do_sect_fault(unsigned long addr, int error_code, struct pt_regs *regs)
{
	struct task_struct *tsk = current;
	do_bad_area(tsk, tsk->active_mm, addr, error_code, regs);
	return 0;
}

/*
 * Hook for things that need to trap external faults.  Note that
 * we don't guarantee that this will be the final version of the
 * interface.
 */
int (*external_fault)(unsigned long addr, struct pt_regs *regs);

static int
do_external_fault(unsigned long addr, int error_code, struct pt_regs *regs)
{
	if (external_fault)
		return external_fault(addr, regs);
	return 1;
}

/*
 * This abort handler always returns "fault".
 */
static int
do_bad(unsigned long addr, int error_code, struct pt_regs *regs)
{
	return 1;
}

static const struct fsr_info {
	int	(*fn)(unsigned long addr, int error_code, struct pt_regs *regs);
	int	sig;
	const char *name;
} fsr_info[] = {
	{ do_bad,		SIGSEGV, "vector exception"		   },
	{ do_alignment,		SIGILL,	 "alignment exception"		   },
	{ do_bad,		SIGKILL, "terminal exception"		   },
	{ do_alignment,		SIGILL,	 "alignment exception"		   },
	{ do_external_fault,	SIGBUS,	 "external abort on linefetch"	   },
	{ do_translation_fault,	SIGSEGV, "section translation fault"	   },
	{ do_external_fault,	SIGBUS,	 "external abort on linefetch"	   },
	{ do_page_fault,	SIGSEGV, "page translation fault"	   },
	{ do_external_fault,	SIGBUS,	 "external abort on non-linefetch" },
	{ do_bad,		SIGSEGV, "section domain fault"		   },
	{ do_external_fault,	SIGBUS,	 "external abort on non-linefetch" },
	{ do_bad,		SIGSEGV, "page domain fault"		   },
	{ do_bad,		SIGBUS,	 "external abort on translation"   },
	{ do_sect_fault,	SIGSEGV, "section permission fault"	   },
	{ do_bad,		SIGBUS,	 "external abort on translation"   },
	{ do_page_fault,	SIGSEGV, "page permission fault"	   }
};

/*
 * Dispatch a data abort to the relevant handler.
 */
asmlinkage void
do_DataAbort(unsigned long addr, int error_code, struct pt_regs *regs, int fsr)
{
	const struct fsr_info *inf = fsr_info + (fsr & 15);

	if (!inf->fn(addr, error_code, regs))
		return;

	printk(KERN_ALERT "Unhandled fault: %s (0x%03x) at 0x%08lx\n",
		inf->name, fsr, addr);
	force_sig(inf->sig, current);
	show_pte(current->mm, addr);
	die_if_kernel("Oops", regs, 0);
}

asmlinkage void
do_PrefetchAbort(unsigned long addr, struct pt_regs *regs)
{
	do_translation_fault(addr, 0, regs);
}

/*
 * We take the easy way out of this problem - we make the
 * PTE uncacheable.  However, we leave the write buffer on.
 */
static void adjust_pte(struct vm_area_struct *vma, unsigned long address)
{
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte, entry;

	pgd = pgd_offset(vma->vm_mm, address);
	if (pgd_none(*pgd))
		return;
	if (pgd_bad(*pgd))
		goto bad_pgd;

	pmd = pmd_offset(pgd, address);
	if (pmd_none(*pmd))
		return;
	if (pmd_bad(*pmd))
		goto bad_pmd;

	pte = pte_offset(pmd, address);
	entry = *pte;

	/*
	 * If this page isn't present, or is already setup to
	 * fault (ie, is old), we can safely ignore any issues.
	 */
	if (pte_present(entry) && pte_val(entry) & L_PTE_CACHEABLE) {
		flush_cache_page(vma, address);
		pte_val(entry) &= ~L_PTE_CACHEABLE;
		set_pte(pte, entry);
		flush_tlb_page(vma, address);
	}
	return;

bad_pgd:
	pgd_ERROR(*pgd);
	pgd_clear(pgd);
	return;

bad_pmd:
	pmd_ERROR(*pmd);
	pmd_clear(pmd);
	return;
}

static void
make_coherent(struct vm_area_struct *vma, unsigned long addr, struct page *page)
{
	struct vm_area_struct *mpnt;
	struct mm_struct *mm = vma->vm_mm;
	unsigned long pgoff = (addr - vma->vm_start) >> PAGE_SHIFT;
	int aliases = 0;

	/*
	 * If we have any shared mappings that are in the same mm
	 * space, then we need to handle them specially to maintain
	 * cache coherency.
	 */
	for (mpnt = page->mapping->i_mmap_shared; mpnt;
	     mpnt = mpnt->vm_next_share) {
		unsigned long off;

		/*
		 * If this VMA is not in our MM, we can ignore it.
		 * Note that we intentionally don't mask out the VMA
		 * that we are fixing up.
		 */
		if (mpnt->vm_mm != mm || mpnt == vma)
			continue;

		/*
		 * If the page isn't in this VMA, we can also ignore it.
		 */
		if (pgoff < mpnt->vm_pgoff)
			continue;

		off = pgoff - mpnt->vm_pgoff;
		if (off >= (mpnt->vm_end - mpnt->vm_start) >> PAGE_SHIFT)
			continue;

		/*
		 * Ok, it is within mpnt.  Fix it up.
		 */
		adjust_pte(mpnt, mpnt->vm_start + (off << PAGE_SHIFT));
		aliases ++;
	}
	if (aliases)
		adjust_pte(vma, addr);
}

/*
 * Take care of architecture specific things when placing a new PTE into
 * a page table, or changing an existing PTE.  Basically, there are two
 * things that we need to take care of:
 *
 *  1. If PG_dcache_dirty is set for the page, we need to ensure
 *     that any cache entries for the kernels virtual memory
 *     range are written back to the page.
 *  2. If we have multiple shared mappings of the same space in
 *     an object, we need to deal with the cache aliasing issues.
 *
 * Note that the page_table_lock will be held.
 */
void update_mmu_cache(struct vm_area_struct *vma, unsigned long addr, pte_t pte)
{
	unsigned long pfn = pte_pfn(pte);
	struct page *page;

	if (!pfn_valid(pfn))
		return;
	page = pfn_to_page(pfn);
	if (page->mapping) {
		if (test_and_clear_bit(PG_dcache_dirty, &page->flags)) {
			unsigned long kvirt = (unsigned long)page_address(page);
			cpu_cache_clean_invalidate_range(kvirt, kvirt + PAGE_SIZE, 0);
		}

		make_coherent(vma, addr, page);
	}
}
