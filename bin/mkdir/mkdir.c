/*
 * Copyright (c) 1983 Regents of the University of California.
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
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mkdir.c	5.7 (Berkeley) 5/31/90";
static char rcsid[] = "$Header: /a/cvs/386BSD/src/bin/mkdir/mkdir.c,v 1.2 1993/07/21 22:54:09 conklin Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern int errno;
extern void *setmode();
extern mode_t getmode();

main(argc, argv)
	int argc;
	char **argv;
{
	int ch, exitval, pflag;
	void *set;
	mode_t mode, dir_mode;

	/* default file mode is a=rwx (777) with selected permissions
	   removed in accordance with the file mode creation mask.
	   For intermediate path name components, the mode is the default
	   modified by u+wx so that the subdirectories can always be 
	   created. */
	mode = 0777 & ~umask(0);
	dir_mode = mode | S_IWUSR | S_IXUSR;

	pflag = 0;
	while ((ch = getopt(argc, argv, "pm:")) != EOF)
		switch(ch) {
		case 'p':
			pflag = 1;
			break;
		case 'm':
			if ((set = setmode(optarg)) == NULL) {
				(void)fprintf(stderr,
					"mkdir: invalid file mode.\n");
				exit(1);
			}
			mode = getmode (set, S_IRWXU | S_IRWXG | S_IRWXO);
			break;
		case '?':
		default:
			usage();
		}

	if (!*(argv += optind))
		usage();
	
	for (exitval = 0; *argv; ++argv) {
		if (pflag)
			exitval |= build(*argv, mode, dir_mode);
		else if (mkdir(*argv, mode) < 0) {
			(void)fprintf(stderr, "mkdir: %s: %s\n",
			    *argv, strerror(errno));
			exitval = 1;
		}
	}
	exit(exitval);
}

/*
 * build -- create directories.  
 *	mode     - file mode of terminal directory
 *	dir_mode - file mode of intermediate directories
 */
build(path, mode, dir_mode)
	char *path;
	mode_t mode;
	mode_t dir_mode;
{
	register char *p;
	struct stat sb;
	int ch;

	for (p = path;; ++p) {
		if (!*p || *p  == '/') {
			ch = *p;
			*p = '\0';
			if (stat(path, &sb)) {
				if (errno != ENOENT || mkdir(path, (ch) ? dir_mode : mode) < 0) {
					(void)fprintf(stderr, "mkdir: %s: %s\n",
					    path, strerror(errno));
					return(1);
				}
			}
			if (!(*p = ch))
				break;
		}
	}
	return(0);
}

usage()
{
	(void)fprintf(stderr, "usage: mkdir [-p] [-m mode] dirname ...\n");
	exit(1);
}
