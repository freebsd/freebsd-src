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

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>

#include "pthread_md.h"

struct tcb *
_tcb_ctor(struct pthread *thread)
{
	struct tcb *tcb;
	void *addr;

	addr = malloc(sizeof(struct tcb) + 63);
	if (addr == NULL)
		tcb = NULL;
	else {
		tcb = (struct tcb *)(((uintptr_t)(addr) + 63) & ~63);
		bzero(tcb, sizeof(struct tcb));
		tcb->tcb_addr = addr;
		tcb->tcb_thread = thread;
		/* XXX - Allocate tdv/tls */
	}
	return (tcb);
}

void
_tcb_dtor(struct tcb *tcb)
{
	void *addr;

	addr = tcb->tcb_addr;
	tcb->tcb_addr = NULL;
	free(addr);
}

struct kcb *
_kcb_ctor(struct kse *kse)
{
	struct kcb *kcb;

	kcb = malloc(sizeof(struct kcb));
	if (kcb != NULL) {
		bzero(kcb, sizeof(struct kcb));
		kcb->kcb_kse = kse;
	}
	return (kcb);
}

void
_kcb_dtor(struct kcb *kcb)
{
	free(kcb);
}
