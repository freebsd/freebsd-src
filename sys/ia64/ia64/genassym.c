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

#include <stddef.h>

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
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>
#include <net/if.h>
#include <netinet/in.h>
#include <nfs/nfsv2.h>
#include <nfs/rpcv2.h>
#include <nfs/nfs.h>
#include <nfs/nfsdiskless.h>

ASSYM(GD_CURPROC, offsetof(struct globaldata, gd_curproc));
ASSYM(GD_FPCURPROC, offsetof(struct globaldata, gd_fpcurproc));
ASSYM(GD_CURPCB, offsetof(struct globaldata, gd_curpcb));
ASSYM(GD_SWITCHTIME, offsetof(struct globaldata, gd_switchtime));
ASSYM(GD_CPUNO, offsetof(struct globaldata, gd_cpuno));
ASSYM(GD_ASTPENDING, offsetof(struct globaldata, gd_astpending));

ASSYM(MTX_LOCK, offsetof(struct mtx, mtx_lock));
ASSYM(MTX_RECURSE, offsetof(struct mtx, mtx_recurse));
ASSYM(MTX_SAVEINTR, offsetof(struct mtx, mtx_saveintr));
ASSYM(MTX_UNOWNED, MTX_UNOWNED);

ASSYM(P_ADDR, offsetof(struct proc, p_addr));
ASSYM(P_MD_FLAGS, offsetof(struct proc, p_md.md_flags));

ASSYM(VM_MAXUSER_ADDRESS, VM_MAXUSER_ADDRESS);

ASSYM(SIZEOF_USER,	sizeof(struct user));

ASSYM(TF_CR_IPSR,	offsetof(struct trapframe, tf_cr_ipsr));
ASSYM(TF_CR_IFS,	offsetof(struct trapframe, tf_cr_ifs));
ASSYM(TF_NDIRTY,	offsetof(struct trapframe, tf_ndirty));
ASSYM(TF_B,		offsetof(struct trapframe, tf_b));
ASSYM(TF_R,		offsetof(struct trapframe, tf_r));
ASSYM(TF_F,		offsetof(struct trapframe, tf_f));

ASSYM(FRAME_SP,		FRAME_SP);

ASSYM(U_PCB_R4,		offsetof(struct user, u_pcb.pcb_r4));
ASSYM(U_PCB_R5,		offsetof(struct user, u_pcb.pcb_r5));
ASSYM(U_PCB_R6,		offsetof(struct user, u_pcb.pcb_r6));
ASSYM(U_PCB_R7,		offsetof(struct user, u_pcb.pcb_r7));

ASSYM(U_PCB_F2,		offsetof(struct user, u_pcb.pcb_f2));
ASSYM(U_PCB_F3,		offsetof(struct user, u_pcb.pcb_f3));
ASSYM(U_PCB_F4,		offsetof(struct user, u_pcb.pcb_f4));
ASSYM(U_PCB_F5,		offsetof(struct user, u_pcb.pcb_f5));

ASSYM(U_PCB_B0,		offsetof(struct user, u_pcb.pcb_b0));
ASSYM(U_PCB_B1,		offsetof(struct user, u_pcb.pcb_b1));
ASSYM(U_PCB_B2,		offsetof(struct user, u_pcb.pcb_b2));
ASSYM(U_PCB_B3,		offsetof(struct user, u_pcb.pcb_b3));
ASSYM(U_PCB_B4,		offsetof(struct user, u_pcb.pcb_b4));
ASSYM(U_PCB_B5,		offsetof(struct user, u_pcb.pcb_b5));

ASSYM(U_PCB_OLD_UNAT,	offsetof(struct user, u_pcb.pcb_old_unat));
ASSYM(U_PCB_SP,		offsetof(struct user, u_pcb.pcb_sp));
ASSYM(U_PCB_PFS,	offsetof(struct user, u_pcb.pcb_pfs));
ASSYM(U_PCB_BSPSTORE,	offsetof(struct user, u_pcb.pcb_bspstore));

ASSYM(U_PCB_UNAT,	offsetof(struct user, u_pcb.pcb_unat));
ASSYM(U_PCB_RNAT,	offsetof(struct user, u_pcb.pcb_rnat));
ASSYM(U_PCB_PR,		offsetof(struct user, u_pcb.pcb_pr));

ASSYM(U_PCB_SCHEDNEST,	offsetof(struct user, u_pcb.pcb_onfault));
ASSYM(U_PCB_ONFAULT,	offsetof(struct user, u_pcb.pcb_onfault));

ASSYM(U_PCB_HIGHFP,	offsetof(struct user, u_pcb.pcb_highfp));

ASSYM(UC_MCONTEXT_MC_AR_BSP,  offsetof(ucontext_t, uc_mcontext.mc_ar_bsp));
ASSYM(UC_MCONTEXT_MC_AR_RNAT, offsetof(ucontext_t, uc_mcontext.mc_ar_rnat));

ASSYM(EFAULT, EFAULT);
ASSYM(ENAMETOOLONG, ENAMETOOLONG);

ASSYM(SIZEOF_TRAPFRAME, sizeof(struct trapframe));
