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

#include <machine/alpha_cpu.h>

/*
 * Various simple arithmetic on memory which is atomic in the presence
 * of interrupts and SMP safe.
 */

void atomic_set_8(volatile u_int8_t *, u_int8_t);
void atomic_clear_8(volatile u_int8_t *, u_int8_t);
void atomic_add_8(volatile u_int8_t *, u_int8_t);
void atomic_subtract_8(volatile u_int8_t *, u_int8_t);

void atomic_set_16(volatile u_int16_t *, u_int16_t);
void atomic_clear_16(volatile u_int16_t *, u_int16_t);
void atomic_add_16(volatile u_int16_t *, u_int16_t);
void atomic_subtract_16(volatile u_int16_t *, u_int16_t);

static __inline void atomic_set_32(volatile u_int32_t *p, u_int32_t v)
{
	u_int32_t temp;

	__asm __volatile (
		"1:\tldl_l %0, %2\n\t"		/* load old value */
		"bis %0, %3, %0\n\t"		/* calculate new value */
		"stl_c %0, %1\n\t"		/* attempt to store */
		"beq %0, 2f\n\t"		/* spin if failed */
		"mb\n\t"			/* drain to memory */
		".section .text3,\"ax\"\n"	/* improve branch prediction */
		"2:\tbr 1b\n"			/* try again */
		".previous\n"
		: "=&r" (temp), "=m" (*p)
		: "m" (*p), "r" (v)
		: "memory");
}

static __inline void atomic_clear_32(volatile u_int32_t *p, u_int32_t v)
{
	u_int32_t temp;

	__asm __volatile (
		"1:\tldl_l %0, %2\n\t"		/* load old value */
		"bic %0, %3, %0\n\t"		/* calculate new value */
		"stl_c %0, %1\n\t"		/* attempt to store */
		"beq %0, 2f\n\t"		/* spin if failed */
		"mb\n\t"			/* drain to memory */
		".section .text3,\"ax\"\n"	/* improve branch prediction */
		"2:\tbr 1b\n"			/* try again */
		".previous\n"
		: "=&r" (temp), "=m" (*p)
		: "m" (*p), "r" (v)
		: "memory");
}

static __inline void atomic_add_32(volatile u_int32_t *p, u_int32_t v)
{
	u_int32_t temp;

	__asm __volatile (
		"1:\tldl_l %0, %2\n\t"		/* load old value */
		"addl %0, %3, %0\n\t"		/* calculate new value */
		"stl_c %0, %1\n\t"		/* attempt to store */
		"beq %0, 2f\n\t"		/* spin if failed */
		"mb\n\t"			/* drain to memory */
		".section .text3,\"ax\"\n"	/* improve branch prediction */
		"2:\tbr 1b\n"			/* try again */
		".previous\n"
		: "=&r" (temp), "=m" (*p)
		: "m" (*p), "r" (v)
		: "memory");
}

static __inline void atomic_subtract_32(volatile u_int32_t *p, u_int32_t v)
{
	u_int32_t temp;

	__asm __volatile (
		"1:\tldl_l %0, %2\n\t"		/* load old value */
		"subl %0, %3, %0\n\t"		/* calculate new value */
		"stl_c %0, %1\n\t"		/* attempt to store */
		"beq %0, 2f\n\t"		/* spin if failed */
		"mb\n\t"			/* drain to memory */
		".section .text3,\"ax\"\n"	/* improve branch prediction */
		"2:\tbr 1b\n"			/* try again */
		".previous\n"
		: "=&r" (temp), "=m" (*p)
		: "m" (*p), "r" (v)
		: "memory");
}

