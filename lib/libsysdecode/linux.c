/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <stdbool.h>
#include <stdio.h>
#include <sysdecode.h>

#include "support.h"

#ifdef __aarch64__
#include <arm64/linux/linux.h>
#elif __i386__
#include <i386/linux/linux.h>
#elif __amd64__
#include <amd64/linux/linux.h>
#else
#error "Unsupported Linux arch"
#endif

#include <compat/linux/linux.h>
#include <compat/linux/linux_file.h>
#include <compat/linux/linux_fork.h>
#include <compat/linux/linux_time.h>

#define	X(a,b)	{ a, #b },
#define	XEND	{ 0, NULL }

#define	TABLE_START(n)	static struct name_table n[] = {
#define	TABLE_ENTRY	X
#define	TABLE_END	XEND };

#include "tables_linux.h"

#undef TABLE_START
#undef TABLE_ENTRY
#undef TABLE_END

static const char *linux_signames[] = {
	[LINUX_SIGHUP] = "SIGHUP",
	[LINUX_SIGINT] = "SIGINT",
	[LINUX_SIGQUIT] = "SIGQUIT",
	[LINUX_SIGILL] = "SIGILL",
	[LINUX_SIGTRAP] = "SIGTRAP",
	[LINUX_SIGABRT] = "SIGABRT",
	[LINUX_SIGBUS] = "SIGBUS",
	[LINUX_SIGFPE] = "SIGFPE",
	[LINUX_SIGKILL] = "SIGKILL",
	[LINUX_SIGUSR1] = "SIGUSR1",
	[LINUX_SIGSEGV] = "SIGSEGV",
	[LINUX_SIGUSR2] = "SIGUSR2",
	[LINUX_SIGPIPE] = "SIGPIPE",
	[LINUX_SIGALRM] = "SIGALRM",
	[LINUX_SIGTERM] = "SIGTERM",
	[LINUX_SIGSTKFLT] = "SIGSTKFLT",
	[LINUX_SIGCHLD] = "SIGCHLD",
	[LINUX_SIGCONT] = "SIGCONT",
	[LINUX_SIGSTOP] = "SIGSTOP",
	[LINUX_SIGTSTP] = "SIGTSTP",
	[LINUX_SIGTTIN] = "SIGTTIN",
	[LINUX_SIGTTOU] = "SIGTTOU",
	[LINUX_SIGURG] = "SIGURG",
	[LINUX_SIGXCPU] = "SIGXCPU",
	[LINUX_SIGXFSZ] = "SIGXFSZ",
	[LINUX_SIGVTALRM] = "SIGVTALRM",
	[LINUX_SIGPROF] = "SIGPROF",
	[LINUX_SIGWINCH] = "SIGWINCH",
	[LINUX_SIGIO] = "SIGIO",
	[LINUX_SIGPWR] = "SIGPWR",
	[LINUX_SIGSYS] = "SIGSYS",

	[LINUX_SIGRTMIN] = "SIGCANCEL",
	[LINUX_SIGRTMIN + 1] = "SIGSETXID",
	[LINUX_SIGRTMIN + 2] = "SIGRT2",
	[LINUX_SIGRTMIN + 3] = "SIGRT3",
	[LINUX_SIGRTMIN + 4] = "SIGRT4",
	[LINUX_SIGRTMIN + 5] = "SIGRT5",
	[LINUX_SIGRTMIN + 6] = "SIGRT6",
	[LINUX_SIGRTMIN + 7] = "SIGRT7",
	[LINUX_SIGRTMIN + 8] = "SIGRT8",
	[LINUX_SIGRTMIN + 9] = "SIGRT9",
	[LINUX_SIGRTMIN + 10] = "SIGRT10",
	[LINUX_SIGRTMIN + 11] = "SIGRT11",
	[LINUX_SIGRTMIN + 12] = "SIGRT12",
	[LINUX_SIGRTMIN + 13] = "SIGRT13",
	[LINUX_SIGRTMIN + 14] = "SIGRT14",
	[LINUX_SIGRTMIN + 15] = "SIGRT15",
	[LINUX_SIGRTMIN + 16] = "SIGRT16",
	[LINUX_SIGRTMIN + 17] = "SIGRT17",
	[LINUX_SIGRTMIN + 18] = "SIGRT18",
	[LINUX_SIGRTMIN + 19] = "SIGRT19",
	[LINUX_SIGRTMIN + 20] = "SIGRT20",
	[LINUX_SIGRTMIN + 21] = "SIGRT21",
	[LINUX_SIGRTMIN + 22] = "SIGRT22",
	[LINUX_SIGRTMIN + 23] = "SIGRT23",
	[LINUX_SIGRTMIN + 24] = "SIGRT24",
	[LINUX_SIGRTMIN + 25] = "SIGRT25",
	[LINUX_SIGRTMIN + 26] = "SIGRT26",
	[LINUX_SIGRTMIN + 27] = "SIGRT27",
	[LINUX_SIGRTMIN + 28] = "SIGRT28",
	[LINUX_SIGRTMIN + 29] = "SIGRT29",
	[LINUX_SIGRTMIN + 30] = "SIGRT30",
	[LINUX_SIGRTMIN + 31] = "SIGRT31",
	[LINUX_SIGRTMIN + 32] = "SIGRTMAX",
};
_Static_assert(nitems(linux_signames) == LINUX_SIGRTMAX + 1,
    "invalid entries count in linux_signames");

