/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
/*
static char sccsid[] = "@(#)common.c	8.5 (Berkeley) 4/28/95";
*/
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lp.h"
#include "lp.local.h"
#include "pathnames.h"

/*
 * Routines and data common to all the line printer functions.
 */
char	line[BUFSIZ];
const char	*name;		/* program name */

extern uid_t	uid, euid;

static int compar(const void *_p1, const void *_p2);

/*
 * Getline reads a line from the control file cfp, removes tabs, converts
 *  new-line to null and leaves it in line.
 * Returns 0 at EOF or the number of characters read.
 */
int
getline(FILE *cfp)
{
	register int linel = 0;
	register char *lp = line;
	register int c;

	while ((c = getc(cfp)) != '\n' && linel+1 < sizeof(line)) {
		if (c == EOF)
			return(0);
		if (c == '\t') {
			do {
				*lp++ = ' ';
				linel++;
			} while ((linel & 07) != 0 && linel+1 < sizeof(line));
			continue;
		}
		*lp++ = c;
		linel++;
	}
	*lp++ = '\0';
	return(linel);
}

/*
 * Scan the current directory and make a list of daemon files sorted by
 * creation time.
 * Return the number of entries and a pointer to the list.
 */
int
getq(const struct printer *pp, struct jobqueue *(*namelist[]))
{
	register struct dirent *d;
	register struct jobqueue *q, **queue;
	register int nitems;
	struct stat stbuf;
	DIR *dirp;
	int arraysz, statres;

	seteuid(euid);
	if ((dirp = opendir(pp->spool_dir)) == NULL) {
		seteuid(uid);
		return (-1);
	}
	if (fstat(dirp->dd_fd, &stbuf) < 0)
		goto errdone;
	seteuid(uid);

	/*
	 * Estimate the array size by taking the size of the directory file
	 * and dividing it by a multiple of the minimum size entry. 
	 */
	arraysz = (stbuf.st_size / 24);
	queue = (struct jobqueue **)malloc(arraysz * sizeof(struct jobqueue *));
	if (queue == NULL)
		goto errdone;

	nitems = 0;
	while ((d = readdir(dirp)) != NULL) {
		if (d->d_name[0] != 'c' || d->d_name[1] != 'f')
			continue;	/* daemon control files only */
		seteuid(euid);
		statres = stat(d->d_name, &stbuf);
		seteuid(uid);
		if (statres < 0)
			continue;	/* Doesn't exist */
		q = (struct jobqueue *)malloc(sizeof(time_t) + strlen(d->d_name)
		    + 1);
		if (q == NULL)
			goto errdone;
		q->job_time = stbuf.st_mtime;
		strcpy(q->job_cfname, d->d_name);
		/*
		 * Check to make sure the array has space left and
		 * realloc the maximum size.
		 */
		if (++nitems > arraysz) {
			arraysz *= 2;
			queue = (struct jobqueue **)realloc((char *)queue,
			    arraysz * sizeof(struct jobqueue *));
			if (queue == NULL)
				goto errdone;
		}
		queue[nitems-1] = q;
	}
	closedir(dirp);
	if (nitems)
		qsort(queue, nitems, sizeof(struct jobqueue *), compar);
	*namelist = queue;
	return(nitems);

errdone:
	closedir(dirp);
	seteuid(uid);
	return (-1);
}

/*
 * Compare modification times.
 */
static int
compar(const void *p1, const void *p2)
{
	const struct jobqueue *qe1, *qe2;

	qe1 = *(const struct jobqueue **)p1;
	qe2 = *(const struct jobqueue **)p2;
	
	if (qe1->job_time < qe2->job_time)
		return (-1);
	if (qe1->job_time > qe2->job_time)
		return (1);
	/*
	 * At this point, the two files have the same last-modification time.
	 * return a result based on filenames, so that 'cfA001some.host' will
	 * come before 'cfA002some.host'.  Since the jobid ('001') will wrap
	 * around when it gets to '999', we also assume that '9xx' jobs are
	 * older than '0xx' jobs.
	*/
	if ((qe1->job_cfname[3] == '9') && (qe2->job_cfname[3] == '0'))
		return (-1);
	if ((qe1->job_cfname[3] == '0') && (qe2->job_cfname[3] == '9'))
		return (1);
	return (strcmp(qe1->job_cfname, qe2->job_cfname));
}

