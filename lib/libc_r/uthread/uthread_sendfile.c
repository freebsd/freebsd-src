/*
 * Copyright (C) 2000 Jason Evans <jasone@freebsd.org>.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libc_r/uthread/uthread_sendfile.c,v 1.2.2.1 2000/07/18 01:57:21 jasone Exp $
 */

#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <errno.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
sendfile(int fd, int s, off_t offset, size_t nbytes, struct sf_hdtr *hdtr,
    off_t *sbytes, int flags)
{
	int	ret, type, blocking;
	ssize_t wvret, num = 0;
	off_t	n, nwritten = 0;

	/* Write the headers if any. */
	if ((hdtr != NULL) && (hdtr->headers != NULL)) {
		if (wvret = writev(s, hdtr->headers, hdtr->hdr_cnt) == -1) {
			ret = -1;
			goto ERROR;
		} else
			nwritten += wvret;
	}
	
	/* Lock the descriptors. */
	if ((ret = _FD_LOCK(fd, FD_READ, NULL)) != 0) {
		ret = -1;
		errno = EBADF;
		goto ERROR;
	}
	if ((ret = _FD_LOCK(s, FD_WRITE, NULL)) != 0) {
		ret = -1;
		errno = EBADF;
		goto ERROR_1;
	}
	
	/* Check the descriptor access modes. */
	type = _thread_fd_table[fd]->flags & O_ACCMODE;
	if (type != O_RDONLY && type != O_RDWR) {
		/* File is not open for read. */
		ret = -1;
		errno = EBADF;
		goto ERROR_2;
	}
	type = _thread_fd_table[s]->flags & O_ACCMODE;
	if (type != O_WRONLY && type != O_RDWR) {
		/* File is not open for write. */
		ret = -1;
		errno = EBADF;
		goto ERROR_2;
	}

	/* Check if file operations are to block */
	blocking = ((_thread_fd_table[s]->flags & O_NONBLOCK) == 0);
	
	/*
	 * Loop while no error occurs and until the expected number of bytes are
	 * written.
	 */
	for (;;) {
		/* Perform a non-blocking sendfile syscall. */
		ret = _thread_sys_sendfile(fd, s, offset + num, nbytes - num,
		    NULL, &n, flags);

		if (ret == 0) {
			/* Writing completed. */
			num += n;
			break;
		} else if ((blocking) && (ret == -1) && (errno == EAGAIN)) {
			/*
			 * Some bytes were written but there are still more to
			 * write.
			 */

			/* Update the count of bytes written. */
			num += n;

			_thread_run->data.fd.fd = fd;
			_thread_kern_set_timeout(NULL);

			/* Reset the interrupted operation flag. */
			_thread_run->interrupted = 0;

			_thread_kern_sched_state(PS_FDW_WAIT, __FILE__,
			    __LINE__);

			if (_thread_run->interrupted) {
				/* Interrupted by a signal.  Return an error. */
				break;
			}
		} else {
			/* Incomplete non-blocking syscall, or error. */
			break;
		}
	}

  ERROR_2:
	_FD_UNLOCK(s, FD_WRITE);
  ERROR_1:
	_FD_UNLOCK(fd, FD_READ);
  ERROR:
	if (ret == 0) {
		/* Write the trailers, if any. */
		if ((hdtr != NULL) && (hdtr->trailers != NULL)) {
			if (wvret = writev(s, hdtr->trailers, hdtr->trl_cnt)
			    == -1)
				ret = -1;
			else
				nwritten += wvret;
		}
	}
	if (sbytes != NULL) {
		/*
		 * Number of bytes written in headers/trailers, plus in the main
		 * sendfile() loop.
		 */
		*sbytes = nwritten + num;
	}
	return ret;
}
#endif
