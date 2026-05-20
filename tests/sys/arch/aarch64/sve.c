/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023,2024 Arm Ltd
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

#include <sys/types.h>
#include <sys/auxv.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/ucontext.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <machine/armreg.h>
#include <machine/sysarch.h>

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <atf-c.h>

static unsigned long vl;

static void
check_for_sve(void)
{
	unsigned long hwcap;

	if (elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap)) != 0)
		atf_tc_skip("No HWCAP");

	if ((hwcap & HWCAP_SVE) == 0)
		atf_tc_skip("No SVE support in HW");

	ATF_REQUIRE(sysarch(ARM64_GET_SVE_VL, &vl) == 0);
}

ATF_TC_WITHOUT_HEAD(sve_registers);
ATF_TC_BODY(sve_registers, tc)
{
	uint64_t reg, val;

	check_for_sve();

	/* Check the ID registers are sane */
	reg = READ_SPECIALREG(id_aa64pfr0_el1);
	ATF_REQUIRE((reg & ID_AA64PFR0_SVE_MASK) >= ID_AA64PFR0_SVE_IMPL);

	/*
	 * Store 1.0 in z0, repeated every 16 bits. Read it from d0 as
	 * the registers alias.
	 */
	asm volatile(
	    ".arch_extension sve	\n"
	    "fmov	z0.h, #1	\n"
	    "str	d0, [%0]	\n"
	    ".arch_extension nosve	\n"
	    :: "r"(&val) : "z0", "d0"
	);

	/* Check for the 1.0 bit pattern */
	ATF_REQUIRE_EQ(val, 0x3c003c003c003c00);
}

static void
sve_signal_handler(int sig __unused, siginfo_t *info, void *context)
{
	struct arm64_reg_context *regctx;
	struct sve_context *svectx;
	ucontext_t *ctx;
	uint64_t *sveregs;

	ctx = context;

	/* Check the trap is from a breakpoint instruction */
	ATF_REQUIRE_EQ(info->si_trapno, EXCP_BRK);
	ctx->uc_mcontext.mc_gpregs.gp_elr += 4;

	/* Trash z0 to check it's not kept when exiting the handler */
	asm volatile(
	    ".arch_extension sve	\n"
	    "fmov	z0.h, #2	\n"
	    ".arch_extension nosve	\n"
	::: "z0");

	/* Change the lower bits of z1 through the siginfo struct */
	ctx->uc_mcontext.mc_fpregs.fp_q[1] = 0x5a5a5a5a5a5a5a5a;

	/* Find the SVE registers */
	regctx = (struct arm64_reg_context *)ctx->uc_mcontext.mc_ptr;
	if (regctx != NULL) {
		int idx, next;
		do {
			if (regctx->ctx_id == ARM64_CTX_SVE)
				break;

			ATF_REQUIRE(regctx->ctx_id != ARM64_CTX_END);
			regctx = (struct arm64_reg_context *)
			    ((uintptr_t)regctx + regctx->ctx_size);
		} while (1);

		/* Update the register context */
		svectx = (struct sve_context *)regctx;
		ATF_REQUIRE_EQ(svectx->sve_vector_len, vl);

		sveregs = (uint64_t *)(void *)(svectx + 1);
		/* Find the array entries to change */
		idx = 2 * svectx->sve_vector_len / sizeof(*sveregs);
		next = 3 * svectx->sve_vector_len / sizeof(*sveregs);
		while (idx != next) {
			sveregs[idx] = 0xdeaddeaddeaddead;
			idx++;
		}
	}
}

ATF_TC_WITHOUT_HEAD(sve_signal);
ATF_TC_BODY(sve_signal, tc)
{
	struct sigaction sa = {
		.sa_sigaction = sve_signal_handler,
		.sa_flags = SA_SIGINFO,
	};
	uint64_t val0, val1, *val2;

	check_for_sve();
	ATF_REQUIRE(sigaction(SIGTRAP, &sa, NULL) == 0);
	val2 = malloc(vl);
	ATF_REQUIRE(val2 != NULL);

	asm volatile(
	    ".arch_extension sve	\n"
	    "fmov	z0.h, #1	\n"
	    "fmov	z1.h, #1	\n"
	    "fmov	z2.h, #1	\n"
	    /* Raise a SIGTRAP */
	    "brk	#1		\n"
	    "str	d0, [%0]	\n"
	    "str	d1, [%1]	\n"
	    "str	z2, [%2]	\n"
	    ".arch_extension nosve	\n"
	    :: "r"(&val0), "r"(&val1), "r"(val2) : "z0", "z1", "z2", "d0", "d1"
	);

	/* Check for the 1.0 bit pattern */
	ATF_REQUIRE_EQ(val0, 0x3c003c003c003c00);
	/* Check for the changed bit pattern */
	ATF_REQUIRE_EQ(val1, 0x5a5a5a5a5a5a5a5a);
	/*
	 * Check the lower 128 bits are restored from fp_q and the
	 * upper bits are restored from the sve data region
	 */
	for (size_t i = 0; i < vl / sizeof(*val2); i++) {
		if (i < 2)
			ATF_REQUIRE_EQ(val2[i], 0x3c003c003c003c00);
		else
			ATF_REQUIRE_EQ(val2[i], 0xdeaddeaddeaddead);
	}

	free(val2);
}

