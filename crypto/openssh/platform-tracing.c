/*
 * Copyright (c) 2016 Darren Tucker.  All rights reserved.
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

#include "includes.h"

#include <sys/types.h>
#ifdef HAVE_SYS_PROCCTL_H
#include <sys/procctl.h>
#endif
#if defined(HAVE_SYS_PRCTL_H)
#include <sys/prctl.h>	/* For prctl() and PR_SET_DUMPABLE */
#endif
#ifdef HAVE_SYS_PTRACE_H
#include <sys/ptrace.h>
#endif
#ifdef HAVE_PRIV_H
#include <priv.h> /* For setpflags() and __PROC_PROTECT  */
#endif
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "log.h"

void
platform_disable_tracing(int strict)
{
#if defined(HAVE_PROCCTL) && defined(PROC_TRACE_CTL)
	/* On FreeBSD, we should make this process untraceable */
	int disable_trace = PROC_TRACE_CTL_DISABLE;

	/*
	 * On FreeBSD, we should make this process untraceable.
	 * pid=0 means "this process" but some older kernels do not
	 * understand that so retry with our own pid before failing.
	 */
	if (procctl(P_PID, 0, PROC_TRACE_CTL, &disable_trace) == 0)
		return;
	if (procctl(P_PID, getpid(), PROC_TRACE_CTL, &disable_trace) == 0)
		return;
	if (strict)
		fatal("unable to make the process untraceable: %s",
		    strerror(errno));
#endif
#if defined(HAVE_PRCTL) && defined(PR_SET_DUMPABLE)
	/* Disable ptrace on Linux without sgid bit */
	if (prctl(PR_SET_DUMPABLE, 0) != 0 && strict)
		fatal("unable to make the process undumpable: %s",
		    strerror(errno));
#endif
#if defined(HAVE_SETPFLAGS) && defined(__PROC_PROTECT)
	/* On Solaris, we should make this process untraceable */
	if (setpflags(__PROC_PROTECT, 1) != 0 && strict)
		fatal("unable to make the process untraceable: %s",
		    strerror(errno));
#endif
#ifdef PT_DENY_ATTACH
	/* Mac OS X */
	if (ptrace(PT_DENY_ATTACH, 0, 0, 0) == -1 && strict)
		fatal("unable to set PT_DENY_ATTACH: %s", strerror(errno));
#endif
}
