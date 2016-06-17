/*
 * Copyright (C) 1999 Cort Dougan <cort@cs.nmt.edu>
 */
#ifndef __PPC_SYSTEM_H
#define __PPC_SYSTEM_H

#include <linux/config.h>
#include <linux/kdev_t.h>

#include <asm/processor.h>
#include <asm/atomic.h>
#include <asm/hw_irq.h>

/*
 * Memory barrier.
 * The sync instruction guarantees that all memory accesses initiated
 * by this processor have been performed (with respect to all other
 * mechanisms that access memory).  The eieio instruction is a barrier
 * providing an ordering (separately) for (a) cacheable stores and (b)
 * loads and stores to non-cacheable memory (e.g. I/O devices).
 *
 * mb() prevents loads and stores being reordered across this point.
 * rmb() prevents loads being reordered across this point.
 * wmb() prevents stores being reordered across this point.
 *
 * We can use the eieio instruction for wmb, but since it doesn't
 * give any ordering guarantees about loads, we have to use the
 * stronger but slower sync instruction for mb and rmb.
 */
#define mb()  __asm__ __volatile__ ("sync" : : : "memory")
#define rmb()  __asm__ __volatile__ ("sync" : : : "memory")
#define wmb()  __asm__ __volatile__ ("eieio" : : : "memory")

#define set_mb(var, value)	do { var = value; mb(); } while (0)
#define set_wmb(var, value)	do { var = value; wmb(); } while (0)

#ifdef CONFIG_SMP
#define smp_mb()	mb()
#define smp_rmb()	rmb()
#define smp_wmb()	wmb()
#else
#define smp_mb()	__asm__ __volatile__("": : :"memory")
#define smp_rmb()	__asm__ __volatile__("": : :"memory")
#define smp_wmb()	__asm__ __volatile__("": : :"memory")
#endif /* CONFIG_SMP */

#ifdef __KERNEL__
extern void xmon_irq(int, void *, struct pt_regs *);
extern void xmon(struct pt_regs *excp);
extern void print_backtrace(unsigned long *);
extern void show_regs(struct pt_regs * regs);
extern void flush_instruction_cache(void);
extern void hard_reset_now(void);
extern void poweroff_now(void);
#ifdef CONFIG_6xx
extern long _get_L2CR(void);
extern void _set_L2CR(unsigned long);
extern long _get_L3CR(void);
extern void _set_L3CR(unsigned long);
#else
#define _get_L2CR()	0L
#define _set_L2CR(val)	do { } while(0)
#define _get_L3CR()	0L
#define _set_L3CR(val)	do { } while(0)
#endif
extern void via_cuda_init(void);
extern void pmac_nvram_init(void);
extern void read_rtc_time(void);
extern void pmac_find_display(void);
extern void giveup_fpu(struct task_struct *);
extern void enable_kernel_fp(void);
extern void giveup_altivec(struct task_struct *);
extern void load_up_altivec(struct task_struct *);
extern void cvt_fd(float *from, double *to, unsigned long *fpscr);
extern void cvt_df(double *from, float *to, unsigned long *fpscr);
extern int call_rtas(const char *, int, int, unsigned long *, ...);
extern int abs(int);
extern void cacheable_memzero(void *p, unsigned int nb);

struct device_node;
extern void note_scsi_host(struct device_node *, void *);

struct task_struct;
#define prepare_to_switch()	do { } while(0)
#define switch_to(prev,next,last) _switch_to((prev),(next),&(last))
extern void _switch_to(struct task_struct *, struct task_struct *,
		       struct task_struct **);

struct thread_struct;
extern struct task_struct *_switch(struct thread_struct *prev,
				   struct thread_struct *next);

extern unsigned int rtas_data;

struct pt_regs;
extern void dump_regs(struct pt_regs *);

#ifndef CONFIG_SMP

#define cli()	__cli()
#define sti()	__sti()
#define save_flags(flags)	__save_flags(flags)
#define restore_flags(flags)	__restore_flags(flags)
#define save_and_cli(flags)	__save_and_cli(flags)
#define save_and_sti(flags)	__save_and_sti(flags)

