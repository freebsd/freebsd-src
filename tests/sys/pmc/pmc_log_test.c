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
 * Tests for the log file a sampling PMC writes through: which descriptors
 * PMC_OP_CONFIGURELOG accepts, when a log is required at all, and what the
 * descriptor-less log operations do without one.
 *
 * Two properties are asserted because they are easy to get backwards.  A
 * sampling PMC does not as such need a log: PMC_F_NEEDS_LOGFILE covers
 * only one attached to a process other than its owner, and system-mode
 * PMCs, so a PMC that samples its owner starts with none.  And the
 * descriptor need not be a regular file - a socket is accepted, and has to
 * be: that is the shape pmcstat(8) uses for a pipe or a network peer.
 *
 * Log ownership is per process and sticky, a second PMC_OP_CONFIGURELOG
 * being refused with EBUSY, so no case may configure a log the next one
 * depends on; ATF's per-case process supplies that isolation, and the case
 * needing a second fresh owner forks for it.
 *
 * The sampling cases skip where there is no hardware PMC, so the skip
 * count is part of the result.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <pmc.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

/**
 * @internal
 * Sampling needs a real counter, and which one it is does not matter, so try
 * the usual spellings across vendors until one is accepted.
 */
static const char *const hw_events[] = {
	"instructions", "inst_retired.any", "unhalted-core-cycles",
	"cycles", "cpu_clk_unhalted.thread", NULL
};

#define	SAMPLE_RATE	10000

static void
require_hwpmc(void)
{

	if (pmc_init() != 0)
		atf_tc_skip("hwpmc(4) is not available");
}

static int
alloc_sampling(pmc_id_t *idp, uint32_t flags)
{
	int k;

	for (k = 0; hw_events[k] != NULL; k++) {
		if (pmc_allocate(hw_events[k], PMC_MODE_TS, flags, PMC_CPU_ANY,
		    idp, SAMPLE_RATE) == 0)
			return (0);
	}
	return (-1);
}

static pmc_id_t
require_sampling_pmc(uint32_t flags)
{
	pmc_id_t id;

	if (alloc_sampling(&id, flags) != 0)
		atf_tc_skip("no hardware sampling PMC is allocatable: %s",
		    strerror(errno));
	return (id);
}

/** @internal A writable regular file in the case's own work directory. */
static int
work_file(const char *name)
{
	int fd;

	fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0600);
	ATF_REQUIRE_MSG(fd >= 0, "open %s: %s", name, strerror(errno));
	return (fd);
}

/**
 * @internal
 * A sampling PMC whose only target is its owner needs no log: pmc_start()
 * attaches the owner implicitly when the target list is empty, and that is
 * not the case PMC_F_NEEDS_LOGFILE covers.
 */
ATF_TC_WITHOUT_HEAD(sampling_owner_needs_no_log);
ATF_TC_BODY(sampling_owner_needs_no_log, tc)
{
	pmc_id_t id;

	require_hwpmc();
	id = require_sampling_pmc(0);

	ATF_CHECK_MSG(pmc_start(id) == 0,
	    "a sampling PMC targeting its owner was refused a start without "
	    "a log: %s", strerror(errno));
	ATF_CHECK_MSG(pmc_stop(id) == 0, "pmc_stop: %s", strerror(errno));
	ATF_CHECK_MSG(pmc_release(id) == 0, "pmc_release: %s", strerror(errno));
}

/**
 * @internal
 * The other half of the same rule: once the target is a different process,
 * the samples have nowhere to go without a log and PMCSTART refuses.
 */
ATF_TC_WITHOUT_HEAD(sampling_foreign_target_needs_a_log);
ATF_TC_BODY(sampling_foreign_target_needs_a_log, tc)
{
	pmc_id_t id;
	pid_t child;
	int status, rc;

	require_hwpmc();
	id = require_sampling_pmc(0);

	ATF_REQUIRE((child = fork()) >= 0);
	if (child == 0) {
		/* Stay alive long enough to be a target. */
		(void)sleep(5);
		_exit(0);
	}

	rc = pmc_attach(id, child);
	if (rc != 0) {
		(void)kill(child, SIGKILL);
		(void)waitpid(child, &status, 0);
		(void)pmc_release(id);
		atf_tc_skip("pmc_attach to a child failed: %s",
		    strerror(errno));
	}

	errno = 0;
	ATF_CHECK_MSG(pmc_start(id) != 0,
	    "a sampling PMC attached to another process started with no log "
	    "configured");
	ATF_CHECK_MSG(errno == EDOOFUS, "pmc_start: expected EDOOFUS, got %s",
	    strerror(errno));

	ATF_CHECK_MSG(pmc_detach(id, child) == 0, "pmc_detach: %s",
	    strerror(errno));
	(void)kill(child, SIGKILL);
	(void)waitpid(child, &status, 0);
	ATF_CHECK_MSG(pmc_release(id) == 0, "pmc_release: %s", strerror(errno));
}

/**
 * @internal
 * Descriptors the kernel must refuse.  All three fail, so none of them
 * leaves a log configured and one process can carry the whole case.
 */
