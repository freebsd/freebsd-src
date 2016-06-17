/* $Id: process.c,v 1.24 2003/03/06 14:19:32 pkj Exp $
 * 
 *  linux/arch/cris/kernel/process.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Copyright (C) 2000, 2001  Axis Communications AB
 *
 *  Authors:   Bjorn Wesen (bjornw@axis.com)
 *
 *  $Log: process.c,v $
 *  Revision 1.24  2003/03/06 14:19:32  pkj
 *  Use a cpu_idle() function identical to the one used by i386.
 *
 *  Revision 1.23  2002/10/14 18:29:27  johana
 *  Call etrax_gpio_wake_up_check() from cpu_idle() to reduce gpio latency
 *  from ~15 ms to ~6 ms.
 *
 *  Revision 1.22  2001/11/13 09:40:43  orjanf
 *  Added dump_fpu (needed for core dumps).
 *
 *  Revision 1.21  2001/11/12 18:26:21  pkj
 *  Fixed compiler warnings.
 *
 *  Revision 1.20  2001/10/03 08:21:39  jonashg
 *  cause_of_death does not exist if CONFIG_SVINTO_SIM is defined.
 *
 *  Revision 1.19  2001/09/26 11:52:54  bjornw
 *  INIT_MMAP is gone in 2.4.10
 *
 *  Revision 1.18  2001/08/21 21:43:51  hp
 *  Move last watchdog fix inside #ifdef CONFIG_ETRAX_WATCHDOG
 *
 *  Revision 1.17  2001/08/21 13:48:01  jonashg
 *  Added fix by HP to avoid oops when doing a hard_reset_now.
 *
 *  Revision 1.16  2001/06/21 02:00:40  hp
 *  	* entry.S: Include asm/unistd.h.
 *  	(_sys_call_table): Use section .rodata, not .data.
 *  	(_kernel_thread): Move from...
 *  	* process.c: ... here.
 *  	* entryoffsets.c (VAL): Break out from...
 *  	(OF): Use VAL.
 *  	(LCLONE_VM): New asmified value from CLONE_VM.
 *
 *  Revision 1.15  2001/06/20 16:31:57  hp
 *  Add comments to describe empty functions according to review.
 *
 *  Revision 1.14  2001/05/29 11:27:59  markusl
 *  Fixed so that hard_reset_now will do reset even if watchdog wasn't enabled
 *
 *  Revision 1.13  2001/03/20 19:44:06  bjornw
 *  Use the 7th syscall argument for regs instead of current_regs
 *
 */

/*
 * This file handles the architecture-dependent parts of process handling..
 */

#define __KERNEL_SYSCALLS__
#include <stdarg.h>

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/elfcore.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/processor.h>

#include <linux/smp.h>

//#define DEBUG

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

/*
 * Initial task structure.
 *
 * We need to make sure that this is 8192-byte aligned due to the
 * way process stacks are handled. This is done by having a special
 * "init_task" linker map entry..
 */

union task_union init_task_union 
      __attribute__((__section__(".data.init_task"))) =
             { INIT_TASK(init_task_union.task) };

/*
 * The hlt_counter, disable_hlt and enable_hlt is just here as a hook if
 * there would ever be a halt sequence (for power save when idle) with
 * some largish delay when halting or resuming *and* a driver that can't
 * afford that delay.  The hlt_counter would then be checked before
 * executing the halt sequence, and the driver marks the unhaltable
 * region by enable_hlt/disable_hlt.
 */

static int hlt_counter;

/*
 * Powermanagement idle function, if any..
 */
void (*pm_idle)(void);

/*
 * Power off function, if any
 */
void (*pm_power_off)(void);

void disable_hlt(void)
{
	hlt_counter++;
}

void enable_hlt(void)
{
	hlt_counter--;
}

/*
 * We use this if we don't have any better
 * idle routine..
 */
