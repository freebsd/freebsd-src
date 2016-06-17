/*
 * include/asm-alpha/processor.h
 *
 * Copyright (C) 1994 Linus Torvalds
 */

#ifndef __ASM_ALPHA_PROCESSOR_H
#define __ASM_ALPHA_PROCESSOR_H

#include <linux/personality.h>	/* for ADDR_LIMIT_32BIT */

/*
 * Returns current instruction pointer ("program counter").
 */
#define current_text_addr() \
  ({ void *__pc; __asm__ ("br %0,.+4" : "=r"(__pc)); __pc; })

/*
 * We have a 42-bit user address space: 4TB user VM...
 */
#define TASK_SIZE (0x40000000000UL)

/* This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE \
  ((current->personality & ADDR_LIMIT_32BIT) ? 0x40000000 : TASK_SIZE / 2)

/*
 * Bus types
 */
#define EISA_bus 1
#define EISA_bus__is_a_macro /* for versions in ksyms.c */
#define MCA_bus 0
#define MCA_bus__is_a_macro /* for versions in ksyms.c */

typedef struct {
	unsigned long seg;
} mm_segment_t;

struct thread_struct {
	/* the fields below are used by PALcode and must match struct pcb: */
	unsigned long ksp;
	unsigned long usp;
	unsigned long ptbr;
	unsigned int pcc;
	unsigned int asn;
	unsigned long unique;
	/*
	 * bit  0: floating point enable
	 * bit 62: performance monitor enable
	 */
	unsigned long pal_flags;
	unsigned long res1, res2;

	/*
	 * The fields below are Linux-specific:
	 *
	 * bit 1..6: IEEE_TRAP_ENABLE bits (see fpu.h)
	 * bit 7..9: UAC bits (see sysinfo.h)
	 * bit 17..21: IEEE_STATUS_MASK bits (see fpu.h)
	 * bit 63: die_if_kernel recursion lock
	 */
	unsigned long flags;

	/* Perform syscall argument validation (get/set_fs). */
	mm_segment_t fs;

	/* Breakpoint handling for ptrace.  */
	unsigned long bpt_addr[2];
	unsigned int bpt_insn[2];
	int bpt_nsaved;
};

#define INIT_THREAD  { \
	0, 0, 0, \
	0, 0, 0, \
	0, 0, 0, \
	0, \
	KERNEL_DS \
}

#define THREAD_SIZE (2*PAGE_SIZE)

#include <asm/ptrace.h>

/*
 * Return saved PC of a blocked thread.  This assumes the frame
 * pointer is the 6th saved long on the kernel stack and that the
 * saved return address is the first long in the frame.  This all
 * holds provided the thread blocked through a call to schedule() ($15
 * is the frame pointer in schedule() and $15 is saved at offset 48 by
 * entry.S:do_switch_stack).
 *
 * Under heavy swap load I've seen this lose in an ugly way.  So do
 * some extra sanity checking on the ranges we expect these pointers
 * to be in so that we can fail gracefully.  This is just for ps after
 * all.  -- r~
 */
extern inline unsigned long thread_saved_pc(struct thread_struct *t)
{
	unsigned long fp, sp = t->ksp, base = (unsigned long)t;
 
	if (sp > base && sp+6*8 < base + 16*1024) {
		fp = ((unsigned long*)sp)[6];
		if (fp > sp && fp < base + 16*1024)
			return *(unsigned long *)fp;
	}

	return 0;
}

/* Do necessary setup to start up a newly executed thread.  */
extern void start_thread(struct pt_regs *, unsigned long, unsigned long);

struct task_struct;

/* Free all resources held by a thread. */
extern void release_thread(struct task_struct *);

/* Create a kernel thread without removing it from tasklists.  */
extern long arch_kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);

#define copy_segments(tsk, mm)		do { } while (0)
#define release_segments(mm)		do { } while (0)

unsigned long get_wchan(struct task_struct *p);

/* See arch/alpha/kernel/ptrace.c for details.  */
#define PT_REG(reg)	(PAGE_SIZE*2 - sizeof(struct pt_regs)		\
			 + (long)&((struct pt_regs *)0)->reg)

#define SW_REG(reg)	(PAGE_SIZE*2 - sizeof(struct pt_regs)		\
			 - sizeof(struct switch_stack)			\
			 + (long)&((struct switch_stack *)0)->reg)

#define KSTK_EIP(tsk) \
    (*(unsigned long *)(PT_REG(pc) + (unsigned long)(tsk)))

#define KSTK_ESP(tsk)	((tsk) == current ? rdusp() : (tsk)->thread.usp)

/* NOTE: The task struct and the stack go together!  */
#define alloc_task_struct() \
        ((struct task_struct *) __get_free_pages(GFP_KERNEL,1))
#define free_task_struct(p)     free_pages((unsigned long)(p),1)
#define get_task_struct(tsk)      atomic_inc(&virt_to_page(tsk)->count)

#define init_task	(init_task_union.task)
#define init_stack	(init_task_union.stack)

#define cpu_relax()	barrier()

#define ARCH_HAS_PREFETCH
#define ARCH_HAS_PREFETCHW
#define ARCH_HAS_SPINLOCK_PREFETCH

extern inline void prefetch(const void *ptr)  
{ 
	__asm__ ("ldl $31,%0" : : "m"(*(char *)ptr)); 
}

extern inline void prefetchw(const void *ptr)  
{
	__asm__ ("ldl $31,%0" : : "m"(*(char *)ptr)); 
}

extern inline void spin_lock_prefetch(const void *ptr)  
{
	__asm__ ("ldl $31,%0" : : "m"(*(char *)ptr)); 
}
	


#endif /* __ASM_ALPHA_PROCESSOR_H */
