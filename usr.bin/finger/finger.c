/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Tony Nardo of the Johns Hopkins University/Applied Physics Lab.
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
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)finger.c	5.22 (Berkeley) 6/29/90";
#endif /* not lint */

/*
 * Finger prints out information about users.  It is not portable since
 * certain fields (e.g. the full user name, office, and phone numbers) are
 * extracted from the gecos field of the passwd file which other UNIXes
 * may not have or may use for other things.
 *
 * There are currently two output formats; the short format is one line
 * per user and displays login name, tty, login time, real name, idle time,
 * and office location/phone number.  The long format gives the same
 * information (in a more legible format) as well as home directory, shell,
 * mail info, and .plan/.project files.
 */

#include <sys/param.h>
#include <sys/file.h>
#include <stdio.h>
#include "finger.h"

time_t now;
int lflag, sflag, mflag, pplan;
char tbuf[1024];

main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	int ch;
	time_t time();

	while ((ch = getopt(argc, argv, "lmps")) != EOF)
		switch(ch) {
		case 'l':
			lflag = 1;		/* long format */
			break;
		case 'm':
			mflag = 1;		/* force exact match of names */
			break;
		case 'p':
			pplan = 1;		/* don't show .plan/.project */
			break;
		case 's':
			sflag = 1;		/* short format */
			break;
		case '?':
		default:
			(void)fprintf(stderr,
			    "usage: finger [-lmps] [login ...]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	(void)time(&now);
	setpassent(1);
	if (!*argv) {
		/*
		 * Assign explicit "small" format if no names given and -l
		 * not selected.  Force the -s BEFORE we get names so proper
		 * screening will be done.
		 */
		if (!lflag)
			sflag = 1;	/* if -l not explicit, force -s */
		loginlist();
		if (entries == 0)
			(void)printf("No one logged on.\n");
	} else {
		userlist(argc, argv);
		/*
		 * Assign explicit "large" format if names given and -s not
		 * explicitly stated.  Force the -l AFTER we get names so any
		 * remote finger attempts specified won't be mishandled.
		 */
		if (!sflag)
			lflag = 1;	/* if -s not explicit, force -l */
	}
	if (entries != 0) {
		if (lflag)
			lflag_print();
		else
			sflag_print();
	}
	exit(0);
}

loginlist()
{
	register PERSON *pn;
	struct passwd *pw;
	struct utmp user;
	char name[UT_NAMESIZE + 1];

	if (!freopen(_PATH_UTMP, "r", stdin)) {
		(void)fprintf(stderr, "finger: can't read %s.\n", _PATH_UTMP);
		exit(2);
	}
	name[UT_NAMESIZE] = NULL;
	while (fread((char *)&user, sizeof(user), 1, stdin) == 1) {
		if (!user.ut_name[0])
			continue;
		if ((pn = find_person(user.ut_name)) == NULL) {
			bcopy(user.ut_name, name, UT_NAMESIZE);
			if ((pw = getpwnam(name)) == NULL)
				continue;
			pn = enter_person(pw);
		}
		enter_where(&user, pn);
	}
	for (pn = phead; lflag && pn != NULL; pn = pn->next)
		enter_lastlog(pn);
}

userlist(argc, argv)
	register argc;
	register char **argv;
{
	register i;
	register PERSON *pn;
	PERSON *nethead, **nettail;
	struct utmp user;
	struct passwd *pw;
	int dolocal, *used;
	char *index();

	if (!(used = (int *)calloc((u_int)argc, (u_int)sizeof(int)))) {
		(void)fprintf(stderr, "finger: out of space.\n");
		exit(1);
	}

	/* pull out all network requests */
	for (i = 0, dolocal = 0, nettail = &nethead; i < argc; i++) {
		if (!index(argv[i], '@')) {
			dolocal = 1;
			continue;
		}
		pn = palloc();
		*nettail = pn;
		nettail = &pn->next;
		pn->name = argv[i];
		used[i] = -1;
	}
	*nettail = NULL;

	if (!dolocal)
		goto net;

	/*
	 * traverse the list of possible login names and check the login name
	 * and real name against the name specified by the user.
	 */
	if (mflag) {
		for (i = 0; i < argc; i++)
			if (used[i] >= 0 && (pw = getpwnam(argv[i]))) {
				enter_person(pw);
				used[i] = 1;
			}
	} else while (pw = getpwent())
		for (i = 0; i < argc; i++)
			if (used[i] >= 0 &&
			    (!strcasecmp(pw->pw_name, argv[i]) ||
			    match(pw, argv[i]))) {
				enter_person(pw);
				used[i] = 1;
			}

	/* list errors */
	for (i = 0; i < argc; i++)
		if (!used[i])
			(void)fprintf(stderr,
			    "finger: %s: no such user.\n", argv[i]);

	/* handle network requests */
net:	for (pn = nethead; pn; pn = pn->next) {
		netfinger(pn->name);
		if (pn->next || entries)
			putchar('\n');
	}

	if (entries == 0)
		return;

	/*
	 * Scan thru the list of users currently logged in, saving
	 * appropriate data whenever a match occurs.
	 */
	if (!freopen(_PATH_UTMP, "r", stdin)) {
		(void)fprintf( stderr, "finger: can't read %s.\n", _PATH_UTMP);
		exit(1);
	}
	while (fread((char *)&user, sizeof(user), 1, stdin) == 1) {
		if (!user.ut_name[0])
			continue;
		if ((pn = find_person(user.ut_name)) == NULL)
			continue;
		enter_where(&user, pn);
	}
	for (pn = phead; pn != NULL; pn = pn->next)
		enter_lastlog(pn);
}
