/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	$NetBSD: pcb.h,v 1.4 2000/06/04 11:57:17 tsubai Exp $
 * $FreeBSD$
 */

#ifndef _MACHINE_PCB_H_
#define	_MACHINE_PCB_H_

typedef int faultbuf[23];

struct pcb {
	register_t	pcb_context[20];	/* non-volatile r14-r31 */
	register_t	pcb_cr;			/* Condition register */
	register_t	pcb_sp;			/* stack pointer */
	register_t	pcb_lr;			/* link register */
	register_t	pcb_usr;		/* USER_SR segment register */
	struct		pmap *pcb_pm;		/* pmap of our vmspace */
	struct		pmap *pcb_pmreal;	 /* real address of above */
	faultbuf	*pcb_onfault;		/* For use during
						    copyin/copyout */
	int		pcb_flags;
#define	PCB_FPU		1	/* Process had FPU initialized */
	struct fpu {
		double	fpr[32];
		double	fpscr;	/* FPSCR stored as double for easier access */
	} pcb_fpu;		/* Floating point processor */
	unsigned int	pcb_fpcpu;		/* which CPU had our FPU
							stuff. */
};

#ifdef	_KERNEL

#ifndef curpcb
extern struct pcb *curpcb;
#endif

extern struct pmap *curpm;
extern struct proc *fpuproc;

#endif
#endif	/* _MACHINE_PCB_H_ */
