/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Iván Ezequiel Rodriguez <ivanrwcm25@gmail.com>
 */

#include <sys/ioctl.h>
#include <sys/linker.h>
#include <sys/module.h>

#include <dev/ntsync/ntsync.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static void
require_ntsync(void)
{

	if (modfind("ntsync") == -1) {
		if (kldload("ntsync") != 0)
			atf_tc_skip("ntsync module unavailable: %s",
			    strerror(errno));
	}
	if (access("/dev/ntsync", F_OK) != 0)
		atf_tc_skip("/dev/ntsync is not present");
}

static int
ntsync_open_domain(void)
{
	int fd;

	fd = open("/dev/ntsync", O_RDWR);
	if (fd < 0)
		atf_tc_skip("open(/dev/ntsync) failed: %s", strerror(errno));
	return (fd);
}

static int
ntsync_create_sem(int domain, uint32_t count, uint32_t max)
{
	struct ntsync_sem_args args = {
		.count = count,
		.max = max,
	};
	int semfd;

	semfd = ioctl(domain, NTSYNC_IOC_CREATE_SEM, &args);
	ATF_REQUIRE_MSG(semfd >= 0, "NTSYNC_IOC_CREATE_SEM: %s",
	    strerror(errno));
	return (semfd);
}

ATF_TC_WITHOUT_HEAD(open_domain);
ATF_TC_BODY(open_domain, tc)
{
	int domain;

	require_ntsync();
	domain = ntsync_open_domain();
	ATF_REQUIRE(close(domain) == 0);
}

ATF_TC_WITHOUT_HEAD(sem_create_read);
ATF_TC_BODY(sem_create_read, tc)
{
	struct ntsync_sem_args args;
	int domain, semfd;

	require_ntsync();
	domain = ntsync_open_domain();
	semfd = ntsync_create_sem(domain, 2, 5);

	memset(&args, 0, sizeof(args));
	ATF_REQUIRE(ioctl(semfd, NTSYNC_IOC_SEM_READ, &args) == 0);
	ATF_REQUIRE_EQ(2U, args.count);
	ATF_REQUIRE_EQ(5U, args.max);

	ATF_REQUIRE(close(semfd) == 0);
	ATF_REQUIRE(close(domain) == 0);
}

ATF_TC_WITHOUT_HEAD(sem_wait_any);
ATF_TC_BODY(sem_wait_any, tc)
{
	struct ntsync_sem_args args;
	struct ntsync_wait_args wa;
	int domain, objs[1], semfd;

	require_ntsync();
	domain = ntsync_open_domain();
	semfd = ntsync_create_sem(domain, 1, 1);

	objs[0] = semfd;
	memset(&wa, 0, sizeof(wa));
	wa.objs = (uint64_t)(uintptr_t)objs;
	wa.count = 1;
	wa.owner = getpid();

	ATF_REQUIRE(ioctl(domain, NTSYNC_IOC_WAIT_ANY, &wa) == 0);
	ATF_REQUIRE_EQ(0U, wa.index);

	memset(&args, 0, sizeof(args));
	ATF_REQUIRE(ioctl(semfd, NTSYNC_IOC_SEM_READ, &args) == 0);
	ATF_REQUIRE_EQ(0U, args.count);
	ATF_REQUIRE_EQ(1U, args.max);

	ATF_REQUIRE(close(semfd) == 0);
	ATF_REQUIRE(close(domain) == 0);
}

ATF_TC_WITHOUT_HEAD(event_set_read);
ATF_TC_BODY(event_set_read, tc)
{
	struct ntsync_event_args args;
	uint32_t prev;
	int domain, evtfd;

	require_ntsync();
	domain = ntsync_open_domain();

	memset(&args, 0, sizeof(args));
	args.manual = 1;
	args.signaled = 0;
	evtfd = ioctl(domain, NTSYNC_IOC_CREATE_EVENT, &args);
	ATF_REQUIRE_MSG(evtfd >= 0, "NTSYNC_IOC_CREATE_EVENT: %s",
	    strerror(errno));

	ATF_REQUIRE(ioctl(evtfd, NTSYNC_IOC_EVENT_SET, &prev) == 0);
	ATF_REQUIRE_EQ(0U, prev);

	memset(&args, 0, sizeof(args));
	ATF_REQUIRE(ioctl(evtfd, NTSYNC_IOC_EVENT_READ, &args) == 0);
	ATF_REQUIRE_EQ(1U, args.manual);
	ATF_REQUIRE_EQ(1U, args.signaled);

	ATF_REQUIRE(close(evtfd) == 0);
	ATF_REQUIRE(close(domain) == 0);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, open_domain);
	ATF_TP_ADD_TC(tp, sem_create_read);
	ATF_TP_ADD_TC(tp, sem_wait_any);
	ATF_TP_ADD_TC(tp, event_set_read);
	return (atf_no_error());
}
