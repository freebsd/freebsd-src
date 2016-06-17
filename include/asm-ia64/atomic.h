#ifndef _ASM_IA64_ATOMIC_H
#define _ASM_IA64_ATOMIC_H

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 *
 * NOTE: don't mess with the types below!  The "unsigned long" and
 * "int" types were carefully placed so as to ensure proper operation
 * of the macros.
 *
 * Copyright (C) 1998, 1999, 2002 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */
#include <linux/types.h>

#include <asm/intrinsics.h>

/*
 * On IA-64, counter must always be volatile to ensure that that the
 * memory accesses are ordered.
 */
typedef struct { volatile __s32 counter; } atomic_t;

#define ATOMIC_INIT(i)		((atomic_t) { (i) })

#define atomic_read(v)		((v)->counter)
#define atomic_set(v,i)		(((v)->counter) = (i))

static __inline__ int
ia64_atomic_add (int i, atomic_t *v)
{
	__s32 old, new;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = atomic_read(v);
		new = old + i;
	} while (ia64_cmpxchg("acq", v, old, old + i, sizeof(atomic_t)) != old);
	return new;
}

static __inline__ int
ia64_atomic_sub (int i, atomic_t *v)
{
	__s32 old, new;
	CMPXCHG_BUGCHECK_DECL

	do {
		CMPXCHG_BUGCHECK(v);
		old = atomic_read(v);
		new = old - i;
	} while (ia64_cmpxchg("acq", v, old, new, sizeof(atomic_t)) != old);
	return new;
}

/*
 * Atomically add I to V and return TRUE if the resulting value is
 * negative.
 */
static __inline__ int
atomic_add_negative (int i, atomic_t *v)
{
	return ia64_atomic_add(i, v) < 0;
}

#define atomic_add_return(i,v)						\
	((__builtin_constant_p(i) &&					\
	  (   (i ==  1) || (i ==  4) || (i ==  8) || (i ==  16)		\
	   || (i == -1) || (i == -4) || (i == -8) || (i == -16)))	\
	 ? ia64_fetch_and_add(i, &(v)->counter)				\
	 : ia64_atomic_add(i, v))

#define atomic_sub_return(i,v)						\
	((__builtin_constant_p(i) &&					\
	  (   (i ==  1) || (i ==  4) || (i ==  8) || (i ==  16)		\
	   || (i == -1) || (i == -4) || (i == -8) || (i == -16)))	\
	 ? ia64_fetch_and_add(-(i), &(v)->counter)			\
	 : ia64_atomic_sub(i, v))

#define atomic_dec_return(v)		atomic_sub_return(1, (v))
#define atomic_inc_return(v)		atomic_add_return(1, (v))

#define atomic_sub_and_test(i,v)	(atomic_sub_return((i), (v)) == 0)
#define atomic_dec_and_test(v)		(atomic_sub_return(1, (v)) == 0)
#define atomic_inc_and_test(v)		(atomic_add_return(1, (v)) != 0)

#define atomic_add(i,v)			atomic_add_return((i), (v))
#define atomic_sub(i,v)			atomic_sub_return((i), (v))
#define atomic_inc(v)			atomic_add(1, (v))
#define atomic_dec(v)			atomic_sub(1, (v))

/* Atomic operations are already serializing */
#define smp_mb__before_atomic_dec()	barrier()
#define smp_mb__after_atomic_dec()	barrier()
#define smp_mb__before_atomic_inc()	barrier()
#define smp_mb__after_atomic_inc()	barrier()

#endif /* _ASM_IA64_ATOMIC_H */
