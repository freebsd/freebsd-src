/*  $Id: process.c,v 1.158 2001/11/26 23:45:00 davem Exp $
 *  linux/arch/sparc/kernel/process.c
 *
 *  Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 *  Copyright (C) 1996 Eddie C. Dost   (ecd@skynet.be)
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
#include <linux/config.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/pm.h>

#include <asm/auxio.h>
#include <asm/oplib.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/delay.h>
#include <asm/processor.h>
#include <asm/psr.h>
#include <asm/elf.h>

/* 
 * Power management idle function 
 * Set in pm platform drivers
 */
void (*pm_idle)(void);

/* 
 * Power-off handler instantiation for pm.h compliance
 * This is done via auxio, but could be used as a fallback
 * handler when auxio is not present-- unused for now...
 */
void (*pm_power_off)(void);

extern void fpsave(unsigned long *, unsigned long *, void *, unsigned long *);

struct task_struct *last_task_used_math = NULL;
struct task_struct *current_set[NR_CPUS] = {&init_task, };

#ifndef CONFIG_SMP

#define SUN4C_FAULT_HIGH 100

/*
 * the idle loop on a Sparc... ;)
 */
int cpu_idle(void)
{
	int ret = -EPERM;

	if (current->pid != 0)
		goto out;

	/* endless idle loop with no priority at all */
	current->nice = 20;
	current->counter = -100;
	init_idle();

	for (;;) {
		if (ARCH_SUN4C_SUN4) {
			static int count = HZ;
			static unsigned long last_jiffies;
			static unsigned long last_faults;
			static unsigned long fps;
			unsigned long now;
			unsigned long faults;
			unsigned long flags;

			extern unsigned long sun4c_kernel_faults;
			extern void sun4c_grow_kernel_ring(void);

			save_and_cli(flags);
			now = jiffies;
			count -= (now - last_jiffies);
			last_jiffies = now;
			if (count < 0) {
				count += HZ;
				faults = sun4c_kernel_faults;
				fps = (fps + (faults - last_faults)) >> 1;
				last_faults = faults;
#if 0
				printk("kernel faults / second = %ld\n", fps);
#endif
				if (fps >= SUN4C_FAULT_HIGH) {
					sun4c_grow_kernel_ring();
				}
			}
			restore_flags(flags);
		}

		while((!current->need_resched) && pm_idle) {
				(*pm_idle)();
		}

		schedule();
		check_pgt_cache();
	}
	ret = 0;
out:
	return ret;
}

#else

/* This is being executed in task 0 'user space'. */
int cpu_idle(void)
{
	/* endless idle loop with no priority at all */
	current->nice = 20;
	current->counter = -100;
	init_idle();

	while(1) {
		if(current->need_resched) {
			schedule();
			check_pgt_cache();
		}
		barrier(); /* or else gcc optimizes... */
	}
}

#endif

extern char reboot_command [];

extern int serial_console;

#ifdef CONFIG_SUN_CONSOLE
extern void (*prom_palette)(int);
#endif

void machine_halt(void)
{
	sti();
	mdelay(8);
	cli();
#ifdef CONFIG_SUN_CONSOLE
	if (!serial_console && prom_palette)
		prom_palette (1);
#endif
	prom_halt();
	panic("Halt failed!");
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
	if (cmd)
		prom_reboot(cmd);
	if (*reboot_command)
		prom_reboot(reboot_command);
	prom_feval ("reset");
	panic("Reboot failed!");
}

void machine_power_off(void)
{
#ifdef CONFIG_SUN_AUXIO
	if (auxio_power_register && !serial_console)
		*auxio_power_register |= AUXIO_POWER_OFF;
#endif
	machine_halt();
}

void show_regwindow(struct reg_window *rw)
{
	printk("l0: %08lx l1: %08lx l2: %08lx l3: %08lx "
	       "l4: %08lx l5: %08lx l6: %08lx l7: %08lx\n",
	       rw->locals[0], rw->locals[1], rw->locals[2], rw->locals[3],
	       rw->locals[4], rw->locals[5], rw->locals[6], rw->locals[7]);
	printk("i0: %08lx i1: %08lx i2: %08lx i3: %08lx "
	       "i4: %08lx i5: %08lx fp: %08lx i7: %08lx\n",
	       rw->ins[0], rw->ins[1], rw->ins[2], rw->ins[3],
	       rw->ins[4], rw->ins[5], rw->ins[6], rw->ins[7]);
}

