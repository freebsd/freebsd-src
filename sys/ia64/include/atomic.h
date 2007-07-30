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
#define	_MACHINE_ATOMIC_H_

/*
 * Various simple arithmetic on memory which is atomic in the presence
 * of interrupts and SMP safe.
 */

/*
 * Everything is built out of cmpxchg.
 */
#define	IA64_CMPXCHG(sz, sem, p, cmpval, newval, ret)			\
	__asm __volatile (						\
		"mov ar.ccv=%2;;\n\t"					\
		"cmpxchg" #sz "." #sem " %0=%4,%3,ar.ccv\n\t"		\
		: "=r" (ret), "=m" (*p)					\
		: "r" (cmpval), "r" (newval), "m" (*p)			\
		: "memory")

/*
 * Some common forms of cmpxch.
 */
static __inline uint32_t
ia64_cmpxchg_acq_32(volatile uint32_t* p, uint32_t cmpval, uint32_t newval)
{
	uint32_t ret;
	IA64_CMPXCHG(4, acq, p, cmpval, newval, ret);
	return (ret);
}

static __inline uint32_t
ia64_cmpxchg_rel_32(volatile uint32_t* p, uint32_t cmpval, uint32_t newval)
{
	uint32_t ret;
	IA64_CMPXCHG(4, rel, p, cmpval, newval, ret);
	return (ret);
}

static __inline uint64_t
ia64_cmpxchg_acq_64(volatile uint64_t* p, uint64_t cmpval, uint64_t newval)
{
	uint64_t ret;
	IA64_CMPXCHG(8, acq, p, cmpval, newval, ret);
	return (ret);
}

static __inline uint64_t
ia64_cmpxchg_rel_64(volatile uint64_t* p, uint64_t cmpval, uint64_t newval)
{
	uint64_t ret;
	IA64_CMPXCHG(8, rel, p, cmpval, newval, ret);
	return (ret);
}

#define	ATOMIC_STORE_LOAD(type, width, size)				\
	static __inline uint##width##_t					\
	ia64_ld_acq_##width(volatile uint##width##_t* p)		\
	{								\
		uint##width##_t v;					\
		__asm __volatile ("ld" size ".acq %0=%1" : "=r" (v)	\
		    : "m" (*p) : "memory");				\
		return (v);						\
	}								\
									\
	static __inline uint##width##_t					\
	atomic_load_acq_##width(volatile uint##width##_t* p)		\
	{								\
		uint##width##_t v;					\
		__asm __volatile ("ld" size ".acq %0=%1" : "=r" (v)	\
		    : "m" (*p) : "memory");				\
		return (v);						\
	}								\
									\
	static __inline uint##width##_t					\
	atomic_load_acq_##type(volatile uint##width##_t* p)		\
	{								\
		uint##width##_t v;					\
		__asm __volatile ("ld" size ".acq %0=%1" : "=r" (v)	\
		    : "m" (*p) : "memory");				\
		return (v);						\
	}								\
								       	\
	static __inline void						\
	ia64_st_rel_##width(volatile uint##width##_t* p, uint##width##_t v) \
	{								\
		__asm __volatile ("st" size ".rel %0=%1" : "=m" (*p)	\
		    : "r" (v) : "memory");				\
	}								\
									\
	static __inline void						\
	atomic_store_rel_##width(volatile uint##width##_t* p,		\
	    uint##width##_t v)						\
	{								\
		__asm __volatile ("st" size ".rel %0=%1" : "=m" (*p)	\
		    : "r" (v) : "memory");				\
	}								\
									\
	static __inline void						\
	atomic_store_rel_##type(volatile uint##width##_t* p,		\
	    uint##width##_t v)						\
	{								\
		__asm __volatile ("st" size ".rel %0=%1" : "=m" (*p)	\
		    : "r" (v) : "memory");				\
	}

ATOMIC_STORE_LOAD(char,	 8,  "1")
ATOMIC_STORE_LOAD(short, 16, "2")
ATOMIC_STORE_LOAD(int,	 32, "4")
ATOMIC_STORE_LOAD(long,	 64, "8")

#undef ATOMIC_STORE_LOAD

#define	atomic_load_acq_ptr(p)		\
    ((void *)atomic_load_acq_64((volatile uint64_t *)p))

#define	atomic_store_rel_ptr(p, v)	\
    atomic_store_rel_64((volatile uint64_t *)p, (uint64_t)v)

#define	IA64_ATOMIC(sz, type, name, width, op)				\
	static __inline type						\
	atomic_##name##_acq_##width(volatile type *p, type v)		\
	{								\
		type old, ret;						\
		do {							\
			old = *p;					\
			IA64_CMPXCHG(sz, acq, p, old, old op v, ret);	\
		} while (ret != old);					\
		return (old);						\
	}								\
									\
	static __inline type						\
	atomic_##name##_rel_##width(volatile type *p, type v)		\
	{								\
		type old, ret;						\
		do {							\
			old = *p;					\
			IA64_CMPXCHG(sz, rel, p, old, old op v, ret);	\
		} while (ret != old);					\
		return (old);						\
	}

