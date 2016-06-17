/*  $Id: process.c,v 1.125.2.1 2001/12/18 19:40:17 davem Exp $
 *  arch/sparc64/kernel/process.c
 *
 *  Copyright (C) 1995, 1996 David S. Miller (davem@caip.rutgers.edu)
 *  Copyright (C) 1996       Eddie C. Dost   (ecd@skynet.be)
 *  Copyright (C) 1997, 1998 Jakub Jelinek   (jj@sunsite.mff.cuni.cz)
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
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/config.h>
#include <linux/reboot.h>
#include <linux/delay.h>

#include <asm/oplib.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/pstate.h>
#include <asm/elf.h>
#include <asm/fpumacro.h>
#include <asm/head.h>

/* #define VERBOSE_SHOWREGS */

#ifndef CONFIG_SMP

/*
 * the idle loop on a Sparc... ;)
 */
int cpu_idle(void)
{
	if (current->pid != 0)
		return -EPERM;

	/* endless idle loop with no priority at all */
	current->nice = 20;
	current->counter = -100;
	init_idle();

	for (;;) {
		/* If current->need_resched is zero we should really
		 * setup for a system wakup event and execute a shutdown
		 * instruction.
		 *
		 * But this requires writing back the contents of the
		 * L2 cache etc. so implement this later. -DaveM
		 */
		while (!current->need_resched)
			barrier();

		schedule();
		check_pgt_cache();
	}
	return 0;
}

#else

/*
 * the idle loop on a UltraMultiPenguin...
 */
#define idle_me_harder()	(cpu_data[current->processor].idle_volume += 1)
#define unidle_me()		(cpu_data[current->processor].idle_volume = 0)
int cpu_idle(void)
{
	current->nice = 20;
	current->counter = -100;
	init_idle();

	while(1) {
		if (current->need_resched != 0) {
			unidle_me();
			schedule();
			check_pgt_cache();
		}
		idle_me_harder();

		/* The store ordering is so that IRQ handlers on
		 * other cpus see our increasing idleness for the buddy
		 * redistribution algorithm.  -DaveM
		 */
		membar("#StoreStore | #StoreLoad");
	}
}

#endif

extern char reboot_command [];

#ifdef CONFIG_SUN_CONSOLE
extern void (*prom_palette)(int);
extern int serial_console;
#endif
extern void (*prom_keyboard)(void);

void machine_halt(void)
{
	sti();
	mdelay(8);
	cli();
#ifdef CONFIG_SUN_CONSOLE
	if (!serial_console && prom_palette)
		prom_palette (1);
#endif
	if (prom_keyboard)
		prom_keyboard();
	prom_halt();
	panic("Halt failed!");
}

void machine_alt_power_off(void)
{
	sti();
	mdelay(8);
	cli();
#ifdef CONFIG_SUN_CONSOLE
	if (!serial_console && prom_palette)
		prom_palette(1);
#endif
	if (prom_keyboard)
		prom_keyboard();
	prom_halt_power_off();
	panic("Power-off failed!");
}

void machine_restart(char * cmd)
{
	char *p;
	
	sti();
	mdelay(8);
	cli();

	p = strchr (reboot_command, '\n');
	if (p) *p = 0;
#ifdef CONFIG_SUN_CONSOLE
	if (!serial_console && prom_palette)
		prom_palette (1);
#endif
	if (prom_keyboard)
		prom_keyboard();
	if (cmd)
		prom_reboot(cmd);
	if (*reboot_command)
		prom_reboot(reboot_command);
	prom_reboot("");
	panic("Reboot failed!");
}

static void show_regwindow32(struct pt_regs *regs)
{
	struct reg_window32 *rw;
	struct reg_window32 r_w;
	mm_segment_t old_fs;
	
	__asm__ __volatile__ ("flushw");
	rw = (struct reg_window32 *)((long)(unsigned)regs->u_regs[14]);
	old_fs = get_fs();
	set_fs (USER_DS);
	if (copy_from_user (&r_w, rw, sizeof(r_w))) {
		set_fs (old_fs);
		return;
	}
	rw = &r_w;
	set_fs (old_fs);			
	printk("l0: %08x l1: %08x l2: %08x l3: %08x "
	       "l4: %08x l5: %08x l6: %08x l7: %08x\n",
	       rw->locals[0], rw->locals[1], rw->locals[2], rw->locals[3],
	       rw->locals[4], rw->locals[5], rw->locals[6], rw->locals[7]);
	printk("i0: %08x i1: %08x i2: %08x i3: %08x "
	       "i4: %08x i5: %08x i6: %08x i7: %08x\n",
	       rw->ins[0], rw->ins[1], rw->ins[2], rw->ins[3],
	       rw->ins[4], rw->ins[5], rw->ins[6], rw->ins[7]);
}

