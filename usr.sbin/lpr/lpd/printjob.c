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
static const char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*
static char sccsid[] = "@(#)printjob.c	8.7 (Berkeley) 5/10/95";
*/
static const char rcsid[] =
  "$FreeBSD$";
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

#define DORETURN	0	/* dofork should return "can't fork" error */
#define DOABORT		1	/* dofork should just die if fork() fails */

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
static int	 job_dfcnt;	/* count of datafiles in current user job */
static int	 lfd;		/* lock file descriptor */
static int	 ofd;		/* output filter file descriptor */
static int	 ofilter;	/* id of output filter, if any */
static int	 tfd = -1;	/* output filter temp file output */
static int	 pfd;		/* prstatic inter file descriptor */
static int	 pid;		/* pid of lpd process */
static int	 prchild;	/* id of pr process */
static char	 title[80];	/* ``pr'' title */
static char      locale[80];    /* ``pr'' locale */

/* these two are set from pp->daemon_user, but only if they are needed */
static char	*daemon_uname;	/* set from pwd->pw_name */
static int	 daemon_defgid;

static char	class[32];		/* classification field */
static char	origin_host[MAXHOSTNAMELEN];	/* user's host machine */
				/* indentation size in static characters */
static char	indent[10] = "-i0";
static char	jobname[100];		/* job or file name */
static char	length[10] = "-l";	/* page length in lines */
static char	logname[32];		/* user's login name */
static char	pxlength[10] = "-y";	/* page length in pixels */
static char	pxwidth[10] = "-x";	/* page width in pixels */
/* tempstderr is the filename used to catch stderr from exec-ing filters */
static char	tempstderr[] = "errs.XXXXXXX";
static char	width[10] = "-w";	/* page width in static characters */
#define TFILENAME "fltXXXXXX"
static char	tfile[] = TFILENAME;	/* file name for filter output */

static void	 abortpr(int _signo);
static void	 alarmhandler(int _signo);
static void	 banner(struct printer *_pp, char *_name1, char *_name2);
static int	 dofork(const struct printer *_pp, int _action);
static int	 dropit(int _c);
static void	 init(struct printer *_pp);
static void	 openpr(const struct printer *_pp);
static void	 opennet(const struct printer *_pp);
static void	 opentty(const struct printer *_pp);
static void	 openrem(const struct printer *pp);
static int	 print(struct printer *_pp, int _format, char *_file);
static int	 printit(struct printer *_pp, char *_file);
static void	 pstatus(const struct printer *_pp, const char *_msg, ...)
		    __printflike(2, 3);
static char	 response(const struct printer *_pp);
static void	 scan_out(struct printer *_pp, int _scfd, char *_scsp, 
		    int _dlm);
static char	*scnline(int _key, char *_p, int _c);
static int	 sendfile(struct printer *_pp, int _type, char *_file, 
		    char _format);
static int	 sendit(struct printer *_pp, char *_file);
static void	 sendmail(struct printer *_pp, char *_user, int _bombed);
static void	 setty(const struct printer *_pp);

