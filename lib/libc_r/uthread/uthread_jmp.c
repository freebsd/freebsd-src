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
#include <sys/user.h>
#ifdef _THREAD_SAFE
#include <machine/reg.h>
#include <pthread.h>
#include "pthread_private.h"

void
siglongjmp(sigjmp_buf env, int savemask)
{
	void	*jmp_stackp;
	void	*stack_begin, *stack_end;
	int	frame, dst_frame;

	if ((frame = _thread_run->sigframe_count) == 0)
		__siglongjmp(env, savemask);

	/* Get the stack pointer from the jump buffer. */
	jmp_stackp = (void *) GET_STACK_SJB(env);

	/* Get the bounds of the current threads stack. */
	PTHREAD_ASSERT(_thread_run->stack != NULL,
	    "Thread stack pointer is null");
	stack_begin = _thread_run->stack;
	stack_end = stack_begin + _thread_run->attr.stacksize_attr;

	/*
	 * Make sure we aren't jumping to a different stack.  Make sure
	 * jmp_stackp is between stack_begin and stack end, to correctly detect
	 * this condition regardless of whether the stack grows up or down.
	 */
	if (((jmp_stackp < stack_begin) && (jmp_stackp < stack_end)) ||
	    ((jmp_stackp > stack_begin) && (jmp_stackp > stack_end)))
		PANIC("siglongjmp()ing between thread contexts is undefined by "
		    "POSIX 1003.1");

	if ((dst_frame = _thread_sigframe_find(_thread_run, jmp_stackp)) < 0)
		/*
		 * The stack pointer was verified above, so this
		 * shouldn't happen.  Let's be anal anyways.
		 */
		PANIC("Error locating signal frame");
	else if (dst_frame == frame) {
		/*
		 * The stack pointer is somewhere within the current
		 * frame.  Jump to the users context.
		 */
		__siglongjmp(env, savemask);
	}
	/*
	 * Copy the users context to the return context of the
	 * destination frame.
	 */
	memcpy(&_thread_run->sigframes[dst_frame]->ctx.sigjb, env, sizeof(*env));
	_thread_run->sigframes[dst_frame]->ctxtype = CTX_SJB;
	_thread_run->sigframes[dst_frame]->longjmp_val = savemask;
	_thread_run->curframe->dst_frame = dst_frame;
	___longjmp(*_thread_run->curframe->sig_jb, 1);
}

void
longjmp(jmp_buf env, int val)
{
	void	*jmp_stackp;
	void	*stack_begin, *stack_end;
	int	frame, dst_frame;

	if ((frame = _thread_run->sigframe_count) == 0)
		__longjmp(env, val);

	/* Get the stack pointer from the jump buffer. */
	jmp_stackp = (void *) GET_STACK_JB(env);

	/* Get the bounds of the current threads stack. */
	PTHREAD_ASSERT(_thread_run->stack != NULL,
	    "Thread stack pointer is null");
	stack_begin = _thread_run->stack;
	stack_end = stack_begin + _thread_run->attr.stacksize_attr;

	/*
	 * Make sure we aren't jumping to a different stack.  Make sure
	 * jmp_stackp is between stack_begin and stack end, to correctly detect
	 * this condition regardless of whether the stack grows up or down.
	 */
	if (((jmp_stackp < stack_begin) && (jmp_stackp < stack_end)) ||
	    ((jmp_stackp > stack_begin) && (jmp_stackp > stack_end)))
		PANIC("longjmp()ing between thread contexts is undefined by "
		    "POSIX 1003.1");

	if ((dst_frame = _thread_sigframe_find(_thread_run, jmp_stackp)) < 0)
		/*
		 * The stack pointer was verified above, so this
		 * shouldn't happen.  Let's be anal anyways.
		 */
		PANIC("Error locating signal frame");
	else if (dst_frame == frame) {
		/*
		 * The stack pointer is somewhere within the current
		 * frame.  Jump to the users context.
		 */
		__longjmp(env, val);
	}

	/*
	 * Copy the users context to the return context of the
	 * destination frame.
	 */
	memcpy(&_thread_run->sigframes[dst_frame]->ctx.jb, env, sizeof(*env));
	_thread_run->sigframes[dst_frame]->ctxtype = CTX_JB;
	_thread_run->sigframes[dst_frame]->longjmp_val = val;
	_thread_run->curframe->dst_frame = dst_frame;
	___longjmp(*_thread_run->curframe->sig_jb, 1);
}

void
_longjmp(jmp_buf env, int val)
{
	void	*jmp_stackp;
	void	*stack_begin, *stack_end;
	int	frame, dst_frame;

	if ((frame = _thread_run->sigframe_count) == 0)
		___longjmp(env, val);

	/* Get the stack pointer from the jump buffer. */
	jmp_stackp = (void *) GET_STACK_JB(env);

	/* Get the bounds of the current threads stack. */
	PTHREAD_ASSERT(_thread_run->stack != NULL,
	    "Thread stack pointer is null");
	stack_begin = _thread_run->stack;
	stack_end = stack_begin + _thread_run->attr.stacksize_attr;

	/*
	 * Make sure we aren't jumping to a different stack.  Make sure
	 * jmp_stackp is between stack_begin and stack end, to correctly detect
	 * this condition regardless of whether the stack grows up or down.
	 */
	if (((jmp_stackp < stack_begin) && (jmp_stackp < stack_end)) ||
	    ((jmp_stackp > stack_begin) && (jmp_stackp > stack_end)))
		PANIC("_longjmp()ing between thread contexts is undefined by "
		    "POSIX 1003.1");

	if ((dst_frame = _thread_sigframe_find(_thread_run, jmp_stackp)) < 0)
		/*
		 * The stack pointer was verified above, so this
		 * shouldn't happen.  Let's be anal anyways.
		 */
		PANIC("Error locating signal frame");
	else if (dst_frame == frame) {
		/*
		 * The stack pointer is somewhere within the current
		 * frame.  Jump to the users context.
		 */
		___longjmp(env, val);
	}
	/*
	 * Copy the users context to the return context of the
	 * destination frame.
	 */
	memcpy(&_thread_run->sigframes[dst_frame]->ctx.jb, env, sizeof(*env));
	_thread_run->sigframes[dst_frame]->ctxtype = CTX_JB_NOSIG;
	_thread_run->sigframes[dst_frame]->longjmp_val = val;
	_thread_run->curframe->dst_frame = dst_frame;
	___longjmp(*_thread_run->curframe->sig_jb, 1);
}
#endif
