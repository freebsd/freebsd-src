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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "From: @(#)swapon.c	8.1 (Berkeley) 6/5/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <paths.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/disklabel.h>
#include <sysexits.h>

void	usage(void) __dead2;

int
main(int argc, char *argv[])
{
	int ch, verbose, rv;
	int i, fd;
	u_int u;

	verbose = rv = 0;
	while ((ch = getopt(argc, argv, "v")) != -1)
		switch((char)ch) {
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;

	if (!argv[0] || argv[1])
		usage();

	if (strcmp(argv[0], "off")) {
		fd = open(argv[0], O_RDONLY);
		if (fd < 0)
			err(EX_OSFILE, "%s", argv[0]);
		u = 0;
		i = ioctl(fd, DIOCGKERNELDUMP, &u);
		u = 1;
		i = ioctl(fd, DIOCGKERNELDUMP, &u);
		if (i == 0 && verbose)
			printf("kernel dumps on %s\n", argv[0]);
			
	} else {
		fd = open(_PATH_DEVNULL, O_RDONLY);
		if (fd < 0)
			err(EX_OSFILE, "%s", _PATH_DEVNULL);
		u = 0;
		i = ioctl(fd, DIOCGKERNELDUMP, &u);
		if (i == 0 && verbose)
			printf("kernel dumps disabled\n");
	}
	if (i < 0)
		err(EX_OSERR, "ioctl(DIOCGKERNELDUMP)");

	exit (0);
}

void
usage(void)
{
	fprintf(stderr,
		"usage: dumpon [-v] special_file\n"
		"       dumpon [-v] off\n");
	exit(EX_USAGE);
}
