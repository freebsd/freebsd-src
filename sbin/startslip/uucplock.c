/*
 * Copyright (c) 1988, 1993
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
static const char sccsid[] = "@(#)uucplock.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/dir.h>
#include <errno.h>
#ifndef USE_PERROR
#include <syslog.h>
#endif
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <paths.h>

#define LOCKFMT "LCK..%s"

/* Forward declarations */
static int put_pid (int fd, pid_t pid);
static pid_t get_pid (int fd);

/*
 * uucp style locking routines
 * return: 0 - success
 * 	  -1 - failure
 */

int uu_lock (char *ttyname)
{
	int fd;
	pid_t pid;
	char tbuf[sizeof(_PATH_UUCPLOCK) + MAXNAMLEN];

	(void)sprintf(tbuf, _PATH_UUCPLOCK LOCKFMT, ttyname);
	fd = open(tbuf, O_RDWR|O_CREAT|O_EXCL, 0660);
	if (fd < 0) {
		/*
		 * file is already locked
		 * check to see if the process holding the lock still exists
		 */
		fd = open(tbuf, O_RDWR, 0);
		if (fd < 0) {
#ifndef USE_PERROR
			syslog(LOG_ERR, "lock open: %m");
#else
			perror("lock open");
#endif
			return(-1);
		}
		if ((pid = get_pid (fd)) == -1) {
#ifndef USE_PERROR
			syslog(LOG_ERR, "lock read: %m");
#else
			perror("lock read");
#endif
			(void)close(fd);
			return(-1);
		}

		if (kill(pid, 0) == 0 || errno != ESRCH) {
			(void)close(fd);	/* process is still running */
			return(-1);
		}
		/*
		 * The process that locked the file isn't running, so
		 * we'll lock it ourselves
		 */
		if (lseek(fd, (off_t) 0, L_SET) < 0) {
#ifndef USE_PERROR
			syslog(LOG_ERR, "lock lseek: %m");
#else
			perror("lock lseek");
#endif
			(void)close(fd);
			return(-1);
		}
		/* fall out and finish the locking process */
	}
	pid = getpid();
	if (!put_pid (fd, pid)) {
#ifndef USE_PERROR
		syslog(LOG_ERR, "lock write: %m");
#else
		perror("lock write");
#endif
		(void)close(fd);
		(void)unlink(tbuf);
		return(-1);
	}
	(void)close(fd);
	return(0);
}

int uu_unlock (char *ttyname)
{
	char tbuf[sizeof(_PATH_UUCPLOCK) + MAXNAMLEN];

	(void)sprintf(tbuf, _PATH_UUCPLOCK LOCKFMT, ttyname);
	return(unlink(tbuf));
}

static int put_pid (int fd, pid_t pid)
{
	char buf[32];
	int len;

	len = sprintf (buf, "%10d\n", pid);
	return write (fd, buf, len) == len;
}

static pid_t get_pid (int fd)
{
	int bytes_read;
	char buf[32];
	pid_t pid;

	bytes_read = read (fd, buf, sizeof (buf) - 1);
	if (bytes_read > 0) {
		buf[bytes_read] = '\0';
		pid = strtol (buf, (char **) NULL, 10);
	} else
		pid = -1;
	return pid;
}

/* end of uucplock.c */
