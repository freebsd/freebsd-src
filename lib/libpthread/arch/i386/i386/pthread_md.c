/*-
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
#include <machine/cpufunc.h>
#include <machine/segments.h>
#include <machine/sysarch.h>

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>

#include "rtld_tls.h"
#include "pthread_md.h"

struct tcb *
_tcb_ctor(struct pthread *thread, int initial)
{
	struct tcb *tcb;
	void *oldtls;

	if (initial) {
		__asm __volatile("movl %%gs:0, %0" : "=r" (oldtls));
	} else {
		oldtls = NULL;
	}

	tcb = _rtld_allocate_tls(oldtls, sizeof(struct tcb), 16);
	if (tcb) {
		tcb->tcb_thread = thread;
		tcb->tcb_spare = 0;
		bzero(&tcb->tcb_tmbx, sizeof(tcb->tcb_tmbx));
	}

	return (tcb);
}

void
_tcb_dtor(struct tcb *tcb)
{
	_rtld_free_tls(tcb, sizeof(struct tcb), 16);
}

/*
 * Initialize KSD.  This also includes setting up the LDT.
 */
struct kcb *
_kcb_ctor(struct kse *kse)
{
#ifndef COMPAT_32BIT
	union descriptor ldt;
#endif
	struct kcb *kcb;

	kcb = malloc(sizeof(struct kcb));
	if (kcb != NULL) {
		bzero(kcb, sizeof(struct kcb));
		kcb->kcb_self = kcb;
		kcb->kcb_kse = kse;
#ifndef COMPAT_32BIT
		ldt.sd.sd_hibase = (unsigned int)kcb >> 24;
		ldt.sd.sd_lobase = (unsigned int)kcb & 0xFFFFFF;
		ldt.sd.sd_hilimit = (sizeof(struct kcb) >> 16) & 0xF;
		ldt.sd.sd_lolimit = sizeof(struct kcb) & 0xFFFF;
		ldt.sd.sd_type = SDT_MEMRWA;
		ldt.sd.sd_dpl = SEL_UPL;
		ldt.sd.sd_p = 1;
		ldt.sd.sd_xx = 0;
		ldt.sd.sd_def32 = 1;
		ldt.sd.sd_gran = 0;	/* no more than 1M */
		kcb->kcb_ldt = i386_set_ldt(LDT_AUTO_ALLOC, &ldt, 1);
		if (kcb->kcb_ldt < 0) {
			free(kcb);
			return (NULL);
		}
#endif
	}
	return (kcb);
}

void
_kcb_dtor(struct kcb *kcb)
{
#ifndef COMPAT_32BIT
	if (kcb->kcb_ldt >= 0) {
		i386_set_ldt(kcb->kcb_ldt, NULL, 1);
		kcb->kcb_ldt = -1;	/* just in case */
	}
#endif
	free(kcb);
}
