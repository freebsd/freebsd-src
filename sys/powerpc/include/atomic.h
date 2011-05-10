/*-
 * Copyright (c) 2008 Marcel Moolenaar
 * Copyright (c) 2001 Benno Rice
 * Copyright (c) 2001 David E. O'Brien
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_ATOMIC_H_
#define	_MACHINE_ATOMIC_H_

#ifndef _SYS_CDEFS_H_
#error this file needs sys/cdefs.h as a prerequisite
#endif

#define	__ATOMIC_BARRIER					\
    __asm __volatile("sync" : : : "memory")

#define mb()	__ATOMIC_BARRIER
#define	wmb()	mb()
#define	rmb()	mb()

/*
 * atomic_add(p, v)
 * { *p += v; }
 */

#define __atomic_add_int(p, v, t)				\
    __asm __volatile(						\
	"1:	lwarx	%0, 0, %2\n"				\
	"	add	%0, %3, %0\n"				\
	"	stwcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __atomic_add_int */

#ifdef __powerpc64__
#define __atomic_add_long(p, v, t)				\
    __asm __volatile(						\
	"1:	ldarx	%0, 0, %2\n"				\
	"	add	%0, %3, %0\n"				\
	"	stdcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __atomic_add_long */
#else
#define	__atomic_add_long(p, v, t)				\
    __asm __volatile(						\
	"1:	lwarx	%0, 0, %2\n"				\
	"	add	%0, %3, %0\n"				\
	"	stwcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __atomic_add_long */
#endif

#define	_ATOMIC_ADD(type)					\
    static __inline void					\
    atomic_add_##type(volatile u_##type *p, u_##type v) {	\
	u_##type t;						\
	__atomic_add_##type(p, v, t);				\
    }								\
								\
    static __inline void					\
    atomic_add_acq_##type(volatile u_##type *p, u_##type v) {	\
	u_##type t;						\
	__atomic_add_##type(p, v, t);				\
	__ATOMIC_BARRIER;					\
    }								\
								\
    static __inline void					\
    atomic_add_rel_##type(volatile u_##type *p, u_##type v) {	\
	u_##type t;						\
	__ATOMIC_BARRIER;					\
	__atomic_add_##type(p, v, t);				\
    }								\
    /* _ATOMIC_ADD */

_ATOMIC_ADD(int)
_ATOMIC_ADD(long)

#define	atomic_add_32		atomic_add_int
#define	atomic_add_acq_32	atomic_add_acq_int
#define	atomic_add_rel_32	atomic_add_rel_int

#ifdef __powerpc64__
#define	atomic_add_64		atomic_add_long
#define	atomic_add_acq_64	atomic_add_acq_long
#define	atomic_add_rel_64	atomic_add_rel_long

#define	atomic_add_ptr		atomic_add_long
#define	atomic_add_acq_ptr	atomic_add_acq_long
#define	atomic_add_rel_ptr	atomic_add_rel_long
#else
#define	atomic_add_ptr		atomic_add_int
#define	atomic_add_acq_ptr	atomic_add_acq_int
#define	atomic_add_rel_ptr	atomic_add_rel_int
#endif
#undef _ATOMIC_ADD
#undef __atomic_add_long
#undef __atomic_add_int

/*
 * atomic_clear(p, v)
 * { *p &= ~v; }
 */

#define __atomic_clear_int(p, v, t)				\
    __asm __volatile(						\
	"1:	lwarx	%0, 0, %2\n"				\
	"	andc	%0, %0, %3\n"				\
	"	stwcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __atomic_clear_int */

#ifdef __powerpc64__
#define __atomic_clear_long(p, v, t)				\
    __asm __volatile(						\
	"1:	ldarx	%0, 0, %2\n"				\
	"	andc	%0, %0, %3\n"				\
	"	stdcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __atomic_clear_long */
#else
#define	__atomic_clear_long(p, v, t)				\
    __asm __volatile(						\
	"1:	lwarx	%0, 0, %2\n"				\
	"	andc	%0, %0, %3\n"				\
	"	stwcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __atomic_clear_long */
#endif

#define	_ATOMIC_CLEAR(type)					\
    static __inline void					\
    atomic_clear_##type(volatile u_##type *p, u_##type v) {	\
	u_##type t;						\
	__atomic_clear_##type(p, v, t);				\
    }								\
								\
    static __inline void					\
    atomic_clear_acq_##type(volatile u_##type *p, u_##type v) {	\
	u_##type t;						\
	__atomic_clear_##type(p, v, t);				\
	__ATOMIC_BARRIER;					\
    }								\
								\
    static __inline void					\
    atomic_clear_rel_##type(volatile u_##type *p, u_##type v) {	\
	u_##type t;						\
	__ATOMIC_BARRIER;					\
	__atomic_clear_##type(p, v, t);				\
    }								\
    /* _ATOMIC_CLEAR */


