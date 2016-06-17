/* $Id: system.h,v 1.86 2001/10/30 04:57:10 davem Exp $ */
#include <linux/config.h>

#ifndef __SPARC_SYSTEM_H
#define __SPARC_SYSTEM_H

#include <linux/kernel.h>

#include <asm/segment.h>

#ifdef __KERNEL__
#include <asm/page.h>
#include <asm/psr.h>
#include <asm/ptrace.h>
#include <asm/btfixup.h>

#endif /* __KERNEL__ */

#ifndef __ASSEMBLY__

/*
 * Sparc (general) CPU types
 */
enum sparc_cpu {
  sun4        = 0x00,
  sun4c       = 0x01,
  sun4m       = 0x02,
  sun4d       = 0x03,
  sun4e       = 0x04,
  sun4u       = 0x05, /* V8 ploos ploos */
  sun_unknown = 0x06,
  ap1000      = 0x07, /* almost a sun4m */
};

/* Really, userland should not be looking at any of this... */
#ifdef __KERNEL__

extern enum sparc_cpu sparc_cpu_model;

#ifndef CONFIG_SUN4
#define ARCH_SUN4C_SUN4 (sparc_cpu_model==sun4c)
#define ARCH_SUN4 0
#else
#define ARCH_SUN4C_SUN4 1
#define ARCH_SUN4 1
#endif

#define SUN4M_NCPUS            4              /* Architectural limit of sun4m. */

extern unsigned long empty_bad_page;
extern unsigned long empty_bad_page_table;
extern unsigned long empty_zero_page;

/* When a context switch happens we must flush all user windows so that
 * the windows of the current process are flushed onto its stack. This
 * way the windows are all clean for the next process and the stack
 * frames are up to date.
 */
extern void flush_user_windows(void);
extern void kill_user_windows(void);
extern void synchronize_user_stack(void);
extern void fpsave(unsigned long *fpregs, unsigned long *fsr,
		   void *fpqueue, unsigned long *fpqdepth);

#ifdef CONFIG_SMP
#define SWITCH_ENTER \
	if(prev->flags & PF_USEDFPU) { \
		put_psr(get_psr() | PSR_EF); \
		fpsave(&prev->thread.float_regs[0], &prev->thread.fsr, \
		       &prev->thread.fpqueue[0], &prev->thread.fpqdepth); \
		prev->flags &= ~PF_USEDFPU; \
		prev->thread.kregs->psr &= ~PSR_EF; \
	}

#define SWITCH_DO_LAZY_FPU
#else
#define SWITCH_ENTER
#define SWITCH_DO_LAZY_FPU if(last_task_used_math != next) next->thread.kregs->psr&=~PSR_EF;
#endif

/*
 * Flush windows so that the VM switch which follows
 * would not pull the stack from under us.
 *
 * SWITCH_ENTER and SWITH_DO_LAZY_FPU do not work yet (e.g. SMP does not work)
 */
#define prepare_to_switch() do { \
	__asm__ __volatile__( \
	".globl\tflush_patch_switch\nflush_patch_switch:\n\t" \
	"save %sp, -0x40, %sp; save %sp, -0x40, %sp; save %sp, -0x40, %sp\n\t" \
	"save %sp, -0x40, %sp; save %sp, -0x40, %sp; save %sp, -0x40, %sp\n\t" \
	"save %sp, -0x40, %sp\n\t" \
	"restore; restore; restore; restore; restore; restore; restore"); \
} while(0)

	/* Much care has gone into this code, do not touch it.
	 *
	 * We need to loadup regs l0/l1 for the newly forked child
	 * case because the trap return path relies on those registers
	 * holding certain values, gcc is told that they are clobbered.
	 * Gcc needs registers for 3 values in and 1 value out, so we
	 * clobber every non-fixed-usage register besides l2/l3/o4/o5.  -DaveM
	 *
	 * Hey Dave, that do not touch sign is too much of an incentive
	 * - Anton
	 */
