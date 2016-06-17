/* $Id: fault.c,v 1.1.1.1.2.3 2002/10/24 05:52:58 mrbrown Exp $
 *
 *  linux/arch/sh/mm/fault.c
 *  Copyright (C) 1999  Niibe Yutaka
 *
 *  Based on linux/arch/i386/mm/fault.c:
 *   Copyright (C) 1995  Linus Torvalds
 */

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/hardirq.h>
#include <asm/mmu_context.h>

#if defined(CONFIG_SH_KGDB)
#include <asm/kgdb.h>
#endif

extern void die(const char *,struct pt_regs *,long);

/*
 * Ugly, ugly, but the goto's result in better assembly..
 */
int __verify_write(const void * addr, unsigned long size)
{
	struct vm_area_struct * vma;
	unsigned long start = (unsigned long) addr;

	if (!size)
		return 1;

	vma = find_vma(current->mm, start);
	if (!vma)
		goto bad_area;
	if (vma->vm_start > start)
		goto check_stack;

good_area:
	if (!(vma->vm_flags & VM_WRITE))
		goto bad_area;
	size--;
	size += start & ~PAGE_MASK;
	size >>= PAGE_SHIFT;
	start &= PAGE_MASK;

	for (;;) {
		if (handle_mm_fault(current->mm, vma, start, 1) <= 0)
			goto bad_area;
		if (!size)
			break;
		size--;
		start += PAGE_SIZE;
		if (start < vma->vm_end)
			continue;
		vma = vma->vm_next;
		if (!vma || vma->vm_start != start)
			goto bad_area;
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;;
	}
	return 1;

check_stack:
	if (!(vma->vm_flags & VM_GROWSDOWN))
		goto bad_area;
	if (expand_stack(vma, start) == 0)
		goto good_area;

bad_area:
	return 0;
}

/*
 * This routine handles page faults.  It determines the address,
 * and the problem, and then passes it off to one of the appropriate
 * routines.
 */
asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long writeaccess,
			      unsigned long address)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	struct vm_area_struct * vma;
	unsigned long page;
	unsigned long fixup;

#if defined(CONFIG_SH_KGDB)
	if (kgdb_nofault && kgdb_bus_err_hook)
	  kgdb_bus_err_hook();
#endif

	tsk = current;
	mm = tsk->mm;

	/*
	 * If we're in an interrupt or have no user
	 * context, we must not take the fault..
	 */
	if (in_interrupt() || !mm)
		goto no_context;

	down_read(&mm->mmap_sem);

	vma = find_vma(mm, address);
	if (!vma)
		goto bad_area;
	if (vma->vm_start <= address)
		goto good_area;
	if (!(vma->vm_flags & VM_GROWSDOWN))
		goto bad_area;
	if (expand_stack(vma, address))
		goto bad_area;
/*
 * Ok, we have a good vm_area for this memory access, so
 * we can handle it..
 */
good_area:
	if (writeaccess) {
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;
	} else {
		if (!(vma->vm_flags & (VM_READ | VM_EXEC)))
			goto bad_area;
	}

	/*
	 * If for any reason at all we couldn't handle the fault,
	 * make sure we exit gracefully rather than endlessly redo
	 * the fault.
	 */
survive:
	switch (handle_mm_fault(mm, vma, address, writeaccess)) {
	case 1:
		tsk->min_flt++;
		break;
	case 2:
		tsk->maj_flt++;
		break;
	case 0:
		goto do_sigbus;
	default:
		goto out_of_memory;
	}

	up_read(&mm->mmap_sem);
	return;

/*
 * Something tried to access memory that isn't in our memory map..
 * Fix it, but check if it's kernel or user first..
 */
bad_area:
	up_read(&mm->mmap_sem);

	if (user_mode(regs)) {
		tsk->thread.address = address;
		tsk->thread.error_code = writeaccess;
		force_sig(SIGSEGV, tsk);
		return;
	}

no_context:
	/* Are we prepared to handle this kernel fault?  */
	fixup = search_exception_table(regs->pc);
	if (fixup != 0) {
		regs->pc = fixup;
		return;
	}

