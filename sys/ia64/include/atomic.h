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

#define ATOMIC_STORE_LOAD(type, width, size)			\
static __inline u_int##width##_t				\
ia64_ld_acq_##width(volatile u_int##width##_t* p)		\
{								\
	u_int##width##_t v;					\
								\
	__asm __volatile ("ld" size ".acq %0=%1"		\
			  : "=r" (v)				\
			  : "m" (*p)				\
			  : "memory");				\
	return (v);						\
}								\
								\
static __inline u_int##width##_t				\
atomic_load_acq_##width(volatile u_int##width##_t* p)		\
{								\
	u_int##width##_t v;					\
								\
	__asm __volatile ("ld" size ".acq %0=%1"		\
			  : "=r" (v)				\
			  : "m" (*p)				\
			  : "memory");				\
	return (v);						\
}								\
								\
static __inline u_int##width##_t				\
atomic_load_acq_##type(volatile u_int##width##_t* p)		\
{								\
	u_int##width##_t v;					\
								\
	__asm __volatile ("ld" size ".acq %0=%1"		\
			  : "=r" (v)				\
			  : "m" (*p)				\
			  : "memory");				\
	return (v);						\
}								\
							       	\
static __inline void						\
ia64_st_rel_##width(volatile u_int##width##_t* p, u_int##width##_t v)\
{								\
	__asm __volatile ("st" size ".rel %0=%1"		\
			  : "=m" (*p)				\
			  : "r" (v)				\
			  : "memory");				\
}								\
							       	\
static __inline void						\
atomic_store_rel_##width(volatile u_int##width##_t* p, u_int##width##_t v)\
{								\
	__asm __volatile ("st" size ".rel %0=%1"		\
			  : "=m" (*p)				\
			  : "r" (v)				\
			  : "memory");				\
}								\
							       	\
static __inline void						\
atomic_store_rel_##type(volatile u_int##width##_t* p, u_int##width##_t v)\
{								\
	__asm __volatile ("st" size ".rel %0=%1"		\
			  : "=m" (*p)				\
			  : "r" (v)				\
			  : "memory");				\
}

ATOMIC_STORE_LOAD(char,		8,	"1")
ATOMIC_STORE_LOAD(short,	16,	"2")
ATOMIC_STORE_LOAD(int,		32,	"4")
ATOMIC_STORE_LOAD(long,		64,	"8")

#undef ATOMIC_STORE_LOAD

#define IA64_ATOMIC(sz, type, name, width, op)				\
									\
static __inline void							\
atomic_##name##_acq_##width(volatile type *p, type v)			\
{									\
	type old;							\
	do {								\
		old = *p;						\
	} while (IA64_CMPXCHG(sz, acq, type, p, old, old op v) != old);	\
}									\
									\
static __inline void							\
atomic_##name##_rel_##width(volatile type *p, type v)			\
{									\
	type old;							\
	do {								\
		old = *p;						\
	} while (IA64_CMPXCHG(sz, rel, type, p, old, old op v) != old);	\
}

IA64_ATOMIC(1, u_int8_t,  set,	8,	|)
IA64_ATOMIC(2, u_int16_t, set,	16,	|)
IA64_ATOMIC(4, u_int32_t, set,	32,	|)
IA64_ATOMIC(8, u_int64_t, set,	64,	|)

IA64_ATOMIC(1, u_int8_t,  clear,	8,	&~)
IA64_ATOMIC(2, u_int16_t, clear,	16,	&~)
IA64_ATOMIC(4, u_int32_t, clear,	32,	&~)
IA64_ATOMIC(8, u_int64_t, clear,	64,	&~)

IA64_ATOMIC(1, u_int8_t,  add,	8,	+)
IA64_ATOMIC(2, u_int16_t, add,	16,	+)
IA64_ATOMIC(4, u_int32_t, add,	32,	+)
IA64_ATOMIC(8, u_int64_t, add,	64,	+)

