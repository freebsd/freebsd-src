/*-
 * Copyright (c) 2013 Ed Schouten <ed@FreeBSD.org>
 * All rights reserved.
 *
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#ifdef _KERNEL
#include "opt_global.h"
#endif

/*
 * Memory barriers.
 *
 * It turns out __sync_synchronize() does not emit any code when used
 * with GCC 4.2. Implement our own version that does work reliably.
 *
 * Although __sync_lock_test_and_set() should only perform an acquire
 * barrier, make it do a full barrier like the other functions. This
 * should make <stdatomic.h>'s atomic_exchange_explicit() work reliably.
 */

static inline void
mips_sync(void)
{

	__asm volatile (
#if !defined(_KERNEL) || defined(SMP)
		".set noreorder\n"
		"\tsync\n"
		"\tnop\n"
		"\tnop\n"
		"\tnop\n"
		"\tnop\n"
		"\tnop\n"
		"\tnop\n"
		"\tnop\n"
		"\tnop\n"
		".set reorder\n"
#else /* _KERNEL && !SMP */
		""
#endif /* !KERNEL || SMP */
		: : : "memory");
}

typedef union {
	uint8_t		v8[4];
	uint32_t	v32;
} reg_t;

static inline uint32_t *
round_to_word(void *ptr)
{

	return ((uint32_t *)((intptr_t)ptr & ~3));
}

/*
 * 8-bit routines.
 */

static inline void
put_1(reg_t *r, uint8_t *offset_ptr, uint8_t val)
{
	size_t offset;

	offset = (intptr_t)offset_ptr & 3;
	r->v8[offset] = val;
}

static inline uint8_t
get_1(const reg_t *r, uint8_t *offset_ptr)
{
	size_t offset;

	offset = (intptr_t)offset_ptr & 3;
	return (r->v8[offset]);
}

uint8_t
__sync_lock_test_and_set_1(uint8_t *mem8, uint8_t val8)
{
	uint32_t *mem32;
	reg_t val32, negmask32, old;
	uint32_t temp;

	mem32 = round_to_word(mem8);
	val32.v32 = 0x00000000;
	put_1(&val32, mem8, val8);
	negmask32.v32 = 0xffffffff;
	put_1(&negmask32, mem8, val8);

	mips_sync();
	__asm volatile (
		"1:"
		"\tll	%0, %5\n"	/* Load old value. */
		"\tand	%2, %4, %0\n"	/* Trim out affected part. */
		"\tor	%2, %3\n"	/* Put in the new value. */
		"\tsc	%2, %1\n"	/* Attempt to store. */
		"\tbeqz	%2, 1b\n"	/* Spin if failed. */
		: "=&r" (old.v32), "=m" (*mem32), "=&r" (temp)
		: "r" (val32.v32), "r" (negmask32.v32), "m" (*mem32));
	return (get_1(&old, mem8));
}

uint8_t
__sync_val_compare_and_swap_1(uint8_t *mem8, uint8_t expected, uint8_t desired)
{
	uint32_t *mem32;
	reg_t expected32, desired32, posmask32, negmask32, old;
	uint32_t temp;

	mem32 = round_to_word(mem8);
	expected32.v32 = 0x00000000;
	put_1(&expected32, mem8, expected);
	desired32.v32 = 0x00000000;
	put_1(&desired32, mem8, desired);
	posmask32.v32 = 0x00000000;
	put_1(&posmask32, mem8, 0xff);
	negmask32.v32 = ~posmask32.v32;

	mips_sync();
	__asm volatile (
		"1:"
		"\tll	%0, %7\n"	/* Load old value. */
		"\tand	%2, %5, %0\n"	/* Isolate affected part. */
		"\tbne	%2, %3, 2f\n"	/* Compare to expected value. */
		"\tand	%2, %6, %0\n"	/* Trim out affected part. */
		"\tor	%2, %4\n"	/* Put in the new value. */
		"\tsc	%2, %1\n"	/* Attempt to store. */
		"\tbeqz	%2, 1b\n"	/* Spin if failed. */
		"2:"
		: "=&r" (old), "=m" (*mem32), "=&r" (temp)
		: "r" (expected32.v32), "r" (desired32.v32),
		  "r" (posmask32.v32), "r" (negmask32.v32), "m" (*mem32));
	return (get_1(&old, mem8));
}