#define switch_to(prev, next, last) do {						\
	extern struct task_struct *current_set[NR_CPUS];				\
	SWITCH_ENTER									\
	SWITCH_DO_LAZY_FPU								\
	next->active_mm->cpu_vm_mask |= (1 << smp_processor_id());			\
	__asm__ __volatile__(								\
	"sethi	%%hi(here - 0x8), %%o7\n\t"						\
	"mov	%%g6, %%g3\n\t"								\
	"or	%%o7, %%lo(here - 0x8), %%o7\n\t"					\
	"rd	%%psr, %%g4\n\t"							\
	"std	%%sp, [%%g6 + %4]\n\t"							\
	"rd	%%wim, %%g5\n\t"							\
	"wr	%%g4, 0x20, %%psr\n\t"							\
	"nop\n\t"									\
	"std	%%g4, [%%g6 + %3]\n\t"							\
	"ldd	[%2 + %3], %%g4\n\t"							\
	"mov	%2, %%g6\n\t"								\
	".globl	patchme_store_new_current\n"						\
"patchme_store_new_current:\n\t"							\
	"st	%2, [%1]\n\t"								\
	"wr	%%g4, 0x20, %%psr\n\t"							\
	"nop\n\t"									\
	"nop\n\t"									\
	"nop\n\t"	/* LEON needs this: load to %sp depends on CWP. */		\
	"ldd	[%%g6 + %4], %%sp\n\t"							\
	"wr	%%g5, 0x0, %%wim\n\t"							\
	"ldd	[%%sp + 0x00], %%l0\n\t"						\
	"ldd	[%%sp + 0x38], %%i6\n\t"						\
	"wr	%%g4, 0x0, %%psr\n\t"							\
	"nop\n\t"									\
	"nop\n\t"									\
	"jmpl	%%o7 + 0x8, %%g0\n\t"							\
	" mov	%%g3, %0\n"								\
	"here:\n"									\
        : "=&r" (last)									\
        : "r" (&(current_set[hard_smp_processor_id()])), "r" (next),			\
	  "i" ((const unsigned long)(&((struct task_struct *)0)->thread.kpsr)),		\
	  "i" ((const unsigned long)(&((struct task_struct *)0)->thread.ksp))		\
	: "g1", "g2", "g3", "g4", "g5", "g7", "l0", "l1", "l3",				\
	"l4", "l5", "l6", "l7", "i0", "i1", "i2", "i3", "i4", "i5", "o0", "o1", "o2",	\
	"o3", "o7");									\
	} while(0)

/*
 * Changing the IRQ level on the Sparc.
 */
extern __inline__ void setipl(unsigned long __orig_psr)
{
	__asm__ __volatile__(
		"wr	%0, 0x0, %%psr\n\t"
		"nop; nop; nop\n"
		: /* no outputs */
		: "r" (__orig_psr)
		: "memory", "cc");
}

extern __inline__ void __cli(void)
{
	unsigned long tmp;

	__asm__ __volatile__(
		"rd	%%psr, %0\n\t"
		"nop; nop; nop;\n\t"	/* Sun4m + Cypress + SMP bug */
		"or	%0, %1, %0\n\t"
		"wr	%0, 0x0, %%psr\n\t"
		"nop; nop; nop\n"
		: "=r" (tmp)
		: "i" (PSR_PIL)
		: "memory");
}

extern __inline__ void __sti(void)
{
	unsigned long tmp;

	__asm__ __volatile__(
		"rd	%%psr, %0\n\t"
		"nop; nop; nop;\n\t"	/* Sun4m + Cypress + SMP bug */
		"andn	%0, %1, %0\n\t"
		"wr	%0, 0x0, %%psr\n\t"
		"nop; nop; nop\n"
		: "=r" (tmp)
		: "i" (PSR_PIL)
		: "memory");
}

extern __inline__ unsigned long getipl(void)
{
	unsigned long retval;

	__asm__ __volatile__("rd	%%psr, %0" : "=r" (retval));
	return retval;
}

extern __inline__ unsigned long swap_pil(unsigned long __new_psr)
{
	unsigned long retval;

	__asm__ __volatile__(
		"rd	%%psr, %0\n\t"
		"nop; nop; nop;\n\t"	/* Sun4m + Cypress + SMP bug */
		"and	%0, %2, %%g1\n\t"
		"and	%1, %2, %%g2\n\t"
		"xorcc	%%g1, %%g2, %%g0\n\t"
		"be	1f\n\t"
		" nop\n\t"
		"wr	%0, %2, %%psr\n\t"
		"nop; nop; nop;\n"
		"1:\n"
		: "=&r" (retval)
		: "r" (__new_psr), "i" (PSR_PIL)
		: "g1", "g2", "memory", "cc");

	return retval;
}

