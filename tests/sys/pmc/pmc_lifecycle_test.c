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
 * Tests for hwpmc(4)'s process-attachment lifecycle: what happens as a PMC
 * is attached, inherited, and torn down, especially when the teardown does
 * not happen in the tidy order the common path assumes.
 *
 * The orderings driven here are a target that exits before it is
 * detached, an owner that exits before its target (which reaches the
 * REMOVE side of pmc_find_process_descriptor()),
 * a release of a still-running attached PMC, row exhaustion released out
 * of order, and PMC_F_DESCENDANTS, where the teardown has to account for a
 * target the owner never attached by hand.
 *
 * All PMCs are SOFT-class, so nothing here needs a hardware counter.
 */

#include <sys/param.h>
#include <sys/wait.h>

#include <errno.h>
#include <pmc.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

static void
require_hwpmc(void)
{

	if (pmc_init() != 0)
		atf_tc_skip("hwpmc(4) is not available");
}

/**
 * @internal
 * Allocate the first SOFT event this mode and flag set accept.  Which event
 * it is does not matter; that one exists is what the case needs.
 */
static pmc_id_t
allocate_soft_pmc_flags(enum pmc_mode mode, uint32_t flags)
{
	const char **names;
	char spec[128];
	pmc_id_t id;
	int nnames, i;

	if (pmc_event_names_of_class(PMC_CLASS_SOFT, &names, &nnames) != 0)
		return (PMC_ID_INVALID);
	for (i = 0; i < nnames; i++) {
		(void)snprintf(spec, sizeof(spec), "SOFT-%s", names[i]);
		if (pmc_allocate(spec, mode, flags, PMC_CPU_ANY, &id, 0) == 0)
			return (id);
	}
	return (PMC_ID_INVALID);
}

static pmc_id_t
require_soft_pmc(enum pmc_mode mode)
{
	pmc_id_t id;

	if ((id = allocate_soft_pmc_flags(mode, 0)) == PMC_ID_INVALID)
		atf_tc_skip("no SOFT-class PMC is allocatable");
	return (id);
}

/**
 * @internal
 * A child that does nothing until killed, for use as an attach target.
 */
static pid_t
spawn_idle_child(void)
{
	pid_t pid;

	pid = fork();
	if (pid == 0) {
		for (;;)
			(void)pause();
	}
	return (pid);
}

static void
reap(pid_t pid)
{
	int status;

	(void)kill(pid, SIGKILL);
	(void)waitpid(pid, &status, 0);
}

/**
 * @internal
 * The bookkeeping around attach and detach.
 */
ATF_TC_WITHOUT_HEAD(attach_detach_bookkeeping);
ATF_TC_BODY(attach_detach_bookkeeping, tc)
{
	pmc_id_t id;
	pid_t other;

	require_hwpmc();
	id = require_soft_pmc(PMC_MODE_TC);
	other = spawn_idle_child();
	ATF_REQUIRE(other > 0);

	ATF_REQUIRE_MSG(pmc_attach(id, getpid()) == 0, "pmc_attach: %s",
	    strerror(errno));

	/* A second attach of the same target must be refused, not doubled. */
	errno = 0;
	ATF_CHECK_MSG(pmc_attach(id, getpid()) != 0,
	    "a second attach of the same target succeeded");

	/* Detaching a pid that was never attached must be refused. */
	errno = 0;
	ATF_CHECK_MSG(pmc_detach(id, other) != 0,
	    "detached a target that was never attached");

	/* One detach undoes the one attach. */
	ATF_CHECK_MSG(pmc_detach(id, getpid()) == 0, "pmc_detach: %s",
	    strerror(errno));

	/* A second detach must now be refused. */
	errno = 0;
	ATF_CHECK_MSG(pmc_detach(id, getpid()) != 0,
	    "a second detach of the same target succeeded");

	ATF_CHECK_MSG(pmc_release(id) == 0, "pmc_release: %s",
	    strerror(errno));
	reap(other);
}

/**
 * @internal
 * Detach a target that has already exited and been reaped.  The
 * kernel's own eventhandler removes an exiting process's descriptor, so by
 * the time we detach there is nothing there; it must say so cleanly, and
 * the PMC must still release.
 */
