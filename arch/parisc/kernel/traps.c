/*
 *  linux/arch/parisc/traps.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 1999, 2000  Philipp Rumpf <prumpf@tux.org>
 */

/*
 * 'Traps.c' handles hardware traps and faults after we have saved some
 * state in 'asm.s'.
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/console.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/unaligned.h>
#include <asm/atomic.h>
#include <asm/smp.h>
#include <asm/pdc.h>
#include <asm/pdc_chassis.h>

#include "../math-emu/math-emu.h"	/* for handle_fpe() */

#define PRINT_USER_FAULTS /* (turn this on if you want user faults to be */
			  /*  dumped to the console via printk)          */

static int printbinary(char *buf, unsigned long x, int nbits)
{
	unsigned long mask = 1UL << (nbits - 1);
	while (mask != 0) {
		*buf++ = (mask & x ? '1' : '0');
		mask >>= 1;
	}
	*buf = '\0';

	return nbits;
}

#ifdef __LP64__
#define RFMT "%016lx"
#else
#define RFMT "%08lx"
#endif

static int kstack_depth_to_print = 24;
extern struct module *module_list;
extern struct module kernel_module;

static inline int kernel_text_address(unsigned long addr)
{
#ifdef CONFIG_MODULES
	int retval = 0;
	struct module *mod;
#endif
	extern char _stext, _etext;

	if (addr >= (unsigned long) &_stext &&
	    addr <= (unsigned long) &_etext)
		return 1;

#ifdef CONFIG_MODULES
	for (mod = module_list; mod != &kernel_module; mod = mod->next) {
		/* mod_bound tests for addr being inside the vmalloc'ed
		 * module area. Of course it'd be better to test only
		 * for the .text subset... */
		if (mod_bound(addr, 0, mod)) {
			retval = 1;
			break;
		}
	}
	return retval;
#endif
}


void show_trace(unsigned long * stack)
{
	unsigned long *startstack;
	unsigned long addr;
	int i;

	startstack = (unsigned long *)((unsigned long)stack & ~(THREAD_SIZE - 1));
	i = 1;
	printk("Kernel addresses on the stack:\n");
	while (stack >= startstack) {
		addr = *stack--;
		if (kernel_text_address(addr)) {
			printk(" [<" RFMT ">] ", addr);
			if ((i & 0x03) == 0)
				printk("\n");
			i++;
		}
	}
	printk("\n");
}

void show_trace_task(struct task_struct *tsk)
{
	show_trace((unsigned long *)tsk->thread.regs.ksp);
}

void show_stack(unsigned long * sp)
{
	unsigned long *stack;
	int i;

	/*
	 * debugging aid: "show_stack(NULL);" prints the
	 * back trace for this cpu.
	 */
	if (sp==NULL)
		sp = (unsigned long*)&sp;

	stack = sp;
	printk("\n" KERN_CRIT "Stack Dump:\n");
	printk(KERN_CRIT " " RFMT ":  ", (unsigned long) stack);
	for (i=0; i < kstack_depth_to_print; i++) {
		if (((long) stack & (THREAD_SIZE-1)) == 0)
			break;
		if (i && ((i & 0x03) == 0))
			printk("\n" KERN_CRIT " " RFMT ":  ",
				(unsigned long) stack);
		printk(RFMT " ", *stack--);
	}
	printk("\n" KERN_CRIT "\n");
	show_trace(sp);
}

/*
 * The architecture-independent backtrace generator
 */
void dump_stack(void)
{
	show_stack(0);
}