#define	EMIT_ARITHMETIC_FETCH_AND_OP_1(name, op)			\
uint8_t									\
__sync_##name##_1(uint8_t *mem8, uint8_t val8)				\
{									\
	uint32_t *mem32;						\
	reg_t val32, posmask32, negmask32, old;				\
	uint32_t temp1, temp2;						\
									\
	mem32 = round_to_word(mem8);					\
	val32.v32 = 0x00000000;						\
	put_1(&val32, mem8, val8);					\
	posmask32.v32 = 0x00000000;					\
	put_1(&posmask32, mem8, 0xff);					\
	negmask32.v32 = ~posmask32.v32;					\
									\
	mips_sync();							\
	__asm volatile (						\
		"1:"							\
		"\tll	%0, %7\n"	/* Load old value. */		\
		"\t"op"	%2, %0, %4\n"	/* Calculate new value. */	\
		"\tand	%2, %5\n"	/* Isolate affected part. */	\
		"\tand	%3, %6, %0\n"	/* Trim out affected part. */	\
		"\tor	%2, %3\n"	/* Put in the new value. */	\
		"\tsc	%2, %1\n"	/* Attempt to store. */		\
		"\tbeqz	%2, 1b\n"	/* Spin if failed. */		\
		: "=&r" (old.v32), "=m" (*mem32), "=&r" (temp1),	\
		  "=&r" (temp2)						\
		: "r" (val32.v32), "r" (posmask32.v32),			\
		  "r" (negmask32.v32), "m" (*mem32));			\
	return (get_1(&old, mem8));					\
}

EMIT_ARITHMETIC_FETCH_AND_OP_1(fetch_and_add, "addu")
EMIT_ARITHMETIC_FETCH_AND_OP_1(fetch_and_sub, "subu")

#define	EMIT_BITWISE_FETCH_AND_OP_1(name, op, idempotence)		\
uint8_t									\
__sync_##name##_1(uint8_t *mem8, uint8_t val8)				\
{									\
	uint32_t *mem32;						\
	reg_t val32, old;						\
	uint32_t temp;							\
									\
	mem32 = round_to_word(mem8);					\
	val32.v32 = idempotence ? 0xffffffff : 0x00000000;		\
	put_1(&val32, mem8, val8);					\
									\
	mips_sync();							\
	__asm volatile (						\
		"1:"							\
		"\tll	%0, %4\n"	/* Load old value. */		\
		"\t"op"	%2, %3, %0\n"	/* Calculate new value. */	\
		"\tsc	%2, %1\n"	/* Attempt to store. */		\
		"\tbeqz	%2, 1b\n"	/* Spin if failed. */		\
		: "=&r" (old.v32), "=m" (*mem32), "=&r" (temp)		\
		: "r" (val32.v32), "m" (*mem32));			\
	return (get_1(&old, mem8));					\
}

EMIT_BITWISE_FETCH_AND_OP_1(fetch_and_and, "and", 1)
EMIT_BITWISE_FETCH_AND_OP_1(fetch_and_or, "or", 0)
EMIT_BITWISE_FETCH_AND_OP_1(fetch_and_xor, "xor", 0)

/*
 * 16-bit routines.
 */

static inline void
put_2(reg_t *r, uint16_t *offset_ptr, uint16_t val)
{
	size_t offset;
	union {
		uint16_t in;
		uint8_t out[2];
	} bytes;

	offset = (intptr_t)offset_ptr & 3;
	bytes.in = val;
	r->v8[offset] = bytes.out[0];
	r->v8[offset + 1] = bytes.out[1];
}

static inline uint16_t
get_2(const reg_t *r, uint16_t *offset_ptr)
{
	size_t offset;
	union {
		uint8_t in[2];
		uint16_t out;
	} bytes;

	offset = (intptr_t)offset_ptr & 3;
	bytes.in[0] = r->v8[offset];
	bytes.in[1] = r->v8[offset + 1];
	return (bytes.out);
}

uint16_t
__sync_lock_test_and_set_2(uint16_t *mem16, uint16_t val16)
{
	uint32_t *mem32;
	reg_t val32, negmask32, old;
	uint32_t temp;

	mem32 = round_to_word(mem16);
	val32.v32 = 0x00000000;
	put_2(&val32, mem16, val16);
	negmask32.v32 = 0xffffffff;
	put_2(&negmask32, mem16, 0x0000);

	mips_sync();
	__asm volatile (
		"1:"
		"\tll	%0, %5\n"	/* Load old value. */
		"\tand	%2, %4, %0\n"	/* Trim out affected part. */
		"\tor	%2, %3\n"	/* Combine to new value. */
		"\tsc	%2, %1\n"	/* Attempt to store. */
		"\tbeqz	%2, 1b\n"	/* Spin if failed. */
		: "=&r" (old.v32), "=m" (*mem32), "=&r" (temp)
		: "r" (val32.v32), "r" (negmask32.v32), "m" (*mem32));
	return (get_2(&old, mem16));
}