_ATOMIC_CLEAR(int)
_ATOMIC_CLEAR(long)

#define	atomic_clear_32		atomic_clear_int
#define	atomic_clear_acq_32	atomic_clear_acq_int
#define	atomic_clear_rel_32	atomic_clear_rel_int

#ifdef __powerpc64__
#define	atomic_clear_64		atomic_clear_long
#define	atomic_clear_acq_64	atomic_clear_acq_long
#define	atomic_clear_rel_64	atomic_clear_rel_long

#define	atomic_clear_ptr	atomic_clear_long
#define	atomic_clear_acq_ptr	atomic_clear_acq_long
#define	atomic_clear_rel_ptr	atomic_clear_rel_long
#else
#define	atomic_clear_ptr	atomic_clear_int
#define	atomic_clear_acq_ptr	atomic_clear_acq_int
#define	atomic_clear_rel_ptr	atomic_clear_rel_int
#endif
#undef _ATOMIC_CLEAR
#undef __atomic_clear_long
#undef __atomic_clear_int

/*
 * atomic_cmpset(p, o, n)
 */
/* TODO -- see below */

/*
 * atomic_load_acq(p)
 */
/* TODO -- see below */

/*
 * atomic_readandclear(p)
 */
/* TODO -- see below */

/*
 * atomic_set(p, v)
 * { *p |= v; }
 */

#define __atomic_set_int(p, v, t)				\
    __asm __volatile(						\
	"1:	lwarx	%0, 0, %2\n"				\
	"	or	%0, %3, %0\n"				\
	"	stwcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __atomic_set_int */

#ifdef __powerpc64__
#define __atomic_set_long(p, v, t)				\
    __asm __volatile(						\
	"1:	ldarx	%0, 0, %2\n"				\
	"	or	%0, %3, %0\n"				\
	"	stdcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __atomic_set_long */
#else
#define	__atomic_set_long(p, v, t)				\
    __asm __volatile(						\
	"1:	lwarx	%0, 0, %2\n"				\
	"	or	%0, %3, %0\n"				\
	"	stwcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __atomic_set_long */
#endif

#define	_ATOMIC_SET(type)					\
    static __inline void					\
    atomic_set_##type(volatile u_##type *p, u_##type v) {	\
	u_##type t;						\
	__atomic_set_##type(p, v, t);				\
    }								\
								\
    static __inline void					\
    atomic_set_acq_##type(volatile u_##type *p, u_##type v) {	\
	u_##type t;						\
	__atomic_set_##type(p, v, t);				\
	__ATOMIC_BARRIER;					\
    }								\
								\
    static __inline void					\
    atomic_set_rel_##type(volatile u_##type *p, u_##type v) {	\
	u_##type t;						\
	__ATOMIC_BARRIER;					\
	__atomic_set_##type(p, v, t);				\
    }								\
    /* _ATOMIC_SET */

_ATOMIC_SET(int)
_ATOMIC_SET(long)

#define	atomic_set_32		atomic_set_int
#define	atomic_set_acq_32	atomic_set_acq_int
#define	atomic_set_rel_32	atomic_set_rel_int

#ifdef __powerpc64__
#define	atomic_set_64		atomic_set_long
#define	atomic_set_acq_64	atomic_set_acq_long
#define	atomic_set_rel_64	atomic_set_rel_long

