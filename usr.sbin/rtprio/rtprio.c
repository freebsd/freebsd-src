/*
 * Copyright (c) 1994 David Greenman
 * Copyright (c) 1994 Henrik Vestergaard Draboel (hvd@terry.ping.dk)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Henrik Vestergaard Draboel.
 *	This product includes software developed by David Greenman.
 * 4. Neither the names of the authors nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/rtprio.h>
#include <sys/errno.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage();

int
main(argc, argv)
	int     argc;
	char  **argv;
{
	char   *p;
	int     proc = 0;
	struct rtprio rtp;

	/* find basename */
	if ((p = rindex(argv[0], '/')) == NULL)
		p = argv[0];
	else
		++p;

	if (!strcmp(p, "rtprio"))
		rtp.type = RTP_PRIO_REALTIME;
	else if (!strcmp(p, "idprio"))
		rtp.type = RTP_PRIO_IDLE;

	switch (argc) {
	case 2:
		proc = abs(atoi(argv[1]));	/* Should check if numeric
						 * arg! */
		/* FALLTHROUGH */
	case 1:
		if (rtprio(RTP_LOOKUP, proc, &rtp) != 0)
			err(1, "%s", argv[0]);
		printf("%s: ", p);
		switch (rtp.type) {
		case RTP_PRIO_REALTIME:
		case RTP_PRIO_FIFO:
			printf("realtime priority %d\n", rtp.prio);
			break;
		case RTP_PRIO_NORMAL:
			printf("normal priority\n");
			break;
		case RTP_PRIO_IDLE:
			printf("idle priority %d\n", rtp.prio);
			break;
		default:
			printf("invalid priority type %d\n", rtp.type);
			break;
		}
		exit(0);
	default:
		if (argv[1][0] == '-' || isdigit(argv[1][0])) {
			if (argv[1][0] == '-') {
				if (strcmp(argv[1], "-t") == 0) {
					rtp.type = RTP_PRIO_NORMAL;
					rtp.prio = 0;
				} else {
					usage();
					break;
				}
			} else {
				rtp.prio = atoi(argv[1]);
			}
		} else {
			usage();
			break;
		}

		if (argv[2][0] == '-')
			proc = -atoi(argv[2]);

		if (rtprio(RTP_SET, proc, &rtp) != 0)
			err(1, "%s", argv[0]);

		if (proc == 0) {
			execvp(argv[2], &argv[2]);
			err(1, "%s", argv[2]);
		}
		exit(0);
	}
	exit (1);
}

static void
usage()
{
	(void) fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n%s\n",
		"usage: [id|rt]prio",
		"       [id|rt]prio [-]pid",
		"       [id|rt]prio priority command [args]",
		"       [id|rt]prio priority -pid",
		"       [id|rt]prio -t command [args]",
		"       [id|rt]prio -t -pid");
	exit(1);
}
