#ifndef _ASM_IA64_SYSTEM_H
#define _ASM_IA64_SYSTEM_H

/*
 * System defines. Note that this is included both from .c and .S
 * files, so it does only defines, not any C code.  This is based
 * on information published in the Processor Abstraction Layer
 * and the System Abstraction Layer manual.
 *
 * Copyright (C) 1998-2002 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 * Copyright (C) 1999 Asit Mallick <asit.k.mallick@intel.com>
 * Copyright (C) 1999 Don Dugger <don.dugger@intel.com>
 */
#include <linux/config.h>

#include <asm/kregs.h>
#include <asm/page.h>
#include <asm/pal.h>

#define KERNEL_START		(PAGE_OFFSET + 68*1024*1024)

#define GATE_ADDR		(0xa000000000000000 + PAGE_SIZE)
#define PERCPU_ADDR		(0xa000000000000000 + 2*PAGE_SIZE)

#ifndef __ASSEMBLY__

#include <linux/kernel.h>
#include <linux/types.h>

struct pci_vector_struct {
	__u16 segment;	/* PCI Segment number */
	__u16 bus;	/* PCI Bus number */
	__u32 pci_id;	/* ACPI split 16 bits device, 16 bits function (see section 6.1.1) */
	__u8 pin;	/* PCI PIN (0 = A, 1 = B, 2 = C, 3 = D) */
	__u32 irq;	/* IRQ assigned */
};

extern struct ia64_boot_param {
	__u64 command_line;		/* physical address of command line arguments */
	__u64 efi_systab;		/* physical address of EFI system table */
	__u64 efi_memmap;		/* physical address of EFI memory map */
	__u64 efi_memmap_size;		/* size of EFI memory map */
	__u64 efi_memdesc_size;		/* size of an EFI memory map descriptor */
	__u32 efi_memdesc_version;	/* memory descriptor version */
	struct {
		__u16 num_cols;	/* number of columns on console output device */
		__u16 num_rows;	/* number of rows on console output device */
		__u16 orig_x;	/* cursor's x position */
		__u16 orig_y;	/* cursor's y position */
	} console_info;
	__u64 fpswa;		/* physical address of the fpswa interface */
	__u64 initrd_start;
	__u64 initrd_size;
} *ia64_boot_param;

static inline void
ia64_insn_group_barrier (void)
{
	__asm__ __volatile__ (";;" ::: "memory");
}

/*
 * Macros to force memory ordering.  In these descriptions, "previous"
 * and "subsequent" refer to program order; "visible" means that all
 * architecturally visible effects of a memory access have occurred
 * (at a minimum, this means the memory has been read or written).
 *
 *   wmb():	Guarantees that all preceding stores to memory-
 *		like regions are visible before any subsequent
 *		stores and that all following stores will be
 *		visible only after all previous stores.
 *   rmb():	Like wmb(), but for reads.
 *   mb():	wmb()/rmb() combo, i.e., all previous memory
 *		accesses are visible before all subsequent
 *		accesses and vice versa.  This is also known as
 *		a "fence."
 *
 * Note: "mb()" and its variants cannot be used as a fence to order
 * accesses to memory mapped I/O registers.  For that, mf.a needs to
 * be used.  However, we don't want to always use mf.a because (a)
 * it's (presumably) much slower than mf and (b) mf.a is supported for
 * sequential memory pages only.
 */
#define mb()	__asm__ __volatile__ ("mf" ::: "memory")
#define rmb()	mb()
#define wmb()	mb()

#ifdef CONFIG_SMP
# define smp_mb()	mb()
# define smp_rmb()	rmb()
# define smp_wmb()	wmb()
#else
# define smp_mb()	barrier()
# define smp_rmb()	barrier()
# define smp_wmb()	barrier()
#endif

/*
 * XXX check on these---I suspect what Linus really wants here is
 * acquire vs release semantics but we can't discuss this stuff with
 * Linus just yet.  Grrr...
 */
