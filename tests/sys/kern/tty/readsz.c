/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void
usage(void)
{

	fprintf(stderr, "usage: %s [-b bytes | -c lines | -e] [-s buffer-size]\n",
	    getprogname());
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *buf;
	const char *errstr;
	size_t bufsz = 0, reps;
	ssize_t ret;
	enum { MODE_BYTES, MODE_COUNT, MODE_EOF } mode;
	int ch;

	/*
	 * -b specifies number of bytes.
	 * -c specifies number of read() calls.
	 * -e specifies eof (default)
	 * -s to pass a buffer size
	 *
	 * Reading N lines is the same as -c with a high buffer size.
	 */
	mode = MODE_EOF;
	while ((ch = getopt(argc, argv, "b:c:es:")) != -1) {
		switch (ch) {
		case 'b':
			mode = MODE_BYTES;
			reps = strtonum(optarg, 0, SSIZE_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "strtonum: %s", errstr);
			break;
		case 'c':
			mode = MODE_COUNT;
			reps = strtonum(optarg, 1, SSIZE_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "strtonum: %s", errstr);
			break;
		case 'e':
			mode = MODE_EOF;
			break;
		case 's':
			bufsz = strtonum(optarg, 1, SSIZE_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "strtonum: %s", errstr);
			break;
		default:
			usage();
		}
	}

	if (bufsz == 0) {
		if (mode == MODE_BYTES)
			bufsz = reps;
		else
			bufsz = LINE_MAX;
	}

	buf = malloc(bufsz);
	if (buf == NULL)
		err(1, "malloc");

	for (;;) {
		size_t readsz;

		/*
		 * Be careful not to over-read if we're in byte-mode.  In every other
		 * mode, we'll read as much as we can.
		 */
		if (mode == MODE_BYTES)
			readsz = MIN(bufsz, reps);
		else
			readsz = bufsz;

		ret = read(STDIN_FILENO, buf, readsz);
		if (ret == -1 && errno == EINTR)
			continue;
		if (ret == -1)
			err(1, "read");
		if (ret == 0) {
			if (mode == MODE_EOF)
				return (0);
			errx(1, "premature EOF");
		}

		/* Write out what we've got */
		write(STDOUT_FILENO, buf, ret);

		/*
		 * Bail out if we've hit our metric (byte mode / count mode).
		 */
		switch (mode) {
		case MODE_BYTES:
			reps -= ret;
			if (reps == 0)
				return (0);
			break;
		case MODE_COUNT:
			reps--;
			if (reps == 0)
				return (0);
			break;
		default:
			break;
		}
	}

	return (0);
}