static spinlock_t sparc_backtrace_lock = SPIN_LOCK_UNLOCKED;

void __show_backtrace(unsigned long fp)
{
	struct reg_window *rw;
	unsigned long flags;
	int cpu = smp_processor_id();

	spin_lock_irqsave(&sparc_backtrace_lock, flags);

	rw = (struct reg_window *)fp;
        while(rw && (((unsigned long) rw) >= PAGE_OFFSET) &&
            !(((unsigned long) rw) & 0x7)) {
		printk("CPU[%d]: ARGS[%08lx,%08lx,%08lx,%08lx,%08lx,%08lx] "
		       "FP[%08lx] CALLER[%08lx]\n", cpu,
		       rw->ins[0], rw->ins[1], rw->ins[2], rw->ins[3],
		       rw->ins[4], rw->ins[5],
		       rw->ins[6],
		       rw->ins[7]);
		rw = (struct reg_window *) rw->ins[6];
	}
	spin_unlock_irqrestore(&sparc_backtrace_lock, flags);
}

#define __SAVE __asm__ __volatile__("save %sp, -0x40, %sp\n\t")
#define __RESTORE __asm__ __volatile__("restore %g0, %g0, %g0\n\t")
#define __GET_FP(fp) __asm__ __volatile__("mov %%i6, %0" : "=r" (fp))

void show_backtrace(void)
{
	unsigned long fp;

	__SAVE; __SAVE; __SAVE; __SAVE;
	__SAVE; __SAVE; __SAVE; __SAVE;
	__RESTORE; __RESTORE; __RESTORE; __RESTORE;
	__RESTORE; __RESTORE; __RESTORE; __RESTORE;

	__GET_FP(fp);

	__show_backtrace(fp);
}

#ifdef CONFIG_SMP
void smp_show_backtrace_all_cpus(void)
{
	xc0((smpfunc_t) show_backtrace);
	show_backtrace();
}
#endif

void show_stackframe(struct sparc_stackf *sf)
{
	unsigned long size;
	unsigned long *stk;
	int i;

	printk("l0: %08lx l1: %08lx l2: %08lx l3: %08lx "
	       "l4: %08lx l5: %08lx l6: %08lx l7: %08lx\n",
	       sf->locals[0], sf->locals[1], sf->locals[2], sf->locals[3],
	       sf->locals[4], sf->locals[5], sf->locals[6], sf->locals[7]);
	printk("i0: %08lx i1: %08lx i2: %08lx i3: %08lx "
	       "i4: %08lx i5: %08lx fp: %08lx i7: %08lx\n",
	       sf->ins[0], sf->ins[1], sf->ins[2], sf->ins[3],
	       sf->ins[4], sf->ins[5], (unsigned long)sf->fp, sf->callers_pc);
	printk("sp: %08lx x0: %08lx x1: %08lx x2: %08lx "
	       "x3: %08lx x4: %08lx x5: %08lx xx: %08lx\n",
	       (unsigned long)sf->structptr, sf->xargs[0], sf->xargs[1],
	       sf->xargs[2], sf->xargs[3], sf->xargs[4], sf->xargs[5],
	       sf->xxargs[0]);
	size = ((unsigned long)sf->fp) - ((unsigned long)sf);
	size -= STACKFRAME_SZ;
	stk = (unsigned long *)((unsigned long)sf + STACKFRAME_SZ);
	i = 0;
	do {
		printk("s%d: %08lx\n", i++, *stk++);
	} while ((size -= sizeof(unsigned long)));
}

