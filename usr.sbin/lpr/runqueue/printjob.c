/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
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
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)printjob.c	8.7 (Berkeley) 5/10/95";
#endif /* not lint */


/*
 * printjob -- print jobs in the queue.
 *
 *	NOTE: the lock file is used to pass information to lpq and lprm.
 *	it does not need to be removed because file locks are dynamic.
 */

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <pwd.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include "lp.h"
#include "lp.local.h"
#include "pathnames.h"
#include "extern.h"

#define DORETURN	0	/* absorb fork error */
#define DOABORT		1	/* abort if dofork fails */

/*
 * Error tokens
 */
#define REPRINT		-2
#define ERROR		-1
#define	OK		0
#define	FATALERR	1
#define	NOACCT		2
#define	FILTERERR	3
#define	ACCESS		4

static dev_t	 fdev;		/* device of file pointed to by symlink */
static ino_t	 fino;		/* inode of file pointed to by symlink */
static FILE	*cfp;		/* control file */
static int	 child;		/* id of any filters */
static int	 lfd;		/* lock file descriptor */
static int	 ofd;		/* output filter file descriptor */
static int	 ofilter;	/* id of output filter, if any */
static int	 tfd = -1;	/* output filter temp file output */
static int	 pfd;		/* prstatic inter file descriptor */
static int	 pid;		/* pid of lpd process */
static int	 prchild;	/* id of pr process */
static char	 title[80];	/* ``pr'' title */
static int	 tof;		/* true if at top of form */

static char	class[32];		/* classification field */
static char	fromhost[32];		/* user's host machine */
				/* indentation size in static characters */
static char	indent[10] = "-i0";
static char	jobname[100];		/* job or file name */
static char	length[10] = "-l";	/* page length in lines */
static char	logname[32];		/* user's login name */
static char	pxlength[10] = "-y";	/* page length in pixels */
static char	pxwidth[10] = "-x";	/* page width in pixels */
static char	tempfile[] = "errsXXXXXX"; /* file name for filter errors */
static char	width[10] = "-w";	/* page width in static characters */
#define TFILENAME "fltXXXXXX"
static char	tfile[] = TFILENAME;	/* file name for filter output */

static void       abortpr __P((int));
static void       banner __P((char *, char *));
static int        dofork __P((int));
static int        dropit __P((int));
static void       init __P((void));
static void       openpr __P((void));
static void       opennet __P((char *));
static void       opentty __P((void));
static void       openrem __P((void));
static int        print __P((int, char *));
static int        printit __P((char *));
static void       pstatus __P((const char *, ...));
static char       response __P((void));
static void       scan_out __P((int, char *, int));
static char      *scnline __P((int, char *, int));
static int        sendfile __P((int, char *, char));
static int        sendit __P((char *));
static void       sendmail __P((char *, int));
static void       setty __P((void));

void
printjob()
{
	struct stat stb;
	register struct queue *q, **qp;
	struct queue **queue;
	register int i, nitems;
	off_t pidoff;
	int errcnt, count = 0;

	init();					/* set up capabilities */
	(void) write(1, "", 1);			/* ack that daemon is started */
	(void) close(2);			/* set up log file */
	if (open(LF, O_WRONLY|O_APPEND, 0664) < 0) {
		syslog(LOG_ERR, "%s: %m", LF);
		(void) open(_PATH_DEVNULL, O_WRONLY);
	}
	setgid(getegid());
	pid = getpid();				/* for use with lprm */
	setpgrp(0, pid);
	signal(SIGHUP, abortpr);
	signal(SIGINT, abortpr);
	signal(SIGQUIT, abortpr);
	signal(SIGTERM, abortpr);

	(void) mktemp(tempfile);

	/*
	 * uses short form file names
	 */
	if (chdir(SD) < 0) {
		syslog(LOG_ERR, "%s: %m", SD);
		exit(1);
	}
	if (stat(LO, &stb) == 0 && (stb.st_mode & 0100))
		exit(0);		/* printing disabled */
	lfd = open(LO, O_WRONLY|O_CREAT, 0644);
	if (lfd < 0) {
		syslog(LOG_ERR, "%s: %s: %m", printer, LO);
		exit(1);
	}
	if (flock(lfd, LOCK_EX|LOCK_NB) < 0) {
		if (errno == EWOULDBLOCK)	/* active deamon present */
			exit(0);
		syslog(LOG_ERR, "%s: %s: %m", printer, LO);
		exit(1);
	}
	ftruncate(lfd, 0);
	/*
	 * write process id for others to know
	 */
	sprintf(line, "%u\n", pid);
	pidoff = i = strlen(line);
	if (write(lfd, line, i) != i) {
		syslog(LOG_ERR, "%s: %s: %m", printer, LO);
		exit(1);
	}
	/*
	 * search the spool directory for work and sort by queue order.
	 */
	if ((nitems = getq(&queue)) < 0) {
		syslog(LOG_ERR, "%s: can't scan %s", printer, SD);
		exit(1);
	}
	if (nitems == 0)		/* no work to do */
		exit(0);
	if (stb.st_mode & 01) {		/* reset queue flag */
		if (fchmod(lfd, stb.st_mode & 0776) < 0)
			syslog(LOG_ERR, "%s: %s: %m", printer, LO);
	}
	openpr();			/* open printer or remote */
again:
	/*
	 * we found something to do now do it --
	 *    write the name of the current control file into the lock file
	 *    so the spool queue program can tell what we're working on
	 */
	for (qp = queue; nitems--; free((char *) q)) {
		q = *qp++;
		if (stat(q->q_name, &stb) < 0)
			continue;
		errcnt = 0;
	restart:
		(void) lseek(lfd, pidoff, 0);
		(void) sprintf(line, "%s\n", q->q_name);
		i = strlen(line);
		if (write(lfd, line, i) != i)
			syslog(LOG_ERR, "%s: %s: %m", printer, LO);
		if (!remote)
			i = printit(q->q_name);
		else
			i = sendit(q->q_name);
		/*
		 * Check to see if we are supposed to stop printing or
		 * if we are to rebuild the queue.
		 */
		if (fstat(lfd, &stb) == 0) {
			/* stop printing before starting next job? */
			if (stb.st_mode & 0100)
				goto done;
			/* rebuild queue (after lpc topq) */
			if (stb.st_mode & 01) {
				for (free((char *) q); nitems--; free((char *) q))
					q = *qp++;
				if (fchmod(lfd, stb.st_mode & 0776) < 0)
					syslog(LOG_WARNING, "%s: %s: %m",
						printer, LO);
				break;
			}
		}
		if (i == OK)		/* file ok and printed */
			count++;
		else if (i == REPRINT && ++errcnt < 5) {
			/* try reprinting the job */
			syslog(LOG_INFO, "restarting %s", printer);
			if (ofilter > 0) {
				kill(ofilter, SIGCONT);	/* to be sure */
				(void) close(ofd);
				while ((i = wait(NULL)) > 0 && i != ofilter)
					;
				ofilter = 0;
			}
			(void) close(pfd);	/* close printer */
			if (ftruncate(lfd, pidoff) < 0)
				syslog(LOG_WARNING, "%s: %s: %m", printer, LO);
			openpr();		/* try to reopen printer */
			goto restart;
		} else {
			syslog(LOG_WARNING, "%s: job could not be %s (%s)", printer,
				remote ? "sent to remote host" : "printed", q->q_name);
			if (i == REPRINT) {
				/* insure we don't attempt this job again */
				(void) unlink(q->q_name);
				q->q_name[0] = 'd';
				(void) unlink(q->q_name);
				if (logname[0])
					sendmail(logname, FATALERR);
			}
		}
	}
	free((char *) queue);
	/*
	 * search the spool directory for more work.
	 */
	if ((nitems = getq(&queue)) < 0) {
		syslog(LOG_ERR, "%s: can't scan %s", printer, SD);
		exit(1);
	}
	if (nitems == 0) {		/* no more work to do */
	done:
		if (count > 0) {	/* Files actually printed */
			if (!SF && !tof)
				(void) write(ofd, FF, strlen(FF));
			if (TR != NULL)		/* output trailer */
				(void) write(ofd, TR, strlen(TR));
		}
		(void) close(ofd);
		(void) wait(NULL);
		(void) unlink(tempfile);
		exit(0);
	}
	goto again;
}