ATF_TC_WITHOUT_HEAD(configurelog_refuses_unwritable_fd);
ATF_TC_BODY(configurelog_refuses_unwritable_fd, tc)
{
	int fd;

	require_hwpmc();

	/* Closed before the call. */
	fd = work_file("closed.pmclog");
	ATF_REQUIRE(close(fd) == 0);
	errno = 0;
	ATF_CHECK_MSG(pmc_configure_logfile(fd) != 0,
	    "a closed descriptor was accepted as a log");
	ATF_CHECK_MSG(errno == EBADF, "closed fd: expected EBADF, got %s",
	    strerror(errno));

	/* Open, but not for writing: fget_write() has to refuse it. */
	fd = work_file("ro.pmclog");
	ATF_REQUIRE(close(fd) == 0);
	fd = open("ro.pmclog", O_RDONLY);
	ATF_REQUIRE_MSG(fd >= 0, "reopen read-only: %s", strerror(errno));
	errno = 0;
	ATF_CHECK_MSG(pmc_configure_logfile(fd) != 0,
	    "a read-only descriptor was accepted as a log");
	ATF_CHECK_MSG(errno == EBADF, "read-only fd: expected EBADF, got %s",
	    strerror(errno));
	(void)close(fd);

	/* A directory, which can only ever be open read-only. */
	fd = open(".", O_RDONLY);
	ATF_REQUIRE_MSG(fd >= 0, "open .: %s", strerror(errno));
	errno = 0;
	ATF_CHECK_MSG(pmc_configure_logfile(fd) != 0,
	    "a directory was accepted as a log");
	ATF_CHECK_MSG(errno == EBADF, "directory fd: expected EBADF, got %s",
	    strerror(errno));
	(void)close(fd);

	/*
	 * A negative descriptor means "deconfigure".  With nothing
	 * configured there is nothing to deconfigure, and the op refuses.
	 */
	errno = 0;
	ATF_CHECK_MSG(pmc_configure_logfile(-1) != 0,
	    "deconfiguring succeeded with no log configured");
	ATF_CHECK_MSG(errno == EINVAL, "fd -1: expected EINVAL, got %s",
	    strerror(errno));
}

/**
 * @internal
 * A socket is a legitimate log destination - pmcstat(8) logs to one when
 * told to pipe - so the fd check must not be narrowed to regular files.
 */
ATF_TC_WITHOUT_HEAD(configurelog_accepts_a_socket);
ATF_TC_BODY(configurelog_accepts_a_socket, tc)
{
	int sv[2];

	require_hwpmc();

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
		atf_tc_skip("socketpair: %s", strerror(errno));

	ATF_CHECK_MSG(pmc_configure_logfile(sv[0]) == 0,
	    "a socket was refused as a log destination: %s", strerror(errno));

	(void)pmc_close_logfile();
	(void)close(sv[0]);
	(void)close(sv[1]);
}

/**
 * @internal
 * One log per owner: the second configure is refused while the first holds.
 */
ATF_TC_WITHOUT_HEAD(configurelog_twice_is_refused);
ATF_TC_BODY(configurelog_twice_is_refused, tc)
{
	int fd1, fd2;

	require_hwpmc();
	fd1 = work_file("first.pmclog");
	fd2 = work_file("second.pmclog");

	ATF_REQUIRE_MSG(pmc_configure_logfile(fd1) == 0,
	    "pmc_configure_logfile: %s", strerror(errno));

	errno = 0;
	ATF_CHECK_MSG(pmc_configure_logfile(fd2) != 0,
	    "a second log was configured over a live one");
	ATF_CHECK_MSG(errno == EBUSY, "second configure: expected EBUSY, "
	    "got %s", strerror(errno));

	(void)pmc_close_logfile();
	(void)close(fd1);
	(void)close(fd2);
}

/**
 * @internal
 * The log operations take no descriptor and have to find the caller's owner
 * record themselves.  A process that has never allocated a PMC has none.
 */
ATF_TC_WITHOUT_HEAD(log_ops_without_an_owner);
ATF_TC_BODY(log_ops_without_an_owner, tc)
{

	require_hwpmc();

	errno = 0;
	ATF_CHECK_MSG(pmc_flush_logfile() != 0,
	    "pmc_flush_logfile succeeded with no owner record");
	ATF_CHECK_MSG(errno == EINVAL, "flush: expected EINVAL, got %s",
	    strerror(errno));

	errno = 0;
	ATF_CHECK_MSG(pmc_close_logfile() != 0,
	    "pmc_close_logfile succeeded with no owner record");
	ATF_CHECK_MSG(errno == EINVAL, "close: expected EINVAL, got %s",
	    strerror(errno));

	errno = 0;
	ATF_CHECK_MSG(pmc_writelog(0x5a5a5a5a) != 0,
	    "pmc_writelog succeeded with no owner record");
	ATF_CHECK_MSG(errno == EINVAL, "writelog: expected EINVAL, got %s",
	    strerror(errno));
}

/**
 * @internal
 * The kernel takes its own reference on the log file, so userland closing
 * its descriptor must not break logging or fault anything.
 */