ATF_TC_WITHOUT_HEAD(sve_signal_altstack);
ATF_TC_BODY(sve_signal_altstack, tc)
{
	struct sigaction sa = {
		.sa_sigaction = sve_signal_handler,
		.sa_flags = SA_ONSTACK | SA_SIGINFO,
	};
	stack_t ss = {
		.ss_size = SIGSTKSZ,
	};
	uint64_t val0, val1, *val2;

	check_for_sve();
	ss.ss_sp = malloc(ss.ss_size);
	ATF_REQUIRE(ss.ss_sp != NULL);
	ATF_REQUIRE(sigaltstack(&ss, NULL) == 0);
	ATF_REQUIRE(sigaction(SIGTRAP, &sa, NULL) == 0);
	val2 = malloc(vl);
	ATF_REQUIRE(val2 != NULL);

	asm volatile(
	    ".arch_extension sve	\n"
	    "fmov	z0.h, #1	\n"
	    "fmov	z1.h, #1	\n"
	    "fmov	z2.h, #1	\n"
	    /* Raise a SIGTRAP */
	    "brk	#1		\n"
	    "str	d0, [%0]	\n"
	    "str	d1, [%1]	\n"
	    "str	z2, [%2]	\n"
	    ".arch_extension nosve	\n"
	    :: "r"(&val0), "r"(&val1), "r"(val2) : "z0", "z1", "z2", "d0", "d1"
	);

	/* Check for the 1.0 bit pattern */
	ATF_REQUIRE_EQ(val0, 0x3c003c003c003c00);
	/* Check for the changed bit pattern */
	ATF_REQUIRE_EQ(val1, 0x5a5a5a5a5a5a5a5a);
	/*
	 * Check the lower 128 bits are restored from fp_q and the
	 * upper bits are restored from the sve data region
	 */
	for (size_t i = 0; i < vl / sizeof(*val2); i++) {
		if (i < 2)
			ATF_REQUIRE_EQ(val2[i], 0x3c003c003c003c00);
		else
			ATF_REQUIRE_EQ(val2[i], 0xdeaddeaddeaddead);
	}

	free(val2);
}