#else /* CONFIG_SMP */

extern void __global_cli(void);
extern void __global_sti(void);
extern unsigned long __global_save_flags(void);
extern void __global_restore_flags(unsigned long);
#define cli() __global_cli()
#define sti() __global_sti()
#define save_flags(x) ((x)=__global_save_flags())
#define restore_flags(x) __global_restore_flags(x)

#define save_and_cli(x) do { save_flags(x); cli(); } while(0);
#define save_and_sti(x) do { save_flags(x); sti(); } while(0);

#endif /* !CONFIG_SMP */

#define local_irq_disable()		__cli()
#define local_irq_enable()		__sti()
#define local_irq_save(flags)		__save_and_cli(flags)
#define local_irq_set(flags)		__save_and_sti(flags)
#define local_irq_restore(flags)	__restore_flags(flags)

static __inline__ unsigned long
xchg_u32(volatile void *p, unsigned long val)
{
	unsigned long prev;

	__asm__ __volatile__ ("\n\
1:	lwarx	%0,0,%2 \n"
	PPC405_ERR77(0,%2)
"	stwcx.	%3,0,%2 \n\
	bne-	1b"
	: "=&r" (prev), "=m" (*(volatile unsigned long *)p)
	: "r" (p), "r" (val), "m" (*(volatile unsigned long *)p)
	: "cc", "memory");

	return prev;
}

/*
 * This function doesn't exist, so you'll get a linker error
 * if something tries to do an invalid xchg().
 */
extern void __xchg_called_with_bad_pointer(void);

#define xchg(ptr,x) ((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))
#define tas(ptr) (xchg((ptr),1))

static inline unsigned long __xchg(unsigned long x, void * ptr, int size)
{
	switch (size) {
	case 4:
		return (unsigned long )xchg_u32(ptr, x);
#if 0	/* xchg_u64 doesn't exist on 32-bit PPC */
	case 8:
		return (unsigned long )xchg_u64(ptr, x);
#endif /* 0 */
	}
	__xchg_called_with_bad_pointer();
	return x;


}

extern inline void * xchg_ptr(void * m, void * val)
{
	return (void *) xchg_u32(m, (unsigned long) val);
}


#define __HAVE_ARCH_CMPXCHG	1

static __inline__ unsigned long
__cmpxchg_u32(volatile int *p, int old, int new)
{
	int prev;

	__asm__ __volatile__ ("\n\
1:	lwarx	%0,0,%2 \n\
	cmpw	0,%0,%3 \n\
	bne	2f \n"
	PPC405_ERR77(0,%2)
"	stwcx.	%4,0,%2 \n\
	bne-	1b\n"
#ifdef CONFIG_SMP
"	sync\n"
#endif /* CONFIG_SMP */
"2:"
	: "=&r" (prev), "=m" (*p)
	: "r" (p), "r" (old), "r" (new), "m" (*p)
	: "cc", "memory");

	return prev;
}

/* This function doesn't exist, so you'll get a linker error
   if something tries to do an invalid cmpxchg().  */
extern void __cmpxchg_called_with_bad_pointer(void);

static __inline__ unsigned long
__cmpxchg(volatile void *ptr, unsigned long old, unsigned long new, int size)
{
	switch (size) {
	case 4:
		return __cmpxchg_u32(ptr, old, new);
#if 0	/* we don't have __cmpxchg_u64 on 32-bit PPC */
	case 8:
		return __cmpxchg_u64(ptr, old, new);
#endif /* 0 */
	}
	__cmpxchg_called_with_bad_pointer();
	return old;
}

#define cmpxchg(ptr,o,n)						 \
  ({									 \
     __typeof__(*(ptr)) _o_ = (o);					 \
     __typeof__(*(ptr)) _n_ = (n);					 \
     (__typeof__(*(ptr))) __cmpxchg((ptr), (unsigned long)_o_,		 \
				    (unsigned long)_n_, sizeof(*(ptr))); \
  })

#endif /* __KERNEL__ */
#endif /* __PPC_SYSTEM_H */