ATF_TC_WITHOUT_HEAD(log_survives_userland_closing_the_fd);
ATF_TC_BODY(log_survives_userland_closing_the_fd, tc)
{
	int fd;

	require_hwpmc();
	fd = work_file("behind.pmclog");

	ATF_REQUIRE_MSG(pmc_configure_logfile(fd) == 0,
	    "pmc_configure_logfile: %s", strerror(errno));
	ATF_REQUIRE_MSG(close(fd) == 0, "close: %s", strerror(errno));

	ATF_CHECK_MSG(pmc_writelog(0xdeadbeef) == 0,
	    "writelog after the fd was closed in userland: %s",
	    strerror(errno));
	ATF_CHECK_MSG(pmc_flush_logfile() == 0,
	    "flush after the fd was closed in userland: %s", strerror(errno));
	ATF_CHECK_MSG(pmc_close_logfile() == 0,
	    "close after the fd was closed in userland: %s", strerror(errno));
}

/**
 * @internal
 * A write error on the log has to reach userland rather than be swallowed.
 * /dev/full accepts the configure and fails every write with ENOSPC, which
 * the flush - which waits for the buffers to be written - reports.
 */
ATF_TC_WITHOUT_HEAD(log_write_error_is_reported);
ATF_TC_BODY(log_write_error_is_reported, tc)
{
	int fd;

	require_hwpmc();

	if ((fd = open("/dev/full", O_WRONLY)) < 0)
		atf_tc_skip("/dev/full: %s", strerror(errno));

	ATF_REQUIRE_MSG(pmc_configure_logfile(fd) == 0,
	    "/dev/full was refused as a log: %s", strerror(errno));

	ATF_REQUIRE_MSG(pmc_writelog(0x1234) == 0, "pmc_writelog: %s",
	    strerror(errno));

	errno = 0;
	ATF_CHECK_MSG(pmc_flush_logfile() != 0,
	    "flushing a log on a full device reported success");
	ATF_CHECK_MSG(errno == ENOSPC, "flush: expected ENOSPC, got %s",
	    strerror(errno));

	(void)close(fd);
}

/**
 * @internal
 * Process exit is a teardown path of its own: the owner leaves with the PMC
 * running and the log still configured, and the kernel has to unwind both.
 * The child does no cleanup on purpose.  A wedge or a fault here shows up as
 * a signal or a timeout rather than a failed assertion, so the case also
 * confirms hwpmc is still usable afterwards.
 */
ATF_TC_WITHOUT_HEAD(owner_exit_with_log_and_running_pmc);
ATF_TC_BODY(owner_exit_with_log_and_running_pmc, tc)
{
	pmc_id_t id;
	pid_t child;
	int status;

	require_hwpmc();

	/* Establish up front that this machine can sample at all. */
	id = require_sampling_pmc(PMC_F_CALLCHAIN);
	ATF_REQUIRE(pmc_release(id) == 0);

	ATF_REQUIRE((child = fork()) >= 0);
	if (child == 0) {
		volatile unsigned long sink = 0;
		unsigned long i;
		pmc_id_t cid;
		int fd;

		if (pmc_init() != 0)
			_exit(10);
		if ((fd = open("owner-exit.pmclog",
		    O_RDWR | O_CREAT | O_TRUNC, 0600)) < 0)
			_exit(11);
		if (pmc_configure_logfile(fd) != 0)
			_exit(12);
		if (alloc_sampling(&cid, PMC_F_CALLCHAIN) != 0)
			_exit(13);
		if (pmc_start(cid) != 0)
			_exit(14);
		for (i = 0; i < 20000000; i++)
			sink += i;
		/* No stop, no release, no close: exit is the teardown. */
		_exit(0);
	}

	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	ATF_REQUIRE_MSG(!WIFSIGNALED(status),
	    "the owner died on signal %d while exiting with a live PMC and a "
	    "configured log", WIFSIGNALED(status) ? WTERMSIG(status) : 0);
	ATF_REQUIRE_MSG(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	    "the child could not set up the case (exit %d)",
	    WIFEXITED(status) ? WEXITSTATUS(status) : -1);

	/* The owner's teardown must not have left the driver unusable. */
	ATF_CHECK_MSG(alloc_sampling(&id, 0) == 0,
	    "no sampling PMC could be allocated after an owner exited with "
	    "one running: %s", strerror(errno));
	if (id != PMC_ID_INVALID)
		ATF_CHECK(pmc_release(id) == 0);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, sampling_owner_needs_no_log);
	ATF_TP_ADD_TC(tp, sampling_foreign_target_needs_a_log);
	ATF_TP_ADD_TC(tp, configurelog_refuses_unwritable_fd);
	ATF_TP_ADD_TC(tp, configurelog_accepts_a_socket);
	ATF_TP_ADD_TC(tp, configurelog_twice_is_refused);
	ATF_TP_ADD_TC(tp, log_ops_without_an_owner);
	ATF_TP_ADD_TC(tp, log_survives_userland_closing_the_fd);
	ATF_TP_ADD_TC(tp, log_write_error_is_reported);
	ATF_TP_ADD_TC(tp, owner_exit_with_log_and_running_pmc);

	return (atf_no_error());
}
