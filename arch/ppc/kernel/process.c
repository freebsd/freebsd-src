/*
 *  linux/arch/ppc/kernel/process.c
 *
 *  Derived from "arch/i386/kernel/process.c"
 *    Copyright (C) 1995  Linus Torvalds
 *
 *  Updated and modified by Cort Dougan (cort@cs.nmt.edu) and
 *  Paul Mackerras (paulus@cs.anu.edu.au)
 *
 *  PowerPC version
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
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
#include <linux/elf.h>
#include <linux/init.h>
#include <linux/prctl.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/mmu.h>
#include <asm/prom.h>

int dump_fpu(struct pt_regs *regs, elf_fpregset_t *fpregs);
extern unsigned long _get_SP(void);

struct task_struct *last_task_used_math = NULL;
struct task_struct *last_task_used_altivec = NULL;
static struct fs_struct init_fs = INIT_FS;
static struct files_struct init_files = INIT_FILES;
static struct signal_struct init_signals = INIT_SIGNALS;
struct mm_struct init_mm = INIT_MM(init_mm);
/* this is 16-byte aligned because it has a stack in it */
union task_union __attribute((aligned(16))) init_task_union = {
	INIT_TASK(init_task_union.task)
};
/* only used to get secondary processor up */
struct task_struct *current_set[NR_CPUS] = {&init_task, };

#undef SHOW_TASK_SWITCHES
#undef CHECK_STACK

#if defined(CHECK_STACK)
unsigned long
kernel_stack_top(struct task_struct *tsk)
{
	return ((unsigned long)tsk) + sizeof(union task_union);
}

unsigned long
task_top(struct task_struct *tsk)
{
	return ((unsigned long)tsk) + sizeof(struct task_struct);
}

/* check to make sure the kernel stack is healthy */
int check_stack(struct task_struct *tsk)
{
	unsigned long stack_top = kernel_stack_top(tsk);
	unsigned long tsk_top = task_top(tsk);
	int ret = 0;

#if 0
	/* check thread magic */
	if ( tsk->thread.magic != THREAD_MAGIC )
	{
		ret |= 1;
		printk("thread.magic bad: %08x\n", tsk->thread.magic);
	}
#endif

	if ( !tsk )
		printk("check_stack(): tsk bad tsk %p\n",tsk);

	/* check if stored ksp is bad */
	if ( (tsk->thread.ksp > stack_top) || (tsk->thread.ksp < tsk_top) )
	{
		printk("stack out of bounds: %s/%d\n"
		       " tsk_top %08lx ksp %08lx stack_top %08lx\n",
		       tsk->comm,tsk->pid,
		       tsk_top, tsk->thread.ksp, stack_top);
		ret |= 2;
	}

	/* check if stack ptr RIGHT NOW is bad */
	if ( (tsk == current) && ((_get_SP() > stack_top ) || (_get_SP() < tsk_top)) )
	{
		printk("current stack ptr out of bounds: %s/%d\n"
		       " tsk_top %08lx sp %08lx stack_top %08lx\n",
		       current->comm,current->pid,
		       tsk_top, _get_SP(), stack_top);
		ret |= 4;
	}

#if 0
	/* check amount of free stack */
	for ( i = (unsigned long *)task_top(tsk) ; i < kernel_stack_top(tsk) ; i++ )
	{
		if ( !i )
			printk("check_stack(): i = %p\n", i);
		if ( *i != 0 )
		{
			/* only notify if it's less than 900 bytes */
			if ( (i - (unsigned long *)task_top(tsk))  < 900 )
				printk("%d bytes free on stack\n",
				       i - task_top(tsk));
			break;
		}
	}
#endif

	if (ret)
	{
		panic("bad kernel stack");
	}
	return(ret);
}
#endif /* defined(CHECK_STACK) */

#ifdef CONFIG_ALTIVEC
int
dump_altivec(struct pt_regs *regs, elf_vrregset_t *vrregs)
{
	if (regs->msr & MSR_VEC)
		giveup_altivec(current);
	memcpy(vrregs, &current->thread.vr[0], sizeof(*vrregs));
	return 1;
}

