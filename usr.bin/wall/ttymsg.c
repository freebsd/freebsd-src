/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "ttymsg.h"

/*
 * Display the contents of a uio structure on a terminal.  If shout is
 * non-zero, do so even if the terminal has messages disabled.  Used by
 * wall(1), syslogd(8), and talkd(8).  Forks and finishes in child if
 * write would block, waiting up to timeout seconds.  Various "normal"
 * errors are ignored (exclusive-use, lack of permission, etc.).
 */
int
ttymsg(struct iovec *iov, int iovcnt, const char *tty, int timeout,
    bool shout)
{
	struct iovec localiov[TTYMSG_IOV_MAX];
	struct stat sb;
	ssize_t wret;
	size_t resid;
	int cnt, dd, fd, serrno;
	int forked;

	forked = 0;
	if (iovcnt > (int)(sizeof(localiov) / sizeof(localiov[0]))) {
		errno = EFBIG;
		return (-1);
	}

	dd = open(_PATH_DEV, O_SEARCH | O_DIRECTORY);
	if (dd < 0)
		return (-1);
	fd = openat(dd, tty, O_WRONLY | O_NONBLOCK | O_RESOLVE_BENEATH);
	if (fd < 0) {
		serrno = errno;
		close(dd);
		/*
		 * open will fail on slip lines or exclusive-use lines
		 * if not running as root; not an error.
		 */
		if (serrno == EBUSY || serrno == EACCES)
			return (0);
		errno = serrno;
		return (-1);
	}
	close(dd);
	if (!shout) {
		if (fstat(fd, &sb) != 0) {
			serrno = errno;
			close(fd);
			errno = serrno;
			return (-1);
		}
		if ((sb.st_mode & S_IWGRP) == 0) {
			close(fd);
			return (0);
		}
	}

	for (cnt = 0, resid = 0; cnt < iovcnt; ++cnt)
		resid += iov[cnt].iov_len;

	do {
		wret = writev(fd, iov, iovcnt);
		if (wret >= 0) {
			resid -= wret;
			if (iov != localiov) {
				bcopy(iov, localiov,
				    iovcnt * sizeof(struct iovec));
				iov = localiov;
			}
			while ((size_t)wret >= iov->iov_len) {
				wret -= iov->iov_len;
				++iov;
				--iovcnt;
			}
			if (wret) {
				iov->iov_base = (char *)iov->iov_base + wret;
				iov->iov_len -= wret;
			}
			continue;
		}
		if (errno == EWOULDBLOCK) {
			int cpid;

			if (forked) {
				(void) close(fd);
				_exit(1);
			}
			cpid = fork();
			if (cpid < 0) {
				serrno = errno;
				(void) close(fd);
				errno = serrno;
				return (-1);
			}
			if (cpid) {	/* parent */
				(void) close(fd);
				return (0);
			}
			forked++;
			/* wait at most timeout seconds */
			(void) signal(SIGALRM, SIG_DFL);
			(void) signal(SIGTERM, SIG_DFL); /* XXX */
			(void) sigsetmask(0);
			(void) alarm((u_int)timeout);
			(void) fcntl(fd, F_SETFL, 0);	/* clear O_NONBLOCK */
			continue;
		}
		/*
		 * We get ENODEV on a slip line if we're running as root,
		 * and EIO if the line just went away.
		 */
		serrno = errno;
		if (serrno == ENODEV || serrno == EIO)
			break;
		(void) close(fd);
		if (forked)
			_exit(1);
		errno = serrno;
		return (-1);
	} while (resid > 0);

	(void) close(fd);
	if (forked)
		_exit(0);
	return (0);
}
