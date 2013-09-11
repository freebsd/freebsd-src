/*
 * Copyright (c) 2012 Will Drewry <wad@dataspill.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Uncomment the SANDBOX_SECCOMP_FILTER_DEBUG macro below to help diagnose
 * filter breakage during development. *Do not* use this in production,
 * as it relies on making library calls that are unsafe in signal context.
 *
 * Instead, live systems the auditctl(8) may be used to monitor failures.
 * E.g.
 *   auditctl -a task,always -F uid=<privsep uid>
 */
/* #define SANDBOX_SECCOMP_FILTER_DEBUG 1 */

#ifdef SANDBOX_SECCOMP_FILTER_DEBUG
/* Use the kernel headers in case of an older toolchain. */
# include <asm/siginfo.h>
# define __have_siginfo_t 1
# define __have_sigval_t 1
# define __have_sigevent_t 1
#endif /* SANDBOX_SECCOMP_FILTER_DEBUG */

#include "includes.h"

#ifdef SANDBOX_SECCOMP_FILTER

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/prctl.h>

#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <elf.h>

#include <asm/unistd.h>

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>  /* for offsetof */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "ssh-sandbox.h"
#include "xmalloc.h"

/* Linux seccomp_filter sandbox */
#define SECCOMP_FILTER_FAIL SECCOMP_RET_KILL

/* Use a signal handler to emit violations when debugging */
#ifdef SANDBOX_SECCOMP_FILTER_DEBUG
# undef SECCOMP_FILTER_FAIL
# define SECCOMP_FILTER_FAIL SECCOMP_RET_TRAP
#endif /* SANDBOX_SECCOMP_FILTER_DEBUG */

/* Simple helpers to avoid manual errors (but larger BPF programs). */
#define SC_DENY(_nr, _errno) \
	BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_ ## _nr, 0, 1), \
	BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ERRNO|(_errno))
#define SC_ALLOW(_nr) \
	BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_ ## _nr, 0, 1), \
	BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ALLOW)

/* Syscall filtering set for preauth. */
static const struct sock_filter preauth_insns[] = {
	/* Ensure the syscall arch convention is as expected. */
	BPF_STMT(BPF_LD+BPF_W+BPF_ABS,
		offsetof(struct seccomp_data, arch)),
	BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, SECCOMP_AUDIT_ARCH, 1, 0),
	BPF_STMT(BPF_RET+BPF_K, SECCOMP_FILTER_FAIL),
	/* Load the syscall number for checking. */
	BPF_STMT(BPF_LD+BPF_W+BPF_ABS,
		offsetof(struct seccomp_data, nr)),
	SC_DENY(open, EACCES),
	SC_ALLOW(getpid),
	SC_ALLOW(gettimeofday),
#ifdef __NR_time /* not defined on EABI ARM */
	SC_ALLOW(time),
#endif
	SC_ALLOW(read),
	SC_ALLOW(write),
	SC_ALLOW(close),
	SC_ALLOW(brk),
	SC_ALLOW(poll),
#ifdef __NR__newselect
	SC_ALLOW(_newselect),
#else
	SC_ALLOW(select),
#endif
	SC_ALLOW(madvise),
#ifdef __NR_mmap2 /* EABI ARM only has mmap2() */
	SC_ALLOW(mmap2),
#endif
#ifdef __NR_mmap
	SC_ALLOW(mmap),
#endif
	SC_ALLOW(munmap),
	SC_ALLOW(exit_group),
#ifdef __NR_rt_sigprocmask
	SC_ALLOW(rt_sigprocmask),
#else
	SC_ALLOW(sigprocmask),
#endif
	BPF_STMT(BPF_RET+BPF_K, SECCOMP_FILTER_FAIL),
};

static const struct sock_fprog preauth_program = {
	.len = (unsigned short)(sizeof(preauth_insns)/sizeof(preauth_insns[0])),
	.filter = (struct sock_filter *)preauth_insns,
};