void
enable_kernel_altivec(void)
{
#ifdef CONFIG_SMP
	if (current->thread.regs && (current->thread.regs->msr & MSR_VEC))
		giveup_altivec(current);
	else
		giveup_altivec(NULL);	/* just enable AltiVec for kernel - force */
#else
	giveup_altivec(last_task_used_altivec);
#endif /* __SMP __ */
}
#endif /* CONFIG_ALTIVEC */

void
enable_kernel_fp(void)
{
#ifdef CONFIG_SMP
	if (current->thread.regs && (current->thread.regs->msr & MSR_FP))
		giveup_fpu(current);
	else
		giveup_fpu(NULL);	/* just enables FP for kernel */
#else
	giveup_fpu(last_task_used_math);
#endif /* CONFIG_SMP */
}

int
dump_fpu(struct pt_regs *regs, elf_fpregset_t *fpregs)
{
	if (regs->msr & MSR_FP)
		giveup_fpu(current);
	memcpy(fpregs, &current->thread.fpr[0], sizeof(*fpregs));
	return 1;
}

void
_switch_to(struct task_struct *prev, struct task_struct *new,
	  struct task_struct **last)
{
	struct thread_struct *new_thread, *old_thread;
	unsigned long s;

	__save_flags(s);
	__cli();
#if CHECK_STACK
	check_stack(prev);
	check_stack(new);
#endif

#ifdef CONFIG_SMP
	/* avoid complexity of lazy save/restore of fpu
	 * by just saving it every time we switch out if
	 * this task used the fpu during the last quantum.
	 *
	 * If it tries to use the fpu again, it'll trap and
	 * reload its fp regs.  So we don't have to do a restore
	 * every switch, just a save.
	 *  -- Cort
	 */
	if ( prev->thread.regs && (prev->thread.regs->msr & MSR_FP) )
		giveup_fpu(prev);
#ifdef CONFIG_ALTIVEC
	/*
	 * If the previous thread used altivec in the last quantum
	 * (thus changing altivec regs) then save them.
	 * We used to check the VRSAVE register but not all apps
	 * set it, so we don't rely on it now (and in fact we need
	 * to save & restore VSCR even if VRSAVE == 0).  -- paulus
	 *
	 * On SMP we always save/restore altivec regs just to avoid the
	 * complexity of changing processors.
	 *  -- Cort
	 */
	if ((prev->thread.regs && (prev->thread.regs->msr & MSR_VEC)))
		giveup_altivec(prev);
#endif /* CONFIG_ALTIVEC */
#endif /* CONFIG_SMP */

	current_set[smp_processor_id()] = new;

	/* Avoid the trap.  On smp this this never happens since
	 * we don't set last_task_used_altivec -- Cort
	 */
	if (new->thread.regs && last_task_used_altivec == new)
		new->thread.regs->msr |= MSR_VEC;
	new_thread = &new->thread;
	old_thread = &current->thread;
	*last = _switch(old_thread, new_thread);
	__restore_flags(s);
}

