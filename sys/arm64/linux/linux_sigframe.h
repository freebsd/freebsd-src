/*-
 * Copyright (c) 1994-1996 Søren Schmidt
 * Copyright (c) 2018 Turing Robotic Industries Inc.
 * Copyright (c) 2022 Dmitry Chagin <dchagin@FreeBSD.org>
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
 */

#ifndef _ARM64_LINUX_SIGFRAME_H_
#define	_ARM64_LINUX_SIGFRAME_H_

struct _l_aarch64_ctx {
	uint32_t	magic;
	uint32_t	size;
};

#define	L_FPSIMD_MAGIC	0x46508001
#define	L_ESR_MAGIC	0x45535201

struct l_fpsimd_context {
	struct _l_aarch64_ctx head;
	uint32_t	fpsr;
	uint32_t	fpcr;
	__uint128_t	vregs[32];
};

struct l_esr_context {
	struct _l_aarch64_ctx head;
	uint64_t	esr;
};

struct l_sigcontext {
	uint64_t	fault_address;
	uint64_t	regs[31];
	uint64_t	sp;
	uint64_t	pc;
	uint64_t	pstate;
	uint8_t		__reserved[4096] __attribute__((__aligned__(16)));
};

struct l_ucontext {
	unsigned long	uc_flags;
	struct l_ucontext *uc_link;
	l_stack_t	uc_stack;
	l_sigset_t	uc_sigmask;
	uint8_t		__glibc_hole[1024 / 8 - sizeof(l_sigset_t)];
	struct l_sigcontext uc_sc;
};

struct l_rt_sigframe {
	l_siginfo_t	sf_si;
	struct l_ucontext sf_uc;
} __attribute__((__aligned__(16)));

struct l_sigframe {
	struct l_rt_sigframe sf;
	/* frame_record */
	uint64_t	fp;
	uint64_t	lr;
	ucontext_t	uc;
};

#define	LINUX_MINSIGSTKSZ	roundup(sizeof(struct l_sigframe), 16)

#endif /* _ARM64_LINUX_SIGFRAME_H_ */
