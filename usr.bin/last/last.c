/*
 * Copyright (c) 1987, 1993, 1994
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
"@(#) Copyright (c) 1987, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)last.c	8.2 (Berkeley) 4/2/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <locale.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>
#include <sys/queue.h>

#define	NO	0				/* false/no */
#define	YES	1				/* true/yes */

static struct utmp	buf[1024];		/* utmp read buffer */

typedef struct arg {
	char	*name;				/* argument */
#define	HOST_TYPE	-2
#define	TTY_TYPE	-3
#define	USER_TYPE	-4
	int	type;				/* type of arg */
	struct arg	*next;			/* linked list pointer */
} ARG;
ARG	*arglist;				/* head of linked list */

LIST_HEAD(ttylisthead, ttytab) ttylist;

struct ttytab {
	long	logout;				/* log out time */
	char	tty[UT_LINESIZE + 1];		/* terminal name */
	LIST_ENTRY(ttytab) list;
};

static long	currentout,			/* current logout value */
		maxrec;				/* records to display */
static char	*file = _PATH_WTMP;		/* wtmp file */

void	 addarg __P((int, char *));
void	 hostconv __P((char *));
void	 onintr __P((int));
char	*ttyconv __P((char *));
int	 want __P((struct utmp *));
void	 wtmp __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int optind;
	extern char *optarg;
	int ch;
	char *p;

	(void) setlocale(LC_TIME, "");

	maxrec = -1;
	while ((ch = getopt(argc, argv, "0123456789f:h:t:")) != -1)
		switch (ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/*
			 * kludge: last was originally designed to take
			 * a number after a dash.
			 */
			if (maxrec == -1) {
				p = argv[optind - 1];
				if (p[0] == '-' && p[1] == ch && !p[2])
					maxrec = atol(++p);
				else
					maxrec = atol(argv[optind] + 1);
				if (!maxrec)
					exit(0);
			}
			break;
		case 'f':
			file = optarg;
			break;
		case 'h':
			hostconv(optarg);
			addarg(HOST_TYPE, optarg);
			break;
		case 't':
			addarg(TTY_TYPE, ttyconv(optarg));
			break;
		case '?':
		default:
			(void)fprintf(stderr,
	"usage: last [-#] [-f file] [-t tty] [-h hostname] [user ...]\n");
			exit(1);
		}

	if (argc) {
		setlinebuf(stdout);
		for (argv += optind; *argv; ++argv) {
#define	COMPATIBILITY
#ifdef	COMPATIBILITY
			/* code to allow "last p5" to work */
			addarg(TTY_TYPE, ttyconv(*argv));
#endif
			addarg(USER_TYPE, *argv);
		}
	}
	wtmp();
	exit(0);
}

/*
 * wtmp --
 *	read through the wtmp file
 */
