/*-
 * Copyright (c) 1980, 1988, 1993
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
#if 0
static char sccsid[] = "@(#)optr.c	8.2 (Berkeley) 1/6/94";
#endif
static const char rcsid[] =
  "$FreeBSD: src/sbin/dump/optr.c,v 1.9.2.1 2000/07/01 07:32:37 ps Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <errno.h>
#include <fstab.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <utmp.h>

#include "dump.h"
#include "pathnames.h"

void	alarmcatch __P((/* int, int */));
int	datesort __P((const void *, const void *));
static	void sendmes __P((char *, char *));

/*
 *	Query the operator; This previously-fascist piece of code
 *	no longer requires an exact response.
 *	It is intended to protect dump aborting by inquisitive
 *	people banging on the console terminal to see what is
 *	happening which might cause dump to croak, destroying
 *	a large number of hours of work.
 *
 *	Every 2 minutes we reprint the message, alerting others
 *	that dump needs attention.
 */
static	int timeout;
static	char *attnmessage;		/* attention message */

int
query(question)
	char	*question;
{
	char	replybuffer[64];
	int	back, errcount;
	FILE	*mytty;

	if ((mytty = fopen(_PATH_TTY, "r")) == NULL)
		quit("fopen on %s fails: %s\n", _PATH_TTY, strerror(errno));
	attnmessage = question;
	timeout = 0;
	alarmcatch();
	back = -1;
	errcount = 0;
	do {
		if (fgets(replybuffer, 63, mytty) == NULL) {
			clearerr(mytty);
			if (++errcount > 30)	/* XXX	ugly */
				quit("excessive operator query failures\n");
		} else if (replybuffer[0] == 'y' || replybuffer[0] == 'Y') {
			back = 1;
		} else if (replybuffer[0] == 'n' || replybuffer[0] == 'N') {
			back = 0;
		} else {
			(void) fprintf(stderr,
			    "  DUMP: \"Yes\" or \"No\"?\n");
			(void) fprintf(stderr,
			    "  DUMP: %s: (\"yes\" or \"no\") ", question);
		}
	} while (back < 0);

	/*
	 *	Turn off the alarm, and reset the signal to trap out..
	 */
	(void) alarm(0);
	if (signal(SIGALRM, sig) == SIG_IGN)
		signal(SIGALRM, SIG_IGN);
	(void) fclose(mytty);
	return(back);
}

char lastmsg[100];

/*
 *	Alert the console operator, and enable the alarm clock to
 *	sleep for 2 minutes in case nobody comes to satisfy dump
 */
void
alarmcatch()
{
	if (notify == 0) {
		if (timeout == 0)
			(void) fprintf(stderr,
			    "  DUMP: %s: (\"yes\" or \"no\") ",
			    attnmessage);
		else
			msgtail("\7\7");
	} else {
		if (timeout) {
			msgtail("\n");
			broadcast("");		/* just print last msg */
		}
		(void) fprintf(stderr,"  DUMP: %s: (\"yes\" or \"no\") ",
		    attnmessage);
	}
	signal(SIGALRM, alarmcatch);
	(void) alarm(120);
	timeout = 1;
}

/*
 *	Here if an inquisitive operator interrupts the dump program
 */
void
interrupt(signo)
	int signo;
{
	msg("Interrupt received.\n");
	if (query("Do you want to abort dump?"))
		dumpabort(0);
}

/*
 *	The following variables and routines manage alerting
 *	operators to the status of dump.
 *	This works much like wall(1) does.
 */
struct	group *gp;

/*
 *	Get the names from the group entry "operator" to notify.
 */
void
set_operators()
{
	if (!notify)		/*not going to notify*/
		return;
	gp = getgrnam(OPGRENT);
	(void) endgrent();
	if (gp == NULL) {
		msg("No group entry for %s.\n", OPGRENT);
		notify = 0;
		return;
	}
}

struct tm *localclock;

/*
 *	We fork a child to do the actual broadcasting, so
 *	that the process control groups are not messed up
 */
