/*
 *  arch/s390/kernel/process.c
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
 *               Hartmut Penner (hp@de.ibm.com),
 *               Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com),
 *
 *  Derived from "arch/i386/kernel/process.c"
 *    Copyright (C) 1995, Linus Torvalds
 */

/*
 * This file handles the architecture-dependent parts of process handling..
 */

#define __KERNEL_SYSCALLS__
#include <stdarg.h>

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
#include <linux/vmalloc.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/irq.h>

asmlinkage void ret_from_fork(void) __asm__("ret_from_fork");

/*
 * The idle loop on a S390...
 */

int cpu_idle(void *unused)
{
	psw_t wait_psw;
	unsigned long reg;

	/* endless idle loop with no priority at all */
        init_idle();
	current->nice = 20;
	current->counter = -100;
	while (1) {
		__cli();
		if (current->need_resched) {
			__sti();
			schedule();
			check_pgt_cache();
			continue;
		}

		/* 
		 * Wait for external, I/O or machine check interrupt and
		 * switch of machine check bit after the wait has ended.
		 */
		wait_psw.mask = _WAIT_PSW_MASK;
		asm volatile (
			"    basr %0,0\n"
			"0:  la   %0,1f-0b(%0)\n"
			"    st   %0,4(%1)\n"
			"    oi   4(%1),0x80\n"
			"    lpsw 0(%1)\n"
			"1:  la   %0,2f-1b(%0)\n"
			"    st   %0,4(%1)\n"
			"    oi   4(%1),0x80\n"
			"    ni   1(%1),0xf9\n"
			"    lpsw 0(%1)\n"
			"2:"
			: "=&a" (reg) : "a" (&wait_psw) : "memory", "cc" );
	}
}

extern void show_registers(struct pt_regs *regs);
extern void show_trace(unsigned long *sp);

void show_regs(struct pt_regs *regs)
{
	struct task_struct *tsk = current;

        printk("CPU:    %d    %s\n", tsk->processor, print_tainted());
        printk("Process %s (pid: %d, task: %08lx, ksp: %08x)\n",
	       current->comm, current->pid, (unsigned long) tsk,
	       tsk->thread.ksp);

	show_registers(regs);
	/* Show stack backtrace if pt_regs is from kernel mode */
	if (!(regs->psw.mask & PSW_PROBLEM_STATE))
		show_trace((unsigned long *) regs->gprs[15]);
}

int arch_kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
        int clone_arg = flags | CLONE_VM;
        int retval;

        __asm__ __volatile__(
                "     sr    2,2\n"
                "     lr    3,%1\n"
                "     l     4,%6\n"     /* load kernel stack ptr of parent */
                "     svc   %b2\n"                     /* Linux system call*/
                "     cl    4,%6\n"    /* compare ksp's: child or parent ? */
                "     je    0f\n"                          /* parent - jump*/
                "     l     15,%6\n"            /* fix kernel stack pointer*/
                "     ahi   15,%7\n"
                "     xc    0(96,15),0(15)\n"           /* clear save area */
                "     lr    2,%4\n"                        /* load argument*/
                "     lr    14,%5\n"                      /* get fn-pointer*/
                "     basr  14,14\n"                             /* call fn*/
                "     svc   %b3\n"                     /* Linux system call*/
                "0:   lr    %0,2"
                : "=a" (retval)
                : "d" (clone_arg), "i" (__NR_clone), "i" (__NR_exit),
                  "d" (arg), "d" (fn), "i" (__LC_KERNEL_STACK) , "i" (-STACK_FRAME_OVERHEAD)
                : "2", "3", "4" );
        return retval;
}

/*
 * Free current thread data structures etc..
 */
void exit_thread(void)
{
}

void flush_thread(void)
{

        current->used_math = 0;
        current->flags &= ~PF_USEDFPU;
}

void release_thread(struct task_struct *dead_task)
{
}

int copy_thread(int nr, unsigned long clone_flags, unsigned long new_stackp,
	unsigned long unused,
        struct task_struct * p, struct pt_regs * regs)
{
        struct stack_frame
          {
            unsigned long back_chain;
            unsigned long eos;
            unsigned long glue1;
            unsigned long glue2;
            unsigned long scratch[2];
            unsigned long gprs[10];    /* gprs 6 -15                       */
            unsigned long fprs[4];     /* fpr 4 and 6                      */
            unsigned long empty[4];
            struct pt_regs childregs;
          } *frame;

