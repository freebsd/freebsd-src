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
 * Copyright 2025 Oxide Computer Company
 */

/*
 * Verify that our file descriptors starting after stderr are correct based upon
 * the series of passed in arguments from the 'oclo' program. Arguments are
 * passed as a string that represents the flags that were originally verified
 * pre-fork/exec via fcntl(F_GETFD). In addition, anything that was originally
 * closed because it had FD_CLOFORK set was reopened with the same flags. This
 * allows us to verify that the combinations worked and that FD_CLOFORK was
 * properly cleared.
 */

#include <err.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

static int
verify_fdwalk_cb(void *arg, int fd)
{
	int *max = arg;
	*max = fd;
	return (0);
}

/*
 * Our flags may have FD_CLOFORK set in them (anything with FD_CLOEXEC Should
 * not exist by definition). FD_CLOFORK is supposed to be cleared on exec. We
 * still indicate which file descriptors FD_CLOFORK so we can check where it
 * wasn't cleared.
 */
static bool
verify_flags(int fd, int exp_flags)
{
	bool fail = (exp_flags & FD_CLOEXEC) != 0;
	int flags = fcntl(fd, F_GETFD, NULL);
	bool clofork = (exp_flags & FD_CLOFORK) != 0;
	exp_flags &= ~FD_CLOFORK;

	if (flags < 0) {
		int e = errno;

		if (fail) {
			if (e == EBADF) {
				(void) printf("TEST PASSED: post-exec fd %d: "
				    "flags 0x%x: correctly closed\n", fd,
				    exp_flags);
				return (true);
			}


			warn("TEST FAILED: post-fork fd %d: expected fcntl to "
			    "fail with EBADF, but found %s", fd,
			    strerrorname_np(e));
			return (false);
		}

		warnx("TEST FAILED: post-fork fd %d: fcntl(F_GETFD) "
		    "unexpectedly failed with %s, expected flags %d", fd,
		    strerrorname_np(e), exp_flags);
		return (false);
	}

	if (fail) {
		warnx("TEST FAILED: post-fork fd %d: received flags %d, but "
		    "expected to fail based on flags %d", fd, flags, exp_flags);
		return (false);
	}

	if (clofork && (flags & FD_CLOFORK) != 0) {
		warnx("TEST FAILED: post-fork fd %d (flags %d) retained "
		    "FD_CLOFORK, but it should have been cleared", fd, flags);
		return (false);
	}

	if (flags != exp_flags) {
		warnx("TEST FAILED: post-exec fd %d: discovered flags 0x%x do "
		    "not match expected flags 0x%x", fd, flags, exp_flags);
		return (false);
	}

	(void) printf("TEST PASSED: post-exec fd %d: flags 0x%x: successfully "
	    "matched\n", fd, exp_flags);
	return (true);
}

int
main(int argc, char *argv[])
{
	int maxfd = STDIN_FILENO;
	int ret = EXIT_SUCCESS;

	/*
	 * We should have one argument for each fd we found, ignoring stdin,
	 * stdout, and stderr. argc will also have an additional entry for our
	 * program name, which we want to skip. Note, the last fd may not exist
	 * because it was marked for close, hence the use of '>' below.
	 */
	(void) fdwalk(verify_fdwalk_cb, &maxfd);
	if (maxfd - 3 > argc - 1) {
		errx(EXIT_FAILURE, "TEST FAILED: found more fds %d than "
		    "arguments %d", maxfd - 3, argc - 1);
	}

	for (int i = 1; i < argc; i++) {
		const char *errstr;
		int targ_fd = i + STDERR_FILENO;
		long long targ_flags = strtonumx(argv[i], 0,
		    FD_CLOEXEC | FD_CLOFORK, &errstr, 0);

		if (errstr != NULL) {
			errx(EXIT_FAILURE, "TEST FAILED: failed to parse "
			    "argument %d: %s is %s", i, argv[i], errstr);
		}

		if (!verify_flags(targ_fd, (int)targ_flags))
			ret = EXIT_FAILURE;
	}

	return (ret);
}
