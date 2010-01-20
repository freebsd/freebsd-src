/*
 * Copyright 2004 by Peter Grehan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Machine-dependent thread prototypes/definitions.
 */
#ifndef _PTHREAD_MD_H_
#define	_PTHREAD_MD_H_

#include <stddef.h>
#include <sys/types.h>

#define	DTV_OFFSET		offsetof(struct tcb, tcb_dtv)
#define	TP_OFFSET		0x7008

/*
 * Variant I tcb. The structure layout is fixed, don't blindly
 * change it.
 * %r2 points to end of the structure.
 */
struct tcb {
	void			*tcb_dtv;
	struct pthread		*tcb_thread;
};

struct tcb	*_tcb_ctor(struct pthread *, int);
void		_tcb_dtor(struct tcb *);

static __inline void
_tcb_set(struct tcb *tcb)
{
	register uint8_t *_tp __asm__("%r2");

	__asm __volatile("mr %0,%1" : "=r"(_tp) :
	    "r"((uint8_t *)tcb + TP_OFFSET));
}

static __inline struct tcb *
_tcb_get(void)
{
	register uint8_t *_tp __asm__("%r2");

	return ((struct tcb *)(_tp - TP_OFFSET));
}

extern struct pthread *_thr_initial;

static __inline struct pthread *
_get_curthread(void)
{
	if (_thr_initial)
		return (_tcb_get()->tcb_thread);
	return (NULL);
}

#endif /* _PTHREAD_MD_H_ */
