/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2021 Kyle Evans <kevans@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/jail.h>
#include <sys/procdesc.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include <atf-c.h>

#define	SP_PARENT	0
#define	SP_CHILD	1

struct jail_test_info {
	cpuset_t	jail_tidmask;
	cpusetid_t	jail_cpuset;
	cpusetid_t	jail_child_cpuset;
};

struct jail_test_cb_params {
	struct jail_test_info		info;
	cpuset_t			mask;
	cpusetid_t			rootid;
	cpusetid_t			setid;
};

typedef void (*jail_test_cb)(struct jail_test_cb_params *);

#define	FAILURE_JAIL	42
#define	FAILURE_MASK	43
#define	FAILURE_JAILSET	44
#define	FAILURE_PIDSET	45
#define	FAILURE_SEND	46
#define	FAILURE_DEADLK	47
#define	FAILURE_ATTACH	48
#define	FAILURE_BADAFFIN	49
#define	FAILURE_SUCCESS	50

static const char *
do_jail_errstr(int error)
{

	switch (error) {
	case FAILURE_JAIL:
		return ("jail_set(2) failed");
	case FAILURE_MASK:
		return ("Failed to get the thread cpuset mask");
	case FAILURE_JAILSET:
		return ("Failed to get the jail setid");
	case FAILURE_PIDSET:
		return ("Failed to get the pid setid");
	case FAILURE_SEND:
		return ("Failed to send(2) cpuset information");
	case FAILURE_DEADLK:
		return ("Deadlock hit trying to attach to jail");
	case FAILURE_ATTACH:
		return ("jail_attach(2) failed");
	case FAILURE_BADAFFIN:
		return ("Unexpected post-attach affinity");
	case FAILURE_SUCCESS:
		return ("jail_attach(2) succeeded, but should have failed.");
	default:
		return (NULL);
	}
}

static void
skip_ltncpu(int ncpu, cpuset_t *mask)
{

	CPU_ZERO(mask);
	ATF_REQUIRE_EQ(0, cpuset_getaffinity(CPU_LEVEL_CPUSET, CPU_WHICH_PID,
	    -1, sizeof(*mask), mask));
	if (CPU_COUNT(mask) < ncpu)
		atf_tc_skip("Test requires %d or more cores.", ncpu);
}

ATF_TC(newset);
ATF_TC_HEAD(newset, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test cpuset(2)");
}
ATF_TC_BODY(newset, tc)
{
	cpusetid_t nsetid, setid, qsetid;

	/* Obtain our initial set id. */
	ATF_REQUIRE_EQ(0, cpuset_getid(CPU_LEVEL_CPUSET, CPU_WHICH_TID, -1,
	    &setid));

	/* Create a new one. */
	ATF_REQUIRE_EQ(0, cpuset(&nsetid));
	ATF_CHECK(nsetid != setid);

	/* Query id again, make sure it's equal to the one we just got. */
	ATF_REQUIRE_EQ(0, cpuset_getid(CPU_LEVEL_CPUSET, CPU_WHICH_TID, -1,
	    &qsetid));
	ATF_CHECK_EQ(nsetid, qsetid);
}

ATF_TC(transient);
ATF_TC_HEAD(transient, tc)
{
	atf_tc_set_md_var(tc, "descr",
	   "Test that transient cpusets are freed.");
}
ATF_TC_BODY(transient, tc)
{
	cpusetid_t isetid, scratch, setid;

	ATF_REQUIRE_EQ(0, cpuset_getid(CPU_LEVEL_CPUSET, CPU_WHICH_PID, -1,
	    &isetid));

	ATF_REQUIRE_EQ(0, cpuset(&setid));
	ATF_REQUIRE_EQ(0, cpuset_getid(CPU_LEVEL_CPUSET, CPU_WHICH_CPUSET,
	    setid, &scratch));

	/*
	 * Return back to our initial cpuset; the kernel should free the cpuset
	 * we just created.
	 */
	ATF_REQUIRE_EQ(0, cpuset_setid(CPU_WHICH_PID, -1, isetid));
	ATF_REQUIRE_EQ(-1, cpuset_getid(CPU_LEVEL_CPUSET, CPU_WHICH_CPUSET,
	    setid, &scratch));
	ATF_CHECK_EQ(ESRCH, errno);
}

