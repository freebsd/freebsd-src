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
 *	from: src/sys/i386/i386/genassym.c,v 1.86.2.1 2000/05/16 06:58:06 dillon
 *	JNPR: genassym.c,v 1.4 2007/08/09 11:23:32 katta
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/assym.h>
#include <machine/pte.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/resourcevar.h>
#include <sys/ucontext.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/proc.h>
#include <machine/cpuregs.h>
#include <machine/pcb.h>
#include <machine/sigframe.h>
#include <machine/proc.h>

#ifndef offsetof
#define	offsetof(t,m) (int)((&((t *)0L)->m))
#endif


ASSYM(TD_PCB, offsetof(struct thread, td_pcb));
ASSYM(TD_UPTE, offsetof(struct thread, td_md.md_upte));
ASSYM(TD_KSTACK, offsetof(struct thread, td_kstack));
ASSYM(TD_FLAGS, offsetof(struct thread, td_flags));
ASSYM(TD_LOCK, offsetof(struct thread, td_lock));
ASSYM(TD_FRAME, offsetof(struct thread, td_frame));
ASSYM(TD_TLS, offsetof(struct thread, td_md.md_tls));

ASSYM(TF_REG_SR, offsetof(struct trapframe, sr));

ASSYM(U_PCB_REGS, offsetof(struct pcb, pcb_regs.zero));
ASSYM(U_PCB_CONTEXT, offsetof(struct pcb, pcb_context));
ASSYM(U_PCB_ONFAULT, offsetof(struct pcb, pcb_onfault));
ASSYM(U_PCB_FPREGS, offsetof(struct pcb, pcb_regs.f0));

ASSYM(PC_CURPCB, offsetof(struct pcpu, pc_curpcb));
ASSYM(PC_SEGBASE, offsetof(struct pcpu, pc_segbase));
ASSYM(PC_CURTHREAD, offsetof(struct pcpu, pc_curthread));
ASSYM(PC_FPCURTHREAD, offsetof(struct pcpu, pc_fpcurthread));
ASSYM(PC_CPUID, offsetof(struct pcpu, pc_cpuid));
ASSYM(PC_CURPMAP, offsetof(struct pcpu, pc_curpmap));

ASSYM(VM_MAX_KERNEL_ADDRESS, VM_MAX_KERNEL_ADDRESS);
ASSYM(VM_MAXUSER_ADDRESS, VM_MAXUSER_ADDRESS);
ASSYM(SIGF_UC, offsetof(struct sigframe, sf_uc));
ASSYM(SIGFPE, SIGFPE);
ASSYM(PAGE_SHIFT, PAGE_SHIFT);
ASSYM(PAGE_SIZE, PAGE_SIZE);
ASSYM(PAGE_MASK, PAGE_MASK);
ASSYM(SEGSHIFT, SEGSHIFT);
ASSYM(NPTEPG, NPTEPG);
ASSYM(TDF_NEEDRESCHED, TDF_NEEDRESCHED);
ASSYM(TDF_ASTPENDING, TDF_ASTPENDING);
ASSYM(PCPU_SIZE, sizeof(struct pcpu));
ASSYM(MAXCOMLEN, MAXCOMLEN);

ASSYM(MIPS_KSEG0_START, MIPS_KSEG0_START);
ASSYM(MIPS_KSEG1_START, MIPS_KSEG1_START);
ASSYM(MIPS_KSEG2_START, MIPS_KSEG2_START);
