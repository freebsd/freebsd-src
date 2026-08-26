/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include "lp.cdefs.h"		/* A cross-platform version of <sys/cdefs.h> */
/*
 * Receive printer jobs from the network, queue them and
 * start the printer daemon.
 */
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <signal.h>
#include <stdarg.h>
#include <stdckdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include "lp.h"
#include "lp.local.h"
#include "ctlinfo.h"
#include "extern.h"
#include "pathnames.h"

#define digit(ch) ((ch) >= '0' && (ch) <= '9')

/*
 * The buffer size to use when reading/writing spool files.
 */
#define	SPL_BUFSIZ	BUFSIZ

static char	 dfname[NAME_MAX];	/* data files */
static size_t	 minfree;       /* keep at least minfree blocks available */
static char	 tfname[NAME_MAX];	/* tmp copy of cf before linking */

static void	 ack(struct printer *_pp);
static void	 nak(struct printer *_pp);
static int	 chksize(size_t _size);
static void	 frecverr(const char *_msg, ...) __printf0like(1, 2);
static int	 noresponse(void);
static void	 rcleanup(int _signo);
static void	 read_minfree(void);
static int	 readfile(struct printer *_pp, char *_file, size_t _size);
static int	 readjob(struct printer *_pp);


void
recvjob(const char *printer)
{
	struct stat stb;
	int status;
	struct printer myprinter, *pp = &myprinter;

	/*
	 * Perform lookup for printer name or abbreviation
	 */
	init_printer(pp);
	status = getprintcap(printer, pp);
	switch (status) {
	case PCAPERR_OSERR:
		frecverr("cannot open printer description file");
		break;
	case PCAPERR_NOTFOUND:
		frecverr("unknown printer %s", printer);
		break;
	case PCAPERR_TCLOOP:
		fatal(pp, "potential reference loop detected in printcap file");
	default:
		break;
	}
	
	(void) close(STDERR_FILENO);			/* set up log file */
	if (open(pp->log_file, O_WRONLY|O_APPEND, 0664) < 0) {
		syslog(LOG_ERR, "%s: %m", pp->log_file);
		(void) open(_PATH_DEVNULL, O_WRONLY);
	}

	if (chdir(pp->spool_dir) < 0)
		frecverr("%s: chdir(%s): %s", pp->printer, pp->spool_dir,
		    strerror(errno));
	if (stat(pp->lock_file, &stb) == 0) {
		if (stb.st_mode & 010) {
			/* queue is disabled */
			putchar('\1');		/* return error code */
			exit(1);
		}
	} else if (stat(pp->spool_dir, &stb) < 0)
		frecverr("%s: stat(%s): %s", pp->printer, pp->spool_dir,
		    strerror(errno));
	read_minfree();
	signal(SIGTERM, rcleanup);
	signal(SIGPIPE, rcleanup);

	if (readjob(pp))
		printjob(pp);
}

/*
 * Read printer jobs sent by lpd and copy them to the spooling directory.
 * Return the number of jobs successfully transferred.
 *
 * In theory, the protocol allows any number of jobs to be transferred in
 * a single connection, with control and data files in any order.  This
 * would be very difficult to police effectively, so enforce a single job
 * per connection.  The control file can be sent at any time (first, last,
 * or between data files).  All files must bear the same job number, and
 * the data files must be sent sequentially.  If any of these requirements
 * is violated, we close the connection and delete everything.
 *
 * Note that RFC 1179 strongly implies only one data file per job; I
 * assume this is a mistake in the RFC since it is supposed to describe
 * this code, which predates it, but was written by a third party.
 */