ATF_TC(deadlk);
ATF_TC_HEAD(deadlk, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test against disjoint cpusets.");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(deadlk, tc)
{
	cpusetid_t setid;
	cpuset_t dismask, mask, omask;
	int fcpu, i, found, ncpu, second;

	/* Make sure we have 3 cpus, so we test partial overlap. */
	skip_ltncpu(3, &omask);

	ATF_REQUIRE_EQ(0, cpuset(&setid));
	CPU_ZERO(&mask);
	CPU_ZERO(&dismask);
	CPU_COPY(&omask, &mask);
	CPU_COPY(&omask, &dismask);
	fcpu = CPU_FFS(&mask);
	ncpu = CPU_COUNT(&mask);

	/*
	 * Turn off all but the first two for mask, turn off the first for
	 * dismask and turn them all off for both after the third.
	 */
	for (i = fcpu - 1, found = 0; i < CPU_MAXSIZE && found != ncpu; i++) {
		if (CPU_ISSET(i, &omask)) {
			found++;
			if (found == 1) {
				CPU_CLR(i, &dismask);
			} else if (found == 2) {
				second = i;
			} else if (found >= 3) {
				CPU_CLR(i, &mask);
				if (found > 3)
					CPU_CLR(i, &dismask);
			}
		}
	}

	ATF_REQUIRE_EQ(0, cpuset_setaffinity(CPU_LEVEL_CPUSET, CPU_WHICH_PID,
	    -1, sizeof(mask), &mask));

	/* Must be a strict subset! */
	ATF_REQUIRE_EQ(-1, cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID,
	    -1, sizeof(dismask), &dismask));
	ATF_REQUIRE_EQ(EINVAL, errno);

	/*
	 * We'll set our anonymous set to the 0,1 set that currently matches
	 * the process.  If we then set the process to the 1,2 set that's in
	 * dismask, we should then personally be restricted down to the single
	 * overlapping CPOU.
	 */
	ATF_REQUIRE_EQ(0, cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID,
	    -1, sizeof(mask), &mask));
	ATF_REQUIRE_EQ(0, cpuset_setaffinity(CPU_LEVEL_CPUSET, CPU_WHICH_PID,
	    -1, sizeof(dismask), &dismask));
	ATF_REQUIRE_EQ(0, cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID,
	    -1, sizeof(mask), &mask));
	ATF_REQUIRE_EQ(1, CPU_COUNT(&mask));
	ATF_REQUIRE(CPU_ISSET(second, &mask));

	/*
	 * Finally, clearing the overlap and attempting to set the process
	 * cpuset to a completely disjoint mask should fail, because this
	 * process will then not have anything to run on.
	 */
	CPU_CLR(second, &dismask);
	ATF_REQUIRE_EQ(-1, cpuset_setaffinity(CPU_LEVEL_CPUSET, CPU_WHICH_PID,
	    -1, sizeof(dismask), &dismask));
	ATF_REQUIRE_EQ(EDEADLK, errno);
}

static int
do_jail(int sock)
{
	struct jail_test_info info;
	struct iovec iov[2];
	char *name;
	int error;

	if (asprintf(&name, "cpuset_%d", getpid()) == -1)
		_exit(42);

	iov[0].iov_base = "name";
	iov[0].iov_len = 5;

	iov[1].iov_base = name;
	iov[1].iov_len = strlen(name) + 1;

	if (jail_set(iov, 2, JAIL_CREATE | JAIL_ATTACH) < 0)
		return (FAILURE_JAIL);

	/* Record parameters, kick them over, then make a swift exit. */
	CPU_ZERO(&info.jail_tidmask);
	error = cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID,
	    -1, sizeof(info.jail_tidmask), &info.jail_tidmask);
	if (error != 0)
		return (FAILURE_MASK);

	error = cpuset_getid(CPU_LEVEL_ROOT, CPU_WHICH_TID, -1,
	    &info.jail_cpuset);
	if (error != 0)
		return (FAILURE_JAILSET);
	error = cpuset_getid(CPU_LEVEL_CPUSET, CPU_WHICH_TID, -1,
	    &info.jail_child_cpuset);
	if (error != 0)
		return (FAILURE_PIDSET);
	if (send(sock, &info, sizeof(info), 0) != sizeof(info))
		return (FAILURE_SEND);
	return (0);
}

