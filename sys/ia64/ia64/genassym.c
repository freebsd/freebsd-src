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
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/ucontext.h>
#include <machine/frame.h>
#include <machine/mutex.h>
#include <machine/elf.h>
#include <machine/pal.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>
#include <net/if.h>
#include <netinet/in.h>

ASSYM(GD_CURTHREAD,	offsetof(struct globaldata, gd_curthread));
ASSYM(GD_FPCURTHREAD,	offsetof(struct globaldata, gd_fpcurthread));
ASSYM(GD_CURPCB,	offsetof(struct globaldata, gd_curpcb));
ASSYM(GD_SWITCHTIME,	offsetof(struct globaldata, gd_switchtime));
ASSYM(GD_CPUID,		offsetof(struct globaldata, gd_cpuid));

ASSYM(MTX_LOCK,		offsetof(struct mtx, mtx_lock));
ASSYM(MTX_RECURSE,	offsetof(struct mtx, mtx_recurse));
ASSYM(MTX_SAVECRIT,	offsetof(struct mtx, mtx_savecrit));
ASSYM(MTX_UNOWNED,	MTX_UNOWNED);

ASSYM(TD_PROC,		offsetof(struct thread, td_proc));
ASSYM(TD_PCB,		offsetof(struct thread, td_pcb));
ASSYM(TD_KSTACK,	offsetof(struct thread, td_kstack));
ASSYM(TD_MD_FLAGS,	offsetof(struct thread, td_md.md_flags));

ASSYM(VM_MAXUSER_ADDRESS, VM_MAXUSER_ADDRESS);

ASSYM(FRAME_SYSCALL,	FRAME_SYSCALL);

ASSYM(TF_CR_IPSR,	offsetof(struct trapframe, tf_cr_ipsr));
ASSYM(TF_CR_IFS,	offsetof(struct trapframe, tf_cr_ifs));
ASSYM(TF_NDIRTY,	offsetof(struct trapframe, tf_ndirty));
ASSYM(TF_AR_FPSR,	offsetof(struct trapframe, tf_ar_fpsr));
ASSYM(TF_B,		offsetof(struct trapframe, tf_b));
ASSYM(TF_R,		offsetof(struct trapframe, tf_r));
ASSYM(TF_R_R1,		offsetof(struct trapframe, tf_r[FRAME_R1]));
ASSYM(TF_R_R2,		offsetof(struct trapframe, tf_r[FRAME_R2]));
ASSYM(TF_R_R3,		offsetof(struct trapframe, tf_r[FRAME_R3]));
ASSYM(TF_R_R4,		offsetof(struct trapframe, tf_r[FRAME_R4]));
ASSYM(TF_R_R5,		offsetof(struct trapframe, tf_r[FRAME_R5]));
ASSYM(TF_R_R6,		offsetof(struct trapframe, tf_r[FRAME_R6]));
ASSYM(TF_R_R7,		offsetof(struct trapframe, tf_r[FRAME_R7]));
ASSYM(TF_R_R8,		offsetof(struct trapframe, tf_r[FRAME_R8]));
ASSYM(TF_R_R9,		offsetof(struct trapframe, tf_r[FRAME_R9]));
ASSYM(TF_R_R10,		offsetof(struct trapframe, tf_r[FRAME_R10]));
ASSYM(TF_R_R11,		offsetof(struct trapframe, tf_r[FRAME_R11]));
ASSYM(TF_R_SP,		offsetof(struct trapframe, tf_r[FRAME_SP]));
ASSYM(TF_R_R13,		offsetof(struct trapframe, tf_r[FRAME_R13]));
ASSYM(TF_R_R14,		offsetof(struct trapframe, tf_r[FRAME_R14]));
ASSYM(TF_R_R15,		offsetof(struct trapframe, tf_r[FRAME_R15]));
ASSYM(TF_F,		offsetof(struct trapframe, tf_f));

ASSYM(PCB_R4,		offsetof(struct pcb, pcb_r4));
ASSYM(PCB_R5,		offsetof(struct pcb, pcb_r5));
ASSYM(PCB_R6,		offsetof(struct pcb, pcb_r6));
ASSYM(PCB_R7,		offsetof(struct pcb, pcb_r7));

ASSYM(PCB_F2,		offsetof(struct pcb, pcb_f2));
ASSYM(PCB_F3,		offsetof(struct pcb, pcb_f3));
ASSYM(PCB_F4,		offsetof(struct pcb, pcb_f4));
ASSYM(PCB_F5,		offsetof(struct pcb, pcb_f5));

ASSYM(PCB_B0,		offsetof(struct pcb, pcb_b0));
ASSYM(PCB_B1,		offsetof(struct pcb, pcb_b1));
ASSYM(PCB_B2,		offsetof(struct pcb, pcb_b2));
ASSYM(PCB_B3,		offsetof(struct pcb, pcb_b3));
ASSYM(PCB_B4,		offsetof(struct pcb, pcb_b4));
ASSYM(PCB_B5,		offsetof(struct pcb, pcb_b5));

ASSYM(PCB_OLD_UNAT,	offsetof(struct pcb, pcb_old_unat));
ASSYM(PCB_SP,		offsetof(struct pcb, pcb_sp));
ASSYM(PCB_PFS,		offsetof(struct pcb, pcb_pfs));
ASSYM(PCB_BSPSTORE,	offsetof(struct pcb, pcb_bspstore));

ASSYM(PCB_UNAT,		offsetof(struct pcb, pcb_unat));
ASSYM(PCB_RNAT,		offsetof(struct pcb, pcb_rnat));
ASSYM(PCB_PR,		offsetof(struct pcb, pcb_pr));

ASSYM(PCB_ONFAULT,	offsetof(struct pcb, pcb_onfault));

ASSYM(PCB_HIGHFP,	offsetof(struct pcb, pcb_highfp));

ASSYM(UC_MCONTEXT_MC_AR_BSP,  offsetof(ucontext_t, uc_mcontext.mc_ar_bsp));
ASSYM(UC_MCONTEXT_MC_AR_RNAT, offsetof(ucontext_t, uc_mcontext.mc_ar_rnat));

ASSYM(EFAULT,		EFAULT);
ASSYM(ENAMETOOLONG,	ENAMETOOLONG);

ASSYM(PAGE_SHIFT,	PAGE_SHIFT); 
ASSYM(PAGE_SIZE,	PAGE_SIZE);
ASSYM(KSTACK_PAGES,	KSTACK_PAGES);

ASSYM(SIZEOF_TRAPFRAME,	sizeof(struct trapframe));
ASSYM(SIZEOF_PCB,	sizeof(struct pcb));

ASSYM(DT_NULL,		DT_NULL);
ASSYM(DT_RELA,		DT_RELA);
ASSYM(DT_RELASZ,	DT_RELASZ);
ASSYM(DT_SYMTAB,	DT_SYMTAB);
ASSYM(DT_SYMENT,	DT_SYMENT);
ASSYM(DT_RELAENT,	DT_RELAENT);
ASSYM(R_IA64_NONE,	R_IA64_NONE);
ASSYM(R_IA64_DIR64LSB,	R_IA64_DIR64LSB);
ASSYM(R_IA64_FPTR64LSB,	R_IA64_FPTR64LSB);
ASSYM(R_IA64_REL64LSB,	R_IA64_REL64LSB);

ASSYM(PAL_PTCE_INFO,	PAL_PTCE_INFO);
ASSYM(PAL_FREQ_RATIOS,	PAL_FREQ_RATIOS);
