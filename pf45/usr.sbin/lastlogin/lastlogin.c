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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>

	int	main(int, char **);
static	void	output(struct utmpx *);
static	void	usage(void);

static int
utcmp(const void *u1, const void *u2)
{

	return (strcmp(((const struct utmpx *)u1)->ut_user,
	    ((const struct utmpx *)u2)->ut_user));
}

int
main(int argc, char *argv[])
{
	int	ch, i, ulistsize;
	struct utmpx *u, *ulist;

	while ((ch = getopt(argc, argv, "")) != -1) {
		usage();
	}

	/* Process usernames given on the command line. */
	if (argc > 1) {
		for (i = 1; i < argc; ++i) {
			if (setutxdb(UTXDB_LASTLOGIN, NULL) != 0)
				errx(1, "failed to open lastlog database");
			if ((u = getutxuser(argv[i])) == NULL) {
				warnx("user '%s' not found", argv[i]);
				continue;
			}
			output(u);
			endutxent();
		}
	}
	/* Read all lastlog entries, looking for active ones */
	else {
		if (setutxdb(UTXDB_LASTLOGIN, NULL) != 0)
			errx(1, "failed to open lastlog database");
		ulist = NULL;
		ulistsize = 0;
		while ((u = getutxent()) != NULL) {
			if (u->ut_type != USER_PROCESS)
				continue;
			if ((ulistsize % 16) == 0) {
				ulist = realloc(ulist,
				    (ulistsize + 16) * sizeof(struct utmpx));
				if (ulist == NULL)
					err(1, "malloc");
			}
			ulist[ulistsize++] = *u;
		}
		endutxent();

		qsort(ulist, ulistsize, sizeof(struct utmpx), utcmp);
		for (i = 0; i < ulistsize; i++)
			output(&ulist[i]);
	}

	exit(0);
}

/* Duplicate the output of last(1) */
static void
output(struct utmpx *u)
{
	time_t t = u->ut_tv.tv_sec;

	printf("%-10s %-8s %-22.22s %s",
		u->ut_user, u->ut_line, u->ut_host, ctime(&t));
}

static void
usage(void)
{
	fprintf(stderr, "usage: lastlogin [user ...]\n");
	exit(1);
}
