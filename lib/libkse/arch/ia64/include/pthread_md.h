/*
 * Copyright (c) 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _PTHREAD_MD_H_
#define	_PTHREAD_MD_H_

#define	THR_GETCONTEXT(ucp)	getcontext(ucp)
#define	THR_SETCONTEXT(ucp)	setcontext(ucp)

#define	THR_ALIGNBYTES	15
#define	THR_ALIGN(td)	(((uintptr_t)(td) + THR_ALIGNBYTES) & ~THR_ALIGNBYTES)

/* KSE Specific Data. */
struct ksd {
	void	*ksd_base;
	int	ksd_size;
};

void _ia64_enter_uts(kse_func_t uts, struct kse_mailbox *km, void *stack,
    size_t stacksz);
int _ia64_restore_context(mcontext_t *mc, intptr_t val, intptr_t *loc);
int _ia64_save_context(mcontext_t *mc);

static __inline int
_thread_enter_uts(struct kse_thr_mailbox *tm, struct kse_mailbox *km)
{
	if (tm == NULL)
		return (-1);
	if (!_ia64_save_context(&tm->tm_context.uc_mcontext)) {
		_ia64_enter_uts(km->km_func, km, km->km_stack.ss_sp,
		    km->km_stack.ss_size);
		/* We should not reach here. */
		return (-1);
	}
	return (0);
}

static __inline int
_thread_switch(struct kse_thr_mailbox *tm, struct kse_thr_mailbox **thrp)
{
	if (tm == NULL)
		return (-1);
	_ia64_restore_context(&tm->tm_context.uc_mcontext, (intptr_t)tm,
	    (intptr_t*)thrp);
	/* We should not reach here. */
	return (-1);
}

#endif /* _PTHREAD_MD_H_ */