void
sysdecode_linux_clockid(FILE *fp, clockid_t which)
{
	const char *str;
	clockid_t ci;
	pid_t pid;

	if (which >= 0) {
		str = lookup_value(clockids, which);
		if (str == NULL)
			fprintf(fp, "UNKNOWN(%d)", which);
		else
			fputs(str, fp);
		return;
	}
	if ((which & LINUX_CLOCKFD_MASK) == LINUX_CLOCKFD_MASK) {
		fputs("INVALID PERTHREAD|CLOCKFD", fp);
		goto pidp;
	}
	ci = LINUX_CPUCLOCK_WHICH(which);
	if (LINUX_CPUCLOCK_PERTHREAD(which) == true)
		fputs("THREAD|", fp);
	else
		fputs("PROCESS|", fp);
	str = lookup_value(clockcpuids, ci);
	if (str != NULL)
		fputs(str, fp);
	else {
		if (ci == LINUX_CLOCKFD)
			fputs("CLOCKFD", fp);
		else
			fprintf(fp, "UNKNOWN(%d)", which);
	}

pidp:
	pid = LINUX_CPUCLOCK_ID(which);
	fprintf(fp, "(%d)", pid);
}

const char *
sysdecode_linux_signal(int sig)
{

	if ((unsigned)sig < nitems(linux_signames))
		return (linux_signames[sig]);
	return (NULL);
}

const char *
sysdecode_linux_sigprocmask_how(int how)
{

	return (lookup_value(sigprocmaskhow, how));
}

bool
sysdecode_linux_clock_flags(FILE *fp, int flags, int *rem)
{

	return (print_mask_int(fp, clockflags, flags, rem));
}

bool
sysdecode_linux_atflags(FILE *fp, int flag, int *rem)
{

	return (print_mask_int(fp, atflags, flag, rem));
}

bool
sysdecode_linux_open_flags(FILE *fp, int flags, int *rem)
{
	bool printed;
	int mode;
	uintmax_t val;

	mode = flags & LINUX_O_ACCMODE;
	flags &= ~LINUX_O_ACCMODE;
	switch (mode) {
	case LINUX_O_RDONLY:
		fputs("O_RDONLY", fp);
		printed = true;
		mode = 0;
		break;
	case LINUX_O_WRONLY:
		fputs("O_WRONLY", fp);
		printed = true;
		mode = 0;
		break;
	case LINUX_O_RDWR:
		fputs("O_RDWR", fp);
		printed = true;
		mode = 0;
		break;
	default:
		printed = false;
	}
	val = (unsigned)flags;
	print_mask_part(fp, openflags, &val, &printed);
	if (rem != NULL)
		*rem = val | mode;
	return (printed);
}

bool
sysdecode_linux_clone_flags(FILE *fp, int flags, int *rem)
{
	uintmax_t val;
	bool printed;
	int sig;

	sig = flags & LINUX_CSIGNAL;
	if (sig != 0)
		fprintf(fp, "(%s)", sysdecode_linux_signal(sig));
	val = (unsigned)flags & ~LINUX_CSIGNAL;
	print_mask_part(fp, cloneflags, &val, &printed);
	if (rem != NULL)
		*rem = val;
	return (printed);
}