void show_regs(struct pt_regs *regs)
{
	int i;
	char buf[128], *p;
	char *level;
	unsigned long cr30;
	unsigned long cr31;

	level = user_mode(regs) ? KERN_DEBUG : KERN_CRIT;

	printk("%s\n", level); /* don't want to have that pretty register dump messed up */

	printk("%s     YZrvWESTHLNXBCVMcbcbcbcbOGFRQPDI\n", level);
	printbinary(buf, regs->gr[0], 32);
	printk("%sPSW: %s %s\n", level, buf, print_tainted());

	for (i = 0; i < 32; i += 4) {
		int j;
		p = buf;
		p += sprintf(p, "%sr%02d-%02d ", level, i, i + 3);
		for (j = 0; j < 4; j++) {
			p += sprintf(p, " " RFMT, (i+j) == 0 ? 0 : regs->gr[i + j]);
		}
		printk("%s\n", buf);
	}

	for (i = 0; i < 8; i += 4) {
		int j;
		p = buf;
		p += sprintf(p, "%ssr%d-%d  ", level, i, i + 3);
		for (j = 0; j < 4; j++) {
			p += sprintf(p, " " RFMT, regs->sr[i + j]);
		}
		printk("%s\n", buf);
	}

#if RIDICULOUSLY_VERBOSE
	for (i = 0; i < 32; i += 2)
		printk("%sFR%02d : %016lx  FR%2d : %016lx", level, i,
				regs->fr[i], i+1, regs->fr[i+1]);
#endif

	cr30 = mfctl(30);
	cr31 = mfctl(31);
	printk("%s\n", level);
	printk("%sIASQ: " RFMT " " RFMT " IAOQ: " RFMT " " RFMT "\n",
	       level, regs->iasq[0], regs->iasq[1], regs->iaoq[0], regs->iaoq[1]);
	printk("%s IIR: %08lx    ISR: " RFMT "  IOR: " RFMT "\n",
	       level, regs->iir, regs->isr, regs->ior);
	printk("%s CPU: %8d   CR30: " RFMT " CR31: " RFMT "\n",
	       level, ((struct task_struct *)cr30)->processor, cr30, cr31);
	printk("%s ORIG_R28: " RFMT "\n", level, regs->orig_r28);
}


void die_if_kernel(char *str, struct pt_regs *regs, long err)
{
	if (user_mode(regs)) {
#ifdef PRINT_USER_FAULTS
		if (err == 0)
			return; /* STFU */

		/* XXX for debugging only */
		printk(KERN_DEBUG "%s (pid %d): %s (code %ld)\n",
			current->comm, current->pid, str, err);
		show_regs(regs);
#endif
		return;
	}
	
	/* unlock the pdc lock if necessary */
	pdc_emergency_unlock();

	/* maybe the kernel hasn't booted very far yet and hasn't been able 
	 * to initialize the serial or STI console. In that case we should 
	 * re-enable the pdc console, so that the user will be able to 
	 * identify the problem. */
	if (!console_drivers)
		pdc_console_restart();
	
	printk(KERN_CRIT "%s (pid %d): %s (code %ld)\n",
		current->comm, current->pid, str, err);
	show_regs(regs);

	/* Wot's wrong wif bein' racy? */
	if (current->thread.flags & PARISC_KERNEL_DEATH) {
		printk(KERN_CRIT "%s() recursion detected.\n", __FUNCTION__);
		sti();
		while (1);
	}

	current->thread.flags |= PARISC_KERNEL_DEATH;
	do_exit(SIGSEGV);
}

int syscall_ipi(int (*syscall) (struct pt_regs *), struct pt_regs *regs)
{
	return syscall(regs);
}

/* gdb uses break 4,8 */
#define GDB_BREAK_INSN 0x10004
void handle_gdb_break(struct pt_regs *regs, int wot)
{
	struct siginfo si;

	si.si_code = wot;
	si.si_addr = (void *) (regs->iaoq[0] & ~3);
	si.si_signo = SIGTRAP;
	si.si_errno = 0;
	force_sig_info(SIGTRAP, &si, current);
}

void handle_break(unsigned iir, struct pt_regs *regs)
{
	struct siginfo si;

	switch(iir) {
	case 0x00:
#ifdef PRINT_USER_FAULTS
		printk(KERN_DEBUG "break 0,0: pid=%d command='%s'\n",
		       current->pid, current->comm);
#endif
		die_if_kernel("Breakpoint", regs, 0);
#ifdef PRINT_USER_FAULTS
		show_regs(regs);
#endif
		si.si_code = TRAP_BRKPT;
		si.si_addr = (void *) (regs->iaoq[0] & ~3);
		si.si_signo = SIGTRAP;
		force_sig_info(SIGTRAP, &si, current);
		break;

	case GDB_BREAK_INSN:
		die_if_kernel("Breakpoint", regs, 0);
		handle_gdb_break(regs, TRAP_BRKPT);
		break;

	default:
#ifdef PRINT_USER_FAULTS
		printk(KERN_DEBUG "break %#08x: pid=%d command='%s'\n",
		       iir, current->pid, current->comm);
		show_regs(regs);
#endif
		si.si_signo = SIGTRAP;
		si.si_code = TRAP_BRKPT;
		si.si_addr = (void *) (regs->iaoq[0] & ~3);
		force_sig_info(SIGTRAP, &si, current);
		return;
	}
}


int handle_toc(void)
{
	printk(KERN_CRIT "TOC call.\n");
	return 0;
}

static void default_trap(int code, struct pt_regs *regs)
{
	printk(KERN_ERR "Trap %d on CPU %d\n", code, smp_processor_id());
	show_regs(regs);
}

