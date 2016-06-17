/*
 *  linux/arch/x86-64/mm/fault.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Copyright (C) 2001,2002 Andi Kleen, SuSE Labs.
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
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/vt_kern.h>		/* For unblank_screen() */
#include <linux/compiler.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/hardirq.h>
#include <asm/smp.h>
#include <asm/proto.h>
#include <asm/kdebug.h>

extern spinlock_t console_lock, timerlist_lock;

void bust_spinlocks(int yes)
{
 	spin_lock_init(&timerlist_lock);
	if (yes) {
		oops_in_progress = 1;
#ifdef CONFIG_SMP
		global_irq_lock = 0;	/* Many serial drivers do __global_cli() */
#endif
	} else {
	int loglevel_save = console_loglevel;
#ifdef CONFIG_VT
		unblank_screen();
#endif
		oops_in_progress = 0;
		/*
		 * OK, the message is on the console.  Now we call printk()
		 * without oops_in_progress set so that printk will give klogd
		 * a poke.  Hold onto your hats...
		 */
		console_loglevel = 15;		/* NMI oopser may have shut the console up */
		printk(" ");
		console_loglevel = loglevel_save;
	}
}

static int bad_address(void *p) 
{ 
	unsigned long dummy;
	return __get_user(dummy, (unsigned long *)p);
} 

void dump_pagetable(unsigned long address)
{
	pml4_t *pml4;
	asm("movq %%cr3,%0" : "=r" (pml4));

	pml4 = __va((unsigned long)pml4 & PHYSICAL_PAGE_MASK); 
	pml4 += pml4_index(address);
	printk("PML4 %lx ", pml4_val(*pml4));
	if (bad_address(pml4)) goto bad;
	if (!pml4_present(*pml4)) goto ret; 

	pgd_t *pgd = __pgd_offset_k((pgd_t *)pml4_page(*pml4), address); 
	if (bad_address(pgd)) goto bad;
	printk("PGD %lx ", pgd_val(*pgd)); 
	if (!pgd_present(*pgd))	goto ret;

	pmd_t *pmd = pmd_offset(pgd, address); 
	if (bad_address(pmd)) goto bad;
	printk("PMD %lx ", pmd_val(*pmd));
	if (!pmd_present(*pmd))	goto ret;	 

	pte_t *pte = pte_offset(pmd, address);
	if (bad_address(pte)) goto bad;
	printk("PTE %lx", pte_val(*pte)); 
ret:
	printk("\n");
	return;
bad:
	printk("BAD\n");
}

/* Sometimes the CPU reports invalid exceptions on prefetch.
   Check that here and ignore.
   Opcode checker based on code by Richard Brunner */
static int is_prefetch(struct pt_regs *regs, unsigned long addr)
{ 
	unsigned char *instr = (unsigned char *)(regs->rip);
	int scan_more = 1;
	int prefetch = 0; 
	unsigned char *max_instr = instr + 15;

	/* Avoid recursive faults for this common case */
	if (regs->rip == addr)
		return 0; 

	if (regs->cs & (1<<2))
		return 0;

	while (scan_more && instr < max_instr) { 
		unsigned char opcode;
		unsigned char instr_hi;
		unsigned char instr_lo;

		if (__get_user(opcode, instr))
			break; 

		instr_hi = opcode & 0xf0; 
		instr_lo = opcode & 0x0f; 
		instr++;

		switch (instr_hi) { 
		case 0x20:
		case 0x30:
			/* Values 0x26,0x2E,0x36,0x3E are valid x86
			   prefixes.  In long mode, the CPU will signal
			   invalid opcode if some of these prefixes are
			   present so we will never get here anyway */
			scan_more = ((instr_lo & 7) == 0x6);
			break;
			
		case 0x40:
			/* In AMD64 long mode, 0x40 to 0x4F are valid REX prefixes
			   Need to figure out under what instruction mode the
			   instruction was issued ... */
			/* Could check the LDT for lm, but for now it's good
			   enough to assume that long mode only uses well known
			   segments or kernel. */
			scan_more = ((regs->cs & 3) == 0) || (regs->cs == __USER_CS);
			break;
			
		case 0x60:
			/* 0x64 thru 0x67 are valid prefixes in all modes. */
			scan_more = (instr_lo & 0xC) == 0x4;
			break;		
		case 0xF0:
			/* 0xF0, 0xF2, and 0xF3 are valid prefixes in all modes. */
			scan_more = !instr_lo || (instr_lo>>1) == 1;
			break;			
		case 0x00:
			/* Prefetch instruction is 0x0F0D or 0x0F18 */
			scan_more = 0;
			if (__get_user(opcode, instr)) 
				break;
			prefetch = (instr_lo == 0xF) &&
				(opcode == 0x0D || opcode == 0x18);
			break;			
		default:
			scan_more = 0;
			break;
		} 
	}

#if 0
	if (prefetch)
		printk("%s: prefetch caused page fault at %lx/%lx\n", current->comm,
		       regs->rip, addr);
#endif
	return prefetch;
}