static int
readjob(struct printer *pp)
{
	ssize_t rlen;
	size_t len, size;
	int cfcnt, dfcnt, curd0, curd2, curn, n;
	char *cp, *clastp, *errmsg, *sep;
	char givenid[32], givenhost[MAXHOSTNAMELEN];

	tfname[0] = dfname[0] = '\0';
	ack(pp);
	cfcnt = 0;
	dfcnt = 0;
	curd0 = 'd';
	curd2 = 'A';
	curn = -1;
	for (;;) {
		/*
		 * Read a command to tell us what to do
		 */
		cp = line;
		clastp = line + sizeof(line) - 1;
		do {
			rlen = read(STDOUT_FILENO, cp, 1);
			if (rlen != 1) {
				if (rlen < 0) {
					frecverr("%s: lost connection",
					    pp->printer);
					/*NOTREACHED*/
				}
				return (cfcnt);
			}
		} while (*cp++ != '\n' && cp <= clastp);
		if (cp > clastp) {
			frecverr("%s: readjob overflow", pp->printer);
			/*NOTREACHED*/
		}
		*--cp = '\0';
		cp = line;
		switch (*cp++) {
		case '\1':	/* abort print job */
			rcleanup(0);
			continue;

		case '\2':	/* read control file */
			if (tfname[0] != '\0') {
				/* multiple control files */
				break;
			}
			for (size = 0; digit(*cp); cp++) {
				if (ckd_mul(&size, size, 10) ||
				    ckd_add(&size, size, *cp - '0'))
					break;
			}
			if (*cp++ != ' ')
				break;
			/*
			 * The next six bytes must be "cfA" followed by a
			 * three-digit job number.  The rest of the line
			 * is the client host name, but we substitute the
			 * host name we've already authenticated.
			 */
			if (cp[0] != 'c' || cp[1] != 'f' || cp[2] != 'A' ||
			    !digit(cp[3]) || !digit(cp[4]) || !digit(cp[5]))
				break;
			n = (cp[3] - '0') * 100 + (cp[4] - '0') * 10 +
			    cp[5] - '0';
			if (curn == -1)
				curn = n;
			else if (curn != n)
				break;
			len = snprintf(tfname, sizeof(tfname), "%.6s%s", cp,
			    from_host);
			if (len >= sizeof(tfname))
				break;
			tfname[0] = 't';
			if (!chksize(size)) {
				nak(pp);
				continue;
			}
			if (!readfile(pp, tfname, size)) {
				rcleanup(0);
				continue;
			}
			errmsg = ctl_renametf(pp->printer, tfname);
			tfname[0] = '\0';
			if (errmsg != NULL) {
				frecverr("%s: %s", pp->printer, errmsg);
				/*NOTREACHED*/
			}
			cfcnt++;
			continue;

		case '\3':	/* read data file */
			if (curd0 > 'z') {
				/* too many data files */
				break;
			}
			*givenid = '\0';
			*givenhost = '\0';
			for (size = 0; digit(*cp); cp++) {
				if (ckd_mul(&size, size, 10) ||
				    ckd_add(&size, size, *cp - '0'))
					break;
			}
			if (*cp++ != ' ')
				break;
			/*
			 * The next six bytes must be curd0, 'f', curd2
			 * followed by a three-digit job number, where
			 * curd2 cycles through [A-Za-z] while curd0
			 * starts at 'd' and increments when curd2 rolls
			 * around.  The rest of the line is the client
			 * host name, but we substitute the host name
			 * we've already authenticated.
			 */
			if (cp[0] != curd0 || cp[1] != 'f' || cp[2] != curd2 ||
			    !digit(cp[3]) || !digit(cp[4]) || !digit(cp[5]))
				break;
			n = (cp[3] - '0') * 100 + (cp[4] - '0') * 10 +
			    cp[5] - '0';
			if (curn == -1)
				curn = n;
			else if (curn != n)
				break;
			len = snprintf(dfname, sizeof(dfname), "%.6s%s", cp,
			    from_host);
			if (len >= sizeof(dfname))
				break;
			if (!chksize(size)) {
				nak(pp);
				continue;
			}
			switch (curd2++) {
			case 'Z':
				curd2 = 'a';
				break;
			case 'z':
				curd0++;
				curd2 = 'A';
				break;
			}
			dfcnt++;
			trstat_init(pp, dfname, dfcnt);
			if (!readfile(pp, dfname, size)) {
				rcleanup(0);
				continue;
			}
			trstat_write(pp, TR_RECVING, size, givenid,
			    from_host, givenhost);
			continue;
		}
		frecverr("protocol screwup: %s", line);
		/*NOTREACHED*/
	}
}

/*
 * Read files send by lpd and copy them to the spooling directory.
 */