void (*cpu_lpmc) (int code, struct pt_regs *regs) = default_trap;


void transfer_pim_to_trap_frame(struct pt_regs *regs)
{
    register int i;
    extern unsigned int hpmc_pim_data[];
    struct pdc_hpmc_pim_11 *pim_narrow;
    struct pdc_hpmc_pim_20 *pim_wide;

    if (boot_cpu_data.cpu_type >= pcxu) {

	pim_wide = (struct pdc_hpmc_pim_20 *)hpmc_pim_data;

	/*
	 * Note: The following code will probably generate a
	 * bunch of truncation error warnings from the compiler.
	 * Could be handled with an ifdef, but perhaps there
	 * is a better way.
	 */

	regs->gr[0] = pim_wide->cr[22];

	for (i = 1; i < 32; i++)
	    regs->gr[i] = pim_wide->gr[i];

	for (i = 0; i < 32; i++)
	    regs->fr[i] = pim_wide->fr[i];

	for (i = 0; i < 8; i++)
	    regs->sr[i] = pim_wide->sr[i];

	regs->iasq[0] = pim_wide->cr[17];
	regs->iasq[1] = pim_wide->iasq_back;
	regs->iaoq[0] = pim_wide->cr[18];
	regs->iaoq[1] = pim_wide->iaoq_back;

	regs->sar  = pim_wide->cr[11];
	regs->iir  = pim_wide->cr[19];
	regs->isr  = pim_wide->cr[20];
	regs->ior  = pim_wide->cr[21];
    }
    else {
	pim_narrow = (struct pdc_hpmc_pim_11 *)hpmc_pim_data;

	regs->gr[0] = pim_narrow->cr[22];

	for (i = 1; i < 32; i++)
	    regs->gr[i] = pim_narrow->gr[i];

	for (i = 0; i < 32; i++)
	    regs->fr[i] = pim_narrow->fr[i];

	for (i = 0; i < 8; i++)
	    regs->sr[i] = pim_narrow->sr[i];

	regs->iasq[0] = pim_narrow->cr[17];
	regs->iasq[1] = pim_narrow->iasq_back;
	regs->iaoq[0] = pim_narrow->cr[18];
	regs->iaoq[1] = pim_narrow->iaoq_back;

	regs->sar  = pim_narrow->cr[11];
	regs->iir  = pim_narrow->cr[19];
	regs->isr  = pim_narrow->cr[20];
	regs->ior  = pim_narrow->cr[21];
    }

    /*
     * The following fields only have meaning if we came through
     * another path. So just zero them here.
     */

    regs->ksp = 0;
    regs->kpc = 0;
    regs->orig_r28 = 0;
}


/*
 * This routine handles various exception codes.  It determines the address,
 * and the problem, and then passes it off to one of the appropriate
 * routines.
 */
void parisc_terminate(char *msg, struct pt_regs *regs, int code, unsigned long offset)
{
	static spinlock_t terminate_lock = SPIN_LOCK_UNLOCKED;

	set_eiem(0);
	__cli();
	spin_lock(&terminate_lock);

	/* unlock the pdc lock if necessary */
	pdc_emergency_unlock();

	/* restart pdc console if necessary */
	if (!console_drivers)
		pdc_console_restart();


	/* Not all switch paths will gutter the processor... */
	switch(code){

	case 1:
		transfer_pim_to_trap_frame(regs);
		break;
	    
	default:
		/* Fall through */
		break;
	}

	show_stack((unsigned long *)regs->gr[30]);

	printk("\n");
	printk(KERN_CRIT "%s: Code=%d regs=%p (Addr=" RFMT ")\n",
			msg, code, regs, offset);
	show_regs(regs);

	spin_unlock(&terminate_lock);

	/* put soft power button back under hardware control;
	 * if the user had pressed it once at any time, the 
	 * system will shut down immediately right here. */
	pdc_soft_power_button(0);
	
	/* Gutter the processor... */
	for(;;)
	    ;
}