static void
do_jail_test(int ncpu, bool newset, jail_test_cb prologue,
    jail_test_cb epilogue)
{
	struct jail_test_cb_params cbp;
	const char *errstr;
	pid_t pid;
	int error, sock, sockpair[2], status;

	memset(&cbp.info, '\0', sizeof(cbp.info));

	skip_ltncpu(ncpu, &cbp.mask);

	ATF_REQUIRE_EQ(0, cpuset_getid(CPU_LEVEL_ROOT, CPU_WHICH_PID, -1,
	    &cbp.rootid));
	if (newset)
		ATF_REQUIRE_EQ(0, cpuset(&cbp.setid));
	else
		ATF_REQUIRE_EQ(0, cpuset_getid(CPU_LEVEL_CPUSET, CPU_WHICH_PID,
		    -1, &cbp.setid));
	/* Special hack for prison0; it uses cpuset 1 as the root. */
	if (cbp.rootid == 0)
		cbp.rootid = 1;

	/* Not every test needs early setup. */
	if (prologue != NULL)
		(*prologue)(&cbp);

	ATF_REQUIRE_EQ(0, socketpair(PF_UNIX, SOCK_STREAM, 0, sockpair));
	ATF_REQUIRE((pid = fork()) != -1);

	if (pid == 0) {
		/* Child */
		close(sockpair[SP_PARENT]);
		sock = sockpair[SP_CHILD];

		_exit(do_jail(sock));
	} else {
		/* Parent */
		sock = sockpair[SP_PARENT];
		close(sockpair[SP_CHILD]);

		while ((error = waitpid(pid, &status, 0)) == -1 &&
		    errno == EINTR) {
		}

		ATF_REQUIRE_EQ(sizeof(cbp.info), recv(sock, &cbp.info,
		    sizeof(cbp.info), 0));

		/* Sanity check the exit info. */
		ATF_REQUIRE_EQ(pid, error);
		ATF_REQUIRE(WIFEXITED(status));
		if (WEXITSTATUS(status) != 0) {
			errstr = do_jail_errstr(WEXITSTATUS(status));
			if (errstr != NULL)
				atf_tc_fail("%s", errstr);
			else
				atf_tc_fail("Unknown error '%d'",
				    WEXITSTATUS(status));
		}

		epilogue(&cbp);
	}
}

static void
jail_attach_mutate_pro(struct jail_test_cb_params *cbp)
{
	cpuset_t *mask;
	int count;

	mask = &cbp->mask;

	/* Knock out the first cpu. */
	count = CPU_COUNT(mask);
	CPU_CLR(CPU_FFS(mask) - 1, mask);
	ATF_REQUIRE_EQ(count - 1, CPU_COUNT(mask));
	ATF_REQUIRE_EQ(0, cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID,
	    -1, sizeof(*mask), mask));
}

static void
jail_attach_newbase_epi(struct jail_test_cb_params *cbp)
{
	struct jail_test_info *info;
	cpuset_t *mask;

	info = &cbp->info;
	mask = &cbp->mask;

	/*
	 * The rootid test has been thrown in because a bug was discovered
	 * where any newly derived cpuset during attach would be parented to
	 * the wrong cpuset.  Otherwise, we should observe that a new cpuset
	 * has been created for this process.
	 */
	ATF_REQUIRE(info->jail_cpuset != cbp->rootid);
	ATF_REQUIRE(info->jail_cpuset != cbp->setid);
	ATF_REQUIRE(info->jail_cpuset != info->jail_child_cpuset);
	ATF_REQUIRE_EQ(0, CPU_CMP(mask, &info->jail_tidmask));
}

