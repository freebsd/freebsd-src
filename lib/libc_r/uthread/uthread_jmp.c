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
 * $FreeBSD: src/lib/libc_r/uthread/uthread_jmp.c,v 1.2.2.1 2000/07/18 02:05:56 jasone Exp $
 */

#include <unistd.h>
#include <setjmp.h>
#include <sys/param.h>
#include <sys/user.h>
#ifdef _THREAD_SAFE
#include <machine/reg.h>
#include <pthread.h>
#include "pthread_private.h"

/*
 * Offset into the jmp_buf.  This is highly machine-dependent, but is a
 * necessary evil in order to compare stack pointers and make decisions based on
 * where a *longjmp() is jumping to.
 */
#if defined(__i386__)
#define	JMP_BUF_SP_OFFSET 2
#elif defined(__alpha)
#define	JMP_BUF_SP_OFFSET (4 + R_SP)
#else
#error "Don't recognize this architecture!"
#endif

void
siglongjmp(sigjmp_buf env, int savemask)
{
	void	*jmp_stackp;
	void	*stack_begin, *stack_end;

	if (_thread_run->signal_nest_level == 0)
		__siglongjmp(env, savemask);

	/* Get the stack pointer from the jump buffer. */
	jmp_stackp = (void *)env->_sjb[JMP_BUF_SP_OFFSET];

	/* Get the bounds of the current threads stack. */
	if (_thread_run->stack != NULL) {
		stack_begin = _thread_run->stack;
		stack_end = stack_begin + _thread_run->attr.stacksize_attr;
	} else {
		stack_end = (void *)USRSTACK;
		stack_begin = stack_end - PTHREAD_STACK_INITIAL;
	}

	/*
	 * Make sure we aren't jumping to a different stack.  Make sure
	 * jmp_stackp is between stack_begin and stack end, to correctly detect
	 * this condition regardless of whether the stack grows up or down.
	 */
	if (((jmp_stackp < stack_begin) && (jmp_stackp < stack_end)) ||
	    ((jmp_stackp > stack_begin) && (jmp_stackp > stack_end)))
		PANIC("siglongjmp()ing between thread contexts is undefined by "
		    "POSIX 1003.1");

	memcpy(_thread_run->nested_jmp.sigjmp, env,
	    sizeof(_thread_run->nested_jmp.sigjmp));

	/*
	 * Only save oldstate once so that dispatching multiple signals will not
	 * lose the thread's original state.
	 */
	if (_thread_run->jmpflags == JMPFLAGS_NONE)
		_thread_run->oldstate = _thread_run->state;
	PTHREAD_SET_STATE(_thread_run, PS_RUNNING);
	_thread_run->jmpflags = JMPFLAGS_SIGLONGJMP;
	_thread_run->longjmp_val = savemask;
	___longjmp(*_thread_run->sighandler_jmp_buf, 1);
}

void
longjmp(jmp_buf env, int val)
{
	void	*jmp_stackp;
	void	*stack_begin, *stack_end;

	if (_thread_run->signal_nest_level == 0)
		__longjmp(env, val);

	/* Get the stack pointer from the jump buffer. */
	jmp_stackp = (void *)env->_jb[JMP_BUF_SP_OFFSET];

	/* Get the bounds of the current threads stack. */
	if (_thread_run->stack != NULL) {
		stack_begin = _thread_run->stack;
		stack_end = stack_begin + _thread_run->attr.stacksize_attr;
	} else {
		stack_end = (void *)USRSTACK;
		stack_begin = stack_end - PTHREAD_STACK_INITIAL;
	}

	/*
	 * Make sure we aren't jumping to a different stack.  Make sure
	 * jmp_stackp is between stack_begin and stack end, to correctly detect
	 * this condition regardless of whether the stack grows up or down.
	 */
	if (((jmp_stackp < stack_begin) && (jmp_stackp < stack_end)) ||
	    ((jmp_stackp > stack_begin) && (jmp_stackp > stack_end)))
		PANIC("longjmp()ing between thread contexts is undefined by "
		    "POSIX 1003.1");

	memcpy(_thread_run->nested_jmp.jmp, env,
	    sizeof(_thread_run->nested_jmp.jmp));

	/*
	 * Only save oldstate once so that dispatching multiple signals will not
	 * lose the thread's original state.
	 */
	if (_thread_run->jmpflags == JMPFLAGS_NONE)
		_thread_run->oldstate = _thread_run->state;
	PTHREAD_SET_STATE(_thread_run, PS_RUNNING);
	_thread_run->jmpflags = JMPFLAGS_LONGJMP;
	_thread_run->longjmp_val = val;
	___longjmp(*_thread_run->sighandler_jmp_buf, 1);
}

void
_longjmp(jmp_buf env, int val)
{
	void	*jmp_stackp;
	void	*stack_begin, *stack_end;

	if (_thread_run->signal_nest_level == 0)
		___longjmp(env, val);

	/* Get the stack pointer from the jump buffer. */
	jmp_stackp = (void *)env->_jb[JMP_BUF_SP_OFFSET];

	/* Get the bounds of the current threads stack. */
	if (_thread_run->stack != NULL) {
		stack_begin = _thread_run->stack;
		stack_end = stack_begin + _thread_run->attr.stacksize_attr;
	} else {
		stack_end = (void *)USRSTACK;
		stack_begin = stack_end - PTHREAD_STACK_INITIAL;
	}

	/*
	 * Make sure we aren't jumping to a different stack.  Make sure
	 * jmp_stackp is between stack_begin and stack end, to correctly detect
	 * this condition regardless of whether the stack grows up or down.
	 */
	if (((jmp_stackp < stack_begin) && (jmp_stackp < stack_end)) ||
	    ((jmp_stackp > stack_begin) && (jmp_stackp > stack_end)))
		PANIC("_longjmp()ing between thread contexts is undefined by "
		    "POSIX 1003.1");

	memcpy(_thread_run->nested_jmp.jmp, env,
	    sizeof(_thread_run->nested_jmp.jmp));

	/*
	 * Only save oldstate once so that dispatching multiple signals will not
	 * lose the thread's original state.
	 */
	if (_thread_run->jmpflags == JMPFLAGS_NONE)
		_thread_run->oldstate = _thread_run->state;
	PTHREAD_SET_STATE(_thread_run, PS_RUNNING);
	_thread_run->jmpflags = JMPFLAGS__LONGJMP;
	_thread_run->longjmp_val = val;
	___longjmp(*_thread_run->sighandler_jmp_buf, 1);
}
#endif
