/*
 * Copyright (c) 1988, 1990, 1993
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
"@(#) Copyright (c) 1988, 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)shutdown.c	8.2 (Berkeley) 2/16/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/syslog.h>

#include <ctype.h>
#include <fcntl.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"

#ifdef DEBUG
#undef _PATH_NOLOGIN
#define	_PATH_NOLOGIN	"./nologin"
#endif

#define	H		*60*60
#define	M		*60
#define	S		*1
#define	NOLOG_TIME	5*60
struct interval {
	int timeleft, timetowait;
} tlist[] = {
	10 H,  5 H,	 5 H,  3 H,	 2 H,  1 H,	1 H, 30 M,
	30 M, 10 M,	20 M, 10 M,	10 M,  5 M,	5 M,  3 M,
	 2 M,  1 M,	 1 M, 30 S,	30 S, 30 S,
	 0, 0,
};
#undef H
#undef M
#undef S

static time_t offset, shuttime;
static int dohalt, doreboot, killflg, mbuflen;
static char *nosync, *whom, mbuf[BUFSIZ];

void badtime __P((void));
void die_you_gravy_sucking_pig_dog __P((void));
void finish __P((int));
void getoffset __P((char *));
void loop __P((void));
void nolog __P((void));
void timeout __P((int));
void timewarn __P((int));
void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int optind;
	register char *p, *endp;
	struct passwd *pw;
	int arglen, ch, len, readstdin;

#ifndef DEBUG
	if (geteuid()) {
		(void)fprintf(stderr, "shutdown: NOT super-user\n");
		exit(1);
	}
#endif
	nosync = NULL;
	readstdin = 0;
	while ((ch = getopt(argc, argv, "-hknr")) != EOF)
		switch (ch) {
		case '-':
			readstdin = 1;
			break;
		case 'h':
			dohalt = 1;
			break;
		case 'k':
			killflg = 1;
			break;
		case 'n':
			nosync = "-n";
			break;
		case 'r':
			doreboot = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	if (doreboot && dohalt) {
		(void)fprintf(stderr,
		    "shutdown: incompatible switches -h and -r.\n");
		usage();
	}
	getoffset(*argv++);

	if (*argv) {
		for (p = mbuf, len = sizeof(mbuf); *argv; ++argv) {
			arglen = strlen(*argv);
			if ((len -= arglen) <= 2)
				break;
			if (p != mbuf)
				*p++ = ' ';
			bcopy(*argv, p, arglen);
			p += arglen;
		}
		*p = '\n';
		*++p = '\0';
	}

	if (readstdin) {
		p = mbuf;
		endp = mbuf + sizeof(mbuf) - 2;
		for (;;) {
			if (!fgets(p, endp - p + 1, stdin))
				break;
			for (; *p &&  p < endp; ++p);
			if (p == endp) {
				*p = '\n';
				*++p = '\0';
				break;
			}
		}
	}
	mbuflen = strlen(mbuf);

	if (offset)
		(void)printf("Shutdown at %.24s.\n", ctime(&shuttime));
	else
		(void)printf("Shutdown NOW!\n");

	if (!(whom = getlogin()))
		whom = (pw = getpwuid(getuid())) ? pw->pw_name : "???";

#ifdef DEBUG
	(void)putc('\n', stdout);
#else
	(void)setpriority(PRIO_PROCESS, 0, PRIO_MIN);
	{
		int forkpid;

		forkpid = fork();
		if (forkpid == -1) {
			perror("shutdown: fork");
			exit(1);
		}
		if (forkpid) {
			(void)printf("shutdown: [pid %d]\n", forkpid);
			exit(0);
		}
	}
#endif
	openlog("shutdown", LOG_CONS, LOG_AUTH);
	loop();
	/* NOTREACHED */
}

void
loop()
{
	struct interval *tp;
	u_int sltime;
	int logged;

	if (offset <= NOLOG_TIME) {
		logged = 1;
		nolog();
	}
	else
		logged = 0;
	tp = tlist;
	if (tp->timeleft < offset)
		(void)sleep((u_int)(offset - tp->timeleft));
	else {
		while (offset < tp->timeleft)
			++tp;
		/*
		 * Warn now, if going to sleep more than a fifth of
		 * the next wait time.
		 */
		if (sltime = offset - tp->timeleft) {
			if (sltime > tp->timetowait / 5)
				timewarn(offset);
			(void)sleep(sltime);
		}
	}
	for (;; ++tp) {
		timewarn(tp->timeleft);
		if (!logged && tp->timeleft <= NOLOG_TIME) {
			logged = 1;
			nolog();
		}
		(void)sleep((u_int)tp->timetowait);
		if (!tp->timeleft)
			break;
	}
	die_you_gravy_sucking_pig_dog();
}

static jmp_buf alarmbuf;

