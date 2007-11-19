/*-
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

#include <machine/cpufunc.h>

#ifndef _SYS_CDEFS_H_
#error this file needs sys/cdefs.h as a prerequisite
#endif

/*
 * Various simple arithmetic on memory which is atomic in the presence
 * of interrupts and SMP safe.
 */

void	atomic_set_8(volatile uint8_t *, uint8_t);
void	atomic_clear_8(volatile uint8_t *, uint8_t);
void	atomic_add_8(volatile uint8_t *, uint8_t);
void	atomic_subtract_8(volatile uint8_t *, uint8_t);

void	atomic_set_16(volatile uint16_t *, uint16_t);
void	atomic_clear_16(volatile uint16_t *, uint16_t);
void	atomic_add_16(volatile uint16_t *, uint16_t);
void	atomic_subtract_16(volatile uint16_t *, uint16_t);

static __inline void
atomic_set_32(volatile uint32_t *p, uint32_t v)
{
	uint32_t temp;

#ifdef __GNUCLIKE_ASM
	__asm __volatile (
		"1:\tlwarx %0, 0, %2\n\t"	/* load old value */
		"or %0, %3, %0\n\t"		/* calculate new value */
		"stwcx. %0, 0, %2\n\t"      	/* attempt to store */
		"bne- 1b\n\t"			/* spin if failed */
		: "=&r" (temp), "=m" (*p)
		: "r" (p), "r" (v), "m" (*p)
		: "cc", "memory");
#endif
}

static __inline void
atomic_clear_32(volatile uint32_t *p, uint32_t v)
{
	uint32_t temp;

#ifdef __GNUCLIKE_ASM
	__asm __volatile (
		"1:\tlwarx %0, 0, %2\n\t"	/* load old value */
		"andc %0, %0, %3\n\t"		/* calculate new value */
		"stwcx. %0, 0, %2\n\t"      	/* attempt to store */
		"bne- 1b\n\t"			/* spin if failed */
		: "=&r" (temp), "=m" (*p)
		: "r" (p), "r" (v), "m" (*p)
		: "cc", "memory");
#endif
}

static __inline void
atomic_add_32(volatile uint32_t *p, uint32_t v)
{
	uint32_t temp;

#ifdef __GNUCLIKE_ASM
	__asm __volatile (
		"1:\tlwarx %0, 0, %2\n\t"	/* load old value */
		"add %0, %3, %0\n\t"		/* calculate new value */
		"stwcx. %0, 0, %2\n\t"      	/* attempt to store */
		"bne- 1b\n\t"			/* spin if failed */
		: "=&r" (temp), "=m" (*p)
		: "r" (p), "r" (v), "m" (*p)
		: "cc", "memory");
#endif
}

static __inline void
atomic_subtract_32(volatile uint32_t *p, uint32_t v)
{
	uint32_t temp;

#ifdef __GNUCLIKE_ASM
	__asm __volatile (
		"1:\tlwarx %0, 0, %2\n\t"	/* load old value */
		"subf %0, %3, %0\n\t"		/* calculate new value */
		"stwcx. %0, 0, %2\n\t"      	/* attempt to store */
		"bne- 1b\n\t"			/* spin if failed */
		: "=&r" (temp), "=m" (*p)
		: "r" (p), "r" (v), "m" (*p)
		: "cc", "memory");
#endif
}

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

#if 0

/*
 * So far I haven't found a way to implement atomic 64-bit ops on the
 * 32-bit PowerPC without involving major headaches.  If anyone has
 * any ideas, please let me know. =)
 * 	- benno@FreeBSD.org
 */

static __inline void
atomic_set_64(volatile u_int64_t *p, u_int64_t v)
{
	u_int64_t temp;

	__asm __volatile (
		: "=&r" (temp), "=r" (*p)
		: "r" (*p), "r" (v)
		: "memory");
}

static __inline void
atomic_clear_64(volatile u_int64_t *p, u_int64_t v)
{
	u_int64_t temp;

	__asm __volatile (
		: "=&r" (temp), "=r" (*p)
		: "r" (*p), "r" (v)
		: "memory");
}

static __inline void
atomic_add_64(volatile u_int64_t *p, u_int64_t v)
{
	u_int64_t temp;

	__asm __volatile (
		: "=&r" (temp), "=r" (*p)
		: "r" (*p), "r" (v)
		: "memory");
}

static __inline void
atomic_subtract_64(volatile u_int64_t *p, u_int64_t v)
{
	u_int64_t temp;

	__asm __volatile (
		: "=&r" (temp), "=r" (*p)
		: "r" (*p), "r" (v)
		: "memory");
}

