/*-
 * Copyright (c) 2015-2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/assym.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <machine/riscvreg.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/cpu.h>
#include <machine/proc.h>
#include <machine/cpufunc.h>
#include <machine/pte.h>
#include <machine/machdep.h>
#include <machine/vmparam.h>

#include <riscv/vmm/riscv.h>

ASSYM(KERNBASE, KERNBASE);
ASSYM(VM_MAXUSER_ADDRESS, VM_MAXUSER_ADDRESS);
ASSYM(VM_MAX_KERNEL_ADDRESS, VM_MAX_KERNEL_ADDRESS);
ASSYM(PMAP_MAPDEV_EARLY_SIZE, PMAP_MAPDEV_EARLY_SIZE);

ASSYM(PM_SATP, offsetof(struct pmap, pm_satp));

ASSYM(PCB_ONFAULT, offsetof(struct pcb, pcb_onfault));
ASSYM(PCB_SIZE, sizeof(struct pcb));
ASSYM(PCB_RA, offsetof(struct pcb, pcb_ra));
ASSYM(PCB_SP, offsetof(struct pcb, pcb_sp));
ASSYM(PCB_GP, offsetof(struct pcb, pcb_gp));
ASSYM(PCB_TP, offsetof(struct pcb, pcb_tp));
ASSYM(PCB_S, offsetof(struct pcb, pcb_s));
ASSYM(PCB_X, offsetof(struct pcb, pcb_x));
ASSYM(PCB_FCSR, offsetof(struct pcb, pcb_fcsr));

ASSYM(SF_UC, offsetof(struct sigframe, sf_uc));

ASSYM(PC_CURPCB, offsetof(struct pcpu, pc_curpcb));
ASSYM(PC_CURTHREAD, offsetof(struct pcpu, pc_curthread));

ASSYM(TD_PCB, offsetof(struct thread, td_pcb));
ASSYM(TD_FLAGS, offsetof(struct thread, td_flags));
ASSYM(TD_AST, offsetof(struct thread, td_ast));
ASSYM(TD_PROC, offsetof(struct thread, td_proc));
ASSYM(TD_FRAME, offsetof(struct thread, td_frame));
ASSYM(TD_MD, offsetof(struct thread, td_md));
ASSYM(TD_LOCK, offsetof(struct thread, td_lock));

ASSYM(TF_SIZE, TF_SIZE);
ASSYM(TF_RA, offsetof(struct trapframe, tf_ra));
ASSYM(TF_SP, offsetof(struct trapframe, tf_sp));
ASSYM(TF_GP, offsetof(struct trapframe, tf_gp));
ASSYM(TF_TP, offsetof(struct trapframe, tf_tp));
ASSYM(TF_T, offsetof(struct trapframe, tf_t));
ASSYM(TF_S, offsetof(struct trapframe, tf_s));
ASSYM(TF_A, offsetof(struct trapframe, tf_a));
ASSYM(TF_SEPC, offsetof(struct trapframe, tf_sepc));
ASSYM(TF_STVAL, offsetof(struct trapframe, tf_stval));
ASSYM(TF_SCAUSE, offsetof(struct trapframe, tf_scause));
ASSYM(TF_SSTATUS, offsetof(struct trapframe, tf_sstatus));

ASSYM(KF_TP, offsetof(struct kernframe, kf_tp));

ASSYM(HYP_H_RA, offsetof(struct hypctx, host_regs.hyp_ra));
ASSYM(HYP_H_SP, offsetof(struct hypctx, host_regs.hyp_sp));
ASSYM(HYP_H_GP, offsetof(struct hypctx, host_regs.hyp_gp));
ASSYM(HYP_H_TP, offsetof(struct hypctx, host_regs.hyp_tp));
ASSYM(HYP_H_T, offsetof(struct hypctx, host_regs.hyp_t));
ASSYM(HYP_H_S, offsetof(struct hypctx, host_regs.hyp_s));
ASSYM(HYP_H_A, offsetof(struct hypctx, host_regs.hyp_a));
ASSYM(HYP_H_SEPC, offsetof(struct hypctx, host_regs.hyp_sepc));
ASSYM(HYP_H_SSTATUS, offsetof(struct hypctx, host_regs.hyp_sstatus));
ASSYM(HYP_H_HSTATUS, offsetof(struct hypctx, host_regs.hyp_hstatus));
ASSYM(HYP_H_SSCRATCH, offsetof(struct hypctx, host_sscratch));
ASSYM(HYP_H_STVEC, offsetof(struct hypctx, host_stvec));
ASSYM(HYP_H_SCOUNTEREN, offsetof(struct hypctx, host_scounteren));

ASSYM(HYP_G_RA, offsetof(struct hypctx, guest_regs.hyp_ra));
ASSYM(HYP_G_SP, offsetof(struct hypctx, guest_regs.hyp_sp));
ASSYM(HYP_G_GP, offsetof(struct hypctx, guest_regs.hyp_gp));
ASSYM(HYP_G_TP, offsetof(struct hypctx, guest_regs.hyp_tp));
ASSYM(HYP_G_T, offsetof(struct hypctx, guest_regs.hyp_t));
ASSYM(HYP_G_S, offsetof(struct hypctx, guest_regs.hyp_s));
ASSYM(HYP_G_A, offsetof(struct hypctx, guest_regs.hyp_a));
ASSYM(HYP_G_SEPC, offsetof(struct hypctx, guest_regs.hyp_sepc));
ASSYM(HYP_G_SSTATUS, offsetof(struct hypctx, guest_regs.hyp_sstatus));
ASSYM(HYP_G_HSTATUS, offsetof(struct hypctx, guest_regs.hyp_hstatus));
ASSYM(HYP_G_SCOUNTEREN, offsetof(struct hypctx, guest_scounteren));

ASSYM(HYP_TRAP_SEPC, offsetof(struct hyptrap, sepc));
ASSYM(HYP_TRAP_SCAUSE, offsetof(struct hyptrap, scause));
ASSYM(HYP_TRAP_STVAL, offsetof(struct hyptrap, stval));
ASSYM(HYP_TRAP_HTVAL, offsetof(struct hyptrap, htval));
ASSYM(HYP_TRAP_HTINST, offsetof(struct hyptrap, htinst));

ASSYM(RISCV_BOOTPARAMS_SIZE, sizeof(struct riscv_bootparams));
ASSYM(RISCV_BOOTPARAMS_KERN_PHYS, offsetof(struct riscv_bootparams, kern_phys));
ASSYM(RISCV_BOOTPARAMS_KERN_STACK, offsetof(struct riscv_bootparams,
    kern_stack));
ASSYM(RISCV_BOOTPARAMS_DTBP_PHYS, offsetof(struct riscv_bootparams, dtbp_phys));
ASSYM(RISCV_BOOTPARAMS_MODULEP, offsetof(struct riscv_bootparams, modulep));