IA64_ATOMIC(1, u_int8_t,  subtract,	8,	-)
IA64_ATOMIC(2, u_int16_t, subtract,	16,	-)
IA64_ATOMIC(4, u_int32_t, subtract,	32,	-)
IA64_ATOMIC(8, u_int64_t, subtract,	64,	-)

#undef IA64_ATOMIC
#undef IA64_CMPXCHG

#define atomic_set_8		atomic_set_acq_8
#define	atomic_clear_8		atomic_clear_acq_8
#define atomic_add_8		atomic_add_acq_8
#define	atomic_subtract_8	atomic_subtract_acq_8

#define atomic_set_16		atomic_set_acq_16
#define	atomic_clear_16		atomic_clear_acq_16
#define atomic_add_16		atomic_add_acq_16
#define	atomic_subtract_16	atomic_subtract_acq_16

#define atomic_set_32		atomic_set_acq_32
#define	atomic_clear_32		atomic_clear_acq_32
#define atomic_add_32		atomic_add_acq_32
#define	atomic_subtract_32	atomic_subtract_acq_32

#define atomic_set_64		atomic_set_acq_64
#define	atomic_clear_64		atomic_clear_acq_64
#define atomic_add_64		atomic_add_acq_64
#define	atomic_subtract_64	atomic_subtract_acq_64

#define atomic_set_char			atomic_set_8
#define atomic_clear_char		atomic_clear_8
#define atomic_add_char			atomic_add_8
#define atomic_subtract_char		atomic_subtract_8
#define atomic_set_acq_char		atomic_set_acq_8
#define atomic_clear_acq_char		atomic_clear_acq_8
#define atomic_add_acq_char		atomic_add_acq_8
#define atomic_subtract_acq_char	atomic_subtract_acq_8
#define atomic_set_rel_char		atomic_set_rel_8
#define atomic_clear_rel_char		atomic_clear_rel_8
#define atomic_add_rel_char		atomic_add_rel_8
#define atomic_subtract_rel_char	atomic_subtract_rel_8

#define atomic_set_short		atomic_set_16
#define atomic_clear_short		atomic_clear_16
#define atomic_add_short		atomic_add_16
#define atomic_subtract_short		atomic_subtract_16
#define atomic_set_acq_short		atomic_set_acq_16
#define atomic_clear_acq_short		atomic_clear_acq_16
#define atomic_add_acq_short		atomic_add_acq_16
#define atomic_subtract_acq_short	atomic_subtract_acq_16
#define atomic_set_rel_short		atomic_set_rel_16
#define atomic_clear_rel_short		atomic_clear_rel_16
#define atomic_add_rel_short		atomic_add_rel_16
#define atomic_subtract_rel_short	atomic_subtract_rel_16

#define atomic_set_int			atomic_set_32
#define atomic_clear_int		atomic_clear_32
#define atomic_add_int			atomic_add_32
#define atomic_subtract_int		atomic_subtract_32
#define atomic_set_acq_int		atomic_set_acq_32
#define atomic_clear_acq_int		atomic_clear_acq_32
#define atomic_add_acq_int		atomic_add_acq_32
#define atomic_subtract_acq_int		atomic_subtract_acq_32
#define atomic_set_rel_int		atomic_set_rel_32
#define atomic_clear_rel_int		atomic_clear_rel_32
#define atomic_add_rel_int		atomic_add_rel_32
#define atomic_subtract_rel_int		atomic_subtract_rel_32

#define atomic_set_long			atomic_set_64
#define atomic_clear_long		atomic_clear_64
#define atomic_add_long			atomic_add_64
#define atomic_subtract_long		atomic_subtract_64
#define atomic_set_acq_long		atomic_set_acq_64
#define atomic_clear_acq_long		atomic_clear_acq_64
#define atomic_add_acq_long		atomic_add_acq_64
#define atomic_subtract_acq_long	atomic_subtract_acq_64
#define atomic_set_rel_long		atomic_set_rel_64
#define atomic_clear_rel_long		atomic_clear_rel_64
#define atomic_add_rel_long		atomic_add_rel_64
#define atomic_subtract_rel_long	atomic_subtract_rel_64