static void show_regwindow(struct pt_regs *regs)
{
	struct reg_window *rw;
	struct reg_window r_w;
	mm_segment_t old_fs;

	if ((regs->tstate & TSTATE_PRIV) || !(current->thread.flags & SPARC_FLAG_32BIT)) {
		__asm__ __volatile__ ("flushw");
		rw = (struct reg_window *)(regs->u_regs[14] + STACK_BIAS);
		if (!(regs->tstate & TSTATE_PRIV)) {
			old_fs = get_fs();
			set_fs (USER_DS);
			if (copy_from_user (&r_w, rw, sizeof(r_w))) {
				set_fs (old_fs);
				return;
			}
			rw = &r_w;
			set_fs (old_fs);			
		}
	} else {
		show_regwindow32(regs);
		return;
	}
	printk("l0: %016lx l1: %016lx l2: %016lx l3: %016lx\n",
	       rw->locals[0], rw->locals[1], rw->locals[2], rw->locals[3]);
	printk("l4: %016lx l5: %016lx l6: %016lx l7: %016lx\n",
	       rw->locals[4], rw->locals[5], rw->locals[6], rw->locals[7]);
	printk("i0: %016lx i1: %016lx i2: %016lx i3: %016lx\n",
	       rw->ins[0], rw->ins[1], rw->ins[2], rw->ins[3]);
	printk("i4: %016lx i5: %016lx i6: %016lx i7: %016lx\n",
	       rw->ins[4], rw->ins[5], rw->ins[6], rw->ins[7]);
}

void show_stackframe(struct sparc_stackf *sf)
{
	unsigned long size;
	unsigned long *stk;
	int i;

	printk("l0: %016lx l1: %016lx l2: %016lx l3: %016lx\n"
	       "l4: %016lx l5: %016lx l6: %016lx l7: %016lx\n",
	       sf->locals[0], sf->locals[1], sf->locals[2], sf->locals[3],
	       sf->locals[4], sf->locals[5], sf->locals[6], sf->locals[7]);
	printk("i0: %016lx i1: %016lx i2: %016lx i3: %016lx\n"
	       "i4: %016lx i5: %016lx fp: %016lx ret_pc: %016lx\n",
	       sf->ins[0], sf->ins[1], sf->ins[2], sf->ins[3],
	       sf->ins[4], sf->ins[5], (unsigned long)sf->fp, sf->callers_pc);
	printk("sp: %016lx x0: %016lx x1: %016lx x2: %016lx\n"
	       "x3: %016lx x4: %016lx x5: %016lx xx: %016lx\n",
	       (unsigned long)sf->structptr, sf->xargs[0], sf->xargs[1],
	       sf->xargs[2], sf->xargs[3], sf->xargs[4], sf->xargs[5],
	       sf->xxargs[0]);
	size = ((unsigned long)sf->fp) - ((unsigned long)sf);
	size -= STACKFRAME_SZ;
	stk = (unsigned long *)((unsigned long)sf + STACKFRAME_SZ);
	i = 0;
	do {
		printk("s%d: %016lx\n", i++, *stk++);
	} while ((size -= sizeof(unsigned long)));
}

