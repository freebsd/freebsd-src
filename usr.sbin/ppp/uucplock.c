/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
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
 * $Id: uucplock.c,v 1.5 1995/05/30 03:51:00 rgrimes Exp $
 *
 */

#ifndef lint
static char sccsid[] = "@(#)uucplock.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/dir.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include "pathnames.h"

/*
 * uucp style locking routines
 * return: 0 - success
 * 	  -1 - failure
 */

int
uu_lock(ttyname)
	char *ttyname;
{
	extern int errno;
	int fd, pid;
	char tbuf[sizeof(_PATH_LOCKDIRNAME) + MAXNAMLEN];
	char pid_buf[64];
	off_t lseek();

	(void)sprintf(tbuf, _PATH_LOCKDIRNAME, ttyname);
	fd = open(tbuf, O_RDWR|O_CREAT|O_EXCL, 0664);
	if (fd < 0) {
		/*
		 * file is already locked
		 * check to see if the process holding the lock still exists
		 */
		fd = open(tbuf, O_RDWR, 0);
		if (fd < 0) {
			perror("lock open");
			return(-1);
		}
		if (read(fd, pid_buf, sizeof(pid_buf)) <= 0) {
			(void)close(fd);
			perror("lock read");
			return(-1);
		}

		pid = atoi(pid_buf);

		if (kill(pid, 0) == 0 || errno != ESRCH) {
			(void)close(fd);	/* process is still running */
			return(-1);
		}
		/*
		 * The process that locked the file isn't running, so
		 * we'll lock it ourselves
		 */
		if (lseek(fd, (off_t) 0, L_SET) < 0) {
			(void)close(fd);
			perror("lock lseek");
			return(-1);
		}
		/* fall out and finish the locking process */
	}
	pid = getpid();
	sprintf(pid_buf, "%10u\n", pid);
	if (write(fd, pid_buf, strlen(pid_buf)) != strlen(pid_buf)) {
		(void)close(fd);
		(void)unlink(tbuf);
		perror("lock write");
		return(-1);
	}
	(void)close(fd);
	return(0);
}

uu_unlock(ttyname)
	char *ttyname;
{
	char tbuf[sizeof(_PATH_LOCKDIRNAME) + MAXNAMLEN];

	(void)sprintf(tbuf, _PATH_LOCKDIRNAME, ttyname);
	return(unlink(tbuf));
}
