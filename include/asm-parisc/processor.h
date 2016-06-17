/*
 * include/asm-parisc/processor.h
 *
 * Copyright (C) 1994 Linus Torvalds
 * Copyright (C) 2001 Grant Grundler
 */

#ifndef __ASM_PARISC_PROCESSOR_H
#define __ASM_PARISC_PROCESSOR_H

#ifndef __ASSEMBLY__
#include <linux/config.h>
#include <linux/threads.h>

#include <asm/hardware.h>
#include <asm/page.h>
#include <asm/pdc.h>
#include <asm/ptrace.h>
#include <asm/types.h>
#include <asm/system.h>
#ifdef CONFIG_SMP
#include <asm/spinlock_t.h>
#endif
#endif /* __ASSEMBLY__ */

/*
 * Default implementation of macro that returns current
 * instruction pointer ("program counter").
 */

/* We cannot use MFIA as it was added for PA2.0 - prumpf

   At one point there were no "0f/0b" type local symbols in gas for
   PA-RISC.  This is no longer true, but this still seems like the
   nicest way to implement this. */

#define current_text_addr() ({ void *pc; __asm__("\n\tblr 0,%0\n\tnop":"=r" (pc)); pc; })

#define TASK_SIZE               (current->thread.task_size)
#define DEFAULT_TASK_SIZE       (0xFFF00000UL)

#define TASK_UNMAPPED_BASE      (current->thread.map_base)
#define DEFAULT_MAP_BASE        (0x40000000UL)

#ifndef __ASSEMBLY__

/*
** Data detected about CPUs at boot time which is the same for all CPU's.
** HP boxes are SMP - ie identical processors.
**
** FIXME: some CPU rev info may be processor specific...
*/
struct system_cpuinfo_parisc {
	unsigned int	cpu_count;
	unsigned int	cpu_hz;
	unsigned int	hversion;
	unsigned int	sversion;
	enum cpu_type	cpu_type;

	struct {
		struct pdc_model model;
		unsigned long versions;
		unsigned long cpuid;
		unsigned long capabilities;
		char   sys_model_name[81]; /* PDC-ROM returnes this model name */
	} pdc;

	char		*cpu_name;	/* e.g. "PA7300LC (PCX-L2)" */
	char		*family_name;	/* e.g. "1.1e" */
};


/*
** Per CPU data structure - ie varies per CPU.
*/
struct cpuinfo_parisc {

	unsigned long it_value;     /* Interval Timer value at last timer Intr */
	unsigned long it_delta;     /* Interval Timer delta (tic_10ms / HZ * 100) */
	unsigned long irq_count;    /* number of IRQ's since boot */
	unsigned long irq_max_cr16; /* longest time to handle a single IRQ */
	unsigned long cpuid;        /* aka slot_number or set to NO_PROC_ID */
	unsigned long hpa;          /* Host Physical address */
	unsigned long txn_addr;     /* MMIO addr of EIR or id_eid */
#ifdef CONFIG_SMP
	spinlock_t lock;            /* synchronization for ipi's */
	unsigned long pending_ipi;  /* bitmap of type ipi_message_type */
	unsigned long ipi_count;    /* number ipi Interrupts */
#endif
	unsigned long bh_count;     /* number of times bh was invoked */
	unsigned long prof_counter; /* per CPU profiling support */
	unsigned long prof_multiplier;	/* per CPU profiling support */
	unsigned long fp_rev;
	unsigned long fp_model;
	unsigned int state;
	struct parisc_device *dev;
};

extern struct system_cpuinfo_parisc boot_cpu_data;
extern struct cpuinfo_parisc cpu_data[NR_CPUS];
#define current_cpu_data cpu_data[smp_processor_id()]

#define CPU_HVERSION ((boot_cpu_data.hversion >> 4) & 0x0FFF)

#ifdef CONFIG_EISA
extern int EISA_bus;
#else
#define EISA_bus 0
#endif

#define MCA_bus 0
#define MCA_bus__is_a_macro /* for versions in ksyms.c */

typedef struct {
	int seg;  
} mm_segment_t;