void show_stackframe32(struct sparc_stackf32 *sf)
{
	unsigned long size;
	unsigned *stk;
	int i;

	printk("l0: %08x l1: %08x l2: %08x l3: %08x\n",
	       sf->locals[0], sf->locals[1], sf->locals[2], sf->locals[3]);
	printk("l4: %08x l5: %08x l6: %08x l7: %08x\n",
	       sf->locals[4], sf->locals[5], sf->locals[6], sf->locals[7]);
	printk("i0: %08x i1: %08x i2: %08x i3: %08x\n",
	       sf->ins[0], sf->ins[1], sf->ins[2], sf->ins[3]);
	printk("i4: %08x i5: %08x fp: %08x ret_pc: %08x\n",
	       sf->ins[4], sf->ins[5], sf->fp, sf->callers_pc);
	printk("sp: %08x x0: %08x x1: %08x x2: %08x\n"
	       "x3: %08x x4: %08x x5: %08x xx: %08x\n",
	       sf->structptr, sf->xargs[0], sf->xargs[1],
	       sf->xargs[2], sf->xargs[3], sf->xargs[4], sf->xargs[5],
	       sf->xxargs[0]);
	size = ((unsigned long)sf->fp) - ((unsigned long)sf);
	size -= STACKFRAME32_SZ;
	stk = (unsigned *)((unsigned long)sf + STACKFRAME32_SZ);
	i = 0;
	do {
		printk("s%d: %08x\n", i++, *stk++);
	} while ((size -= sizeof(unsigned)));
}

#ifdef CONFIG_SMP
static spinlock_t regdump_lock = SPIN_LOCK_UNLOCKED;
#endif

void __show_regs(struct pt_regs * regs)
{
#ifdef CONFIG_SMP
	unsigned long flags;

	/* Protect against xcall ipis which might lead to livelock on the lock */
	__asm__ __volatile__("rdpr      %%pstate, %0\n\t"
			     "wrpr      %0, %1, %%pstate"
			     : "=r" (flags)
			     : "i" (PSTATE_IE));
	spin_lock(&regdump_lock);
	printk("CPU[%d]: local_irq_count[%u] irqs_running[%d]\n",
	       smp_processor_id(),
	       local_irq_count(smp_processor_id()),
	       irqs_running());
#endif
	printk("TSTATE: %016lx TPC: %016lx TNPC: %016lx Y: %08x    %s\n", regs->tstate,
	       regs->tpc, regs->tnpc, regs->y, print_tainted());
	printk("g0: %016lx g1: %016lx g2: %016lx g3: %016lx\n",
	       regs->u_regs[0], regs->u_regs[1], regs->u_regs[2],
	       regs->u_regs[3]);
	printk("g4: %016lx g5: %016lx g6: %016lx g7: %016lx\n",
	       regs->u_regs[4], regs->u_regs[5], regs->u_regs[6],
	       regs->u_regs[7]);
	printk("o0: %016lx o1: %016lx o2: %016lx o3: %016lx\n",
	       regs->u_regs[8], regs->u_regs[9], regs->u_regs[10],
	       regs->u_regs[11]);
	printk("o4: %016lx o5: %016lx sp: %016lx ret_pc: %016lx\n",
	       regs->u_regs[12], regs->u_regs[13], regs->u_regs[14],
	       regs->u_regs[15]);
	show_regwindow(regs);
#ifdef CONFIG_SMP
	spin_unlock(&regdump_lock);
	__asm__ __volatile__("wrpr	%0, 0, %%pstate"
			     : : "r" (flags));
#endif
}

#ifdef VERBOSE_SHOWREGS
static void idump_from_user (unsigned int *pc)
{
	int i;
	int code;
	
	if((((unsigned long) pc) & 3))
		return;
	
	pc -= 3;
	for(i = -3; i < 6; i++) {
		get_user(code, pc);
		printk("%c%08x%c",i?' ':'<',code,i?' ':'>');
		pc++;
	}
	printk("\n");
}
#endif

void show_regs(struct pt_regs *regs)
{
#ifdef VERBOSE_SHOWREGS
	extern long etrap, etraptl1;
#endif
	__show_regs(regs);
#ifdef CONFIG_SMP
	{
		extern void smp_report_regs(void);

		smp_report_regs();
	}
#endif

#ifdef VERBOSE_SHOWREGS	
	if (regs->tpc >= &etrap && regs->tpc < &etraptl1 &&
	    regs->u_regs[14] >= (long)current - PAGE_SIZE &&
	    regs->u_regs[14] < (long)current + 6 * PAGE_SIZE) {
		printk ("*********parent**********\n");
		__show_regs((struct pt_regs *)(regs->u_regs[14] + PTREGS_OFF));
		idump_from_user(((struct pt_regs *)(regs->u_regs[14] + PTREGS_OFF))->tpc);
		printk ("*********endpar**********\n");
	}
#endif
}

