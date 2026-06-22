/*
 * Copyright (c) 2026 Mark Johnston <markj@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/user.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <kvm.h>

/*
 * Block until a thread in the specified process is sleeping in the specified
 * wait message.
 */
static void
wait_for_naptime(pid_t pid, const char *wmesg)
{
	kvm_t *kd;
	int count;

	kd = kvm_openfiles(NULL, "/dev/null", NULL, O_RDONLY, NULL);
	ATF_REQUIRE(kd != NULL);
	for (;;) {
		struct kinfo_proc *kip;
		int i;

		usleep(1000);
		kip = kvm_getprocs(kd, KERN_PROC_PID | KERN_PROC_INC_THREAD,
		    pid, &count);
		ATF_REQUIRE(kip != NULL);
		for (i = 0; i < count; i++) {
			ATF_REQUIRE(kip[i].ki_stat != SZOMB);
			if (kip[i].ki_stat == SSLEEP &&
			    strcmp(kip[i].ki_wmesg, wmesg) == 0)
				break;
		}
		if (i < count)
			break;
	}

	kvm_close(kd);
}

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

	wait_for_naptime(getpid(), "select");

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

	wait_for_naptime(getpid(), "select");

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

	wait_for_naptime(getpid(), "select");

	ATF_REQUIRE_MSG(close(jd) == 0, "close: %s", strerror(errno));

	error = pthread_join(thr, (void *)&revents);
	ATF_REQUIRE_MSG(error == 0, "pthread_join: %s", strerror(error));
	ATF_REQUIRE_EQ(revents, POLLNVAL);

	ATF_REQUIRE_MSG(close(owning_jd) == 0, "close: %s", strerror(errno));
}

/*
 * Verify that a process inside a jail cannot obtain a jail descriptor for
 * its own jail.
 */
ATF_TC(curjail_get);
ATF_TC_HEAD(curjail_get, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(curjail_get, tc)
{
	char namebuf[MAXHOSTNAMELEN];
	struct iovec iov[4];
	int desc, error, jid, n;

	(void)create_jail("jaildesc_get_desc_current_jail");

	strlcpy(namebuf, "jaildesc_get_desc_current_jail", sizeof(namebuf));
	jid = -1;
	n = 0;
	iov[n].iov_base = __DECONST(void *, "name");
	iov[n++].iov_len = sizeof("name");
	iov[n].iov_base = namebuf;
	iov[n++].iov_len = strlen(namebuf) + 1;
	jid = jail_get(iov, n, 0);
	ATF_REQUIRE_MSG(jid >= 0, "jail_get: %s", strerror(errno));

	error = jail_attach(jid);
	ATF_REQUIRE_MSG(error == 0, "jail_attach: %s", strerror(errno));

	/*
	 * Now that we are inside the jail, verify that we cannot obtain a
	 * descriptor for it.
	 */
	strlcpy(namebuf, "jaildesc_get_desc_current_jail", sizeof(namebuf));
	desc = -1;
	n = 0;
	iov[n].iov_base = __DECONST(void *, "name");
	iov[n++].iov_len = sizeof("name");
	iov[n].iov_base = namebuf;
	iov[n++].iov_len = strlen(namebuf) + 1;
	iov[n].iov_base = __DECONST(void *, "desc");
	iov[n++].iov_len = sizeof("desc");
	iov[n].iov_base = &desc;
	iov[n++].iov_len = sizeof(desc);
	ATF_REQUIRE_MSG(jail_get(iov, n, JAIL_GET_DESC) == -1,
	    "jail_get succeeded unexpectedly");
	ATF_REQUIRE_MSG(jail_get(iov, n, JAIL_OWN_DESC) == -1,
	    "jail_get succeeded unexpectedly");
}

static void
nop(int signum __unused)
{
}

/*
 * Close an owning descriptor while in the corresponding jail.
 */
ATF_TC(self_destruct);
ATF_TC_HEAD(self_destruct, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(self_destruct, tc)
{
	int error, jd, status;
	pid_t pid;

	jd = create_jail("jaildesc_self_destruct");

	pid = fork();
	ATF_REQUIRE_MSG(pid >= 0, "fork: %s", strerror(errno));
	if (pid == 0) {
		struct sigaction sa;

		error = jail_attach_jd(jd);
		if (error != 0)
			_exit(1);

		sa.sa_handler = nop;
		sa.sa_flags = 0;
		sigemptyset(&sa.sa_mask);
		error = sigaction(SIGALRM, &sa, NULL);
		if (error != 0)
			_exit(2);

		pause();
		close(jd);
		/* NOTREACHED? */
		_exit(3);
	}

	wait_for_naptime(pid, "sigsusp");

	error = close(jd);
	ATF_REQUIRE_MSG(error == 0, "close: %s", strerror(errno));

	error = kill(pid, SIGALRM);
	ATF_REQUIRE_MSG(error == 0, "kill: %s", strerror(errno));

	pid = waitpid(pid, &status, 0);
	ATF_REQUIRE_MSG(pid >= 0, "waitpid: %s", strerror(errno));
	ATF_REQUIRE(WIFSIGNALED(status));
	ATF_REQUIRE_EQ(WTERMSIG(status), SIGKILL);
}

/*
 * Try to get an fd for a non-existent jail.
 */
ATF_TC(at_desc_bad_fd);
ATF_TC_HEAD(at_desc_bad_fd, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(at_desc_bad_fd, tc)
{
	int jd, status;
	pid_t pid;

	jd = create_jail("jaildesc_at_desc_bad_fd");

	pid = fork();
	ATF_REQUIRE_MSG(pid >= 0, "fork: %s", strerror(errno));
	if (pid == 0) {
		struct iovec iov[4];
		char namebuf[MAXHOSTNAMELEN];
		int desc, i, n;

		if (jail_attach_jd(jd) != 0)
			_exit(1);

		/* Regression test: loop here to trigger a refcount leak. */
		for (i = 0; i < 100; i++) {
			strlcpy(namebuf, "nonexistent", sizeof(namebuf));
			desc = STDIN_FILENO;
			n = 0;
			iov[n].iov_base = __DECONST(void *, "name");
			iov[n++].iov_len = strlen("name") + 1;
			iov[n].iov_base = namebuf;
			iov[n++].iov_len = sizeof(namebuf);
			iov[n].iov_base = __DECONST(void *, "desc");
			iov[n++].iov_len = strlen("desc") + 1;
			iov[n].iov_base = &desc;
			iov[n++].iov_len = sizeof(desc);
			if (jail_get(iov, n, JAIL_AT_DESC) != -1)
				_exit(2);
			if (errno != EINVAL)
				_exit(3);
		}
		_exit(0);
	}

	pid = waitpid(pid, &status, 0);
	ATF_REQUIRE_MSG(pid >= 0, "waitpid: %s", strerror(errno));
	ATF_REQUIRE_MSG(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	    "child failed with status %d", status);

	ATF_REQUIRE_MSG(close(jd) == 0, "close: %s", strerror(errno));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, poll_close_race);
	ATF_TP_ADD_TC(tp, poll_remove_wakeup);
	ATF_TP_ADD_TC(tp, poll_close_race_get_desc);
	ATF_TP_ADD_TC(tp, curjail_get);
	ATF_TP_ADD_TC(tp, self_destruct);
	ATF_TP_ADD_TC(tp, at_desc_bad_fd);

	return (atf_no_error());
}