char	fonts[4][50];	/* fonts for troff */

char ifonts[4][40] = {
	_PATH_VFONTR,
	_PATH_VFONTI,
	_PATH_VFONTB,
	_PATH_VFONTS,
};

/*
 * The remaining part is the reading of the control file (cf)
 * and performing the various actions.
 */
static int
printit(file)
	char *file;
{
	register int i;
	char *cp;
	int bombed = OK;

	/*
	 * open control file; ignore if no longer there.
	 */
	if ((cfp = fopen(file, "r")) == NULL) {
		syslog(LOG_INFO, "%s: %s: %m", printer, file);
		return(OK);
	}
	/*
	 * Reset troff fonts.
	 */
	for (i = 0; i < 4; i++)
		strcpy(fonts[i], ifonts[i]);
	sprintf(&width[2], "%d", PW);
	strcpy(indent+2, "0");

	/*
	 *      read the control file for work to do
	 *
	 *      file format -- first character in the line is a command
	 *      rest of the line is the argument.
	 *      valid commands are:
	 *
	 *		S -- "stat info" for symbolic link protection
	 *		J -- "job name" on banner page
	 *		C -- "class name" on banner page
	 *              L -- "literal" user's name to print on banner
	 *		T -- "title" for pr
	 *		H -- "host name" of machine where lpr was done
	 *              P -- "person" user's login name
	 *              I -- "indent" amount to indent output
	 *		R -- laser dpi "resolution"
	 *              f -- "file name" name of text file to print
	 *		l -- "file name" text file with control chars
	 *		p -- "file name" text file to print with pr(1)
	 *		t -- "file name" troff(1) file to print
	 *		n -- "file name" ditroff(1) file to print
	 *		d -- "file name" dvi file to print
	 *		g -- "file name" plot(1G) file to print
	 *		v -- "file name" plain raster file to print
	 *		c -- "file name" cifplot file to print
	 *		1 -- "R font file" for troff
	 *		2 -- "I font file" for troff
	 *		3 -- "B font file" for troff
	 *		4 -- "S font file" for troff
	 *		N -- "name" of file (used by lpq)
	 *              U -- "unlink" name of file to remove
	 *                    (after we print it. (Pass 2 only)).
	 *		M -- "mail" to user when done printing
	 *
	 *      getline reads a line and expands tabs to blanks
	 */

	/* pass 1 */

	while (getline(cfp))
		switch (line[0]) {
		case 'H':
			strcpy(fromhost, line+1);
			if (class[0] == '\0')
				strncpy(class, line+1, sizeof(class)-1);
			continue;

		case 'P':
			strncpy(logname, line+1, sizeof(logname)-1);
			if (RS) {			/* restricted */
				if (getpwnam(logname) == NULL) {
					bombed = NOACCT;
					sendmail(line+1, bombed);
					goto pass2;
				}
			}
			continue;

		case 'S':
			cp = line+1;
			i = 0;
			while (*cp >= '0' && *cp <= '9')
				i = i * 10 + (*cp++ - '0');
			fdev = i;
			cp++;
			i = 0;
			while (*cp >= '0' && *cp <= '9')
				i = i * 10 + (*cp++ - '0');
			fino = i;
			continue;

		case 'J':
			if (line[1] != '\0')
				strncpy(jobname, line+1, sizeof(jobname)-1);
			else
				strcpy(jobname, " ");
			continue;

		case 'C':
			if (line[1] != '\0')
				strncpy(class, line+1, sizeof(class)-1);
			else if (class[0] == '\0')
				gethostname(class, sizeof(class));
			continue;

		case 'T':	/* header title for pr */
			strncpy(title, line+1, sizeof(title)-1);
			continue;

		case 'L':	/* identification line */
			if (!SH && !HL)
				banner(line+1, jobname);
			continue;

		case '1':	/* troff fonts */
		case '2':
		case '3':
		case '4':
			if (line[1] != '\0')
				strcpy(fonts[line[0]-'1'], line+1);
			continue;

		case 'W':	/* page width */
			strncpy(width+2, line+1, sizeof(width)-3);
			continue;

		case 'I':	/* indent amount */
			strncpy(indent+2, line+1, sizeof(indent)-3);
			continue;

		default:	/* some file to print */
			switch (i = print(line[0], line+1)) {
			case ERROR:
				if (bombed == OK)
					bombed = FATALERR;
				break;
			case REPRINT:
				(void) fclose(cfp);
				return(REPRINT);
			case FILTERERR:
			case ACCESS:
				bombed = i;
				sendmail(logname, bombed);
			}
			title[0] = '\0';
			continue;

		case 'N':
		case 'U':
		case 'M':
		case 'R':
			continue;
		}

	/* pass 2 */

pass2:
	fseek(cfp, 0L, 0);
	while (getline(cfp))
		switch (line[0]) {
		case 'L':	/* identification line */
			if (!SH && HL)
				banner(line+1, jobname);
			continue;

		case 'M':
			if (bombed < NOACCT)	/* already sent if >= NOACCT */
				sendmail(line+1, bombed);
			continue;

		case 'U':
			(void) unlink(line+1);
		}
	/*
	 * clean-up in case another control file exists
	 */
	(void) fclose(cfp);
	(void) unlink(file);
	return(bombed == OK ? OK : ERROR);
}