void show_regs32(struct pt_regs32 *regs)
{
	printk("PSR: %08x PC: %08x NPC: %08x Y: %08x    %s\n", regs->psr,
	       regs->pc, regs->npc, regs->y, print_tainted());
	printk("g0: %08x g1: %08x g2: %08x g3: %08x ",
	       regs->u_regs[0], regs->u_regs[1], regs->u_regs[2],
	       regs->u_regs[3]);
	printk("g4: %08x g5: %08x g6: %08x g7: %08x\n",
	       regs->u_regs[4], regs->u_regs[5], regs->u_regs[6],
	       regs->u_regs[7]);
	printk("o0: %08x o1: %08x o2: %08x o3: %08x ",
	       regs->u_regs[8], regs->u_regs[9], regs->u_regs[10],
	       regs->u_regs[11]);
	printk("o4: %08x o5: %08x sp: %08x ret_pc: %08x\n",
	       regs->u_regs[12], regs->u_regs[13], regs->u_regs[14],
	       regs->u_regs[15]);
}

void show_thread(struct thread_struct *thread)
{
	int i;

#if 0
	printk("kregs:             0x%016lx\n", (unsigned long)thread->kregs);
	show_regs(thread->kregs);
#endif	
	printk("ksp:               0x%016lx\n", thread->ksp);

	if (thread->w_saved) {
		for (i = 0; i < NSWINS; i++) {
			if (!thread->rwbuf_stkptrs[i])
				continue;
			printk("reg_window[%d]:\n", i);
			printk("stack ptr:         0x%016lx\n", thread->rwbuf_stkptrs[i]);
		}
		printk("w_saved:           0x%04x\n", thread->w_saved);
	}

	printk("flags:             0x%08x\n", thread->flags);
	printk("current_ds:        0x%x\n", thread->current_ds.seg);
}

/* Free current thread data structures etc.. */
void exit_thread(void)
{
	struct thread_struct *t = &current->thread;

	if (t->utraps) {
		if (t->utraps[0] < 2)
			kfree (t->utraps);
		else
			t->utraps[0]--;
	}

	/* Turn off performance counters if on. */
	if (t->flags & SPARC_FLAG_PERFCTR) {
		t->user_cntd0 = t->user_cntd1 = NULL;
		t->pcr_reg = 0;
		t->flags &= ~(SPARC_FLAG_PERFCTR);
		write_pcr(0);
	}
}

void flush_thread(void)
{
	struct thread_struct *t = &current->thread;

	if (t->flags & SPARC_FLAG_ABI_PENDING)
		t->flags ^= (SPARC_FLAG_ABI_PENDING |
			     SPARC_FLAG_32BIT);
	if (current->mm) {
		unsigned long pgd_cache = 0UL;

		if (t->flags & SPARC_FLAG_32BIT) {
			struct mm_struct *mm = current->mm;
			pgd_t *pgd0 = &mm->pgd[0];

			if (pgd_none(*pgd0)) {
				pmd_t *page = pmd_alloc_one_fast(NULL, 0);
				if (!page)
					page = pmd_alloc_one(NULL, 0);
				pgd_set(pgd0, page);
			}
			pgd_cache = pgd_val(*pgd0) << 11UL;
		}
		__asm__ __volatile__("stxa %0, [%1] %2\n\t"
				     "membar #Sync"
				     : /* no outputs */
				     : "r" (pgd_cache),
				     "r" (TSB_REG),
				     "i" (ASI_DMMU));
	}
	t->w_saved = 0;

	/* Turn off performance counters if on. */
	if (t->flags & SPARC_FLAG_PERFCTR) {
		t->user_cntd0 = t->user_cntd1 = NULL;
		t->pcr_reg = 0;
		t->flags &= ~(SPARC_FLAG_PERFCTR);
		write_pcr(0);
	}

	/* Clear FPU register state. */
	t->fpsaved[0] = 0;
	
	if (t->current_ds.seg != ASI_AIUS)
		set_fs(USER_DS);

	/* Init new signal delivery disposition. */
	t->flags &= ~SPARC_FLAG_NEWSIGNALS;
}

