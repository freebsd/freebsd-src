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
 * Credential-transition tests for a process-mode PMC across exec(), the
 * companion to pmc_exec_test.c.  Where that file proves the driver drops a
 * PMC when its target execs into credentials its owner may no longer
 * trace, these prove the two cases the drop must not overreach into: an
 * exec that changes no credentials keeps the PMC, and an exec of a set-id
 * binary whose credential change the kernel then suppresses, because the
 * target is ptrace(2)d, keeps it too - do_execve() derives the set-id from
 * the image alone and suppresses it afterwards, clearing P_SUGID and
 * leaving the ids untouched, so the owner is still entitled.  The third
 * case covers fexecve(2), which reaches the same hook as execve(2).
 * These exercise what FreeBSD-SA-26:56.hwpmc reworked, not what it fixed.
 *
 * As in pmc_exec_test.c the owner must be unprivileged, and the test must
 * not lower its own privilege: setuid(2) sets P_SUGID, fork(2) passes it
 * to the target, and p_candebug() then refuses that target to its own
 * owner.  Not covered, because each needs privileged setup a require.user
 * body cannot build: a set-id exec on a nosuid mount, a set-id #!
 * interpreter, and the two jail transitions.
 */

#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
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
 * Exec targets.  'setid' is the bit the entry must carry to exercise the
 * case; the argument vectors are chosen so the program touches nothing.
 * A target that blocks on stdin stays alive after the exec so that the
 * still-attached question can be asked; the caller holds the read end open.
 */
struct exec_target {
	const char	*path;
	const char	*const argv[5];
	mode_t		 setid;
	int		 blocks_on_stdin;
};

/**
 * @internal
 * Set-gid, blocks on stdin: stays alive so "still attached?" is answerable.
 */
static const struct exec_target setgid_targets[] = {
	{ "/usr/bin/wall", { "wall", NULL }, S_ISGID, 1 },
	{ NULL, { NULL }, 0, 0 }
};

/** Set-uid, exits at once: for a drop-case, completing without a panic is
 * @internal
 * the whole assertion. */
static const struct exec_target setuid_targets[] = {
	{ "/sbin/ping", { "ping", "-c", "1", "127.0.0.1", NULL }, S_ISUID, 0 },
	{ NULL, { NULL }, 0, 0 }
};

/** An ordinary, non-set-id target that blocks: sleep ignores stdin but
 * @internal
 * stays alive on its own, which is all exec_ordinary_keeps_pmc needs. */
static const struct exec_target ordinary_targets[] = {
	{ "/bin/sleep", { "sleep", "30", NULL }, 0, 0 },
	{ NULL, { NULL }, 0, 0 }
};

