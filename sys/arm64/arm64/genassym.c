/*-
 * Copyright (c) 2004 Olivier Houchard
 * Copyright (c) 2014 Andrew Turner
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
 */

#include <sys/param.h>
#include <sys/assym.h>
#include <sys/bus.h>
#include <sys/pcpu.h>
#include <sys/proc.h>

#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/machdep.h>
#include <machine/pcb.h>

/* Sizeof arm64_bootparams, rounded to keep stack alignment */
ASSYM(BOOTPARAMS_SIZE, roundup2(sizeof(struct arm64_bootparams),
    STACKALIGNBYTES + 1));
ASSYM(BP_MODULEP, offsetof(struct arm64_bootparams, modulep));
ASSYM(BP_KERN_STACK, offsetof(struct arm64_bootparams, kern_stack));
ASSYM(BP_KERN_TTBR0, offsetof(struct arm64_bootparams, kern_ttbr0));
ASSYM(BP_BOOT_EL, offsetof(struct arm64_bootparams, boot_el));

ASSYM(PCPU_SIZE, sizeof(struct pcpu));
ASSYM(PC_CURPCB, offsetof(struct pcpu, pc_curpcb));
ASSYM(PC_CURTHREAD, offsetof(struct pcpu, pc_curthread));
ASSYM(PC_SSBD, offsetof(struct pcpu, pc_ssbd));

/* Size of pcb, rounded to keep stack alignment */
ASSYM(PCB_SIZE, roundup2(sizeof(struct pcb), STACKALIGNBYTES + 1));
ASSYM(PCB_SINGLE_STEP_SHIFT, PCB_SINGLE_STEP_SHIFT);
ASSYM(PCB_REGS, offsetof(struct pcb, pcb_x));
ASSYM(PCB_X19, PCB_X19);
ASSYM(PCB_SP, offsetof(struct pcb, pcb_sp));
ASSYM(PCB_TPIDRRO, offsetof(struct pcb, pcb_tpidrro_el0));
ASSYM(PCB_ONFAULT, offsetof(struct pcb, pcb_onfault));
ASSYM(PCB_FLAGS, offsetof(struct pcb, pcb_flags));

ASSYM(P_PID, offsetof(struct proc, p_pid));

ASSYM(SF_UC, offsetof(struct sigframe, sf_uc));

ASSYM(TD_PROC, offsetof(struct thread, td_proc));
ASSYM(TD_PCB, offsetof(struct thread, td_pcb));
ASSYM(TD_FLAGS, offsetof(struct thread, td_flags));
ASSYM(TD_AST, offsetof(struct thread, td_ast));
ASSYM(TD_FRAME, offsetof(struct thread, td_frame));
ASSYM(TD_LOCK, offsetof(struct thread, td_lock));
ASSYM(TD_MD_CANARY, offsetof(struct thread, td_md.md_canary));

ASSYM(TF_SIZE, sizeof(struct trapframe));
ASSYM(TF_SP, offsetof(struct trapframe, tf_sp));
ASSYM(TF_LR, offsetof(struct trapframe, tf_lr));
ASSYM(TF_ELR, offsetof(struct trapframe, tf_elr));
ASSYM(TF_SPSR, offsetof(struct trapframe, tf_spsr));
ASSYM(TF_ESR, offsetof(struct trapframe, tf_esr));
ASSYM(TF_X, offsetof(struct trapframe, tf_x));

ASSYM(INTR_ROOT_IRQ, INTR_ROOT_IRQ);
ASSYM(INTR_ROOT_FIQ, INTR_ROOT_FIQ);
