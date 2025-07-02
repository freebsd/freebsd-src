/*
 * Copyright (c) 2025 Rick Macklem
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/wait.h>
#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
usage(void)
{
	(void)fprintf(stderr, "usage: runat <file> "
	    "<shell command>\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int i, file_fd, nameddir_fd, outsiz;
	char *buf;
	long named_enabled;
	size_t pos, siz;

	if (argc <= 2)
		usage();
	argv++;
	argc--;
	if (argc < 2)
		usage();

	named_enabled = pathconf(argv[0], _PC_NAMEDATTR_ENABLED);
	if (named_enabled <= 0)
		errx(1, "Named attributes not enabled for %s", argv[0]);

	/* Generate the command string for "sh". */
	siz = 0;
	for (i = 1; i < argc; i++)
		siz += strlen(argv[i]) + 1;
	buf = malloc(siz);
	if (buf == NULL)
		errx(1, "Cannot malloc");
	pos = 0;
	for (i = 1; i < argc; i++) {
		outsiz = snprintf(&buf[pos], siz, "%s ", argv[i]);
		if (outsiz <= 0)
			errx(1, "snprintf failed: returned %d", outsiz);
		if ((size_t)outsiz > siz)
			errx(1, "Arguments too large");
		pos += outsiz;
		siz -= outsiz;
	}
	buf[pos - 1] = '\0';

	file_fd = open(argv[0], O_RDONLY | O_CLOEXEC, 0);
	if (file_fd < 0)
		err(1, "Cannot open %s", argv[0]);
	nameddir_fd = openat(file_fd, ".", O_RDONLY | O_CLOEXEC | O_NAMEDATTR,
	    0);
	if (nameddir_fd < 0)
		err(1, "Cannot open named attribute directory "
		    "for %s", argv[0]);

	if (fchdir(nameddir_fd) < 0)
		err(1, "Cannot fchdir to named attribute dir");

	execl(_PATH_BSHELL, "sh", "-c", buf, NULL);
	err(1, "Could not exec %s", _PATH_BSHELL);
}