static const struct exec_target *
pick_target(const struct exec_target *tab, bool need_setid)
{
	struct stat sb;
	int i;

	for (i = 0; tab[i].path != NULL; i++) {
		if (stat(tab[i].path, &sb) != 0)
			continue;
		if (need_setid && (sb.st_mode & tab[i].setid) == 0)
			continue;
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
 * How the target should behave between fork and the measured exec.
 */
enum exec_via {
	VIA_EXECVE,	/* plain execve(2) */
	VIA_FEXECVE,	/* fexecve(2) of an fd opened O_EXEC */
	VIA_TRACED,	/* PT_TRACE_ME first, so the exec is credential-safe */
};

/**
 * @internal
 * Fork a target, attach a running counting PMC, and let it exec.  On
 * return the PMC is released.  When 'inspect' is set the target is expected
 * to stay alive past the exec, and *still_attached reports whether the PMC
 * survived it; a drop-case passes inspect=0 for a target that exits at once
 * (it has already torn its own descriptor down, so there is nothing to ask).
 */
static void
run_target(const struct exec_target *t, enum exec_via via, int inspect,
    int *still_attached)
{
	pmc_id_t id;
	pid_t target;
	char token;
	int gopipe[2], inpipe[2], status;

	ATF_REQUIRE(pipe(gopipe) == 0);
	ATF_REQUIRE(pipe(inpipe) == 0);
	ATF_REQUIRE((target = fork()) >= 0);

	if (target == 0) {
		int fd;

		(void)close(gopipe[1]);
		if (t->blocks_on_stdin)
			(void)dup2(inpipe[0], STDIN_FILENO);
		(void)close(inpipe[1]);
		if (via == VIA_TRACED)
			(void)ptrace(PT_TRACE_ME, 0, NULL, 0);
		if (read(gopipe[0], &token, 1) != 1)
			_exit(1);
		spin();
		if (via == VIA_FEXECVE) {
			if ((fd = open(t->path, O_EXEC)) < 0)
				_exit(1);
			(void)fexecve(fd, __DECONST(char **, t->argv), NULL);
		} else {
			(void)execv(t->path, __DECONST(char **, t->argv));
		}
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

	/* Into the exec. */
	ATF_REQUIRE(write(gopipe[1], "g", 1) == 1);
	(void)close(gopipe[1]);

	if (via == VIA_TRACED) {
		/*
		 * The exec of a set-id binary under a tracer stops the
		 * target with SIGTRAP and leaves its credentials unchanged.
		 * Reap that stop and detach the tracer; the hwpmc decision
		 * was already taken at exec time, so from here the target
		 * runs as an ordinary blocked process.
		 */
		ATF_REQUIRE(waitpid(target, &status, 0) == target);
		ATF_REQUIRE_MSG(WIFSTOPPED(status),
		    "traced target did not stop at exec (status 0x%x)", status);
		ATF_REQUIRE_MSG(ptrace(PT_DETACH, target, (caddr_t)1, 0) == 0,
		    "PT_DETACH: %s", strerror(errno));
	}

	if (inspect) {
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

ATF_TC(exec_ordinary_keeps_pmc);
ATF_TC_HEAD(exec_ordinary_keeps_pmc, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "a process-mode PMC survives an exec that does not change the "
	    "target's credentials");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(exec_ordinary_keeps_pmc, tc)
{
	const struct exec_target *t;
	int still_attached;

	require_unprivileged_owner();
	if ((t = pick_target(ordinary_targets, false)) == NULL)
		atf_tc_skip("no ordinary exec target available");

	run_target(t, VIA_EXECVE, 1, &still_attached);
	ATF_REQUIRE_MSG(still_attached,
	    "the PMC was dropped across an exec that changed no credentials");
}

ATF_TC(exec_setid_traced_keeps_pmc);
ATF_TC_HEAD(exec_setid_traced_keeps_pmc, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "a process-mode PMC survives its target's exec of a set-id binary "
	    "when tracing suppresses the credential change and the owner "
	    "remains entitled");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(exec_setid_traced_keeps_pmc, tc)
{
	const struct exec_target *t;
	int still_attached;

	require_unprivileged_owner();
	if ((t = pick_target(setgid_targets, true)) == NULL)
		atf_tc_skip("no set-gid exec target available");

	run_target(t, VIA_TRACED, 1, &still_attached);
	ATF_REQUIRE_MSG(still_attached,
	    "the PMC was dropped although tracing left the target's "
	    "credentials unchanged and its owner still entitled");
}

ATF_TC(exec_fexecve_setid_drops_pmc);
ATF_TC_HEAD(exec_fexecve_setid_drops_pmc, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "a process-mode PMC is detached when its target reaches a "
	    "credential-changing set-id binary through fexecve(2)");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(exec_fexecve_setid_drops_pmc, tc)
{
	const struct exec_target *t;
	int still_attached;

	require_unprivileged_owner();
	if ((t = pick_target(setuid_targets, true)) == NULL)
		atf_tc_skip("no set-uid exec target available");

	/* Completing at all is the assertion. */
	run_target(t, VIA_FEXECVE, 0, &still_attached);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, exec_ordinary_keeps_pmc);
	ATF_TP_ADD_TC(tp, exec_setid_traced_keeps_pmc);
	ATF_TP_ADD_TC(tp, exec_fexecve_setid_drops_pmc);

	return (atf_no_error());
}
