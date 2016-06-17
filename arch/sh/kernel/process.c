/* $Id: process.c,v 1.1.1.1.2.4 2003/05/29 04:21:33 lethal Exp $
 *
 *  linux/arch/sh/kernel/process.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *
 *  SuperH version:  Copyright (C) 1999, 2000  Niibe Yutaka & Kaz Kojima
 */

/*
 * This file handles the architecture-dependent parts of process handling..
 */

#include <linux/unistd.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/mmu_context.h>
#include <asm/elf.h>

static int hlt_counter=0;

int ubc_usercnt = 0;

#define HARD_IDLE_TIMEOUT (HZ / 3)

void disable_hlt(void)
{
	hlt_counter++;
}

void enable_hlt(void)
{
	hlt_counter--;
}

/*
 * The idle loop on a uniprocessor i386..
 */ 
void cpu_idle(void *unused)
{
	/* endless idle loop with no priority at all */
	init_idle();
	current->nice = 20;
	current->counter = -100;

	while (1) {
		if (hlt_counter) {
			while (1)
				if (current->need_resched)
					break;
		} else {
			__cli();
			while (!current->need_resched) {
				__sti();
				asm volatile("sleep" : : : "memory");
				__cli();
			}
			__sti();
		}
		schedule();
		check_pgt_cache();
	}
}

void machine_restart(char * __unused)
{
	/* SR.BL=1 and invoke address error to let CPU reset (manual reset) */
	asm volatile("ldc %0, sr\n\t"
		     "mov.l @%1, %0" : : "r" (0x10000000), "r" (0x80000001));
}

void machine_halt(void)
{
	while (1)
		asm volatile("sleep" : : : "memory");
}

void machine_power_off(void)
{
}

void show_regs(struct pt_regs * regs)
{
	printk("\n");
	printk("PC  : %08lx SP  : %08lx SR  : %08lx TEA : %08x    %s\n",
	       regs->pc, regs->regs[15], regs->sr, ctrl_inl(MMU_TEA), print_tainted());
	printk("R0  : %08lx R1  : %08lx R2  : %08lx R3  : %08lx\n",
	       regs->regs[0],regs->regs[1],
	       regs->regs[2],regs->regs[3]);
	printk("R4  : %08lx R5  : %08lx R6  : %08lx R7  : %08lx\n",
	       regs->regs[4],regs->regs[5],
	       regs->regs[6],regs->regs[7]);
	printk("R8  : %08lx R9  : %08lx R10 : %08lx R11 : %08lx\n",
	       regs->regs[8],regs->regs[9],
	       regs->regs[10],regs->regs[11]);
	printk("R12 : %08lx R13 : %08lx R14 : %08lx\n",
	       regs->regs[12],regs->regs[13],
	       regs->regs[14]);
	printk("MACH: %08lx MACL: %08lx GBR : %08lx PR  : %08lx\n",
	       regs->mach, regs->macl, regs->gbr, regs->pr);
}

struct task_struct * alloc_task_struct(void)
{
	/* Get two pages */
	return (struct task_struct *) __get_free_pages(GFP_KERNEL,1);
}

void free_task_struct(struct task_struct *p)
{
	free_pages((unsigned long) p, 1);
}

/*
 * Create a kernel thread
 */

/*
 * This is the mechanism for creating a new kernel thread.
 *
 */
int arch_kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{	/* Don't use this in BL=1(cli).  Or else, CPU resets! */
	register unsigned long __sc0 __asm__ ("r0");
	register unsigned long __sc3 __asm__ ("r3") = __NR_clone;
	register unsigned long __sc4 __asm__ ("r4") = (long) flags | CLONE_VM;
	register unsigned long __sc5 __asm__ ("r5") = 0;
	register unsigned long __sc8 __asm__ ("r8") = (long) arg;
	register unsigned long __sc9 __asm__ ("r9") = (long) fn;

	__asm__("trapa	#0x12\n\t" 	/* Linux/SH system call */
		"tst	r0, r0\n\t"	/* child or parent? */
		"bf	1f\n\t"		/* parent - jump */
		"jsr	@r9\n\t"	/* call fn */
		" mov	r8, r4\n\t"	/* push argument */
		"mov	r0, r4\n\t"	/* return value to arg of exit */
		"mov	%1, r3\n\t"	/* exit */
		"trapa	#0x11\n"
		"1:"
		: "=z" (__sc0)
		: "i" (__NR_exit), "r" (__sc3), "r" (__sc4), "r" (__sc5), 
		  "r" (__sc8), "r" (__sc9)
		: "memory", "t");
	return __sc0;
}

