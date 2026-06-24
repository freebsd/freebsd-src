/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2026 Intel Corporation */
#ifndef LAC_LOCK_FREE_STACK_H_1
#define LAC_LOCK_FREE_STACK_H_1
#include "lac_mem_pools.h"

#ifdef __LP64__
typedef unsigned int atomic_int __attribute__((mode(TI)));
#else
typedef unsigned int atomic_int __attribute__((mode(DI)));
#endif

typedef union {
	struct {
		unsigned long ctr;
		void *ptr;
	};
	atomic_int atomic;
} pointer_t;

typedef struct {
	volatile pointer_t top;
} lock_free_stack_t;

static inline char
lac_atomic_cmp_swap(volatile pointer_t *ptr,
		    pointer_t old_val,
		    pointer_t new_val)
{
	uint64_t new_high, new_low, old_high, old_low;
	char res;

	old_low = old_val.ctr;
	old_high = (uintptr_t)old_val.ptr;
	new_low = new_val.ctr;
	new_high = (uintptr_t)new_val.ptr;

	__asm volatile("lock;cmpxchg16b\t%1"
		       : "=@cce"(res), "+m"(*ptr), "+a"(old_low), "+d"(old_high)
		       : "b"(new_low), "c"(new_high)
		       : "memory", "cc");
	return (res);
}

static inline lac_mem_blk_t *
pop(lock_free_stack_t *stack)
{
	pointer_t old_top;
	pointer_t new_top;
	lac_mem_blk_t *next;

	do {
		old_top.atomic = stack->top.atomic;
		next = old_top.ptr;
		if (NULL == next)
			return next;

		new_top.ptr = next->pNext;
		new_top.ctr = old_top.ctr + 1;
	} while (!lac_atomic_cmp_swap(&stack->top, old_top, new_top));

	return next;
}

static inline void
push(lock_free_stack_t *stack, lac_mem_blk_t *val)
{
	pointer_t new_top;
	pointer_t old_top;

	do {
		old_top.atomic = stack->top.atomic;
		val->pNext = old_top.ptr;
		new_top.ptr = val;
		new_top.ctr = old_top.ctr + 1;
	} while (!lac_atomic_cmp_swap(&stack->top, old_top, new_top));
}

static inline lock_free_stack_t
_init_stack(void)
{
	lock_free_stack_t stack = { .top.atomic = 0 };
	return stack;
}

static inline lac_mem_blk_t *
top(lock_free_stack_t *stack)
{
	pointer_t old_top = stack->top;
	lac_mem_blk_t *next = old_top.ptr;
	return next;
}

#endif
