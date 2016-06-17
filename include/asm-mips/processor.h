/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 Waldorf GMBH
 * Copyright (C) 1995, 1996, 1997, 1998, 1999, 2001, 2002, 2003 Ralf Baechle
 * Copyright (C) 1996 Paul M. Antoine
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 */
#ifndef _ASM_PROCESSOR_H
#define _ASM_PROCESSOR_H

#include <linux/config.h>
#include <linux/cache.h>
#include <asm/isadep.h>

/*
 * Return current * instruction pointer ("program counter").
 */
#define current_text_addr() ({ __label__ _l; _l: &&_l;})

#ifndef __ASSEMBLY__
#include <linux/smp.h>
#include <linux/threads.h>

#include <asm/cachectl.h>
#include <asm/cpu.h>
#include <asm/mipsregs.h>
#include <asm/reg.h>
#include <asm/system.h>

/*
 * Descriptor for a cache
 */
struct cache_desc {
	unsigned short linesz;	/* Size of line in bytes */
	unsigned short ways;	/* Number of ways */
	unsigned short sets;	/* Number of lines per set */
	unsigned int waysize;	/* Bytes per way */
	unsigned int waybit;	/* Bits to select in a cache set */
	unsigned int flags;	/* Flags describing cache properties */
};

/*
 * Flag definitions
 */
#define MIPS_CACHE_NOT_PRESENT	0x00000001
#define MIPS_CACHE_VTAG		0x00000002	/* Virtually tagged cache */
#define MIPS_CACHE_ALIASES	0x00000004	/* Cache could have aliases */
#define MIPS_CACHE_IC_F_DC	0x00000008	/* Ic can refill from D-cache */

struct cpuinfo_mips {
	unsigned long udelay_val;
	unsigned long *pgd_quick;
	unsigned long *pte_quick;
	unsigned long pgtable_cache_sz;
	unsigned long asid_cache;
	/*
	 * Capability and feature descriptor structure for MIPS CPU
	 */
	unsigned long options;
	unsigned int processor_id;
	unsigned int fpu_id;
	unsigned int cputype;
	int isa_level;
	int tlbsize;
	struct cache_desc icache;	/* Primary I-cache */
	struct cache_desc dcache;	/* Primary D or combined I/D cache */
	struct cache_desc scache;	/* Secondary cache */
	struct cache_desc tcache;	/* Tertiary/split secondary cache */
} __attribute__((aligned(SMP_CACHE_BYTES)));

/*
 * Assumption: Options of CPU 0 are a superset of all processors.
 * This is true for all known MIPS systems.
 */
#define cpu_has_tlb		(cpu_data[0].options & MIPS_CPU_TLB)
#define cpu_has_4kex		(cpu_data[0].options & MIPS_CPU_4KEX)
#define cpu_has_4ktlb		(cpu_data[0].options & MIPS_CPU_4KTLB)
#define cpu_has_fpu		(cpu_data[0].options & MIPS_CPU_FPU)
#define cpu_has_32fpr		(cpu_data[0].options & MIPS_CPU_32FPR)
#define cpu_has_counter		(cpu_data[0].options & MIPS_CPU_COUNTER)
#define cpu_has_watch		(cpu_data[0].options & MIPS_CPU_WATCH)
#define cpu_has_mips16		(cpu_data[0].options & MIPS_CPU_MIPS16)
#define cpu_has_divec		(cpu_data[0].options & MIPS_CPU_DIVEC)
#define cpu_has_vce		(cpu_data[0].options & MIPS_CPU_VCE)
#define cpu_has_cache_cdex_p	(cpu_data[0].options & MIPS_CPU_CACHE_CDEX_P)
#define cpu_has_cache_cdex_s	(cpu_data[0].options & MIPS_CPU_CACHE_CDEX_S)
#define cpu_has_prefetch	(cpu_data[0].options & MIPS_CPU_PREFETCH)
#define cpu_has_mcheck		(cpu_data[0].options & MIPS_CPU_MCHECK)
#define cpu_has_ejtag		(cpu_data[0].options & MIPS_CPU_EJTAG)
#define cpu_has_nofpuex		(cpu_data[0].options & MIPS_CPU_NOFPUEX)
#define cpu_has_llsc		(cpu_data[0].options & MIPS_CPU_LLSC)
#define cpu_has_vtag_icache	(cpu_data[0].icache.flags & MIPS_CACHE_VTAG)
#define cpu_has_dc_aliases	(cpu_data[0].dcache.flags & MIPS_CACHE_ALIASES)
#define cpu_has_ic_fills_f_dc	(cpu_data[0].dcache.flags & MIPS_CACHE_IC_F_DC)
#define cpu_has_64bits		(cpu_data[0].isa_level & MIPS_CPU_ISA_64BIT)
#define cpu_has_64bit_addresses	0
#define cpu_has_subset_pcaches	(cpu_data[0].options & MIPS_CPU_SUBSET_CACHES)

