/*-
 * Copyright (c) 2013 Ed Schouten <ed@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/stdatomic.h>
#include <sys/types.h>

#include <machine/cpufunc.h>

#ifdef _KERNEL
#include "opt_global.h"
#endif

/*
 * Executing statements with interrupts disabled.
 */

#ifndef SMP
#define	WITHOUT_INTERRUPTS(s) do {					\
	register_t regs;						\
									\
	regs = intr_disable();						\
	do s while (0);							\
	intr_restore(regs);						\
} while (0)
#endif /* !SMP */

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
do_sync(void)
{

#if defined(_KERNEL) && !defined(SMP)
	__asm volatile ("" : : : "memory");
#elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__)
	__asm volatile ("dmb" : : : "memory");
#else /* __ARM_ARCH_6__ */
	__asm volatile ("mcr p15, 0, %0, c7, c10, 5" : : "r" (0) : "memory");
#endif
}

#if defined(__CLANG_ATOMICS) || defined(__GNUC_ATOMICS)

/*
 * New C11 __atomic_* API.
 */

#if defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || \
    defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || \
    defined(__ARM_ARCH_6ZK__) || \
    defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__)

/* These systems should be supported by the compiler. */

#elif defined(_KERNEL)

#ifdef SMP
#error "On SMP systems we should have proper atomic operations."
#endif

/* Clang doesn't allow us to reimplement builtins without this. */
#ifdef __clang__
#pragma redefine_extname __sync_synchronize_ext __sync_synchronize
#define __sync_synchronize __sync_synchronize_ext
#endif

void
__sync_synchronize(void)
{

	do_sync();
}

/*
 * On uniprocessor systems, we can perform the atomic operations by
 * disabling interrupts.
 */

#define	EMIT_LOAD_N(N, uintN_t)						\
uintN_t									\
__atomic_load_##N(uintN_t *mem, int model __unused)			\
{									\
	uintN_t ret;							\
									\
	WITHOUT_INTERRUPTS({						\
		ret = *mem;						\
	});								\
	return (ret);							\
}

#define	EMIT_STORE_N(N, uintN_t)					\
void									\
__atomic_store_##N(uintN_t *mem, uintN_t val, int model __unused)	\
{									\
									\
	WITHOUT_INTERRUPTS({						\
		*mem = val;						\
	});								\
}

#define	EMIT_COMPARE_EXCHANGE_N(N, uintN_t)				\
_Bool									\
__atomic_compare_exchange_##N(uintN_t *mem, uintN_t *expected,		\
    uintN_t desired, int success __unused, int failure __unused)	\
{									\
	_Bool ret;							\
									\
	WITHOUT_INTERRUPTS({						\
		if (*mem == *expected) {				\
			*mem = desired;					\
			ret = 1;					\
		} else {						\
			*expected = *mem;				\
			ret = 0;					\
		}							\
	});								\
	return (ret);							\
}

#define	EMIT_FETCH_OP_N(N, uintN_t, name, op)				\
uintN_t									\
__atomic_##name##_##N(uintN_t *mem, uintN_t val, int model __unused)	\
{									\
	uintN_t ret;							\
									\
	WITHOUT_INTERRUPTS({						\
		ret = *mem;						\
		*mem op val;						\
	});								\
	return (ret);							\
}

#define	EMIT_ALL_OPS_N(N, uintN_t)					\
EMIT_LOAD_N(N, uintN_t)							\
EMIT_STORE_N(N, uintN_t)						\
EMIT_COMPARE_EXCHANGE_N(N, uintN_t)					\
EMIT_FETCH_OP_N(N, uintN_t, exchange, =)				\
EMIT_FETCH_OP_N(N, uintN_t, fetch_add, +=)				\
EMIT_FETCH_OP_N(N, uintN_t, fetch_and, &=)				\
EMIT_FETCH_OP_N(N, uintN_t, fetch_or, |=)				\
EMIT_FETCH_OP_N(N, uintN_t, fetch_sub, -=)				\
EMIT_FETCH_OP_N(N, uintN_t, fetch_xor, ^=)

EMIT_ALL_OPS_N(1, uint8_t)
EMIT_ALL_OPS_N(2, uint16_t)
EMIT_ALL_OPS_N(4, uint32_t)
EMIT_ALL_OPS_N(8, uint64_t)

#else /* !__ARM_ARCH_6__ && !__ARM_ARCH_7__ && !_KERNEL */

/* XXX: Implement intrinsics for ARMv5 userspace. */

#endif

#endif /* __CLANG_ATOMICS || __GNUC_ATOMICS */

/*
 * Old __sync_* API.
 */

