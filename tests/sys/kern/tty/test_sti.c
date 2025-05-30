/*-
 * Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <termios.h>

#include <atf-c.h>
#include <libutil.h>

enum stierr {
	STIERR_CONFIG_FETCH,
	STIERR_CONFIG,
	STIERR_INJECT,
	STIERR_READFAIL,
	STIERR_BADTEXT,
	STIERR_DATAFOUND,
	STIERR_ROTTY,
	STIERR_WOTTY,
	STIERR_WOOK,
	STIERR_BADERR,

	STIERR_MAXERR
};

static const struct stierr_map {
	enum stierr	 stierr;
	const char	*msg;
} stierr_map[] = {
	{ STIERR_CONFIG_FETCH, "Failed to fetch ctty configuration" },
	{ STIERR_CONFIG, "Failed to configure ctty in the child" },
	{ STIERR_INJECT, "Failed to inject characters via TIOCSTI" },
	{ STIERR_READFAIL, "Failed to read(2) from stdin" },
	{ STIERR_BADTEXT, "read(2) data did not match injected data" },
	{ STIERR_DATAFOUND, "read(2) data when we did not expected to" },
	{ STIERR_ROTTY, "Failed to open tty r/o" },
	{ STIERR_WOTTY, "Failed to open tty w/o" },
	{ STIERR_WOOK, "TIOCSTI on w/o tty succeeded" },
	{ STIERR_BADERR, "Received wrong error from failed TIOCSTI" },
};
_Static_assert(nitems(stierr_map) == STIERR_MAXERR,
    "Failed to describe all errors");

/*
 * Inject each character of the input string into the TTY.  The caller can
 * assume that errno is preserved on return.
 */
static ssize_t
inject(int fileno, const char *str)
{
	size_t nb = 0;

	for (const char *walker = str; *walker != '\0'; walker++) {
		if (ioctl(fileno, TIOCSTI, walker) != 0)
			return (-1);
		nb++;
	}

	return (nb);
}

/*
 * Forks off a new process, stashes the parent's handle for the pty in *termfd
 * and returns the pid.  0 for the child, >0 for the parent, as usual.
 *
 * Most tests fork so that we can do them while unprivileged, which we can only
 * do if we're operating on our ctty (and we don't want to touch the tty of
 * whatever may be running the tests).
 */
static int
init_pty(int *termfd, bool canon)
{
	int pid;

	pid = forkpty(termfd, NULL, NULL, NULL);
	ATF_REQUIRE(pid != -1);

	if (pid == 0) {
		struct termios term;

		/*
		 * Child reconfigures tty to disable echo and put it into raw
		 * mode if requested.
		 */
		if (tcgetattr(STDIN_FILENO, &term) == -1)
			_exit(STIERR_CONFIG_FETCH);
		term.c_lflag &= ~ECHO;
		if (!canon)
			term.c_lflag &= ~ICANON;
		if (tcsetattr(STDIN_FILENO, TCSANOW, &term) == -1)
			_exit(STIERR_CONFIG);
	}

	return (pid);
}

static void
finalize_child(pid_t pid, int signo)
{
	int status, wpid;

	while ((wpid = waitpid(pid, &status, 0)) != pid) {
		if (wpid != -1)
			continue;
		ATF_REQUIRE_EQ_MSG(EINTR, errno,
		    "waitpid: %s", strerror(errno));
	}

	/*
	 * Some tests will signal the child for whatever reason, and we're
	 * expecting it to terminate it.  For those cases, it's OK to just see
	 * that termination.  For all other cases, we expect a graceful exit
	 * with an exit status that reflects a cause that we have an error
	 * mapped for.
	 */
	if (signo >= 0) {
		ATF_REQUIRE(WIFSIGNALED(status));
		ATF_REQUIRE_EQ(signo, WTERMSIG(status));
	} else {
		ATF_REQUIRE(WIFEXITED(status));
		if (WEXITSTATUS(status) != 0) {
			int err = WEXITSTATUS(status);

			for (size_t i = 0; i < nitems(stierr_map); i++) {
				const struct stierr_map *map = &stierr_map[i];

				if ((int)map->stierr == err) {
					atf_tc_fail("%s", map->msg);
					__assert_unreachable();
				}
			}
		}
	}
}

