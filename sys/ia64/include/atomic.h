/*-
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
#define _MACHINE_ATOMIC_H_

/*
 * Various simple arithmetic on memory which is atomic in the presence
 * of interrupts and SMP safe.
 */

/*
 * Everything is built out of cmpxchg.
 */
#define IA64_CMPXCHG(sz, sem, type, p, cmpval, newval)		\
({								\
	type _cmpval = cmpval;					\
	type _newval = newval;					\
	volatile type *_p = (volatile type *) p;		\
	type _ret;						\
								\
	__asm __volatile (					\
		"mov ar.ccv=%2;;\n\t"				\
		"cmpxchg" #sz "." #sem " %0=%4,%3,ar.ccv\n\t"	\
		: "=r" (_ret), "=m" (*_p)			\
		: "r" (_cmpval), "r" (_newval), "m" (*_p)	\
		: "memory");					\
	_ret;							\
})

/*
 * Some common forms of cmpxch.
 */
static __inline u_int32_t
ia64_cmpxchg_acq_32(volatile u_int32_t* p, u_int32_t cmpval, u_int32_t newval)
{
	return IA64_CMPXCHG(4, acq, u_int32_t, p, cmpval, newval);
}

static __inline u_int32_t
ia64_cmpxchg_rel_32(volatile u_int32_t* p, u_int32_t cmpval, u_int32_t newval)
{
	return IA64_CMPXCHG(4, rel, u_int32_t, p, cmpval, newval);
}

static __inline u_int64_t
ia64_cmpxchg_acq_64(volatile u_int64_t* p, u_int64_t cmpval, u_int64_t newval)
{
	return IA64_CMPXCHG(8, acq, u_int64_t, p, cmpval, newval);
}

static __inline u_int64_t
ia64_cmpxchg_rel_64(volatile u_int64_t* p, u_int64_t cmpval, u_int64_t newval)
{
	return IA64_CMPXCHG(8, rel, u_int64_t, p, cmpval, newval);
}

/*
 * Store with release semantics is used to release locks.
 */
static __inline void
ia64_st_rel_32(volatile u_int32_t* p, u_int32_t v)
{
	__asm __volatile ("st4.rel %0=%1"
			  : "=m" (*p)
			  : "r" (v)
			  : "memory");
}

static __inline void
ia64_st_rel_64(volatile u_int64_t* p, u_int64_t v)
{
	__asm __volatile ("st8.rel %0=%1"
			  : "=m" (*p)
			  : "r" (v)
			  : "memory");
}

#define IA64_ATOMIC(sz, type, name, op)					\
									\
static __inline void							\
atomic_##name(volatile type *p, type v)					\
{									\
	type old;							\
	do {								\
		old = *p;						\
	} while (IA64_CMPXCHG(sz, acq, type, p, old, old op v) != old);	\
}

IA64_ATOMIC(1, u_int8_t,  set_8,	|)
IA64_ATOMIC(2, u_int16_t, set_16,	|)
IA64_ATOMIC(4, u_int32_t, set_32,	|)
IA64_ATOMIC(8, u_int64_t, set_64,	|)

IA64_ATOMIC(1, u_int8_t,  clear_8,	&~)
IA64_ATOMIC(2, u_int16_t, clear_16,	&~)
IA64_ATOMIC(4, u_int32_t, clear_32,	&~)
IA64_ATOMIC(8, u_int64_t, clear_64,	&~)

IA64_ATOMIC(1, u_int8_t,  add_8,	+)
IA64_ATOMIC(2, u_int16_t, add_16,	+)
IA64_ATOMIC(4, u_int32_t, add_32,	+)
IA64_ATOMIC(8, u_int64_t, add_64,	+)

IA64_ATOMIC(1, u_int8_t,  subtract_8,	-)
IA64_ATOMIC(2, u_int16_t, subtract_16,	-)
IA64_ATOMIC(4, u_int32_t, subtract_32,	-)
IA64_ATOMIC(8, u_int64_t, subtract_64,	-)

#undef IA64_ATOMIC
#undef IA64_CMPXCHG

#define atomic_set_char		atomic_set_8
#define atomic_clear_char	atomic_clear_8
#define atomic_add_char		atomic_add_8
#define atomic_subtract_char	atomic_subtract_8

#define atomic_set_short	atomic_set_16
#define atomic_clear_short	atomic_clear_16
#define atomic_add_short	atomic_add_16
#define atomic_subtract_short	atomic_subtract_16

#define atomic_set_int		atomic_set_32
#define atomic_clear_int	atomic_clear_32
#define atomic_add_int		atomic_add_32
#define atomic_subtract_int	atomic_subtract_32

#define atomic_set_long		atomic_set_64
#define atomic_clear_long	atomic_clear_64
#define atomic_add_long		atomic_add_64
#define atomic_subtract_long	atomic_subtract_64

/*
 * Atomically compare the value stored at *p with cmpval and if the
 * two values are equal, update the value of *p with newval. Returns
 * zero if the compare failed, nonzero otherwise.
 */
static __inline int
atomic_cmpset_32(volatile u_int32_t* p, u_int32_t cmpval, u_int32_t newval)
{
	return ia64_cmpxchg_acq_32(p, cmpval, newval) == cmpval;
}

/*
 * Atomically compare the value stored at *p with cmpval and if the
 * two values are equal, update the value of *p with newval. Returns
 * zero if the compare failed, nonzero otherwise.
 */
static __inline int
atomic_cmpset_64(volatile u_int64_t* p, u_int64_t cmpval, u_int64_t newval)
{
	return ia64_cmpxchg_acq_64(p, cmpval, newval) == cmpval;
}

#define	atomic_cmpset_int	atomic_cmpset_32
#define	atomic_cmpset_long	atomic_cmpset_64

static __inline int
atomic_cmpset_ptr(volatile void *dst, void *exp, void *src)
{
        return atomic_cmpset_long((volatile u_long *)dst,
				  (u_long)exp, (u_long)src);
}

#endif /* ! _MACHINE_ATOMIC_H_ */