/* It's a bit more tricky when 64-bit tasks are involved... */
static unsigned long clone_stackframe(unsigned long csp, unsigned long psp)
{
	unsigned long fp, distance, rval;

	if (!(current->thread.flags & SPARC_FLAG_32BIT)) {
		csp += STACK_BIAS;
		psp += STACK_BIAS;
		__get_user(fp, &(((struct reg_window *)psp)->ins[6]));
		fp += STACK_BIAS;
	} else
		__get_user(fp, &(((struct reg_window32 *)psp)->ins[6]));

	/* Now 8-byte align the stack as this is mandatory in the
	 * Sparc ABI due to how register windows work.  This hides
	 * the restriction from thread libraries etc.  -DaveM
	 */
	csp &= ~7UL;

	distance = fp - psp;
	rval = (csp - distance);
	if (copy_in_user(rval, psp, distance))
		rval = 0;
	else if (current->thread.flags & SPARC_FLAG_32BIT) {
		if (put_user(((u32)csp), &(((struct reg_window32 *)rval)->ins[6])))
			rval = 0;
	} else {
		if (put_user(((u64)csp - STACK_BIAS),
			     &(((struct reg_window *)rval)->ins[6])))
			rval = 0;
		else
			rval = rval - STACK_BIAS;
	}

	return rval;
}

/* Standard stuff. */
static inline void shift_window_buffer(int first_win, int last_win,
				       struct thread_struct *t)
{
	int i;

	for (i = first_win; i < last_win; i++) {
		t->rwbuf_stkptrs[i] = t->rwbuf_stkptrs[i+1];
		memcpy(&t->reg_window[i], &t->reg_window[i+1],
		       sizeof(struct reg_window));
	}
}

void synchronize_user_stack(void)
{
	struct thread_struct *t = &current->thread;
	unsigned long window;

	flush_user_windows();
	if ((window = t->w_saved) != 0) {
		int winsize = sizeof(struct reg_window);
		int bias = 0;

		if (t->flags & SPARC_FLAG_32BIT)
			winsize = sizeof(struct reg_window32);
		else
			bias = STACK_BIAS;

		window -= 1;
		do {
			unsigned long sp = (t->rwbuf_stkptrs[window] + bias);
			struct reg_window *rwin = &t->reg_window[window];

			if (!copy_to_user((char *)sp, rwin, winsize)) {
				shift_window_buffer(window, t->w_saved - 1, t);
				t->w_saved--;
			}
		} while (window--);
	}
}

void fault_in_user_windows(void)
{
	struct thread_struct *t = &current->thread;
	unsigned long window;
	int winsize = sizeof(struct reg_window);
	int bias = 0;

	if (t->flags & SPARC_FLAG_32BIT)
		winsize = sizeof(struct reg_window32);
	else
		bias = STACK_BIAS;

	flush_user_windows();
	window = t->w_saved;

	if (window != 0) {
		window -= 1;
		do {
			unsigned long sp = (t->rwbuf_stkptrs[window] + bias);
			struct reg_window *rwin = &t->reg_window[window];

			if (copy_to_user((char *)sp, rwin, winsize))
				goto barf;
		} while (window--);
	}
	t->w_saved = 0;
	return;

barf:
	t->w_saved = window + 1;
	do_exit(SIGILL);
}

/* Copy a Sparc thread.  The fork() return value conventions
 * under SunOS are nothing short of bletcherous:
 * Parent -->  %o0 == childs  pid, %o1 == 0
 * Child  -->  %o0 == parents pid, %o1 == 1
 */
int copy_thread(int nr, unsigned long clone_flags, unsigned long sp,
		unsigned long unused,
		struct task_struct *p, struct pt_regs *regs)
{
	struct thread_struct *t = &p->thread;
	char *child_trap_frame;

#ifdef CONFIG_DEBUG_SPINLOCK
	t->smp_lock_count = 0;
	t->smp_lock_pc = 0;
#endif

	/* Calculate offset to stack_frame & pt_regs */
	child_trap_frame = ((char *)p) + (THREAD_SIZE - (TRACEREG_SZ+STACKFRAME_SZ));
	memcpy(child_trap_frame, (((struct sparc_stackf *)regs)-1), (TRACEREG_SZ+STACKFRAME_SZ));
	t->ksp = ((unsigned long) child_trap_frame) - STACK_BIAS;
	t->flags |= SPARC_FLAG_NEWCHILD;
	t->kregs = (struct pt_regs *)(child_trap_frame+sizeof(struct sparc_stackf));
	t->cwp = (regs->tstate + 1) & TSTATE_CWP;
	t->fpsaved[0] = 0;