/*
 * Print a file.
 * Set up the chain [ PR [ | {IF, OF} ] ] or {IF, RF, TF, NF, DF, CF, VF}.
 * Return -1 if a non-recoverable error occured,
 * 2 if the filter detected some errors (but printed the job anyway),
 * 1 if we should try to reprint this job and
 * 0 if all is well.
 * Note: all filters take stdin as the file, stdout as the printer,
 * stderr as the log file, and must not ignore SIGINT.
 */
static int
print(format, file)
	int format;
	char *file;
{
	register int n;
	register char *prog;
	int dtablesize, fi, fo;
	FILE *fp;
	char *av[15], buf[BUFSIZ];
	int pid, p[2], stopped = 0;
	union wait status;
	struct stat stb;

	if (lstat(file, &stb) < 0 || (fi = open(file, O_RDONLY)) < 0)
		return(ERROR);
	/*
	 * Check to see if data file is a symbolic link. If so, it should
	 * still point to the same file or someone is trying to print
	 * something he shouldn't.
	 */
	if ((stb.st_mode & S_IFMT) == S_IFLNK && fstat(fi, &stb) == 0 &&
	    (stb.st_dev != fdev || stb.st_ino != fino))
		return(ACCESS);
	if (!SF && !tof) {		/* start on a fresh page */
		(void) write(ofd, FF, strlen(FF));
		tof = 1;
	}
	if (IF == NULL && (format == 'f' || format == 'l')) {
		tof = 0;
		while ((n = read(fi, buf, BUFSIZ)) > 0)
			if (write(ofd, buf, n) != n) {
				(void) close(fi);
				return(REPRINT);
			}
		(void) close(fi);
		return(OK);
	}
	switch (format) {
	case 'p':	/* print file using 'pr' */
		if (IF == NULL) {	/* use output filter */
			prog = _PATH_PR;
			av[0] = "pr";
			av[1] = width;
			av[2] = length;
			av[3] = "-h";
			av[4] = *title ? title : " ";
			av[5] = "-F";
			av[6] = 0;
			fo = ofd;
			goto start;
		}
		pipe(p);
		if ((prchild = dofork(DORETURN)) == 0) {	/* child */
			dup2(fi, 0);		/* file is stdin */
			dup2(p[1], 1);		/* pipe is stdout */
			closelog();
			for (n = 3, dtablesize = getdtablesize();
			     n < dtablesize; n++)
				(void) close(n);
			execl(_PATH_PR, "pr", width, length,
			    "-h", *title ? title : " ", "-F", 0);
			syslog(LOG_ERR, "cannot execl %s", _PATH_PR);
			exit(2);
		}
		(void) close(p[1]);		/* close output side */
		(void) close(fi);
		if (prchild < 0) {
			prchild = 0;
			(void) close(p[0]);
			return(ERROR);
		}
		fi = p[0];			/* use pipe for input */
	case 'f':	/* print plain text file */
		prog = IF;
		av[1] = width;
		av[2] = length;
		av[3] = indent;
		n = 4;
		break;
	case 'l':	/* like 'f' but pass control characters */
		prog = IF;
		av[1] = "-c";
		av[2] = width;
		av[3] = length;
		av[4] = indent;
		n = 5;
		break;
	case 'r':	/* print a fortran text file */
		prog = RF;
		av[1] = width;
		av[2] = length;
		n = 3;
		break;
	case 't':	/* print troff output */
	case 'n':	/* print ditroff output */
	case 'd':	/* print tex output */
		(void) unlink(".railmag");
		if ((fo = creat(".railmag", FILMOD)) < 0) {
			syslog(LOG_ERR, "%s: cannot create .railmag", printer);
			(void) unlink(".railmag");
		} else {
			for (n = 0; n < 4; n++) {
				if (fonts[n][0] != '/')
					(void) write(fo, _PATH_VFONT,
					    sizeof(_PATH_VFONT) - 1);
				(void) write(fo, fonts[n], strlen(fonts[n]));
				(void) write(fo, "\n", 1);
			}
			(void) close(fo);
		}
		prog = (format == 't') ? TF : (format == 'n') ? NF : DF;
		av[1] = pxwidth;
		av[2] = pxlength;
		n = 3;
		break;
	case 'c':	/* print cifplot output */
		prog = CF;
		av[1] = pxwidth;
		av[2] = pxlength;
		n = 3;
		break;
	case 'g':	/* print plot(1G) output */
		prog = GF;
		av[1] = pxwidth;
		av[2] = pxlength;
		n = 3;
		break;
	case 'v':	/* print raster output */
		prog = VF;
		av[1] = pxwidth;
		av[2] = pxlength;
		n = 3;
		break;
	default:
		(void) close(fi);
		syslog(LOG_ERR, "%s: illegal format character '%c'",
			printer, format);
		return(ERROR);
	}
	if (prog == NULL) {
		(void) close(fi);
		syslog(LOG_ERR,
		   "%s: no filter found in printcap for format character '%c'",
		   printer, format);
		return(ERROR);
	}
	if ((av[0] = strrchr(prog, '/')) != NULL)
		av[0]++;
	else
		av[0] = prog;
	av[n++] = "-n";
	av[n++] = logname;
	av[n++] = "-h";
	av[n++] = fromhost;
	av[n++] = AF;
	av[n] = 0;
	fo = pfd;
	if (ofilter > 0) {		/* stop output filter */
		write(ofd, "\031\1", 2);
		while ((pid =
		    wait3((int *)&status, WUNTRACED, 0)) > 0 && pid != ofilter)
			;
		if (status.w_stopval != WSTOPPED) {
			(void) close(fi);
			syslog(LOG_WARNING,
				"%s: output filter died (retcode=%d termsig=%d)",
				printer, status.w_retcode, status.w_termsig);
			return(REPRINT);
		}
		stopped++;
	}
start:
	if ((child = dofork(DORETURN)) == 0) {	/* child */
		dup2(fi, 0);
		dup2(fo, 1);
		n = open(tempfile, O_WRONLY|O_CREAT|O_TRUNC, 0664);
		if (n >= 0)
			dup2(n, 2);
		closelog();
		for (n = 3, dtablesize = getdtablesize(); n < dtablesize; n++)
			(void) close(n);
		execv(prog, av);
		syslog(LOG_ERR, "cannot execv %s", prog);
		exit(2);
	}
	(void) close(fi);
	if (child < 0)
		status.w_retcode = 100;
	else
		while ((pid = wait((int *)&status)) > 0 && pid != child)
			;
	child = 0;
	prchild = 0;
	if (stopped) {		/* restart output filter */
		if (kill(ofilter, SIGCONT) < 0) {
			syslog(LOG_ERR, "cannot restart output filter");
			exit(1);
		}
	}
	tof = 0;

	/* Copy filter output to "lf" logfile */
	if (fp = fopen(tempfile, "r")) {
		while (fgets(buf, sizeof(buf), fp))
			fputs(buf, stderr);
		fclose(fp);
	}

	if (!WIFEXITED(status)) {
		syslog(LOG_WARNING, "%s: filter '%c' terminated (termsig=%d)",
			printer, format, status.w_termsig);
		return(ERROR);
	}
	switch (status.w_retcode) {
	case 0:
		tof = 1;
		return(OK);
	case 1:
		return(REPRINT);
	case 2:
		return(ERROR);
	default:
		syslog(LOG_WARNING, "%s: filter '%c' exited (retcode=%d)",
			printer, format, status.w_retcode);
		return(FILTERERR);
	}
}

