/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2024 Oxide Computer Company
 */

/*
 * Verify that unsupported flags will properly generate errors across the
 * functions that we know perform strict error checking. This includes:
 *
 *  o fcntl(..., F_DUP3FD, ...)
 *  o dup3()
 *  o pipe2()
 *  o socket()
 *  o accept4()
 */

#include <stdlib.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stdbool.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/socket.h>

static bool
oclo_check(const char *desc, const char *act, int ret, int e)
{
	if (ret >= 0) {
		warnx("TEST FAILED: %s: fd was %s!", desc, act);
		return (false);
	} else if (errno != EINVAL) {
		int e = errno;
		warnx("TEST FAILED: %s: failed with %s, expected "
		    "EINVAL", desc, strerrorname_np(e));
		return (false);
	}

	(void) printf("TEST PASSED: %s: correctly failed with EINVAL\n",
	    desc);
	return (true);
}

static bool
oclo_dup3(const char *desc, int flags)
{
	int fd = dup3(STDERR_FILENO, 23, flags);
	return (oclo_check(desc, "duplicated", fd, errno));
}

static bool
oclo_dup3fd(const char *desc, int flags)
{
	int fd = fcntl(STDERR_FILENO, F_DUP3FD, 23, flags);
	return (oclo_check(desc, "duplicated", fd, errno));
}


static bool
oclo_pipe2(const char *desc, int flags)
{
	int fds[2], ret;

	ret = pipe2(fds, flags);
	return (oclo_check(desc, "piped", ret, errno));
}

static bool
oclo_socket(const char *desc, int type)
{
	int fd = socket(PF_UNIX, SOCK_STREAM | type, 0);
	return (oclo_check(desc, "created", fd, errno));
}

static bool
oclo_accept(const char *desc, int flags)
{
	int sock, fd, e;
	struct sockaddr_in in;

	sock = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (sock < 0) {
		warn("TEST FAILED: %s: failed to create listen socket", desc);
		return (false);
	}

	(void) memset(&in, 0, sizeof (in));
	in.sin_family = AF_INET;
	in.sin_port = 0;
	in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(sock, (struct sockaddr *)&in, sizeof (in)) != 0) {
		warn("TEST FAILED: %s: failed to bind socket", desc);
		(void) close(sock);
		return (false);
	}

	if (listen(sock, 5) < 0) {
		warn("TEST FAILED: %s: failed to listen on socket", desc);
		(void) close(sock);
		return (false);
	}


	fd = accept4(sock, NULL, NULL, flags);
	e = errno;
	(void) close(sock);
	return (oclo_check(desc, "accepted", fd, e));
}

int
main(void)
{
	int ret = EXIT_SUCCESS;

	closefrom(STDERR_FILENO + 1);

	if (!oclo_dup3("dup3(): O_RDWR", O_RDWR)) {
		ret = EXIT_FAILURE;
	}

	if (!oclo_dup3("dup3(): O_NONBLOCK|O_CLOXEC", O_NONBLOCK | O_CLOEXEC)) {
		ret = EXIT_FAILURE;
	}

	if (!oclo_dup3("dup3(): O_CLOFORK|O_WRONLY", O_CLOFORK | O_WRONLY)) {
		ret = EXIT_FAILURE;
	}

	if (!oclo_dup3fd("fcntl(FDUP3FD): 0x7777", 0x7777)) {
		ret = EXIT_FAILURE;
	}

	if (!oclo_dup3fd("fcntl(FDUP3FD): FD_CLOEXEC|FD_CLOFORK + 1",
	    (FD_CLOEXEC | FD_CLOFORK) + 1)) {
		ret = EXIT_FAILURE;
	}

	if (!oclo_dup3fd("fcntl(FDUP3FD): INT_MAX", INT_MAX)) {
		ret = EXIT_FAILURE;
	}


	if (!oclo_pipe2("pipe2(): O_RDWR", O_RDWR)) {
		ret = EXIT_FAILURE;
	}

	if (!oclo_pipe2("pipe2(): O_SYNC|O_CLOXEC", O_SYNC | O_CLOEXEC)) {
		ret = EXIT_FAILURE;
	}

	if (!oclo_pipe2("pipe2(): O_CLOFORK|O_WRONLY", O_CLOFORK | O_WRONLY)) {
		ret = EXIT_FAILURE;
	}

	if (!oclo_pipe2("pipe2(): INT32_MAX", INT32_MAX)) {
		ret = EXIT_FAILURE;
	}

	if (!oclo_socket("socket(): INT32_MAX", INT32_MAX)) {
		ret = EXIT_FAILURE;
	}

	if (!oclo_socket("socket(): 3 << 25", 3 << 25)) {
		ret = EXIT_FAILURE;
	}

	if (!oclo_accept("accept4(): INT32_MAX", INT32_MAX)) {
		ret = EXIT_FAILURE;
	}

	if (!oclo_accept("accept4(): 3 << 25", 3 << 25)) {
		ret = EXIT_FAILURE;
	}

	if (ret == EXIT_SUCCESS) {
		(void) printf("All tests completed successfully\n");
	}

	return (ret);
}