struct thread_struct {
	struct pt_regs regs;
	unsigned long  task_size;
	unsigned long  map_base;
	unsigned long  flags;
}; 

/* Thread struct flags. */
#define PARISC_KERNEL_DEATH	(1UL << 31)	/* see die_if_kernel()... */

#define INIT_THREAD { \
	regs:	{	gr: { 0, }, \
			fr: { 0, }, \
			sr: { 0, }, \
			iasq: { 0, }, \
			iaoq: { 0, }, \
			cr27: 0, \
		}, \
	task_size:      DEFAULT_TASK_SIZE, \
	map_base:       DEFAULT_MAP_BASE, \
	flags:          0 \
	}

/*
 * Return saved PC of a blocked thread.  This is used by ps mostly.
 */

static inline unsigned long thread_saved_pc(struct thread_struct *t)
{
	return 0xabcdef;
}

/*
 * Start user thread in another space.
 *
 * Note that we set both the iaoq and r31 to the new pc. When
 * the kernel initially calls execve it will return through an
 * rfi path that will use the values in the iaoq. The execve
 * syscall path will return through the gateway page, and
 * that uses r31 to branch to.
 *
 * For ELF we clear r23, because the dynamic linker uses it to pass
 * the address of the finalizer function.
 *
 * We also initialize sr3 to an illegal value (illegal for our
 * implementation, not for the architecture).
 */

#define start_thread_som(regs, new_pc, new_sp) do {	\
	unsigned long *sp = (unsigned long *)new_sp;	\
	__u32 spaceid = (__u32)current->mm->context;	\
	unsigned long pc = (unsigned long)new_pc;	\
	/* offset pc for priv. level */			\
	pc |= 3;					\
							\
	set_fs(USER_DS);				\
	regs->iasq[0] = spaceid;			\
	regs->iasq[1] = spaceid;			\
	regs->iaoq[0] = pc;				\
	regs->iaoq[1] = pc + 4;                         \
	regs->sr[2] = LINUX_GATEWAY_SPACE;              \
	regs->sr[3] = 0xffff;				\
	regs->sr[4] = spaceid;				\
	regs->sr[5] = spaceid;				\
	regs->sr[6] = spaceid;				\
	regs->sr[7] = spaceid;				\
	regs->gr[ 0] = USER_PSW;                        \
	regs->gr[30] = ((new_sp)+63)&~63;		\
	regs->gr[31] = pc;				\
							\
	get_user(regs->gr[26],&sp[0]);			\
	get_user(regs->gr[25],&sp[-1]); 		\
	get_user(regs->gr[24],&sp[-2]); 		\
	get_user(regs->gr[23],&sp[-3]); 		\
} while(0)

/* The ELF abi wants things done a "wee bit" differently than
 * som does.  Supporting this behavior here avoids
 * having our own version of create_elf_tables.
 *
 * Oh, and yes, that is not a typo, we are really passing argc in r25
 * and argv in r24 (rather than r26 and r25).  This is because that's
 * where __libc_start_main wants them.
 *
 * Duplicated from dl-machine.h for the benefit of readers:
 *
 *  Our initial stack layout is rather different from everyone else's
 *  due to the unique PA-RISC ABI.  As far as I know it looks like
 *  this:

   -----------------------------------  (user startup code creates this frame)
   |         32 bytes of magic       |
   |---------------------------------|
   | 32 bytes argument/sp save area  |
   |---------------------------------| (bprm->p)
   |	    ELF auxiliary info	     |
   |         (up to 28 words)        |
   |---------------------------------|
   |		   NULL		     |
   |---------------------------------|
   |	   Environment pointers	     |
   |---------------------------------|
   |		   NULL		     |
   |---------------------------------|
   |        Argument pointers        |
   |---------------------------------| <- argv
   |          argc (1 word)          |
   |---------------------------------| <- bprm->exec (HACK!)
   |         N bytes of slack        |
   |---------------------------------|
   |	filename passed to execve    |
   |---------------------------------| (mm->env_end)
   |           env strings           |
   |---------------------------------| (mm->env_start, mm->arg_end)
   |           arg strings           |
   |---------------------------------|
   | additional faked arg strings if |
   | we're invoked via binfmt_script |
   |---------------------------------| (mm->arg_start)
   stack base is at TASK_SIZE - rlim_max.

on downward growing arches, it looks like this:
   stack base at TASK_SIZE
   | filename passed to execve
   | env strings
   | arg strings
   | faked arg strings
   | slack
   | ELF
   | envps
   | argvs
   | argc

 *  The pleasant part of this is that if we need to skip arguments we
 *  can just decrement argc and move argv, because the stack pointer
 *  is utterly unrelated to the location of the environment and
 *  argument vectors.
 *
 * Note that the S/390 people took the easy way out and hacked their
 * GCC to make the stack grow downwards.
 */