void handle_interruption(int code, struct pt_regs *regs)
{
	unsigned long fault_address = 0;
	unsigned long fault_space = 0;
	struct siginfo si;

	switch(code) {

	case  1:
		/* High-priority machine check (HPMC) */
		pdc_console_restart();  /* switch back to pdc if HPMC */

		/* set up a new led state on systems shipped with a LED State panel */
		pdc_chassis_send_status(PDC_CHASSIS_DIRECT_HPMC);

		parisc_terminate("High Priority Machine Check (HPMC)",
				regs, code, 0);
		/* NOT REACHED */
		
	case  2:
		/* Power failure interrupt */
		printk(KERN_CRIT "Power failure interrupt !\n");
		return;

	case  3:
		/* Recovery counter trap */
		regs->gr[0] &= ~PSW_R;
		if (regs->iasq[0])
			handle_gdb_break(regs, TRAP_TRACE);
		/* else this must be the start of a syscall - just let it run */
		return;

	case  5:
		/* Low-priority machine check */

		pdc_chassis_send_status(PDC_CHASSIS_DIRECT_LPMC);

		flush_all_caches();
		cpu_lpmc(5, regs);
		return;

	case  6:
		/* Instruction TLB miss fault/Instruction page fault */
		fault_address = regs->iaoq[0];
		fault_space   = regs->iasq[0];
		break;

	case  8:
		/* Illegal instruction trap */
		die_if_kernel("Illegal instruction", regs, code);
		si.si_code = ILL_ILLOPC;
		goto give_sigill;

	case  9:
		/* Break instruction trap */
		handle_break(regs->iir,regs);
		return;
	
	case 10:
		/* Privileged operation trap */
		die_if_kernel("Privileged operation", regs, code);
		si.si_code = ILL_PRVOPC;
		goto give_sigill;
	
	case 11:
		/* Privileged register trap */
		if ((regs->iir & 0xffdfffe0) == 0x034008a0) {

			/* This is a MFCTL cr26/cr27 to gr instruction.
			 * PCXS traps on this, so we need to emulate it.
			 */

			if (regs->iir & 0x00200000)
				regs->gr[regs->iir & 0x1f] = mfctl(27);
			else
				regs->gr[regs->iir & 0x1f] = mfctl(26);

			regs->iaoq[0] = regs->iaoq[1];
			regs->iaoq[1] += 4;
			regs->iasq[0] = regs->iasq[1];
			return;
		}

		die_if_kernel("Privileged register usage", regs, code);
		si.si_code = ILL_PRVREG;
		/* Fall thru */
	give_sigill:
		si.si_signo = SIGILL;
		si.si_errno = 0;
		si.si_addr = (void *) regs->iaoq[0];
		force_sig_info(SIGILL, &si, current);
		return;

	case 12:
		/* Overflow Trap, let the userland signal handler do the cleanup */
		si.si_signo = SIGFPE;
		si.si_code = FPE_INTOVF;
		si.si_addr = (void *) regs->iaoq[0];
		force_sig_info(SIGFPE, &si, current);
		return;
	
	case 13:
		/* Conditional Trap 
		   The condition succees in an instruction which traps on condition  */
		si.si_signo = SIGFPE;
		/* Set to zero, and let the userspace app figure it out from
		   the insn pointed to by si_addr */
		si.si_code = 0;
		si.si_addr = (void *) regs->iaoq[0];
		force_sig_info(SIGFPE, &si, current);
		return;

	case 14:
		/* Assist Exception Trap, i.e. floating point exception. */
		die_if_kernel("Floating point exception", regs, 0); /* quiet */
		handle_fpe(regs);
		return;

	case 15: 
		/* Data TLB miss fault/Data page fault */	
		/* Fall thru */
	case 16:
		/* Non-access instruction TLB miss fault */
		/* The instruction TLB entry needed for the target address of the FIC
		   is absent, and hardware can't find it, so we get to cleanup */
		/* Fall thru */
	case 17:
		/* Non-access data TLB miss fault/Non-access data page fault */
		/* TODO: Still need to add slow path emulation code here */
		/* TODO: Understand what is meant by the TODO listed 
		   above this one. (Carlos) */
		fault_address = regs->ior;
		fault_space = regs->isr;
		break;

	case 18:
		/* PCXS only -- later cpu's split this into types 26,27 & 28 */
		/* Check for unaligned access */
		if (check_unaligned(regs)) {
			handle_unaligned(regs);
			return;
		}
		/* Fall Through */
	case 26: 
		/* PCXL: Data memory access rights trap */
		fault_address = regs->ior;
		fault_space   = regs->isr;
		break;

	case 19:
		/* Data memory break trap */
		regs->gr[0] |= PSW_X; /* So we can single-step over the trap */
		/* fall thru */
	case 21:
		/* Page reference trap */
		handle_gdb_break(regs, TRAP_HWBKPT);
		return;

	case 25:
		/* Taken branch trap */
		regs->gr[0] &= ~PSW_T;
		if (regs->iasq[0])
			handle_gdb_break(regs, TRAP_BRANCH);
		/* else this must be the start of a syscall - just let it
		 * run.
		 */
		return;

	case  7:  
		/* Instruction access rights */
		/* PCXL: Instruction memory protection trap */

		/*
		 * This could be caused by either: 1) a process attempting
		 * to execute within a vma that does not have execute
		 * permission, or 2) an access rights violation caused by a
		 * flush only translation set up by ptep_get_and_clear().
		 * So we check the vma permissions to differentiate the two.
		 * If the vma indicates we have execute permission, then
		 * the cause is the latter one. In this case, we need to
		 * call do_page_fault() to fix the problem.
		 */

		if (user_mode(regs)) {
			struct vm_area_struct *vma;

			down_read(&current->mm->mmap_sem);
			vma = find_vma(current->mm,regs->iaoq[0]);
			if (vma && (regs->iaoq[0] >= vma->vm_start)
				&& (vma->vm_flags & VM_EXEC)) {

				fault_address = regs->iaoq[0];
				fault_space = regs->iasq[0];

				up_read(&current->mm->mmap_sem);
				break; /* call do_page_fault() */
			}
			up_read(&current->mm->mmap_sem);
		}
		/* Fall Through */
	case 27: 
		/* Data memory protection ID trap */
		die_if_kernel("Protection id trap", regs, code);
		si.si_code = SEGV_MAPERR;
		si.si_signo = SIGSEGV;
		si.si_errno = 0;
		if (code == 7)
		    si.si_addr = (void *) regs->iaoq[0];
		else
		    si.si_addr = (void *) regs->ior;
		force_sig_info(SIGSEGV, &si, current);
		return;

	case 28: 
		/* Unaligned data reference trap */
		handle_unaligned(regs);
		return;

	default:
		if (user_mode(regs)) {
#ifdef PRINT_USER_FAULTS
			printk(KERN_DEBUG "\nhandle_interruption() pid=%d command='%s'\n",
			    current->pid, current->comm);
			show_regs(regs);
#endif
			/* SIGBUS, for lack of a better one. */
			si.si_signo = SIGBUS;
			si.si_code = BUS_OBJERR;
			si.si_errno = 0;
			si.si_addr = (void *) regs->ior;
			force_sig_info(SIGBUS, &si, current);
			return;
		}
		
		pdc_chassis_send_status(PDC_CHASSIS_DIRECT_PANIC);

		parisc_terminate("Unexpected interruption", regs, code, 0);
		/* NOT REACHED */
	}

	if (user_mode(regs)) {
	    if (fault_space != regs->sr[7]) {
#ifdef PRINT_USER_FAULTS
		if (fault_space == 0)
			printk(KERN_DEBUG "User Fault on Kernel Space ");
		else
			printk(KERN_DEBUG "User Fault (long pointer) ");
		printk("pid=%d command='%s'\n", current->pid, current->comm);
		show_regs(regs);
#endif
		si.si_signo = SIGSEGV;
		si.si_errno = 0;
		si.si_code = SEGV_MAPERR;
		si.si_addr = (void *) regs->ior;
		force_sig_info(SIGSEGV, &si, current);
		return;
	    }
	}
	else {

	    /*
	     * The kernel should never fault on its own address space.
	     */

	    if (fault_space == 0) {
		pdc_chassis_send_status(PDC_CHASSIS_DIRECT_PANIC);
		parisc_terminate("Kernel Fault", regs, code, fault_address);
		/** NOT REACHED **/
	    }
	}

	local_irq_enable();
	do_page_fault(regs, code, fault_address);
}