#define	atomic_set_ptr		atomic_set_long
#define	atomic_set_acq_ptr	atomic_set_acq_long
#define	atomic_set_rel_ptr	atomic_set_rel_long
#else
#define	atomic_set_ptr		atomic_set_int
#define	atomic_set_acq_ptr	atomic_set_acq_int
#define	atomic_set_rel_ptr	atomic_set_rel_int
#endif
#undef _ATOMIC_SET
#undef __atomic_set_long
#undef __atomic_set_int

/*
 * atomic_subtract(p, v)
 * { *p -= v; }
 */

#define __atomic_subtract_int(p, v, t)				\
    __asm __volatile(						\
	"1:	lwarx	%0, 0, %2\n"				\
	"	subf	%0, %3, %0\n"				\
	"	stwcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __atomic_subtract_int */

#ifdef __powerpc64__
#define __atomic_subtract_long(p, v, t)				\
    __asm __volatile(						\
	"1:	ldarx	%0, 0, %2\n"				\
	"	subf	%0, %3, %0\n"				\
	"	stdcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __atomic_subtract_long */
#else
#define	__atomic_subtract_long(p, v, t)				\
    __asm __volatile(						\
	"1:	lwarx	%0, 0, %2\n"				\
	"	subf	%0, %3, %0\n"				\
	"	stwcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __atomic_subtract_long */
#endif

#define	_ATOMIC_SUBTRACT(type)						\
    static __inline void						\
    atomic_subtract_##type(volatile u_##type *p, u_##type v) {		\
	u_##type t;							\
	__atomic_subtract_##type(p, v, t);				\
    }									\
									\
    static __inline void						\
    atomic_subtract_acq_##type(volatile u_##type *p, u_##type v) {	\
	u_##type t;							\
	__atomic_subtract_##type(p, v, t);				\
	__ATOMIC_BARRIER;						\
    }									\
									\
    static __inline void						\
    atomic_subtract_rel_##type(volatile u_##type *p, u_##type v) {	\
	u_##type t;							\
	__ATOMIC_BARRIER;						\
	__atomic_subtract_##type(p, v, t);				\
    }									\
    /* _ATOMIC_SUBTRACT */

_ATOMIC_SUBTRACT(int)
_ATOMIC_SUBTRACT(long)

#define	atomic_subtract_32	atomic_subtract_int
#define	atomic_subtract_acq_32	atomic_subtract_acq_int
#define	atomic_subtract_rel_32	atomic_subtract_rel_int

#ifdef __powerpc64__
#define	atomic_subtract_64	atomic_subtract_long
#define	atomic_subtract_acq_64	atomic_subract_acq_long
#define	atomic_subtract_rel_64	atomic_subtract_rel_long

#define	atomic_subtract_ptr	atomic_subtract_long
#define	atomic_subtract_acq_ptr	atomic_subtract_acq_long
#define	atomic_subtract_rel_ptr	atomic_subtract_rel_long
#else
#define	atomic_subtract_ptr	atomic_subtract_int
#define	atomic_subtract_acq_ptr	atomic_subtract_acq_int
#define	atomic_subtract_rel_ptr	atomic_subtract_rel_int
#endif
#undef _ATOMIC_SUBTRACT
#undef __atomic_subtract_long
#undef __atomic_subtract_int

/*
 * atomic_store_rel(p, v)
 */
/* TODO -- see below */

/*
 * Old/original implementations that still need revisiting.
 */

static __inline u_int
atomic_readandclear_int(volatile u_int *addr)
{
	u_int result,temp;

#ifdef __GNUCLIKE_ASM
	__asm __volatile (
		"\tsync\n"			/* drain writes */
		"1:\tlwarx %0, 0, %3\n\t"	/* load old value */
		"li %1, 0\n\t"			/* load new value */
		"stwcx. %1, 0, %3\n\t"      	/* attempt to store */
		"bne- 1b\n\t"			/* spin if failed */
		: "=&r"(result), "=&r"(temp), "=m" (*addr)
		: "r" (addr), "m" (*addr)
		: "cc", "memory");
#endif

	return (result);
}

