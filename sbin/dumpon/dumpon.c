/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "From: @(#)swapon.c	8.1 (Berkeley) 6/5/93";*/
static const char rcsid[] =
	"$FreeBSD$";
#endif /* not lint */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <err.h>

void	usage __P((void)) __dead2;
static char *whoami;

int
main(int argc, char **argv)
{
	extern char *optarg;
	extern int optind;
	int ch, verbose, rv;
	struct stat stab;
	int mib[2];

	verbose = rv = 0;
	whoami = argv[0];
	while ((ch = getopt(argc, argv, "v")) != EOF)
		switch((char)ch) {
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;

	if (!argv[0] || argv[1])
		usage();

	if (strcmp(argv[0], "off")) {
		rv = stat(argv[0], &stab);
		if (rv) {
			err(EX_OSFILE, "%s", argv[0]);
		}

		if (!S_ISBLK(stab.st_mode)) {
			errx(EX_USAGE, "%s: must specify a block device",
			     argv[0]);
		}
	} else {
		stab.st_rdev = NODEV;
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_DUMPDEV;

	rv = sysctl(mib, 2, (void *)0, (size_t *)0, &stab.st_rdev,
		    sizeof stab.st_rdev);
	if (rv) {
		err(EX_OSERR, "sysctl: kern.dumpdev");
	}

	if (verbose) {
		if (stab.st_rdev == NODEV) {
			printf("%s: crash dumps disabled\n", whoami);
		} else {
			printf("%s: crash dumps to %s (%lu, %lu)\n",
			       whoami, argv[0],
			       (unsigned long)major(stab.st_rdev),
			       (unsigned long)minor(stab.st_rdev));
		}
	}

	return 0;
}

void
usage()
{
	fprintf(stderr,
		"usage: %s [-v] special_file\n"
		"       %s [-v] off\n", whoami, whoami);
	exit(EX_USAGE);
}