void
timewarn(timeleft)
	int timeleft;
{
	static int first;
	static char hostname[MAXHOSTNAMELEN + 1];
	FILE *pf;
	char wcmd[MAXPATHLEN + 4];

	if (!first++)
		(void)gethostname(hostname, sizeof(hostname));

	/* undoc -n option to wall suppresses normal wall banner */
	(void)snprintf(wcmd, sizeof(wcmd), "%s -n", _PATH_WALL);
	if (!(pf = popen(wcmd, "w"))) {
		syslog(LOG_ERR, "shutdown: can't find %s: %m", _PATH_WALL);
		return;
	}

	(void)fprintf(pf,
	    "\007*** %sSystem shutdown message from %s@%s ***\007\n",
	    timeleft ? "": "FINAL ", whom, hostname);

	if (timeleft > 10*60)
		(void)fprintf(pf, "System going down at %5.5s\n\n",
		    ctime(&shuttime) + 11);
	else if (timeleft > 59)
		(void)fprintf(pf, "System going down in %d minute%s\n\n",
		    timeleft / 60, (timeleft > 60) ? "s" : "");
	else if (timeleft)
		(void)fprintf(pf, "System going down in 30 seconds\n\n");
	else
		(void)fprintf(pf, "System going down IMMEDIATELY\n\n");

	if (mbuflen)
		(void)fwrite(mbuf, sizeof(*mbuf), mbuflen, pf);

	/*
	 * play some games, just in case wall doesn't come back
	 * probably unecessary, given that wall is careful.
	 */
	if (!setjmp(alarmbuf)) {
		(void)signal(SIGALRM, timeout);
		(void)alarm((u_int)30);
		(void)pclose(pf);
		(void)alarm((u_int)0);
		(void)signal(SIGALRM, SIG_DFL);
	}
}

void
timeout(signo)
	int signo;
{
	longjmp(alarmbuf, 1);
}

void
die_you_gravy_sucking_pig_dog()
{

	syslog(LOG_NOTICE, "%s by %s: %s",
	    doreboot ? "reboot" : dohalt ? "halt" : "shutdown", whom, mbuf);
	(void)sleep(2);

	(void)printf("\r\nSystem shutdown time has arrived\007\007\r\n");
	if (killflg) {
		(void)printf("\rbut you'll have to do it yourself\r\n");
		exit(0);
	}
#ifdef DEBUG
	if (doreboot)
		(void)printf("reboot");
	else if (dohalt)
		(void)printf("halt");
	if (nosync)
		(void)printf(" no sync");
	(void)printf("\nkill -HUP 1\n");
#else
	if (doreboot) {
		execle(_PATH_REBOOT, "reboot", "-l", nosync, 0);
		syslog(LOG_ERR, "shutdown: can't exec %s: %m.", _PATH_REBOOT);
		perror("shutdown");
	}
	else if (dohalt) {
		execle(_PATH_HALT, "halt", "-l", nosync, 0);
		syslog(LOG_ERR, "shutdown: can't exec %s: %m.", _PATH_HALT);
		perror("shutdown");
	}
	(void)kill(1, SIGTERM);		/* to single user */
#endif
	finish(0);
}

#define	ATOI2(p)	(p[0] - '0') * 10 + (p[1] - '0'); p += 2;

void
getoffset(timearg)
	register char *timearg;
{
	register struct tm *lt;
	register char *p;
	time_t now;

	if (!strcasecmp(timearg, "now")) {		/* now */
		offset = 0;
		return;
	}

	(void)time(&now);
	if (*timearg == '+') {				/* +minutes */
		if (!isdigit(*++timearg))
			badtime();
		offset = atoi(timearg) * 60;
		shuttime = now + offset;
		return;
	}

	/* handle hh:mm by getting rid of the colon */
	for (p = timearg; *p; ++p)
		if (!isascii(*p) || !isdigit(*p))
			if (*p == ':' && strlen(p) == 3) {
				p[0] = p[1];
				p[1] = p[2];
				p[2] = '\0';
			}
			else
				badtime();

	unsetenv("TZ");					/* OUR timezone */
	lt = localtime(&now);				/* current time val */

	switch(strlen(timearg)) {
	case 10:
		lt->tm_year = ATOI2(timearg);
		/* FALLTHROUGH */
	case 8:
		lt->tm_mon = ATOI2(timearg);
		if (--lt->tm_mon < 0 || lt->tm_mon > 11)
			badtime();
		/* FALLTHROUGH */
	case 6:
		lt->tm_mday = ATOI2(timearg);
		if (lt->tm_mday < 1 || lt->tm_mday > 31)
			badtime();
		/* FALLTHROUGH */
	case 4:
		lt->tm_hour = ATOI2(timearg);
		if (lt->tm_hour < 0 || lt->tm_hour > 23)
			badtime();
		lt->tm_min = ATOI2(timearg);
		if (lt->tm_min < 0 || lt->tm_min > 59)
			badtime();
		lt->tm_sec = 0;
		if ((shuttime = mktime(lt)) == -1)
			badtime();
		if ((offset = shuttime - now) < 0) {
			(void)fprintf(stderr,
			    "shutdown: that time is already past.\n");
			exit(1);
		}
		break;
	default:
		badtime();
	}
}

#define	NOMSG	"\n\nNO LOGINS: System going down at "
void
nolog()
{
	int logfd;
	char *ct;

	(void)unlink(_PATH_NOLOGIN);	/* in case linked to another file */
	(void)signal(SIGINT, finish);
	(void)signal(SIGHUP, finish);
	(void)signal(SIGQUIT, finish);
	(void)signal(SIGTERM, finish);
	if ((logfd = open(_PATH_NOLOGIN, O_WRONLY|O_CREAT|O_TRUNC,
	    0664)) >= 0) {
		(void)write(logfd, NOMSG, sizeof(NOMSG) - 1);
		ct = ctime(&shuttime);
		(void)write(logfd, ct + 11, 5);
		(void)write(logfd, "\n\n", 2);
		(void)write(logfd, mbuf, strlen(mbuf));
		(void)close(logfd);
	}
}

void
finish(signo)
	int signo;
{
	(void)unlink(_PATH_NOLOGIN);
	exit(0);
}

void
badtime()
{
	(void)fprintf(stderr, "shutdown: bad time format.\n");
	exit(1);
}

void
usage()
{
	fprintf(stderr, "usage: shutdown [-hknr] shutdowntime [ message ]\n");
	exit(1);
}