static __inline u_int64_t
atomic_readandclear_64(volatile u_int64_t *addr)
{
	u_int64_t result,temp;

	__asm __volatile (
		: "=&r"(result), "=&r"(temp), "=r" (*addr)
		: "r"(*addr)
		: "memory");

	return result;
}

#endif /* 0 */

#define	atomic_set_char			atomic_set_8
#define	atomic_clear_char		atomic_clear_8
#define	atomic_add_char			atomic_add_8
#define	atomic_subtract_char		atomic_subtract_8

#define	atomic_set_short		atomic_set_16
#define	atomic_clear_short		atomic_clear_16
#define	atomic_add_short		atomic_add_16
#define	atomic_subtract_short		atomic_subtract_16

#define	atomic_set_int			atomic_set_32
#define	atomic_clear_int		atomic_clear_32
#define	atomic_add_int			atomic_add_32
#define	atomic_subtract_int		atomic_subtract_32
#define	atomic_readandclear_int		atomic_readandclear_32

#define	atomic_set_long			atomic_set_32
#define	atomic_clear_long		atomic_clear_32
#define	atomic_add_long(p, v)		atomic_add_32((uint32_t *)p, (uint32_t)v)
#define	atomic_subtract_long(p, v)     	atomic_subtract_32((uint32_t *)p, (uint32_t)v)
#define	atomic_readandclear_long	atomic_readandclear_32

#define	atomic_set_ptr			atomic_set_32
#define	atomic_clear_ptr		atomic_clear_32
#define	atomic_add_ptr			atomic_add_32
#define	atomic_subtract_ptr     	atomic_subtract_32

#if 0

/* See above. */

#define	atomic_set_long_long		atomic_set_64
#define	atomic_clear_long_long		atomic_clear_64
#define	atomic_add_long_long		atomic_add_64
#define	atomic_subtract_long_long	atomic_subtract_64
#define	atomic_readandclear_long_long	atomic_readandclear_64

#endif /* 0 */

#define	ATOMIC_ACQ_REL(NAME, WIDTH, TYPE)				\
static __inline void							\
atomic_##NAME##_acq_##WIDTH(volatile u_int##WIDTH##_t *p, u_int##WIDTH##_t v) \
{									\
	atomic_##NAME##_##WIDTH(p, v);					\
	powerpc_mb();							\
}									\
									\
static __inline void							\
atomic_##NAME##_rel_##WIDTH(volatile u_int##WIDTH##_t *p, u_int##WIDTH##_t v) \
{									\
	powerpc_mb();							\
	atomic_##NAME##_##WIDTH(p, v);					\
}									\
									\
static __inline void							\
atomic_##NAME##_acq_##TYPE(volatile u_int##WIDTH##_t *p, u_int##WIDTH##_t v) \
{									\
	atomic_##NAME##_##WIDTH(p, v);					\
	powerpc_mb();							\
}									\
									\
static __inline void							\
atomic_##NAME##_rel_##TYPE(volatile u_int##WIDTH##_t *p, u_int##WIDTH##_t v) \
{									\
	powerpc_mb();							\
	atomic_##NAME##_##WIDTH(p, v);					\
}

ATOMIC_ACQ_REL(set, 8, char)
ATOMIC_ACQ_REL(clear, 8, char)
ATOMIC_ACQ_REL(add, 8, char)
ATOMIC_ACQ_REL(subtract, 8, char)
ATOMIC_ACQ_REL(set, 16, short)
ATOMIC_ACQ_REL(clear, 16, short)
ATOMIC_ACQ_REL(add, 16, short)
ATOMIC_ACQ_REL(subtract, 16, short)
ATOMIC_ACQ_REL(set, 32, int)
ATOMIC_ACQ_REL(clear, 32, int)
ATOMIC_ACQ_REL(add, 32, int)
ATOMIC_ACQ_REL(subtract, 32, int)

#define	atomic_set_acq_long		atomic_set_acq_32
#define	atomic_set_rel_long		atomic_set_rel_32
#define	atomic_clear_acq_long		atomic_clear_acq_32
#define	atomic_clear_rel_long		atomic_clear_rel_32
#define	atomic_add_acq_long		atomic_add_acq_32
#define	atomic_add_rel_long		atomic_add_rel_32
#define	atomic_subtract_acq_long	atomic_subtract_acq_32
#define	atomic_subtract_rel_long	atomic_subtract_rel_32

