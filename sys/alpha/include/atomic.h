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

        return (
            atomic_cmpset_long((volatile u_long *)dst, (u_long)exp, (u_long)src));
}

#endif /* ! _MACHINE_ATOMIC_H_ */
