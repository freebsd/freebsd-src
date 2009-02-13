/*	$NetBSD: pcb.h,v 1.10 2003/10/13 21:46:39 scw Exp $	*/

/*-
 * Copyright (c) 2001 Matt Thomas <matt@3am-software.com>.
 * Copyright (c) 1994 Mark Brinicombe.
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
 *	This product includes software developed by the RiscBSD team.
 * 4. The name "RiscBSD" nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RISCBSD ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL RISCBSD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_MACHINE_PCB_H_
#define	_MACHINE_PCB_H_

#include <machine/frame.h>
#include <machine/fp.h>


struct trapframe;

struct pcb_arm32 {
	vm_offset_t	pcb32_pagedir;		/* PT hooks */
	uint32_t *pcb32_pl1vec;		/* PTR to vector_base L1 entry*/
	uint32_t pcb32_l1vec;			/* Value to stuff on ctx sw */
	u_int	pcb32_dacr;			/* Domain Access Control Reg */
	/*
	 * WARNING!
	 * cpuswitch.S relies on pcb32_r8 being quad-aligned in struct pcb
	 * (due to the use of "strd" when compiled for XSCALE)
	 */
	u_int	pcb32_r8;			/* used */
	u_int	pcb32_r9;			/* used */
	u_int	pcb32_r10;			/* used */
	u_int	pcb32_r11;			/* used */
	u_int	pcb32_r12;			/* used */
	u_int	pcb32_sp;			/* used */
	u_int	pcb32_lr;
	u_int	pcb32_pc;
	u_int	pcb32_und_sp;
};
#define	pcb_pagedir	un_32.pcb32_pagedir
#define	pcb_pl1vec	un_32.pcb32_pl1vec
#define	pcb_l1vec	un_32.pcb32_l1vec
#define	pcb_dacr	un_32.pcb32_dacr
#define	pcb_cstate	un_32.pcb32_cstate

/*
 * WARNING!
 * See warning for struct pcb_arm32, above, before changing struct pcb!
 */
struct pcb {
	u_int	pcb_flags;
#define	PCB_OWNFPU	0x00000001
#define PCB_NOALIGNFLT	0x00000002
	caddr_t	pcb_onfault;			/* On fault handler */
	struct	pcb_arm32 un_32;
	struct	fpe_sp_state pcb_fpstate;	/* Floating Point state */
};

/*
 * No additional data for core dumps.
 */
struct md_coredump {
	int	md_empty;
};

void	makectx(struct trapframe *tf, struct pcb *pcb);

#ifdef _KERNEL

void    savectx(struct pcb *);
#endif	/* _KERNEL */

#endif	/* !_MACHINE_PCB_H_ */
