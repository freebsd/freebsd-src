/*	$NetBSD: usbd.c,v 1.4 1998/12/09 00:57:19 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@netbsd.org).
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#if defined(__FreeBSD__)
#include <sys/ioctl.h>
#endif
#include <dev/usb/usb.h>

/* the name of the device spitting out usb attach/detach events as well as
 * the prefix for the individual busses (used as a semi kernel thread).
 */
#define USBDEV "/dev/usb"

/* Maximum number of USB busses expected to be in a system
 */
#define MAXUSBDEV 4

/*
 * Sometimes a device does not respond in time for interrupt
 * driven explore to find it.  Therefore we run an exploration
 * at regular intervals to catch those.
 */
#define TIMEOUT 30


extern char *__progname;

void usage(void);

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-d] [-e] [-f dev] [-t timeout] [-v]\n",
		__progname);
	fprintf(stderr, "  -d         for debugging\n");
	fprintf(stderr, "  -e         only do 1 explore\n");
	fprintf(stderr, "  -f dev     for example /dev/usb0, "
		"and can be specified multiple times.");
	fprintf(stderr, "  -t timeout timeout between explores\n");
	fprintf(stderr, "  -v         verbose output\n");

	exit(1);
}

int
main(int argc, char **argv)
{
	int error, i;
	int ch;			/* getopt option */
	extern char *optarg;	/* from getopt */
	extern int optind;	/* from getopt */
	char *devs[MAXUSBDEV];	/* device names */
	int ndevs = 0;		/* number of devices found */
	int verbose = 0;	/* print message on what it is doing */
	int debug = 0;		/* print debugging output */
	int explore = 0;	/* don't do only explore */
	int maxfd;		/* maximum fd in use */
	char buf[50];		/* for creation of the filename */
	int fds[MAXUSBDEV];	/* open filedescriptors */
	fd_set fdset;
	int itimo = TIMEOUT;	/* timeout for select */
	struct timeval timo;

	while ((ch = getopt(argc, argv, "def:t:v")) != -1) {
		switch(ch) {
		case 'd':
			debug++;
			break;
		case 'e':
			explore++;
			break;
		case 'f':
			if (ndevs < MAXUSBDEV)
				devs[ndevs++] = optarg;
			break;
		case 't':
			itimo = atoi(optarg);
			break;
		case 'v':
			verbose++;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* open all the files /dev/usb\d+ or specified with -f */
	maxfd = 0;
	if (ndevs == 0) {
		for (i = 0; i < MAXUSBDEV; i++) {
			sprintf(buf, "%s%d", USBDEV, i);
			fds[ndevs] = open(buf, O_RDWR);
			if (fds[ndevs] >= 0) {
				devs[ndevs] = strdup(buf);
				if (verbose)
					printf("%s: opening %s\n", 
					       __progname, devs[ndevs]);
				if (fds[ndevs] > maxfd)
					maxfd = fds[ndevs];
				ndevs++;
			}
		}
	} else {
		for (i = 0; i < ndevs; i++) {
			fds[i] = open(devs[i], O_RDWR);
			if (fds[i] < 0)
				err(1, "%s", devs[i]);
			else if (fds[i] > maxfd)
				maxfd = fds[i];
		}
	}
	if (ndevs == 0) {
		fprintf(stderr, "No USB host controllers found\n");
		exit(1);
	}


	/* Do the explore once and exit */
	if (explore) {
		for (i = 0; i < ndevs; i++) {
			error = ioctl(fds[i], USB_DISCOVER);
			if (error < 0)
				err(1, "USB_DISCOVER");
		}
		exit(0);
	}

	/* move to the background */
	if (!debug)
		daemon(0, 0);

	/* start select on all the open file descriptors */
	for (;;) {
		FD_ZERO(&fdset);
		for (i = 0; i < ndevs; i++)
			FD_SET(fds[i], &fdset);
		timo.tv_usec = 0;
		timo.tv_sec = itimo;
		error = select(maxfd+1, 0, &fdset, 0, itimo ? &timo : 0);
		if (error < 0)
			warn("select failed\n");
		for (i = 0; i < ndevs; i++)
			if (error == 0 || FD_ISSET(fds[i], &fdset)) {
				if (verbose)
					printf("%s: doing %sdiscovery on %s\n", 
					       __progname,
					       (error? "":"timeout "), devs[i]);
				if (ioctl(fds[i], USB_DISCOVER) < 0)
					err(1, "USB_DISCOVER");
			}
	}
}