void show_regs(struct pt_regs * regs)
{
	int i;

	printk("NIP: %08lX XER: %08lX LR: %08lX SP: %08lX REGS: %p TRAP: %04lx    %s\n",
	       regs->nip, regs->xer, regs->link, regs->gpr[1], regs,regs->trap, print_tainted());
	printk("MSR: %08lx EE: %01x PR: %01x FP: %01x ME: %01x IR/DR: %01x%01x\n",
	       regs->msr, regs->msr&MSR_EE ? 1 : 0, regs->msr&MSR_PR ? 1 : 0,
	       regs->msr & MSR_FP ? 1 : 0,regs->msr&MSR_ME ? 1 : 0,
	       regs->msr&MSR_IR ? 1 : 0,
	       regs->msr&MSR_DR ? 1 : 0);
#ifdef CONFIG_4xx
	/*
	 * TRAP 0x800 is the hijacked FPU unavailable exception vector
	 * on 40x used to implement the heavyweight data access
	 * functionality.  It is an emulated value (like all trap
	 * vectors) on 440.
	 */
	if (regs->trap == 0x300 || regs->trap == 0x600 || regs->trap == 0x800)
		printk("DEAR: %08lX, ESR: %08lX\n", regs->dar, regs->dsisr);
#else
	if (regs->trap == 0x300 || regs->trap == 0x600)
		printk("DAR: %08lX, DSISR: %08lX\n", regs->dar, regs->dsisr);
#endif
	printk("TASK = %p[%d] '%s' ",
	       current, current->pid, current->comm);
	printk("Last syscall: %ld ", current->thread.last_syscall);
	printk("\nlast math %p last altivec %p", last_task_used_math,
	       last_task_used_altivec);

#if defined(CONFIG_4xx) && defined(DCRN_PLB0_BEAR)
	printk("\nPLB0: bear= 0x%8.8x acr=   0x%8.8x besr=  0x%8.8x\n",
	    mfdcr(DCRN_PLB0_BEAR), mfdcr(DCRN_PLB0_ACR),
	    mfdcr(DCRN_PLB0_BESR));
#endif
#if defined(CONFIG_4xx) && defined(DCRN_POB0_BEAR)
	printk("PLB0 to OPB: bear= 0x%8.8x besr0= 0x%8.8x besr1= 0x%8.8x\n",
	    mfdcr(DCRN_POB0_BEAR), mfdcr(DCRN_POB0_BESR0),
	    mfdcr(DCRN_POB0_BESR1));
#endif

#ifdef CONFIG_SMP
	printk(" CPU: %d", current->processor);
#endif /* CONFIG_SMP */

	printk("\n");
	for (i = 0;  i < 32;  i++)
	{
		long r;
		if ((i % 8) == 0)
		{
			printk("GPR%02d: ", i);
		}

		if ( __get_user(r, &(regs->gpr[i])) )
		    goto out;
		printk("%08lX ", r);
		if ((i % 8) == 7)
		{
			printk("\n");
		}
	}
out:
	print_backtrace((unsigned long *)regs->gpr[1]);
}

void exit_thread(void)
{
	if (last_task_used_math == current)
		last_task_used_math = NULL;
	if (last_task_used_altivec == current)
		last_task_used_altivec = NULL;
}

void flush_thread(void)
{
	if (last_task_used_math == current)
		last_task_used_math = NULL;
	if (last_task_used_altivec == current)
		last_task_used_altivec = NULL;
}

void
release_thread(struct task_struct *t)
{
}

/*
 * Copy a thread..
 */
int
copy_thread(int nr, unsigned long clone_flags, unsigned long usp,
	    unsigned long unused,
	    struct task_struct *p, struct pt_regs *regs)
{
	struct pt_regs *childregs, *kregs;
	extern void ret_from_fork(void);
	unsigned long sp = (unsigned long)p + sizeof(union task_union);
	unsigned long childframe;

	/* Copy registers */
	sp -= sizeof(struct pt_regs);
	childregs = (struct pt_regs *) sp;
	*childregs = *regs;
	if ((childregs->msr & MSR_PR) == 0) {
		/* for kernel thread, set `current' and stackptr in new task */
		childregs->gpr[1] = sp + sizeof(struct pt_regs);
		childregs->gpr[2] = (unsigned long) p;
		p->thread.regs = NULL;	/* no user register state */
	} else
		p->thread.regs = childregs;
	childregs->gpr[3] = 0;  /* Result from fork() */
	sp -= STACK_FRAME_OVERHEAD;
	childframe = sp;

	/*
	 * The way this works is that at some point in the future
	 * some task will call _switch to switch to the new task.
	 * That will pop off the stack frame created below and start
	 * the new task running at ret_from_fork.  The new task will
	 * do some house keeping and then return from the fork or clone
	 * system call, using the stack frame created above.
	 */
	sp -= sizeof(struct pt_regs);
	kregs = (struct pt_regs *) sp;
	sp -= STACK_FRAME_OVERHEAD;
	p->thread.ksp = sp;
	kregs->nip = (unsigned long)ret_from_fork;

