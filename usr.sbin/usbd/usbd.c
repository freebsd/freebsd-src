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
#include <sys/malloc.h>
#endif
#include <dev/usb/usb.h>

#define USBDEV "/dev/usb"
#define MAXUSBDEV 4

extern char *__progname;

void usage(void);

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-d] [-e] [-f dev] [-t timeout] [-v]\n",
		__progname);
	exit(1);
}

#define NDEVS 20		/* maximum number of usb controllers */

/*
 * Sometimes a device does not respond in time for interrupt
 * driven explore to find it.  Therefore we run an exploration
 * at regular intervals to catch those.
 */
#define TIMEOUT 300

int
main(int argc, char **argv)
{
	int r, i;
	char *devs[NDEVS];
	int ndevs = 0;
	int fds[NDEVS];
	fd_set fdset;
	int ch, verbose = 0;
	int debug = 0;
	int explore = 0;
	int itimo = TIMEOUT;
	int maxfd;
	char buf[50];
	struct timeval timo;
	extern char *optarg;
	extern int optind;

	while ((ch = getopt(argc, argv, "def:t:v")) != -1) {
		switch(ch) {
		case 'd':
			debug++;
			break;
		case 'e':
			explore++;
			break;
		case 'f':
			if (ndevs < NDEVS)
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
		if (verbose)
			printf("%s: no USB controllers found\n", __progname);
		exit(0);
	}

	if (explore) {
		for (i = 0; i < ndevs; i++) {
			r = ioctl(fds[i], USB_DISCOVER);
			if (r < 0)
				err(1, "USB_DISCOVER");
		}
		exit(0);
	}

	if (!debug)
		daemon(0, 0);

	for (;;) {
		FD_ZERO(&fdset);
		for (i = 0; i < ndevs; i++)
			FD_SET(fds[i], &fdset);
		timo.tv_usec = 0;
		timo.tv_sec = itimo;
		r = select(maxfd+1, 0, &fdset, 0, itimo ? &timo : 0);
		if (r < 0)
			warn("select failed\n");
		for (i = 0; i < ndevs; i++)
			if (r == 0 || FD_ISSET(fds[i], &fdset)) {
				if (verbose)
					printf("%s: doing %sdiscovery on %s\n", 
					       __progname, r ? "" : "timeout ",
					       devs[i]);
				if (ioctl(fds[i], USB_DISCOVER) < 0)
					err(1, "USB_DISCOVER");
			}
	}
}