static __inline u_int32_t atomic_readandclear_32(volatile u_int32_t *addr)
{
	u_int32_t result,temp;

	__asm __volatile (
		"wmb\n"			/* ensure pending writes have drained */
		"1:\tldl_l %0,%3\n\t"	/* load current value, asserting lock */
		"ldiq %1,0\n\t"		/* value to store */
		"stl_c %1,%2\n\t"	/* attempt to store */
		"beq %1,2f\n\t"		/* if the store failed, spin */
		"br 3f\n"		/* it worked, exit */
		"2:\tbr 1b\n"		/* *addr not updated, loop */
		"3:\tmb\n"		/* it worked */
		: "=&r"(result), "=&r"(temp), "=m" (*addr)
		: "m"(*addr)
		: "memory");

	return result;
}

static __inline void atomic_set_64(volatile u_int64_t *p, u_int64_t v)
{
	u_int64_t temp;

	__asm __volatile (
		"1:\tldq_l %0, %2\n\t"		/* load old value */
		"bis %0, %3, %0\n\t"		/* calculate new value */
		"stq_c %0, %1\n\t"		/* attempt to store */
		"beq %0, 2f\n\t"		/* spin if failed */
		"mb\n\t"			/* drain to memory */
		".section .text3,\"ax\"\n"	/* improve branch prediction */
		"2:\tbr 1b\n"			/* try again */
		".previous\n"
		: "=&r" (temp), "=m" (*p)
		: "m" (*p), "r" (v)
		: "memory");
}

static __inline void atomic_clear_64(volatile u_int64_t *p, u_int64_t v)
{
	u_int64_t temp;

	__asm __volatile (
		"1:\tldq_l %0, %2\n\t"		/* load old value */
		"bic %0, %3, %0\n\t"		/* calculate new value */
		"stq_c %0, %1\n\t"		/* attempt to store */
		"beq %0, 2f\n\t"		/* spin if failed */
		"mb\n\t"			/* drain to memory */
		".section .text3,\"ax\"\n"	/* improve branch prediction */
		"2:\tbr 1b\n"			/* try again */
		".previous\n"
		: "=&r" (temp), "=m" (*p)
		: "m" (*p), "r" (v)
		: "memory");
}

static __inline void atomic_add_64(volatile u_int64_t *p, u_int64_t v)
{
	u_int64_t temp;

	__asm __volatile (
		"1:\tldq_l %0, %2\n\t"		/* load old value */
		"addq %0, %3, %0\n\t"		/* calculate new value */
		"stq_c %0, %1\n\t"		/* attempt to store */
		"beq %0, 2f\n\t"		/* spin if failed */
		"mb\n\t"			/* drain to memory */
		".section .text3,\"ax\"\n"	/* improve branch prediction */
		"2:\tbr 1b\n"			/* try again */
		".previous\n"
		: "=&r" (temp), "=m" (*p)
		: "m" (*p), "r" (v)
		: "memory");
}

static __inline void atomic_subtract_64(volatile u_int64_t *p, u_int64_t v)
{
	u_int64_t temp;

	__asm __volatile (
		"1:\tldq_l %0, %2\n\t"		/* load old value */
		"subq %0, %3, %0\n\t"		/* calculate new value */
		"stq_c %0, %1\n\t"		/* attempt to store */
		"beq %0, 2f\n\t"		/* spin if failed */
		"mb\n\t"			/* drain to memory */
		".section .text3,\"ax\"\n"	/* improve branch prediction */
		"2:\tbr 1b\n"			/* try again */
		".previous\n"
		: "=&r" (temp), "=m" (*p)
		: "m" (*p), "r" (v)
		: "memory");
}

static __inline u_int64_t atomic_readandclear_64(volatile u_int64_t *addr)
{
	u_int64_t result,temp;

	__asm __volatile (
		"wmb\n"			/* ensure pending writes have drained */
		"1:\tldq_l %0,%3\n\t"	/* load current value, asserting lock */
		"ldiq %1,0\n\t"		/* value to store */
		"stq_c %1,%2\n\t"	/* attempt to store */
		"beq %1,2f\n\t"		/* if the store failed, spin */
		"br 3f\n"		/* it worked, exit */
		"2:\tbr 1b\n"		/* *addr not updated, loop */
		"3:\tmb\n"		/* it worked */
		: "=&r"(result), "=&r"(temp), "=m" (*addr)
		: "m"(*addr)
		: "memory");

	return result;
}

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
#define atomic_readandclear_int	atomic_readandclear_32