uint16_t
__sync_val_compare_and_swap_2(uint16_t *mem16, uint16_t expected,
    uint16_t desired)
{
	uint32_t *mem32;
	reg_t expected32, desired32, posmask32, negmask32, old;
	uint32_t temp;

	mem32 = round_to_word(mem16);
	expected32.v32 = 0x00000000;
	put_2(&expected32, mem16, expected);
	desired32.v32 = 0x00000000;
	put_2(&desired32, mem16, desired);
	posmask32.v32 = 0x00000000;
	put_2(&posmask32, mem16, 0xffff);
	negmask32.v32 = ~posmask32.v32;

	mips_sync();
	__asm volatile (
		"1:"
		"\tll	%0, %7\n"	/* Load old value. */
		"\tand	%2, %5, %0\n"	/* Isolate affected part. */
		"\tbne	%2, %3, 2f\n"	/* Compare to expected value. */
		"\tand	%2, %6, %0\n"	/* Trim out affected part. */
		"\tor	%2, %4\n"	/* Put in the new value. */
		"\tsc	%2, %1\n"	/* Attempt to store. */
		"\tbeqz	%2, 1b\n"	/* Spin if failed. */
		"2:"
		: "=&r" (old), "=m" (*mem32), "=&r" (temp)
		: "r" (expected32.v32), "r" (desired32.v32),
		  "r" (posmask32.v32), "r" (negmask32.v32), "m" (*mem32));
	return (get_2(&old, mem16));
}

#define	EMIT_ARITHMETIC_FETCH_AND_OP_2(name, op)			\
uint16_t								\
__sync_##name##_2(uint16_t *mem16, uint16_t val16)			\
{									\
	uint32_t *mem32;						\
	reg_t val32, posmask32, negmask32, old;				\
	uint32_t temp1, temp2;						\
									\
	mem32 = round_to_word(mem16);					\
	val32.v32 = 0x00000000;						\
	put_2(&val32, mem16, val16);					\
	posmask32.v32 = 0x00000000;					\
	put_2(&posmask32, mem16, 0xffff);				\
	negmask32.v32 = ~posmask32.v32;					\
									\
	mips_sync();							\
	__asm volatile (						\
		"1:"							\
		"\tll	%0, %7\n"	/* Load old value. */		\
		"\t"op"	%2, %0, %4\n"	/* Calculate new value. */	\
		"\tand	%2, %5\n"	/* Isolate affected part. */	\
		"\tand	%3, %6, %0\n"	/* Trim out affected part. */	\
		"\tor	%2, %3\n"	/* Combine to new value. */	\
		"\tsc	%2, %1\n"	/* Attempt to store. */		\
		"\tbeqz	%2, 1b\n"	/* Spin if failed. */		\
		: "=&r" (old.v32), "=m" (*mem32), "=&r" (temp1),	\
		  "=&r" (temp2)						\
		: "r" (val32.v32), "r" (posmask32.v32),			\
		  "r" (negmask32.v32), "m" (*mem32));			\
	return (get_2(&old, mem16));					\
}

EMIT_ARITHMETIC_FETCH_AND_OP_2(fetch_and_add, "addu")
EMIT_ARITHMETIC_FETCH_AND_OP_2(fetch_and_sub, "subu")

#define	EMIT_BITWISE_FETCH_AND_OP_2(name, op, idempotence)		\
uint16_t								\
__sync_##name##_2(uint16_t *mem16, uint16_t val16)			\
{									\
	uint32_t *mem32;						\
	reg_t val32, old;						\
	uint32_t temp;							\
									\
	mem32 = round_to_word(mem16);					\
	val32.v32 = idempotence ? 0xffffffff : 0x00000000;		\
	put_2(&val32, mem16, val16);					\
									\
	mips_sync();							\
	__asm volatile (						\
		"1:"							\
		"\tll	%0, %4\n"	/* Load old value. */		\
		"\t"op"	%2, %3, %0\n"	/* Calculate new value. */	\
		"\tsc	%2, %1\n"	/* Attempt to store. */		\
		"\tbeqz	%2, 1b\n"	/* Spin if failed. */		\
		: "=&r" (old.v32), "=m" (*mem32), "=&r" (temp)		\
		: "r" (val32.v32), "m" (*mem32));			\
	return (get_2(&old, mem16));					\
}

EMIT_BITWISE_FETCH_AND_OP_2(fetch_and_and, "and", 1)
EMIT_BITWISE_FETCH_AND_OP_2(fetch_and_or, "or", 0)
EMIT_BITWISE_FETCH_AND_OP_2(fetch_and_xor, "xor", 0)