void
wtmp()
{
	struct utmp	*bp;			/* current structure */
	struct ttytab	*tt, *ttx;		/* ttylist entry */
	struct stat	stb;			/* stat of file for size */
	long	bl, delta;			/* time difference */
	int	bytes, wfd;
	char    *crmsg;
	char ct[80];
	struct tm *tm;

	LIST_INIT(&ttylist);

	if ((wfd = open(file, O_RDONLY, 0)) < 0 || fstat(wfd, &stb) == -1)
		err(1, "%s", file);
	bl = (stb.st_size + sizeof(buf) - 1) / sizeof(buf);

	(void)time(&buf[0].ut_time);
	(void)signal(SIGINT, onintr);
	(void)signal(SIGQUIT, onintr);

	while (--bl >= 0) {
		if (lseek(wfd, (off_t)(bl * sizeof(buf)), L_SET) == -1 ||
		    (bytes = read(wfd, buf, sizeof(buf))) == -1)
			err(1, "%s", file);
		for (bp = &buf[bytes / sizeof(buf[0]) - 1]; bp >= buf; --bp) {
			/*
			 * if the terminal line is '~', the machine stopped.
			 * see utmp(5) for more info.
			 */
			if (bp->ut_line[0] == '~' && !bp->ut_line[1]) {
				/* everybody just logged out */
				for (tt = ttylist.lh_first; tt;) {
					LIST_REMOVE(tt, list);
					ttx = tt;
					tt = tt->list.le_next;
					free(ttx);
				}
				currentout = -bp->ut_time;
				crmsg = strncmp(bp->ut_name, "shutdown",
				    UT_NAMESIZE) ? "crash" : "shutdown";
				if (want(bp)) {
					tm = localtime(&bp->ut_time);
					(void) strftime(ct, sizeof(ct), "%c", tm);
					printf("%-*.*s %-*.*s %-*.*s %10.10s %5.5s \n",
					    UT_NAMESIZE, UT_NAMESIZE,
					    bp->ut_name, UT_LINESIZE,
					    UT_LINESIZE, bp->ut_line,
					    UT_HOSTSIZE, UT_HOSTSIZE,
					    bp->ut_host, ct, ct + 11);
					if (maxrec != -1 && !--maxrec)
						return;
				}
				continue;
			}
			/*
			 * if the line is '{' or '|', date got set; see
			 * utmp(5) for more info.
			 */
			if ((bp->ut_line[0] == '{' || bp->ut_line[0] == '|')
			    && !bp->ut_line[1]) {
				if (want(bp)) {
					tm = localtime(&bp->ut_time);
					(void) strftime(ct, sizeof(ct), "%c", tm);
					printf("%-*.*s %-*.*s %-*.*s %10.10s %5.5s \n",
					    UT_NAMESIZE, UT_NAMESIZE, bp->ut_name,
					    UT_LINESIZE, UT_LINESIZE, bp->ut_line,
					    UT_HOSTSIZE, UT_HOSTSIZE, bp->ut_host,
					    ct, ct + 11);
					if (maxrec && !--maxrec)
						return;
				}
				continue;
			}
			if (bp->ut_name[0] == '\0' || want(bp)) {
				/* find associated tty */
				for (tt = ttylist.lh_first; ; tt = tt->list.le_next) {
					if (tt == NULL) {
						/* add new one */
						tt = malloc(sizeof(struct ttytab));
						if (tt == NULL)
							err(1, "malloc failure");
						tt->logout = currentout;
						strncpy(tt->tty, bp->ut_line, UT_LINESIZE);
						LIST_INSERT_HEAD(&ttylist, tt, list);
						break;
					}
					if (!strncmp(tt->tty, bp->ut_line, UT_LINESIZE))
						break;
				}
				if (bp->ut_name[0]) {
					/*
					 * when uucp and ftp log in over a network, the entry in
					 * the utmp file is the name plus their process id.  See
					 * etc/ftpd.c and usr.bin/uucp/uucpd.c for more information.
					 */
					if (!strncmp(bp->ut_line, "ftp", sizeof("ftp") - 1))
						bp->ut_line[3] = '\0';
					else if (!strncmp(bp->ut_line, "uucp", sizeof("uucp") - 1))
						bp->ut_line[4] = '\0';
					tm = localtime(&bp->ut_time);
					(void) strftime(ct, sizeof(ct), "%c", tm);
					printf("%-*.*s %-*.*s %-*.*s %10.10s %5.5s ",
					    UT_NAMESIZE, UT_NAMESIZE, bp->ut_name,
					    UT_LINESIZE, UT_LINESIZE, bp->ut_line,
					    UT_HOSTSIZE, UT_HOSTSIZE, bp->ut_host,
					    ct, ct + 11);
					if (!tt->logout)
						puts("  still logged in");
					else {
						if (tt->logout < 0) {
							tt->logout = -tt->logout;
							printf("- %s", crmsg);
						}
						else {
							tm = localtime(&tt->logout);
							(void) strftime(ct, sizeof(ct), "%c", tm);
							printf("- %5.5s", ct + 11);
						}
						delta = tt->logout - bp->ut_time;
						tm = gmtime(&delta);
						(void) strftime(ct, sizeof(ct), "%c", tm);
						if (delta < 86400)
							printf("  (%5.5s)\n", ct + 11);
						else
							printf(" (%ld+%5.5s)\n",
							    delta / 86400, ct + 11);
					}
					LIST_REMOVE(tt, list);
					free(tt);
					if (maxrec != -1 && !--maxrec)
						return;
				} else {
					tt->logout = bp->ut_time;
				}
			}
		}
	}
	tm = localtime(&buf[0].ut_time);
	(void) strftime(ct, sizeof(ct), "\nwtmp begins %c\n", tm);
	printf(ct);
}

