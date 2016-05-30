/*-
 * Copyright (c) 2016 Robert N. M. Watson
 * Copyright (c) 2015-2016 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/unistd.h>
#include <sys/ktrace.h>

#include <machine/cheri.h>
#include <machine/pcb.h>

void
ktrccall_mdfill(struct pcb *pcb, struct ktr_ccall *kc)
{

	cheri_serialize(&kc->ktr_pcc, &pcb->pcb_regs.c1);
	cheri_serialize(&kc->ktr_idc, &pcb->pcb_regs.c2);
	kc->ktr_method = pcb->pcb_regs.v0;
}

void
ktrcreturn_mdfill(struct pcb *pcb, struct ktr_creturn *kr)
{

	cheri_serialize(&kr->ktr_cret, &pcb->pcb_regs.c3);
	kr->ktr_iret = pcb->pcb_regs.v0;
}

void
ktrcexception_mdfill(struct trapframe *frame,
    struct ktr_cexception *ke)
{
	struct chericap *cp;
	register_t cause;

	/* XXXCHERI: Should translate to MI exception code? */
	cause = frame->capcause;
	ke->ktr_exccode = (cause & CHERI_CAPCAUSE_EXCCODE_MASK) >>
	    CHERI_CAPCAUSE_EXCCODE_SHIFT;
	ke->ktr_regnum = cause & CHERI_CAPCAUSE_REGNUM_MASK;

	cp =
	    ke->ktr_regnum == CHERI_CR_DDC ? &frame->ddc :
	    ke->ktr_regnum == CHERI_CR_C1 ? &frame->c1 :
	    ke->ktr_regnum == CHERI_CR_C2 ? &frame->c2 :
	    ke->ktr_regnum == CHERI_CR_C3 ? &frame->c3 :
	    ke->ktr_regnum == CHERI_CR_C4 ? &frame->c4 :
	    ke->ktr_regnum == CHERI_CR_C5 ? &frame->c5 :
	    ke->ktr_regnum == CHERI_CR_C6 ? &frame->c6 :
	    ke->ktr_regnum == CHERI_CR_C7 ? &frame->c7 :
	    ke->ktr_regnum == CHERI_CR_C8 ? &frame->c8 :
	    ke->ktr_regnum == CHERI_CR_C9 ? &frame->c9 :
	    ke->ktr_regnum == CHERI_CR_C10 ? &frame->c10 :
	    ke->ktr_regnum == CHERI_CR_STC ? &frame->stc :
	    ke->ktr_regnum == CHERI_CR_C12 ? &frame->c12 :
	    ke->ktr_regnum == CHERI_CR_C13 ? &frame->c13 :
	    ke->ktr_regnum == CHERI_CR_C14 ? &frame->c14 :
	    ke->ktr_regnum == CHERI_CR_C15 ? &frame->c15 :
	    ke->ktr_regnum == CHERI_CR_C16 ? &frame->c16 :
	    ke->ktr_regnum == CHERI_CR_C17 ? &frame->c17 :
	    ke->ktr_regnum == CHERI_CR_C18 ? &frame->c18 :
	    ke->ktr_regnum == CHERI_CR_C19 ? &frame->c19 :
	    ke->ktr_regnum == CHERI_CR_C20 ? &frame->c20 :
	    ke->ktr_regnum == CHERI_CR_C21 ? &frame->c21 :
	    ke->ktr_regnum == CHERI_CR_C22 ? &frame->c22 :
	    ke->ktr_regnum == CHERI_CR_C23 ? &frame->c23 :
	    ke->ktr_regnum == CHERI_CR_C24 ? &frame->c24 :
	    ke->ktr_regnum == CHERI_CR_C25 ? &frame->c25 :
	    ke->ktr_regnum == CHERI_CR_IDC ? &frame->idc :
	    ke->ktr_regnum == 0xff ? &frame->pcc :
	    NULL;
	if (cp != NULL)
		cheri_serialize(&ke->ktr_cap, cp);
	else
		bzero(&ke->ktr_cap, sizeof(ke->ktr_cap));
}
