/*
 * include/asm-sh/processor.h
 *
 * Copyright (C) 1999, 2000  Niibe Yutaka
 */

#ifndef __ASM_SH_PROCESSOR_H
#define __ASM_SH_PROCESSOR_H

#include <asm/page.h>
#include <asm/types.h>
#include <linux/threads.h>

/*
 * Default implementation of macro that returns current
 * instruction pointer ("program counter").
 */
#define current_text_addr() ({ void *pc; __asm__("mova	1f, %0\n1:":"=z" (pc)); pc; })

/*
 *  CPU type and hardware bug flags. Kept separately for each CPU.
 */
enum cpu_type {
	CPU_SH7708,		/* Represents 7707, 7708, 7708S, 7708R, 7709 */
	CPU_SH7729,		/* Represents 7709A, 7729 */
	CPU_SH7750,             /* Represents 7750, 7751 */
	CPU_ST40,		/* Represents ST40STB1 and ST40GX1 */
        CPU_SH4202,
	CPU_SH_NONE
};

struct sh_cpuinfo {
	enum cpu_type type;
	char	hard_math;
	unsigned long loops_per_jiffy;

	unsigned int cpu_clock, master_clock, bus_clock, module_clock;
#ifdef CONFIG_CPU_SUBTYPE_ST40
	unsigned int memory_clock;
#endif
};

extern struct sh_cpuinfo boot_cpu_data;

#define cpu_data (&boot_cpu_data)
#define current_cpu_data boot_cpu_data

/*
 * User space process size: 2GB.
 *
 * Since SH7709 and SH7750 have "area 7", we can't use 0x7c000000--0x7fffffff
 */
#define TASK_SIZE	0x7c000000UL

/* This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE	(TASK_SIZE / 3)

/*
 * Bit of SR register
 *
 * FD-bit:
 *     When it's set, it means the processor doesn't have right to use FPU,
 *     and it results exception when the floating operation is executed.
 *
 * IMASK-bit:
 *     Interrupt level mask
 */
#define SR_FD    0x00008000
#define SR_DSP   0x00001000
#define SR_IMASK 0x000000f0

/*
 * FPU structure and data
 */

struct sh_fpu_hard_struct {
	unsigned long fp_regs[16];
	unsigned long xfp_regs[16];
	unsigned long fpscr;
	unsigned long fpul;

	long status; /* software status information */
};

/* Dummy fpu emulator  */
struct sh_fpu_soft_struct {
	unsigned long fp_regs[16];
	unsigned long xfp_regs[16];
	unsigned long fpscr;
	unsigned long fpul;

	unsigned char lookahead;
	unsigned long entry_pc;
};

union sh_fpu_union {
	struct sh_fpu_hard_struct hard;
	struct sh_fpu_soft_struct soft;
};

struct thread_struct {
	unsigned long sp;
	unsigned long pc;

	unsigned long trap_no, error_code;
	unsigned long address;

	/* Hardware debugging registers */
	unsigned long ubc_pc1, ubc_pc2;

	/* floating point info */
	union sh_fpu_union fpu;
};

/* Count of active tasks with UBC settings */
extern int ubc_usercnt;

#define INIT_THREAD  {						\
	sizeof(init_stack) + (long) &init_stack, /* sp */	\
	0,					 /* pc */	\
	0, 0, 							\
	0, 							\
	0, -1, 							\
	{{{0,}},} 				/* fpu state */	\
}

/*
 * Do necessary setup to start up a newly executed thread.
 */
#define start_thread(regs, new_pc, new_sp)	 \
	set_fs(USER_DS);			 \
	regs->pr = 0;   		 	 \
	regs->sr = 0;		/* User mode. */ \
	regs->pc = new_pc;			 \
	regs->regs[15] = new_sp

/* Forward declaration, a strange C thing */
struct task_struct;
struct mm_struct;

/* Free all resources held by a thread. */
extern void release_thread(struct task_struct *);
/*
 * create a kernel thread without removing it from tasklists
 */
extern int arch_kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

/*
 * Bus types
 */
#define EISA_bus 0
#define EISA_bus__is_a_macro /* for versions in ksyms.c */
#define MCA_bus 0
#define MCA_bus__is_a_macro /* for versions in ksyms.c */


/* Copy and release all segment info associated with a VM */
#define copy_segments(p, mm)	do { } while(0)
#define release_segments(mm)	do { } while(0)

/*
 * FPU lazy state save handling.
 */

static __inline__ void release_fpu(void)
{
	unsigned long __dummy;

	/* Set FD flag in SR */
	__asm__ __volatile__("stc	sr, %0\n\t"
			     "or	%1, %0\n\t"
			     "ldc	%0, sr"
			     : "=&r" (__dummy)
			     : "r" (SR_FD));
}

static __inline__ void grab_fpu(void)
{
	unsigned long __dummy;

	/* Clear out FD flag in SR */
	__asm__ __volatile__("stc	sr, %0\n\t"
			     "and	%1, %0\n\t"
			     "ldc	%0, sr"
			     : "=&r" (__dummy)
			     : "r" (~SR_FD));
}

extern void save_fpu(struct task_struct *__tsk);

#define unlazy_fpu(tsk) do { 			\
	if ((tsk)->flags & PF_USEDFPU) {	\
		save_fpu(tsk); 			\
	}					\
} while (0)

#define clear_fpu(tsk) do { 			\
	if ((tsk)->flags & PF_USEDFPU) { 	\
		(tsk)->flags &= ~PF_USEDFPU; 	\
		release_fpu();			\
	}					\
} while (0)

/* Double presision, NANS as NANS, rounding to nearest, no exceptions */
#define FPSCR_INIT  0x00080000

#define	FPSCR_CAUSE_MASK	0x0001f000	/* Cause bits */
#define	FPSCR_FLAG_MASK		0x0000007c	/* Flag bits */

/*
 * Return saved PC of a blocked thread.
 */
static __inline__ unsigned long thread_saved_pc(struct thread_struct *t)
{
	return t->pc;
}

extern unsigned long get_wchan(struct task_struct *p);

#define KSTK_EIP(tsk)  ((tsk)->thread.pc)
#define KSTK_ESP(tsk)  ((tsk)->thread.sp)

#define THREAD_SIZE (2*PAGE_SIZE)
extern struct task_struct * alloc_task_struct(void);
extern void free_task_struct(struct task_struct *);
#define get_task_struct(tsk)      atomic_inc(&virt_to_page(tsk)->count)

#define init_task	(init_task_union.task)
#define init_stack	(init_task_union.stack)

#define cpu_relax()	do { } while (0)

#endif /* __ASM_SH_PROCESSOR_H */