	/*
	 * copy fpu info - assume lazy fpu switch now always
	 *  -- Cort
	 */
	if (regs->msr & MSR_FP) {
		giveup_fpu(current);
		childregs->msr &= ~(MSR_FP | MSR_FE0 | MSR_FE1);
	}
	memcpy(&p->thread.fpr, &current->thread.fpr, sizeof(p->thread.fpr));
	p->thread.fpscr = current->thread.fpscr;

#ifdef CONFIG_ALTIVEC
	/*
	 * copy altiVec info - assume lazy altiVec switch
	 * - kumar
	 */
	if (regs->msr & MSR_VEC)
		giveup_altivec(current);
	memcpy(&p->thread.vr, &current->thread.vr, sizeof(p->thread.vr));
	p->thread.vscr = current->thread.vscr;
	childregs->msr &= ~MSR_VEC;
#endif /* CONFIG_ALTIVEC */

	p->thread.last_syscall = -1;

	return 0;
}

/*
 * Set up a thread for executing a new program
 */
void start_thread(struct pt_regs *regs, unsigned long nip, unsigned long sp)
{
	set_fs(USER_DS);
	memset(regs->gpr, 0, sizeof(regs->gpr));
	memset(&regs->ctr, 0, 5 * sizeof(regs->ctr));
	regs->nip = nip;
	regs->gpr[1] = sp;
	regs->msr = MSR_USER;
	if (last_task_used_math == current)
		last_task_used_math = 0;
	if (last_task_used_altivec == current)
		last_task_used_altivec = 0;
	memset(current->thread.fpr, 0, sizeof(current->thread.fpr));
	current->thread.fpscr = 0;
#ifdef CONFIG_ALTIVEC
	memset(current->thread.vr, 0, sizeof(current->thread.vr));
	memset(&current->thread.vscr, 0, sizeof(current->thread.vscr));
	current->thread.vrsave = 0;
	current->thread.used_vr = 0;
#endif /* CONFIG_ALTIVEC */
}

/*
 * Support for the PR_GET/SET_FPEXC prctl() calls.
 */
static inline unsigned int __unpack_fe01(unsigned int msr_bits)
{
	return ((msr_bits & MSR_FE0) >> 10) | ((msr_bits & MSR_FE1) >> 8);
}

static inline unsigned int __pack_fe01(unsigned int fpmode)
{
	return ((fpmode << 10) & MSR_FE0) | ((fpmode << 8) & MSR_FE1);
}

int set_fpexc_mode(struct task_struct *tsk, unsigned int val)
{
	struct pt_regs *regs = tsk->thread.regs;

	if (val > PR_FP_EXC_PRECISE)
		return -EINVAL;
	tsk->thread.fpexc_mode = __pack_fe01(val);
	if (regs != NULL && (regs->msr & MSR_FP) != 0)
		regs->msr = (regs->msr & ~(MSR_FE0|MSR_FE1))
			| tsk->thread.fpexc_mode;
	return 0;
}

int get_fpexc_mode(struct task_struct *tsk, unsigned long adr)
{
	unsigned int val;

	val = __unpack_fe01(tsk->thread.fpexc_mode);
	return put_user(val, (unsigned int *) adr);
}

int sys_clone(int p1, int p2, int p3, int p4, int p5, int p6,
	      struct pt_regs *regs)
{
	return do_fork(p1, p2, regs, 0);
}

int sys_fork(int p1, int p2, int p3, int p4, int p5, int p6,
	     struct pt_regs *regs)
{
	return do_fork(SIGCHLD, regs->gpr[1], regs, 0);
}

int sys_vfork(int p1, int p2, int p3, int p4, int p5, int p6,
	      struct pt_regs *regs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs->gpr[1], regs, 0);
}

int sys_execve(unsigned long a0, unsigned long a1, unsigned long a2,
	       unsigned long a3, unsigned long a4, unsigned long a5,
	       struct pt_regs *regs)
{
	int error;
	char * filename;

	filename = getname((char *) a0);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	if (regs->msr & MSR_FP)
		giveup_fpu(current);
#ifdef CONFIG_ALTIVEC
	if (regs->msr & MSR_VEC)
		giveup_altivec(current);
#endif /* CONFIG_ALTIVEC */
	error = do_execve(filename, (char **) a1, (char **) a2, regs);
	if (error == 0)
		current->ptrace &= ~PT_DTRACE;
	putname(filename);
out:
	return error;
}

