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

#define __ATOMIC_ADD_8(p, v, t)					\
    8-bit atomic_add not implemented

#define __ATOMIC_ADD_16(p, v, t)				\
    16-bit atomic_add not implemented

#define __ATOMIC_ADD_32(p, v, t)				\
    __asm __volatile(						\
	"1:	lwarx	%0, 0, %2\n"				\
	"	add	%0, %3, %0\n"				\
	"	stwcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __ATOMIC_ADD_32 */

#ifdef __powerpc64__
#define __ATOMIC_ADD_64(p, v, t)				\
    __asm __volatile(						\
	"1:	ldarx	%0, 0, %2\n"				\
	"	add	%0, %3, %0\n"				\
	"	stdcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __ATOMIC_ADD_64 */
#else
#define	__ATOMIC_ADD_64(p, v, t)				\
    64-bit atomic_add not implemented
#endif

#define	_ATOMIC_ADD(width, suffix, type)			\
    static __inline void					\
    atomic_add_##suffix(volatile type *p, type v) {		\
	type t;							\
	__ATOMIC_ADD_##width(p, v, t);				\
    }								\
								\
    static __inline void					\
    atomic_add_acq_##suffix(volatile type *p, type v) {		\
	type t;							\
	__ATOMIC_ADD_##width(p, v, t);				\
	__ATOMIC_BARRIER;					\
    }								\
								\
    static __inline void					\
    atomic_add_rel_##suffix(volatile type *p, type v) {		\
	type t;							\
	__ATOMIC_BARRIER;					\
	__ATOMIC_ADD_##width(p, v, t);				\
    }								\
    /* _ATOMIC_ADD */

#if 0
_ATOMIC_ADD(8, 8, uint8_t)
_ATOMIC_ADD(8, char, u_char)
_ATOMIC_ADD(16, 16, uint16_t)
_ATOMIC_ADD(16, short, u_short)
#endif
_ATOMIC_ADD(32, 32, uint32_t)
_ATOMIC_ADD(32, int, u_int)
#ifdef __powerpc64__
_ATOMIC_ADD(64, 64, uint64_t)
_ATOMIC_ADD(64, long, u_long)
_ATOMIC_ADD(64, ptr, uintptr_t)
#else
_ATOMIC_ADD(32, long, u_long)
_ATOMIC_ADD(32, ptr, uintptr_t)
#endif

#undef _ATOMIC_ADD
#undef __ATOMIC_ADD_64
#undef __ATOMIC_ADD_32
#undef __ATOMIC_ADD_16
#undef __ATOMIC_ADD_8

/*
 * atomic_clear(p, v)
 * { *p &= ~v; }
 */

#define __ATOMIC_CLEAR_8(p, v, t)				\
    8-bit atomic_clear not implemented

#define __ATOMIC_CLEAR_16(p, v, t)				\
    16-bit atomic_clear not implemented

#define __ATOMIC_CLEAR_32(p, v, t)				\
    __asm __volatile(						\
	"1:	lwarx	%0, 0, %2\n"				\
	"	andc	%0, %0, %3\n"				\
	"	stwcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __ATOMIC_CLEAR_32 */

#ifdef __powerpc64__
#define __ATOMIC_CLEAR_64(p, v, t)				\
    __asm __volatile(						\
	"1:	ldarx	%0, 0, %2\n"				\
	"	andc	%0, %0, %3\n"				\
	"	stdcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __ATOMIC_CLEAR_64 */
#else
#define	__ATOMIC_CLEAR_64(p, v, t)				\
    64-bit atomic_clear not implemented
#endif

#define	_ATOMIC_CLEAR(width, suffix, type)			\
    static __inline void					\
    atomic_clear_##suffix(volatile type *p, type v) {		\
	type t;							\
	__ATOMIC_CLEAR_##width(p, v, t);			\
    }								\
								\
    static __inline void					\
    atomic_clear_acq_##suffix(volatile type *p, type v) {	\
	type t;							\
	__ATOMIC_CLEAR_##width(p, v, t);			\
	__ATOMIC_BARRIER;					\
    }								\
								\
    static __inline void					\
    atomic_clear_rel_##suffix(volatile type *p, type v) {	\
	type t;							\
	__ATOMIC_BARRIER;					\
	__ATOMIC_CLEAR_##width(p, v, t);			\
    }								\
    /* _ATOMIC_CLEAR */