ATF_TC_WITHOUT_HEAD(detach_after_target_reaped);
ATF_TC_BODY(detach_after_target_reaped, tc)
{
	pmc_id_t id;
	pid_t target;

	require_hwpmc();
	id = require_soft_pmc(PMC_MODE_TC);
	target = spawn_idle_child();
	ATF_REQUIRE(target > 0);

	ATF_REQUIRE_MSG(pmc_attach(id, target) == 0, "pmc_attach: %s",
	    strerror(errno));

	/* Kill and fully reap the target before detaching. */
	reap(target);

	errno = 0;
	ATF_CHECK_MSG(pmc_detach(id, target) != 0,
	    "detached a target that had exited and been reaped");

	ATF_CHECK_MSG(pmc_release(id) == 0, "pmc_release: %s",
	    strerror(errno));
}

/**
 * @internal
 * Release a PMC that is still attached and still running, with no
 * prior detach or stop: the release path has to reclaim the reference
 * the running PMC still holds rather than wedge.
 */
ATF_TC_WITHOUT_HEAD(release_running_attached);
ATF_TC_BODY(release_running_attached, tc)
{
	pmc_id_t id;

	require_hwpmc();
	id = require_soft_pmc(PMC_MODE_TC);

	ATF_REQUIRE_MSG(pmc_attach(id, getpid()) == 0, "pmc_attach: %s",
	    strerror(errno));
	ATF_REQUIRE_MSG(pmc_start(id) == 0, "pmc_start: %s", strerror(errno));

	/* Run a little, so the PMC is actually loaded on a CPU. */
	{
		volatile unsigned long s = 0;
		int i;

		for (i = 0; i < 5000000; i++)
			s += i;
	}
	(void)usleep(100000);

	/* Straight to release: no stop, no detach. */
	ATF_CHECK_MSG(pmc_release(id) == 0,
	    "release of a running, attached PMC failed: %s", strerror(errno));

	/* The driver is still usable afterwards. */
	{
		pmc_id_t again = allocate_soft_pmc_flags(PMC_MODE_TC, 0);

		ATF_CHECK_MSG(again != PMC_ID_INVALID,
		    "hwpmc is unusable after the release");
		if (again != PMC_ID_INVALID)
			(void)pmc_release(again);
	}
}

/**
 * @internal
 * The owner exits before the target.  A child process is the owner:
 * it allocates a PMC and attaches it to a sibling that outlives it, then
 * exits without releasing.  The kernel tears the owner's PMCs down through
 * the REMOVE side of pmc_find_process_descriptor() - the other of its two
 * unlink mechanisms, which no test exercised.  The parent then confirms
 * the driver is still usable and the target still alive.
 */
ATF_TC_WITHOUT_HEAD(owner_exit_before_target);
ATF_TC_BODY(owner_exit_before_target, tc)
{
	pmc_id_t id;
	pid_t target, owner;
	int status;

	require_hwpmc();

	target = spawn_idle_child();
	ATF_REQUIRE(target > 0);

	owner = fork();
	ATF_REQUIRE(owner >= 0);
	if (owner == 0) {
		/* Owner: attach to the sibling target, then just exit. */
		if (pmc_init() != 0)
			_exit(10);
		id = allocate_soft_pmc_flags(PMC_MODE_TC, 0);
		if (id == PMC_ID_INVALID)
			_exit(11);
		if (pmc_attach(id, target) != 0)
			_exit(12);
		if (pmc_start(id) != 0)
			_exit(13);
		/* Exit with the PMC still allocated, attached and running. */
		_exit(0);
	}

	ATF_REQUIRE(waitpid(owner, &status, 0) == owner);
	ATF_REQUIRE_MSG(WIFEXITED(status),
	    "the owner did not exit normally (it may have panicked the "
	    "kernel; status 0x%x)", status);
	ATF_REQUIRE_MSG(WEXITSTATUS(status) == 0,
	    "the owner's setup failed (exit %d): 10=init 11=alloc 12=attach "
	    "13=start", WEXITSTATUS(status));

	/*
	 * The owner is gone.  Its descriptor teardown must have run without
	 * wedging: prove the driver still works and the target is still here.
	 */
	id = allocate_soft_pmc_flags(PMC_MODE_TC, 0);
	ATF_CHECK_MSG(id != PMC_ID_INVALID,
	    "hwpmc is unusable after the owner exited with a live target");
	if (id != PMC_ID_INVALID) {
		ATF_CHECK(pmc_attach(id, getpid()) == 0);
		ATF_CHECK(pmc_detach(id, getpid()) == 0);
		ATF_CHECK(pmc_release(id) == 0);
	}
	ATF_CHECK_MSG(kill(target, 0) == 0,
	    "the target did not survive the owner's exit");
	reap(target);
}

/**
 * @internal
 * Allocate SOFT PMCs until the rows are exhausted, then release them
 * in an order other than allocation order.  Exercises the row free-list
 * against out-of-order frees.
 */
