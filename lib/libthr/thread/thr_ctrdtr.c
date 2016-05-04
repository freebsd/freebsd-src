/*-
 * Copyright (C) 2003 Jake Burkholder <jake@freebsd.org>
 * Copyright (C) 2003 David Xu <davidxu@freebsd.org>
 * Copyright (c) 2001,2003 Daniel Eischen <deischen@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <rtld_tls.h>

#include "thr_private.h"

/* 16 Byte alignment works for most architectures but on CHERI256 in the pure
 * capability ABI we need need the TCB to be aligned to 32 bytes */
#ifndef TCB_ALIGN
/* TODO: this definition should be enough, no need have another in pthread_md.h */
/* Or do we want it to be a capability in hybrid code? */
/* XXX-AR: #include <sys/param.h> -> MAX(sizeof(void*), 16) */
#define TCB_ALIGN (sizeof(void*) < 16 ? 16 : sizeof(void*))
#endif

struct tcb *
_tcb_ctor(struct pthread *thread, int initial)
{
	struct tcb *tcb;

	if (initial)
		tcb = _tcb_get();
	else
		tcb = _rtld_allocate_tls(NULL, sizeof(struct tcb), TCB_ALIGN);
	if (tcb)
		tcb->tcb_thread = thread;
	return (tcb);
}

void
_tcb_dtor(struct tcb *tcb)
{

	_rtld_free_tls(tcb, sizeof(struct tcb), TCB_ALIGN);
}
