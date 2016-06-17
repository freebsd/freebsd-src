/*
 *  linux/arch/arm/kernel/traps.c
 *
 *  Copyright (C) 1995-2002 Russell King
 *  Fragments that appear the same as linux/arch/i386/kernel/traps.c (C) Linus Torvalds
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  'traps.c' handles hardware exceptions after we have saved some state in
 *  'linux/arch/arm/lib/traps.S'.  Mostly a debugging aid, but will probably
 *  kill the offending process.
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/personality.h>
#include <linux/ptrace.h>
#include <linux/elf.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>

#include "ptrace.h"

extern void c_backtrace (unsigned long fp, int pmode);
extern void show_pte(struct mm_struct *mm, unsigned long addr);

const char *processor_modes[]=
{ "USER_26", "FIQ_26" , "IRQ_26" , "SVC_26" , "UK4_26" , "UK5_26" , "UK6_26" , "UK7_26" ,
  "UK8_26" , "UK9_26" , "UK10_26", "UK11_26", "UK12_26", "UK13_26", "UK14_26", "UK15_26",
  "USER_32", "FIQ_32" , "IRQ_32" , "SVC_32" , "UK4_32" , "UK5_32" , "UK6_32" , "ABT_32" ,
  "UK8_32" , "UK9_32" , "UK10_32", "UND_32" , "UK12_32", "UK13_32", "UK14_32", "SYS_32"
};

static const char *handler[]= { "prefetch abort", "data abort", "address exception", "interrupt" };

/*
 * Stack pointers should always be within the kernels view of
 * physical memory.  If it is not there, then we can't dump
 * out any information relating to the stack.
 */
static int verify_stack(unsigned long sp)
{
	if (sp < PAGE_OFFSET || (sp > (unsigned long)high_memory && high_memory != 0))
		return -EFAULT;

	return 0;
}

/*
 * Dump out the contents of some memory nicely...
 */
static void dump_mem(const char *str, unsigned long bottom, unsigned long top)
{
	unsigned long p = bottom & ~31;
	mm_segment_t fs;
	int i;

	/*
	 * We need to switch to kernel mode so that we can use __get_user
	 * to safely read from kernel space.  Note that we now dump the
	 * code first, just in case the backtrace kills us.
	 */
	fs = get_fs();
	set_fs(KERNEL_DS);

	printk("%s(0x%08lx to 0x%08lx)\n", str, bottom, top);

	for (p = bottom & ~31; p < top;) {
		printk("%04lx: ", p & 0xffff);

		for (i = 0; i < 8; i++, p += 4) {
			unsigned int val;

			if (p < bottom || p >= top)
				printk("         ");
			else {
				__get_user(val, (unsigned long *)p);
				printk("%08x ", val);
			}
		}
		printk ("\n");
	}

	set_fs(fs);
}

static void dump_instr(struct pt_regs *regs)
{
	unsigned long addr = instruction_pointer(regs);
	const int thumb = thumb_mode(regs);
	const int width = thumb ? 4 : 8;
	mm_segment_t fs;
	int i;

	/*
	 * We need to switch to kernel mode so that we can use __get_user
	 * to safely read from kernel space.  Note that we now dump the
	 * code first, just in case the backtrace kills us.
	 */
	fs = get_fs();
	set_fs(KERNEL_DS);

	printk("Code: ");
	for (i = -4; i < 1; i++) {
		unsigned int val, bad;

		if (thumb)
			bad = __get_user(val, &((u16 *)addr)[i]);
		else
			bad = __get_user(val, &((u32 *)addr)[i]);

		if (!bad)
			printk(i == 0 ? "(%0*x) " : "%0*x ", width, val);
		else {
			printk("bad PC value.");
			break;
		}
	}
	printk("\n");

	set_fs(fs);
}

static void I_really_mean_dump_stack_so_dont_mess_with_me(struct task_struct *tsk, unsigned long sp)
{
	dump_mem("Stack: ", sp, 8192+(unsigned long)tsk);
}