void
broadcast(message)
	char	*message;
{
	time_t		clock;
	FILE	*f_utmp;
	struct	utmp	utmp;
	char	**np;
	int	pid, s;

	if (!notify || gp == NULL)
		return;

	switch (pid = fork()) {
	case -1:
		return;
	case 0:
		break;
	default:
		while (wait(&s) != pid)
			continue;
		return;
	}

	clock = time((time_t *)0);
	localclock = localtime(&clock);

	if ((f_utmp = fopen(_PATH_UTMP, "r")) == NULL) {
		msg("Cannot open %s: %s\n", _PATH_UTMP, strerror(errno));
		return;
	}

	while (!feof(f_utmp)) {
		if (fread((char *) &utmp, sizeof (struct utmp), 1, f_utmp) != 1)
			break;
		if (utmp.ut_name[0] == 0)
			continue;
		for (np = gp->gr_mem; *np; np++) {
			if (strncmp(*np, utmp.ut_name, sizeof(utmp.ut_name)) != 0)
				continue;
			/*
			 *	Do not send messages to operators on dialups
			 */
			if (strncmp(utmp.ut_line, DIALUP, strlen(DIALUP)) == 0)
				continue;
#ifdef DEBUG
			msg("Message to %s at %s\n", *np, utmp.ut_line);
#endif
			sendmes(utmp.ut_line, message);
		}
	}
	(void) fclose(f_utmp);
	Exit(0);	/* the wait in this same routine will catch this */
	/* NOTREACHED */
}

static void
sendmes(tty, message)
	char *tty, *message;
{
	char t[MAXPATHLEN], buf[BUFSIZ];
	register char *cp;
	int lmsg = 1;
	FILE *f_tty;

	(void) strcpy(t, _PATH_DEV);
	(void) strncat(t, tty, sizeof t - strlen(_PATH_DEV) - 1);

	if ((f_tty = fopen(t, "w")) != NULL) {
		setbuf(f_tty, buf);
		(void) fprintf(f_tty,
		    "\n\
\7\7\7Message from the dump program to all operators at %d:%02d ...\r\n\n\
DUMP: NEEDS ATTENTION: ",
		    localclock->tm_hour, localclock->tm_min);
		for (cp = lastmsg; ; cp++) {
			if (*cp == '\0') {
				if (lmsg) {
					cp = message;
					if (*cp == '\0')
						break;
					lmsg = 0;
				} else
					break;
			}
			if (*cp == '\n')
				(void) putc('\r', f_tty);
			(void) putc(*cp, f_tty);
		}
		(void) fclose(f_tty);
	}
}

/*
 *	print out an estimate of the amount of time left to do the dump
 */

time_t	tschedule = 0;

void
timeest()
{
	time_t	tnow, deltat;

	(void) time((time_t *) &tnow);
	if (tnow >= tschedule) {
		tschedule = tnow + 300;
		if (blockswritten < 500)
			return;
		deltat = tstart_writing - tnow +
			(1.0 * (tnow - tstart_writing))
			/ blockswritten * tapesize;
		msg("%3.2f%% done, finished in %d:%02d\n",
			(blockswritten * 100.0) / tapesize,
			deltat / 3600, (deltat % 3600) / 60);
	}
}

void
#if __STDC__
msg(const char *fmt, ...)
#else
msg(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

	(void) fprintf(stderr,"  DUMP: ");
#ifdef TDEBUG
	(void) fprintf(stderr, "pid=%d ", getpid());
#endif
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void) vfprintf(stderr, fmt, ap);
	(void) fflush(stdout);
	(void) fflush(stderr);
	(void) vsnprintf(lastmsg, sizeof(lastmsg), fmt, ap);
	va_end(ap);
}

void
#if __STDC__
msgtail(const char *fmt, ...)
#else
msgtail(fmt, va_alist)
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
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void
#if __STDC__
quit(const char *fmt, ...)
#else
quit(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

	(void) fprintf(stderr,"  DUMP: ");
#ifdef TDEBUG
	(void) fprintf(stderr, "pid=%d ", getpid());
#endif
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void) fflush(stdout);
	(void) fflush(stderr);
	dumpabort(0);
}

/*
 *	Tell the operator what has to be done;
 *	we don't actually do it
 */

struct fstab *
allocfsent(fs)
	register struct fstab *fs;
{
	register struct fstab *new;

	new = (struct fstab *)malloc(sizeof (*fs));
	if (new == NULL ||
	    (new->fs_file = strdup(fs->fs_file)) == NULL ||
	    (new->fs_type = strdup(fs->fs_type)) == NULL ||
	    (new->fs_spec = strdup(fs->fs_spec)) == NULL)
		quit("%s\n", strerror(errno));
	new->fs_passno = fs->fs_passno;
	new->fs_freq = fs->fs_freq;
	return (new);
}

