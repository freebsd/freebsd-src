/*-
 * Copyright (c) 2002 Daniel Eischen <deischen@freebsd.org>.
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
/*
 * Machine-dependent thread prototypes/definitions for the thread kernel.
 */
#ifndef _PTHREAD_MD_H_
#define	_PTHREAD_MD_H_

#include <setjmp.h>
#include <ucontext.h>

extern int _thr_setcontext(ucontext_t *);
extern int _thr_getcontext(ucontext_t *);

/*
 * These are needed to ensure an application doesn't attempt to jump
 * between stacks of different threads.  They return the stack of
 * jmp_buf, sigjmp_buf, and ucontext respectively.
 */
#define	GET_STACK_JB(jb)	((unsigned long)((jb)[0]._jb[2]))
#define	GET_STACK_SJB(sjb)	((unsigned long)((sjb)[0]._sjb[2]))
#define	GET_STACK_UC(ucp)	((unsigned long)((ucp)->uc_mcontext.mc_esp))

#define	THR_GETCONTEXT(ucp)	_thr_getcontext(ucp)
#define	THR_SETCONTEXT(ucp)	_thr_setcontext(ucp)

#define	THR_ALIGNBYTES	15
#define	THR_ALIGN(td)	(((unsigned)(td) + THR_ALIGNBYTES) & ~THR_ALIGNBYTES)

/*
 * KSE Specific Data.
 */
struct ksd {
	int	ldt;
#define	KSDF_INITIALIZED	0x01
	long	flags;
	void	*base;
	long	size;
};

#endif
