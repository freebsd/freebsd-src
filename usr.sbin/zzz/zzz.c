/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * This software was developed by Aymeric Wibo <obiwac@freebsd.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/power.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

static void
usage(void)
{
	(void)fprintf(stderr, "usage: zzz\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int powerfd;
	enum power_transition trans;

	(void)argv;
	if (argc > 1)
		usage();

	powerfd = open(_PATH_DEVPOWER, O_RDWR);
	if (powerfd < 0)
		err(EX_OSFILE, "could not open power device");

	trans = POWER_TRANSITION_SUSPEND;
	if (ioctl(powerfd, PIOTRANSITION, &trans) != 0)
		err(EX_IOERR, "could not request suspend transition");

	return (EXIT_SUCCESS);
}
