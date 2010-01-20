/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "From: @(#)swapon.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/disk.h>
#include <sys/sysctl.h>

#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static int	verbose;

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n",
	    "usage: dumpon [-v] special_file",
	    "       dumpon [-v] off");
	exit(EX_USAGE);
}

static void
check_size(int fd, const char *fn)
{
	int name[] = { CTL_HW, HW_PHYSMEM };
	size_t namelen = sizeof(name) / sizeof(*name);
	unsigned long physmem;
	size_t len;
	off_t mediasize;
	int minidump;

	len = sizeof(minidump);
	if (sysctlbyname("debug.minidump", &minidump, &len, NULL, 0) == 0 &&
	    minidump == 1)
		return;
	len = sizeof(physmem);
	if (sysctl(name, namelen, &physmem, &len, NULL, 0) != 0)
		err(EX_OSERR, "can't get memory size");
	if (ioctl(fd, DIOCGMEDIASIZE, &mediasize) != 0)
		err(EX_OSERR, "%s: can't get size", fn);
	if ((uintmax_t)mediasize < (uintmax_t)physmem) {
		if (verbose)
			printf("%s is smaller than physical memory\n", fn);
		exit(EX_IOERR);
	}
}

int
main(int argc, char *argv[])
{
	int ch;
	int i, fd;
	u_int u;

	while ((ch = getopt(argc, argv, "v")) != -1)
		switch((char)ch) {
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	if (strcmp(argv[0], "off") != 0) {
		fd = open(argv[0], O_RDONLY);
		if (fd < 0)
			err(EX_OSFILE, "%s", argv[0]);
		check_size(fd, argv[0]);
		u = 0;
		i = ioctl(fd, DIOCSKERNELDUMP, &u);
		u = 1;
		i = ioctl(fd, DIOCSKERNELDUMP, &u);
		if (i == 0 && verbose)
			printf("kernel dumps on %s\n", argv[0]);
	} else {
		fd = open(_PATH_DEVNULL, O_RDONLY);
		if (fd < 0)
			err(EX_OSFILE, "%s", _PATH_DEVNULL);
		u = 0;
		i = ioctl(fd, DIOCSKERNELDUMP, &u);
		if (i == 0 && verbose)
			printf("kernel dumps disabled\n");
	}
	if (i < 0)
		err(EX_OSERR, "ioctl(DIOCSKERNELDUMP)");

	exit (0);
}
