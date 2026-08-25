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
 * Tests for hwpmc(4)'s handling of the PMC handle and for the privilege
 * boundaries around the operations that do not take one.
 *
 * A pmc_id_t packs CPU, mode, class and row index into one 32-bit word,
 * which the kernel hands to userland and accepts back on eleven
 * operations.  The cases pass ids that were never allocated, ids that were
 * released, and ids belonging to a different process, and require each
 * operation to refuse cleanly rather than act on a wrong PMC.
 *
 * Two properties are asserted because a refactor could break either with
 * every existing test still passing: the CPU field is 12 bits, so a caller
 * may name CPU 4095 on a machine that has four; and ids are unique per
 * owner rather than globally, so two processes are given the same numeric
 * id and only the per-owner lookup keeps them apart.
 *
 * The PMCs are SOFT-class, so nothing here needs a hardware counter.  The
 * privilege cases need an unprivileged subject - as root the checks under
 * test are never reached - hence require.user.
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
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

/**
 * @internal
 * The operations that take a pmc_id_t and reach the kernel with it.
 * pmc_width() is deliberately absent: it answers from a userland copy of
 * the CPU info and never enters the driver.
 */
static int op_attach(pmc_id_t id)	{ return (pmc_attach(id, getpid())); }
static int op_detach(pmc_id_t id)	{ return (pmc_detach(id, getpid())); }
static int op_start(pmc_id_t id)	{ return (pmc_start(id)); }
static int op_stop(pmc_id_t id)		{ return (pmc_stop(id)); }
static int op_write(pmc_id_t id)	{ return (pmc_write(id, 0)); }
static int op_set(pmc_id_t id)		{ return (pmc_set(id, 0)); }
static int op_release(pmc_id_t id)	{ return (pmc_release(id)); }

static int
op_read(pmc_id_t id)
{
	pmc_value_t v;

	return (pmc_read(id, &v));
}

static int
op_rw(pmc_id_t id)
{
	pmc_value_t v;

	return (pmc_rw(id, 0, &v));
}

static int
op_caps(pmc_id_t id)
{
	uint32_t c;

	return (pmc_capabilities(id, &c));
}

static int
op_getmsr(pmc_id_t id)
{
	uint32_t msr;

	return (pmc_get_msr(id, &msr));
}

static const struct {
	const char	*name;
	int		(*fn)(pmc_id_t);
} id_ops[] = {
	{ "attach",	op_attach },
	{ "detach",	op_detach },
	{ "start",	op_start },
	{ "stop",	op_stop },
	{ "read",	op_read },
	{ "write",	op_write },
	{ "rw",		op_rw },
	{ "set",	op_set },
	{ "caps",	op_caps },
	{ "getmsr",	op_getmsr },
	{ "release",	op_release },
};

static void
require_hwpmc(void)
{

	if (pmc_init() != 0)
		atf_tc_skip("hwpmc(4) is not available");
}

static void
require_unprivileged(void)
{

	if (geteuid() == 0)
		atf_tc_skip("the subject must be unprivileged: root passes "
		    "the check under test");
}

/**
 * @internal
 * Allocate the first SOFT event this mode accepts.  Every SOFT event takes
 * the same allocate/attach/detach/release path, so which one it is does not
 * matter; that one exists at all is what the case needs.
 */
static pmc_id_t
allocate_soft_pmc(enum pmc_mode mode)
{
	const char **names;
	char spec[128];
	pmc_id_t id;
	int nnames, i;

	if (pmc_event_names_of_class(PMC_CLASS_SOFT, &names, &nnames) != 0)
		return (PMC_ID_INVALID);
	for (i = 0; i < nnames; i++) {
		(void)snprintf(spec, sizeof(spec), "SOFT-%s", names[i]);
		if (pmc_allocate(spec, mode, 0, PMC_CPU_ANY, &id, 0) == 0)
			return (id);
	}
	return (PMC_ID_INVALID);
}

static pmc_id_t
require_soft_pmc(enum pmc_mode mode)
{
	pmc_id_t id;

	if ((id = allocate_soft_pmc(mode)) == PMC_ID_INVALID)
		atf_tc_skip("no SOFT-class PMC is allocatable");
	return (id);
}

