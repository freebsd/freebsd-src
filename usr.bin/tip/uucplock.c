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
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00139
 * --------------------         -----   ----------------------
 *
 * 19 Aug 93	Peter da Silva		Better errors and Taylor UUCP locks
 *
 */

#ifndef lint
static char sccsid[] = "@(#)uucplock.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/dir.h>
#include <errno.h>
#include "pathnames.h"

/* 
 * uucp style locking routines
 * return: 0 - success
 * 	  -1 - failure
 */

uu_lock(ttyname)
	char *ttyname;
{
	extern int errno;
	int fd, pid;
	char tbuf[sizeof(_PATH_LOCKDIRNAME) + MAXNAMLEN];
	off_t lseek();
	char text_pid[81];	/* PDS 93 */
	int len;		/* PDS 93 */

	(void)sprintf(tbuf, _PATH_LOCKDIRNAME, ttyname);
	fd = open(tbuf, O_RDWR|O_CREAT|O_EXCL, 0660);
	if (fd < 0) {
		/*
		 * file is already locked
		 * check to see if the process holding the lock still exists
		 */
		fd = open(tbuf, O_RDWR, 0);
		if (fd < 0) {
			perror(tbuf);	/* +PDS 93 */
			fprintf(stderr, "Can't open lock file.\n");
			return(-1);
		}
		len = read(fd, text_pid, sizeof(text_pid)-1);
		if(len<=0) {
			perror(tbuf);
			(void)close(fd);
			fprintf(stderr, "Can't read lock file.\n");
			return(-1);
		}
		text_pid[len] = 0;
		pid = atol(text_pid);	/* -PDS 93 */

		if (kill(pid, 0) == 0 || errno != ESRCH) {
			(void)close(fd);	/* process is still running */
			return(-1);
		}
		/*
		 * The process that locked the file isn't running, so
		 * we'll lock it ourselves
		 */
	/* +PDS 93 */
		fprintf(stderr, "Stale lock on %s PID=%d... overriding.\n",
			ttyname, pid);
		if (lseek(fd, 0L, L_SET) < 0) {
			perror(tbuf);
			(void)close(fd);
			fprintf(stderr, "Can't seek lock file.\n");
			return(-1);
		}
		/* fall out and finish the locking process */
	}
	pid = getpid();
	sprintf(text_pid, "%10d\n", pid);
	len = strlen(text_pid);
	if (write(fd, text_pid, len) != len) {	/* -PDS 93 */
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