static void dump_backtrace(struct pt_regs *regs, struct task_struct *tsk)
{
	unsigned int fp = regs->ARM_fp;
	char *msg = "";
	int ok = 1;

#ifdef CONFIG_FRAME_POINTER
	if (!fp) {
		msg = "no frame pointer";
		ok = 0;
	} else if (verify_stack(fp)) {
		msg = "invalid frame pointer";
		ok = 0;
	} else if (fp < 4096+(unsigned long)tsk)
		msg = "frame pointer underflow";
#else
	msg = "not available";
	ok = 0;
#endif

	printk("Backtrace: %s\n", msg);

	if (ok)
		c_backtrace(fp, processor_mode(regs));
}

/*
 * This is called from SysRq-T (show_task) to display the current
 * call trace for each process.  Very useful.
 */
void show_trace_task(struct task_struct *tsk)
{
	if (tsk != current) {
		unsigned int fp = thread_saved_fp(&tsk->thread);
		c_backtrace(fp, 0x10);
	}
}

spinlock_t die_lock = SPIN_LOCK_UNLOCKED;

/*
 * This function is protected against re-entrancy.
 */
NORET_TYPE void die(const char *str, struct pt_regs *regs, int err)
{
	struct task_struct *tsk = current;

	console_verbose();
	spin_lock_irq(&die_lock);

	printk("Internal error: %s: %x\n", str, err);
	printk("CPU: %d\n", smp_processor_id());
	show_regs(regs);
	printk("Process %s (pid: %d, stack limit = 0x%p)\n",
		current->comm, current->pid, tsk + 1);

	if (!user_mode(regs) || in_interrupt()) {
		I_really_mean_dump_stack_so_dont_mess_with_me(tsk, (unsigned long)(regs + 1));
		dump_backtrace(regs, tsk);
		dump_instr(regs);
	}

	spin_unlock_irq(&die_lock);
	do_exit(SIGSEGV);
}

void die_if_kernel(const char *str, struct pt_regs *regs, int err)
{
	if (user_mode(regs))
    		return;

    	die(str, regs, err);
}

asmlinkage void do_undefinstr(int address, struct pt_regs *regs, int mode)
{
	unsigned long *pc;
	siginfo_t info;

	/*
	 * According to the ARM ARM, PC is 2 or 4 bytes ahead, depending
	 * whether we're in Thumb mode or not.
	 */
	regs->ARM_pc -= thumb_mode(regs) ? 2 : 4;
	pc = (unsigned long *)instruction_pointer(regs);

#ifdef CONFIG_DEBUG_USER
	printk(KERN_INFO "%s (%d): undefined instruction: pc=%p\n",
		current->comm, current->pid, pc);
	dump_instr(regs);
#endif

	current->thread.error_code = 0;
	current->thread.trap_no = 6;

	info.si_signo = SIGILL;
	info.si_errno = 0;
	info.si_code  = ILL_ILLOPC;
	info.si_addr  = pc;

	force_sig_info(SIGILL, &info, current);

	die_if_kernel("Oops - undefined instruction", regs, mode);
}

#ifdef CONFIG_CPU_26
asmlinkage void do_excpt(int address, struct pt_regs *regs, int mode)
{
	siginfo_t info;

#ifdef CONFIG_DEBUG_USER
	printk(KERN_INFO "%s (%d): address exception: pc=%08lx\n",
		current->comm, current->pid, instruction_pointer(regs));
	dump_instr(regs);
#endif

	current->thread.error_code = 0;
	current->thread.trap_no = 11;

	info.si_signo = SIGBUS;
	info.si_errno = 0;
	info.si_code  = BUS_ADRERR;
	info.si_addr  = (void *)address;

	force_sig_info(SIGBUS, &info, current);

	die_if_kernel("Oops - address exception", regs, mode);
}
#endif

asmlinkage void do_unexp_fiq (struct pt_regs *regs)
{
#ifndef CONFIG_IGNORE_FIQ
	printk("Hmm.  Unexpected FIQ received, but trying to continue\n");
	printk("You may have a hardware problem...\n");
#endif
}

