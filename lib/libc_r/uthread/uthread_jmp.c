/*
 * Copyright (C) 2000 Jason Evans <jasone@freebsd.org>.
 * All rights reserved.
 * Copyright (C) 2000 Daniel M. Eischen <eischen@vigrid.com>.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <unistd.h>
#include <setjmp.h>
#include <sys/param.h>
#include <machine/reg.h>
#include <pthread.h>
#include "pthread_private.h"

/* Prototypes: */
static inline int	check_stack(pthread_t thread, void *stackp);

__weak_reference(_thread_siglongjmp, siglongjmp);
__weak_reference(_thread_longjmp, longjmp);
__weak_reference(__thread_longjmp, _longjmp);

void
_thread_siglongjmp(sigjmp_buf env, int savemask)
{
	struct pthread	*curthread = _get_curthread();

	if (check_stack(curthread, (void *) GET_STACK_SJB(env)))
		PANIC("siglongjmp()ing between thread contexts is undefined by "
		    "POSIX 1003.1");

	/*
	 * The stack pointer is somewhere within the threads stack.
	 * Jump to the users context.
	 */
	__siglongjmp(env, savemask);
}

void
_thread_longjmp(jmp_buf env, int val)
{
	struct pthread	*curthread = _get_curthread();

	if (check_stack(curthread, (void *) GET_STACK_JB(env)))
		PANIC("longjmp()ing between thread contexts is undefined by "
		    "POSIX 1003.1");

	/*
	 * The stack pointer is somewhere within the threads stack.
	 * Jump to the users context.
	 */
	__longjmp(env, val);
}

void
__thread_longjmp(jmp_buf env, int val)
{
	struct pthread	*curthread = _get_curthread();

	if (check_stack(curthread, (void *) GET_STACK_JB(env)))
		PANIC("_longjmp()ing between thread contexts is undefined by "
		    "POSIX 1003.1");

	/*
	 * The stack pointer is somewhere within the threads stack.
	 * Jump to the users context.
	 */
	___longjmp(env, val);
}

/* Returns 0 if stack check is OK, non-zero otherwise. */
static inline int
check_stack(pthread_t thread, void *stackp)
{
	void	*stack_begin, *stack_end;

	/* Get the bounds of the current threads stack. */
	PTHREAD_ASSERT(thread->stack != NULL,
	    "Thread stack pointer is null");
	stack_begin = thread->stack;
	stack_end = stack_begin + thread->attr.stacksize_attr;

	/*
	 * Make sure we aren't jumping to a different stack.  Make sure
	 * jmp_stackp is between stack_begin and stack end, to correctly detect
	 * this condition regardless of whether the stack grows up or down.
	 */
	if (((stackp < stack_begin) && (stackp < stack_end)) ||
	    ((stackp > stack_begin) && (stackp > stack_end)))
		return (1);
	else
		return (0);
}
