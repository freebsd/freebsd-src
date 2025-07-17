/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef LAC_LOCK_FREE_STACK_H_1
#define LAC_LOCK_FREE_STACK_H_1
#include "lac_mem_pools.h"

typedef union {
	struct {
		uint64_t ctr : 16;
		uint64_t ptr : 48;
	};
	uint64_t atomic;
} pointer_t;

typedef struct {
	volatile pointer_t top;
} lock_free_stack_t;

static inline void *
PTR(const uintptr_t addr48)
{
#ifdef __x86_64__
	const int64_t addr64 = addr48 << 16;

	/* Do arithmetic shift to restore kernel canonical address (if not NULL)
	 */
	return (void *)(addr64 >> 16);
#else
	return (void *)(addr48);
#endif
}

static inline lac_mem_blk_t *
pop(lock_free_stack_t *stack)
{
	pointer_t old_top;
	pointer_t new_top;
	lac_mem_blk_t *next;

	do {
		old_top.atomic = stack->top.atomic;
		next = PTR(old_top.ptr);
		if (NULL == next)
			return next;

		new_top.ptr = (uintptr_t)next->pNext;
		new_top.ctr = old_top.ctr + 1;
	} while (!__sync_bool_compare_and_swap(&stack->top.atomic,
					       old_top.atomic,
					       new_top.atomic));

	return next;
}

static inline void
push(lock_free_stack_t *stack, lac_mem_blk_t *val)
{
	pointer_t new_top;
	pointer_t old_top;

	do {
		old_top.atomic = stack->top.atomic;
		val->pNext = PTR(old_top.ptr);
		new_top.ptr = (uintptr_t)val;
		new_top.ctr = old_top.ctr + 1;
	} while (!__sync_bool_compare_and_swap(&stack->top.atomic,
					       old_top.atomic,
					       new_top.atomic));
}

static inline lock_free_stack_t
_init_stack(void)
{
	lock_free_stack_t stack = { { { 0 } } };
	return stack;
}

static inline lac_mem_blk_t *
top(lock_free_stack_t *stack)
{
	pointer_t old_top = stack->top;
	lac_mem_blk_t *next = PTR(old_top.ptr);
	return next;
}

#endif
