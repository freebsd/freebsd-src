/*
 *  linux/arch/arm/mm/fault-common.c
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
#include <linux/init.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/unaligned.h>

#ifdef CONFIG_CPU_26
#define FAULT_CODE_WRITE	0x02
#define FAULT_CODE_FORCECOW	0x01
#define DO_COW(m)		((m) & (FAULT_CODE_WRITE|FAULT_CODE_FORCECOW))
#define READ_FAULT(m)		(!((m) & FAULT_CODE_WRITE))
#else
/*
 * On 32-bit processors, we define "mode" to be zero when reading,
 * non-zero when writing.  This now ties up nicely with the polarity
 * of the 26-bit machines, and also means that we avoid the horrible
 * gcc code for "int val = !other_val;".
 */
#define DO_COW(m)		(m)
#define READ_FAULT(m)		(!(m))
#endif

/*
 * This is useful to dump out the page tables associated with
 * 'addr' in mm 'mm'.
 */
void show_pte(struct mm_struct *mm, unsigned long addr)
{
	mm_segment_t fs;

	if (!mm)
		mm = &init_mm;

	printk(KERN_ALERT "mm = %p pgd = %p\n", mm, mm->pgd);

	fs = get_fs();
	set_fs(get_ds());
	do {
		pgd_t pg, *pgd = pgd_offset(mm, addr);
		pmd_t pm, *pmd;
		pte_t pt, *pte;

		printk(KERN_ALERT "*pgd = ");

		if (__get_user(pgd_val(pg), (unsigned long *)pgd)) {
			printk("(faulted)");
			break;
		}

		printk("%08lx", pgd_val(pg));

		if (pgd_none(pg))
			break;

		if (pgd_bad(pg)) {
			printk("(bad)");
			break;
		}

		pmd = pmd_offset(pgd, addr);

		printk(", *pmd = ");

		if (__get_user(pmd_val(pm), (unsigned long *)pmd)) {
			printk("(faulted)");
			break;
		}

		printk("%08lx", pmd_val(pm));

		if (pmd_none(pm))
			break;

		if (pmd_bad(pm)) {
			printk("(bad)");
			break;
		}

		pte = pte_offset(pmd, addr);

		printk(", *pte = ");

		if (__get_user(pte_val(pt), (unsigned long *)pte)) {
			printk("(faulted)");
			break;
		}

		printk("%08lx", pte_val(pt));
#ifdef CONFIG_CPU_32
		printk(", *ppte = %08lx", pte_val(pte[-PTRS_PER_PTE]));
#endif
	} while(0);
	set_fs(fs);

	printk("\n");
}

/*
 * Oops.  The kernel tried to access some page that wasn't present.
 */
static void
__do_kernel_fault(struct mm_struct *mm, unsigned long addr, int error_code,
		  struct pt_regs *regs)
{
	unsigned long fixup;

	/*
	 * Are we prepared to handle this kernel fault?
	 */
	if ((fixup = search_exception_table(instruction_pointer(regs))) != 0) {
#ifdef DEBUG
		printk(KERN_DEBUG "%s: Exception at [<%lx>] addr=%lx (fixup: %lx)\n",
			current->comm, regs->ARM_pc, addr, fixup);
#endif
		regs->ARM_pc = fixup;
		return;
	}

	/*
	 * No handler, we'll have to terminate things with extreme prejudice.
	 */
	printk(KERN_ALERT
		"Unable to handle kernel %s at virtual address %08lx\n",
		(addr < PAGE_SIZE) ? "NULL pointer dereference" :
		"paging request", addr);

	show_pte(mm, addr);
	die("Oops", regs, error_code);
	do_exit(SIGKILL);
}

/*
 * Something tried to access memory that isn't in our memory map..
 * User mode accesses just cause a SIGSEGV
 */
static void
__do_user_fault(struct task_struct *tsk, unsigned long addr, int error_code,
		int code, struct pt_regs *regs)
{
	struct siginfo si;

#ifdef CONFIG_DEBUG_USER
	printk(KERN_DEBUG "%s: unhandled page fault at pc=0x%08lx, "
	       "lr=0x%08lx (bad address=0x%08lx, code %d)\n",
	       tsk->comm, regs->ARM_pc, regs->ARM_lr, addr, error_code);
	show_regs(regs);
#endif

	tsk->thread.address = addr;
	tsk->thread.error_code = error_code;
	tsk->thread.trap_no = 14;
	si.si_signo = SIGSEGV;
	si.si_errno = 0;
	si.si_code = code;
	si.si_addr = (void *)addr;
	force_sig_info(SIGSEGV, &si, tsk);
}

void
do_bad_area(struct task_struct *tsk, struct mm_struct *mm, unsigned long addr,
	    int error_code, struct pt_regs *regs)
{
	/*
	 * If we are in kernel mode at this point, we
	 * have no context to handle this fault with.
	 */
	if (user_mode(regs))
		__do_user_fault(tsk, addr, error_code, SEGV_MAPERR, regs);
	else
		__do_kernel_fault(mm, addr, error_code, regs);
}

static int
__do_page_fault(struct mm_struct *mm, unsigned long addr, int error_code,
		struct task_struct *tsk)
{
	struct vm_area_struct *vma;
	int fault, mask;

