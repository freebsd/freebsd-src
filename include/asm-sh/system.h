#ifndef __ASM_SH_SYSTEM_H
#define __ASM_SH_SYSTEM_H

/*
 * Copyright (C) 1999, 2000  Niibe Yutaka  &  Kaz Kojima
 */

#include <linux/config.h>
#include <linux/kernel.h>

/*
 *	switch_to() should switch tasks to task nr n, first
 */

typedef struct {
	unsigned long seg;
} mm_segment_t;

#ifdef CONFIG_SMP
#error no SMP SuperH
#else
#define prepare_to_switch()	do { } while(0)
#define switch_to(prev,next,last) do { \
 register struct task_struct *__last; \
 register unsigned long *__ts1 __asm__ ("r1") = &prev->thread.sp; \
 register unsigned long *__ts2 __asm__ ("r2") = &prev->thread.pc; \
 register unsigned long *__ts4 __asm__ ("r4") = (unsigned long *)prev; \
 register unsigned long *__ts5 __asm__ ("r5") = (unsigned long *)next; \
 register unsigned long *__ts6 __asm__ ("r6") = &next->thread.sp; \
 register unsigned long __ts7 __asm__ ("r7") = next->thread.pc; \
 __asm__ __volatile__ (".balign 4\n\t" \
		       "stc.l	gbr, @-r15\n\t" \
		       "sts.l	pr, @-r15\n\t" \
		       "mov.l	r8, @-r15\n\t" \
		       "mov.l	r9, @-r15\n\t" \
		       "mov.l	r10, @-r15\n\t" \
		       "mov.l	r11, @-r15\n\t" \
		       "mov.l	r12, @-r15\n\t" \
		       "mov.l	r13, @-r15\n\t" \
		       "mov.l	r14, @-r15\n\t" \
		       "mov.l	r15, @r1	! save SP\n\t" \
		       "mov.l	@r6, r15	! change to new stack\n\t" \
		       "mov.l	%0, @-r15	! push R0 onto new stack\n\t" \
		       "mova	1f, %0\n\t" \
		       "mov.l	%0, @r2	! save PC\n\t" \
		       "mov.l	2f, %0\n\t" \
		       "jmp	@%0		! call __switch_to\n\t" \
		       " lds	r7, pr	!  with return to new PC\n\t" \
		       ".balign	4\n"	\
		       "2:\n\t" \
		       ".long	" "__switch_to\n" \
		       "1:\n\t" \
		       "mov.l	@r15+, %0	! pop R0 from new stack\n\t" \
		       "mov.l	@r15+, r14\n\t" \
		       "mov.l	@r15+, r13\n\t" \
		       "mov.l	@r15+, r12\n\t" \
		       "mov.l	@r15+, r11\n\t" \
		       "mov.l	@r15+, r10\n\t" \
		       "mov.l	@r15+, r9\n\t" \
		       "mov.l	@r15+, r8\n\t" \
		       "lds.l	@r15+, pr\n\t" \
		       "ldc.l	@r15+, gbr\n\t" \
		       :"=&z" (__last) \
		       :"0" (prev), \
			"r" (__ts1), "r" (__ts2), \
			"r" (__ts4), "r" (__ts5), "r" (__ts6), "r" (__ts7) \
		       :"r3", "t"); \
  last = __last; \
} while (0)
#endif

#define nop() __asm__ __volatile__ ("nop")


#define xchg(ptr,x) ((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))

static __inline__ unsigned long tas(volatile int *m)
{ /* #define tas(ptr) (xchg((ptr),1)) */
	unsigned long retval;

	__asm__ __volatile__ ("tas.b	@%1\n\t"
			      "movt	%0"
			      : "=r" (retval): "r" (m): "t", "memory");
	return retval;
}

extern void __xchg_called_with_bad_pointer(void);

#define mb()	__asm__ __volatile__ ("": : :"memory")
#define rmb()	mb()
#define wmb()	__asm__ __volatile__ ("": : :"memory")

#ifdef CONFIG_SMP
#define smp_mb()	mb()
#define smp_rmb()	rmb()
#define smp_wmb()	wmb()
#else
#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
#endif

#define set_mb(var, value)  do { var = value; mb(); } while (0)
#define set_wmb(var, value) do { var = value; wmb(); } while (0)

/* Interrupt Control */
static __inline__ void __sti(void)
{
	unsigned long __dummy0, __dummy1;

	__asm__ __volatile__("stc	sr, %0\n\t"
			     "and	%1, %0\n\t"
			     "stc	r6_bank, %1\n\t"
			     "or	%1, %0\n\t"
			     "ldc	%0, sr"
			     : "=&r" (__dummy0), "=r" (__dummy1)
			     : "1" (~0x000000f0)
			     : "memory");
}

