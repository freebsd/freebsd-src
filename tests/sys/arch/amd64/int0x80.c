/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 The FreeBSD Foundation
 *
 * This software were developed by
 * Konstantin Belousov <kib@FreeBSD.org> under sponsorship from
 * the FreeBSD Foundation.
 */

#include <sys/syscall.h>
#include <err.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef __amd64__
#error "amd64 only"
#endif

/*
 * The check to see how "INT $0x80" behaves for amd64 native ABI
 * processes.  Before bd8edba0792b71be3f8ed5dea9c22287e95c986a it
 * executed syscalls with amd64 calling conventions.  After that
 * revision, it should raise SIGBUS.
 */

static uintptr_t int0x80_loc;
static int signo;
extern char after_int0x80[];

static void
sigill_action(int signo1, siginfo_t *si __unused, void *uctxv)
{
	ucontext_t *uctx = uctxv;

	signo = signo1;
	int0x80_loc = uctx->uc_mcontext.mc_rip;
}

static int
fire(void)
{
	int res;

	res = SYS_getpid;
	asm volatile(
	    ".globl\tafter_int0x80\n"
	    "\tint\t$0x80\n"
	    "after_int0x80:"
	    : "+a" (res)
	    :
	    : "rdx", "memory", "cc");
	return (res);
}

int
main(void)
{
	struct sigaction sa;
	char signame[SIG2STR_MAX];
	int pid, res;

	res = 0;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = sigill_action;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGBUS, &sa, NULL) == -1)
		err(1, "catching SIGBUS");

	pid = fire();
	atomic_signal_fence(memory_order_seq_cst);
	if (int0x80_loc == 0) {
		printf("int $0x80 does not raise SIGBUS\n");
		if (pid == getpid())
			printf("and syscall worked\n");
		else
			printf("but syscall did not worked\n");
		res = 1;
	} else {
		sig2str(signo, signame);
		printf("int $0x80 raises SIG%s\n", signame);
		if (int0x80_loc == (uintptr_t)after_int0x80) {
			printf("at expected location\n");
		} else {
			printf("not at expected location\n");
			res = 1;
		}
	}
	exit(res);
}