void
printjob(struct printer *pp)
{
	struct stat stb;
	register struct jobqueue *q, **qp;
	struct jobqueue **queue;
	register int i, nitems;
	off_t pidoff;
	int errcnt, jobcount, tempfd;

	jobcount = 0;
	init(pp); /* set up capabilities */
	(void) write(1, "", 1);	/* ack that daemon is started */
	(void) close(2);			/* set up log file */
	if (open(pp->log_file, O_WRONLY|O_APPEND, LOG_FILE_MODE) < 0) {
		syslog(LOG_ERR, "%s: %m", pp->log_file);
		(void) open(_PATH_DEVNULL, O_WRONLY);
	}
	setgid(getegid());
	pid = getpid();				/* for use with lprm */
	setpgrp(0, pid);

	/*
	 * At initial lpd startup, printjob may be called with various
	 * signal handlers in effect.  After that initial startup, any
	 * calls to printjob will have a *different* set of signal-handlers
	 * in effect.  Make sure all handlers are the ones we want.
	 */
	signal(SIGCHLD, SIG_DFL);
	signal(SIGHUP, abortpr);
	signal(SIGINT, abortpr);
	signal(SIGQUIT, abortpr);
	signal(SIGTERM, abortpr);

	/*
	 * uses short form file names
	 */
	if (chdir(pp->spool_dir) < 0) {
		syslog(LOG_ERR, "%s: %m", pp->spool_dir);
		exit(1);
	}
	if (stat(pp->lock_file, &stb) == 0 && (stb.st_mode & LFM_PRINT_DIS))
		exit(0);		/* printing disabled */
	lfd = open(pp->lock_file, O_WRONLY|O_CREAT|O_EXLOCK|O_NONBLOCK, 
		   LOCK_FILE_MODE);
	if (lfd < 0) {
		if (errno == EWOULDBLOCK)	/* active daemon present */
			exit(0);
		syslog(LOG_ERR, "%s: %s: %m", pp->printer, pp->lock_file);
		exit(1);
	}
	/* turn off non-blocking mode (was turned on for lock effects only) */
	if (fcntl(lfd, F_SETFL, 0) < 0) {
		syslog(LOG_ERR, "%s: %s: %m", pp->printer, pp->lock_file);
		exit(1);
	}
	ftruncate(lfd, 0);
	/*
	 * write process id for others to know
	 */
	sprintf(line, "%u\n", pid);
	pidoff = i = strlen(line);
	if (write(lfd, line, i) != i) {
		syslog(LOG_ERR, "%s: %s: %m", pp->printer, pp->lock_file);
		exit(1);
	}
	/*
	 * search the spool directory for work and sort by queue order.
	 */
	if ((nitems = getq(pp, &queue)) < 0) {
		syslog(LOG_ERR, "%s: can't scan %s", pp->printer, 
		       pp->spool_dir);
		exit(1);
	}
	if (nitems == 0)		/* no work to do */
		exit(0);
	if (stb.st_mode & LFM_RESET_QUE) { /* reset queue flag */
		if (fchmod(lfd, stb.st_mode & ~LFM_RESET_QUE) < 0)
			syslog(LOG_ERR, "%s: %s: %m", pp->printer,
			       pp->lock_file);
	}

	/* create a file which will be used to hold stderr from filters */
	if ((tempfd = mkstemp(tempstderr)) == -1) {
		syslog(LOG_ERR, "%s: mkstemp(%s): %m", pp->printer,
		       tempstderr);
		exit(1);
	}
	if ((i = fchmod(tempfd, 0664)) == -1) {
		syslog(LOG_ERR, "%s: fchmod(%s): %m", pp->printer,
		       tempstderr);
		exit(1);
	}
	/* lpd doesn't need it to be open, it just needs it to exist */
	close(tempfd);

	openpr(pp);			/* open printer or remote */
again:
	/*
	 * we found something to do now do it --
	 *    write the name of the current control file into the lock file
	 *    so the spool queue program can tell what we're working on
	 */
	for (qp = queue; nitems--; free((char *) q)) {
		q = *qp++;
		if (stat(q->job_cfname, &stb) < 0)
			continue;
		errcnt = 0;
	restart:
		(void) lseek(lfd, pidoff, 0);
		(void) snprintf(line, sizeof(line), "%s\n", q->job_cfname);
		i = strlen(line);
		if (write(lfd, line, i) != i)
			syslog(LOG_ERR, "%s: %s: %m", pp->printer,
			       pp->lock_file);
		if (!pp->remote)
			i = printit(pp, q->job_cfname);
		else
			i = sendit(pp, q->job_cfname);
		/*
		 * Check to see if we are supposed to stop printing or
		 * if we are to rebuild the queue.
		 */
		if (fstat(lfd, &stb) == 0) {
			/* stop printing before starting next job? */
			if (stb.st_mode & LFM_PRINT_DIS)
				goto done;
			/* rebuild queue (after lpc topq) */
			if (stb.st_mode & LFM_RESET_QUE) {
				for (free(q); nitems--; free(q))
					q = *qp++;
				if (fchmod(lfd, stb.st_mode & ~LFM_RESET_QUE)
				    < 0)
					syslog(LOG_WARNING, "%s: %s: %m",
					       pp->printer, pp->lock_file);
				break;
			}
		}
		if (i == OK)		/* all files of this job printed */
			jobcount++;
		else if (i == REPRINT && ++errcnt < 5) {
			/* try reprinting the job */
			syslog(LOG_INFO, "restarting %s", pp->printer);
			if (ofilter > 0) {
				kill(ofilter, SIGCONT);	/* to be sure */
				(void) close(ofd);
				while ((i = wait(NULL)) > 0 && i != ofilter)
					;
				if (i < 0)
					syslog(LOG_WARNING, "%s: after kill(of=%d), wait() returned: %m",
					    pp->printer, ofilter);
				ofilter = 0;
			}
			(void) close(pfd);	/* close printer */
			if (ftruncate(lfd, pidoff) < 0)
				syslog(LOG_WARNING, "%s: %s: %m", 
				       pp->printer, pp->lock_file);
			openpr(pp);		/* try to reopen printer */
			goto restart;
		} else {
			syslog(LOG_WARNING, "%s: job could not be %s (%s)", 
			       pp->printer,
			       pp->remote ? "sent to remote host" : "printed",
			       q->job_cfname);
			if (i == REPRINT) {
				/* ensure we don't attempt this job again */
				(void) unlink(q->job_cfname);
				q->job_cfname[0] = 'd';
				(void) unlink(q->job_cfname);
				if (logname[0])
					sendmail(pp, logname, FATALERR);
			}
		}
	}
	free(queue);
	/*
	 * search the spool directory for more work.
	 */
	if ((nitems = getq(pp, &queue)) < 0) {
		syslog(LOG_ERR, "%s: can't scan %s", pp->printer,
		       pp->spool_dir);
		exit(1);
	}
	if (nitems == 0) {		/* no more work to do */
	done:
		if (jobcount > 0) {	/* jobs actually printed */
			if (!pp->no_formfeed && !pp->tof)
				(void) write(ofd, pp->form_feed,
					     strlen(pp->form_feed));
			if (pp->trailer != NULL) /* output trailer */
				(void) write(ofd, pp->trailer,
					     strlen(pp->trailer));
		}
		(void) close(ofd);
		(void) wait(NULL);
		(void) unlink(tempstderr);
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
printit(struct printer *pp, char *file)
{
	register int i;
	char *cp;
	int bombed, didignorehdr;

	bombed = OK;
	didignorehdr = 0;
	/*
	 * open control file; ignore if no longer there.
	 */
	if ((cfp = fopen(file, "r")) == NULL) {
		syslog(LOG_INFO, "%s: %s: %m", pp->printer, file);
		return(OK);
	}
	/*
	 * Reset troff fonts.
	 */
	for (i = 0; i < 4; i++)
		strcpy(fonts[i], ifonts[i]);
	sprintf(&width[2], "%ld", pp->page_width);
	strcpy(indent+2, "0");

	/* initialize job-specific count of datafiles processed */
	job_dfcnt = 0;
	
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
	 *              Z -- "locale" for pr
	 *
	 *      getline reads a line and expands tabs to blanks
	 */

	/* pass 1 */

	while (getline(cfp))
		switch (line[0]) {
		case 'H':
			strlcpy(origin_host, line + 1, sizeof(origin_host));
			if (class[0] == '\0') {
				strlcpy(class, line+1, sizeof(class));
			}
			continue;

		case 'P':
			strlcpy(logname, line + 1, sizeof(logname));
			if (pp->restricted) { /* restricted */
				if (getpwnam(logname) == NULL) {
					bombed = NOACCT;
					sendmail(pp, line+1, bombed);
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
			if (line[1] != '\0') {
				strlcpy(jobname, line + 1, sizeof(jobname));
			} else
				strcpy(jobname, " ");
			continue;

		case 'C':
			if (line[1] != '\0')
				strlcpy(class, line + 1, sizeof(class));
			else if (class[0] == '\0') {
				/* XXX - why call gethostname instead of
				 *       just strlcpy'ing local_host? */
				gethostname(class, sizeof(class));
				class[sizeof(class) - 1] = '\0';
			}
			continue;

		case 'T':	/* header title for pr */
			strlcpy(title, line + 1, sizeof(title));
			continue;

		case 'L':	/* identification line */
			if (!pp->no_header && !pp->header_last)
				banner(pp, line+1, jobname);
			continue;

		case '1':	/* troff fonts */
		case '2':
		case '3':
		case '4':
			if (line[1] != '\0') {
				strlcpy(fonts[line[0]-'1'], line + 1,
				    (size_t)50);
			}
			continue;

		case 'W':	/* page width */
			strlcpy(width+2, line + 1, sizeof(width) - 2);
			continue;

		case 'I':	/* indent amount */
			strlcpy(indent+2, line + 1, sizeof(indent) - 2);
			continue;

		case 'Z':       /* locale for pr */
			strlcpy(locale, line + 1, sizeof(locale));
			locale[sizeof(locale) - 1] = '\0';
			continue;

		default:	/* some file to print */
			/* only lowercase cmd-codes include a file-to-print */
			if ((line[0] < 'a') || (line[0] > 'z')) {
				/* ignore any other lines */
				if (lflag <= 1)
					continue;
				if (!didignorehdr) {
					syslog(LOG_INFO, "%s: in %s :",
					       pp->printer, file);
					didignorehdr = 1;
				}
				syslog(LOG_INFO, "%s: ignoring line: '%c' %s",
				       pp->printer, line[0], &line[1]);
				continue;
			}
			i = print(pp, line[0], line+1);
			switch (i) {
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
				sendmail(pp, logname, bombed);
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
			if (!pp->no_header && pp->header_last)
				banner(pp, line+1, jobname);
			continue;

		case 'M':
			if (bombed < NOACCT)	/* already sent if >= NOACCT */
				sendmail(pp, line+1, bombed);
			continue;

		case 'U':
			if (strchr(line+1, '/'))
				continue;
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
print(struct printer *pp, int format, char *file)
{
	register int n, i;
	register char *prog;
	int fi, fo;
	FILE *fp;
	char *av[15], buf[BUFSIZ];
	int pid, p[2], stopped;
	union wait status;
	struct stat stb;

	if (lstat(file, &stb) < 0 || (fi = open(file, O_RDONLY)) < 0) {
		syslog(LOG_INFO, "%s: unable to open %s ('%c' line)",
		       pp->printer, file, format);
		return(ERROR);
	}
	/*
	 * Check to see if data file is a symbolic link. If so, it should
	 * still point to the same file or someone is trying to print
	 * something he shouldn't.
	 */
	if ((stb.st_mode & S_IFMT) == S_IFLNK && fstat(fi, &stb) == 0 &&
	    (stb.st_dev != fdev || stb.st_ino != fino))
		return(ACCESS);

	job_dfcnt++;		/* increment datafile counter for this job */
	stopped = 0;		/* output filter is not stopped */

	/* everything seems OK, start it up */
	if (!pp->no_formfeed && !pp->tof) { /* start on a fresh page */
		(void) write(ofd, pp->form_feed, strlen(pp->form_feed));
		pp->tof = 1;
	}
	if (pp->filters[LPF_INPUT] == NULL
	    && (format == 'f' || format == 'l')) {
		pp->tof = 0;
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
		if (pp->filters[LPF_INPUT] == NULL) {	/* use output filter */
			prog = _PATH_PR;
			i = 0;
			av[i++] = "pr";
			av[i++] = width;
			av[i++] = length;
			av[i++] = "-h";
			av[i++] = *title ? title : " ";
			av[i++] = "-L";
			av[i++] = *locale ? locale : "C";
			av[i++] = "-F";
			av[i] = 0;
			fo = ofd;
			goto start;
		}
		pipe(p);
		if ((prchild = dofork(pp, DORETURN)) == 0) {	/* child */
			dup2(fi, 0);		/* file is stdin */
			dup2(p[1], 1);		/* pipe is stdout */
			closelog();
			closeallfds(3);
			execl(_PATH_PR, "pr", width, length,
			    "-h", *title ? title : " ",
			    "-L", *locale ? locale : "C",
			    "-F", (char *)0);
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
		prog = pp->filters[LPF_INPUT];
		av[1] = width;
		av[2] = length;
		av[3] = indent;
		n = 4;
		break;
	case 'l':	/* like 'f' but pass control characters */
		prog = pp->filters[LPF_INPUT];
		av[1] = "-c";
		av[2] = width;
		av[3] = length;
		av[4] = indent;
		n = 5;
		break;
	case 'r':	/* print a fortran text file */
		prog = pp->filters[LPF_FORTRAN];
		av[1] = width;
		av[2] = length;
		n = 3;
		break;
	case 't':	/* print troff output */
	case 'n':	/* print ditroff output */
	case 'd':	/* print tex output */
		(void) unlink(".railmag");
		if ((fo = creat(".railmag", FILMOD)) < 0) {
			syslog(LOG_ERR, "%s: cannot create .railmag", 
			       pp->printer);
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
		prog = (format == 't') ? pp->filters[LPF_TROFF] 
			: ((format == 'n') ? pp->filters[LPF_DITROFF]
			   : pp->filters[LPF_DVI]);
		av[1] = pxwidth;
		av[2] = pxlength;
		n = 3;
		break;
	case 'c':	/* print cifplot output */
		prog = pp->filters[LPF_CIFPLOT];
		av[1] = pxwidth;
		av[2] = pxlength;
		n = 3;
		break;
	case 'g':	/* print plot(1G) output */
		prog = pp->filters[LPF_GRAPH];
		av[1] = pxwidth;
		av[2] = pxlength;
		n = 3;
		break;
	case 'v':	/* print raster output */
		prog = pp->filters[LPF_RASTER];
		av[1] = pxwidth;
		av[2] = pxlength;
		n = 3;
		break;
	default:
		(void) close(fi);
		syslog(LOG_ERR, "%s: illegal format character '%c'",
			pp->printer, format);
		return(ERROR);
	}
	if (prog == NULL) {
		(void) close(fi);
		syslog(LOG_ERR,
		   "%s: no filter found in printcap for format character '%c'",
		   pp->printer, format);
		return(ERROR);
	}
	if ((av[0] = strrchr(prog, '/')) != NULL)
		av[0]++;
	else
		av[0] = prog;
	av[n++] = "-n";
	av[n++] = logname;
	av[n++] = "-h";
	av[n++] = origin_host;
	av[n++] = pp->acct_file;
	av[n] = 0;
	fo = pfd;
	if (ofilter > 0) {		/* stop output filter */
		write(ofd, "\031\1", 2);
		while ((pid =
		    wait3((int *)&status, WUNTRACED, 0)) > 0 && pid != ofilter)
			;
		if (pid < 0)
			syslog(LOG_WARNING, "%s: after stopping 'of', wait3() returned: %m",
			    pp->printer);
		else if (status.w_stopval != WSTOPPED) {
			(void) close(fi);
			syslog(LOG_WARNING,
			       "%s: output filter died "
			       "(pid=%d retcode=%d termsig=%d)",
				pp->printer, ofilter, status.w_retcode,
			       status.w_termsig);
			return(REPRINT);
		}
		stopped++;
	}
start:
	if ((child = dofork(pp, DORETURN)) == 0) { /* child */
		dup2(fi, 0);
		dup2(fo, 1);
		/* setup stderr for the filter (child process)
		 * so it goes to our temporary errors file */
		n = open(tempstderr, O_WRONLY|O_TRUNC, 0664);
		if (n >= 0)
			dup2(n, 2);
		closelog();
		closeallfds(3);
		execv(prog, av);
		syslog(LOG_ERR, "cannot execv %s", prog);
		exit(2);
	}
	(void) close(fi);
	if (child < 0)
		status.w_retcode = 100;
	else {
		while ((pid = wait((int *)&status)) > 0 && pid != child)
			;
		if (pid < 0) {
			status.w_retcode = 100;
			syslog(LOG_WARNING, "%s: after execv(%s), wait() returned: %m",
			    pp->printer, prog);
		}
	}
	child = 0;
	prchild = 0;
	if (stopped) {		/* restart output filter */
		if (kill(ofilter, SIGCONT) < 0) {
			syslog(LOG_ERR, "cannot restart output filter");
			exit(1);
		}
	}
	pp->tof = 0;

	/* Copy the filter's output to "lf" logfile */
	if ((fp = fopen(tempstderr, "r"))) {
		while (fgets(buf, sizeof(buf), fp))
			fputs(buf, stderr);
		fclose(fp);
	}

	if (!WIFEXITED(status)) {
		syslog(LOG_WARNING, "%s: filter '%c' terminated (termsig=%d)",
			pp->printer, format, status.w_termsig);
		return(ERROR);
	}
	switch (status.w_retcode) {
	case 0:
		pp->tof = 1;
		return(OK);
	case 1:
		return(REPRINT);
	case 2:
		return(ERROR);
	default:
		syslog(LOG_WARNING, "%s: filter '%c' exited (retcode=%d)",
			pp->printer, format, status.w_retcode);
		return(FILTERERR);
	}
}

/*
 * Send the daemon control file (cf) and any data files.
 * Return -1 if a non-recoverable error occured, 1 if a recoverable error and
 * 0 if all is well.
 */
static int
sendit(struct printer *pp, char *file)
{
	register int i, err = OK;
	char *cp, last[BUFSIZ];

	/*
	 * open control file
	 */
	if ((cfp = fopen(file, "r")) == NULL)
		return(OK);

	/* initialize job-specific count of datafiles processed */
	job_dfcnt = 0;

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
			strlcpy(origin_host, line + 1, sizeof(origin_host));
			if (class[0] == '\0') {
				strlcpy(class, line + 1, sizeof(class));
			}
		} else if (line[0] == 'P') {
			strlcpy(logname, line + 1, sizeof(logname));
			if (pp->restricted) { /* restricted */
				if (getpwnam(logname) == NULL) {
					sendmail(pp, line+1, NOACCT);
					err = ERROR;
					break;
				}
			}
		} else if (line[0] == 'I') {
			strlcpy(indent+2, line + 1, sizeof(indent) - 2);
		} else if (line[0] >= 'a' && line[0] <= 'z') {
			strcpy(last, line);
			while ((i = getline(cfp)) != 0)
				if (strcmp(last, line))
					break;
			switch (sendfile(pp, '\3', last+1, *last)) {
			case OK:
				if (i)
					goto again;
				break;
			case REPRINT:
				(void) fclose(cfp);
				return(REPRINT);
			case ACCESS:
				sendmail(pp, logname, ACCESS);
			case ERROR:
				err = ERROR;
			}
			break;
		}
	}
	if (err == OK && sendfile(pp, '\2', file, '\0') > 0) {
		(void) fclose(cfp);
		return(REPRINT);
	}
	/*
	 * pass 2
	 */
	fseek(cfp, 0L, 0);
	while (getline(cfp))
		if (line[0] == 'U' && !strchr(line+1, '/'))
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
sendfile(struct printer *pp, int type, char *file, char format)
{
	register int f, i, amt;
	struct stat stb;
	FILE *fp;
	char buf[BUFSIZ];
	int closedpr, resp, sizerr, statrc;

	statrc = lstat(file, &stb);
	if (statrc < 0) {
		syslog(LOG_ERR, "%s: error from lstat(%s): %m",
		    pp->printer, file);
		return(ERROR);
	}
	f = open(file, O_RDONLY);
	if (f < 0) {
		syslog(LOG_ERR, "%s: error from open(%s,O_RDONLY): %m",
		    pp->printer, file);
		return(ERROR);
	}
	/*
	 * Check to see if data file is a symbolic link. If so, it should
	 * still point to the same file or someone is trying to print something
	 * he shouldn't.
	 */
	if ((stb.st_mode & S_IFMT) == S_IFLNK && fstat(f, &stb) == 0 &&
	    (stb.st_dev != fdev || stb.st_ino != fino))
		return(ACCESS);

	job_dfcnt++;		/* increment datafile counter for this job */

	/* everything seems OK, start it up */
	sizerr = 0;
	closedpr = 0;
	if (type == '\3') {
		if (pp->filters[LPF_INPUT]) {
			/*
			 * We're sending something with an ifilter.  We have to
			 * run the ifilter and store the output as a temporary
			 * spool file (tfile...), because the protocol requires
			 * us to send the file size before we start sending any
			 * of the data.
			 */
			char *av[15];
			int n;
			int ifilter;
			union wait status; /* XXX */

			strcpy(tfile,TFILENAME);
			if ((tfd = mkstemp(tfile)) == -1) {
				syslog(LOG_ERR, "mkstemp: %m");
				return(ERROR);
			}
			if ((av[0] = strrchr(pp->filters[LPF_INPUT], '/')) == NULL)
				av[0] = pp->filters[LPF_INPUT];
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
			av[++n] = origin_host;
			av[++n] = pp->acct_file;
			av[++n] = 0;
			if ((ifilter = dofork(pp, DORETURN)) == 0) { /* child */
				dup2(f, 0);
				dup2(tfd, 1);
				/* setup stderr for the filter (child process)
				 * so it goes to our temporary errors file */
				n = open(tempstderr, O_WRONLY|O_TRUNC, 0664);
				if (n >= 0)
					dup2(n, 2);
				closelog();
				closeallfds(3);
				execv(pp->filters[LPF_INPUT], av);
				syslog(LOG_ERR, "cannot execv %s", 
				       pp->filters[LPF_INPUT]);
				exit(2);
			}
			(void) close(f);
			if (ifilter < 0)
				status.w_retcode = 100;
			else {
				while ((pid = wait((int *)&status)) > 0 &&
					pid != ifilter)
					;
				if (pid < 0) {
					status.w_retcode = 100;
					syslog(LOG_WARNING, "%s: after execv(%s), wait() returned: %m",
					    pp->printer, pp->filters[LPF_INPUT]);
				}
			}
			/* Copy the filter's output to "lf" logfile */
			if ((fp = fopen(tempstderr, "r"))) {
				while (fgets(buf, sizeof(buf), fp))
					fputs(buf, stderr);
				fclose(fp);
			}
			/* process the return-code from the filter */
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
					pp->printer, format, status.w_retcode);
				unlink(tfile);
				return(FILTERERR);
			}
			statrc = fstat(tfd, &stb);   /* to find size of tfile */
			if (statrc < 0)	{
				syslog(LOG_ERR, "%s: error processing 'if', fstat(%s): %m",
				    pp->printer, tfile);
				return(ERROR);
			}
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
			if (i < 0)
				syslog(LOG_WARNING, "%s: after closing 'of', wait() returned: %m",
				    pp->printer);
			ofilter = 0;
			statrc = fstat(tfd, &stb);   /* to find size of tfile */
			if (statrc < 0)	{
				syslog(LOG_ERR, "%s: error processing 'of', fstat(%s): %m",
				    pp->printer, tfile);
				openpr(pp);
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
		    (resp = response(pp)) < 0 || resp == '\1') {
			(void) close(f);
			if (tfd != -1 && type == '\3') {
				tfd = -1;
				unlink(tfile);
				if (closedpr)
					openpr(pp);
			}
			return(REPRINT);
		} else if (resp == '\0')
			break;
		if (i == 0)
			pstatus(pp,
				"no space on remote; waiting for queue to drain");
		if (i == 10)
			syslog(LOG_ALERT, "%s: can't send to %s; queue full",
				pp->printer, pp->remote_host);
		sleep(5 * 60);
	}
	if (i)
		pstatus(pp, "sending to %s", pp->remote_host);
	if (type == '\3')
		trstat_init(pp, file, job_dfcnt);
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
					openpr(pp);
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
		syslog(LOG_INFO, "%s: %s: changed size", pp->printer, file);
		/* tell recvjob to ignore this file */
		(void) write(pfd, "\1", 1);
		if (closedpr)
			openpr(pp);
		return(ERROR);
	}
	if (write(pfd, "", 1) != 1 || response(pp)) {
		if (closedpr)
			openpr(pp);
		return(REPRINT);
	}
	if (closedpr)
		openpr(pp);
	if (type == '\3')
		trstat_write(pp, TR_SENDING, stb.st_size, logname,
				 pp->remote_host, origin_host);
	return(OK);
}

/*
 * Check to make sure there have been no errors and that both programs
 * are in sync with eachother.
 * Return non-zero if the connection was lost.
 */
static char
response(const struct printer *pp)
{
	char resp;

	if (read(pfd, &resp, 1) != 1) {
		syslog(LOG_INFO, "%s: lost connection", pp->printer);
		return(-1);
	}
	return(resp);
}

/*
 * Banner printing stuff
 */
static void
banner(struct printer *pp, char *name1, char *name2)
{
	time_t tvec;

	time(&tvec);
	if (!pp->no_formfeed && !pp->tof)
		(void) write(ofd, pp->form_feed, strlen(pp->form_feed));
	if (pp->short_banner) {	/* short banner only */
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
		scan_out(pp, ofd, name1, '\0');
		(void) write(ofd, "\n\n", 2);
		scan_out(pp, ofd, name2, '\0');
		if (class[0]) {
			(void) write(ofd,"\n\n\n",3);
			scan_out(pp, ofd, class, '\0');
		}
		(void) write(ofd, "\n\n\n\n\t\t\t\t\tJob:  ", 15);
		(void) write(ofd, name2, strlen(name2));
		(void) write(ofd, "\n\t\t\t\t\tDate: ", 12);
		(void) write(ofd, ctime(&tvec), 24);
		(void) write(ofd, "\n", 1);
	}
	if (!pp->no_formfeed)
		(void) write(ofd, pp->form_feed, strlen(pp->form_feed));
	pp->tof = 1;
}

static char *
scnline(int key, char *p, int c)
{
	register int scnwidth;

	for (scnwidth = WIDTH; --scnwidth;) {
		key <<= 1;
		*p++ = key & 0200 ? c : BACKGND;
	}
	return (p);
}

#define TRC(q)	(((q)-' ')&0177)

static void
scan_out(struct printer *pp, int scfd, char *scsp, int dlm)
{
	register char *strp;
	register int nchrs, j;
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
				strp = scnline(scnkey[(int)c][scnhgt-1-d], strp, cc);
			if (*sp == dlm || *sp == '\0' || 
			    nchrs++ >= pp->page_width/(WIDTH+1)-1)
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
dropit(int c)
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
sendmail(struct printer *pp, char *user, int bombed)
{
	register int i;
	int p[2], s;
	register const char *cp;
	struct stat stb;
	FILE *fp;

	pipe(p);
	if ((s = dofork(pp, DORETURN)) == 0) {		/* child */
		dup2(p[0], 0);
		closelog();
		closeallfds(3);
		if ((cp = strrchr(_PATH_SENDMAIL, '/')) != NULL)
			cp++;
		else
			cp = _PATH_SENDMAIL;
		execl(_PATH_SENDMAIL, cp, "-t", (char *)0);
		_exit(0);
	} else if (s > 0) {				/* parent */
		dup2(p[1], 1);
		printf("To: %s@%s\n", user, origin_host);
		printf("Subject: %s printer job \"%s\"\n", pp->printer,
			*jobname ? jobname : "<unknown>");
		printf("Reply-To: root@%s\n\n", local_host);
		printf("Your printer job ");
		if (*jobname)
			printf("(%s) ", jobname);
		
		cp = "XXX compiler confusion"; /* XXX shut GCC up */
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
			printf("\ncould not be printed without an account on %s\n",
			    local_host);
			cp = "NOACCT";
			break;
		case FILTERERR:
			if (stat(tempstderr, &stb) < 0 || stb.st_size == 0
			    || (fp = fopen(tempstderr, "r")) == NULL) {
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
	} else {
		syslog(LOG_WARNING, "unable to send mail to %s: %m", user);
		return;
	}
	(void) close(p[0]);
	(void) close(p[1]);
	wait(NULL);
	syslog(LOG_INFO, "mail sent to user %s about job %s on printer %s (%s)",
		user, *jobname ? jobname : "<unknown>", pp->printer, cp);
}

/*
 * dofork - fork with retries on failure
 */
static int
dofork(const struct printer *pp, int action)
{
	int i, fail, forkpid;
	struct passwd *pwd;

	forkpid = -1;
	if (daemon_uname == NULL) {
		pwd = getpwuid(pp->daemon_user);
		if (pwd == NULL) {
			syslog(LOG_ERR, "%s: Can't lookup default daemon uid (%ld) in password file",
			    pp->printer, pp->daemon_user);
			goto error_ret;
		}
		daemon_uname = strdup(pwd->pw_name);
		daemon_defgid = pwd->pw_gid;
	}

	for (i = 0; i < 20; i++) {
		forkpid = fork();
		if (forkpid < 0) {
			sleep((unsigned)(i*i));
			continue;
		}
		/*
		 * Child should run as daemon instead of root
		 */
		if (forkpid == 0) {
			errno = 0;
			fail = initgroups(daemon_uname, daemon_defgid);
			if (fail) {
				syslog(LOG_ERR, "%s: initgroups(%s,%u): %m",
				    pp->printer, daemon_uname, daemon_defgid);
				break;
			}
			fail = setgid(daemon_defgid);
			if (fail) {
				syslog(LOG_ERR, "%s: setgid(%u): %m",
				    pp->printer, daemon_defgid);
				break;
			}
			fail = setuid(pp->daemon_user);
			if (fail) {
				syslog(LOG_ERR, "%s: setuid(%ld): %m",
				    pp->printer, pp->daemon_user);
				break;
			}
		}
		return forkpid;
	}

	/*
	 * An error occurred.  If the error is in the child process, then
	 * this routine MUST always exit().  DORETURN only effects how
	 * errors should be handled in the parent process.
	 */
error_ret:
	if (forkpid == 0) {
		syslog(LOG_ERR, "%s: dofork(): aborting child process...",
		    pp->printer);
		exit(1);
	}
	syslog(LOG_ERR, "%s: dofork(): failure in fork", pp->printer);

	sleep(1);		/* throttle errors, as a safety measure */
	switch (action) {
	case DORETURN:
		return -1;
	default:
		syslog(LOG_ERR, "bad action (%d) to dofork", action);
		/* FALLTHROUGH */
	case DOABORT:
		exit(1);
	}
	/*NOTREACHED*/
}

/*
 * Kill child processes to abort current job.
 */
static void
abortpr(int signo __unused)
{

	(void) unlink(tempstderr);
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
init(struct printer *pp)
{
	char *s;

	sprintf(&width[2], "%ld", pp->page_width);
	sprintf(&length[2], "%ld", pp->page_length);
	sprintf(&pxwidth[2], "%ld", pp->page_pwidth);
	sprintf(&pxlength[2], "%ld", pp->page_plength);
	if ((s = checkremote(pp)) != 0) {
		syslog(LOG_WARNING, "%s", s);
		free(s);
	}
}

void
startprinting(const char *printer)
{
	struct printer myprinter, *pp = &myprinter;
	int status;

	init_printer(pp);
	status = getprintcap(printer, pp);
	switch(status) {
	case PCAPERR_OSERR:
		syslog(LOG_ERR, "can't open printer description file: %m");
		exit(1);
	case PCAPERR_NOTFOUND:
		syslog(LOG_ERR, "unknown printer: %s", printer);
		exit(1);
	case PCAPERR_TCLOOP:
		fatal(pp, "potential reference loop detected in printcap file");
	default:
		break;
	}
	printjob(pp);
}

/*
 * Acquire line printer or remote connection.
 */
static void
openpr(const struct printer *pp)
{
	int p[2];
	char *cp;

	if (pp->remote) {
		openrem(pp);
	} else if (*pp->lp) {
		if ((cp = strchr(pp->lp, '@')) != NULL)
			opennet(pp);
		else
			opentty(pp);
	} else {
		syslog(LOG_ERR, "%s: no line printer device or host name",
			pp->printer);
		exit(1);
	}

	/*
	 * Start up an output filter, if needed.
	 */
	if (pp->filters[LPF_OUTPUT] && !pp->filters[LPF_INPUT] && !ofilter) {
		pipe(p);
		if (pp->remote) {
			strcpy(tfile, TFILENAME);
			tfd = mkstemp(tfile);
		}
		if ((ofilter = dofork(pp, DOABORT)) == 0) {	/* child */
			dup2(p[0], 0);		/* pipe is std in */
			/* tfile/printer is stdout */
			dup2(pp->remote ? tfd : pfd, 1);
			closelog();
			closeallfds(3);
			if ((cp = strrchr(pp->filters[LPF_OUTPUT], '/')) == NULL)
				cp = pp->filters[LPF_OUTPUT];
			else
				cp++;
			execl(pp->filters[LPF_OUTPUT], cp, width, length,
			      (char *)0);
			syslog(LOG_ERR, "%s: %s: %m", pp->printer, 
			       pp->filters[LPF_OUTPUT]);
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
opennet(const struct printer *pp)
{
	register int i;
	int resp;
	u_long port;
	char *ep;
	void (*savealrm)(int);

	port = strtoul(pp->lp, &ep, 0);
	if (*ep != '@' || port > 65535) {
		syslog(LOG_ERR, "%s: bad port number: %s", pp->printer,
		       pp->lp);
		exit(1);
	}
	ep++;

	for (i = 1; ; i = i < 256 ? i << 1 : i) {
		resp = -1;
		savealrm = signal(SIGALRM, alarmhandler);
		alarm(pp->conn_timeout);
		pfd = getport(pp, ep, port);
		alarm(0);
		(void)signal(SIGALRM, savealrm);
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
				pstatus(pp, "waiting for %s to come up",
					pp->lp);
			else
				pstatus(pp, 
					"waiting for access to printer on %s",
					pp->lp);
		}
		sleep(i);
	}
	pstatus(pp, "sending to %s port %lu", ep, port);
}

/*
 * Printer is connected to an RS232 port on this host
 */
static void
opentty(const struct printer *pp)
{
	register int i;

	for (i = 1; ; i = i < 32 ? i << 1 : i) {
		pfd = open(pp->lp, pp->rw ? O_RDWR : O_WRONLY);
		if (pfd >= 0) {
			delay(500);
			break;
		}
		if (errno == ENOENT) {
			syslog(LOG_ERR, "%s: %m", pp->lp);
			exit(1);
		}
		if (i == 1)
			pstatus(pp, 
				"waiting for %s to become ready (offline?)",
				pp->printer);
		sleep(i);
	}
	if (isatty(pfd))
		setty(pp);
	pstatus(pp, "%s is ready and printing", pp->printer);
}

/*
 * Printer is on a remote host
 */
static void
openrem(const struct printer *pp)
{
	register int i;
	int resp;
	void (*savealrm)(int);

	for (i = 1; ; i = i < 256 ? i << 1 : i) {
		resp = -1;
		savealrm = signal(SIGALRM, alarmhandler);
		alarm(pp->conn_timeout);
		pfd = getport(pp, pp->remote_host, 0);
		alarm(0);
		(void)signal(SIGALRM, savealrm);
		if (pfd >= 0) {
			if ((writel(pfd, "\2", pp->remote_queue, "\n", 
				    (char *)0)
			     == 2 + strlen(pp->remote_queue))
			    && (resp = response(pp)) == 0)
				break;
			(void) close(pfd);
		}
		if (i == 1) {
			if (resp < 0)
				pstatus(pp, "waiting for %s to come up", 
					pp->remote_host);
			else {
				pstatus(pp,
					"waiting for queue to be enabled on %s",
					pp->remote_host);
				i = 256;
			}
		}
		sleep(i);
	}
	pstatus(pp, "sending to %s", pp->remote_host);
}

/*
 * setup tty lines.
 */
static void
setty(const struct printer *pp)
{
	struct termios ttybuf;

	if (ioctl(pfd, TIOCEXCL, (char *)0) < 0) {
		syslog(LOG_ERR, "%s: ioctl(TIOCEXCL): %m", pp->printer);
		exit(1);
	}
	if (tcgetattr(pfd, &ttybuf) < 0) {
		syslog(LOG_ERR, "%s: tcgetattr: %m", pp->printer);
		exit(1);
	}
	if (pp->baud_rate > 0)
		cfsetspeed(&ttybuf, pp->baud_rate);
	if (pp->mode_set) {
		char *s = strdup(pp->mode_set), *tmp;

		while ((tmp = strsep(&s, ",")) != NULL) {
			(void) msearch(tmp, &ttybuf);
		}
	}
	if (pp->mode_set != 0 || pp->baud_rate > 0) {
		if (tcsetattr(pfd, TCSAFLUSH, &ttybuf) == -1) {
			syslog(LOG_ERR, "%s: tcsetattr: %m", pp->printer);
		}
	}
}

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

static void
#ifdef __STDC__
pstatus(const struct printer *pp, const char *msg, ...)
#else
pstatus(pp, msg, va_alist)
	const struct printer *pp;
	char *msg;
        va_dcl
#endif
{
	int fd;
	char *buf;
	va_list ap;
#ifdef __STDC__
	va_start(ap, msg);
#else
	va_start(ap);
#endif

	umask(0);
	fd = open(pp->status_file, O_WRONLY|O_CREAT|O_EXLOCK, STAT_FILE_MODE);
	if (fd < 0) {
		syslog(LOG_ERR, "%s: %s: %m", pp->printer, pp->status_file);
		exit(1);
	}
	ftruncate(fd, 0);
	vasprintf(&buf, msg, ap);
	va_end(ap);
	writel(fd, buf, "\n", (char *)0);
	close(fd);
	free(buf);
}

void
alarmhandler(int signo __unused)
{
	/* the signal is ignored */
	/* (the '__unused' is just to avoid a compile-time warning) */
}
