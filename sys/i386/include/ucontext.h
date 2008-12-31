/*-
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
 * $FreeBSD: src/sys/i386/include/ucontext.h,v 1.11.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _MACHINE_UCONTEXT_H_
#define	_MACHINE_UCONTEXT_H_

typedef struct __mcontext {
	/*
	 * The first 20 fields must match the definition of
	 * sigcontext. So that we can support sigcontext
	 * and ucontext_t at the same time.
	 */
	__register_t	mc_onstack;	/* XXX - sigcontext compat. */
	__register_t	mc_gs;		/* machine state (struct trapframe) */
	__register_t	mc_fs;
	__register_t	mc_es;
	__register_t	mc_ds;
	__register_t	mc_edi;
	__register_t	mc_esi;
	__register_t	mc_ebp;
	__register_t	mc_isp;
	__register_t	mc_ebx;
	__register_t	mc_edx;
	__register_t	mc_ecx;
	__register_t	mc_eax;
	__register_t	mc_trapno;
	__register_t	mc_err;
	__register_t	mc_eip;
	__register_t	mc_cs;
	__register_t	mc_eflags;
	__register_t	mc_esp;
	__register_t	mc_ss;

	int	mc_len;			/* sizeof(mcontext_t) */
#define	_MC_FPFMT_NODEV		0x10000	/* device not present or configured */
#define	_MC_FPFMT_387		0x10001
#define	_MC_FPFMT_XMM		0x10002
	int	mc_fpformat;
#define	_MC_FPOWNED_NONE	0x20000	/* FP state not used */
#define	_MC_FPOWNED_FPU		0x20001	/* FP state came from FPU */
#define	_MC_FPOWNED_PCB		0x20002	/* FP state came from PCB */
	int	mc_ownedfp;
	int	mc_spare1[1];		/* align next field to 16 bytes */
	/*
	 * See <machine/npx.h> for the internals of mc_fpstate[].
	 */
	int	mc_fpstate[128] __aligned(16);
	int	mc_spare2[8];
} mcontext_t;

#if defined(_KERNEL) && defined(COMPAT_FREEBSD4)
struct mcontext4 {
	__register_t	mc_onstack;	/* XXX - sigcontext compat. */
	__register_t	mc_gs;		/* machine state (struct trapframe) */
	__register_t	mc_fs;
	__register_t	mc_es;
	__register_t	mc_ds;
	__register_t	mc_edi;
	__register_t	mc_esi;
	__register_t	mc_ebp;
	__register_t	mc_isp;
	__register_t	mc_ebx;
	__register_t	mc_edx;
	__register_t	mc_ecx;
	__register_t	mc_eax;
	__register_t	mc_trapno;
	__register_t	mc_err;
	__register_t	mc_eip;
	__register_t	mc_cs;
	__register_t	mc_eflags;
	__register_t	mc_esp;		/* machine state */
	__register_t	mc_ss;
	__register_t	mc_fpregs[28];	/* env87 + fpacc87 + u_long */
	__register_t	__spare__[17];
};
#endif

#endif /* !_MACHINE_UCONTEXT_H_ */