/*
 * Free current thread data structures etc..
 */
void exit_thread(void)
{
	if (current->thread.ubc_pc1) {
		current->thread.ubc_pc1 = 0;
		ubc_usercnt -= 1;
	}
}

void flush_thread(void)
{
#if defined(__sh3__)
	/* do nothing */
	/* Possibly, set clear debug registers */
#elif defined(__SH4__)
	struct task_struct *tsk = current;

	/* Forget lazy FPU state */
	clear_fpu(tsk);
	tsk->used_math = 0;
#endif
}

void release_thread(struct task_struct *dead_task)
{
	/* do nothing */
}

/* Fill in the fpu structure for a core dump.. */
int dump_fpu(struct pt_regs *regs, elf_fpregset_t *fpu)
{
#if defined(__SH4__)
	int fpvalid;
	struct task_struct *tsk = current;

	fpvalid = tsk->used_math;
	if (fpvalid) {
		unlazy_fpu(tsk);
		memcpy(fpu, &tsk->thread.fpu.hard, sizeof(*fpu));
	}

	return fpvalid;
#else
	return 0; /* Task didn't use the fpu at all. */
#endif
}

asmlinkage void ret_from_fork(void);

int copy_thread(int nr, unsigned long clone_flags, unsigned long usp,
		unsigned long unused,
		struct task_struct *p, struct pt_regs *regs)
{
	struct pt_regs *childregs;
#if defined(__SH4__)
	struct task_struct *tsk = current;

	unlazy_fpu(tsk);
	p->thread.fpu = current->thread.fpu;
	p->used_math = tsk->used_math;
#endif
	childregs = ((struct pt_regs *)(THREAD_SIZE + (unsigned long) p)) - 1;
	*childregs = *regs;

	if (user_mode(regs)) {
		childregs->regs[15] = usp;
	} else {
		childregs->regs[15] = (unsigned long)p+2*PAGE_SIZE;
	}
	childregs->regs[0] = 0; /* Set return value for child */
	childregs->sr |= SR_FD; /* Invalidate FPU flag */

	p->thread.sp = (unsigned long) childregs;
	p->thread.pc = (unsigned long) ret_from_fork;

	p->thread.ubc_pc1 = 0;
	p->thread.ubc_pc2 = 0;

	return 0;
}

/*
 * fill in the user structure for a core dump..
 */
void dump_thread(struct pt_regs * regs, struct user * dump)
{
	dump->magic = CMAGIC;
	dump->start_code = current->mm->start_code;
	dump->start_data  = current->mm->start_data;
	dump->start_stack = regs->regs[15] & ~(PAGE_SIZE - 1);
	dump->u_tsize = (current->mm->end_code - dump->start_code) >> PAGE_SHIFT;
	dump->u_dsize = (current->mm->brk + (PAGE_SIZE-1) - dump->start_data) >> PAGE_SHIFT;
	dump->u_ssize = (current->mm->start_stack - dump->start_stack +
			 PAGE_SIZE - 1) >> PAGE_SHIFT;
	/* Debug registers will come here. */

	dump->regs = *regs;

	dump->u_fpvalid = dump_fpu(regs, &dump->fpu);
}

/* Tracing by user break controller.  */
static inline void
ubc_set_tracing(int asid, unsigned long nextpc1, unsigned nextpc2)
{
	ctrl_outl(nextpc1, UBC_BARA);
	ctrl_outb(asid, UBC_BASRA);
	if(UBC_TYPE_SH7729){
		ctrl_outl(0, UBC_BAMRA);
		ctrl_outw(BBR_INST | BBR_READ | BBR_CPU, UBC_BBRA);
	}else{
		ctrl_outb(0, UBC_BAMRA);
		ctrl_outw(BBR_INST | BBR_READ, UBC_BBRA);
	}

	if (nextpc2 != (unsigned long) -1) {
		ctrl_outl(nextpc2, UBC_BARB);
		ctrl_outb(asid, UBC_BASRB);
		if(UBC_TYPE_SH7729){
			ctrl_outl(0, UBC_BAMRB);
			ctrl_outw(BBR_INST | BBR_READ | BBR_CPU, UBC_BBRB);
		}else{
			ctrl_outb(0, UBC_BAMRB);
			ctrl_outw(BBR_INST | BBR_READ, UBC_BBRB);
		}
	}
	if(UBC_TYPE_SH7729)
		ctrl_outl(BRCR_PCTE, UBC_BRCR);
	else
		ctrl_outw(0, UBC_BRCR);
}

