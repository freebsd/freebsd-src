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
#include <sys/assym.h>
#include <sys/errno.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/signal.h>
#include <sys/smp.h>
#include <sys/systm.h>
#include <sys/ucontext.h>
#include <sys/user.h>
#include <sys/ucontext.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/sigframe.h>

ASSYM(PC_CURTHREAD, offsetof(struct pcpu, pc_curthread));
ASSYM(PC_CURPCB, offsetof(struct pcpu, pc_curpcb));
ASSYM(PC_CURPMAP, offsetof(struct pcpu, pc_curpmap));

ASSYM(MTX_LOCK, offsetof(struct mtx, mtx_lock));
ASSYM(MTX_RECURSECNT, offsetof(struct mtx, mtx_recurse));

ASSYM(PM_KERNELSR, offsetof(struct pmap, pm_sr[KERNEL_SR]));
ASSYM(PM_USRSR, offsetof(struct pmap, pm_sr[USER_SR]));
ASSYM(PM_SR, offsetof(struct pmap, pm_sr));

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

ASSYM(IFRAMELEN, IFRAMELEN);
ASSYM(IFRAME_R1, offsetof(struct intrframe, r1));
ASSYM(IFRAME_SRR1, offsetof(struct intrframe, srr1));
ASSYM(IFRAME_SRR0, offsetof(struct intrframe, srr0));
ASSYM(IFRAME_PRI, offsetof(struct intrframe, pri));
ASSYM(IFRAME_INTR_DEPTH, offsetof(struct intrframe, intr_depth));
ASSYM(IFRAME_VRSAVE, offsetof(struct intrframe, vrsave));
ASSYM(IFRAME_CTR, offsetof(struct intrframe, ctr));
ASSYM(IFRAME_XER, offsetof(struct intrframe, xer));
ASSYM(IFRAME_CR, offsetof(struct intrframe, cr));
ASSYM(IFRAME_LR, offsetof(struct intrframe, lr));
ASSYM(IFRAME_R12, offsetof(struct intrframe, r12));
ASSYM(IFRAME_R11, offsetof(struct intrframe, r11));
ASSYM(IFRAME_R10, offsetof(struct intrframe, r10));
ASSYM(IFRAME_R9, offsetof(struct intrframe, r9));
ASSYM(IFRAME_R8, offsetof(struct intrframe, r8));
ASSYM(IFRAME_R7, offsetof(struct intrframe, r7));
ASSYM(IFRAME_R6, offsetof(struct intrframe, r6));
ASSYM(IFRAME_R5, offsetof(struct intrframe, r5));
ASSYM(IFRAME_R4, offsetof(struct intrframe, r4));
ASSYM(IFRAME_R3, offsetof(struct intrframe, r3));
ASSYM(IFRAME_R0, offsetof(struct intrframe, r0));

ASSYM(SPFRAMELEN, SPFRAMELEN);
ASSYM(SPFRAME_R1, offsetof(struct spillframe, r1));
ASSYM(SPFRAME_R12, offsetof(struct spillframe, r12));
ASSYM(SPFRAME_R11, offsetof(struct spillframe, r11));
ASSYM(SPFRAME_R10, offsetof(struct spillframe, r10));
ASSYM(SPFRAME_R9, offsetof(struct spillframe, r9));
ASSYM(SPFRAME_R8, offsetof(struct spillframe, r8));
ASSYM(SPFRAME_R7, offsetof(struct spillframe, r7));
ASSYM(SPFRAME_R6, offsetof(struct spillframe, r6));
ASSYM(SPFRAME_R5, offsetof(struct spillframe, r5));
ASSYM(SPFRAME_R4, offsetof(struct spillframe, r4));
ASSYM(SPFRAME_R3, offsetof(struct spillframe, r3));
ASSYM(SPFRAME_R0, offsetof(struct spillframe, r0));

ASSYM(CF_FUNC, offsetof(struct callframe, cf_func));
ASSYM(CF_ARG0, offsetof(struct callframe, cf_arg0));
ASSYM(CF_ARG1, offsetof(struct callframe, cf_arg1));

ASSYM(PCB_CONTEXT, offsetof(struct pcb, pcb_context));
ASSYM(PCB_CR, offsetof(struct pcb, pcb_cr));
ASSYM(PCB_PMR, offsetof(struct pcb, pcb_pmreal));
ASSYM(PCB_SP, offsetof(struct pcb, pcb_sp));
ASSYM(PCB_LR, offsetof(struct pcb, pcb_lr));
ASSYM(PCB_USR, offsetof(struct pcb, pcb_usr));
ASSYM(PCB_ONFAULT, offsetof(struct pcb, pcb_onfault));
ASSYM(PCB_FLAGS, offsetof(struct pcb, pcb_flags));

ASSYM(TD_PROC, offsetof(struct thread, td_proc));
ASSYM(TD_PCB, offsetof(struct thread, td_pcb));

ASSYM(P_VMSPACE, offsetof(struct proc, p_vmspace));

ASSYM(VM_PMAP, offsetof(struct vmspace, vm_pmap));

ASSYM(TD_FLAGS, offsetof(struct thread, td_flags));

ASSYM(TDF_ASTPENDING, TDF_ASTPENDING);
ASSYM(TDF_NEEDRESCHED, TDF_NEEDRESCHED);

ASSYM(SF_UC, offsetof(struct sigframe, sf_uc));
