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
static char sccsid[] = "@(#)displayq.c	8.4 (Berkeley) 4/28/95";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lp.h"
#include "lp.local.h"
#include "pathnames.h"

/*
 * Routines to display the state of the queue.
 */
#define JOBCOL	40		/* column for job # in -l format */
#define OWNCOL	7		/* start of Owner column in normal */
#define SIZCOL	62		/* start of Size column in normal */

/*
 * Stuff for handling job specifications
 */
extern uid_t	uid, euid;

static int	col;		/* column on screen */
static char	current[40];	/* current file being printed */
static char	file[132];	/* print file name */
static int	first;		/* first file in ``files'' column? */
static int	garbage;	/* # of garbage cf files */
static int	lflag;		/* long output option */
static int	rank;		/* order to be printed (-1=none, 0=active) */
static long	totsize;	/* total print job size in bytes */

static char	*head0 = "Rank   Owner      Job  Files";
static char	*head1 = "Total Size\n";

static void	alarmhandler __P((int));
static void	warn __P((void));

/*
 * Display the current state of the queue. Format = 1 if long format.
 */
void
displayq(format)
	int format;
{
	register struct queue *q;
	register int i, nitems, fd, ret;
	register char	*cp;
	struct queue **queue;
	struct stat statb;
	FILE *fp;
	void (*savealrm)(int);

	lflag = format;
	totsize = 0;
	rank = -1;
	if ((i = cgetent(&bp, printcapdb, printer)) == -2)
		fatal("can't open printer description file");
	else if (i == -1)
		fatal("unknown printer");
	else if (i == -3)
		fatal("potential reference loop detected in printcap file");
	if (cgetstr(bp, "lp", &LP) < 0)
		LP = _PATH_DEFDEVLP;
	if (cgetstr(bp, "rp", &RP) < 0)
		RP = DEFLP;
	if (cgetstr(bp, "sd", &SD) < 0)
		SD = _PATH_DEFSPOOL;
	if (cgetstr(bp,"lo", &LO) < 0)
		LO = DEFLOCK;
	if (cgetstr(bp, "st", &ST) < 0)
		ST = DEFSTAT;
	cgetstr(bp, "rm", &RM);
	if ((cp = checkremote()))
		printf("Warning: %s\n", cp);

	/*
	 * Print out local queue
	 * Find all the control files in the spooling directory
	 */
	seteuid(euid);
	if (chdir(SD) < 0)
		fatal("cannot chdir to spooling directory");
	seteuid(uid);
	if ((nitems = getq(&queue)) < 0)
		fatal("cannot examine spooling area\n");
	seteuid(euid);
	ret = stat(LO, &statb);
	seteuid(uid);
	if (ret >= 0) {
		if (statb.st_mode & 0100) {
			if (remote)
				printf("%s: ", host);
			printf("Warning: %s is down: ", printer);
			seteuid(euid);
			fd = open(ST, O_RDONLY);
			seteuid(uid);
			if (fd >= 0) {
				(void) flock(fd, LOCK_SH);
				while ((i = read(fd, line, sizeof(line))) > 0)
					(void) fwrite(line, 1, i, stdout);
				(void) close(fd);	/* unlocks as well */
			} else
				putchar('\n');
		}
		if (statb.st_mode & 010) {
			if (remote)
				printf("%s: ", host);
			printf("Warning: %s queue is turned off\n", printer);
		}
	}

	if (nitems) {
		seteuid(euid);
		fp = fopen(LO, "r");
		seteuid(uid);
		if (fp == NULL)
			warn();
		else {
			/* get daemon pid */
			cp = current;
			while ((i = getc(fp)) != EOF && i != '\n')
				*cp++ = i;
			*cp = '\0';
			i = atoi(current);
			if (i <= 0) {
				ret = -1;
			} else {
				seteuid(euid);
				ret = kill(i, 0);
				seteuid(uid);
			}
			if (ret < 0) {
				warn();
			} else {
				/* read current file name */
				cp = current;
				while ((i = getc(fp)) != EOF && i != '\n')
					*cp++ = i;
				*cp = '\0';
				/*
				 * Print the status file.
				 */
				if (remote)
					printf("%s: ", host);
				seteuid(euid);
				fd = open(ST, O_RDONLY);
				seteuid(uid);
				if (fd >= 0) {
					(void) flock(fd, LOCK_SH);
					while ((i = read(fd, line, sizeof(line))) > 0)
						(void) fwrite(line, 1, i, stdout);
					(void) close(fd);	/* unlocks as well */
				} else
					putchar('\n');
			}
			(void) fclose(fp);
		}
		/*
		 * Now, examine the control files and print out the jobs to
		 * be done for each user.
		 */
		if (!lflag)
			header();
		for (i = 0; i < nitems; i++) {
			q = queue[i];
			inform(q->q_name);
			free(q);
		}
		free(queue);
	}
	if (!remote) {
		if (nitems == 0)
			puts("no entries");
		return;
	}

	/*
	 * Print foreign queue
	 * Note that a file in transit may show up in either queue.
	 */
	if (nitems)
		putchar('\n');
	(void) snprintf(line, sizeof(line), "%c%s", format + '\3', RP);
	cp = line;
	for (i = 0; i < requests && cp-line+10 < sizeof(line) - 1; i++) {
		cp += strlen(cp);
		(void) sprintf(cp, " %d", requ[i]);
	}
	for (i = 0; i < users && cp - line + 1 + strlen(user[i]) < 
		sizeof(line) - 1; i++) {
		cp += strlen(cp);
		*cp++ = ' ';
		(void) strcpy(cp, user[i]);
	}
	strcat(line, "\n");
	savealrm = signal(SIGALRM, alarmhandler);
	alarm(10);
	fd = getport(RM, 0);
	(void)signal(SIGALRM, savealrm);
	if (fd < 0) {
		if (from != host)
			printf("%s: ", host);
		printf("connection to %s is down\n", RM);
	}
	else {
		i = strlen(line);
		if (write(fd, line, i) != i)
			fatal("Lost connection");
		while ((i = read(fd, line, sizeof(line))) > 0)
			(void) fwrite(line, 1, i, stdout);
		(void) close(fd);
	}
}