IA64_ATOMIC(1, uint8_t,	 set, 8,  |)
IA64_ATOMIC(2, uint16_t, set, 16, |)
IA64_ATOMIC(4, uint32_t, set, 32, |)
IA64_ATOMIC(8, uint64_t, set, 64, |)

IA64_ATOMIC(1, uint8_t,  clear,	8,  &~)
IA64_ATOMIC(2, uint16_t, clear,	16, &~)
IA64_ATOMIC(4, uint32_t, clear,	32, &~)
IA64_ATOMIC(8, uint64_t, clear,	64, &~)

IA64_ATOMIC(1, uint8_t,  add, 8,  +)
IA64_ATOMIC(2, uint16_t, add, 16, +)
IA64_ATOMIC(4, uint32_t, add, 32, +)
IA64_ATOMIC(8, uint64_t, add, 64, +)

IA64_ATOMIC(1, uint8_t,  subtract, 8,  -)
IA64_ATOMIC(2, uint16_t, subtract, 16, -)
IA64_ATOMIC(4, uint32_t, subtract, 32, -)
IA64_ATOMIC(8, uint64_t, subtract, 64, -)

#undef IA64_ATOMIC

#define	atomic_set_8			atomic_set_acq_8
#define	atomic_clear_8			atomic_clear_acq_8
#define	atomic_add_8			atomic_add_acq_8
#define	atomic_subtract_8		atomic_subtract_acq_8

#define	atomic_set_16			atomic_set_acq_16
#define	atomic_clear_16			atomic_clear_acq_16
#define	atomic_add_16			atomic_add_acq_16
#define	atomic_subtract_16		atomic_subtract_acq_16

#define	atomic_set_32			atomic_set_acq_32
#define	atomic_clear_32			atomic_clear_acq_32
#define	atomic_add_32			atomic_add_acq_32
#define	atomic_subtract_32		atomic_subtract_acq_32

#define	atomic_set_64			atomic_set_acq_64
#define	atomic_clear_64			atomic_clear_acq_64
#define	atomic_add_64			atomic_add_acq_64
#define	atomic_subtract_64		atomic_subtract_acq_64

#define	atomic_set_char			atomic_set_8
#define	atomic_clear_char		atomic_clear_8
#define	atomic_add_char			atomic_add_8
#define	atomic_subtract_char		atomic_subtract_8
#define	atomic_set_acq_char		atomic_set_acq_8
#define	atomic_clear_acq_char		atomic_clear_acq_8
#define	atomic_add_acq_char		atomic_add_acq_8
#define	atomic_subtract_acq_char	atomic_subtract_acq_8
#define	atomic_set_rel_char		atomic_set_rel_8
#define	atomic_clear_rel_char		atomic_clear_rel_8
#define	atomic_add_rel_char		atomic_add_rel_8
#define	atomic_subtract_rel_char	atomic_subtract_rel_8

#define	atomic_set_short		atomic_set_16
#define	atomic_clear_short		atomic_clear_16
#define	atomic_add_short		atomic_add_16
#define	atomic_subtract_short		atomic_subtract_16
#define	atomic_set_acq_short		atomic_set_acq_16
#define	atomic_clear_acq_short		atomic_clear_acq_16
#define	atomic_add_acq_short		atomic_add_acq_16
#define	atomic_subtract_acq_short	atomic_subtract_acq_16
#define	atomic_set_rel_short		atomic_set_rel_16
#define	atomic_clear_rel_short		atomic_clear_rel_16
#define	atomic_add_rel_short		atomic_add_rel_16
#define	atomic_subtract_rel_short	atomic_subtract_rel_16

#define	atomic_set_int			atomic_set_32
#define	atomic_clear_int		atomic_clear_32
#define	atomic_add_int			atomic_add_32
#define	atomic_subtract_int		atomic_subtract_32
#define	atomic_set_acq_int		atomic_set_acq_32
#define	atomic_clear_acq_int		atomic_clear_acq_32
#define	atomic_add_acq_int		atomic_add_acq_32
#define	atomic_subtract_acq_int		atomic_subtract_acq_32
#define	atomic_set_rel_int		atomic_set_rel_32
#define	atomic_clear_rel_int		atomic_clear_rel_32
#define	atomic_add_rel_int		atomic_add_rel_32
#define	atomic_subtract_rel_int		atomic_subtract_rel_32