#define set_mb(var, value)	do { (var) = (value); mb(); } while (0)
#define set_wmb(var, value)	do { (var) = (value); mb(); } while (0)

#define safe_halt()         ia64_pal_halt_light()    /* PAL_HALT_LIGHT */

/*
 * The group barrier in front of the rsm & ssm are necessary to ensure
 * that none of the previous instructions in the same group are
 * affected by the rsm/ssm.
 */
/* For spinlocks etc */

#ifdef CONFIG_IA64_DEBUG_IRQ

  extern unsigned long last_cli_ip;

# define local_irq_save(x)								\
do {											\
	unsigned long ip, psr;								\
											\
	__asm__ __volatile__ ("mov %0=psr;; rsm psr.i;;" : "=r" (psr) :: "memory");	\
	if (psr & IA64_PSR_I) {								\
		__asm__ ("mov %0=ip" : "=r"(ip));					\
		last_cli_ip = ip;							\
	}										\
	(x) = psr;									\
} while (0)

# define local_irq_disable()								\
do {											\
	unsigned long ip, psr;								\
											\
	__asm__ __volatile__ ("mov %0=psr;; rsm psr.i;;" : "=r" (psr) :: "memory");	\
	if (psr & IA64_PSR_I) {								\
		__asm__ ("mov %0=ip" : "=r"(ip));					\
		last_cli_ip = ip;							\
	}										\
} while (0)

# define local_irq_set(x)								\
do {											\
	unsigned long psr;								\
											\
	__asm__ __volatile__ ("mov %0=psr;;"						\
			      "ssm psr.i;;"						\
			      "srlz.d"							\
			      : "=r" (psr) :: "memory");				\
	(x) = psr;									\
} while (0)

# define local_irq_restore(x)							\
do {										\
	unsigned long ip, old_psr, psr = (x);					\
										\
	__asm__ __volatile__ ("mov %0=psr;"					\
			      "cmp.ne p6,p7=%1,r0;;"				\
			      "(p6) ssm psr.i;"					\
			      "(p7) rsm psr.i;;"				\
			      "(p6) srlz.d"					\
			      : "=&r" (old_psr) : "r"((psr) & IA64_PSR_I)	\
			      : "p6", "p7", "memory");				\
	if ((old_psr & IA64_PSR_I) && !(psr & IA64_PSR_I)) {			\
		__asm__ ("mov %0=ip" : "=r"(ip));				\
		last_cli_ip = ip;						\
	}									\
} while (0)

#else /* !CONFIG_IA64_DEBUG_IRQ */
  /* clearing of psr.i is implicitly serialized (visible by next insn) */
# define local_irq_save(x)	__asm__ __volatile__ ("mov %0=psr;; rsm psr.i;;"	\
						      : "=r" (x) :: "memory")
# define local_irq_disable()	__asm__ __volatile__ (";; rsm psr.i;;" ::: "memory")
/* (potentially) setting psr.i requires data serialization: */
# define local_irq_set(x)	__asm__ __volatile__ ("mov %0=psr;;"			\
						      "ssm psr.i;;"			\
						      "srlz.d"				\
						      : "=r" (x) :: "memory")
# define local_irq_restore(x)	__asm__ __volatile__ ("cmp.ne p6,p7=%0,r0;;"		\
						      "(p6) ssm psr.i;"			\
						      "(p7) rsm psr.i;;"		\
						      "srlz.d"				\
						      :: "r"((x) & IA64_PSR_I)		\
						      : "p6", "p7", "memory")
#endif /* !CONFIG_IA64_DEBUG_IRQ */

#define local_irq_enable()	__asm__ __volatile__ (";; ssm psr.i;; srlz.d" ::: "memory")

#define __cli()			local_irq_disable ()
#define __save_flags(flags)	__asm__ __volatile__ ("mov %0=psr" : "=r" (flags) :: "memory")
#define __save_and_cli(flags)	local_irq_save(flags)
#define __save_and_sti(flags)	local_irq_set(flags)
#define save_and_cli(flags)	__save_and_cli(flags)
#define save_and_sti(flags)	__save_and_sti(flags)
#define __sti()			local_irq_enable ()
#define __restore_flags(flags)	local_irq_restore(flags)

