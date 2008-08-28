/*-
 * Copyright (C) 2008 Jung-uk Kim <jkim@FreeBSD.org>. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>

#include <net/bpf.h>

#include BPF_TEST_H

#define	PASSED		0
#define	FAILED		1
#define	FATAL		-1

#ifndef LOG_LEVEL
#define	LOG_LEVEL	1
#endif

#ifdef BPF_BENCHMARK
#define	BPF_NRUNS	10000000
#else
#define	BPF_NRUNS	1
#endif

static void	sig_handler(int);

static int	nins = sizeof(pc) / sizeof(pc[0]);
static int	verbose = LOG_LEVEL;

#ifdef BPF_JIT_COMPILER

#include <net/bpf_jitter.h>

static u_int
bpf_compile_and_filter(void)
{
	bpf_jit_filter	*filter;
	u_int		i, ret;

	/* Compile the BPF filter program and generate native code. */
	if ((filter = bpf_jitter(pc, nins)) == NULL) {
		if (verbose > 1)
			printf("Failed to allocate memory:\t");
		if (verbose > 0)
			printf("FATAL\n");
		exit(FATAL);
	}

	for (i = 0; i < BPF_NRUNS; i++)
		ret = (*(filter->func))(pkt, wirelen, buflen);

	bpf_destroy_jit_filter(filter);

	return (ret);
}

#else

u_int	bpf_filter(const struct bpf_insn *, u_char *, u_int, u_int);

#endif

#ifdef BPF_VALIDATE
/*
 * XXX Copied from sys/net/bpf_filter.c and modified.
 *
 * Return true if the 'fcode' is a valid filter program.
 * The constraints are that each jump be forward and to a valid
 * code.  The code must terminate with either an accept or reject.
 *
 * The kernel needs to be able to verify an application's filter code.
 * Otherwise, a bogus program could easily crash the system.
 */
static int
bpf_validate(const struct bpf_insn *f, int len)
{
	register int i;
	register const struct bpf_insn *p;

	/* Do not accept negative length filter. */
	if (len < 0)
		return (0);

	/* An empty filter means accept all. */
	if (len == 0)
		return (1);

	for (i = 0; i < len; ++i) {
		/*
		 * Check that that jumps are forward, and within
		 * the code block.
		 */
		p = &f[i];
#if BPF_VALIDATE > 1
		/*
		 * XXX JK: Illegal instructions must be checked here.
		 */
		switch (p->code) {
		default:
			return (0);
		case BPF_RET|BPF_K:
		case BPF_RET|BPF_A:
		case BPF_LD|BPF_W|BPF_ABS:
		case BPF_LD|BPF_H|BPF_ABS:
		case BPF_LD|BPF_B|BPF_ABS:
		case BPF_LD|BPF_W|BPF_LEN:
		case BPF_LDX|BPF_W|BPF_LEN:
		case BPF_LD|BPF_W|BPF_IND:
		case BPF_LD|BPF_H|BPF_IND:
		case BPF_LD|BPF_B|BPF_IND:
		case BPF_LDX|BPF_MSH|BPF_B:
		case BPF_LD|BPF_IMM:
		case BPF_LDX|BPF_IMM:
		case BPF_LD|BPF_MEM:
		case BPF_LDX|BPF_MEM:
		case BPF_ST:
		case BPF_STX:
		case BPF_JMP|BPF_JA:
		case BPF_JMP|BPF_JGT|BPF_K:
		case BPF_JMP|BPF_JGE|BPF_K:
		case BPF_JMP|BPF_JEQ|BPF_K:
		case BPF_JMP|BPF_JSET|BPF_K:
		case BPF_JMP|BPF_JGT|BPF_X:
		case BPF_JMP|BPF_JGE|BPF_X:
		case BPF_JMP|BPF_JEQ|BPF_X:
		case BPF_JMP|BPF_JSET|BPF_X:
		case BPF_ALU|BPF_ADD|BPF_X:
		case BPF_ALU|BPF_SUB|BPF_X:
		case BPF_ALU|BPF_MUL|BPF_X:
		case BPF_ALU|BPF_DIV|BPF_X:
		case BPF_ALU|BPF_AND|BPF_X:
		case BPF_ALU|BPF_OR|BPF_X:
		case BPF_ALU|BPF_LSH|BPF_X:
		case BPF_ALU|BPF_RSH|BPF_X:
		case BPF_ALU|BPF_ADD|BPF_K:
		case BPF_ALU|BPF_SUB|BPF_K:
		case BPF_ALU|BPF_MUL|BPF_K:
		case BPF_ALU|BPF_DIV|BPF_K:
		case BPF_ALU|BPF_AND|BPF_K:
		case BPF_ALU|BPF_OR|BPF_K:
		case BPF_ALU|BPF_LSH|BPF_K:
		case BPF_ALU|BPF_RSH|BPF_K:
		case BPF_ALU|BPF_NEG:
		case BPF_MISC|BPF_TAX:
		case BPF_MISC|BPF_TXA:
			break;
		}
#endif
		if (BPF_CLASS(p->code) == BPF_JMP) {
			register int from = i + 1;

			if (BPF_OP(p->code) == BPF_JA) {
				if (from >= len || p->k >= (u_int)len - from)
					return (0);
			}
			else if (from >= len || p->jt >= len - from ||
				 p->jf >= len - from)
				return (0);
		}
		/*
		 * Check that memory operations use valid addresses.
		 */
		if ((BPF_CLASS(p->code) == BPF_ST ||
		     BPF_CLASS(p->code) == BPF_STX ||
		     ((BPF_CLASS(p->code) == BPF_LD ||
		       BPF_CLASS(p->code) == BPF_LDX) &&
		      (p->code & 0xe0) == BPF_MEM)) &&
		    p->k >= BPF_MEMWORDS)
			return (0);
		/*
		 * Check for constant division by 0.
		 */
		if (p->code == (BPF_ALU|BPF_DIV|BPF_K) && p->k == 0)
			return (0);
	}
	return (BPF_CLASS(f[len - 1].code) == BPF_RET);
}
#endif