ATF_TC_WITHOUT_HEAD(exhaust_then_release_reverse);
ATF_TC_BODY(exhaust_then_release_reverse, tc)
{
	pmc_id_t ids[64];
	int n, i;

	require_hwpmc();

	n = 0;
	while (n < (int)nitems(ids)) {
		pmc_id_t id = allocate_soft_pmc_flags(PMC_MODE_TC, 0);

		if (id == PMC_ID_INVALID)
			break;
		ids[n++] = id;
	}
	ATF_REQUIRE_MSG(n > 0, "not one SOFT PMC could be allocated");

	/* At least one allocation should have failed at exhaustion. */
	ATF_CHECK_MSG(n < (int)nitems(ids),
	    "allocation never hit a limit in %zu tries", nitems(ids));

	/* Release in reverse order; every one must succeed. */
	for (i = n - 1; i >= 0; i--)
		ATF_CHECK_MSG(pmc_release(ids[i]) == 0,
		    "reverse release of id 0x%08x (slot %d) failed: %s",
		    ids[i], i, strerror(errno));
}

/**
 * @internal
 * A PMC_F_DESCENDANTS counter, whose child inherits the attachment.
 * The owner attaches to itself with descendants set, forks a child that
 * does a little work and exits, reaps it, and releases.  The teardown has
 * to account for the child the owner never attached to by hand.
 */
ATF_TC_WITHOUT_HEAD(descendants_inherit_and_release);
ATF_TC_BODY(descendants_inherit_and_release, tc)
{
	pmc_id_t id;
	pid_t child;
	int status;

	require_hwpmc();
	id = allocate_soft_pmc_flags(PMC_MODE_TC, PMC_F_DESCENDANTS);
	if (id == PMC_ID_INVALID)
		atf_tc_skip("no SOFT PMC allocatable with PMC_F_DESCENDANTS");

	ATF_REQUIRE_MSG(pmc_attach(id, getpid()) == 0, "pmc_attach: %s",
	    strerror(errno));
	ATF_REQUIRE_MSG(pmc_start(id) == 0, "pmc_start: %s", strerror(errno));

	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		volatile unsigned long s = 0;
		int i;

		for (i = 0; i < 2000000; i++)
			s += i;
		_exit(0);
	}
	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	ATF_CHECK_MSG(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	    "the descendant did not exit cleanly (status 0x%x)", status);

	ATF_CHECK(pmc_stop(id) == 0);
	ATF_CHECK_MSG(pmc_release(id) == 0,
	    "release of a descendants PMC failed: %s", strerror(errno));
}

/**
 * @internal
 * The fork-storm variant: descendants set, then many short-lived children,
 * so several inherit-and-exit teardowns overlap, then release.
 */
ATF_TC_WITHOUT_HEAD(descendants_fork_storm);
ATF_TC_BODY(descendants_fork_storm, tc)
{
	pmc_id_t id;
	int i, status;

	require_hwpmc();
	id = allocate_soft_pmc_flags(PMC_MODE_TC, PMC_F_DESCENDANTS);
	if (id == PMC_ID_INVALID)
		atf_tc_skip("no SOFT PMC allocatable with PMC_F_DESCENDANTS");

	ATF_REQUIRE_MSG(pmc_attach(id, getpid()) == 0, "pmc_attach: %s",
	    strerror(errno));
	ATF_REQUIRE_MSG(pmc_start(id) == 0, "pmc_start: %s", strerror(errno));

	for (i = 0; i < 32; i++) {
		pid_t c = fork();

		if (c == 0) {
			volatile unsigned long s = 0;
			int j;

			for (j = 0; j < 200000; j++)
				s += j;
			_exit(0);
		}
		if (c > 0)
			(void)waitpid(c, &status, 0);
	}

	ATF_CHECK(pmc_stop(id) == 0);
	ATF_CHECK_MSG(pmc_release(id) == 0,
	    "release after a descendants fork storm failed: %s",
	    strerror(errno));
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, attach_detach_bookkeeping);
	ATF_TP_ADD_TC(tp, detach_after_target_reaped);
	ATF_TP_ADD_TC(tp, release_running_attached);
	ATF_TP_ADD_TC(tp, owner_exit_before_target);
	ATF_TP_ADD_TC(tp, exhaust_then_release_reverse);
	ATF_TP_ADD_TC(tp, descendants_inherit_and_release);
	ATF_TP_ADD_TC(tp, descendants_fork_storm);

	return (atf_no_error());
}