int __init check_ivt(void *iva)
{
	int i;
	u32 check = 0;
	u32 *ivap;
	u32 *hpmcp;
	u32 length;
	extern void os_hpmc(void);
	extern void os_hpmc_end(void);

	if (strcmp((char *)iva, "cows can fly"))
		return -1;

	ivap = (u32 *)iva;

	for (i = 0; i < 8; i++)
	    *ivap++ = 0;

	/* Compute Checksum for HPMC handler */

	length = (u32)((unsigned long)os_hpmc_end - (unsigned long)os_hpmc);
	ivap[7] = length;

	hpmcp = (u32 *)os_hpmc;

	for (i=0; i<length/4; i++)
	    check += *hpmcp++;

	for (i=0; i<8; i++)
	    check += ivap[i];

	ivap[5] = -check;

	return 0;
}
	
#ifndef __LP64__
extern const void fault_vector_11;
#endif
extern const void fault_vector_20;

void __init trap_init(void)
{
	void *iva;

	if (boot_cpu_data.cpu_type >= pcxu)
		iva = (void *) &fault_vector_20;
	else
#ifdef __LP64__
		panic("Can't boot 64-bit OS on PA1.1 processor!");
#else
		iva = (void *) &fault_vector_11;
#endif

	if (check_ivt(iva))
		panic("IVT invalid");
}