int
main(void)
{
#ifndef BPF_JIT_COMPILER
	u_int	i;
#endif
	u_int   ret;
	int     sig;
#ifdef BPF_VALIDATE
	int	valid;
#endif

	/* Try to catch all signals */
	for (sig = SIGHUP; sig <= SIGUSR2; sig++)
		signal(sig, sig_handler);

#ifdef BPF_VALIDATE
	valid = bpf_validate(pc, nins);
	if (valid != 0 && invalid != 0) {
		if (verbose > 1)
			printf("Validated invalid instruction(s):\t");
		if (verbose > 0)
			printf("FAILED\n");
		return (FAILED);
	} else if (valid == 0 && invalid == 0) {
		if (verbose > 1)
			printf("Invalidated valid instruction(s):\t");
		if (verbose > 0)
			printf("FAILED\n");
		return (FAILED);
	} else if (invalid != 0) {
		if (verbose > 1)
			printf("Expected and invalidated:\t");
		if (verbose > 0)
			printf("PASSED\n");
		return (PASSED);
	}
#endif

#ifdef BPF_JIT_COMPILER
	ret = bpf_compile_and_filter();
#else
	for (i = 0; i < BPF_NRUNS; i++)
		ret = bpf_filter(nins != 0 ? pc : NULL, pkt, wirelen, buflen);
#endif
	if (ret != expect) {
		if (verbose > 1)
			printf("Expected 0x%x but got 0x%x:\t", expect, ret);
		if (verbose > 0)
			printf("FAILED\n");
		return (FAILED);
	}
	if (verbose > 1)
		printf("Expected and got 0x%x:\t", ret);
	if (verbose > 0)
		printf("PASSED\n");

	return (PASSED);
}

static void
sig_handler(int sig)
{

	if (expect_signal == 0) {
		if (verbose > 1)
			printf("Received unexpected signal %d:\t", sig);
		if (verbose > 0)
			printf("FATAL\n");
		exit(FATAL);
	}
	if (expect_signal != sig) {
		if (verbose > 1)
			printf("Expected signal %d but got %d:\t",
			    expect_signal, sig);
		if (verbose > 0)
			printf("FAILED\n");
		exit(FAILED);
	}

	if (verbose > 1)
		printf("Expected and got signal %d:\t", sig);
	if (verbose > 0)
		printf("PASSED\n");

	exit(PASSED);
}