/*
 * bad_mode handles the impossible case in the vectors.  If you see one of
 * these, then it's extremely serious, and could mean you have buggy hardware.
 * It never returns, and never tries to sync.  We hope that we can at least
 * dump out some state information...
 */
asmlinkage void bad_mode(struct pt_regs *regs, int reason, int proc_mode)
{
	unsigned int vectors = vectors_base();

	console_verbose();

	printk(KERN_CRIT "Bad mode in %s handler detected: mode %s\n",
		handler[reason], processor_modes[proc_mode]);

	/*
	 * Dump out the vectors and stub routines.  Maybe a better solution
	 * would be to dump them out only if we detect that they are corrupted.
	 */
	dump_mem(KERN_CRIT "Vectors: ", vectors, vectors + 0x40);
	dump_mem(KERN_CRIT "Stubs: ", vectors + 0x200, vectors + 0x4b8);

	die("Oops", regs, 0);
	local_irq_disable();
	panic("bad mode");
}

static int bad_syscall(int n, struct pt_regs *regs)
{
	siginfo_t info;

	/* You might think just testing `handler' would be enough, but PER_LINUX
	 * points it to no_lcall7 to catch undercover SVr4 binaries.  Gutted.
	 */
	if (current->personality != PER_LINUX && current->exec_domain->handler) {
		/* Hand it off to iBCS.  The extra parameter and consequent type 
		 * forcing is necessary because of the weird ARM calling convention.
		 */
		current->exec_domain->handler(n, regs);
		return regs->ARM_r0;
	}

#ifdef CONFIG_DEBUG_USER
	printk(KERN_ERR "[%d] %s: obsolete system call %08x.\n",
		current->pid, current->comm, n);
	dump_instr(regs);
#endif

	info.si_signo = SIGILL;
	info.si_errno = 0;
	info.si_code  = ILL_ILLTRP;
	info.si_addr  = (void *)instruction_pointer(regs) -
			 (thumb_mode(regs) ? 2 : 4);

	force_sig_info(SIGILL, &info, current);
	die_if_kernel("Oops", regs, n);
	return regs->ARM_r0;
}

/*
 * Handle all unrecognised system calls.
 *  0x9f0000 - 0x9fffff are some more esoteric system calls
 */
#define NR(x) ((__ARM_NR_##x) - __ARM_NR_BASE)
asmlinkage int arm_syscall(int no, struct pt_regs *regs)
{
	siginfo_t info;

	if ((no >> 16) != 0x9f)
		return bad_syscall(no, regs);

	switch (no & 0xffff) {
	case 0: /* branch through 0 */
		info.si_signo = SIGSEGV;
		info.si_errno = 0;
		info.si_code  = SEGV_MAPERR;
		info.si_addr  = NULL;

		force_sig_info(SIGSEGV, &info, current);

		die_if_kernel("branch through zero", regs, 0);
		return 0;

	case NR(breakpoint): /* SWI BREAK_POINT */
		ptrace_break(current, regs);
		return regs->ARM_r0;

#ifdef CONFIG_CPU_32
	/*
	 * Flush a region from virtual address 'r0' to virtual address 'r1'
	 * _inclusive_.  There is no alignment requirement on either address;
	 * user space does not need to know the hardware cache layout.
	 *
	 * r2 contains flags.  It should ALWAYS be passed as ZERO until it
	 * is defined to be something else.  For now we ignore it, but may
	 * the fires of hell burn in your belly if you break this rule. ;)
	 *
	 * (at a later date, we may want to allow this call to not flush
	 * various aspects of the cache.  Passing '0' will guarantee that
	 * everything necessary gets flushed to maintain consistency in
	 * the specified region).
	 */
	case NR(cacheflush):
		cpu_cache_clean_invalidate_range(regs->ARM_r0, regs->ARM_r1, 1);
		return 0;

	case NR(usr26):
		if (!(elf_hwcap & HWCAP_26BIT))
			break;
		regs->ARM_cpsr &= ~0x10;
		return regs->ARM_r0;

	case NR(usr32):
		if (!(elf_hwcap & HWCAP_26BIT))
			break;
		regs->ARM_cpsr |= 0x10;
		return regs->ARM_r0;
#else
	case NR(cacheflush):
		return 0;

	case NR(usr26):
	case NR(usr32):
		break;
#endif

	default:
		/* Calls 9f00xx..9f07ff are defined to return -ENOSYS
		   if not implemented, rather than raising SIGILL.  This
		   way the calling program can gracefully determine whether
		   a feature is supported.  */
		if (no <= 0x7ff)
			return -ENOSYS;
		break;
	}
#ifdef CONFIG_DEBUG_USER
	/*
	 * experience shows that these seem to indicate that
	 * something catastrophic has happened
	 */
	printk("[%d] %s: arm syscall %d\n", current->pid, current->comm, no);
	dump_instr(regs);
	if (user_mode(regs)) {
		show_regs(regs);
		c_backtrace(regs->ARM_fp, processor_mode(regs));
	}
#endif
	info.si_signo = SIGILL;
	info.si_errno = 0;
	info.si_code  = ILL_ILLTRP;
	info.si_addr  = (void *)instruction_pointer(regs) -
			 (thumb_mode(regs) ? 2 : 4);

	force_sig_info(SIGILL, &info, current);
	die_if_kernel("Oops", regs, no);
	return 0;
}

