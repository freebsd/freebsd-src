/*
 * Copyright (c) 1983 The Regents of the University of California.
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
"@(#) Copyright (c) 1983 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rmdir.c	5.3 (Berkeley) 5/31/90";
#endif /* not lint */

/*
 * Remove directory
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

main(argc, argv)
	int argc;
	char **argv;
{
	int errors;
	int ch;
	int delete_parent_directories = 0;

	while ((ch = getopt (argc, argv, "p")) != EOF) {
		switch (ch) {
		case 'p':
			delete_parent_directories = 1;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (!*(argv += optind)) {
		usage ();
		/* NOTREACHED */
	}

	for (errors = 0; *argv; argv++) {
		if (!delete_parent_directories) {
			if (rmdir(*argv) < 0) {
				fprintf(stderr, "rmdir: %s: %s\n", 
					*argv, strerror(errno));
				errors = 1;
			} 
		} else {
			if (rmdirp(*argv) < 0) {
				errors = 1;
			}
		}
	}

	exit(errors);
}

int
rmdirp (char *path)
{
	char *slash;

	/* point slash at last slash */
	slash = strrchr (path, '/');

	while (slash != NULL) {
		if (rmdir (path) < 0) {
			fprintf(stderr, "rmdir: %s: %s\n", 
				path, strerror(errno));
			return -1;
		}

		/* skip trailing slash characters */
		while (slash > path && *slash == '/')
			slash--;

		*++slash = '\0';
		slash = strrchr (path, '/');
	}

	if (rmdir (path) < 0) {
		fprintf(stderr, "rmdir: %s: %s\n", path, strerror(errno));
		return -1;
	}

	return 0;
}

usage()
{
	fprintf(stderr, "usage: rmdir [-p] directory ...\n");
	exit(1);
}