/*
 * Send the daemon control file (cf) and any data files.
 * Return -1 if a non-recoverable error occured, 1 if a recoverable error and
 * 0 if all is well.
 */
static int
sendit(file)
	char *file;
{
	register int i, err = OK;
	char *cp, last[BUFSIZ];

	/*
	 * open control file
	 */
	if ((cfp = fopen(file, "r")) == NULL)
		return(OK);
	/*
	 *      read the control file for work to do
	 *
	 *      file format -- first character in the line is a command
	 *      rest of the line is the argument.
	 *      commands of interest are:
	 *
	 *            a-z -- "file name" name of file to print
	 *              U -- "unlink" name of file to remove
	 *                    (after we print it. (Pass 2 only)).
	 */

	/*
	 * pass 1
	 */
	while (getline(cfp)) {
	again:
		if (line[0] == 'S') {
			cp = line+1;
			i = 0;
			while (*cp >= '0' && *cp <= '9')
				i = i * 10 + (*cp++ - '0');
			fdev = i;
			cp++;
			i = 0;
			while (*cp >= '0' && *cp <= '9')
				i = i * 10 + (*cp++ - '0');
			fino = i;
		} else if (line[0] == 'H') {
			strcpy(fromhost, line+1);
			if (class[0] == '\0')
				strncpy(class, line+1, sizeof(class)-1);
		} else if (line[0] == 'P') {
			strncpy(logname, line+1, sizeof(logname)-1);
			if (RS) {			/* restricted */
				if (getpwnam(logname) == NULL) {
					sendmail(line+1, NOACCT);
					err = ERROR;
					break;
				}
			}
		} else if (line[0] == 'I') {
			strncpy(indent+2, line+1, sizeof(indent)-3);
		} else if (line[0] >= 'a' && line[0] <= 'z') {
			strcpy(last, line);
			while (i = getline(cfp))
				if (strcmp(last, line))
					break;
			switch (sendfile('\3', last+1, *last)) {
			case OK:
				if (i)
					goto again;
				break;
			case REPRINT:
				(void) fclose(cfp);
				return(REPRINT);
			case ACCESS:
				sendmail(logname, ACCESS);
			case ERROR:
				err = ERROR;
			}
			break;
		}
	}
	if (err == OK && sendfile('\2', file, '\0') > 0) {
		(void) fclose(cfp);
		return(REPRINT);
	}
	/*
	 * pass 2
	 */
	fseek(cfp, 0L, 0);
	while (getline(cfp))
		if (line[0] == 'U')
			(void) unlink(line+1);
	/*
	 * clean-up in case another control file exists
	 */
	(void) fclose(cfp);
	(void) unlink(file);
	return(err);
}

