/*
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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/user.h>

#include <machine/fpu.h>
#include <machine/psl.h>

void
enable_fpu(struct thread *td)
{
	int	msr, scratch;
	struct	pcb *pcb;
	struct	trapframe *tf;

	pcb = td->td_pcb;
	tf = trapframe(td);
	
	tf->srr1 |= PSL_FP;
	if (!(pcb->pcb_flags & PCB_FPU)) {
		memset(&pcb->pcb_fpu, 0, sizeof pcb->pcb_fpu);
		pcb->pcb_flags |= PCB_FPU;
	}
	__asm __volatile ("mfmsr %0; ori %1,%0,%2; mtmsr %1; isync"
			  : "=r"(msr), "=r"(scratch) : "K"(PSL_FP));
	__asm __volatile ("lfd 0,0(%0); mtfsf 0xff,0"
			  :: "b"(&pcb->pcb_fpu.fpscr));
	__asm ("lfd 0,0(%0);"
	       "lfd 1,8(%0);"
	       "lfd 2,16(%0);"
	       "lfd 3,24(%0);"
	       "lfd 4,32(%0);"
	       "lfd 5,40(%0);"
	       "lfd 6,48(%0);"
	       "lfd 7,56(%0);"
	       "lfd 8,64(%0);"
	       "lfd 9,72(%0);"
	       "lfd 10,80(%0);"
	       "lfd 11,88(%0);"
	       "lfd 12,96(%0);"
	       "lfd 13,104(%0);"
	       "lfd 14,112(%0);"
	       "lfd 15,120(%0);"
	       "lfd 16,128(%0);"
	       "lfd 17,136(%0);"
	       "lfd 18,144(%0);"
	       "lfd 19,152(%0);"
	       "lfd 20,160(%0);"
	       "lfd 21,168(%0);"
	       "lfd 22,176(%0);"
	       "lfd 23,184(%0);"
	       "lfd 24,192(%0);"
	       "lfd 25,200(%0);"
	       "lfd 26,208(%0);"
	       "lfd 27,216(%0);"
	       "lfd 28,224(%0);"
	       "lfd 29,232(%0);"
	       "lfd 30,240(%0);"
	       "lfd 31,248(%0)" :: "b"(&pcb->pcb_fpu.fpr[0]));
	__asm __volatile ("mtmsr %0; isync" :: "r"(msr));
}

void
save_fpu(struct thread *td)
{
	int	msr, scratch;
	struct	pcb *pcb;

	pcb = td->td_pcb;
	
	__asm __volatile ("mfmsr %0; ori %1,%0,%2; mtmsr %1; isync"
			  : "=r"(msr), "=r"(scratch) : "K"(PSL_FP));
	__asm ("stfd 0,0(%0);"
	       "stfd 1,8(%0);"
	       "stfd 2,16(%0);"
	       "stfd 3,24(%0);"
	       "stfd 4,32(%0);"
	       "stfd 5,40(%0);"
	       "stfd 6,48(%0);"
	       "stfd 7,56(%0);"
	       "stfd 8,64(%0);"
	       "stfd 9,72(%0);"
	       "stfd 10,80(%0);"
	       "stfd 11,88(%0);"
	       "stfd 12,96(%0);"
	       "stfd 13,104(%0);"
	       "stfd 14,112(%0);"
	       "stfd 15,120(%0);"
	       "stfd 16,128(%0);"
	       "stfd 17,136(%0);"
	       "stfd 18,144(%0);"
	       "stfd 19,152(%0);"
	       "stfd 20,160(%0);"
	       "stfd 21,168(%0);"
	       "stfd 22,176(%0);"
	       "stfd 23,184(%0);"
	       "stfd 24,192(%0);"
	       "stfd 25,200(%0);"
	       "stfd 26,208(%0);"
	       "stfd 27,216(%0);"
	       "stfd 28,224(%0);"
	       "stfd 29,232(%0);"
	       "stfd 30,240(%0);"
	       "stfd 31,248(%0)" :: "b"(&pcb->pcb_fpu.fpr[0]));
	__asm __volatile ("mffs 0; stfd 0,0(%0)" :: "b"(&pcb->pcb_fpu.fpscr));
	__asm __volatile ("mtmsr %0; isync" :: "r"(msr));
	pcb->pcb_fpcpu = NULL;
	PCPU_SET(fputhread, NULL);
}