#if defined(__SYNC_ATOMICS)

#if defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || \
    defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || \
    defined(__ARM_ARCH_6ZK__) || \
    defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__)

/* Implementations for old GCC versions, lacking support for atomics. */

typedef union {
	uint8_t		v8[4];
	uint32_t	v32;
} reg_t;

/*
 * Given a memory address pointing to an 8-bit or 16-bit integer, return
 * the address of the 32-bit word containing it.
 */

static inline uint32_t *
round_to_word(void *ptr)
{

	return ((uint32_t *)((intptr_t)ptr & ~3));
}

/*
 * Utility functions for loading and storing 8-bit and 16-bit integers
 * in 32-bit words at an offset corresponding with the location of the
 * atomic variable.
 */

static inline void
put_1(reg_t *r, const uint8_t *offset_ptr, uint8_t val)
{
	size_t offset;

	offset = (intptr_t)offset_ptr & 3;
	r->v8[offset] = val;
}

static inline uint8_t
get_1(const reg_t *r, const uint8_t *offset_ptr)
{
	size_t offset;

	offset = (intptr_t)offset_ptr & 3;
	return (r->v8[offset]);
}

static inline void
put_2(reg_t *r, const uint16_t *offset_ptr, uint16_t val)
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
get_2(const reg_t *r, const uint16_t *offset_ptr)
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

/*
 * 8-bit and 16-bit routines.
 *
 * These operations are not natively supported by the CPU, so we use
 * some shifting and bitmasking on top of the 32-bit instructions.
 */

