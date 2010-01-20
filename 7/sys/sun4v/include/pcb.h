/*-
 * Copyright (c) 2001 Jake Burkholder.
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

#ifndef	_MACHINE_PCB_H_
#define	_MACHINE_PCB_H_

#ifndef LOCORE
#include <machine/frame.h>
#endif

#define	MAXWIN	8

#define	PCB_FEF	(1 << 0)

#ifndef LOCORE

/* NOTE: pcb_ufp must be aligned on a 64 byte boundary. */
struct pcb {
	struct	rwindow pcb_rw[MAXWIN];    /* wbuf for page faults during spill / fill trap handling */ 
	uint32_t pcb_kfp[64];              /* used for regs in handling kernel floating point exc */
	uint32_t pcb_ufp[64];              /* used for regs in handling user floating point exc */
	uint64_t pcb_rwsp[MAXWIN];         /* spbuf sp's for each wbuf */
	uint64_t pcb_flags;
	uint64_t pcb_nsaved;               /* number of windows saved in pcb_rw */
	uint64_t pcb_pc;
	uint64_t pcb_sp;
	uint64_t pcb_kstack;               /* pcb's kernel stack */
	uint64_t pcb_tstate;
	uint64_t pcb_tpc;
	uint64_t pcb_tnpc;
	uint64_t pcb_tt;
	uint64_t pcb_sfar;
	uint64_t pcb_pad[7]; 
} __aligned(64);

#ifdef _KERNEL
void	makectx(struct trapframe *tf, struct pcb *pcb);
int	savectx(struct pcb *pcb);
#endif

#endif /* !LOCORE */

#endif /* !_MACHINE_PCB_H_ */
