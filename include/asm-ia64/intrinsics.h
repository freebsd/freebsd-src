#ifndef _ASM_IA64_INTRINSICS_H
#define _ASM_IA64_INTRINSICS_H

/*
 * Compiler-dependent intrinsics.
 *
 * Copyright (C) 2002-2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */

#include <linux/config.h>

/*
 * Force an unresolved reference if someone tries to use
 * ia64_fetch_and_add() with a bad value.
 */
extern unsigned long __bad_size_for_ia64_fetch_and_add (void);
extern unsigned long __bad_increment_for_ia64_fetch_and_add (void);

#define IA64_FETCHADD(tmp,v,n,sz)						\
({										\
	switch (sz) {								\
	      case 4:								\
		__asm__ __volatile__ ("fetchadd4.rel %0=[%1],%2"		\
				      : "=r"(tmp) : "r"(v), "i"(n) : "memory");	\
		break;								\
										\
	      case 8:								\
		__asm__ __volatile__ ("fetchadd8.rel %0=[%1],%2"		\
				      : "=r"(tmp) : "r"(v), "i"(n) : "memory");	\
		break;								\
										\
	      default:								\
		__bad_size_for_ia64_fetch_and_add();				\
	}									\
})

#define ia64_fetch_and_add(i,v)								\
({											\
	__u64 _tmp;									\
	volatile __typeof__(*(v)) *_v = (v);						\
	/* Can't use a switch () here: gcc isn't always smart enough for that... */	\
	if ((i) == -16)									\
		IA64_FETCHADD(_tmp, _v, -16, sizeof(*(v)));				\
	else if ((i) == -8)								\
		IA64_FETCHADD(_tmp, _v, -8, sizeof(*(v)));				\
	else if ((i) == -4)								\
		IA64_FETCHADD(_tmp, _v, -4, sizeof(*(v)));				\
	else if ((i) == -2)								\
		IA64_FETCHADD(_tmp, _v, -2, sizeof(*(v)));				\
	else if ((i) == -1)								\
		IA64_FETCHADD(_tmp, _v, -1, sizeof(*(v)));				\
	else if ((i) == 1)								\
		IA64_FETCHADD(_tmp, _v, 1, sizeof(*(v)));				\
	else if ((i) == 2)								\
		IA64_FETCHADD(_tmp, _v, 2, sizeof(*(v)));				\
	else if ((i) == 4)								\
		IA64_FETCHADD(_tmp, _v, 4, sizeof(*(v)));				\
	else if ((i) == 8)								\
		IA64_FETCHADD(_tmp, _v, 8, sizeof(*(v)));				\
	else if ((i) == 16)								\
		IA64_FETCHADD(_tmp, _v, 16, sizeof(*(v)));				\
	else										\
		_tmp = __bad_increment_for_ia64_fetch_and_add();			\
	(__typeof__(*(v))) (_tmp + (i));	/* return new value */			\
})

/*
 * This function doesn't exist, so you'll get a linker error if
 * something tries to do an invalid xchg().
 */
extern void __xchg_called_with_bad_pointer (void);

static __inline__ unsigned long
__xchg (unsigned long x, volatile void *ptr, int size)
{
	unsigned long result;

	switch (size) {
	      case 1:
		__asm__ __volatile ("xchg1 %0=[%1],%2" : "=r" (result)
				    : "r" (ptr), "r" (x) : "memory");
		return result;

	      case 2:
		__asm__ __volatile ("xchg2 %0=[%1],%2" : "=r" (result)
				    : "r" (ptr), "r" (x) : "memory");
		return result;

	      case 4:
		__asm__ __volatile ("xchg4 %0=[%1],%2" : "=r" (result)
				    : "r" (ptr), "r" (x) : "memory");
		return result;

	      case 8:
		__asm__ __volatile ("xchg8 %0=[%1],%2" : "=r" (result)
				    : "r" (ptr), "r" (x) : "memory");
		return result;
	}
	__xchg_called_with_bad_pointer();
	return x;
}