/*
 *	switch_to(x,y) should switch tasks from x to y.
 *
 */
void __switch_to(struct task_struct *prev, struct task_struct *next)
{
#if defined(__SH4__)
	unlazy_fpu(prev);
#endif

	/*
	 * Restore the kernel mode register
	 *   	k7 (r7_bank1)
	 */
	asm volatile("ldc	%0, r7_bank"
		     : /* no output */
		     :"r" (next));

	/* If no tasks are using the UBC, we're done */
	if (ubc_usercnt == 0)
		return;

	/* Otherwise, set or clear UBC as appropriate */
	if (next->thread.ubc_pc1) {
		ubc_set_tracing(next->mm->context & MMU_CONTEXT_ASID_MASK,
				next->thread.ubc_pc1, next->thread.ubc_pc2);
	} else {
		ctrl_outw(0, UBC_BBRA);
		ctrl_outw(0, UBC_BBRB);
	}
}

asmlinkage int sys_fork(unsigned long r4, unsigned long r5,
			unsigned long r6, unsigned long r7,
			struct pt_regs regs)
{
	return do_fork(SIGCHLD, regs.regs[15], &regs, 0);
}

asmlinkage int sys_clone(unsigned long clone_flags, unsigned long newsp,
			 unsigned long r6, unsigned long r7,
			 struct pt_regs regs)
{
	if (!newsp)
		newsp = regs.regs[15];
	return do_fork(clone_flags, newsp, &regs, 0);
}

/*
 * This is trivial, and on the face of it looks like it
 * could equally well be done in user mode.
 *
 * Not so, for quite unobvious reasons - register pressure.
 * In user mode vfork() cannot have a stack frame, and if
 * done by calling the "clone()" system call directly, you
 * do not have enough call-clobbered registers to hold all
 * the information you need.
 */
asmlinkage int sys_vfork(unsigned long r4, unsigned long r5,
			 unsigned long r6, unsigned long r7,
			 struct pt_regs regs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs.regs[15], &regs, 0);
}

/*
 * sys_execve() executes a new program.
 */
asmlinkage int sys_execve(char *ufilename, char **uargv,
			  char **uenvp, unsigned long r7,
			  struct pt_regs regs)
{
	int error;
	char *filename;

	filename = getname(ufilename);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;

	error = do_execve(filename, uargv, uenvp, &regs);
	if (error == 0)
		current->ptrace &= ~PT_DTRACE;
	putname(filename);
out:
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
	unsigned long schedule_frame;
	unsigned long pc;

	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	/*
	 * The same comment as on the Alpha applies here, too ...
	 */
	pc = thread_saved_pc(&p->thread);
	if (pc >= (unsigned long) interruptible_sleep_on && pc < (unsigned long) add_timer) {
		schedule_frame = ((unsigned long *)(long)p->thread.sp)[1];
		return (unsigned long)((unsigned long *)schedule_frame)[1];
	}
	return pc;
}

asmlinkage void print_syscall(int x)
{
	unsigned long flags, sr;
	asm("stc	sr, %0": "=r" (sr));
	save_and_cli(flags);
	printk("%c: %c %c, %c: SYSCALL\n", (x&63)+32,
	       (current->flags&PF_USEDFPU)?'C':' ',
	       (init_task.flags&PF_USEDFPU)?'K':' ', (sr&SR_FD)?' ':'F');
	restore_flags(flags);
}

asmlinkage void break_point_trap(unsigned long r4, unsigned long r5,
				 unsigned long r6, unsigned long r7,
				 struct pt_regs regs)
{
	/* Clear tracing.  */
	ctrl_outw(0, UBC_BBRA);
	ctrl_outw(0, UBC_BBRB);
	current->thread.ubc_pc1 = 0;
	current->thread.ubc_pc2 = (unsigned long) -1;
	ubc_usercnt -= 1;

	force_sig(SIGTRAP, current);
}

asmlinkage void break_point_trap_software(unsigned long r4, unsigned long r5,
					  unsigned long r6, unsigned long r7,
					  struct pt_regs regs)
{
	regs.pc -= 2;
	force_sig(SIGTRAP, current);
}
