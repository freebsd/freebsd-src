/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 - 1999 by Ralf Baechle
 * Copyright (C) 1996 by Paul M. Antoine
 * Copyright (C) 1994 - 1999 by Ralf Baechle
 * Kevin D. Kissell, kevink@mips.org and Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.
 */
#ifndef _ASM_SYSTEM_H
#define _ASM_SYSTEM_H

#include <linux/config.h>
#include <asm/sgidefs.h>

#include <linux/kernel.h>

#include <asm/addrspace.h>
#include <asm/ptrace.h>

__asm__ (
	".macro\t__sti\n\t"
	".set\tpush\n\t"
	".set\treorder\n\t"
	".set\tnoat\n\t"
	"mfc0\t$1,$12\n\t"
	"ori\t$1,0x1f\n\t"
	"xori\t$1,0x1e\n\t"
	"mtc0\t$1,$12\n\t"
	".set\tpop\n\t"
	".endm");

static __inline__ void
__sti(void)
{
	__asm__ __volatile__(
		"__sti"
		: /* no outputs */
		: /* no inputs */
		: "memory");
}

/*
 * For cli() we have to insert nops to make sure that the new value
 * has actually arrived in the status register before the end of this
 * macro.
 * R4000/R4400 need three nops, the R4600 two nops and the R10000 needs
 * no nops at all.
 */
__asm__ (
	".macro\t__cli\n\t"
	".set\tpush\n\t"
	".set\tnoat\n\t"
	"mfc0\t$1,$12\n\t"
	"ori\t$1,1\n\t"
	"xori\t$1,1\n\t"
	".set\tnoreorder\n\t"
	"mtc0\t$1,$12\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	".set\tpop\n\t"
	".endm");

static __inline__ void
__cli(void)
{
	__asm__ __volatile__(
		"__cli"
		: /* no outputs */
		: /* no inputs */
		: "memory");
}

__asm__ (
	".macro\t__save_flags flags\n\t"
	".set\tpush\n\t"
	".set\treorder\n\t"
	"mfc0\t\\flags, $12\n\t"
	".set\tpop\n\t"
	".endm");

#define __save_flags(x)							\
__asm__ __volatile__(							\
	"__save_flags %0"						\
	: "=r" (x))

__asm__ (
	".macro\t__save_and_cli result\n\t"
	".set\tpush\n\t"
	".set\treorder\n\t"
	".set\tnoat\n\t"
	"mfc0\t\\result, $12\n\t"
	"ori\t$1, \\result, 1\n\t"
	"xori\t$1, 1\n\t"
	".set\tnoreorder\n\t"
	"mtc0\t$1, $12\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	".set\tpop\n\t"
	".endm");

#define __save_and_cli(x)						\
__asm__ __volatile__(							\
	"__save_and_cli\t%0"						\
	: "=r" (x)							\
	: /* no inputs */						\
	: "memory")

__asm__ (
	".macro\t__save_and_sti result\n\t"
	".set\tpush\n\t"
	".set\treorder\n\t"
	".set\tnoat\n\t"
	"mfc0\t\\result, $12\n\t"
	"ori\t$1, \\result, 1\n\t"
	".set\tnoreorder\n\t"
	"mtc0\t$1, $12\n\t"
	".set\tpop\n\t"
	".endm");

#define __save_and_sti(x)						\
__asm__ __volatile__(							\
	"__save_and_sti\t%0"						\
	: "=r" (x)							\
	: /* no inputs */						\
	: "memory")

__asm__(".macro\t__restore_flags flags\n\t"
	".set\tnoreorder\n\t"
	".set\tnoat\n\t"
	"mfc0\t$1, $12\n\t"
	"andi\t\\flags, 1\n\t"
	"ori\t$1, 1\n\t"
	"xori\t$1, 1\n\t"
	"or\t\\flags, $1\n\t"
	"mtc0\t\\flags, $12\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	"sll\t$0, $0, 1\t\t\t# nop\n\t"
	".set\tat\n\t"
	".set\treorder\n\t"
	".endm");

#define __restore_flags(flags)						\
do {									\
	unsigned long __tmp1;						\
									\
	__asm__ __volatile__(						\
		"__restore_flags\t%0"					\
		: "=r" (__tmp1)						\
		: "0" (flags)						\
		: "memory");						\
} while(0)

#ifdef CONFIG_SMP

extern void __global_sti(void);
extern void __global_cli(void);
extern unsigned long __global_save_flags(void);
extern void __global_restore_flags(unsigned long);
#  define sti() __global_sti()
#  define cli() __global_cli()
#  define save_flags(x) do { x = __global_save_flags(); } while (0)
#  define restore_flags(x) __global_restore_flags(x)
#  define save_and_cli(x) do { save_flags(x); cli(); } while(0)
#  define save_and_sti(x) do { save_flags(x); sti(); } while(0)