	if (regs->tstate & TSTATE_PRIV) {
		/* Special case, if we are spawning a kernel thread from
		 * a userspace task (via KMOD, NFS, or similar) we must
		 * disable performance counters in the child because the
		 * address space and protection realm are changing.
		 */
		if (t->flags & SPARC_FLAG_PERFCTR) {
			t->user_cntd0 = t->user_cntd1 = NULL;
			t->pcr_reg = 0;
			t->flags &= ~(SPARC_FLAG_PERFCTR);
		}
		t->kregs->u_regs[UREG_FP] = p->thread.ksp;
		t->current_ds = KERNEL_DS;
		flush_register_windows();
		memcpy((void *)(t->ksp + STACK_BIAS),
		       (void *)(regs->u_regs[UREG_FP] + STACK_BIAS),
		       sizeof(struct sparc_stackf));
		t->kregs->u_regs[UREG_G6] = (unsigned long) p;
	} else {
		if (t->flags & SPARC_FLAG_32BIT) {
			sp &= 0x00000000ffffffffUL;
			regs->u_regs[UREG_FP] &= 0x00000000ffffffffUL;
		}
		t->kregs->u_regs[UREG_FP] = sp;
		t->current_ds = USER_DS;
		if (sp != regs->u_regs[UREG_FP]) {
			unsigned long csp;

			csp = clone_stackframe(sp, regs->u_regs[UREG_FP]);
			if (!csp)
				return -EFAULT;
			t->kregs->u_regs[UREG_FP] = csp;
		}
		if (t->utraps)
			t->utraps[0]++;
	}

	/* Set the return value for the child. */
	t->kregs->u_regs[UREG_I0] = current->pid;
	t->kregs->u_regs[UREG_I1] = 1;

	/* Set the second return value for the parent. */
	regs->u_regs[UREG_I1] = 0;

	return 0;
}

/*
 * This is the mechanism for creating a new kernel thread.
 *
 * NOTE! Only a kernel-only process(ie the swapper or direct descendants
 * who haven't done an "execve()") should use this: it will work within
 * a system call from a "real" process, but the process memory space will
 * not be free'd until both the parent and the child have exited.
 */
pid_t arch_kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	long retval;

	/* If the parent runs before fn(arg) is called by the child,
	 * the input registers of this function can be clobbered.
	 * So we stash 'fn' and 'arg' into global registers which
	 * will not be modified by the parent.
	 */
	__asm__ __volatile("mov %4, %%g2\n\t"	   /* Save FN into global */
			   "mov %5, %%g3\n\t"	   /* Save ARG into global */
			   "mov %1, %%g1\n\t"	   /* Clone syscall nr. */
			   "mov %2, %%o0\n\t"	   /* Clone flags. */
			   "mov 0, %%o1\n\t"	   /* usp arg == 0 */
			   "t 0x6d\n\t"		   /* Linux/Sparc clone(). */
			   "brz,a,pn %%o1, 1f\n\t" /* Parent, just return. */
			   " mov %%o0, %0\n\t"
			   "jmpl %%g2, %%o7\n\t"   /* Call the function. */
			   " mov %%g3, %%o0\n\t"   /* Set arg in delay. */
			   "mov %3, %%g1\n\t"
			   "t 0x6d\n\t"		   /* Linux/Sparc exit(). */
			   /* Notreached by child. */
			   "1:" :
			   "=r" (retval) :
			   "i" (__NR_clone), "r" (flags | CLONE_VM),
			   "i" (__NR_exit),  "r" (fn), "r" (arg) :
			   "g1", "g2", "g3", "o0", "o1", "memory", "cc");
	return retval;
}

/*
 * fill in the user structure for a core dump..
 */
