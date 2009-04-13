/*-
 * Copyright (c) 2003 Peter Wemm
 * Copyright (c) 1999 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_UCONTEXT_H_
#define	_MACHINE_UCONTEXT_H_

/*
 * mc_trapno bits. Shall be in sync with TF_XXX.
 */
#define	_MC_HASSEGS	0x1
#define	_MC_HASBASES	0x2
#define	_MC_FLAG_MASK	(_MC_HASSEGS | _MC_HASBASES)

typedef struct __mcontext {
	/*
	 * The first 24 fields must match the definition of
	 * sigcontext. So that we can support sigcontext
	 * and ucontext_t at the same time.
	 */
	__register_t	mc_onstack;		/* XXX - sigcontext compat. */
	__register_t	mc_rdi;			/* machine state (struct trapframe) */
	__register_t	mc_rsi;
	__register_t	mc_rdx;
	__register_t	mc_rcx;
	__register_t	mc_r8;
	__register_t	mc_r9;
	__register_t	mc_rax;
	__register_t	mc_rbx;
	__register_t	mc_rbp;
	__register_t	mc_r10;
	__register_t	mc_r11;
	__register_t	mc_r12;
	__register_t	mc_r13;
	__register_t	mc_r14;
	__register_t	mc_r15;
	__uint32_t	mc_trapno;
	__uint16_t	mc_fs;
	__uint16_t	mc_gs;
	__register_t	mc_addr;
	__uint32_t	mc_flags;
	__uint16_t	mc_es;
	__uint16_t	mc_ds;
	__register_t	mc_err;
	__register_t	mc_rip;
	__register_t	mc_cs;
	__register_t	mc_rflags;
	__register_t	mc_rsp;
	__register_t	mc_ss;

	long	mc_len;			/* sizeof(mcontext_t) */

#define	_MC_FPFMT_NODEV		0x10000	/* device not present or configured */
#define	_MC_FPFMT_XMM		0x10002
	long	mc_fpformat;
#define	_MC_FPOWNED_NONE	0x20000	/* FP state not used */
#define	_MC_FPOWNED_FPU		0x20001	/* FP state came from FPU */
#define	_MC_FPOWNED_PCB		0x20002	/* FP state came from PCB */
	long	mc_ownedfp;
	/*
	 * See <machine/fpu.h> for the internals of mc_fpstate[].
	 */
	long	mc_fpstate[64] __aligned(16);

	__register_t	mc_fsbase;
	__register_t	mc_gsbase;

	long	mc_spare[6];
} mcontext_t;

#endif /* !_MACHINE_UCONTEXT_H_ */