int page_fault_trace; 
int exception_trace = 1;

/*
 * This routine handles page faults.  It determines the address,
 * and the problem, and then passes it off to one of the appropriate
 * routines.
 *
 * error_code:
 *	bit 0 == 0 means no page found, 1 means protection fault
 *	bit 1 == 0 means read, 1 means write
 *	bit 2 == 0 means kernel, 1 means user-mode
 *      bit 3 == 1 means fault was an instruction fetch
 */
asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long error_code)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	struct vm_area_struct * vma;
	unsigned long address;
	unsigned long fixup;
	int write;
	siginfo_t info;

	/* get the address */
	__asm__("movq %%cr2,%0":"=r" (address));

	if (regs->eflags & X86_EFLAGS_IF)
		__sti();

#ifdef CONFIG_CHECKING
	if (page_fault_trace) 
		printk("pagefault rip:%lx rsp:%lx cs:%lu ss:%lu address %lx error %lx\n",
		       regs->rip,regs->rsp,regs->cs,regs->ss,address,error_code); 


	{ 
		unsigned long gs; 
		struct x8664_pda *pda = cpu_pda + safe_smp_processor_id(); 
		rdmsrl(MSR_GS_BASE, gs); 
		if (gs != (unsigned long)pda) { 
			wrmsrl(MSR_GS_BASE, pda); 
			printk("page_fault: wrong gs %lx expected %p\n", gs, pda);
		}
	}
#endif
			
	tsk = current;
	mm = tsk->mm;
	info.si_code = SEGV_MAPERR;

	/* 5 => page not present and from supervisor mode */
	if (unlikely(!(error_code & 5) &&
		     ((address >= VMALLOC_START && address <= VMALLOC_END) ||
		      (address >= MODULES_VADDR && address <= MODULES_END))))
		goto vmalloc_fault;
  
	/*
	 * If we're in an interrupt or have no user
	 * context, we must not take the fault..
	 */
	if (in_interrupt() || !mm)
		goto bad_area_nosemaphore;

	/* 
	 * Work around K8 errata #100. See the K8 specification update for 
	 * details. Any code segment in LDT is compatibility mode.
	 */
	if ((regs->cs == __USER32_CS || (regs->cs & (1<<2))) &&
		(address >> 32))
		return;

again:
	down_read(&mm->mmap_sem);

	vma = find_vma(mm, address);
	if (!vma)
		goto bad_area;
	if (vma->vm_start <= address)
		goto good_area;
	if (!(vma->vm_flags & VM_GROWSDOWN))
		goto bad_area;
	if (error_code & 4) {
		// XXX: align red zone size with ABI 
		if (address + 128 < regs->rsp)
			goto bad_area;
	}
	if (expand_stack(vma, address))
		goto bad_area;
/*
 * Ok, we have a good vm_area for this memory access, so
 * we can handle it..
 */