#ifdef CONFIG_SMP
  extern void __global_cli (void);
  extern void __global_sti (void);
  extern unsigned long __global_save_flags (void);
  extern void __global_restore_flags (unsigned long);
# define cli()			__global_cli()
# define sti()			__global_sti()
# define save_flags(flags)	((flags) = __global_save_flags())
# define restore_flags(flags)	__global_restore_flags(flags)
#else /* !CONFIG_SMP */
# define cli()			__cli()
# define sti()			__sti()
# define save_flags(flags)	__save_flags(flags)
# define restore_flags(flags)	__restore_flags(flags)
#endif /* !CONFIG_SMP */

#ifdef __KERNEL__

#define prepare_to_switch()    local_irq_disable()

#ifdef CONFIG_IA32_SUPPORT
# define IS_IA32_PROCESS(regs)	(ia64_psr(regs)->is != 0)
#else
# define IS_IA32_PROCESS(regs)		0
#endif

/*
 * Context switch from one thread to another.  If the two threads have
 * different address spaces, schedule() has already taken care of
 * switching to the new address space by calling switch_mm().
 *
 * Disabling access to the fph partition and the debug-register
 * context switch MUST be done before calling ia64_switch_to() since a
 * newly created thread returns directly to
 * ia64_ret_from_syscall_clear_r8.
 */
extern struct task_struct *ia64_switch_to (void *next_task);

extern void ia64_save_extra (struct task_struct *task);
extern void ia64_load_extra (struct task_struct *task);

#ifdef CONFIG_PERFMON
# define PERFMON_IS_SYSWIDE() (local_cpu_data->pfm_syst_info & 0x1)
#else
# define PERFMON_IS_SYSWIDE() (0)
#endif

#define IA64_HAS_EXTRA_STATE(t)							\
	((t)->thread.flags & (IA64_THREAD_DBG_VALID|IA64_THREAD_PM_VALID)	\
	 || IS_IA32_PROCESS(ia64_task_regs(t)) || PERFMON_IS_SYSWIDE())

#define __switch_to(prev,next,last) do {							 \
	if (IA64_HAS_EXTRA_STATE(prev))								 \
		ia64_save_extra(prev);								 \
	if (IA64_HAS_EXTRA_STATE(next))								 \
		ia64_load_extra(next);								 \
	ia64_psr(ia64_task_regs(next))->dfh = !ia64_is_local_fpu_owner(next);			 \
	(last) = ia64_switch_to((next));							 \
} while (0)

#ifdef CONFIG_SMP

/* Return true if this CPU can call the console drivers in printk() */
#define arch_consoles_callable() (cpu_online_map & (1UL << smp_processor_id()))

/*
 * In the SMP case, we save the fph state when context-switching away from a thread that
 * modified fph.  This way, when the thread gets scheduled on another CPU, the CPU can
 * pick up the state from task->thread.fph, avoiding the complication of having to fetch
 * the latest fph state from another CPU.  In other words: eager save, lazy restore.
 */
# define switch_to(prev,next,last) do {						\
	if (ia64_psr(ia64_task_regs(prev))->mfh) {				\
		ia64_psr(ia64_task_regs(prev))->mfh = 0;			\
		(prev)->thread.flags |= IA64_THREAD_FPH_VALID;			\
		__ia64_save_fpu((prev)->thread.fph);				\
	}									\
	__switch_to(prev, next, last);						\
} while (0)
#else
# define switch_to(prev,next,last)	__switch_to(prev, next, last)
#endif

#define ia64_platform_is(x) (strcmp(x, platform_name) == 0)

#endif /* __KERNEL__ */

#endif /* __ASSEMBLY__ */

#endif /* _ASM_IA64_SYSTEM_H */
