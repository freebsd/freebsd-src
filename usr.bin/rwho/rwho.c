/*
 * Copyright (c) 1983, 1993
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
static const char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)rwho.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <protocols/rwhod.h>
#include <dirent.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

DIR	*dirp;

struct	whod wd;
int	utmpcmp();
#define	NUSERS	1000
struct	myutmp {
	char    myhost[sizeof(wd.wd_hostname)];
	int	myidle;
	struct	outmp myutmp;
} myutmp[NUSERS];
int	nusers;

#define	WHDRSIZE	(sizeof (wd) - sizeof (wd.wd_we))
/*
 * this macro should be shared with ruptime.
 */
#define	down(w,now)	((now) - (w)->wd_recvtime > 11 * 60)

time_t	now;
int	aflg;

static void usage __P((void));
int utmpcmp __P((struct myutmp *, struct myutmp *));

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch;
	struct dirent *dp;
	int cc, width;
	register struct whod *w = &wd;
	register struct whoent *we;
	register struct myutmp *mp;
	int f, n, i;

	(void) setlocale(LC_TIME, "");

	while ((ch = getopt(argc, argv, "a")) != -1)
		switch((char)ch) {
		case 'a':
			aflg = 1;
			break;
		case '?':
		default:
			usage();
		}
	if (chdir(_PATH_RWHODIR) || (dirp = opendir(".")) == NULL)
		err(1, "%s", _PATH_RWHODIR);
	mp = myutmp;
	(void)time(&now);
	while ((dp = readdir(dirp))) {
		if (dp->d_ino == 0 || strncmp(dp->d_name, "whod.", 5))
			continue;
		f = open(dp->d_name, O_RDONLY);
		if (f < 0)
			continue;
		cc = read(f, (char *)&wd, sizeof (struct whod));
		if (cc < WHDRSIZE) {
			(void) close(f);
			continue;
		}
		if (down(w,now)) {
			(void) close(f);
			continue;
		}
		cc -= WHDRSIZE;
		we = w->wd_we;
		for (n = cc / sizeof (struct whoent); n > 0; n--) {
			if (aflg == 0 && we->we_idle >= 60*60) {
				we++;
				continue;
			}
			if (nusers >= NUSERS)
				errx(1, "too many users");
			mp->myutmp = we->we_utmp; mp->myidle = we->we_idle;
			(void) strcpy(mp->myhost, w->wd_hostname);
			nusers++; we++; mp++;
		}
		(void) close(f);
	}
	qsort((char *)myutmp, nusers, sizeof (struct myutmp), utmpcmp);
	mp = myutmp;
	width = 0;
	for (i = 0; i < nusers; i++) {
		/* append one for the blank and use 8 for the out_line */
		int j = strlen(mp->myhost) + 1 + sizeof(mp->myutmp.out_line);
		if (j > width)
			width = j;
		mp++;
	}
	mp = myutmp;
	for (i = 0; i < nusers; i++) {
		char buf[BUFSIZ], cbuf[80];
		strftime(cbuf, sizeof(cbuf), "%c", localtime((time_t *)&mp->myutmp.out_time));
		(void)sprintf(buf, "%s:%-.*s", mp->myhost,
		   sizeof(mp->myutmp.out_line), mp->myutmp.out_line);
		printf("%-*.*s %-*s %.12s",
		   UT_NAMESIZE, sizeof(mp->myutmp.out_name),
		   mp->myutmp.out_name,
		   width,
		   buf,
		   cbuf + 4);
		mp->myidle /= 60;
		if (mp->myidle) {
			if (aflg) {
				if (mp->myidle >= 100*60)
					mp->myidle = 100*60 - 1;
				if (mp->myidle >= 60)
					printf(" %2d", mp->myidle / 60);
				else
					printf("   ");
			} else
				printf(" ");
			printf(":%02d", mp->myidle % 60);
		}
		printf("\n");
		mp++;
	}
	exit(0);
}


static void
usage()
{
	fprintf(stderr, "usage: rwho [-a]\n");
	exit(1);
}

int
utmpcmp(u1, u2)
	struct myutmp *u1, *u2;
{
	int rc;

	rc = strncmp(u1->myutmp.out_name, u2->myutmp.out_name, sizeof(u2->myutmp.out_name));
	if (rc)
		return (rc);
	rc = strcmp(u1->myhost, u2->myhost);
	if (rc)
		return (rc);
	return (strncmp(u1->myutmp.out_line, u2->myutmp.out_line, sizeof(u2->myutmp.out_line)));
}
