/*-
 * Copyright (c) 2013 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/endian.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	PATH_TEMPFANCTL	"/dev/de4tempfan"

#define	OFF_TEMP	0
#define	OFF_FAN	4

static int	qflag;	/* Quiet flag -- print only numbers, not field names. */

static void
usage(void)
{

	fprintf(stderr, "usage: de4ctl [-q] fan | temp\n");
	exit(0);
}

static void
do_temp(void)
{
	uint32_t temp;
	ssize_t len;
	int fd;

	fd = open(PATH_TEMPFANCTL, O_RDONLY);
	if (fd < 0)
		err(1, "%s", PATH_TEMPFANCTL);

	len = pread(fd, &temp, sizeof(temp), OFF_TEMP);
	if (len < 0)
		err(1, "%s", PATH_TEMPFANCTL);
	if (len != sizeof(temp))
		errx(1, "%s: short read", PATH_TEMPFANCTL);
	printf("%s%u\n", qflag ? "" : "temp:\t", le32toh(temp));
	close(fd);
}

static void
do_fan(void)
{
	uint32_t fan;
	ssize_t len;
	int fd;

	fd = open(PATH_TEMPFANCTL, O_RDONLY);
	if (fd < 0)
		err(1, "%s", PATH_TEMPFANCTL);

	len = pread(fd, &fan, sizeof(fan), OFF_FAN);
	if (len < 0)
		err(1, "%s", PATH_TEMPFANCTL);
	if (len != sizeof(fan))
		errx(1, "%s: short read", PATH_TEMPFANCTL);
	printf("%s%u\n", qflag ? "" : "fan:\t", le32toh(fan));
	close(fd);
}

int
main(int argc, char **argv)
{
	int ch;

	while ((ch = getopt(argc, argv, "q")) != -1) {
		switch (ch) {
		case 'q':
			qflag = 1;
			break;

		case '?':
		default:
			usage();
			/* NOTREACHED */
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	if (strcmp(argv[0], "fan") == 0)
		do_fan();
	else if (strcmp(argv[0], "temp") == 0)
		do_temp();
	else
		usage();
	return (0);
}
