/*-
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)genassym.c	5.11 (Berkeley) 5/10/91
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/assym.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/ktr.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <net/if.h>
#include <netinet/in.h>
#include <machine/pcb.h>
#include <machine/pmap.h>

ASSYM(PC_CURTHREAD, offsetof(struct pcpu, pc_curthread));
ASSYM(PC_CURPCB, offsetof(struct pcpu, pc_curpcb));
ASSYM(PC_SWITCHTIME, offsetof(struct pcpu, pc_switchtime));

ASSYM(MTX_LOCK, offsetof(struct mtx, mtx_lock));
ASSYM(MTX_RECURSECNT, offsetof(struct mtx, mtx_recurse));

ASSYM(PM_KERNELSR, offsetof(struct pmap, pm_sr[KERNEL_SR]));
ASSYM(PM_USERSR, offsetof(struct pmap, pm_sr[USER_SR]));

ASSYM(FRAMELEN, FRAMELEN);
ASSYM(FRAME_0, offsetof(struct trapframe, fixreg[0]));
ASSYM(FRAME_1, offsetof(struct trapframe, fixreg[1]));
ASSYM(FRAME_2, offsetof(struct trapframe, fixreg[2]));
ASSYM(FRAME_3, offsetof(struct trapframe, fixreg[3]));
ASSYM(FRAME_LR, offsetof(struct trapframe, lr));
ASSYM(FRAME_CR, offsetof(struct trapframe, cr));
ASSYM(FRAME_CTR, offsetof(struct trapframe, ctr));
ASSYM(FRAME_XER, offsetof(struct trapframe, xer));
ASSYM(FRAME_SRR0, offsetof(struct trapframe, srr0));
ASSYM(FRAME_SRR1, offsetof(struct trapframe, srr1));
ASSYM(FRAME_DAR, offsetof(struct trapframe, dar));
ASSYM(FRAME_DSISR, offsetof(struct trapframe, dsisr));
ASSYM(FRAME_EXC, offsetof(struct trapframe, exc));

ASSYM(SFRAMELEN, roundup(sizeof(struct switchframe), 16));

ASSYM(PCB_CONTEXT, offsetof(struct pcb, pcb_context));
ASSYM(PCB_CR, offsetof(struct pcb, pcb_cr));
ASSYM(PCB_PMR, offsetof(struct pcb, pcb_pmreal));
ASSYM(PCB_SP, offsetof(struct pcb, pcb_sp));
ASSYM(PCB_SPL, offsetof(struct pcb, pcb_spl));
ASSYM(PCB_ONFAULT, offsetof(struct pcb, pcb_onfault));
ASSYM(PCB_FLAGS, offsetof(struct pcb, pcb_flags));

ASSYM(TD_PROC, offsetof(struct thread, td_proc));
ASSYM(TD_PCB, offsetof(struct thread, td_pcb));
