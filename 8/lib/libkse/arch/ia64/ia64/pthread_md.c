/*
 * Copyright (c) 2003 Daniel Eischen <deischen@freebsd.org>
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <stdlib.h>
#include <strings.h>

#include "rtld_tls.h"
#include "pthread_md.h"

/*
 * The constructors.
 */
struct tcb *
_tcb_ctor(struct pthread *thread, int initial)
{
	register char *tp __asm("%r13");
	struct tcb *tcb;

	tcb = _rtld_allocate_tls((initial) ? tp : NULL,
	    sizeof(struct tcb), 16);
	if (tcb == NULL)
		return (NULL);
	tcb->tcb_thread = thread;
	return (tcb);
}

void
_tcb_dtor(struct tcb *tcb)
{

	_rtld_free_tls(tcb, sizeof(struct tcb), 16);
}

struct kcb *
_kcb_ctor(struct kse *kse)
{
	struct kcb *kcb;

	kcb = malloc(sizeof(struct kcb));
	if (kcb == NULL)
		return (NULL);
	bzero(kcb, sizeof(struct kcb));
	kcb->kcb_kse = kse;
	kcb->kcb_faketcb.tcb_isfake = 1;
	kcb->kcb_faketcb.tcb_tmbx.tm_flags = TMF_NOUPCALL;
	kcb->kcb_curtcb = &kcb->kcb_faketcb;
	return (kcb);
}

void
_kcb_dtor(struct kcb *kcb)
{
	free(kcb);
}
