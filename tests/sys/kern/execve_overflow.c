/*
 * Copyright (c) 2026 Mark Johnston <markj@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * A minimal regression test for FreeBSD-SA-26:13.exec.
 */

#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	SCRIPTNAME	"script"
#define	SCRIPTBODY	"#!/bin/sh\nexit 0\n"

int
main(void)
{
	char *argv;
	size_t size;
	int fd;

	fd = open(SCRIPTNAME, O_WRONLY | O_CREAT | O_TRUNC, 0700);
	if (fd == -1)
		err(1, "open");
	if (write(fd, SCRIPTBODY, sizeof(SCRIPTBODY) - 1) !=
	    sizeof(SCRIPTBODY) - 1)
		err(1, "write");
	close(fd);

	size = ARG_MAX / 2;
	argv = malloc(size);
	if (argv == NULL)
		err(1, "malloc");
	memset(argv, 'a', size - 1);
	argv[size - 1] = '\0';

	execve(SCRIPTNAME, (char *[]){ argv, NULL }, (char *[]){ NULL });

	exit(1);
}