#define atomic_set_long		atomic_set_64
#define atomic_clear_long	atomic_clear_64
#define atomic_add_long		atomic_add_64
#define atomic_subtract_long	atomic_subtract_64
#define atomic_readandclear_long	atomic_readandclear_64

#define ATOMIC_ACQ_REL(NAME, WIDTH, TYPE)				\
static __inline void							\
atomic_##NAME##_acq_##WIDTH(volatile u_int##WIDTH##_t *p, u_int##WIDTH##_t v)\
{									\
	alpha_mb();							\
	atomic_##NAME##_##WIDTH(p, v);					\
}									\
									\
static __inline void							\
atomic_##NAME##_rel_##WIDTH(volatile u_int##WIDTH##_t *p, u_int##WIDTH##_t v)\
{									\
	atomic_##NAME##_##WIDTH(p, v);					\
	alpha_wmb();							\
}									\
									\
static __inline void							\
atomic_##NAME##_acq_##TYPE(volatile u_int##WIDTH##_t *p, u_int##WIDTH##_t v)\
{									\
	alpha_mb();							\
	atomic_##NAME##_##WIDTH(p, v);					\
}									\
									\
static __inline void							\
atomic_##NAME##_rel_##TYPE(volatile u_int##WIDTH##_t *p, u_int##WIDTH##_t v)\
{									\
	atomic_##NAME##_##WIDTH(p, v);					\
	alpha_wmb();							\
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
ATOMIC_ACQ_REL(set, 64, long)
ATOMIC_ACQ_REL(clear, 64, long)
ATOMIC_ACQ_REL(add, 64, long)
ATOMIC_ACQ_REL(subtract, 64, long)

#undef ATOMIC_ACQ_REL

/*
 * We assume that a = b will do atomic loads and stores.
 */
#define ATOMIC_STORE_LOAD(TYPE, WIDTH)			\
static __inline u_##TYPE				\
atomic_load_acq_##WIDTH(volatile u_##TYPE *p)		\
{							\
	alpha_mb();					\
	return (*p);					\
}							\
							\
static __inline void					\
atomic_store_rel_##WIDTH(volatile u_##TYPE *p, u_##TYPE v)\
{							\
	*p = v;						\
	alpha_wmb();					\
}							\
static __inline u_##TYPE				\
atomic_load_acq_##TYPE(volatile u_##TYPE *p)		\
{							\
	alpha_mb();					\
	return (*p);					\
}							\
							\
static __inline void					\
atomic_store_rel_##TYPE(volatile u_##TYPE *p, u_##TYPE v)\
{							\
	*p = v;						\
	alpha_wmb();					\
}

ATOMIC_STORE_LOAD(char,		8)
ATOMIC_STORE_LOAD(short,	16)
ATOMIC_STORE_LOAD(int,		32)
ATOMIC_STORE_LOAD(long,		64)

#undef ATOMIC_STORE_LOAD

/*
 * Atomically compare the value stored at *p with cmpval and if the
 * two values are equal, update the value of *p with newval. Returns
 * zero if the compare failed, nonzero otherwise.
 */
static __inline u_int32_t
atomic_cmpset_32(volatile u_int32_t* p, u_int32_t cmpval, u_int32_t newval)
{
	u_int32_t ret;

	__asm __volatile (
		"1:\tldl_l %0, %4\n\t"		/* load old value */
		"cmpeq %0, %2, %0\n\t"		/* compare */
		"beq %0, 2f\n\t"		/* exit if not equal */
		"mov %3, %0\n\t"		/* value to store */
		"stl_c %0, %1\n\t"		/* attempt to store */
		"beq %0, 3f\n\t"		/* if it failed, spin */
		"2:\n"				/* done */
		".section .text3,\"ax\"\n"	/* improve branch prediction */
		"3:\tbr 1b\n"			/* try again */
		".previous\n"
		: "=&r" (ret), "=m" (*p)
		: "r" (cmpval), "r" (newval), "m" (*p)
		: "memory");

	return ret;
}

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
		"1:\tldq_l %0, %4\n\t"		/* load old value */
		"cmpeq %0, %2, %0\n\t"		/* compare */
		"beq %0, 2f\n\t"		/* exit if not equal */
		"mov %3, %0\n\t"		/* value to store */
		"stq_c %0, %1\n\t"		/* attempt to store */
		"beq %0, 3f\n\t"		/* if it failed, spin */
		"2:\n"				/* done */
		".section .text3,\"ax\"\n"	/* improve branch prediction */
		"3:\tbr 1b\n"			/* try again */
		".previous\n"
		: "=&r" (ret), "=m" (*p)
		: "r" (cmpval), "r" (newval), "m" (*p)
		: "memory");

	return ret;
}