void
print_backtrace(unsigned long *sp)
{
	int cnt = 0;
	unsigned long i;

	if (sp == NULL)
		asm("mr %0,1" : "=r" (sp));
	printk("Call backtrace: ");
	while (sp) {
		if (__get_user( i, &sp[1] ))
			break;
		if (cnt++ % 7 == 0)
			printk("\n");
		printk("%08lX ", i);
		if (cnt > 32) break;
		if (__get_user(sp, (unsigned long **)sp))
			break;
	}
	printk("\n");
}

void show_trace_task(struct task_struct *tsk)
{
	unsigned long stack_top = (unsigned long) tsk + THREAD_SIZE;
	unsigned long sp, prev_sp;
	int count = 0;

	if (tsk == NULL)
		return;
	sp = (unsigned long) &tsk->thread.ksp;
	do {
		prev_sp = sp;
		sp = *(unsigned long *)sp;
		if (sp <= prev_sp || sp >= stack_top || (sp & 3) != 0)
			break;
		if (count > 0)
			printk("[%08lx] ", *(unsigned long *)(sp + 4));
	} while (++count < 16);
	if (count > 1)
		printk("\n");
}

#if 0
/*
 * Low level print for debugging - Cort
 */
int __init ll_printk(const char *fmt, ...)
{
        va_list args;
	char buf[256];
        int i;

        va_start(args, fmt);
        i=vsprintf(buf,fmt,args);
	ll_puts(buf);
        va_end(args);
        return i;
}

int lines = 24, cols = 80;
int orig_x = 0, orig_y = 0;

void puthex(unsigned long val)
{
	unsigned char buf[10];
	int i;
	for (i = 7;  i >= 0;  i--)
	{
		buf[i] = "0123456789ABCDEF"[val & 0x0F];
		val >>= 4;
	}
	buf[8] = '\0';
	prom_print(buf);
}

void __init ll_puts(const char *s)
{
	int x,y;
	char *vidmem = (char *)/*(_ISA_MEM_BASE + 0xB8000) */0xD00B8000;
	char c;
	extern int mem_init_done;

	if ( mem_init_done ) /* assume this means we can printk */
	{
		printk(s);
		return;
	}

#if 0
	if ( have_of )
	{
		prom_print(s);
		return;
	}
#endif

	/*
	 * can't ll_puts on chrp without openfirmware yet.
	 * vidmem just needs to be setup for it.
	 * -- Cort
	 */
	if ( _machine != _MACH_prep )
		return;
	x = orig_x;
	y = orig_y;

	while ( ( c = *s++ ) != '\0' ) {
		if ( c == '\n' ) {
			x = 0;
			if ( ++y >= lines ) {
				/*scroll();*/
				/*y--;*/
				y = 0;
			}
		} else {
			vidmem [ ( x + cols * y ) * 2 ] = c;
			if ( ++x >= cols ) {
				x = 0;
				if ( ++y >= lines ) {
					/*scroll();*/
					/*y--;*/
					y = 0;
				}
			}
		}
	}

	orig_x = x;
	orig_y = y;
}
#endif

/*
 * These bracket the sleeping functions..
 */
extern void scheduling_functions_start_here(void);
extern void scheduling_functions_end_here(void);
#define first_sched    ((unsigned long) scheduling_functions_start_here)
#define last_sched     ((unsigned long) scheduling_functions_end_here)

unsigned long get_wchan(struct task_struct *p)
{
	unsigned long ip, sp;
	unsigned long stack_page = (unsigned long) p;
	int count = 0;
	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;
	sp = p->thread.ksp;
	do {
		sp = *(unsigned long *)sp;
		if (sp < stack_page || sp >= stack_page + 8188)
			return 0;
		if (count > 0) {
			ip = *(unsigned long *)(sp + 4);
			if (ip < first_sched || ip >= last_sched)
				return ip;
		}
	} while (count++ < 16);
	return 0;
}