/*
 * 32-bit routines.
 */

uint32_t
__sync_val_compare_and_swap_4(uint32_t *mem, uint32_t expected,
    uint32_t desired)
{
	uint32_t old, temp;

	mips_sync();
	__asm volatile (
		"1:"
		"\tll	%0, %5\n"	/* Load old value. */
		"\tbne	%0, %3, 2f\n"	/* Compare to expected value. */
		"\tmove	%2, %4\n"	/* Value to store. */
		"\tsc	%2, %1\n"	/* Attempt to store. */
		"\tbeqz	%2, 1b\n"	/* Spin if failed. */
		"2:"
		: "=&r" (old), "=m" (*mem), "=&r" (temp)
		: "r" (expected), "r" (desired), "m" (*mem));
	return (old);
}

#define	EMIT_FETCH_AND_OP_4(name, op)					\
uint32_t								\
__sync_##name##_4(uint32_t *mem, uint32_t val)				\
{									\
	uint32_t old, temp;						\
									\
	mips_sync();							\
	__asm volatile (						\
		"1:"							\
		"\tll	%0, %4\n"	/* Load old value. */		\
		"\t"op"\n"		/* Calculate new value. */	\
		"\tsc	%2, %1\n"	/* Attempt to store. */		\
		"\tbeqz	%2, 1b\n"	/* Spin if failed. */		\
		: "=&r" (old), "=m" (*mem), "=&r" (temp)		\
		: "r" (val), "m" (*mem));				\
	return (old);							\
}

EMIT_FETCH_AND_OP_4(lock_test_and_set, "move %2, %3")
EMIT_FETCH_AND_OP_4(fetch_and_add, "addu %2, %0, %3")
EMIT_FETCH_AND_OP_4(fetch_and_and, "and %2, %0, %3")
EMIT_FETCH_AND_OP_4(fetch_and_or, "or %2, %0, %3")
EMIT_FETCH_AND_OP_4(fetch_and_sub, "subu %2, %0, %3")
EMIT_FETCH_AND_OP_4(fetch_and_xor, "xor %2, %0, %3")

/*
 * 64-bit routines.
 *
 * Note: All the 64-bit atomic operations are only atomic when running
 * in 64-bit mode. It is assumed that code compiled for n32 and n64 fits
 * into this definition and no further safeties are needed.
 */

#if defined(__mips_n32) || defined(__mips_n64)

uint64_t
__sync_val_compare_and_swap_8(uint64_t *mem, uint64_t expected,
    uint64_t desired)
{
	uint64_t old, temp;

	mips_sync();
	__asm volatile (
		"1:"
		"\tlld	%0, %5\n"	/* Load old value. */
		"\tbne	%0, %3, 2f\n"	/* Compare to expected value. */
		"\tmove	%2, %4\n"	/* Value to store. */
		"\tscd	%2, %1\n"	/* Attempt to store. */
		"\tbeqz	%2, 1b\n"	/* Spin if failed. */
		"2:"
		: "=&r" (old), "=m" (*mem), "=&r" (temp)
		: "r" (expected), "r" (desired), "m" (*mem));
	return (old);
}

#define	EMIT_FETCH_AND_OP_8(name, op)					\
uint64_t								\
__sync_##name##_8(uint64_t *mem, uint64_t val)				\
{									\
	uint64_t old, temp;						\
									\
	mips_sync();							\
	__asm volatile (						\
		"1:"							\
		"\tlld	%0, %4\n"	/* Load old value. */		\
		"\t"op"\n"		/* Calculate new value. */	\
		"\tscd	%2, %1\n"	/* Attempt to store. */		\
		"\tbeqz	%2, 1b\n"	/* Spin if failed. */		\
		: "=&r" (old), "=m" (*mem), "=&r" (temp)		\
		: "r" (val), "m" (*mem));				\
	return (old);							\
}

EMIT_FETCH_AND_OP_8(lock_test_and_set, "move %2, %3")
EMIT_FETCH_AND_OP_8(fetch_and_add, "daddu %2, %0, %3")
EMIT_FETCH_AND_OP_8(fetch_and_and, "and %2, %0, %3")
EMIT_FETCH_AND_OP_8(fetch_and_or, "or %2, %0, %3")
EMIT_FETCH_AND_OP_8(fetch_and_sub, "dsubu %2, %0, %3")
EMIT_FETCH_AND_OP_8(fetch_and_xor, "xor %2, %0, %3")

#endif /* __mips_n32 || __mips_n64 */
