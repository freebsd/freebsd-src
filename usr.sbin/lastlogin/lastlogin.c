/* $FreeBSD$ */
/*	$NetBSD: lastlogin.c,v 1.4 1998/02/03 04:45:35 perry Exp $	*/
/*
 * Copyright (c) 1996 John M. Vinopal
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
 *	This product includes software developed for the NetBSD Project
 *	by John M. Vinopal.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$FreeBSD$");
__RCSID("$NetBSD: lastlogin.c,v 1.4 1998/02/03 04:45:35 perry Exp $");
#endif

#include <sys/types.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <utmp.h>
#include <unistd.h>

extern	char *__progname;
static	const char *logfile = _PATH_LASTLOG;

	int	main __P((int, char **));
static	void	output __P((struct passwd *, struct lastlog *));
static	void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int	ch, i;
	FILE	*fp;
	struct passwd	*passwd;
	struct lastlog	last;

	while ((ch = getopt(argc, argv, "")) != -1) {
		usage();
	}

	fp = fopen(logfile, "r");
	if (fp == NULL)
		err(1, "%s", logfile);

	setpassent(1);	/* Keep passwd file pointers open */

	/* Process usernames given on the command line. */
	if (argc > 1) {
		long offset;
		for (i = 1; i < argc; ++i) {
			if ((passwd = getpwnam(argv[i])) == NULL) {
				warnx("user '%s' not found", argv[i]);
				continue;
			}
			/* Calculate the offset into the lastlog file. */
			offset = (long)(passwd->pw_uid * sizeof(last));
			if (fseek(fp, offset, SEEK_SET)) {
				warn("fseek error");
				continue;
			}
			if (fread(&last, sizeof(last), 1, fp) != 1) {
				warnx("fread error on '%s'", passwd->pw_name);
				clearerr(fp);
				continue;
			}
			output(passwd, &last);
		}
	}
	/* Read all lastlog entries, looking for active ones */
	else {
		for (i = 0; fread(&last, sizeof(last), 1, fp) == 1; i++) {
			if (last.ll_time == 0)
				continue;
			if ((passwd = getpwuid((uid_t)i)) != NULL)
				output(passwd, &last);
		}
		if (ferror(fp))
			warnx("fread error");
	}

	setpassent(0);	/* Close passwd file pointers */

	fclose(fp);
	exit(0);
}

/* Duplicate the output of last(1) */
static void
output(p, l)
	struct passwd *p;
	struct lastlog *l;
{
	printf("%-*.*s  %-*.*s %-*.*s   %s",
		UT_NAMESIZE, UT_NAMESIZE, p->pw_name,
		UT_LINESIZE, UT_LINESIZE, l->ll_line,
		UT_HOSTSIZE, UT_HOSTSIZE, l->ll_host,
		(l->ll_time) ? ctime(&(l->ll_time)) : "Never logged in\n");
}

static void
usage()
{
	fprintf(stderr, "usage: %s [user ...]\n", __progname);
	exit(1);
}
