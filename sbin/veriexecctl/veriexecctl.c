/*-
 * $FreeBSD$
 *
 * Copyright (c) 2015, Juniper Networks, Inc.
 * All rights reserved.
 *
 * Copyright (c) 1998-1999 Brett Lymn
 *                         (blymn@baea.com.au, brett_lymn@yahoo.com.au)
 * All rights reserved.
 *
 * Originally derived from:
 *	$NetBSD: veriexecctl.c,v 1.5 2004/03/06 11:57:14 blymn Exp $
 *
 * This code has been donated to The NetBSD Foundation by the Author.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <err.h>
#include <paths.h>
#include <sysexits.h>

#include <dev/veriexec/veriexec_ioctl.h>

/* globals */
int fd;
extern FILE *yyin;
int yyparse(void);

const char **algorithms;

static void
parse_algorithms(void)
{
	static char buf[BUFSIZ];
	size_t len = sizeof(buf);
	char *bufp;
	int count, indx;

	if (sysctlbyname("security.mac.veriexec.algorithms", buf, &len, NULL,
	    0) == -1)
		err(EXIT_FAILURE,
		    "Unable to determine any available algorithms");
	if (len >= sizeof(buf))
		err(EXIT_FAILURE, "Too many algorithms");

	bufp = buf;
	count = 0;
	while (strsep(&bufp, " ") != NULL)
		count++;

	algorithms = malloc(sizeof(const char *) * (count + 1));
	if (algorithms == NULL)
		err(EX_SOFTWARE, "memory allocation failed");

	indx = 0;
	for (bufp = buf; *bufp != '\0'; bufp += strlen(bufp) + 1)
		algorithms[indx++] = bufp;
}

int
main(int argc, char *argv[])
{
	int ctl = 0;
	int x = 0;
	
	if (argv[1] == NULL) {
		fprintf(stderr, "usage: veriexecctl signature_file\n");
		return (1);
	}

	fd = open(_PATH_DEV_VERIEXEC, O_WRONLY, 0);
	if (fd < 0) {
		err(EX_UNAVAILABLE, "Open of veriexec device %s failed",
		    _PATH_DEV_VERIEXEC);
	}

	if (strncmp(argv[1], "--", 2) == 0) {
		switch (argv[1][2]) {
		case 'a':		/* --active */
			ctl = VERIEXEC_ACTIVE;
			break;
		case 'd':		/* --debug* */
			ctl = (strstr(argv[1], "off")) ?
				VERIEXEC_DEBUG_OFF : VERIEXEC_DEBUG_ON;
			if (argc > 2 && ctl == VERIEXEC_DEBUG_ON) {
				x = atoi(argv[2]);
			}
			break;
		case 'e':		/* --enforce */
			ctl = VERIEXEC_ENFORCE;
			break;
		case 'l':		/* --lock */
			ctl = VERIEXEC_LOCK;
			break;
		}
		if (ctl) {
			if (ioctl(fd, ctl, &x)) {
				err(EXIT_FAILURE, "Cannot %s veriexec",
				    argv[1]);
			}
			if (ctl == VERIEXEC_DEBUG_ON) {
				printf("debug is: %d\n", x);
			}
			return (0);
		}
	} else if (strcmp(argv[1], "-")) {
		if ((yyin = fopen(argv[1], "r")) == NULL) {
			err(EXIT_FAILURE, "Opening signature file %s failed",
			    argv[1]);
		}
	}

	parse_algorithms();

	yyparse();
        return (0);
}
