/*
 * Copyright (C) 1994, 2001 by Joerg Wunsch, Dresden
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/fdcio.h>
#include <sys/file.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "fdutil.h"


static	int debug = -1, format, verbose, show = 1, showfmt;
static	char *fmtstring;

static void showdev(enum fd_drivetype, const char *);
static void usage(void);

static void
usage(void)
{
	errx(EX_USAGE,
	     "usage: fdcontrol [-F] [-d dbg] [-f fmt] [-s fmtstr] [-v] device");
}

void
showdev(enum fd_drivetype type, const char *fname)
{
	const char *name, *descr;

	getname(type, &name, &descr);
	if (verbose)
		printf("%s: %s drive (%s)\n", fname, name, descr);
	else
		printf("%s\n", name);
}

int
main(int argc, char **argv)
{
	enum fd_drivetype type;
	struct fd_type ft, newft, *fdtp;
	const char *name, *descr;
	int fd, i, mode;

	while((i = getopt(argc, argv, "d:Ff:s:v")) != -1)
		switch(i) {
		case 'd':
			if (strcmp(optarg, "0") == 0)
				debug = 0;
			else if (strcmp(optarg, "1") == 0)
				debug = 1;
			else
				usage();
			show = 0;
			break;

		case 'F':
			showfmt = 1;
			show = 0;
			break;

		case 'f':
			if (getnum(optarg, &format)) {
				fprintf(stderr,
			"Bad argument %s to -f option; must be numeric\n",
					optarg);
				usage();
			}
			show = 0;
			break;

		case 's':
			fmtstring = optarg;
			show = 0;
			break;

		case 'v':
			verbose++;
			break;

		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if(argc != 1)
		usage();

	if (show || showfmt)
		mode = O_RDONLY | O_NONBLOCK;
	else
		mode = O_RDWR;

	if((fd = open(argv[0], mode)) < 0)
		err(EX_UNAVAILABLE, "open(%s)", argv[0]);

	if (ioctl(fd, FD_GDTYPE, &type) == -1)
		err(EX_OSERR, "ioctl(FD_GDTYPE)");
	if (ioctl(fd, FD_GTYPE, &ft) == -1)
		err(EX_OSERR, "ioctl(FD_GTYPE)");

	if (show) {
		showdev(type, argv[0]);
		return (0);
	}

	if (format) {
		getname(type, &name, &descr);
		fdtp = get_fmt(format, type);
		if (fdtp == 0)
			errx(EX_USAGE,
			    "unknown format %d KB for drive type %s",
			    format, name);
		ft = *fdtp;
	}

	if (fmtstring) {
		parse_fmt(fmtstring, type, ft, &newft);
		ft = newft;
	}

	if (showfmt) {
		if (verbose)
			printf("%s: %d KB media type, fmt = ",
			       argv[0], ft.size / 2);
		print_fmt(ft);
		return (0);
	}

	if (format || fmtstring) {
		if (ioctl(fd, FD_STYPE, &ft) == -1)
			err(EX_OSERR, "ioctl(FD_STYPE)");
		return (0);
	}

	if (debug != -1) {
		if (ioctl(fd, FD_DEBUG, &debug) == -1)
			err(EX_OSERR, "ioctl(FD_DEBUG)");
		return (0);
	}

	return 0;
}
