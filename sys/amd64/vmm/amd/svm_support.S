/*-
 * Copyright (c) 2013, Anish Gupta (akgupt3@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <machine/asmacros.h>

#include "svm_assym.s"

/*
 * Macros to save and restore GPRs.
 */
#define SAVE_GPR_STATE(reg);			\
	movq %rbp, 	SCTX_RBP(reg);		\
	movq %rbx, 	SCTX_RBX(reg);		\
	movq %rcx, 	SCTX_RCX(reg);		\
	movq %r8,  	SCTX_R8(reg);		\
	movq %r9,  	SCTX_R9(reg);		\
	movq %r10, 	SCTX_R10(reg);		\
	movq %r11, 	SCTX_R11(reg);		\
	movq %r12, 	SCTX_R12(reg);		\
	movq %r13, 	SCTX_R13(reg);		\
	movq %r14, 	SCTX_R14(reg);		\
	movq %r15, 	SCTX_R15(reg);		\

#define LOAD_GPR_STATE(reg)			\
	movq  SCTX_RBP(reg), 	%rbp;		\
	movq  SCTX_RBX(reg), 	%rbx;		\
	movq  SCTX_RCX(reg), 	%rcx;		\
	movq  SCTX_R8(reg),  	%r8;		\
	movq  SCTX_R9(reg),  	%r9;		\
	movq  SCTX_R10(reg), 	%r10;		\
	movq  SCTX_R11(reg), 	%r11;		\
	movq  SCTX_R12(reg), 	%r12;		\
	movq  SCTX_R13(reg), 	%r13;		\
	movq  SCTX_R14(reg), 	%r14;		\
	movq  SCTX_R15(reg), 	%r15;		\

/*
 * Macros to save and restore vcpu registers which are not
 * done by SVM.
 */
#define SAVE_GUEST_STATE(reg)			\
	movq %rdi, 	SCTX_GUEST_RDI(reg);	\
	movq %rsi, 	SCTX_GUEST_RSI(reg);	\
	movq %rdx, 	SCTX_GUEST_RDX(reg);	\
	SAVE_GPR_STATE(reg)

#define LOAD_GUEST_STATE(reg)			\
	movq SCTX_GUEST_RDI(reg),	%rdi;	\
	movq SCTX_GUEST_RSI(reg),	%rsi;	\
	movq SCTX_GUEST_RDX(reg),	%rdx;	\
	LOAD_GPR_STATE(reg)

/*
 * Macros to save and restore host registers which are not
 * saved by SVM.
 */
#define SAVE_HOST_STATE(reg)			\
	mov %fs, 	SCTX_HOST_FS(reg);	\
	mov %gs, 	SCTX_HOST_GS(reg);	\
	movq %rsp, 	SCTX_HOST_RSP(reg);	\
	SAVE_GPR_STATE(reg)

#define LOAD_HOST_STATE(reg)			\
	mov SCTX_HOST_FS(reg),		%fs;	\
	mov SCTX_HOST_GS(reg),		%gs;	\
	movq SCTX_HOST_RSP(reg),	%rsp;	\
	LOAD_GPR_STATE(reg)

/*
 * This is where virtual machine vcpu start execution.
 * int svm_launch(vmcb_pa, gswctx, hswctx)
 * vmcb_pa  - VMCB physical address is in %rdi.
 * gswctx   - Guest context is in %rsi.
 * hswctx   - Host context is in %rdx.
 *
 * Note: SVM guarantees host RSP and RAX will be restored
 * back after guest exit. RAX is where VMCB Phy addr is so
 * we are left with only RSP. RSP will hold base for guest
 * software context which will have base for host software
 * context.
 */
ENTRY(svm_launch)

	/* Save host GPRs. */
	SAVE_HOST_STATE(%rdx)

	/*
	 * Move the parameters to final destinations.
	 * RAX - VMCB phy addr.
	 * RSP - Guest software context.
	 * SCTX_GUEST_HOST(guest) - Host software context.
	 */
	movq %rdi, %rax
	movq %rsi, %rsp
	movq %rdx, SCTX_GUEST_HCTX_BASE(%rsp)

	/* Load guest context. */
	LOAD_GUEST_STATE(%rsp)

	vmload %rax

	vmrun %rax

	vmsave %rax

	/* Save guest state. */
	SAVE_GUEST_STATE(%rsp)

	/* Restore host context base in RDX. */
	movq SCTX_GUEST_HCTX_BASE(%rsp), %rdx
	/* Restore host GPRs. */
	LOAD_HOST_STATE(%rdx)

	ret
END(svm_launch)