static int
readfile(struct printer *pp, char *file, size_t size)
{
	char buf[SPL_BUFSIZ];
	ssize_t rlen, wlen;
	int err, f0, fd, j;

	fd = open(file, O_CREAT|O_EXCL|O_WRONLY, FILMOD);
	if (fd < 0) {
		/*
		 * We need to pass the file name to frecverr() so it can
		 * log an error, but frecverr() will then call rcleanup()
		 * which will delete the file if we don't zero out the
		 * first character.
		 */
		f0 = file[0];
		file[0] = '\0';
		frecverr("%s: readfile: error on open(%c%s): %s",
		    pp->printer, f0, file + 1, strerror(errno));
		/*NOTREACHED*/
	}
	ack(pp);
	while (size > 0) {
		rlen = read(STDOUT_FILENO, buf, MIN(SPL_BUFSIZ, size));
		if (rlen < 0 && errno == EINTR)
			continue;
		if (rlen <= 0) {
			frecverr("%s: lost connection", pp->printer);
			/*NOTREACHED*/
		}
		size -= rlen;
		while (rlen > 0) {
			wlen = write(fd, buf, rlen);
			if (wlen < 0 && errno == EINTR)
				continue;
			if (wlen <= 0) {
				frecverr("%s: write error on write(%s)",
				    pp->printer, file);
				/*NOTREACHED*/
			}
			rlen -= wlen;
		}
	}
	if (close(fd) != 0) {
		frecverr("%s: write error on close(%s)", pp->printer, file);
		/*NOTREACHED*/
	}
	if (noresponse()) {		/* file sent had bad data in it */
		(void) unlink(file);
		return (0);
	}
	ack(pp);
	return (1);
}

static int
noresponse(void)
{
	char resp;

	if (read(STDOUT_FILENO, &resp, 1) != 1) {
		frecverr("lost connection in noresponse()");
		/*NOTREACHED*/
	}
	if (resp == '\0')
		return (0);
	return (1);
}

/*
 * Check to see if there is enough space on the disk for size bytes.
 * 1 == OK, 0 == Not OK.
 */
static int
chksize(size_t size)
{
	struct statfs sfb;
	size_t avail;

	if (statfs(".", &sfb) < 0) {
		syslog(LOG_ERR, "%s: %m", "statfs(\".\")");
		return (1);
	}
	/* more free space than we can count; possible on 32-bit */
	if (ckd_mul(&avail, sfb.f_bavail, (sfb.f_bsize / 512)))
		return (1);
	/* already at or below minfree */
	if (avail <= minfree)
		return (0);
	/* not enough space */
	if (avail - minfree <= size / 512)
		return (0);
	return (1);
}

static void
read_minfree(void)
{
	FILE *fp;

	minfree = 0;
	/* read from disk */
	if ((fp = fopen("minfree", "r")) != NULL) {
		if (fscanf(fp, "%zu", &minfree) != 1)
			minfree = 0;
		fclose(fp);
	}
	/* scale kB to 512-byte blocks */
	if (ckd_mul(&minfree, minfree, 2))
		minfree = SIZE_MAX;
}

/*
 * Remove all the files associated with the current job being transferred.
 */
static void
rcleanup(int signo __unused)
{
	if (tfname[0] && strchr(tfname, '/') == NULL)
		(void) unlink(tfname);
	if (dfname[0] && strchr(dfname, '/') == NULL) {
		do {
			do
				(void) unlink(dfname);
			while (dfname[2]-- != 'A');
			dfname[2] = 'z';
		} while (dfname[0]-- != 'd');
	}
	dfname[0] = '\0';
}

static void
frecverr(const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	syslog(LOG_ERR, "Error receiving job from %s:", from_host);
	vsyslog(LOG_ERR, msg, ap);
	va_end(ap);
	/*
	 * rcleanup is not called until AFTER logging the error message,
	 * because rcleanup will zap some variables which may have been
	 * supplied as parameters for that msg...
	 */
	rcleanup(0);
	/* 
	 * Add a minimal delay before returning the final error code to
	 * the sending host.  This just in case that machine responds
	 * this error by INSTANTLY retrying (and instantly re-failing...).
	 * It would be stupid of the sending host to do that, but if there
	 * was a broken implementation which did it, the result might be
	 * obscure performance problems and a flood of syslog messages on
	 * the receiving host.
	 */ 
	sleep(2);		/* a paranoid throttling measure */
	putchar('\1');		/* return error code */
	exit(1);
}

static void
ack(struct printer *pp)
{
	if (write(STDOUT_FILENO, "\0", 1) != 1)
		frecverr("%s: write error on ack", pp->printer);
}

static void
nak(struct printer *pp)
{
	if (write(STDOUT_FILENO, "\2", 1) != 1)
		frecverr("%s: write error on nak", pp->printer);
}
