/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 ConnectWise
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
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/procdesc.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <atf-c.h>
#include <stdio.h>

/* Tests for procdesc(4) that aren't specific to any one syscall */

/*
 * Even after waiting on a process descriptor with waitpid(2), the kernel will
 * not recycle the pid until after the process descriptor is closed.  That is
 * important to prevent users from trying to wait() twice, the second time
 * using a dangling pid.
 *
 * Whether this same anti-recycling behavior is used with pdwait() is
 * unimportant, because pdwait _always_ uses a process descriptor.
 */
ATF_TC_WITHOUT_HEAD(pid_recycle);
ATF_TC_BODY(pid_recycle, tc)
{
	size_t len;
	int i, pd, pid_max;
	pid_t dangle_pid;

	len = sizeof(pid_max);
	ATF_REQUIRE_EQ_MSG(0,
	    sysctlbyname("kern.pid_max", &pid_max, &len, NULL, 0),
	    "sysctlbyname: %s", strerror(errno));

	/* Create a process descriptor */
	dangle_pid = pdfork(&pd, PD_CLOEXEC | PD_DAEMON);
	ATF_REQUIRE_MSG(dangle_pid >= 0, "pdfork: %s", strerror(errno));
	if (dangle_pid == 0) {
		// In child
		_exit(0);
	}
	/*
	 * Reap the child, but don't close the pd, creating a dangling pid.
	 * Notably, it isn't a Zombie, because the process is reaped.
	 */
	ATF_REQUIRE_EQ(dangle_pid, waitpid(dangle_pid, NULL, WEXITED));

	/*
	 * Now create and kill pid_max additional children.  Test to see if pid
	 * gets reused.  If not, that means the kernel is correctly reserving
	 * the dangling pid from reuse.
	 */
	for (i = 0; i < pid_max; i++) {
		pid_t pid;

		pid = vfork();
		ATF_REQUIRE_MSG(pid >= 0, "vfork: %s", strerror(errno));
		if (pid == 0)
			_exit(0);
		ATF_REQUIRE_MSG(pid != dangle_pid,
		    "pid got recycled after %d forks", i);
		ATF_REQUIRE_EQ(pid, waitpid(pid, NULL, WEXITED));
	}
	close(pd);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pid_recycle);

	return (atf_no_error());
}
