/*
 * Copyright (c) 1990 Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1990 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mkfifo.c	5.3 (Berkeley) 6/1/90";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void *setmode();
extern mode_t getmode();

main(argc, argv)
	int argc;
	char **argv;
{
	int ch, exitval;
	void * set;
	mode_t mode;

	/* The default mode is the value of the bitwise inclusive or of
	   S_IRUSR, S_IWUSR, S_IRGRP, S_IWGRP, S_IROTH, and S_IWOTH
	   modified by the file creation mask */
	mode = 0666 & ~umask(0);

	while ((ch = getopt(argc, argv, "m:")) != EOF)
		switch(ch) {
		case 'm':
			if (!(set = setmode(optarg))) {
				(void)fprintf(stderr, "mkfifo: invalid file mode.\n");
				exit(1);
			}
			/* In symbolic mode strings, the + and - operators are
			   interpreted relative to an assumed initial mode of
			   a=rw. */
			mode = getmode (set, 0666);
			break;
		case '?':
		default:
			usage();
		}

	if (!*(argv += optind))
		usage();

	for (exitval = 0; *argv; ++argv) {
		if (mkfifo(*argv, mode) < 0) {
			(void)fprintf(stderr, "mkfifo: %s: %s\n",
			    *argv, strerror(errno));
			exitval = 1;
		}
	}
	exit(exitval);
}

usage()
{
	(void)fprintf(stderr, "usage: mkfifo [-m mode] fifoname ...\n");
	exit(1);
}
