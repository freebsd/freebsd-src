/*-
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$NetBSD: fpu.c,v 1.5 2001/07/22 11:29:46 wiz Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/limits.h>

#include <machine/altivec.h>
#include <machine/pcb.h>
#include <machine/psl.h>

static void
save_vec_int(struct thread *td)
{
	int	msr;
	struct	pcb *pcb;

	pcb = td->td_pcb;

	/*
	 * Temporarily re-enable the vector unit during the save
	 */
	msr = mfmsr();
	mtmsr(msr | PSL_VEC);
	isync();

	/*
	 * Save the vector registers and SPEFSCR to the PCB
	 */
#define EVSTDW(n)   __asm ("evstdw %1,0(%0)" \
		:: "b"(pcb->pcb_vec.vr[n]), "n"(n));
	EVSTDW(0);	EVSTDW(1);	EVSTDW(2);	EVSTDW(3);
	EVSTDW(4);	EVSTDW(5);	EVSTDW(6);	EVSTDW(7);
	EVSTDW(8);	EVSTDW(9);	EVSTDW(10);	EVSTDW(11);
	EVSTDW(12);	EVSTDW(13);	EVSTDW(14);	EVSTDW(15);
	EVSTDW(16);	EVSTDW(17);	EVSTDW(18);	EVSTDW(19);
	EVSTDW(20);	EVSTDW(21);	EVSTDW(22);	EVSTDW(23);
	EVSTDW(24);	EVSTDW(25);	EVSTDW(26);	EVSTDW(27);
	EVSTDW(28);	EVSTDW(29);	EVSTDW(30);	EVSTDW(31);
#undef EVSTDW

	__asm ( "evxor 0,0,0\n"
		"evaddumiaaw 0,0\n"
		"evstdd 0,0(%0)" :: "b"(&pcb->pcb_vec.vr[17][0]));
	pcb->pcb_vec.vscr = mfspr(SPR_SPEFSCR);

	/*
	 * Disable vector unit again
	 */
	isync();
	mtmsr(msr);

}

void
enable_vec(struct thread *td)
{
	int	msr;
	struct	pcb *pcb;
	struct	trapframe *tf;

	pcb = td->td_pcb;
	tf = trapframe(td);

	/*
	 * Save the thread's SPE CPU number, and set the CPU's current
	 * vector thread
	 */
	td->td_pcb->pcb_veccpu = PCPU_GET(cpuid);
	PCPU_SET(vecthread, td);

	/*
	 * Enable the vector unit for when the thread returns from the
	 * exception. If this is the first time the unit has been used by
	 * the thread, initialise the vector registers and VSCR to 0, and
	 * set the flag to indicate that the vector unit is in use.
	 */
	tf->srr1 |= PSL_VEC;
	if (!(pcb->pcb_flags & PCB_VEC)) {
		memset(&pcb->pcb_vec, 0, sizeof pcb->pcb_vec);
		pcb->pcb_flags |= PCB_VEC;
	}

	/*
	 * Temporarily enable the vector unit so the registers
	 * can be restored.
	 */
	msr = mfmsr();
	mtmsr(msr | PSL_VEC);
	isync();

	/* Restore SPEFSCR and ACC.  Use %r0 as the scratch for ACC. */
	mtspr(SPR_SPEFSCR, pcb->pcb_vec.vscr);
	__asm __volatile("evldd 0, 0(%0); evmra 0,0\n"
	    :: "b"(&pcb->pcb_vec.vr[17][0]));

	/* 
	 * The lower half of each register will be restored on trap return.  Use
	 * %r0 as a scratch register, and restore it last.
	 */
#define	EVLDW(n)   __asm __volatile("evldw 0, 0(%0); evmergehilo "#n",0,"#n \
	    :: "b"(&pcb->pcb_vec.vr[n]));
	EVLDW(1);	EVLDW(2);	EVLDW(3);	EVLDW(4);
	EVLDW(5);	EVLDW(6);	EVLDW(7);	EVLDW(8);
	EVLDW(9);	EVLDW(10);	EVLDW(11);	EVLDW(12);
	EVLDW(13);	EVLDW(14);	EVLDW(15);	EVLDW(16);
	EVLDW(17);	EVLDW(18);	EVLDW(19);	EVLDW(20);
	EVLDW(21);	EVLDW(22);	EVLDW(23);	EVLDW(24);
	EVLDW(25);	EVLDW(26);	EVLDW(27);	EVLDW(28);
	EVLDW(29);	EVLDW(30);	EVLDW(31);	EVLDW(0);
#undef EVLDW

	isync();
	mtmsr(msr);
}

void
save_vec(struct thread *td)
{
	struct pcb *pcb;

	pcb = td->td_pcb;

	save_vec_int(td);

	/*
	 * Clear the current vec thread and pcb's CPU id
	 * XXX should this be left clear to allow lazy save/restore ?
	 */
	pcb->pcb_veccpu = INT_MAX;
	PCPU_SET(vecthread, NULL);
}

/*
 * Save SPE state without dropping ownership.  This will only save state if
 * the current vector-thread is `td'.
 */
void
save_vec_nodrop(struct thread *td)
{
	struct thread *vtd;

	vtd = PCPU_GET(vecthread);
	if (td != vtd) {
		return;
	}

	save_vec_int(td);
}