/*
 * Print a warning message if there is no daemon present.
 */
static void
warn()
{
	if (remote)
		printf("\n%s: ", host);
	puts("Warning: no daemon present");
	current[0] = '\0';
}

/*
 * Print the header for the short listing format
 */
void
header()
{
	printf(head0);
	col = strlen(head0)+1;
	blankfill(SIZCOL);
	printf(head1);
}

void
inform(cf)
	char *cf;
{
	register int j;
	FILE *cfp;

	/*
	 * There's a chance the control file has gone away
	 * in the meantime; if this is the case just keep going
	 */
	seteuid(euid);
	if ((cfp = fopen(cf, "r")) == NULL)
		return;
	seteuid(uid);

	if (rank < 0)
		rank = 0;
	if (remote || garbage || strcmp(cf, current))
		rank++;
	j = 0;
	while (getline(cfp)) {
		switch (line[0]) {
		case 'P': /* Was this file specified in the user's list? */
			if (!inlist(line+1, cf)) {
				fclose(cfp);
				return;
			}
			if (lflag) {
				printf("\n%s: ", line+1);
				col = strlen(line+1) + 2;
				prank(rank);
				blankfill(JOBCOL);
				printf(" [job %s]\n", cf+3);
			} else {
				col = 0;
				prank(rank);
				blankfill(OWNCOL);
				printf("%-10s %-3d  ", line+1, atoi(cf+3));
				col += 16;
				first = 1;
			}
			continue;
		default: /* some format specifer and file name? */
			if (line[0] < 'a' || line[0] > 'z')
				continue;
			if (j == 0 || strcmp(file, line+1) != 0) {
				(void) strncpy(file, line+1, sizeof(file) - 1);
				file[sizeof(file) - 1] = '\0';
			}
			j++;
			continue;
		case 'N':
			show(line+1, file, j);
			file[0] = '\0';
			j = 0;
		}
	}
	fclose(cfp);
	if (!lflag) {
		blankfill(SIZCOL);
		printf("%ld bytes\n", totsize);
		totsize = 0;
	}
}

int
inlist(name, file)
	char *name, *file;
{
	register int *r, n;
	register char **u, *cp;

	if (users == 0 && requests == 0)
		return(1);
	/*
	 * Check to see if it's in the user list
	 */
	for (u = user; u < &user[users]; u++)
		if (!strcmp(*u, name))
			return(1);
	/*
	 * Check the request list
	 */
	for (n = 0, cp = file+3; isdigit(*cp); )
		n = n * 10 + (*cp++ - '0');
	for (r = requ; r < &requ[requests]; r++)
		if (*r == n && !strcmp(cp, from))
			return(1);
	return(0);
}

void
show(nfile, file, copies)
	register char *nfile, *file;
	int copies;
{
	if (strcmp(nfile, " ") == 0)
		nfile = "(standard input)";
	if (lflag)
		ldump(nfile, file, copies);
	else
		dump(nfile, file, copies);
}

/*
 * Fill the line with blanks to the specified column
 */
void
blankfill(n)
	register int n;
{
	while (col++ < n)
		putchar(' ');
}

/*
 * Give the abbreviated dump of the file names
 */
void
dump(nfile, file, copies)
	char *nfile, *file;
	int copies;
{
	register short n, fill;
	struct stat lbuf;

	/*
	 * Print as many files as will fit
	 *  (leaving room for the total size)
	 */
	 fill = first ? 0 : 2;	/* fill space for ``, '' */
	 if (((n = strlen(nfile)) + col + fill) >= SIZCOL-4) {
		if (col < SIZCOL) {
			printf(" ..."), col += 4;
			blankfill(SIZCOL);
		}
	} else {
		if (first)
			first = 0;
		else
			printf(", ");
		printf("%s", nfile);
		col += n+fill;
	}
	seteuid(euid);
	if (*file && !stat(file, &lbuf))
		totsize += copies * lbuf.st_size;
	seteuid(uid);
}

/*
 * Print the long info about the file
 */
void
ldump(nfile, file, copies)
	char *nfile, *file;
	int copies;
{
	struct stat lbuf;

	putchar('\t');
	if (copies > 1)
		printf("%-2d copies of %-19s", copies, nfile);
	else
		printf("%-32s", nfile);
	if (*file && !stat(file, &lbuf))
		printf(" %qd bytes", lbuf.st_size);
	else
		printf(" ??? bytes");
	putchar('\n');
}

/*
 * Print the job's rank in the queue,
 *   update col for screen management
 */
void
prank(n)
	int n;
{
	char rline[100];
	static char *r[] = {
		"th", "st", "nd", "rd", "th", "th", "th", "th", "th", "th"
	};

	if (n == 0) {
		printf("active");
		col += 6;
		return;
	}
	if ((n/10)%10 == 1)
		(void)snprintf(rline, sizeof(rline), "%dth", n);
	else
		(void)snprintf(rline, sizeof(rline), "%d%s", n, r[n%10]);
	col += strlen(rline);
	printf("%s", rline);
}

void
alarmhandler(signo)
	int signo;
{
	/* ignored */
}
