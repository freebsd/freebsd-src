/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
static char copyright[] =
"@(#) Copyright (c) 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)eeprom.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * eeprom - openprom control utility
 *
 * usage:
 *
 *	eeprom [field] [field=value]
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#include <machine/openpromio.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pathnames.h"

static	char openprom[] = _PATH_OPENPROM;
static	char usage[] = "usage: %s [field] [field=value]\n";

int
main(argc, argv)
	int argc;
	char **argv;
{
	register char *cp;
	register int i, op, fd, flags, status;
	register struct opiocdesc *dp;
	struct opiocdesc desc;
	char buf[1024], buf2[sizeof(buf)];
	char *prog, *name;

	/* Determine simple program name for error messages */
	if (cp = rindex(argv[0], '/'))
		prog = cp + 1;
	else
		prog = argv[0];

	/* Parse flags */
	opterr = 0;
	while ((op = getopt(argc, argv, "")) != EOF)
		switch (op) {

		default:
			(void) fprintf(stderr, usage, prog);
			exit(1);
		}

	argc -= optind;
	argv += optind;

	/* Determine flags and open device */
	flags = O_RDONLY;
	for (i = 0; i < argc; ++i)
		if (index(argv[i], '=') != NULL) {
			flags = O_RDWR;
			break;
		}
	if ((fd = open(openprom, flags, 0)) < 0) {
		fprintf(stderr, "%s: open %s: %s\n",
		    prog, openprom, strerror(errno));
		exit(1);
	}

	dp = &desc;
	bzero(dp, sizeof(*dp));
	if (ioctl(fd, OPIOCGETOPTNODE, &dp->op_nodeid) < 0) {
		fprintf(stderr, "%s: get optionsnode: %s\n",
		    prog, strerror(errno));
		exit(1);
	}

	if (argc <= 0) {
		/* Prime the pump with a zero length name */
		dp->op_name = buf;
		dp->op_name[0] = '\0';
		dp->op_namelen = 0;
		dp->op_buf = buf2;
		for (;;) {
			/* Get the next property name */
			dp->op_buflen = sizeof(buf);
			if (ioctl(fd, OPIOCNEXTPROP, dp) < 0) {
				fprintf(stderr, "%s: get next: %s\n",
				    prog, strerror(errno));
				exit(1);
			}

			/* Zero length name means we're done */
			if (dp->op_buflen <= 0)
				break;

			/* Clever hack, swap buffers */
			cp = dp->op_buf;
			dp->op_buf = dp->op_name;
			dp->op_name = cp;
			dp->op_namelen = dp->op_buflen;

			/* Get the value */
			dp->op_buflen = sizeof(buf);
			if (ioctl(fd, OPIOCGET, dp) < 0) {
				fprintf(stderr, "%s: get \"%s\": %s\n",
				    prog, cp, strerror(errno));
				exit(1);
			}
			printf("%.*s=%.*s\n", dp->op_namelen, dp->op_name,
			    dp->op_buflen, dp->op_buf);
		}
		exit(0);
	}
	
	status = 0;
	for (i = 0; i < argc; ++i) {
		dp->op_name = name = argv[i];
		cp = index(name, '=');
		if (cp) {
			*cp++ = '\0';
			dp->op_buf = cp;
			dp->op_buflen = strlen(dp->op_buf);
		} else {
			dp->op_buf = buf;
			dp->op_buflen = sizeof(buf);
		}
		dp->op_namelen = strlen(name);
		if (ioctl(fd, cp ? OPIOCSET : OPIOCGET, dp) < 0) {
			fprintf(stderr, "%s: %s \"%s\": %s\n",
			    prog, cp ? "set" : "get", name, strerror(errno));
			status |= 1;
			continue;
		}

		/* If setting an entry, we're done */
		if (cp)
			continue;
		if (dp->op_buflen < 0) {
			fprintf(stderr, "%s: \"%s\" not found\n", prog, name);
			status |= 1;
			continue;
		}
		if (dp->op_buflen >= sizeof(buf)) {
			fprintf(stderr, "%s: \"%s\" truncated\n", prog, name);
			status |= 1;
			/* fall thorugh and print truncated value */
		}
		printf("%s=%.*s\n", name, dp->op_buflen, buf);
	}

	exit(status);
}