#define	atomic_set_acq_ptr		atomic_set_acq_32
#define	atomic_set_rel_ptr		atomic_set_rel_32
#define	atomic_clear_acq_ptr		atomic_clear_acq_32
#define	atomic_clear_rel_ptr		atomic_clear_rel_32
#define	atomic_add_acq_ptr		atomic_add_acq_32
#define	atomic_add_rel_ptr		atomic_add_rel_32
#define	atomic_subtract_acq_ptr		atomic_subtract_acq_32
#define	atomic_subtract_rel_ptr		atomic_subtract_rel_32

#undef ATOMIC_ACQ_REL

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
	powerpc_mb();						\
	return (v);						\
}								\
								\
static __inline void						\
atomic_store_rel_##WIDTH(volatile u_##TYPE *p, u_##TYPE v)	\
{								\
	powerpc_mb();						\
	*p = v;							\
}								\
								\
static __inline u_##TYPE					\
atomic_load_acq_##TYPE(volatile u_##TYPE *p)			\
{								\
	u_##TYPE v;						\
								\
	v = *p;							\
	powerpc_mb();						\
	return (v);						\
}								\
								\
static __inline void						\
atomic_store_rel_##TYPE(volatile u_##TYPE *p, u_##TYPE v)	\
{								\
	powerpc_mb();						\
	*p = v;							\
}

ATOMIC_STORE_LOAD(char,		8)
ATOMIC_STORE_LOAD(short,	16)
ATOMIC_STORE_LOAD(int,		32)

#define	atomic_load_acq_long	atomic_load_acq_32
#define	atomic_store_rel_long	atomic_store_rel_32

#define	atomic_load_acq_ptr	atomic_load_acq_32
#define	atomic_store_rel_ptr	atomic_store_rel_32

#undef ATOMIC_STORE_LOAD

/*
 * Atomically compare the value stored at *p with cmpval and if the
 * two values are equal, update the value of *p with newval. Returns
 * zero if the compare failed, nonzero otherwise.
 */
static __inline uint32_t
atomic_cmpset_32(volatile uint32_t* p, uint32_t cmpval, uint32_t newval)
{
	uint32_t	ret;

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

#if 0

/*
 * Atomically compare the value stored at *p with cmpval and if the
 * two values are equal, update the value of *p with newval. Returns
 * zero if the compare failed, nonzero otherwise.
 */
static __inline u_int64_t
atomic_cmpset_64(volatile u_int64_t* p, u_int64_t cmpval, u_int64_t newval)
{
	u_int64_t ret;

	__asm __volatile (
		: "=&r" (ret), "=r" (*p)
		: "r" (cmpval), "r" (newval), "r" (*p)
		: "memory");

	return ret;
}

#endif /* 0 */

#define	atomic_cmpset_int	atomic_cmpset_32
#define	atomic_cmpset_long	atomic_cmpset_32

#define	atomic_cmpset_ptr(dst, old, new)	\
    atomic_cmpset_32((volatile u_int *)(dst), (u_int)(old), (u_int)(new))

#if 0
#define	atomic_cmpset_long_long	atomic_cmpset_64
#endif /* 0 */

static __inline uint32_t
atomic_cmpset_acq_32(volatile uint32_t *p, uint32_t cmpval, uint32_t newval)
{
	int retval;

	retval = atomic_cmpset_32(p, cmpval, newval);
	powerpc_mb();
	return (retval);
}

static __inline uint32_t
atomic_cmpset_rel_32(volatile uint32_t *p, uint32_t cmpval, uint32_t newval)
{
	powerpc_mb();
	return (atomic_cmpset_32(p, cmpval, newval));
}

#define	atomic_cmpset_acq_int	atomic_cmpset_acq_32
#define	atomic_cmpset_rel_int	atomic_cmpset_rel_32
#define	atomic_cmpset_acq_long(dst, old, new)	\
    atomic_cmpset_acq_32((volatile u_int *)(dst), (u_int)(old), (u_int)(new))
#define	atomic_cmpset_rel_long(dst, old, new)	\
    atomic_cmpset_rel_32((volatile u_int *)(dst), (u_int)(old), (u_int)(new))

#define	atomic_cmpset_acq_ptr(dst, old, new)	\
    atomic_cmpset_acq_32((volatile u_int *)(dst), (u_int)(old), (u_int)(new))
#define	atomic_cmpset_rel_ptr(dst, old, new)	\
    atomic_cmpset_rel_32((volatile u_int *)(dst), (u_int)(old), (u_int)(new))

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

#endif /* ! _MACHINE_ATOMIC_H_ */
