/*
 * Copyright (c) 2026 Mark Johnston <markj@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/uio.h>

#include <atf-c.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>

/*
 * Create a persistent jail and return an owning descriptor for it.
 * The jail is removed when the returned descriptor is closed.
 */
static int
create_jail(const char *name)
{
	struct iovec iov[8];
	int desc, jid, n;

	desc = -1;
	n = 0;
	iov[n].iov_base = __DECONST(void *, "name");
	iov[n++].iov_len = strlen("name") + 1;
	iov[n].iov_base = __DECONST(void *, name);
	iov[n++].iov_len = strlen(name) + 1;
	iov[n].iov_base = __DECONST(void *, "path");
	iov[n++].iov_len = strlen("path") + 1;
	iov[n].iov_base = __DECONST(void *, "/");
	iov[n++].iov_len = strlen("/") + 1;
	iov[n].iov_base = __DECONST(void *, "persist");
	iov[n++].iov_len = strlen("persist") + 1;
	iov[n].iov_base = NULL;
	iov[n++].iov_len = 0;
	iov[n].iov_base = __DECONST(void *, "desc");
	iov[n++].iov_len = strlen("desc") + 1;
	iov[n].iov_base = &desc;
	iov[n++].iov_len = sizeof(desc);
	jid = jail_set(iov, n, JAIL_CREATE | JAIL_OWN_DESC);
	ATF_REQUIRE_MSG(jid >= 0, "jail_set: %s", strerror(errno));
	return (desc);
}

static void *
poll_jaildesc(void *arg)
{
	struct pollfd pfd;

	pfd.fd = *(int *)arg;
	pfd.events = POLLHUP;
	(void)poll(&pfd, 1, 5000);
	return ((void *)(uintptr_t)pfd.revents);
}

/*
 * Regression test for the case where a jail descriptor is closed while a
 * thread is blocking in poll(2) on it.
 */
ATF_TC(poll_close_race);
ATF_TC_HEAD(poll_close_race, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(poll_close_race, tc)
{
	pthread_t thr;
	uintptr_t revents;
	int error, jd;

	jd = create_jail("jaildesc_poll_close_race");

	error = pthread_create(&thr, NULL, poll_jaildesc, &jd);
	ATF_REQUIRE_MSG(error == 0, "pthread_create: %s", strerror(error));

	/* Wait for the thread to block in poll(2). */
	usleep(250000);

	ATF_REQUIRE_MSG(close(jd) == 0, "close: %s", strerror(errno));

	error = pthread_join(thr, (void *)&revents);
	ATF_REQUIRE_MSG(error == 0, "pthread_join: %s", strerror(error));
	ATF_REQUIRE_EQ(revents, POLLNVAL);
}

/*
 * Verify that poll(2) of a jail descriptor returns POLLHUP when the jail
 * is removed.
 */
ATF_TC(poll_remove_wakeup);
ATF_TC_HEAD(poll_remove_wakeup, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(poll_remove_wakeup, tc)
{
	pthread_t thr;
	uintptr_t revents;
	int error, jd;

	jd = create_jail("jaildesc_poll_remove_wakeup");

	error = pthread_create(&thr, NULL, poll_jaildesc, &jd);
	ATF_REQUIRE_MSG(error == 0, "pthread_create: %s", strerror(error));

	/* Wait for the thread to block in poll(2). */
	usleep(250000);

	ATF_REQUIRE_MSG(jail_remove_jd(jd) == 0,
	    "jail_remove_jd: %s", strerror(errno));

	error = pthread_join(thr, (void *)&revents);
	ATF_REQUIRE_MSG(error == 0, "pthread_join: %s", strerror(error));
	ATF_REQUIRE_EQ(revents, POLLHUP);

	ATF_REQUIRE_MSG(close(jd) == 0, "close: %s", strerror(errno));
}

static int
get_jaildesc(const char *name)
{
	struct iovec iov[4];
	char namebuf[MAXHOSTNAMELEN];
	int desc, jid, n;

	strlcpy(namebuf, name, sizeof(namebuf));
	desc = -1;
	n = 0;
	iov[n].iov_base = __DECONST(void *, "name");
	iov[n++].iov_len = strlen("name") + 1;
	iov[n].iov_base = namebuf;
	iov[n++].iov_len = sizeof(namebuf);
	iov[n].iov_base = __DECONST(void *, "desc");
	iov[n++].iov_len = strlen("desc") + 1;
	iov[n].iov_base = &desc;
	iov[n++].iov_len = sizeof(desc);
	jid = jail_get(iov, n, JAIL_GET_DESC);
	ATF_REQUIRE_MSG(jid >= 0, "jail_get: %s", strerror(errno));
	return (desc);
}

/*
 * Regression test for the same use-after-free as poll_close_race, but with a
 * non-owning JAIL_GET_DESC descriptor obtained without root privileges.
 */
ATF_TC(poll_close_race_get_desc);
ATF_TC_HEAD(poll_close_race_get_desc, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(poll_close_race_get_desc, tc)
{
	struct passwd *pw;
	pthread_t thr;
	uintptr_t revents;
	int error, jd, owning_jd;

	/* Create the jail as root; keep the owning descriptor for cleanup. */
	owning_jd = create_jail("jaildesc_poll_close_get_desc");

	/*
	 * Drop root privileges.  jail_get(2) with JAIL_GET_DESC does not
	 * require PRIV_JAIL_REMOVE, so a non-root process in the host prison
	 * can obtain a read-only descriptor for any visible jail.
	 */
	pw = getpwnam("nobody");
	ATF_REQUIRE_MSG(pw != NULL, "getpwnam: %s", strerror(errno));
	ATF_REQUIRE_MSG(setuid(pw->pw_uid) == 0, "setuid: %s", strerror(errno));

	jd = get_jaildesc("jaildesc_poll_close_get_desc");

	error = pthread_create(&thr, NULL, poll_jaildesc, &jd);
	ATF_REQUIRE_MSG(error == 0, "pthread_create: %s", strerror(error));

	/* Wait for the thread to block in poll(2). */
	usleep(250000);

	ATF_REQUIRE_MSG(close(jd) == 0, "close: %s", strerror(errno));

	error = pthread_join(thr, (void *)&revents);
	ATF_REQUIRE_MSG(error == 0, "pthread_join: %s", strerror(error));
	ATF_REQUIRE_EQ(revents, POLLNVAL);

	ATF_REQUIRE_MSG(close(owning_jd) == 0, "close: %s", strerror(errno));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, poll_close_race);
	ATF_TP_ADD_TC(tp, poll_remove_wakeup);
	ATF_TP_ADD_TC(tp, poll_close_race_get_desc);

	return (atf_no_error());
}
