/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2004 Tim J. Robbins
 * Copyright (c) 2001 Doug Rabson
 * Copyright (c) 1994-1996 Søren Schmidt
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
 *    derived from this software without specific prior written permission
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

#ifndef _X86_LINUX_SIGFRAME_H_
#define	_X86_LINUX_SIGFRAME_H_

#define	LINUX_UC_FP_XSTATE		0x1

#define	LINUX_FP_XSTATE_MAGIC1		0x46505853U
#define	LINUX_FP_XSTATE_MAGIC2		0x46505845U
#define	LINUX_FP_XSTATE_MAGIC2_SIZE	sizeof(uint32_t)

struct l_fpx_sw_bytes {
	uint32_t	magic1;
	uint32_t	extended_size;
	uint64_t	xfeatures;
	uint32_t	xstate_size;
	uint32_t	padding[7];
};

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))

/* The Linux sigcontext, pretty much a standard 386 trapframe. */
struct l_sigcontext {
	l_uint		sc_gs;
	l_uint		sc_fs;
	l_uint		sc_es;
	l_uint		sc_ds;
	l_uint		sc_edi;
	l_uint		sc_esi;
	l_uint		sc_ebp;
	l_uint		sc_esp;
	l_uint		sc_ebx;
	l_uint		sc_edx;
	l_uint		sc_ecx;
	l_uint		sc_eax;
	l_uint		sc_trapno;
	l_uint		sc_err;
	l_uint		sc_eip;
	l_uint		sc_cs;
	l_uint		sc_eflags;
	l_uint		sc_esp_at_signal;
	l_uint		sc_ss;
	l_uint		sc_387;
	l_uint		sc_mask;
	l_uint		sc_cr2;
};

struct l_ucontext {
	l_ulong		uc_flags;
	l_uintptr_t	uc_link;
	l_stack_t	uc_stack;
	struct l_sigcontext	uc_mcontext;
	l_sigset_t	uc_sigmask;
} __packed;

struct l_fpreg {
	u_int16_t	significand[4];
	u_int16_t	exponent;
};

struct l_fpxreg {
	u_int16_t	significand[4];
	u_int16_t	exponent;
	u_int16_t	padding[3];
};

struct l_xmmreg {
	u_int32_t	element[4];
};

struct l_fpstate {
	/* Regular FPU environment */
	u_int32_t		cw;
	u_int32_t		sw;
	u_int32_t		tag;
	u_int32_t		ipoff;
	u_int32_t		cssel;
	u_int32_t		dataoff;
	u_int32_t		datasel;
	struct l_fpreg		_st[8];
	u_int16_t		status;
	u_int16_t		magic;		/* 0xffff = regular FPU data */

	/* FXSR FPU environment */
	u_int32_t		_fxsr_env[6];	/* env is ignored. */
	u_int32_t		mxcsr;
	u_int32_t		reserved;
	struct l_fpxreg		_fxsr_st[8];	/* reg data is ignored. */
	struct l_xmmreg		_xmm[8];
	u_int32_t		padding[56];
};

/*
 * We make the stack look like Linux expects it when calling a signal
 * handler, but use the BSD way of calling the handler and sigreturn().
 */
struct l_sigframe {
	l_int			sf_sig;
	struct l_sigcontext	sf_sc;
	struct l_fpstate	sf_fpstate;
	sigset_t		sf_sigmask;
};

struct l_rt_sigframe {
	l_int			sf_sig;
	l_uintptr_t		sf_siginfo;
	l_uintptr_t		sf_ucontext;
	l_siginfo_t		sf_si;
	struct l_ucontext	sf_uc;
};

#else

struct l_fpstate {
	u_int16_t cwd;
	u_int16_t swd;
	u_int16_t twd;
	u_int16_t fop;
	u_int64_t rip;
	u_int64_t rdp;
	u_int32_t mxcsr;
	u_int32_t mxcsr_mask;
	u_int8_t st[8][16];
	u_int8_t xmm[16][16];
	u_int32_t reserved2[12];
	union {
		u_int32_t		reserved3[12];
		struct l_fpx_sw_bytes	sw_reserved;
	};
} __aligned(16);

struct l_sigcontext {
	l_ulong		sc_r8;
	l_ulong		sc_r9;
	l_ulong		sc_r10;
	l_ulong		sc_r11;
	l_ulong		sc_r12;
	l_ulong		sc_r13;
	l_ulong		sc_r14;
	l_ulong		sc_r15;
	l_ulong		sc_rdi;
	l_ulong		sc_rsi;
	l_ulong		sc_rbp;
	l_ulong		sc_rbx;
	l_ulong		sc_rdx;
	l_ulong		sc_rax;
	l_ulong		sc_rcx;
	l_ulong		sc_rsp;
	l_ulong		sc_rip;
	l_ulong		sc_rflags;
	l_ushort	sc_cs;
	l_ushort	sc_gs;
	l_ushort	sc_fs;
	l_ushort	sc___pad0;
	l_ulong		sc_err;
	l_ulong		sc_trapno;
	l_sigset_t	sc_mask;
	l_ulong		sc_cr2;
	/*
	 * On Linux sc_fpstate is (struct l_fpstate *) or (struct l_xstate *)
	 * depending on the FP_XSTATE_MAGIC1 encoded in the sw_reserved
	 * bytes of (struct l_fpstate) and FP_XSTATE_MAGIC2 present at the end
	 * of extended memory layout.
	 */
	l_uintptr_t	sc_fpstate;
	l_ulong		sc_reserved1[8];
};

struct l_ucontext {
	l_ulong		uc_flags;
	l_uintptr_t	uc_link;
	l_stack_t	uc_stack;
	struct l_sigcontext	uc_mcontext;
	l_sigset_t	uc_sigmask;
};

/*
 * We make the stack look like Linux expects it when calling a signal
 * handler, but use the BSD way of calling the handler and sigreturn().
 */
struct l_rt_sigframe {
	struct l_ucontext	sf_uc;
	struct l_siginfo	sf_si;
};

#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

#endif /* !_X86_LINUX_SIGFRAME_H_ */