#define xchg(ptr,x)							     \
  ((__typeof__(*(ptr))) __xchg ((unsigned long) (x), (ptr), sizeof(*(ptr))))

/*
 * Atomic compare and exchange.  Compare OLD with MEM, if identical,
 * store NEW in MEM.  Return the initial value in MEM.  Success is
 * indicated by comparing RETURN with OLD.
 */

#define __HAVE_ARCH_CMPXCHG 1

/*
 * This function doesn't exist, so you'll get a linker error
 * if something tries to do an invalid cmpxchg().
 */
extern long __cmpxchg_called_with_bad_pointer(void);

#define ia64_cmpxchg(sem,ptr,old,new,size)						\
({											\
	__typeof__(ptr) _p_ = (ptr);							\
	__typeof__(new) _n_ = (new);							\
	__u64 _o_, _r_;									\
											\
	switch (size) {									\
	      case 1: _o_ = (__u8 ) (long) (old); break;				\
	      case 2: _o_ = (__u16) (long) (old); break;				\
	      case 4: _o_ = (__u32) (long) (old); break;				\
	      case 8: _o_ = (__u64) (long) (old); break;				\
	      default: break;								\
	}										\
	 __asm__ __volatile__ ("mov ar.ccv=%0;;" :: "rO"(_o_));				\
	switch (size) {									\
	      case 1:									\
		__asm__ __volatile__ ("cmpxchg1."sem" %0=[%1],%2,ar.ccv"		\
				      : "=r"(_r_) : "r"(_p_), "r"(_n_) : "memory");	\
		break;									\
											\
	      case 2:									\
		__asm__ __volatile__ ("cmpxchg2."sem" %0=[%1],%2,ar.ccv"		\
				      : "=r"(_r_) : "r"(_p_), "r"(_n_) : "memory");	\
		break;									\
											\
	      case 4:									\
		__asm__ __volatile__ ("cmpxchg4."sem" %0=[%1],%2,ar.ccv"		\
				      : "=r"(_r_) : "r"(_p_), "r"(_n_) : "memory");	\
		break;									\
											\
	      case 8:									\
		__asm__ __volatile__ ("cmpxchg8."sem" %0=[%1],%2,ar.ccv"		\
				      : "=r"(_r_) : "r"(_p_), "r"(_n_) : "memory");	\
		break;									\
											\
	      default:									\
		_r_ = __cmpxchg_called_with_bad_pointer();				\
		break;									\
	}										\
	(__typeof__(old)) _r_;								\
})

#define cmpxchg_acq(ptr,o,n)	ia64_cmpxchg("acq", (ptr), (o), (n), sizeof(*(ptr)))
#define cmpxchg_rel(ptr,o,n)	ia64_cmpxchg("rel", (ptr), (o), (n), sizeof(*(ptr)))

/* for compatibility with other platforms: */
#define cmpxchg(ptr,o,n)	cmpxchg_acq(ptr,o,n)

#ifdef CONFIG_IA64_DEBUG_CMPXCHG
# define CMPXCHG_BUGCHECK_DECL	int _cmpxchg_bugcheck_count = 128;
# define CMPXCHG_BUGCHECK(v)							\
  do {										\
	if (_cmpxchg_bugcheck_count-- <= 0) {					\
		void *ip;							\
		extern int printk(const char *fmt, ...);			\
		asm ("mov %0=ip" : "=r"(ip));					\
		printk("CMPXCHG_BUGCHECK: stuck at %p on word %p\n", ip, (v));	\
		break;								\
	}									\
  } while (0)
#else /* !CONFIG_IA64_DEBUG_CMPXCHG */
# define CMPXCHG_BUGCHECK_DECL
# define CMPXCHG_BUGCHECK(v)
#endif /* !CONFIG_IA64_DEBUG_CMPXCHG */

#endif /* _ASM_IA64_INTRINSICS_H */
