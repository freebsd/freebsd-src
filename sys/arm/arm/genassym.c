/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/assym.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/vmparam.h>
#include <machine/armreg.h>
#include <machine/pcb.h>
#include <machine/cpu.h>
#include <machine/proc.h>
#include <machine/cpufunc.h>
#include <machine/pcb.h>
#include <machine/pte.h>
#include <machine/intr.h>
#include <machine/sysarch.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_var.h>

ASSYM(KERNBASE, KERNBASE);
ASSYM(PCB_NOALIGNFLT, PCB_NOALIGNFLT);
ASSYM(PCB_ONFAULT, offsetof(struct pcb, pcb_onfault));
ASSYM(PCB_DACR, offsetof(struct pcb, pcb_dacr));
ASSYM(PCB_FLAGS, offsetof(struct pcb, pcb_flags));
ASSYM(PCB_UND_SP, offsetof(struct pcb, un_32.pcb32_und_sp));
ASSYM(PCB_PAGEDIR, offsetof(struct pcb, pcb_pagedir));
ASSYM(PCB_L1VEC, offsetof(struct pcb, pcb_l1vec));
ASSYM(PCB_PL1VEC, offsetof(struct pcb, pcb_pl1vec));
ASSYM(PCB_R8, offsetof(struct pcb, un_32.pcb32_r8));
ASSYM(PCB_R9, offsetof(struct pcb, un_32.pcb32_r9));
ASSYM(PCB_R10, offsetof(struct pcb, un_32.pcb32_r10));
ASSYM(PCB_R11, offsetof(struct pcb, un_32.pcb32_r11));
ASSYM(PCB_R12, offsetof(struct pcb, un_32.pcb32_r12));
ASSYM(PCB_PC, offsetof(struct pcb, un_32.pcb32_pc));
ASSYM(PCB_SP, offsetof(struct pcb, un_32.pcb32_sp));

ASSYM(PC_CURPCB, offsetof(struct pcpu, pc_curpcb));
ASSYM(PC_CURTHREAD, offsetof(struct pcpu, pc_curthread));
ASSYM(M_LEN, offsetof(struct mbuf, m_len));
ASSYM(M_DATA, offsetof(struct mbuf, m_data));
ASSYM(M_NEXT, offsetof(struct mbuf, m_next));
ASSYM(IP_SRC, offsetof(struct ip, ip_src));
ASSYM(IP_DST, offsetof(struct ip, ip_dst));
ASSYM(CF_SETTTB, offsetof(struct cpu_functions, cf_setttb));
ASSYM(CF_CONTROL, offsetof(struct cpu_functions, cf_control));
ASSYM(CF_CONTEXT_SWITCH, offsetof(struct cpu_functions, cf_context_switch));
ASSYM(CF_DCACHE_WB_RANGE, offsetof(struct cpu_functions, cf_dcache_wb_range));
ASSYM(CF_IDCACHE_WBINV_ALL, offsetof(struct cpu_functions, cf_idcache_wbinv_all));
ASSYM(CF_TLB_FLUSHID_SE, offsetof(struct cpu_functions, cf_tlb_flushID_SE));
ASSYM(CF_ICACHE_SYNC, offsetof(struct cpu_functions, cf_icache_sync_all));

ASSYM(V_TRAP, offsetof(struct vmmeter, v_trap));
ASSYM(V_SOFT, offsetof(struct vmmeter, v_soft));
ASSYM(V_INTR, offsetof(struct vmmeter, v_intr));

ASSYM(TD_PCB, offsetof(struct thread, td_pcb));
ASSYM(TD_FLAGS, offsetof(struct thread, td_flags));
ASSYM(TD_PROC, offsetof(struct thread, td_proc));
ASSYM(TD_FRAME, offsetof(struct thread, td_frame));
ASSYM(TD_MD, offsetof(struct thread, td_md));
ASSYM(MD_TP, offsetof(struct mdthread, md_tp));

ASSYM(TF_R0, offsetof(struct trapframe, tf_r0));
ASSYM(TF_R1, offsetof(struct trapframe, tf_r1));
ASSYM(TF_PC, offsetof(struct trapframe, tf_pc));
ASSYM(P_PID, offsetof(struct proc, p_pid));
ASSYM(P_FLAG, offsetof(struct proc, p_flag));

ASSYM(ARM_TP_ADDRESS, ARM_TP_ADDRESS);
ASSYM(PAGE_SIZE, PAGE_SIZE);
ASSYM(PDESIZE, PDESIZE);
ASSYM(PMAP_DOMAIN_KERNEL, PMAP_DOMAIN_KERNEL);
#ifdef PMAP_INCLUDE_PTE_SYNC
ASSYM(PMAP_INCLUDE_PTE_SYNC, 1);
#endif
ASSYM(TDF_ASTPENDING, TDF_ASTPENDING);
ASSYM(TDF_NEEDRESCHED, TDF_NEEDRESCHED);
ASSYM(P_TRACED, P_TRACED);
ASSYM(P_SIGEVENT, P_SIGEVENT);
ASSYM(P_PROFIL, P_PROFIL);
ASSYM(TRAPFRAMESIZE, sizeof(struct trapframe));

ASSYM(MAXCOMLEN, MAXCOMLEN);
ASSYM(NIRQ, NIRQ);
