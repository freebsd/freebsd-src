#ifndef _M68K_SYSTEM_H
#define _M68K_SYSTEM_H

#include <linux/config.h> /* get configuration macros */
#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/segment.h>
#include <asm/entry.h>

#define prepare_to_switch()	do { } while(0)

#ifdef __KERNEL__

/*
 * switch_to(n) should switch tasks to task ptr, first checking that
 * ptr isn't the current task, in which case it does nothing.  This
 * also clears the TS-flag if the task we switched to has used the
 * math co-processor latest.
 */
/*
 * switch_to() saves the extra registers, that are not saved
 * automatically by SAVE_SWITCH_STACK in resume(), ie. d0-d5 and
 * a0-a1. Some of these are used by schedule() and its predecessors
 * and so we might get see unexpected behaviors when a task returns
 * with unexpected register values.
 *
 * syscall stores these registers itself and none of them are used
 * by syscall after the function in the syscall has been called.
 *
 * Beware that resume now expects *next to be in d1 and the offset of
 * tss to be in a1. This saves a few instructions as we no longer have
 * to push them onto the stack and read them back right after.
 *
 * 02/17/96 - Jes Sorensen (jds@kom.auc.dk)
 *
 * Changed 96/09/19 by Andreas Schwab
 * pass prev in a0, next in a1, offset of tss in d1, and whether
 * the mm structures are shared in d2 (to avoid atc flushing).
 */
asmlinkage void resume(void);
#define switch_to(prev,next,last) { \
  register void *_prev __asm__ ("a0") = (prev); \
  register void *_next __asm__ ("a1") = (next); \
  register void *_last __asm__ ("d1"); \
  __asm__ __volatile__("jbsr " SYMBOL_NAME_STR(resume) \
		       : "=a" (_prev), "=a" (_next), "=d" (_last) \
		       : "0" (_prev), "1" (_next)		  \
		       : "d0", "d2", "d3", "d4", "d5"); \
  (last) = _last; \
}

/* interrupt control.. */
#if 0
#define __sti() asm volatile ("andiw %0,%%sr": : "i" (ALLOWINT) : "memory")
#else
#include <asm/hardirq.h>
#define __sti() ({							      \
	if (MACH_IS_Q40 || !local_irq_count(smp_processor_id()))              \
		asm volatile ("andiw %0,%%sr": : "i" (ALLOWINT) : "memory");  \
})
#endif
#define __cli() asm volatile ("oriw  #0x0700,%%sr": : : "memory")
#define __save_flags(x) asm volatile ("movew %%sr,%0":"=d" (x) : : "memory")
#define __restore_flags(x) asm volatile ("movew %0,%%sr": :"d" (x) : "memory")

/* For spinlocks etc */
#define local_irq_save(x)	({ __save_flags(x); __cli(); })
#define local_irq_set(x)	({ __save_flags(x); __sti(); })
#define local_irq_restore(x)	__restore_flags(x)
#define local_irq_disable()	__cli()
#define local_irq_enable()	__sti()

#define cli()			__cli()
#define sti()			__sti()
#define save_flags(x)		__save_flags(x)
#define restore_flags(x)	__restore_flags(x)
#define save_and_cli(x)		do { save_flags(x); cli(); } while(0)
#define save_and_set(x)		do { save_flags(x); sti(); } while(0)

/*
 * Force strict CPU ordering.
 * Not really required on m68k...
 */
#define nop()		do { asm volatile ("nop"); barrier(); } while (0)
#define mb()		barrier()
#define rmb()		barrier()
#define wmb()		barrier()
#define set_mb(var, value)     do { var = value; mb(); } while (0)
#define set_wmb(var, value)    do { var = value; wmb(); } while (0)

#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()


#define xchg(ptr,x) ((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))
#define tas(ptr) (xchg((ptr),1))

struct __xchg_dummy { unsigned long a[100]; };
#define __xg(x) ((volatile struct __xchg_dummy *)(x))

#ifndef CONFIG_RMW_INSNS
static inline unsigned long __xchg(unsigned long x, volatile void * ptr, int size)
{
  unsigned long tmp, flags;

  save_flags(flags);
  cli();

  switch (size) {
  case 1:
    __asm__ __volatile__
    ("moveb %2,%0\n\t"
     "moveb %1,%2"
    : "=&d" (tmp) : "d" (x), "m" (*__xg(ptr)) : "memory");
    break;
  case 2:
    __asm__ __volatile__
    ("movew %2,%0\n\t"
     "movew %1,%2"
    : "=&d" (tmp) : "d" (x), "m" (*__xg(ptr)) : "memory");
    break;
  case 4:
    __asm__ __volatile__
    ("movel %2,%0\n\t"
     "movel %1,%2"
    : "=&d" (tmp) : "d" (x), "m" (*__xg(ptr)) : "memory");
    break;
  }
  restore_flags(flags);
  return tmp;
}
#else
static inline unsigned long __xchg(unsigned long x, volatile void * ptr, int size)
{
	switch (size) {
	    case 1:
		__asm__ __volatile__
			("moveb %2,%0\n\t"
			 "1:\n\t"
			 "casb %0,%1,%2\n\t"
			 "jne 1b"
			 : "=&d" (x) : "d" (x), "m" (*__xg(ptr)) : "memory");
		break;
	    case 2:
		__asm__ __volatile__
			("movew %2,%0\n\t"
			 "1:\n\t"
			 "casw %0,%1,%2\n\t"
			 "jne 1b"
			 : "=&d" (x) : "d" (x), "m" (*__xg(ptr)) : "memory");
		break;
	    case 4:
		__asm__ __volatile__
			("movel %2,%0\n\t"
			 "1:\n\t"
			 "casl %0,%1,%2\n\t"
			 "jne 1b"
			 : "=&d" (x) : "d" (x), "m" (*__xg(ptr)) : "memory");
		break;
	}
	return x;
}
#endif

#endif /* __KERNEL__ */

#endif /* _M68K_SYSTEM_H */