static __inline__ void __cli(void)
{
	unsigned long __dummy;
	__asm__ __volatile__("stc	sr, %0\n\t"
			     "or	#0xf0, %0\n\t"
			     "ldc	%0, sr"
			     : "=&z" (__dummy)
			     : /* no inputs */
			     : "memory");
}

#define __save_flags(x) \
	__asm__ __volatile__("stc sr, %0\n\t" \
			     "and #0xf0, %0"  \
			     : "=&z" (x) :/**/: "memory" )

static __inline__ unsigned long __save_and_cli(void)
{
	unsigned long flags, __dummy;

	__asm__ __volatile__("stc	sr, %1\n\t"
			     "mov	%1, %0\n\t"
			     "or	#0xf0, %0\n\t"
			     "ldc	%0, sr\n\t"
			     "mov	%1, %0\n\t"
			     "and	#0xf0, %0"
			     : "=&z" (flags), "=&r" (__dummy)
			     :/**/
			     : "memory" );
	return flags;
}

#ifdef DEBUG_CLI_STI
static __inline__ void  __restore_flags(unsigned long x)
{
	if ((x & 0x000000f0) != 0x000000f0)
		__sti();
	else {
		unsigned long flags;
		__save_flags(flags);

		if (flags == 0) {
			extern void dump_stack(void);
			printk(KERN_ERR "BUG!\n");
			dump_stack();
			__cli();
		}
	}
}
#else
#define __restore_flags(x) do { 			\
	if ((x & 0x000000f0) != 0x000000f0)		\
		__sti();				\
} while (0)
#endif

#define really_restore_flags(x) do { 			\
	if ((x & 0x000000f0) != 0x000000f0)		\
		__sti();				\
	else						\
		__cli();				\
} while (0)

/*
 * Jump to P2 area.
 * When handling TLB or caches, we need to do it from P2 area.
 */
#define jump_to_P2()			\
do {					\
	unsigned long __dummy;		\
	__asm__ __volatile__(		\
		"mov.l	1f, %0\n\t"	\
		"or	%1, %0\n\t"	\
		"jmp	@%0\n\t"	\
		" nop\n\t" 		\
		".balign 4\n"		\
		"1:	.long 2f\n"	\
		"2:"			\
		: "=&r" (__dummy)	\
		: "r" (0x20000000));	\
} while (0)

/*
 * Back to P1 area.
 */
#define back_to_P1()					\
do {							\
	unsigned long __dummy;				\
	__asm__ __volatile__(				\
		"nop;nop;nop;nop;nop;nop;nop\n\t"	\
		"mov.l	1f, %0\n\t"			\
		"jmp	@%0\n\t"			\
		" nop\n\t"				\
		".balign 4\n"				\
		"1:	.long 2f\n"			\
		"2:"					\
		: "=&r" (__dummy));			\
} while (0)

#define __save_and_sti(x)       do { __save_flags(x); __sti(); } while(0);

/* For spinlocks etc */
#define local_irq_save(x)	x = __save_and_cli()
#define local_irq_set(x)	__save_and_sti(x)
#define local_irq_restore(x)	__restore_flags(x)
#define local_irq_disable()	__cli()
#define local_irq_enable()	__sti()

#ifdef CONFIG_SMP

extern void __global_cli(void);
extern void __global_sti(void);
extern unsigned long __global_save_flags(void);
extern void __global_restore_flags(unsigned long);
#define cli() __global_cli()
#define sti() __global_sti()
#define save_flags(x) ((x)=__global_save_flags())
#define restore_flags(x) __global_restore_flags(x)
#define save_and_sti(x) do { save_flags(x); sti(); } while(0);
#else

#define cli() __cli()
#define sti() __sti()
#define save_flags(x) __save_flags(x)
#define save_and_cli(x) x = __save_and_cli()
#define save_and_sti(x) __save_and_sti(x)
#define restore_flags(x) __restore_flags(x)

#endif

static __inline__ unsigned long xchg_u32(volatile int * m, unsigned long val)
{
	unsigned long flags, retval;

	save_and_cli(flags);
	retval = *m;
	*m = val;
	restore_flags(flags);
	return retval;
}

static __inline__ unsigned long xchg_u8(volatile unsigned char * m, unsigned long val)
{
	unsigned long flags, retval;

	save_and_cli(flags);
	retval = *m;
	*m = val & 0xff;
	restore_flags(flags);
	return retval;
}

static __inline__ unsigned long __xchg(unsigned long x, volatile void * ptr, int size)
{
	switch (size) {
	case 4:
		return xchg_u32(ptr, x);
		break;
	case 1:
		return xchg_u8(ptr, x);
		break;
	}
	__xchg_called_with_bad_pointer();
	return x;
}

/* XXX
 * disable hlt during certain critical i/o operations
 */
#define HAVE_DISABLE_HLT
void disable_hlt(void);
void enable_hlt(void);

#endif