#if 0
_ATOMIC_CLEAR(8, 8, uint8_t)
_ATOMIC_CLEAR(8, char, u_char)
_ATOMIC_CLEAR(16, 16, uint16_t)
_ATOMIC_CLEAR(16, short, u_short)
#endif
_ATOMIC_CLEAR(32, 32, uint32_t)
_ATOMIC_CLEAR(32, int, u_int)
#ifdef __powerpc64__
_ATOMIC_CLEAR(64, 64, uint64_t)
_ATOMIC_CLEAR(64, long, u_long)
_ATOMIC_CLEAR(64, ptr, uintptr_t)
#else
_ATOMIC_CLEAR(32, long, u_long)
_ATOMIC_CLEAR(32, ptr, uintptr_t)
#endif

#undef _ATOMIC_CLEAR
#undef __ATOMIC_CLEAR_64
#undef __ATOMIC_CLEAR_32
#undef __ATOMIC_CLEAR_16
#undef __ATOMIC_CLEAR_8

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

#define __ATOMIC_SET_8(p, v, t)					\
    8-bit atomic_set not implemented

#define __ATOMIC_SET_16(p, v, t)				\
    16-bit atomic_set not implemented

#define __ATOMIC_SET_32(p, v, t)				\
    __asm __volatile(						\
	"1:	lwarx	%0, 0, %2\n"				\
	"	or	%0, %3, %0\n"				\
	"	stwcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __ATOMIC_SET_32 */

#ifdef __powerpc64__
#define __ATOMIC_SET_64(p, v, t)				\
    __asm __volatile(						\
	"1:	ldarx	%0, 0, %2\n"				\
	"	or	%0, %3, %0\n"				\
	"	stdcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __ATOMIC_SET_64 */
#else
#define	__ATOMIC_SET_64(p, v, t)				\
    64-bit atomic_set not implemented
#endif

#define	_ATOMIC_SET(width, suffix, type)			\
    static __inline void					\
    atomic_set_##suffix(volatile type *p, type v) {		\
	type t;							\
	__ATOMIC_SET_##width(p, v, t);				\
    }								\
								\
    static __inline void					\
    atomic_set_acq_##suffix(volatile type *p, type v) {		\
	type t;							\
	__ATOMIC_SET_##width(p, v, t);				\
	__ATOMIC_BARRIER;					\
    }								\
								\
    static __inline void					\
    atomic_set_rel_##suffix(volatile type *p, type v) {		\
	type t;							\
	__ATOMIC_BARRIER;					\
	__ATOMIC_SET_##width(p, v, t);				\
    }								\
    /* _ATOMIC_SET */

#if 0
_ATOMIC_SET(8, 8, uint8_t)
_ATOMIC_SET(8, char, u_char)
_ATOMIC_SET(16, 16, uint16_t)
_ATOMIC_SET(16, short, u_short)
#endif
_ATOMIC_SET(32, 32, uint32_t)
_ATOMIC_SET(32, int, u_int)
#ifdef __powerpc64__
_ATOMIC_SET(64, 64, uint64_t)
_ATOMIC_SET(64, long, u_long)
_ATOMIC_SET(64, ptr, uintptr_t)
#else
_ATOMIC_SET(32, long, u_long)
_ATOMIC_SET(32, ptr, uintptr_t)
#endif

#undef _ATOMIC_SET
#undef __ATOMIC_SET_64
#undef __ATOMIC_SET_32
#undef __ATOMIC_SET_16
#undef __ATOMIC_SET_8

/*
 * atomic_subtract(p, v)
 * { *p -= v; }
 */

#define __ATOMIC_SUBTRACT_8(p, v, t)				\
    8-bit atomic_subtract not implemented

#define __ATOMIC_SUBTRACT_16(p, v, t)				\
    16-bit atomic_subtract not implemented