/*
 * Send a data file to the remote machine and spool it.
 * Return positive if we should try resending.
 */
static int
sendfile(type, file, format)
	int type;
	char *file;
	char format;
{
	register int f, i, amt;
	struct stat stb;
	char buf[BUFSIZ];
	int sizerr, resp, closedpr;

	if (lstat(file, &stb) < 0 || (f = open(file, O_RDONLY)) < 0)
		return(ERROR);
	/*
	 * Check to see if data file is a symbolic link. If so, it should
	 * still point to the same file or someone is trying to print something
	 * he shouldn't.
	 */
	if ((stb.st_mode & S_IFMT) == S_IFLNK && fstat(f, &stb) == 0 &&
	    (stb.st_dev != fdev || stb.st_ino != fino))
		return(ACCESS);

	sizerr = 0;
	closedpr = 0;
	if (type == '\3') {
		if (IF) {
			/*
			 * We're sending something with an ifilter, we have to
			 * run the ifilter and store the output as a
			 * temporary file (tfile)... the protocol requires us
			 * to send the file size
			 */
			char *av[15];
			int n;
			int nfd;
			int ifilter;
			union wait status;

			strcpy(tfile,TFILENAME);
			if ((tfd = mkstemp(tfile)) == -1) {
				syslog(LOG_ERR, "mkstemp: %m");
				return(ERROR);
			}
			if ((av[0] = strrchr(IF, '/')) == NULL)
				av[0] = IF;
			else
				av[0]++;
			if (format == 'l')
				av[n=1] = "-c";
			else
				n = 0;
			av[++n] = width;
			av[++n] = length;
			av[++n] = indent;
			av[++n] = "-n";
			av[++n] = logname;
			av[++n] = "-h";
			av[++n] = fromhost;
			av[++n] = AF;
			av[++n] = 0;
			if ((ifilter = dofork(DORETURN)) == 0) {  /* child */
				dup2(f, 0);
				dup2(tfd, 1);
				n = open(tempfile, O_WRONLY|O_CREAT|O_TRUNC, 0664);
				if (n >= 0)
					dup2(n, 2);
				closelog();
				for (n = 3, nfd = getdtablesize(); n < nfd; n++)
					(void) close(n);
				execv(IF, av);
				syslog(LOG_ERR, "cannot execv %s", IF);
				exit(2);
			}
			(void) close(f);
			if (ifilter < 0)
				status.w_retcode = 100;
			else
				while ((pid = wait((int *)&status)) > 0 &&
					pid != ifilter)
					;
			switch (status.w_retcode) {
			case 0:
				break;
			case 1:
				unlink(tfile);
				return(REPRINT);
			case 2:
				unlink(tfile);
				return(ERROR);
			default:
				syslog(LOG_WARNING, "%s: filter '%c' exited"
					" (retcode=%d)",
					printer, format, status.w_retcode);
				unlink(tfile);
				return(FILTERERR);
			}
			if (fstat(tfd, &stb) < 0)	/* the size of tfile */
				return(ERROR);
			f = tfd;
			lseek(f,0,SEEK_SET);
		} else if (ofilter) {
			/*
			 * We're sending something with an ofilter, we have to
			 * store the output as a temporary file (tfile)... the
			 * protocol requires us to send the file size
			 */
			int i;
			for (i = 0; i < stb.st_size; i += BUFSIZ) {
				amt = BUFSIZ;
				if (i + amt > stb.st_size)
					amt = stb.st_size - i;
				if (sizerr == 0 && read(f, buf, amt) != amt) {
					sizerr = 1;
					break;
				}
				if (write(ofd, buf, amt) != amt) {
					(void) close(f);
					return(REPRINT);
				}
			}
			close(ofd);
			close(f);
			while ((i = wait(NULL)) > 0 && i != ofilter)
				;
			ofilter = 0;
			if (fstat(tfd, &stb) < 0) {	/* the size of tfile */
				openpr();
				return(ERROR);
			}
			f = tfd;
			lseek(f,0,SEEK_SET);
			closedpr = 1;
		}
	}

	(void) sprintf(buf, "%c%qd %s\n", type, stb.st_size, file);
	amt = strlen(buf);
	for (i = 0;  ; i++) {
		if (write(pfd, buf, amt) != amt ||
		    (resp = response()) < 0 || resp == '\1') {
			(void) close(f);
			if (tfd != -1 && type == '\3') {
				tfd = -1;
				unlink(tfile);
				if (closedpr)
					openpr();
			}
			return(REPRINT);
		} else if (resp == '\0')
			break;
		if (i == 0)
			pstatus("no space on remote; waiting for queue to drain");
		if (i == 10)
			syslog(LOG_ALERT, "%s: can't send to %s; queue full",
				printer, RM);
		sleep(5 * 60);
	}
	if (i)
		pstatus("sending to %s", RM);
	for (i = 0; i < stb.st_size; i += BUFSIZ) {
		amt = BUFSIZ;
		if (i + amt > stb.st_size)
			amt = stb.st_size - i;
		if (sizerr == 0 && read(f, buf, amt) != amt)
			sizerr = 1;
		if (write(pfd, buf, amt) != amt) {
			(void) close(f);
			if (tfd != -1 && type == '\3') {
				tfd = -1;
				unlink(tfile);
				if (closedpr)
					openpr();
			}
			return(REPRINT);
		}
	}

	(void) close(f);
	if (tfd != -1 && type == '\3') {
		tfd = -1;
		unlink(tfile);
	}
	if (sizerr) {
		syslog(LOG_INFO, "%s: %s: changed size", printer, file);
		/* tell recvjob to ignore this file */
		(void) write(pfd, "\1", 1);
		if (closedpr)
			openpr();
		return(ERROR);
	}
	if (write(pfd, "", 1) != 1 || response()) {
		if (closedpr)
			openpr();
		return(REPRINT);
	}
	if (closedpr)
		openpr();
	return(OK);
}

