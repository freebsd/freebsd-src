/*
 *  linux/arch/m68k/kernel/process.c
 *
 *  Copyright (C) 1995  Hamish Macdonald
 *
 *  68060 fixes by Jesper Skov
 */

/*
 * This file handles the architecture-dependent parts of process handling..
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/reboot.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/traps.h>
#include <asm/machdep.h>
#include <asm/setup.h>
#include <asm/pgtable.h>

/*
 * Initial task structure. Make this a per-architecture thing,
 * because different architectures tend to have different
 * alignment requirements and potentially different initial
 * setup.
 */
static struct fs_struct init_fs = INIT_FS;
static struct files_struct init_files = INIT_FILES;
static struct signal_struct init_signals = INIT_SIGNALS;
struct mm_struct init_mm = INIT_MM(init_mm);

union task_union init_task_union
__attribute__((section("init_task"), aligned(KTHREAD_SIZE)))
	= { task: INIT_TASK(init_task_union.task) };

asmlinkage void ret_from_fork(void);


/*
 * The idle loop on an m68k..
 */
static void default_idle(void)
{
	while(1) {
		if (!current->need_resched)
#if defined(MACH_ATARI_ONLY) && !defined(CONFIG_HADES)
			/* block out HSYNC on the atari (falcon) */
			__asm__("stop #0x2200" : : : "cc");
#else
			__asm__("stop #0x2000" : : : "cc");
#endif
		schedule();
		check_pgt_cache();
	}
}

void (*idle)(void) = default_idle;

/*
 * The idle thread. There's no useful work to be
 * done, so just try to conserve power and have a
 * low exit latency (ie sit in a loop waiting for
 * somebody to say that they'd like to reschedule)
 */
void cpu_idle(void)
{
	/* endless idle loop with no priority at all */
	init_idle();
	current->nice = 20;
	current->counter = -100;
	idle();
}

void machine_restart(char * __unused)
{
	if (mach_reset)
		mach_reset();
	for (;;);
}

void machine_halt(void)
{
	if (mach_halt)
		mach_halt();
	for (;;);
}

void machine_power_off(void)
{
	if (mach_power_off)
		mach_power_off();
	for (;;);
}

void show_regs(struct pt_regs * regs)
{
	printk("\n");
	printk("Format %02x  Vector: %04x  PC: %08lx  Status: %04x    %s\n",
	       regs->format, regs->vector, regs->pc, regs->sr, print_tainted());
	printk("ORIG_D0: %08lx  D0: %08lx  A2: %08lx  A1: %08lx\n",
	       regs->orig_d0, regs->d0, regs->a2, regs->a1);
	printk("A0: %08lx  D5: %08lx  D4: %08lx\n",
	       regs->a0, regs->d5, regs->d4);
	printk("D3: %08lx  D2: %08lx  D1: %08lx\n",
	       regs->d3, regs->d2, regs->d1);
	if (!(regs->sr & PS_S))
		printk("USP: %08lx\n", rdusp());
}

/*
 * Create a kernel thread
 */
int arch_kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	int pid;
	mm_segment_t fs;

	fs = get_fs();
	set_fs (KERNEL_DS);

	{
	register long retval __asm__ ("d0");
	register long clone_arg __asm__ ("d1") = flags | CLONE_VM;

	retval = __NR_clone;
	__asm__ __volatile__
	  ("clrl %%d2\n\t"
	   "trap #0\n\t"		/* Linux/m68k system call */
	   "tstl %0\n\t"		/* child or parent */
	   "jne 1f\n\t"			/* parent - jump */
	   "lea %%sp@(%c7),%6\n\t"	/* reload current */
	   "movel %3,%%sp@-\n\t"	/* push argument */
	   "jsr %4@\n\t"		/* call fn */
	   "movel %0,%%d1\n\t"		/* pass exit value */
	   "movel %2,%%d0\n\t"		/* exit */
	   "trap #0\n"
	   "1:"
	   : "+d" (retval)
	   : "i" (__NR_clone), "i" (__NR_exit),
	     "r" (arg), "a" (fn), "d" (clone_arg), "r" (current),
	     "i" (-KTHREAD_SIZE)
	   : "d2");

	pid = retval;
	}

	set_fs (fs);
	return pid;
}