static void default_idle(void)
{
#ifdef CONFIG_ETRAX_GPIO
	extern void etrax_gpio_wake_up_check(void); /* drivers/gpio.c */

	/* This can reduce latency from 15 ms to 6 ms */
	etrax_gpio_wake_up_check(); /* drivers/gpio.c */
#endif
}

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

	while(1) {
		void (*idle)(void) = pm_idle;
		if (!idle)
			idle = default_idle;
		while (!current->need_resched)
			idle();
		schedule();
		check_pgt_cache();
	}
}

/* if the watchdog is enabled, we can simply disable interrupts and go
 * into an eternal loop, and the watchdog will reset the CPU after 0.1s
 * if on the other hand the watchdog wasn't enabled, we just enable it and wait
 */

void hard_reset_now (void)
{
	/*
	 * Don't declare this variable elsewhere.  We don't want any other
	 * code to know about it than the watchdog handler in entry.S and
	 * this code, implementing hard reset through the watchdog.
	 */
#if defined(CONFIG_ETRAX_WATCHDOG) && !defined(CONFIG_SVINTO_SIM)
	extern int cause_of_death;
#endif

	printk("*** HARD RESET ***\n");
	cli();

#if defined(CONFIG_ETRAX_WATCHDOG) && !defined(CONFIG_SVINTO_SIM)
	cause_of_death = 0xbedead;
#else
	/* Since we dont plan to keep on reseting the watchdog,
	   the key can be arbitrary hence three */
	*R_WATCHDOG = IO_FIELD(R_WATCHDOG, key, 3) |
		IO_STATE(R_WATCHDOG, enable, start);
#endif

	while(1) /* waiting for RETRIBUTION! */ ;
}

void machine_restart(void)
{
	hard_reset_now();
}

/*
 * Similar to machine_power_off, but don't shut off power.  Add code
 * here to freeze the system for e.g. post-mortem debug purpose when
 * possible.  This halt has nothing to do with the idle halt.
 */

void machine_halt(void)
{
}

/* If or when software power-off is implemented, add code here.  */

void machine_power_off(void)
{
	if (pm_power_off)
		pm_power_off();
}

/*
 * When a process does an "exec", machine state like FPU and debug
 * registers need to be reset.  This is a hook function for that.
 * Currently we don't have any such state to reset, so this is empty.
 */

void flush_thread(void)
{
}

asmlinkage void ret_from_sys_call(void);

/* setup the child's kernel stack with a pt_regs and switch_stack on it.
 * it will be un-nested during _resume and _ret_from_sys_call when the
 * new thread is scheduled.
 *
 * also setup the thread switching structure which is used to keep
 * thread-specific data during _resumes.
 *
 */

int copy_thread(int nr, unsigned long clone_flags, unsigned long usp,
		unsigned long unused,
		struct task_struct *p, struct pt_regs *regs)
{
	struct pt_regs * childregs;
	struct switch_stack *swstack;
	
	/* put the pt_regs structure at the end of the new kernel stack page and fix it up
	 * remember that the task_struct doubles as the kernel stack for the task
	 */

	childregs = user_regs(p);

	*childregs = *regs;  /* struct copy of pt_regs */

	childregs->r10 = 0;  /* child returns 0 after a fork/clone */
	
	/* put the switch stack right below the pt_regs */

	swstack = ((struct switch_stack *)childregs) - 1;

	swstack->r9 = 0; /* parameter to ret_from_sys_call, 0 == dont restart the syscall */

	/* we want to return into ret_from_sys_call after the _resume */

	swstack->return_ip = (unsigned long) ret_from_sys_call;
	
	/* fix the user-mode stackpointer */

	p->thread.usp = usp;	

	/* and the kernel-mode one */

	p->thread.ksp = (unsigned long) swstack;

#ifdef DEBUG
	printk("copy_thread: new regs at 0x%p, as shown below:\n", childregs);
	show_registers(childregs);
#endif

	return 0;
}

/*
 * fill in the user structure for a core dump..
 */