#ifdef __powerpc64__
static __inline u_long
atomic_readandclear_long(volatile u_long *addr)
{
	u_long result,temp;

#ifdef __GNUCLIKE_ASM
	__asm __volatile (
		"\tsync\n"			/* drain writes */
		"1:\tldarx %0, 0, %3\n\t"	/* load old value */
		"li %1, 0\n\t"			/* load new value */
		"stdcx. %1, 0, %3\n\t"      	/* attempt to store */
		"bne- 1b\n\t"			/* spin if failed */
		: "=&r"(result), "=&r"(temp), "=m" (*addr)
		: "r" (addr), "m" (*addr)
		: "cc", "memory");
#endif

	return (result);
}
#endif

#define	atomic_readandclear_32		atomic_readandclear_int

#ifdef __powerpc64__
#define	atomic_readandclear_64		atomic_readandclear_long

#define	atomic_readandclear_ptr		atomic_readandclear_long
#else
static __inline u_long
atomic_readandclear_long(volatile u_long *addr)
{

	return ((u_long)atomic_readandclear_int((volatile u_int *)addr));
}

#define	atomic_readandclear_ptr		atomic_readandclear_int
#endif

/*
 * We assume that a = b will do atomic loads and stores.
 */
#define	ATOMIC_STORE_LOAD(TYPE)					\
static __inline u_##TYPE					\
atomic_load_acq_##TYPE(volatile u_##TYPE *p)			\
{								\
	u_##TYPE v;						\
								\
	v = *p;							\
	__ATOMIC_BARRIER;					\
	return (v);						\
}								\
								\
static __inline void						\
atomic_store_rel_##TYPE(volatile u_##TYPE *p, u_##TYPE v)	\
{								\
	__ATOMIC_BARRIER;					\
	*p = v;							\
}

ATOMIC_STORE_LOAD(int)

#define	atomic_load_acq_32	atomic_load_acq_int
#define	atomic_store_rel_32	atomic_store_rel_int

#ifdef __powerpc64__
ATOMIC_STORE_LOAD(long)

#define	atomic_load_acq_64	atomic_load_acq_long
#define	atomic_store_rel_64	atomic_store_rel_long

#define	atomic_load_acq_ptr	atomic_load_acq_long
#define	atomic_store_rel_ptr	atomic_store_rel_long
#else
static __inline u_long
atomic_load_acq_long(volatile u_long *addr)
{

	return ((u_long)atomic_load_acq_int((volatile u_int *)addr));
}

static __inline void
atomic_store_rel_long(volatile u_long *addr, u_long val)
{

	atomic_store_rel_int((volatile u_int *)addr, (u_int)val);
}

#define	atomic_load_acq_ptr	atomic_load_acq_int
#define	atomic_store_rel_ptr	atomic_store_rel_int
#endif
#undef ATOMIC_STORE_LOAD

/*
 * Atomically compare the value stored at *p with cmpval and if the
 * two values are equal, update the value of *p with newval. Returns
 * zero if the compare failed, nonzero otherwise.
 */
