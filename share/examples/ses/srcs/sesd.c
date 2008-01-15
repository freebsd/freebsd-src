/* $FreeBSD: src/share/examples/ses/srcs/sesd.c,v 1.1 2000/02/29 05:44:18 mjacob Exp $ */
/*
 * Copyright (c) 2000 by Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * the GNU Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * Matthew Jacob
 * Feral Software
 * mjacob@feral.com
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include SESINC

#define	ALLSTAT (SES_ENCSTAT_UNRECOV | SES_ENCSTAT_CRITICAL | \
	SES_ENCSTAT_NONCRITICAL | SES_ENCSTAT_INFO)

/*
 * Monitor named SES devices and note (via syslog) any changes in status.
 */

int
main(a, v)
	int a;
	char **v;
{
	static char *usage =
	    "usage: %s [ -d ] [ -t pollinterval ] device [ device ]\n";
	int fd, polltime, dev, devbase, nodaemon, bpri;
	ses_encstat stat, *carray;

	if (a < 2) {
		fprintf(stderr, usage, *v);
		return (1);
	}

	devbase = 1;

	if (strcmp(v[1], "-d") == 0) {
		nodaemon = 1;
		devbase++;
	} else {
		nodaemon = 0;
	}

	if (a > 2 && strcmp(v[2], "-t") == 0) {
		devbase += 2;
		polltime = atoi(v[3]);
	} else {
		polltime = 30;
	}

	carray = malloc(a);
	if (carray == NULL) {
		perror("malloc");
		return (1);
	}
	for (dev = devbase; dev < a; dev++)
		carray[dev] = (ses_encstat) -1;

	/*
	 * Check to make sure we can open all devices
	 */
	for (dev = devbase; dev < a; dev++) {
		fd = open(v[dev], O_RDWR);
		if (fd < 0) {
			perror(v[dev]);
			return (1);
		}
		if (ioctl(fd, SESIOC_INIT, NULL) < 0) {
			fprintf(stderr, "%s: SESIOC_INIT fails- %s\n",
			    v[dev], strerror(errno));
			return (1);
		}
		(void) close(fd);
	}
	if (nodaemon == 0) {
		if (daemon(0, 0) < 0) {
			perror("daemon");
			return (1);
		}
		openlog("sesd", LOG_CONS, LOG_USER);
	} else {
		openlog("sesd", LOG_CONS|LOG_PERROR, LOG_USER);
	}

	for (;;) {
		for (dev = devbase; dev < a; dev++) {
			char buf[128];
			fd = open(v[dev], O_RDWR);
			if (fd < 0) {
				syslog(LOG_ERR, "%s: %m", v[dev]);
				continue;
			}

			/*
			 * Get the actual current enclosure status.
			 */
			if (ioctl(fd, SESIOC_GETENCSTAT, (caddr_t) &stat) < 0) {
				syslog(LOG_ERR,
				    "%s: SESIOC_GETENCSTAT- %m", v[dev]);
				(void) close(fd);
				continue;
			}
			(void) close(fd);

			if (stat == carray[dev])
				continue;

			carray[dev] = stat;
			if ((stat & ALLSTAT) == 0) {
				syslog(LOG_NOTICE,
				    "%s: Enclosure Status OK", v[dev]);
			}
			if (stat & SES_ENCSTAT_INFO) {
				syslog(LOG_INFO,
				    "%s: Enclosure Status Has Information",
				    v[dev]);
			}
			if (stat & SES_ENCSTAT_NONCRITICAL) {
				syslog(LOG_WARNING,
				    "%s: Enclosure Non-Critical", v[dev]);
			}
			if (stat & SES_ENCSTAT_CRITICAL) {
				syslog(LOG_CRIT,
				    "%s: Enclosure Critical", v[dev]);
			}
			if (stat & SES_ENCSTAT_UNRECOV) {
				syslog(LOG_ALERT,
				    "%s: Enclosure Unrecoverable", v[dev]);
			}
		}
		sleep(polltime);
	}
	/* NOTREACHED */
}