#define start_thread(regs, new_pc, new_sp) do {		\
	elf_addr_t *sp = (elf_addr_t *)new_sp;		\
	__u32 spaceid = (__u32)current->mm->context;	\
	elf_addr_t pc = (elf_addr_t)new_pc | 3;		\
	elf_caddr_t *argv = (elf_caddr_t *)bprm->exec + 1;	\
							\
	set_fs(USER_DS);				\
	regs->iasq[0] = spaceid;			\
	regs->iasq[1] = spaceid;			\
	regs->iaoq[0] = pc;				\
	regs->iaoq[1] = pc + 4;                         \
	regs->sr[2] = LINUX_GATEWAY_SPACE;              \
	regs->sr[3] = 0xffff;				\
	regs->sr[4] = spaceid;				\
	regs->sr[5] = spaceid;				\
	regs->sr[6] = spaceid;				\
	regs->sr[7] = spaceid;				\
	regs->gr[ 0] = USER_PSW;                        \
	regs->fr[ 0] = 0LL;                            	\
	regs->fr[ 1] = 0LL;                            	\
	regs->fr[ 2] = 0LL;                            	\
	regs->fr[ 3] = 0LL;                            	\
	regs->gr[30] = ((unsigned long)sp + 63) &~ 63;	\
	regs->gr[31] = pc;				\
							\
	get_user(regs->gr[25], (argv - 1));		\
	regs->gr[24] = (long) argv;			\
	regs->gr[23] = 0;				\
} while(0)

struct task_struct;
struct mm_struct;

/* Free all resources held by a thread. */
extern void release_thread(struct task_struct *);
extern int arch_kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

extern void map_hpux_gateway_page(struct task_struct *tsk, struct mm_struct *mm);

#define copy_segments(tsk, mm)  do { \
					if (tsk->personality == PER_HPUX)  \
					    map_hpux_gateway_page(tsk,mm); \
				} while (0)
#define release_segments(mm)	do { } while (0)

static inline unsigned long get_wchan(struct task_struct *p)
{
	return 0xdeadbeef; /* XXX */
}

#define KSTK_EIP(tsk)	((tsk)->thread.regs.iaoq[0])
#define KSTK_ESP(tsk)	((tsk)->thread.regs.gr[30])

#endif /* __ASSEMBLY__ */

#ifdef  CONFIG_PA20
#define ARCH_HAS_PREFETCH
extern inline void prefetch(const void *addr)
{
	__asm__("ldw 0(%0), %%r0" : : "r" (addr));
}

#define ARCH_HAS_PREFETCHW
extern inline void prefetchw(const void *addr)
{
	__asm__("ldd 0(%0), %%r0" : : "r" (addr));
}
#endif

/* Be sure to hunt all references to this down when you change the size of
 * the kernel stack */

#define THREAD_SIZE	(4*PAGE_SIZE)

#define alloc_task_struct() \
	((struct task_struct *)	__get_free_pages(GFP_KERNEL,2))
#define free_task_struct(p)	free_pages((unsigned long)(p),2)
#define get_task_struct(tsk)	atomic_inc(&virt_to_page(tsk)->count)

#define init_task (init_task_union.task) 
#define init_stack (init_task_union.stack)

#define cpu_relax()	do { } while (0)

#endif /* __ASM_PARISC_PROCESSOR_H */