void show_regs(struct pt_regs * regs)
{
        printk("PSR: %08lx PC: %08lx NPC: %08lx Y: %08lx    %s\n", regs->psr,
	       regs->pc, regs->npc, regs->y, print_tainted());
	printk("g0: %08lx g1: %08lx g2: %08lx g3: %08lx ",
	       regs->u_regs[0], regs->u_regs[1], regs->u_regs[2],
	       regs->u_regs[3]);
	printk("g4: %08lx g5: %08lx g6: %08lx g7: %08lx\n",
	       regs->u_regs[4], regs->u_regs[5], regs->u_regs[6],
	       regs->u_regs[7]);
	printk("o0: %08lx o1: %08lx o2: %08lx o3: %08lx ",
	       regs->u_regs[8], regs->u_regs[9], regs->u_regs[10],
	       regs->u_regs[11]);
	printk("o4: %08lx o5: %08lx sp: %08lx o7: %08lx\n",
	       regs->u_regs[12], regs->u_regs[13], regs->u_regs[14],
	       regs->u_regs[15]);
	show_regwindow((struct reg_window *)regs->u_regs[14]);
}

void show_trace_task(struct task_struct *tsk)
{
	unsigned long pc, fp;
	unsigned long task_base = (unsigned long) tsk;
	struct reg_window *rw;
	int count = 0;

	if (!tsk)
		return;

	fp = tsk->thread.ksp;
	do {
		/* Bogus frame pointer? */
		if (fp < (task_base + sizeof(struct task_struct)) ||
		    fp >= (task_base + (PAGE_SIZE << 1)))
			break;
		rw = (struct reg_window *) fp;
		pc = rw->ins[7];
		printk("[%08lx] ", pc);
		fp = rw->ins[6];
	} while (++count < 16);
	printk("\n");
}

/*
 * Free current thread data structures etc..
 */