/*
 * want --
 *	see if want this entry
 */
int
want(bp)
	struct utmp *bp;
{
	ARG *step;

	if (!arglist)
		return (YES);

	for (step = arglist; step; step = step->next)
		switch(step->type) {
		case HOST_TYPE:
			if (!strncasecmp(step->name, bp->ut_host, UT_HOSTSIZE))
				return (YES);
			break;
		case TTY_TYPE:
			if (!strncmp(step->name, bp->ut_line, UT_LINESIZE))
				return (YES);
			break;
		case USER_TYPE:
			if (!strncmp(step->name, bp->ut_name, UT_NAMESIZE))
				return (YES);
			break;
	}
	return (NO);
}

/*
 * addarg --
 *	add an entry to a linked list of arguments
 */
void
addarg(type, arg)
	int type;
	char *arg;
{
	ARG *cur;

	if (!(cur = (ARG *)malloc((u_int)sizeof(ARG))))
		err(1, "malloc failure");
	cur->next = arglist;
	cur->type = type;
	cur->name = arg;
	arglist = cur;
}

/*
 * hostconv --
 *	convert the hostname to search pattern; if the supplied host name
 *	has a domain attached that is the same as the current domain, rip
 *	off the domain suffix since that's what login(1) does.
 */
void
hostconv(arg)
	char *arg;
{
	static int first = 1;
	static char *hostdot, name[MAXHOSTNAMELEN];
	char *argdot;

	if (!(argdot = strchr(arg, '.')))
		return;
	if (first) {
		first = 0;
		if (gethostname(name, sizeof(name)))
			err(1, "gethostname");
		hostdot = strchr(name, '.');
	}
	if (hostdot && !strcasecmp(hostdot, argdot))
		*argdot = '\0';
}

/*
 * ttyconv --
 *	convert tty to correct name.
 */
char *
ttyconv(arg)
	char *arg;
{
	char *mval;

	/*
	 * kludge -- we assume that all tty's end with
	 * a two character suffix.
	 */
	if (strlen(arg) == 2) {
		/* either 6 for "ttyxx" or 8 for "console" */
		if (!(mval = malloc((u_int)8)))
			err(1, "malloc failure");
		if (!strcmp(arg, "co"))
			(void)strcpy(mval, "console");
		else {
			(void)strcpy(mval, "tty");
			(void)strcpy(mval + 3, arg);
		}
		return (mval);
	}
	if (!strncmp(arg, _PATH_DEV, sizeof(_PATH_DEV) - 1))
		return (arg + 5);
	return (arg);
}

/*
 * onintr --
 *	on interrupt, we inform the user how far we've gotten
 */
void
onintr(signo)
	int signo;
{
	char ct[80];
	struct tm *tm;

	tm = localtime(&buf[0].ut_time);
	(void) strftime(ct, sizeof(ct), "%c", tm);
	printf("\ninterrupted %10.10s %5.5s \n", ct, ct + 11);
	if (signo == SIGINT)
		exit(1);
	(void)fflush(stdout);			/* fix required for rsh */
}