/*
 * Check to make sure there have been no errors and that both programs
 * are in sync with eachother.
 * Return non-zero if the connection was lost.
 */
static char
response()
{
	char resp;

	if (read(pfd, &resp, 1) != 1) {
		syslog(LOG_INFO, "%s: lost connection", printer);
		return(-1);
	}
	return(resp);
}

/*
 * Banner printing stuff
 */
static void
banner(name1, name2)
	char *name1, *name2;
{
	time_t tvec;

	time(&tvec);
	if (!SF && !tof)
		(void) write(ofd, FF, strlen(FF));
	if (SB) {	/* short banner only */
		if (class[0]) {
			(void) write(ofd, class, strlen(class));
			(void) write(ofd, ":", 1);
		}
		(void) write(ofd, name1, strlen(name1));
		(void) write(ofd, "  Job: ", 7);
		(void) write(ofd, name2, strlen(name2));
		(void) write(ofd, "  Date: ", 8);
		(void) write(ofd, ctime(&tvec), 24);
		(void) write(ofd, "\n", 1);
	} else {	/* normal banner */
		(void) write(ofd, "\n\n\n", 3);
		scan_out(ofd, name1, '\0');
		(void) write(ofd, "\n\n", 2);
		scan_out(ofd, name2, '\0');
		if (class[0]) {
			(void) write(ofd,"\n\n\n",3);
			scan_out(ofd, class, '\0');
		}
		(void) write(ofd, "\n\n\n\n\t\t\t\t\tJob:  ", 15);
		(void) write(ofd, name2, strlen(name2));
		(void) write(ofd, "\n\t\t\t\t\tDate: ", 12);
		(void) write(ofd, ctime(&tvec), 24);
		(void) write(ofd, "\n", 1);
	}
	if (!SF)
		(void) write(ofd, FF, strlen(FF));
	tof = 1;
}

static char *
scnline(key, p, c)
	register int key;
	register char *p;
	int c;
{
	register scnwidth;

	for (scnwidth = WIDTH; --scnwidth;) {
		key <<= 1;
		*p++ = key & 0200 ? c : BACKGND;
	}
	return (p);
}

#define TRC(q)	(((q)-' ')&0177)

static void
scan_out(scfd, scsp, dlm)
	int scfd, dlm;
	char *scsp;
{
	register char *strp;
	register nchrs, j;
	char outbuf[LINELEN+1], *sp, c, cc;
	int d, scnhgt;

	for (scnhgt = 0; scnhgt++ < HEIGHT+DROP; ) {
		strp = &outbuf[0];
		sp = scsp;
		for (nchrs = 0; ; ) {
			d = dropit(c = TRC(cc = *sp++));
			if ((!d && scnhgt > HEIGHT) || (scnhgt <= DROP && d))
				for (j = WIDTH; --j;)
					*strp++ = BACKGND;
			else
				strp = scnline(scnkey[c][scnhgt-1-d], strp, cc);
			if (*sp == dlm || *sp == '\0' || nchrs++ >= PW/(WIDTH+1)-1)
				break;
			*strp++ = BACKGND;
			*strp++ = BACKGND;
		}
		while (*--strp == BACKGND && strp >= outbuf)
			;
		strp++;
		*strp++ = '\n';
		(void) write(scfd, outbuf, strp-outbuf);
	}
}

static int
dropit(c)
	int c;
{
	switch(c) {

	case TRC('_'):
	case TRC(';'):
	case TRC(','):
	case TRC('g'):
	case TRC('j'):
	case TRC('p'):
	case TRC('q'):
	case TRC('y'):
		return (DROP);

	default:
		return (0);
	}
}

/*
 * sendmail ---
 *   tell people about job completion
 */
static void
sendmail(user, bombed)
	char *user;
	int bombed;
{
	register int i;
	int dtablesize;
	int p[2], s;
	register char *cp;
	char buf[100];
	struct stat stb;
	FILE *fp;

	pipe(p);
	if ((s = dofork(DORETURN)) == 0) {		/* child */
		dup2(p[0], 0);
		closelog();
		for (i = 3, dtablesize = getdtablesize(); i < dtablesize; i++)
			(void) close(i);
		if ((cp = strrchr(_PATH_SENDMAIL, '/')) != NULL)
			cp++;
	else
			cp = _PATH_SENDMAIL;
		sprintf(buf, "%s@%s", user, fromhost);
		execl(_PATH_SENDMAIL, cp, buf, 0);
		exit(0);
	} else if (s > 0) {				/* parent */
		dup2(p[1], 1);
		printf("To: %s@%s\n", user, fromhost);
		printf("Subject: %s printer job \"%s\"\n", printer,
			*jobname ? jobname : "<unknown>");
		printf("Reply-To: root@%s\n\n", host);
		printf("Your printer job ");
		if (*jobname)
			printf("(%s) ", jobname);
		switch (bombed) {
		case OK:
			printf("\ncompleted successfully\n");
			cp = "OK";
			break;
		default:
		case FATALERR:
			printf("\ncould not be printed\n");
			cp = "FATALERR";
			break;
		case NOACCT:
			printf("\ncould not be printed without an account on %s\n", host);
			cp = "NOACCT";
			break;
		case FILTERERR:
			if (stat(tempfile, &stb) < 0 || stb.st_size == 0 ||
			    (fp = fopen(tempfile, "r")) == NULL) {
				printf("\nhad some errors and may not have printed\n");
				break;
			}
			printf("\nhad the following errors and may not have printed:\n");
			while ((i = getc(fp)) != EOF)
				putchar(i);
			(void) fclose(fp);
			cp = "FILTERERR";
			break;
		case ACCESS:
			printf("\nwas not printed because it was not linked to the original file\n");
			cp = "ACCESS";
		}
		fflush(stdout);
		(void) close(1);
	}
	(void) close(p[0]);
	(void) close(p[1]);
	wait(NULL);
	syslog(LOG_INFO, "mail sent to user %s about job %s on printer %s (%s)",
		user, *jobname ? jobname : "<unknown>", printer, cp);
}

