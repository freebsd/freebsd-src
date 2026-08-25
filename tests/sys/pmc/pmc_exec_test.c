/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Alexander Leidinger <netchild@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other advertising materials provided with the
 *    distribution.
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

/**
 * @file
 * Regression tests for a process-mode PMC whose target exec()s a program
 * that changes its credentials: what FreeBSD-SA-26:56.hwpmc fixed.
 *
 * pmc_process_exec() must drop such a PMC unless its owner may still trace
 * the new credentials, and the detach must unlink the process descriptor
 * exactly once.
 *
 * The owner must be unprivileged: root may trace anything, so as root
 * neither case reaches the branch under test - hence require.user.  The
 * test must not drop privileges itself either: setuid(2) sets P_SUGID,
 * fork(2) passes it to the child, and p_candebug() then refuses the target
 * to its unprivileged owner, so pmc_attach() would fail with EPERM first.
 *
 * The privileged exec target is picked at run time from base binaries, and
 * a case skips if none of them carries a set-id bit any more.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <pmc.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

static const char *counting_events[] = {
	"instructions",
	"cycles",
	"branches",
	"unhalted-core-cycles",
	"inst_retired.any",
	"cpu_clk_unhalted.thread",
	"ls_not_halted_cyc",
	NULL
};

/**
 * @internal
 * Exec targets.  'setid' is the bit the entry needs; the argument vectors
 * are chosen so the program exits at once and touches nothing.  wall(1)
 * reads its message from stdin, which the caller holds open on a pipe, so
 * that target stays alive long enough to be inspected.
 */
struct exec_target {
	const char	*path;
	const char	*const argv[5];
	mode_t		 setid;
	int		 blocks_on_stdin;
};

static const struct exec_target setgid_targets[] = {
	{ "/usr/bin/wall", { "wall", NULL }, S_ISGID, 1 },
	{ NULL, { NULL }, 0, 0 }
};

static const struct exec_target setuid_targets[] = {
	{ "/sbin/ping", { "ping", "-c", "1", "127.0.0.1", NULL }, S_ISUID, 0 },
	{ NULL, { NULL }, 0, 0 }
};

static const struct exec_target *
pick_target(const struct exec_target *tab)
{
	struct stat sb;
	int i;

	for (i = 0; tab[i].path != NULL; i++) {
		if (stat(tab[i].path, &sb) != 0)
			continue;
		if ((sb.st_mode & tab[i].setid) != 0)
			return (&tab[i]);
	}
	return (NULL);
}

static pmc_id_t
allocate_counting_pmc(void)
{
	pmc_id_t id = PMC_ID_INVALID;
	int i;

	for (i = 0; counting_events[i] != NULL; i++) {
		if (pmc_allocate(counting_events[i], PMC_MODE_TC, 0,
		    PMC_CPU_ANY, &id, 0) == 0)
			return (id);
	}
	return (PMC_ID_INVALID);
}

static void
spin(void)
{
	volatile unsigned long s = 0;
	int i;

	for (i = 0; i < 2000000; i++)
		s += i;
}

static void
require_unprivileged_owner(void)
{

	if (geteuid() == 0)
		atf_tc_skip("the PMC owner must be unprivileged: root may "
		    "trace any credentials, so the check under test is never "
		    "reached");
}

/**
 * @internal
 * Fork a target, attach a running counting PMC to it, and let it exec the
 * privileged program.  Returns with the PMC released; *still_attached is
 * only meaningful for a target that blocks after the exec.
 */
static void
run_target(const struct exec_target *t, int *still_attached)
{
	pmc_id_t id;
	pid_t target;
	char token;
	int gopipe[2], inpipe[2], status;

	ATF_REQUIRE(pipe(gopipe) == 0);
	ATF_REQUIRE(pipe(inpipe) == 0);
	ATF_REQUIRE((target = fork()) >= 0);

	if (target == 0) {
		(void)close(gopipe[1]);
		if (t->blocks_on_stdin)
			(void)dup2(inpipe[0], STDIN_FILENO);
		(void)close(inpipe[1]);
		if (read(gopipe[0], &token, 1) != 1)
			_exit(1);
		spin();
		(void)execv(t->path, __DECONST(char **, t->argv));
		_exit(1);
	}
	(void)close(gopipe[0]);
	(void)close(inpipe[0]);

	if (pmc_init() != 0) {
		(void)kill(target, SIGKILL);
		(void)waitpid(target, &status, 0);
		atf_tc_skip("hwpmc(4) is not available");
	}
	if ((id = allocate_counting_pmc()) == PMC_ID_INVALID) {
		(void)kill(target, SIGKILL);
		(void)waitpid(target, &status, 0);
		atf_tc_skip("no process-mode counting event is allocatable");
	}

	ATF_REQUIRE_MSG(pmc_attach(id, target) == 0, "pmc_attach: %s",
	    strerror(errno));
	ATF_REQUIRE(pmc_start(id) == 0);

	/* Into execve(2). */
	ATF_REQUIRE(write(gopipe[1], "g", 1) == 1);
	(void)close(gopipe[1]);

	if (t->blocks_on_stdin) {
		(void)usleep(400000);
		errno = 0;
		*still_attached = pmc_detach(id, target) == 0;
		if (!*still_attached)
			ATF_REQUIRE_MSG(errno == ESRCH, "pmc_detach: %s",
			    strerror(errno));
		(void)kill(target, SIGKILL);
	} else {
		*still_attached = 0;
	}
	(void)waitpid(target, &status, 0);
	(void)pmc_release(id);
}

ATF_TC(exec_setgid_drops_pmc);
ATF_TC_HEAD(exec_setgid_drops_pmc, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "a process-mode PMC is detached when its target execs into "
	    "credentials its owner may not trace");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(exec_setgid_drops_pmc, tc)
{
	const struct exec_target *t;
	int still_attached;

	require_unprivileged_owner();
	if ((t = pick_target(setgid_targets)) == NULL)
		atf_tc_skip("no set-gid exec target available");

	run_target(t, &still_attached);
	ATF_REQUIRE_MSG(!still_attached,
	    "the PMC survived an exec into credentials its owner may not "
	    "trace");
}

ATF_TC(exec_setuid_no_double_unlink);
ATF_TC_HEAD(exec_setuid_no_double_unlink, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "detaching a process-mode PMC at a credential-changing exec "
	    "unlinks the process descriptor exactly once");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(exec_setuid_no_double_unlink, tc)
{
	const struct exec_target *t;
	int still_attached;

	require_unprivileged_owner();
	if ((t = pick_target(setuid_targets)) == NULL)
		atf_tc_skip("no set-uid exec target available");

	/* Completing at all is the assertion. */
	run_target(t, &still_attached);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, exec_setgid_drops_pmc);
	ATF_TP_ADD_TC(tp, exec_setuid_no_double_unlink);

	return (atf_no_error());
}
