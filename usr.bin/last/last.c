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
 *
 * $FreeBSD$
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1987, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static const char sccsid[] = "@(#)last.c	8.2 (Berkeley) 4/2/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <langinfo.h>
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
#define	ATOI2(ar)	((ar)[0] - '0') * 10 + ((ar)[1] - '0'); (ar) += 2;

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
	time_t	logout;				/* log out time */
	char	tty[UT_LINESIZE + 1];		/* terminal name */
	LIST_ENTRY(ttytab) list;
};

static long	currentout,			/* current logout value */
		maxrec;				/* records to display */
static const	char *file = _PATH_WTMP;		/* wtmp file */
static int	sflag = 0;			/* show delta in seconds */
static int	width = 5;			/* show seconds in delta */
static int      d_first;
static time_t	snaptime;			/* if != 0, we will only
						 * report users logged in
						 * at this snapshot time
						 */

void	 addarg __P((int, char *));
time_t	 dateconv __P((char *));
void	 hostconv __P((char *));
void	 onintr __P((int));
char	*ttyconv __P((char *));
int	 want __P((struct utmp *));
void	 usage __P((void));
void	 wtmp __P((void));

void
usage(void)
{
	(void)fprintf(stderr,
"usage: last [-#] [-d [[CC]YY][MMDD]hhmm[.SS]] [-f file] [-h hostname]\n"
"\t[-t tty] [-s|w] [user ...]\n");
	exit(1);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch;
	char *p;

	(void) setlocale(LC_TIME, "");
	d_first = (*nl_langinfo(D_MD_ORDER) == 'd');

	maxrec = -1;
	snaptime = 0;
	while ((ch = getopt(argc, argv, "0123456789d:f:h:st:w")) != -1)
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
		case 'd':
			snaptime = dateconv(optarg);
			break;
		case 'f':
			file = optarg;
			break;
		case 'h':
			hostconv(optarg);
			addarg(HOST_TYPE, optarg);
			break;
		case 's':
			sflag++;	/* Show delta as seconds */
			break;
		case 't':
			addarg(TTY_TYPE, ttyconv(optarg));
			break;
		case 'w':
			width = 8;
			break;
		case '?':
		default:
			usage();
		}

	if (sflag && width == 8) usage();

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
	long	bl;
	time_t	delta;				/* time difference */
	int	bytes, wfd;
	const	char *crmsg;
	char ct[80];
	struct tm *tm;
	int	 snapfound = 0;			/* found snapshot entry? */
	time_t	t;

	LIST_INIT(&ttylist);

	if ((wfd = open(file, O_RDONLY, 0)) < 0 || fstat(wfd, &stb) == -1)
		err(1, "%s", file);
	bl = (stb.st_size + sizeof(buf) - 1) / sizeof(buf);

	(void)time(&t);
	buf[0].ut_time = _time_to_int(t);
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
				for (tt = LIST_FIRST(&ttylist); tt;) {
					LIST_REMOVE(tt, list);
					ttx = tt;
					tt = LIST_NEXT(tt, list);
					free(ttx);
				}
				currentout = -bp->ut_time;
				crmsg = strncmp(bp->ut_name, "shutdown",
				    UT_NAMESIZE) ? "crash" : "shutdown";
				/*
				 * if we're in snapshot mode, we want to
				 * exit if this shutdown/reboot appears
				 * while we we are tracking the active
				 * range
				 */
				if (snaptime && snapfound)
					return;
				/*
				 * don't print shutdown/reboot entries
				 * unless flagged for 
				 */ 
				if (!snaptime && want(bp)) {
					t = _int_to_time(bp->ut_time);
					tm = localtime(&t);
					(void) strftime(ct, sizeof(ct),
						     d_first ? "%a %e %b %R" :
							       "%a %b %e %R",
						     tm);
					printf("%-*.*s %-*.*s %-*.*s %s\n",
					    UT_NAMESIZE, UT_NAMESIZE,
					    bp->ut_name, UT_LINESIZE,
					    UT_LINESIZE, bp->ut_line,
					    UT_HOSTSIZE, UT_HOSTSIZE,
					    bp->ut_host, ct);
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
				if (want(bp) && !snaptime) {
					t = _int_to_time(bp->ut_time);
					tm = localtime(&t);
					(void) strftime(ct, sizeof(ct),
						     d_first ? "%a %e %b %R" :
							       "%a %b %e %R",
						     tm);
					printf("%-*.*s %-*.*s %-*.*s %s\n",
					    UT_NAMESIZE, UT_NAMESIZE, bp->ut_name,
					    UT_LINESIZE, UT_LINESIZE, bp->ut_line,
					    UT_HOSTSIZE, UT_HOSTSIZE, bp->ut_host,
					    ct);
					if (maxrec && !--maxrec)
						return;
				}
				continue;
			}
			/* find associated tty */
			LIST_FOREACH(tt, &ttylist, list)
			    if (!strncmp(tt->tty, bp->ut_line, UT_LINESIZE))
				    break;
			
			if (tt == NULL) {
				/* add new one */
				tt = malloc(sizeof(struct ttytab));
				if (tt == NULL)
					err(1, "malloc failure");
				tt->logout = currentout;
				strncpy(tt->tty, bp->ut_line, UT_LINESIZE);
				LIST_INSERT_HEAD(&ttylist, tt, list);
			}
			
			/*
			 * print record if not in snapshot mode and wanted
			 * or in snapshot mode and in snapshot range
			 */
			if (bp->ut_name[0] && (want(bp) ||
			    (bp->ut_time < snaptime &&
				(tt->logout > snaptime || tt->logout < 1)))) {
				snapfound = 1;
				/*
				 * when uucp and ftp log in over a network, the entry in
				 * the utmp file is the name plus their process id.  See
				 * etc/ftpd.c and usr.bin/uucp/uucpd.c for more information.
				 */
				if (!strncmp(bp->ut_line, "ftp", sizeof("ftp") - 1))
					bp->ut_line[3] = '\0';
				else if (!strncmp(bp->ut_line, "uucp", sizeof("uucp") - 1))
					bp->ut_line[4] = '\0';
				t = _int_to_time(bp->ut_time);
				tm = localtime(&t);
				(void) strftime(ct, sizeof(ct),
				    d_first ? "%a %e %b %R" :
				    "%a %b %e %R",
				    tm);
				printf("%-*.*s %-*.*s %-*.*s %s ",
				    UT_NAMESIZE, UT_NAMESIZE, bp->ut_name,
				    UT_LINESIZE, UT_LINESIZE, bp->ut_line,
				    UT_HOSTSIZE, UT_HOSTSIZE, bp->ut_host,
				    ct);
				if (!tt->logout)
					puts("  still logged in");
				else {
					if (tt->logout < 0) {
						tt->logout = -tt->logout;
						printf("- %s", crmsg);
					}
					else {
						tm = localtime(&tt->logout);
						(void) strftime(ct, sizeof(ct), "%R", tm);
						printf("- %s", ct);
					}
					delta = tt->logout - bp->ut_time;
					if ( sflag ) {
						printf("  (%8ld)\n", 
						    (long)delta);
					} else {
						tm = gmtime(&delta);
						(void) strftime(ct, sizeof(ct),
						    width >= 8 ? "%T" : "%R",
						    tm);
						if (delta < 86400)
							printf("  (%s)\n", ct);
						else
							printf(" (%ld+%s)\n",
							    (long)delta /
							    86400, ct);
					}
				}
				if (maxrec != -1 && !--maxrec)
					return;
			}
			tt->logout = bp->ut_time;
		}
	}
	t = _int_to_time(buf[0].ut_time);
	tm = localtime(&t);
	(void) strftime(ct, sizeof(ct), "\nwtmp begins %+\n", tm);
	printf("%s", ct);
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

	if (snaptime)
		return (NO);

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
 * dateconv --
 * 	Convert the snapshot time in command line given in the format
 * 	[[CC]YY]MMDDhhmm[.SS]] to a time_t.
 * 	Derived from atime_arg1() in usr.bin/touch/touch.c
 */