#define	EMIT_LOCK_TEST_AND_SET_N(N, uintN_t)				\
uintN_t									\
__sync_lock_test_and_set_##N(uintN_t *mem, uintN_t val)			\
{									\
	uint32_t *mem32;						\
	reg_t val32, negmask, old;					\
	uint32_t temp1, temp2;						\
									\
	mem32 = round_to_word(mem);					\
	val32.v32 = 0x00000000;						\
	put_##N(&val32, mem, val);					\
	negmask.v32 = 0xffffffff;					\
	put_##N(&negmask, mem, 0);					\
									\
	do_sync();							\
	__asm volatile (						\
		"1:"							\
		"\tldrex %0, %6\n"	/* Load old value. */		\
		"\tand   %2, %5, %0\n"	/* Remove the old value. */	\
		"\torr   %2, %2, %4\n"	/* Put in the new value. */	\
		"\tstrex %3, %2, %1\n"	/* Attempt to store. */		\
		"\tcmp   %3, #0\n"	/* Did it succeed? */		\
		"\tbne   1b\n"		/* Spin if failed. */		\
		: "=&r" (old.v32), "=m" (*mem32), "=&r" (temp1),	\
		  "=&r" (temp2)						\
		: "r" (val32.v32), "r" (negmask.v32), "m" (*mem32));	\
	return (get_##N(&old, mem));					\
}

EMIT_LOCK_TEST_AND_SET_N(1, uint8_t)
EMIT_LOCK_TEST_AND_SET_N(2, uint16_t)

#define	EMIT_VAL_COMPARE_AND_SWAP_N(N, uintN_t)				\
uintN_t									\
__sync_val_compare_and_swap_##N(uintN_t *mem, uintN_t expected,		\
    uintN_t desired)							\
{									\
	uint32_t *mem32;						\
	reg_t expected32, desired32, posmask, negmask, old;		\
	uint32_t temp1, temp2;						\
									\
	mem32 = round_to_word(mem);					\
	expected32.v32 = 0x00000000;					\
	put_##N(&expected32, mem, expected);				\
	desired32.v32 = 0x00000000;					\
	put_##N(&desired32, mem, desired);				\
	posmask.v32 = 0x00000000;					\
	put_##N(&posmask, mem, ~0);					\
	negmask.v32 = ~posmask.v32;					\
									\
	do_sync();							\
	__asm volatile (						\
		"1:"							\
		"\tldrex %0, %8\n"	/* Load old value. */		\
		"\tand   %2, %6, %0\n"	/* Isolate the old value. */	\
		"\tcmp   %2, %4\n"	/* Compare to expected value. */\
		"\tbne   2f\n"		/* Values are unequal. */	\
		"\tand   %2, %7, %0\n"	/* Remove the old value. */	\
		"\torr   %2, %5\n"	/* Put in the new value. */	\
		"\tstrex %3, %2, %1\n"	/* Attempt to store. */		\
		"\tcmp   %3, #0\n"	/* Did it succeed? */		\
		"\tbne   1b\n"		/* Spin if failed. */		\
		"2:"							\
		: "=&r" (old), "=m" (*mem32), "=&r" (temp1),		\
		  "=&r" (temp2)						\
		: "r" (expected32.v32), "r" (desired32.v32),		\
		  "r" (posmask.v32), "r" (negmask.v32), "m" (*mem32));	\
	return (get_##N(&old, mem));					\
}

EMIT_VAL_COMPARE_AND_SWAP_N(1, uint8_t)
EMIT_VAL_COMPARE_AND_SWAP_N(2, uint16_t)

#define	EMIT_ARITHMETIC_FETCH_AND_OP_N(N, uintN_t, name, op)		\
uintN_t									\
__sync_##name##_##N(uintN_t *mem, uintN_t val)				\
{									\
	uint32_t *mem32;						\
	reg_t val32, posmask, negmask, old;				\
	uint32_t temp1, temp2;						\
									\
	mem32 = round_to_word(mem);					\
	val32.v32 = 0x00000000;						\
	put_##N(&val32, mem, val);					\
	posmask.v32 = 0x00000000;					\
	put_##N(&posmask, mem, ~0);					\
	negmask.v32 = ~posmask.v32;					\
									\
	do_sync();							\
	__asm volatile (						\
		"1:"							\
		"\tldrex %0, %7\n"	/* Load old value. */		\
		"\t"op"  %2, %0, %4\n"	/* Calculate new value. */	\
		"\tand   %2, %5\n"	/* Isolate the new value. */	\
		"\tand   %3, %6, %0\n"	/* Remove the old value. */	\
		"\torr   %2, %2, %3\n"	/* Put in the new value. */	\
		"\tstrex %3, %2, %1\n"	/* Attempt to store. */		\
		"\tcmp   %3, #0\n"	/* Did it succeed? */		\
		"\tbne   1b\n"		/* Spin if failed. */		\
		: "=&r" (old.v32), "=m" (*mem32), "=&r" (temp1),	\
		  "=&r" (temp2)						\
		: "r" (val32.v32), "r" (posmask.v32),			\
		  "r" (negmask.v32), "m" (*mem32));			\
	return (get_##N(&old, mem));					\
}

EMIT_ARITHMETIC_FETCH_AND_OP_N(1, uint8_t, fetch_and_add, "add")
EMIT_ARITHMETIC_FETCH_AND_OP_N(1, uint8_t, fetch_and_sub, "sub")
EMIT_ARITHMETIC_FETCH_AND_OP_N(2, uint16_t, fetch_and_add, "add")
EMIT_ARITHMETIC_FETCH_AND_OP_N(2, uint16_t, fetch_and_sub, "sub")

#define	EMIT_BITWISE_FETCH_AND_OP_N(N, uintN_t, name, op, idempotence)	\
uintN_t									\
__sync_##name##_##N(uintN_t *mem, uintN_t val)				\
{									\
	uint32_t *mem32;						\
	reg_t val32, old;						\
	uint32_t temp1, temp2;						\
									\
	mem32 = round_to_word(mem);					\
	val32.v32 = idempotence ? 0xffffffff : 0x00000000;		\
	put_##N(&val32, mem, val);					\
									\
	do_sync();							\
	__asm volatile (						\
		"1:"							\
		"\tldrex %0, %5\n"	/* Load old value. */		\
		"\t"op"  %2, %4, %0\n"	/* Calculate new value. */	\
		"\tstrex %3, %2, %1\n"	/* Attempt to store. */		\
		"\tcmp   %3, #0\n"	/* Did it succeed? */		\
		"\tbne   1b\n"		/* Spin if failed. */		\
		: "=&r" (old.v32), "=m" (*mem32), "=&r" (temp1),	\
		  "=&r" (temp2)						\
		: "r" (val32.v32), "m" (*mem32));			\
	return (get_##N(&old, mem));					\
}

EMIT_BITWISE_FETCH_AND_OP_N(1, uint8_t, fetch_and_and, "and", 1)
EMIT_BITWISE_FETCH_AND_OP_N(1, uint8_t, fetch_and_or, "orr", 0)
EMIT_BITWISE_FETCH_AND_OP_N(1, uint8_t, fetch_and_xor, "eor", 0)
EMIT_BITWISE_FETCH_AND_OP_N(2, uint16_t, fetch_and_and, "and", 1)
EMIT_BITWISE_FETCH_AND_OP_N(2, uint16_t, fetch_and_or, "orr", 0)
EMIT_BITWISE_FETCH_AND_OP_N(2, uint16_t, fetch_and_xor, "eor", 0)

/*
 * 32-bit routines.
 */

uint32_t
__sync_val_compare_and_swap_4(uint32_t *mem, uint32_t expected,
    uint32_t desired)
{
	uint32_t old, temp1, temp2;

	do_sync();
	__asm volatile (
		"1:"
		"\tldrex %0, %6\n"	/* Load old value. */
		"\tcmp   %0, %4\n"	/* Compare to expected value. */
		"\tbne   2f\n"		/* Values are unequal. */
		"\tmov   %2, %5\n"	/* Value to store. */
		"\tstrex %3, %2, %1\n"	/* Attempt to store. */
		"\tcmp   %3, #0\n"	/* Did it succeed? */
		"\tbne   1b\n"		/* Spin if failed. */
		"2:"
		: "=&r" (old), "=m" (*mem), "=&r" (temp1), "=&r" (temp2)
		: "r" (expected), "r" (desired), "m" (*mem));
	return (old);
}

#define	EMIT_FETCH_AND_OP_4(name, op)					\
uint32_t								\
__sync_##name##_4(uint32_t *mem, uint32_t val)				\
{									\
	uint32_t old, temp1, temp2;					\
									\
	do_sync();							\
	__asm volatile (						\
		"1:"							\
		"\tldrex %0, %5\n"	/* Load old value. */		\
		"\t"op"\n"		/* Calculate new value. */	\
		"\tstrex %3, %2, %1\n"	/* Attempt to store. */		\
		"\tcmp   %3, #0\n"	/* Did it succeed? */		\
		"\tbne   1b\n"		/* Spin if failed. */		\
		: "=&r" (old), "=m" (*mem), "=&r" (temp1),		\
		  "=&r" (temp2)						\
		: "r" (val), "m" (*mem));				\
	return (old);							\
}

EMIT_FETCH_AND_OP_4(lock_test_and_set, "mov %2, %4")
EMIT_FETCH_AND_OP_4(fetch_and_add, "add %2, %0, %4")
EMIT_FETCH_AND_OP_4(fetch_and_and, "and %2, %0, %4")
EMIT_FETCH_AND_OP_4(fetch_and_or, "orr %2, %0, %4")
EMIT_FETCH_AND_OP_4(fetch_and_sub, "sub %2, %0, %4")
EMIT_FETCH_AND_OP_4(fetch_and_xor, "eor %2, %0, %4")

#elif defined(_KERNEL)

#ifdef SMP
#error "On SMP systems we should have proper atomic operations."
#endif

/*
 * On uniprocessor systems, we can perform the atomic operations by
 * disabling interrupts.
 */

#define	EMIT_VAL_COMPARE_AND_SWAP_N(N, uintN_t)				\
uintN_t									\
__sync_val_compare_and_swap_##N(uintN_t *mem, uintN_t expected,		\
    uintN_t desired)							\
{									\
	uintN_t ret;							\
									\
	WITHOUT_INTERRUPTS({						\
		ret = *mem;						\
		if (*mem == expected)					\
			*mem = desired;					\
	});								\
	return (ret);							\
}

#define	EMIT_FETCH_AND_OP_N(N, uintN_t, name, op)			\
uintN_t									\
__sync_##name##_##N(uintN_t *mem, uintN_t val)				\
{									\
	uintN_t ret;							\
									\
	WITHOUT_INTERRUPTS({						\
		ret = *mem;						\
		*mem op val;						\
	});								\
	return (ret);							\
}

#define	EMIT_ALL_OPS_N(N, uintN_t)					\
EMIT_VAL_COMPARE_AND_SWAP_N(N, uintN_t)					\
EMIT_FETCH_AND_OP_N(N, uintN_t, lock_test_and_set, =)			\
EMIT_FETCH_AND_OP_N(N, uintN_t, fetch_and_add, +=)			\
EMIT_FETCH_AND_OP_N(N, uintN_t, fetch_and_and, &=)			\
EMIT_FETCH_AND_OP_N(N, uintN_t, fetch_and_or, |=)			\
EMIT_FETCH_AND_OP_N(N, uintN_t, fetch_and_sub, -=)			\
EMIT_FETCH_AND_OP_N(N, uintN_t, fetch_and_xor, ^=)

EMIT_ALL_OPS_N(1, uint8_t)
EMIT_ALL_OPS_N(2, uint16_t)
EMIT_ALL_OPS_N(4, uint32_t)
EMIT_ALL_OPS_N(8, uint64_t)

#else /* !__ARM_ARCH_6__ && !__ARM_ARCH_7__ && !_KERNEL */

/* XXX: Implement intrinsics for ARMv5 userspace. */

#endif

#endif /* __SYNC_ATOMICS */