        frame = (struct stack_frame *) (2*PAGE_SIZE + (unsigned long) p) -1;
        p->thread.ksp = (unsigned long) frame;
        memcpy(&frame->childregs,regs,sizeof(struct pt_regs));
        frame->childregs.gprs[15] = new_stackp;
        frame->back_chain = frame->eos = 0;

        /* new return point is ret_from_sys_call */
        frame->gprs[8] = ((unsigned long) &ret_from_fork) | 0x80000000;

        /* fake return stack for resume(), don't go back to schedule */
        frame->gprs[9]  = (unsigned long) frame;
        /* save fprs, if used in last task */
	save_fp_regs(&p->thread.fp_regs);
        p->thread.user_seg = __pa((unsigned long) p->mm->pgd) | _SEGMENT_TABLE;
	/* start process with ar4 pointing to the correct address space */
	p->thread.ar4 = get_fs().ar4;
        /* Don't copy debug registers */
        memset(&p->thread.per_info,0,sizeof(p->thread.per_info));
        return 0;
}

asmlinkage int sys_fork(struct pt_regs regs)
{
        return do_fork(SIGCHLD, regs.gprs[15], &regs, 0);
}

asmlinkage int sys_clone(struct pt_regs regs)
{
        unsigned long clone_flags;
        unsigned long newsp;

        clone_flags = regs.gprs[3];
        newsp = regs.orig_gpr2;
        if (!newsp)
                newsp = regs.gprs[15];
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
asmlinkage int sys_vfork(struct pt_regs regs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD,
                       regs.gprs[15], &regs, 0);
}

/*
 * sys_execve() executes a new program.
 */
asmlinkage int sys_execve(struct pt_regs regs)
{
        int error;
        char * filename;

        filename = getname((char *) regs.orig_gpr2);
        error = PTR_ERR(filename);
        if (IS_ERR(filename))
                goto out;
        error = do_execve(filename, (char **) regs.gprs[3], (char **) regs.gprs[4], &regs);
	if (error == 0)
	{
		current->ptrace &= ~PT_DTRACE;
		current->thread.fp_regs.fpc=0;
		if(MACHINE_HAS_IEEE)
		{
			__asm__ __volatile__
			("sr  0,0\n\t"
			 "sfpc 0,0\n\t"
				:
			        :
                                :"0");
		}
	}
        putname(filename);
out:
        return error;
}


/*
 * fill in the FPU structure for a core dump.
 */
int dump_fpu (struct pt_regs * regs, s390_fp_regs *fpregs)
{
	save_fp_regs(fpregs);
	return 1;
}

/*
 * fill in the user structure for a core dump..
 */
void dump_thread(struct pt_regs * regs, struct user * dump)
{

/* changed the size calculations - should hopefully work better. lbt */
	dump->magic = CMAGIC;
	dump->start_code = 0;
	dump->start_stack = regs->gprs[15] & ~(PAGE_SIZE - 1);
	dump->u_tsize = ((unsigned long) current->mm->end_code) >> PAGE_SHIFT;
	dump->u_dsize = ((unsigned long) (current->mm->brk + (PAGE_SIZE-1))) >> PAGE_SHIFT;
	dump->u_dsize -= dump->u_tsize;
	dump->u_ssize = 0;
	if (dump->start_stack < TASK_SIZE)
		dump->u_ssize = ((unsigned long) (TASK_SIZE - dump->start_stack)) >> PAGE_SHIFT;
	memcpy(&dump->regs.gprs[0],regs,sizeof(s390_regs));
	dump_fpu (regs, &dump->regs.fp_regs);
	memcpy(&dump->regs.per_info,&current->thread.per_info,sizeof(per_struct));
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
	unsigned long r14, r15, bc;
	unsigned long stack_page;
	int count = 0;
	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;
	stack_page = (unsigned long) p;
	r15 = p->thread.ksp;
        if (!stack_page || r15 < stack_page || r15 >= 8188+stack_page)
                return 0;
        bc = (*(unsigned long *) r15) & 0x7fffffff;
	do {
                if (bc < stack_page || bc >= 8188+stack_page)
                        return 0;
		r14 = (*(unsigned long *) (bc+56)) & 0x7fffffff;
		if (r14 < first_sched || r14 >= last_sched)
			return r14;
		bc = (*(unsigned long *) bc) & 0x7fffffff;
	} while (count++ < 16);
	return 0;
}
#undef last_sched
#undef first_sched

