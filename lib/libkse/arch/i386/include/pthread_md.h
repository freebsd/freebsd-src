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

#include <sys/kse.h>
#include <ucontext.h>

extern int _thr_setcontext(mcontext_t *, intptr_t, intptr_t *);
extern int _thr_getcontext(mcontext_t *);

#define	THR_GETCONTEXT(ucp)	_thr_getcontext(&(ucp)->uc_mcontext);
#define	THR_SETCONTEXT(ucp)	_thr_setcontext(&(ucp)->uc_mcontext, NULL, NULL);

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

extern void _i386_enter_uts(struct kse_mailbox *, kse_func_t, void *, long);

static __inline int
_thread_enter_uts(struct kse_thr_mailbox *tmbx, struct kse_mailbox *kmbx)
{
	int ret;

	ret = _thr_getcontext(&tmbx->tm_context.uc_mcontext);
	if (ret == 0) {
		_i386_enter_uts(kmbx, kmbx->km_func,
		    kmbx->km_stack.ss_sp, kmbx->km_stack.ss_size);
		/* We should not reach here. */
		return (-1);
	}
	else if (ret < 0)
		return (-1);
	return (0);
}

static __inline int
_thread_switch(struct kse_thr_mailbox *tmbx, struct kse_thr_mailbox **loc)
{
	_thr_setcontext(&tmbx->tm_context.uc_mcontext,
	    (intptr_t)tmbx, (intptr_t *)loc);
	/* We should not reach here. */
	return (-1);
}

#endif