ATF_TC(basic);
ATF_TC_HEAD(basic, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test for basic functionality of TIOCSTI");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(basic, tc)
{
	int pid, term;

	/*
	 * We don't canonicalize on this test because we can assume that the
	 * injected data will be available after TIOCSTI returns.  This is all
	 * within a single thread for the basic test, so we simplify our lives
	 * slightly in raw mode.
	 */
	pid = init_pty(&term, false);
	if (pid == 0) {
		static const char sending[] = "Text";
		char readbuf[32];
		ssize_t injected, readsz;

		injected = inject(STDIN_FILENO, sending);
		if (injected != sizeof(sending) - 1)
			_exit(STIERR_INJECT);

		readsz = read(STDIN_FILENO, readbuf, sizeof(readbuf));

		if (readsz < 0 || readsz != injected)
			_exit(STIERR_READFAIL);
		if (memcmp(readbuf, sending, readsz) != 0)
			_exit(STIERR_BADTEXT);

		_exit(0);
	}

	finalize_child(pid, -1);
}

ATF_TC(root);
ATF_TC_HEAD(root, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that root can inject into another TTY");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(root, tc)
{
	static const char sending[] = "Text\r";
	ssize_t injected;
	int pid, term;

	/*
	 * We leave canonicalization enabled for this one so that the read(2)
	 * below hangs until we have all of the data available, rather than
	 * having to signal OOB that it's safe to read.
	 */
	pid = init_pty(&term, true);
	if (pid == 0) {
		char readbuf[32];
		ssize_t readsz;

		readsz = read(STDIN_FILENO, readbuf, sizeof(readbuf));
		if (readsz < 0 || readsz != sizeof(sending) - 1)
			_exit(STIERR_READFAIL);

		/*
		 * Here we ignore the trailing \r, because it won't have
		 * surfaced in our read(2).
		 */
		if (memcmp(readbuf, sending, readsz - 1) != 0)
			_exit(STIERR_BADTEXT);

		_exit(0);
	}

	injected = inject(term, sending);
	ATF_REQUIRE_EQ_MSG(sizeof(sending) - 1, injected,
	    "Injected %zu characters, expected %zu", injected,
	    sizeof(sending) - 1);

	finalize_child(pid, -1);
}

ATF_TC(unprivileged_fail_noctty);
ATF_TC_HEAD(unprivileged_fail_noctty, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that unprivileged cannot inject into non-controlling TTY");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(unprivileged_fail_noctty, tc)
{
	const char sending[] = "Text";
	ssize_t injected;
	int pid, serrno, term;

	pid = init_pty(&term, false);
	if (pid == 0) {
		char readbuf[32];
		ssize_t readsz;

		/*
		 * This should hang until we get terminated by the parent.
		 */
		readsz = read(STDIN_FILENO, readbuf, sizeof(readbuf));
		if (readsz > 0)
			_exit(STIERR_DATAFOUND);

		_exit(0);
	}

	/* Should fail. */
	injected = inject(term, sending);
	serrno = errno;

	/* Done with the child, just kill it now to avoid problems later. */
	kill(pid, SIGINT);
	finalize_child(pid, SIGINT);

	ATF_REQUIRE_EQ_MSG(-1, (ssize_t)injected,
	    "TIOCSTI into non-ctty succeeded");
	ATF_REQUIRE_EQ(EACCES, serrno);
}

ATF_TC(unprivileged_fail_noread);
ATF_TC_HEAD(unprivileged_fail_noread, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that unprivileged cannot inject into TTY not opened for read");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(unprivileged_fail_noread, tc)
{
	int pid, term;

	/*
	 * Canonicalization actually doesn't matter for this one, we'll trust
	 * that the failure means we didn't inject anything.
	 */
	pid = init_pty(&term, true);
	if (pid == 0) {
		static const char sending[] = "Text";
		ssize_t injected;
		int rotty, wotty;

		/*
		 * We open the tty both r/o and w/o to ensure we got the device
		 * name right; one of these will pass, one of these will fail.
		 */
		wotty = openat(STDIN_FILENO, "", O_EMPTY_PATH | O_WRONLY);
		if (wotty == -1)
			_exit(STIERR_WOTTY);
		rotty = openat(STDIN_FILENO, "", O_EMPTY_PATH | O_RDONLY);
		if (rotty == -1)
			_exit(STIERR_ROTTY);

		/*
		 * This injection is expected to fail with EPERM, because it may
		 * be our controlling tty but it is not open for reading.
		 */
		injected = inject(wotty, sending);
		if (injected != -1)
			_exit(STIERR_WOOK);
		if (errno != EPERM)
			_exit(STIERR_BADERR);

		/*
		 * Demonstrate that it does succeed on the other fd we opened,
		 * which is r/o.
		 */
		injected = inject(rotty, sending);
		if (injected != sizeof(sending) - 1)
			_exit(STIERR_INJECT);

		_exit(0);
	}

	finalize_child(pid, -1);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, basic);
	ATF_TP_ADD_TC(tp, root);
	ATF_TP_ADD_TC(tp, unprivileged_fail_noctty);
	ATF_TP_ADD_TC(tp, unprivileged_fail_noread);

	return (atf_no_error());
}
