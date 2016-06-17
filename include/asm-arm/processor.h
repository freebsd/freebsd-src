/*
 *  linux/include/asm-arm/processor.h
 *
 *  Copyright (C) 1995-2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARM_PROCESSOR_H
#define __ASM_ARM_PROCESSOR_H

/*
 * Default implementation of macro that returns current
 * instruction pointer ("program counter").
 */
#define current_text_addr() ({ __label__ _l; _l: &&_l;})

#define FP_SIZE 35

struct fp_hard_struct {
	unsigned int save[FP_SIZE];		/* as yet undefined */
};

struct fp_soft_struct {
	unsigned int save[FP_SIZE];		/* undefined information */
};

union fp_state {
	struct fp_hard_struct	hard;
	struct fp_soft_struct	soft;
};

typedef unsigned long mm_segment_t;		/* domain register	*/

#ifdef __KERNEL__

#define EISA_bus 0
#define MCA_bus 0
#define MCA_bus__is_a_macro

#include <asm/atomic.h>
#include <asm/ptrace.h>
#include <asm/arch/memory.h>
#include <asm/proc/processor.h>
#include <asm/types.h>

union debug_insn {
	u32	arm;
	u16	thumb;
};

struct debug_entry {
	u32			address;
	union debug_insn	insn;
};

struct debug_info {
	int			nsaved;
	struct debug_entry	bp[2];
};

struct thread_struct {
	atomic_t			refcount;
							/* fault info	  */
	unsigned long			address;
	unsigned long			trap_no;
	unsigned long			error_code;
							/* floating point */
	union fp_state			fpstate;
							/* debugging	  */
	struct debug_info		debug;
							/* context info	  */
	struct context_save_struct	*save;
	EXTRA_THREAD_STRUCT
};

#define INIT_THREAD  {					\
	refcount:	ATOMIC_INIT(1),			\
	EXTRA_THREAD_STRUCT_INIT			\
}

/*
 * Return saved PC of a blocked thread.
 */
static inline unsigned long thread_saved_pc(struct thread_struct *t)
{
	return t->save ? pc_pointer(t->save->pc) : 0;
}

static inline unsigned long thread_saved_fp(struct thread_struct *t)
{
	return t->save ? t->save->fp : 0;
}

/* Forward declaration, a strange C thing */
struct task_struct;

/* Free all resources held by a thread. */
extern void release_thread(struct task_struct *);

/* Copy and release all segment info associated with a VM */
#define copy_segments(tsk, mm)		do { } while (0)
#define release_segments(mm)		do { } while (0)

unsigned long get_wchan(struct task_struct *p);

#define THREAD_SIZE	(8192)

extern struct task_struct *alloc_task_struct(void);
extern void __free_task_struct(struct task_struct *);
#define get_task_struct(p)	atomic_inc(&(p)->thread.refcount)
#define free_task_struct(p)					\
 do {								\
	if (atomic_dec_and_test(&(p)->thread.refcount))		\
		__free_task_struct((p));			\
 } while (0)

#define init_task	(init_task_union.task)
#define init_stack	(init_task_union.stack)

#define cpu_relax()	barrier()

/*
 * Create a new kernel thread
 */
extern int arch_kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);

#endif

#endif /* __ASM_ARM_PROCESSOR_H */
