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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <machine/segments.h>
#include <machine/sysarch.h>
#include <string.h>
#include <rtld_tls.h>

#include "pthread_md.h"

struct tcb *
_tcb_ctor(struct pthread *thread, int initial)
{
#ifndef COMPAT_32BIT
	union descriptor ldt;
#endif
	struct tcb *tcb;
	void *oldtls;

	if (initial)
		__asm __volatile("movl %%gs:0, %0" : "=r" (oldtls));
	else
		oldtls = NULL;

	tcb = _rtld_allocate_tls(oldtls, sizeof(struct tcb), 16);
	if (tcb) {
		tcb->tcb_thread = thread;
#ifndef COMPAT_32BIT
		ldt.sd.sd_hibase = (unsigned int)tcb >> 24;
		ldt.sd.sd_lobase = (unsigned int)tcb & 0xFFFFFF;
		ldt.sd.sd_hilimit = (sizeof(struct tcb) >> 16) & 0xF;
		ldt.sd.sd_lolimit = sizeof(struct tcb) & 0xFFFF;
		ldt.sd.sd_type = SDT_MEMRWA;
		ldt.sd.sd_dpl = SEL_UPL;
		ldt.sd.sd_p = 1;
		ldt.sd.sd_xx = 0;
		ldt.sd.sd_def32 = 1;
		ldt.sd.sd_gran = 0;	/* no more than 1M */
		tcb->tcb_ldt = i386_set_ldt(LDT_AUTO_ALLOC, &ldt, 1);
		if (tcb->tcb_ldt < 0) {
			_rtld_free_tls(tcb, sizeof(struct tcb), 16);
			tcb = NULL;
		}
#endif
	}
	return (tcb);
}

void
_tcb_dtor(struct tcb *tcb)
{
#ifndef COMPAT_32BIT
	if (tcb->tcb_ldt >= 0)
		i386_set_ldt(tcb->tcb_ldt, NULL, 1);
#endif
	_rtld_free_tls(tcb, sizeof(struct tcb), 16);
}