#else /* Single processor */

#  define sti() __sti()
#  define cli() __cli()
#  define save_flags(x) __save_flags(x)
#  define save_and_cli(x) __save_and_cli(x)
#  define restore_flags(x) __restore_flags(x)
#  define save_and_sti(x) __save_and_sti(x)

#endif /* SMP */

/* For spinlocks etc */
#define local_irq_save(x)	__save_and_cli(x)
#define local_irq_set(x)	__save_and_sti(x)
#define local_irq_restore(x)	__restore_flags(x)
#define local_irq_disable()	__cli()
#define local_irq_enable()	__sti()

#ifdef CONFIG_CPU_HAS_SYNC
#define __sync()				\
	__asm__ __volatile__(			\
		".set	push\n\t"		\
		".set	noreorder\n\t"		\
		".set	mips2\n\t"		\
		"sync\n\t"			\
		".set	pop"			\
		: /* no output */		\
		: /* no input */		\
		: "memory")
#else
#define __sync()	do { } while(0)
#endif

#define __fast_iob()				\
	__asm__ __volatile__(			\
		".set	push\n\t"		\
		".set	noreorder\n\t"		\
		"lw	$0,%0\n\t"		\
		"nop\n\t"			\
		".set	pop"			\
		: /* no output */		\
		: "m" (*(int *)KSEG1)		\
		: "memory")

#define fast_wmb()	__sync()
#define fast_rmb()	__sync()
#define fast_mb()	__sync()
#define fast_iob()				\
	do {					\
		__sync();			\
		__fast_iob();			\
	} while (0)

#ifdef CONFIG_CPU_HAS_WB

#include <asm/wbflush.h>

#define wmb()		fast_wmb()
#define rmb()		fast_rmb()
#define mb()		wbflush()
#define iob()		wbflush()

#else /* !CONFIG_CPU_HAS_WB */

#define wmb()		fast_wmb()
#define rmb()		fast_rmb()
#define mb()		fast_mb()
#define iob()		fast_iob()

#endif /* !CONFIG_CPU_HAS_WB */

#ifdef CONFIG_SMP
#define smp_mb()	mb()
#define smp_rmb()	rmb()
#define smp_wmb()	wmb()
#else
#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
#endif

#define set_mb(var, value) \
do { var = value; mb(); } while (0)

#define set_wmb(var, value) \
do { var = value; wmb(); } while (0)

/*
 * switch_to(n) should switch tasks to task nr n, first
 * checking that n isn't the current task, in which case it does nothing.
 */
extern asmlinkage void *resume(void *last, void *next);

#define prepare_to_switch()	do { } while(0)

struct task_struct;

#define switch_to(prev,next,last) \
do { \
	(last) = resume(prev, next); \
} while(0)

/*
 * For 32 and 64 bit operands we can take advantage of ll and sc.
 * FIXME: This doesn't work for R3000 machines.
 */
static __inline__ unsigned long xchg_u32(volatile int * m, unsigned long val)
{
#ifdef CONFIG_CPU_HAS_LLSC
	unsigned long dummy;

	__asm__ __volatile__(
		".set\tpush\t\t\t\t# xchg_u32\n\t"
		".set\tnoreorder\n\t"
		".set\tnomacro\n\t"
		"ll\t%0, %3\n"
		"1:\tmove\t%2, %z4\n\t"
		"sc\t%2, %1\n\t"
		"beqzl\t%2, 1b\n\t"
		" ll\t%0, %3\n\t"
		"sync\n\t"
		".set\tpop"
		: "=&r" (val), "=m" (*m), "=&r" (dummy)
		: "R" (*m), "Jr" (val)
		: "memory");

	return val;
#else
	unsigned long flags, retval;

	local_irq_save(flags);
	retval = *m;
	*m = val;
	local_irq_restore(flags);	/* implies memory barrier  */
	return retval;
#endif /* Processor-dependent optimization */
}

#define xchg(ptr,x) ((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))
#define tas(ptr) (xchg((ptr),1))

static __inline__ unsigned long
__xchg(unsigned long x, volatile void * ptr, int size)
{
	switch (size) {
		case 4:
			return xchg_u32(ptr, x);
	}
	return x;
}

extern void *set_except_vector(int n, void *addr);
extern void per_cpu_trap_init(void);

extern void __die(const char *, struct pt_regs *, const char *file,
	const char *func, unsigned long line) __attribute__((noreturn));
extern void __die_if_kernel(const char *, struct pt_regs *, const char *file,
	const char *func, unsigned long line);

#define die(msg, regs)							\
	__die(msg, regs, __FILE__ ":", __FUNCTION__, __LINE__)
#define die_if_kernel(msg, regs)					\
	__die_if_kernel(msg, regs, __FILE__ ":", __FUNCTION__, __LINE__)

#endif /* _ASM_SYSTEM_H */
