/* 
 * Copyright (C) 1996
 *   interface business GmbH
 *   Tolkewitzer Strasse 49
 *   D-01277 Dresden
 *   F.R. Germany
 *
 * All rights reserved.
 *
 * Written by Joerg Wunsch <joerg_wunsch@interface-business.de>
 *
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
 *
 * $Id: wormcontrol.c,v 1.1.1.1.2.1 1997/03/10 20:56:34 joerg Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <sysexits.h>

#include <sys/ioctl.h>
#include <sys/wormio.h>


void
usage(void)
{
	errx(EX_USAGE,
	     "usage: wormcontrol [-f device] command [args]\n"
	     "commands:\n"
	     "       select vendor-id model-id\n"
	     "       prepdisk [dummy] single|double\n"
	     "       track audio|data [preemp]\n"
	     "       fixate toc-type [onp]\n");
}

#define eq(a, b) (strcmp(a, b) == 0)

int
main(int argc, char **argv)
{
	int fd, c, i;
	int errs = 0;
	const char *devname = "/dev/rworm0";

	while ((c = getopt(argc, argv, "f:")) !=  -1)
		switch(c) {
		case 'f':
			devname = optarg;
			break;

		case '?':
		default:
			errs++;
		}
	
	argc -= optind;
	argv += optind;

	if (errs || argc < 1)
		usage();

	if ((fd = open(devname, O_RDONLY | O_NONBLOCK, 0)) == -1)
		err(EX_NOINPUT, "open(%s)", devname);

	if (eq(argv[0], "select")) {
		struct wormio_quirk_select q;
		if (argc != 3)
			errx(EX_USAGE, "wrong params for \"select\"");
		q.vendor = argv[1];
		q.model = argv[2];
		if (ioctl(fd, WORMIOCQUIRKSELECT, &q) == -1)
			err(EX_IOERR, "ioctl(WORMIOCQUIRKSELECT)");
	}
	else if (eq(argv[0], "prepdisk")) {
		struct wormio_prepare_disk d;
		d.dummy = 0;
		d.speed = -1;
		for (i = 1; i < argc; i++) {
			if (eq(argv[i], "dummy"))
				d.dummy = 1;
			else if (eq(argv[i], "single"))
				d.speed = 1;
			else if (eq(argv[i], "double"))
				d.speed = 2;
			else
				errx(EX_USAGE,
				     "wrong param for \"prepdisk\": %s",
				     argv[i]);
		}
		if (d.speed == -1)
			errx(EX_USAGE, "missing speed parameter");
		if (ioctl(fd, WORMIOCPREPDISK, &d) == -1)
			err(EX_IOERR, "ioctl(WORMIOCPREPDISK)");
	}
	else if (eq(argv[0], "track")) {
		struct wormio_prepare_track t;		
		t.audio = -1;
		t.preemp = 0;
		for (i = 1; i < argc; i++) {
			if (eq(argv[i], "audio"))
				t.audio = 1;
			else if (eq(argv[i], "data"))
				t.audio = 0;
			else if (eq(argv[i], "preemp"))
				t.preemp = 1;
			else
				errx(EX_USAGE,
				     "wrong param for \"track\": %s",
				     argv[i]);
		}
		if (t.audio == -1)
			errx(EX_USAGE, "missing track type parameter");
		if (t.audio == 0 && t.preemp == 1)
			errx(EX_USAGE, "\"preemp\" attempted on data track");
		if (ioctl(fd, WORMIOCPREPTRACK, &t) == -1)
			err(EX_IOERR, "ioctl(WORMIOCPREPTRACK)");
	}
	else if (eq(argv[0], "fixate")) {
		struct wormio_fixation f;
		f.toc_type = -1;
		f.onp = 0;
		for (i = 1; i < argc; i++) {
			if (eq(argv[i], "onp"))
				f.onp = 1;
			else if (argv[i][0] >= '0' && argv[i][0] <= '4' &&
				 argv[i][1] == '\0')
				f.toc_type = argv[i][0] - '0';
			else
				errx(EX_USAGE,
				     "wrong param for \"fixate\": %s",
				     argv[i]);
		}
		if (f.toc_type == -1)
			errx(EX_USAGE, "missing TOC type parameter");
		if (ioctl(fd, WORMIOCFIXATION, &f) == -1)
			err(EX_IOERR, "ioctl(WORMIOFIXATION)");
	}
	else
		errx(EX_USAGE, "unknown command: %s", argv[0]);

	return EX_OK;
}