void flush_thread(void)
{
	unsigned long zero = 0;
	set_fs(USER_DS);
	current->thread.fs = __USER_DS;
	if (!FPU_IS_EMU)
		asm volatile (".chip 68k/68881\n\t"
			      "frestore %0@\n\t"
			      ".chip 68k" : : "a" (&zero));
}

/*
 * "m68k_fork()".. By the time we get here, the
 * non-volatile registers have also been saved on the
 * stack. We do some ugly pointer stuff here.. (see
 * also copy_thread)
 */

asmlinkage int m68k_fork(struct pt_regs *regs)
{
	return do_fork(SIGCHLD, rdusp(), regs, 0);
}

asmlinkage int m68k_vfork(struct pt_regs *regs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, rdusp(), regs, 0);
}

asmlinkage int m68k_clone(struct pt_regs *regs)
{
	unsigned long clone_flags;
	unsigned long newsp;

	/* syscall2 puts clone_flags in d1 and usp in d2 */
	clone_flags = regs->d1;
	newsp = regs->d2;
	if (!newsp)
		newsp = rdusp();
	return do_fork(clone_flags, newsp, regs, 0);
}

int copy_thread(int nr, unsigned long clone_flags, unsigned long usp,
		 unsigned long unused,
		 struct task_struct * p, struct pt_regs * regs)
{
	struct pt_regs * childregs;
	struct switch_stack * childstack, *stack;
	unsigned long stack_offset, *retp;

	stack_offset = KTHREAD_SIZE - sizeof(struct pt_regs);
	childregs = (struct pt_regs *) ((unsigned long) p + stack_offset);

	*childregs = *regs;
	childregs->d0 = 0;

	retp = ((unsigned long *) regs);
	stack = ((struct switch_stack *) retp) - 1;

	childstack = ((struct switch_stack *) childregs) - 1;
	*childstack = *stack;
	childstack->retpc = (unsigned long)ret_from_fork;

	p->thread.usp = usp;
	p->thread.ksp = (unsigned long)childstack;
	/*
	 * Must save the current SFC/DFC value, NOT the value when
	 * the parent was last descheduled - RGH  10-08-96
	 */
	p->thread.fs = get_fs().seg;

	if (!FPU_IS_EMU) {
		/* Copy the current fpu state */
		asm volatile ("fsave %0" : : "m" (p->thread.fpstate[0]) : "memory");

		if (!CPU_IS_060 ? p->thread.fpstate[0] : p->thread.fpstate[2])
		  asm volatile ("fmovemx %/fp0-%/fp7,%0\n\t"
				"fmoveml %/fpiar/%/fpcr/%/fpsr,%1"
				: : "m" (p->thread.fp[0]), "m" (p->thread.fpcntl[0])
				: "memory");
		/* Restore the state in case the fpu was busy */
		asm volatile ("frestore %0" : : "m" (p->thread.fpstate[0]));
	}

	return 0;
}

/* Fill in the fpu structure for a core dump.  */

int dump_fpu (struct pt_regs *regs, struct user_m68kfp_struct *fpu)
{
	char fpustate[216];

	if (FPU_IS_EMU) {
		int i;

		memcpy(fpu->fpcntl, current->thread.fpcntl, 12);
		memcpy(fpu->fpregs, current->thread.fp, 96);
		/* Convert internal fpu reg representation
		 * into long double format
		 */
		for (i = 0; i < 24; i += 3)
			fpu->fpregs[i] = ((fpu->fpregs[i] & 0xffff0000) << 15) |
			                 ((fpu->fpregs[i] & 0x0000ffff) << 16);
		return 1;
	}

	/* First dump the fpu context to avoid protocol violation.  */
	asm volatile ("fsave %0" :: "m" (fpustate[0]) : "memory");
	if (!CPU_IS_060 ? !fpustate[0] : !fpustate[2])
		return 0;

	asm volatile ("fmovem %/fpiar/%/fpcr/%/fpsr,%0"
		:: "m" (fpu->fpcntl[0])
		: "memory");
	asm volatile ("fmovemx %/fp0-%/fp7,%0"
		:: "m" (fpu->fpregs[0])
		: "memory");
	return 1;
}

