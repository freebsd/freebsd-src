/*
 * Copyright (c) 2004, David Xu <davidxu@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#include <sys/types.h>
#include <sys/ucontext.h>

#include <pthread.h>
#include <machine/sysarch.h>

#include "thr_private.h"
#include "rtld_tls.h"

struct tcb {
	struct tcb	*tcb_self;      /* required by rtld */
	void		*tcb_dtv;       /* required by rtld */
	struct pthread	*tcb_thread;
};

void
_retire_thread(void *entry)
{
	struct tcb *tcb = (struct tcb *)entry;

	_rtld_free_tls(tcb, sizeof(struct tcb), 16);
}

void *
_set_curthread(ucontext_t *uc, struct pthread *thr, int *err)
{
	struct tcb *tcb;
	void *oldtls;

	*err = 0;

	if (thr->arch_id != NULL && uc == NULL) {
		amd64_set_fsbase(thr->arch_id);
		return (thr->arch_id);
	}	

	if (uc == NULL) {
		__asm __volatile("movq %%fs:0, %0" : "=r" (oldtls));
        } else {
		oldtls = NULL;
        }

	/*
	 * Allocate and initialise a new TLS block with enough extra
	 * space for our self pointer.
	 */
	tcb = _rtld_allocate_tls(oldtls, sizeof(struct tcb), 16);

	/*
	 * Cache the address of the thread structure here, after
	 * rtld's two words of private space.
 	 */
	tcb->tcb_thread = thr;

	if (uc == NULL)
		amd64_set_fsbase(tcb);
	return (tcb);
}

pthread_t
_get_curthread(void)
{
	extern pthread_t _thread_initial;
	pthread_t td;

	if (_thread_initial == NULL)
		return (NULL);
	__asm __volatile("movq %%fs:%1, %0"	\
		: "=r" (td)         		\
		: "m" (*(long *)(__offsetof(struct tcb, tcb_thread))));

	return (td);
}
