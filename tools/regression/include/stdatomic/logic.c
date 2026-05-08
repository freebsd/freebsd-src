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
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Tool for testing the logical behaviour of operations on atomic
 * integer types. These tests make no attempt to actually test whether
 * the functions are atomic or provide the right barrier semantics.
 *
 * For every type, we create an array of 16 elements and repeat the test
 * on every element in the array. This allows us to test whether the
 * atomic operations have no effect on surrounding values. This is
 * especially useful for the smaller integer types, as it may be the
 * case that these operations are implemented by processing entire words
 * (e.g. on MIPS).
 *
 * Before the randomised loop, a small surface check exercises lock-free
 * macros, fences, atomic_flag, atomic_bool, pointer fetch_add/fetch_sub
 * stride, kill_dependency, atomic_is_lock_free, and when available
 * ATOMIC_VAR_INIT / ATOMIC_FLAG_INIT (pre-C23).
 */

#define	CHECK_LOCK_FREE(name)						\
	_Static_assert((name) == 0 || (name) == 1 || (name) == 2,	\
	    #name " out of range")

CHECK_LOCK_FREE(ATOMIC_BOOL_LOCK_FREE);
CHECK_LOCK_FREE(ATOMIC_CHAR_LOCK_FREE);
CHECK_LOCK_FREE(ATOMIC_SHORT_LOCK_FREE);
CHECK_LOCK_FREE(ATOMIC_INT_LOCK_FREE);
CHECK_LOCK_FREE(ATOMIC_LONG_LOCK_FREE);
CHECK_LOCK_FREE(ATOMIC_LLONG_LOCK_FREE);
CHECK_LOCK_FREE(ATOMIC_POINTER_LOCK_FREE);
#ifdef ATOMIC_CHAR8_T_LOCK_FREE
CHECK_LOCK_FREE(ATOMIC_CHAR8_T_LOCK_FREE);
#endif
#ifdef ATOMIC_CHAR16_T_LOCK_FREE
CHECK_LOCK_FREE(ATOMIC_CHAR16_T_LOCK_FREE);
#endif
#ifdef ATOMIC_CHAR32_T_LOCK_FREE
CHECK_LOCK_FREE(ATOMIC_CHAR32_T_LOCK_FREE);
#endif
#ifdef ATOMIC_WCHAR_T_LOCK_FREE
CHECK_LOCK_FREE(ATOMIC_WCHAR_T_LOCK_FREE);
#endif

#if defined(__STDC_VERSION_STDATOMIC_H__)
_Static_assert(__STDC_VERSION_STDATOMIC_H__ == 202311L,
    "__STDC_VERSION_STDATOMIC_H__");
#endif

static void
test_surface(void)
{
	atomic_flag f = ATOMIC_FLAG_INIT;
	atomic_int probe;
	atomic_bool b;
	_Atomic(int *) ap;
	int arr[4];
	int dep;
	static const memory_order k_orders[] = {
		memory_order_relaxed,
		memory_order_consume,
		memory_order_acquire,
		memory_order_release,
		memory_order_acq_rel,
		memory_order_seq_cst,
	};
	size_t k;

	dep = kill_dependency(123);
	assert(dep == 123);

	atomic_init(&probe, 0);
	assert(atomic_is_lock_free(&probe) ==
	    atomic_is_lock_free((atomic_int *)NULL));

	for (k = 0; k < sizeof(k_orders) / sizeof(k_orders[0]); k++) {
		atomic_thread_fence(k_orders[k]);
		atomic_signal_fence(k_orders[k]);
	}

	assert(atomic_flag_test_and_set_explicit(&f,
	    memory_order_seq_cst) == 0);
	assert(atomic_flag_test_and_set_explicit(&f,
	    memory_order_acquire) != 0);
	atomic_flag_clear_explicit(&f, memory_order_relaxed);

	atomic_init(&b, false);
	atomic_store_explicit(&b, true, memory_order_relaxed);
	assert(atomic_load_explicit(&b, memory_order_relaxed) == true);

	memset(arr, 0, sizeof(arr));
	atomic_init(&ap, &arr[3]);
	atomic_fetch_sub_explicit(&ap, 2, memory_order_relaxed);
	assert(ap == &arr[1]);
	atomic_fetch_add_explicit(&ap, 1, memory_order_relaxed);
	assert(ap == &arr[2]);
}

#ifdef ATOMIC_VAR_INIT
static void
test_atomic_var_init(void)
{
	atomic_int x = ATOMIC_VAR_INIT(42);

	assert(atomic_load_explicit(&x, memory_order_relaxed) == 42);
	atomic_store_explicit(&x, 7, memory_order_relaxed);
	assert(atomic_load_explicit(&x, memory_order_relaxed) == 7);

	{
		atomic_flag f = ATOMIC_FLAG_INIT;

		assert(atomic_flag_test_and_set_explicit(&f,
		    memory_order_seq_cst) == 0);
		atomic_flag_clear_explicit(&f, memory_order_relaxed);
	}
}
#endif

static inline intmax_t
rndnum(void)
{
	intmax_t v;

	arc4random_buf(&v, sizeof(v));
	return (v);
}

#define	DO_FETCH_TEST(T, a, name, result) do {				\
	T v1 = atomic_load(a);						\
	T v2 = rndnum();						\
	assert(atomic_##name(a, v2) == v1); 				\
	assert(atomic_load(a) == (T)(result)); 				\
} while (0)

#define	DO_COMPARE_EXCHANGE_TEST(T, a, name) do {			\
	T v1 = atomic_load(a);						\
	T v2 = rndnum();						\
	T v3 = rndnum();						\
	if (atomic_compare_exchange_##name(a, &v2, v3))			\
		assert(v1 == v2);					\
	else								\
		assert(atomic_compare_exchange_##name(a, &v2, v3));	\
	assert(atomic_load(a) == v3);					\
} while (0)

#define	DO_ALL_TESTS(T, a) do {						\
	{								\
		T v1 = rndnum();					\
		atomic_init(a, v1);					\
		assert(atomic_load(a) == v1);				\
	}								\
	{								\
		T v1 = rndnum();					\
		atomic_store(a, v1);					\
		assert(atomic_load(a) == v1);				\
	}								\
									\
	DO_FETCH_TEST(T, a, exchange, v2);				\
	DO_FETCH_TEST(T, a, fetch_add, v1 + v2);			\
	DO_FETCH_TEST(T, a, fetch_and, v1 & v2);			\
	DO_FETCH_TEST(T, a, fetch_or, v1 | v2);				\
	DO_FETCH_TEST(T, a, fetch_sub, v1 - v2);			\
	DO_FETCH_TEST(T, a, fetch_xor, v1 ^ v2);			\
									\
	DO_COMPARE_EXCHANGE_TEST(T, a, weak);				\
	DO_COMPARE_EXCHANGE_TEST(T, a, strong);				\
} while (0)

#define	TEST_TYPE(T) do {						\
	int j;								\
	struct { _Atomic(T) v[16]; } list, cmp;				\
	arc4random_buf(&cmp, sizeof(cmp));				\
	for (j = 0; j < 16; j++) {					\
		list = cmp;						\
		DO_ALL_TESTS(T, &list.v[j]);				\
		list.v[j] = cmp.v[j];					\
		assert(memcmp(&list, &cmp, sizeof(list)) == 0);		\
	}								\
} while (0)

int
main(void)
{
	int i;

	test_surface();
#ifdef ATOMIC_VAR_INIT
	test_atomic_var_init();
#endif
	for (i = 0; i < 1000; i++) {
		TEST_TYPE(int8_t);
		TEST_TYPE(uint8_t);
		TEST_TYPE(int16_t);
		TEST_TYPE(uint16_t);
		TEST_TYPE(int32_t);
		TEST_TYPE(uint32_t);
		TEST_TYPE(int64_t);
		TEST_TYPE(uint64_t);
	}

	return (0);
}
