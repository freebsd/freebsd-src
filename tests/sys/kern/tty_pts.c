/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <atf-c.h>
#include <libutil.h>

/* Just a little more concise. */
#define	newpty(masterp, slavep)	openpty((masterp), (slavep), NULL, NULL, NULL)

ATF_TC_WITHOUT_HEAD(fionread);
ATF_TC_BODY(fionread, tc)
{
	char rbuf[32];
	char buf[] = "Hello";
	int master, slave;
	int bytes;

	ATF_REQUIRE_EQ(0, newpty(&master, &slave));

	/* Should be empty to begin with. */
	ATF_REQUIRE_EQ(0, ioctl(master, FIONREAD, &bytes));
	ATF_REQUIRE_EQ(0, bytes);

	ATF_REQUIRE_EQ(sizeof(buf) - 1, write(slave, buf, sizeof(buf) - 1));
	ATF_REQUIRE_EQ(0, ioctl(master, FIONREAD, &bytes));
	ATF_REQUIRE_EQ(sizeof(buf) - 1, bytes);

	/* Drain what we have available, should result in 0 bytes again. */
	ATF_REQUIRE_EQ(sizeof(buf) - 1, read(master, rbuf, sizeof(rbuf)));
	ATF_REQUIRE_EQ(0, ioctl(master, FIONREAD, &bytes));
	ATF_REQUIRE_EQ(0, bytes);

	/*
	 * Write once more, then close the slave side with data still in the
	 * buffer.
	 */
	ATF_REQUIRE_EQ(sizeof(buf) - 1, write(slave, buf, sizeof(buf) - 1));
	ATF_REQUIRE_EQ(0, ioctl(master, FIONREAD, &bytes));
	ATF_REQUIRE_EQ(sizeof(buf) - 1, bytes);

	ATF_REQUIRE_EQ(0, close(slave));

	/*
	 * The tty's output queue is discarded upon close, so we shouldn't have
	 * anything else to read().
	 */
	ATF_REQUIRE_EQ(0, ioctl(master, FIONREAD, &bytes));
	ATF_REQUIRE_EQ(0, bytes);
	ATF_REQUIRE_EQ(0, read(master, rbuf, sizeof(rbuf)));
}

ATF_TC(ctty_last_close);
ATF_TC_HEAD(ctty_last_close, tc)
{

	atf_tc_set_md_var(tc, "descr", "Drop a native controlling terminal "
	    "on the last slave close");
}
ATF_TC_BODY(ctty_last_close, tc)
{
	struct {
		int fd;
		int error;
	} result;
	char byte;
	int go[2], master, ready[2], slave, status;
	ssize_t nread;
	pid_t child;

	ATF_REQUIRE_EQ(0, newpty(&master, &slave));
	ATF_REQUIRE_EQ(0, pipe(ready));
	ATF_REQUIRE_EQ(0, pipe(go));

	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		(void)close(master);
		(void)close(ready[0]);
		(void)close(go[1]);
		if (setsid() < 0)
			_exit(10);
		if (ioctl(slave, TIOCSCTTY, 0) < 0)
			_exit(11);
		if (write(ready[1], "r", 1) != 1)
			_exit(12);
		if (read(go[0], &byte, 1) != 1)
			_exit(13);
		if (close(slave) < 0)
			_exit(14);
		errno = 0;
		result.fd = open("/dev/tty", O_RDWR);
		result.error = errno;
		if (write(ready[1], &result, sizeof(result)) !=
		    (ssize_t)sizeof(result))
			_exit(15);
		if (result.fd >= 0)
			(void)close(result.fd);
		_exit(0);
	}

	(void)close(ready[1]);
	(void)close(go[0]);
	ATF_REQUIRE_EQ(1, read(ready[0], &byte, 1));
	ATF_REQUIRE_EQ(0, close(slave));
	ATF_REQUIRE_EQ(1, write(go[1], "g", 1));
	nread = read(ready[0], &result, sizeof(result));
	ATF_REQUIRE_MSG(waitpid(child, &status, 0) == child,
	    "waitpid failed: %s", strerror(errno));
	(void)close(ready[0]);
	(void)close(go[1]);
	(void)close(master);

	ATF_REQUIRE_MSG(WIFEXITED(status), "child died from signal %d",
	    WTERMSIG(status));
	ATF_REQUIRE_MSG(WEXITSTATUS(status) == 0,
	    "child exited with status %d", WEXITSTATUS(status));
	ATF_REQUIRE_MSG(nread == (ssize_t)sizeof(result),
	    "short result read: %zd", nread);
	ATF_CHECK_EQ(-1, result.fd);
	ATF_CHECK_EQ(ENXIO, result.error);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, fionread);
	ATF_TP_ADD_TC(tp, ctty_last_close);
	return (atf_no_error());
}
