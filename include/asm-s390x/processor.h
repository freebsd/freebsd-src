/*
 *  include/asm-s390/processor.h
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Hartmut Penner (hp@de.ibm.com),
 *               Martin Schwidefsky (schwidefsky@de.ibm.com)
 *
 *  Derived from "include/asm-i386/processor.h"
 *    Copyright (C) 1994, Linus Torvalds
 */

#ifndef __ASM_S390_PROCESSOR_H
#define __ASM_S390_PROCESSOR_H

#include <asm/page.h>
#include <asm/ptrace.h>

#ifdef __KERNEL__
/*
 * Default implementation of macro that returns current
 * instruction pointer ("program counter").
 */
#define current_text_addr() ({ void *pc; __asm__("basr %0,0":"=a"(pc)); pc; })

/*
 *  CPU type and hardware bug flags. Kept separately for each CPU.
 *  Members of this structure are referenced in head.S, so think twice
 *  before touching them. [mj]
 */

typedef struct
{
        unsigned int version :  8;
        unsigned int ident   : 24;
        unsigned int machine : 16;
        unsigned int unused  : 16;
} __attribute__ ((packed)) cpuid_t;

struct cpuinfo_S390
{
        cpuid_t  cpu_id;
        unsigned long loops_per_jiffy;
        unsigned long *pgd_quick;
        unsigned long *pmd_quick;
        unsigned long *pte_quick;
        unsigned long pgtable_cache_sz;
        __u16    cpu_addr;
        __u16    cpu_nr;
        __u16    pad[2];
};

extern void print_cpu_info(struct cpuinfo_S390 *);

/* Lazy FPU handling on uni-processor */
extern struct task_struct *last_task_used_math;

#define S390_FLAG_31BIT 0x01UL

/*
 * User space process size: 4TB (default).
 */
#define TASK_SIZE       (0x20000000000UL)
#define TASK31_SIZE     (0x80000000UL)

/* This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE      ((current->thread.flags & S390_FLAG_31BIT) ? \
	(TASK31_SIZE / 2) : (TASK_SIZE / 2))

#define THREAD_SIZE (4*PAGE_SIZE)

typedef struct {
       __u32 ar4;
} mm_segment_t;

/* if you change the thread_struct structure, you must
 * update the _TSS_* defines in entry.S
 */

struct thread_struct
 {
	s390_fp_regs fp_regs;
        __u32   ar2;                   /* kernel access register 2         */
        __u32   ar4;                   /* kernel access register 4         */
        addr_t  ksp;                   /* kernel stack pointer             */
        addr_t  user_seg;              /* HSTD                             */
        addr_t  prot_addr;             /* address of protection-excep.     */
        __u32   error_code;            /* error-code of last prog-excep.   */
        __u32   trap_no;
        per_struct per_info;/* Must be aligned on an 4 byte boundary*/
	/* Used to give failing instruction back to user for ieee exceptions */
	addr_t  ieee_instruction_pointer; 
	unsigned long flags;            /* various flags */
        /* pfault_wait is used to block the process on a pfault event */
	addr_t  pfault_wait;
};

typedef struct thread_struct thread_struct;

#define INIT_THREAD {{0,{{0},{0},{0},{0},{0},{0},{0},{0},{0},{0}, \
			    {0},{0},{0},{0},{0},{0}}},            \
                     0, 0,                                        \
                    sizeof(init_stack) + (addr_t) &init_stack,    \
              (__pa((addr_t) &swapper_pg_dir[0]) + _REGION_TABLE),\
                     0,0,0,                                       \
                     (per_struct) {{{{0,}}},0,0,0,0,{{0,}}},      \
		     0, 0, 0				          \
} 

/* need to define ... */
#define start_thread(regs, new_psw, new_stackp) do {            \
        regs->psw.mask  = _USER_PSW_MASK;                       \
        regs->psw.addr  = new_psw;                              \
        regs->gprs[15]  = new_stackp;                           \
} while (0)

#define start_thread31(regs, new_psw, new_stackp) do {          \
	regs->psw.mask  = _USER_PSW_MASK & ~(1L << 32);		\
        regs->psw.addr  = new_psw;                              \
        regs->gprs[15]  = new_stackp;                           \
} while (0)


/* Forward declaration, a strange C thing */
struct mm_struct;

/* Free all resources held by a thread. */
extern void release_thread(struct task_struct *);
extern int arch_kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

/* Copy and release all segment info associated with a VM */
#define copy_segments(nr, mm)           do { } while (0)
#define release_segments(mm)            do { } while (0)

/*
 * Return saved PC of a blocked thread. used in kernel/sched
 * resume in entry.S does not create a new stack frame, it
 * just stores the registers %r6-%r15 to the frame given by
 * schedule. We want to return the address of the caller of
 * schedule, so we have to walk the backchain one time to
 * find the frame schedule() store its return address.
 */
extern inline unsigned long thread_saved_pc(struct thread_struct *t)
{
	unsigned long bc;
	bc = *((unsigned long *) t->ksp);
	return *((unsigned long *) (bc+112));
}