/*
 * fill in the user structure for a core dump..
 */
void dump_thread(struct pt_regs * regs, struct user * dump)
{
	struct switch_stack *sw;

/* changed the size calculations - should hopefully work better. lbt */
	dump->magic = CMAGIC;
	dump->start_code = 0;
	dump->start_stack = rdusp() & ~(PAGE_SIZE - 1);
	dump->u_tsize = ((unsigned long) current->mm->end_code) >> PAGE_SHIFT;
	dump->u_dsize = ((unsigned long) (current->mm->brk +
					  (PAGE_SIZE-1))) >> PAGE_SHIFT;
	dump->u_dsize -= dump->u_tsize;
	dump->u_ssize = 0;

	if (dump->start_stack < TASK_SIZE)
		dump->u_ssize = ((unsigned long) (TASK_SIZE - dump->start_stack)) >> PAGE_SHIFT;

	dump->u_ar0 = (struct user_regs_struct *)((int)&dump->regs - (int)dump);
	sw = ((struct switch_stack *)regs) - 1;
	dump->regs.d1 = regs->d1;
	dump->regs.d2 = regs->d2;
	dump->regs.d3 = regs->d3;
	dump->regs.d4 = regs->d4;
	dump->regs.d5 = regs->d5;
	dump->regs.d6 = sw->d6;
	dump->regs.d7 = sw->d7;
	dump->regs.a0 = regs->a0;
	dump->regs.a1 = regs->a1;
	dump->regs.a2 = regs->a2;
	dump->regs.a3 = sw->a3;
	dump->regs.a4 = sw->a4;
	dump->regs.a5 = sw->a5;
	dump->regs.a6 = sw->a6;
	dump->regs.d0 = regs->d0;
	dump->regs.orig_d0 = regs->orig_d0;
	dump->regs.stkadj = regs->stkadj;
	dump->regs.sr = regs->sr;
	dump->regs.pc = regs->pc;
	dump->regs.fmtvec = (regs->format << 12) | regs->vector;
	/* dump floating point stuff */
	dump->u_fpvalid = dump_fpu (regs, &dump->m68kfp);
}

/*
 * sys_execve() executes a new program.
 */
asmlinkage int sys_execve(char *name, char **argv, char **envp)
{
	int error;
	char * filename;
	struct pt_regs *regs = (struct pt_regs *) &name;

	lock_kernel();
	filename = getname(name);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = do_execve(filename, argv, envp, regs);
	putname(filename);
out:
	unlock_kernel();
	return error;
}

/*
 * These bracket the sleeping functions..
 */
extern void scheduling_functions_start_here(void);
extern void scheduling_functions_end_here(void);
#define first_sched	((unsigned long) scheduling_functions_start_here)
#define last_sched	((unsigned long) scheduling_functions_end_here)

unsigned long get_wchan(struct task_struct *p)
{
	unsigned long fp, pc;
	unsigned long stack_page;
	int count = 0;
	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	stack_page = (unsigned long)p;
	fp = ((struct switch_stack *)p->thread.ksp)->a6;
	do {
		if (fp < stack_page+sizeof(struct task_struct) ||
		    fp >= 8184+stack_page)
			return 0;
		pc = ((unsigned long *)fp)[1];
		/* FIXME: This depends on the order of these functions. */
		if (pc < first_sched || pc >= last_sched)
			return pc;
		fp = *(unsigned long *) fp;
	} while (count++ < 16);
	return 0;
}
