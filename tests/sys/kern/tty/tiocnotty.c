/*
 * Copyright (c) 2026 Mark Johnston <markj@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * A regression test that exercises a bug where TIOCNOTTY would leave some
 * dangling pointers behind in the controlling terminal structure.
 */

#include <sys/ioctl.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(void)
{
	int master, slave, status;
	pid_t child;

	master = posix_openpt(O_RDWR | O_NOCTTY);
	if (master < 0)
		err(1, "posix_openpt");
	if (grantpt(master) < 0)
		err(1, "grantpt");
	if (unlockpt(master) < 0)
		err(1, "unlockpt");

	child = fork();
	if (child < 0)
		err(1, "fork");
	if (child == 0) {
		if (setsid() < 0)
			err(1, "setsid");
		slave = open(ptsname(master), O_RDWR | O_NOCTTY);
		if (slave < 0)
			err(2, "open");
		if (ioctl(slave, TIOCSCTTY, 0) < 0)
			err(3, "ioctl(TIOCSCTTY)");
		/* Detach ourselves from the controlling terminal. */
		if (ioctl(slave, TIOCNOTTY, 0) < 0)
			err(4, "ioctl(TIOCNOTTY)");
		_exit(0);
	}

	if (waitpid(child, &status, 0) < 0)
		err(1, "waitpid");
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		errx(1, "child exited with status %d", WEXITSTATUS(status));

	child = fork();
	if (child < 0)
		err(1, "fork");
	if (child == 0) {
		struct winsize winsz;

		if (setsid() < 0)
			err(1, "setsid");
		slave = open(ptsname(master), O_RDWR | O_NOCTTY);
		if (slave < 0)
			err(2, "open");
		/* Dereferences dangling t_pgrp pointer in the terminal. */
		memset(&winsz, 0xff, sizeof(winsz));
		if (ioctl(slave, TIOCSWINSZ, &winsz) < 0)
			err(3, "ioctl(TIOCSWINSZ)");
		/* Dereferences dangling t_session pointer in the terminal. */
		if (ioctl(slave, TIOCSCTTY, 0) < 0)
			err(4, "ioctl(TIOCSCTTY)");
		_exit(0);
	}

	if (waitpid(child, &status, 0) < 0)
		err(1, "waitpid");
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		errx(1, "child exited with status %d", WEXITSTATUS(status));
}