static __inline int
atomic_cmpset_int(volatile u_int* p, u_int cmpval, u_int newval)
{
	int	ret;

#ifdef __GNUCLIKE_ASM
	__asm __volatile (
		"1:\tlwarx %0, 0, %2\n\t"	/* load old value */
		"cmplw %3, %0\n\t"		/* compare */
		"bne 2f\n\t"			/* exit if not equal */
		"stwcx. %4, 0, %2\n\t"      	/* attempt to store */
		"bne- 1b\n\t"			/* spin if failed */
		"li %0, 1\n\t"			/* success - retval = 1 */
		"b 3f\n\t"			/* we've succeeded */
		"2:\n\t"
		"stwcx. %0, 0, %2\n\t"       	/* clear reservation (74xx) */
		"li %0, 0\n\t"			/* failure - retval = 0 */
		"3:\n\t"
		: "=&r" (ret), "=m" (*p)
		: "r" (p), "r" (cmpval), "r" (newval), "m" (*p)
		: "cc", "memory");
#endif

	return (ret);
}
static __inline int
atomic_cmpset_long(volatile u_long* p, u_long cmpval, u_long newval)
{
	int ret;

#ifdef __GNUCLIKE_ASM
	__asm __volatile (
	    #ifdef __powerpc64__
		"1:\tldarx %0, 0, %2\n\t"	/* load old value */
		"cmpld %3, %0\n\t"		/* compare */
		"bne 2f\n\t"			/* exit if not equal */
		"stdcx. %4, 0, %2\n\t"		/* attempt to store */
	    #else
		"1:\tlwarx %0, 0, %2\n\t"	/* load old value */
		"cmplw %3, %0\n\t"		/* compare */
		"bne 2f\n\t"			/* exit if not equal */
		"stwcx. %4, 0, %2\n\t"		/* attempt to store */
	    #endif
		"bne- 1b\n\t"			/* spin if failed */
		"li %0, 1\n\t"			/* success - retval = 1 */
		"b 3f\n\t"			/* we've succeeded */
		"2:\n\t"
	    #ifdef __powerpc64__
		"stdcx. %0, 0, %2\n\t"		/* clear reservation (74xx) */
	    #else
		"stwcx. %0, 0, %2\n\t"		/* clear reservation (74xx) */
	    #endif
		"li %0, 0\n\t"			/* failure - retval = 0 */
		"3:\n\t"
		: "=&r" (ret), "=m" (*p)
		: "r" (p), "r" (cmpval), "r" (newval), "m" (*p)
		: "cc", "memory");
#endif

	return (ret);
}

static __inline int
atomic_cmpset_acq_int(volatile u_int *p, u_int cmpval, u_int newval)
{
	int retval;

	retval = atomic_cmpset_int(p, cmpval, newval);
	__ATOMIC_BARRIER;
	return (retval);
}

static __inline int
atomic_cmpset_rel_int(volatile u_int *p, u_int cmpval, u_int newval)
{
	__ATOMIC_BARRIER;
	return (atomic_cmpset_int(p, cmpval, newval));
}

static __inline int
atomic_cmpset_acq_long(volatile u_long *p, u_long cmpval, u_long newval)
{
	u_long retval;

	retval = atomic_cmpset_long(p, cmpval, newval);
	__ATOMIC_BARRIER;
	return (retval);
}

static __inline int
atomic_cmpset_rel_long(volatile u_long *p, u_long cmpval, u_long newval)
{
	__ATOMIC_BARRIER;
	return (atomic_cmpset_long(p, cmpval, newval));
}

#define	atomic_cmpset_32	atomic_cmpset_int
#define	atomic_cmpset_acq_32	atomic_cmpset_acq_int
#define	atomic_cmpset_rel_32	atomic_cmpset_rel_int

#ifdef __powerpc64__
#define	atomic_cmpset_64	atomic_cmpset_long
#define	atomic_cmpset_acq_64	atomic_cmpset_acq_long
#define	atomic_cmpset_rel_64	atomic_cmpset_rel_long

#define	atomic_cmpset_ptr	atomic_cmpset_long
#define	atomic_cmpset_acq_ptr	atomic_cmpset_acq_long
#define	atomic_cmpset_rel_ptr	atomic_cmpset_rel_long
#else
#define	atomic_cmpset_ptr	atomic_cmpset_int
#define	atomic_cmpset_acq_ptr	atomic_cmpset_acq_int
#define	atomic_cmpset_rel_ptr	atomic_cmpset_rel_int
#endif

static __inline u_int
atomic_fetchadd_int(volatile u_int *p, u_int v)
{
	u_int value;

	do {
		value = *p;
	} while (!atomic_cmpset_int(p, value, value + v));
	return (value);
}

static __inline u_long
atomic_fetchadd_long(volatile u_long *p, u_long v)
{
	u_long value;

	do {
		value = *p;
	} while (!atomic_cmpset_long(p, value, value + v));
	return (value);
}

#define	atomic_fetchadd_32	atomic_fetchadd_int

#ifdef __powerpc64__
#define	atomic_fetchadd_64	atomic_fetchadd_long
#endif

#endif /* ! _MACHINE_ATOMIC_H_ */