time_t
dateconv(arg)
        char *arg;
{
        time_t timet;
        struct tm *t;
        int yearset;
        char *p;

        /* Start with the current time. */
        if (time(&timet) < 0)
                err(1, "time");
        if ((t = localtime(&timet)) == NULL)
                err(1, "localtime");

        /* [[CC]YY]MMDDhhmm[.SS] */
        if ((p = strchr(arg, '.')) == NULL)
                t->tm_sec = 0; 		/* Seconds defaults to 0. */
        else {
                if (strlen(p + 1) != 2)
                        goto terr;
                *p++ = '\0';
                t->tm_sec = ATOI2(p);
        }

        yearset = 0;
        switch (strlen(arg)) {
        case 12:                	/* CCYYMMDDhhmm */
                t->tm_year = ATOI2(arg);
                t->tm_year *= 100;
                yearset = 1;
                /* FALLTHOUGH */
        case 10:                	/* YYMMDDhhmm */
                if (yearset) {
                        yearset = ATOI2(arg);
                        t->tm_year += yearset;
                } else {
                        yearset = ATOI2(arg);
                        if (yearset < 69)
                                t->tm_year = yearset + 2000;
                        else
                                t->tm_year = yearset + 1900;
                }
                t->tm_year -= 1900;     /* Convert to UNIX time. */
                /* FALLTHROUGH */
        case 8:				/* MMDDhhmm */
                t->tm_mon = ATOI2(arg);
                --t->tm_mon;    	/* Convert from 01-12 to 00-11 */
                t->tm_mday = ATOI2(arg);
                t->tm_hour = ATOI2(arg);
                t->tm_min = ATOI2(arg);
                break;
        case 4:				/* hhmm */
                t->tm_hour = ATOI2(arg);
                t->tm_min = ATOI2(arg);
                break;
        default:
                goto terr;
        }
        t->tm_isdst = -1;       	/* Figure out DST. */
        timet = mktime(t);
        if (timet == -1)
terr:           errx(1,
        "out of range or illegal time specification: [[CC]YY]MMDDhhmm[.SS]");
        return timet;
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
	time_t t = _int_to_time(buf[0].ut_time);

	tm = localtime(&t);
	(void) strftime(ct, sizeof(ct),
			d_first ? "%a %e %b %R" : "%a %b %e %R",
			tm);
	printf("\ninterrupted %s\n", ct);
	if (signo == SIGINT)
		exit(1);
	(void)fflush(stdout);			/* fix required for rsh */
}
