/*
 * Copyright (c) 2003, Jeffrey Roberson <jeff@freebsd.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <machine/sysarch.h>
#include <machine/segments.h>

#include "thr_private.h"
#include "rtld_tls.h"

/* in _curthread.S */
extern void _set_gs(int);

struct tcb {
	struct tcb		*tcb_self;	/* required by rtld */
	void			*tcb_dtv;	/* required by rtld */
	struct pthread		*tcb_thread;
	int			tcb_ldt;
};

void
_retire_thread(void *entry)
{
	struct tcb *tcb = (struct tcb *)entry;

	i386_set_ldt(tcb->tcb_ldt, NULL, 1);
	_rtld_free_tls(tcb, sizeof(struct tcb), 16);
}

void *
_set_curthread(ucontext_t *uc, struct pthread *thr, int *err)
{
	union descriptor desc;
	struct tcb *tcb;
	void *oldtls;
	int ldt_index;

	*err = 0;

	if (uc == NULL && thr->arch_id != NULL) {
		return (thr->arch_id);
	}

	if (uc == NULL) {
		__asm __volatile("movl %%gs:0, %0" : "=r" (oldtls));
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

	bzero(&desc, sizeof(desc));

	/*
	 * Set up the descriptor to point at the TLS block.
	 */
	desc.sd.sd_lolimit = 0xFFFF;
	desc.sd.sd_lobase = (unsigned int)tcb & 0xFFFFFF;
	desc.sd.sd_type = SDT_MEMRW;
	desc.sd.sd_dpl = SEL_UPL;
	desc.sd.sd_p = 1;
	desc.sd.sd_hilimit = 0xF;
	desc.sd.sd_xx = 0;
	desc.sd.sd_def32 = 1;
	desc.sd.sd_gran = 1;
	desc.sd.sd_hibase = (unsigned int)tcb >> 24;

	/* Get a slot from the process' LDT list */
	ldt_index = i386_set_ldt(LDT_AUTO_ALLOC, &desc, 1);
	if (ldt_index == -1)
		abort();
	tcb->tcb_ldt = ldt_index;
	/*
	 * Set up our gs with the index into the ldt for this entry.
	 */
	if (uc != NULL)
		uc->uc_mcontext.mc_gs = LSEL(ldt_index, SEL_UPL);
	else
		_set_gs(LSEL(ldt_index, SEL_UPL));

	return (tcb);
}
