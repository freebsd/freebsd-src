/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 *	from: @(#)genassym.c	5.11 (Berkeley) 5/10/91
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/assym.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/asi.h>
#include <machine/vmparam.h>
#include <machine/cpufunc.h>
#include <machine/fp.h>
#include <machine/frame.h>
#include <machine/globals.h>
#include <machine/pcb.h>
#include <machine/pstate.h>
#include <machine/setjmp.h>
#include <machine/pv.h>
#include <machine/tte.h>
#include <machine/tlb.h>
#include <machine/tsb.h>

/*
 * XXX: gas, as of version 2.11.2, does not know this ASI (and some other
 * UltraSparc specific ones). This definition will probably get us into trouble
 * as soon as they are added.
 */
ASSYM(ASI_BLK_S, ASI_BLK_S);

ASSYM(EFAULT, EFAULT);
ASSYM(ENAMETOOLONG, ENAMETOOLONG);

ASSYM(UPAGES, UPAGES);
ASSYM(PAGE_SIZE, PAGE_SIZE);

ASSYM(PSTATE_AG, PSTATE_AG);
ASSYM(PSTATE_IE, PSTATE_IE);
ASSYM(PSTATE_PRIV, PSTATE_PRIV);
ASSYM(PSTATE_PEF, PSTATE_PEF);
ASSYM(PSTATE_MG, PSTATE_MG);
ASSYM(PSTATE_IG, PSTATE_IG);

ASSYM(TSTATE_AG, TSTATE_AG);
ASSYM(TSTATE_IE, TSTATE_IE);
ASSYM(TSTATE_PRIV, TSTATE_PRIV);
ASSYM(TSTATE_PEF, TSTATE_PEF);
ASSYM(TSTATE_MG, TSTATE_MG);
ASSYM(TSTATE_IG, TSTATE_IG);

ASSYM(FPRS_DL, FPRS_DL);
ASSYM(FPRS_DU, FPRS_DU);
ASSYM(FPRS_FEF, FPRS_FEF);

ASSYM(TTE_SHIFT, TTE_SHIFT);
ASSYM(STTE_SHIFT, STTE_SHIFT);
ASSYM(TSB_PRIMARY_BUCKET_SHIFT, TSB_PRIMARY_BUCKET_SHIFT);
ASSYM(TSB_KERNEL_MIN_ADDRESS, TSB_KERNEL_MIN_ADDRESS);
ASSYM(TSB_MASK_WIDTH, TSB_MASK_WIDTH);
ASSYM(TSB_SECONDARY_BUCKET_SHIFT, TSB_SECONDARY_BUCKET_SHIFT);
ASSYM(TSB_BUCKET_SPREAD_SHIFT, TSB_BUCKET_SPREAD_SHIFT);
ASSYM(TSB_SECONDARY_STTE_MASK, TSB_SECONDARY_STTE_MASK);
ASSYM(TSB_SECONDARY_STTE_SHIFT, TSB_SECONDARY_STTE_SHIFT);
ASSYM(TSB_LEVEL1_BUCKET_MASK, TSB_LEVEL1_BUCKET_MASK);
ASSYM(TSB_LEVEL1_BUCKET_SHIFT, TSB_LEVEL1_BUCKET_SHIFT);
ASSYM(TSB_1M_STTE_SHIFT, TSB_1M_STTE_SHIFT);
ASSYM(TSB_KERNEL_MASK, TSB_KERNEL_MASK);

ASSYM(PAGE_SHIFT, PAGE_SHIFT);
ASSYM(PAGE_MASK, PAGE_MASK);

ASSYM(TTE_DATA, offsetof(struct tte, tte_data));
ASSYM(TTE_TAG, offsetof(struct tte, tte_tag));
ASSYM(ST_TTE, offsetof(struct stte, st_tte));
ASSYM(STTE_SIZEOF, sizeof(struct stte));

ASSYM(TD_VA_LOW_MASK, TD_VA_LOW_MASK);
ASSYM(TD_VA_LOW_SHIFT, TD_VA_LOW_SHIFT);
ASSYM(TD_MOD, TD_MOD);
ASSYM(TD_REF, TD_REF);
ASSYM(TD_W, TD_W);

ASSYM(TT_VA_MASK, TT_VA_MASK);
ASSYM(TT_VA_SHIFT, TT_VA_SHIFT);
ASSYM(TT_CTX_SHIFT, TT_CTX_SHIFT);

ASSYM(GD_CURPROC, offsetof(struct globaldata, gd_curproc));
ASSYM(GD_CURPCB, offsetof(struct globaldata, gd_curpcb));

ASSYM(JB_FP, offsetof(struct _jmp_buf, _jb[_JB_FP]));
ASSYM(JB_PC, offsetof(struct _jmp_buf, _jb[_JB_PC]));
ASSYM(JB_SP, offsetof(struct _jmp_buf, _jb[_JB_SP]));

ASSYM(P_ADDR, offsetof(struct proc, p_addr));
ASSYM(P_VMSPACE, offsetof(struct proc, p_vmspace));
ASSYM(P_FRAME, offsetof(struct proc, p_frame));

ASSYM(PCB_FPSTATE, offsetof(struct pcb, pcb_fpstate));
ASSYM(PCB_FP, offsetof(struct pcb, pcb_fp));
ASSYM(PCB_PC, offsetof(struct pcb, pcb_pc));
ASSYM(PCB_ONFAULT, offsetof(struct pcb, pcb_onfault));

ASSYM(FP_FB0, offsetof(struct fpstate, fp_fb[0]));
ASSYM(FP_FB1, offsetof(struct fpstate, fp_fb[1]));
ASSYM(FP_FB2, offsetof(struct fpstate, fp_fb[2]));
ASSYM(FP_FB3, offsetof(struct fpstate, fp_fb[3]));
ASSYM(FP_FSR, offsetof(struct fpstate, fp_fsr));
ASSYM(FP_FPRS, offsetof(struct fpstate, fp_fprs));

ASSYM(F_L0, offsetof(struct frame, f_local[0]));
ASSYM(F_L1, offsetof(struct frame, f_local[1]));
ASSYM(F_L2, offsetof(struct frame, f_local[2]));
ASSYM(F_L3, offsetof(struct frame, f_local[3]));
ASSYM(F_L4, offsetof(struct frame, f_local[4]));
ASSYM(F_L5, offsetof(struct frame, f_local[5]));
ASSYM(F_L6, offsetof(struct frame, f_local[6]));
ASSYM(F_L7, offsetof(struct frame, f_local[7]));
ASSYM(F_I0, offsetof(struct frame, f_in[0]));
ASSYM(F_I1, offsetof(struct frame, f_in[1]));
ASSYM(F_I2, offsetof(struct frame, f_in[2]));
ASSYM(F_I3, offsetof(struct frame, f_in[3]));
ASSYM(F_I4, offsetof(struct frame, f_in[4]));
ASSYM(F_I5, offsetof(struct frame, f_in[5]));
ASSYM(F_I6, offsetof(struct frame, f_in[6]));
ASSYM(F_I7, offsetof(struct frame, f_in[7]));
ASSYM(CCFSZ, sizeof(struct frame));
ASSYM(SPOFF, SPOFF);

ASSYM(KF_FP, offsetof(struct kdbframe, kf_fp));
ASSYM(KF_SIZEOF, sizeof(struct kdbframe));

ASSYM(MF_SFAR, offsetof(struct mmuframe, mf_sfar));
ASSYM(MF_SFSR, offsetof(struct mmuframe, mf_sfsr));
ASSYM(MF_TAR, offsetof(struct mmuframe, mf_tar));
ASSYM(MF_SIZEOF, sizeof(struct mmuframe));

ASSYM(TF_G0, offsetof(struct trapframe, tf_global[0]));
ASSYM(TF_G1, offsetof(struct trapframe, tf_global[1]));
ASSYM(TF_G2, offsetof(struct trapframe, tf_global[2]));
ASSYM(TF_G3, offsetof(struct trapframe, tf_global[3]));
ASSYM(TF_G4, offsetof(struct trapframe, tf_global[4]));
ASSYM(TF_G5, offsetof(struct trapframe, tf_global[5]));
ASSYM(TF_G6, offsetof(struct trapframe, tf_global[6]));
ASSYM(TF_G7, offsetof(struct trapframe, tf_global[7]));
ASSYM(TF_O0, offsetof(struct trapframe, tf_out[0]));
ASSYM(TF_O1, offsetof(struct trapframe, tf_out[1]));
ASSYM(TF_O2, offsetof(struct trapframe, tf_out[2]));
ASSYM(TF_O3, offsetof(struct trapframe, tf_out[3]));
ASSYM(TF_O4, offsetof(struct trapframe, tf_out[4]));
ASSYM(TF_O5, offsetof(struct trapframe, tf_out[5]));
ASSYM(TF_O6, offsetof(struct trapframe, tf_out[6]));
ASSYM(TF_O7, offsetof(struct trapframe, tf_out[7]));
ASSYM(TF_TSTATE, offsetof(struct trapframe, tf_tstate));
ASSYM(TF_TPC, offsetof(struct trapframe, tf_tpc));
ASSYM(TF_TNPC, offsetof(struct trapframe, tf_tnpc));
ASSYM(TF_TYPE, offsetof(struct trapframe, tf_type));
ASSYM(TF_ARG, offsetof(struct trapframe, tf_arg));
ASSYM(TF_SIZEOF, sizeof(struct trapframe));

ASSYM(U_PCB, offsetof(struct user, u_pcb));
