/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2004 Olivier Houchard
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
#include <sys/cpuset.h>
#include <sys/systm.h>
#include <sys/assym.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/vmmeter.h>
#include <sys/bus.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <machine/armreg.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/cpu.h>
#include <machine/proc.h>
#include <machine/cpufunc.h>
#include <machine/cpuinfo.h>
#include <machine/intr.h>
#include <machine/sysarch.h>
#include <machine/vmparam.h>	/* For KERNVIRTADDR */

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_var.h>

ASSYM(KERNBASE, KERNBASE);
ASSYM(KERNVIRTADDR, KERNVIRTADDR);
ASSYM(CPU_ASID_KERNEL,CPU_ASID_KERNEL);
ASSYM(PCB_ONFAULT, offsetof(struct pcb, pcb_onfault));
ASSYM(PCB_PAGEDIR, offsetof(struct pcb, pcb_pagedir));
ASSYM(PCB_R4, offsetof(struct pcb, pcb_regs.sf_r4));
ASSYM(PCB_R5, offsetof(struct pcb, pcb_regs.sf_r5));
ASSYM(PCB_R6, offsetof(struct pcb, pcb_regs.sf_r6));
ASSYM(PCB_R7, offsetof(struct pcb, pcb_regs.sf_r7));
ASSYM(PCB_R8, offsetof(struct pcb, pcb_regs.sf_r8));
ASSYM(PCB_R9, offsetof(struct pcb, pcb_regs.sf_r9));
ASSYM(PCB_R10, offsetof(struct pcb, pcb_regs.sf_r10));
ASSYM(PCB_R11, offsetof(struct pcb, pcb_regs.sf_r11));
ASSYM(PCB_R12, offsetof(struct pcb, pcb_regs.sf_r12));
ASSYM(PCB_SP, offsetof(struct pcb, pcb_regs.sf_sp));
ASSYM(PCB_LR, offsetof(struct pcb, pcb_regs.sf_lr));
ASSYM(PCB_PC, offsetof(struct pcb, pcb_regs.sf_pc));
ASSYM(PCB_TPIDRURW, offsetof(struct pcb, pcb_regs.sf_tpidrurw));

ASSYM(PC_CURPCB, offsetof(struct pcpu, pc_curpcb));
ASSYM(PC_CURTHREAD, offsetof(struct pcpu, pc_curthread));
ASSYM(M_LEN, offsetof(struct mbuf, m_len));
ASSYM(M_DATA, offsetof(struct mbuf, m_data));
ASSYM(M_NEXT, offsetof(struct mbuf, m_next));
ASSYM(IP_SRC, offsetof(struct ip, ip_src));
ASSYM(IP_DST, offsetof(struct ip, ip_dst));

ASSYM(TD_PCB, offsetof(struct thread, td_pcb));
ASSYM(TD_FLAGS, offsetof(struct thread, td_flags));
ASSYM(TD_AST, offsetof(struct thread, td_ast));
ASSYM(TD_PROC, offsetof(struct thread, td_proc));
ASSYM(TD_MD, offsetof(struct thread, td_md));
ASSYM(TD_LOCK, offsetof(struct thread, td_lock));

ASSYM(TF_SPSR, offsetof(struct trapframe, tf_spsr));
ASSYM(TF_R0, offsetof(struct trapframe, tf_r0));
ASSYM(TF_R1, offsetof(struct trapframe, tf_r1));
ASSYM(TF_PC, offsetof(struct trapframe, tf_pc));
ASSYM(P_PID, offsetof(struct proc, p_pid));
ASSYM(P_FLAG, offsetof(struct proc, p_flag));

ASSYM(SIGF_UC, offsetof(struct sigframe, sf_uc));

#ifdef VFP
ASSYM(PCB_VFPSTATE, offsetof(struct pcb, pcb_vfpstate));
#endif

ASSYM(PC_CURPMAP, offsetof(struct pcpu, pc_curpmap));
ASSYM(PC_BP_HARDEN_KIND, offsetof(struct pcpu, pc_bp_harden_kind));
ASSYM(PCPU_BP_HARDEN_KIND_NONE, PCPU_BP_HARDEN_KIND_NONE);
ASSYM(PCPU_BP_HARDEN_KIND_BPIALL, PCPU_BP_HARDEN_KIND_BPIALL);
ASSYM(PCPU_BP_HARDEN_KIND_ICIALLU, PCPU_BP_HARDEN_KIND_ICIALLU);

ASSYM(PAGE_SIZE, PAGE_SIZE);
#ifdef PMAP_INCLUDE_PTE_SYNC
ASSYM(PMAP_INCLUDE_PTE_SYNC, 1);
#endif

ASSYM(MAXCOMLEN, MAXCOMLEN);
ASSYM(MAXCPU, MAXCPU);
ASSYM(_NCPUWORDS, _NCPUWORDS);
ASSYM(PCPU_SIZE, sizeof(struct pcpu));
ASSYM(P_VMSPACE, offsetof(struct proc, p_vmspace));
ASSYM(VM_PMAP, offsetof(struct vmspace, vm_pmap));
ASSYM(PM_ACTIVE, offsetof(struct pmap, pm_active));
ASSYM(PC_CPUID, offsetof(struct pcpu, pc_cpuid));
ASSYM(VM_MAXUSER_ADDRESS, VM_MAXUSER_ADDRESS);

ASSYM(DCACHE_LINE_SIZE, offsetof(struct cpuinfo, dcache_line_size));
ASSYM(DCACHE_LINE_MASK, offsetof(struct cpuinfo, dcache_line_mask));
ASSYM(ICACHE_LINE_SIZE, offsetof(struct cpuinfo, icache_line_size));
ASSYM(ICACHE_LINE_MASK, offsetof(struct cpuinfo, icache_line_mask));

/*
 * Emit the LOCORE_MAP_MB option as a #define only if the option was set.
 */
#include "opt_locore.h"

#ifdef LOCORE_MAP_MB
ASSYM(LOCORE_MAP_MB, LOCORE_MAP_MB);
#endif