	vma = find_vma(mm, addr);
	fault = -2; /* bad map area */
	if (!vma)
		goto out;
	if (vma->vm_start > addr)
		goto check_stack;

	/*
	 * Ok, we have a good vm_area for this
	 * memory access, so we can handle it.
	 */
good_area:
	if (READ_FAULT(error_code)) /* read? */
		mask = VM_READ|VM_EXEC;
	else
		mask = VM_WRITE;

	fault = -1; /* bad access type */
	if (!(vma->vm_flags & mask))
		goto out;

	/*
	 * If for any reason at all we couldn't handle
	 * the fault, make sure we exit gracefully rather
	 * than endlessly redo the fault.
	 */
survive:
	fault = handle_mm_fault(mm, vma, addr & PAGE_MASK, DO_COW(error_code));

	/*
	 * Handle the "normal" cases first - successful and sigbus
	 */
	switch (fault) {
	case 2:
		tsk->maj_flt++;
		return fault;
	case 1:
		tsk->min_flt++;
	case 0:
		return fault;
	}

	fault = -3; /* out of memory */
	if (tsk->pid != 1)
		goto out;

	/*
	 * If we are out of memory for pid1,
	 * sleep for a while and retry
	 */
	yield();
	goto survive;

check_stack:
	if (vma->vm_flags & VM_GROWSDOWN && !expand_stack(vma, addr))
		goto good_area;
out:
	return fault;
}

int do_page_fault(unsigned long addr, int error_code, struct pt_regs *regs)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	int fault;

	tsk = current;
	mm  = tsk->mm;

	/*
	 * If we're in an interrupt or have no user
	 * context, we must not take the fault..
	 */
	if (in_interrupt() || !mm)
		goto no_context;

	down_read(&mm->mmap_sem);
	fault = __do_page_fault(mm, addr, error_code, tsk);
	up_read(&mm->mmap_sem);

	/*
	 * Handle the "normal" case first
	 */
	if (fault > 0)
		return 0;

	/*
	 * We had some memory, but were unable to
	 * successfully fix up this page fault.
	 */
	if (fault == 0)
		goto do_sigbus;

	/*
	 * If we are in kernel mode at this point, we
	 * have no context to handle this fault with.
	 */
	if (!user_mode(regs))
		goto no_context;

	if (fault == -3) {
		/*
		 * We ran out of memory, or some other thing happened to
		 * us that made us unable to handle the page fault gracefully.
		 */
		printk("VM: killing process %s\n", tsk->comm);
		do_exit(SIGKILL);
	} else
		__do_user_fault(tsk, addr, error_code, fault == -1 ?
				SEGV_ACCERR : SEGV_MAPERR, regs);
	return 0;


/*
 * We ran out of memory, or some other thing happened to us that made
 * us unable to handle the page fault gracefully.
 */
do_sigbus:
	/*
	 * Send a sigbus, regardless of whether we were in kernel
	 * or user mode.
	 */
	tsk->thread.address = addr;
	tsk->thread.error_code = error_code;
	tsk->thread.trap_no = 14;
	force_sig(SIGBUS, tsk);
#ifdef CONFIG_DEBUG_USER
	printk(KERN_DEBUG "%s: sigbus at 0x%08lx, pc=0x%08lx\n",
		current->comm, addr, instruction_pointer(regs));
#endif

	/* Kernel mode? Handle exceptions or die */
	if (user_mode(regs))
		return 0;

no_context:
	__do_kernel_fault(mm, addr, error_code, regs);
	return 0;
}

/*
 * First Level Translation Fault Handler
 *
 * We enter here because the first level page table doesn't contain
 * a valid entry for the address.
 *
 * If the address is in kernel space (>= TASK_SIZE), then we are
 * probably faulting in the vmalloc() area.
 *
 * If the init_task's first level page tables contains the relevant
 * entry, we copy the it to this task.  If not, we send the process
 * a signal, fixup the exception, or oops the kernel.
 *
 * NOTE! We MUST NOT take any locks for this case. We may be in an
 * interrupt or a critical region, and should only copy the information
 * from the master page table, nothing more.
 */
int do_translation_fault(unsigned long addr, int error_code, struct pt_regs *regs)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	int offset;
	pgd_t *pgd, *pgd_k;
	pmd_t *pmd, *pmd_k;

	if (addr < TASK_SIZE)
		return do_page_fault(addr, error_code, regs);

	offset = __pgd_offset(addr);

	/*
	 * FIXME: CP15 C1 is write only on ARMv3 architectures.
	 */
	pgd = cpu_get_pgd() + offset;
	pgd_k = init_mm.pgd + offset;

	if (pgd_none(*pgd_k))
		goto bad_area;

#if 0	/* note that we are two-level */
	if (!pgd_present(*pgd))
		set_pgd(pgd, *pgd_k);
#endif

	pmd_k = pmd_offset(pgd_k, addr);
	pmd   = pmd_offset(pgd, addr);

	if (pmd_none(*pmd_k))
		goto bad_area;

	set_pmd(pmd, *pmd_k);
	return 0;

bad_area:
	tsk = current;
	mm  = tsk->active_mm;

	do_bad_area(tsk, mm, addr, error_code, regs);
	return 0;
}
