/*
 * Copyright (c) 2001 Daniel Eischen <deischen@freebsd.org>
 * Copyright (c) 2000-2001 Jason Evans <jasone@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/user.h>
#include <stdlib.h>
#include <pthread.h>
#include "thr_private.h"

/* Spare thread stack. */
struct stack {
	LIST_ENTRY(stack)	qe;		/* Stack queue linkage. */
	size_t			stacksize;	/* Stack size (rounded up). */
	size_t			guardsize;	/* Guard size. */
	void			*stackaddr;	/* Stack address. */
};

/*
 * Default sized (stack and guard) spare stack queue.  Stacks are cached to
 * avoid additional complexity managing mmap()ed stack regions.  Spare stacks
 * are used in LIFO order to increase cache locality.
 */
static LIST_HEAD(, stack)	_dstackq = LIST_HEAD_INITIALIZER(_dstackq);

/*
 * Miscellaneous sized (non-default stack and/or guard) spare stack queue.
 * Stacks are cached to avoid additional complexity managing mmap()ed stack
 * regions.  This list is unordered, since ordering on both stack size and guard
 * size would be more trouble than it's worth.  Stacks are allocated from this
 * cache on a first size match basis.
 */
static LIST_HEAD(, stack)	_mstackq = LIST_HEAD_INITIALIZER(_mstackq);

/**
 * Base address of the last stack allocated (including its red zone, if there is
 * one).  Stacks are allocated contiguously, starting beyond the top of the main
 * stack.  When a new stack is created, a red zone is typically created
 * (actually, the red zone is simply left unmapped) above the top of the stack,
 * such that the stack will not be able to grow all the way to the bottom of the
 * next stack.  This isn't fool-proof.  It is possible for a stack to grow by a
 * large amount, such that it grows into the next stack, and as long as the
 * memory within the red zone is never accessed, nothing will prevent one thread
 * stack from trouncing all over the next.
 *
 * low memory
 *     . . . . . . . . . . . . . . . . . . 
 *    |                                   |
 *    |             stack 3               | start of 3rd thread stack
 *    +-----------------------------------+
 *    |                                   |
 *    |       Red Zone (guard page)       | red zone for 2nd thread
 *    |                                   |
 *    +-----------------------------------+
 *    |  stack 2 - PTHREAD_STACK_DEFAULT  | top of 2nd thread stack
 *    |                                   |
 *    |                                   |
 *    |                                   |
 *    |                                   |
 *    |             stack 2               |
 *    +-----------------------------------+ <-- start of 2nd thread stack
 *    |                                   |
 *    |       Red Zone                    | red zone for 1st thread
 *    |                                   |
 *    +-----------------------------------+
 *    |  stack 1 - PTHREAD_STACK_DEFAULT  | top of 1st thread stack
 *    |                                   |
 *    |                                   |
 *    |                                   |
 *    |                                   |
 *    |             stack 1               |
 *    +-----------------------------------+ <-- start of 1st thread stack
 *    |                                   |   (initial value of last_stack)
 *    |       Red Zone                    |
 *    |                                   | red zone for main thread
 *    +-----------------------------------+
 *    | USRSTACK - PTHREAD_STACK_INITIAL  | top of main thread stack
 *    |                                   | ^
 *    |                                   | |
 *    |                                   | |
 *    |                                   | | stack growth
 *    |                                   |
 *    +-----------------------------------+ <-- start of main thread stack
 *                                              (USRSTACK)
 * high memory
 *
 */
static void *	last_stack;

void *
_thread_stack_alloc(size_t stacksize, size_t guardsize)
{
	void		*stack = NULL;
	struct stack	*spare_stack;
	size_t		stack_size;

	/*
	 * Round up stack size to nearest multiple of _pthread_page_size,
	 * so that mmap() * will work.  If the stack size is not an even
	 * multiple, we end up initializing things such that there is unused
	 * space above the beginning of the stack, so the stack sits snugly
	 * against its guard.
	 */
	if (stacksize % _pthread_page_size != 0)
		stack_size = ((stacksize / _pthread_page_size) + 1) *
		    _pthread_page_size;
	else
		stack_size = stacksize;

	/*
	 * If the stack and guard sizes are default, try to allocate a stack
	 * from the default-size stack cache:
	 */
	if (stack_size == PTHREAD_STACK_DEFAULT &&
	    guardsize == _pthread_guard_default) {
		/*
		 * Use the garbage collector mutex for synchronization of the
		 * spare stack list.
		 */
		STACK_LOCK;

		if ((spare_stack = LIST_FIRST(&_dstackq)) != NULL) {
				/* Use the spare stack. */
			LIST_REMOVE(spare_stack, qe);
			stack = spare_stack->stackaddr;
		}

		/* Unlock the garbage collector mutex. */
		STACK_UNLOCK;
	}
	/*
	 * The user specified a non-default stack and/or guard size, so try to
	 * allocate a stack from the non-default size stack cache, using the
	 * rounded up stack size (stack_size) in the search:
	 */
	else {
		/*
		 * Use the garbage collector mutex for synchronization of the
		 * spare stack list.
		 */
		STACK_LOCK;

		LIST_FOREACH(spare_stack, &_mstackq, qe) {
			if (spare_stack->stacksize == stack_size &&
			    spare_stack->guardsize == guardsize) {
				LIST_REMOVE(spare_stack, qe);
				stack = spare_stack->stackaddr;
				break;
			}
		}

		/* Unlock the garbage collector mutex. */
		STACK_UNLOCK;
	}

	/* Check if a stack was not allocated from a stack cache: */
	if (stack == NULL) {

		if (last_stack == NULL)
			last_stack = _usrstack - PTHREAD_STACK_INITIAL -
			    _pthread_guard_default;

		/* Allocate a new stack. */
		stack = last_stack - stack_size;

		/*
		 * Even if stack allocation fails, we don't want to try to use
		 * this location again, so unconditionally decrement
		 * last_stack.  Under normal operating conditions, the most
		 * likely reason for an mmap() error is a stack overflow of the
		 * adjacent thread stack.
		 */
		last_stack -= (stack_size + guardsize);

		/* Stack: */
		if (mmap(stack, stack_size, PROT_READ | PROT_WRITE, MAP_STACK,
		    -1, 0) == MAP_FAILED)
			stack = NULL;
	}

	return (stack);
}

/* This function must be called with the 'dead thread list' lock held. */
void
_thread_stack_free(void *stack, size_t stacksize, size_t guardsize)
{
	struct stack	*spare_stack;

	spare_stack = (stack + stacksize - sizeof(struct stack));
	/* Round stacksize up to nearest multiple of _pthread_page_size. */
	if (stacksize % _pthread_page_size != 0) {
		spare_stack->stacksize =
		    ((stacksize / _pthread_page_size) + 1) *
		    _pthread_page_size;
	} else
		spare_stack->stacksize = stacksize;
	spare_stack->guardsize = guardsize;
	spare_stack->stackaddr = stack;

	if (spare_stack->stacksize == PTHREAD_STACK_DEFAULT &&
	    spare_stack->guardsize == _pthread_guard_default) {
		/* Default stack/guard size. */
		LIST_INSERT_HEAD(&_dstackq, spare_stack, qe);
	} else {
		/* Non-default stack/guard size. */
		LIST_INSERT_HEAD(&_mstackq, spare_stack, qe);
	}
}