/*
 * Oops. The kernel tried to access some bad page. We'll have to
 * terminate things with extreme prejudice.
 *
 */
	if (address < PAGE_SIZE)
		printk(KERN_ALERT "Unable to handle kernel NULL pointer dereference");
	else
		printk(KERN_ALERT "Unable to handle kernel paging request");
	printk(" at virtual address %08lx\n", address);
	printk(KERN_ALERT "pc = %08lx\n", regs->pc);
	asm volatile("mov.l	%1, %0"
		     : "=r" (page)
		     : "m" (__m(MMU_TTB)));
	if (page) {
		page = ((unsigned long *) page)[address >> 22];
		printk(KERN_ALERT "*pde = %08lx\n", page);
		if (page & _PAGE_PRESENT) {
			page &= PAGE_MASK;
			address &= 0x003ff000;
			page = ((unsigned long *) __va(page))[address >> PAGE_SHIFT];
			printk(KERN_ALERT "*pte = %08lx\n", page);
		}
	}
	die("Oops", regs, writeaccess);
	do_exit(SIGKILL);

/*
 * We ran out of memory, or some other thing happened to us that made
 * us unable to handle the page fault gracefully.
 */
out_of_memory:
	if (current->pid == 1) {
		yield();
		goto survive;
	}
	up_read(&mm->mmap_sem);
	printk("VM: killing process %s\n", tsk->comm);
	if (user_mode(regs))
		do_exit(SIGKILL);
	goto no_context;

do_sigbus:
	up_read(&mm->mmap_sem);

	/*
	 * Send a sigbus, regardless of whether we were in kernel
	 * or user mode.
	 */
	tsk->thread.address = address;
	tsk->thread.error_code = writeaccess;
	tsk->thread.trap_no = 14;
	force_sig(SIGBUS, tsk);

	/* Kernel mode? Handle exceptions or die */
	if (!user_mode(regs))
		goto no_context;
}

/*
 * Called with interrupt disabled.
 */
asmlinkage int __do_page_fault(struct pt_regs *regs, unsigned long writeaccess,
			       unsigned long address)
{
	pgd_t *dir;
	pmd_t *pmd;
	pte_t *pte;
	pte_t entry;

#if defined(CONFIG_SH_KGDB)
	if (kgdb_nofault && kgdb_bus_err_hook)
	  kgdb_bus_err_hook();
#endif
	if (address >= P3SEG && address < P4SEG)
		dir = pgd_offset_k(address);
	else if (address >= TASK_SIZE)
		return 1;
	else if (!current->mm)
		return 1;
	else
		dir = pgd_offset(current->mm, address);

	pmd = pmd_offset(dir, address);
	if (pmd_none(*pmd))
		return 1;
	if (pmd_bad(*pmd)) {
		pmd_ERROR(*pmd);
		pmd_clear(pmd);
		return 1;
	}
	pte = pte_offset(pmd, address);
	entry = *pte;
	if (pte_none(entry) || pte_not_present(entry)
	    || (writeaccess && !pte_write(entry)))
		return 1;

	if (writeaccess)
		entry = pte_mkdirty(entry);
	entry = pte_mkyoung(entry);
#if defined(__SH4__)
	/*
	 * ITLB is not affected by "ldtlb" instruction.
	 * So, we need to flush the entry by ourselves.
	 */
	__flush_tlb_page(get_asid(), address&PAGE_MASK);
#endif
	set_pte(pte, entry);
	update_mmu_cache(NULL, address, entry);
	return 0;
}

void update_mmu_cache(struct vm_area_struct * vma,
		      unsigned long address, pte_t pte)
{
	unsigned long flags;
	unsigned long pteval;
	unsigned long vpn;
#if defined(__SH4__)
	struct page *page;
	unsigned long ptea;
#endif

	/* Ptrace may call this routine. */
	if (vma && current->active_mm != vma->vm_mm)
		return;

#if defined(__SH4__)
	page = pte_page(pte);
	if (VALID_PAGE(page) && !test_bit(PG_mapped, &page->flags)) {
		unsigned long phys = pte_val(pte) & PTE_PHYS_MASK;
		__flush_wback_region((void *)P1SEGADDR(phys), PAGE_SIZE);
		__set_bit(PG_mapped, &page->flags);
	}
#endif

	save_and_cli(flags);

	/* Set PTEH register */
	vpn = (address & MMU_VPN_MASK) | get_asid();
	ctrl_outl(vpn, MMU_PTEH);

	pteval = pte_val(pte);
#if defined(__SH4__)
	/* Set PTEA register */
	/* TODO: make this look less hacky */
	ptea = ((pteval >> 28) & 0xe) | (pteval & 0x1);
	ctrl_outl(ptea, MMU_PTEA);
#endif

	/* Set PTEL register */
	pteval &= _PAGE_FLAGS_HARDWARE_MASK; /* drop software flags */
	/* conveniently, we want all the software flags to be 0 anyway */
	ctrl_outl(pteval, MMU_PTEL);

	/* Load the TLB */
	asm volatile("ldtlb": /* no output */ : /* no input */ : "memory");
	restore_flags(flags);
}