void exit_thread(void)
{
#ifndef CONFIG_SMP
	if(last_task_used_math == current) {
#else
	if(current->flags & PF_USEDFPU) {
#endif
		/* Keep process from leaving FPU in a bogon state. */
		put_psr(get_psr() | PSR_EF);
		fpsave(&current->thread.float_regs[0], &current->thread.fsr,
		       &current->thread.fpqueue[0], &current->thread.fpqdepth);
#ifndef CONFIG_SMP
		last_task_used_math = NULL;
#else
		current->flags &= ~PF_USEDFPU;
#endif
	}
}

void flush_thread(void)
{
	current->thread.w_saved = 0;

	/* No new signal delivery by default */
	current->thread.new_signal = 0;
#ifndef CONFIG_SMP
	if(last_task_used_math == current) {
#else
	if(current->flags & PF_USEDFPU) {
#endif
		/* Clean the fpu. */
		put_psr(get_psr() | PSR_EF);
		fpsave(&current->thread.float_regs[0], &current->thread.fsr,
		       &current->thread.fpqueue[0], &current->thread.fpqdepth);
#ifndef CONFIG_SMP
		last_task_used_math = NULL;
#else
		current->flags &= ~PF_USEDFPU;
#endif
	}

	/* Now, this task is no longer a kernel thread. */
	current->thread.current_ds = USER_DS;
	if (current->thread.flags & SPARC_FLAG_KTHREAD) {
		current->thread.flags &= ~SPARC_FLAG_KTHREAD;

		/* We must fixup kregs as well. */
		current->thread.kregs = (struct pt_regs *)
			(((unsigned long)current) +
			 (TASK_UNION_SIZE - TRACEREG_SZ));
	}
}

static __inline__ struct sparc_stackf *
clone_stackframe(struct sparc_stackf *dst, struct sparc_stackf *src)
{
	unsigned long size;
	struct sparc_stackf *sp;

	size = ((unsigned long)src->fp) - ((unsigned long)src);
	sp = (struct sparc_stackf *)(((unsigned long)dst) - size); 

	/* do_fork() grabs the parent semaphore, we must release it
	 * temporarily so we can build the child clone stack frame
	 * without deadlocking.
	 */
	if (copy_to_user(sp, src, size))
		sp = (struct sparc_stackf *) 0;
	else if (put_user(dst, &sp->fp))
		sp = (struct sparc_stackf *) 0;

	return sp;
}


/* Copy a Sparc thread.  The fork() return value conventions
 * under SunOS are nothing short of bletcherous:
 * Parent -->  %o0 == childs  pid, %o1 == 0
 * Child  -->  %o0 == parents pid, %o1 == 1
 *
 * NOTE: We have a separate fork kpsr/kwim because
 *       the parent could change these values between
 *       sys_fork invocation and when we reach here
 *       if the parent should sleep while trying to
 *       allocate the task_struct and kernel stack in
 *       do_fork().
 */
extern void ret_from_fork(void);

int copy_thread(int nr, unsigned long clone_flags, unsigned long sp,
		unsigned long unused,
		struct task_struct *p, struct pt_regs *regs)
{
	struct pt_regs *childregs;
	char *new_stack;

#ifndef CONFIG_SMP
	if(last_task_used_math == current) {
#else
	if(current->flags & PF_USEDFPU) {
#endif
		put_psr(get_psr() | PSR_EF);
		fpsave(&p->thread.float_regs[0], &p->thread.fsr,
		       &p->thread.fpqueue[0], &p->thread.fpqdepth);
#ifdef CONFIG_SMP
		current->flags &= ~PF_USEDFPU;
#endif
	}

	/*
	 *  p                      new_stack   childregs
	 *  !                      !           !             {if(PSR_PS) }
	 *  V                      V (stk.fr.) V  (pt_regs)  { (stk.fr.) }
	 *  +----- - - - - - ------+===========+============={+==========}+
	 */
	new_stack = (char*)p + TASK_UNION_SIZE;
	if (regs->psr & PSR_PS)
		new_stack -= STACKFRAME_SZ;
	new_stack -= STACKFRAME_SZ + TRACEREG_SZ;
	memcpy(new_stack, (char *)regs - STACKFRAME_SZ, STACKFRAME_SZ + TRACEREG_SZ);
	childregs = (struct pt_regs *) (new_stack + STACKFRAME_SZ);

	p->thread.ksp = (unsigned long) new_stack;
	p->thread.kpc = (((unsigned long) ret_from_fork) - 0x8);
	p->thread.kpsr = current->thread.fork_kpsr;
	p->thread.kwim = current->thread.fork_kwim;

	/* This is used for sun4c only */
	atomic_set(&p->thread.refcount, 1);

	if(regs->psr & PSR_PS) {
		extern struct pt_regs fake_swapper_regs;

		p->thread.kregs = &fake_swapper_regs;
		new_stack += STACKFRAME_SZ + TRACEREG_SZ;
		childregs->u_regs[UREG_FP] = (unsigned long) new_stack;
		p->thread.flags |= SPARC_FLAG_KTHREAD;
		p->thread.current_ds = KERNEL_DS;
		memcpy(new_stack, (void *)regs->u_regs[UREG_FP], STACKFRAME_SZ);
		childregs->u_regs[UREG_G6] = (unsigned long) p;
	} else {
		p->thread.kregs = childregs;
		childregs->u_regs[UREG_FP] = sp;
		p->thread.flags &= ~SPARC_FLAG_KTHREAD;
		p->thread.current_ds = USER_DS;

		if (sp != regs->u_regs[UREG_FP]) {
			struct sparc_stackf *childstack;
			struct sparc_stackf *parentstack;

			/*
			 * This is a clone() call with supplied user stack.
			 * Set some valid stack frames to give to the child.
			 */
			childstack = (struct sparc_stackf *) (sp & ~0x7UL);
			parentstack = (struct sparc_stackf *) regs->u_regs[UREG_FP];

#if 0
			printk("clone: parent stack:\n");
			show_stackframe(parentstack);
#endif

			childstack = clone_stackframe(childstack, parentstack);
			if (!childstack)
				return -EFAULT;

#if 0
			printk("clone: child stack:\n");
			show_stackframe(childstack);
#endif

			childregs->u_regs[UREG_FP] = (unsigned long)childstack;
		}
	}

	/* Set the return value for the child. */
	childregs->u_regs[UREG_I0] = current->pid;
	childregs->u_regs[UREG_I1] = 1;

	/* Set the return value for the parent. */
	regs->u_regs[UREG_I1] = 0;

	return 0;
}

/*
 * fill in the user structure for a core dump..
 */
void dump_thread(struct pt_regs * regs, struct user * dump)
{
	unsigned long first_stack_page;

	dump->magic = SUNOS_CORE_MAGIC;
	dump->len = sizeof(struct user);
	dump->regs.psr = regs->psr;
	dump->regs.pc = regs->pc;
	dump->regs.npc = regs->npc;
	dump->regs.y = regs->y;
	/* fuck me plenty */
	memcpy(&dump->regs.regs[0], &regs->u_regs[1], (sizeof(unsigned long) * 15));
	dump->uexec = current->thread.core_exec;
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
	dump->fpu.fpstatus.fpq_count = current->thread.fpqdepth;
	memcpy(&dump->fpu.fpstatus.fpq[0], &current->thread.fpqueue[0],
	       ((sizeof(unsigned long) * 2) * 16));
	dump->sigcode = 0;
}

/*
 * fill in the fpu structure for a core dump.
 */
int dump_fpu (struct pt_regs * regs, elf_fpregset_t * fpregs)
{
	if (current->used_math == 0) {
		memset(fpregs, 0, sizeof(*fpregs));
		fpregs->pr_q_entrysize = 8;
		return 1;
	}
#ifdef CONFIG_SMP
	if (current->flags & PF_USEDFPU) {
		put_psr(get_psr() | PSR_EF);
		fpsave(&current->thread.float_regs[0], &current->thread.fsr,
		       &current->thread.fpqueue[0], &current->thread.fpqdepth);
		regs->psr &= ~(PSR_EF);
		current->flags &= ~(PF_USEDFPU);
	}
#else
	if (current == last_task_used_math) {
		put_psr(get_psr() | PSR_EF);
		fpsave(&current->thread.float_regs[0], &current->thread.fsr,
		       &current->thread.fpqueue[0], &current->thread.fpqdepth);
		last_task_used_math = 0;
		regs->psr &= ~(PSR_EF);
	}
#endif
	memcpy(&fpregs->pr_fr.pr_regs[0],
	       &current->thread.float_regs[0],
	       (sizeof(unsigned long) * 32));
	fpregs->pr_fsr = current->thread.fsr;
	fpregs->pr_qcnt = current->thread.fpqdepth;
	fpregs->pr_q_entrysize = 8;
	fpregs->pr_en = 1;
	if(fpregs->pr_qcnt != 0) {
		memcpy(&fpregs->pr_q[0],
		       &current->thread.fpqueue[0],
		       sizeof(struct fpq) * fpregs->pr_qcnt);
	}
	/* Zero out the rest. */
	memset(&fpregs->pr_q[fpregs->pr_qcnt], 0,
	       sizeof(struct fpq) * (32 - fpregs->pr_qcnt));
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

	/* Check for indirect call. */
	if(regs->u_regs[UREG_G1] == 0)
		base = 1;

	filename = getname((char *)regs->u_regs[base + UREG_I0]);
	error = PTR_ERR(filename);
	if(IS_ERR(filename))
		goto out;
	error = do_execve(filename, (char **) regs->u_regs[base + UREG_I1],
			  (char **) regs->u_regs[base + UREG_I2], regs);
	putname(filename);
out:
	return error;
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

	__asm__ __volatile("mov %4, %%g2\n\t"    /* Set aside fn ptr... */
			   "mov %5, %%g3\n\t"    /* and arg. */
			   "mov %1, %%g1\n\t"
			   "mov %2, %%o0\n\t"    /* Clone flags. */
			   "mov 0, %%o1\n\t"     /* usp arg == 0 */
			   "t 0x10\n\t"          /* Linux/Sparc clone(). */
			   "cmp %%o1, 0\n\t"
			   "be 1f\n\t"           /* The parent, just return. */
			   " nop\n\t"            /* Delay slot. */
			   "jmpl %%g2, %%o7\n\t" /* Call the function. */
			   " mov %%g3, %%o0\n\t" /* Get back the arg in delay. */
			   "mov %3, %%g1\n\t"
			   "t 0x10\n\t"          /* Linux/Sparc exit(). */
			   /* Notreached by child. */
			   "1: mov %%o0, %0\n\t" :
			   "=r" (retval) :
			   "i" (__NR_clone), "r" (flags | CLONE_VM),
			   "i" (__NR_exit),  "r" (fn), "r" (arg) :
			   "g1", "g2", "g3", "o0", "o1", "memory", "cc");
	return retval;
}
