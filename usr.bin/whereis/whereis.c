/*	$NetBSD: whereis.c,v 1.11 2002/06/11 06:06:21 itojun Exp $	*/

/*-
 * Copyright (c) 1993
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

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)whereis.c	8.3 (Berkeley) 5/4/95";
#endif
__RCSID("$NetBSD: whereis.c,v 1.11 2002/06/11 06:06:21 itojun Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void usage __P((void));
int main __P((int, char *[]));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct stat sb;
	size_t len;
	int ch, sverrno, mib[2];
	char *p, *t, *std, path[MAXPATHLEN];
	int useenvpath = 0;

	while ((ch = getopt(argc, argv, "p")) != -1)
		switch (ch) {
		case 'p':
			useenvpath = 1;	/* use environment for PATH */
			break;

		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

 	if (useenvpath) {
 		if ((std = getenv("PATH")) == NULL)
 			err(1, "getenv: PATH" );
	} else {
		/* Retrieve the standard path. */
		mib[0] = CTL_USER;
		mib[1] = USER_CS_PATH;
		if (sysctl(mib, 2, NULL, &len, NULL, 0) == -1)
			return (-1);
		if (len == 0)
			err(1, "user_cs_path: sysctl: zero length");
		if ((std = malloc(len)) == NULL)
			err(1, NULL);
		if (sysctl(mib, 2, std, &len, NULL, 0) == -1) {
			sverrno = errno;
			free(std);
			errno = sverrno;
			err(1, "sysctl: user_cs_path");
		}
	}

	/* For each path, for each program... */
	for (; *argv; ++argv)
		for (p = std;; *p++ = ':') {
			t = p;
			if ((p = strchr(p, ':')) != NULL) {
				*p = '\0';
				if (t == p)
					t = ".";
			} else
				if (strlen(t) == 0)
					t = ".";
			(void)snprintf(path, sizeof(path), "%s/%s", t, *argv);
			if (!stat(path, &sb))
				(void)printf("%s\n", path);
			if (p == NULL)
				break;
		}

	return (0);
}

void
usage()
{

	(void)fprintf(stderr, "usage: whereis [-p] program [...]\n");
	exit (1);
}