struct	pfstab {
	struct	pfstab *pf_next;
	struct	fstab *pf_fstab;
};

static	struct pfstab *table;

void
getfstab()
{
	register struct fstab *fs;
	register struct pfstab *pf;

	if (setfsent() == 0) {
		msg("Can't open %s for dump table information: %s\n",
		    _PATH_FSTAB, strerror(errno));
		return;
	}
	while ((fs = getfsent()) != NULL) {
		if (strcmp(fs->fs_type, FSTAB_RW) &&
		    strcmp(fs->fs_type, FSTAB_RO) &&
		    strcmp(fs->fs_type, FSTAB_RQ))
			continue;
		fs = allocfsent(fs);
		if ((pf = (struct pfstab *)malloc(sizeof (*pf))) == NULL)
			quit("%s\n", strerror(errno));
		pf->pf_fstab = fs;
		pf->pf_next = table;
		table = pf;
	}
	(void) endfsent();
}

/*
 * Search in the fstab for a file name.
 * This file name can be either the special or the path file name.
 *
 * The file name can omit the leading '/'.
 */
struct fstab *
fstabsearch(key)
	char *key;
{
	register struct pfstab *pf;
	register struct fstab *fs;
	char *rn;

	for (pf = table; pf != NULL; pf = pf->pf_next) {
		fs = pf->pf_fstab;
		if (strcmp(fs->fs_file, key) == 0 ||
		    strcmp(fs->fs_spec, key) == 0)
			return (fs);
		rn = rawname(fs->fs_spec);
		if (rn != NULL && strcmp(rn, key) == 0)
			return (fs);
		if (key[0] != '/') {
			if (*fs->fs_spec == '/' &&
			    strcmp(fs->fs_spec + 1, key) == 0)
				return (fs);
			if (*fs->fs_file == '/' &&
			    strcmp(fs->fs_file + 1, key) == 0)
				return (fs);
		}
	}
	return (NULL);
}

/*
 *	Tell the operator what to do
 */
void
lastdump(arg)
	char	arg;	/* w ==> just what to do; W ==> most recent dumps */
{
	register int i;
	register struct fstab *dt;
	register struct dumpdates *dtwalk;
	char *lastname, *date;
	int dumpme;
	time_t tnow;
	struct tm *tlast;

	(void) time(&tnow);
	getfstab();		/* /etc/fstab input */
	initdumptimes();	/* /etc/dumpdates input */
	qsort((char *) ddatev, nddates, sizeof(struct dumpdates *), datesort);

	if (arg == 'w')
		(void) printf("Dump these file systems:\n");
	else
		(void) printf("Last dump(s) done (Dump '>' file systems):\n");
	lastname = "??";
	ITITERATE(i, dtwalk) {
		if (strncmp(lastname, dtwalk->dd_name,
		    sizeof(dtwalk->dd_name)) == 0)
			continue;
		date = (char *)ctime(&dtwalk->dd_ddate);
		date[16] = '\0';	/* blast away seconds and year */
		lastname = dtwalk->dd_name;
		dt = fstabsearch(dtwalk->dd_name);
		dumpme = (dt != NULL && dt->fs_freq != 0);
		if (dumpme) {
		    tlast = localtime(&dtwalk->dd_ddate);
		    dumpme = tnow > (dtwalk->dd_ddate - (tlast->tm_hour * 3600)
				     - (tlast->tm_min * 60) - tlast->tm_sec
				     + (dt->fs_freq * 86400));
		};
		if (arg != 'w' || dumpme)
			(void) printf(
			    "%c %8s\t(%6s) Last dump: Level %c, Date %s\n",
			    dumpme && (arg != 'w') ? '>' : ' ',
			    dtwalk->dd_name,
			    dt ? dt->fs_file : "",
			    dtwalk->dd_level,
			    date);
	}
}

int
datesort(a1, a2)
	const void *a1, *a2;
{
	struct dumpdates *d1 = *(struct dumpdates **)a1;
	struct dumpdates *d2 = *(struct dumpdates **)a2;
	int diff;

	diff = strncmp(d1->dd_name, d2->dd_name, sizeof(d1->dd_name));
	if (diff == 0)
		return (d2->dd_ddate - d1->dd_ddate);
	return (diff);
}
