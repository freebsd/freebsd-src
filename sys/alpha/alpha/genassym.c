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
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/ktr.h>
#include <machine/frame.h>
#include <machine/chipset.h>
#include <machine/globaldata.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>
#include <net/if.h>
#include <netinet/in.h>
#include <nfs/nfsproto.h>
#include <nfs/rpcv2.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsdiskless.h>

ASSYM(GD_CURTHREAD, offsetof(struct globaldata, gd_curthread));
ASSYM(GD_FPCURTHREAD, offsetof(struct globaldata, gd_fpcurthread));
ASSYM(GD_CURPCB, offsetof(struct globaldata, gd_curpcb));
ASSYM(GD_SWITCHTIME, offsetof(struct globaldata, gd_switchtime));
ASSYM(GD_CPUID, offsetof(struct globaldata, gd_cpuid));
ASSYM(GD_IDLEPCBPHYS, offsetof(struct globaldata, gd_idlepcbphys));

ASSYM(MTX_LOCK, offsetof(struct mtx, mtx_lock));
ASSYM(MTX_RECURSE, offsetof(struct mtx, mtx_recurse));
ASSYM(MTX_SAVECRIT, offsetof(struct mtx, mtx_savecrit));
ASSYM(MTX_UNOWNED, MTX_UNOWNED);

ASSYM(TD_PCB, offsetof(struct thread, td_pcb));
ASSYM(TD_KSE, offsetof(struct thread, td_kse));
ASSYM(TD_PROC, offsetof(struct thread, td_proc));

ASSYM(TD_MD_FLAGS, offsetof(struct thread, td_md.md_flags));
ASSYM(TD_MD_PCBPADDR, offsetof(struct thread, td_md.md_pcbpaddr));
ASSYM(TD_MD_HAE, offsetof(struct thread, td_md.md_hae));
#ifdef SMP
ASSYM(TD_MD_KERNNEST, offsetof(struct thread, td_md.md_kernnest));
#endif
ASSYM(MDP_HAEUSED, MDP_HAEUSED);

ASSYM(CHIPSET_WRITE_HAE, offsetof(struct alpha_chipset, write_hae));

ASSYM(VM_MAXUSER_ADDRESS, VM_MAXUSER_ADDRESS);
ASSYM(PTLEV1I, PTLEV1I);
ASSYM(PTESIZE, PTESIZE);

ASSYM(PCB_ONFAULT, offsetof(struct pcb, pcb_onfault));
ASSYM(PCB_HWPCB_KSP, offsetof(struct pcb, pcb_hw.apcb_ksp));
ASSYM(PCB_CONTEXT, offsetof(struct pcb, pcb_context));
ASSYM(PCB_HW, offsetof(struct pcb, pcb_hw));

ASSYM(FPREG_FPR_REGS, offsetof(struct fpreg, fpr_regs));
ASSYM(FPREG_FPR_CR, offsetof(struct fpreg, fpr_cr));

ASSYM(EFAULT, EFAULT);
ASSYM(ENAMETOOLONG, ENAMETOOLONG);

/* Register offsets, for stack frames. */
ASSYM(FRAME_V0, FRAME_V0);
ASSYM(FRAME_T0, FRAME_T0);
ASSYM(FRAME_T1, FRAME_T1);
ASSYM(FRAME_T2, FRAME_T2);
ASSYM(FRAME_T3, FRAME_T3);
ASSYM(FRAME_T4, FRAME_T4);
ASSYM(FRAME_T5, FRAME_T5);
ASSYM(FRAME_T6, FRAME_T6);
ASSYM(FRAME_T7, FRAME_T7);
ASSYM(FRAME_S0, FRAME_S0);
ASSYM(FRAME_S1, FRAME_S1);
ASSYM(FRAME_S2, FRAME_S2);
ASSYM(FRAME_S3, FRAME_S3);
ASSYM(FRAME_S4, FRAME_S4);
ASSYM(FRAME_S5, FRAME_S5);
ASSYM(FRAME_S6, FRAME_S6);
ASSYM(FRAME_A3, FRAME_A3);
ASSYM(FRAME_A4, FRAME_A4);
ASSYM(FRAME_A5, FRAME_A5);
ASSYM(FRAME_T8, FRAME_T8);
ASSYM(FRAME_T9, FRAME_T9);
ASSYM(FRAME_T10, FRAME_T10);
ASSYM(FRAME_T11, FRAME_T11);
ASSYM(FRAME_RA, FRAME_RA);
ASSYM(FRAME_T12, FRAME_T12);
ASSYM(FRAME_AT, FRAME_AT);
ASSYM(FRAME_SP, FRAME_SP);
ASSYM(FRAME_FLAGS, FRAME_FLAGS);
ASSYM(FRAME_FLAGS_SYSCALL, FRAME_FLAGS_SYSCALL);

ASSYM(FRAME_SW_SIZE, FRAME_SW_SIZE);

ASSYM(FRAME_PS, FRAME_PS);
ASSYM(FRAME_PC, FRAME_PC);
ASSYM(FRAME_GP, FRAME_GP);
ASSYM(FRAME_A0, FRAME_A0);
ASSYM(FRAME_A1, FRAME_A1);
ASSYM(FRAME_A2, FRAME_A2);

ASSYM(FRAME_SIZE, FRAME_SIZE);

/* bits of the PS register */
ASSYM(ALPHA_PSL_USERMODE, ALPHA_PSL_USERMODE);
ASSYM(ALPHA_PSL_IPL_MASK, ALPHA_PSL_IPL_MASK);
ASSYM(ALPHA_PSL_IPL_0, ALPHA_PSL_IPL_0);
ASSYM(ALPHA_PSL_IPL_SOFT, ALPHA_PSL_IPL_SOFT);
ASSYM(ALPHA_PSL_IPL_HIGH, ALPHA_PSL_IPL_HIGH);

/* pte bits */
ASSYM(ALPHA_L1SHIFT, ALPHA_L1SHIFT);
ASSYM(ALPHA_L2SHIFT, ALPHA_L2SHIFT);
ASSYM(ALPHA_L3SHIFT, ALPHA_L3SHIFT);
ASSYM(ALPHA_K1SEG_BASE, ALPHA_K1SEG_BASE);
ASSYM(ALPHA_PTE_VALID, ALPHA_PTE_VALID);
ASSYM(ALPHA_PTE_ASM, ALPHA_PTE_ASM);
ASSYM(ALPHA_PTE_KR, ALPHA_PTE_KR);
ASSYM(ALPHA_PTE_KW, ALPHA_PTE_KW);

/* Kernel entries */
ASSYM(ALPHA_KENTRY_ARITH, ALPHA_KENTRY_ARITH);
ASSYM(ALPHA_KENTRY_MM, ALPHA_KENTRY_MM);

ASSYM(ALPHA_KENTRY_IF, ALPHA_KENTRY_IF);
ASSYM(ALPHA_KENTRY_UNA, ALPHA_KENTRY_UNA);

ASSYM(VPTBASE, VPTBASE);
ASSYM(KERNBASE, KERNBASE);