/*
 * dofork - fork with retries on failure
 */
static int
dofork(action)
	int action;
{
	register int i, pid;

	for (i = 0; i < 20; i++) {
		if ((pid = fork()) < 0) {
			sleep((unsigned)(i*i));
			continue;
		}
		/*
		 * Child should run as daemon instead of root
		 */
		if (pid == 0)
			setuid(DU);
		return(pid);
	}
	syslog(LOG_ERR, "can't fork");

	switch (action) {
	case DORETURN:
		return (-1);
	default:
		syslog(LOG_ERR, "bad action (%d) to dofork", action);
		/*FALL THRU*/
	case DOABORT:
		exit(1);
	}
	/*NOTREACHED*/
}

/*
 * Kill child processes to abort current job.
 */
static void
abortpr(signo)
	int signo;
{
	(void) unlink(tempfile);
	kill(0, SIGINT);
	if (ofilter > 0)
		kill(ofilter, SIGCONT);
	while (wait(NULL) > 0)
		;
	if (ofilter > 0 && tfd != -1)
		unlink(tfile);
	exit(0);
}

static void
init()
{
	int status;
	char *s;

	if ((status = cgetent(&bp, printcapdb, printer)) == -2) {
		syslog(LOG_ERR, "can't open printer description file");
		exit(1);
	} else if (status == -1) {
		syslog(LOG_ERR, "unknown printer: %s", printer);
		exit(1);
	} else if (status == -3)
		fatal("potential reference loop detected in printcap file");

	if (cgetstr(bp, "lp", &LP) == -1)
		LP = _PATH_DEFDEVLP;
	if (cgetstr(bp, "rp", &RP) == -1)
		RP = DEFLP;
	if (cgetstr(bp, "lo", &LO) == -1)
		LO = DEFLOCK;
	if (cgetstr(bp, "st", &ST) == -1)
		ST = DEFSTAT;
	if (cgetstr(bp, "lf", &LF) == -1)
		LF = _PATH_CONSOLE;
	if (cgetstr(bp, "sd", &SD) == -1)
		SD = _PATH_DEFSPOOL;
	if (cgetnum(bp, "du", &DU) < 0)
		DU = DEFUID;
	if (cgetstr(bp,"ff", &FF) == -1)
		FF = DEFFF;
	if (cgetnum(bp, "pw", &PW) < 0)
		PW = DEFWIDTH;
	sprintf(&width[2], "%d", PW);
	if (cgetnum(bp, "pl", &PL) < 0)
		PL = DEFLENGTH;
	sprintf(&length[2], "%d", PL);
	if (cgetnum(bp,"px", &PX) < 0)
		PX = 0;
	sprintf(&pxwidth[2], "%d", PX);
	if (cgetnum(bp, "py", &PY) < 0)
		PY = 0;
	sprintf(&pxlength[2], "%d", PY);
	cgetstr(bp, "rm", &RM);
	if (s = checkremote())
		syslog(LOG_WARNING, s);

	cgetstr(bp, "af", &AF);
	cgetstr(bp, "of", &OF);
	cgetstr(bp, "if", &IF);
	cgetstr(bp, "rf", &RF);
	cgetstr(bp, "tf", &TF);
	cgetstr(bp, "nf", &NF);
	cgetstr(bp, "df", &DF);
	cgetstr(bp, "gf", &GF);
	cgetstr(bp, "vf", &VF);
	cgetstr(bp, "cf", &CF);
	cgetstr(bp, "tr", &TR);
	cgetstr(bp, "ms", &MS);

	RS = (cgetcap(bp, "rs", ':') != NULL);
	SF = (cgetcap(bp, "sf", ':') != NULL);
	SH = (cgetcap(bp, "sh", ':') != NULL);
	SB = (cgetcap(bp, "sb", ':') != NULL);
	HL = (cgetcap(bp, "hl", ':') != NULL);
	RW = (cgetcap(bp, "rw", ':') != NULL);

	cgetnum(bp, "br", &BR);

	tof = (cgetcap(bp, "fo", ':') == NULL);
}

/*
 * Acquire line printer or remote connection.
 */