extern __inline__ unsigned long read_psr_and_cli(void)
{
	unsigned long retval;

	__asm__ __volatile__(
		"rd	%%psr, %0\n\t"
		"nop; nop; nop;\n\t"	/* Sun4m + Cypress + SMP bug */
		"or	%0, %1, %%g1\n\t"
		"wr	%%g1, 0x0, %%psr\n\t"
		"nop; nop; nop\n\t"
		: "=r" (retval)
		: "i" (PSR_PIL)
		: "g1", "memory");

	return retval;
}

extern __inline__ unsigned long read_psr_and_sti(void)
{
	unsigned long retval;

	__asm__ __volatile__(
		"rd	%%psr, %0\n\t"
		"nop; nop; nop;\n\t"	/* Sun4m + Cypress + SMP bug */
		"andn	%0, %1, %%g1\n\t"
		"wr	%%g1, 0x0, %%psr\n\t"
		"nop; nop; nop\n\t"
		: "=r" (retval)
		: "i" (PSR_PIL)
		: "g1", "memory");

	return retval;
}

#define __save_flags(flags)	((flags) = getipl())
#define __save_and_cli(flags)	((flags) = read_psr_and_cli())
#define __save_and_sti(flags)	((flags) = read_psr_and_sti())
#define __restore_flags(flags)	setipl((flags))
#define local_irq_disable()		__cli()
#define local_irq_enable()		__sti()
#define local_irq_save(flags)		__save_and_cli(flags)
#define local_irq_set(flags)		__save_and_sti(flags)
#define local_irq_restore(flags)	__restore_flags(flags)

#ifdef CONFIG_SMP

extern unsigned char global_irq_holder;

#define save_and_cli(flags)   do { save_flags(flags); cli(); } while(0)

extern void __global_cli(void);
extern void __global_sti(void);
extern unsigned long __global_save_flags(void);
extern void __global_restore_flags(unsigned long flags);
#define cli()			__global_cli()
#define sti()			__global_sti()
#define save_flags(flags)	((flags)=__global_save_flags())
#define restore_flags(flags)	__global_restore_flags(flags)

#else

#define cli() __cli()
#define sti() __sti()
#define save_flags(x) __save_flags(x)
#define restore_flags(x) __restore_flags(x)
#define save_and_cli(x) __save_and_cli(x)

#endif

/* XXX Change this if we ever use a PSO mode kernel. */
#define mb()	__asm__ __volatile__ ("" : : : "memory")
#define rmb()	mb()
#define wmb()	mb()
#define set_mb(__var, __value)  do { __var = __value; mb(); } while(0)
#define set_wmb(__var, __value) set_mb(__var, __value)
#define smp_mb()	__asm__ __volatile__("":::"memory");
#define smp_rmb()	__asm__ __volatile__("":::"memory");
#define smp_wmb()	__asm__ __volatile__("":::"memory");

#define nop() __asm__ __volatile__ ("nop");

/* This has special calling conventions */
#ifndef CONFIG_SMP
BTFIXUPDEF_CALL(void, ___xchg32, void)
#endif

extern __inline__ unsigned long xchg_u32(__volatile__ unsigned long *m, unsigned long val)
{
#ifdef CONFIG_SMP
	__asm__ __volatile__("swap [%2], %0"
			     : "=&r" (val)
			     : "0" (val), "r" (m)
			     : "memory");
	return val;
#else
	register unsigned long *ptr asm("g1");
	register unsigned long ret asm("g2");

	ptr = (unsigned long *) m;
	ret = val;

	/* Note: this is magic and the nop there is
	   really needed. */
	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___f____xchg32\n\t"
	" nop\n\t"
	: "=&r" (ret)
	: "0" (ret), "r" (ptr)
	: "g3", "g4", "g7", "memory", "cc");

	return ret;
#endif
}

#define xchg(ptr,x) ((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))
#define tas(ptr) (xchg((ptr),1))

extern void __xchg_called_with_bad_pointer(void);

static __inline__ unsigned long __xchg(unsigned long x, __volatile__ void * ptr, int size)
{
	switch (size) {
	case 4:
		return xchg_u32(ptr, x);
	};
	__xchg_called_with_bad_pointer();
	return x;
}

extern void die_if_kernel(char *str, struct pt_regs *regs) __attribute__ ((noreturn));

#endif /* __KERNEL__ */

#endif /* __ASSEMBLY__ */

#endif /* !(__SPARC_SYSTEM_H) */
