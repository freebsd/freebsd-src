/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Alex S <iwtcex@gmail.com>
 */

#include <machine/reg.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef __amd64__
#error "amd64 only"
#endif

/*
 * This test substitutes exit(42) instead of getpid() using ptrace.
 */

static const int EXPECTED_EXIT_CODE = 42;

static void
tamper(pid_t pid)
{
	struct ptrace_lwpinfo info;
	struct reg regs;

	if (ptrace(PT_LWPINFO, pid, (caddr_t)&info, sizeof(info)) == -1)
		err(1, "ptrace(PT_LWPINFO)");

	if ((info.pl_flags & PL_FLAG_SCE) != 0 &&
	    info.pl_syscall_code == SYS_getpid) {
		if (ptrace(PT_GETREGS, pid, (caddr_t)&regs, sizeof(regs)) == -1)
			err(1, "ptrace(PT_GETREGS)");

		regs.r_rax = SYS_exit;
		regs.r_rdi = EXPECTED_EXIT_CODE;

		if (ptrace(PT_SETREGS, pid, (caddr_t)&regs, sizeof(regs)) == -1)
			err(1, "ptrace(PT_SETREGS)");
	}
}

int
main(void)
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid == -1)
		err(1, "fork");

	if (pid == 0) {
		raise(SIGSTOP);
		(void)getpid();
		exit(0);
	} else {
		if (ptrace(PT_ATTACH, pid, 0, 0) == -1)
			err(1, "ptrace(PT_ATTACH)");

		for (;;) {
			if (wait(&status) == -1)
				err(1, "wait");

			if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) == EXPECTED_EXIT_CODE) {
					printf("exit code changed\n");
					exit(0);
				} else {
					printf("unable to change exit code\n");
					exit(1);
				}
			}

			assert(WIFSTOPPED(status));
			tamper(pid);

			if (ptrace(PT_TO_SCE, pid, (caddr_t)1, 0) == -1)
				err(1, "ptrace(PT_TO_SCE)");
		}
	}
}
