/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
"@(#) Copyright (c) 1991 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)id.c	5.1 (Berkeley) 6/29/91";
#endif /* not lint */

#include <sys/param.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct passwd PW;
typedef struct group GR;

void	current __P((void));
void	err __P((const char *, ...));
int	gcmp __P((const void *, const void *));
void	sgroup __P((PW *));
void	ugroup __P((PW *));
void	usage __P((void));
void	user __P((PW *));
PW     *who __P((char *));

int Gflag, gflag, nflag, rflag, uflag;

main(argc, argv)
	int argc;
	char *argv[];
{
	GR *gr;
	PW *pw;
	int ch, id;

	while ((ch = getopt(argc, argv, "Ggnru")) != EOF)
		switch(ch) {
		case 'G':
			Gflag = 1;
			break;
		case 'g':
			gflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 'u':
			uflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	pw = *argv ? who(*argv) : NULL;

	if (Gflag + gflag + uflag > 1)
		usage();

	if (Gflag) {
		if (nflag)
			sgroup(pw);
		else
			ugroup(pw);
		exit(0);
	}

	if (gflag) {
		id = pw ? pw->pw_gid : rflag ? getgid() : getegid();
		if (nflag && (gr = getgrgid(id))) {
			(void)printf("%s\n", gr->gr_name);
			exit(0);
		}
		(void)printf("%u\n", id);
		exit(0);
	}

	if (uflag) {
		id = pw ? pw->pw_uid : rflag ? getuid() : geteuid();
		if (nflag && (pw = getpwuid(id))) {
			(void)printf("%s\n", pw->pw_name);
			exit(0);
		}
		(void)printf("%u\n", id);
		exit(0);
	}

	if (pw)
		user(pw);
	else
		current();
	exit(0);
}

void
sgroup(pw)
	PW *pw;
{
	register int id, lastid;
	char *fmt;

	if (pw) {
		register GR *gr;
		register char *name, **p;

		name = pw->pw_name;
		for (fmt = "%s", lastid = -1; gr = getgrent(); lastid = id) {
			for (p = gr->gr_mem; p && *p; p++)
				if (!strcmp(*p, name)) {
					(void)printf(fmt, gr->gr_name);
					fmt = " %s";
					break;
				}
		}
	} else {
		GR *gr;
		register int ngroups;
		int groups[NGROUPS + 1];

		groups[0] = getgid();
		ngroups = getgroups(NGROUPS, groups + 1) + 1;
		heapsort(groups, ngroups, sizeof(groups[0]), gcmp);
		for (fmt = "%s", lastid = -1; --ngroups >= 0;) {
			if (lastid == (id = groups[ngroups]))
				continue;
			if (gr = getgrgid(id))
				(void)printf(fmt, gr->gr_name);
			else
				(void)printf(*fmt == ' ' ? " %u" : "%u", id);
			fmt = " %s";
			lastid = id;
		}
	}
	(void)printf("\n");
}

void
ugroup(pw)
	PW *pw;
{
	register int id, lastid;
	register char *fmt;

	if (pw) {
		register GR *gr;
		register char *name, **p;

		name = pw->pw_name;
		for (fmt = "%u", lastid = -1; gr = getgrent(); lastid = id) {
			for (p = gr->gr_mem; p && *p; p++)
				if (!strcmp(*p, name)) {
					(void)printf(fmt, gr->gr_gid);
					fmt = " %u";
					break;
				}
		}
	} else {
		register int ngroups;
		int groups[NGROUPS + 1];

		groups[0] = getgid();
		ngroups = getgroups(NGROUPS, groups + 1) + 1;
		heapsort(groups, ngroups, sizeof(groups[0]), gcmp);
		for (fmt = "%u", lastid = -1; --ngroups >= 0;) {
			if (lastid == (id = groups[ngroups]))
				continue;
			(void)printf(fmt, id);
			fmt = " %u";
			lastid = id;
		}
	}
	(void)printf("\n");
}

void
current()
{
	GR *gr;
	PW *pw;
	int id, eid, lastid, ngroups, groups[NGROUPS];
	char *fmt;

	id = getuid();
	(void)printf("uid=%u", id);
	if (pw = getpwuid(id))
		(void)printf("(%s)", pw->pw_name);
	if ((eid = geteuid()) != id) {
		(void)printf(" euid=%u", eid);
		if (pw = getpwuid(eid))
			(void)printf("(%s)", pw->pw_name);
	}
	id = getgid();
	(void)printf(" gid=%u", id);
	if (gr = getgrgid(id))
		(void)printf("(%s)", gr->gr_name);
	if ((eid = getegid()) != id) {
		(void)printf(" egid=%u", eid);
		if (gr = getgrgid(eid))
			(void)printf("(%s)", gr->gr_name);
	}
	if (ngroups = getgroups(NGROUPS, groups)) {
		heapsort(groups, ngroups, sizeof(groups[0]), gcmp);
		for (fmt = " groups=%u", lastid = -1; --ngroups >= 0;
		    fmt = ", %u", lastid = id) {
			id = groups[ngroups];
			if (lastid == id)
				continue;
			(void)printf(fmt, id);
			if (gr = getgrgid(id))
				(void)printf("(%s)", gr->gr_name);
		}
	}
	(void)printf("\n");
}

void
user(pw)
	register PW *pw;
{
	register GR *gr;
	register int id, lastid;
	register char *fmt, **p;

	id = pw->pw_uid;
	(void)printf("uid=%u(%s)", id, pw->pw_name);
	(void)printf(" gid=%u", pw->pw_gid);
	if (gr = getgrgid(id))
		(void)printf("(%s)", gr->gr_name);
	for (fmt = " groups=%u(%s)", lastid = -1; gr = getgrent();
	    lastid = id) {
		if (pw->pw_gid == gr->gr_gid)
			continue;
		for (p = gr->gr_mem; p && *p; p++)
			if (!strcmp(*p, pw->pw_name)) {
				(void)printf(fmt, gr->gr_gid, gr->gr_name);
				fmt = ", %u(%s)";
				break;
			}
	}
	(void)printf("\n");
}

PW *
who(u)
	char *u;
{
	PW *pw;
	long id;
	char *ep;

	/*
	 * Translate user argument into a pw pointer.  First, try to
	 * get it as specified.  If that fails, try it as a number.
	 */
	if (pw = getpwnam(u))
		return(pw);
	id = strtol(u, &ep, 10);
	if (*u && !*ep && (pw = getpwuid(id)))
		return(pw);
	err("%s: No such user", u);
	/* NOTREACHED */
}

gcmp(a, b)
	const void *a, *b;
{
	return(*(int *)b - *(int *)a);
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#if __STDC__
err(const char *fmt, ...)
#else
err(fmt, va_alist)
	char *fmt;
        va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "id: ");
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit(1);
	/* NOTREACHED */
}

void
usage()
{
	(void)fprintf(stderr, "usage: id [user]\n");
	(void)fprintf(stderr, "       id -G [-n] [user]\n");
	(void)fprintf(stderr, "       id -g [-nr] [user]\n");
	(void)fprintf(stderr, "       id -u [-nr] [user]\n");
	exit(1);
}
