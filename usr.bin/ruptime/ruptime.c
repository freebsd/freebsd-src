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
static char sccsid[] = "@(#)ruptime.c	5.8 (Berkeley) 7/21/90";
#endif /* not lint */

#include <sys/param.h>
#include <sys/dir.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <protocols/rwhod.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t	nhosts, hspace = 20;
struct hs {
	struct	whod *hs_wd;
	int	hs_nusers;
} *hs;
struct	whod awhod;

#define	ISDOWN(h)		(now - (h)->hs_wd->wd_recvtime > 11 * 60)
#define	WHDRSIZE	(sizeof (awhod) - sizeof (awhod.wd_we))

time_t now;
int rflg = 1;
int hscmp(), ucmp(), lcmp(), tcmp();

main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	register struct hs *hsp;
	register struct whod *wd;
	register struct whoent *we;
	register DIR *dirp;
	struct direct *dp;
	int aflg, cc, ch, f, i, maxloadav;
	char buf[sizeof(struct whod)];
	int (*cmp)() = hscmp;
	time_t time();
	char *interval();

	aflg = 0;
	while ((ch = getopt(argc, argv, "alrut")) != EOF)
		switch((char)ch) {
		case 'a':
			aflg = 1;
			break;
		case 'l':
			cmp = lcmp;
			break;
		case 'r':
			rflg = -1;
			break;
		case 't':
			cmp = tcmp;
			break;
		case 'u':
			cmp = ucmp;
			break;
		default: 
			(void)fprintf(stderr, "usage: ruptime [-alrut]\n");
			exit(1);
		}

	if (chdir(_PATH_RWHODIR) || (dirp = opendir(".")) == NULL) {
		(void)fprintf(stderr, "ruptime: %s: %s.\n",
		    _PATH_RWHODIR, strerror(errno));
		exit(1);
	}
	morehosts();
	hsp = hs;
	maxloadav = -1;
	while (dp = readdir(dirp)) {
		if (dp->d_ino == 0 || strncmp(dp->d_name, "whod.", 5))
			continue;
		if ((f = open(dp->d_name, O_RDONLY, 0)) < 0) {
			(void)fprintf(stderr, "ruptime: %s: %s\n",
			    dp->d_name, strerror(errno));
			continue;
		}
		cc = read(f, buf, sizeof(struct whod));
		(void)close(f);
		if (cc < WHDRSIZE)
			continue;
		if (nhosts == hspace) {
			morehosts();
			hsp = hs + nhosts;
		}
		/* NOSTRICT */
		hsp->hs_wd = malloc((size_t)WHDRSIZE);
		wd = (struct whod *)buf;
		bcopy((char *)wd, (char *)hsp->hs_wd, (size_t)WHDRSIZE);
		hsp->hs_nusers = 0;
		for (i = 0; i < 2; i++)
			if (wd->wd_loadav[i] > maxloadav)
				maxloadav = wd->wd_loadav[i];
		we = (struct whoent *)(buf+cc);
		while (--we >= wd->wd_we)
			if (aflg || we->we_idle < 3600)
				hsp->hs_nusers++;
		nhosts++;
		hsp++;
	}
	if (!nhosts) {
		(void)printf("ruptime: no hosts in %s.\n", _PATH_RWHODIR);
		exit(1);
	}
	qsort((char *)hs, nhosts, sizeof (hs[0]), cmp);
	(void)time(&now);
	for (i = 0; i < nhosts; i++) {
		hsp = &hs[i];
		if (ISDOWN(hsp)) {
			(void)printf("%-12.12s%s\n", hsp->hs_wd->wd_hostname,
			    interval(now - hsp->hs_wd->wd_recvtime, "down"));
			continue;
		}
		(void)printf(
		    "%-12.12s%s,  %4d user%s  load %*.2f, %*.2f, %*.2f\n",
		    hsp->hs_wd->wd_hostname,
		    interval((time_t)hsp->hs_wd->wd_sendtime -
			(time_t)hsp->hs_wd->wd_boottime, "  up"),
		    hsp->hs_nusers,
		    hsp->hs_nusers == 1 ? ", " : "s,",
		    maxloadav >= 1000 ? 5 : 4,
			hsp->hs_wd->wd_loadav[0] / 100.0,
		    maxloadav >= 1000 ? 5 : 4,
		        hsp->hs_wd->wd_loadav[1] / 100.0,
		    maxloadav >= 1000 ? 5 : 4,
		        hsp->hs_wd->wd_loadav[2] / 100.0);
		free((void *)hsp->hs_wd);
	}
	exit(0);
}

char *
interval(tval, updown)
	time_t tval;
	char *updown;
{
	static char resbuf[32];
	int days, hours, minutes;

	if (tval < 0 || tval > 365*24*60*60) {
		(void)sprintf(resbuf, "   %s ??:??", updown);
		return(resbuf);
	}
	minutes = (tval + 59) / 60;		/* round to minutes */
	hours = minutes / 60; minutes %= 60;
	days = hours / 24; hours %= 24;
	if (days)
		(void)sprintf(resbuf, "%s %2d+%02d:%02d",
		    updown, days, hours, minutes);
	else
		(void)sprintf(resbuf, "%s    %2d:%02d",
		    updown, hours, minutes);
	return(resbuf);
}

/* alphabetical comparison */
hscmp(a1, a2)
	void *a1, *a2;
{
	struct hs *h1 = a1, *h2 = a2;

	return(rflg * strcmp(h1->hs_wd->wd_hostname, h2->hs_wd->wd_hostname));
}

/* load average comparison */
lcmp(a1, a2)
	void *a1, *a2;
{
	register struct hs *h1 = a1, *h2 = a2;

	if (ISDOWN(h1))
		if (ISDOWN(h2))
			return(tcmp(a1, a2));
		else
			return(rflg);
	else if (ISDOWN(h2))
		return(-rflg);
	else
		return(rflg *
			(h2->hs_wd->wd_loadav[0] - h1->hs_wd->wd_loadav[0]));
}

/* number of users comparison */
ucmp(a1, a2)
	void *a1, *a2;
{
	register struct hs *h1 = a1, *h2 = a2;

	if (ISDOWN(h1))
		if (ISDOWN(h2))
			return(tcmp(a1, a2));
		else
			return(rflg);
	else if (ISDOWN(h2))
		return(-rflg);
	else
		return(rflg * (h2->hs_nusers - h1->hs_nusers));
}

/* uptime comparison */
tcmp(a1, a2)
	void *a1, *a2;
{
	register struct hs *h1 = a1, *h2 = a2;

	return(rflg * (
		(ISDOWN(h2) ? h2->hs_wd->wd_recvtime - now
			  : h2->hs_wd->wd_sendtime - h2->hs_wd->wd_boottime)
		-
		(ISDOWN(h1) ? h1->hs_wd->wd_recvtime - now
			  : h1->hs_wd->wd_sendtime - h1->hs_wd->wd_boottime)
	));
}

morehosts()
{
	hs = realloc((char *)hs, (hspace *= 2) * sizeof(*hs));
	if (hs == NULL) {
		(void)fprintf(stderr, "ruptime: %s.\n", strerror(ENOMEM));
		exit(1);
	}
}
