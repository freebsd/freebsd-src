/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
"@(#) Copyright (c) 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)kvm_mkdb.c	5.11 (Berkeley) 4/27/91";*/
static char rcsid[] = "$Id";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <db.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <paths.h>

char *tmp;
#define basename(cp)	((tmp=rindex((cp), '/')) ? tmp+1 : (cp))

main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	DB *db;
	int ch;
	char *nlistpath, *nlistname, dbtemp[MAXPATHLEN], dbname[MAXPATHLEN];

	while ((ch = getopt(argc, argv, "")) != EOF)
		switch((char)ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	nlistpath = argc > 0 ? argv[0] : _PATH_UNIX;
	nlistname = basename(nlistpath);

	(void)sprintf(dbtemp, "%s/kvm_%s.tmp", _PATH_VARRUN, nlistname);
	(void)sprintf(dbname, "%s/kvm_%s.db", _PATH_VARRUN, nlistname);
	(void)umask(0);
	db = dbopen(dbtemp, O_CREAT|O_RDWR|O_EXCL,
	    S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH, DB_HASH, NULL);
	if (!db) {
		(void)fprintf(stderr,
		    "kvm_mkdb: %s: %s\n", dbtemp, strerror(errno));
		exit(1);
	}
	create_knlist(nlistpath, db);
	(void)(db->close)(db);
	if (rename(dbtemp, dbname)) {
		(void)fprintf(stderr, "kvm_mkdb: %s to %s: %s.\n",
		    dbtemp, dbname, strerror(errno));
		exit(1);
	}
	exit(0);
}

error(n)
	char *n;
{
	int sverr;

	sverr = errno;
	(void)fprintf(stderr, "kvm_mkdb: ");
	if (n)
		(void)fprintf(stderr, "%s: ", n);
	(void)fprintf(stderr, "%s\n", strerror(sverr));
	exit(1);
}

usage()
{
	(void)fprintf(stderr, "usage: kvm_mkdb [file]\n");
	exit(1);
}
