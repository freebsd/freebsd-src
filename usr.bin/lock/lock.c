/*
 * Copyright (c) 1980, 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Bob Toxen.
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
"@(#) Copyright (c) 1980, 1987, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)lock.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * Lock a terminal up until the given key is entered, until the root
 * password is entered, or the given interval times out.
 *
 * Timeout interval is by default TIMEOUT, it can be changed with
 * an argument of the form -time where time is in minutes
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <err.h>
#include <ctype.h>
#include <pwd.h>
#include <sgtty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <varargs.h>

#define	TIMEOUT	15

void quit(int);
void bye(int);
void hi(int);
static void usage(void);

struct timeval	timeout;
struct timeval	zerotime;
struct sgttyb	tty, ntty;
long	nexttime;			/* keep the timeout time */
int            no_timeout;                     /* lock terminal forever */

/*ARGSUSED*/
int
main(argc, argv)
	int argc;
	char **argv;
{
	struct passwd *pw;
	struct timeval timval;
	time_t timval_sec;
	struct itimerval ntimer, otimer;
	struct tm *timp;
	int ch, failures, sectimeout, usemine;
	char *ap, *mypw, *ttynam, *tzn;
	char hostname[MAXHOSTNAMELEN], s[BUFSIZ], s1[BUFSIZ];

	openlog("lock", LOG_ODELAY, LOG_AUTH);

	sectimeout = TIMEOUT;
	mypw = NULL;
	usemine = 0;
       no_timeout = 0;
       while ((ch = getopt(argc, argv, "npt:")) != -1)
		switch((char)ch) {
		case 't':
			if ((sectimeout = atoi(optarg)) <= 0)
				errx(1, "illegal timeout value");
			break;
		case 'p':
			usemine = 1;
			if (!(pw = getpwuid(getuid())))
				errx(1, "unknown uid %d", getuid());
			mypw = strdup(pw->pw_passwd);
			break;
               case 'n':
                       no_timeout = 1;
                       break;
		case '?':
		default:
			usage();
	}
	timeout.tv_sec = sectimeout * 60;

	setuid(getuid());		/* discard privs */

	if (ioctl(0, TIOCGETP, &tty))	/* get information for header */
		exit(1);
	gethostname(hostname, sizeof(hostname));
	if (!(ttynam = ttyname(0)))
		errx(1, "not a terminal?");
	if (gettimeofday(&timval, (struct timezone *)NULL))
		err(1, "gettimeofday");
	nexttime = timval.tv_sec + (sectimeout * 60);
	timval_sec = timval.tv_sec;
	timp = localtime(&timval_sec);
	ap = asctime(timp);
	tzn = timp->tm_zone;

	(void)signal(SIGINT, quit);
	(void)signal(SIGQUIT, quit);
	ntty = tty; ntty.sg_flags &= ~ECHO;
	(void)ioctl(0, TIOCSETP, &ntty);

	if (!mypw) {
		/* get key and check again */
		(void)printf("Key: ");
		if (!fgets(s, sizeof(s), stdin) || *s == '\n')
			quit(0);
		(void)printf("\nAgain: ");
		/*
		 * Don't need EOF test here, if we get EOF, then s1 != s
		 * and the right things will happen.
		 */
		(void)fgets(s1, sizeof(s1), stdin);
		(void)putchar('\n');
		if (strcmp(s1, s)) {
			(void)printf("\07lock: passwords didn't match.\n");
			ioctl(0, TIOCSETP, &tty);
			exit(1);
		}
		s[0] = '\0';
		mypw = s1;
	}

	/* set signal handlers */
	(void)signal(SIGINT, hi);
	(void)signal(SIGQUIT, hi);
	(void)signal(SIGTSTP, hi);
	(void)signal(SIGALRM, bye);

	ntimer.it_interval = zerotime;
	ntimer.it_value = timeout;
       if (!no_timeout)
               setitimer(ITIMER_REAL, &ntimer, &otimer);

	/* header info */
       if (no_timeout) {
(void)printf("lock: %s on %s. no timeout\ntime now is %.20s%s%s",
           ttynam, hostname, ap, tzn, ap + 19);
       } else {
(void)printf("lock: %s on %s. timeout in %d minutes\ntime now is %.20s%s%s",
	    ttynam, hostname, sectimeout, ap, tzn, ap + 19);
       }
       failures = 0;

	for (;;) {
		(void)printf("Key: ");
		if (!fgets(s, sizeof(s), stdin)) {
			clearerr(stdin);
			hi(0);
			continue;
		}
		if (usemine) {
			s[strlen(s) - 1] = '\0';
			if (!strcmp(mypw, crypt(s, mypw)))
				break;
		}
		else if (!strcmp(s, s1))
			break;
		(void)printf("\07\n");
	    	failures++;
		if (getuid() == 0)
	    	    syslog(LOG_NOTICE, "%d ROOT UNLOCK FAILURE%s (%s on %s)",
			failures, failures > 1 ? "S": "", ttynam, hostname);
		if (ioctl(0, TIOCGETP, &ntty))
			exit(1);
		sleep(1);		/* to discourage guessing */
	}
	if (getuid() == 0)
	    syslog(LOG_NOTICE, "ROOT UNLOCK ON hostname %s port %s",
		   hostname, ttynam);
	quit(0);
	return(0); /* not reached */
}


static void
usage()
{
	(void)fprintf(stderr, "usage: lock [-n] [-p] [-t timeout]\n");
	exit(1);
}

void
hi(int signo __unused)
{
	struct timeval timval;

       if (!gettimeofday(&timval, (struct timezone *)NULL)) {
               (void)printf("lock: type in the unlock key. ");
               if (no_timeout) {
                       (void)putchar('\n');
               } else {
                       (void)printf("timeout in %ld:%ld minutes\n",
                               (nexttime - timval.tv_sec) / 60,
                               (nexttime - timval.tv_sec) % 60);
               }
       }
}

void
quit(int signo __unused)
{
	(void)putchar('\n');
	(void)ioctl(0, TIOCSETP, &tty);
	exit(0);
}

void
bye(int signo __unused)
{
       if (!no_timeout) {
               (void)ioctl(0, TIOCSETP, &tty);
               (void)printf("lock: timeout\n");
               exit(1);
       }
}