struct ssh_sandbox {
	pid_t child_pid;
};

struct ssh_sandbox *
ssh_sandbox_init(void)
{
	struct ssh_sandbox *box;

	/*
	 * Strictly, we don't need to maintain any state here but we need
	 * to return non-NULL to satisfy the API.
	 */
	debug3("%s: preparing seccomp filter sandbox", __func__);
	box = xcalloc(1, sizeof(*box));
	box->child_pid = 0;

	return box;
}

#ifdef SANDBOX_SECCOMP_FILTER_DEBUG
extern struct monitor *pmonitor;
void mm_log_handler(LogLevel level, const char *msg, void *ctx);

static void
ssh_sandbox_violation(int signum, siginfo_t *info, void *void_context)
{
	char msg[256];

	snprintf(msg, sizeof(msg),
	    "%s: unexpected system call (arch:0x%x,syscall:%d @ %p)",
	    __func__, info->si_arch, info->si_syscall, info->si_call_addr);
	mm_log_handler(SYSLOG_LEVEL_FATAL, msg, pmonitor);
	_exit(1);
}

static void
ssh_sandbox_child_debugging(void)
{
	struct sigaction act;
	sigset_t mask;

	debug3("%s: installing SIGSYS handler", __func__);
	memset(&act, 0, sizeof(act));
	sigemptyset(&mask);
	sigaddset(&mask, SIGSYS);

	act.sa_sigaction = &ssh_sandbox_violation;
	act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSYS, &act, NULL) == -1)
		fatal("%s: sigaction(SIGSYS): %s", __func__, strerror(errno));
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
		fatal("%s: sigprocmask(SIGSYS): %s",
		      __func__, strerror(errno));
}
#endif /* SANDBOX_SECCOMP_FILTER_DEBUG */

void
ssh_sandbox_child(struct ssh_sandbox *box)
{
	struct rlimit rl_zero;
	int nnp_failed = 0;

	/* Set rlimits for completeness if possible. */
	rl_zero.rlim_cur = rl_zero.rlim_max = 0;
	if (setrlimit(RLIMIT_FSIZE, &rl_zero) == -1)
		fatal("%s: setrlimit(RLIMIT_FSIZE, { 0, 0 }): %s",
			__func__, strerror(errno));
	if (setrlimit(RLIMIT_NOFILE, &rl_zero) == -1)
		fatal("%s: setrlimit(RLIMIT_NOFILE, { 0, 0 }): %s",
			__func__, strerror(errno));
	if (setrlimit(RLIMIT_NPROC, &rl_zero) == -1)
		fatal("%s: setrlimit(RLIMIT_NPROC, { 0, 0 }): %s",
			__func__, strerror(errno));

#ifdef SANDBOX_SECCOMP_FILTER_DEBUG
	ssh_sandbox_child_debugging();
#endif /* SANDBOX_SECCOMP_FILTER_DEBUG */

	debug3("%s: setting PR_SET_NO_NEW_PRIVS", __func__);
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1) {
		debug("%s: prctl(PR_SET_NO_NEW_PRIVS): %s",
		      __func__, strerror(errno));
		nnp_failed = 1;
	}
	debug3("%s: attaching seccomp filter program", __func__);
	if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &preauth_program) == -1)
		debug("%s: prctl(PR_SET_SECCOMP): %s",
		      __func__, strerror(errno));
	else if (nnp_failed)
		fatal("%s: SECCOMP_MODE_FILTER activated but "
		    "PR_SET_NO_NEW_PRIVS failed", __func__);
}

void
ssh_sandbox_parent_finish(struct ssh_sandbox *box)
{
	free(box);
	debug3("%s: finished", __func__);
}

void
ssh_sandbox_parent_preauth(struct ssh_sandbox *box, pid_t child_pid)
{
	box->child_pid = child_pid;
}

#endif /* SANDBOX_SECCOMP_FILTER */