#define cpu_dcache_line_size()	current_cpu_data.dcache.linesz
#define cpu_icache_line_size()	current_cpu_data.icache.linesz
#define cpu_scache_line_size()	current_cpu_data.scache.linesz

extern struct cpuinfo_mips cpu_data[];
#define current_cpu_data cpu_data[smp_processor_id()]

extern void cpu_probe(void);
extern void cpu_report(void);

/*
 * System setup and hardware flags..
 */
extern void (*cpu_wait)(void);

extern unsigned int vced_count, vcei_count;

/*
 * Bus types (default is ISA, but people can check others with these..)
 */
#ifdef CONFIG_EISA
extern int EISA_bus;
#else
#define EISA_bus (0)
#endif

#define MCA_bus 0
#define MCA_bus__is_a_macro /* for versions in ksyms.c */

/*
 * User space process size: 2GB. This is hardcoded into a few places,
 * so don't change it unless you know what you are doing.  TASK_SIZE
 * for a 64 bit kernel expandable to 8192EB, of which the current MIPS
 * implementations will "only" be able to use 1TB ...
 */
#define TASK_SIZE	0x7fff8000UL

/* This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE	(TASK_SIZE / 3)

/*
 * Size of io_bitmap in longwords: 32 is ports 0-0x3ff.
 */
#define IO_BITMAP_SIZE	32

#define NUM_FPU_REGS	32

struct mips_fpu_hard_struct {
	double fp_regs[NUM_FPU_REGS];
	unsigned int control;
};

/*
 * It would be nice to add some more fields for emulator statistics, but there
 * are a number of fixed offsets in offset.h and elsewhere that would have to
 * be recalculated by hand.  So the additional information will be private to
 * the FPU emulator for now.  See asm-mips/fpu_emulator.h.
 */
typedef u64 fpureg_t;
struct mips_fpu_soft_struct {
	fpureg_t	regs[NUM_FPU_REGS];
	unsigned int	sr;
};

union mips_fpu_union {
        struct mips_fpu_hard_struct hard;
        struct mips_fpu_soft_struct soft;
};

#define INIT_FPU { \
	{{0,},} \
}

typedef struct {
	unsigned long seg;
} mm_segment_t;

/*
 * If you change thread_struct remember to change the #defines below too!
 */
struct thread_struct {
	/* Saved main processor registers. */
	unsigned long reg16;
	unsigned long reg17, reg18, reg19, reg20, reg21, reg22, reg23;
	unsigned long reg29, reg30, reg31;

	/* Saved cp0 stuff. */
	unsigned long cp0_status;

	/* Saved fpu/fpu emulator stuff. */
	union mips_fpu_union fpu;

	/* Other stuff associated with the thread. */
	unsigned long cp0_badvaddr;	/* Last user fault */
	unsigned long cp0_baduaddr;	/* Last kernel fault accessing USEG */
	unsigned long error_code;
	unsigned long trap_no;
#define MF_FIXADE	1		/* Fix address errors in software */
#define MF_LOGADE	2		/* Log address errors to syslog */
	unsigned long mflags;
	mm_segment_t current_ds;
	unsigned long irix_trampoline;  /* Wheee... */
	unsigned long irix_oldctx;
};