void __bad_xchg(volatile void *ptr, int size)
{
	printk("xchg: bad data size: pc 0x%p, ptr 0x%p, size %d\n",
		__builtin_return_address(0), ptr, size);
	BUG();
}

/*
 * A data abort trap was taken, but we did not handle the instruction.
 * Try to abort the user program, or panic if it was the kernel.
 */
asmlinkage void
baddataabort(int code, unsigned long instr, struct pt_regs *regs)
{
	unsigned long addr = instruction_pointer(regs);
	siginfo_t info;

#ifdef CONFIG_DEBUG_USER
	printk(KERN_ERR "[%d] %s: bad data abort: code %d instr 0x%08lx\n",
		current->pid, current->comm, code, instr);
	dump_instr(regs);
	show_pte(current->mm, addr);
#endif

	info.si_signo = SIGILL;
	info.si_errno = 0;
	info.si_code  = ILL_ILLOPC;
	info.si_addr  = (void *)addr;

	force_sig_info(SIGILL, &info, current);
	die_if_kernel("unknown data abort code", regs, instr);
}

void __bug(const char *file, int line, void *data)
{
	printk(KERN_CRIT"kernel BUG at %s:%d!", file, line);
	if (data)
		printk(KERN_CRIT" - extra data = %p", data);
	printk("\n");
	*(int *)0 = 0;
}

void __readwrite_bug(const char *fn)
{
	printk("%s called, but not implemented", fn);
	BUG();
}

void __pte_error(const char *file, int line, unsigned long val)
{
	printk("%s:%d: bad pte %08lx.\n", file, line, val);
}

void __pmd_error(const char *file, int line, unsigned long val)
{
	printk("%s:%d: bad pmd %08lx.\n", file, line, val);
}

void __pgd_error(const char *file, int line, unsigned long val)
{
	printk("%s:%d: bad pgd %08lx.\n", file, line, val);
}

asmlinkage void __div0(void)
{
	printk("Division by zero in kernel.\n");
	__backtrace();
}

void abort(void)
{
	BUG();

	/* if that doesn't kill us, halt */
	panic("Oops failed to kill thread");
}

void __init trap_init(void)
{
	extern void __trap_init(unsigned long);
	unsigned long base = vectors_base();

	__trap_init(base);
	if (base != 0)
		printk(KERN_DEBUG "Relocating machine vectors to 0x%08lx\n",
			base);
#ifdef CONFIG_CPU_32
	modify_domain(DOMAIN_USER, DOMAIN_CLIENT);
#endif
}