/* sleep n milliseconds */
void
delay(int millisec)
{
	struct timeval tdelay;

	if (millisec <= 0 || millisec > 10000)
		fatal((struct printer *)0, /* fatal() knows how to deal */
		    "unreasonable delay period (%d)", millisec);
	tdelay.tv_sec = millisec / 1000;
	tdelay.tv_usec = millisec * 1000 % 1000000;
	(void) select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &tdelay);
}

char *
lock_file_name(const struct printer *pp, char *buf, size_t len)
{
	static char staticbuf[MAXPATHLEN];

	if (buf == 0)
		buf = staticbuf;
	if (len == 0)
		len = MAXPATHLEN;

	if (pp->lock_file[0] == '/') {
		buf[0] = '\0';
		strncpy(buf, pp->lock_file, len);
	} else {
		snprintf(buf, len, "%s/%s", pp->spool_dir, pp->lock_file);
	}
	return buf;
}

char *
status_file_name(const struct printer *pp, char *buf, size_t len)
{
	static char staticbuf[MAXPATHLEN];

	if (buf == 0)
		buf = staticbuf;
	if (len == 0)
		len = MAXPATHLEN;

	if (pp->status_file[0] == '/') {
		buf[0] = '\0';
		strncpy(buf, pp->status_file, len);
	} else {
		snprintf(buf, len, "%s/%s", pp->spool_dir, pp->status_file);
	}
	return buf;
}

/* routine to get a current timestamp, optionally in a standard-fmt string */
void
lpd_gettime(struct timespec *tsp, char *strp, int strsize)
{
	struct timespec local_ts;
	struct timeval btime;
	char *destp;
	char tempstr[TIMESTR_SIZE];
	
	if (tsp == NULL)
		tsp = &local_ts;

	/* some platforms have a routine called clock_gettime, but the
	 * routine does nothing but return "not implemented". */
	memset(tsp, 0, sizeof(struct timespec));
	if (clock_gettime(CLOCK_REALTIME, tsp)) {
		/* nanosec-aware rtn failed, fall back to microsec-aware rtn */
		memset(tsp, 0, sizeof(struct timespec));
		gettimeofday(&btime, NULL);
		tsp->tv_sec = btime.tv_sec;
		tsp->tv_nsec = btime.tv_usec * 1000;
	}

	/* caller may not need a character-ized version */
	if ((strp == NULL) || (strsize < 1))
		return;

	strftime(tempstr, TIMESTR_SIZE, LPD_TIMESTAMP_PATTERN,
		 localtime(&tsp->tv_sec));

	/*
	 * This check is for implementations of strftime which treat %z
	 * (timezone as [+-]hhmm ) like %Z (timezone as characters), or
	 * completely ignore %z.  This section is not needed on freebsd.
	 * I'm not sure this is completely right, but it should work OK
	 * for EST and EDT...
	 */
#ifdef STRFTIME_WRONG_z
	destp = strrchr(tempstr, ':');
	if (destp != NULL) {
		destp += 3;
		if ((*destp != '+') && (*destp != '-')) {
			char savday[6];
			int tzmin = timezone / 60;
			int tzhr = tzmin / 60;
			if (daylight)
				tzhr--;
			strcpy(savday, destp + strlen(destp) - 4);
			snprintf(destp, (destp - tempstr), "%+03d%02d",
			    (-1*tzhr), tzmin % 60);
			strcat(destp, savday);
		}
	}
#endif

	if (strsize > TIMESTR_SIZE) {
		strsize = TIMESTR_SIZE;
		strp[TIMESTR_SIZE+1] = '\0';
	}
	strncpy(strp, tempstr, strsize);
}

/* routines for writing transfer-statistic records */
void
trstat_init(struct printer *pp, const char *fname, int filenum)
{
	register const char *srcp;
	register char *destp, *endp;

	/*
	 * Figure out the job id of this file.  The filename should be
	 * 'cf', 'df', or maybe 'tf', followed by a letter (or sometimes
	 * two), followed by the jobnum, followed by a hostname.
	 * The jobnum is usually 3 digits, but might be as many as 5.
	 * Note that some care has to be taken parsing this, as the
	 * filename could be coming from a remote-host, and thus might
	 * not look anything like what is expected...
	 */
	memset(pp->jobnum, 0, sizeof(pp->jobnum));
	pp->jobnum[0] = '0';
	srcp = strchr(fname, '/');
	if (srcp == NULL)
		srcp = fname;
	destp = &(pp->jobnum[0]);
	endp = destp + 5;
	while (*srcp != '\0' && (*srcp < '0' || *srcp > '9'))
		srcp++;
	while (*srcp >= '0' && *srcp <= '9' && destp < endp)
		*(destp++) = *(srcp++);

	/* get the starting time in both numeric and string formats, and
	 * save those away along with the file-number */
	pp->jobdfnum = filenum;
	lpd_gettime(&pp->tr_start, pp->tr_timestr, TIMESTR_SIZE);

	return;
}

