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
  "$FreeBSD: src/usr.sbin/lpr/common_source/common.c,v 1.12.2.1 2000/07/03 06:22:35 ps Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <dirent.h>
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
char	*name;		/* program name */

extern uid_t	uid, euid;

static int compar __P((const void *, const void *));

/*
 * Getline reads a line from the control file cfp, removes tabs, converts
 *  new-line to null and leaves it in line.
 * Returns 0 at EOF or the number of characters read.
 */
int
getline(cfp)
	FILE *cfp;
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
getq(pp, namelist)
	const struct printer *pp;
	struct queue *(*namelist[]);
{
	register struct dirent *d;
	register struct queue *q, **queue;
	register int nitems;
	struct stat stbuf;
	DIR *dirp;
	int arraysz;

	seteuid(euid);
	if ((dirp = opendir(pp->spool_dir)) == NULL)
		return(-1);
	if (fstat(dirp->dd_fd, &stbuf) < 0)
		goto errdone;
	seteuid(uid);

	/*
	 * Estimate the array size by taking the size of the directory file
	 * and dividing it by a multiple of the minimum size entry. 
	 */
	arraysz = (stbuf.st_size / 24);
	queue = (struct queue **)malloc(arraysz * sizeof(struct queue *));
	if (queue == NULL)
		goto errdone;

	nitems = 0;
	while ((d = readdir(dirp)) != NULL) {
		if (d->d_name[0] != 'c' || d->d_name[1] != 'f')
			continue;	/* daemon control files only */
		seteuid(euid);
		if (stat(d->d_name, &stbuf) < 0)
			continue;	/* Doesn't exist */
		seteuid(uid);
		q = (struct queue *)malloc(sizeof(time_t)+strlen(d->d_name)+1);
		if (q == NULL)
			goto errdone;
		q->q_time = stbuf.st_mtime;
		strcpy(q->q_name, d->d_name);
		/*
		 * Check to make sure the array has space left and
		 * realloc the maximum size.
		 */
		if (++nitems > arraysz) {
			arraysz *= 2;
			queue = (struct queue **)realloc((char *)queue,
				arraysz * sizeof(struct queue *));
			if (queue == NULL)
				goto errdone;
		}
		queue[nitems-1] = q;
	}
	closedir(dirp);
	if (nitems)
		qsort(queue, nitems, sizeof(struct queue *), compar);
	*namelist = queue;
	return(nitems);

errdone:
	closedir(dirp);
	return(-1);
}

/*
 * Compare modification times.
 */
static int
compar(p1, p2)
	const void *p1, *p2;
{
	const struct queue *qe1, *qe2;
	qe1 = *(const struct queue **)p1;
	qe2 = *(const struct queue **)p2;
	
	if (qe1->q_time < qe2->q_time)
		return (-1);
	if (qe1->q_time > qe2->q_time)
		return (1);
	/*
	 * At this point, the two files have the same last-modification time.
	 * return a result based on filenames, so that 'cfA001some.host' will
	 * come before 'cfA002some.host'.  Since the jobid ('001') will wrap
	 * around when it gets to '999', we also assume that '9xx' jobs are
	 * older than '0xx' jobs.
	*/
	if ((qe1->q_name[3] == '9') && (qe2->q_name[3] == '0'))
		return (-1);
	if ((qe1->q_name[3] == '0') && (qe2->q_name[3] == '9'))
		return (1);
	return (strcmp(qe1->q_name,qe2->q_name));
}

/* sleep n milliseconds */
void
delay(n)
	int n;
{
	struct timeval tdelay;

	if (n <= 0 || n > 10000)
		fatal((struct printer *)0, /* fatal() knows how to deal */
		      "unreasonable delay period (%d)", n);
	tdelay.tv_sec = n / 1000;
	tdelay.tv_usec = n * 1000 % 1000000;
	(void) select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &tdelay);
}

char *
lock_file_name(pp, buf, len)
	const struct printer *pp;
	char *buf;
	size_t len;
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
status_file_name(pp, buf, len)
	const struct printer *pp;
	char *buf;
	size_t len;
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
closeallfds(start)
	int start;
{
	int stop = getdtablesize();
	for (; start < stop; start++)
		close(start);
}