static __inline void
atomic_set_ptr(volatile void *p, u_int64_t v)
{
	atomic_set_64((volatile u_int64_t *) p, v);
}

static __inline void
atomic_clear_ptr(volatile void *p, u_int64_t v)
{
	atomic_clear_64((volatile u_int64_t *) p, v);
}

/*
 * Atomically compare the value stored at *p with cmpval and if the
 * two values are equal, update the value of *p with newval. Returns
 * zero if the compare failed, nonzero otherwise.
 */
static __inline int
atomic_cmpset_acq_32(volatile u_int32_t* p, u_int32_t cmpval, u_int32_t newval)
{
	return ia64_cmpxchg_acq_32(p, cmpval, newval) == cmpval;
}

static __inline int
atomic_cmpset_rel_32(volatile u_int32_t* p, u_int32_t cmpval, u_int32_t newval)
{
	return ia64_cmpxchg_rel_32(p, cmpval, newval) == cmpval;
}

/*
 * Atomically compare the value stored at *p with cmpval and if the
 * two values are equal, update the value of *p with newval. Returns
 * zero if the compare failed, nonzero otherwise.
 */
static __inline int
atomic_cmpset_acq_64(volatile u_int64_t* p, u_int64_t cmpval, u_int64_t newval)
{
	return ia64_cmpxchg_acq_64(p, cmpval, newval) == cmpval;
}

static __inline int
atomic_cmpset_rel_64(volatile u_int64_t* p, u_int64_t cmpval, u_int64_t newval)
{
	return ia64_cmpxchg_rel_64(p, cmpval, newval) == cmpval;
}

#define atomic_cmpset_32	atomic_cmpset_acq_32
#define atomic_cmpset_64	atomic_cmpset_acq_64
#define	atomic_cmpset_int	atomic_cmpset_32
#define	atomic_cmpset_long	atomic_cmpset_64
#define atomic_cmpset_acq_int	atomic_cmpset_acq_32
#define atomic_cmpset_rel_int	atomic_cmpset_rel_32
#define atomic_cmpset_acq_long	atomic_cmpset_acq_64
#define atomic_cmpset_rel_long	atomic_cmpset_rel_64

static __inline int
atomic_cmpset_acq_ptr(volatile void *dst, void *exp, void *src)
{
        return atomic_cmpset_acq_long((volatile u_long *)dst,
				      (u_long)exp, (u_long)src);
}

static __inline int
atomic_cmpset_rel_ptr(volatile void *dst, void *exp, void *src)
{
        return atomic_cmpset_rel_long((volatile u_long *)dst,
				      (u_long)exp, (u_long)src);
}

#define	atomic_cmpset_ptr	atomic_cmpset_acq_ptr

static __inline void *
atomic_load_acq_ptr(volatile void *p)
{
	return (void *)atomic_load_acq_long((volatile u_long *)p);
}

static __inline void
atomic_store_rel_ptr(volatile void *p, void *v)
{
	atomic_store_rel_long((volatile u_long *)p, (u_long)v);
}

static __inline u_int32_t
atomic_readandclear_32(volatile u_int32_t* p)
{
	u_int32_t val;
	do {
		val = *p;
	} while (!atomic_cmpset_32(p, val, 0));
	return val;
}

static __inline u_int64_t
atomic_readandclear_64(volatile u_int64_t* p)
{
	u_int64_t val;
	do {
		val = *p;
	} while (!atomic_cmpset_64(p, val, 0));
	return val;
}

#define atomic_readandclear_int	atomic_readandclear_32
#define atomic_readandclear_long atomic_readandclear_64

#endif /* ! _MACHINE_ATOMIC_H_ */