good_area:
	info.si_code = SEGV_ACCERR;
	write = 0;
	switch (error_code & 3) {
		default:	/* 3: write, present */
			/* fall through */
		case 2:		/* write, not present */
			if (!(vma->vm_flags & VM_WRITE))
				goto bad_area;
			write++;
			break;
		case 1:		/* read, present */
			goto bad_area;
		case 0:		/* read, not present */
			if (!(vma->vm_flags & (VM_READ | VM_EXEC)))
				goto bad_area;
	}

	/*
	 * If for any reason at all we couldn't handle the fault,
	 * make sure we exit gracefully rather than endlessly redo
	 * the fault.
	 */
	switch (handle_mm_fault(mm, vma, address, write)) {
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

bad_area_nosemaphore:
	/* User mode accesses just cause a SIGSEGV */
	if (error_code & 4) {
		if (is_prefetch(regs, address))
			return;

		if (exception_trace && !(tsk->ptrace & PT_PTRACED) && 
		    (tsk->sig->action[SIGSEGV-1].sa.sa_handler == SIG_IGN ||
		    (tsk->sig->action[SIGSEGV-1].sa.sa_handler == SIG_DFL)))
			printk(KERN_INFO 
		       "%s[%d]: segfault at %016lx rip %016lx rsp %016lx error %lx\n",
					tsk->comm, tsk->pid, address, regs->rip,
					regs->rsp, error_code);
	
		tsk->thread.cr2 = address;
		/* Kernel addresses are always protection faults */
		tsk->thread.error_code = error_code | (address >= TASK_SIZE);
		tsk->thread.trap_no = 14;
		info.si_signo = SIGSEGV;
		info.si_errno = 0;
		/* info.si_code has been set above */
		info.si_addr = (void *)address;
		force_sig_info(SIGSEGV, &info, tsk);
		return;
	}

no_context:
	
	/* Are we prepared to handle this kernel fault?  */
	if ((fixup = search_exception_table(regs->rip)) != 0) {
		regs->rip = fixup;
		if (0 && exception_trace) 
		printk(KERN_ERR 
		       "%s: fixed kernel exception at %lx address %lx err:%ld\n", 
		       tsk->comm, regs->rip, address, error_code);
		return;
	}

	if (is_prefetch(regs, address))
		return;

/*
 * Oops. The kernel tried to access some bad page. We'll have to
 * terminate things with extreme prejudice.
 */

	unsigned long flags; 
	prepare_die(&flags);
	if (address < PAGE_SIZE)
		printk(KERN_ALERT "Unable to handle kernel NULL pointer dereference");
	else
		printk(KERN_ALERT "Unable to handle kernel paging request");
	printk(KERN_ALERT " at %016lx RIP: ", address); 
	printk_address(regs->rip);
	dump_pagetable(address);
	__die("Oops", regs, error_code);
	/* Executive summary in case the oops scrolled away */
	printk(KERN_EMERG "CR2: %016lx\n", address);
	exit_die(flags);
	do_exit(SIGKILL);

/*
 * We ran out of memory, or some other thing happened to us that made
 * us unable to handle the page fault gracefully.
 */
out_of_memory:
	up_read(&mm->mmap_sem);
	if (current->pid == 1) { 
		tsk->policy |= SCHED_YIELD;
		schedule();
		goto again;
	}
	printk("VM: killing process %s\n", tsk->comm);
	if (error_code & 4)
		do_exit(SIGKILL);
	goto no_context;

do_sigbus:
	up_read(&mm->mmap_sem);

	/* Kernel mode? Handle exceptions or die */
	if (!(error_code & 4))
		goto no_context;
		
	if (is_prefetch(regs, address))
		return;

	tsk->thread.cr2 = address;
	tsk->thread.error_code = error_code;
	tsk->thread.trap_no = 14;
	info.si_signo = SIGBUS;
	info.si_errno = 0;
	info.si_code = BUS_ADRERR;
	info.si_addr = (void *)address;
	force_sig_info(SIGBUS, &info, tsk);
	return;


vmalloc_fault:
	{
		pgd_t *pgd;
		pmd_t *pmd;
		pte_t *pte; 

		/* 
		 * x86-64 has the same kernel 3rd level pages for all CPUs.
		 * But for vmalloc/modules the TLB synchronization works lazily,
		 * so it can happen that we get a page fault for something
		 * that is really already in the page table. Just check if it
		 * is really there and when yes flush the local TLB. 
		 */
#if 0
		printk("vmalloc fault %lx index %lu\n",address,pml4_index(address));
		dump_pagetable(address);
#endif

		pgd = pgd_offset_k(address);
		if (pgd != current_pgd_offset_k(address)) 
			goto bad_area_nosemaphore;	 
		if (!pgd_present(*pgd))
			goto bad_area_nosemaphore;
		pmd = pmd_offset(pgd, address);
		if (!pmd_present(*pmd))
			goto bad_area_nosemaphore;
		pte = pte_offset(pmd, address); 
		if (!pte_present(*pte))
			goto bad_area_nosemaphore; 

		__flush_tlb_all();		
		return;
	}
}