void __flush_tlb_page(unsigned long asid, unsigned long page)
{
	unsigned long addr, data;

	/*
	 * NOTE: PTEH.ASID should be set to this MM
	 *       _AND_ we need to write ASID to the array.
	 *
	 * It would be simple if we didn't need to set PTEH.ASID...
	 */
#if defined(__sh3__)
	addr = MMU_TLB_ADDRESS_ARRAY |(page & 0x1F000)| MMU_PAGE_ASSOC_BIT;
	data = (page & 0xfffe0000) | asid; /* VALID bit is off */
	ctrl_outl(data, addr);
#elif defined(__SH4__)
	addr = MMU_UTLB_ADDRESS_ARRAY | MMU_PAGE_ASSOC_BIT;
	data = page | asid; /* VALID bit is off */
	jump_to_P2();
	ctrl_outl(data, addr);
	back_to_P1();
#endif
}

void flush_tlb_page(struct vm_area_struct *vma, unsigned long page)
{
	if (vma->vm_mm && vma->vm_mm->context != NO_CONTEXT) {
		unsigned long flags;
		unsigned long asid;
		unsigned long saved_asid = MMU_NO_ASID;

		asid = vma->vm_mm->context & MMU_CONTEXT_ASID_MASK;
		page &= PAGE_MASK;

		save_and_cli(flags);
		if (vma->vm_mm != current->mm) {
			saved_asid = get_asid();
			set_asid(asid);
		}
		__flush_tlb_page(asid, page);
		if (saved_asid != MMU_NO_ASID)
			set_asid(saved_asid);
		restore_flags(flags);
	}
}

void flush_tlb_range(struct mm_struct *mm, unsigned long start,
		     unsigned long end)
{
	if (mm->context != NO_CONTEXT) {
		unsigned long flags;
		int size;

		save_and_cli(flags);
		size = (end - start + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		if (size > (MMU_NTLB_ENTRIES/4)) { /* Too many TLB to flush */
			mm->context = NO_CONTEXT;
			if (mm == current->mm)
				activate_context(mm);
		} else {
			unsigned long asid = mm->context&MMU_CONTEXT_ASID_MASK;
			unsigned long saved_asid = MMU_NO_ASID;

			start &= PAGE_MASK;
			end += (PAGE_SIZE - 1);
			end &= PAGE_MASK;
			if (mm != current->mm) {
				saved_asid = get_asid();
				set_asid(asid);
			}
			while (start < end) {
				__flush_tlb_page(asid, start);
				start += PAGE_SIZE;
			}
			if (saved_asid != MMU_NO_ASID)
				set_asid(saved_asid);
		}
		restore_flags(flags);
	}
}

void flush_tlb_mm(struct mm_struct *mm)
{
	/* Invalidate all TLB of this process. */
	/* Instead of invalidating each TLB, we get new MMU context. */
	if (mm->context != NO_CONTEXT) {
		unsigned long flags;

		save_and_cli(flags);
		mm->context = NO_CONTEXT;
		if (mm == current->mm)
			activate_context(mm);
		restore_flags(flags);
	}
}

void flush_tlb_all(void)
{
	unsigned long flags, status;

	/*
	 * Flush all the TLB.
	 *
	 * Write to the MMU control register's bit:
	 * 	TF-bit for SH-3, TI-bit for SH-4.
	 *      It's same position, bit #2.
	 */
	save_and_cli(flags);
	status = ctrl_inl(MMUCR);
	status |= 0x04;		
	ctrl_outl(status, MMUCR);
	restore_flags(flags);
}