#define	atomic_cmpset_int	atomic_cmpset_32
#define	atomic_cmpset_long	atomic_cmpset_64

static __inline int
atomic_cmpset_ptr(volatile void *dst, void *exp, void *src)
{

        return (atomic_cmpset_long((volatile u_long *)dst, (u_long)exp,
            (u_long)src));
}

static __inline u_int32_t
atomic_cmpset_acq_32(volatile u_int32_t *p, u_int32_t cmpval, u_int32_t newval)
{
	alpha_mb();
	return (atomic_cmpset_32(p, cmpval, newval));
}

static __inline u_int32_t
atomic_cmpset_rel_32(volatile u_int32_t *p, u_int32_t cmpval, u_int32_t newval)
{
	int retval;

	retval = atomic_cmpset_32(p, cmpval, newval);
	alpha_wmb();
	return (retval);
}

static __inline u_int64_t
atomic_cmpset_acq_64(volatile u_int64_t *p, u_int64_t cmpval, u_int64_t newval)
{
	alpha_mb();
	return (atomic_cmpset_64(p, cmpval, newval));
}

static __inline u_int64_t
atomic_cmpset_rel_64(volatile u_int64_t *p, u_int64_t cmpval, u_int64_t newval)
{
	int retval;

	retval = atomic_cmpset_64(p, cmpval, newval);
	alpha_wmb();
	return (retval);
}

static __inline int
atomic_cmpset_acq_ptr(volatile void *dst, void *exp, void *src)
{

        return (atomic_cmpset_acq_long((volatile u_long *)dst, (u_long)exp,
	    (u_long)src));
}

static __inline int
atomic_cmpset_rel_ptr(volatile void *dst, void *exp, void *src)
{

        return (atomic_cmpset_rel_long((volatile u_long *)dst, (u_long)exp,
            (u_long)src));
}

#define	atomic_cmpset_acq_int	atomic_cmpset_acq_32
#define	atomic_cmpset_rel_int	atomic_cmpset_rel_32
#define	atomic_cmpset_acq_long	atomic_cmpset_acq_64
#define	atomic_cmpset_rel_long	atomic_cmpset_rel_64

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

#define ATOMIC_PTR(NAME)				\
static __inline void					\
atomic_##NAME##_ptr(volatile void *p, uintptr_t v)	\
{							\
	atomic_##NAME##_long((volatile u_long *)p, v);	\
}							\
							\
static __inline void					\
atomic_##NAME##_acq_ptr(volatile void *p, uintptr_t v)	\
{							\
	alpha_mb();					\
	atomic_##NAME##_acq_long((volatile u_long *)p, v);\
}							\
							\
static __inline void					\
atomic_##NAME##_rel_ptr(volatile void *p, uintptr_t v)	\
{							\
	atomic_##NAME##_rel_long((volatile u_long *)p, v);\
	alpha_wmb();					\
}

ATOMIC_PTR(set)
ATOMIC_PTR(clear)
ATOMIC_PTR(add)
ATOMIC_PTR(subtract)

#undef ATOMIC_PTR

#endif /* ! _MACHINE_ATOMIC_H_ */