static void
openpr()
{
	register int i;
	int dtablesize;
	char *cp;

	if (!remote && *LP) {
		if (cp = strchr(LP, '@'))
			opennet(cp);
		else
			opentty();
	} else if (remote) {
		openrem();
	} else {
		syslog(LOG_ERR, "%s: no line printer device or host name",
			printer);
		exit(1);
	}

	/*
	 * Start up an output filter, if needed.
	 */
	if (OF && !IF && !ofilter) {
		int p[2];

		pipe(p);
		if (remote) {
			strcpy(tfile,TFILENAME);
			tfd = mkstemp(tfile);
		}
		if ((ofilter = dofork(DOABORT)) == 0) {	/* child */
			dup2(p[0], 0);		/* pipe is std in */
			/* tfile/printer is stdout */
			dup2(remote ? tfd : pfd, 1);
			closelog();
			for (i = 3, dtablesize = getdtablesize();
			     i < dtablesize; i++)
				(void) close(i);
			if ((cp = strrchr(OF, '/')) == NULL)
				cp = OF;
			else
				cp++;
			execl(OF, cp, width, length, 0);
			syslog(LOG_ERR, "%s: %s: %m", printer, OF);
			exit(1);
		}
		(void) close(p[0]);		/* close input side */
		ofd = p[1];			/* use pipe for output */
	} else {
		ofd = pfd;
		ofilter = 0;
	}
}

/*
 * Printer connected directly to the network
 * or to a terminal server on the net
 */
static void
opennet(cp)
	char *cp;
{
	register int i;
	int resp, port;
	char save_ch;

	save_ch = *cp;
	*cp = '\0';
	port = atoi(LP);
	if (port <= 0) {
		syslog(LOG_ERR, "%s: bad port number: %s", printer, LP);
		exit(1);
	}
	*cp++ = save_ch;

	for (i = 1; ; i = i < 256 ? i << 1 : i) {
		resp = -1;
		pfd = getport(cp, port);
		if (pfd < 0 && errno == ECONNREFUSED)
			resp = 1;
		else if (pfd >= 0) {
			/*
			 * need to delay a bit for rs232 lines
			 * to stabilize in case printer is
			 * connected via a terminal server
			 */
			delay(500);
			break;
		}
		if (i == 1) {
		   if (resp < 0)
			pstatus("waiting for %s to come up", LP);
		   else
			pstatus("waiting for access to printer on %s", LP);
		}
		sleep(i);
	}
	pstatus("sending to %s port %d", cp, port);
}

/*
 * Printer is connected to an RS232 port on this host
 */
static void
opentty()
{
	register int i;
	int resp, port;

	for (i = 1; ; i = i < 32 ? i << 1 : i) {
		pfd = open(LP, RW ? O_RDWR : O_WRONLY);
		if (pfd >= 0) {
			delay(500);
			break;
		}
		if (errno == ENOENT) {
			syslog(LOG_ERR, "%s: %m", LP);
			exit(1);
		}
		if (i == 1)
			pstatus("waiting for %s to become ready (offline ?)",
				printer);
		sleep(i);
	}
	if (isatty(pfd))
		setty();
	pstatus("%s is ready and printing", printer);
}

/*
 * Printer is on a remote host
 */
static void
openrem()
{
	register int i, n;
	int resp, port;

	for (i = 1; ; i = i < 256 ? i << 1 : i) {
		resp = -1;
		pfd = getport(RM, 0);
		if (pfd >= 0) {
			(void) sprintf(line, "\2%s\n", RP);
			n = strlen(line);
			if (write(pfd, line, n) == n &&
			    (resp = response()) == '\0')
				break;
			(void) close(pfd);
		}
		if (i == 1) {
			if (resp < 0)
				pstatus("waiting for %s to come up", RM);
			else {
				pstatus("waiting for queue to be enabled on %s",
					RM);
				i = 256;
			}
		}
		sleep(i);
	}
	pstatus("sending to %s", RM);
}

struct bauds {
	int	baud;
	int	speed;
} bauds[] = {
	50,	B50,
	75,	B75,
	110,	B110,
	134,	B134,
	150,	B150,
	200,	B200,
	300,	B300,
	600,	B600,
	1200,	B1200,
	1800,	B1800,
	2400,	B2400,
	4800,	B4800,
	9600,	B9600,
	19200,	EXTA,
	38400,	EXTB,
	57600,	B57600,
	115200,	B115200,
	0,	0
};

/*
 * setup tty lines.
 */
static void
setty()
{
	struct termios ttybuf;
	struct bauds *bp;

	if (ioctl(pfd, TIOCEXCL, (char *)0) < 0) {
		syslog(LOG_ERR, "%s: ioctl(TIOCEXCL): %m", printer);
		exit(1);
	}
	if (tcgetattr(pfd, &ttybuf) < 0) {
		syslog(LOG_ERR, "%s: tcgetattr: %m", printer);
		exit(1);
	}
	if (BR > 0) {
		for (bp = bauds; bp->baud; bp++)
			if (BR == bp->baud)
				break;
		if (!bp->baud) {
			syslog(LOG_ERR, "%s: illegal baud rate %d", printer, BR);
			exit(1);
		}
		cfsetspeed(&ttybuf, bp->speed);
	}
	if (MS) {
		char *s = strdup(MS), *tmp;

		while (tmp = strsep (&s, ",")) {
			msearch(tmp, &ttybuf);
		}
	}
	if (MS || (BR > 0)) {
		if (tcsetattr(pfd, TCSAFLUSH, &ttybuf) == -1) {
			syslog(LOG_ERR, "%s: tcsetattr: %m", printer);
		}
	}
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

static void
#if __STDC__
pstatus(const char *msg, ...)
#else
pstatus(msg, va_alist)
	char *msg;
        va_dcl
#endif
{
	register int fd;
	char buf[BUFSIZ];
	va_list ap;
#if __STDC__
	va_start(ap, msg);
#else
	va_start(ap);
#endif

	umask(0);
	fd = open(ST, O_WRONLY|O_CREAT, 0664);
	if (fd < 0 || flock(fd, LOCK_EX) < 0) {
		syslog(LOG_ERR, "%s: %s: %m", printer, ST);
		exit(1);
	}
	ftruncate(fd, 0);
	(void)vsnprintf(buf, sizeof(buf), msg, ap);
	va_end(ap);
	strcat(buf, "\n");
	(void) write(fd, buf, strlen(buf));
	(void) close(fd);
}
