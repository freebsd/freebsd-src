/*
 * Copyright (c) 1987, 1993
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
"@(#) Copyright (c) 1987, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)dm.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <ctype.h>
#include <nlist.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#include "pathnames.h"

extern int errno;
static time_t	now;			/* current time value */
static int	priority = 0;		/* priority game runs at */
static char	*game,			/* requested game */
		*gametty;		/* from tty? */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	char *cp;

	nogamefile();
	game = (cp = rindex(*argv, '/')) ? ++cp : *argv;

	if (!strcmp(game, "dm"))
		exit(0);

	gametty = ttyname(0);
	(void)time(&now);
	read_config();
#ifdef LOG
	logfile();
#endif
	play(argv);
	/*NOTREACHED*/
}

/*
 * play --
 *	play the game
 */
play(args)
	char **args;
{
	char pbuf[MAXPATHLEN];

	(void)strcpy(pbuf, _PATH_HIDE);
	(void)strcpy(pbuf + sizeof(_PATH_HIDE) - 1, game);
	if (priority > 0)	/* < 0 requires root */
		(void)setpriority(PRIO_PROCESS, 0, priority);
	setgid(getgid());	/* we run setgid kmem; lose it */
	execv(pbuf, args);
	(void)fprintf(stderr, "dm: %s: %s\n", pbuf, strerror(errno));
	exit(1);
}

/*
 * read_config --
 *	read through config file, looking for key words.
 */
read_config()
{
	FILE *cfp;
	char lbuf[BUFSIZ], f1[40], f2[40], f3[40], f4[40], f5[40];

	if (!(cfp = fopen(_PATH_CONFIG, "r")))
		return;
	while (fgets(lbuf, sizeof(lbuf), cfp))
		switch(*lbuf) {
		case 'b':		/* badtty */
			if (sscanf(lbuf, "%s%s", f1, f2) != 2 ||
			    strcasecmp(f1, "badtty"))
				break;
			c_tty(f2);
			break;
		case 'g':		/* game */
			if (sscanf(lbuf, "%s%s%s%s%s",
			    f1, f2, f3, f4, f5) != 5 || strcasecmp(f1, "game"))
				break;
			c_game(f2, f3, f4, f5);
			break;
		case 't':		/* time */
			if (sscanf(lbuf, "%s%s%s%s", f1, f2, f3, f4) != 4 ||
			    strcasecmp(f1, "time"))
				break;
			c_day(f2, f3, f4);
		}
	(void)fclose(cfp);
}

/*
 * c_day --
 *	if day is today, see if okay to play
 */
c_day(s_day, s_start, s_stop)
	char *s_day, *s_start, *s_stop;
{
	static char *days[] = {
		"sunday", "monday", "tuesday", "wednesday",
		"thursday", "friday", "saturday",
	};
	static struct tm *ct;
	int start, stop;

	if (!ct)
		ct = localtime(&now);
	if (strcasecmp(s_day, days[ct->tm_wday]))
		return;
	if (!isdigit(*s_start) || !isdigit(*s_stop))
		return;
	start = atoi(s_start);
	stop = atoi(s_stop);
	if (ct->tm_hour >= start && ct->tm_hour < stop) {
		fputs("dm: Sorry, games are not available from ", stderr);
		hour(start);
		fputs(" to ", stderr);
		hour(stop);
		fputs(" today.\n", stderr);
		exit(0);
	}
}

/*
 * c_tty --
 *	decide if this tty can be used for games.
 */
c_tty(tty)
	char *tty;
{
	static int first = 1;
	static char *p_tty;

	if (first) {
		p_tty = rindex(gametty, '/');
		first = 0;
	}

	if (!strcmp(gametty, tty) || p_tty && !strcmp(p_tty, tty)) {
		fprintf(stderr, "dm: Sorry, you may not play games on %s.\n", gametty);
		exit(0);
	}
}

/*
 * c_game --
 *	see if game can be played now.
 */
c_game(s_game, s_load, s_users, s_priority)
	char *s_game, *s_load, *s_users, *s_priority;
{
	static int found;
	double load();

	if (found)
		return;
	if (strcmp(game, s_game) && strcasecmp("default", s_game))
		return;
	++found;
	if (isdigit(*s_load) && atoi(s_load) < load()) {
		fputs("dm: Sorry, the load average is too high right now.\n", stderr);
		exit(0);
	}
	if (isdigit(*s_users) && atoi(s_users) <= users()) {
		fputs("dm: Sorry, there are too many users logged on right now.\n", stderr);
		exit(0);
	}
	if (isdigit(*s_priority))
		priority = atoi(s_priority);
}

/*
 * load --
 *	return 15 minute load average
 */
double
load()
{
	double avenrun[3];

	if (getloadavg(avenrun, sizeof(avenrun)/sizeof(avenrun[0])) < 0) {
		fputs("dm: getloadavg() failed.\n", stderr);
		exit(1);
	}
	return(avenrun[2]);
}

/*
 * users --
 *	return current number of users
 *	todo: check idle time; if idle more than X minutes, don't
 *	count them.
 */
users()
{
	
	register int nusers, utmp;
	struct utmp buf;

	if ((utmp = open(_PATH_UTMP, O_RDONLY, 0)) < 0) {
		(void)fprintf(stderr, "dm: %s: %s\n",
		    _PATH_UTMP, strerror(errno));
		exit(1);
	}
	for (nusers = 0; read(utmp, (char *)&buf, sizeof(struct utmp)) > 0;)
		if (buf.ut_name[0] != '\0')
			++nusers;
	return(nusers);
}

nogamefile()
{
	register int fd, n;
	char buf[BUFSIZ];

	if ((fd = open(_PATH_NOGAMES, O_RDONLY, 0)) >= 0) {
#define	MESG	"Sorry, no games right now.\n\n"
		(void)write(2, MESG, sizeof(MESG) - 1);
		while ((n = read(fd, buf, sizeof(buf))) > 0)
			(void)write(2, buf, n);
		exit(1);
	}
}

/*
 * hour --
 *	print out the hour in human form
 */
hour(h)
	int h;
{
	switch(h) {
	case 0:
		fputs("midnight", stderr);
		break;
	case 12:
		fputs("noon", stderr);
		break;
	default:
		if (h > 12)
			fprintf(stderr, "%dpm", h - 12);
		else
			fprintf(stderr, "%dam", h);
	}
}

#ifdef LOG
/*
 * logfile --
 *	log play of game
 */
logfile()
{
	struct passwd *pw;
	FILE *lp;
	uid_t uid;
	int lock_cnt;

	if (lp = fopen(_PATH_LOG, "a")) {
		for (lock_cnt = 0;; ++lock_cnt) {
			if (!flock(fileno(lp), LOCK_EX))
				break;
			if (lock_cnt == 4) {
				perror("dm: log lock");
				(void)fclose(lp);
				return;
			}
			sleep((u_int)1);
		}
		if (pw = getpwuid(uid = getuid()))
			fputs(pw->pw_name, lp);
		else
			fprintf(lp, "%u", uid);
		fprintf(lp, "\t%s\t%s\t%s", game, gametty, ctime(&now));
		(void)fclose(lp);
		(void)flock(fileno(lp), LOCK_UN);
	}
}
#endif /* LOG */