/**
 * @internal
 * Every id-taking operation must refuse this id.  Checks rather than
 * requires, so one operation that wrongly accepts does not hide the rest.
 */
static void
check_all_ops_refuse(pmc_id_t id, const char *what)
{
	size_t i;

	for (i = 0; i < nitems(id_ops); i++) {
		errno = 0;
		ATF_CHECK_MSG(id_ops[i].fn(id) != 0,
		    "pmc_%s() accepted %s id 0x%08x", id_ops[i].name, what,
		    id);
	}
}

ATF_TC_WITHOUT_HEAD(never_allocated_id);
ATF_TC_BODY(never_allocated_id, tc)
{
	pmc_id_t forged[6];
	size_t i;
	int ncpu;

	require_hwpmc();
	ncpu = pmc_ncpu();
	ATF_REQUIRE(ncpu > 0);

	/*
	 * Nothing has been allocated in this process, so every one of these
	 * has to be refused - including the two that are well-formed apart
	 * from naming a CPU that does not exist.
	 */
	forged[0] = PMC_ID_INVALID;
	forged[1] = PMC_ID_MAKE_ID(0xFFF, PMC_MODE_TC, PMC_CLASS_SOFT, 0);
	forged[2] = PMC_ID_MAKE_ID(PMC_CPU_ANY, PMC_MODE_TC, PMC_CLASS_SOFT,
	    0xFF);
	forged[3] = PMC_ID_MAKE_ID(ncpu, PMC_MODE_TC, PMC_CLASS_SOFT, 0);
	forged[4] = 0;
	forged[5] = PMC_ID_MAKE_ID(PMC_CPU_ANY, 0xF, 0xFF, 0);

	for (i = 0; i < nitems(forged); i++)
		check_all_ops_refuse(forged[i], "never-allocated");
}

ATF_TC_WITHOUT_HEAD(released_id);
ATF_TC_BODY(released_id, tc)
{
	pmc_id_t id;

	require_hwpmc();
	id = require_soft_pmc(PMC_MODE_TC);

	ATF_REQUIRE_MSG(pmc_attach(id, getpid()) == 0, "pmc_attach: %s",
	    strerror(errno));
	ATF_REQUIRE_MSG(pmc_start(id) == 0, "pmc_start: %s", strerror(errno));
	ATF_REQUIRE_MSG(pmc_stop(id) == 0, "pmc_stop: %s", strerror(errno));
	ATF_REQUIRE_MSG(pmc_detach(id, getpid()) == 0, "pmc_detach: %s",
	    strerror(errno));
	ATF_REQUIRE_MSG(pmc_release(id) == 0, "pmc_release: %s",
	    strerror(errno));

	/* The same value is now stale.  Reusing it is the double-free shape. */
	check_all_ops_refuse(id, "released");
}