ATF_TC(jail_attach_newbase);
ATF_TC_HEAD(jail_attach_newbase, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test jail attachment effect on affinity with a new base cpuset.");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(jail_attach_newbase, tc)
{

	/* Need >= 2 cpus to test restriction. */
	do_jail_test(2, true, &jail_attach_mutate_pro,
	    &jail_attach_newbase_epi);
}

ATF_TC(jail_attach_newbase_plain);
ATF_TC_HEAD(jail_attach_newbase_plain, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test jail attachment effect on affinity with a new, unmodified base cpuset.");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(jail_attach_newbase_plain, tc)
{

	do_jail_test(2, true, NULL, &jail_attach_newbase_epi);
}

/*
 * Generic epilogue for tests that are expecting to use the jail's root cpuset
 * with their own mask, whether that's been modified or not.
 */
static void
jail_attach_jset_epi(struct jail_test_cb_params *cbp)
{
	struct jail_test_info *info;
	cpuset_t *mask;

	info = &cbp->info;
	mask = &cbp->mask;

	ATF_REQUIRE(info->jail_cpuset != cbp->setid);
	ATF_REQUIRE_EQ(info->jail_cpuset, info->jail_child_cpuset);
	ATF_REQUIRE_EQ(0, CPU_CMP(mask, &info->jail_tidmask));
}

ATF_TC(jail_attach_prevbase);
ATF_TC_HEAD(jail_attach_prevbase, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test jail attachment effect on affinity without a new base.");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(jail_attach_prevbase, tc)
{

	do_jail_test(2, false, &jail_attach_mutate_pro, &jail_attach_jset_epi);
}

static void
jail_attach_plain_pro(struct jail_test_cb_params *cbp)
{

	if (cbp->setid != cbp->rootid)
		atf_tc_skip("Must be running with the root cpuset.");
}

ATF_TC(jail_attach_plain);
ATF_TC_HEAD(jail_attach_plain, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test jail attachment effect on affinity without specialization.");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(jail_attach_plain, tc)
{

	do_jail_test(1, false, &jail_attach_plain_pro, &jail_attach_jset_epi);
}

static int
jail_attach_disjoint_newjail(int fd)
{
	struct iovec iov[2];
	char *name;
	int jid;

	if (asprintf(&name, "cpuset_%d", getpid()) == -1)
		_exit(42);

	iov[0].iov_base = "name";
	iov[0].iov_len = sizeof("name");

	iov[1].iov_base = name;
	iov[1].iov_len = strlen(name) + 1;

	if ((jid = jail_set(iov, 2, JAIL_CREATE | JAIL_ATTACH)) < 0)
		return (FAILURE_JAIL);

	/* Signal that we're ready. */
	write(fd, &jid, sizeof(jid));
	for (;;) {
		/* Spin */
	}
}

static int
wait_jail(int fd, int pfd)
{
	fd_set lset;
	struct timeval tv;
	int error, jid, maxfd;

	FD_ZERO(&lset);
	FD_SET(fd, &lset);
	FD_SET(pfd, &lset);

	maxfd = MAX(fd, pfd);

	tv.tv_sec = 5;
	tv.tv_usec = 0;

	/* Wait for jid to be written. */
	do {
		error = select(maxfd + 1, &lset, NULL, NULL, &tv);
	} while (error == -1 && errno == EINTR);

	if (error == 0) {
		atf_tc_fail("Jail creator did not respond in time.");
	}

	ATF_REQUIRE_MSG(error > 0, "Unexpected error %d from select()", errno);

	if (FD_ISSET(pfd, &lset)) {
		/* Process died */
		atf_tc_fail("Jail creator died unexpectedly.");
	}

	ATF_REQUIRE(FD_ISSET(fd, &lset));
	ATF_REQUIRE_EQ(sizeof(jid), recv(fd, &jid, sizeof(jid), 0));

	return (jid);
}

static int
try_attach_child(int jid, cpuset_t *expected_mask)
{
	cpuset_t mask;

	if (jail_attach(jid) == -1) {
		if (errno == EDEADLK)
			return (FAILURE_DEADLK);
		return (FAILURE_ATTACH);
	}

	if (expected_mask == NULL)
		return (FAILURE_SUCCESS);

	/* If we had an expected mask, check it against the new process mask. */
	CPU_ZERO(&mask);
	if (cpuset_getaffinity(CPU_LEVEL_CPUSET, CPU_WHICH_PID,
	    -1, sizeof(mask), &mask) != 0) {
		return (FAILURE_MASK);
	}

	if (CPU_CMP(expected_mask, &mask) != 0)
		return (FAILURE_BADAFFIN);

	return (0);
}

static void
try_attach(int jid, cpuset_t *expected_mask)
{
	const char *errstr;
	pid_t pid;
	int error, fail, status;

	ATF_REQUIRE(expected_mask != NULL);
	ATF_REQUIRE((pid = fork()) != -1);
	if (pid == 0)
		_exit(try_attach_child(jid, expected_mask));

	while ((error = waitpid(pid, &status, 0)) == -1 && errno == EINTR) {
		/* Try again. */
	}

	/* Sanity check the exit info. */
	ATF_REQUIRE_EQ(pid, error);
	ATF_REQUIRE(WIFEXITED(status));
	if ((fail = WEXITSTATUS(status)) != 0) {
		errstr = do_jail_errstr(fail);
		if (errstr != NULL)
			atf_tc_fail("%s", errstr);
		else
			atf_tc_fail("Unknown error '%d'", WEXITSTATUS(status));
	}
}

ATF_TC(jail_attach_disjoint);
ATF_TC_HEAD(jail_attach_disjoint, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test root attachment into completely disjoint jail cpuset.");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(jail_attach_disjoint, tc)
{
	cpuset_t smask, jmask;
	int sockpair[2];
	cpusetid_t setid;
	pid_t pid;
	int fcpu, jid, pfd, sock, scpu;

	ATF_REQUIRE_EQ(0, cpuset(&setid));

	skip_ltncpu(2, &jmask);
	fcpu = CPU_FFS(&jmask) - 1;
	ATF_REQUIRE_EQ(0, socketpair(PF_UNIX, SOCK_STREAM, 0, sockpair));

	/* We'll wait on the procdesc, too, so we can fail faster if it dies. */
	ATF_REQUIRE((pid = pdfork(&pfd, 0)) != -1);

	if (pid == 0) {
		/* First child sets up the jail. */
		sock = sockpair[SP_CHILD];
		close(sockpair[SP_PARENT]);

		_exit(jail_attach_disjoint_newjail(sock));
	}

	close(sockpair[SP_CHILD]);
	sock = sockpair[SP_PARENT];

	ATF_REQUIRE((jid = wait_jail(sock, pfd)) > 0);

	/*
	 * This process will be clamped down to the first cpu, while the jail
	 * will simply have the first CPU removed to make it a completely
	 * disjoint operation.
	 */
	CPU_ZERO(&smask);
	CPU_SET(fcpu, &smask);
	CPU_CLR(fcpu, &jmask);

	/*
	 * We'll test with the first and second cpu set as well.  Only the
	 * second cpu should be used.
	 */
	scpu = CPU_FFS(&jmask) - 1;

	ATF_REQUIRE_EQ(0, cpuset_setaffinity(CPU_LEVEL_ROOT, CPU_WHICH_JAIL,
	    jid, sizeof(jmask), &jmask));
	ATF_REQUIRE_EQ(0, cpuset_setaffinity(CPU_LEVEL_CPUSET, CPU_WHICH_CPUSET,
	    setid, sizeof(smask), &smask));

	try_attach(jid, &jmask);

	CPU_SET(scpu, &smask);
	ATF_REQUIRE_EQ(0, cpuset_setaffinity(CPU_LEVEL_CPUSET, CPU_WHICH_CPUSET,
	    setid, sizeof(smask), &smask));

	CPU_CLR(fcpu, &smask);
	try_attach(jid, &smask);
}

ATF_TC(badparent);
ATF_TC_HEAD(badparent, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test parent assignment when assigning a new cpuset.");
}
ATF_TC_BODY(badparent, tc)
{
	cpuset_t mask;
	cpusetid_t finalsetid, origsetid, setid;

	/* Need to mask off at least one CPU. */
	skip_ltncpu(2, &mask);

	ATF_REQUIRE_EQ(0, cpuset_getid(CPU_LEVEL_CPUSET, CPU_WHICH_TID, -1,
	    &origsetid));

	ATF_REQUIRE_EQ(0, cpuset(&setid));

	/*
	 * Mask off the first CPU, then we'll reparent ourselves to our original
	 * set.
	 */
	CPU_CLR(CPU_FFS(&mask) - 1, &mask);
	ATF_REQUIRE_EQ(0, cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID,
	    -1, sizeof(mask), &mask));

	ATF_REQUIRE_EQ(0, cpuset_setid(CPU_WHICH_PID, -1, origsetid));
	ATF_REQUIRE_EQ(0, cpuset_getid(CPU_LEVEL_CPUSET, CPU_WHICH_TID, -1,
	    &finalsetid));

	ATF_REQUIRE_EQ(finalsetid, origsetid);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, newset);
	ATF_TP_ADD_TC(tp, transient);
	ATF_TP_ADD_TC(tp, deadlk);
	ATF_TP_ADD_TC(tp, jail_attach_newbase);
	ATF_TP_ADD_TC(tp, jail_attach_newbase_plain);
	ATF_TP_ADD_TC(tp, jail_attach_prevbase);
	ATF_TP_ADD_TC(tp, jail_attach_plain);
	ATF_TP_ADD_TC(tp, jail_attach_disjoint);
	ATF_TP_ADD_TC(tp, badparent);
	return (atf_no_error());
}