void dump_thread(struct pt_regs * regs, struct user * dump)
{
#if 1
	/* Only should be used for SunOS and ancient a.out
	 * SparcLinux binaries...  Fixme some day when bored.
	 * But for now at least plug the security hole :-)
	 */
	memset(dump, 0, sizeof(struct user));
#else
	unsigned long first_stack_page;
	dump->magic = SUNOS_CORE_MAGIC;
	dump->len = sizeof(struct user);
	dump->regs.psr = regs->psr;
	dump->regs.pc = regs->pc;
	dump->regs.npc = regs->npc;
	dump->regs.y = regs->y;
	/* fuck me plenty */
	memcpy(&dump->regs.regs[0], &regs->u_regs[1], (sizeof(unsigned long) * 15));
	dump->u_tsize = (((unsigned long) current->mm->end_code) -
		((unsigned long) current->mm->start_code)) & ~(PAGE_SIZE - 1);
	dump->u_dsize = ((unsigned long) (current->mm->brk + (PAGE_SIZE-1)));
	dump->u_dsize -= dump->u_tsize;
	dump->u_dsize &= ~(PAGE_SIZE - 1);
	first_stack_page = (regs->u_regs[UREG_FP] & ~(PAGE_SIZE - 1));
	dump->u_ssize = (TASK_SIZE - first_stack_page) & ~(PAGE_SIZE - 1);
	memcpy(&dump->fpu.fpstatus.fregs.regs[0], &current->thread.float_regs[0], (sizeof(unsigned long) * 32));
	dump->fpu.fpstatus.fsr = current->thread.fsr;
	dump->fpu.fpstatus.flags = dump->fpu.fpstatus.extra = 0;
#endif	
}

typedef struct {
	union {
		unsigned int	pr_regs[32];
		unsigned long	pr_dregs[16];
	} pr_fr;
	unsigned int __unused;
	unsigned int	pr_fsr;
	unsigned char	pr_qcnt;
	unsigned char	pr_q_entrysize;
	unsigned char	pr_en;
	unsigned int	pr_q[64];
} elf_fpregset_t32;

/*
 * fill in the fpu structure for a core dump.
 */
int dump_fpu (struct pt_regs * regs, elf_fpregset_t * fpregs)
{
	unsigned long *kfpregs = (unsigned long *)(((char *)current) + AOFF_task_fpregs);
	unsigned long fprs = current->thread.fpsaved[0];

	if ((current->thread.flags & SPARC_FLAG_32BIT) != 0) {
		elf_fpregset_t32 *fpregs32 = (elf_fpregset_t32 *)fpregs;

		if (fprs & FPRS_DL)
			memcpy(&fpregs32->pr_fr.pr_regs[0], kfpregs,
			       sizeof(unsigned int) * 32);
		else
			memset(&fpregs32->pr_fr.pr_regs[0], 0,
			       sizeof(unsigned int) * 32);
		fpregs32->pr_qcnt = 0;
		fpregs32->pr_q_entrysize = 8;
		memset(&fpregs32->pr_q[0], 0,
		       (sizeof(unsigned int) * 64));
		if (fprs & FPRS_FEF) {
			fpregs32->pr_fsr = (unsigned int) current->thread.xfsr[0];
			fpregs32->pr_en = 1;
		} else {
			fpregs32->pr_fsr = 0;
			fpregs32->pr_en = 0;
		}
	} else {
		if(fprs & FPRS_DL)
			memcpy(&fpregs->pr_regs[0], kfpregs,
			       sizeof(unsigned int) * 32);
		else
			memset(&fpregs->pr_regs[0], 0,
			       sizeof(unsigned int) * 32);
		if(fprs & FPRS_DU)
			memcpy(&fpregs->pr_regs[16], kfpregs+16,
			       sizeof(unsigned int) * 32);
		else
			memset(&fpregs->pr_regs[16], 0,
			       sizeof(unsigned int) * 32);
		if(fprs & FPRS_FEF) {
			fpregs->pr_fsr = current->thread.xfsr[0];
			fpregs->pr_gsr = current->thread.gsr[0];
		} else {
			fpregs->pr_fsr = fpregs->pr_gsr = 0;
		}
		fpregs->pr_fprs = fprs;
	}
	return 1;
}

/*
 * sparc_execve() executes a new program after the asm stub has set
 * things up for us.  This should basically do what I want it to.
 */
asmlinkage int sparc_execve(struct pt_regs *regs)
{
	int error, base = 0;
	char *filename;

	/* User register window flush is done by entry.S */

	/* Check for indirect call. */
	if (regs->u_regs[UREG_G1] == 0)
		base = 1;

	filename = getname((char *)regs->u_regs[base + UREG_I0]);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = do_execve(filename, (char **) regs->u_regs[base + UREG_I1],
			  (char **) regs->u_regs[base + UREG_I2], regs);
	putname(filename);
	if (!error) {
		fprs_write(0);
		current->thread.xfsr[0] = 0;
		current->thread.fpsaved[0] = 0;
		regs->tstate &= ~TSTATE_PEF;
	}
out:
	return error;
}