#define __ATOMIC_SUBTRACT_32(p, v, t)				\
    __asm __volatile(						\
	"1:	lwarx	%0, 0, %2\n"				\
	"	subf	%0, %3, %0\n"				\
	"	stwcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __ATOMIC_SUBTRACT_32 */

#ifdef __powerpc64__
#define __ATOMIC_SUBTRACT_64(p, v, t)				\
    __asm __volatile(						\
	"1:	ldarx	%0, 0, %2\n"				\
	"	subf	%0, %3, %0\n"				\
	"	stdcx.	%0, 0, %2\n"				\
	"	bne-	1b\n"					\
	: "=&r" (t), "=m" (*p)					\
	: "r" (p), "r" (v), "m" (*p)				\
	: "cc", "memory")					\
    /* __ATOMIC_SUBTRACT_64 */
#else
#define	__ATOMIC_SUBTRACT_64(p, v, t)				\
    64-bit atomic_subtract not implemented
#endif

#define	_ATOMIC_SUBTRACT(width, suffix, type)			\
    static __inline void					\
    atomic_subtract_##suffix(volatile type *p, type v) {	\
	type t;							\
	__ATOMIC_SUBTRACT_##width(p, v, t);			\
    }								\
								\
    static __inline void					\
    atomic_subtract_acq_##suffix(volatile type *p, type v) {	\
	type t;							\
	__ATOMIC_SUBTRACT_##width(p, v, t);			\
	__ATOMIC_BARRIER;					\
    }								\
								\
    static __inline void					\
    atomic_subtract_rel_##suffix(volatile type *p, type v) {	\
	type t;							\
	__ATOMIC_BARRIER;					\
	__ATOMIC_SUBTRACT_##width(p, v, t);			\
    }								\
    /* _ATOMIC_SUBTRACT */

#if 0
_ATOMIC_SUBTRACT(8, 8, uint8_t)
_ATOMIC_SUBTRACT(8, char, u_char)
_ATOMIC_SUBTRACT(16, 16, uint16_t)
_ATOMIC_SUBTRACT(16, short, u_short)
#endif
_ATOMIC_SUBTRACT(32, 32, uint32_t)
_ATOMIC_SUBTRACT(32, int, u_int)
#ifdef __powerpc64__
_ATOMIC_SUBTRACT(64, 64, uint64_t)
_ATOMIC_SUBTRACT(64, long, u_long)
_ATOMIC_SUBTRACT(64, ptr, uintptr_t)
#else
_ATOMIC_SUBTRACT(32, long, u_long)
_ATOMIC_SUBTRACT(32, ptr, uintptr_t)
#endif

#undef _ATOMIC_SUBTRACT
#undef __ATOMIC_SUBTRACT_64
#undef __ATOMIC_SUBTRACT_32
#undef __ATOMIC_SUBTRACT_16
#undef __ATOMIC_SUBTRACT_8

/*
 * atomic_store_rel(p, v)
 */
/* TODO -- see below */

/*
 * Old/original implementations that still need revisiting.
 */

static __inline uint32_t
atomic_readandclear_32(volatile uint32_t *addr)
{
	uint32_t result,temp;

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
static __inline uint64_t
atomic_readandclear_64(volatile uint64_t *addr)
{
	uint64_t result,temp;

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

#define	atomic_readandclear_int		atomic_readandclear_32

#ifdef __powerpc64__
#define	atomic_readandclear_long	atomic_readandclear_64
#define	atomic_readandclear_ptr		atomic_readandclear_64
#else
#define	atomic_readandclear_long	atomic_readandclear_32
#define	atomic_readandclear_ptr		atomic_readandclear_32
#endif

/*
 * We assume that a = b will do atomic loads and stores.
 */
#define	ATOMIC_STORE_LOAD(TYPE, WIDTH)				\
static __inline u_##TYPE					\
atomic_load_acq_##WIDTH(volatile u_##TYPE *p)			\
{								\
	u_##TYPE v;						\
								\
	v = *p;							\
	__ATOMIC_BARRIER;					\
	return (v);						\
}								\
								\
static __inline void						\
atomic_store_rel_##WIDTH(volatile u_##TYPE *p, u_##TYPE v)	\
{								\
	__ATOMIC_BARRIER;					\
	*p = v;							\
}								\
								\
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

ATOMIC_STORE_LOAD(char,		8)
ATOMIC_STORE_LOAD(short,	16)
ATOMIC_STORE_LOAD(int,		32)
#ifdef __powerpc64__
ATOMIC_STORE_LOAD(long,		64)
#endif

#ifdef __powerpc64__
#define	atomic_load_acq_long	atomic_load_acq_64
#define	atomic_store_rel_long	atomic_store_rel_64
#define	atomic_load_acq_ptr	atomic_load_acq_64
#define	atomic_store_rel_ptr	atomic_store_rel_64
#else
#define	atomic_load_acq_long	atomic_load_acq_32
#define	atomic_store_rel_long	atomic_store_rel_32
#define	atomic_load_acq_ptr	atomic_load_acq_32
#define	atomic_store_rel_ptr	atomic_store_rel_32
#endif

#undef ATOMIC_STORE_LOAD

/*
 * Atomically compare the value stored at *p with cmpval and if the
 * two values are equal, update the value of *p with newval. Returns
 * zero if the compare failed, nonzero otherwise.
 */
static __inline int
atomic_cmpset_32(volatile uint32_t* p, uint32_t cmpval, uint32_t newval)
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
		"stdcx. %4, 0, %2\n\t"      	/* attempt to store */
	    #else
		"1:\tlwarx %0, 0, %2\n\t"	/* load old value */
		"cmplw %3, %0\n\t"		/* compare */
		"bne 2f\n\t"			/* exit if not equal */
		"stwcx. %4, 0, %2\n\t"      	/* attempt to store */
	    #endif
		"bne- 1b\n\t"			/* spin if failed */
		"li %0, 1\n\t"			/* success - retval = 1 */
		"b 3f\n\t"			/* we've succeeded */
		"2:\n\t"
	    #ifdef __powerpc64__
		"stdcx. %0, 0, %2\n\t"       	/* clear reservation (74xx) */
	    #else
		"stwcx. %0, 0, %2\n\t"       	/* clear reservation (74xx) */
	    #endif
		"li %0, 0\n\t"			/* failure - retval = 0 */
		"3:\n\t"
		: "=&r" (ret), "=m" (*p)
		: "r" (p), "r" (cmpval), "r" (newval), "m" (*p)
		: "cc", "memory");
#endif

	return (ret);
}