void
trstat_write(struct printer *pp, tr_sendrecv sendrecv, size_t bytecnt,
    const char *userid, const char *otherhost, const char *orighost)
{
#define STATLINE_SIZE 1024
	double trtime;
	int remspace, statfile;
	char thishost[MAXHOSTNAMELEN], statline[STATLINE_SIZE];
	char *eostat;
	const char *lprhost, *recvdev, *recvhost, *rectype;
	const char *sendhost, *statfname;
#define UPD_EOSTAT(xStr) do {         \
	eostat = strchr(xStr, '\0');  \
	remspace = eostat - xStr;     \
} while(0)
	
	lpd_gettime(&pp->tr_done, NULL, 0);
	trtime = DIFFTIME_TS(pp->tr_done, pp->tr_start);

	gethostname(thishost, sizeof(thishost));
	lprhost = sendhost = recvhost = recvdev = NULL;
	switch (sendrecv) {
	    case TR_SENDING:
		rectype = "send";
		statfname = pp->stat_send;
		sendhost = thishost;
		recvhost = otherhost;
		break;
	    case TR_RECVING:
		rectype = "recv";
		statfname = pp->stat_recv;
		sendhost = otherhost;
		recvhost = thishost;
		break;
	    case TR_PRINTING:
		/*
		 * This case is for copying to a device (presumably local,
		 * though filters using things like 'net/CAP' can confuse
		 * this assumption...).
		 */
		rectype = "prnt";
		statfname = pp->stat_send;
		sendhost = thishost;
		recvdev = _PATH_DEFDEVLP;
		if (pp->lp) recvdev = pp->lp;
		break;
	    default:
		/* internal error...  should we syslog/printf an error? */
		return;
	}
	if (statfname == NULL)
		return;

	/*
	 * the original-host and userid are found out by reading thru the
	 * cf (control-file) for the job.  Unfortunately, on incoming jobs
	 * the df's (data-files) are sent before the matching cf, so the
	 * orighost & userid are generally not-available for incoming jobs.
	 *
	 * (it would be nice to create a work-around for that..)
	 */
	if (orighost && (*orighost != '\0'))
		lprhost = orighost;
	else
		lprhost = ".na.";
	if (*userid == '\0')
		userid = NULL;

	/*
	 * Format of statline.
	 * Some of the keywords listed here are not implemented here, but
	 * they are listed to reserve the meaning for a given keyword.
	 * Fields are separated by a blank.  The fields in statline are:
	 *   <tstamp>      - time the transfer started
	 *   <ptrqueue>    - name of the printer queue (the short-name...)
	 *   <hname>       - hostname the file originally came from (the
	 *		     'lpr host'), if known, or  "_na_" if not known.
	 *   <xxx>         - id of job from that host (generally three digits)
	 *   <n>           - file count (# of file within job)
	 *   <rectype>     - 4-byte field indicating the type of transfer
	 *		     statistics record.  "send" means it's from the
	 *		     host sending a datafile, "recv" means it's from
	 *		     a host as it receives a datafile.
	 *   user=<userid> - user who sent the job (if known)
	 *   secs=<n>      - seconds it took to transfer the file
	 *   bytes=<n>     - number of bytes transfered (ie, "bytecount")
	 *   bps=<n.n>e<n> - Bytes/sec (if the transfer was "big enough"
	 *		     for this to be useful) 
	 * ! top=<str>     - type of printer (if the type is defined in
	 *		     printcap, and if this statline is for sending
	 *		     a file to that ptr)
	 * ! qls=<n>       - queue-length at start of send/print-ing a job
	 * ! qle=<n>       - queue-length at end of send/print-ing a job
	 *   sip=<addr>    - IP address of sending host, only included when
	 *		     receiving a job.
	 *   shost=<hname> - sending host (if that does != the original host)
	 *   rhost=<hname> - hostname receiving the file (ie, "destination")
	 *   rdev=<dev>    - device receiving the file, when the file is being
	 *		     send to a device instead of a remote host.
	 *
	 * Note: A single print job may be transferred multiple times.  The
	 * original 'lpr' occurs on one host, and that original host might
	 * send to some interim host (or print server).  That interim host
	 * might turn around and send the job to yet another host (most likely
	 * the real printer).  The 'shost=' parameter is only included if the
	 * sending host for this particular transfer is NOT the same as the
	 * host which did the original 'lpr'.
	 *
	 * Many values have 'something=' tags before them, because they are
	 * in some sense "optional", or their order may vary.  "Optional" may
	 * mean in the sense that different SITES might choose to have other
	 * fields in the record, or that some fields are only included under
	 * some circumstances.  Programs processing these records should not
	 * assume the order or existence of any of these keyword fields.
	 */
	snprintf(statline, STATLINE_SIZE, "%s %s %s %s %03ld %s",
	    pp->tr_timestr, pp->printer, lprhost, pp->jobnum,
	    pp->jobdfnum, rectype);
	UPD_EOSTAT(statline);

	if (userid != NULL) {
		snprintf(eostat, remspace, " user=%s", userid);
		UPD_EOSTAT(statline);
	}
	snprintf(eostat, remspace, " secs=%#.2f bytes=%u", trtime, bytecnt);
	UPD_EOSTAT(statline);
	
	/*
	 * The bps field duplicates info from bytes and secs, so do
	 * not bother to include it for very small files.
	 */
	if ((bytecnt > 25000) && (trtime > 1.1)) {
		snprintf(eostat, remspace, " bps=%#.2e",
		    ((double)bytecnt/trtime));
		UPD_EOSTAT(statline);
	}

	if (sendrecv == TR_RECVING) {
		if (remspace > 5+strlen(from_ip) ) {
			snprintf(eostat, remspace, " sip=%s", from_ip);
			UPD_EOSTAT(statline);
		}
	}
	if (0 != strcmp(lprhost, sendhost)) {
		if (remspace > 7+strlen(sendhost) ) {
			snprintf(eostat, remspace, " shost=%s", sendhost);
			UPD_EOSTAT(statline);
		}
	}
	if (recvhost) {
		if (remspace > 7+strlen(recvhost) ) {
			snprintf(eostat, remspace, " rhost=%s", recvhost);
			UPD_EOSTAT(statline);
		}
	}
	if (recvdev) {
		if (remspace > 6+strlen(recvdev) ) {
			snprintf(eostat, remspace, " rdev=%s", recvdev);
			UPD_EOSTAT(statline);
		}
	}
	if (remspace > 1) {
		strcpy(eostat, "\n");
	} else {
		/* probably should back up to just before the final " x=".. */  
		strcpy(statline+STATLINE_SIZE-2, "\n");
	}
	statfile = open(statfname, O_WRONLY|O_APPEND, 0664);
	if (statfile < 0) {
		/* statfile was given, but we can't open it.  should we
		 * syslog/printf this as an error? */
		return;
	}
	write(statfile, statline, strlen(statline));
	close(statfile);

	return;
#undef UPD_EOSTAT	
}

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#ifdef __STDC__
fatal(const struct printer *pp, const char *msg, ...)
#else
fatal(pp, msg, va_alist)
	const struct printer *pp;
	char *msg;
        va_dcl
#endif
{
	va_list ap;
#ifdef __STDC__
	va_start(ap, msg);
#else
	va_start(ap);
#endif
	if (from != host)
		(void)printf("%s: ", host);
	(void)printf("%s: ", name);
	if (pp && pp->printer)
		(void)printf("%s: ", pp->printer);
	(void)vprintf(msg, ap);
	va_end(ap);
	(void)putchar('\n');
	exit(1);
}

/*
 * Close all file descriptors from START on up.
 * This is a horrific kluge, since getdtablesize() might return
 * ``infinity'', in which case we will be spending a long time
 * closing ``files'' which were never open.  Perhaps it would
 * be better to close the first N fds, for some small value of N.
 */
void
closeallfds(int start)
{
	int stop = getdtablesize();
	for (; start < stop; start++)
		close(start);
}