ATF_TC_WITHOUT_HEAD(sve_ptrace);
ATF_TC_BODY(sve_ptrace, tc)
{
	struct reg reg;
	struct iovec fpvec, svevec;
	struct svereg_header *header;
	pid_t child, wpid;
	int status;

	check_for_sve();

	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		char exec_path[1024];

		/* Calculate the location of the helper */
		snprintf(exec_path, sizeof(exec_path), "%s/sve_ptrace_helper",
		    atf_tc_get_config_var(tc, "srcdir"));

		ptrace(PT_TRACE_ME, 0, NULL, 0);

		/* Execute the helper so SVE will be disabled */
		execl(exec_path, "sve_ptrace_helper", NULL);
		_exit(1);
	}

	/* The first event should be the SIGSTOP at the start of the child */
	wpid = waitpid(child, &status, 0);
	ATF_REQUIRE(WIFSTOPPED(status));

	fpvec.iov_base = NULL;
	fpvec.iov_len = 0;
	/* Read the length before SVE has been used */
	ATF_REQUIRE(ptrace(PT_GETREGSET, wpid, (caddr_t)&fpvec, NT_ARM_SVE) ==
	    0);
	ATF_REQUIRE(fpvec.iov_len == (sizeof(struct svereg_header) +
	    sizeof(struct fpregs)));

	fpvec.iov_base = malloc(fpvec.iov_len);
	header = fpvec.iov_base;
	ATF_REQUIRE(fpvec.iov_base != NULL);
	ATF_REQUIRE(ptrace(PT_GETREGSET, wpid, (caddr_t)&fpvec, NT_ARM_SVE) ==
	    0);
	ATF_REQUIRE((header->sve_flags & SVEREG_FLAG_REGS_MASK) ==
	    SVEREG_FLAG_FP);

	/* Check writing back the FP registers works */
	ATF_REQUIRE(ptrace(PT_SETREGSET, wpid, (caddr_t)&fpvec, NT_ARM_SVE) ==
	    0);

	ptrace(PT_CONTINUE, wpid, (caddr_t)1, 0);

	/* The second event should be the SIGINFO at the end */
	wpid = waitpid(child, &status, 0);
	ATF_REQUIRE(WIFSTOPPED(status));
	ATF_REQUIRE_EQ(WSTOPSIG(status), SIGTRAP);

	/* Check writing back FP registers when SVE has started will fail */
	ATF_REQUIRE(ptrace(PT_SETREGSET, wpid, (caddr_t)&fpvec, NT_ARM_SVE) ==
	    -1);

	svevec.iov_base = NULL;
	svevec.iov_len = 0;
	/* Read the length after SVE has been used */
	ATF_REQUIRE(ptrace(PT_GETREGSET, wpid, (caddr_t)&svevec, NT_ARM_SVE) ==
	    0);
	/* TODO: Check the length is correct based on vector length */
	ATF_REQUIRE(svevec.iov_len > (sizeof(struct svereg_header) +
	    sizeof(struct fpregs)));

	svevec.iov_base = malloc(svevec.iov_len);
	header = svevec.iov_base;
	ATF_REQUIRE(svevec.iov_base != NULL);
	ATF_REQUIRE(ptrace(PT_GETREGSET, wpid, (caddr_t)&svevec, NT_ARM_SVE) ==
	    0);
	ATF_REQUIRE((header->sve_flags & SVEREG_FLAG_REGS_MASK) ==
	    SVEREG_FLAG_SVE);

	/* Test writing back the SVE registers works */
	ATF_REQUIRE(ptrace(PT_SETREGSET, wpid, (caddr_t)&svevec, NT_ARM_SVE) ==
	    0);

	free(svevec.iov_base);
	free(fpvec.iov_base);

	/* Step over the brk instruction */
	ATF_REQUIRE(ptrace(PT_GETREGS, wpid, (caddr_t)&reg, 0) != -1);
	reg.elr += 4;
	ATF_REQUIRE(ptrace(PT_SETREGS, wpid, (caddr_t)&reg, 0) != -1);

	ptrace(PT_CONTINUE, wpid, (caddr_t)1, 0);
	waitpid(child, &status, 0);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE_EQ(WEXITSTATUS(status), 0);
}

ATF_TC_WITHOUT_HEAD(sve_fork_env);
ATF_TC_BODY(sve_fork_env, tc)
{
	pid_t child, wpid;
	int status;

	check_for_sve();

	child = fork();
	ATF_REQUIRE(child >= 0);

	/* Check the child environment is sane */
	if (child == 0) {
		unsigned long child_vl, hwcap;

		if (elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap)) != 0)
			_exit(1);

		if ((hwcap & HWCAP_SVE) == 0)
			_exit(2);

		if (sysarch(ARM64_GET_SVE_VL, &child_vl) != 0)
			_exit(3);

		if (child_vl != vl)
			_exit(4);

		_exit(0);
	}

	wpid = waitpid(child, &status, 0);
	ATF_REQUIRE_EQ(child, wpid);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 0);
}

ATF_TC_WITHOUT_HEAD(sve_fork_regs);
ATF_TC_BODY(sve_fork_regs, tc)
{
	pid_t child, wpid;
	uint64_t *val;
	int status;

	check_for_sve();

	/*
	 * Malloc before fork to reduce the change of trashing sve registers
	 */
	val = malloc(vl);
	ATF_REQUIRE(val != NULL);

	/*
	 * Store 1.0 in z0, repeated every 16 bits. Read it from d0 as
	 * the registers alias.
	 */
	asm volatile(
	    ".arch_extension sve	\n"
	    "fmov	z8.h, #1	\n"
	    ".arch_extension nosve	\n"
	    ::: "z8"
	);

	/* TODO: Move to asm to ensure z8 isn't trashed */
	child = fork();

	asm volatile(
	    ".arch_extension sve	\n"
	    "str	z8, [%0]	\n"
	    ".arch_extension nosve	\n"
	    :: "r"(val) : "z8"
	);

	ATF_REQUIRE(child >= 0);

	/* Check the child environment is sane */
	if (child == 0) {
		for (size_t i = 0; i < vl / sizeof(*val); i++) {
			if (val[i] != 0x3c003c003c003c00)
				_exit(i + 1);
		}

		free(val);
		_exit(0);
	}

	free(val);
	wpid = waitpid(child, &status, 0);
	ATF_REQUIRE_EQ(child, wpid);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, sve_registers);
	ATF_TP_ADD_TC(tp, sve_signal);
	ATF_TP_ADD_TC(tp, sve_signal_altstack);
	/* TODO: Check a too small signal stack */
	ATF_TP_ADD_TC(tp, sve_ptrace);
	ATF_TP_ADD_TC(tp, sve_fork_env);
	ATF_TP_ADD_TC(tp, sve_fork_regs);
	return (atf_no_error());
}