#define	atomic_set_long			atomic_set_64
#define	atomic_clear_long		atomic_clear_64
#define	atomic_add_long			atomic_add_64
#define	atomic_subtract_long		atomic_subtract_64
#define	atomic_set_acq_long		atomic_set_acq_64
#define	atomic_clear_acq_long		atomic_clear_acq_64
#define	atomic_add_acq_long		atomic_add_acq_64
#define	atomic_subtract_acq_long	atomic_subtract_acq_64
#define	atomic_set_rel_long		atomic_set_rel_64
#define	atomic_clear_rel_long		atomic_clear_rel_64
#define	atomic_add_rel_long		atomic_add_rel_64
#define	atomic_subtract_rel_long	atomic_subtract_rel_64

/* XXX Needs casting. */
#define	atomic_set_ptr			atomic_set_64
#define	atomic_clear_ptr		atomic_clear_64
#define	atomic_add_ptr			atomic_add_64
#define	atomic_subtract_ptr		atomic_subtract_64
#define	atomic_set_acq_ptr		atomic_set_acq_64
#define	atomic_clear_acq_ptr		atomic_clear_acq_64
#define	atomic_add_acq_ptr		atomic_add_acq_64
#define	atomic_subtract_acq_ptr		atomic_subtract_acq_64
#define	atomic_set_rel_ptr		atomic_set_rel_64
#define	atomic_clear_rel_ptr		atomic_clear_rel_64
#define	atomic_add_rel_ptr		atomic_add_rel_64
#define	atomic_subtract_rel_ptr		atomic_subtract_rel_64

#undef IA64_CMPXCHG

/*
 * Atomically compare the value stored at *p with cmpval and if the
 * two values are equal, update the value of *p with newval. Returns
 * zero if the compare failed, nonzero otherwise.
 */
static __inline int
atomic_cmpset_acq_32(volatile uint32_t* p, uint32_t cmpval, uint32_t newval)
{
	return (ia64_cmpxchg_acq_32(p, cmpval, newval) == cmpval);
}

static __inline int
atomic_cmpset_rel_32(volatile uint32_t* p, uint32_t cmpval, uint32_t newval)
{
	return (ia64_cmpxchg_rel_32(p, cmpval, newval) == cmpval);
}

/*
 * Atomically compare the value stored at *p with cmpval and if the
 * two values are equal, update the value of *p with newval. Returns
 * zero if the compare failed, nonzero otherwise.
 */
static __inline int
atomic_cmpset_acq_64(volatile uint64_t* p, uint64_t cmpval, uint64_t newval)
{
	return (ia64_cmpxchg_acq_64(p, cmpval, newval) == cmpval);
}

static __inline int
atomic_cmpset_rel_64(volatile uint64_t* p, uint64_t cmpval, uint64_t newval)
{
	return (ia64_cmpxchg_rel_64(p, cmpval, newval) == cmpval);
}

#define	atomic_cmpset_32		atomic_cmpset_acq_32
#define	atomic_cmpset_64		atomic_cmpset_acq_64
#define	atomic_cmpset_int		atomic_cmpset_32
#define	atomic_cmpset_long		atomic_cmpset_64
#define	atomic_cmpset_acq_int		atomic_cmpset_acq_32
#define	atomic_cmpset_rel_int		atomic_cmpset_rel_32
#define	atomic_cmpset_acq_long		atomic_cmpset_acq_64
#define	atomic_cmpset_rel_long		atomic_cmpset_rel_64

#define	atomic_cmpset_acq_ptr(p, o, n)	\
    (atomic_cmpset_acq_64((volatile uint64_t *)p, (uint64_t)o, (uint64_t)n))

#define	atomic_cmpset_ptr		atomic_cmpset_acq_ptr

#define	atomic_cmpset_rel_ptr(p, o, n)	\
    (atomic_cmpset_rel_64((volatile uint64_t *)p, (uint64_t)o, (uint64_t)n))

static __inline uint32_t
atomic_readandclear_32(volatile uint32_t* p)
{
	uint32_t val;
	do {
		val = *p;
	} while (!atomic_cmpset_32(p, val, 0));
	return (val);
}

static __inline uint64_t
atomic_readandclear_64(volatile uint64_t* p)
{
	uint64_t val;
	do {
		val = *p;
	} while (!atomic_cmpset_64(p, val, 0));
	return (val);
}

#define	atomic_readandclear_int		atomic_readandclear_32
#define	atomic_readandclear_long	atomic_readandclear_64

/*
 * Atomically add the value of v to the integer pointed to by p and return
 * the previous value of *p.
 *
 * XXX: Should we use the fetchadd instruction here?
 */
static __inline uint32_t
atomic_fetchadd_32(volatile uint32_t *p, uint32_t v)
{
	uint32_t value;

	do {
		value = *p;
	} while (!atomic_cmpset_32(p, value, value + v));
	return (value);
}

#define	atomic_fetchadd_int		atomic_fetchadd_32

#endif /* ! _MACHINE_ATOMIC_H_ */