unsigned long get_wchan(struct task_struct *p);
#define __KSTK_PTREGS(tsk) ((struct pt_regs *) \
        (((unsigned long) tsk + THREAD_SIZE - sizeof(struct pt_regs)) & -8L))
#define KSTK_EIP(tsk)	(__KSTK_PTREGS(tsk)->psw.addr)
#define KSTK_ESP(tsk)	(__KSTK_PTREGS(tsk)->gprs[15])

/* Allocation and freeing of basic task resources. */
extern struct task_struct *alloc_task_struct(void);
extern void free_task_struct(struct task_struct *tsk);
extern void get_task_struct(struct task_struct *tsk);

#define init_task       (init_task_union.task)
#define init_stack      (init_task_union.stack)

#define cpu_relax()	do { } while (0)

/*
 * Set of msr bits that gdb can change on behalf of a process.
 */
/* Only let our hackers near the condition codes */
#define PSW_MASK_DEBUGCHANGE    0x0000300000000000UL
/* Don't let em near the addressing mode either */    
#define PSW_ADDR_DEBUGCHANGE    0xFFFFFFFFFFFFFFFFUL
#define PSW_ADDR_MASK           0xFFFFFFFFFFFFFFFFUL
/* Program event recording mask */    
#define PSW_PER_MASK            0x4000000000000000UL
#define USER_STD_MASK           0x0000000000000080UL
#define PSW_PROBLEM_STATE       0x0001000000000000UL

/*
 * Set PSW mask to specified value, while leaving the
 * PSW addr pointing to the next instruction.
 */

static inline void __load_psw_mask (unsigned long mask)
{
	unsigned long addr;

	psw_t psw;
	psw.mask = mask;

	asm volatile (
		"    larl  %0,1f\n"
		"    stg   %0,8(%1)\n"
		"    lpswe 0(%1)\n"
		"1:"
		: "=&d" (addr) : "a" (&psw) : "memory", "cc" );
}

/*
 * Function to stop a processor until an interruption occured
 */
static inline void enabled_wait(void)
{
	unsigned long reg;
	psw_t wait_psw;

	wait_psw.mask = 0x0706000180000000;
	asm volatile (
		"    larl  %0,0f\n"
		"    stg   %0,8(%1)\n"
		"    lpswe 0(%1)\n"
		"0:"
		: "=&a" (reg) : "a" (&wait_psw) : "memory", "cc" );
}

/*
 * Function to drop a processor into disabled wait state
 */

static inline void disabled_wait(addr_t code)
{
        char psw_buffer[2*sizeof(psw_t)];
        char ctl_buf[8];
        psw_t *dw_psw = (psw_t *)(((unsigned long) &psw_buffer+sizeof(psw_t)-1)
                                  & -sizeof(psw_t));

        dw_psw->mask = 0x0002000180000000;
        dw_psw->addr = code;
        /* 
         * Store status and then load disabled wait psw,
         * the processor is dead afterwards
         */
        asm volatile ("    stctg 0,0,0(%1)\n"
                      "    ni    4(%1),0xef\n" /* switch off protection */
                      "    lctlg 0,0,0(%1)\n"
                      "    lghi  1,0x1000\n"
                      "    stpt  0x328(1)\n"      /* store timer */
                      "    stckc 0x330(1)\n"      /* store clock comparator */
                      "    stpx  0x318(1)\n"      /* store prefix register */
                      "    stam  0,15,0x340(1)\n" /* store access registers */
                      "    stfpc 0x31c(1)\n"      /* store fpu control */
                      "    std   0,0x200(1)\n"    /* store f0 */
                      "    std   1,0x208(1)\n"    /* store f1 */
                      "    std   2,0x210(1)\n"    /* store f2 */
                      "    std   3,0x218(1)\n"    /* store f3 */
                      "    std   4,0x220(1)\n"    /* store f4 */
                      "    std   5,0x228(1)\n"    /* store f5 */
                      "    std   6,0x230(1)\n"    /* store f6 */
                      "    std   7,0x238(1)\n"    /* store f7 */
                      "    std   8,0x240(1)\n"    /* store f8 */
                      "    std   9,0x248(1)\n"    /* store f9 */
                      "    std   10,0x250(1)\n"   /* store f10 */
                      "    std   11,0x258(1)\n"   /* store f11 */
                      "    std   12,0x260(1)\n"   /* store f12 */
                      "    std   13,0x268(1)\n"   /* store f13 */
                      "    std   14,0x270(1)\n"   /* store f14 */
                      "    std   15,0x278(1)\n"   /* store f15 */
                      "    stmg  0,15,0x280(1)\n" /* store general registers */
                      "    stctg 0,15,0x380(1)\n" /* store control registers */
                      "    oi    0x384(1),0x10\n" /* fake protection bit */
                      "    lpswe 0(%0)"
                      : : "a" (dw_psw), "a" (&ctl_buf) : "cc", "0", "1");
}

#endif

#endif                                 /* __ASM_S390_PROCESSOR_H           */