#endif /* !__ASSEMBLY__ */

#define INIT_THREAD  { \
        /* \
         * saved main processor registers \
         */ \
	0, 0, 0, 0, 0, 0, 0, 0, \
	               0, 0, 0, \
	/* \
	 * saved cp0 stuff \
	 */ \
	0, \
	/* \
	 * saved fpu/fpu emulator stuff \
	 */ \
	INIT_FPU, \
	/* \
	 * Other stuff associated with the process \
	 */ \
	0, 0, 0, 0, \
	/* \
	 * For now the default is to fix address errors \
	 */ \
	MF_FIXADE, { 0 }, 0, 0 \
}

#ifdef __KERNEL__

#define KERNEL_STACK_SIZE	0x2000

#ifndef __ASSEMBLY__

/* Free all resources held by a thread. */
#define release_thread(thread) do { } while(0)

extern int arch_kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

/* Copy and release all segment info associated with a VM */
#define copy_segments(p, mm) do { } while(0)
#define release_segments(mm) do { } while(0)

struct mips_frame_info {
	int frame_offset;
	int pc_offset;
};
extern struct mips_frame_info schedule_frame;
/*
 * Return saved PC of a blocked thread.
 */
static inline unsigned long thread_saved_pc(struct thread_struct *t)
{
	extern void ret_from_fork(void);

	/* New born processes are a special case */
	if (t->reg31 == (unsigned long) ret_from_fork)
		return t->reg31;

	if (schedule_frame.pc_offset < 0)
		return 0;
	return ((unsigned long *)t->reg29)[schedule_frame.pc_offset];
}

/*
 * Do necessary setup to start up a newly executed thread.
 */
extern void start_thread(struct pt_regs * regs, unsigned long pc, unsigned long sp);

struct task_struct;
unsigned long get_wchan(struct task_struct *p);

#define __PT_REG(reg) ((long)&((struct pt_regs *)0)->reg - sizeof(struct pt_regs))
#define __KSTK_TOS(tsk) ((unsigned long)(tsk) + KERNEL_STACK_SIZE - 32)
#define KSTK_EIP(tsk) (*(unsigned long *)(__KSTK_TOS(tsk) + __PT_REG(cp0_epc)))
#define KSTK_ESP(tsk) (*(unsigned long *)(__KSTK_TOS(tsk) + __PT_REG(regs[29])))
#define KSTK_STATUS(tsk) (*(unsigned long *)(__KSTK_TOS(tsk) + __PT_REG(cp0_status)))

/* Allocation and freeing of basic task resources. */
/*
 * NOTE! The task struct and the stack go together
 */
#define THREAD_ORDER		(PAGE_SHIFT >= 14 ? 0 : 1)
#define THREAD_SIZE		(PAGE_SIZE << THREAD_ORDER)
#define THREAD_MASK		(THREAD_SIZE - 1UL)
#define alloc_task_struct() \
	((struct task_struct *) __get_free_pages(GFP_KERNEL,1))
#define free_task_struct(p)	free_pages((unsigned long)(p),1)
#define get_task_struct(tsk)      atomic_inc(&virt_to_page(tsk)->count)

#define init_task	(init_task_union.task)
#define init_stack	(init_task_union.stack)

#define cpu_relax()	barrier()

#endif /* !__ASSEMBLY__ */
#endif /* __KERNEL__ */

/*
 * Return_address is a replacement for __builtin_return_address(count)
 * which on certain architectures cannot reasonably be implemented in GCC
 * (MIPS, Alpha) or is unuseable with -fomit-frame-pointer (i386).
 * Note that __builtin_return_address(x>=1) is forbidden because GCC
 * aborts compilation on some CPUs.  It's simply not possible to unwind
 * some CPU's stackframes.
 *
 * __builtin_return_address works only for non-leaf functions.  We avoid the
 * overhead of a function call by forcing the compiler to save the return
 * address register on the stack.
 */
#define return_address() ({__asm__ __volatile__("":::"$31");__builtin_return_address(0);})

#endif /* _ASM_PROCESSOR_H */