#define	atomic_cmpset_int	atomic_cmpset_32

#ifdef __powerpc64__
#define	atomic_cmpset_ptr(dst, old, new)	\
    atomic_cmpset_long((volatile u_long *)(dst), (u_long)(old), (u_long)(new))
#else
#define	atomic_cmpset_ptr(dst, old, new)	\
    atomic_cmpset_32((volatile u_int *)(dst), (u_int)(old), (u_int)(new))
#endif

static __inline int
atomic_cmpset_acq_32(volatile uint32_t *p, uint32_t cmpval, uint32_t newval)
{
	int retval;

	retval = atomic_cmpset_32(p, cmpval, newval);
	__ATOMIC_BARRIER;
	return (retval);
}

static __inline int
atomic_cmpset_rel_32(volatile uint32_t *p, uint32_t cmpval, uint32_t newval)
{
	__ATOMIC_BARRIER;
	return (atomic_cmpset_32(p, cmpval, newval));
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

#define	atomic_cmpset_acq_int	atomic_cmpset_acq_32
#define	atomic_cmpset_rel_int	atomic_cmpset_rel_32

#ifdef __powerpc64__
#define	atomic_cmpset_acq_ptr(dst, old, new)	\
    atomic_cmpset_acq_long((volatile u_long *)(dst), (u_long)(old), (u_long)(new))
#define	atomic_cmpset_rel_ptr(dst, old, new)	\
    atomic_cmpset_rel_long((volatile u_long *)(dst), (u_long)(old), (u_long)(new))
#else
#define	atomic_cmpset_acq_ptr(dst, old, new)	\
    atomic_cmpset_acq_32((volatile u_int *)(dst), (u_int)(old), (u_int)(new))
#define	atomic_cmpset_rel_ptr(dst, old, new)	\
    atomic_cmpset_rel_32((volatile u_int *)(dst), (u_int)(old), (u_int)(new))
#endif

static __inline uint32_t
atomic_fetchadd_32(volatile uint32_t *p, uint32_t v)
{
	uint32_t value;

	do {
		value = *p;
	} while (!atomic_cmpset_32(p, value, value + v));
	return (value);
}

#define	atomic_fetchadd_int	atomic_fetchadd_32

#ifdef __powerpc64__
static __inline uint64_t
atomic_fetchadd_64(volatile uint64_t *p, uint64_t v)
{
	uint64_t value;

	do {
		value = *p;
	} while (!atomic_cmpset_long(p, value, value + v));
	return (value);
}

#define	atomic_fetchadd_long	atomic_fetchadd_64
#else
#define	atomic_fetchadd_long(p, v)	\
    (u_long)atomic_fetchadd_32((volatile u_int *)(p), (u_int)(v))
#endif

#endif /* ! _MACHINE_ATOMIC_H_ */