ATF_TC_WITHOUT_HEAD(another_owners_id);
ATF_TC_BODY(another_owners_id, tc)
{
	pmc_id_t id, mine;
	pmc_value_t v;
	pid_t child;
	ssize_t n;
	int down[2], up[2], status;
	char token;

	require_hwpmc();
	ATF_REQUIRE(pipe(down) == 0);
	ATF_REQUIRE(pipe(up) == 0);
	ATF_REQUIRE((child = fork()) >= 0);

	if (child == 0) {
		pmc_value_t cv;
		pmc_id_t cid;

		(void)close(down[1]);
		(void)close(up[0]);
		if (pmc_init() != 0)
			_exit(2);
		if ((cid = allocate_soft_pmc(PMC_MODE_TC)) == PMC_ID_INVALID)
			_exit(3);
		if (pmc_attach(cid, getpid()) != 0 || pmc_start(cid) != 0)
			_exit(4);
		if (write(up[1], &cid, sizeof(cid)) != (ssize_t)sizeof(cid))
			_exit(5);
		if (read(down[0], &token, 1) != 1)
			_exit(6);
		/* Untouched by anything the parent did? */
		_exit(pmc_read(cid, &cv) == 0 ? 0 : 7);
	}
	(void)close(down[0]);
	(void)close(up[1]);

	n = read(up[0], &id, sizeof(id));
	if (n != (ssize_t)sizeof(id)) {
		(void)kill(child, SIGKILL);
		(void)waitpid(child, &status, 0);
		atf_tc_skip("the child could not allocate a SOFT-class PMC");
	}

	/* We own nothing, so the child's id must not resolve for us. */
	check_all_ops_refuse(id, "another owner's");

	/*
	 * Now hold one of our own.  Ids are per owner, so ours is very
	 * likely the same number; operating on it must still reach only
	 * ours, which the child's exit status confirms.
	 */
	mine = allocate_soft_pmc(PMC_MODE_TC);
	if (mine != PMC_ID_INVALID) {
		ATF_CHECK_MSG(pmc_attach(mine, getpid()) == 0, "pmc_attach: %s",
		    strerror(errno));
		ATF_CHECK(pmc_read(mine, &v) == 0);
		ATF_CHECK(pmc_detach(mine, getpid()) == 0);
		ATF_CHECK(pmc_release(mine) == 0);
	}

	ATF_REQUIRE(write(down[1], "g", 1) == 1);
	(void)close(down[1]);
	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	ATF_REQUIRE_MSG(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	    "the child's own PMC did not survive our use of its id "
	    "(child exit %d)", WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

ATF_TC_WITHOUT_HEAD(read_write_before_start);
ATF_TC_BODY(read_write_before_start, tc)
{
	pmc_value_t old, v;
	pmc_id_t id;

	require_hwpmc();
	id = require_soft_pmc(PMC_MODE_TC);
	ATF_REQUIRE_MSG(pmc_attach(id, getpid()) == 0, "pmc_attach: %s",
	    strerror(errno));

	/* Allocated and attached, never started: a read must still work. */
	ATF_CHECK_MSG(pmc_read(id, &v) == 0, "pmc_read before start: %s",
	    strerror(errno));

	ATF_CHECK_MSG(pmc_write(id, 42) == 0, "pmc_write before start: %s",
	    strerror(errno));
	ATF_CHECK_MSG(pmc_read(id, &v) == 0, "pmc_read: %s", strerror(errno));
	ATF_CHECK_MSG(v == 42, "wrote 42, read back %ju", (uintmax_t)v);

	ATF_CHECK(pmc_rw(id, 7, &old) == 0);
	ATF_CHECK_MSG(old == 42, "pmc_rw returned %ju, expected the 42 "
	    "written before it", (uintmax_t)old);

	ATF_REQUIRE(pmc_start(id) == 0);
	ATF_REQUIRE(pmc_stop(id) == 0);
	ATF_CHECK(pmc_detach(id, getpid()) == 0);
	ATF_CHECK(pmc_release(id) == 0);
}

ATF_TC(pmcadmin_requires_privilege);
ATF_TC_HEAD(pmcadmin_requires_privilege, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "PMC_OP_PMCADMIN is refused to an unprivileged caller");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(pmcadmin_requires_privilege, tc)
{

	require_hwpmc();
	require_unprivileged();

	errno = 0;
	ATF_CHECK_MSG(pmc_disable(0, 0) != 0,
	    "an unprivileged process disabled a PMC row");
	ATF_CHECK_MSG(errno == EPERM, "pmc_disable: expected EPERM, got %s",
	    strerror(errno));

	errno = 0;
	ATF_CHECK_MSG(pmc_enable(0, 0) != 0,
	    "an unprivileged process enabled a PMC row");
	ATF_CHECK_MSG(errno == EPERM, "pmc_enable: expected EPERM, got %s",
	    strerror(errno));
}

ATF_TC(system_mode_requires_privilege);
ATF_TC_HEAD(system_mode_requires_privilege, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "a system-wide PMC is refused to an unprivileged caller");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(system_mode_requires_privilege, tc)
{
	const char **names;
	char spec[128];
	pmc_id_t id;
	size_t len;
	int nnames, i, unpriv;

	require_hwpmc();
	require_unprivileged();

	len = sizeof(unpriv);
	if (sysctlbyname("security.bsd.unprivileged_syspmcs", &unpriv, &len,
	    NULL, 0) != 0)
		atf_tc_skip("security.bsd.unprivileged_syspmcs is unreadable");
	if (unpriv != 0)
		atf_tc_skip("security.bsd.unprivileged_syspmcs is set: "
		    "unprivileged system-wide PMCs are permitted here");

	if (pmc_event_names_of_class(PMC_CLASS_SOFT, &names, &nnames) != 0 ||
	    nnames == 0)
		atf_tc_skip("no SOFT-class events");

	/*
	 * A system-mode PMC must name a real CPU: PMC_CPU_ANY is rejected
	 * before the privilege check is reached, which would make this case
	 * pass for the wrong reason.
	 */
	for (i = 0; i < nnames; i++) {
		(void)snprintf(spec, sizeof(spec), "SOFT-%s", names[i]);
		errno = 0;
		if (pmc_allocate(spec, PMC_MODE_SC, 0, 0, &id, 0) == 0) {
			(void)pmc_release(id);
			atf_tc_fail("an unprivileged process allocated a "
			    "system-wide PMC (%s)", spec);
		}
		ATF_CHECK_MSG(errno == EPERM,
		    "%s: expected EPERM, got %s", spec, strerror(errno));
	}
}

/**
 * @internal
 * A target that has exec'ed a set-id binary carries P_SUGID, which
 * p_candebug() refuses to an unprivileged subject - the same rule
 * PMC_OP_PMCATTACH inherits.  wall(1) is set-gid and reads its message from
 * stdin, so it stays alive on a pipe long enough to be attached to.
 */
ATF_TC(attach_to_sugid_target);
ATF_TC_HEAD(attach_to_sugid_target, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "attaching a PMC to a process that has exec'ed a set-id binary "
	    "is refused to an unprivileged owner");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(attach_to_sugid_target, tc)
{
	static const char *const argv[] = { "wall", NULL };
	static const char path[] = "/usr/bin/wall";
	struct stat sb;
	pmc_id_t id;
	pid_t target;
	int gopipe[2], inpipe[2], status, rc;
	char token;

	require_hwpmc();
	require_unprivileged();

	if (stat(path, &sb) != 0 || (sb.st_mode & S_ISGID) == 0)
		atf_tc_skip("%s is not set-gid here", path);
	if (getegid() == sb.st_gid)
		atf_tc_skip("the subject is already in %s's group, so it may "
		    "trace the target", path);

	ATF_REQUIRE(pipe(gopipe) == 0);
	ATF_REQUIRE(pipe(inpipe) == 0);
	ATF_REQUIRE((target = fork()) >= 0);
	if (target == 0) {
		(void)close(gopipe[1]);
		(void)dup2(inpipe[0], STDIN_FILENO);
		(void)close(inpipe[1]);
		if (read(gopipe[0], &token, 1) != 1)
			_exit(1);
		(void)execv(path, __DECONST(char **, argv));
		_exit(1);
	}
	(void)close(gopipe[0]);
	(void)close(inpipe[0]);

	id = require_soft_pmc(PMC_MODE_TC);

	/* Let it exec, then give it time to get there. */
	ATF_REQUIRE(write(gopipe[1], "g", 1) == 1);
	(void)close(gopipe[1]);
	(void)usleep(400000);

	errno = 0;
	rc = pmc_attach(id, target);
	if (rc == 0)
		(void)pmc_detach(id, target);
	(void)kill(target, SIGKILL);
	(void)close(inpipe[1]);
	(void)waitpid(target, &status, 0);
	(void)pmc_release(id);

	ATF_CHECK_MSG(rc != 0,
	    "an unprivileged owner attached a PMC to a set-id target");
	ATF_CHECK_MSG(errno == EPERM, "pmc_attach: expected EPERM, got %s",
	    strerror(errno));
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, never_allocated_id);
	ATF_TP_ADD_TC(tp, released_id);
	ATF_TP_ADD_TC(tp, another_owners_id);
	ATF_TP_ADD_TC(tp, read_write_before_start);
	ATF_TP_ADD_TC(tp, pmcadmin_requires_privilege);
	ATF_TP_ADD_TC(tp, system_mode_requires_privilege);
	ATF_TP_ADD_TC(tp, attach_to_sugid_target);

	return (atf_no_error());
}