void dump_thread(struct pt_regs * regs, struct user * dump)
{
#if 0
	int i;

	/* changed the size calculations - should hopefully work better. lbt */
	dump->magic = CMAGIC;
	dump->start_code = 0;
	dump->start_stack = regs->esp & ~(PAGE_SIZE - 1);
	dump->u_tsize = ((unsigned long) current->mm->end_code) >> PAGE_SHIFT;
	dump->u_dsize = ((unsigned long) (current->mm->brk + (PAGE_SIZE-1))) >> PAGE_SHIFT;
	dump->u_dsize -= dump->u_tsize;
	dump->u_ssize = 0;
	for (i = 0; i < 8; i++)
		dump->u_debugreg[i] = current->debugreg[i];  

	if (dump->start_stack < TASK_SIZE)
		dump->u_ssize = ((unsigned long) (TASK_SIZE - dump->start_stack)) >> PAGE_SHIFT;

	dump->regs = *regs;

	dump->u_fpvalid = dump_fpu (regs, &dump->i387);
#endif 
}

/* Fill in the fpu structure for a core dump. */
int dump_fpu(struct pt_regs *regs, elf_fpregset_t *fpu)
{
        return 0;
}

/* 
 * Be aware of the "magic" 7th argument in the four system-calls below.
 * They need the latest stackframe, which is put as the 7th argument by
 * entry.S. The previous arguments are dummies or actually used, but need
 * to be defined to reach the 7th argument.
 *
 * N.B.: Another method to get the stackframe is to use current_regs(). But
 * it returns the latest stack-frame stacked when going from _user mode_ and
 * some of these (at least sys_clone) are called from kernel-mode sometimes
 * (for example during kernel_thread, above) and thus cannot use it. Thus,
 * to be sure not to get any surprises, we use the method for the other calls
 * as well.
 */

asmlinkage int sys_fork(long r10, long r11, long r12, long r13, long mof, long srp,
			struct pt_regs *regs)
{
	return do_fork(SIGCHLD, rdusp(), regs, 0);
}

/* if newusp is 0, we just grab the old usp */

asmlinkage int sys_clone(unsigned long newusp, unsigned long flags,
			 long r12, long r13, long mof, long srp,
			 struct pt_regs *regs)
{
	if (!newusp)
		newusp = rdusp();
	return do_fork(flags, newusp, regs, 0);
}

/* vfork is a system call in i386 because of register-pressure - maybe
 * we can remove it and handle it in libc but we put it here until then.
 */

asmlinkage int sys_vfork(long r10, long r11, long r12, long r13, long mof, long srp,
			 struct pt_regs *regs)
{
        return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, rdusp(), regs, 0);
}

/*
 * sys_execve() executes a new program.
 */
asmlinkage int sys_execve(const char *fname, char **argv, char **envp,
			  long r13, long mof, long srp, 
			  struct pt_regs *regs)
{
	int error;
	char *filename;

	filename = getname(fname);
	error = PTR_ERR(filename);

	if (IS_ERR(filename))
	        goto out;
	error = do_execve(filename, argv, envp, regs);
	putname(filename);
 out:
	return error;
}

/*
 * These bracket the sleeping functions..
 */

extern void scheduling_functions_start_here(void);
extern void scheduling_functions_end_here(void);
#define first_sched     ((unsigned long) scheduling_functions_start_here)
#define last_sched      ((unsigned long) scheduling_functions_end_here)

unsigned long get_wchan(struct task_struct *p)
{
#if 0
	/* YURGH. TODO. */

        unsigned long ebp, esp, eip;
        unsigned long stack_page;
        int count = 0;
        if (!p || p == current || p->state == TASK_RUNNING)
                return 0;
        stack_page = (unsigned long)p;
        esp = p->thread.esp;
        if (!stack_page || esp < stack_page || esp > 8188+stack_page)
                return 0;
        /* include/asm-i386/system.h:switch_to() pushes ebp last. */
        ebp = *(unsigned long *) esp;
        do {
                if (ebp < stack_page || ebp > 8184+stack_page)
                        return 0;
                eip = *(unsigned long *) (ebp+4);
                if (eip < first_sched || eip >= last_sched)
                        return eip;
                ebp = *(unsigned long *) ebp;
        } while (count++ < 16);
#endif
        return 0;
}
#undef last_sched
#undef first_sched
